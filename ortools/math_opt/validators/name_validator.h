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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_NAME_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_NAME_VALIDATOR_H_

#include <string>

#include "absl/status/status.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"

namespace operations_research {
namespace math_opt {

// Checks basic validity of name_vector view: i.e. ids_size() = values_size().
// In addition, if check_unique is set to true, the function checks that every
// name that is not "" is distinct.
absl::Status CheckNameVector(
    const SparseVectorView<const std::string*>& name_vector, bool check_unique);

// Checks new_names are compatible with old_names: i.e. new_names does not
// duplicate names in old_names. Assumes basic validity of new_names view and
// does not check for duplicates within old_names or new_names.
absl::Status CheckNewNames(
    const IdNameBiMap& old_names,
    const SparseVectorView<const std::string*>& new_names);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_NAME_VALIDATOR_H_
