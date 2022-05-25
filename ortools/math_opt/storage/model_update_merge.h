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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_MODEL_UPDATE_MERGE_H_
#define OR_TOOLS_MATH_OPT_STORAGE_MODEL_UPDATE_MERGE_H_

#include "ortools/math_opt/model_update.pb.h"

namespace operations_research::math_opt {

// Merges the `from_new` update into the `into_old` one.
//
// The `from_new` update must represent an update that happens after the
// `into_old` one is applied. Thus when the two updates have overlaps, the
// `from_new` one overrides the value of the `into_old` one (i.e. the `from_new`
// update is expected to be more recent).
//
// This function also CHECKs that the ids of new variables and constraints in
// `from_new` are greater than the ones in `into_old` (as expected if `from_new`
// happens after `into_old`).
//
// Note that the complexity is O(size(from_new) + size(into_old)) thus if you
// need to merge a long list of updates this may be not efficient enough. In
// that case an n-way merge would be needed to be implemented here.
void MergeIntoUpdate(const ModelUpdateProto& from_new,
                     ModelUpdateProto& into_old);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_MODEL_UPDATE_MERGE_H_
