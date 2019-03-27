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
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

struct PresolveOptions {
  bool log_info = true;
  SatParameters parameters;
  TimeLimit* time_limit = nullptr;
};

// Wrap the CpModelProto we are presolving with extra data structure like the
// in-memory domain of each variables and the constraint variable graph.
struct PresolveContext {
  // Helpers to adds new variables to the presolved model.
  int NewIntVar(const Domain& domain);
  int NewBoolVar();
  int GetOrCreateConstantVar(int64 cst);

  // a => b.
  void AddImplication(int a, int b);

  // b => x in [lb, ub].
  void AddImplyInDomain(int b, int x, const Domain& domain);

  // Helpers to query the current domain of a variable.
  bool DomainIsEmpty(int ref) const;
  bool IsFixed(int ref) const;
  bool LiteralIsTrue(int lit) const;
  bool LiteralIsFalse(int lit) const;
  int64 MinOf(int ref) const;
  int64 MaxOf(int ref) const;
  bool DomainContains(int ref, int64 value) const;
  Domain DomainOf(int ref) const;

  // Returns true if this ref only appear in one constraint.
  bool VariableIsUniqueAndRemovable(int ref) const;

  // Returns false if the new domain is empty. Sets 'domain_modified' (if
  // provided) to true iff the domain is modified otherwise does not change it.
  ABSL_MUST_USE_RESULT bool IntersectDomainWith(
      int ref, const Domain& domain, bool* domain_modified = nullptr);

  // Returns false if the 'lit' doesn't have the desired value in the domain.
  ABSL_MUST_USE_RESULT bool SetLiteralToFalse(int lit);
  ABSL_MUST_USE_RESULT bool SetLiteralToTrue(int lit);

  // This function always return false. It is just a way to make a little bit
  // more sure that we abort right away when infeasibility is detected.
  ABSL_MUST_USE_RESULT bool NotifyThatModelIsUnsat() {
    DCHECK(!is_unsat);
    is_unsat = true;
    return false;
  }
  bool ModelIsUnsat() const { return is_unsat; }

  // Stores a description of a rule that was just applied to have a summary of
  // what the presolve did at the end.
  void UpdateRuleStats(const std::string& name);

  // Update the constraints <-> variables graph. This needs to be called each
  // time a constraint is modified.
  void UpdateConstraintVariableUsage(int c);

  // Calls UpdateConstraintVariableUsage() on all newly created constraints.
  void UpdateNewConstraintsVariableUsage();

  // Returns true if our current constraints <-> variables graph is ok.
  // This is meant to be used in DEBUG mode only.
  bool ConstraintVariableUsageIsConsistent();

  // Regroups fixed variables with the same value.
  // TODO(user): Also regroup cte and -cte?
  void ExploitFixedDomain(int var);

  // Adds the relation (ref_x = coeff * ref_y + offset) to the repository.
  void StoreAffineRelation(const ConstraintProto& ct, int ref_x, int ref_y,
                           int64 coeff, int64 offset);

  void StoreBooleanEqualityRelation(int ref_a, int ref_b);

  // This makes sure that the affine relation only uses one of the
  // representative from the var_equiv_relations.
  AffineRelation::Relation GetAffineRelation(int ref);

  // Create the internal structure for any new variables in working_model.
  void InitializeNewDomains();

  // Gets the associated literal if it is already created. Otherwise
  // create it, add the corresponding constraints and returns it.
  int GetOrCreateVarValueEncoding(int ref, int64 value);

  // This regroup all the affine relations between variables. Note that the
  // constraints used to detect such relations will not be removed from the
  // model at detection time (thus allowing proper domain propagation). However,
  // if the arity of a variable becomes one, then such constraint will be
  // removed.
  AffineRelation affine_relations;
  AffineRelation var_equiv_relations;

  // Set of constraint that implies an "affine relation". We need to mark them,
  // because we can't simplify them using the relation they added.
  //
  // WARNING: This assumes the ConstraintProto* to stay valid during the full
  // presolve even if we add new constraint to the CpModelProto.
  absl::flat_hash_set<ConstraintProto const*> affine_constraints;

  // For each constant variable appearing in the model, we maintain a reference
  // variable with the same constant value. If two variables end up having the
  // same fixed value, then we can detect it using this and add a new
  // equivalence relation. See ExploitFixedDomain().
  absl::flat_hash_map<int64, int> constant_to_ref;

  // Contains fully expanded variables.
  // expanded_variables[std::pair(i, v)] point to the literal attached to the
  // value v of the variable i.
  absl::flat_hash_map<std::pair<int, int64>, int> encoding;

  // Variable <-> constraint graph.
  // The vector list is sorted and contains unique elements.
  //
  // Important: To properly handle the objective, var_to_constraints[objective]
  // contains -1 so that if the objective appear in only one constraint, the
  // constraint cannot be simplified.
  //
  // TODO(user): Make this private?
  std::vector<std::vector<int>> constraint_to_vars;
  std::vector<absl::flat_hash_set<int>> var_to_constraints;

  // We maintain how many time each interval is used.
  std::vector<std::vector<int>> constraint_to_intervals;
  std::vector<int> interval_usage;

  CpModelProto* working_model;
  CpModelProto* mapping_model;

  // Indicate if we are enumerating all solutions. This disable some presolve
  // rules.
  bool enumerate_all_solutions = false;

  // Just used to display statistics on the presolve rules that were used.
  absl::flat_hash_map<std::string, int> stats_by_rule_name;

  // Temporary storage.
  std::vector<int> tmp_literals;
  std::vector<Domain> tmp_term_domains;
  std::vector<Domain> tmp_left_domains;
  absl::flat_hash_set<int> tmp_literal_set;

  // Each time a domain is modified this is set to true.
  SparseBitset<int64> modified_domains;

 private:
  void AddVariableUsage(int c);

  // Initially false, and set to true on the first inconsistency.
  bool is_unsat = false;

  // The current domain of each variables.
  std::vector<Domain> domains;
};

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
//
// Returns false if a non-recoverable error was encountered.
//
// TODO(user): Make sure this can never run into this case provided that the
// initial model is valid!
bool PresolveCpModel(const PresolveOptions& options,
                     CpModelProto* presolved_model, CpModelProto* mapping_model,
                     std::vector<int>* postsolve_mapping);

// Replaces all the instance of a variable i (and the literals referring to it)
// by mapping[i]. The definition of variables i is also moved to its new index.
// Variables with a negative mapping value are ignored and it is an error if
// such variable is referenced anywhere (this is CHECKed).
//
// The image of the mapping should be dense in [0, new_num_variables), this is
// also CHECKed.
void ApplyVariableMapping(const std::vector<int>& mapping, CpModelProto* proto);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_PRESOLVE_H_
