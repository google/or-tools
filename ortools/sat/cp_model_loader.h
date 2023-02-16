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

#ifndef OR_TOOLS_SAT_CP_MODEL_LOADER_H_
#define OR_TOOLS_SAT_CP_MODEL_LOADER_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Extracts all the used variables in the CpModelProto and creates a
// sat::Model representation for them. More precisely
//  - All Boolean variables will be mapped.
//  - All Interval variables will be mapped.
//  - All non-Boolean variable will have a corresponding IntegerVariable, and
//    depending on the view_all_booleans_as_integers, some or all of the
//    BooleanVariable will also have an IntegerVariable corresponding to its
//    "integer view".
//
// Note(user): We could create IntegerVariable on the fly as they are needed,
// but that loose the original variable order which might be useful in
// heuristics later.
void LoadVariables(const CpModelProto& model_proto,
                   bool view_all_booleans_as_integers, Model* m);

// Automatically detect optional variables.
void DetectOptionalVariables(const CpModelProto& model_proto, Model* m);

// Experimental. Loads the symmetry form the proto symmetry field, as long as
// they only involve Booleans.
//
// TODO(user): We currently only have the code for Booleans, it is why we
// currently ignore symmetries involving integer variables.
void LoadBooleanSymmetries(const CpModelProto& model_proto, Model* m);

// Extract the encodings (IntegerVariable <-> Booleans) present in the model.
// This effectively load some linear constraints of size 1 that will be marked
// as already loaded.
void ExtractEncoding(const CpModelProto& model_proto, Model* m);

// Extract element encodings from exactly_one constraints and
//    lit => var == value constraints.
// This function must be called after ExtractEncoding() has been called.
void ExtractElementEncoding(const CpModelProto& model_proto, Model* m);

// Process all affine relations of the form a*X + b*Y == cte. For each
// literals associated to (X >= bound) or (X == value) associate it to its
// corresponding relation on Y. Also do the other side.
//
// TODO(user): In an ideal world, all affine relations like this should be
// removed in the presolve.
void PropagateEncodingFromEquivalenceRelations(const CpModelProto& model_proto,
                                               Model* m);

// Inspect the search strategy stored in the model, and adds a full encoding to
// variables appearing in a SELECT_MEDIAN_VALUE search strategy if the search
// branching is set to FIXED_SEARCH.
void AddFullEncodingFromSearchBranching(const CpModelProto& model_proto,
                                        Model* m);

// Calls one of the functions below.
// Returns false if we do not know how to load the given constraints.
bool LoadConstraint(const ConstraintProto& ct, Model* m);

void LoadBoolOrConstraint(const ConstraintProto& ct, Model* m);
void LoadBoolAndConstraint(const ConstraintProto& ct, Model* m);
void LoadAtMostOneConstraint(const ConstraintProto& ct, Model* m);
void LoadExactlyOneConstraint(const ConstraintProto& ct, Model* m);
void LoadBoolXorConstraint(const ConstraintProto& ct, Model* m);
void LoadLinearConstraint(const ConstraintProto& ct, Model* m);
void LoadAllDiffConstraint(const ConstraintProto& ct, Model* m);
void LoadIntProdConstraint(const ConstraintProto& ct, Model* m);
void LoadIntDivConstraint(const ConstraintProto& ct, Model* m);
void LoadIntMinConstraint(const ConstraintProto& ct, Model* m);
void LoadLinMaxConstraint(const ConstraintProto& ct, Model* m);
void LoadIntMaxConstraint(const ConstraintProto& ct, Model* m);
void LoadNoOverlapConstraint(const ConstraintProto& ct, Model* m);
void LoadNoOverlap2dConstraint(const ConstraintProto& ct, Model* m);
void LoadCumulativeConstraint(const ConstraintProto& ct, Model* m);
void LoadCircuitConstraint(const ConstraintProto& ct, Model* m);
void LoadReservoirConstraint(const ConstraintProto& ct, Model* m);
void LoadRoutesConstraint(const ConstraintProto& ct, Model* m);
void LoadCircuitCoveringConstraint(const ConstraintProto& ct, Model* m);

// Part of LoadLinearConstraint() that we reuse to load the objective.
//
// We split large constraints into a square root number of parts.
// This is to avoid a bad complexity while propagating them since our
// algorithm is not in O(num_changes).
//
// TODO(user): Alternatively, we could use a O(num_changes) propagation (a
// bit tricky to implement), or a decomposition into a tree with more than
// one level. Both requires experimentations.
void SplitAndLoadIntermediateConstraints(bool lb_required, bool ub_required,
                                         std::vector<IntegerVariable>* vars,
                                         std::vector<int64_t>* coeffs,
                                         Model* m);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_LOADER_H_
