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

#ifndef OR_TOOLS_SAT_CP_MODEL_SYMMETRIES_H_
#define OR_TOOLS_SAT_CP_MODEL_SYMMETRIES_H_

#include <limits>
#include <memory>
#include <vector>

#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

// Returns a list of generators of the symmetry group of the given problem. Each
// generator is a permutation of the integer range [0, n) where n is the number
// of variables of the problem. They are permutations of the (index
// representation of the) problem variables.
//
// Note that we ignore the variables that appear in no constraint, instead of
// outputing the full symmetry group involving them.
//
// TODO(user): On SAT problems it is more powerful to detect permutations also
// involving the negation of the problem variables. So that we could find a
// symmetry x <-> not(y) for instance.
//
// TODO(user): As long as we only exploit symmetry involving only Boolean
// variables we can make this code more efficient by not detecting symmetries
// involing integer variable.
void FindCpModelSymmetries(
    const SatParameters& params, const CpModelProto& problem,
    std::vector<std::unique_ptr<SparsePermutation>>* generators,
    double deterministic_limit, SolverLogger* logger);

// Detects symmetries and fill the symmetry field.
void DetectAndAddSymmetryToProto(const SatParameters& params,
                                 CpModelProto* proto, SolverLogger* logger);

// Basic implementation of some symmetry breaking during presolve.
//
// Currently this just try to fix variables by detecting symmetries between
// Booleans in bool_and, at_most_one or exactly_one constraints.
bool DetectAndExploitSymmetriesInPresolve(PresolveContext* context);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SYMMETRIES_H_
