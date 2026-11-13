/*!
 *  Copyright (c) 2025 by Contributors
 * \file xgrammar/row_cache.h
 * \brief Storage of the parser rows a GrammarMatcher reaches while checking uncertain tokens.
 */

#ifndef XGRAMMAR_ROW_CACHE_H_
#define XGRAMMAR_ROW_CACHE_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace xgrammar {

struct AdaptiveTokenMask;

/*! \brief Open-addressing map from uint64_t keys to int32_t values. The all-ones key is reserved. */
class FlatUInt64Map {
 public:
  int32_t* Find(uint64_t key) {
    if (keys_.empty()) {
      return nullptr;
    }
    for (size_t slot = Mix(key) & mask_;; slot = (slot + 1) & mask_) {
      if (keys_[slot] == key) {
        return &values_[slot];
      }
      if (keys_[slot] == kEmpty) {
        return nullptr;
      }
    }
  }

  /*! \brief Insert a key that is not in the map. */
  void Insert(uint64_t key, int32_t value) {
    if ((size_ + 1) * 2 > keys_.size()) {
      Grow();
    }
    InsertNoGrow(key, value);
    ++size_;
  }

  void Clear() {
    keys_.clear();
    values_.clear();
    size_ = 0;
    mask_ = 0;
  }

 private:
  static constexpr uint64_t kEmpty = ~static_cast<uint64_t>(0);

  static size_t Mix(uint64_t key) {
    key *= 0x9E3779B97F4A7C15ull;
    return static_cast<size_t>(key ^ (key >> 29));
  }

  void InsertNoGrow(uint64_t key, int32_t value) {
    size_t slot = Mix(key) & mask_;
    while (keys_[slot] != kEmpty) {
      slot = (slot + 1) & mask_;
    }
    keys_[slot] = key;
    values_[slot] = value;
  }

  void Grow() {
    std::vector<uint64_t> old_keys = std::move(keys_);
    std::vector<int32_t> old_values = std::move(values_);
    const size_t capacity = old_keys.empty() ? 64 : old_keys.size() * 2;
    keys_.assign(capacity, kEmpty);
    values_.assign(capacity, 0);
    mask_ = capacity - 1;
    for (size_t i = 0; i < old_keys.size(); ++i) {
      if (old_keys[i] != kEmpty) {
        InsertNoGrow(old_keys[i], old_values[i]);
      }
    }
  }

  std::vector<uint64_t> keys_;
  std::vector<int32_t> values_;
  size_t size_ = 0;
  size_t mask_ = 0;
};

/*! \brief Hash of a stored form; never the reserved key of FlatUInt64Map. */
inline uint64_t RowCacheHash(const int32_t* data, size_t size) {
  uint64_t hash = 0x9E3779B97F4A7C15ull;
  size_t i = 0;
  for (; i + 2 <= size; i += 2) {
    uint64_t two_values;
    std::memcpy(&two_values, data + i, sizeof(two_values));
    hash = (hash ^ two_values) * 0xFF51AFD7ED558CCDull;
    hash ^= hash >> 32;
  }
  if (i < size) {
    hash = (hash ^ static_cast<uint32_t>(data[i])) * 0xFF51AFD7ED558CCDull;
    hash ^= hash >> 32;
  }
  return hash & ~(static_cast<uint64_t>(1) << 63);
}

/*! \brief A parser row in position-independent form. */
struct RowCacheNode {
  /*! \brief The stored form: a span of RowCacheStore::node_keys. */
  int32_t key_begin;
  int32_t key_size;
  /*! \brief The next node with the same hash, or -1. */
  int32_t next_with_same_hash;
  /*! \brief A history row holding this node's states during the check numbered history_row_check_id. */
  int32_t history_row;
  uint64_t history_row_check_id;
};

/*! \brief The content of a committed rule_id_to_completable_states_ row, positions replaced
 * as in a node. */
struct RowCacheContext {
  int32_t key_begin;
  int32_t key_size;
  int32_t next_with_same_hash;
  /*! \brief A committed row that had this content when it was last seen. */
  int32_t history_row;
};

/*!
 * \brief The result of the check for the uncertain tokens that share a prefix, from the node
 * reached by that prefix. The tokens are uncertain_indices[list_begin, list_end) of `mask`, and
 * share their first prefix_size bytes.
 */
struct RowCacheRun {
  int32_t node;
  int32_t list_begin;
  int32_t prefix_size;
  const AdaptiveTokenMask* mask;
  int32_t list_end;
  /*! \brief The accepted token ids: a span of RowCacheStore::run_accepted. */
  int32_t accepted_begin;
  int32_t accepted_size;
  int32_t next_with_same_hash;
};

/*!
 * \brief Everything the row cache of a matcher knows. Nothing in it depends on the matcher or on
 * positions in its input, so a store filled by one matcher of a compiled grammar is valid for
 * any other matcher of the same compiled grammar.
 */
struct RowCacheStore {
  std::vector<RowCacheNode> nodes;
  std::vector<int32_t> node_keys;
  /*! \brief Hash of a stored form -> the first node with that hash. */
  FlatUInt64Map node_by_hash;
  /*! \brief (node << 8 | byte) -> the node after the byte, or -1 if the byte is rejected. */
  FlatUInt64Map transitions;
  std::vector<RowCacheContext> contexts;
  std::vector<int32_t> context_keys;
  FlatUInt64Map context_by_hash;
  std::vector<RowCacheRun> runs;
  std::vector<int32_t> run_accepted;
  FlatUInt64Map run_by_hash;
  /*! \brief The number of checks made on the store; identifies the running check. */
  uint64_t check_id = 0;

  void Clear() {
    nodes.clear();
    node_keys.clear();
    node_by_hash.Clear();
    transitions.Clear();
    contexts.clear();
    context_keys.clear();
    context_by_hash.Clear();
    runs.clear();
    run_accepted.clear();
    run_by_hash.Clear();
  }
};

}  // namespace xgrammar

#endif  // XGRAMMAR_ROW_CACHE_H_
