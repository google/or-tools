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

#ifndef OR_TOOLS_MATH_OPT_CORE_MODEL_SUMMARY_H_
#define OR_TOOLS_MATH_OPT_CORE_MODEL_SUMMARY_H_

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/log/check.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research {
namespace math_opt {

// Maintains a bidirectional mapping between names and ids, e.g. as used for
// variables and linear constraints.
//
// The following invariants are enforced:
//  * Ids must be unique and increasing (in insertion order).
//  * Ids are non-negative.
//  * Ids are not equal to std::numeric_limits<int64_t>::max()
//  * Ids removed are never reused.
//  * Names must be either empty or unique when built with check_names=true.
class IdNameBiMap {
 public:
  // If check_names=false, the names need not be unique and the reverse mapping
  // of name to id is not available.
  explicit IdNameBiMap(bool check_names = true)
      : nonempty_name_to_id_(
            check_names ? std::make_optional<
                              absl::flat_hash_map<absl::string_view, int64_t>>()
                        : std::nullopt) {}

  // Needs a custom copy constructor/assign because absl::string_view to
  // internal data is held as a member. No custom move is needed.
  IdNameBiMap(const IdNameBiMap& other);
  IdNameBiMap& operator=(const IdNameBiMap& other);
  IdNameBiMap(IdNameBiMap&& other) = default;
  IdNameBiMap& operator=(IdNameBiMap&& other) = default;

  // This constructor CHECKs that the input ids are sorted in increasing
  // order. This constructor is expected to be used only for unit tests of
  // validation code.
  IdNameBiMap(std::initializer_list<std::pair<int64_t, absl::string_view>> ids);

  // Inserts the provided id and associate the provided name to it.
  //
  // An error is returned if:
  //   * id is negative
  //   * id is not at least next_free_id()
  //   * id is max(int64_t)
  //   * name is a duplicated and check_names was true at construction.
  //
  // As a side effect it updates next_free_id to id + 1.
  inline absl::Status Insert(int64_t id, std::string name);

  // Removes the given id, or returns an error if id is not present.
  inline absl::Status Erase(int64_t id);

  inline bool HasId(int64_t id) const;

  // Will always return false if name is empty or if check_names was false.
  inline bool HasName(absl::string_view name) const;
  inline bool Empty() const;
  inline int Size() const;

  // The next id that has never been used (0 initially since ids are
  // non-negative).
  inline int64_t next_free_id() const;

  // Updates next_free_id(). Succeeds when the provided id is greater than every
  // exiting id and is non-negative.
  inline absl::Status SetNextFreeId(int64_t new_next_free_id);

  // Iteration order is in increasing id order.
  const gtl::linked_hash_map<int64_t, std::string>& id_to_name() const {
    return id_to_name_;
  }

  // Is std::nullopt if check_names was false at construction.
  const std::optional<absl::flat_hash_map<absl::string_view, int64_t>>&
  nonempty_name_to_id() const {
    return nonempty_name_to_id_;
  }

  // Warning: this may be mutated (partially updated) if an error is returned.
  absl::Status BulkUpdate(absl::Span<const int64_t> deleted_ids,
                          absl::Span<const int64_t> new_ids,
                          absl::Span<const std::string* const> names);

 private:
  // Next unused id.
  int64_t next_free_id_ = 0;

  // Pointer stability for name strings and iterating in insertion order are
  // both needed (so we do not use flat_hash_map).
  gtl::linked_hash_map<int64_t, std::string> id_to_name_;
  std::optional<absl::flat_hash_map<absl::string_view, int64_t>>
      nonempty_name_to_id_;
};

// TODO(b/232619901): In the guide for how to add new constraints, include how
// this class must updated.
struct ModelSummary {
  explicit ModelSummary(bool check_names = true);
  static absl::StatusOr<ModelSummary> Create(const ModelProto& model,
                                             bool check_names = true);
  absl::Status Update(const ModelUpdateProto& model_update);

  IdNameBiMap variables;
  std::string primary_objective_name;
  IdNameBiMap auxiliary_objectives;
  IdNameBiMap linear_constraints;
  IdNameBiMap quadratic_constraints;
  IdNameBiMap sos1_constraints;
  IdNameBiMap sos2_constraints;
  IdNameBiMap indicator_constraints;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

absl::Status IdNameBiMap::Insert(const int64_t id, std::string name) {
  if (id < next_free_id_) {
    return util::InvalidArgumentErrorBuilder()
           << "expected id=" << id
           << " to be at least next_free_id_=" << next_free_id_
           << " (ids should be nonnegative and inserted in strictly increasing "
              "order)";
  }
  if (id == std::numeric_limits<int64_t>::max()) {
    return absl::InvalidArgumentError("id of max(int64_t) is not allowed");
  }
  next_free_id_ = id + 1;

  const auto [it, success] = id_to_name_.emplace(id, std::move(name));
  CHECK(success);  // CHECK is okay, we have the invariant that next_free_id_ is
                   // larger than everything in the map.
  const absl::string_view name_view(it->second);
  if (nonempty_name_to_id_.has_value() && !name_view.empty()) {
    const auto [it, success] = nonempty_name_to_id_->insert({name_view, id});
    if (!success) {
      return util::InvalidArgumentErrorBuilder()
             << "duplicate name inserted: " << name_view;
    }
  }
  return absl::OkStatus();
}

absl::Status IdNameBiMap::Erase(const int64_t id) {
  const auto it = id_to_name_.find(id);
  if (it == id_to_name_.end()) {
    return util::InvalidArgumentErrorBuilder()
           << "cannot delete missing id " << id;
  }
  const absl::string_view name_view(it->second);
  if (nonempty_name_to_id_.has_value() && !name_view.empty()) {
    // CHECK OK, name_view being in nonempty_name_to_id_ when the above is met
    // is a class invariant.
    CHECK_EQ(nonempty_name_to_id_->erase(name_view), 1)
        << "name: " << name_view << " id: " << id;
  }
  id_to_name_.erase(it);
  return absl::OkStatus();
}

bool IdNameBiMap::HasId(const int64_t id) const {
  return id_to_name_.contains(id);
}
bool IdNameBiMap::HasName(const absl::string_view name) const {
  if (name.empty()) {
    return false;
  }
  if (!nonempty_name_to_id_.has_value()) {
    return false;
  }
  return nonempty_name_to_id_->contains(name);
}

bool IdNameBiMap::Empty() const { return id_to_name_.empty(); }

int IdNameBiMap::Size() const { return id_to_name_.size(); }

int64_t IdNameBiMap::next_free_id() const { return next_free_id_; }

absl::Status IdNameBiMap::SetNextFreeId(const int64_t new_next_free_id) {
  if (!Empty()) {
    const int64_t largest_id = id_to_name_.back().first;
    if (new_next_free_id <= largest_id) {
      return util::InvalidArgumentErrorBuilder()
             << "new_next_free_id=" << new_next_free_id
             << " must be greater than largest_id=" << largest_id;
    }
  } else {
    if (new_next_free_id < 0) {
      return util::InvalidArgumentErrorBuilder()
             << "new_next_free_id=" << new_next_free_id
             << " must be nonnegative";
    }
  }
  next_free_id_ = new_next_free_id;
  return absl::OkStatus();
}

namespace internal {

// TODO(b/232526223): this is an exact copy of
// CheckIdsRangeAndStrictlyIncreasing from ids_validator.h, find a way to share
// the code.
// NOTE: This function is only exposed in the header because we need
//       UpdateBiMapFromMappedData here for testing purposes.
absl::Status CheckIdsRangeAndStrictlyIncreasing2(absl::Span<const int64_t> ids);

// NOTE: This is only exposed in the header for testing purposes.
template <typename DataProto>
absl::Status UpdateBiMapFromMappedData(
    const absl::Span<const int64_t> deleted_ids,
    const google::protobuf::Map<int64_t, DataProto>& proto_map,
    IdNameBiMap& bimap) {
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing2(deleted_ids))
      << "invalid deleted ids";
  for (const int64_t id : deleted_ids) {
    RETURN_IF_ERROR(bimap.Erase(id));
  }
  std::vector<int64_t> new_ids;
  new_ids.reserve(proto_map.size());
  for (const auto& [id, value] : proto_map) {
    new_ids.push_back(id);
  }
  absl::c_sort(new_ids);
  for (const int64_t id : new_ids) {
    RETURN_IF_ERROR(bimap.Insert(id, proto_map.at(id).name()));
  }
  return absl::OkStatus();
}

}  // namespace internal

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_MODEL_SUMMARY_H_
