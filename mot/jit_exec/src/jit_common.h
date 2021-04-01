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
 * jit_common.h
 *    Definitions used both by LLVM and TVM jitted code.
 *
 * IDENTIFICATION
 *    src/gausskernel/storage/mot/jit_exec/src/jit_common.h
 *
 * -------------------------------------------------------------------------
 */

#ifndef JIT_COMMON_H
#define JIT_COMMON_H

#include "jit_helpers.h"
#include "jit_pgproc.h"
#include "jit_context.h"

#include "mot_engine.h"

// This file contains definitions used both by LLVM and TVM jitted code
namespace JitExec {
// common helpers for code generation
/** @brief Converts a PG command type to LLVM command type. */
extern JitCommandType ConvertCommandType(int pgCommandType, bool isPKey);

/** @brief Converts an LLVM command type to string form. */
extern const char* CommandToString(JitCommandType commandType);

/** @breif Queries whether an LLVM command requires a range scan. */
extern bool IsRangeCommand(JitCommandType commandType);

/** @breif Queries whether an LLVM command requires a JOIN scan. */
extern bool IsJoinCommand(JitCommandType commandType);

/** @brief Extract table from query. */
extern MOT::Table* GetTableFromQuery(const Query* query);

/** @brief Maps a table column id to an index column id. */
extern int MapTableColumnToIndex(MOT::Table* table, const MOT::Index* index, int columnId);

/** @brief builds a map from table column ids to index column ids. */
extern int BuildColumnMap(MOT::Table* table, MOT::Index* index, int** columnMap);

/** @brief Builds an array of index column offsets. */
extern int BuildIndexColumnOffsets(MOT::Table* table, const MOT::Index* index, int** offsets);

/** @brief Queries whether a PG type is supported by MOT tables. */
extern bool IsTypeSupported(int resultType);

/** @brief Queries whether a WHERE clause operator is supported. */
extern bool IsWhereOperatorSupported(int whereOp);

/** @brief Classifies a WHERE clause operator. */
extern JitWhereOperatorClass ClassifyWhereOperator(int whereOp);

/** @brief Queries whether the WHERE clause represents a full-prefix search. */
extern bool IsFullPrefixSearch(const int* columnArray, int columnCount, int* firstZeroColumn);

/** @brief Retrieves the sort order of a query if specified. */
extern JitQuerySortOrder GetQuerySortOrder(const Query* query);

/** @brief Retrieves the type of the aggregate column. */
extern int GetAggregateTupleColumnIdAndType(const Query* query, int& zeroType);

/** @struct Compile-time table information. */
struct TableInfo {
    /** @var The table used for the query. */
    MOT::Table* m_table;

    /** @var The index used for the query. */
    MOT::Index* m_index;

    /** @var Map from table column id to index column id. */
    int* m_columnMap;

    /** @var Offsets of index columns. */
    int* m_indexColumnOffsets;

    /** @var The number of entries in the column map. */
    int m_tableColumnCount;

    /** @var The number of entries in the offset array. */
    int m_indexColumnCount;
};

/**
 * @brief Initializes compile-time table information structure.
 * @param table_info The table information structure.
 * @param table The table to be examined.
 * @param index The index to be examined.
 * @return True if succeeded, otherwise false.
 */
extern bool InitTableInfo(TableInfo* table_info, MOT::Table* table, MOT::Index* index);

/**
 * @brief Cleanup table information.
 * @param table_info The table information structure to cleanup.
 */
extern void DestroyTableInfo(TableInfo* table_info);
}  // namespace JitExec

#endif
