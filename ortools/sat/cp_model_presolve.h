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

#ifndef OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
#define OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Replaces all the instance of a variable i (and the literals referring to it)
// by mapping[i] in the working model. The definition of variables i is also
// moved to its new index. If mapping[i] < 0 the variable can be ignored if
// possible. If it is not possible, then we will use a new index for it (at the
// end) and the mapping will be updated to reflect that.
//
// The image of the mapping should be dense in [0, reverse_mapping->size()).
void ApplyVariableMapping(PresolveContext* context, absl::Span<int> mapping,
                          std::vector<int>* reverse_mapping);

// Presolves the initial content of presolved_model.
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
class CpModelPresolver {
 public:
  CpModelPresolver(PresolveContext* context,
                   std::vector<int>* postsolve_mapping);

  // We returns the status of the problem after presolve:
  //  - UNKNOWN if everything was ok.
  //  - INFEASIBLE if the model was proven so during presolve
  //  - MODEL_INVALID if the model caused some issues, like if we are not able
  //    to scale a floating point objective with enough precision.
  CpSolverStatus Presolve();

  // Executes presolve method for the given constraint. Public for testing only.
  bool PresolveOneConstraint(int c);

  // Visible for testing.
  void RemoveEmptyConstraints();
  void DetectDuplicateColumns();
  // Detects variable that must take different values.
  void DetectDifferentVariables();

 private:
  // A simple helper that logs the rules applied so far and return INFEASIBLE.
  CpSolverStatus InfeasibleStatus();

  // If there is a large proportion of fixed variables, remap the whole proto
  // before we start the presolve.
  bool MaybeRemoveFixedVariables(std::vector<int>* postsolve_mapping);

  // Runs the inner loop of the presolver.
  bool ProcessChangedVariables(std::vector<bool>* in_queue,
                               std::deque<int>* queue);
  void PresolveToFixPoint();

  // Runs the probing.
  void Probe();

  // Runs the expansion and fix constraints that became non-canonical.
  void ExpandCpModelAndCanonicalizeConstraints();

  // Presolve functions.
  //
  // They should return false only if the constraint <-> variable graph didn't
  // change. This is just an optimization, returning true is always correct.
  //
  // Invariant about UNSAT: All these functions should abort right away if
  // context_.IsUnsat() is true. And the only way to change the status to unsat
  // is through ABSL_MUST_USE_RESULT function that should also abort right away
  // the current code. This way we shouldn't keep doing computation on an
  // inconsistent state.
  // TODO(user): Make these public and unit test.
  bool PresolveAllDiff(ConstraintProto* ct);
  bool PresolveAutomaton(ConstraintProto* ct);
  bool PresolveElement(int c, ConstraintProto* ct);
  bool PresolveIntDiv(int c, ConstraintProto* ct);
  bool PresolveIntMod(int c, ConstraintProto* ct);
  bool PresolveIntProd(ConstraintProto* ct);
  bool PresolveInterval(int c, ConstraintProto* ct);
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
  bool PresolveCumulative(ConstraintProto* ct);
  bool PresolveNoOverlap(ConstraintProto* ct);
  bool PresolveNoOverlap2D(int c, ConstraintProto* ct);
  bool PresolveReservoir(ConstraintProto* ct);

  bool PresolveCircuit(ConstraintProto* ct);
  bool PresolveRoutes(ConstraintProto* ct);

  bool PresolveAtMostOrExactlyOne(ConstraintProto* ct);
  bool PresolveAtMostOne(ConstraintProto* ct);
  bool PresolveExactlyOne(ConstraintProto* ct);

  bool PresolveBoolAnd(ConstraintProto* ct);
  bool PresolveBoolOr(ConstraintProto* ct);
  bool PresolveBoolXor(ConstraintProto* ct);
  bool PresolveEnforcementLiteral(ConstraintProto* ct);

  // Regroups terms and substitute affine relations.
  // Returns true if the set of variables in the expression changed.
  bool CanonicalizeLinearExpression(const ConstraintProto& ct,
                                    LinearExpressionProto* proto);
  bool CanonicalizeLinearArgument(const ConstraintProto& ct,
                                  LinearArgumentProto* proto);

  // For the linear constraints, we have more than one function.
  ABSL_MUST_USE_RESULT bool CanonicalizeLinear(ConstraintProto* ct,
                                               bool* changed);
  bool PropagateDomainsInLinear(int ct_index, ConstraintProto* ct);
  bool RemoveSingletonInLinear(ConstraintProto* ct);
  bool PresolveSmallLinear(ConstraintProto* ct);
  bool PresolveLinearOfSizeOne(ConstraintProto* ct);
  bool PresolveLinearOfSizeTwo(ConstraintProto* ct);
  bool PresolveLinearOnBooleans(ConstraintProto* ct);
  bool PresolveDiophantine(ConstraintProto* ct);
  bool AddVarAffineRepresentativeFromLinearEquality(int target_index,
                                                    ConstraintProto* ct);
  bool PresolveLinearEqualityWithModulo(ConstraintProto* ct);

  // If a constraint is of the form "a * expr_X + expr_Y" and expr_Y can only
  // take small values compared to a, depending on the bounds, the constraint
  // can be equivalent to a constraint on expr_X only.
  //
  // For instance "10'001 X + 9'999 Y <= 105'000, with X, Y in [0, 100]" can
  // be rewritten as X + Y <= 10 ! This can easily happen after scaling to
  // integer cofficient a floating point constraint.
  void TryToReduceCoefficientsOfLinearConstraint(int c, ConstraintProto* ct);

  // This detects and converts constraints of the form:
  // "X = sum Boolean * value", with "sum Boolean <= 1".
  //
  // Note that it is not super fast, so it shouldn't be called too often.
  void ExtractEncodingFromLinear();
  bool ProcessEncodingFromLinear(int linear_encoding_ct_index,
                                 const ConstraintProto& at_most_or_exactly_one,
                                 int64_t* num_unique_terms,
                                 int64_t* num_multiple_terms);

  // Remove duplicate constraints. This also merge domain of linear constraints
  // with duplicate linear expressions.
  void DetectDuplicateConstraints();
  void DetectDuplicateConstraintsWithDifferentEnforcements(
      const CpModelMapping* mapping = nullptr,
      BinaryImplicationGraph* implication_graph = nullptr,
      Trail* trail = nullptr);

  // Detects if a linear constraint is "included" in another one, and do
  // related presolve.
  void DetectDominatedLinearConstraints();

  // Precomputes info about at most one, and use it to presolve linear
  // constraints. It can be interesting to know for a given linear constraint
  // that a subset of its variables are in at most one relation.
  void ProcessAtMostOneAndLinear();
  void ProcessOneLinearWithAmo(int ct_index, ConstraintProto* ct,
                               ActivityBoundHelper* helper);

  // Presolve a no_overlap_2d constraint where all the non-fixed rectangles are
  // framed by exactly four fixed rectangles and at most one single box can fit
  // inside the frame. This is a rather specific situation, but it is fast to
  // check and happens often in LNS problems.
  bool PresolveNoOverlap2DFramed(
      absl::Span<const Rectangle> fixed_boxes,
      absl::Span<const RectangleInRange> non_fixed_boxes, ConstraintProto* ct);

  // Detects when the space where items of a no_overlap_2d constraint can placed
  // is disjoint (ie., fixed boxes split the domain). When it is the case, we
  // can introduce a boolean for each pair <item, component> encoding whether
  // the item is in the component or not. Then we replace the original
  // no_overlap_2d constraint by one no_overlap_2d constraint for each
  // component, with the new booleans as the enforcement_literal of the
  // intervals. This is equivalent to expanding the original no_overlap_2d
  // constraint into a bin packing problem with each connected component being a
  // bin.
  bool ExpandEncoded2DBinPacking(
      absl::Span<const Rectangle> fixed_boxes,
      absl::Span<const RectangleInRange> non_fixed_boxes, ConstraintProto* ct);

  // SetPPC is short for set packing, partitioning and covering constraints.
  // These are sum of booleans <=, = and >= 1 respectively.
  // We detect inclusion of these constraint which allows a few simplifications.
  void ProcessSetPPC();

  // Detect if one constraints has a subset of enforcement of another.
  void DetectIncludedEnforcement();

  // Removes dominated constraints or fixes some variables for given pair of
  // setppc constraints included in each other.
  bool ProcessSetPPCSubset(int subset_c, int superset_c,
                           absl::flat_hash_set<int>* tmp_set,
                           bool* remove_subset, bool* remove_superset,
                           bool* stop_processing_superset);

  // Run SAT specific presolve code.
  // Returns false on UNSAT.
  bool PresolvePureSatPart();

  // Extracts AtMostOne constraint from Linear constraint.
  void ExtractAtMostOneFromLinear(ConstraintProto* ct);

  // Returns true if the constraint changed.
  bool DivideLinearByGcd(ConstraintProto* ct);

  void ExtractEnforcementLiteralFromLinearConstraint(int ct_index,
                                                     ConstraintProto* ct);
  void LowerThanCoeffStrengthening(bool from_lower_bound, int64_t min_magnitude,
                                   int64_t rhs, ConstraintProto* ct);

  // Extracts cliques from bool_and and small at_most_one constraints and
  // transforms them into maximal cliques.
  void TransformIntoMaxCliques();

  // Checks if there are any clauses that can be transformed to an at most
  // one constraint.
  void TransformClausesToExactlyOne();

  // Converts bool_or and at_most_one of size 2 to bool_and.
  void ConvertToBoolAnd();

  // Sometimes an upper bound on the objective can reduce the domains of many
  // variables. This "propagates" the objective like a normal linear constraint.
  bool PropagateObjective();

  // Try to reformulate the objective in term of "base" variables. This is
  // mainly useful for core based approach where having more terms in the
  // objective (but with a same trivial lower bound) should help.
  void ExpandObjective();

  // This makes a big difference on the flatzinc mznc2017_aes_opt* problems.
  // Where, with this, the core based approach can find small cores and close
  // them quickly.
  //
  // TODO(user): Is it by chance or there is a underlying deep reason? try to
  // merge this with what ExpandObjective() is doing.
  void ShiftObjectiveWithExactlyOnes();

  void MaybeTransferLinear1ToAnotherVariable(int var);
  void ProcessVariableOnlyUsedInEncoding(int var);
  void TryToSimplifyDomain(int var);

  void LookAtVariableWithDegreeTwo(int var);
  void ProcessVariableInTwoAtMostOrExactlyOne(int var);

  bool MergeCliqueConstraintsHelper(std::vector<std::vector<Literal>>& cliques,
                                    std::string_view entry_name,
                                    PresolveTimer& timer);
  bool MergeNoOverlapConstraints();
  bool MergeNoOverlap2DConstraints();

  // Assumes that all [constraint_index, multiple] in block are linear
  // constraint that contains multiple * common_part and perform the
  // substitution.
  //
  // Returns false if the substitution cannot be performed because the equation
  // common_part = new_variable is a linear equation with potential overflow.
  //
  // TODO(user): I would be great to change the overflow precondition so that
  // this cannot happen by maybe taking the rhs into account?
  bool RemoveCommonPart(
      const absl::flat_hash_map<int, int64_t>& common_var_coeff_map,
      absl::Span<const std::pair<int, int64_t>> block,
      ActivityBoundHelper* helper);

  // Try to identify many linear constraints that share a common linear
  // expression. We have two slightly different heuristic.
  //
  // TODO(user): consolidate them.
  void FindAlmostIdenticalLinearConstraints();
  void FindBigAtMostOneAndLinearOverlap(ActivityBoundHelper* helper);
  void FindBigHorizontalLinearOverlap(ActivityBoundHelper* helper);
  void FindBigVerticalLinearOverlap(ActivityBoundHelper* helper);

  // Heuristic to merge clauses that differ in only one literal.
  // The idea is to regroup a bunch of clauses into a single bool_and.
  // This serves a bunch of purpose:
  // - Smaller model.
  // - Stronger dual reasoning since less locks.
  // - If the negation of the rhs of the bool_and are in at most one, we will
  //   have a stronger LP relaxation.
  //
  // TODO(user): If the merge procedure is successful we might want to develop
  // a custom propagators for such bool_and. It should in theory be more
  // efficient than the two watcher literal scheme for clauses. Investigate!
  void MergeClauses();

  void RunPropagatorsForConstraint(const ConstraintProto& ct);

  // Boths function are responsible for dealing with affine relations.
  // The second one returns false on UNSAT.
  void EncodeAllAffineRelations();
  bool PresolveAffineRelationIfAny(int var);

  bool ExploitEquivalenceRelations(int c, ConstraintProto* ct);

  ABSL_MUST_USE_RESULT bool RemoveConstraint(ConstraintProto* ct);
  ABSL_MUST_USE_RESULT bool MarkConstraintAsFalse(ConstraintProto* ct,
                                                  std::string_view reason);

  std::vector<int>* postsolve_mapping_;
  PresolveContext* context_;
  SolutionCrush& solution_crush_;
  SolverLogger* logger_;
  TimeLimit* time_limit_;

  // Used by CanonicalizeLinearExpressionInternal().
  std::vector<std::pair<int, int64_t>> tmp_terms_;

  // Used by DetectAndProcessAtMostOneInLinear().
  std::vector<std::array<int64_t, 2>> conditional_mins_;
  std::vector<std::array<int64_t, 2>> conditional_maxs_;

  // Used by ProcessSetPPCSubset() and DetectAndProcessAtMostOneInLinear() to
  // propagate linear with an at_most_one or exactly_one included inside.
  absl::flat_hash_map<int, int> temp_map_;
  absl::flat_hash_set<int> temp_set_;
  ConstraintProto temp_ct_;

  // Used by RunPropagatorsForConstraint().
  CpModelProto tmp_model_;

  // Use by TryToReduceCoefficientsOfLinearConstraint().
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

  // We have an hash-map of know relation between two variables.
  // In particular, this will include all known precedences a <= b.
  //
  // We reuse an IntegerVariable/IntegerValue based class via
  // GetLinearExpression2FromProto() only visible in the .cc.
  BestBinaryRelationBounds known_linear2_;

  struct IntervalConstraintEq {
    const CpModelProto* working_model;
    bool operator()(int a, int b) const;
  };

  struct IntervalConstraintHash {
    const CpModelProto* working_model;
    std::size_t operator()(int ct_idx) const;
  };

  // Used by DetectDuplicateIntervals() and RemoveEmptyConstraints(). Note that
  // changing the interval constraints of the model will change the hash and
  // invalidate this hash map.
  absl::flat_hash_map<int, int, IntervalConstraintHash, IntervalConstraintEq>
      interval_representative_;
};

// Convenient wrapper to call the full presolve.
CpSolverStatus PresolveCpModel(PresolveContext* context,
                               std::vector<int>* postsolve_mapping);

// Returns the index of duplicate constraints in the given proto in the first
// element of each pair. The second element of each pair is the "representative"
// that is the first constraint in the proto in a set of duplicate constraints.
//
// Empty constraints are ignored. We also do a bit more:
// - We ignore names when comparing constraint.
// - For linear constraints, we ignore the domain. This is because we can
//   just merge them if the constraints are the same.
// - We return the special kObjectiveConstraint (< 0) representative if a linear
//   constraint is parallel to the objective and has no enforcement literals.
//   The domain of such constraint can just be merged with the objective domain.
//
// If ignore_enforcement is true, we ignore enforcement literal, but do not
// do the linear domain or objective special cases. This allow to cover some
// other cases like:
// - enforced constraint duplicate of non-enforced one.
// - Two enforced constraints with singleton enforcement (vpphard).
//
// Visible here for testing. This is meant to be called at the end of the
// presolve where constraints have been canonicalized.
std::vector<std::pair<int, int>> FindDuplicateConstraints(
    const CpModelProto& model_proto, bool ignore_enforcement = false);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
