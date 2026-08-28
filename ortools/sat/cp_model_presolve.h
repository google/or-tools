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

#ifndef ORTOOLS_SAT_CP_MODEL_PRESOLVE_H_
#define ORTOOLS_SAT_CP_MODEL_PRESOLVE_H_

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_constraint_presolve.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Replaces all the instances of a variable i (and the literals referring to it)
// by mapping[i] in the given cp_model. The definition of variables i is also
// moved to its new index.
//
// If mapping[i] < 0 the variable can be ignored if there are no references to
// it at all. If it is not possible (i.e. some field uses it), then we will use
// a new index for it (at the end) and reverse_mapping will be updated to
// reflect that. This is the only time we touch reverse_mapping.
// The image of the mapping should be dense in [0, reverse_mapping->size()).
//
// If mapping[i] == mapping[j], the variables will be merged, but it will be the
// IntegerVariableProto definition of max(i, j) that will be kept in the output.
// TODO(user): This behavior is not well unit-tested.
void ApplyVariableMapping(absl::Span<int> mapping, CpModelProto* cp_model,
                          std::vector<int>* reverse_mapping,
                          SolutionCrush& solution_crush);

// Presolves the initial content of presolved_model.
//
// This also creates a mapping model that encodes the correspondence between the
// two problems. This works as follows:
// - The first variables of mapping_model are in one-to-one correspondence with
//   the variables of the initial model.
// - The presolved_model variables are in one-to-one correspondence with the
//   variables at the indices given by postsolve_mapping in the mapping model.
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
// TODO(user): Identify disconnected components and return a vector of
// presolved models? If we go this route, it may be nicer to store the indices
// inside the model. We can add an IntegerVariableProto::initial_index;
class CpModelPresolver {
 public:
  CpModelPresolver(PresolveContext* context,
                   std::vector<int>* postsolve_mapping);

  // We return the status of the problem after presolve:
  //  - UNKNOWN if everything was ok.
  //  - INFEASIBLE if the model was proven so during presolve
  //  - MODEL_INVALID if the model caused some issues, like if we are not able
  //    to scale a floating point objective with enough precision.
  CpSolverStatus Presolve();

  // Visible for testing.
  void DetectDuplicateColumns();

  // Detects variables that must take different values.
  void DetectDifferentVariables();

 private:
  // A simple helper that logs the rules applied so far and returns INFEASIBLE.
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

  ABSL_MUST_USE_RESULT bool CanonicalizeAllLinears();

  // This detects and converts constraints of the form:
  // "X = sum Boolean * value", with "sum Boolean <= 1".
  //
  // Note that it is not super fast, so it shouldn't be called too often.
  void ExtractEncodingFromLinear();
  bool ProcessEncodingFromLinear(int linear_encoding_ct_index,
                                 const ConstraintProto& at_most_or_exactly_one,
                                 int64_t* num_unique_terms,
                                 int64_t* num_multiple_terms);

  // Remove duplicate constraints. This also merges domains of linear
  // constraints with duplicate linear expressions.
  void DetectDuplicateConstraints();
  void DetectDuplicateConstraintsWithDifferentEnforcements(
      const CpModelMapping* mapping = nullptr,
      BinaryImplicationGraph* implication_graph = nullptr,
      Trail* trail = nullptr);

  // A bit like DetectDuplicateConstraintsWithDifferentEnforcements() but
  // for linear constraints with different rhs.
  void DetectUnenforcedEnforcedLinearPair();

  // If var only appears in
  // literal => var \in domain
  // var + linear_terms \in other_domain which is trivial if var is relaxed.
  //
  // then we can remove var, and transform the constraint to
  // literal => linear_terms \in tighter_domain.
  void MaybeRemoveLinkingVariable(int var, int c_linear1, int c_linear);

  // Detects if a linear constraint is "included" in another one, and does
  // related presolve.
  void DetectDominatedLinearConstraints();

  // Detects encodings of the form:
  //   b1 => x \in Domain1
  //  ~b1 => x \in Domain1.Complement()
  //   b2 => x \in Domain2
  //  ~b2 => x \in Domain2.Complement()
  //   b3 => x \in Domain3
  //  ~b3 => x \in Domain3.Complement()
  //   ...
  //   bool_or(b1, b2, ..., bn, y, z, ...)

  // Where the bi do not appear in any other constraints. When we find this
  // pattern, we create a new boolean variable `l` and replace all the
  // constraints above by three new constraints:
  //   l => x \in Domain1 U Domain2 U ... U Domainn
  //  ~l => x \in (Domain1 U Domain2 U ... U Domainn).Complement()
  //   bool_or(l, y, z, ...),
  // Note that `l` is equivalent to at least one of the bi to be true, which is
  // a consequence that it is encoding a domain that is the union of the domains
  // of the bis.
  //
  // It does the same when bool_or is replaced by an at_most_one or exactly_one
  // but we need to add an extra constraint that
  //  x \notin (Domain_a U Domain_b) for all a != b.
  void DetectEncodedComplexDomains(PresolveContext* context);
  bool DetectEncodedComplexDomain(PresolveContext* context, ConstraintProto* ct,
                                  const Bitset64<int>& pertinent_bools);

  // Precomputes info about at most one, and uses it to presolve linear
  // constraints. It can be interesting to know for a given linear constraint
  // that a subset of its variables are in at most one relation.
  void ProcessAtMostOneAndLinear();
  void ProcessOneLinearWithAmo(int ct_index, ConstraintProto* ct,
                               ActivityBoundHelper* helper);

  // SetPPC is short for set packing, partitioning and covering constraints.
  // These are sum of booleans <=, = and >= 1 respectively.
  // We detect inclusion of these constraints which allows a few
  // simplifications.
  void ProcessSetPPC();

  // Detect if one constraint has a subset of enforcement of another.
  void DetectIncludedEnforcement();

  // Removes dominated constraints or fixes some variables for given pair of
  // setppc constraints included in each other.
  bool ProcessSetPPCSubset(int subset_c, int superset_c,
                           absl::flat_hash_set<int>* tmp_set,
                           bool* remove_subset, bool* remove_superset,
                           bool* stop_processing_superset);

  // Runs SAT specific presolve code.
  // Returns false on UNSAT.
  bool PresolvePureSatPart();

  // Runs a simplified presolve code for pure SAT problems.
  // Returns false on UNSAT.
  bool PresolvePureSatProblem();

  // Extracts AtMostOne constraint from Linear constraint.
  void ExtractAtMostOneFromLinear(ConstraintProto* ct);

  // Extracts cliques from bool_and and small at_most_one constraints and
  // transforms them into maximal cliques.
  void TransformIntoMaxCliques();

  // Checks if there are any clauses that can be transformed to an at-most-one
  // constraint.
  void TransformClausesToExactlyOne();

  // Use all the detected precedences to detect if a part of a no_overlap
  // constraint can only be executed after the rest and thus the no_overlap
  // constraint can be split into smaller no_overlap constraints.
  void SplitNoOverlapAndCumulativeConstraints();

  // Converts bool_or and at_most_one of size 2 to bool_and.
  void ConvertToBoolAnd();

  // Sometimes an upper bound on the objective can reduce the domains of many
  // variables. This "propagates" the objective like a normal linear constraint.
  bool PropagateObjective();

  // Try to reformulate the objective in terms of "base" variables. This is
  // mainly useful for a core-based approach where having more terms in the
  // objective (but with a same trivial lower bound) should help.
  void ExpandObjective();

  // This makes a big difference on the flatzinc mznc2017_aes_opt* problems.
  // Where, with this, the core-based approach can find small cores and close
  // them quickly.
  //
  // TODO(user): Is it by chance or is there an underlying deep reason? Try to
  // merge this with what ExpandObjective() is doing.
  void ShiftObjectiveWithExactlyOnes();

  void ProcessVariablesOnlyUsedInEncoding();

  void LookAtVariableWithDegreeTwo(int var);

  void PresolveVarOnlyInIntProdAndLinMax(int var, int int_prod_ct_index,
                                         int lin_max_ct_index);
  void PresolveVarOnlyInLinearAndLinear(int var, int linear1_ct_index,
                                        int linear2_ct_index);
  void PresolveVarOnlyInLinMaxAndLinear(int var, int lin_max_ct_index,
                                        int linear_ct_index);

  void ProcessVariableInTwoAtMostOrExactlyOne(int var);

  bool MergeCliqueConstraintsHelper(std::vector<std::vector<Literal>>& cliques,
                                    std::string_view entry_name,
                                    PresolveTimer& timer);
  bool MergeNoOverlapConstraints();
  bool MergeNoOverlap2DConstraints();

  // Assumes that all [constraint_index, multiple] in block are linear
  // constraints that contain multiple * common_part and performs the
  // substitution.
  //
  // Returns false if the substitution cannot be performed because the equation
  // common_part = new_variable is a linear equation with potential overflow.
  //
  // TODO(user): It would be great to change the overflow precondition so that
  // this cannot happen by maybe taking the rhs into account?
  bool RemoveCommonPart(
      const absl::flat_hash_map<int, int64_t>& common_var_coeff_map,
      absl::Span<const std::pair<int, int64_t>> block,
      ActivityBoundHelper* helper);

  // Try to identify many linear constraints that share a common linear
  // expression. We have two slightly different heuristics.
  //
  // TODO(user): consolidate them.
  void FindAlmostIdenticalLinearConstraints();
  void FindBigAtMostOneAndLinearOverlap(ActivityBoundHelper* helper);
  void FindBigHorizontalLinearOverlap(ActivityBoundHelper* helper);
  void FindBigVerticalLinearOverlap(ActivityBoundHelper* helper);

  // Heuristic to merge clauses that differ in only one literal.
  // The idea is to regroup a bunch of clauses into a single bool_and.
  // This serves a bunch of purposes:
  // - Smaller model.
  // - Stronger dual reasoning since less locks.
  // - If the negation of the rhs of the bool_and are in at most one, we will
  //   have a stronger LP relaxation.
  //
  // TODO(user): If the merge procedure is successful we might want to develop
  // a custom propagator for such bool_and. It should in theory be more
  // efficient than the two watcher literal scheme for clauses. Investigate!
  void MergeClauses();

  void EncodeAllAffineRelations();

  void MaybePermuteVariablesRandomly(std::vector<int>& mapping);
  CpSolverStatus LogAndValidatePresolvedModel();

  std::vector<int>* postsolve_mapping_;
  PresolveContext* context_;
  SolutionCrush& solution_crush_;
  std::unique_ptr<CpConstraintPresolver> constraint_presolver_;
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

  // Used by ProcessVariablesOnlyUsedInEncoding()
  int encoding_tmp_num_vars_ = 0;
  std::vector<int> encoding_tmp_vars_;
};

// Convenient wrapper to call the full presolve.
CpSolverStatus PresolveCpModel(PresolveContext* context,
                               std::vector<int>* postsolve_mapping);

// Returns the index of duplicate constraints in the given proto in the first
// element of each pair. The second element of each pair is the "representative"
// that is the first constraint in the proto in a set of duplicate constraints.
//
// Empty constraints are ignored. We also do a bit more:
// - We ignore names when comparing constraints.
// - For linear constraints, we ignore the domain if ignore_linear_domain is
//   true. This is because we can just merge them if the constraints are the
//   same.
// - We return the special kObjectiveConstraint (< 0) representative if a linear
//   constraint is parallel to the objective and has no enforcement literals.
//   The domain of such constraints can just be merged with the objective
//   domain.
//
// If ignore_enforcement is true, we ignore enforcement literals. This allows
// covering some other cases like:
// - enforced constraint duplicate of non-enforced one.
// - Two enforced constraints with singleton enforcement (vpphard).
//
// Visible here for testing. This is meant to be called at the end of the
// presolve where constraints have been canonicalized.
std::vector<std::pair<int, int>> FindDuplicateConstraints(
    const CpModelProto& model_proto, bool ignore_enforcement,
    bool ignore_linear_domain, bool ignore_target_of_expression);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CP_MODEL_PRESOLVE_H_
