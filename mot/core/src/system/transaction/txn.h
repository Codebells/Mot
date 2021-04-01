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
 * txn.h
 *    Transaction manager used to manage the life cycle of a single transaction.
 *
 * IDENTIFICATION
 *    src/gausskernel/storage/mot/core/src/system/transaction/txn.h
 *
 * -------------------------------------------------------------------------
 */

#ifndef MOT_TXN_H
#define MOT_TXN_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <unordered_map>

#include "global.h"
#include "redo_log.h"
#include "occ_transaction_manager.h"
#include "session_context.h"
#include "surrogate_key_generator.h"
#include "mm_gc_manager.h"
#include "bitmapset.h"
#include "txn_ddl_access.h"
#include "mm_session_api.h"

namespace MOT {
class MOTContext;
class IndexIterator;
class CheckpointManager;
class AsyncRedoLogHandler;

class Sentinel;
class TxnInsertAction;
class TxnAccess;
class InsItem;
class LoggerTask;
class Key;
class Index;

/**
 * @class TxnManager
 * @brief Transaction manager is used to manage the life cycle of a single
 * transaction. It is a pooled object, meaning it is allocated from a NUMA-aware
 * memory pool.
 */
class alignas(64) TxnManager {
public:
    /** @brief Destructor. */
    ~TxnManager();

    /**
     * @brief Constructor.
     * @param session_context The owning session's context.
     */
    TxnManager(SessionContext* sessionContext);

    /**
     * @brief Initializes the object
     * @param threadId The logical identifier of the thread executing this transaction.
     * @param connectionId The logical identifier of the connection executing this transaction.
     * @return Boolean value denoting success or failure.
     */
    bool Init(uint64_t threadId, uint64_t connectionId, bool isLightTxn = false);
     RC CommitInternalII();
    ///
    bool isOnlyRead();
    ///
    /**
     * @brief Override placement-new operator.
     * @param size Unused.
     * @param ptr The pointer to the memory buffer.
     * @return The pointer to the memory buffer.
     */
    static inline __attribute__((always_inline)) void* operator new(size_t size, void* ptr) noexcept
    {
        (void)size;
        return ptr;
    }

    /**
     * @brief Retrieves the logical identifier of the thread in which the
     * transaction is being executed.
     * @return The thread identifier.
     */
    inline uint64_t GetThdId() const
    {
        return m_threadId;
    }  // Attention: this may be invalid in thread-pooled envelope!

    inline uint64_t GetConnectionId() const
    {
        return m_connectionId;
    }

    inline uint64_t GetCommitSequenceNumber() const
    {
        return m_csn;
    }

    inline void SetCommitSequenceNumber(uint64_t commitSequenceNumber)
    {
        m_csn = commitSequenceNumber;
    }

    inline uint64_t GetTransactionId() const
    {
        return m_transactionId;
    }

    inline void SetTransactionId(uint64_t transactionId)
    {
        m_transactionId = transactionId;
    }

    inline uint64_t GetInternalTransactionId() const
    {
        return m_internalTransactionId;
    }

    inline void SetInternalTransactionId(uint64_t transactionId)
    {
        m_internalTransactionId = transactionId;
    }

    void RemoveTableFromStat(Table* t);

    /**
     * @brief Start transaction, set transaction id and isolation level.
     * @return Result code denoting success or failure.
     */
    RC StartTransaction(uint64_t transactionId, int isolationLevel);

    /**
     * @brief Commits the transaction (performs OCC validation).
     * @return Result code denoting success or failure.
     */
    RC Commit();
    RC Commit(uint64_t transcationId);
    RC LiteCommit(uint64_t transcationId);
    
    
  
    RC ValidateOcc(uint64_t transcationId);
    bool localMergeValidate(uint64_t csn);
    /**
     * @brief End transaction, release locks and perform cleanups.
     * @detail Finishes a transaction. This method is automatically called after
     * commit and is not necessary to be called manually. It propagates the
     * changes to the redo log and prepares this object to execute another
     * transaction.
     * @return Result code denoting success or failure.
     */
    RC EndTransaction();

    /**
     * @brief Commits the prepared transaction in 2PC (performs OCC last validation phase).
     * @return Result code denoting success or failure.
     */
    RC CommitPrepared();
    RC CommitPrepared(TransactionId transactionId);
    RC LiteCommitPrepared(TransactionId transactionId);

    /**
     * @brief Prepare the transaction in 2PC (performs OCC first validation phase).
     * @return Result code denoting success or failure.
     */
    inline RC Prepare()
    {
        // Run only first validation phase
        return Prepare(INVALID_TRANSACTIOIN_ID);
    }
    RC Prepare(TransactionId transactionId);
    RC LitePrepare(TransactionId transactionId);

    /**
     * @brief Rolls back the transaction. No changes are propagated to the logs.
     * @return Return code denoting the execution result.
     */
    inline RC Rollback()
    {
        return Rollback(INVALID_TRANSACTIOIN_ID);
    }

    inline RC Rollback(TransactionId transactionId)
    {
        if (transactionId != INVALID_TRANSACTIOIN_ID)
            m_transactionId = transactionId;
        return RollbackInternal(false);
    }

    RC LiteRollback(TransactionId transactionId);

    /**
     * @brief Rollback the prepared transaction in 2PC.
     * @return Result code denoting success or failure.
     */
    inline RC RollbackPrepared()
    {
        return RollbackPrepared(INVALID_TRANSACTIOIN_ID);
    }

    inline RC RollbackPrepared(TransactionId transactionId)
    {
        if (transactionId != INVALID_TRANSACTIOIN_ID)
            m_transactionId = transactionId;
        return RollbackInternal(true);
    }

    RC LiteRollbackPrepared(TransactionId transactionId);

    /**
     * @brief Handle CommitPrepared failure - keep the rows locked until gsclean releases them.
     * @return Result code denoting success or failure.
     */
    RC FailedCommitPrepared(uint64_t transcationId);

    /**
     * @brief Rolls back the transaction. No changes are propagated to the logs.
     */
    void CleanTxn();

    /**
     * @brief Retrieves the owning session context.
     * @return The session context.
     */
    inline SessionContext* GetSessionContext()
    {
        return m_sessionContext;
    }

    /**
     * @brief Inserts a row and updates all affected indices.
     * @param row The row to insert.
     * @param insItem The affected index keys.
     * @param insItemSize The number of affected index keys.
     * @return Return code denoting the execution result.
     */
    RC InsertRow(Row* row);

    InsItem* GetNextInsertItem(Index* index = nullptr);
    Key* GetTxnKey(Index* index);

    void DestroyTxnKey(Key* key) const
    {
        MemSessionFree(key);
    }

    /**
     * @brief Delete a row
     * @return Return code denoting the execution result.
     */
    RC RowDel();

    /**
     * @brief Searches for a row in the local cache by a primary key.
     * @detail The row is searched in the local cache and if not found then the
     * primary index of the table is consulted. In such a case the found row is
     * stored in the local cache for subsequent searches.
     * @param table The table in which the row is to be searched.
     * @param type The purpose for retrieving the row.
     * @param currentKey The primary key by which to search the row.
     * @return The row or null pointer if none was found.
     */
    Row* RowLookupByKey(Table* const& table, const AccessType type, MOT::Key* const currentKey);

    /**
     * @brief Searches for a row in the local cache by a row.
     * @detail Rows may be updated concurrently, so the cache layer needs to be
     * consulted to retrieves the most recent version of a row.
     * @param table The table in which the row is to be searched.
     * @param type The purpose for retrieving the row.
     * @param originalRow The original row to search for an updated version of it.
     * @return The cached row or the original row if none was found in the cache (in
     * which case the original row is stored in the cache for subsequent searches).
     */
    Row* RowLookup(const AccessType type, Sentinel* const& originalSentinel, RC& rc);

    /**
     * @brief Searches for a row in the local cache by a row.
     * @detail Rows may be updated concurrently, so the cache layer needs to be
     * consulted to retrieves the most recent version of a row.
     * @param table The table in which the row is to be searched.
     * @param type The purpose for retrieving the row.
     * @param originalRow The original row to search for an updated version of it.
     * @return The cached row or the original row if none was found in the cache (in
     * which case the original row is stored in the cache for subsequent searches).
     */
    RC AccessLookup(const AccessType type, Sentinel* const& originalSentinel, Row*& localRow);

    /**
     * @brief Searches in the cache for the row following a secondary index item.
     * @param table The table in which the row is to be searched.
     * @param type The purpose for retrieving the row.
     * @param[in,out] curr_item The secondary index item. The secondary index
     * item is advanced to the next item.
     * @return The cached row or the original row stored in the secondary index
     * item if none was found in the cache (in which case the original row is
     * stored in the cache for subsequent searches).
     */
    inline Row* RowLookupSecondaryNextSame(Table* table, AccessType type, void*& curr_item);

    RC DeleteLastRow();

    /**
     * @brief Updates the state of the last row accessed in the local cache.
     * @param state The new row state.
     * @return Return code denoting the execution result.
     */
    RC UpdateLastRowState(AccessType state);

    void CommitSecondaryItems();

    Row* RemoveRow(Row* row);

    Row* RemoveKeyFromIndex(Row* row, Sentinel* sentinel);

    void UndoInserts();

    RC RollbackInsert(Access* ac);
    void RollbackSecondaryIndexInsert(Index* index);
    void RollbackDDLs();

    RC OverwriteRow(Row* updatedRow, BitmapSet& modifiedColumns);

    /**
     * @brief Updates the value in a single field in a row.
     * @param row The row to update.
     * @param attr_id The column identifier.
     * @param attr_value The new field value.
     */
    void UpdateRow(Row* row, const int attr_id, double attr_value);
    void UpdateRow(Row* row, const int attr_id, uint64_t attr_value);

    /**
     * @brief Retrieves the latest epoch seen by this transaction.
     * @return The latest epoch.
     */
    inline uint64_t GetLatestEpoch() const
    {
        return m_latestEpoch;
    }

    /**
     * @brief Retrieves the transaction state.
     * @return The transaction state.
     */
    inline TxnState GetTxnState() const
    {
        return m_state;
    }

    /**
     * @brief Sets the transaction state.
     */
    void SetTxnState(TxnState envelopeState);

    /**
     * @brief Retrieves the transaction isolation level.
     * @return The transaction isolation level.
     */
    inline int GetTxnIsoLevel() const
    {
        return m_isolationLevel;
    }

    /**
     * @brief Sets the transaction isolation level.
     */
    void SetTxnIsoLevel(int envelopeIsoLevel);

    inline void IncStmtCount()
    {
        m_internalStmtCount++;
    }

    inline uint32_t GetStmtCount() const
    {
        return m_internalStmtCount;
    }

    uint64_t GetSurrogateKey()
    {
        return m_surrogateGen.GetSurrogateKey(m_connectionId);
    }

    uint64_t GetSurrogateCount() const
    {
        return m_surrogateGen.GetCurrentCount();
    }

    inline void GcSessionStart()
    {
        m_gcSession->GcStartTxn();
    }

    void GcSessionRecordRcu(
        uint32_t index_id, void* object_ptr, void* object_pool, DestroyValueCbFunc cb, uint32_t obj_size);

    inline void GcSessionEnd()
    {
        m_gcSession->GcEndTxn();
    }

    inline void GcAddSession()
    {
        if (m_isLightSession == true) {
            return;
        }
        m_gcSession->AddToGcList();
    }

    inline void GcRemoveSession()
    {
        if (m_isLightSession == true) {
            return;
        }
        m_gcSession->GcCleanAll();
        m_gcSession->RemoveFromGcList(m_gcSession);
        m_gcSession->~GcManager();
        MemSessionFree(m_gcSession);
        m_gcSession = nullptr;
    }
    void RedoWriteAction(bool isCommit);

    inline GcManager* GetGcSession()
    {
        return m_gcSession;
    }

    inline bool IsFailedCommitPrepared() const
    {
        return m_failedCommitPrepared;
    }

    inline void SetFailedCommitPrepared(bool value)
    {
        m_failedCommitPrepared = value;
    }
    //设置时间戳
    void SetStartTS(uint64_t timestamp)
    {
        startts = timestamp;
    }
    uint64_t GetStartTS()
    {
        return startts;
    }

    void SetCommitTS(uint64_t timestamp)
    {
        committs = timestamp;
    }
    uint64_t GetCommitTS()
    {
        return committs;
    }
    ////
    void SetStartEpoch(uint64_t epoch)
    {
        startEpoch = epoch;
    }
    uint64_t GetStartEpoch()
    {
        return startEpoch;
    }
    void SetStartInMerge(bool inMerge){
        startInMerge = inMerge;
    }
    bool GetStartInMerge(){
        return startInMerge;
    }
    ////

private:
    static constexpr uint32_t SESSION_ID_BITS = 32;

    /**
     * @brief Apply all transactional DDL changes. DDL changes are not handled
     * by occ.
     */
    void WriteDDLChanges();

    /**
     * @brief Reclaims all resources associated with the transaction and
     * prepares this object to execute another transaction.
     */
    inline void Cleanup();

    /**
     * @brief Reset redo log or allocate new one
     */
    inline void RedoCleanup();

    /**
     * @brief Internal commit (used by commit and commitPrepared)
     */
    RC CommitInternal();
    RC RollbackInternal(bool isPrepared);

    // Disable class level new operator
    /** @cond EXCLUDE_DOC */
    void* operator new(std::size_t size) = delete;
    void* operator new(std::size_t size, const std::nothrow_t& tag) = delete;
    /** @endcond */

    /**
     * @brief Enqueues prepared rows/ddls to be later release by gs_clean
     * @param none
     * @return Boolean value denoting success or failure.
     */
    RC SavePreparedData();

    // allow privileged access
    friend TxnInsertAction;
    friend TxnAccess;
    friend class MOTContext;
    friend void benchmarkLoggerTask(LoggerTask*, TxnManager*);
    friend class CheckpointManager;
    friend class AsyncRedoLogHandler;
    friend class RedoLog;
    friend class OccTransactionManager;
    friend class SessionContext;

public:
    Table* GetTableByExternalId(uint64_t id);
    Index* GetIndexByExternalId(uint64_t table_id, uint64_t index_id);
    Index* GetIndex(uint64_t table_id, uint16_t position);
    Index* GetIndex(Table* table, uint16_t position);
    RC CreateTable(Table* table);
    RC DropTable(Table* table);
    RC CreateIndex(Table* table, Index* index, bool is_primary);
    RC DropIndex(Index* index);
    RC TruncateTable(Table* table);

private:
    /** @var The latest epoch seen. */
    uint64_t m_latestEpoch;

    /** @vat The logical identifier of the thread executing this transaction. */
    uint64_t m_threadId;

    /** @vat The logical identifier of the connection executing this transaction. */
    uint64_t m_connectionId;

    /** @var Owning session context. */
    SessionContext* m_sessionContext;

    /** @var The redo log. */
    RedoLog m_redoLog;

    /** @var Concurrency control. */
    OccTransactionManager m_occManager;

    GcManager* m_gcSession;

    /** @var Checkpoint phase captured during transaction start. */
    CheckpointPhase m_checkpointPhase;

    /** @var Checkpoint not available capture during being transaction. */
    bool m_checkpointNABit;

    /** @var CSN taken at the commit stage. */
    uint64_t m_csn;

    /** @var transaction_id Provided by envelop on start transaction. */
    uint64_t m_transactionId;

    /** @var serogate_counter Promotes every insert
     transaction. */
    SurrogateKeyGenerator m_surrogateGen;

    bool m_flushDone;

    /** @var internal_transaction_id Generated by txn_manager. */
    /** It is a concatenation of session_id and a counter */
    uint64_t m_internalTransactionId;

    uint32_t m_internalStmtCount;

    /** @brief Transaction state.
     * this state only reflects the envelope states to avoid invalid transitions
     * Transaction state machine is managed by the envelope!
     */
    TxnState m_state;

    int m_isolationLevel;

    bool m_failedCommitPrepared;
    /** @var timestamp for start and commit of the transaction. */
    uint64_t startts;
    uint64_t committs;
    uint64_t startEpoch;
    bool startInMerge;

public:
    /** @var Transaction cache (OCC optimization). */
    MemSessionPtr<TxnAccess> m_accessMgr;

    TxnDDLAccess* m_txnDdlAccess;

    MOT::Key* m_key;

    bool m_isLightSession;

    /** @var In case of unique violation this will be set to a violating index */
    Index* m_errIx;

    /** @var In case of error this will contain the exact error code */
    RC m_err;

    char m_errMsgBuf[1024];

    /** @var holds query states from MOTAdaptor */
    std::unordered_map<uint64_t, uint64_t> m_queryState;
};
}  // namespace MOT

#endif  // MOT_TXN_H
