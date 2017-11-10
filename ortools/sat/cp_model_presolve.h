// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
#define OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_

#include <vector>

#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

// Presolves the given CpModelProto into presolved_model.
//
// This also creates a mapping model that encode the correspondence between the
// two problems. This works as follow:
// - The first variables of mapping_model are in one to one correspondence with
//   the variables of the initial model.
// - The presolved_model variables are in one to one correspondence with the
//   variable at the indices given by postsolve_mapping in the mapping model.
// - Fixing one of the two sets of variables and solving the model will assign
//   the other set to a feasible solution of the other problem. Moreover, the
//   objective value of these solutions will be the same. Note that solving such
//   problems will take little time in practice because the propagation will
//   basically do all the work.
//
// Note(user): an optimization model can be transformed into a decision problem,
// if for instance the objective is fixed, or independent from the rest of the
// problem.
//
// TODO(user): Identify disconnected components and returns a vector of
// presolved model? If we go this route, it may be nicer to store the indices
// inside the model. We can add a IntegerVariableProto::initial_index;
void PresolveCpModel(const CpModelProto& initial_model,
                     CpModelProto* presolved_model, CpModelProto* mapping_model,
                     std::vector<int>* postsolve_mapping);

// Replaces all the instance of a variable i (and the literals referring to it)
// by mapping[i]. The definition of variables i is also moved to its new index.
// Variables with a negative mapping value are ignored and it is an error if
// such variable is referenced anywhere (this is CHECKed).
//
// The image of the mapping should be dense in [0, new_num_variables), this is
// also CHECKed.
void ApplyVariableMapping(const std::vector<int>& mapping, CpModelProto* proto);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
