// Copyright 2010-2014 Google
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

#include <deque>
#include <functional>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

// =============================================================================
// Utilities.
// =============================================================================

// An in-memory representation of a variable domain with convenient functions.
class Domain {
 public:
  // This takes a pointer to an "external" SparseBitset whose position "id" will
  // be set to true each time this domain changes.
  Domain(const IntegerVariableProto& var_proto, int id,
         SparseBitset<int64>* watcher)
      : id_(id),
        watcher_(watcher),
        sorted_disjoint_intervals_(ReadDomain(var_proto)) {}

  bool IsEmpty() const { return sorted_disjoint_intervals_.empty(); }
  int64 Min() const { return sorted_disjoint_intervals_.front().start; }
  int64 Max() const { return sorted_disjoint_intervals_.back().end; }

  bool Contains(int64 value) const {
    return SortedDisjointIntervalsContain(sorted_disjoint_intervals_, value);
  }

  bool IsFixedTo(int64 value) const {
    if (IsEmpty()) return false;
    return Min() == value && Max() == value;
  }

  bool IsFixed() const {
    if (IsEmpty()) return false;
    return Min() == Max();
  }

  // Returns true iff the domain changed.
  bool IntersectWith(const std::vector<ClosedInterval>& intervals) {
    tmp_ = IntersectionOfSortedDisjointIntervals(sorted_disjoint_intervals_,
                                                 intervals);
    if (tmp_ != sorted_disjoint_intervals_) {
      watcher_->Set(id_);
      sorted_disjoint_intervals_ = tmp_;
      return true;
    }
    return false;
  }
  bool IntersectWith(const Domain& domain) {
    return IntersectWith(domain.sorted_disjoint_intervals_);
  }

  // This works in O(n).
  // TODO(user): Move to O(log(n)) if needed.
  void RemovePoint(int64 point) {
    CHECK_NE(point, kint64min);
    CHECK_NE(point, kint64max);
    if (Contains(point)) {
      watcher_->Set(id_);
      IntersectWith({{kint64min, point - 1}, {point + 1, kint64max}});
    }
  }

  void CopyToIntegerVariableProto(IntegerVariableProto* proto) const {
    FillDomain(sorted_disjoint_intervals_, proto);
  }

  const std::vector<ClosedInterval>& InternalRepresentation() const {
    return sorted_disjoint_intervals_;
  }

  void NotifyChanged() { watcher_->Set(id_); }

 private:
  const int id_;
  SparseBitset<int64>* watcher_;
  std::vector<ClosedInterval> sorted_disjoint_intervals_;
  std::vector<ClosedInterval> tmp_;
};

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
  STLSortAndRemoveDuplicates(&used_variables);
  return used_variables;
}

struct PresolveContext {
  bool LiteralIsTrue(int lit) const {
    if (lit >= 0) return !domains[lit].Contains(0);
    return domains[NegatedRef(lit)].IsFixedTo(0);
  }
  bool LiteralIsFalse(int lit) const { return LiteralIsTrue(NegatedRef(lit)); }

  void SetLiteralToFalse(int lit) {
    const int var = PositiveRef(lit);
    if (lit >= 0) {
      domains[var].IntersectWith({{0ll, 0ll}});
    } else {
      domains[var].RemovePoint(0ll);
    }
    if (domains[var].IsEmpty()) {
      is_unsat = true;
    }
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

  int64 MinOf(int ref) const {
    return RefIsPositive(ref) ? domains[ref].Min()
                              : -domains[PositiveRef(ref)].Max();
  }
  int64 MaxOf(int ref) const {
    return RefIsPositive(ref) ? domains[ref].Max()
                              : -domains[PositiveRef(ref)].Min();
  }

  // Regroups fixed variables with the same value.
  // TODO(user): Also regroup cte and -cte?
  void ExploitFixedDomain(int var) {
    CHECK(domains[var].IsFixed());
    const int min = domains[var].Min();
    if (ContainsKey(constant_to_ref, min)) {
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
    if (domains[x].IsFixed() || domains[y].IsFixed()) return;

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
    if (domains[rep_y].Min() == 0 && domains[rep_y].Max() == 1) {
      // We force the new representative to be rep_y.
      force = true;
    } else if (domains[rep_x].Min() == 0 && domains[rep_x].Max() == 1) {
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
      domains[x].NotifyChanged();
      domains[y].NotifyChanged();
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

  // Returns the current domain of the given variable reference.
  std::vector<ClosedInterval> GetRefDomain(int ref) const {
    if (RefIsPositive(ref)) return domains[ref].InternalRepresentation();
    return NegationOfSortedDisjointIntervals(
        domains[PositiveRef(ref)].InternalRepresentation());
  }

  // The current domain of each variables.
  std::vector<Domain> domains;

  // This regroup all the affine relations between variables. Note that the
  // constraints used to detect such relations will not be removed from the
  // model at detection time (thus allowing proper domain propagation). However,
  // if the arity of a variable becomes one, then such constraint will be
  // removed.
  AffineRelation affine_relations;
  AffineRelation var_equiv_relations;

  // Set of constraint that implies an "affine relation". We need to mark them,
  // because we can't simplify them using the relation they added.
  std::unordered_set<ConstraintProto const*> affine_constraints;

  // For each constant variable appearing in the model, we maintain a reference
  // variable with the same constant value. If two variables end up having the
  // same fixed value, then we can detect it using this and add a new
  // equivalence relation. See TestAndExploitFixedDomain().
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

  // Temporary storage for PresolveLinear().
  std::vector<std::vector<ClosedInterval>> tmp_term_domains;
  std::vector<std::vector<ClosedInterval>> tmp_left_domains;
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
  if (context->LiteralIsFalse(literal)) {
    context->UpdateRuleStats("false enforcement literal");
    return RemoveConstraint(ct, context);
  }
  if (context->var_to_constraints[PositiveRef(literal)].size() == 1) {
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
  google::protobuf::RepeatedField<int> new_literals;
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
    if (context->var_to_constraints[PositiveRef(literal)].size() == 1) {
      context->SetLiteralToTrue(literal);
      return RemoveConstraint(ct, context);
    }
    new_literals.Add(literal);
  }

  if (new_literals.empty()) {
    context->UpdateRuleStats("bool_or: empty");
    return MarkConstraintAsFalse(ct, context);
  }
  if (new_literals.size() == 1) {
    context->UpdateRuleStats("bool_or: only one literal");
    context->SetLiteralToTrue(new_literals.Get(0));
    return RemoveConstraint(ct, context);
  }
  if (new_literals.size() == 2) {
    // For consistency, we move all "implication" into half-reified bool_and.
    // TODO(user): merge by enforcement literal and detect implication cycles.
    context->UpdateRuleStats("bool_or: implications");
    ct->add_enforcement_literal(NegatedRef(new_literals.Get(0)));
    ct->mutable_bool_and()->add_literals(new_literals.Get(1));
    return changed;
  }

  ct->mutable_bool_or()->mutable_literals()->Swap(&new_literals);
  if (changed) context->UpdateRuleStats("bool_or: fixed literals");
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
  google::protobuf::RepeatedField<int> new_literals;
  for (const int literal : ct->bool_and().literals()) {
    if (context->LiteralIsFalse(literal)) {
      context->UpdateRuleStats("bool_and: always false");
      return MarkConstraintAsFalse(ct, context);
    }
    if (context->LiteralIsTrue(literal)) {
      changed = true;
      continue;
    }
    if (context->var_to_constraints[PositiveRef(literal)].size() == 1) {
      changed = true;
      context->SetLiteralToTrue(literal);
      continue;
    }
    new_literals.Add(literal);
  }

  if (new_literals.empty()) return RemoveConstraint(ct, context);

  ct->mutable_bool_and()->mutable_literals()->Swap(&new_literals);
  if (changed) context->UpdateRuleStats("bool_and: fixed literals");
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
  std::string old = ct->DebugString();
  for (const int ref : ct->int_max().vars()) {
    if (ref == target_ref) contains_target_ref = true;
    if (ContainsKey(used_ref, ref)) continue;
    if (ContainsKey(used_ref, NegatedRef(ref)) ||
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
    context->UpdateRuleStats("int_max: x = std::max(x, ...)");
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
    if (!RefIsPositive(target_ref)) {
      infered_domain = NegationOfSortedDisjointIntervals(infered_domain);
    }
    domain_reduced |=
        context->domains[target_var].IntersectWith(infered_domain);
  }

  // Pass 2, update the argument domains. Filter them eventually.
  new_size = 0;
  const int size = ct->int_max().vars_size();
  const int64 target_max = context->MaxOf(target_ref);
  for (const int ref : ct->int_max().vars()) {
    if (!HasEnforcementLiteral(*ct)) {
      if (RefIsPositive(ref)) {
        domain_reduced |= context->domains[PositiveRef(ref)].IntersectWith(
            {{kint64min, target_max}});
      } else {
        domain_reduced |= context->domains[PositiveRef(ref)].IntersectWith(
            {{-target_max, kint64max}});
      }
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
      context->var_to_constraints[target_var].size() == 1) {
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
  // For now, we only presolve the case where all variable are Booleans.
  const int target_ref = ct->int_prod().target();
  if (!RefIsPositive(target_ref)) return false;
  for (const int var : ct->int_prod().vars()) {
    if (!RefIsPositive(var)) return false;
    if (context->domains[var].Min() != 0) return false;
    if (context->domains[var].Max() != 1) return false;
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

bool ExploitEquivalenceRelations(ConstraintProto* ct,
                                 PresolveContext* context) {
  if (ContainsKey(context->affine_constraints, ct)) return false;
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
                context->domains[var].IsFixed());
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
  bool var_was_removed = false;
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
  const bool was_affine = ContainsKey(context->affine_constraints, ct);
  for (int i = 0; i < arg.vars_size(); ++i) {
    const int var = PositiveRef(arg.vars(i));
    const int64 coeff =
        RefIsPositive(arg.vars(i)) ? arg.coeffs(i) : -arg.coeffs(i);
    if (coeff == 0) continue;
    if (context->domains[var].IsFixed()) {
      sum_of_fixed_terms += coeff * context->domains[var].Min();
    } else {
      if (!was_affine && context->var_to_constraints[var].size() == 1) {
        bool success;
        const auto term_domain = PreciseMultiplicationOfSortedDisjointIntervals(
            context->domains[var].InternalRepresentation(), -coeff, &success);
        if (success) {
          // Note that we can't do that if we loose information in the
          // multiplication above because the new domain might not be as strict
          // as the initial constraint otherwise. TODO(user): because of the
          // addition, it might be possible to cover more cases though.
          var_was_removed = true;
          rhs = AdditionOfSortedDisjointIntervals(rhs, term_domain);
          continue;
        }
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
  }
  if (var_was_removed) {
    context->UpdateRuleStats("linear: singleton column");
    // TODO(user): we could add the constraint to mapping_model only once
    // instead of adding a reduced version of it each time a new singleton
    // variable appear in the same constraint later. That would work but would
    // also force the postsolve to take search decisions...
    *(context->mapping_model->add_constraints()) = *ct;
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
      context->domains[var].IntersectWith(rhs);
    } else {
      DCHECK_EQ(coeff, -1);  // Because of the GCD above.
      context->domains[var].IntersectWith(
          NegationOfSortedDisjointIntervals(rhs));
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
    const auto& domain = context->domains[var].InternalRepresentation();

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
      if (context->domains[arg.vars(i)].IntersectWith(new_domain)) {
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
  if (ContainsKey(context->affine_constraints, ct)) return false;
  const LinearConstraintProto& arg = ct->linear();
  const int num_vars = arg.vars_size();
  int64 min_coeff = kint64max;
  int64 offset = 0;
  for (int i = 0; i < num_vars; ++i) {
    const int var = PositiveRef(arg.vars(i));
    if (context->domains[var].Min() != 0) return false;
    if (context->domains[var].Max() != 1) return false;
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
  // TODO(user): find a way to not care about this by extending the context API.
  if (!RefIsPositive(ct->interval().start())) return false;
  if (!RefIsPositive(ct->interval().end())) return false;
  if (!RefIsPositive(ct->interval().size())) return false;

  Domain& start = context->domains[PositiveRef(ct->interval().start())];
  Domain& end = context->domains[PositiveRef(ct->interval().end())];
  Domain& size = context->domains[PositiveRef(ct->interval().size())];
  bool changed = false;
  changed |= end.IntersectWith(AdditionOfSortedDisjointIntervals(
      start.InternalRepresentation(), size.InternalRepresentation()));
  changed |= start.IntersectWith(AdditionOfSortedDisjointIntervals(
      end.InternalRepresentation(),
      NegationOfSortedDisjointIntervals(size.InternalRepresentation())));
  changed |= size.IntersectWith(AdditionOfSortedDisjointIntervals(
      end.InternalRepresentation(),
      NegationOfSortedDisjointIntervals(start.InternalRepresentation())));
  if (changed) {
    context->UpdateRuleStats("interval: reduced domains");
  }

  if (size.IsFixed()) {
    // We add it even if the interval is optional.
    // TODO(user): we must verify that all the variable of an optional interval
    // do not appear in a constraint which is not reified by the same literal.
    context->AddAffineRelation(*ct, ct->interval().end(),
                               ct->interval().start(), 1, size.Min());
  }

  // This never change the constraint-variable graph.
  return false;
}

bool PresolveElement(ConstraintProto* ct, PresolveContext* context) {
  const int index_ref = ct->element().index();
  if (context->var_to_constraints[PositiveRef(index_ref)].size() == 1) {
    context->UpdateRuleStats("TODO element: index not used elsewhere");
  }
  const int target_ref = ct->element().target();
  if (context->var_to_constraints[PositiveRef(target_ref)].size() == 1) {
    context->UpdateRuleStats("TODO element: target not used elsewhere");
  }

  if (HasEnforcementLiteral(*ct)) return false;

  bool reduced_index_domain = false;
  std::vector<ClosedInterval> infered_domain;
  const std::vector<ClosedInterval> target_dom =
      context->GetRefDomain(target_ref);
  for (const ClosedInterval interval : context->GetRefDomain(index_ref)) {
    for (int i = interval.start; i <= interval.end; ++i) {
      CHECK_GE(i, 0);
      CHECK_LE(i, ct->element().vars_size());
      const int ref = ct->element().vars(i);
      const auto& domain = context->GetRefDomain(ref);
      if (IntersectionOfSortedDisjointIntervals(target_dom, domain).empty()) {
        context->domains[PositiveRef(index_ref)].RemovePoint(
            RefIsPositive(index_ref) ? i : -i);
        reduced_index_domain = true;
      } else {
        infered_domain = UnionOfSortedDisjointIntervals(infered_domain, domain);
      }
    }
  }
  if (reduced_index_domain) {
    context->UpdateRuleStats("element: reduced index domain");
  }
  if (!RefIsPositive(target_ref)) {
    infered_domain = NegationOfSortedDisjointIntervals(infered_domain);
  }
  if (context->domains[PositiveRef(target_ref)].IntersectWith(infered_domain)) {
    context->UpdateRuleStats("element: reduced target domain");
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
      if (!context->domains[PositiveRef(ref)].Contains(
              RefIsPositive(ref) ? v : -v)) {
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
  STLSortAndRemoveDuplicates(&new_tuples);

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
    changed |= context->domains[PositiveRef(ref)].IntersectWith(
        SortedDisjointIntervalsFromValues(
            std::vector<int64>(new_domains[j].begin(), new_domains[j].end())));
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
    STLSortAndRemoveDuplicates(&all_tuples);

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
    if (context->domains[PositiveRef(ct->all_diff().vars(i))].IsFixed()) {
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
  if (!context->domains[PositiveRef(proto.capacity())].IsFixed()) {
    return false;
  }
  const int64 capacity = context->domains[PositiveRef(proto.capacity())].Min();

  const int size = proto.intervals_size();
  std::vector<int> start_indices(size, -1);

  int num_duration_one = 0;
  int num_greater_half_capacity = 0;

  for (int i = 0; i < size; ++i) {
    const IntervalConstraintProto& interval =
        context->working_model->constraints(proto.intervals(i)).interval();
    start_indices[i] = interval.start();
    const int duration_index = interval.size();
    const int demand_index = proto.demands(i);
    if (context->domains[duration_index].IsFixedTo(1)) {
      num_duration_one++;
    }
    const int demand_min = context->domains[demand_index].Min();
    const int demand_max = context->domains[demand_index].Max();
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
    if (num_duration_one == size) {
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
void PresolveCpModel(const CpModelProto& initial_model,
                     CpModelProto* presolved_model, CpModelProto* mapping_model,
                     std::vector<int>* postsolve_mapping) {
  // The list of modified domain.
  SparseBitset<int64> modified_domains(initial_model.variables_size());

  PresolveContext context;
  for (int i = 0; i < initial_model.variables_size(); ++i) {
    context.domains.push_back(
        Domain(initial_model.variables(i), i, &modified_domains));
    if (context.domains[i].IsFixed()) context.ExploitFixedDomain(i);
  }
  context.working_model = presolved_model;
  context.mapping_model = mapping_model;
  *presolved_model = initial_model;

  // We copy the search strategy from the initial_model to mapping_model.
  for (const auto& decision_strategy : initial_model.search_strategy()) {
    *mapping_model->add_search_strategy() = decision_strategy;
  }

  // The queue of "active" constraints, initialized to all of them.
  std::vector<bool> in_queue(initial_model.constraints_size(), true);
  std::deque<int> queue(initial_model.constraints_size());
  std::iota(queue.begin(), queue.end(), 0);

  // This is used for constraint having unique variables in them (i.e. not
  // appearing anywhere else) to not call the presolve more than once for this
  // reason.
  std::unordered_set<std::pair<int, int>> var_constraint_pair_already_called;

  // Initialize the constraint <-> variable graph.
  context.constraint_to_vars.resize(initial_model.constraints_size());
  context.var_to_constraints.resize(initial_model.variables_size());
  for (int c = 0; c < initial_model.constraints_size(); ++c) {
    context.UpdateConstraintVariableUsage(c);
  }

  // Hack for the objective so that it is never considered to appear in only one
  // constraint.
  for (const CpObjectiveProto& obj : initial_model.objectives()) {
    context.var_to_constraints[PositiveRef(obj.objective_var())].insert(-1);
  }

  while (!queue.empty() && !context.is_unsat) {
    while (!queue.empty() && !context.is_unsat) {
      const int c = queue.front();
      in_queue[c] = false;
      queue.pop_front();

      const int old_num_constraint = context.working_model->constraints_size();
      ConstraintProto* ct = context.working_model->mutable_constraints(c);

      // Special generic presolve for reified constraint.
      bool changed = PresolveEnforcementLiteral(ct, &context);

      // Generic presolve to exploit variable/literal equivalence.
      changed |= ExploitEquivalenceRelations(ct, &context);

      // Because the functions below relies on proper usage stats, we need
      // to update it now.
      if (changed) {
        context.UpdateConstraintVariableUsage(c);
        changed = false;
      }

      // Call the presolve function for this constraint if any.
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
        case ConstraintProto::ConstraintCase::kLinear:
          changed |= PresolveLinear(ct, &context);
          if (ct->constraint_case() ==
              ConstraintProto::ConstraintCase::kLinear) {
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
      const std::unordered_set<int>& constraints =
          context.var_to_constraints[v];
      if (constraints.size() != 1) continue;
      const int c = *constraints.begin();
      if (c < 0) continue;
      if (ContainsKey(var_constraint_pair_already_called,
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
    for (const int v : modified_domains.PositionsSetAtLeastOnce()) {
      if (context.domains[v].IsFixed()) context.ExploitFixedDomain(v);
      for (const int c : context.var_to_constraints[v]) {
        if (c >= 0 && !in_queue[c]) {
          in_queue[c] = true;
          queue.push_back(c);
        }
      }
    }
    modified_domains.SparseClearAll();
  }

  if (context.is_unsat) {
    // Set presolved_model to the simplest UNSAT problem (empty clause).
    presolved_model->Clear();
    presolved_model->add_constraints()->mutable_bool_or();
    return;
  }

  // Remove all empty or affine constraints (they will be re-added later if
  // needed) in the presolved model. Note that we need to remap the interval
  // references.
  std::vector<int> interval_mapping(presolved_model->constraints_size());
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
      if (ContainsKey(context.affine_constraints, ct)) {
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
      if (context.domains[r.representative].IntersectWith(implied)) {
        LOG(WARNING) << "Domain of " << r.representative
                     << " was not fully propagated using the affine relation "
                     << "(representative =" << r.representative << ", coeff = "
                     << r.coeff << ", offset = " << r.offset << ")";
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
      if (context.domains[var].IsFixed()) continue;

      // There is not point having a variable appear twice, so we only keep
      // the first occurrence in the first strategy in which it occurs.
      if (ContainsKey(used_variables, var)) continue;
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
  for (int i = 0; i < context.domains.size(); ++i) {
    context.domains[i].CopyToIntegerVariableProto(
        presolved_model->mutable_variables(i));
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
  // Remap all the variable/literal references in the contraints.
  for (ConstraintProto& ct_ref : *proto->mutable_constraints()) {
    auto f = [&mapping](int* ref) {
      const int image = mapping[PositiveRef(*ref)];
      CHECK_GE(image, 0);
      *ref = *ref >= 0 ? image : NegatedRef(image);
    };
    ApplyToAllVariableIndices(f, &ct_ref);
    ApplyToAllLiteralIndices(f, &ct_ref);
  }

  // Remap the objectives.
  for (CpObjectiveProto& objective : *proto->mutable_objectives()) {
    const int ref = objective.objective_var();
    const int image = mapping[PositiveRef(ref)];
    CHECK_GE(image, 0);
    objective.set_objective_var(ref >= 0 ? image : NegatedRef(image));
  }

  // Remap the search decision heuristic.
  // Note that we delete any heuristic related to a removed variable.
  for (DecisionStrategyProto& strategy : *proto->mutable_search_strategy()) {
    DecisionStrategyProto copy = strategy;
    strategy.clear_variables();
    for (const int ref : copy.variables()) {
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        strategy.add_variables(ref >= 0 ? image : NegatedRef(image));
      }
    }
    strategy.clear_transformations();
    for (const auto& transform : copy.transformations()) {
      const int ref = transform.var();
      const int image = mapping[PositiveRef(ref)];
      if (image >= 0) {
        auto* new_transform = strategy.add_transformations();
        *new_transform = transform;
        new_transform->set_var(ref >= 0 ? image : NegatedRef(image));
      }
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
