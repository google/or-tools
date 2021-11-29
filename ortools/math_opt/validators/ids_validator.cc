// Copyright 2010-2021 Google LLC
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

#include "ortools/math_opt/validators/ids_validator.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

namespace {
absl::Status CheckSortedIdsSubsetWithIndexOffset(
    const absl::Span<const int64_t> ids,
    const absl::Span<const int64_t> universe, const int64_t offset) {
  int id_index = 0;
  int universe_index = 0;
  // NOTE(user): in the common case where ids and/or universe is consecutive,
  // we can avoid iterating though the list and do interval based checks.
  while (id_index < ids.size() && universe_index < universe.size()) {
    if (universe[universe_index] < ids[id_index]) {
      ++universe_index;
    } else if (universe[universe_index] == ids[id_index]) {
      ++id_index;
    } else {
      break;
    }
  }
  if (id_index < ids.size()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Bad id: ", ids[id_index],
                     " (at index: ", id_index + offset, ") found"));
  }
  return absl::OkStatus();
}

// Given a sorted, strictly increasing set of ids, provides `contains()` to
// check if another id is in the set in O(1) time.
//
// Implementation note: when ids are consecutive, they are stored as a single
// interval [lb, ub), otherwise they are stored as a hash table of integers.
class FastIdCheck {
 public:
  // ids must be sorted with unique strictly increasing entries.
  explicit FastIdCheck(const absl::Span<const int64_t> ids) {
    if (ids.empty()) {
      interval_mode_ = true;
    } else if (ids.size() == ids.back() + 1 - ids.front()) {
      interval_mode_ = true;
      interval_lb_ = ids.front();
      interval_ub_ = ids.back() + 1;
    } else {
      ids_ = absl::flat_hash_set<int64_t>(ids.begin(), ids.end());
    }
  }
  bool contains(int64_t id) const {
    if (interval_mode_) {
      return id >= interval_lb_ && id < interval_ub_;
    } else {
      return ids_.contains(id);
    }
  }

 private:
  bool interval_mode_ = false;
  int64_t interval_lb_ = 0;
  int64_t interval_ub_ = 0;
  absl::flat_hash_set<int64_t> ids_;
};

// Checks that the elements of ids and bad_list have no overlap.
//
// Assumed: ids and bad_list are sorted in increasing order, repeats allowed.
absl::Status CheckSortedIdsNotBad(const absl::Span<const int64_t> ids,
                                  const absl::Span<const int64_t> bad_list) {
  int id_index = 0;
  int bad_index = 0;
  while (id_index < ids.size() && bad_index < bad_list.size()) {
    if (bad_list[bad_index] < ids[id_index]) {
      ++bad_index;
    } else if (bad_list[bad_index] > ids[id_index]) {
      ++id_index;
    } else {
      return absl::InvalidArgumentError(absl::StrCat(
          "Bad id: ", ids[id_index], " (at index: ", id_index, ") found."));
    }
  }
  return absl::OkStatus();
}
}  // namespace

absl::Status CheckIdsNonnegativeAndStrictlyIncreasing(
    absl::Span<const int64_t> ids) {
  int64_t previous{-1};
  for (int i = 0; i < ids.size(); ++i) {
    if (ids[i] <= previous) {
      std::string error_base = absl::StrCat(
          "Expected ids to be nonnegative and strictly increasing, but at "
          "index ",
          i, ", found id: ", ids[i]);
      if (i == 0) {
        return absl::InvalidArgumentError(
            absl::StrCat(error_base, " (a negative id)"));
      } else {
        return absl::InvalidArgumentError(absl::StrCat(
            error_base, " and at index ", i - 1, " found id: ", ids[i - 1]));
      }
    }
    previous = ids[i];
  }
  return absl::OkStatus();
}

absl::Status CheckSortedIdsSubset(const absl::Span<const int64_t> ids,
                                  const absl::Span<const int64_t> universe) {
  RETURN_IF_ERROR(CheckSortedIdsSubsetWithIndexOffset(ids, universe, 0));
  return absl::OkStatus();
}

absl::Status CheckUnsortedIdsSubset(const absl::Span<const int64_t> ids,
                                    const absl::Span<const int64_t> universe) {
  if (ids.empty()) {
    return absl::OkStatus();
  }
  const FastIdCheck id_check(universe);
  for (int i = 0; i < ids.size(); ++i) {
    if (!id_check.contains(ids[i])) {
      return absl::InvalidArgumentError(
          absl::StrCat("Bad id: ", ids[i], " (at index: ", i, ") not found."));
    }
  }
  return absl::OkStatus();
}

absl::Status IdUpdateValidator::IsValid() const {
  for (int i = 0; i < deleted_ids_.size(); ++i) {
    const int64_t deleted_id = deleted_ids_[i];
    if (!old_ids_.HasId(deleted_id)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Tried to delete id: ", deleted_id, " (at index: ", i,
                       ") but it was not present."));
    }
  }
  if (old_ids_.Empty() || new_ids_.empty()) {
    return absl::OkStatus();
  }
  if (old_ids_.LargestId() >= new_ids_.front()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "All old ids should be less than all new ids, but final old id was: ",
        old_ids_.LargestId(), " and first new id was: ", new_ids_.front()));
  }
  return absl::OkStatus();
}

absl::Status IdUpdateValidator::CheckSortedIdsSubsetOfNotDeleted(
    const absl::Span<const int64_t> ids) const {
  RETURN_IF_ERROR(CheckSortedIdsNotBad(ids, deleted_ids_)) << " was deleted";
  for (int i = 0; i < ids.size(); ++i) {
    if (!old_ids_.HasId(ids[i])) {
      return absl::InvalidArgumentError(
          absl::StrCat("Bad id: ", ids[i], " (at index: ", i, ") not found."));
    }
  }
  return absl::OkStatus();
}

absl::Status IdUpdateValidator::CheckSortedIdsSubsetOfFinal(
    const absl::Span<const int64_t> ids) const {
  // Implementation:
  //  * Partition ids into "old" and "new"
  //  * Check that the old ids are in old_ids_ but not deleted_ids_.
  //  * Check that the new ids are in new_ids_.
  size_t split_point = ids.size();
  if (!new_ids_.empty()) {
    split_point = std::distance(
        ids.begin(), std::lower_bound(ids.begin(), ids.end(), new_ids_[0]));
  }
  RETURN_IF_ERROR(
      CheckSortedIdsSubsetOfNotDeleted(ids.subspan(0, split_point)));
  RETURN_IF_ERROR(CheckSortedIdsSubsetWithIndexOffset(ids.subspan(split_point),
                                                      new_ids_, split_point));
  return absl::OkStatus();
}

// Checks that ids is a subset of FINAL = old_ids_ - deleted_ids_ + new_ids_.
//
// If ids is sorted, prefer CheckSortedIdsSubsetOfFinal.
absl::Status IdUpdateValidator::CheckIdsSubsetOfFinal(
    const absl::Span<const int64_t> ids) const {
  if (ids.empty()) {
    return absl::OkStatus();
  }
  const FastIdCheck deleted_fast(deleted_ids_);
  const FastIdCheck new_fast(new_ids_);
  for (int i = 0; i < ids.size(); ++i) {
    const int64_t id = ids[i];
    if (id <= old_ids_.LargestId()) {
      if (!old_ids_.HasId(id)) {
        return absl::InvalidArgumentError(
            absl::StrCat("Bad id: ", id, " (at index: ", i, ") not found."));
      } else if (deleted_fast.contains(id)) {
        return absl::InvalidArgumentError(
            absl::StrCat("Bad id: ", id, " (at index: ", i, ") was deleted."));
      }
    } else if (!new_fast.contains(id)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Bad id: ", id, " (at index: ", i, ") not found."));
    }
  }
  return absl::OkStatus();
}

absl::Status CheckIdsSubset(absl::Span<const int64_t> ids,
                            const IdNameBiMap& universe,
                            absl::string_view ids_description,
                            absl::string_view universe_description) {
  for (int i = 0; i < ids.size(); ++i) {
    const int64_t id = ids[i];
    if (!universe.HasId(id)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Id: ", id, " (at index: ", i, ") in ", ids_description,
                       " is missing from ", universe_description, "."));
    }
  }
  return absl::OkStatus();
}

absl::Status CheckIdsIdentical(absl::Span<const int64_t> first_ids,
                               const IdNameBiMap& second_ids,
                               absl::string_view first_description,
                               absl::string_view second_description) {
  if (first_ids.size() != second_ids.Size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        first_description, " has size ", first_ids.size(), ", but ",
        second_description, " has size ", second_ids.Size(), "."));
  }
  RETURN_IF_ERROR(CheckIdsSubset(first_ids, second_ids, first_description,
                                 second_description));
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
