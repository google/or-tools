// Copyright 2010-2018 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Reversible (i.e Backtrackable) classes, used to simplify coding propagators.
#ifndef OR_TOOLS_UTIL_REV_H_
#define OR_TOOLS_UTIL_REV_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {

// Interface for reversible objects used to maintain them in sync with a tree
// search organized by decision levels.
class ReversibleInterface {
 public:
  ReversibleInterface() {}
  virtual ~ReversibleInterface() {}

  // Initially a reversible class starts at level zero. Increasing the level
  // saves the state of the current old level. Decreasing the level restores the
  // state to what it was at this level and all higher levels are forgotten.
  // Everything done at level zero cannot be backtracked over.
  //
  // The level is assumed to be non-negative.
  virtual void SetLevel(int level) = 0;
};

// A repository that maintains a set of reversible objects of type T.
// This is meant to be used for small types that are efficient to copy, like
// all the basic types, std::pair and things like this.
template <class T>
class RevRepository : public ReversibleInterface {
 public:
  RevRepository() : stamp_(0) {}

  // This works in O(level_diff) on level increase.
  // For level decrease, it is in O(level_diff + num_restored_states).
  void SetLevel(int level) final;
  int Level() const { return end_of_level_.size(); }

  // Saves the given object value for the current level. If this is called
  // multiple time by level, only the value of the first call matter. This is
  // NOT optimized for many calls by level and should mainly be used just once
  // for a given level. If a client cannot do that efficiently, it can use the
  // SaveStateWithStamp() function below.
  void SaveState(T* object) {
    if (end_of_level_.empty()) return;  // Not useful for level zero.
    stack_.push_back({object, *object});
  }

  // Calls SaveState() if the given stamp is not the same as the current one.
  // This also sets the given stamp to the current one. The current stamp is
  // maintained by this class and is updated on each level changes. The whole
  // process make sure that only one SaveValue() par level will ever be called,
  // so it is efficient to call this before each update to the object T.
  void SaveStateWithStamp(T* object, int64* stamp) {
    if (*stamp == stamp_) return;
    *stamp = stamp_;
    SaveState(object);
  }

 private:
  int64 stamp_;
  std::vector<int> end_of_level_;  // In stack_.

  // TODO(user): If we ever see this in any cpu profile, consider using two
  // vectors for a better memory packing in case sizeof(T) is not sizeof(T*).
  std::vector<std::pair<T*, T>> stack_;
};

// A basic reversible vector implementation.
template <class IndexType, class T>
class RevVector : public ReversibleInterface {
 public:
  const T& operator[](IndexType index) const { return vector_[index]; }

  // TODO(user): Maybe we could have also used the [] operator, but it is harder
  // to be 100% sure that the mutable version is only called when we modify
  // the vector. And I had performance bug because of that.
  T& MutableRef(IndexType index) {
    // Save on the stack first.
    if (!end_of_level_.empty()) stack_.push_back({index, vector_[index]});
    return vector_[index];
  }

  int size() const { return vector_.size(); }

  void Grow(int new_size) {
    CHECK_GE(new_size, vector_.size());
    vector_.resize(new_size);
  }

  void GrowByOne() { vector_.resize(vector_.size() + 1); }

  int Level() const { return end_of_level_.size(); }

  void SetLevel(int level) final {
    DCHECK_GE(level, 0);
    if (level == Level()) return;
    if (level < Level()) {
      const int index = end_of_level_[level];
      end_of_level_.resize(level);  // Shrinks.
      for (int i = stack_.size() - 1; i >= index; --i) {
        vector_[stack_[i].first] = stack_[i].second;
      }
      stack_.resize(index);
    } else {
      end_of_level_.resize(level, stack_.size());  // Grows.
    }
  }

 private:
  std::vector<int> end_of_level_;  // In stack_.
  std::vector<std::pair<IndexType, T>> stack_;
  absl::StrongVector<IndexType, T> vector_;
};

template <class T>
void RevRepository<T>::SetLevel(int level) {
  DCHECK_GE(level, 0);
  if (level == Level()) return;
  ++stamp_;
  if (level < Level()) {
    const int index = end_of_level_[level];
    end_of_level_.resize(level);  // Shrinks.
    for (int i = stack_.size() - 1; i >= index; --i) {
      *stack_[i].first = stack_[i].second;
    }
    stack_.resize(index);
  } else {
    end_of_level_.resize(level, stack_.size());  // Grows.
  }
}

// Like a normal map but support backtrackable operations.
//
// This works on any class "Map" that supports: begin(), end(), find(), erase(),
// insert(), key_type, value_type, mapped_type and const_iterator.
template <class Map>
class RevMap : ReversibleInterface {
 public:
  typedef typename Map::key_type key_type;
  typedef typename Map::mapped_type mapped_type;
  typedef typename Map::value_type value_type;
  typedef typename Map::const_iterator const_iterator;

  // Backtracking support: changes the current "level" (always non-negative).
  //
  // Initially the class starts at level zero. Increasing the level works in
  // O(level diff) and saves the state of the current old level. Decreasing the
  // level restores the state to what it was at this level and all higher levels
  // are forgotten. Everything done at level zero cannot be backtracked over.
  void SetLevel(int level) final;
  int Level() const { return first_op_index_of_next_level_.size(); }

  bool ContainsKey(key_type key) const { return gtl::ContainsKey(map_, key); }
  const mapped_type& FindOrDie(key_type key) const {
    return gtl::FindOrDie(map_, key);
  }

  void EraseOrDie(key_type key);
  void Set(key_type key, mapped_type value);  // Adds or overwrites.

  // Wrapper to the underlying const map functions.
  int size() const { return map_.size(); }
  bool empty() const { return map_.empty(); }
  const_iterator find(const key_type& k) const { return map_.find(k); }
  const_iterator begin() const { return map_.begin(); }
  const_iterator end() const { return map_.end(); }

 private:
  Map map_;

  // The operation that needs to be performed to reverse one modification:
  // - If is_deletion is true, then we need to delete the entry with given key.
  // - Otherwise we need to add back (or overwrite) the saved entry.
  struct UndoOperation {
    bool is_deletion;
    key_type key;
    mapped_type value;
  };

  // TODO(user): We could merge the operations with the same key from the same
  // level. Investigate and implement if this is worth the effort for our use
  // case.
  std::vector<UndoOperation> operations_;
  std::vector<int> first_op_index_of_next_level_;
};

template <class Map>
void RevMap<Map>::SetLevel(int level) {
  DCHECK_GE(level, 0);
  if (level < Level()) {
    const int backtrack_level = first_op_index_of_next_level_[level];
    first_op_index_of_next_level_.resize(level);  // Shrinks.
    while (operations_.size() > backtrack_level) {
      const UndoOperation& to_undo = operations_.back();
      if (to_undo.is_deletion) {
        map_.erase(to_undo.key);
      } else {
        map_.insert({to_undo.key, to_undo.value}).first->second = to_undo.value;
      }
      operations_.pop_back();
    }
    return;
  }

  // This is ok even if level == Level().
  first_op_index_of_next_level_.resize(level, operations_.size());  // Grows.
}

template <class Map>
void RevMap<Map>::EraseOrDie(key_type key) {
  const auto iter = map_.find(key);
  if (iter == map_.end()) LOG(FATAL) << "key not present: '" << key << "'.";
  if (Level() > 0) {
    operations_.push_back({false, key, iter->second});
  }
  map_.erase(iter);
}

template <class Map>
void RevMap<Map>::Set(key_type key, mapped_type value) {
  auto insertion_result = map_.insert({key, value});
  if (Level() > 0) {
    if (insertion_result.second) {
      // It is an insertion. Undo = delete.
      operations_.push_back({true, key});
    } else {
      // It is a modification. Undo = change back to old value.
      operations_.push_back({false, key, insertion_result.first->second});
    }
  }
  insertion_result.first->second = value;
}

// A basic backtrackable multi map that can only grow (except on backtrack).
template <class Key, class Value>
class RevGrowingMultiMap : ReversibleInterface {
 public:
  void SetLevel(int level) final;

  // Adds a new value at the given key.
  void Add(Key key, Value value);

  // Returns the list of values for a given key (can be empty).
  const std::vector<Value>& Values(Key key) const;

 private:
  std::vector<Value> empty_values_;

  // TODO(user): use inlined vectors. Another datastructure that may be more
  // efficient is to use a linked list inside added_keys_ for the values sharing
  // the same key.
  absl::flat_hash_map<Key, std::vector<Value>> map_;

  // Backtracking data.
  std::vector<Key> added_keys_;
  std::vector<int> first_added_key_of_next_level_;
};

template <class Key, class Value>
void RevGrowingMultiMap<Key, Value>::SetLevel(int level) {
  DCHECK_GE(level, 0);
  if (level < first_added_key_of_next_level_.size()) {
    const int backtrack_level = first_added_key_of_next_level_[level];
    first_added_key_of_next_level_.resize(level);  // Shrinks.
    while (added_keys_.size() > backtrack_level) {
      auto it = map_.find(added_keys_.back());
      if (it->second.size() > 1) {
        it->second.pop_back();
      } else {
        map_.erase(it);
      }
      added_keys_.pop_back();
    }
    return;
  }

  // This is ok even if level == Level().
  first_added_key_of_next_level_.resize(level, added_keys_.size());  // Grows.
}

template <class Key, class Value>
const std::vector<Value>& RevGrowingMultiMap<Key, Value>::Values(
    Key key) const {
  const auto it = map_.find(key);
  if (it != map_.end()) return it->second;
  return empty_values_;
}

template <class Key, class Value>
void RevGrowingMultiMap<Key, Value>::Add(Key key, Value value) {
  if (!first_added_key_of_next_level_.empty()) {
    added_keys_.push_back(key);
  }
  map_[key].push_back(value);
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_REV_H_
