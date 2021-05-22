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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_IDS_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_IDS_VALIDATOR_H_

#include <stdint.h>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/core/model_summary.h"

namespace operations_research {
namespace math_opt {

absl::Status CheckIdsNonnegativeAndStrictlyIncreasing(
    absl::Span<const int64_t> ids);

// Checks that the elements of ids are a subset of universe.
//
// Assumed: ids and universe are sorted in increasing order, repeats allowed.
absl::Status CheckSortedIdsSubset(const absl::Span<const int64_t> ids,
                                  const absl::Span<const int64_t> universe);

// Checks that the elements of ids are a subset of universe.
//
// Assumed: universe are sorted in strictly increasing order (no repeats). No
// assumptions on ids.
absl::Status CheckUnsortedIdsSubset(const absl::Span<const int64_t> ids,
                                    const absl::Span<const int64_t> universe);

// Checks that the elements of ids are a subset of universe. Elements of ids
// do not need to be sorted or distinct.
absl::Status CheckIdsSubset(absl::Span<const int64_t> ids,
                            const IdNameBiMap& universe,
                            absl::string_view ids_description,
                            absl::string_view universe_description);

// first_ids and second_ids must include distinct ids.
absl::Status CheckIdsIdentical(absl::Span<const int64_t> first_ids,
                               const IdNameBiMap& second_ids,
                               absl::string_view first_description,
                               absl::string_view second_description);

// Provides a unified view of the id sets:
//   * NOT_DELETED = old - deleted
//   * FINAL = old - deleted + new
// so users can validate if a list of ids (sorted or unsorted) is a subset of
// either of the sets above.
//
// Implementation note: this class does not allocate by default, but some
// functions will allocate at most O(#deleted + #new).
class IdUpdateValidator {
 public:
  // deleted_ids and new_ids must be sorted with unique strictly increasing
  // entries.
  IdUpdateValidator(const IdNameBiMap& old_ids,
                    const absl::Span<const int64_t> deleted_ids,
                    const absl::Span<const int64_t> new_ids)
      : old_ids_(old_ids), deleted_ids_(deleted_ids), new_ids_(new_ids) {}

  absl::Status IsValid() const;

  // Checks that ids is a subset of NOT_DELETED = old_ids_ - deleted_ids_.
  //
  // ids must be sorted in increasing order (repeats are allowed).
  absl::Status CheckSortedIdsSubsetOfNotDeleted(
      const absl::Span<const int64_t> ids) const;

  // Checks that ids is a subset of FINAL = old_ids_ - deleted_ids_ + new_ids_.
  //
  // ids must be sorted in increasing order (repeats are allowed).
  absl::Status CheckSortedIdsSubsetOfFinal(
      const absl::Span<const int64_t> ids) const;

  // Checks that ids is a subset of FINAL = old_ids_ - deleted_ids_ + new_ids_.
  //
  // If ids is sorted, prefer CheckSortedIdsSubsetOfFinal.
  absl::Status CheckIdsSubsetOfFinal(const absl::Span<const int64_t> ids) const;

 private:
  // NOT OWNED
  const IdNameBiMap& old_ids_;
  absl::Span<const int64_t> deleted_ids_;
  absl::Span<const int64_t> new_ids_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_IDS_VALIDATOR_H_
