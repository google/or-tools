// Copyright 2010-2014 Google
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

#include "base/logging.h"
#include "base/map_util.h"

namespace operations_research {

// Like a normal map but support backtrackable operations.
//
// This works on any class "Map" that supports: begin(), end(), find(), erase(),
// insert(), key_type, value_type, mapped_type and const_iterator.
template <class Map>
class RevMap {
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
  void SetLevel(int level);
  int Level() const { return first_op_index_of_next_level_.size(); }

  bool ContainsKey(key_type key) const { return operations_research::ContainsKey(map_, key); }
  const mapped_type& FindOrDie(key_type key) const {
    return operations_research::FindOrDie(map_, key);
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

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_REV_H_
