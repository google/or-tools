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

#include <deque>
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

// We use some special constraint index in our variable <-> constraint graph.
constexpr int kObjectiveConstraint = -1;
constexpr int kAffineRelationConstraint = -2;
constexpr int kAssumptionsConstraint = -3;

struct PresolveOptions {
  bool log_info = true;
  SatParameters parameters;
  TimeLimit* time_limit = nullptr;
};

class PresolveContext;

// When storing a reference to a literal, it is important not to forget when
// reading it back to take its representative. Otherwise, we might introduce
// literal that have already been removed, which will break invariants in a
// bunch of places.
class SavedLiteral {
 public:
  SavedLiteral() {}
  explicit SavedLiteral(int ref) : ref_(ref) {}
  int Get(PresolveContext* context) const;

 private:
  int ref_ = 0;
};

// Same as SavedLiteral for variable.
class SavedVariable {
 public:
  SavedVariable() {}
  explicit SavedVariable(int ref) : ref_(ref) {}
  int Get(PresolveContext* context) const;

 private:
  int ref_ = 0;
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

  // Returns true if this ref only appear in one constraint.
  bool VariableIsUniqueAndRemovable(int ref) const;

  // Returns true if this ref no longer appears in the model.
  bool VariableIsNotUsedAnymore(int ref) const;

  // Functions to make sure that once we remove a variable, we no longer reuse
  // it.
  void MarkVariableAsRemoved(int ref);
  bool VariableWasRemoved(int ref) const;

  // Same as VariableIsUniqueAndRemovable() except that in this case the
  // variable also appear in the objective in addition to a single constraint.
  bool VariableWithCostIsUniqueAndRemovable(int ref) const;

  // Returns true if an integer variable is only appearing in the rhs of
  // constraints of the form lit => var in domain. When this is the case, then
  // we can usually remove this variable and replace these constraints with
  // the proper constraints on the enforcement literals.
  bool VariableIsOnlyUsedInEncoding(int ref) const;

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
  void UpdateRuleStats(const std::string& name, int num_times = 1);

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
  // Once the relation is added, it doesn't need to be enforced by a constraint
  // in the model proto, since we will propagate such relation directly and add
  // them to the proto at the end of the presolve.
  //
  // Returns true if the relation was added.
  // In some rare case, like if x = 3*z and y = 5*t are already added, we
  // currently cannot add x = 2 * y and we will return false in these case. So
  // when this returns false, the relation needs to be enforced by a separate
  // constraint.
  //
  // If the relation was added, both variables will be marked to appear in the
  // special kAffineRelationConstraint. This will allow to identify when a
  // variable is no longer needed (only appear there and is not a
  // representative).
  bool StoreAffineRelation(int ref_x, int ref_y, int64 coeff, int64 offset);

  // Adds the fact that ref_a == ref_b using StoreAffineRelation() above.
  // This should never fail, so the relation will always be added.
  void StoreBooleanEqualityRelation(int ref_a, int ref_b);

  // Stores/Get the relation target_ref = abs(ref); The first function returns
  // false if it already exist and the second false if it is not present.
  bool StoreAbsRelation(int target_ref, int ref);
  bool GetAbsRelation(int target_ref, int* ref);

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

  // To facilitate debugging.
  std::string RefDebugString(int ref) const;
  std::string AffineRelationDebugString(int ref) const;

  // Makes sure the domain of ref and of its representative are in sync.
  // Returns false on unsat.
  bool PropagateAffineRelation(int ref);

  // Creates the internal structure for any new variables in working_model.
  void InitializeNewDomains();

  // Clears the "rules" statistics.
  void ClearStats();

  // Inserts the given literal to encode ref == value.
  // If an encoding already exists, it adds the two implications between
  // the previous encoding and the new encoding.
  //
  // Important: This does not update the constraint<->variable graph, so
  // ConstraintVariableGraphIsUpToDate() will be false until
  // UpdateNewConstraintsVariableUsage() is called.
  void InsertVarValueEncoding(int literal, int ref, int64 value);

  // Gets the associated literal if it is already created. Otherwise
  // create it, add the corresponding constraints and returns it.
  //
  // Important: This does not update the constraint<->variable graph, so
  // ConstraintVariableGraphIsUpToDate() will be false until
  // UpdateNewConstraintsVariableUsage() is called.
  int GetOrCreateVarValueEncoding(int ref, int64 value);

  // If not already done, adds a Boolean to represent any integer variables that
  // take only two values. Make sure all the relevant affine and encoding
  // relations are updated.
  //
  // Note that this might create a new Boolean variable.
  void CanonicalizeDomainOfSizeTwo(int var);

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
  void WriteObjectiveToProto() const;

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

  // Advanced usage. This should be called when a variable can be removed from
  // the problem, so we don't count it as part of an affine relation anymore.
  void RemoveVariableFromAffineRelation(int var);
  void RemoveAllVariablesFromAffineRelationConstraint();

  // Variable <-> constraint graph.
  // The vector list is sorted and contains unique elements.
  //
  // Important: To properly handle the objective, var_to_constraints[objective]
  // contains -1 so that if the objective appear in only one constraint, the
  // constraint cannot be simplified.
  const std::vector<int>& ConstraintToVars(int c) const {
    DCHECK(ConstraintVariableGraphIsUpToDate());
    return constraint_to_vars_[c];
  }
  const absl::flat_hash_set<int>& VarToConstraints(int var) const {
    DCHECK(ConstraintVariableGraphIsUpToDate());
    return var_to_constraints_[var];
  }
  int IntervalUsage(int c) const {
    DCHECK(ConstraintVariableGraphIsUpToDate());
    return interval_usage_[c];
  }

  // Make sure we never delete an "assumption" literal by using a special
  // constraint for that.
  void RegisterVariablesUsedInAssumptions() {
    for (const int ref : working_model->assumptions()) {
      var_to_constraints_[PositiveRef(ref)].insert(kAssumptionsConstraint);
    }
  }

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
  bool AddRelation(int x, int y, int64 c, int64 o, AffineRelation* repo);

  void AddVariableUsage(int c);
  void UpdateLinear1Usage(const ConstraintProto& ct, int c);

  // Returns true iff the variable is not the representative of an equivalence
  // class of size at least 2.
  bool VariableIsNotRepresentativeOfEquivalenceClass(int var) const;

  // Process encoding_remap_queue_ and updates the encoding maps. This could
  // lead to UNSAT being detected, in which case it will return false.
  bool RemapEncodingMaps();

  // Makes sure we only insert encoding about the current representative.
  //
  // Returns false if ref cannot take the given value (it might not have been
  // propagated yed).
  bool CanonicalizeEncoding(int* ref, int64* value);

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

  // Insert fully reified var-value encoding.
  void InsertVarValueEncodingInternal(int literal, int var, int64 value,
                                      bool add_constraints);

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

  // Constraints <-> Variables graph.
  std::vector<std::vector<int>> constraint_to_vars_;
  std::vector<absl::flat_hash_set<int>> var_to_constraints_;

  // Number of constraints of the form [lit =>] var in domain.
  std::vector<int> constraint_to_linear1_var_;
  std::vector<int> var_to_num_linear1_;

  // We maintain how many time each interval is used.
  std::vector<std::vector<int>> constraint_to_intervals_;
  std::vector<int> interval_usage_;

  // Contains abs relation (key = abs(saved_variable)).
  absl::flat_hash_map<int, SavedVariable> abs_relations_;

  // For each constant variable appearing in the model, we maintain a reference
  // variable with the same constant value. If two variables end up having the
  // same fixed value, then we can detect it using this and add a new
  // equivalence relation. See ExploitFixedDomain().
  absl::flat_hash_map<int64, SavedVariable> constant_to_ref_;

  // When a "representative" gets a new representative, it should be enqueued
  // here so that we can lazily update the *encoding_ maps below.
  std::deque<int> encoding_remap_queue_;

  // Contains variables with some encoded value: encoding_[i][v] points
  // to the literal attached to the value v of the variable i.
  absl::flat_hash_map<int, absl::flat_hash_map<int64, SavedLiteral>> encoding_;

  // Contains the currently collected half value encodings:
  //   i.e.: literal => var ==/!= value
  // The state is accumulated (adding x => var == value then !x => var != value)
  // will deduce that x equivalent to var == value.
  absl::flat_hash_map<int, absl::flat_hash_map<int64, absl::flat_hash_set<int>>>
      eq_half_encoding_;
  absl::flat_hash_map<int, absl::flat_hash_map<int64, absl::flat_hash_set<int>>>
      neq_half_encoding_;

  // This regroups all the affine relations between variables. Note that the
  // constraints used to detect such relations will not be removed from the
  // model at detection time (thus allowing proper domain propagation). However,
  // if the arity of a variable becomes one, then such constraint will be
  // removed.
  AffineRelation affine_relations_;
  AffineRelation var_equiv_relations_;

  std::vector<int> tmp_new_usage_;

  // Used by SetVariableAsRemoved() and VariableWasRemoved().
  absl::flat_hash_set<int> removed_variables_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
