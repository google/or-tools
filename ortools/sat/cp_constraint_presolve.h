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

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_canonical_checker.h"
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

// Result of tilting a canonical 2-term linear inequality
//   a1 * x1 + a2 * x2 <= B,  with x1 in [0, R1], x2 in [0, R2],
// into the equivalent inequality new_a1 * x1 + new_a2 * x2 <= best_ub.
struct TwoTermTilt {
  int64_t new_a1;
  int64_t new_a2;
  int64_t best_ub;
};

// Finds the smallest positive integer coefficients (new_a1, new_a2) and
// tightest right-hand side best_ub such that new_a1 * x1 + new_a2 * x2 <=
// best_ub has the exact same integer solutions in [0, R1] x [0, R2] as
// a1 * x1 + a2 * x2 <= B.
//
// Preconditions: 0 < a1 < a2, R1 > 0, R2 > 0, 0 <= B < a1 * R1 + a2 * R2, and
// a1 * R1 + a2 * R2 fits in int64_t.
// See go/cp-two-term-tilting for more details.
TwoTermTilt TiltTwoTermConstraint(int64_t a1, int64_t a2, int64_t B, int64_t R1,
                                  int64_t R2);

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

  FeasibilityStatus PresolveOneConstraint(int c);

  // Regroups terms and substitutes affine relations.
  ModificationStatus CanonicalizeLinearExpression(const ConstraintProto& ct,
                                                  LinearExpressionProto* exp);

  // Propagates variable bounds from a temporary unenforced or singly-enforced
  // linear constraint (clearing `*temp_ct` if trivially satisfied).
  FeasibilityStatus PropagateDomainsInLinear(ConstraintProto* temp_ct);
  FeasibilityStatus PresolveSmallLinear(int c);

  CanonicalizationStatus PresolveCumulative(int c);
  CanonicalizationStatus PresolveNoOverlap(int c);
  CanonicalizationStatus PresolveNoOverlap2D(int c);

  // Detects if a 2-term linear constraint (or constraint with 2 distinct
  // coefficient magnitudes) can be tilted to strictly smaller integer
  // coefficients using continued fraction convergents and Euclidean Algorithm.
  // See go/cp-two-term-tilting for more details.
  CanonicalizationStatus DetectTwoTermTilting(
      int c, const PresolveContext::MergedLinearConstraintView& view);

  FeasibilityStatus TryToSimplifyDomain(int var);

  FeasibilityStatus PresolveAffineRelationIfAny(int var);

  // TODO(user): move this to cp_model_presolve.h?
  void RemoveEmptyConstraints();

  // Marks constraint `c` as false (proving UNSAT if unenforced, or turning its
  // enforcement literals into a negated clause and marking `c` for
  // canonicalization).
  CanonicalizationStatus MarkConstraintAsFalse(int c, std::string_view reason);

 private:
  // Presolve functions.
  //
  // Invariant about UNSAT: All these functions should abort right away if
  // `context_->ModelIsUnsat()` is true. And the only way to change the status
  // to unsat is through a `[[nodiscard]]` function that should also abort the
  // current code right away. This way we don't keep doing computation on an
  // inconsistent state.
  // TODO(user): make all presolve functions use the same return type?
  CanonicalizationStatus PresolveOneConstraintInternal(int c);
  CanonicalizationStatus PresolveAllDiff(int c);
  FeasibilityStatus PresolveAutomaton(int c);
  CanonicalizationStatus PresolveElement(int c);
  CanonicalizationStatus PresolveIntDiv(int c);
  FeasibilityStatus PresolveIntMod(int c);
  CanonicalizationStatus PresolveIntProd(int c);
  FeasibilityStatus PresolveInterval(int c);
  FeasibilityStatus PresolveInverse(int c);
  FeasibilityStatus DivideLinMaxByGcd(int c);
  FeasibilityStatus PresolveLinMax(int c);
  FeasibilityStatus PresolveLinMaxWhenAllBoolean(int c);
  CanonicalizationStatus PropagateAndReduceAffineMax(int c);
  FeasibilityStatus PropagateAndReduceIntAbs(int c);
  CanonicalizationStatus PropagateAndReduceLinMax(int c);
  CanonicalizationStatus PresolveTable(int c);
  void DetectDuplicateIntervals(
      int c, google::protobuf::RepeatedField<int32_t>* intervals);
  CanonicalizationStatus PresolveReservoir(int c);

  CanonicalizationStatus PresolveCircuit(int c);
  FeasibilityStatus PresolveRoutes(int c);

  FeasibilityStatus PresolveEnforcementLiteral(int c);
  FeasibilityStatus PresolveAtMostOrExactlyOne(int c);
  FeasibilityStatus PresolveAtMostOne(int c);
  FeasibilityStatus PresolveExactlyOne(int c);
  CanonicalizationStatus PresolveBoolAnd(int c);
  CanonicalizationStatus PresolveBoolOr(int c);
  FeasibilityStatus PresolveBoolXor(int c);

  // Replaces constraint `c` (keeping the same enforcement literals) with
  // `expr \in domain`.
  CanonicalizationStatus MarkConstraintAsEquivalentToLinear(
      int c, const LinearExpressionProto& expr, const Domain& domain,
      std::string_view reason);

  // For the linear constraints, we have more than one function.
  FeasibilityStatus PropagateDomainsInLinear(int ct_index);
  FeasibilityStatus RemoveSingletonInLinear(int c, bool* modified);
  FeasibilityStatus PresolveLinearOfSizeOne(int c);
  CanonicalizationStatus PresolveLinearOfSizeTwo(int c);
  CanonicalizationStatus PresolveLinearOnBooleans(int c);
  FeasibilityStatus PresolveSmallLinearOnBooleans(int c);
  FeasibilityStatus PresolveDiophantine(int c);
  FeasibilityStatus AddVarAffineRepresentativeFromLinearEquality(
      int c, int target_index);
  FeasibilityStatus PresolveLinearEqualityWithModulo(int c);
  void PresolveLinear2NeCst(int c, int64_t rhs);
  FeasibilityStatus PresolveUnenforcedLinear2EqCst(int c, int64_t rhs);
  CanonicalizationStatus PresolveEnforcedLinear2EqCst(int c, int64_t rhs);
  CanonicalizationStatus PresolveLinear2WithBooleans(int c);

  // If a constraint is of the form "a * expr_X + expr_Y" and expr_Y can only
  // take small values compared to a, depending on the bounds, the constraint
  // can be equivalent to a constraint on expr_X only, or decouple into an
  // equality on expr_X and a big-M-free constraint on expr_Y (for example when
  // opposing big-M terms cancel out in an equality or narrow ranged
  // constraint).
  //
  // For instance "10'001 X + 9'999 Y <= 105'000, with X, Y in [0, 100]" can
  // be rewritten as X + Y <= 10 ! And "x + y + 100 b1 - 100 b2 == 1" on
  // Booleans decouples into "b1 - b2 == 0" and "x + y == 1".
  FeasibilityStatus TryToReduceCoefficientsOfLinearConstraint(
      int c, const PresolveContext::MergedLinearConstraintView& view);

  // Splits linear constraint `c` (preserving enforcement literals) when all
  // terms with `|coeff| >= magnitude` (whose coefficients have GCD `big_gcd`)
  // are forced by `view` bounds to take the single shifted value `forced_big`,
  // leaving `max_error` as the total variation of the remaining
  // smaller-magnitude terms.
  FeasibilityStatus DecoupleForcedBigMSuffix(
      int c, const PresolveContext::MergedLinearConstraintView& view,
      int64_t magnitude, int64_t big_gcd, int64_t forced_big,
      int64_t max_error);

  // Detects if the given linear constraint is equivalent to sum vars \in
  // domain. This is a bit costly: O(n log n); but it does trigger often enough
  // that it seems worth it.
  //
  // TODO(user): We could probably handle other simple coefficient reduction
  // case. In particular, look at the case of only two different coefficient
  // magnitude.
  CanonicalizationStatus DetectSimpleSum(
      int c, const PresolveContext::MergedLinearConstraintView& view);

  // Presolve a no_overlap_2d constraint where all the non-fixed rectangles are
  // framed by exactly four fixed rectangles and at most one single box can fit
  // inside the frame. This is a rather specific situation, but it is fast to
  // check and happens often in LNS problems.
  FeasibilityStatus PresolveNoOverlap2DFramed(
      int c, absl::Span<const Rectangle> fixed_boxes,
      absl::Span<const RectangleInRange> non_fixed_boxes);

  // Detects when the space where items of a no_overlap_2d constraint can be
  // placed is disjoint (ie., fixed boxes split the domain). When it is the
  // case, we can introduce a boolean for each pair <item, component> encoding
  // whether the item is in the component or not. Then we replace the original
  // no_overlap_2d constraint by one no_overlap_2d constraint for each
  // component, with the new booleans as the enforcement_literal of the
  // intervals. This is equivalent to expanding the original no_overlap_2d
  // constraint into a bin packing problem with each connected component being a
  // bin.
  FeasibilityStatus ExpandEncoded2DBinPacking(
      int c, absl::Span<const Rectangle> fixed_boxes,
      absl::Span<const RectangleInRange> non_fixed_boxes);

  // Extracts enforcement literals from one-sided linear constraints, or
  // strengthens oversized coefficients when extraction does not apply.
  CanonicalizationStatus ExtractEnforcementLiteralFromLinearConstraint(
      int ct_index);

  // Fixes variables whose coefficient magnitude exceeds the active upper- or
  // lower-bound slack on an unenforced linear constraint. If any domain is
  // modified, re-canonicalizes `c` to remove the fixed variables and updates
  // `*view` in place (if constraint `c` remains an active linear constraint).
  FeasibilityStatus FixVariablesExceedingRhs(
      int c, PresolveContext::MergedLinearConstraintView* view);

  // Strengthens or reduces coefficients of a one-sided linear constraint (with
  // at least 3 distinct coefficient magnitudes) when large terms force all
  // other terms to their bounds (a "partial at-most-one" structure). Updates
  // `*view` in place.
  CanonicalizationStatus LowerThanCoeffStrengthening(
      int c, PresolveContext::MergedLinearConstraintView* view);

  FeasibilityStatus RunPropagatorsForConstraint(const ConstraintProto& ct);

  // Fixes the enforcement literal of optional interval constraint `c` to false
  // and canonicalizes `c`.
  FeasibilityStatus MarkOptionalIntervalAsFalse(int c);

  FeasibilityStatus AddLinear2ToModel(const LinearExpression2& linear2,
                                      int64_t lb, int64_t ub);

  PresolveContext* context_;
  SolutionCrush& solution_crush_;
  SolverLogger* logger_;
  TimeLimit* time_limit_;

  // Used by RunPropagatorsForConstraint().
  CpModelProto tmp_model_;

  // Used by TryToReduceCoefficientsOfLinearConstraint().
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
