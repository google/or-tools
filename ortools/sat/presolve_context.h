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

#ifndef OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
#define OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/util.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

ABSL_DECLARE_FLAG(bool, cp_model_debug_postsolve);

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
  SavedLiteral() = default;
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
  SavedVariable() = default;
  explicit SavedVariable(int ref) : ref_(ref) {}
  int Get() const;

 private:
  int ref_ = 0;
};

// If a floating point objective is present, scale it using the current domains
// and transform it to an integer_objective.
ABSL_MUST_USE_RESULT bool ScaleFloatingPointObjective(
    const SatParameters& params, SolverLogger* logger, CpModelProto* proto);

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

  // Creates a new integer variable with the given domain.
  // WARNING: this does not set any hint value for the new variable.
  int NewIntVar(const Domain& domain);

  // Creates a new Boolean variable.
  // WARNING: this does not set any hint value for the new variable.
  int NewBoolVar(absl::string_view source);

  // Creates a new integer variable with the given domain and definition.
  // By default this also creates the linking constraint new_var = definition.
  // Its hint value is set to the value of the definition. Returns -1 if we
  // couldn't create the definition due to overflow.
  int NewIntVarWithDefinition(
      const Domain& domain,
      absl::Span<const std::pair<int, int64_t>> definition,
      bool append_constraint_to_mapping_model = false);

  // Creates a new bool var.
  // Its hint value is set to the value of the given clause.
  int NewBoolVarWithClause(absl::Span<const int> clause);

  // Creates a new bool var.
  // Its hint value is set to the value of the given conjunction.
  int NewBoolVarWithConjunction(absl::Span<const int> conjunction);

  // Some expansion code use constant literal to be simpler to write. This will
  // create a NewBoolVar() the first time, but later call will just returns it.
  int GetTrueLiteral();
  int GetFalseLiteral();

  // a => b.
  void AddImplication(int a, int b);

  // b => (x ∈ domain).
  void AddImplyInDomain(int b, int x, const Domain& domain);

  // b => (expr ∈ domain).
  void AddImplyInDomain(int b, const LinearExpressionProto& expr,
                        const Domain& domain);

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
  absl::Span<const Domain> AllDomains() const { return domains_; }

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

  // This is faster than testing IsFixed() + FixedValue().
  std::optional<int64_t> FixedValueOrNullopt(
      const LinearExpressionProto& expr) const;

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

  // Utility function.
  void CappedUpdateMinMaxActivity(int var, int64_t coeff, int64_t* min_activity,
                                  int64_t* max_activity) {
    if (coeff > 0) {
      *min_activity = CapAdd(*min_activity, CapProd(coeff, MinOf(var)));
      *max_activity = CapAdd(*max_activity, CapProd(coeff, MaxOf(var)));
    } else {
      *min_activity = CapAdd(*min_activity, CapProd(coeff, MaxOf(var)));
      *max_activity = CapAdd(*max_activity, CapProd(coeff, MinOf(var)));
    }
  }

  // Canonicalization of linear constraint. This might also be needed when
  // creating new constraint to make sure there are no duplicate variables.
  // Returns true if the set of variables in the expression changed.
  //
  // This uses affine relation and regroup duplicate/fixed terms.
  bool CanonicalizeLinearConstraint(ConstraintProto* ct);
  bool CanonicalizeLinearExpression(absl::Span<const int> enforcements,
                                    LinearExpressionProto* expr);

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
    return domains_[var].IsIncludedIn(domain);
  }

  // Returns true if this ref only appear in one constraint.
  bool VariableIsUnique(int ref) const;
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
  bool VariableIsOnlyUsedInEncodingAndMaybeInObjective(int var) const;

  // Similar to VariableIsOnlyUsedInEncodingAndMaybeInObjective() for the case
  // where we have one extra constraint instead of the objective. Sometimes it
  // is possible to transfer the linear1 domain restrictions to another
  // variable. for instance if the other constraint is of the form Y = abs(X) or
  // Y = X^2, then a domain restriction on Y can be transferred to X. We can
  // then move the extra constraint to the mapping model and remove one
  // variable. This happens on the flatzinc celar problems for instance.
  bool VariableIsOnlyUsedInLinear1AndOneExtraConstraint(int var) const;

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
      absl::string_view message = "") {
    // TODO(user): Report any explanation for the client in a nicer way?
    SOLVER_LOG(logger_, "INFEASIBLE: '", message, "'");
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
  // a LNS neighborhood, many constraints will be reduced significantly by
  // this "simple" presolve.
  bool ConstraintVariableGraphIsUpToDate() const;

  // Calls UpdateConstraintVariableUsage() on all newly created constraints.
  void UpdateNewConstraintsVariableUsage();

  // Returns true if our current constraints <-> variables graph is ok.
  // This is meant to be used in DEBUG mode only.
  bool ConstraintVariableUsageIsConsistent();

  // Loop over all variable and return true if one of them is only used in
  // affine relation and is not a representative. This is in O(num_vars) and
  // only meant to be used in DCHECKs.
  bool HasUnusedAffineVariable() const;

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

  // Adds the relation (var_x = coeff * var_y + offset) to the repository.
  // Returns false if we detect infeasability because of this.
  //
  // Once the relation is added, it doesn't need to be enforced by a constraint
  // in the model proto, since we will propagate such relation directly and add
  // them to the proto at the end of the presolve.
  //
  // Note that this should always add a relation, even though it might need to
  // create a new representative for both var_x and var_y in some cases. Like if
  // x = 3z and y = 5t are already added, if we add x = 2y, we have 3z = 10t and
  // can only resolve this by creating a new variable r such that z = 10r and t
  // = 3r.
  //
  // All involved variables will be marked to appear in the special
  // kAffineRelationConstraint. This will allow to identify when a variable is
  // no longer needed (only appear there and is not a representative).
  bool StoreAffineRelation(int var_x, int var_y, int64_t coeff, int64_t offset,
                           bool debug_no_recursion = false);

  // Adds the fact that ref_a == ref_b using StoreAffineRelation() above.
  // Returns false if this makes the problem infeasible.
  bool StoreBooleanEqualityRelation(int ref_a, int ref_b);

  // Returns the representative of a literal.
  int GetLiteralRepresentative(int ref) const;

  // Used for statistics.
  int NumAffineRelations() const { return affine_relations_.NumRelations(); }

  // Returns the representative of ref under the affine relations.
  AffineRelation::Relation GetAffineRelation(int ref) const;

  // To facilitate debugging.
  std::string RefDebugString(int ref) const;
  std::string AffineRelationDebugString(int ref) const;

  // Makes sure the domain of ref and of its representative (ref = coeff * rep +
  // offset) are in sync. Returns false on unsat.
  bool PropagateAffineRelation(int var);
  bool PropagateAffineRelation(int var, int rep, int64_t coeff, int64_t offset);

  // Creates the internal structure for any new variables in working_model.
  void InitializeNewDomains();

  // This is a bit hacky. Clear some fields. See call site.
  //
  // TODO(user): The ModelCopier should probably not depend on the full context
  // it only need to read/write domains and call UpdateRuleStats(), so we might
  // want to split that part out so that we can just initialize the full context
  // later. Alternatively, we could just move more complex part of the context
  // out, like the graph, the encoding, the affine representative, and so on to
  // individual and easier to manage classes.
  void ResetAfterCopy();

  // Clears the "rules" statistics.
  void ClearStats();

  // Inserts the given literal to encode var == value.
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
  // !context->DomainOf(var).contains(value), we could make it correct but it
  // might be a bit expansive to do so. For now we just have a DCHECK().
  bool InsertVarValueEncoding(int literal, int var, int64_t value);

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

  // When the objective is singleton, we can always restrict the domain of var
  // so that the current objective domain is non-constraining. Returns false
  // on UNSAT.
  bool RecomputeSingletonObjectiveDomain();

  // Some function need the domain to be up to date in the proto.
  // This make sures our in-memory domain are written back to the proto.
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

  // Returns false if the variables in the objective with a positive (resp.
  // negative) coefficient can freely decrease (resp. increase) within their
  // domain (if we ignore the other constraints). Otherwise, returns true.
  bool ObjectiveDomainIsConstraining() const {
    return objective_domain_is_constraining_;
  }

  // If var is an unused variable in an affine relation and is not a
  // representative, we can remove it from the model. Note that this requires
  // the variable usage graph to be up to date.
  void RemoveNonRepresentativeAffineVariableIfUnused(int var);

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
  absl::Span<const int> ConstraintToVars(int c) const {
    DCHECK(ConstraintVariableGraphIsUpToDate());
    return constraint_to_vars_[c];
  }
  const absl::flat_hash_set<int>& VarToConstraints(int var) const {
    DCHECK(ConstraintVariableGraphIsUpToDate());
    return var_to_constraints_[var];
  }
  int IntervalUsage(int c) const {
    DCHECK(ConstraintVariableGraphIsUpToDate());
    if (c >= interval_usage_.size()) return 0;
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

  // This should be called only once after InitializeNewDomains() to load
  // the hint, in order to maintain it as best as possible during presolve.
  // Hint values outside the domain of their variable are adjusted to the
  // nearest value in this domain. Missing hint values are completed when
  // possible (e.g. for the model proto's fixed variables).
  void LoadSolutionHint();

  SolutionCrush& solution_crush() { return solution_crush_; }
  // This is slow O(problem_size) but can be used to debug presolve, either by
  // pinpointing the transition from feasible to infeasible or the other way
  // around if for some reason the presolve drop constraint that it shouldn't.
  bool DebugTestHintFeasibility();

  SolverLogger* logger() const { return logger_; }
  const SatParameters& params() const { return params_; }
  TimeLimit* time_limit() { return time_limit_; }
  ModelRandomGenerator* random() { return random_; }

  CpModelProto* working_model = nullptr;
  CpModelProto* mapping_model = nullptr;

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

  // Adds a new constraint to the mapping proto. The version with the base
  // constraint will copy that constraint to the new constraint.
  //
  // If the flag --cp_model_debug_postsolve is set, we will use the caller
  // file/line number to add debug info in the constraint name() field.
  ConstraintProto* NewMappingConstraint(absl::string_view file, int line);
  ConstraintProto* NewMappingConstraint(const ConstraintProto& base_ct,
                                        absl::string_view file, int line);

 private:
  void MaybeResizeIntervalData();

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
  // Returns false if this make the problem infeasible.
  bool InsertVarValueEncodingInternal(int literal, int var, int64_t value,
                                      bool add_constraints);

  SolverLogger* logger_;
  const SatParameters& params_;
  TimeLimit* time_limit_;
  ModelRandomGenerator* random_;

  // Initially false, and set to true on the first inconsistency.
  bool is_unsat_ = false;

  // The current domain of each variables.
  std::vector<Domain> domains_;

  SolutionCrush solution_crush_;

  // Internal representation of the objective. During presolve, we first load
  // the objective in this format in order to have more efficient substitution
  // on large problems (also because the objective is often dense). At the end
  // we convert it back to its proto form.
  mutable bool objective_proto_is_up_to_date_ = false;
  absl::flat_hash_map<int, int64_t> objective_map_;
  int64_t objective_overflow_detection_;
  mutable std::vector<std::pair<int, int64_t>> tmp_entries_;
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

  // Used by GetTrueLiteral()/GetFalseLiteral().
  bool true_literal_is_defined_ = false;
  int true_literal_;

  // Contains variables with some encoded value: encoding_[i][v] points
  // to the literal attached to the value v of the variable i.
  absl::flat_hash_map<int, absl::flat_hash_map<int64_t, SavedLiteral>>
      encoding_;

  // Contains the currently collected half value encodings:
  // (literal, var, value),  i.e.: literal => var ==/!= value
  // The state is accumulated (adding x => var == value then !x => var != value)
  // will deduce that x equivalent to var == value.
  absl::flat_hash_map<std::tuple<int, int>, int64_t> eq_half_encoding_;
  absl::flat_hash_map<std::tuple<int, int>, int64_t> neq_half_encoding_;

  // This regroups all the affine relations between variables. Note that the
  // constraints used to detect such relations will be removed from the model at
  // detection time. But we mark all the variables in affine relations as part
  // of the kAffineRelationConstraint.
  AffineRelation affine_relations_;

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

  // Used by CanonicalizeLinearExpressionInternal().
  std::vector<std::pair<int, int64_t>> tmp_terms_;

  bool model_is_expanded_ = false;
};

// Utility function to load the current problem into a in-memory representation
// that will be used for probing. Returns false if UNSAT.
bool LoadModelForProbing(PresolveContext* context, Model* local_model);

bool LoadModelForPresolve(const CpModelProto& model_proto, SatParameters params,
                          PresolveContext* context, Model* local_model,
                          absl::string_view name_for_logging);

void CreateValidModelWithSingleConstraint(const ConstraintProto& ct,
                                          const PresolveContext* context,
                                          std::vector<int>* variable_mapping,
                                          CpModelProto* mini_model);
}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PRESOLVE_CONTEXT_H_
