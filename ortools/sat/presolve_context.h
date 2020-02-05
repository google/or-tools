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
class PresolveContext {
 public:
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
  bool CanBeUsedAsLiteral(int ref) const;
  bool LiteralIsTrue(int lit) const;
  bool LiteralIsFalse(int lit) const;
  int64 MinOf(int ref) const;
  int64 MaxOf(int ref) const;
  bool DomainContains(int ref, int64 value) const;
  Domain DomainOf(int ref) const;

  // Helpers to query the current domain of a linear expression.
  // This doesn't check for integer overflow, but our linear expression
  // should be such that this cannot happen (tested at validation).
  int64 MinOf(const LinearExpressionProto& expr) const;
  int64 MaxOf(const LinearExpressionProto& expr) const;

  // This function takes a positive variable reference.
  bool DomainOfVarIsIncludedIn(int var, const Domain& domain) {
    return domains[var].IsIncludedIn(domain);
  }

  // Returns true iff the variable is not the representative of an equivalence
  // class of size at least 2.
  bool VariableIsNotRepresentativeOfEquivalenceClass(int ref) const;

  // Returns true if this ref only appear in one constraint.
  bool VariableIsUniqueAndRemovable(int ref) const;

  // Same as VariableIsUniqueAndRemovable() except that in this case the
  // variable also appear in the objective in addition to a single constraint.
  bool VariableWithCostIsUniqueAndRemovable(int ref) const;

  // Returns false if the new domain is empty. Sets 'domain_modified' (if
  // provided) to true iff the domain is modified otherwise does not change it.
  ABSL_MUST_USE_RESULT bool IntersectDomainWith(
      int ref, const Domain& domain, bool* domain_modified = nullptr);

  // Returns false if the 'lit' doesn't have the desired value in the domain.
  ABSL_MUST_USE_RESULT bool SetLiteralToFalse(int lit);
  ABSL_MUST_USE_RESULT bool SetLiteralToTrue(int lit);

  // This function always return false. It is just a way to make a little bit
  // more sure that we abort right away when infeasibility is detected.
  ABSL_MUST_USE_RESULT bool NotifyThatModelIsUnsat(
      const std::string& message = "") {
    // TODO(user): Report any explanation for the client in a nicer way?
    VLOG(1) << "INFEASIBLE: " << message;
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

  // At the beginning of the presolve, we delay the costly creation of this
  // "graph" until we at least ran some basic presolve. This is because during
  // a LNS neighbhorhood, many constraints will be reduced significantly by
  // this "simple" presolve.
  bool ConstraintVariableGraphIsUpToDate() const;

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

  // Stores the relation target_ref = abs(ref);
  bool StoreAbsRelation(int target_ref, int ref);

  // Returns the representative of a literal.
  int GetLiteralRepresentative(int ref) const;

  // Returns another reference with exactly the same value.
  int GetVariableRepresentative(int ref) const;

  // Used for statistics.
  int NumAffineRelations() const { return affine_relations_.NumRelations(); }
  int NumEquivRelations() const { return var_equiv_relations_.NumRelations(); }

  // This makes sure that the affine relation only uses one of the
  // representative from the var_equiv_relations.
  AffineRelation::Relation GetAffineRelation(int ref) const;

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

  // Returns true if a literal attached to ref == var exists.
  // It assigns the corresponding to `literal` if non null.
  bool HasVarValueEncoding(int ref, int64 value, int* literal = nullptr);

  // Stores the fact that literal implies var == value.
  // It returns true if that information is new.
  bool StoreLiteralImpliesVarEqValue(int literal, int var, int64 value);

  // Stores the fact that literal implies var != value.
  // It returns true if that information is new.
  bool StoreLiteralImpliesVarNEqValue(int literal, int var, int64 value);

  // Objective handling functions. We load it at the beginning so that during
  // presolve we can work on the more efficient hash_map representation.
  //
  // Note that ReadObjectiveFromProto() makes sure that var_to_constraints of
  // all the variable that appear in the objective contains -1. This is later
  // enforced by all the functions modifying the objective.
  //
  // Note(user): Because we process affine relation only on
  // CanonicalizeObjective(), it is possible that when processing a
  // canonicalized linear constraint, we don't detect that a variable in affine
  // relation is in the objective. For now this is fine, because when this is
  // the case, we also have an affine linear constraint, so we can't really do
  // anything with that variable since it appear in at least two constraints.
  void ReadObjectiveFromProto();
  ABSL_MUST_USE_RESULT bool CanonicalizeObjective();
  void WriteObjectiveToProto();

  // Given a variable defined by the given inequality that also appear in the
  // objective, remove it from the objective by transferring its cost to other
  // variables in the equality.
  //
  // If new_vars_in_objective is not nullptr, it will be filled with "new"
  // variables that where not in the objective before and are after
  // substitution.
  void SubstituteVariableInObjective(
      int var_in_equality, int64 coeff_in_equality,
      const ConstraintProto& equality,
      std::vector<int>* new_vars_in_objective = nullptr);

  // Objective getters.
  const Domain& ObjectiveDomain() const { return objective_domain; }
  const absl::flat_hash_map<int, int64>& ObjectiveMap() const {
    return objective_map;
  }
  bool ObjectiveDomainIsConstraining() const {
    return objective_domain_is_constraining;
  }

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

  // Contains the currently collected half value encodings:
  //   i.e.: literal => var ==/!= value
  // The state is accumulated (adding x => var == value then !x => var != value)
  // will deduce that x equivalent to var == value.
  absl::flat_hash_map<std::pair<int, int64>, absl::flat_hash_set<int>>
      eq_half_encoding;
  absl::flat_hash_map<std::pair<int, int64>, absl::flat_hash_set<int>>
      neq_half_encoding;

  // Contains abs relation.
  absl::flat_hash_map<int, int> abs_relations;

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

  // For each variables, list the constraints that just enforce a lower bound
  // (resp. upper bound) on that variable. If all the constraints in which a
  // variable appear are in the same direction, then we can usually fix a
  // variable to one of its bound (modulo its cost).
  //
  // TODO(user): Keeping these extra vector of hash_set seems inefficient. Come
  // up with a better way to detect if a variable is only constrainted in one
  // direction.
  std::vector<absl::flat_hash_set<int>> var_to_ub_only_constraints;
  std::vector<absl::flat_hash_set<int>> var_to_lb_only_constraints;

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

  // If true, fills stats_by_rule_name, otherwise do not do that. This can take
  // a few percent of the run time with a lot of LNS threads.
  bool enable_stats = true;

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
  // Helper to add an affine relation x = c.y + o to the given repository.
  bool AddRelation(int x, int y, int c, int o, AffineRelation* repo);

  void AddVariableUsage(int c);

  // Inserts an half reified var value encoding (literal => var ==/!= value).
  // It returns true if the new state is different from the old state.
  // Not that if imply_eq is false, the literal will be stored in its negated
  // form.
  //
  // Thus, if you detect literal <=> var == value, then two calls must be made:
  //     InsertHalfVarValueEncoding(literal, var, value, true);
  //     InsertHalfVarValueEncoding(NegatedRef(literal), var, value, false);
  bool InsertHalfVarValueEncoding(int literal, int var, int64 value,
                                  bool imply_eq);

  // Initially false, and set to true on the first inconsistency.
  bool is_unsat = false;

  // The current domain of each variables.
  std::vector<Domain> domains;

  // Internal representation of the objective. During presolve, we first load
  // the objective in this format in order to have more efficient substitution
  // on large problems (also because the objective is often dense). At the end
  // we re-convert it to its proto form.
  absl::flat_hash_map<int, int64> objective_map;
  std::vector<std::pair<int, int64>> tmp_entries;
  bool objective_domain_is_constraining = false;
  Domain objective_domain;
  double objective_offset;
  double objective_scaling_factor;

  // This regroups all the affine relations between variables. Note that the
  // constraints used to detect such relations will not be removed from the
  // model at detection time (thus allowing proper domain propagation). However,
  // if the arity of a variable becomes one, then such constraint will be
  // removed.
  AffineRelation affine_relations_;
  AffineRelation var_equiv_relations_;

  std::vector<int> tmp_new_usage_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
