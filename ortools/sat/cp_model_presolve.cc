// Copyright 2010-2017 Google
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

#include "ortools/sat/cp_model_presolve.h"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <unordered_set>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/port.h"
#include "ortools/base/join.h"
#include <unordered_set>
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/hash.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_objective.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/simplification.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

// Returns the sorted list of variables used by a constraint.
std::vector<int> UsedVariables(const ConstraintProto& ct) {
  IndexReferences references;
  AddReferencesUsedByConstraint(ct, &references);

  std::vector<int> used_variables;
  for (const int var : references.variables) {
    used_variables.push_back(PositiveRef(var));
  }
  for (const int lit : references.literals) {
    used_variables.push_back(PositiveRef(lit));
  }
  if (HasEnforcementLiteral(ct)) {
    used_variables.push_back(PositiveRef(ct.enforcement_literal(0)));
  }
  gtl::STLSortAndRemoveDuplicates(&used_variables);
  return used_variables;
}

// Wrap the CpModelProto we are presolving with extra data structure like the
// in-memory domain of each variables and the constraint variable graph.
struct PresolveContext {
  bool DomainIsEmpty(int ref) const {
    return domains[PositiveRef(ref)].empty();
  }

  bool IsFixed(int ref) const {
    CHECK(!DomainIsEmpty(ref));
    return domains[PositiveRef(ref)].front().start ==
           domains[PositiveRef(ref)].back().end;
  }

  bool LiteralIsTrue(int lit) const {
    if (!IsFixed(lit)) return false;
    if (RefIsPositive(lit)) {
      return domains[lit].front().start == 1ll;
    } else {
      return domains[PositiveRef(lit)].front().start == 0ll;
    }
  }

  bool LiteralIsFalse(int lit) const {
    if (!IsFixed(lit)) return false;
    if (RefIsPositive(lit)) {
      return domains[lit].front().start == 0ll;
    } else {
      return domains[PositiveRef(lit)].front().start == 1ll;
    }
  }

  int64 MinOf(int ref) const {
    CHECK(!DomainIsEmpty(ref));
    return RefIsPositive(ref) ? domains[PositiveRef(ref)].front().start
                              : -domains[PositiveRef(ref)].back().end;
  }

  int64 MaxOf(int ref) const {
    CHECK(!DomainIsEmpty(ref));
    return RefIsPositive(ref) ? domains[PositiveRef(ref)].back().end
                              : -domains[PositiveRef(ref)].front().start;
  }

  // Returns true if this ref only appear in one constraint.
  bool IsUnique(int ref) const {
    return var_to_constraints[PositiveRef(ref)].size() == 1;
  }

  std::vector<ClosedInterval> GetRefDomain(int ref) const {
    if (RefIsPositive(ref)) return domains[ref];
    return NegationOfSortedDisjointIntervals(domains[PositiveRef(ref)]);
  }

  // Returns false if the domain changed.
  bool IntersectDomainWith(int ref, const std::vector<ClosedInterval>& domain) {
    const int var = PositiveRef(ref);
    tmp_domain = IntersectionOfSortedDisjointIntervals(
        domains[var], RefIsPositive(ref)
                          ? domain
                          : NegationOfSortedDisjointIntervals(domain));
    if (tmp_domain != domains[var]) {
      modified_domains.Set(var);
      domains[var] = tmp_domain;
      if (tmp_domain.empty()) {
        is_unsat = true;
      }
      return true;
    }
    return false;
  }

  void SetLiteralToFalse(int lit) {
    const int var = PositiveRef(lit);
    const int64 value = RefIsPositive(lit) ? 0ll : 1ll;
    IntersectDomainWith(var, {{value, value}});
  }

  void SetLiteralToTrue(int lit) { return SetLiteralToFalse(NegatedRef(lit)); }

  void UpdateRuleStats(const std::string& name) { stats_by_rule_name[name]++; }

  void UpdateConstraintVariableUsage(int c) {
    if (c >= constraint_to_vars.size()) constraint_to_vars.resize(c + 1);
    const ConstraintProto& ct = working_model->constraints(c);
    for (const int v : constraint_to_vars[c]) var_to_constraints[v].erase(c);
    constraint_to_vars[c] = UsedVariables(ct);
    for (const int v : constraint_to_vars[c]) var_to_constraints[v].insert(c);
  }

  // Regroups fixed variables with the same value.
  // TODO(user): Also regroup cte and -cte?
  void ExploitFixedDomain(int var) {
    CHECK(IsFixed(var));
    const int min = MinOf(var);
    if (gtl::ContainsKey(constant_to_ref, min)) {
      const int representative = constant_to_ref[min];
      if (representative != var) {
        affine_relations.TryAdd(var, representative, 1, 0);
        var_equiv_relations.TryAdd(var, representative, 1, 0);
      }
    } else {
      constant_to_ref[min] = var;
    }
  }

  // Adds the relation (ref_x = coeff * ref_y + offset) to the repository.
  void AddAffineRelation(const ConstraintProto& ct, int ref_x, int ref_y,
                         int64 coeff, int64 offset) {
    int x = PositiveRef(ref_x);
    int y = PositiveRef(ref_y);
    if (IsFixed(x) || IsFixed(y)) return;

    int64 c = RefIsPositive(ref_x) == RefIsPositive(ref_y) ? coeff : -coeff;
    int64 o = RefIsPositive(ref_x) ? offset : -offset;

    // If a Boolean variable (one with domain [0, 1]) appear in this affine
    // equivalence class, then we want its representative to be Boolean. Note
    // that this is always possible because a Boolean variable can never be
    // equal to a multiple of another if std::abs(coeff) is greater than 1 and
    // if it is not fixed to zero. This is important because it allows to simply
    // use the same representative for any referenced literals.
    const int rep_x = affine_relations.Get(x).representative;
    const int rep_y = affine_relations.Get(y).representative;
    bool force = false;
    if (MinOf(rep_y) == 0 && MaxOf(rep_y) == 1) {
      // We force the new representative to be rep_y.
      force = true;
    } else if (MinOf(rep_x) == 0 && MaxOf(rep_x) == 1) {
      // We force the new representative to be rep_x.
      force = true;
      std::swap(x, y);
      CHECK_EQ(std::abs(coeff), 1);  // Would be fixed to zero otherwise.
      if (coeff == 1) o = -o;
    }

    // TODO(user): can we force the rep and remove the GetAffineRelation()?
    bool added = force ? affine_relations.TryAddInGivenOrder(x, y, c, o)
                       : affine_relations.TryAdd(x, y, c, o);
    if ((c == 1 || c == -1) && o == 0) {
      added |= force ? var_equiv_relations.TryAddInGivenOrder(x, y, c, o)
                     : var_equiv_relations.TryAdd(x, y, c, o);
    }
    if (added) {
      // The domain didn't change, but this notification allows to re-process
      // any constraint containing these variables.
      modified_domains.Set(x);
      modified_domains.Set(y);
      affine_constraints.insert(&ct);
    }
  }

  // This makes sure that the affine relation only uses one of the
  // representative from the var_equiv_relations.
  AffineRelation::Relation GetAffineRelation(int var) {
    CHECK(RefIsPositive(var));
    AffineRelation::Relation r = affine_relations.Get(var);
    AffineRelation::Relation o = var_equiv_relations.Get(r.representative);
    r.representative = o.representative;
    if (o.coeff == -1) r.coeff = -r.coeff;
    return r;
  }

  // Create the internal structure for any new variables in working_model.
  void InitializeNewDomains() {
    for (int i = domains.size(); i < working_model->variables_size(); ++i) {
      domains.push_back(ReadDomain(working_model->variables(i)));
      if (IsFixed(i)) ExploitFixedDomain(i);
    }
    modified_domains.Resize(domains.size());
    var_to_constraints.resize(domains.size());
  }

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
  std::unordered_set<ConstraintProto const*> affine_constraints;

  // For each constant variable appearing in the model, we maintain a reference
  // variable with the same constant value. If two variables end up having the
  // same fixed value, then we can detect it using this and add a new
  // equivalence relation. See ExploitFixedDomain().
  std::unordered_map<int64, int> constant_to_ref;

  // Variable <-> constraint graph.
  // The vector list is sorted and contains unique elements.
  //
  // Important: To properly handle the objective, var_to_constraints[objective]
  // contains -1 so that if the objective appear in only one constraint, the
  // constraint cannot be simplified.
  //
  // TODO(user): Make this private?
  std::vector<std::vector<int>> constraint_to_vars;
  std::vector<std::unordered_set<int>> var_to_constraints;

  CpModelProto* working_model;
  CpModelProto* mapping_model;

  // Initially false, and set to true on the first inconsistency.
  bool is_unsat = false;

  // Just used to display statistics on the presolve rules that were used.
  std::unordered_map<std::string, int> stats_by_rule_name;

  // Temporary storage.
  std::vector<int> tmp_literals;
  std::vector<ClosedInterval> tmp_domain;
  std::vector<std::vector<ClosedInterval>> tmp_term_domains;
  std::vector<std::vector<ClosedInterval>> tmp_left_domains;

  // Each time a domain is modified this is set to true.
  SparseBitset<int64> modified_domains;

 private:
  // The current domain of each variables.
  std::vector<std::vector<ClosedInterval>> domains;
};

// =============================================================================
// Presolve functions.
//
// They should return false only if the constraint <-> variable graph didn't
// change. This is just an optimization, returning true is always correct.
//
// TODO(user): it migth be better to simply move all these functions to the
// PresolveContext class.
// =============================================================================

MUST_USE_RESULT bool RemoveConstraint(ConstraintProto* ct,
                                           PresolveContext* context) {
  ct->Clear();
  return true;
}

MUST_USE_RESULT bool MarkConstraintAsFalse(ConstraintProto* ct,
                                                PresolveContext* context) {
  if (HasEnforcementLiteral(*ct)) {
    context->SetLiteralToFalse(ct->enforcement_literal(0));
  } else {
    context->is_unsat = true;
  }
  return RemoveConstraint(ct, context);
}

bool PresolveEnforcementLiteral(ConstraintProto* ct, PresolveContext* context) {
  if (!HasEnforcementLiteral(*ct)) return false;

  const int literal = ct->enforcement_literal(0);
  if (context->LiteralIsTrue(literal)) {
    context->UpdateRuleStats("true enforcement literal");
    ct->clear_enforcement_literal();
    return true;
  }

  // TODO(user): because the cumulative and disjunctive constraint refer to this
  // interval, we cannot simply remove the constraint even if we know that this
  // optional interval will not be present. We could fix that by removing this
  // interval from these constraints, but it is difficult to do that in a
  // general code, so we will need the presolve for these constraint to take
  // care of that, and then we would be able to remove this interval if it is
  // not longer used.
  if (ct->constraint_case() == ConstraintProto::ConstraintCase::kInterval) {
    return false;
  }

  if (context->LiteralIsFalse(literal)) {
    context->UpdateRuleStats("false enforcement literal");
    return RemoveConstraint(ct, context);
  }
  if (context->IsUnique(literal)) {
    // We can simply set it to false and ignore the constraint in this case.
    context->UpdateRuleStats("enforcement literal not used");
    context->SetLiteralToFalse(literal);
    return RemoveConstraint(ct, context);
  }
  return false;
}

bool PresolveBoolOr(ConstraintProto* ct, PresolveContext* context) {
  // Move the enforcement literal inside the clause if any.
  if (HasEnforcementLiteral(*ct)) {
    // Note that we do not mark this as changed though since the literal in the
    // constraint are the same.
    context->UpdateRuleStats("bool_or: removed enforcement literal");
    ct->mutable_bool_or()->add_literals(NegatedRef(ct->enforcement_literal(0)));
    ct->clear_enforcement_literal();
  }

  // Inspects the literals and deal with fixed ones.
  //
  // TODO(user): detect if one literal is the negation of another in which
  // case the constraint is true. Remove duplicates too. Do the same for
  // the PresolveBoolAnd() function.
  bool changed = false;
  context->tmp_literals.clear();
  for (const int literal : ct->bool_or().literals()) {
    if (context->LiteralIsFalse(literal)) {
      changed = true;
      continue;
    }
    if (context->LiteralIsTrue(literal)) {
      context->UpdateRuleStats("bool_or: always true");
      return RemoveConstraint(ct, context);
    }
    // We can just set the variable to true in this case since it is not
    // used in any other constraint (note that we artifically bump the
    // objective var usage by 1).
    if (context->IsUnique(literal)) {
      context->UpdateRuleStats("bool_or: singleton");
      context->SetLiteralToTrue(literal);
      return RemoveConstraint(ct, context);
    }
    context->tmp_literals.push_back(literal);
  }

  if (context->tmp_literals.empty()) {
    context->UpdateRuleStats("bool_or: empty");
    return MarkConstraintAsFalse(ct, context);
  }
  if (context->tmp_literals.size() == 1) {
    context->UpdateRuleStats("bool_or: only one literal");
    context->SetLiteralToTrue(context->tmp_literals[0]);
    return RemoveConstraint(ct, context);
  }
  if (context->tmp_literals.size() == 2) {
    // For consistency, we move all "implication" into half-reified bool_and.
    // TODO(user): merge by enforcement literal and detect implication cycles.
    context->UpdateRuleStats("bool_or: implications");
    ct->add_enforcement_literal(NegatedRef(context->tmp_literals[0]));
    ct->mutable_bool_and()->add_literals(context->tmp_literals[1]);
    return changed;
  }

  if (changed) {
    context->UpdateRuleStats("bool_or: fixed literals");
    ct->mutable_bool_or()->mutable_literals()->Clear();
    for (const int lit : context->tmp_literals) {
      ct->mutable_bool_or()->add_literals(lit);
    }
  }
  return changed;
}

bool PresolveBoolAnd(ConstraintProto* ct, PresolveContext* context) {
  if (!HasEnforcementLiteral(*ct)) {
    context->UpdateRuleStats("bool_and: non-reified.");
    for (const int literal : ct->bool_and().literals()) {
      context->SetLiteralToTrue(literal);
    }
    return RemoveConstraint(ct, context);
  }

  bool changed = false;
  context->tmp_literals.clear();
  for (const int literal : ct->bool_and().literals()) {
    if (context->LiteralIsFalse(literal)) {
      context->UpdateRuleStats("bool_and: always false");
      return MarkConstraintAsFalse(ct, context);
    }
    if (context->LiteralIsTrue(literal)) {
      changed = true;
      continue;
    }
    if (context->IsUnique(literal)) {
      changed = true;
      context->SetLiteralToTrue(literal);
      continue;
    }
    context->tmp_literals.push_back(literal);
  }

  if (context->tmp_literals.empty()) return RemoveConstraint(ct, context);

  if (changed) {
    ct->mutable_bool_and()->mutable_literals()->Clear();
    for (const int lit : context->tmp_literals) {
      ct->mutable_bool_and()->add_literals(lit);
    }
    context->UpdateRuleStats("bool_and: fixed literals");
  }
  return changed;
}

bool PresolveIntMax(ConstraintProto* ct, PresolveContext* context) {
  if (ct->int_max().vars().empty()) {
    return MarkConstraintAsFalse(ct, context);
  }

  const int target_ref = ct->int_max().target();
  const int target_var = PositiveRef(target_ref);

  // Pass 1, compute the infered min of the target, and remove duplicates.
  int64 target_min = context->MinOf(target_ref);
  bool contains_target_ref = false;
  std::set<int> used_ref;
  int new_size = 0;
  for (const int ref : ct->int_max().vars()) {
    if (ref == target_ref) contains_target_ref = true;
    if (gtl::ContainsKey(used_ref, ref)) continue;
    if (gtl::ContainsKey(used_ref, NegatedRef(ref)) ||
        ref == NegatedRef(target_ref)) {
      target_min = std::max(target_min, 0ll);
    }
    used_ref.insert(ref);
    ct->mutable_int_max()->set_vars(new_size++, ref);
    target_min = std::max(target_min, context->MinOf(ref));
  }
  if (new_size < ct->int_max().vars_size()) {
    context->UpdateRuleStats("int_max: removed dup");
  }
  ct->mutable_int_max()->mutable_vars()->Truncate(new_size);
  if (contains_target_ref) {
    context->UpdateRuleStats("int_max: x = max(x, ...)");
    for (const int ref : ct->int_max().vars()) {
      if (ref == target_ref) continue;
      ConstraintProto* new_ct = context->working_model->add_constraints();
      *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
      auto* arg = new_ct->mutable_linear();
      arg->add_vars(target_ref);
      arg->add_coeffs(1);
      arg->add_vars(ref);
      arg->add_coeffs(-1);
      arg->add_domain(0);
      arg->add_domain(kint64max);
    }
    return RemoveConstraint(ct, context);
  }

  // Update the target domain.
  bool domain_reduced = false;
  if (!HasEnforcementLiteral(*ct)) {
    std::vector<ClosedInterval> infered_domain;
    for (const int ref : ct->int_max().vars()) {
      infered_domain = UnionOfSortedDisjointIntervals(
          infered_domain,
          IntersectionOfSortedDisjointIntervals(context->GetRefDomain(ref),
                                                {{target_min, kint64max}}));
    }
    domain_reduced |= context->IntersectDomainWith(target_ref, infered_domain);
  }

  // Pass 2, update the argument domains. Filter them eventually.
  new_size = 0;
  const int size = ct->int_max().vars_size();
  const int64 target_max = context->MaxOf(target_ref);
  for (const int ref : ct->int_max().vars()) {
    if (!HasEnforcementLiteral(*ct)) {
      domain_reduced |=
          context->IntersectDomainWith(ref, {{kint64min, target_max}});
    }
    if (context->MaxOf(ref) >= target_min) {
      ct->mutable_int_max()->set_vars(new_size++, ref);
    }
  }
  if (domain_reduced) {
    context->UpdateRuleStats("int_max: reduced domains");
  }

  bool modified = false;
  if (new_size < size) {
    context->UpdateRuleStats("int_max: removed variables");
    ct->mutable_int_max()->mutable_vars()->Truncate(new_size);
    modified = true;
  }

  // Note that we do that after the domains have been reduced.
  // TODO(user): Even in the reified case we could do something.
  // TODO(user): If the domains have holes, it is possible we will only detect
  // UNSAT at postsolve time, that might be an issue.
  if (new_size > 0 && !HasEnforcementLiteral(*ct) &&
      context->IsUnique(target_var)) {
    context->UpdateRuleStats("int_max: singleton target");
    *(context->mapping_model->add_constraints()) = *ct;
    return RemoveConstraint(ct, context);
  }
  if (new_size == 1) {
    // Convert to an equality. Note that we create a new constraint otherwise it
    // might not be processed again.
    context->UpdateRuleStats("int_max: converted to equality");
    ConstraintProto* new_ct = context->working_model->add_constraints();
    *new_ct = *ct;  // copy name and potential reification.
    auto* arg = new_ct->mutable_linear();
    arg->add_vars(target_ref);
    arg->add_coeffs(1);
    arg->add_vars(ct->int_max().vars(0));
    arg->add_coeffs(-1);
    arg->add_domain(0);
    arg->add_domain(0);
    return RemoveConstraint(ct, context);
  }
  return modified;
}

bool PresolveIntMin(ConstraintProto* ct, PresolveContext* context) {
  const auto copy = ct->int_min();
  ct->mutable_int_max()->set_target(NegatedRef(copy.target()));
  for (const int ref : copy.vars()) {
    ct->mutable_int_max()->add_vars(NegatedRef(ref));
  }
  return PresolveIntMax(ct, context);
}

bool PresolveIntProd(ConstraintProto* ct, PresolveContext* context) {
  if (HasEnforcementLiteral(*ct)) return false;
  // For now, we only presolve the case where all variable are Booleans.
  const int target_ref = ct->int_prod().target();
  if (!RefIsPositive(target_ref)) return false;
  for (const int var : ct->int_prod().vars()) {
    if (!RefIsPositive(var)) return false;
    if (context->MinOf(var) != 0) return false;
    if (context->MaxOf(var) != 1) return false;
  }

  // This is a bool constraint!
  context->UpdateRuleStats("int_prod: converted to reified bool_and");
  {
    ConstraintProto* new_ct = context->working_model->add_constraints();
    new_ct->add_enforcement_literal(target_ref);
    auto* arg = new_ct->mutable_bool_and();
    for (const int var : ct->int_prod().vars()) {
      arg->add_literals(var);
    }
  }
  {
    ConstraintProto* new_ct = context->working_model->add_constraints();
    auto* arg = new_ct->mutable_bool_or();
    arg->add_literals(target_ref);
    for (const int var : ct->int_prod().vars()) {
      arg->add_literals(NegatedRef(var));
    }
  }
  return RemoveConstraint(ct, context);
}

bool PresolveIntDiv(ConstraintProto* ct, PresolveContext* context) {
  // For now, we only presolve the case where the divisor is constant.
  const int target = ct->int_div().target();
  const int ref_x = ct->int_div().vars(0);
  const int ref_div = ct->int_div().vars(1);
  if (!RefIsPositive(target) || !RefIsPositive(ref_x) ||
      !RefIsPositive(ref_div) || !context->IsFixed(ref_div)) {
    return false;
  }

  const int64 divisor = context->MinOf(ref_div);
  if (divisor == 1) {
    context->UpdateRuleStats("TODO int_div: rewrite to equality");
  }
  if (context->IntersectDomainWith(
          target, DivisionOfSortedDisjointIntervals(
                      context->GetRefDomain(ref_x), divisor))) {
    context->UpdateRuleStats(
        "int_div: updated domain of target in target = X / cte");
  }

  // TODO(user): reduce the domain of X by introducing an
  // InverseDivisionOfSortedDisjointIntervals().
  return false;
}

bool ExploitEquivalenceRelations(ConstraintProto* ct,
                                 PresolveContext* context) {
  if (gtl::ContainsKey(context->affine_constraints, ct)) return false;
  bool changed = false;

  // Remap equal and negated variables to their representative.
  ApplyToAllVariableIndices(
      [&changed, context](int* ref) {
        const int var = PositiveRef(*ref);
        const AffineRelation::Relation r =
            context->var_equiv_relations.Get(var);
        if (r.representative != var) {
          CHECK_EQ(r.offset, 0);
          CHECK_EQ(std::abs(r.coeff), 1);
          *ref = (r.coeff == 1) == RefIsPositive(*ref)
                     ? r.representative
                     : NegatedRef(r.representative);
          changed = true;
        }
      },
      ct);

  // Remap literal and negated literal to their representative.
  ApplyToAllLiteralIndices(
      [&changed, context](int* ref) {
        const int var = PositiveRef(*ref);
        const AffineRelation::Relation r = context->GetAffineRelation(var);
        if (r.representative != var) {
          const bool is_positive = (r.offset == 0 && r.coeff == 1);
          CHECK(is_positive || r.offset == 1 && r.coeff == -1 ||
                context->IsFixed(var));
          *ref = (is_positive == RefIsPositive(*ref))
                     ? r.representative
                     : NegatedRef(r.representative);
          changed = true;
        }
      },
      ct);
  return changed;
}

bool PresolveLinear(ConstraintProto* ct, PresolveContext* context) {
  bool var_constraint_graph_changed = false;
  std::vector<ClosedInterval> rhs = ReadDomain(ct->linear());

  // First regroup the terms on the same variables and sum the fixed ones.
  // Note that we use a map to sort the variables and because we expect most
  // constraints to be small.
  //
  // TODO(user): move the map in context to reuse its memory. Add a quick pass
  // to skip most of the work below if the constraint is already in canonical
  // form (strictly increasing var, no-fixed var, gcd = 1).
  int64 sum_of_fixed_terms = 0;
  std::map<int, int64> var_to_coeff;
  const LinearConstraintProto& arg = ct->linear();
  const bool was_affine = gtl::ContainsKey(context->affine_constraints, ct);
  for (int i = 0; i < arg.vars_size(); ++i) {
    const int var = PositiveRef(arg.vars(i));
    const int64 coeff =
        RefIsPositive(arg.vars(i)) ? arg.coeffs(i) : -arg.coeffs(i);
    if (coeff == 0) continue;
    if (context->IsFixed(var)) {
      sum_of_fixed_terms += coeff * context->MinOf(var);
      continue;
    }

    if (!was_affine) {
      const AffineRelation::Relation r = context->GetAffineRelation(var);
      if (r.representative != var) {
        var_constraint_graph_changed = true;
        sum_of_fixed_terms += coeff * r.offset;
      }
      var_to_coeff[r.representative] += coeff * r.coeff;
      if (var_to_coeff[r.representative] == 0) {
        var_to_coeff.erase(r.representative);
      }
    } else {
      var_to_coeff[var] += coeff;
      if (var_to_coeff[var] == 0) var_to_coeff.erase(var);
    }
  }

  // Test for singleton variable. Not that we need to do that after the
  // canonicalization of the constraint in case a variable was appearing more
  // than once.
  if (!was_affine) {
    std::vector<int> var_to_erase;
    for (const auto entry : var_to_coeff) {
      const int var = entry.first;
      const int64 coeff = entry.second;
      if (context->IsUnique(var)) {
        bool success;
        const auto term_domain = PreciseMultiplicationOfSortedDisjointIntervals(
            context->GetRefDomain(var), -coeff, &success);
        if (success) {
          // Note that we can't do that if we loose information in the
          // multiplication above because the new domain might not be as strict
          // as the initial constraint otherwise. TODO(user): because of the
          // addition, it might be possible to cover more cases though.
          var_to_erase.push_back(var);
          rhs = AdditionOfSortedDisjointIntervals(rhs, term_domain);
          continue;
        }
      }
    }
    if (!var_to_erase.empty()) {
      for (const int var : var_to_erase) var_to_coeff.erase(var);
      context->UpdateRuleStats("linear: singleton column");
      // TODO(user): we could add the constraint to mapping_model only once
      // instead of adding a reduced version of it each time a new singleton
      // variable appear in the same constraint later. That would work but would
      // also force the postsolve to take search decisions...
      *(context->mapping_model->add_constraints()) = *ct;
    }
  }

  // Compute the GCD of all coefficients.
  int64 gcd = 1;
  bool first_coeff = true;
  for (const auto entry : var_to_coeff) {
    // GCD(gcd, coeff) = GCD(coeff, gcd % coeff);
    int64 coeff = std::abs(entry.second);
    if (first_coeff) {
      if (coeff != 0) {
        first_coeff = false;
        gcd = coeff;
      }
      continue;
    }
    while (coeff != 0) {
      const int64 r = gcd % coeff;
      gcd = coeff;
      coeff = r;
    }
    if (gcd == 1) break;
  }
  if (gcd > 1) {
    context->UpdateRuleStats("linear: divide by GCD");
  }

  if (var_to_coeff.size() < arg.vars_size()) {
    context->UpdateRuleStats("linear: fixed or dup variables");
    var_constraint_graph_changed = true;
  }

  // Rewrite the constraint in canonical form and update rhs (it will be copied
  // to the constraint later).
  if (sum_of_fixed_terms != 0) {
    rhs = AdditionOfSortedDisjointIntervals(
        rhs, {{-sum_of_fixed_terms, -sum_of_fixed_terms}});
  }
  if (gcd > 1) {
    rhs = InverseMultiplicationOfSortedDisjointIntervals(rhs, gcd);
  }
  ct->mutable_linear()->clear_vars();
  ct->mutable_linear()->clear_coeffs();
  for (const auto entry : var_to_coeff) {
    CHECK(RefIsPositive(entry.first));
    ct->mutable_linear()->add_vars(entry.first);
    ct->mutable_linear()->add_coeffs(entry.second / gcd);
  }

  // Empty constraint?
  if (ct->linear().vars().empty()) {
    context->UpdateRuleStats("linear: empty");
    if (SortedDisjointIntervalsContain(rhs, 0)) {
      return RemoveConstraint(ct, context);
    } else {
      return MarkConstraintAsFalse(ct, context);
    }
  }

  // Size one constraint?
  if (ct->linear().vars().size() == 1 && !HasEnforcementLiteral(*ct)) {
    const int64 coeff =
        RefIsPositive(arg.vars(0)) ? arg.coeffs(0) : -arg.coeffs(0);
    context->UpdateRuleStats("linear: size one");
    const int var = PositiveRef(arg.vars(0));
    if (coeff == 1) {
      context->IntersectDomainWith(var, rhs);
    } else {
      DCHECK_EQ(coeff, -1);  // Because of the GCD above.
      context->IntersectDomainWith(var, NegationOfSortedDisjointIntervals(rhs));
    }
    return RemoveConstraint(ct, context);
  }

  // Compute the implied rhs bounds from the variable ones.
  const int kDomainComplexityLimit = 100;
  auto& term_domains = context->tmp_term_domains;
  auto& left_domains = context->tmp_left_domains;
  const int num_vars = arg.vars_size();
  term_domains.resize(num_vars + 1);
  left_domains.resize(num_vars + 1);
  left_domains[0] = {{0, 0}};
  for (int i = 0; i < num_vars; ++i) {
    const int var = PositiveRef(arg.vars(i));
    const int64 coeff = arg.coeffs(i);
    const auto& domain = context->GetRefDomain(var);

    // TODO(user): Try PreciseMultiplicationOfSortedDisjointIntervals() if
    // the size is reasonable.
    term_domains[i] = MultiplicationOfSortedDisjointIntervals(domain, coeff);
    left_domains[i + 1] =
        AdditionOfSortedDisjointIntervals(left_domains[i], term_domains[i]);
    if (left_domains[i + 1].size() > kDomainComplexityLimit) {
      // We take a super-set, otherwise it will be too slow.
      // TODO(user): We could be smarter in how we compute this if we allow for
      // more than one intervals.
      left_domains[i + 1] = {
          {left_domains[i + 1].front().start, left_domains[i + 1].back().end}};
    }
  }
  const std::vector<ClosedInterval>& implied_rhs = left_domains[num_vars];

  // Abort if intersection is empty.
  const std::vector<ClosedInterval> restricted_rhs =
      IntersectionOfSortedDisjointIntervals(rhs, implied_rhs);
  if (restricted_rhs.empty()) {
    context->UpdateRuleStats("linear: infeasible");
    return MarkConstraintAsFalse(ct, context);
  }

  // Relax the constraint rhs for faster propagation.
  // TODO(user): add an IntersectionIsEmpty() function.
  rhs.clear();
  for (const ClosedInterval i : UnionOfSortedDisjointIntervals(
           restricted_rhs, ComplementOfSortedDisjointIntervals(implied_rhs))) {
    if (!IntersectionOfSortedDisjointIntervals({i}, restricted_rhs).empty()) {
      rhs.push_back(i);
    }
  }
  if (rhs.size() == 1 && rhs[0].start == kint64min && rhs[0].end == kint64max) {
    context->UpdateRuleStats("linear: always true");
    return RemoveConstraint(ct, context);
  }
  if (rhs != ReadDomain(ct->linear())) {
    context->UpdateRuleStats("linear: simplified rhs");
  }
  FillDomain(rhs, ct->mutable_linear());

  // Propagate the variable bounds.
  if (!HasEnforcementLiteral(*ct)) {
    bool new_bounds = false;
    std::vector<ClosedInterval> new_domain;
    std::vector<ClosedInterval> right_domain = {{0, 0}};
    term_domains[num_vars] = NegationOfSortedDisjointIntervals(rhs);
    for (int i = num_vars - 1; i >= 0; --i) {
      right_domain =
          AdditionOfSortedDisjointIntervals(right_domain, term_domains[i + 1]);
      if (right_domain.size() > kDomainComplexityLimit) {
        // We take a super-set, otherwise it will be too slow.
        right_domain = {{right_domain.front().start, right_domain.back().end}};
      }
      new_domain = InverseMultiplicationOfSortedDisjointIntervals(
          AdditionOfSortedDisjointIntervals(left_domains[i], right_domain),
          -arg.coeffs(i));
      if (context->IntersectDomainWith(arg.vars(i), new_domain)) {
        new_bounds = true;
      }
    }
    if (new_bounds) {
      context->UpdateRuleStats("linear: reduced variable domains");
    }
  }

  // Detect affine relation.
  //
  // TODO(user): it might be better to first add only the affine relation with
  // a coefficient of magnitude 1, and later the one with larger coeffs.
  if (!was_affine && !HasEnforcementLiteral(*ct)) {
    const LinearConstraintProto& arg = ct->linear();
    const int64 rhs_min = rhs.front().start;
    const int64 rhs_max = rhs.back().end;
    if (rhs_min == rhs_max && arg.vars_size() == 2) {
      const int v1 = arg.vars(0);
      const int v2 = arg.vars(1);
      const int64 coeff1 = arg.coeffs(0);
      const int64 coeff2 = arg.coeffs(1);
      if (coeff1 == 1) {
        context->AddAffineRelation(*ct, v1, v2, -coeff2, rhs_max);
      } else if (coeff2 == 1) {
        context->AddAffineRelation(*ct, v2, v1, -coeff1, rhs_max);
      } else if (coeff1 == -1) {
        context->AddAffineRelation(*ct, v1, v2, coeff2, -rhs_max);
      } else if (coeff2 == -1) {
        context->AddAffineRelation(*ct, v2, v1, coeff1, -rhs_max);
      }
    }
  }
  return var_constraint_graph_changed;
}

// Convert small linear constraint involving only Booleans to clauses.
bool PresolveLinearIntoClauses(ConstraintProto* ct, PresolveContext* context) {
  // TODO(user): the alternative to mark any newly created constraints might
  // be better.
  if (gtl::ContainsKey(context->affine_constraints, ct)) return false;
  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();
  int64 min_coeff = kint64max;
  int64 offset = 0;
  for (int i = 0; i < num_vars; ++i) {
    const int var = PositiveRef(arg.vars(i));
    if (context->MinOf(var) != 0) return false;
    if (context->MaxOf(var) != 1) return false;
    const int64 coeff = arg.coeffs(i);
    if (coeff > 0) {
      min_coeff = std::min(min_coeff, coeff);
    } else {
      // We replace the Boolean ref, by a ref to its negation (1 - x).
      offset += coeff;
      min_coeff = std::min(min_coeff, -coeff);
    }
  }

  // Detect clauses and reified ands.
  // TODO(user): split an == 1 constraint or similar into a clause and a <= 1
  // constraint?
  const std::vector<ClosedInterval> domain = ReadDomain(arg);
  DCHECK(!domain.empty());
  if (offset + min_coeff > domain.back().end) {
    // All Boolean are false if the reified literal is true.
    context->UpdateRuleStats("linear: reified and");
    const auto copy = arg;
    ct->mutable_bool_and()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_bool_and()->add_literals(
          copy.coeffs(i) > 0 ? NegatedRef(copy.vars(i)) : copy.vars(i));
    }
    return PresolveBoolAnd(ct, context);
  } else if (offset + min_coeff >= domain[0].start &&
             domain[0].end == kint64max) {
    // At least one Boolean is true.
    context->UpdateRuleStats("linear: clause");
    const auto copy = arg;
    ct->mutable_bool_or()->clear_literals();
    for (int i = 0; i < num_vars; ++i) {
      ct->mutable_bool_or()->add_literals(
          copy.coeffs(i) > 0 ? copy.vars(i) : NegatedRef(copy.vars(i)));
    }
    return PresolveBoolOr(ct, context);
  }

  // Expand small expression into clause.
  if (num_vars > 3) return false;
  context->UpdateRuleStats("linear: small Boolean expression");

  // Enumerate all possible value of the Booleans and add a clause if constraint
  // is false. TODO(user): the encoding could be made better in some cases.
  const int max_mask = (1 << arg.vars_size());
  for (int mask = 0; mask < max_mask; ++mask) {
    int64 value = 0;
    for (int i = 0; i < num_vars; ++i) {
      if ((mask >> i) & 1) value += arg.coeffs(i);
    }
    if (SortedDisjointIntervalsContain(domain, value)) continue;

    // Add a new clause to exclude this bad assignment.
    ConstraintProto* new_ct = context->working_model->add_constraints();
    auto* new_arg = new_ct->mutable_bool_or();
    if (HasEnforcementLiteral(*ct)) {
      new_ct->add_enforcement_literal(ct->enforcement_literal(0));
    }
    for (int i = 0; i < num_vars; ++i) {
      new_arg->add_literals(((mask >> i) & 1) ? NegatedRef(arg.vars(i))
                                              : arg.vars(i));
    }
  }

  return RemoveConstraint(ct, context);
}

bool PresolveInterval(ConstraintProto* ct, PresolveContext* context) {
  if (!ct->enforcement_literal().empty()) return false;
  const int start = ct->interval().start();
  const int end = ct->interval().end();
  const int size = ct->interval().size();
  bool changed = false;
  changed |= context->IntersectDomainWith(
      end, AdditionOfSortedDisjointIntervals(context->GetRefDomain(start),
                                             context->GetRefDomain(size)));
  changed |= context->IntersectDomainWith(
      start, AdditionOfSortedDisjointIntervals(
                 context->GetRefDomain(end), NegationOfSortedDisjointIntervals(
                                                 context->GetRefDomain(size))));
  changed |= context->IntersectDomainWith(
      size, AdditionOfSortedDisjointIntervals(
                context->GetRefDomain(end), NegationOfSortedDisjointIntervals(
                                                context->GetRefDomain(start))));
  if (changed) {
    context->UpdateRuleStats("interval: reduced domains");
  }

  // TODO(user): This currently has a side effect that both the interval and
  // a linear constraint are added to the presolved model. Fix.
  if (false && context->IsFixed(size)) {
    // We add it even if the interval is optional.
    // TODO(user): we must verify that all the variable of an optional interval
    // do not appear in a constraint which is not reified by the same literal.
    context->AddAffineRelation(*ct, ct->interval().end(),
                               ct->interval().start(), 1, context->MinOf(size));
  }

  // This never change the constraint-variable graph.
  return false;
}

bool PresolveElement(ConstraintProto* ct, PresolveContext* context) {
  const int index_ref = ct->element().index();
  const int target_ref = ct->element().target();

  // TODO(user): think about this once we do have such constraint.
  if (HasEnforcementLiteral(*ct)) return false;

  int num_vars = 0;
  bool all_constants = true;
  std::unordered_set<int64> constant_set;

  bool all_included_in_target_domain = true;
  bool reduced_index_domain = false;
  const std::vector<ClosedInterval> index_domain =
      context->GetRefDomain(index_ref);
  if (index_domain.front().start < 0 ||
      index_domain.back().end >= ct->element().vars_size()) {
    reduced_index_domain = true;
    context->IntersectDomainWith(index_ref,
                                 {{0, ct->element().vars_size() - 1}});
  }
  std::vector<ClosedInterval> infered_domain;
  const std::vector<ClosedInterval> target_dom =
      context->GetRefDomain(target_ref);
  for (const ClosedInterval interval : context->GetRefDomain(index_ref)) {
    for (int i = interval.start; i <= interval.end; ++i) {
      CHECK_GE(i, 0);
      CHECK_LT(i, ct->element().vars_size());
      const int ref = ct->element().vars(i);
      const auto& domain = context->GetRefDomain(ref);
      if (IntersectionOfSortedDisjointIntervals(target_dom, domain).empty()) {
        context->IntersectDomainWith(index_ref,
                                     {{kint64min, i - 1}, {i + 1, kint64max}});
        reduced_index_domain = true;
      } else {
        ++num_vars;
        if (domain.front().start == domain.back().end) {
          constant_set.insert(domain.front().start);
        } else {
          all_constants = false;
        }
        if (IntersectionOfSortedDisjointIntervals(
                target_dom, ComplementOfSortedDisjointIntervals(domain))
                .empty()) {
          all_included_in_target_domain = false;
        }
        infered_domain = UnionOfSortedDisjointIntervals(infered_domain, domain);
      }
    }
  }
  if (reduced_index_domain) {
    context->UpdateRuleStats("element: reduced index domain");
  }
  if (context->IntersectDomainWith(target_ref, infered_domain)) {
    context->UpdateRuleStats("element: reduced target domain");
  }

  const bool unique_index =
      context->IsUnique(index_ref) || context->IsFixed(index_ref);
  if (all_constants && unique_index) {
    // This constraint is just here to reduce the domain of the target! We can
    // add it to the mapping_model to reconstruct the index value during
    // postsolve and get rid of it now.
    context->UpdateRuleStats("element: trivial target domain reduction");
    *(context->mapping_model->add_constraints()) = *ct;
    return RemoveConstraint(ct, context);
  }
  const bool unique_target =
      context->IsUnique(target_ref) || context->IsFixed(target_ref);
  if (all_included_in_target_domain && unique_target) {
    context->UpdateRuleStats("element: trivial index domain reduction");
    *(context->mapping_model->add_constraints()) = *ct;
    return RemoveConstraint(ct, context);
  }

  if (all_constants && num_vars == constant_set.size()) {
    // TODO(user): We should be able to do something for simple mapping.
    context->UpdateRuleStats("TODO element: one to one mapping");
  }
  if (unique_target) {
    context->UpdateRuleStats("TODO element: target not used elsewhere");
  }
  if (context->IsFixed(index_ref)) {
    context->UpdateRuleStats("TODO element: fixed index.");
  } else if (unique_index) {
    context->UpdateRuleStats("TODO element: index not used elsewhere");
  }

  return false;
}

bool PresolveTable(ConstraintProto* ct, PresolveContext* context) {
  if (HasEnforcementLiteral(*ct)) return false;
  if (ct->table().negated()) return false;
  if (ct->table().vars().empty()) {
    context->UpdateRuleStats("table: empty constraint");
    return RemoveConstraint(ct, context);
  }

  // Filter the unreachable tuples.
  //
  // TODO(user): this is not supper efficient. Optimize if needed.
  const int num_vars = ct->table().vars_size();
  const int num_tuples = ct->table().values_size() / num_vars;
  std::vector<int64> tuple(num_vars);
  std::vector<std::vector<int64>> new_tuples;
  new_tuples.reserve(num_tuples);
  std::vector<std::unordered_set<int64>> new_domains(num_vars);
  for (int i = 0; i < num_tuples; ++i) {
    bool delete_row = false;
    std::string tmp;
    for (int j = 0; j < num_vars; ++j) {
      const int ref = ct->table().vars(j);
      const int64 v = ct->table().values(i * num_vars + j);
      tuple[j] = v;
      if (!SortedDisjointIntervalsContain(context->GetRefDomain(ref), v)) {
        delete_row = true;
        break;
      }
    }
    if (delete_row) continue;
    new_tuples.push_back(tuple);
    for (int j = 0; j < num_vars; ++j) {
      const int ref = ct->table().vars(j);
      const int64 v = tuple[j];
      new_domains[j].insert(RefIsPositive(ref) ? v : -v);
    }
  }
  gtl::STLSortAndRemoveDuplicates(&new_tuples);

  // Update the list of tuples if needed.
  if (new_tuples.size() < num_tuples) {
    ct->mutable_table()->clear_values();
    for (const std::vector<int64>& t : new_tuples) {
      for (const int64 v : t) {
        ct->mutable_table()->add_values(v);
      }
    }
    context->UpdateRuleStats("table: removed rows");
  }

  // Filter the variable domains.
  bool changed = false;
  for (int j = 0; j < num_vars; ++j) {
    const int ref = ct->table().vars(j);
    changed |= context->IntersectDomainWith(
        PositiveRef(ref), SortedDisjointIntervalsFromValues(std::vector<int64>(
                              new_domains[j].begin(), new_domains[j].end())));
  }
  if (changed) {
    context->UpdateRuleStats("table: reduced variable domains");
  }
  if (num_vars == 1) {
    // Now that we properly update the domain, we can remove the constraint.
    context->UpdateRuleStats("table: only one column!");
    return RemoveConstraint(ct, context);
  }

  // Check that the table is not complete or just here to exclude a few tuples.
  int64 prod = 1;
  for (int j = 0; j < num_vars; ++j) prod *= new_domains[j].size();
  if (prod == new_tuples.size()) {
    context->UpdateRuleStats("table: all tuples!");
    return RemoveConstraint(ct, context);
  }

  // Convert to the negated table if we gain a lot of entries by doing so.
  // Note however that currently the negated table do not propagate as much as
  // it could.
  if (new_tuples.size() > 0.7 * prod) {
    // Enumerate all tuples.
    std::vector<std::vector<int64>> var_to_values(num_vars);
    for (int j = 0; j < num_vars; ++j) {
      var_to_values[j].assign(new_domains[j].begin(), new_domains[j].end());
    }
    std::vector<std::vector<int64>> all_tuples(prod);
    for (int i = 0; i < prod; ++i) {
      all_tuples[i].resize(num_vars);
      int index = i;
      for (int j = 0; j < num_vars; ++j) {
        all_tuples[i][j] = var_to_values[j][index % var_to_values[j].size()];
        index /= var_to_values[j].size();
      }
    }
    gtl::STLSortAndRemoveDuplicates(&all_tuples);

    // Compute the complement of new_tuples.
    std::vector<std::vector<int64>> diff(prod - new_tuples.size());
    std::set_difference(all_tuples.begin(), all_tuples.end(),
                        new_tuples.begin(), new_tuples.end(), diff.begin());

    // Negate the constraint.
    ct->mutable_table()->set_negated(!ct->table().negated());
    ct->mutable_table()->clear_values();
    for (const std::vector<int64>& t : diff) {
      for (const int64 v : t) ct->mutable_table()->add_values(v);
    }
    context->UpdateRuleStats("table: negated");
  }
  return false;
}

bool PresolveAllDiff(ConstraintProto* ct, PresolveContext* context) {
  if (HasEnforcementLiteral(*ct)) return false;
  const int size = ct->all_diff().vars_size();
  if (size == 0) {
    context->UpdateRuleStats("all_diff: empty constraint");
    return RemoveConstraint(ct, context);
  }
  if (size == 1) {
    context->UpdateRuleStats("all_diff: only one variable");
    return RemoveConstraint(ct, context);
  }

  bool contains_fixed_variable = false;
  for (int i = 0; i < size; ++i) {
    if (context->IsFixed(ct->all_diff().vars(i))) {
      contains_fixed_variable = true;
      break;
    }
  }
  if (contains_fixed_variable) {
    context->UpdateRuleStats("TODO all_diff: fixed variables");
  }
  return false;
}

bool PresolveCumulative(ConstraintProto* ct, PresolveContext* context) {
  if (HasEnforcementLiteral(*ct)) return false;
  const CumulativeConstraintProto& proto = ct->cumulative();
  if (!context->IsFixed(proto.capacity())) return false;
  const int64 capacity = context->MinOf(proto.capacity());

  const int size = proto.intervals_size();
  std::vector<int> start_indices(size, -1);

  int num_duration_one = 0;
  int num_greater_half_capacity = 0;

  bool has_optional_interval = false;
  for (int i = 0; i < size; ++i) {
    // TODO(user): adapt in the presence of optional intervals.
    const ConstraintProto& ct =
        context->working_model->constraints(proto.intervals(i));
    if (!ct.enforcement_literal().empty()) has_optional_interval = true;
    const IntervalConstraintProto& interval = ct.interval();
    start_indices[i] = interval.start();
    const int duration_index = interval.size();
    const int demand_index = proto.demands(i);
    if (context->IsFixed(duration_index) &&
        context->MinOf(duration_index) == 1) {
      num_duration_one++;
    }
    if (context->MinOf(duration_index) == 0) {
      // The behavior for zero-duration interval is currently not the same in
      // the no-overlap and the cumulative constraint.
      return false;
    }
    const int64 demand_min = context->MinOf(demand_index);
    const int64 demand_max = context->MaxOf(demand_index);
    if (demand_min > capacity / 2) {
      num_greater_half_capacity++;
    }
    if (demand_min > capacity) {
      context->UpdateRuleStats("TODO cumulative: demand_min exceeds capacity");
    } else if (demand_max > capacity) {
      context->UpdateRuleStats("TODO cumulative: demand_max exceeds capacity");
    }
  }

  if (num_greater_half_capacity == size) {
    if (num_duration_one == size && !has_optional_interval) {
      context->UpdateRuleStats("cumulative: convert to all_different");
      ConstraintProto* new_ct = context->working_model->add_constraints();
      auto* arg = new_ct->mutable_all_diff();
      for (const int var : start_indices) {
        arg->add_vars(var);
      }
      return RemoveConstraint(ct, context);
    } else {
      context->UpdateRuleStats("cumulative: convert to no_overlap");
      ConstraintProto* new_ct = context->working_model->add_constraints();
      auto* arg = new_ct->mutable_no_overlap();
      for (const int interval : proto.intervals()) {
        arg->add_intervals(interval);
      }
      return RemoveConstraint(ct, context);
    }
  }

  return false;
}

bool PresolveCircuit(ConstraintProto* ct, PresolveContext* context) {
  if (HasEnforcementLiteral(*ct)) return false;
  CircuitConstraintProto& proto = *ct->mutable_circuit();

  std::vector<std::vector<int>> incoming_arcs;
  std::vector<std::vector<int>> outgoing_arcs;
  const int num_arcs = proto.literals_size();
  int num_nodes = 0;
  for (int i = 0; i < num_arcs; ++i) {
    const int ref = proto.literals(i);
    const int tail = proto.tails(i);
    const int head = proto.heads(i);
    num_nodes = std::max(tail, head) + 1;
    if (context->LiteralIsFalse(ref)) continue;
    if (std::max(tail, head) >= incoming_arcs.size()) {
      incoming_arcs.resize(std::max(tail, head) + 1);
      outgoing_arcs.resize(std::max(tail, head) + 1);
    }
    incoming_arcs[head].push_back(ref);
    outgoing_arcs[tail].push_back(ref);
  }

  int num_fixed_at_true = 0;
  for (const auto& node_to_refs : {incoming_arcs, outgoing_arcs}) {
    for (const std::vector<int>& refs : node_to_refs) {
      if (refs.size() == 1) {
        if (!context->LiteralIsTrue(refs.front())) {
          ++num_fixed_at_true;
          context->SetLiteralToTrue(refs.front());
        }
        continue;
      }

      // At most one true, so if there is one, mark all the other to false.
      int num_true = 0;
      int true_ref;
      for (const int ref : refs) {
        if (context->LiteralIsTrue(ref)) {
          ++num_true;
          true_ref = ref;
          break;
        }
      }
      if (num_true > 0) {
        for (const int ref : refs) {
          if (ref != true_ref) {
            context->SetLiteralToFalse(ref);
          }
        }
      }
    }
  }
  if (num_fixed_at_true > 0) {
    context->UpdateRuleStats("circuit: fixed singleton arcs.");
  }

  // Remove false arcs.
  //
  // TODO(user): all the outgoing/incoming arc of a node should not be all false
  // at the same time. Report unsat in this case. Note however that this part is
  // not well defined since if a node have no incoming/outgoing arcs in the
  // initial proto, it will just be ignored.
  int new_size = 0;
  int num_true = 0;
  int circuit_start = -1;
  std::vector<int> next(num_nodes, -1);
  for (int i = 0; i < num_arcs; ++i) {
    const int ref = proto.literals(i);
    if (context->LiteralIsFalse(ref)) continue;
    if (context->LiteralIsTrue(ref)) {
      if (next[proto.tails(i)] != -1) {
        context->is_unsat = true;
        return true;
      }
      next[proto.tails(i)] = proto.heads(i);
      if (proto.tails(i) != proto.heads(i)) {
        circuit_start = proto.tails(i);
      }
      ++num_true;
    }
    proto.set_tails(new_size, proto.tails(i));
    proto.set_heads(new_size, proto.heads(i));
    proto.set_literals(new_size, proto.literals(i));
    ++new_size;
  }

  // Test if a subcircuit is already present.
  if (circuit_start != -1) {
    std::vector<bool> visited(num_nodes, false);
    int current = circuit_start;
    while (current != -1 && !visited[current]) {
      visited[current] = true;
      current = next[current];
    }
    if (current == circuit_start) {
      // We have a sub-circuit! mark all other arc false except self-loop not in
      // circuit.
      for (int i = 0; i < num_arcs; ++i) {
        if (visited[proto.tails(i)]) continue;
        if (proto.tails(i) == proto.heads(i)) {
          context->SetLiteralToTrue(proto.literals(i));
        } else {
          context->SetLiteralToFalse(proto.literals(i));
        }
      }
      context->UpdateRuleStats("circuit: fully specified.");
      return RemoveConstraint(ct, context);
    }
  } else {
    // All self loop?
    if (num_true == new_size) {
      context->UpdateRuleStats("circuit: empty circuit.");
      return RemoveConstraint(ct, context);
    }
  }

  // Truncate the circuit and return.
  if (new_size < num_arcs) {
    proto.mutable_tails()->Truncate(new_size);
    proto.mutable_heads()->Truncate(new_size);
    proto.mutable_literals()->Truncate(new_size);
    context->UpdateRuleStats("circuit: removed false arcs.");
    return true;
  }
  return false;
}

template <typename ClauseContainer>
void ExtractClauses(const ClauseContainer& container, CpModelProto* proto) {
  // We regroup the "implication" into bool_and to have a more consise proto and
  // also for nicer information about the number of binary clauses.
  std::unordered_map<int, int> ref_to_bool_and;
  for (int i = 0; i < container.NumClauses(); ++i) {
    const std::vector<Literal>& clause = container.Clause(i);
    if (clause.empty()) continue;

    // bool_and.
    if (clause.size() == 2) {
      const int a = clause[0].IsPositive()
                        ? clause[0].Variable().value()
                        : NegatedRef(clause[0].Variable().value());
      const int b = clause[1].IsPositive()
                        ? clause[1].Variable().value()
                        : NegatedRef(clause[1].Variable().value());
      if (gtl::ContainsKey(ref_to_bool_and, NegatedRef(a))) {
        const int ct_index = ref_to_bool_and[NegatedRef(a)];
        proto->mutable_constraints(ct_index)->mutable_bool_and()->add_literals(
            b);
      } else if (gtl::ContainsKey(ref_to_bool_and, NegatedRef(b))) {
        const int ct_index = ref_to_bool_and[NegatedRef(b)];
        proto->mutable_constraints(ct_index)->mutable_bool_and()->add_literals(
            a);
      } else {
        ref_to_bool_and[NegatedRef(a)] = proto->constraints_size();
        ConstraintProto* ct = proto->add_constraints();
        ct->add_enforcement_literal(NegatedRef(a));
        ct->mutable_bool_and()->add_literals(b);
      }
      continue;
    }

    // bool_or.
    ConstraintProto* ct = proto->add_constraints();
    for (const Literal l : clause) {
      if (l.IsPositive()) {
        ct->mutable_bool_or()->add_literals(l.Variable().value());
      } else {
        ct->mutable_bool_or()->add_literals(NegatedRef(l.Variable().value()));
      }
    }
  }
}

void PresolvePureSatPart(PresolveContext* context) {
  const int num_variables = context->working_model->variables_size();
  SatPostsolver postsolver(num_variables);
  SatPresolver presolver(&postsolver);
  presolver.SetNumVariables(num_variables);

  SatParameters params;

  // TODO(user): enable blocked clause. The problem is that our postsolve
  // do not support changing the value of a variable from the solution of the
  // presolved problem, and we do need this for blocked clause.
  params.set_presolve_blocked_clause(false);

  // TODO(user): BVA takes times and do not seems to help on the minizinc
  // benchmarks. That said, it was useful on pure sat problems, so we may
  // want to enable it.
  params.set_presolve_use_bva(false);
  presolver.SetParameters(params);

  // Converts a cp_model literal ref to a sat::Literal used by SatPresolver.
  auto convert = [](int ref) {
    if (RefIsPositive(ref)) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };

  // Load all Clauses into the presolver and remove them from the current model.
  //
  // TODO(user): The removing and adding back of the same clause when nothing
  // happens in the presolve "seems" bad. That said, complexity wise, it is
  // a lot faster that what happens in the presolve though.
  std::vector<Literal> clause;
  int num_removed_constraints = 0;
  for (int i = 0; i < context->working_model->constraints_size(); ++i) {
    const ConstraintProto& ct = context->working_model->constraints(i);

    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
      ++num_removed_constraints;
      clause.clear();
      for (const int ref : ct.bool_or().literals()) {
        clause.push_back(convert(ref));
      }
      presolver.AddClause(clause);

      context->working_model->mutable_constraints(i)->Clear();
      context->UpdateConstraintVariableUsage(i);
      continue;
    }

    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolAnd) {
      ++num_removed_constraints;
      const Literal l = convert(ct.enforcement_literal(0)).Negated();
      CHECK(!ct.bool_and().literals().empty());
      for (const int ref : ct.bool_and().literals()) {
        presolver.AddClause({l, convert(ref)});
      }

      context->working_model->mutable_constraints(i)->Clear();
      context->UpdateConstraintVariableUsage(i);
      continue;
    }
  }

  // Abort early if there was no Boolean constraints.
  if (num_removed_constraints == 0) return;

  // Mark the variables appearing elsewhere or in the objective as non-removable
  // by the sat presolver.
  //
  // TODO(user): do not remove variable that appear in the decision heuristic?
  // TODO(user): We could go further for variable with only one polarity by
  // removing variable from the objective if they can be set to their "low"
  // objective value, and also removing enforcement literal that can be set to
  // false and don't appear elsewhere.
  int num_removable = 0;
  std::vector<bool> can_be_removed(num_variables, false);
  for (int i = 0; i < num_variables; ++i) {
    if (context->var_to_constraints[i].empty()) {
      ++num_removable;
      can_be_removed[i] = true;
    }
  }

  // Run the presolve for a small number of passes.
  // TODO(user): Add probing like we do in the pure sat solver presolve loop?
  // TODO(user): Add a time limit, this can be slow on big SAT problem.
  VLOG(1) << "num removable Booleans: " << num_removable;
  const int num_passes = params.presolve_use_bva() ? 4 : 1;
  for (int i = 0; i < num_passes; ++i) {
    const int old_num_clause = postsolver.NumClauses();
    if (!presolver.Presolve(can_be_removed)) {
      LOG(INFO) << "UNSAT during SAT presolve.";
      context->is_unsat = true;
      return;
    }
    if (old_num_clause == postsolver.NumClauses()) break;
  }

  // Add any new variables to our internal structure.
  const int new_num_variables = presolver.NumVariables();
  if (new_num_variables > context->working_model->variables_size()) {
    LOG(INFO) << "New variables added by the SAT presolver.";
    for (int i = context->working_model->variables_size();
         i < new_num_variables; ++i) {
      IntegerVariableProto* var_proto = context->working_model->add_variables();
      var_proto->add_domain(0);
      var_proto->add_domain(1);
    }
    context->InitializeNewDomains();
  }

  // Add the presolver clauses back into the model.
  const int old_ct_index = context->working_model->constraints_size();
  ExtractClauses(presolver, context->working_model);

  // Update the variable statistics.
  for (int ct_index = old_ct_index;
       ct_index < context->working_model->constraints_size(); ct_index++) {
    context->UpdateConstraintVariableUsage(ct_index);
  }

  // Add the postsolver clauses to mapping_model.
  ExtractClauses(postsolver, context->mapping_model);
}

}  // namespace.

// =============================================================================
// Public API.
// =============================================================================

// The presolve works as follow:
//
// First stage:
// We will process all active constraints until a fix point is reached. During
// this stage:
// - Variable will never be deleted, but their domain will be reduced.
// - Constraint will never be deleted (they will be marked as empty if needed).
// - New variables and new constraints can be added after the existing ones.
// - Constraints are added only when needed to the mapping_problem if they are
//   needed during the postsolve.
//
// Second stage:
// - All the variables domain will be copied to the mapping_model.
// - Everything will be remapped so that only the variables appearing in some
//   constraints will be kept and their index will be in [0, num_new_variables).
void PresolveCpModel(CpModelProto* presolved_model, CpModelProto* mapping_model,
                     std::vector<int>* postsolve_mapping) {
  PresolveContext context;
  context.working_model = presolved_model;
  context.mapping_model = mapping_model;

  // We copy the search strategy to the mapping_model.
  for (const auto& decision_strategy : presolved_model->search_strategy()) {
    *mapping_model->add_search_strategy() = decision_strategy;
  }

  // Encode linear objective, so that it can be presolved like a normal
  // constraint.
  EncodeObjectiveAsSingleVariable(context.working_model);

  // The queue of "active" constraints, initialized to all of them.
  std::vector<bool> in_queue(context.working_model->constraints_size(), true);
  std::deque<int> queue(context.working_model->constraints_size());
  std::iota(queue.begin(), queue.end(), 0);

  // This is used for constraint having unique variables in them (i.e. not
  // appearing anywhere else) to not call the presolve more than once for this
  // reason.
  std::unordered_set<std::pair<int, int>> var_constraint_pair_already_called;

  // Initialize the initial context.working_model domains.
  context.InitializeNewDomains();

  // Initialize the constraint <-> variable graph.
  context.constraint_to_vars.resize(context.working_model->constraints_size());
  context.var_to_constraints.resize(context.working_model->variables_size());
  for (int c = 0; c < context.working_model->constraints_size(); ++c) {
    context.UpdateConstraintVariableUsage(c);
  }

  // Hack for the optional variable so their literal is never considered to
  // appear in only one constraint. TODO(user): if it appears in none, then we
  // can remove the variable...
  for (int i = 0; i < context.working_model->variables_size(); ++i) {
    if (!context.working_model->variables(i).enforcement_literal().empty()) {
      context
          .var_to_constraints[PositiveRef(
              context.working_model->variables(i).enforcement_literal(0))]
          .insert(-1);
    }
  }

  // Hack for the objective so that it is never considered to appear in only one
  // constraint.
  if (context.working_model->has_objective()) {
    for (const int obj_var : context.working_model->objective().vars()) {
      context.var_to_constraints[PositiveRef(obj_var)].insert(-1);
    }
  }

  while (!queue.empty() && !context.is_unsat) {
    while (!queue.empty() && !context.is_unsat) {
      const int c = queue.front();
      in_queue[c] = false;
      queue.pop_front();

      const int old_num_constraint = context.working_model->constraints_size();
      ConstraintProto* ct = context.working_model->mutable_constraints(c);

      // Generic presolve to exploit variable/literal equivalence.
      if (ExploitEquivalenceRelations(ct, &context)) {
        context.UpdateConstraintVariableUsage(c);
      }

      // Generic presolve for reified constraint.
      if (PresolveEnforcementLiteral(ct, &context)) {
        context.UpdateConstraintVariableUsage(c);
      }

      // Call the presolve function for this constraint if any.
      bool changed = false;
      switch (ct->constraint_case()) {
        case ConstraintProto::ConstraintCase::kBoolOr:
          changed |= PresolveBoolOr(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kBoolAnd:
          changed |= PresolveBoolAnd(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kIntMax:
          changed |= PresolveIntMax(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kIntMin:
          changed |= PresolveIntMin(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kIntProd:
          changed |= PresolveIntProd(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kIntDiv:
          changed |= PresolveIntDiv(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kLinear:
          changed |= PresolveLinear(ct, &context);
          if (ct->constraint_case() ==
              ConstraintProto::ConstraintCase::kLinear) {
            // Tricky: This is needed in case the variables have been mapped to
            // their representative by PresolveLinear() above.
            if (changed) context.UpdateConstraintVariableUsage(c);
            changed |= PresolveLinearIntoClauses(ct, &context);
          }
          break;
        case ConstraintProto::ConstraintCase::kInterval:
          changed |= PresolveInterval(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kElement:
          changed |= PresolveElement(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kTable:
          changed |= PresolveTable(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kAllDiff:
          changed |= PresolveAllDiff(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kCumulative:
          changed |= PresolveCumulative(ct, &context);
          break;
        case ConstraintProto::ConstraintCase::kCircuit:
          changed |= PresolveCircuit(ct, &context);
          break;
        default:
          break;
      }

      // Update the variable <-> constraint graph if needed and add any new
      // constraint to the queue of active constraint.
      const int new_num_constraints = context.working_model->constraints_size();
      if (!changed) {
        CHECK_EQ(new_num_constraints, old_num_constraint);
        continue;
      }
      context.UpdateConstraintVariableUsage(c);
      if (new_num_constraints > old_num_constraint) {
        context.constraint_to_vars.resize(new_num_constraints);
        in_queue.resize(new_num_constraints, true);
        for (int c = old_num_constraint; c < new_num_constraints; ++c) {
          queue.push_back(c);
          context.UpdateConstraintVariableUsage(c);
        }
      }
    }

    // Re-add to the queue constraints that have unique variables. Note that to
    // not enter an infinite loop, we call each (var, constraint) pair at most
    // once.
    for (int v = 0; v < context.var_to_constraints.size(); ++v) {
      const auto& constraints = context.var_to_constraints[v];
      if (constraints.size() != 1) continue;
      const int c = *constraints.begin();
      if (c < 0) continue;
      if (gtl::ContainsKey(var_constraint_pair_already_called,
                           std::pair<int, int>(v, c))) {
        continue;
      }
      var_constraint_pair_already_called.insert({v, c});
      if (!in_queue[c]) {
        in_queue[c] = true;
        queue.push_back(c);
      }
    }

    // Re-add to the queue the constraints that touch a variable that changed.
    //
    // TODO(user): Avoid reprocessing the constraints that changed the variables
    // with the use of timestamp.
    const int old_queue_size = queue.size();
    for (const int v : context.modified_domains.PositionsSetAtLeastOnce()) {
      if (context.DomainIsEmpty(v)) {
        context.is_unsat = true;
        break;
      }
      if (context.IsFixed(v)) context.ExploitFixedDomain(v);
      for (const int c : context.var_to_constraints[v]) {
        if (c >= 0 && !in_queue[c]) {
          in_queue[c] = true;
          queue.push_back(c);
        }
      }
    }

    // Make sure the order is deterministic! because var_to_constraints[]
    // order changes from one run to the next.
    std::sort(queue.begin() + old_queue_size, queue.end());
    context.modified_domains.SparseClearAll();
  }

  // Run SAT specific presolve on the pure-SAT part of the problem.
  // Note that because this can only remove/fix variable not used in the other
  // part of the problem, there is no need to redo more presolve afterwards.
  //
  // TODO(user): expose the parameters here so we can use
  // cp_model_use_sat_presolve().
  PresolvePureSatPart(&context);

  if (context.is_unsat) {
    // Set presolved_model to the simplest UNSAT problem (empty clause).
    presolved_model->Clear();
    presolved_model->add_constraints()->mutable_bool_or();
    return;
  }

  // Because of EncodeObjectiveAsSingleVariable(), if we have an objective,
  // it is a single variable and canonicalized.
  if (context.working_model->has_objective()) {
    CHECK_EQ(context.working_model->objective().vars_size(), 1);
    CHECK_EQ(context.working_model->objective().coeffs(0), 1);
    const int initial_obj_ref = context.working_model->objective().vars(0);

    // TODO(user): Expand the linear equation recursively in order to have
    // as much term as possible? This would also enable expanding an objective
    // with multiple terms.
    int expanded_linear_index = -1;
    int64 objective_coeff_in_expanded_constraint;
    int64 size_of_expanded_constraint = 0;
    for (int ct_index = 0; ct_index < context.working_model->constraints_size();
         ++ct_index) {
      const ConstraintProto& ct = context.working_model->constraints(ct_index);
      // Skip everything that is not a linear equality constraint.
      if (!ct.enforcement_literal().empty()) continue;
      if (ct.constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
        continue;
      }
      if (ct.linear().domain().size() != 2) continue;
      if (ct.linear().domain(0) != ct.linear().domain(1)) continue;

      // Find out if initial_obj_ref appear in this constraint.
      bool present = false;
      int64 objective_coeff;
      const int num_terms = ct.linear().vars_size();
      for (int i = 0; i < num_terms; ++i) {
        const int ref = ct.linear().vars(i);
        const int64 coeff = ct.linear().coeffs(i);
        if (PositiveRef(ref) == PositiveRef(initial_obj_ref)) {
          CHECK(!present) << "Duplicate variables not supported";
          present = true;
          objective_coeff = (ref == initial_obj_ref) ? coeff : -coeff;
        }
      }

      // We use the longest equality we can find.
      // TODO(user): Deal with objective_coeff with a magnitude greater than 1?
      if (present && std::abs(objective_coeff) == 1 &&
          num_terms > size_of_expanded_constraint) {
        expanded_linear_index = ct_index;
        size_of_expanded_constraint = num_terms;
        objective_coeff_in_expanded_constraint = objective_coeff;
      }
    }

    if (expanded_linear_index != -1) {
      context.UpdateRuleStats("objective: expanded single objective");

      // Rewrite the objective. Note that we can do that because the objective
      // variable coefficient magnitude was one and so we can take its inverse.
      CHECK_EQ(std::abs(objective_coeff_in_expanded_constraint), 1);
      const int64 inverse = 1 / objective_coeff_in_expanded_constraint;

      const ConstraintProto& ct =
          context.working_model->constraints(expanded_linear_index);
      CpObjectiveProto* const mutable_objective =
          context.working_model->mutable_objective();
      const int64 offset_diff = ct.linear().domain(0) * inverse;
      mutable_objective->set_offset(mutable_objective->offset() +
                                    static_cast<double>(offset_diff));
      mutable_objective->clear_coeffs();
      mutable_objective->clear_vars();
      const int num_terms = ct.linear().vars_size();
      for (int i = 0; i < num_terms; ++i) {
        const int ref = ct.linear().vars(i);
        if (PositiveRef(ref) != PositiveRef(initial_obj_ref)) {
          mutable_objective->add_vars(ref);
          mutable_objective->add_coeffs(-ct.linear().coeffs(i) * inverse);
        }
      }
      FillDomain(AdditionOfSortedDisjointIntervals(
                     context.GetRefDomain(initial_obj_ref),
                     {{-offset_diff, -offset_diff}}),
                 mutable_objective);

      // Remove the objective variable special case and make sure the new
      // objective variables cannot be removed.
      for (int ref : ct.linear().vars()) {
        context.var_to_constraints[PositiveRef(ref)].insert(-1);
      }
      context.var_to_constraints[PositiveRef(initial_obj_ref)].erase(-1);

      // If the objective variable wasn't used in other constraint, we can
      // remove the linear equation.
      if (context.var_to_constraints[PositiveRef(initial_obj_ref)].size() ==
          1) {
        context.UpdateRuleStats("objective: removed old objective definition.");
        *(context.mapping_model->add_constraints()) = ct;
        context.working_model->mutable_constraints(expanded_linear_index)
            ->Clear();
        context.UpdateConstraintVariableUsage(expanded_linear_index);
      }
    }
  }

  // Remove all empty or affine constraints (they will be re-added later if
  // needed) in the presolved model. Note that we need to remap the interval
  // references.
  std::vector<int> interval_mapping(presolved_model->constraints_size(), -1);
  int new_num_constraints = 0;
  const int old_num_constraints = presolved_model->constraints_size();
  for (int i = 0; i < old_num_constraints; ++i) {
    const auto type = presolved_model->constraints(i).constraint_case();
    if (type == ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) continue;

    if (type == ConstraintProto::ConstraintCase::kInterval) {
      interval_mapping[i] = new_num_constraints;
    } else {
      // TODO(user): for now we don't remove interval because they can be used
      // in constraints.
      ConstraintProto* ct = presolved_model->mutable_constraints(i);
      if (gtl::ContainsKey(context.affine_constraints, ct)) {
        ct->Clear();
        context.UpdateConstraintVariableUsage(i);
        continue;
      }
    }
    presolved_model->mutable_constraints(new_num_constraints++)
        ->Swap(presolved_model->mutable_constraints(i));
  }
  presolved_model->mutable_constraints()->DeleteSubrange(
      new_num_constraints, old_num_constraints - new_num_constraints);
  for (ConstraintProto& ct_ref : *presolved_model->mutable_constraints()) {
    ApplyToAllIntervalIndices(
        [&interval_mapping](int* ref) {
          *ref = interval_mapping[*ref];
          DCHECK_NE(-1, *ref);
        },
        &ct_ref);
  }

  // Add back the affine relations to the presolved model or to the mapping
  // model, depending where they are needed.
  //
  // TODO(user): unfortunately, for now, this duplicates the interval relations
  // with a fixed size.
  int num_affine_relations = 0;
  for (int var = 0; var < presolved_model->variables_size(); ++var) {
    if (context.IsFixed(var)) continue;

    const AffineRelation::Relation r = context.GetAffineRelation(var);
    if (r.representative == var) continue;

    // We can get rid of this variable, only if:
    // - it is not used elsewhere.
    // - whatever the value of the representative, we can always find a value
    //   for this variable.
    CpModelProto* proto;
    if (context.var_to_constraints[var].empty()) {
      // Make sure that domain(representative) is tight.
      const auto implied = InverseMultiplicationOfSortedDisjointIntervals(
          AdditionOfSortedDisjointIntervals({{-r.offset, -r.offset}},
                                            context.GetRefDomain(var)),
          r.coeff);
      if (context.IntersectDomainWith(r.representative, implied)) {
        LOG(WARNING) << "Domain of " << r.representative
                     << " was not fully propagated using the affine relation "
                     << "(representative =" << r.representative
                     << ", coeff = " << r.coeff << ", offset = " << r.offset
                     << ")";
      }
      proto = context.mapping_model;
    } else {
      // This is needed for the corner cases where 2 variables in affine
      // relation with the same representative are present but no one use
      // the representative. This makes sure the code below will not try to
      // delete the representative.
      context.var_to_constraints[r.representative].insert(-1);
      proto = context.working_model;
      ++num_affine_relations;
    }

    ConstraintProto* ct = proto->add_constraints();
    auto* arg = ct->mutable_linear();
    arg->add_vars(var);
    arg->add_coeffs(1);
    arg->add_vars(r.representative);
    arg->add_coeffs(-r.coeff);
    arg->add_domain(r.offset);
    arg->add_domain(r.offset);
  }

  // The strategy variable indices will be remapped in ApplyVariableMapping()
  // but first we use the representative of the affine relations for the
  // variables that are not present anymore.
  //
  // Note that we properly take into account the sign of the coefficient which
  // will result in the same domain reduction strategy. Moreover, if the
  // variable order is not CHOOSE_FIRST, then we also encode the associated
  // affine transformation in order to preserve the order.
  std::unordered_set<int> used_variables;
  for (DecisionStrategyProto& strategy :
       *presolved_model->mutable_search_strategy()) {
    DecisionStrategyProto copy = strategy;
    strategy.clear_variables();
    for (const int ref : copy.variables()) {
      const int var = PositiveRef(ref);

      // Remove fixed variables.
      if (context.IsFixed(var)) continue;

      // There is not point having a variable appear twice, so we only keep
      // the first occurrence in the first strategy in which it occurs.
      if (gtl::ContainsKey(used_variables, var)) continue;
      used_variables.insert(var);

      if (context.var_to_constraints[var].empty()) {
        const AffineRelation::Relation r = context.GetAffineRelation(var);
        if (!context.var_to_constraints[r.representative].empty()) {
          const int rep = (r.coeff > 0) == RefIsPositive(ref)
                              ? r.representative
                              : NegatedRef(r.representative);
          strategy.add_variables(rep);
          if (strategy.variable_selection_strategy() !=
              DecisionStrategyProto::CHOOSE_FIRST) {
            DecisionStrategyProto::AffineTransformation* t =
                strategy.add_transformations();
            t->set_var(rep);
            t->set_offset(r.offset);
            t->set_positive_coeff(std::abs(r.coeff));
          }
        } else {
          // TODO(user): this variable was removed entirely by the presolve (no
          // equivalent variable present). We simply ignore it entirely which
          // might result in a different search...
        }
      } else {
        strategy.add_variables(ref);
      }
    }
  }

  // Update the variables domain of the presolved_model.
  for (int i = 0; i < presolved_model->variables_size(); ++i) {
    FillDomain(context.GetRefDomain(i), presolved_model->mutable_variables(i));
  }

  // Set the variables of the mapping_model.
  mapping_model->mutable_variables()->CopyFrom(presolved_model->variables());

  // Remove all the unused variables from the presolved model.
  postsolve_mapping->clear();
  std::vector<int> mapping(presolved_model->variables_size(), -1);
  for (int i = 0; i < presolved_model->variables_size(); ++i) {
    if (context.var_to_constraints[i].empty()) continue;
    mapping[i] = postsolve_mapping->size();
    postsolve_mapping->push_back(i);
  }
  ApplyVariableMapping(mapping, presolved_model);

  // Stats and checks.
  VLOG(1) << "- " << context.affine_relations.NumRelations()
          << " affine relations where detected. " << num_affine_relations
          << " where kept.";
  VLOG(1) << "- " << context.var_equiv_relations.NumRelations()
          << " variable equivalence relations where detected.";
  std::map<std::string, int> sorted_rules(context.stats_by_rule_name.begin(),
                                     context.stats_by_rule_name.end());
  for (const auto& entry : sorted_rules) {
    if (entry.second == 1) {
      VLOG(1) << "- rule '" << entry.first << "' was applied 1 time.";
    } else {
      VLOG(1) << "- rule '" << entry.first << "' was applied " << entry.second
              << " times.";
    }
  }
  CHECK_EQ("", ValidateCpModel(*presolved_model));
  CHECK_EQ("", ValidateCpModel(*mapping_model));
}

void ApplyVariableMapping(const std::vector<int>& mapping,
                          CpModelProto* proto) {
  // Remap all the variable/literal references in the constraints and the
  // enforcement literals in the variables.
  auto mapping_function = [&mapping](int* ref) {
    const int image = mapping[PositiveRef(*ref)];
    CHECK_GE(image, 0);
    *ref = RefIsPositive(*ref) ? image : NegatedRef(image);
  };
  for (ConstraintProto& ct_ref : *proto->mutable_constraints()) {
    ApplyToAllVariableIndices(mapping_function, &ct_ref);
    ApplyToAllLiteralIndices(mapping_function, &ct_ref);
  }
  for (IntegerVariableProto& variable_proto : *proto->mutable_variables()) {
    for (int& ref : *variable_proto.mutable_enforcement_literal()) {
      mapping_function(&ref);
    }
  }

  // Remap the objective variables.
  if (proto->has_objective()) {
    for (int& mutable_ref : *proto->mutable_objective()->mutable_vars()) {
      mapping_function(&mutable_ref);
    }
  }

  // Remap the search decision heuristic.
  // Note that we delete any heuristic related to a removed variable.
  for (DecisionStrategyProto& strategy : *proto->mutable_search_strategy()) {
    DecisionStrategyProto copy = strategy;
    strategy.clear_variables();
    for (const int ref : copy.variables()) {
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        strategy.add_variables(RefIsPositive(ref) ? image : NegatedRef(image));
      }
    }
    strategy.clear_transformations();
    for (const auto& transform : copy.transformations()) {
      const int ref = transform.var();
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        auto* new_transform = strategy.add_transformations();
        *new_transform = transform;
        new_transform->set_var(RefIsPositive(ref) ? image : NegatedRef(image));
      }
    }
  }

  // Remap the solution hint.
  if (proto->has_solution_hint()) {
    auto* mutable_hint = proto->mutable_solution_hint();
    int new_size = 0;
    for (int i = 0; i < mutable_hint->vars_size(); ++i) {
      const int ref = mutable_hint->vars(i);
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        mutable_hint->set_vars(new_size,
                               RefIsPositive(ref) ? image : NegatedRef(image));
        mutable_hint->set_values(new_size, mutable_hint->values(i));
        ++new_size;
      }
    }
    if (new_size > 0) {
      mutable_hint->mutable_vars()->Truncate(new_size);
      mutable_hint->mutable_values()->Truncate(new_size);
    } else {
      proto->clear_solution_hint();
    }
  }

  // Move the variable definitions.
  std::vector<IntegerVariableProto> new_variables;
  for (int i = 0; i < mapping.size(); ++i) {
    const int image = mapping[i];
    if (image < 0) continue;
    if (image >= new_variables.size()) {
      new_variables.resize(image + 1, IntegerVariableProto());
    }
    new_variables[image].Swap(proto->mutable_variables(i));
  }
  proto->clear_variables();
  for (IntegerVariableProto& proto_ref : new_variables) {
    proto->add_variables()->Swap(&proto_ref);
  }

  // Check that all variables are used.
  for (const IntegerVariableProto& v : proto->variables()) {
    CHECK_GT(v.domain_size(), 0);
  }
}

}  // namespace sat
}  // namespace operations_research
