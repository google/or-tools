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

// Functions to remove names from models and updates.
//
// Input models can contain duplicated names which MathOpt solvers will
// refuse. The functions in this header can be use to mitigate that.
//
// These functions can also be used to anonymize models before saving them.
#ifndef OR_TOOLS_MATH_OPT_IO_NAMES_REMOVAL_H_
#define OR_TOOLS_MATH_OPT_IO_NAMES_REMOVAL_H_

#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research::math_opt {

// Removes the model, variables and constraints names of the provided model.
void RemoveNames(ModelProto& model);

// Removes the variables and constraints names of the provided update.
void RemoveNames(ModelUpdateProto& update);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_IO_NAMES_REMOVAL_H_
