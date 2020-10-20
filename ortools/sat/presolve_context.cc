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

#include "ortools/sat/presolve_context.h"

#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace sat {

int SavedLiteral::Get(PresolveContext *context) const {
  return context->GetLiteralRepresentative(ref_);
}

int SavedVariable::Get(PresolveContext *context) const {
  return context->GetVariableRepresentative(ref_);
}

void PresolveContext::ClearStats() { stats_by_rule_name.clear(); }

int PresolveContext::NewIntVar(const Domain &domain) {
  IntegerVariableProto *const var = working_model->add_variables();
  FillDomainInProto(domain, var);
  InitializeNewDomains();
  return working_model->variables_size() - 1;
}

int PresolveContext::NewBoolVar() { return NewIntVar(Domain(0, 1)); }

int PresolveContext::GetOrCreateConstantVar(int64 cst) {
  if (!gtl::ContainsKey(constant_to_ref_, cst)) {
    constant_to_ref_[cst] = SavedVariable(working_model->variables_size());
    IntegerVariableProto *const var_proto = working_model->add_variables();
    var_proto->add_domain(cst);
    var_proto->add_domain(cst);
    InitializeNewDomains();
  }
  return constant_to_ref_[cst].Get(this);
}

// a => b.
void PresolveContext::AddImplication(int a, int b) {
  ConstraintProto *const ct = working_model->add_constraints();
  ct->add_enforcement_literal(a);
  ct->mutable_bool_and()->add_literals(b);
}

// b => x in [lb, ub].
void PresolveContext::AddImplyInDomain(int b, int x, const Domain &domain) {
  ConstraintProto *const imply = working_model->add_constraints();

  // Doing it like this seems to use slightly less memory.
  // TODO(user): Find the best way to create such small proto.
  imply->mutable_enforcement_literal()->Resize(1, b);
  LinearConstraintProto *mutable_linear = imply->mutable_linear();
  mutable_linear->mutable_vars()->Resize(1, x);
  mutable_linear->mutable_coeffs()->Resize(1, 1);
  FillDomainInProto(domain, mutable_linear);
}

bool PresolveContext::DomainIsEmpty(int ref) const {
  return domains[PositiveRef(ref)].IsEmpty();
}

bool PresolveContext::IsFixed(int ref) const {
  DCHECK_LT(PositiveRef(ref), domains.size());
  DCHECK(!DomainIsEmpty(ref));
  return domains[PositiveRef(ref)].IsFixed();
}

bool PresolveContext::CanBeUsedAsLiteral(int ref) const {
  const int var = PositiveRef(ref);
  return domains[var].Min() >= 0 && domains[var].Max() <= 1;
}

bool PresolveContext::LiteralIsTrue(int lit) const {
  DCHECK(CanBeUsedAsLiteral(lit));
  if (RefIsPositive(lit)) {
    return domains[lit].Min() == 1;
  } else {
    return domains[PositiveRef(lit)].Max() == 0;
  }
}

bool PresolveContext::LiteralIsFalse(int lit) const {
  DCHECK(CanBeUsedAsLiteral(lit));
  if (RefIsPositive(lit)) {
    return domains[lit].Max() == 0;
  } else {
    return domains[PositiveRef(lit)].Min() == 1;
  }
}

int64 PresolveContext::MinOf(int ref) const {
  DCHECK(!DomainIsEmpty(ref));
  return RefIsPositive(ref) ? domains[PositiveRef(ref)].Min()
                            : -domains[PositiveRef(ref)].Max();
}

int64 PresolveContext::MaxOf(int ref) const {
  DCHECK(!DomainIsEmpty(ref));
  return RefIsPositive(ref) ? domains[PositiveRef(ref)].Max()
                            : -domains[PositiveRef(ref)].Min();
}

int64 PresolveContext::MinOf(const LinearExpressionProto &expr) const {
  int64 result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int64 coeff = expr.coeffs(i);
    if (coeff > 0) {
      result += coeff * MinOf(expr.vars(i));
    } else {
      result += coeff * MaxOf(expr.vars(i));
    }
  }
  return result;
}

int64 PresolveContext::MaxOf(const LinearExpressionProto &expr) const {
  int64 result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int64 coeff = expr.coeffs(i);
    if (coeff > 0) {
      result += coeff * MaxOf(expr.vars(i));
    } else {
      result += coeff * MinOf(expr.vars(i));
    }
  }
  return result;
}

// Important: To be sure a variable can be removed, we need it to not be a
// representative of both affine and equivalence relation.
bool
PresolveContext::VariableIsNotRepresentativeOfEquivalenceClass(int var) const {
  DCHECK(RefIsPositive(var));
  if (affine_relations_.ClassSize(var) > 1 &&
      affine_relations_.Get(var).representative == var) {
    return false;
  }
  if (var_equiv_relations_.ClassSize(var) > 1 &&
      var_equiv_relations_.Get(var).representative == var) {
    return false;
  }
  return true;
}

// Tricky: If this variable is equivalent to another one (but not the
// representative) and appear in just one constraint, then this constraint must
// be the affine defining one. And in this case the code using this function
// should do the proper stuff.
bool PresolveContext::VariableIsUniqueAndRemovable(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate())
    return false;
  const int var = PositiveRef(ref);
  return var_to_constraints_[var].size() == 1 &&
         VariableIsNotRepresentativeOfEquivalenceClass(var) &&
         !keep_all_feasible_solutions;
}

// Tricky: Same remark as for VariableIsUniqueAndRemovable().
bool PresolveContext::VariableWithCostIsUniqueAndRemovable(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate())
    return false;
  const int var = PositiveRef(ref);
  return !keep_all_feasible_solutions &&
         var_to_constraints_[var].contains(kObjectiveConstraint) &&
         var_to_constraints_[var].size() == 2 &&
         VariableIsNotRepresentativeOfEquivalenceClass(var);
}

// Here, even if the variable is equivalent to others, if its affine defining
// constraints where removed, then it is not needed anymore.
bool PresolveContext::VariableIsNotUsedAnymore(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate())
    return false;
  return var_to_constraints_[PositiveRef(ref)].empty();
}

void PresolveContext::MarkVariableAsRemoved(int ref) {
  removed_variables_.insert(PositiveRef(ref));
}

// Note(user): I added an indirection and a function for this to be able to
// display debug information when this return false. This should actually never
// return false in the cases where it is used.
bool PresolveContext::VariableWasRemoved(int ref) const {
  // It is okay to reuse removed fixed variable.
  if (IsFixed(ref))
    return false;
  if (!removed_variables_.contains(PositiveRef(ref)))
    return false;
  if (!var_to_constraints_[PositiveRef(ref)].empty()) {
    const AffineRelation::Relation r = GetAffineRelation(PositiveRef(ref));
    LOG(INFO) << "Variable " << PositiveRef(ref)
              << " was removed, yet it appears in some constraints!";
    LOG(INFO) << "affine relation = " << r.coeff << " * X" << r.representative
              << " + " << r.offset;
    for (const int c : var_to_constraints_[PositiveRef(ref)]) {
      LOG(INFO)
          << "constraint #" << c << " : "
          << (c >= 0 ? working_model->constraints(c).ShortDebugString() : "");
    }
  }
  return true;
}

bool PresolveContext::VariableIsOnlyUsedInEncoding(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate())
    return false;
  const int var = PositiveRef(ref);
  return var_to_num_linear1_[var] == var_to_constraints_[var].size();
}

Domain PresolveContext::DomainOf(int ref) const {
  Domain result;
  if (RefIsPositive(ref)) {
    result = domains[ref];
  } else {
    result = domains[PositiveRef(ref)].Negation();
  }
  return result;
}

bool PresolveContext::DomainContains(int ref, int64 value) const {
  if (!RefIsPositive(ref)) {
    return domains[PositiveRef(ref)].Contains(-value);
  }
  return domains[ref].Contains(value);
}

ABSL_MUST_USE_RESULT bool
PresolveContext::IntersectDomainWith(int ref, const Domain &domain,
                                     bool *domain_modified) {
  DCHECK(!DomainIsEmpty(ref));
  const int var = PositiveRef(ref);

  if (RefIsPositive(ref)) {
    if (domains[var].IsIncludedIn(domain)) {
      return true;
    }
    domains[var] = domains[var].IntersectionWith(domain);
  } else {
    const Domain temp = domain.Negation();
    if (domains[var].IsIncludedIn(temp)) {
      return true;
    }
    domains[var] = domains[var].IntersectionWith(temp);
  }

  if (domain_modified != nullptr) {
    *domain_modified = true;
  }
  modified_domains.Set(var);
  if (domains[var].IsEmpty()) {
    is_unsat = true;
    return false;
  }

  // Propagate the domain of the representative right away.
  // Note that the recursive call should only by one level deep.
  const AffineRelation::Relation r = GetAffineRelation(var);
  if (r.representative == var)
    return true;
  return IntersectDomainWith(r.representative,
                             DomainOf(var).AdditionWith(Domain(-r.offset))
                                 .InverseMultiplicationBy(r.coeff));
}

ABSL_MUST_USE_RESULT bool PresolveContext::SetLiteralToFalse(int lit) {
  const int var = PositiveRef(lit);
  const int64 value = RefIsPositive(lit) ? 0 : 1;
  return IntersectDomainWith(var, Domain(value));
}

ABSL_MUST_USE_RESULT bool PresolveContext::SetLiteralToTrue(int lit) {
  return SetLiteralToFalse(NegatedRef(lit));
}

void PresolveContext::UpdateRuleStats(const std::string &name) {
  if (enable_stats) {
    VLOG(1) << num_presolve_operations << " : " << name;
    stats_by_rule_name[name]++;
  }
  num_presolve_operations++;
}

void PresolveContext::UpdateLinear1Usage(const ConstraintProto &ct, int c) {
  const int old_var = constraint_to_linear1_var_[c];
  if (old_var >= 0) {
    var_to_num_linear1_[old_var]--;
  }
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear &&
      ct.linear().vars().size() == 1) {
    const int var = PositiveRef(ct.linear().vars(0));
    constraint_to_linear1_var_[c] = var;
    var_to_num_linear1_[var]++;
  }
}

void PresolveContext::AddVariableUsage(int c) {
  const ConstraintProto &ct = working_model->constraints(c);
  constraint_to_vars_[c] = UsedVariables(ct);
  constraint_to_intervals_[c] = UsedIntervals(ct);
  for (const int v : constraint_to_vars_[c]) {
    DCHECK(!VariableWasRemoved(v));
    var_to_constraints_[v].insert(c);
  }
  for (const int i : constraint_to_intervals_[c])
    interval_usage_[i]++;
  UpdateLinear1Usage(ct, c);
}

void PresolveContext::UpdateConstraintVariableUsage(int c) {
  if (is_unsat)
    return;
  DCHECK_EQ(constraint_to_vars_.size(), working_model->constraints_size());
  const ConstraintProto &ct = working_model->constraints(c);

  // We don't optimize the interval usage as this is not super frequent.
  for (const int i : constraint_to_intervals_[c])
    interval_usage_[i]--;
  constraint_to_intervals_[c] = UsedIntervals(ct);
  for (const int i : constraint_to_intervals_[c])
    interval_usage_[i]++;

  // For the variables, we avoid an erase() followed by an insert() for the
  // variables that didn't change.
  tmp_new_usage_ = UsedVariables(ct);
  const std::vector<int> &old_usage = constraint_to_vars_[c];
  const int old_size = old_usage.size();
  int i = 0;
  for (const int var : tmp_new_usage_) {
    DCHECK(!VariableWasRemoved(var));
    while (i < old_size && old_usage[i] < var) {
      var_to_constraints_[old_usage[i]].erase(c);
      ++i;
    }
    if (i < old_size && old_usage[i] == var) {
      ++i;
    } else {
      var_to_constraints_[var].insert(c);
    }
  }
  for (; i < old_size; ++i)
    var_to_constraints_[old_usage[i]].erase(c);
  constraint_to_vars_[c] = tmp_new_usage_;

  UpdateLinear1Usage(ct, c);
}

bool PresolveContext::ConstraintVariableGraphIsUpToDate() const {
  return constraint_to_vars_.size() == working_model->constraints_size();
}

void PresolveContext::UpdateNewConstraintsVariableUsage() {
  if (is_unsat)
    return;
  const int old_size = constraint_to_vars_.size();
  const int new_size = working_model->constraints_size();
  CHECK_LE(old_size, new_size);
  constraint_to_vars_.resize(new_size);
  constraint_to_linear1_var_.resize(new_size, -1);
  constraint_to_intervals_.resize(new_size);
  interval_usage_.resize(new_size);
  for (int c = old_size; c < new_size; ++c) {
    AddVariableUsage(c);
  }
}

// TODO(user): Also test var_to_constraints_ !!
bool PresolveContext::ConstraintVariableUsageIsConsistent() {
  if (is_unsat)
    return true; // We do not care in this case.
  if (constraint_to_vars_.size() != working_model->constraints_size()) {
    LOG(INFO) << "Wrong constraint_to_vars size!";
    return false;
  }
  for (int c = 0; c < constraint_to_vars_.size(); ++c) {
    if (constraint_to_vars_[c] !=
        UsedVariables(working_model->constraints(c))) {
      LOG(INFO) << "Wrong variables usage for constraint: \n"
                << ProtobufDebugString(working_model->constraints(c))
                << "old_size: " << constraint_to_vars_[c].size();
      return false;
    }
  }
  int num_in_objective = 0;
  for (int v = 0; v < var_to_constraints_.size(); ++v) {
    if (var_to_constraints_[v].contains(kObjectiveConstraint)) {
      ++num_in_objective;
      if (!objective_map.contains(v)) {
        LOG(INFO) << "Variable " << v
                  << " is marked as part of the objective but isn't.";
        return false;
      }
    }
  }
  if (num_in_objective != objective_map.size()) {
    LOG(INFO) << "Not all variables are marked as part of the objective";
    return false;
  }

  return true;
}

// If a Boolean variable (one with domain [0, 1]) appear in this affine
// equivalence class, then we want its representative to be Boolean. Note that
// this is always possible because a Boolean variable can never be equal to a
// multiple of another if std::abs(coeff) is greater than 1 and if it is not
// fixed to zero. This is important because it allows to simply use the same
// representative for any referenced literals.
//
// Note(user): When both domain contains [0,1] and later the wrong variable
// become usable as boolean, then we have a bug. Because of that, the code
// for GetLiteralRepresentative() is not as simple as it should be.
bool PresolveContext::AddRelation(int x, int y, int64 c, int64 o,
                                  AffineRelation *repo) {
  // When the coefficient is larger than one, then if later one variable becomes
  // Boolean, it must be the representative.
  if (std::abs(c) != 1)
    return repo->TryAdd(x, y, c, o);

  CHECK(!VariableWasRemoved(x));
  CHECK(!VariableWasRemoved(y));

  // To avoid integer overflow, we always want to use the representative with
  // the smallest domain magnitude. Otherwise we might express a variable in say
  // [0, 3] as ([x, x + 3] - x) for an arbitrary large x, and substituting
  // something like this in a linear expression could break our overflow
  // precondition.
  //
  // Note that if either rep_x or rep_y can be used as a literal, then it will
  // also be the variable with the smallest domain magnitude (1 or 0 if fixed).
  const int rep_x = repo->Get(x).representative;
  const int rep_y = repo->Get(y).representative;
  const int64 m_x = std::max(std::abs(MinOf(rep_x)), std::abs(MaxOf(rep_x)));
  const int64 m_y = std::max(std::abs(MinOf(rep_y)), std::abs(MaxOf(rep_y)));
  bool allow_rep_x = m_x < m_y;
  bool allow_rep_y = m_y < m_x;
  if (m_x == m_y) {
    // If both magnitude are the same, we prefer a positive domain.
    // This is important so we don't use [-1, 0] as a representative for [0, 1].
    allow_rep_x = MinOf(rep_x) >= MinOf(rep_y);
    allow_rep_y = MinOf(rep_y) >= MinOf(rep_x);
  }
  return repo->TryAdd(x, y, c, o, allow_rep_x, allow_rep_y);
}

void PresolveContext::ExploitFixedDomain(int var) {
  CHECK(RefIsPositive(var));
  CHECK(IsFixed(var));
  const int min = MinOf(var);
  if (gtl::ContainsKey(constant_to_ref_, min)) {
    const int rep = constant_to_ref_[min].Get(this);
    if (RefIsPositive(rep)) {
      if (rep != var) {
        AddRelation(var, rep, 1, 0, &affine_relations_);
        AddRelation(var, rep, 1, 0, &var_equiv_relations_);
      }
    } else {
      if (PositiveRef(rep) == var) {
        CHECK_EQ(min, 0);
      } else {
        AddRelation(var, PositiveRef(rep), -1, 0, &affine_relations_);
        AddRelation(var, PositiveRef(rep), -1, 0, &var_equiv_relations_);
      }
    }
  } else {
    constant_to_ref_[min] = SavedVariable(var);
  }
}

bool PresolveContext::PropagateAffineRelation(int ref) {
  const int var = PositiveRef(ref);
  const AffineRelation::Relation r = GetAffineRelation(var);
  if (r.representative == var)
    return true;

  // Propagate domains both ways.
  // var = coeff * rep + offset
  if (!IntersectDomainWith(r.representative,
                           DomainOf(var).AdditionWith(Domain(-r.offset))
                               .InverseMultiplicationBy(r.coeff))) {
    return false;
  }
  if (!IntersectDomainWith(var, DomainOf(r.representative).MultiplicationBy(
                                    r.coeff).AdditionWith(Domain(r.offset)))) {
    return false;
  }

  return true;
}

void PresolveContext::RemoveAllVariablesFromAffineRelationConstraint() {
  for (auto &ref_map : var_to_constraints_) {
    ref_map.erase(kAffineRelationConstraint);
  }
}

// We only call that for a non representative variable that is only used in
// the kAffineRelationConstraint. Such variable can be ignored and should never
// be seen again in the presolve.
void PresolveContext::RemoveVariableFromAffineRelation(int var) {
  const int rep = GetAffineRelation(var).representative;

  CHECK(RefIsPositive(var));
  CHECK_NE(var, rep);
  CHECK_EQ(var_to_constraints_[var].size(), 1);
  CHECK(var_to_constraints_[var].contains(kAffineRelationConstraint));
  CHECK(var_to_constraints_[rep].contains(kAffineRelationConstraint));

  // We shouldn't reuse this variable again!
  MarkVariableAsRemoved(var);

  var_to_constraints_[var].erase(kAffineRelationConstraint);
  affine_relations_.IgnoreFromClassSize(var);
  var_equiv_relations_.IgnoreFromClassSize(var);

  // If the representative is left alone, we can remove it from the special
  // affine relation constraint too.
  if (affine_relations_.ClassSize(rep) == 1 &&
      var_equiv_relations_.ClassSize(rep) == 1) {
    var_to_constraints_[rep].erase(kAffineRelationConstraint);
  }

  if (VLOG_IS_ON(2)) {
    const auto r = GetAffineRelation(var);
    LOG(INFO) << "Removing affine relation for " << var << " : "
              << DomainOf(var) << " =  " << r.coeff << " * "
              << DomainOf(r.representative) << " + " << r.offset
              << " ( rep : " << rep << ").";
  }
}

bool PresolveContext::StoreAffineRelation(int ref_x, int ref_y, int64 coeff,
                                          int64 offset) {
  CHECK_NE(coeff, 0);
  if (is_unsat)
    return false;

  // TODO(user): I am not 100% sure why, but sometimes the representative is
  // fixed but that is not propagated to ref_x or ref_y and this causes issues.
  if (!PropagateAffineRelation(ref_x))
    return true;
  if (!PropagateAffineRelation(ref_y))
    return true;

  if (IsFixed(ref_x)) {
    const int64 lhs = DomainOf(ref_x).Min() - offset;
    if (lhs % std::abs(coeff) != 0) {
      is_unsat = true;
      return true;
    }
    static_cast<void>(IntersectDomainWith(ref_y, Domain(lhs / coeff)));
    UpdateRuleStats("affine: fixed");
    return true;
  }

  if (IsFixed(ref_y)) {
    const int64 value_x = DomainOf(ref_y).Min() * coeff + offset;
    static_cast<void>(IntersectDomainWith(ref_x, Domain(value_x)));
    UpdateRuleStats("affine: fixed");
    return true;
  }

  // If both are already in the same class, we need to make sure the relations
  // are compatible.
  const AffineRelation::Relation rx = GetAffineRelation(ref_x);
  const AffineRelation::Relation ry = GetAffineRelation(ref_y);
  if (rx.representative == ry.representative) {
    // x = rx.coeff * rep + rx.offset;
    // y = ry.coeff * rep + ry.offset_y;
    // And x == coeff * ry.coeff * rep + (coeff * ry.offset + offset).
    //
    // So we get the relation a * rep == b with a and b defined here:
    const int64 a = coeff * ry.coeff - rx.coeff;
    const int64 b = coeff * ry.offset + offset - rx.offset;
    if (a == 0) {
      if (b != 0)
        is_unsat = true;
      return true;
    }
    if (b % a != 0) {
      is_unsat = true;
      return true;
    }
    UpdateRuleStats("affine: unique solution");
    const int64 unique_value = -b / a;
    if (!IntersectDomainWith(rx.representative, Domain(unique_value))) {
      return true;
    }
    if (!IntersectDomainWith(ref_x,
                             Domain(unique_value * rx.coeff + rx.offset))) {
      return true;
    }
    if (!IntersectDomainWith(ref_y,
                             Domain(unique_value * ry.coeff + ry.offset))) {
      return true;
    }
    return true;
  }

  const int x = PositiveRef(ref_x);
  const int y = PositiveRef(ref_y);
  const int64 c = RefIsPositive(ref_x) == RefIsPositive(ref_y) ? coeff : -coeff;
  const int64 o = RefIsPositive(ref_x) ? offset : -offset;

  // TODO(user): can we force the rep and remove GetAffineRelation()?
  bool added = AddRelation(x, y, c, o, &affine_relations_);
  if ((c == 1 || c == -1) && o == 0) {
    added |= AddRelation(x, y, c, o, &var_equiv_relations_);
  }
  if (added) {
    UpdateRuleStats("affine: new relation");

    // Lets propagate again the new relation. We might as well do it as early
    // as possible and not all call site do it.
    if (!PropagateAffineRelation(ref_x))
      return true;
    if (!PropagateAffineRelation(ref_y))
      return true;

    // These maps should only contains representative, so only need to remap
    // either x or y.
    const int rep = GetAffineRelation(x).representative;
    if (x != rep)
      encoding_remap_queue_.push_back(x);
    if (y != rep)
      encoding_remap_queue_.push_back(y);

    // The domain didn't change, but this notification allows to re-process any
    // constraint containing these variables. Note that we do not need to
    // retrigger a propagation of the constraint containing a variable whose
    // representative didn't change.
    if (x != rep)
      modified_domains.Set(x);
    if (y != rep)
      modified_domains.Set(y);

    var_to_constraints_[x].insert(kAffineRelationConstraint);
    var_to_constraints_[y].insert(kAffineRelationConstraint);
    return true;
  }

  UpdateRuleStats("affine: incompatible relation");
  if (VLOG_IS_ON(1)) {
    LOG(INFO) << "Cannot add relation " << DomainOf(ref_x) << " = " << coeff
              << " * " << DomainOf(ref_y) << " + " << offset
              << " because of incompatibilities with existing relation: ";
    for (const int ref : {
      ref_x, ref_y
    }) {
      const auto r = GetAffineRelation(ref);
      LOG(INFO) << DomainOf(ref) << " =  " << r.coeff << " * "
                << DomainOf(r.representative) << " + " << r.offset;
    }
  }

  return false;
}

void PresolveContext::StoreBooleanEqualityRelation(int ref_a, int ref_b) {
  if (is_unsat)
    return;

  CHECK(!VariableWasRemoved(ref_a));
  CHECK(!VariableWasRemoved(ref_b));
  CHECK(!DomainOf(ref_a).IsEmpty());
  CHECK(!DomainOf(ref_b).IsEmpty());
  CHECK(CanBeUsedAsLiteral(ref_a));
  CHECK(CanBeUsedAsLiteral(ref_b));

  if (ref_a == ref_b)
    return;
  if (ref_a == NegatedRef(ref_b)) {
    is_unsat = true;
    return;
  }
  const int var_a = PositiveRef(ref_a);
  const int var_b = PositiveRef(ref_b);
  if (RefIsPositive(ref_a) == RefIsPositive(ref_b)) {
    // a = b
    CHECK(StoreAffineRelation(var_a, var_b, /*coeff=*/ 1, /*offset=*/ 0));
  } else {
    // a = 1 - b
    CHECK(StoreAffineRelation(var_a, var_b, /*coeff=*/ -1, /*offset=*/ 1));
  }
}

bool PresolveContext::StoreAbsRelation(int target_ref, int ref) {
  const auto insert_status = abs_relations_.insert(
      std::make_pair(target_ref, SavedVariable(PositiveRef(ref))));
  if (!insert_status.second) {
    // Tricky: overwrite if the old value refer to a now unused variable.
    const int candidate = insert_status.first->second.Get(this);
    if (removed_variables_.contains(candidate)) {
      insert_status.first->second = SavedVariable(PositiveRef(ref));
      return true;
    }
    return false;
  }
  return true;
}

bool PresolveContext::GetAbsRelation(int target_ref, int *ref) {
  auto it = abs_relations_.find(target_ref);
  if (it == abs_relations_.end())
    return false;

  // Tricky: In some rare case the stored relation can refer to a deleted
  // variable, so we need to ignore it.
  //
  // TODO(user): Incorporate this as part of SavedVariable/SavedLiteral so we
  // make sure we never forget about this.
  const int candidate = it->second.Get(this);
  if (removed_variables_.contains(candidate)) {
    abs_relations_.erase(it);
    return false;
  }
  *ref = candidate;
  return true;
}

int PresolveContext::GetLiteralRepresentative(int ref) const {
  const AffineRelation::Relation r = GetAffineRelation(PositiveRef(ref));

  CHECK(CanBeUsedAsLiteral(ref));
  if (!CanBeUsedAsLiteral(r.representative)) {
    // Note(user): This can happen is some corner cases where the affine
    // relation where added before the variable became usable as Boolean. When
    // this is the case, the domain will be of the form [x, x + 1] and should be
    // later remapped to a Boolean variable.
    return ref;
  }

  // We made sure that the affine representative can always be used as a
  // literal. However, if some variable are fixed, we might not have only
  // (coeff=1 offset=0) or (coeff=-1 offset=1) and we might have something like
  // (coeff=8 offset=0) which is only valid for both variable at zero...
  //
  // What is sure is that depending on the value, only one mapping can be valid
  // because r.coeff can never be zero.
  const bool positive_possible = (r.offset == 0 || r.coeff + r.offset == 1);
  const bool negative_possible = (r.offset == 1 || r.coeff + r.offset == 0);
  DCHECK_NE(positive_possible, negative_possible);
  if (RefIsPositive(ref)) {
    return positive_possible ? r.representative : NegatedRef(r.representative);
  } else {
    return positive_possible ? NegatedRef(r.representative) : r.representative;
  }
}

int PresolveContext::GetVariableRepresentative(int ref) const {
  const AffineRelation::Relation r = var_equiv_relations_.Get(PositiveRef(ref));
  CHECK_EQ(std::abs(r.coeff), 1);
  CHECK_EQ(r.offset, 0);
  return RefIsPositive(ref) == (r.coeff == 1) ? r.representative
                                              : NegatedRef(r.representative);
}

// This makes sure that the affine relation only uses one of the
// representative from the var_equiv_relations_.
AffineRelation::Relation PresolveContext::GetAffineRelation(int ref) const {
  AffineRelation::Relation r = affine_relations_.Get(PositiveRef(ref));
  AffineRelation::Relation o = var_equiv_relations_.Get(r.representative);
  r.representative = o.representative;
  if (o.coeff == -1)
    r.coeff = -r.coeff;
  if (!RefIsPositive(ref)) {
    r.coeff *= -1;
    r.offset *= -1;
  }
  return r;
}

// Create the internal structure for any new variables in working_model.
void PresolveContext::InitializeNewDomains() {
  for (int i = domains.size(); i < working_model->variables_size(); ++i) {
    domains.emplace_back(ReadDomainFromProto(working_model->variables(i)));
    if (domains.back().IsEmpty()) {
      is_unsat = true;
      return;
    }
    if (IsFixed(i))
      ExploitFixedDomain(i);
  }
  modified_domains.Resize(domains.size());
  var_to_constraints_.resize(domains.size());
  var_to_num_linear1_.resize(domains.size());
  var_to_ub_only_constraints.resize(domains.size());
  var_to_lb_only_constraints.resize(domains.size());
}

bool PresolveContext::RemapEncodingMaps() {
  // TODO(user): for now, while the code works most of the time, it triggers
  // weird side effect that causes some issues in some LNS presolve...
  // We should continue the investigation before activating it.
  //
  // Note also that because all our encoding constraints are present in the
  // model, they will be remapped, and the new mapping re-added again. So while
  // the current code might not be efficient, it should eventually reach the
  // same effect.
  encoding_remap_queue_.clear();

  // Note that InsertVarValueEncodingInternal() will potentially add new entry
  // to the encoding_ map, but for a different variables. So this code relies on
  // the fact that the var_map shouldn't change content nor address of the
  // "var_map" below while we iterate on them.
  for (const int var : encoding_remap_queue_) {
    CHECK(RefIsPositive(var));
    const AffineRelation::Relation r = GetAffineRelation(var);
    if (r.representative == var)
      return true;
    int num_remapping = 0;

    // Encoding.
    {
      const absl::flat_hash_map<int64, SavedLiteral> &var_map = encoding_[var];
      for (const auto &entry : var_map) {
        const int lit = entry.second.Get(this);
        if (removed_variables_.contains(PositiveRef(lit)))
          continue;
        if ((entry.first - r.offset) % r.coeff != 0)
          continue;
        const int64 rep_value = (entry.first - r.offset) / r.coeff;
        ++num_remapping;
        InsertVarValueEncodingInternal(lit, r.representative, rep_value,
                                       /*add_constraints=*/ false);
        if (is_unsat)
          return false;
      }
      encoding_.erase(var);
    }

    // Eq half encoding.
    {
      const absl::flat_hash_map<int64, absl::flat_hash_set<int> > &var_map =
          eq_half_encoding_[var];
      for (const auto &entry : var_map) {
        if ((entry.first - r.offset) % r.coeff != 0)
          continue;
        const int64 rep_value = (entry.first - r.offset) / r.coeff;
        for (int literal : entry.second) {
          ++num_remapping;
          InsertHalfVarValueEncoding(GetLiteralRepresentative(literal),
                                     r.representative, rep_value,
                                     /*imply_eq=*/ true);
          if (is_unsat)
            return false;
        }
      }
      eq_half_encoding_.erase(var);
    }

    // Neq half encoding.
    {
      const absl::flat_hash_map<int64, absl::flat_hash_set<int> > &var_map =
          neq_half_encoding_[var];
      for (const auto &entry : var_map) {
        if ((entry.first - r.offset) % r.coeff != 0)
          continue;
        const int64 rep_value = (entry.first - r.offset) / r.coeff;
        for (int literal : entry.second) {
          ++num_remapping;
          InsertHalfVarValueEncoding(GetLiteralRepresentative(literal),
                                     r.representative, rep_value,
                                     /*imply_eq=*/ false);
          if (is_unsat)
            return false;
        }
      }
      neq_half_encoding_.erase(var);
    }

    if (num_remapping > 0) {
      VLOG(1) << "Remapped " << num_remapping << " encodings due to " << var
              << " -> " << r.representative << ".";
    }
  }
  encoding_remap_queue_.clear();
  return !is_unsat;
}

void PresolveContext::CanonicalizeDomainOfSizeTwo(int var) {
  CHECK(RefIsPositive(var));
  CHECK_EQ(DomainOf(var).Size(), 2);
  const int64 var_min = MinOf(var);
  const int64 var_max = MaxOf(var);

  if (is_unsat)
    return;

  absl::flat_hash_map<int64, SavedLiteral> &var_map = encoding_[var];

  // Find encoding for min if present.
  auto min_it = var_map.find(var_min);
  if (min_it != var_map.end()) {
    const int old_var = PositiveRef(min_it->second.Get(this));
    if (removed_variables_.contains(old_var)) {
      var_map.erase(min_it);
      min_it = var_map.end();
    }
  }

  // Find encoding for max if present.
  auto max_it = var_map.find(var_max);
  if (max_it != var_map.end()) {
    const int old_var = PositiveRef(max_it->second.Get(this));
    if (removed_variables_.contains(old_var)) {
      var_map.erase(max_it);
      max_it = var_map.end();
    }
  }

  // Insert missing encoding.
  int min_literal;
  int max_literal;
  if (min_it != var_map.end() && max_it != var_map.end()) {
    min_literal = min_it->second.Get(this);
    max_literal = max_it->second.Get(this);
    if (min_literal != NegatedRef(max_literal)) {
      UpdateRuleStats("variables with 2 values: merge encoding literals");
      StoreBooleanEqualityRelation(min_literal, NegatedRef(max_literal));
      if (is_unsat)
        return;
    }
    min_literal = GetLiteralRepresentative(min_literal);
    max_literal = GetLiteralRepresentative(max_literal);
    if (!IsFixed(min_literal))
      CHECK_EQ(min_literal, NegatedRef(max_literal));
  } else if (min_it != var_map.end() && max_it == var_map.end()) {
    UpdateRuleStats("variables with 2 values: register other encoding");
    min_literal = min_it->second.Get(this);
    max_literal = NegatedRef(min_literal);
    var_map[var_max] = SavedLiteral(max_literal);
  } else if (min_it == var_map.end() && max_it != var_map.end()) {
    UpdateRuleStats("variables with 2 values: register other encoding");
    max_literal = max_it->second.Get(this);
    min_literal = NegatedRef(max_literal);
    var_map[var_min] = SavedLiteral(min_literal);
  } else {
    UpdateRuleStats("variables with 2 values: create encoding literal");
    max_literal = NewBoolVar();
    min_literal = NegatedRef(max_literal);
    var_map[var_min] = SavedLiteral(min_literal);
    var_map[var_max] = SavedLiteral(max_literal);
  }

  if (IsFixed(min_literal) || IsFixed(max_literal)) {
    CHECK(IsFixed(min_literal));
    CHECK(IsFixed(max_literal));
    UpdateRuleStats("variables with 2 values: fixed encoding");
    if (LiteralIsTrue(min_literal)) {
      return static_cast<void>(IntersectDomainWith(var, Domain(var_min)));
    } else {
      return static_cast<void>(IntersectDomainWith(var, Domain(var_max)));
    }
  }

  // Add affine relation.
  if (GetAffineRelation(var).representative != PositiveRef(min_literal)) {
    UpdateRuleStats("variables with 2 values: new affine relation");
    if (RefIsPositive(max_literal)) {
      CHECK(StoreAffineRelation(var, PositiveRef(max_literal),
                                var_max - var_min, var_min));
    } else {
      CHECK(StoreAffineRelation(var, PositiveRef(max_literal),
                                var_min - var_max, var_max));
    }
  }
}

void PresolveContext::InsertVarValueEncodingInternal(int literal, int var,
                                                     int64 value,
                                                     bool add_constraints) {
  CHECK(!VariableWasRemoved(literal));
  CHECK(!VariableWasRemoved(var));
  absl::flat_hash_map<int64, SavedLiteral> &var_map = encoding_[var];

  // Ticky and rare: I have only observed this on the LNS of
  // radiation_m18_12_05_sat.fzn. The value was encoded, but maybe we never
  // used the involved variables / constraints, so it was removed (with the
  // encoding constraints) from the model already! We have to be careful.
  const auto it = var_map.find(value);
  if (it != var_map.end()) {
    const int old_var = PositiveRef(it->second.Get(this));
    if (removed_variables_.contains(old_var)) {
      var_map.erase(it);
    }
  }

  const auto insert =
      var_map.insert(std::make_pair(value, SavedLiteral(literal)));

  // If an encoding already exist, make the two Boolean equals.
  if (!insert.second) {
    UpdateRuleStats("variables: merge equivalent var value encoding literals");
    const int previous_literal = insert.first->second.Get(this);
    CHECK(!VariableWasRemoved(previous_literal));
    if (literal != previous_literal) {
      StoreBooleanEqualityRelation(literal, previous_literal);
    }
    return;
  }

  if (DomainOf(var).Size() == 2) {
    CanonicalizeDomainOfSizeTwo(var);
  } else {
    VLOG(2) << "Insert lit(" << literal << ") <=> var(" << var
            << ") == " << value;
    // eq_half_encoding_[var][value].insert(literal);
    // neq_half_encoding_[var][value].insert(NegatedRef(literal));
    if (add_constraints) {
      UpdateRuleStats("variables: add encoding constraint");
      AddImplyInDomain(literal, var, Domain(value));
      AddImplyInDomain(NegatedRef(literal), var, Domain(value).Complement());
    }
  }
}

bool PresolveContext::InsertHalfVarValueEncoding(int literal, int var,
                                                 int64 value, bool imply_eq) {
  if (is_unsat)
    return false;
  CHECK(RefIsPositive(var));

  // Creates the linking sets on demand.
  // Insert the enforcement literal in the half encoding map.
  auto &direct_set =
      imply_eq ? eq_half_encoding_[var][value] : neq_half_encoding_[var][value];
  if (!direct_set.insert(literal).second)
    return false; // Already there.

  VLOG(2) << "Collect lit(" << literal << ") implies var(" << var
          << (imply_eq ? ") == " : ") != ") << value;
  UpdateRuleStats("variables: detect half reified value encoding");

  // Note(user): We don't expect a lot of literals in these sets, so doing
  // a scan should be okay.
  auto &other_set =
      imply_eq ? neq_half_encoding_[var][value] : eq_half_encoding_[var][value];
  for (const int other : other_set) {
    if (GetLiteralRepresentative(other) != NegatedRef(literal))
      continue;

    UpdateRuleStats("variables: detect fully reified value encoding");
    const int imply_eq_literal = imply_eq ? literal : NegatedRef(literal);
    InsertVarValueEncodingInternal(imply_eq_literal, var, value,
                                   /*add_constraints=*/ false);
    break;
  }

  return true;
}

bool PresolveContext::CanonicalizeEncoding(int *ref, int64 *value) {
  const AffineRelation::Relation r = GetAffineRelation(*ref);
  if ((*value - r.offset) % r.coeff != 0)
    return false;
  *ref = r.representative;
  *value = (*value - r.offset) / r.coeff;
  return true;
}

void PresolveContext::InsertVarValueEncoding(int literal, int ref,
                                             int64 value) {
  if (!RemapEncodingMaps())
    return;
  if (!CanonicalizeEncoding(&ref, &value))
    return;
  literal = GetLiteralRepresentative(literal);
  InsertVarValueEncodingInternal(literal, ref, value,
                                 /*add_constraints=*/ true);
}

bool PresolveContext::StoreLiteralImpliesVarEqValue(int literal, int var,
                                                    int64 value) {
  if (!RemapEncodingMaps())
    return false;
  if (!CanonicalizeEncoding(&var, &value))
    return false;
  literal = GetLiteralRepresentative(literal);
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/ true);
}

bool PresolveContext::StoreLiteralImpliesVarNEqValue(int literal, int var,
                                                     int64 value) {
  if (!RemapEncodingMaps())
    return false;
  if (!CanonicalizeEncoding(&var, &value))
    return false;
  literal = GetLiteralRepresentative(literal);
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/ false);
}

bool PresolveContext::HasVarValueEncoding(int ref, int64 value, int *literal) {
  if (!RemapEncodingMaps())
    return false;
  if (!CanonicalizeEncoding(&ref, &value))
    return false;
  const absl::flat_hash_map<int64, SavedLiteral> &var_map = encoding_[ref];
  const auto it = var_map.find(value);
  if (it != var_map.end()) {
    if (literal != nullptr) {
      *literal = it->second.Get(this);
    }
    return true;
  }
  return false;
}

int PresolveContext::GetOrCreateVarValueEncoding(int ref, int64 value) {
  if (!RemapEncodingMaps())
    return GetOrCreateConstantVar(0);
  if (!CanonicalizeEncoding(&ref, &value))
    return GetOrCreateConstantVar(0);

  // Positive after CanonicalizeEncoding().
  const int var = ref;

  // Returns the false literal if the value is not in the domain.
  if (!domains[var].Contains(value)) {
    return GetOrCreateConstantVar(0);
  }

  // Returns the associated literal if already present.
  absl::flat_hash_map<int64, SavedLiteral> &var_map = encoding_[var];
  auto it = var_map.find(value);
  if (it != var_map.end()) {
    return it->second.Get(this);
  }

  // Special case for fixed domains.
  if (domains[var].Size() == 1) {
    const int true_literal = GetOrCreateConstantVar(1);
    var_map[value] = SavedLiteral(true_literal);
    return true_literal;
  }

  // Special case for domains of size 2.
  const int64 var_min = MinOf(var);
  const int64 var_max = MaxOf(var);
  if (domains[var].Size() == 2) {
    // Checks if the other value is already encoded.
    const int64 other_value = value == var_min ? var_max : var_min;
    auto other_it = var_map.find(other_value);
    if (other_it != var_map.end()) {
      // Update the encoding map. The domain could have been reduced to size
      // two after the creation of the first literal.
      const int literal = NegatedRef(other_it->second.Get(this));
      var_map[value] = SavedLiteral(literal);
      return literal;
    }

    if (var_min == 0 && var_max == 1) {
      const int representative = GetLiteralRepresentative(var);
      var_map[1] = SavedLiteral(representative);
      var_map[0] = SavedLiteral(NegatedRef(representative));
      return value == 1 ? representative : NegatedRef(representative);
    } else {
      const int literal = NewBoolVar();
      InsertVarValueEncoding(literal, var, var_max);
      const int representative = GetLiteralRepresentative(literal);
      return value == var_max ? representative : NegatedRef(representative);
    }
  }

  const int literal = NewBoolVar();
  InsertVarValueEncoding(literal, var, value);
  return GetLiteralRepresentative(literal);
}

void PresolveContext::ReadObjectiveFromProto() {
  const CpObjectiveProto &obj = working_model->objective();

  objective_offset = obj.offset();
  objective_scaling_factor = obj.scaling_factor();
  if (objective_scaling_factor == 0.0) {
    objective_scaling_factor = 1.0;
  }
  if (!obj.domain().empty()) {
    // We might relax this in CanonicalizeObjective() when we will compute
    // the possible objective domain from the domains of the variables.
    objective_domain_is_constraining = true;
    objective_domain = ReadDomainFromProto(obj);
  } else {
    objective_domain_is_constraining = false;
    objective_domain = Domain::AllValues();
  }

  objective_map.clear();
  for (int i = 0; i < obj.vars_size(); ++i) {
    const int ref = obj.vars(i);
    int64 coeff = obj.coeffs(i);
    if (!RefIsPositive(ref))
      coeff = -coeff;
    int var = PositiveRef(ref);

    objective_map[var] += coeff;
    if (objective_map[var] == 0) {
      objective_map.erase(var);
      var_to_constraints_[var].erase(kObjectiveConstraint);
    } else {
      var_to_constraints_[var].insert(kObjectiveConstraint);
    }
  }
}

bool PresolveContext::CanonicalizeObjective() {
  int64 offset_change = 0;

  // We replace each entry by its affine representative.
  // Note that the non-deterministic loop is fine, but because we iterate
  // one the map while modifying it, it is safer to do a copy rather than to
  // try to handle that in one pass.
  tmp_entries.clear();
  for (const auto &entry : objective_map) {
    tmp_entries.push_back(entry);
  }

  // TODO(user): This is a bit duplicated with the presolve linear code.
  // We also do not propagate back any domain restriction from the objective to
  // the variables if any.
  for (const auto &entry : tmp_entries) {
    const int var = entry.first;
    const auto it = objective_map.find(var);
    if (it == objective_map.end())
      continue;
    const int64 coeff = it->second;

    // If a variable only appear in objective, we can fix it!
    // Note that we don't care if it was in affine relation, because if none
    // of the relations are left, then we can still fix it.
    if (!keep_all_feasible_solutions && !objective_domain_is_constraining &&
        ConstraintVariableGraphIsUpToDate() &&
        var_to_constraints_[var].size() == 1 &&
        var_to_constraints_[var].contains(kObjectiveConstraint)) {
      UpdateRuleStats("objective: variable not used elsewhere");
      if (coeff > 0) {
        if (!IntersectDomainWith(var, Domain(MinOf(var)))) {
          return false;
        }
      } else {
        if (!IntersectDomainWith(var, Domain(MaxOf(var)))) {
          return false;
        }
      }
    }

    if (IsFixed(var)) {
      offset_change += coeff * MinOf(var);
      var_to_constraints_[var].erase(kObjectiveConstraint);
      objective_map.erase(var);
      continue;
    }

    const AffineRelation::Relation r = GetAffineRelation(var);
    if (r.representative == var)
      continue;

    objective_map.erase(var);
    var_to_constraints_[var].erase(kObjectiveConstraint);

    // Do the substitution.
    offset_change += coeff * r.offset;
    const int64 new_coeff = objective_map[r.representative] += coeff * r.coeff;

    // Process new term.
    if (new_coeff == 0) {
      objective_map.erase(r.representative);
      var_to_constraints_[r.representative].erase(kObjectiveConstraint);
    } else {
      var_to_constraints_[r.representative].insert(kObjectiveConstraint);
      if (IsFixed(r.representative)) {
        offset_change += new_coeff * MinOf(r.representative);
        var_to_constraints_[r.representative].erase(kObjectiveConstraint);
        objective_map.erase(r.representative);
      }
    }
  }

  Domain implied_domain(0);
  int64 gcd(0);

  // We need to sort the entries to be deterministic.
  tmp_entries.clear();
  for (const auto &entry : objective_map) {
    tmp_entries.push_back(entry);
  }
  std::sort(tmp_entries.begin(), tmp_entries.end());
  for (const auto &entry : tmp_entries) {
    const int var = entry.first;
    const int64 coeff = entry.second;
    gcd = MathUtil::GCD64(gcd, std::abs(coeff));
    implied_domain =
        implied_domain.AdditionWith(DomainOf(var).MultiplicationBy(coeff))
            .RelaxIfTooComplex();
  }

  // This is the new domain.
  // Note that the domain never include the offset.
  objective_domain = objective_domain.AdditionWith(Domain(-offset_change))
      .IntersectionWith(implied_domain);
  objective_domain =
      objective_domain.SimplifyUsingImpliedDomain(implied_domain);

  // Updat the offset.
  objective_offset += offset_change;

  // Maybe divide by GCD.
  if (gcd > 1) {
    for (auto &entry : objective_map) {
      entry.second /= gcd;
    }
    objective_domain = objective_domain.InverseMultiplicationBy(gcd);
    objective_offset /= static_cast<double>(gcd);
    objective_scaling_factor *= static_cast<double>(gcd);
  }

  if (objective_domain.IsEmpty())
    return false;

  // Detect if the objective domain do not limit the "optimal" objective value.
  // If this is true, then we can apply any reduction that reduce the objective
  // value without any issues.
  objective_domain_is_constraining = !implied_domain.IntersectionWith(Domain(
      kint64min, objective_domain.Max())).IsIncludedIn(objective_domain);
  return true;
}

void PresolveContext::SubstituteVariableInObjective(
    int var_in_equality, int64 coeff_in_equality,
    const ConstraintProto &equality, std::vector<int> *new_vars_in_objective) {
  CHECK(equality.enforcement_literal().empty());
  CHECK(RefIsPositive(var_in_equality));

  if (new_vars_in_objective != nullptr)
    new_vars_in_objective->clear();

  // We can only "easily" substitute if the objective coefficient is a multiple
  // of the one in the constraint.
  const int64 coeff_in_objective =
      gtl::FindOrDie(objective_map, var_in_equality);
  CHECK_NE(coeff_in_equality, 0);
  CHECK_EQ(coeff_in_objective % coeff_in_equality, 0);
  const int64 multiplier = coeff_in_objective / coeff_in_equality;

  for (int i = 0; i < equality.linear().vars().size(); ++i) {
    int var = equality.linear().vars(i);
    int64 coeff = equality.linear().coeffs(i);
    if (!RefIsPositive(var)) {
      var = NegatedRef(var);
      coeff = -coeff;
    }
    if (var == var_in_equality)
      continue;

    int64 &map_ref = objective_map[var];
    if (map_ref == 0 && new_vars_in_objective != nullptr) {
      new_vars_in_objective->push_back(var);
    }
    map_ref -= coeff * multiplier;

    if (map_ref == 0) {
      objective_map.erase(var);
      var_to_constraints_[var].erase(kObjectiveConstraint);
    } else {
      var_to_constraints_[var].insert(kObjectiveConstraint);
    }
  }

  objective_map.erase(var_in_equality);
  var_to_constraints_[var_in_equality].erase(kObjectiveConstraint);

  // Deal with the offset.
  Domain offset = ReadDomainFromProto(equality.linear());
  DCHECK_EQ(offset.Min(), offset.Max());
  bool exact = true;
  offset = offset.MultiplicationBy(multiplier, &exact);
  CHECK(exact);

  // Tricky: The objective domain is without the offset, so we need to shift it.
  objective_offset += static_cast<double>(offset.Min());
  objective_domain = objective_domain.AdditionWith(Domain(-offset.Min()));

  // Because we can assume that the constraint we used was constraining
  // (otherwise it would have been removed), the objective domain should be now
  // constraining.
  objective_domain_is_constraining = true;
}

void PresolveContext::WriteObjectiveToProto() {
  if (objective_domain.IsEmpty()) {
    return (void) NotifyThatModelIsUnsat();
  }

  // We need to sort the entries to be deterministic.
  std::vector<std::pair<int, int64> > entries;
  for (const auto &entry : objective_map) {
    entries.push_back(entry);
  }
  std::sort(entries.begin(), entries.end());

  CpObjectiveProto *mutable_obj = working_model->mutable_objective();
  mutable_obj->set_offset(objective_offset);
  mutable_obj->set_scaling_factor(objective_scaling_factor);
  FillDomainInProto(objective_domain, mutable_obj);
  mutable_obj->clear_vars();
  mutable_obj->clear_coeffs();
  for (const auto &entry : entries) {
    mutable_obj->add_vars(entry.first);
    mutable_obj->add_coeffs(entry.second);
  }
}

} // namespace sat
} // namespace operations_research
