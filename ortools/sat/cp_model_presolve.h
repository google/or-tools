// Copyright 2010-2021 Google LLC
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

#include <cstdint>
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
  CpModelPresolver(PresolveContext* context,
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
  // TODO(user): Make these public and unit test.
  bool ConvertIntMax(ConstraintProto* ct);
  bool PresolveAllDiff(ConstraintProto* ct);
  bool PresolveAutomaton(ConstraintProto* ct);
  bool PresolveElement(ConstraintProto* ct);
  bool PresolveIntAbs(ConstraintProto* ct);
  bool PresolveIntDiv(ConstraintProto* ct);
  bool PresolveIntMax(ConstraintProto* ct);
  bool PresolveIntMin(ConstraintProto* ct);
  bool PresolveIntMod(ConstraintProto* ct);
  bool PresolveIntProd(ConstraintProto* ct);
  bool PresolveInterval(int c, ConstraintProto* ct);
  bool PresolveInverse(ConstraintProto* ct);
  bool PresolveLinMax(ConstraintProto* ct);
  bool PresolveLinMin(ConstraintProto* ct);
  bool PresolveTable(ConstraintProto* ct);

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

  // For the linear constraints, we have more than one function.
  bool CanonicalizeLinear(ConstraintProto* ct);
  bool PropagateDomainsInLinear(int c, ConstraintProto* ct);
  bool RemoveSingletonInLinear(ConstraintProto* ct);
  bool PresolveSmallLinear(ConstraintProto* ct);
  bool PresolveLinearOnBooleans(ConstraintProto* ct);
  void PresolveLinearEqualityModuloTwo(ConstraintProto* ct);

  // Scheduling helpers.
  void AddLinearConstraintFromInterval(const ConstraintProto& ct);

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

  bool ExploitEquivalenceRelations(int c, ConstraintProto* ct);

  ABSL_MUST_USE_RESULT bool RemoveConstraint(ConstraintProto* ct);
  ABSL_MUST_USE_RESULT bool MarkConstraintAsFalse(ConstraintProto* ct);

  std::vector<int>* postsolve_mapping_;
  PresolveContext* context_;
  SolverLogger* logger_;

  // Used by CanonicalizeLinearExpressionInternal().
  std::vector<std::pair<int, int64_t>> tmp_terms_;
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
  bool ImportAndSimplifyConstraints(
      const CpModelProto& in_model,
      const std::vector<int>& ignored_constraints);

 private:
  // Overwrites the out_model to be unsat. Returns false.
  bool CreateUnsatModel();

  void CopyEnforcementLiterals(const ConstraintProto& orig,
                               ConstraintProto* dest);
  bool OneEnforcementLiteralIsFalse(const ConstraintProto& ct) const;

  // All these functions return false if the constraint is found infeasible.
  bool CopyBoolOr(const ConstraintProto& ct);
  bool CopyBoolAnd(const ConstraintProto& ct);
  bool CopyLinear(const ConstraintProto& ct);
  bool CopyAtMostOne(const ConstraintProto& ct);
  bool CopyExactlyOne(const ConstraintProto& ct);
  bool CopyInterval(const ConstraintProto& ct, int c);

  PresolveContext* context_;
  int64_t skipped_non_zero_ = 0;

  // Temp vectors.
  std::vector<int> non_fixed_variables_;
  std::vector<int64_t> non_fixed_coefficients_;
  absl::flat_hash_map<int, int> interval_mapping_;
  int starting_constraint_index_ = 0;
  std::vector<int> temp_enforcement_literals_;
  std::vector<int> temp_literals_;
};

// Import the constraints from the in_model to the presolve context.
// It performs on the fly simplification, and returns false if the
// model is proved infeasible.
bool ImportConstraintsWithBasicPresolveIntoContext(const CpModelProto& in_model,
                                                   PresolveContext* context);

// Copies the non constraint, non variables part of the model.
void CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(
    const CpModelProto& in_model, PresolveContext* context);

// Convenient wrapper to call the full presolve.
bool PresolveCpModel(PresolveContext* context,
                     std::vector<int>* postsolve_mapping);

// Returns the index of duplicate constraints in the given proto in the first
// element of each pair. The second element of each pair is the "representative"
// that is the first constraint in the proto in a set of duplicate constraints.
//
// Empty constraints are ignored. We also do a bit more:
// - We ignore names when comparing constraint.
// - For linear constraints, we ignore the domain. This is because we can
//   just merge them if the constraints are the same.
//
// Visible here for testing. This is meant to be called at the end of the
// presolve where constraints have been canonicalized.
std::vector<std::pair<int, int>> FindDuplicateConstraints(
    const CpModelProto& model_proto);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
