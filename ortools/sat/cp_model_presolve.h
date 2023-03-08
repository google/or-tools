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

#ifndef OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
#define OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Replaces all the instance of a variable i (and the literals referring to it)
// by mapping[i]. The definition of variables i is also moved to its new index.
// Variables with a negative mapping value are ignored and it is an error if
// such variable is referenced anywhere (this is CHECKed).
//
// The image of the mapping should be dense in [0, new_num_variables), this is
// also CHECKed.
void ApplyVariableMapping(const std::vector<int>& mapping,
                          const PresolveContext& context);

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

  // Public for testing only.
  void RemoveEmptyConstraints();

 private:
  // A simple helper that logs the rules applied so far and return INFEASIBLE.
  CpSolverStatus InfeasibleStatus();

  // Runs the inner loop of the presolver.
  void PresolveToFixPoint();

  // Runs the probing.
  void Probe();

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
  bool PresolveElement(ConstraintProto* ct);
  bool PresolveIntAbs(ConstraintProto* ct);
  bool PresolveIntDiv(ConstraintProto* ct);
  bool PresolveIntMod(ConstraintProto* ct);
  bool PresolveIntProd(ConstraintProto* ct);
  bool PresolveInterval(int c, ConstraintProto* ct);
  bool PresolveInverse(ConstraintProto* ct);
  bool PresolveLinMax(ConstraintProto* ct);
  bool PresolveLinMaxWhenAllBoolean(ConstraintProto* ct);
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
  template <typename ProtoWithVarsAndCoeffs>
  bool CanonicalizeLinearExpressionInternal(const ConstraintProto& ct,
                                            ProtoWithVarsAndCoeffs* proto,
                                            int64_t* offset);
  bool CanonicalizeLinearExpression(const ConstraintProto& ct,
                                    LinearExpressionProto* exp);
  bool CanonicalizeLinearArgument(const ConstraintProto& ct,
                                  LinearArgumentProto* proto);

  // For the linear constraints, we have more than one function.
  bool CanonicalizeLinear(ConstraintProto* ct);
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

  // It can be interesting to know for a given linear constraint that a subset
  // of its variables are in at most one relation.
  void DetectAndProcessAtMostOneInLinear(int ct_index, ConstraintProto* ct,
                                         ActivityBoundHelper* helper);

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

  // Detects if a linear constraint is "included" in another one, and do
  // related presolve.
  void DetectDominatedLinearConstraints();

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
  void PresolvePureSatPart();

  // Extracts AtMostOne constraint from Linear constraint.
  void ExtractAtMostOneFromLinear(ConstraintProto* ct);

  // Returns true if the constraint changed.
  bool DivideLinearByGcd(ConstraintProto* ct);

  void ExtractEnforcementLiteralFromLinearConstraint(int ct_index,
                                                     ConstraintProto* ct);
  void LowerThanCoeffStrengthening(bool from_lower_bound, int64_t min_magnitude,
                                   int64_t threshold, ConstraintProto* ct);

  // Extracts cliques from bool_and and small at_most_one constraints and
  // transforms them into maximal cliques.
  void TransformIntoMaxCliques();

  // Converts bool_or and at_most_one of size 2 to bool_and.
  void ExtractBoolAnd();

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

  void ProcessVariableOnlyUsedInEncoding(int var);
  void TryToSimplifyDomain(int var);

  void LookAtVariableWithDegreeTwo(int var);
  void ProcessVariableInTwoAtMostOrExactlyOne(int var);

  void MergeNoOverlapConstraints();

  // Try to identify many linear constraints that share a common linear
  // expression.
  void FindBigLinearOverlap();

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

  // Boths function are responsible for dealing with affine relations.
  // The second one returns false on UNSAT.
  void EncodeAllAffineRelations();
  bool PresolveAffineRelationIfAny(int var);

  bool ExploitEquivalenceRelations(int c, ConstraintProto* ct);

  ABSL_MUST_USE_RESULT bool RemoveConstraint(ConstraintProto* ct);
  ABSL_MUST_USE_RESULT bool MarkConstraintAsFalse(ConstraintProto* ct);

  std::vector<int>* postsolve_mapping_;
  PresolveContext* context_;
  SolverLogger* logger_;

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

  // Use by TryToReduceCoefficientsOfLinearConstraint().
  MaxBoundedSubsetSum lb_feasible_;
  MaxBoundedSubsetSum lb_infeasible_;
  MaxBoundedSubsetSum ub_feasible_;
  MaxBoundedSubsetSum ub_infeasible_;
};

// This helper class perform copy with simplification from a model and a
// partial assignment to another model. The purpose is to miminize the size of
// the copied model, as well as to reduce the pressure on the memory sub-system.
//
// It is currently used by the LNS part, but could be used with any other scheme
// that generates partial assignments.
class ModelCopy {
 public:
  explicit ModelCopy(PresolveContext* context);

  // Copies all constraints from in_model to working model of the context.
  //
  // During the process, it will read variable domains from the context, and
  // simplify constraints to minimize the size of the copied model.
  // Thus it is important that the context->working_model already have the
  // variables part copied.
  //
  // It returns false iff the model is proven infeasible.
  //
  // It does not clear the constraints part of the working model of the context.
  //
  // Note(user): If first_copy is true, we will reorder the scheduling
  // constraint so that they only use reference to previously defined intervals.
  // This allow to be more efficient later in a few preprocessing steps.
  bool ImportAndSimplifyConstraints(const CpModelProto& in_model,
                                    const std::vector<int>& ignored_constraints,
                                    bool first_copy = false);

  // Copy variables from the in_model to the working model.
  // It reads the 'ignore_names' parameters from the context, and keeps or
  // deletes names accordingly.
  void ImportVariablesAndMaybeIgnoreNames(const CpModelProto& in_model);

 private:
  // Overwrites the out_model to be unsat. Returns false.
  bool CreateUnsatModel();

  void CopyEnforcementLiterals(const ConstraintProto& orig,
                               ConstraintProto* dest);
  bool OneEnforcementLiteralIsFalse(const ConstraintProto& ct) const;

  // All these functions return false if the constraint is found infeasible.
  bool CopyBoolOr(const ConstraintProto& ct);
  bool CopyBoolOrWithDupSupport(const ConstraintProto& ct);
  bool CopyBoolAnd(const ConstraintProto& ct);
  bool CopyLinear(const ConstraintProto& ct);
  bool CopyAtMostOne(const ConstraintProto& ct);
  bool CopyExactlyOne(const ConstraintProto& ct);
  bool CopyInterval(const ConstraintProto& ct, int c, bool ignore_names);

  // These function remove unperformed intervals. Note that they requires
  // interval to appear before (validated) as they test unperformed by testing
  // if interval_mapping_ is empty.
  void CopyAndMapNoOverlap(const ConstraintProto& ct);
  void CopyAndMapNoOverlap2D(const ConstraintProto& ct);
  void CopyAndMapCumulative(const ConstraintProto& ct);

  PresolveContext* context_;
  int64_t skipped_non_zero_ = 0;

  // Temp vectors.
  std::vector<int> non_fixed_variables_;
  std::vector<int64_t> non_fixed_coefficients_;
  absl::flat_hash_map<int, int> interval_mapping_;
  int starting_constraint_index_ = 0;
  std::vector<int> temp_enforcement_literals_;

  std::vector<int> temp_literals_;
  absl::flat_hash_set<int> tmp_literals_set_;
};

// Copy in_model to the model in the presolve context.
// It performs on the fly simplification, and returns false if the
// model is proved infeasible. If reads the parameters 'ignore_names' and keeps
// or deletes variables and constraints names accordingly.
//
// This should only be called on the first copy of the user given model.
// Note that this reorder all constraints that use intervals last. We loose the
// user-defined order, but hopefully that should not matter too much.
bool ImportModelWithBasicPresolveIntoContext(const CpModelProto& in_model,
                                             PresolveContext* context);

// Copies the non constraint, non variables part of the model.
void CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(
    const CpModelProto& in_model, PresolveContext* context);

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
