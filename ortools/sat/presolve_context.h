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

#ifndef OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
#define OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
#include <vector>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_util.h"
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
  explicit PresolveContext(CpModelProto* model, CpModelProto* mapping)
      : working_model(model), mapping_model(mapping) {}

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

  // This function takes a positive variable reference.
  bool DomainOfVarIsIncludedIn(int var, const Domain& domain) {
    return domains[var].IsIncludedIn(domain);
  }

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

  // Updates the constraints <-> variables graph. This needs to be called each
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

  // Creates the internal structure for any new variables in working_model.
  void InitializeNewDomains();

  // Clears the "rules" statistics.
  void ClearStats();

  // Inserts the given literal to encode ref == value.
  // If an encoding already exists, it adds the two implications between
  // the previous encoding and the new encoding.
  void InsertVarValueEncoding(int literal, int ref, int64 value);

  // Gets the associated literal if it is already created. Otherwise
  // create it, add the corresponding constraints and returns it.
  int GetOrCreateVarValueEncoding(int ref, int64 value);

  // This regroups all the affine relations between variables. Note that the
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

  CpModelProto* working_model = nullptr;
  CpModelProto* mapping_model = nullptr;

  // Indicate if we are allowed to remove irrelevant feasible solution from the
  // set of feasible solution. For example, if a variable is unused, can we fix
  // it to an arbitrary value (or its mimimum objective one)? This must be true
  // if the client wants to enumerate all solutions or wants correct tightened
  // bounds in the response.
  bool keep_all_feasible_solutions = false;

  // Just used to display statistics on the presolve rules that were used.
  absl::flat_hash_map<std::string, int> stats_by_rule_name;

  // Number of "rules" applied. This should be equal to the sum of all numbers
  // in stats_by_rule_name. This is used to decide if we should do one more pass
  // of the presolve or not. Note that depending on the presolve transformation,
  // a rule can correspond to a tiny change or a big change. Because of that,
  // this isn't a perfect proxy for the efficacy of the presolve.
  int64 num_presolve_operations = 0;

  // Temporary storage.
  std::vector<int> tmp_literals;
  std::vector<Domain> tmp_term_domains;
  std::vector<Domain> tmp_left_domains;
  absl::flat_hash_set<int> tmp_literal_set;

  // Each time a domain is modified this is set to true.
  SparseBitset<int64> modified_domains;

  // Advanced presolve. See this class comment.
  DomainDeductions deductions;

 private:
  void AddVariableUsage(int c);

  // Initially false, and set to true on the first inconsistency.
  bool is_unsat = false;

  // The current domain of each variables.
  std::vector<Domain> domains;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
