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

#include "ortools/math_opt/validators/name_validator.h"

#include <cstdint>
#include <string>

#include "ortools/base/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace math_opt {

absl::Status CheckNameVector(
    const SparseVectorView<const std::string*>& name_vector,
    const bool check_unique) {
  if (name_vector.values().empty()) {
    // Names are optional.
    return absl::OkStatus();
  }
  RETURN_IF_ERROR(CheckIdsAndValuesSize(name_vector, "names"));
  absl::flat_hash_map<absl::string_view, int64_t> used_variable_names;
  if (check_unique) {
    for (const auto [id, name_pointer] : name_vector) {
      const std::string& name = *name_pointer;
      if (!name.empty()) {
        if (!gtl::InsertIfNotPresent(&used_variable_names, {name, id})) {
          return absl::InvalidArgumentError(
              absl::StrCat("Found name: ", name, " twice, for ids ", id,
                           " and ", used_variable_names.at(name)));
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::Status CheckNewNames(
    const IdNameBiMap& old_names,
    const SparseVectorView<const std::string*>& new_names) {
  if (old_names.Empty()) {
    return absl::OkStatus();
  }
  for (const auto [id, name_pointer] : new_names) {
    const std::string& new_name = *name_pointer;
    if (!new_name.empty() && old_names.HasName(new_name)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Found name: ", new_name, " twice, for ids ", id,
                       " and ", old_names.nonempty_name_to_id().at(new_name)));
    }
  }
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
