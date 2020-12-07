// Copyright 2010-2018 Google LLC
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
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
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
  CpModelPresolver(const PresolveOptions& options, PresolveContext* context,
                   std::vector<int>* postsolve_mapping);

  // Returns false if a non-recoverable error was encountered.
  //
  // TODO(user): Make sure this can never run into this case provided that the
  // initial model is valid!
  bool Presolve();

  // Executes presolve method for the given constraint. Public for testing only.
  bool PresolveOneConstraint(int c);

  // Public for testing only.
  void RemoveEmptyConstraints();

 private:
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
  // TODO(user,user): Make these public and unit test.
  bool PresolveAutomaton(ConstraintProto* ct);
  bool PresolveCircuit(ConstraintProto* ct);
  bool PresolveRoutes(ConstraintProto* ct);
  bool PresolveCumulative(ConstraintProto* ct);
  bool PresolveNoOverlap(ConstraintProto* ct);
  bool PresolveAllDiff(ConstraintProto* ct);
  bool PresolveTable(ConstraintProto* ct);
  bool PresolveElement(ConstraintProto* ct);
  bool PresolveInterval(int c, ConstraintProto* ct);
  bool PresolveIntDiv(ConstraintProto* ct);
  bool PresolveIntProd(ConstraintProto* ct);
  bool PresolveIntMin(ConstraintProto* ct);
  bool PresolveIntMax(ConstraintProto* ct);
  bool PresolveLinMin(ConstraintProto* ct);
  bool PresolveLinMax(ConstraintProto* ct);
  bool PresolveIntAbs(ConstraintProto* ct);
  bool PresolveBoolXor(ConstraintProto* ct);
  bool PresolveAtMostOne(ConstraintProto* ct);
  bool PresolveBoolAnd(ConstraintProto* ct);
  bool PresolveBoolOr(ConstraintProto* ct);
  bool PresolveEnforcementLiteral(ConstraintProto* ct);

  // For the linear constraints, we have more than one function.
  bool CanonicalizeLinear(ConstraintProto* ct);
  bool PropagateDomainsInLinear(int c, ConstraintProto* ct);
  bool RemoveSingletonInLinear(ConstraintProto* ct);
  bool PresolveSmallLinear(ConstraintProto* ct);
  bool PresolveLinearOnBooleans(ConstraintProto* ct);
  void PresolveLinearEqualityModuloTwo(ConstraintProto* ct);

  // SetPPC is short for set packing, partitioning and covering constraints.
  // These are sum of booleans <=, = and >= 1 respectively.
  bool ProcessSetPPC();

  // Removes dominated constraints or fixes some variables for given pair of
  // setppc constraints. This assumes that literals in constraint c1 is subset
  // of literals in constraint c2.
  bool ProcessSetPPCSubset(int c1, int c2, const std::vector<int>& c2_minus_c1,
                           const std::vector<int>& original_constraint_index,
                           std::vector<bool>* marked_for_removal);

  void PresolvePureSatPart();

  // Extracts AtMostOne constraint from Linear constraint.
  void ExtractAtMostOneFromLinear(ConstraintProto* ct);

  void DivideLinearByGcd(ConstraintProto* ct);

  void ExtractEnforcementLiteralFromLinearConstraint(int ct_index,
                                                     ConstraintProto* ct);

  // Extracts cliques from bool_and and small at_most_one constraints and
  // transforms them into maximal cliques.
  void TransformIntoMaxCliques();

  // Converts bool_or and at_most_one of size 2 to bool_and.
  void ExtractBoolAnd();

  void ExpandObjective();

  void TryToSimplifyDomain(int var);

  void MergeNoOverlapConstraints();

  // Boths function are responsible for dealing with affine relations.
  // The second one returns false on UNSAT.
  void EncodeAllAffineRelations();
  bool PresolveAffineRelationIfAny(int var);

  bool IntervalsCanIntersect(const IntervalConstraintProto& interval1,
                             const IntervalConstraintProto& interval2);

  bool ExploitEquivalenceRelations(int c, ConstraintProto* ct);

  ABSL_MUST_USE_RESULT bool RemoveConstraint(ConstraintProto* ct);
  ABSL_MUST_USE_RESULT bool MarkConstraintAsFalse(ConstraintProto* ct);

  const PresolveOptions& options_;
  std::vector<int>* postsolve_mapping_;
  PresolveContext* context_;

  // Used by CanonicalizeLinear().
  std::vector<std::pair<int, int64>> tmp_terms_;
};

// Convenient wrapper to call the full presolve.
bool PresolveCpModel(const PresolveOptions& options, PresolveContext* context,
                     std::vector<int>* postsolve_mapping);

// Returns the index of exact duplicate constraints in the given proto. That
// is, all returned constraints will have an identical constraint before it in
// the model_proto.constraints() list. Empty constraints are ignored.
//
// Visible here for testing. This is meant to be called at the end of the
// presolve where constraints have been canonicalized.
//
// TODO(user): Ignore names? canonicalize constraint further by sorting
// enforcement literal list for instance...
std::vector<int> FindDuplicateConstraints(const CpModelProto& model_proto);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
