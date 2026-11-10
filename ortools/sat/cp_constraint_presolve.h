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

  bool PresolveOneConstraint(int c);

  // Regroups terms and substitutes affine relations.
  // Returns true if the set of variables in the expression changed.
  bool CanonicalizeLinearExpression(const ConstraintProto& ct,
                                    LinearExpressionProto* exp);

  bool PropagateDomainsInLinear(int ct_index, ConstraintProto* ct);
  bool PresolveSmallLinear(ConstraintProto* ct);

  bool PresolveCumulative(ConstraintProto* ct);
  bool PresolveNoOverlap(ConstraintProto* ct);
  bool PresolveNoOverlap2D(int c, ConstraintProto* ct);

  // Detects if a 2-term linear constraint (or constraint with 2 distinct
  // coefficient magnitudes) can be tilted to strictly smaller integer
  // coefficients using continued fraction convergents and Euclidean Algorithm.
  // Returns true if the detection triggered and the constraint was rewritten.
  // See go/cp-two-term-tilting for more details.
  bool DetectTwoTermTilting(
      ConstraintProto* ct,
      const PresolveContext::MergedCanonicalLinearConstraint& canonical);

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

  bool PresolveEnforcementLiteral(ConstraintProto* ct);
  bool PresolveAtMostOrExactlyOne(ConstraintProto* ct);
  bool PresolveAtMostOne(ConstraintProto* ct);
  bool PresolveExactlyOne(ConstraintProto* ct);
  bool PresolveBoolAnd(ConstraintProto* ct);
  bool PresolveBoolOr(ConstraintProto* ct);
  bool PresolveBoolXor(ConstraintProto* ct);

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
  // can be equivalent to a constraint on expr_X only, or decouple into an
  // equality on expr_X and a big-M-free constraint on expr_Y (for example when
  // opposing big-M terms cancel out in an equality or narrow ranged
  // constraint).
  //
  // For instance "10'001 X + 9'999 Y <= 105'000, with X, Y in [0, 100]" can
  // be rewritten as X + Y <= 10 ! And "x + y + 100 b1 - 100 b2 == 1" on
  // Booleans decouples into "b1 - b2 == 0" and "x + y == 1".
  void TryToReduceCoefficientsOfLinearConstraint(
      int c, ConstraintProto* ct,
      const PresolveContext::MergedCanonicalLinearConstraint& canonical);

  // Splits linear constraint `ct` (at index `c`, preserving enforcement
  // literals) when all terms with `|coeff| >= magnitude` (whose coefficients
  // have GCD `big_gcd`) are forced by `canonical` bounds to take the single
  // shifted value `forced_big`, leaving `max_error` as the total variation of
  // the remaining smaller-magnitude terms.
  void DecoupleForcedBigMSuffix(
      int c, ConstraintProto* ct,
      const PresolveContext::MergedCanonicalLinearConstraint& canonical,
      int64_t magnitude, int64_t big_gcd, int64_t forced_big,
      int64_t max_error);

  // Detects if the given linear constraint is equivalent to sum vars \in
  // domain. This is a bit costly: O(n log n); but it does trigger often enough
  // that it seems worth it.
  //
  // Returns true if the detection triggered and the constraint was rewritten.
  //
  // TODO(user): We could probably handle other simple coefficient reduction
  // case. In particular, look at the case of only two different coefficient
  // magnitude.
  bool DetectSimpleSum(
      ConstraintProto* ct,
      const PresolveContext::MergedCanonicalLinearConstraint& canonical);

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

  // Extracts enforcement literals from one-sided linear constraints, or
  // strengthens oversized coefficients when extraction does not apply. Returns
  // true if any enforcement literal was extracted or coefficient was
  // strengthened and the constraint remains an active linear constraint.
  bool ExtractEnforcementLiteralFromLinearConstraint(int ct_index,
                                                     ConstraintProto* ct);

  // Strengthens or reduces coefficients of a one-sided linear constraint (with
  // at least 3 distinct coefficient magnitudes) when large terms force all
  // other terms to their bounds (a "partial at-most-one" structure), or fixes
  // variables whose coefficient exceeds the slack on unenforced constraints.
  // Updates `*canonical` in place and returns true if `ct` or variable domains
  // are modified.
  bool LowerThanCoeffStrengthening(
      ConstraintProto* ct,
      PresolveContext::MergedCanonicalLinearConstraint* canonical);

  void RunPropagatorsForConstraint(const ConstraintProto& ct);

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
