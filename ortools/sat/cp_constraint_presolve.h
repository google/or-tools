// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_SAT_CP_CONSTRAINT_PRESOLVE_H_
#define ORTOOLS_SAT_CP_CONSTRAINT_PRESOLVE_H_

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Presolves constraints of a CpModelProto one by one, in isolation.
class CpConstraintPresolver {
 public:
  explicit CpConstraintPresolver(PresolveContext* context);

  // Returns all the linear2 in the model or deduced by the presolve.
  BestBinaryRelationBounds& known_linear2() { return known_linear2_; }

  // Returns all the linear2 encoded in the original model.
  const BestBinaryRelationBounds& known_model_linear2() const {
    return known_model_linear2_;
  }

  bool PresolveOneConstraint(int c);

  bool PresolveAtMostOrExactlyOne(ConstraintProto* ct, bool use_dual_reduction);
  bool PresolveAtMostOne(ConstraintProto* ct, bool use_dual_reduction = true);
  bool PresolveExactlyOne(ConstraintProto* ct);
  bool PresolveEnforcementLiteral(ConstraintProto* ct, bool* changed);

  // Regroups terms and substitutes affine relations.
  // Returns true if the set of variables in the expression changed.
  bool CanonicalizeLinearExpression(const ConstraintProto& ct,
                                    LinearExpressionProto* exp);

  ABSL_MUST_USE_RESULT bool CanonicalizeLinear(ConstraintProto* ct,
                                               bool* changed);
  bool PropagateDomainsInLinear(int ct_index, ConstraintProto* ct);
  bool PresolveSmallLinear(ConstraintProto* ct, bool canonicalize = true);
  bool PresolveEmptyLinearConstraint(ConstraintProto* ct);

  bool PresolveCumulative(ConstraintProto* ct);
  bool PresolveNoOverlap(ConstraintProto* ct);
  bool PresolveNoOverlap2D(int c, ConstraintProto* ct);

  // Returns true if the constraint changed.
  bool DivideLinearByGcd(ConstraintProto* ct);

  void TryToSimplifyDomain(int var);

  // Returns false on UNSAT.
  bool PresolveAffineRelationIfAny(int var);

  ABSL_MUST_USE_RESULT bool RemoveConstraint(ConstraintProto* ct);
  // TODO(user): move this to cp_model_presolve.h?
  void RemoveEmptyConstraints();

  ABSL_MUST_USE_RESULT bool MarkConstraintAsFalse(ConstraintProto* ct,
                                                  std::string_view reason);

 private:
  // Presolve functions.
  //
  // They should return false only if the constraint <-> variable graph didn't
  // change. This is just an optimization, returning true is always correct.
  //
  // Invariant about UNSAT: All these functions should abort right away if
  // context_.IsUnsat() is true. And the only way to change the status to unsat
  // is through an ABSL_MUST_USE_RESULT function that should also abort the
  // current code right away. This way we shouldn't keep doing computation on an
  // inconsistent state.
  // TODO(user): Make these public and unit test.
  bool PresolveAllDiff(ConstraintProto* ct);
  bool PresolveAutomaton(ConstraintProto* ct);
  bool PresolveElement(int c, ConstraintProto* ct);
  bool PresolveIntDiv(int c, ConstraintProto* ct);
  bool PresolveIntMod(int c, ConstraintProto* ct);
  bool PresolveIntProd(ConstraintProto* ct);
  bool PresolveInterval(int c, ConstraintProto* ct);
  bool PresolveLegacyInverse(ConstraintProto* ct);
  bool PresolveInverse(ConstraintProto* ct);
  bool DivideLinMaxByGcd(int c, ConstraintProto* ct);
  bool PresolveLinMax(int c, ConstraintProto* ct);
  bool PresolveLinMaxWhenAllBoolean(ConstraintProto* ct);
  bool PropagateAndReduceAffineMax(ConstraintProto* ct);
  bool PropagateAndReduceIntAbs(ConstraintProto* ct);
  bool PropagateAndReduceLinMax(ConstraintProto* ct);
  bool PresolveTable(ConstraintProto* ct);
  void DetectDuplicateIntervals(
      int c, google::protobuf::RepeatedField<int32_t>* intervals);
  bool PresolveReservoir(ConstraintProto* ct);

  bool PresolveCircuit(ConstraintProto* ct);
  bool PresolveRoutes(ConstraintProto* ct);

  bool PresolveBoolAnd(ConstraintProto* ct);
  bool PresolveBoolOr(ConstraintProto* ct);
  bool PresolveBoolXor(ConstraintProto* ct);

  bool CanonicalizeLinearArgument(const ConstraintProto& ct,
                                  LinearArgumentProto* proto);

  // For the linear constraints, we have more than one function.
  bool RemoveSingletonInLinear(ConstraintProto* ct);
  bool PresolveLinearOfSizeOne(ConstraintProto* ct);
  bool PresolveLinearOfSizeTwo(ConstraintProto* ct);
  bool PresolveLinearOnBooleans(ConstraintProto* ct);
  bool PresolveSmallLinearOnBooleans(ConstraintProto* ct);
  bool PresolveDiophantine(ConstraintProto* ct);
  bool AddVarAffineRepresentativeFromLinearEquality(int target_index,
                                                    ConstraintProto* ct);
  bool PresolveLinearEqualityWithModulo(ConstraintProto* ct);
  bool PresolveLinear2NeCst(ConstraintProto* ct, int64_t rhs);
  bool PresolveUnenforcedLinear2EqCst(ConstraintProto* ct, int64_t rhs);
  bool PresolveEnforcedLinear2EqCst(ConstraintProto* ct, int64_t rhs);
  bool PresolveLinear2WithBooleans(ConstraintProto* ct);

  // If a constraint is of the form "a * expr_X + expr_Y" and expr_Y can only
  // take small values compared to a, depending on the bounds, the constraint
  // can be equivalent to a constraint on expr_X only.
  //
  // For instance "10'001 X + 9'999 Y <= 105'000, with X, Y in [0, 100]" can
  // be rewritten as X + Y <= 10 ! This can easily happen after scaling to
  // integer coefficients in a floating-point constraint.
  void TryToReduceCoefficientsOfLinearConstraint(int c, ConstraintProto* ct);

  // Presolve a no_overlap_2d constraint where all the non-fixed rectangles are
  // framed by exactly four fixed rectangles and at most one single box can fit
  // inside the frame. This is a rather specific situation, but it is fast to
  // check and happens often in LNS problems.
  bool PresolveNoOverlap2DFramed(
      absl::Span<const Rectangle> fixed_boxes,
      absl::Span<const RectangleInRange> non_fixed_boxes, ConstraintProto* ct);

  // Detects when the space where items of a no_overlap_2d constraint can be
  // placed is disjoint (ie., fixed boxes split the domain). When it is the
  // case, we can introduce a boolean for each pair <item, component> encoding
  // whether the item is in the component or not. Then we replace the original
  // no_overlap_2d constraint by one no_overlap_2d constraint for each
  // component, with the new booleans as the enforcement_literal of the
  // intervals. This is equivalent to expanding the original no_overlap_2d
  // constraint into a bin packing problem with each connected component being a
  // bin.
  bool ExpandEncoded2DBinPacking(
      absl::Span<const Rectangle> fixed_boxes,
      absl::Span<const RectangleInRange> non_fixed_boxes, ConstraintProto* ct);

  void ExtractEnforcementLiteralFromLinearConstraint(int ct_index,
                                                     ConstraintProto* ct);
  void LowerThanCoeffStrengthening(bool from_lower_bound, int64_t min_magnitude,
                                   int64_t rhs, ConstraintProto* ct);

  void RunPropagatorsForConstraint(const ConstraintProto& ct);

  bool ExploitEquivalenceRelations(int c, ConstraintProto* ct);

  ABSL_MUST_USE_RESULT bool MarkOptionalIntervalAsFalse(ConstraintProto* ct);

  void AddLinear2ToModel(const LinearExpression2& linear2, int64_t lb,
                         int64_t ub);

  PresolveContext* context_;
  SolutionCrush& solution_crush_;
  SolverLogger* logger_;
  TimeLimit* time_limit_;

  // Used by RunPropagatorsForConstraint().
  CpModelProto tmp_model_;

  // Used by TryToReduceCoefficientsOfLinearConstraint().
  struct RdEntry {
    int64_t magnitude;
    int64_t max_variation;
    int index;
  };
  std::vector<RdEntry> rd_entries_;
  std::vector<int> rd_vars_;
  std::vector<int64_t> rd_coeffs_;
  std::vector<int64_t> rd_magnitudes_;
  std::vector<int64_t> rd_lbs_;
  std::vector<int64_t> rd_ubs_;
  std::vector<int64_t> rd_divisors_;
  MaxBoundedSubsetSum lb_feasible_;
  MaxBoundedSubsetSum lb_infeasible_;
  MaxBoundedSubsetSum ub_feasible_;
  MaxBoundedSubsetSum ub_infeasible_;

  // We have a hash map of known relations between two variables.
  // In particular, this will include all known precedences a <= b.
  //
  // We reuse an IntegerVariable/IntegerValue based class via
  // GetLinearExpression2FromProto() only visible in the .cc.
  //
  // We have two versions of this map: one that only considers linear2 that
  // are encoded as such in the model, and a more general one that considers any
  // linear2 that was detected by the presolve.
  BestBinaryRelationBounds known_linear2_;
  BestBinaryRelationBounds known_model_linear2_;

  struct IntervalConstraintEq {
    const CpModelProto* working_model;
    bool operator()(int a, int b) const;
  };

  struct IntervalConstraintHash {
    const CpModelProto* working_model;
    std::size_t operator()(int ct_idx) const;
  };

  // Used by DetectDuplicateIntervals(). Note that changing the interval
  // constraints of the model will change the hash and invalidate this hash map.
  absl::flat_hash_map<int, int, IntervalConstraintHash, IntervalConstraintEq>
      interval_representative_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_CONSTRAINT_PRESOLVE_H_
