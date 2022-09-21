// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_VARIABLE_STORAGE_H_
#define OR_TOOLS_MATH_OPT_STORAGE_VARIABLE_STORAGE_H_

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/range.h"

namespace operations_research::math_opt {

// The in memory representation of the variables of an optimization model.
//
// The setter functions all accept a `DiffIter`, which must be an iterator over
// non-const references to VariableStorage::Diff. These
// functions will modify the VariableStorage::Diff objects.
class VariableStorage {
 public:
  // Tracks a "checkpoint" and the changes to variables that are before the
  // checkpoint. Advancing the checkpoint throws away tracked changes.
  //
  // An instance of this class is owned by each update tracker of ModelStorage.
  struct Diff {
    explicit Diff(const VariableStorage& storage)
        : checkpoint(storage.next_id()) {}

    VariableId checkpoint = VariableId(0);
    absl::flat_hash_set<VariableId> deleted;
    absl::flat_hash_set<VariableId> lower_bounds;
    absl::flat_hash_set<VariableId> upper_bounds;
    absl::flat_hash_set<VariableId> integer;
  };

  // A description of the changes to the variables of the model with respect to
  // a known checkpoint.
  struct UpdateResult {
    // Variables before the checkpoint that have been deleted.
    google::protobuf::RepeatedField<int64_t> deleted;
    // Variables before the checkpoint that have been modified and not deleted.
    VariableUpdatesProto updates;
    // Variables created at or after the checkpoint that have not been deleted.
    VariablesProto creates;
  };

  // Adds a variable to the model and returns its id.
  //
  // The returned ids begin at zero and strictly increase (in particular, if
  // ensure_next_id_at_least() is not used, they will be consecutive). Deleted
  // ids are NOT reused.
  VariableId Add(double lower_bound, double upper_bound, bool is_integer,
                 absl::string_view name);

  inline double lower_bound(VariableId id) const;
  inline double upper_bound(VariableId id) const;
  inline bool is_integer(VariableId id) const;
  inline const std::string& name(VariableId id) const;

  template <typename DiffIter>
  void set_lower_bound(VariableId id, double lower_bound,
                       const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_upper_bound(VariableId id, double upper_bound,
                       const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_integer(VariableId id, bool is_integer,
                   const iterator_range<DiffIter>& diffs);

  // Removes a variable from the model.
  //
  // It is an error to use a deleted variable id as input to any subsequent
  // function calls on this.
  template <typename DiffIter>
  void Delete(VariableId id, const iterator_range<DiffIter>& diffs);

  // The number of variables in the model.
  //
  // Equal to the number of variables created minus the number of variables
  // deleted.
  inline int64_t size() const;

  // The returned id of the next call to AddVariable.
  //
  // Equal to the number of variables created.
  inline VariableId next_id() const;

  // Sets the next variable id to be the maximum of next_id() and `minimum`.
  inline void ensure_next_id_at_least(VariableId minimum);

  // Returns true if this id has been created and not yet deleted.
  inline bool contains(VariableId id) const;

  // The VariableIds in use (not deleted), order not defined.
  std::vector<VariableId> Variables() const;

  // Returns a sorted vector of all existing (not deleted) variables in the
  // model.
  //
  // Runs in O(n log(n)), where n is the number of variables returned.
  std::vector<VariableId> SortedVariables() const;

  // An equivalent proto of `this`.
  VariablesProto Proto() const;

  //////////////////////////////////////////////////////////////////////////////
  // Functions for working with Diff
  //////////////////////////////////////////////////////////////////////////////

  // Returns true if there are no changes (tracked changes before the checkpoint
  // or new constraints after the checkpoint).
  inline bool diff_is_empty(const Diff& diff) const;

  UpdateResult Update(const Diff& diff) const;

  // Updates the checkpoint and clears all stored changes in diff.
  void AdvanceCheckpointInDiff(Diff& diff) const;

  // Returns the variables in the model starting with start (inclusive) and
  // larger in order. Runs in O(next_id() - start).
  std::vector<VariableId> VariablesFrom(VariableId start) const;

 private:
  struct VariableData {
    double lower_bound = -std::numeric_limits<double>::infinity();
    double upper_bound = std::numeric_limits<double>::infinity();
    bool is_integer = false;
    std::string name = "";
  };

  void AppendVariable(VariableId variable, VariablesProto* proto) const;

  VariableId next_variable_id_ = VariableId(0);
  absl::flat_hash_map<VariableId, VariableData> variables_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline functions
////////////////////////////////////////////////////////////////////////////////

double VariableStorage::lower_bound(const VariableId id) const {
  return variables_.at(id).lower_bound;
}

double VariableStorage::upper_bound(const VariableId id) const {
  return variables_.at(id).upper_bound;
}

bool VariableStorage::is_integer(VariableId id) const {
  return variables_.at(id).is_integer;
}

const std::string& VariableStorage::name(const VariableId id) const {
  return variables_.at(id).name;
}

template <typename DiffIter>
void VariableStorage::set_lower_bound(VariableId id, double lower_bound,
                                      const iterator_range<DiffIter>& diffs) {
  VariableData& data = variables_.at(id);
  if (data.lower_bound == lower_bound) {
    return;
  }
  data.lower_bound = lower_bound;
  for (Diff& diff : diffs) {
    if (id < diff.checkpoint) {
      diff.lower_bounds.insert(id);
    }
  }
}

template <typename DiffIter>
void VariableStorage::set_upper_bound(VariableId id, double upper_bound,
                                      const iterator_range<DiffIter>& diffs) {
  VariableData& data = variables_.at(id);
  if (data.upper_bound == upper_bound) {
    return;
  }
  data.upper_bound = upper_bound;
  for (Diff& diff : diffs) {
    if (id < diff.checkpoint) {
      diff.upper_bounds.insert(id);
    }
  }
}

template <typename DiffIter>
void VariableStorage::set_integer(VariableId id, bool is_integer,
                                  const iterator_range<DiffIter>& diffs) {
  VariableData& data = variables_.at(id);
  if (data.is_integer == is_integer) {
    return;
  }
  data.is_integer = is_integer;
  for (Diff& diff : diffs) {
    if (id < diff.checkpoint) {
      diff.integer.insert(id);
    }
  }
}

template <typename DiffIter>
void VariableStorage::Delete(VariableId id,
                             const iterator_range<DiffIter>& diffs) {
  for (Diff& diff : diffs) {
    if (id >= diff.checkpoint) {
      continue;
    }
    diff.deleted.insert(id);
    diff.lower_bounds.erase(id);
    diff.upper_bounds.erase(id);
    diff.integer.erase(id);
  }
  variables_.erase(id);
}

int64_t VariableStorage::size() const { return variables_.size(); }

VariableId VariableStorage::next_id() const { return next_variable_id_; }

void VariableStorage::ensure_next_id_at_least(VariableId minimum) {
  next_variable_id_ = std::max(minimum, next_variable_id_);
}

bool VariableStorage::contains(const VariableId id) const {
  return variables_.contains(id);
}

bool VariableStorage::diff_is_empty(const Diff& diff) const {
  return next_variable_id_ <= diff.checkpoint && diff.deleted.empty() &&
         diff.lower_bounds.empty() && diff.upper_bounds.empty() &&
         diff.integer.empty();
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_VARIABLE_STORAGE_H_
