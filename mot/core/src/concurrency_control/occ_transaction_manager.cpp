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
 * occ_transaction_manager.cpp
 *    Optimistic Concurrency Control (OCC) implementation
 *
 * IDENTIFICATION
 *    src/gausskernel/storage/mot/core/src/concurrency_control/occ_transaction_manager.h
 *
 * -------------------------------------------------------------------------
 */

#include "occ_transaction_manager.h"
#include "../utils/utilities.h"
#include "cycles.h"
#include "mot_engine.h"
#include "row.h"
#include "row_header.h"
#include "txn.h"
#include "txn_access.h"
#include "checkpoint_manager.h"
#include "mm_session_api.h"
#include "mot_error.h"
#include <pthread.h>
#include "../../../fdw_adapter/src/mot_internal.h"

namespace MOT {
DECLARE_LOGGER(OccTransactionManager, ConcurrenyControl);

OccTransactionManager::OccTransactionManager()
    : m_txnCounter(0),
      m_abortsCounter(0),
      m_writeSetSize(0),
      m_rowsSetSize(0),
      m_deleteSetSize(0),
      m_insertSetSize(0),
      m_dynamicSleep(100),
      m_rowsLocked(false),
      m_preAbort(true),
      m_validationNoWait(true)
{}

OccTransactionManager::~OccTransactionManager()
{}

bool OccTransactionManager::Init()
{
    bool result = true;
    return result;
}

bool OccTransactionManager::QuickVersionCheck(const Access* access)
{
    // We always validate on commited rows!
    const Row* row = access->GetRowFromHeader();
    return (row->m_rowHeader.GetCSN() == access->m_tid);
}

bool OccTransactionManager::QuickHeaderValidation(const Access* access)
{
    if (access->m_type != INS) {
        // For WR/DEL lets verify CSN
        return QuickVersionCheck(access);
    } else {
        // Lets verify the inserts
        // For upgrade we verify  the row
        // csn has not changed!
        Sentinel* sent = access->m_origSentinel;
        if (access->m_params.IsUpgradeInsert()) {
            if (sent->GetData()->GetCommitSequenceNumber() != access->m_tid) {
                return false;
            }
        } else {
            // if the sent is commited we abort!
            if (sent->IsCommited()) {
                return false;
            }
        }
    }

    return true;
}

bool OccTransactionManager::ValidateReadSet(TxnManager* txMan)
{
    TxnOrderedSet_t& orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    for (const auto& raPair : orderedSet) {
        const Access* ac = raPair.second;
        if (ac->m_type != RD) {
            continue;
        }
        if (!ac->GetRowFromHeader()->m_rowHeader.ValidateRead(ac->m_tid)) {
            return false;
        }
    }

    return true;
}

bool OccTransactionManager::ValidateWriteSet(TxnManager* txMan)
{
    TxnOrderedSet_t& orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    for (const auto& raPair : orderedSet) {
        const Access* ac = raPair.second;
        if (ac->m_type == RD or !ac->m_params.IsPrimarySentinel()) {
            continue;
        }

        if (!ac->GetRowFromHeader()->m_rowHeader.ValidateWrite(ac->m_tid)) {
            return false;
        }
    }
    return true;
}

///

bool OccTransactionManager::ValidateEpoch(TxnManager * txMan){
    MOT_LOG_INFO("进行Epoch验证，如果事务的开始时间，小于行的当前时间戳，则说明该epoch有其他事务更改了这个行，长事务被abort");
        TxnOrderedSet_t &orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    bool result = true;
    for (const auto &raPair : orderedSet)
    {
        const Access *ac = raPair.second;
        if (ac->m_type == RD)
        {
            continue;
        }
        MOT_LOG_INFO("rowEpoch = %lu", ac->GetRowFromHeader()->m_rowHeader.GetEpoch());
        MOT_LOG_INFO("txnEpoch = %lu", txMan->GetStartEpoch());
        if (ac->GetRowFromHeader()->m_rowHeader.GetEpoch() > txMan->GetStartEpoch())
        {
            return false;
        }
        
    }
    MOT_LOG_INFO("长事务未与当前epoch冲突");
    return result;
}

bool OccTransactionManager::ValidateReadInMerge(TxnManager * txMan){
    MOT_LOG_INFO("验证开始在merge期间执行的事务");
    TxnOrderedSet_t &orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    bool result = true;
    for (const auto &raPair : orderedSet)
    {
        const Access *ac = raPair.second;
        if (ac->m_type == RD)
        {
            if (!ac->GetRowFromHeader()->m_rowHeader.ValidateRead(ac->m_cts))
            {
                return false;
            }
        }
    }
    //MOT_LOG_INFO("长事务未与当前epoch冲突");
    return result;
}

bool OccTransactionManager::ValidateAndSetWriteSet(TxnManager *txMan)
{
    MOT_LOG_INFO("验证并更新CSN ");
    TxnOrderedSet_t &orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    bool result = true;
    for (const auto &raPair : orderedSet)
    {
        const Access *ac = raPair.second;
        if (ac->m_type == RD)
        {
            continue;
        }
        if (!ac->GetRowFromHeader()->m_rowHeader.ValidateAndSetWrite(txMan->GetCommitSequenceNumber(), txMan->GetStartEpoch()))
        {
            result = false;
        }
        if(ac->m_type == INS){
            void* key = ac->m_origSentinel;
            uint64_t currentCSN = txMan->GetCommitSequenceNumber();
            auto search = MOTAdaptor::insertSet.find(key);
            if (search != MOTAdaptor::insertSet.end()) {
                uint64_t csn = search->second;
                // Set current csn
                if(currentCSN >= csn){
                    MOTAdaptor::insertSet[key] = currentCSN;
                }
                
            }
            else
            {
                MOTAdaptor::insertSet.insert(std::pair<void*, uint64_t>(key, currentCSN));
            }
        }
        
    }
    return result;
}

bool OccTransactionManager::ValidateWriteSetII(TxnManager *txMan)
{
    TxnOrderedSet_t &orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    for (const auto &raPair : orderedSet)
    {
        const Access *ac = raPair.second;
        if (ac->m_type == RD or !ac->m_params.IsPrimarySentinel())
        {
            continue;
        }

        if (!ac->GetRowFromHeader()->m_rowHeader.ValidateWrite(txMan->GetCommitSequenceNumber()))
        {
            return false;
        }
        if(ac->m_type == INS){
            void* key = ac->m_origSentinel;
            auto search = MOTAdaptor::insertSet.find(key);
            if (search != MOTAdaptor::insertSet.end()) {
                uint64_t csn = search->second;
                MOT_LOG_INFO(("Insert TID:"+ std::to_string(txMan->GetCommitSequenceNumber())).c_str());
                MOT_LOG_INFO(("Insert CSN:"+ std::to_string(csn)).c_str());
                // compare insert csn
                if(txMan->GetCommitSequenceNumber() != csn){
                    return false;
                }
            }
        }
    }
    return true;
}

RC OccTransactionManager::ExecutionPhase(TxnManager *txMan)
{
    TxnAccess *tx = txMan->m_accessMgr.Get();
    RC rc = RC_OK;
    const uint32_t rowCount = tx->m_rowCnt;

    m_writeSetSize = 0;
    m_rowsSetSize = 0;
    m_deleteSetSize = 0;
    m_insertSetSize = 0;
    m_txnCounter++;

    /// the read only transaction, read the snapshot data is ok
    if (rowCount == 0)
    {
        return rc;
    }

    TxnOrderedSet_t &orderedSet = tx->GetOrderedRowSet();
    MOT_ASSERT(rowCount == orderedSet.size());

    for (const auto &raPair : orderedSet)
    {
        const Access *ac = raPair.second;
        if (ac->m_params.IsPrimarySentinel())
        {
            m_rowsSetSize++;
        }
        switch (ac->m_type)
        {
        case WR:
            m_writeSetSize++;
            break;
        case DEL:
            m_writeSetSize++;
            m_deleteSetSize++;
            break;
        case INS:
            m_insertSetSize++;
            m_writeSetSize++;
            break;
        case RD: /// now only support the "READ-COMMITTED" isolation
            break;
        default:
            break;
        }
    }
    MOT_LOG_INFO("Execution Phase, writeSetSize = %lu, rowsSetSize = %lu, deleteSetSize = %lu, insertSetSize = %lu", m_writeSetSize,
                    m_rowsSetSize, m_deleteSetSize, m_insertSetSize);

    bool result = ValidateAndSetWriteSet(txMan);
    if (!result)
    {
        MOT_LOG_INFO("尝试更新CTS失败");
        return RC::RC_ABORT;
    }
    if(!ValidateEpoch(txMan)){
        MOT_LOG_INFO("EPoch验证未通过，长事务被abort");
        return RC::RC_ABORT;
    }
    return RC::RC_OK;
}

//local txn precommit
RC OccTransactionManager::PreCommitPhase(TxnManager *txMan)
{
    MOT_LOG_INFO("PreCommit Phase");
    bool result = ValidateWriteSetII(txMan);
    if (result) //precommit success 
    {
        
        return RC::RC_OK;
    }
    MOT_LOG_INFO("预提交失败");
    return RC::RC_ABORT;
}
// the commit phase in aria
// commit or abort in the phase
RC OccTransactionManager::CommitPhase(TxnManager *txMan)
{
    MOT_LOG_INFO("Commit Phase");
    bool result = ValidateWriteSetII(txMan);
    if (result) //commit writeChange
    {
        return txMan->CommitInternalII();
    }
    MOT_LOG_INFO("最终提交失败");
    return RC::RC_ABORT;
}
///

RC OccTransactionManager::LockRows(TxnManager* txMan, uint32_t& numRowsLock)
{
    RC rc = RC_OK;
    TxnOrderedSet_t& orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    numRowsLock = 0;
    for (const auto& raPair : orderedSet) {
        const Access* ac = raPair.second;
        if (ac->m_type == RD) {
            continue;
        }
        if (ac->m_params.IsPrimarySentinel()) {
            Row* row = ac->GetRowFromHeader();
            row->m_rowHeader.Lock();
            numRowsLock++;
            MOT_ASSERT(row->GetPrimarySentinel()->IsLocked() == true);
        }
    }

    return rc;
}

RC OccTransactionManager::LockHeaders(TxnManager* txMan, uint32_t& numSentinelsLock)
{
    RC rc = RC_OK;
    uint64_t sleepTime = 1;
    uint64_t thdId = txMan->GetThdId();
    TxnOrderedSet_t& orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    numSentinelsLock = 0;
    if (m_validationNoWait) {
        while (numSentinelsLock != m_writeSetSize) {
            for (const auto& raPair : orderedSet) {
                const Access* ac = raPair.second;
                if (ac->m_type == RD) {
                    continue;
                }
                Sentinel* sent = ac->m_origSentinel;
                if (!sent->TryLock(thdId)) {
                    break;
                }
                numSentinelsLock++;
                if (ac->m_params.IsPrimaryUpgrade()) {
                    ac->m_auxRow->m_rowHeader.Lock();
                }
                // New insert row is already commited!
                // Check if row has chainged in sentinel
                if (!QuickHeaderValidation(ac)) {
                    rc = RC_ABORT;
                    goto final;
                }
            }

            if (numSentinelsLock != m_writeSetSize) {
                ReleaseHeaderLocks(txMan, numSentinelsLock);
                numSentinelsLock = 0;
                if (m_preAbort) {
                    for (const auto& acPair : orderedSet) {
                        const Access* ac = acPair.second;
                        if (!QuickHeaderValidation(ac)) {
                            return RC_ABORT;
                        }
                    }
                }
                if (sleepTime > LOCK_TIME_OUT) {
                    rc = RC_ABORT;
                    goto final;
                } else {
                    if (IsHighContention() == false) {
                        CpuCyclesLevelTime::Sleep(5);
                    } else {
                        usleep(m_dynamicSleep);
                    }
                    sleepTime = sleepTime << 1;
                }
            }
        }
    } else {
        for (const auto& raPair : orderedSet) {
            const Access* ac = raPair.second;
            if (ac->m_type == RD) {
                continue;
            }
            Sentinel* sent = ac->m_origSentinel;
            sent->Lock(thdId);
            numSentinelsLock++;
            if (ac->m_params.IsPrimaryUpgrade()) {
                ac->m_auxRow->m_rowHeader.Lock();
            }
            // New insert row is already commited!
            // Check if row has chainged in sentinel
            if (!QuickHeaderValidation(ac)) {
                rc = RC_ABORT;
                goto final;
            }
        }
    }
final:
    return rc;
}

RC OccTransactionManager::ValidateOcc(TxnManager* txMan)
{
    uint32_t numSentinelLock = 0;
    m_rowsLocked = false;
    TxnAccess* tx = txMan->m_accessMgr.Get();
    RC rc = RC_OK;
    const uint32_t rowCount = tx->m_rowCnt;

    m_writeSetSize = 0;
    m_rowsSetSize = 0;
    m_deleteSetSize = 0;
    m_insertSetSize = 0;
    m_txnCounter++;

    if (rowCount == 0) {
        // READONLY
        return rc;
    }

    uint32_t readSetSize = 0;
    TxnOrderedSet_t& orderedSet = tx->GetOrderedRowSet();
    MOT_ASSERT(rowCount == orderedSet.size());
    /* 1.Perform Quick Version check */
    for (const auto& raPair : orderedSet) {
        const Access* ac = raPair.second;
        if (ac->m_params.IsPrimarySentinel()) {
            m_rowsSetSize++;
        }
        switch (ac->m_type) {
            case WR:
                MOT_LOG_INFO("更新");
                m_writeSetSize++;
                break;
            case DEL:
                MOT_LOG_INFO("删除");
                m_writeSetSize++;
                m_deleteSetSize++;
                break;
            case INS:
                MOT_LOG_INFO("插入");
                m_insertSetSize++;
                m_writeSetSize++;
                break;
            case RD:
                MOT_LOG_INFO("读取");
                if (txMan->GetTxnIsoLevel() > READ_COMMITED) {
                    readSetSize++;
                    if (m_preAbort && !QuickVersionCheck(ac)) {
                        rc = RC_ABORT;
                        goto final;
                    }
                }
                break;
            default:
                break;
        }

        if (m_preAbort) {
            if (!QuickHeaderValidation(ac)) {
                rc = RC_ABORT;
                goto final;
            }
        }
    }

    MOT_LOG_DEBUG("Validate OCC rowCnt=%u RD=%u WR=%u\n", tx->m_rowCnt, tx->m_rowCnt - m_writeSetSize, m_writeSetSize);
    rc = LockHeaders(txMan, numSentinelLock);
    if (rc != RC_OK) {
        goto final;
    }

    // validate rows in the read set and write set
    // for repeatable_read, no need to validate the read set.
    if (txMan->GetTxnIsoLevel() > READ_COMMITED) {
        if (!ValidateReadSet(txMan)) {
            rc = RC_ABORT;
            goto final;
        }
    }
    if (!ValidateWriteSet(txMan)) {
        rc = RC_ABORT;
        goto final;
    }

final:
    if (__builtin_expect(rc == RC_ABORT, 0)) {
        ReleaseHeaderLocks(txMan, numSentinelLock);
        m_abortsCounter++;
    } else {
        MOT_ASSERT(numSentinelLock == m_writeSetSize);
        m_rowsLocked = true;
    }

    return rc;
}

void OccTransactionManager::RollbackInserts(TxnManager* txMan)
{
    return txMan->UndoInserts();
}

bool OccTransactionManager::WriteChanges(TxnManager* txMan)
{
    if (m_writeSetSize == 0 && m_insertSetSize == 0) {
        return true;
    }
    //LockRows(txMan, m_rowsSetSize);
    MOTConfiguration& cfg = GetGlobalConfiguration();

    TxnOrderedSet_t& orderedSet = txMan->m_accessMgr->GetOrderedRowSet();
    // Update CSN with all relevant information on global rows
    // For deletes invalidate sentinels - rows still locked!
    for (const auto& raPair : orderedSet) {
        const Access* access = raPair.second;
        access->GetRowFromHeader()->m_rowHeader.WriteChangesToRow(access, txMan->GetCommitSequenceNumber());
    }

    // Treat Inserts
    if (m_insertSetSize > 0) {
        for (const auto& raPair : orderedSet) {
            Access* access = raPair.second;
            if (access->m_type != INS) {
                continue;
            }
            MOT_ASSERT(access->m_origSentinel->IsLocked() == true);
            if (access->m_params.IsUpgradeInsert() == false) {
                if (access->m_params.IsPrimarySentinel()) {
                    MOT_ASSERT(access->m_origSentinel->IsDirty() == true);
                    // Connect row and sentinel, row is set to absent and locked
                    access->m_origSentinel->SetNextPtr(access->GetRowFromHeader());
                    // Current state: row is set to absent,sentinel is locked and not dirty
                    // Readers will not see the row
                    access->GetTxnRow()->GetTable()->UpdateRowCount(1);
                } else {
                    // We only set the in the secondary sentinel!
                    access->m_origSentinel->SetNextPtr(access->GetRowFromHeader()->GetPrimarySentinel());
                }
            } else {
                MOT_ASSERT(access->m_params.IsUniqueIndex() == true);
                // Rows are locked and marked as deleted
                if (access->m_params.IsPrimarySentinel()) {
                    /* Switch the locked row's in the sentinel
                     * The old row is locked and marked deleted
                     * The new row is locked
                     * Save previous row in the access!
                     * We need it for the row release!
                     */
                    Row* row = access->GetRowFromHeader();
                    access->m_localInsertRow = row;
                    access->m_origSentinel->SetNextPtr(access->m_auxRow);
                    // Add row to GC!
                    txMan->GetGcSession()->GcRecordObject(row->GetTable()->GetPrimaryIndex()->GetIndexId(),
                        row,
                        nullptr,
                        row->RowDtor,
                        ROW_SIZE_FROM_POOL(row->GetTable()));
                } else {
                    // Set Sentinel for
                    access->m_origSentinel->SetNextPtr(access->m_auxRow->GetPrimarySentinel());
                }
                // upgrade should not change the reference count!
                access->m_origSentinel->SetUpgradeCounter();
            }
        }
    }

    if (cfg.m_enableCheckpoint) {
        for (const auto& raPair : orderedSet) {
            const Access* access = raPair.second;
            if (access->m_type == RD) {
                continue;
            }
            if (access->m_params.IsPrimarySentinel()) {
                if (!GetCheckpointManager()->ApplyWrite(txMan, access->GetTxnRow(), access->m_type)) {
                    return false;
                }
            }
        }
    }

    // Treat Inserts
    if (m_insertSetSize > 0) {
        for (const auto& raPair : orderedSet) {
            const Access* access = raPair.second;
            if (access->m_type != INS) {
                continue;
            }
            access->m_origSentinel->UnSetDirty();
        }
    }

    if (cfg.m_enableCheckpoint) {
        GetCheckpointManager()->CommitTransaction(txMan, m_rowsSetSize);
    }

    return true;
}

void OccTransactionManager::CleanRowsFromIndexes(TxnManager* txMan)
{
    if (m_deleteSetSize == 0) {
        return;
    }

    TxnAccess* tx = txMan->m_accessMgr.Get();
    TxnOrderedSet_t& orderedSet = tx->GetOrderedRowSet();
    uint32_t numOfDeletes = m_deleteSetSize;
    // use local counter to optimize
    for (const auto& raPair : orderedSet) {
        const Access* access = raPair.second;
        if (access->m_type == DEL) {
            numOfDeletes--;
            access->GetTxnRow()->GetTable()->UpdateRowCount(-1);
            MOT_ASSERT(access->m_params.IsUpgradeInsert() == false);
            // Use Txn Row as row may change INSERT after DELETE leaves residue
            txMan->RemoveKeyFromIndex(access->GetTxnRow(), access->m_origSentinel);
        }
        if (!numOfDeletes) {
            break;
        }
    }
}

void OccTransactionManager::ReleaseHeaderLocks(TxnManager* txMan, uint32_t numOfLocks)
{
    if (numOfLocks == 0) {
        return;
    }

    TxnAccess* tx = txMan->m_accessMgr.Get();
    TxnOrderedSet_t& orderedSet = tx->GetOrderedRowSet();
    // use local counter to optimize
    for (const auto& raPair : orderedSet) {
        const Access* access = raPair.second;
        if (access->m_type == RD) {
            continue;
        } else {
            numOfLocks--;
            access->m_origSentinel->Release();
        }
        if (!numOfLocks) {
            break;
        }
    }
}

void OccTransactionManager::ReleaseRowsLocks(TxnManager* txMan, uint32_t numOfLocks)
{
    if (numOfLocks == 0) {
        return;
    }

    TxnAccess* tx = txMan->m_accessMgr.Get();
    TxnOrderedSet_t& orderedSet = tx->GetOrderedRowSet();

    // use local counter to optimize
    for (const auto& raPair : orderedSet) {
        const Access* access = raPair.second;
        if (access->m_type == RD) {
            continue;
        }

        if (access->m_params.IsPrimarySentinel()) {
            numOfLocks--;
            access->GetRowFromHeader()->m_rowHeader.Release();
            if (access->m_params.IsUpgradeInsert()) {
                // This is the global row that we switched!
                // Currently it's in the gc!
                access->m_localInsertRow->m_rowHeader.Release();
            }
        }
        if (!numOfLocks) {
            break;
        }
    }
}

void OccTransactionManager::CleanUp()
{
    m_writeSetSize = 0;
    m_insertSetSize = 0;
    m_rowsSetSize = 0;
}
}  // namespace MOT
