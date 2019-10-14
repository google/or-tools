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
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace sat {

void PresolveContext::ClearStats() { stats_by_rule_name.clear(); }

int PresolveContext::NewIntVar(const Domain& domain) {
  IntegerVariableProto* const var = working_model->add_variables();
  FillDomainInProto(domain, var);
  InitializeNewDomains();
  return working_model->variables_size() - 1;
}

int PresolveContext::NewBoolVar() { return NewIntVar(Domain(0, 1)); }

int PresolveContext::GetOrCreateConstantVar(int64 cst) {
  if (!gtl::ContainsKey(constant_to_ref, cst)) {
    constant_to_ref[cst] = working_model->variables_size();
    IntegerVariableProto* const var_proto = working_model->add_variables();
    var_proto->add_domain(cst);
    var_proto->add_domain(cst);
    InitializeNewDomains();
  }
  return constant_to_ref[cst];
}

// a => b.
void PresolveContext::AddImplication(int a, int b) {
  ConstraintProto* const ct = working_model->add_constraints();
  ct->add_enforcement_literal(a);
  ct->mutable_bool_and()->add_literals(b);
}

// b => x in [lb, ub].
void PresolveContext::AddImplyInDomain(int b, int x, const Domain& domain) {
  ConstraintProto* const imply = working_model->add_constraints();

  // Doing it like this seems to use slightly less memory.
  // TODO(user): Find the best way to create such small proto.
  imply->mutable_enforcement_literal()->Resize(1, b);
  LinearConstraintProto* mutable_linear = imply->mutable_linear();
  mutable_linear->mutable_vars()->Resize(1, x);
  mutable_linear->mutable_coeffs()->Resize(1, 1);
  FillDomainInProto(domain, mutable_linear);
}

bool PresolveContext::DomainIsEmpty(int ref) const {
  return domains[PositiveRef(ref)].IsEmpty();
}

bool PresolveContext::IsFixed(int ref) const {
  CHECK(!DomainIsEmpty(ref));
  return domains[PositiveRef(ref)].Min() == domains[PositiveRef(ref)].Max();
}

bool PresolveContext::LiteralIsTrue(int lit) const {
  if (!IsFixed(lit)) return false;
  if (RefIsPositive(lit)) {
    return domains[lit].Min() == 1;
  } else {
    return domains[PositiveRef(lit)].Max() == 0;
  }
}

bool PresolveContext::LiteralIsFalse(int lit) const {
  if (!IsFixed(lit)) return false;
  if (RefIsPositive(lit)) {
    return domains[lit].Max() == 0;
  } else {
    return domains[PositiveRef(lit)].Min() == 1;
  }
}

int64 PresolveContext::MinOf(int ref) const {
  CHECK(!DomainIsEmpty(ref));
  return RefIsPositive(ref) ? domains[PositiveRef(ref)].Min()
                            : -domains[PositiveRef(ref)].Max();
}

int64 PresolveContext::MaxOf(int ref) const {
  CHECK(!DomainIsEmpty(ref));
  return RefIsPositive(ref) ? domains[PositiveRef(ref)].Max()
                            : -domains[PositiveRef(ref)].Min();
}

// TODO(user): In some case, we could still remove var when it has some variable
// in affine relation with it, but we need to be careful that none are used.
bool PresolveContext::VariableIsUniqueAndRemovable(int ref) const {
  return var_to_constraints[PositiveRef(ref)].size() == 1 &&
         affine_relations.ClassSize(PositiveRef(ref)) == 1 &&
         !keep_all_feasible_solutions;
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

ABSL_MUST_USE_RESULT bool PresolveContext::IntersectDomainWith(
    int ref, const Domain& domain, bool* domain_modified) {
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
  return true;
}

ABSL_MUST_USE_RESULT bool PresolveContext::SetLiteralToFalse(int lit) {
  const int var = PositiveRef(lit);
  const int64 value = RefIsPositive(lit) ? 0 : 1;
  return IntersectDomainWith(var, Domain(value));
}

ABSL_MUST_USE_RESULT bool PresolveContext::SetLiteralToTrue(int lit) {
  return SetLiteralToFalse(NegatedRef(lit));
}

void PresolveContext::UpdateRuleStats(const std::string& name) {
  stats_by_rule_name[name]++;
  num_presolve_operations++;
}

void PresolveContext::AddVariableUsage(int c) {
  const ConstraintProto& ct = working_model->constraints(c);

  constraint_to_vars[c] = UsedVariables(working_model->constraints(c));
  constraint_to_intervals[c] = UsedIntervals(ct);
  for (const int v : constraint_to_vars[c]) var_to_constraints[v].insert(c);
  for (const int i : constraint_to_intervals[c]) interval_usage[i]++;
}

void PresolveContext::UpdateConstraintVariableUsage(int c) {
  CHECK_EQ(constraint_to_vars.size(), working_model->constraints_size());

  // Remove old usage.
  for (const int v : constraint_to_vars[c]) var_to_constraints[v].erase(c);
  for (const int i : constraint_to_intervals[c]) interval_usage[i]--;

  AddVariableUsage(c);
}

void PresolveContext::UpdateNewConstraintsVariableUsage() {
  const int old_size = constraint_to_vars.size();
  const int new_size = working_model->constraints_size();
  CHECK_LE(old_size, new_size);
  constraint_to_vars.resize(new_size);
  constraint_to_intervals.resize(new_size);
  interval_usage.resize(new_size);
  for (int c = old_size; c < new_size; ++c) {
    AddVariableUsage(c);
  }
}

bool PresolveContext::ConstraintVariableUsageIsConsistent() {
  if (is_unsat) return false;
  if (constraint_to_vars.size() != working_model->constraints_size()) {
    LOG(INFO) << "Wrong constraint_to_vars size!";
    return false;
  }
  for (int c = 0; c < constraint_to_vars.size(); ++c) {
    if (constraint_to_vars[c] != UsedVariables(working_model->constraints(c))) {
      LOG(INFO) << "Wrong variables usage for constraint: \n"
                << ProtobufDebugString(working_model->constraints(c));
      return false;
    }
  }
  return true;
}

void PresolveContext::ExploitFixedDomain(int var) {
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

void PresolveContext::StoreAffineRelation(const ConstraintProto& ct, int ref_x,
                                          int ref_y, int64 coeff,
                                          int64 offset) {
  int x = PositiveRef(ref_x);
  int y = PositiveRef(ref_y);
  if (is_unsat) return;
  if (IsFixed(x) || IsFixed(y)) return;

  int64 c = RefIsPositive(ref_x) == RefIsPositive(ref_y) ? coeff : -coeff;
  int64 o = RefIsPositive(ref_x) ? offset : -offset;
  const int rep_x = affine_relations.Get(x).representative;
  const int rep_y = affine_relations.Get(y).representative;

  // If a Boolean variable (one with domain [0, 1]) appear in this affine
  // equivalence class, then we want its representative to be Boolean. Note
  // that this is always possible because a Boolean variable can never be
  // equal to a multiple of another if std::abs(coeff) is greater than 1 and
  // if it is not fixed to zero. This is important because it allows to simply
  // use the same representative for any referenced literals.
  bool allow_rep_x = MinOf(rep_x) == 0 && MaxOf(rep_x) == 1;
  bool allow_rep_y = MinOf(rep_y) == 0 && MaxOf(rep_y) == 1;
  if (!allow_rep_x && !allow_rep_y) {
    // If none are Boolean, we can use any representative.
    allow_rep_x = true;
    allow_rep_y = true;
  }

  // TODO(user): can we force the rep and remove GetAffineRelation()?
  bool added = affine_relations.TryAdd(x, y, c, o, allow_rep_x, allow_rep_y);
  if ((c == 1 || c == -1) && o == 0) {
    added |= var_equiv_relations.TryAdd(x, y, c, o, allow_rep_x, allow_rep_y);
  }
  if (added) {
    // The domain didn't change, but this notification allows to re-process
    // any constraint containing these variables.
    modified_domains.Set(x);
    modified_domains.Set(y);
    affine_constraints.insert(&ct);
  }
}

void PresolveContext::StoreBooleanEqualityRelation(int ref_a, int ref_b) {
  if (ref_a == ref_b) return;
  if (ref_a == NegatedRef(ref_b)) {
    is_unsat = true;
    return;
  }
  bool added = false;
  if (RefIsPositive(ref_a) == RefIsPositive(ref_b)) {
    added |=
        affine_relations.TryAdd(PositiveRef(ref_a), PositiveRef(ref_b), 1, 0);
    added |= var_equiv_relations.TryAdd(PositiveRef(ref_a), PositiveRef(ref_b),
                                        1, 0);
  } else {
    added |=
        affine_relations.TryAdd(PositiveRef(ref_a), PositiveRef(ref_b), -1, 1);
  }
  if (!added) return;

  modified_domains.Set(PositiveRef(ref_a));
  modified_domains.Set(PositiveRef(ref_b));

  // For now, we do need to add the relation ref_a == ref_b so we have a
  // proper variable usage count and propagation between ref_a and ref_b.
  //
  // TODO(user): This looks unclean. We should probably handle the affine
  // relation together without the need of keep all the constraints that
  // define them around.
  ConstraintProto* ct = working_model->add_constraints();
  auto* arg = ct->mutable_linear();
  arg->add_vars(PositiveRef(ref_a));
  arg->add_vars(PositiveRef(ref_b));
  if (RefIsPositive(ref_a) == RefIsPositive(ref_b)) {
    // a = b
    arg->add_coeffs(1);
    arg->add_coeffs(-1);
    arg->add_domain(0);
    arg->add_domain(0);
  } else {
    // a = 1 - b
    arg->add_coeffs(1);
    arg->add_coeffs(1);
    arg->add_domain(1);
    arg->add_domain(1);
  }
  affine_constraints.insert(ct);
  UpdateNewConstraintsVariableUsage();
}

// This makes sure that the affine relation only uses one of the
// representative from the var_equiv_relations.
AffineRelation::Relation PresolveContext::GetAffineRelation(int ref) {
  AffineRelation::Relation r = affine_relations.Get(PositiveRef(ref));
  AffineRelation::Relation o = var_equiv_relations.Get(r.representative);
  r.representative = o.representative;
  if (o.coeff == -1) r.coeff = -r.coeff;
  if (!RefIsPositive(ref)) {
    r.coeff *= -1;
    r.offset *= -1;
  }
  return r;
}

// Create the internal structure for any new variables in working_model.
void PresolveContext::InitializeNewDomains() {
  for (int i = domains.size(); i < working_model->variables_size(); ++i) {
    Domain domain = ReadDomainFromProto(working_model->variables(i));
    if (domain.IsEmpty()) {
      is_unsat = true;
      return;
    }
    domains.push_back(domain);
    if (IsFixed(i)) ExploitFixedDomain(i);
  }
  modified_domains.Resize(domains.size());
  var_to_constraints.resize(domains.size());
}

void PresolveContext::InsertVarValueEncoding(int literal, int ref,
                                             int64 value) {
  const int var = PositiveRef(ref);
  const int64 var_value = RefIsPositive(ref) ? value : -value;
  const std::pair<std::pair<int, int64>, int> key =
      std::make_pair(std::make_pair(var, var_value), literal);
  const auto& insert = encoding.insert(key);
  if (insert.second) {
    if (DomainOf(var).Size() == 2) {
      // Encode the other literal.
      const int64 var_min = MinOf(var);
      const int64 var_max = MaxOf(var);
      const int64 other_value = value == var_min ? var_max : var_min;
      const std::pair<int, int64> other_key{var, other_value};
      auto other_it = encoding.find(other_key);
      if (other_it != encoding.end()) {
        // Other value in the domain was already encoded.
        const int previous_other_literal = other_it->second;
        if (previous_other_literal != NegatedRef(literal)) {
          AddImplication(NegatedRef(literal), previous_other_literal);
          AddImplication(previous_other_literal, NegatedRef(literal));
        }
      } else {
        encoding[other_key] = NegatedRef(literal);
        // Add affine relation.
        if (var_min != 0 || var_max != 1) {
          ConstraintProto* const ct = working_model->add_constraints();
          LinearConstraintProto* const lin = ct->mutable_linear();
          lin->add_vars(var);
          lin->add_coeffs(1);
          lin->add_vars(literal);
          lin->add_coeffs(var_min - var_max);
          lin->add_domain(var_min);
          lin->add_domain(var_min);
          StoreAffineRelation(*ct, var, literal, var_max - var_min, var_min);
        }
      }
    } else {
      AddImplyInDomain(literal, var, Domain(var_value));
      AddImplyInDomain(NegatedRef(literal), var,
                       Domain(var_value).Complement());
    }

  } else {
    const int previous_literal = insert.first->second;
    if (literal != previous_literal) {
      AddImplication(literal, previous_literal);
      AddImplication(previous_literal, literal);
    }
  }
}

int PresolveContext::GetOrCreateVarValueEncoding(int ref, int64 value) {
  // TODO(user,user): use affine relation here.
  const int var = PositiveRef(ref);
  const int64 var_value = RefIsPositive(ref) ? value : -value;

  // Returns the false literal if the value is not in the domain.
  if (!domains[var].Contains(var_value)) {
    return GetOrCreateConstantVar(0);
  }

  // Returns the associated literal if already present.
  const std::pair<int, int64> key{var, var_value};
  auto it = encoding.find(key);
  if (it != encoding.end()) {
    return it->second;
  }

  // Special case for fixed domains.
  if (domains[var].Size() == 1) {
    const int true_literal = GetOrCreateConstantVar(1);
    encoding[key] = true_literal;
    return true_literal;
  }

  // Special case for domains of size 2.
  const int64 var_min = MinOf(var);
  const int64 var_max = MaxOf(var);
  if (domains[var].Size() == 2) {
    // Checks if the other value is already encoded.
    const int64 other_value = var_value == var_min ? var_max : var_min;
    const std::pair<int, int64> other_key{var, other_value};
    auto other_it = encoding.find(other_key);
    if (other_it != encoding.end()) {
      // Fill in other value.
      encoding[key] = NegatedRef(other_it->second);
      return NegatedRef(other_it->second);
    }

    if (var_min == 0 && var_max == 1) {
      encoding[{var, 1}] = var;
      encoding[{var, 0}] = NegatedRef(var);
      return value == 1 ? var : NegatedRef(var);
    } else {
      const int literal = NewBoolVar();
      InsertVarValueEncoding(literal, var, var_max);
      return var_value == var_max ? literal : NegatedRef(literal);
    }
  }

  const int literal = NewBoolVar();
  InsertVarValueEncoding(literal, var, var_value);
  return literal;
}

}  // namespace sat
}  // namespace operations_research
