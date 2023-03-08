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

#ifndef OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
#define OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_

#include <cstdint>
#include <deque>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// We use some special constraint index in our variable <-> constraint graph.
constexpr int kObjectiveConstraint = -1;
constexpr int kAffineRelationConstraint = -2;
constexpr int kAssumptionsConstraint = -3;

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
//
// TODO(user): get rid of this, we don't have the notion of equivalent variable
// anymore, but the more general affine relation one. We just need to support
// general affine for the linear1 involving an absolute value.
class SavedVariable {
 public:
  SavedVariable() {}
  explicit SavedVariable(int ref) : ref_(ref) {}
  int Get() const;

 private:
  int ref_ = 0;
};

// Wrap the CpModelProto we are presolving with extra data structure like the
// in-memory domain of each variables and the constraint variable graph.
class PresolveContext {
 public:
  PresolveContext(Model* model, CpModelProto* cp_model, CpModelProto* mapping)
      : working_model(cp_model),
        mapping_model(mapping),
        logger_(model->GetOrCreate<SolverLogger>()),
        params_(*model->GetOrCreate<SatParameters>()),
        time_limit_(model->GetOrCreate<TimeLimit>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()) {}

  // Helpers to adds new variables to the presolved model.
  //
  // TODO(user): We should control more how this is called so we can update
  // a solution hint accordingly.
  int NewIntVar(const Domain& domain);
  int NewBoolVar();

  // Some expansion code use constant literal to be simpler to write. This will
  // create a NewBoolVar() the first time, but later call will just returns it.
  int GetTrueLiteral();
  int GetFalseLiteral();

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
  int64_t MinOf(int ref) const;
  int64_t MaxOf(int ref) const;
  int64_t FixedValue(int ref) const;
  bool DomainContains(int ref, int64_t value) const;
  Domain DomainOf(int ref) const;

  // Helper to query the state of an interval.
  bool IntervalIsConstant(int ct_ref) const;
  int64_t StartMin(int ct_ref) const;
  int64_t StartMax(int ct_ref) const;
  int64_t SizeMin(int ct_ref) const;
  int64_t SizeMax(int ct_ref) const;
  int64_t EndMin(int ct_ref) const;
  int64_t EndMax(int ct_ref) const;
  std::string IntervalDebugString(int ct_ref) const;

  // Helpers to query the current domain of a linear expression.
  // This doesn't check for integer overflow, but our linear expression
  // should be such that this cannot happen (tested at validation).
  int64_t MinOf(const LinearExpressionProto& expr) const;
  int64_t MaxOf(const LinearExpressionProto& expr) const;
  bool IsFixed(const LinearExpressionProto& expr) const;
  int64_t FixedValue(const LinearExpressionProto& expr) const;

  // Accepts any proto with two parallel vector .vars() and .coeffs(), like
  // LinearConstraintProto or ObjectiveProto or LinearExpressionProto but beware
  // that this ignore any offset.
  template <typename ProtoWithVarsAndCoeffs>
  std::pair<int64_t, int64_t> ComputeMinMaxActivity(
      const ProtoWithVarsAndCoeffs& proto) const {
    int64_t min_activity = 0;
    int64_t max_activity = 0;
    const int num_vars = proto.vars().size();
    for (int i = 0; i < num_vars; ++i) {
      const int var = proto.vars(i);
      const int64_t coeff = proto.coeffs(i);
      if (coeff > 0) {
        min_activity += coeff * MinOf(var);
        max_activity += coeff * MaxOf(var);
      } else {
        min_activity += coeff * MaxOf(var);
        max_activity += coeff * MinOf(var);
      }
    }
    return {min_activity, max_activity};
  }

  // This methods only works for affine expressions (checked).
  bool DomainContains(const LinearExpressionProto& expr, int64_t value) const;

  // Return a super-set of the domain of the linear expression.
  Domain DomainSuperSetOf(const LinearExpressionProto& expr) const;

  // Returns true iff the expr is of the form a * literal + b.
  // The other function can be used to get the literal that achieve MaxOf().
  bool ExpressionIsAffineBoolean(const LinearExpressionProto& expr) const;
  int LiteralForExpressionMax(const LinearExpressionProto& expr) const;

  // Returns true iff the expr is of the form 1 * var + 0.
  bool ExpressionIsSingleVariable(const LinearExpressionProto& expr) const;

  // Returns true iff the expr is a literal (x or not(x)).
  bool ExpressionIsALiteral(const LinearExpressionProto& expr,
                            int* literal = nullptr) const;

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
  bool VariableWithCostIsUnique(int ref) const;
  bool VariableWithCostIsUniqueAndRemovable(int ref) const;

  // Returns true if an integer variable is only appearing in the rhs of
  // constraints of the form lit => var in domain. When this is the case, then
  // we can usually remove this variable and replace these constraints with
  // the proper constraints on the enforcement literals.
  bool VariableIsOnlyUsedInEncodingAndMaybeInObjective(int ref) const;

  // Returns false if the new domain is empty. Sets 'domain_modified' (if
  // provided) to true iff the domain is modified otherwise does not change it.
  ABSL_MUST_USE_RESULT bool IntersectDomainWith(
      int ref, const Domain& domain, bool* domain_modified = nullptr);

  // Returns false if the 'lit' doesn't have the desired value in the domain.
  ABSL_MUST_USE_RESULT bool SetLiteralToFalse(int lit);
  ABSL_MUST_USE_RESULT bool SetLiteralToTrue(int lit);

  // Same as IntersectDomainWith() but take a linear expression as input.
  // If this expression if of size > 1, this does nothing for now, so it will
  // only propagates for constant and affine expression.
  ABSL_MUST_USE_RESULT bool IntersectDomainWith(
      const LinearExpressionProto& expr, const Domain& domain,
      bool* domain_modified = nullptr);

  // This function always return false. It is just a way to make a little bit
  // more sure that we abort right away when infeasibility is detected.
  ABSL_MUST_USE_RESULT bool NotifyThatModelIsUnsat(
      const std::string& message = "") {
    // TODO(user): Report any explanation for the client in a nicer way?
    SOLVER_LOG(logger_, "INFEASIBLE: '", message, "'");
    DCHECK(!is_unsat_);
    is_unsat_ = true;
    return false;
  }
  bool ModelIsUnsat() const { return is_unsat_; }

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

  // A "canonical domain" always have a MinOf() equal to zero.
  // If needed we introduce a new variable with such canonical domain and
  // add the relation X = Y + offset.
  //
  // This is useful in some corner case to avoid overflow.
  //
  // TODO(user): When we can always get rid of affine relation, it might be good
  // to do a final pass to canonicalize all domains in a model after presolve.
  void CanonicalizeVariable(int ref);

  // Given the relation (X * coeff % mod = rhs % mod), this creates a new
  // variable so that X = mod * Y + cte.
  //
  // This requires mod != 0 and coeff != 0.
  //
  // Note that the new variable will have a canonical domain (i.e. min == 0).
  // We also do not create anything if this fixes the given variable or the
  // relation simplifies. Returns false if the model is infeasible.
  bool CanonicalizeAffineVariable(int ref, int64_t coeff, int64_t mod,
                                  int64_t rhs);

  // Adds the relation (ref_x = coeff * ref_y + offset) to the repository.
  // Returns false if we detect infeasability because of this.
  //
  // Once the relation is added, it doesn't need to be enforced by a constraint
  // in the model proto, since we will propagate such relation directly and add
  // them to the proto at the end of the presolve.
  //
  // Note that this should always add a relation, even though it might need to
  // create a new representative for both ref_x and ref_y in some cases. Like if
  // x = 3z and y = 5t are already added, if we add x = 2y, we have 3z = 10t and
  // can only resolve this by creating a new variable r such that z = 10r and t
  // = 3r.
  //
  // All involved variables will be marked to appear in the special
  // kAffineRelationConstraint. This will allow to identify when a variable is
  // no longer needed (only appear there and is not a representative).
  bool StoreAffineRelation(int ref_x, int ref_y, int64_t coeff, int64_t offset,
                           bool debug_no_recursion = false);

  // Adds the fact that ref_a == ref_b using StoreAffineRelation() above.
  // Returns false if this makes the problem infeasible.
  bool StoreBooleanEqualityRelation(int ref_a, int ref_b);

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

  // Returns the representative of ref under the affine relations.
  AffineRelation::Relation GetAffineRelation(int ref) const;

  // To facilitate debugging.
  std::string RefDebugString(int ref) const;
  std::string AffineRelationDebugString(int ref) const;

  // Makes sure the domain of ref and of its representative (ref = coeff * rep +
  // offset) are in sync. Returns false on unsat.
  bool PropagateAffineRelation(int ref);
  bool PropagateAffineRelation(int ref, int rep, int64_t coeff, int64_t offset);

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
  //
  // Returns false if the model become UNSAT.
  //
  // TODO(user): This function is not always correct if
  // !context->DomainOf(ref).contains(value), we could make it correct but it
  // might be a bit expansive to do so. For now we just have a DCHECK().
  bool InsertVarValueEncoding(int literal, int ref, int64_t value);

  // Gets the associated literal if it is already created. Otherwise
  // create it, add the corresponding constraints and returns it.
  //
  // Important: This does not update the constraint<->variable graph, so
  // ConstraintVariableGraphIsUpToDate() will be false until
  // UpdateNewConstraintsVariableUsage() is called.
  int GetOrCreateVarValueEncoding(int ref, int64_t value);

  // Gets the associated literal if it is already created. Otherwise
  // create it, add the corresponding constraints and returns it.
  //
  // Important: This does not update the constraint<->variable graph, so
  // ConstraintVariableGraphIsUpToDate() will be false until
  // UpdateNewConstraintsVariableUsage() is called.
  int GetOrCreateAffineValueEncoding(const LinearExpressionProto& expr,
                                     int64_t value);

  // If not already done, adds a Boolean to represent any integer variables that
  // take only two values. Make sure all the relevant affine and encoding
  // relations are updated.
  //
  // Note that this might create a new Boolean variable.
  void CanonicalizeDomainOfSizeTwo(int var);

  // Returns true if a literal attached to ref == var exists.
  // It assigns the corresponding to `literal` if non null.
  bool HasVarValueEncoding(int ref, int64_t value, int* literal = nullptr);

  // Returns true if we have literal <=> var = value for all values of var.
  //
  // TODO(user): If the domain was shrunk, we can have a false positive.
  // Still it means that the number of values removed is greater than the number
  // of values not encoded.
  bool IsFullyEncoded(int ref) const;

  // This methods only works for affine expressions (checked).
  // It returns true iff the expression is constant or its one variable is full
  // encoded.
  bool IsFullyEncoded(const LinearExpressionProto& expr) const;

  // Stores the fact that literal implies var == value.
  // It returns true if that information is new.
  bool StoreLiteralImpliesVarEqValue(int literal, int var, int64_t value);

  // Stores the fact that literal implies var != value.
  // It returns true if that information is new.
  bool StoreLiteralImpliesVarNEqValue(int literal, int var, int64_t value);

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
  bool AddToObjectiveOffset(int64_t delta);
  ABSL_MUST_USE_RESULT bool CanonicalizeOneObjectiveVariable(int var);
  ABSL_MUST_USE_RESULT bool CanonicalizeObjective(bool simplify_domain = true);
  void WriteObjectiveToProto() const;
  ABSL_MUST_USE_RESULT bool ScaleFloatingPointObjective();

  // When the objective is singleton, we can always restrict the domain of var
  // so that the current objective domain is non-constraining. Returns false
  // on UNSAT.
  bool RecomputeSingletonObjectiveDomain();

  // Some function need the domain to be up to date in the proto.
  // This make sures our in-memory domain are writted back to the proto.
  void WriteVariableDomainsToProto() const;

  // Checks if the given exactly_one is included in the objective, and simplify
  // the objective by adding a constant value to all the exactly one terms.
  //
  // Returns true if a simplification was done.
  bool ExploitExactlyOneInObjective(absl::Span<const int> exactly_one);

  // We can always add a multiple of sum X - 1 == 0 to the objective.
  // However, depending on which multiple we choose, this might break our
  // overflow preconditions on the objective. So we return false and do nothing
  // if this happens.
  bool ShiftCostInExactlyOne(absl::Span<const int> exactly_one, int64_t shift);

  // Allows to manipulate the objective coefficients.
  void RemoveVariableFromObjective(int ref);
  void AddToObjective(int var, int64_t value);
  void AddLiteralToObjective(int ref, int64_t value);

  // Given a variable defined by the given inequality that also appear in the
  // objective, remove it from the objective by transferring its cost to other
  // variables in the equality.
  //
  // Returns false, if the substitution cannot be done. This is the case if the
  // model become UNSAT or if doing it will result in an objective that do not
  // satisfy our overflow preconditions. Note that this can only happen if the
  // substituted variable is not implied free (i.e. if its domain is smaller
  // than the implied domain from the equality).
  ABSL_MUST_USE_RESULT bool SubstituteVariableInObjective(
      int var_in_equality, int64_t coeff_in_equality,
      const ConstraintProto& equality);

  // Objective getters.
  const Domain& ObjectiveDomain() const { return objective_domain_; }
  const absl::flat_hash_map<int, int64_t>& ObjectiveMap() const {
    return objective_map_;
  }
  int64_t ObjectiveCoeff(int var) const {
    DCHECK_GE(var, 0);
    const auto it = objective_map_.find(var);
    return it == objective_map_.end() ? 0 : it->second;
  }
  bool ObjectiveDomainIsConstraining() const {
    return objective_domain_is_constraining_;
  }

  // Advanced usage. This should be called when a variable can be removed from
  // the problem, so we don't count it as part of an affine relation anymore.
  void RemoveVariableFromAffineRelation(int var);
  void RemoveAllVariablesFromAffineRelationConstraint();

  // Variable <-> constraint graph.
  // The vector list is sorted and contains unique elements.
  //
  // Important: To properly handle the objective, var_to_constraints[objective]
  // contains kObjectiveConstraint (i.e. -1) so that if the objective appear in
  // only one constraint, the constraint cannot be simplified.
  const std::vector<std::vector<int>>& ConstraintToVarsGraph() const {
    DCHECK(ConstraintVariableGraphIsUpToDate());
    return constraint_to_vars_;
  }
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

  // Checks if a constraint contains an enforcement literal set to false,
  // or if it has been cleared.
  bool ConstraintIsInactive(int ct_index) const;

  // Checks if a constraint contains an enforcement literal not fixed, and
  // no enforcement literals set to false.
  bool ConstraintIsOptional(int ct_ref) const;

  // Make sure we never delete an "assumption" literal by using a special
  // constraint for that.
  void RegisterVariablesUsedInAssumptions() {
    for (const int ref : working_model->assumptions()) {
      var_to_constraints_[PositiveRef(ref)].insert(kAssumptionsConstraint);
    }
  }

  // The "expansion" phase should be done once and allow to transform complex
  // constraints into basic ones (see cp_model_expand.h). Some presolve rules
  // need to know if the expansion was ran before beeing applied.
  bool ModelIsExpanded() const { return model_is_expanded_; }
  void NotifyThatModelIsExpanded() { model_is_expanded_ = true; }

  // The following helper adds the following constraint:
  //    result <=> (time_i <= time_j && active_i is true && active_j is true)
  // and returns the (cached) literal result.
  //
  // Note that this cache should just be used temporarily and then cleared
  // with ClearPrecedenceCache() because there is no mechanism to update the
  // cached literals when literal equivalence are detected.
  int GetOrCreateReifiedPrecedenceLiteral(const LinearExpressionProto& time_i,
                                          const LinearExpressionProto& time_j,
                                          int active_i, int active_j);

  std::tuple<int, int64_t, int, int64_t, int64_t, int, int>
  GetReifiedPrecedenceKey(const LinearExpressionProto& time_i,
                          const LinearExpressionProto& time_j, int active_i,
                          int active_j);

  // Clear the precedence cache.
  void ClearPrecedenceCache();

  // Logs stats to the logger.
  void LogInfo();

  // Return the given index, or the index of an interval with the same data.
  int GetIntervalRepresentative(int index);

  SolverLogger* logger() const { return logger_; }
  const SatParameters& params() const { return params_; }
  TimeLimit* time_limit() { return time_limit_; }
  ModelRandomGenerator* random() { return random_; }

  CpModelProto* working_model = nullptr;
  CpModelProto* mapping_model = nullptr;

  // Indicate if we are allowed to remove irrelevant feasible solution from the
  // set of feasible solution. For example, if a variable is unused, can we fix
  // it to an arbitrary value (or its mimimum objective one)? This must be true
  // if the client wants to enumerate all solutions or wants correct tightened
  // bounds in the response.
  bool keep_all_feasible_solutions = false;

  // Number of "rules" applied. This should be equal to the sum of all numbers
  // in stats_by_rule_name. This is used to decide if we should do one more pass
  // of the presolve or not. Note that depending on the presolve transformation,
  // a rule can correspond to a tiny change or a big change. Because of that,
  // this isn't a perfect proxy for the efficacy of the presolve.
  int64_t num_presolve_operations = 0;

  // Temporary storage.
  std::vector<int> tmp_literals;
  std::vector<Domain> tmp_term_domains;
  std::vector<Domain> tmp_left_domains;
  absl::flat_hash_set<int> tmp_literal_set;

  // Each time a domain is modified this is set to true.
  SparseBitset<int> modified_domains;

  // Each time the constraint <-> variable graph is updated, we update this.
  // A variable is added here iff its usage decreased and is now one or two.
  SparseBitset<int> var_with_reduced_small_degree;

  // Advanced presolve. See this class comment.
  DomainDeductions deductions;

 private:
  void EraseFromVarToConstraint(int var, int c);

  // Helper to add an affine relation x = c.y + o to the given repository.
  bool AddRelation(int x, int y, int64_t c, int64_t o, AffineRelation* repo);

  void AddVariableUsage(int c);
  void UpdateLinear1Usage(const ConstraintProto& ct, int c);

  // Makes sure we only insert encoding about the current representative.
  //
  // Returns false if ref cannot take the given value (it might not have been
  // propagated yet).
  bool CanonicalizeEncoding(int* ref, int64_t* value);

  // Inserts an half reified var value encoding (literal => var ==/!= value).
  // It returns true if the new state is different from the old state.
  // Not that if imply_eq is false, the literal will be stored in its negated
  // form.
  //
  // Thus, if you detect literal <=> var == value, then two calls must be made:
  //     InsertHalfVarValueEncoding(literal, var, value, true);
  //     InsertHalfVarValueEncoding(NegatedRef(literal), var, value, false);
  bool InsertHalfVarValueEncoding(int literal, int var, int64_t value,
                                  bool imply_eq);

  // Insert fully reified var-value encoding.
  void InsertVarValueEncodingInternal(int literal, int var, int64_t value,
                                      bool add_constraints);

  SolverLogger* logger_;
  const SatParameters& params_;
  TimeLimit* time_limit_;
  ModelRandomGenerator* random_;

  // Initially false, and set to true on the first inconsistency.
  bool is_unsat_ = false;

  // The current domain of each variables.
  std::vector<Domain> domains;

  // Internal representation of the objective. During presolve, we first load
  // the objective in this format in order to have more efficient substitution
  // on large problems (also because the objective is often dense). At the end
  // we re-convert it to its proto form.
  absl::flat_hash_map<int, int64_t> objective_map_;
  int64_t objective_overflow_detection_;
  std::vector<std::pair<int, int64_t>> tmp_entries_;
  bool objective_domain_is_constraining_ = false;
  Domain objective_domain_;
  double objective_offset_;
  double objective_scaling_factor_;
  int64_t objective_integer_before_offset_;
  int64_t objective_integer_after_offset_;
  int64_t objective_integer_scaling_factor_;

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

  // Used by GetTrueLiteral()/GetFalseLiteral().
  bool true_literal_is_defined_ = false;
  int true_literal_;

  // Contains variables with some encoded value: encoding_[i][v] points
  // to the literal attached to the value v of the variable i.
  absl::flat_hash_map<int, absl::flat_hash_map<int64_t, SavedLiteral>>
      encoding_;

  // Contains the currently collected half value encodings:
  //   i.e.: literal => var ==/!= value
  // The state is accumulated (adding x => var == value then !x => var != value)
  // will deduce that x equivalent to var == value.
  absl::flat_hash_map<int,
                      absl::flat_hash_map<int64_t, absl::flat_hash_set<int>>>
      eq_half_encoding_;
  absl::flat_hash_map<int,
                      absl::flat_hash_map<int64_t, absl::flat_hash_set<int>>>
      neq_half_encoding_;

  // This regroups all the affine relations between variables. Note that the
  // constraints used to detect such relations will be removed from the model at
  // detection time. But we mark all the variables in affine relations as part
  // of the kAffineRelationConstraint.
  AffineRelation affine_relations_;

  std::vector<int> tmp_new_usage_;

  // Used by SetVariableAsRemoved() and VariableWasRemoved().
  absl::flat_hash_set<int> removed_variables_;

  // Cache for the reified precedence literals created during the expansion of
  // the reservoir constraint. This cache is only valid during the expansion
  // phase, and is cleared afterwards.
  absl::flat_hash_map<std::tuple<int, int64_t, int, int64_t, int64_t, int, int>,
                      int>
      reified_precedences_cache_;

  // Just used to display statistics on the presolve rules that were used.
  absl::flat_hash_map<std::string, int> stats_by_rule_name_;

  // Serialized proto (should be small) to index.
  absl::flat_hash_map<std::string, int> interval_representative_;

  bool model_is_expanded_ = false;
};

// Utility function to load the current problem into a in-memory representation
// that will be used for probing. Returns false if UNSAT.
bool LoadModelForProbing(PresolveContext* context, Model* local_model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
