/*
 * Copyright (c) 2020 Huawei Technologies Co.,Ltd.
 *
 * openGauss is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 *
 * row_header.cpp
 *    Row header implementation in OCC
 *
 * IDENTIFICATION
 *    src/gausskernel/storage/mot/core/src/concurrency_control/row_header.cpp
 *
 * -------------------------------------------------------------------------
 */

#include "row_header.h"
#include "global.h"
#include "mot_atomic_ops.h"
#include "row.h"
#include "txn_access.h"
#include "utilities.h"
#include "cycles.h"
#include "debug_utils.h"
#include "column.h"

namespace MOT {
DECLARE_LOGGER(RowHeader, ConcurrenyControl);

///


/*********************/
//related to aria

bool RowHeader::aria_ValidateAndSetWrite(uint64_t m_csn, uint64_t epoch) {
    /// get the lock first
    Lock();
    //取小的csn写，因为要判断依赖关系
    if (GetEpoch() == epoch)
    {
        if (GetCSN() < m_csn)
        {
            Release();
            return false;
        }
    }
    else
    {
        SetCSN(m_csn);
        SetEpoch(epoch);
        /// release the lock
        Release();
        return true;
    }
    
}


bool RowHeader::check_WAW(int Txn_m_csn, int Txn_m_epoch)
{
    if (GetEpoch() == Txn_m_epoch)
    {
        if (GetCSN() < Txn_m_csn)
            return false;
    }

    return true;
}


bool RowHeader::check_RAW(int Txn_m_csn, int Txn_m_epoch)
{
    if (GetEpoch() == Txn_m_epoch)
    {
        if (GetCSN() < Txn_m_csn)
            return false;
    }

    return true;
}


bool RowHeader::check_WAR(int Txn_m_csn, int Txn_m_epoch)
{
    if (GetEpoch_read() == Txn_m_epoch)
    {
        if (GetCSN_read() < Txn_m_csn)
            return false;
    }
    return true;
}

void RowHeader::reserveRead(int Txn_m_csn, int Txn_m_epoch)
{
    Lock_read();

    if (Txn_m_epoch == GetEpoch_read())
    {
        if (Txn_m_csn < GetCSN_read())
        {
           SetCSN_read(Txn_m_csn);
        }
    }
    else
    {
        SetCSN_read(Txn_m_csn);
        SetEpoch_read(Txn_m_epoch);

    }
    Release_read();
}

/********************/










bool RowHeader::ValidateAndSetWrite(uint64_t m_csn, uint64_t epoch) {
    /// get the lock first
    Lock();
    /// last-update win
    if(m_csn < GetCSN()) return false;
    /// set the csn
    SetCSN(m_csn);
    SetEpoch(epoch);
    /// release the lock
    Release();
    return true;
}

bool RowHeader::ValidateAndSetWrite_1(const Access* access,uint64_t m_csn, uint64_t epoch)
{
    MOT_LOG_INFO("row header---ValidateAndSetWrite_1:进入...");

    if (access->m_type == INS)
    {
        MOT_LOG_INFO("row header---ValidateAndSetWrite_1: INS类型")
    }
    MOT_LOG_INFO("row header---ValidateAndSetWrite_1:开始获取row...");
    const Row* row = access->GetRowFromHeader();
    MOT_LOG_INFO("row header---ValidateAndSetWrite_1:完成获取row...");

    if(row == nullptr)
    {
        MOT_LOG_INFO("row header---ValidateAndSetWrite_1:row 为nullptr...");
    }

    

    MOT::BitmapSet& bmp = const_cast <MOT::BitmapSet&>(access->m_modifiedColumns);
    uint64_t fieldCnt = row->GetTable()->GetFieldCount();
    MOT_LOG_INFO((string("fieldCnt:")+std::to_string(fieldCnt)).c_str());

    if (access->m_type == INS)
    {
        MOT_LOG_INFO("row header---ValidateAndSetWrite_1:IF...");
        for (uint16_t field_i = 0; field_i < fieldCnt; field_i ++)
        {
            int real_field = field_i;

            MOT_LOG_INFO("Lock前");
            mp[real_field].Lock();
            MOT_LOG_INFO("Lock后");
            if (m_csn < mp[real_field].GetCSN())
                return false;
            mp[real_field].SetCSN(m_csn);
            mp[real_field].SetEpoch(epoch);
            mp[real_field].Release();
        }
    }
    else
    {
        MOT_LOG_INFO("row header---ValidateAndSetWrite_1:ELSE...");
        for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
        {
            if (bmp.GetBit(field_i))
            {
                
                int real_field = field_i+1;
                MOT_LOG_INFO((string("real_field:")+std::to_string(real_field)).c_str());
                
                

                MOT_LOG_INFO("Lock前");
                mp[real_field].Lock();
                MOT_LOG_INFO("Lock后");
                if (m_csn < mp[real_field].GetCSN())
                    return false;
                mp[real_field].SetCSN(m_csn);
                mp[real_field].SetEpoch(epoch);
                mp[real_field].Release();              
            }   
        }

    }

    MOT_LOG_INFO("row header---ValidateAndSetWrite_1:离开...");
    return true;
}



///
RC RowHeader::GetLocalCopy(
    TxnAccess* txn, AccessType type, Row* localRow, const Row* origRow, TransactionId& lastTid) const
{
    uint64_t sleepTime = 1;
    uint64_t v = 0;
    uint64_t v2 = 1;

    // concurrent update/delete after delete is not allowed - abort current transaction
    if ((m_csnWord & ABSENT_BIT) && type != AccessType::INS) {
        return RC_ABORT;
    }

    while (v2 != v) {
        // contend for exclusive access
        v = m_csnWord;
        while (v & LOCK_BIT) {
            if (sleepTime > LOCK_TIME_OUT) {
                sleepTime = LOCK_TIME_OUT;
                struct timespec ts = {0, 5000};
                (void)nanosleep(&ts, NULL);
            } else {
                CpuCyclesLevelTime::Sleep(1);
                sleepTime = sleepTime << 1;
            }

            v = m_csnWord;
        }
        // No need to copy new-row.
        if (type != AccessType::INS) {  // get current row contents (not required during insertion of new row)
            localRow->Copy(origRow);
        }
        COMPILER_BARRIER
        v2 = m_csnWord;
    }
    if ((v & ABSENT_BIT) && (v & LATEST_VER_BIT)) {
        return RC_ABORT;
    }
    lastTid = v & (~LOCK_BIT);

    if (type == AccessType::INS) {
        // ROW ALREADY COMMITED
        if ((m_csnWord & (ABSENT_BIT)) == 0) {
            return RC_ABORT;
        }
        lastTid &= (~ABSENT_BIT);
    }

    return RC_OK;
}

bool RowHeader::ValidateWrite(TransactionId tid) const
{
    return (tid == GetCSN());
}

bool RowHeader::aria_ValidateWrite(TransactionId tid) const
{
    if (GetCSN() < tid)
        return false;
    else
        return true;
}


bool RowHeader::ValidateWrite_1(const Access* access,TransactionId tid) const
{

    if (access->m_type == INS)
    {
        const Row* row = access->GetRowFromHeader();
        uint64_t fieldCnt = row->GetTable()->GetFieldCount();
        for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
        {
            int real_field = field_i+1;
            if (tid != mp.at(real_field).GetCSN())
            {
                return false;
            }
                
        }
    }
    else
    {
        const Row* row = access->GetRowFromHeader();
        MOT::BitmapSet& bmp = const_cast <MOT::BitmapSet&>(access->m_modifiedColumns);
        uint64_t fieldCnt = row->GetTable()->GetFieldCount();
        
        for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
        {
            if (bmp.GetBit(field_i))
            {
                int real_field = field_i+1;
                if (tid != mp.at(real_field).GetCSN() )
                {
                    return false;
                }
            }   
        }
    }
    MOT_LOG_INFO("RowHeader::ValidateWrite_1:done.")

    return true;
}




bool RowHeader::ValidateRead(TransactionId tid) const
{
    if (IsLocked() or (tid != GetCSN())) {
        return false;
    }

    return true;
}

bool RowHeader::ValidateRead_1(const Access* access,TransactionId tid) const
{

    if (access->m_type == INS)
    {
        const Row* row = access->GetRowFromHeader();
        uint64_t fieldCnt = row->GetTable()->GetFieldCount();
        for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
        {
        
            int real_field = field_i+1;
                            
            if (mp.at(real_field).IsLocked() or (tid != mp.at(real_field).GetCSN()))
            {
                return false;
            }
              
        }
    }
    else
    {
        const Row* row = access->GetRowFromHeader();
        MOT::BitmapSet& bmp = const_cast <MOT::BitmapSet&>(access->m_modifiedColumns);
        uint64_t fieldCnt = row->GetTable()->GetFieldCount();
        for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
        {
            if (bmp.GetBit(field_i))
            {
                int real_field = field_i+1;
                                
                if (mp.at(real_field).IsLocked() or (tid != mp.at(real_field).GetCSN()))
                {
                    return false;
                }
            }   
        }

    }

    
    return true;
}


bool RowHeader::ValidateEpoch_1(const Access* ac,int epoch)
{

        if (ac->m_type == INS)
        {
            const Row* row = ac->GetRowFromHeader();
            uint64_t fieldCnt = row->GetTable()->GetFieldCount();
            for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
            {
               
                int real_field = field_i+1;
                //如果该列的epoch大于此txn的start epoch，则说明已经有其余的txn修改
                if (mp[real_field].GetEpoch() > epoch)
                {
                    return false;
                } 
            }
        }
        else
        {
            const Row* row = ac->GetRowFromHeader();
            MOT::BitmapSet& bmp = const_cast <MOT::BitmapSet&>(ac->m_modifiedColumns);
            uint64_t fieldCnt = row->GetTable()->GetFieldCount();
            for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
            {
                if (bmp.GetBit(field_i))
                {
                    int real_field = field_i+1;
                    //如果该列的epoch大于此txn的start epoch，则说明已经有其余的txn修改
                    if (mp[real_field].GetEpoch() > epoch)
                    {
                        return false;
                    }

                }   
            }
        }        
    return true;
}

void RowHeader::WriteChangesToRow(const Access* access, uint64_t csn)
{
    Row* row = access->GetRowFromHeader();
    AccessType type = access->m_type;

    if (type == RD) {
        return;
    }
#ifdef MOT_DEBUG
    if (access->m_params.IsPrimarySentinel()) {
        uint64_t v = m_csnWord;
        if (!(csn > GetCSN() && (v & LOCK_BIT))) {
            MOT_LOG_ERROR("csn=%ld, v & LOCK_BIT=%ld, v & (~LOCK_BIT)=%ld\n", csn, (v & LOCK_BIT), (v & (~LOCK_BIT)));
            MOT_ASSERT(false);
        }
    }
#endif
    switch (type) {
        case WR:
            MOT_ASSERT(access->m_params.IsPrimarySentinel() == true);
            row->Copy(access->m_localRow);
            // m_csnWord = (csn | LOCK_BIT);
            break;
        case DEL:
            MOT_ASSERT(access->m_origSentinel->IsCommited() == true);
            if (access->m_params.IsPrimarySentinel()) {
                if (row->GetPrimarySentinel()->GetStable() == nullptr) {
                    m_csnWord = (csn | LOCK_BIT | ABSENT_BIT | LATEST_VER_BIT);
                } else {
                    m_csnWord = (csn | LOCK_BIT | ABSENT_BIT);  // if has stable, avoid destroying the row
                }
                // and allow reuse of the original row
            }
            // Invalidate sentinel  - row is still locked!
            access->m_origSentinel->SetDirty();
            break;
        case INS:
            if (access->m_params.IsPrimarySentinel()) {
                // At this case we have the new-row and the old row
                if (access->m_params.IsUpgradeInsert()) {
                    // We set the global-row to be locked and deleted
                    m_csnWord = (csn | LOCK_BIT | LATEST_VER_BIT);
                    // The new row is locked and absent!
                    access->m_auxRow->UnsetAbsentRow();
                    access->m_auxRow->SetCommitSequenceNumber(csn);
                } else {
                    m_csnWord = csn;
                }
            }
            break;
        default:
            break;
    }
}

void RowHeader::WriteChangesToRow_1(const Access* access, uint64_t csn)
{
    Row* row = access->GetRowFromHeader();
    AccessType type = access->m_type;

    MOT::BitmapSet& bmp = const_cast <MOT::BitmapSet&>(access->m_modifiedColumns);
    uint64_t fieldCnt = row->GetTable()->GetFieldCount();
    Row* src = access->m_localRow;
    int real_field;


    if (type == RD) {
        return;
    }
#ifdef MOT_DEBUG
    if (access->m_params.IsPrimarySentinel()) {
        uint64_t v = m_csnWord;
        if (!(csn > GetCSN() && (v & LOCK_BIT))) {
            MOT_LOG_ERROR("csn=%ld, v & LOCK_BIT=%ld, v & (~LOCK_BIT)=%ld\n", csn, (v & LOCK_BIT), (v & (~LOCK_BIT)));
            MOT_ASSERT(false);
        }
    }
#endif
    switch (type) {
        case WR:
            MOT_ASSERT(access->m_params.IsPrimarySentinel() == true);
            //row->Copy(access->m_localRow);
            MOT_LOG_INFO("RowHeader::WriteChangesToRow_1---WR...");
            for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
            {
                if (bmp.GetBit(field_i))
                {
                    real_field = field_i+1;
                    MOT_LOG_INFO((string("col:")+std::to_string(real_field)+string(" is been modified...")).c_str());

                    row->SetValueVariable_1(real_field,(const void *)src->GetValue(real_field),src->GetTable()->GetFieldSize(real_field));                  
                }   
            }    
            // m_csnWord = (csn | LOCK_BIT);
            break;
        case DEL:
            MOT_ASSERT(access->m_origSentinel->IsCommited() == true);
            if (access->m_params.IsPrimarySentinel()) {
                if (row->GetPrimarySentinel()->GetStable() == nullptr) {
                    m_csnWord = (csn | LOCK_BIT | ABSENT_BIT | LATEST_VER_BIT);
                } else {
                    m_csnWord = (csn | LOCK_BIT | ABSENT_BIT);  // if has stable, avoid destroying the row
                }
                // and allow reuse of the original row
            }
            // Invalidate sentinel  - row is still locked!
            access->m_origSentinel->SetDirty();
            break;
        case INS:
            if (access->m_params.IsPrimarySentinel()) {
                // At this case we have the new-row and the old row
                if (access->m_params.IsUpgradeInsert()) {
                    // We set the global-row to be locked and deleted
                    m_csnWord = (csn | LOCK_BIT | LATEST_VER_BIT);
                    // The new row is locked and absent!
                    access->m_auxRow->UnsetAbsentRow();
                    access->m_auxRow->SetCommitSequenceNumber(csn);
                } else {
                    m_csnWord = csn;
                }
            }
            break;
        default:
            break;
    }
}



void RowHeader::Lock()
{
    uint64_t v = m_csnWord;
    while ((v & LOCK_BIT) || !__sync_bool_compare_and_swap(&m_csnWord, v, v | LOCK_BIT)) {
        PAUSE
        v = m_csnWord;
    }
}

void RowHeader::Lock_read()
{
    uint64_t v = read_csn;
    while ((v & LOCK_BIT) || !__sync_bool_compare_and_swap(&read_csn, v, v | LOCK_BIT)) {
        PAUSE
        v = read_csn;
    }
}


void RowHeader::Lock_1(const Access* access)
{

    if (access->m_type == INS)
    {
        const Row* row = access->GetRowFromHeader();
        uint64_t fieldCnt = row->GetTable()->GetFieldCount();
        for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
        {
            
            int real_field = field_i+1;
            mp[real_field].Lock();                
             
        }
    }
    else
    {
        const Row* row = access->GetRowFromHeader();
        MOT::BitmapSet& bmp = const_cast <MOT::BitmapSet&>(access->m_modifiedColumns);
        uint64_t fieldCnt = row->GetTable()->GetFieldCount();
        for (uint16_t field_i = 0; field_i < fieldCnt-1; field_i ++)
        {
            if (bmp.GetBit(field_i))
            {
                int real_field = field_i+1;
                mp[real_field].Lock();                
            }   
        }  
    }
}


}  // namespace MOT
