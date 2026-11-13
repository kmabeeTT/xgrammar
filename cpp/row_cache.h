/*!
 *  Copyright (c) 2025 by Contributors
 * \file xgrammar/row_cache.h
 * \brief Cache of the Earley parser rows a GrammarMatcher reaches while it checks tokens.
 */

#ifndef XGRAMMAR_ROW_CACHE_H_
#define XGRAMMAR_ROW_CACHE_H_

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "support/utils.h"

namespace xgrammar {

/*!
 * \brief The parser rows reached while checking tokens against the grammar, and the byte
 * transitions between them.
 *
 * A row is the scanable states, the rule_id_to_completable_states_ entries and the completion
 * flag of one history position. The parser uses a rule start position only to tell the root and
 * the row being built from other rows, and to read the rule_id_to_completable_states_ entries at
 * that position. A row is therefore stored with every rule start position replaced by what it
 * stands for: the root, the row itself, the id of another cached row, or the id of the entries
 * of a committed history row (a context, stored the same way). Rows with equal stored forms
 * accept the same byte strings, wherever they are in the input.
 */
struct RowCache {
  /*! \brief A rule start position that is the row itself. */
  static constexpr int32_t kSelf = -2;
  /*! \brief A rule start position inside cached row i is stored as kFirstRowRef - i; one inside
   * the committed history as the id of its context, which is not negative. */
  static constexpr int32_t kFirstRowRef = -3;
  /*! \brief A cache that holds about this many bytes is emptied before the next check. */
  static constexpr size_t kMaxBytes = 4 << 20;
  static constexpr size_t kMaxAcceptedUncertainTokens = 1 << 10;
  /*! \brief A check keeps the rows it pushed until it ends, unless they hold more states. */
  static constexpr int32_t kMaxTemporaryStates = 1 << 12;
  /*! \brief A row or context with more states gets an id of its own without being stored. */
  static constexpr int32_t kMaxKeyStates = 64;

  struct RowInfo {
    /*! \brief Whether the grammar can end at the row. */
    bool completed = false;
    /*! \brief The bytes the row is known to reject. */
    std::bitset<256> rejected_bytes;
    /*! \brief The bytes the row is known to accept, with the id of the row after each. */
    std::vector<std::pair<uint8_t, int32_t>> next_rows;
    /*! \brief The history row that holds the states, if `check` is the running check. */
    int32_t history_row = -1;
    uint64_t check = 0;
    /*! \brief For a row that holds one state: the ids of the uncertain tokens of the state's
     * mask that the row accepts, once a check has covered all of them and found no more than
     * kMaxAcceptedUncertainTokens. */
    bool has_accepted_uncertain_tokens = false;
    std::vector<int32_t> accepted_uncertain_tokens;
  };

  /*! \brief Stored form of a row -> its id. Ids are given in order of first appearance. */
  std::unordered_map<std::vector<int32_t>, int32_t> row_ids;
  /*! \brief Indexed by row id. */
  std::vector<RowInfo> row_infos;
  /*! \brief Stored form of the entries of a committed history row -> the id of the context. */
  std::unordered_map<std::vector<int32_t>, int32_t> context_ids;
  int32_t num_contexts = 0;
  /*! \brief The number of checks started; identifies the running check. */
  uint64_t num_checks = 0;
  /*! \brief An estimate of the memory held, kept up to date by the code that adds entries. */
  size_t num_bytes = 0;

  /*! \brief The bytes a map entry with a key of the given length takes, roughly. */
  static size_t KeyBytes(size_t key_size) { return key_size * sizeof(int32_t) + 64; }
};

}  // namespace xgrammar

#endif  // XGRAMMAR_ROW_CACHE_H_
