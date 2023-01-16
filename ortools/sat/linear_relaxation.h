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

#ifndef OR_TOOLS_SAT_LINEAR_RELAXATION_H_
#define OR_TOOLS_SAT_LINEAR_RELAXATION_H_

#include <optional>
#include <vector>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_util.h"

namespace operations_research {
namespace sat {

struct LinearRelaxation {
  std::vector<LinearConstraint> linear_constraints;
  std::vector<std::vector<Literal>> at_most_ones;
  std::vector<CutGenerator> cut_generators;
};

// Looks at all the encoding literal (li <=> var == value_i) that have a
// view and add a linear relaxation of their relationship with var.
//
// If the encoding is full, we can just add:
// - Sum li == 1
// - var == min_value + Sum li * (value_i - min_value)
//
// When the set of such encoding literals do not cover the full domain of var,
// we do something a bit more involved. Let min_not_encoded/max_not_encoded the
// min and max value of the domain of var that is NOT part of the encoding.
// We add:
//   - Sum li <= 1
//   - var >= (Sum li * value_i) + (1 - Sum li) * min_not_encoded
//   - var <= (Sum li * value_i) + (1 - Sum li) * max_not_encoded
//
// Note of the special case where min_not_encoded == max_not_encoded that kind
// of reduce to the full encoding, except with a different "rhs" value.
//
// We also increment the corresponding counter if we added something. We
// consider the relaxation "tight" if the encoding was full or if
// min_not_encoded == max_not_encoded.
void AppendRelaxationForEqualityEncoding(IntegerVariable var,
                                         const Model& model,
                                         LinearRelaxation* relaxation,
                                         int* num_tight, int* num_loose);

// This is a different relaxation that use a partial set of literal li such that
// (li <=> var >= xi). In which case we use the following encoding:
//   - li >= l_{i+1} for all possible i. Note that the xi need to be sorted.
//   - var >= min + l0 * (x0 - min) + Sum_{i>0} li * (xi - x_{i-1})
//   - and same as above for NegationOf(var) for the upper bound.
//
// Like for AppendRelaxationForEqualityEncoding() we skip any li that do not
// have an integer view.
void AppendPartialGreaterThanEncodingRelaxation(IntegerVariable var,
                                                const Model& model,
                                                LinearRelaxation* relaxation);

// Returns a vector of new literals in exactly one relationship.
// In addition, this create an IntegerView for all these literals and also add
// the exactly one to the LinearRelaxation.
std::vector<Literal> CreateAlternativeLiteralsWithView(
    int num_literals, Model* model, LinearRelaxation* relaxation);

void AppendBoolOrRelaxation(const ConstraintProto& ct, Model* model,
                            LinearRelaxation* relaxation);

void AppendBoolAndRelaxation(const ConstraintProto& ct, Model* model,
                             LinearRelaxation* relaxation,
                             ActivityBoundHelper* activity_helper = nullptr);

void AppendAtMostOneRelaxation(const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation);

void AppendExactlyOneRelaxation(const ConstraintProto& ct, Model* model,
                                LinearRelaxation* relaxation);

// Adds linearization of int max constraints. Returns a vector of z vars such
// that: z_vars[l] == 1 <=> target = exprs[l].
//
// Consider the Lin Max constraint with d expressions and n variables in the
// form: target = max {exprs[l] = Sum (wli * xi + bl)}. l in {1,..,d}.
//   Li = lower bound of xi
//   Ui = upper bound of xi.
// Let zl be in {0,1} for all l in {1,..,d}.
// The target = exprs[l] when zl = 1.
//
// The following is a valid linearization for Lin Max.
//   target >= exprs[l], for all l in {1,..,d}
//   target <= Sum_i(wki * xi) + Sum_l((Nkl + bl) * zl), for all k in {1,..,d}
// Where Nkl is a large number defined as:
//   Nkl = Sum_i(max((wli - wki)*Li, (wli - wki)*Ui))
//       = Sum (max corner difference for variable i, target expr k, max expr l)
// Reference: "Strong mixed-integer programming formulations for trained neural
// networks" by Ross Anderson et. (https://arxiv.org/pdf/1811.01988.pdf).
// TODO(user): Support linear expression as target.
void AppendLinMaxRelaxationPart1(const ConstraintProto& ct, Model* model,
                                 LinearRelaxation* relaxation);

void AppendLinMaxRelaxationPart2(
    IntegerVariable target, const std::vector<Literal>& alternative_literals,
    const std::vector<LinearExpression>& exprs, Model* model,
    LinearRelaxation* relaxation);

// Note: This only works if all affine expressions share the same variable.
void AppendMaxAffineRelaxation(const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation);

// Appends linear constraints to the relaxation. This also handles the
// relaxation of linear constraints with enforcement literals.
// A linear constraint lb <= ax <= ub with enforcement literals {ei} is relaxed
// as following.
// lb <= (Sum Negated(ei) * (lb - implied_lb)) + ax <= inf
// -inf <= (Sum Negated(ei) * (ub - implied_ub)) + ax <= ub
// Where implied_lb and implied_ub are trivial lower and upper bounds of the
// constraint.
void AppendLinearConstraintRelaxation(
    const ConstraintProto& ct, bool linearize_enforced_constraints,
    Model* model, LinearRelaxation* relaxation,
    ActivityBoundHelper* activity_helper = nullptr);

void AppendSquareRelaxation(const ConstraintProto& ct, Model* m,
                            LinearRelaxation* relaxation);

// Adds linearization of no overlap constraints.
// It adds an energetic equation linking the duration of all potential tasks to
// the actual span of the no overlap constraint.
void AppendNoOverlapRelaxationAndCutGenerator(const ConstraintProto& ct,
                                              Model* model,
                                              LinearRelaxation* relaxation);

// Adds linearization of cumulative constraints.The second part adds an
// energetic equation linking the duration of all potential tasks to the actual
// span * capacity of the cumulative constraint.
void AppendCumulativeRelaxationAndCutGenerator(const ConstraintProto& ct,
                                               Model* model,
                                               LinearRelaxation* relaxation);

// Cut generators.
void AddIntProdCutGenerator(const ConstraintProto& ct, int linearization_level,
                            Model* m, LinearRelaxation* relaxation);

void AddSquareCutGenerator(const ConstraintProto& ct, int linearization_level,
                           Model* m, LinearRelaxation* relaxation);

void AddAllDiffRelaxationAndCutGenerator(const ConstraintProto& ct,
                                         int linearization_level, Model* m,
                                         LinearRelaxation* relaxation);

void AddLinMaxCutGenerator(const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation);

// Routing relaxation and cut generators.

void AppendCircuitRelaxation(const ConstraintProto& ct, Model* model,
                             LinearRelaxation* relaxation);

void AppendRoutesRelaxation(const ConstraintProto& ct, Model* model,
                            LinearRelaxation* relaxation);

void AddCircuitCutGenerator(const ConstraintProto& ct, Model* m,
                            LinearRelaxation* relaxation);

void AddRoutesCutGenerator(const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation);

// Scheduling relaxations and cut generators.

// Adds linearization of cumulative constraints.The second part adds an
// energetic equation linking the duration of all potential tasks to the actual
// span * capacity of the cumulative constraint. It uses the makespan to compute
// the span of the constraint if defined.
void AddCumulativeRelaxation(const AffineExpression& capacity,
                             SchedulingConstraintHelper* helper,
                             SchedulingDemandHelper* demands,
                             const std::optional<AffineExpression>& makespan,
                             Model* model, LinearRelaxation* relaxation);

void AddCumulativeCutGenerator(const AffineExpression& capacity,
                               SchedulingConstraintHelper* helper,
                               SchedulingDemandHelper* demands,
                               const std::optional<AffineExpression>& makespan,
                               Model* m, LinearRelaxation* relaxation);

void AddNoOverlapCutGenerator(SchedulingConstraintHelper* helper,
                              const std::optional<AffineExpression>& makespan,
                              Model* m, LinearRelaxation* relaxation);

void AddNoOverlap2dCutGenerator(const ConstraintProto& ct, Model* m,
                                LinearRelaxation* relaxation);

// Adds linearization of different types of constraints.
void TryToLinearizeConstraint(const CpModelProto& model_proto,
                              const ConstraintProto& ct,
                              int linearization_level, Model* model,
                              LinearRelaxation* relaxation,
                              ActivityBoundHelper* helper = nullptr);

// Builds the linear relaxation of a CpModelProto.
LinearRelaxation ComputeLinearRelaxation(const CpModelProto& model_proto,
                                         Model* m);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_RELAXATION_H_
