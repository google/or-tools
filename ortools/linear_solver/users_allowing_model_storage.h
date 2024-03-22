// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_LINEAR_SOLVER_USERS_ALLOWING_MODEL_STORAGE_H_
#define OR_TOOLS_LINEAR_SOLVER_USERS_ALLOWING_MODEL_STORAGE_H_

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"

namespace operations_research {
// List of *exact* MDB users who agreed that we store their MIP/LP/math
// (anonymized) models.
// IMPORTANT: The MDB user has to match exactly with an item in this list: we
// don't do ACL expansion, regexp matching or anything alike.
const absl::flat_hash_set<absl::string_view>& UsersAllowingModelStorage();
}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_USERS_ALLOWING_MODEL_STORAGE_H_
