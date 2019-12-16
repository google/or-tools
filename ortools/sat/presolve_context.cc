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
  DCHECK(!DomainIsEmpty(ref));
  return domains[PositiveRef(ref)].IsFixed();
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

bool PresolveContext::VariableIsNotRepresentativeOfEquivalenceClass(
    int ref) const {
  if (affine_relations.ClassSize(PositiveRef(ref)) == 1) return true;
  return PositiveRef(GetAffineRelation(ref).representative) != PositiveRef(ref);
}

// TODO(user): In some case, we could still remove var when it has some variable
// in affine relation with it, but we need to be careful that none are used.
bool PresolveContext::VariableIsUniqueAndRemovable(int ref) const {
  return var_to_constraints[PositiveRef(ref)].size() == 1 &&
         VariableIsNotRepresentativeOfEquivalenceClass(ref) &&
         !keep_all_feasible_solutions;
}

bool PresolveContext::VariableWithCostIsUniqueAndRemovable(int ref) const {
  const int var = PositiveRef(ref);
  return !keep_all_feasible_solutions && var_to_constraints[var].contains(-1) &&
         var_to_constraints[var].size() == 2 &&
         VariableIsNotRepresentativeOfEquivalenceClass(ref);
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
  //
  // TODO(user): we could avoid erase() and then insert() for the variable
  // that are still in this constraint.
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
    // The domain didn't change, but this notification allows to re-process any
    // constraint containing these variables. Note that we do not need to
    // retrigger a propagation of the constraint containing a variable whose
    // representative didn't change.
    const int new_rep = affine_relations.Get(rep_x).representative;
    if (new_rep != rep_x) modified_domains.Set(x);
    if (new_rep != rep_y) modified_domains.Set(y);
    affine_constraints.insert(&ct);
  }
}

void PresolveContext::StoreBooleanEqualityRelation(int ref_a, int ref_b) {
  if (ref_a == ref_b) return;
  if (ref_a == NegatedRef(ref_b)) {
    is_unsat = true;
    return;
  }

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
    StoreAffineRelation(*ct, PositiveRef(ref_a), PositiveRef(ref_b), 1,
                        /*offset=*/0);
  } else {
    // a = 1 - b
    arg->add_coeffs(1);
    arg->add_coeffs(1);
    arg->add_domain(1);
    arg->add_domain(1);
    StoreAffineRelation(*ct, PositiveRef(ref_a), PositiveRef(ref_b), -1,
                        /*offset=*/1);
  }
  UpdateNewConstraintsVariableUsage();
}

// This makes sure that the affine relation only uses one of the
// representative from the var_equiv_relations.
AffineRelation::Relation PresolveContext::GetAffineRelation(int ref) const {
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
  domains.reserve(working_model->variables_size());
  for (int i = domains.size(); i < working_model->variables_size(); ++i) {
    domains.emplace_back(ReadDomainFromProto(working_model->variables(i)));
    if (domains.back().IsEmpty()) {
      is_unsat = true;
      return;
    }
    if (IsFixed(i)) ExploitFixedDomain(i);
  }
  modified_domains.Resize(domains.size());
  var_to_constraints.resize(domains.size());
  var_to_ub_only_constraints.resize(domains.size());
  var_to_lb_only_constraints.resize(domains.size());
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
          StoreBooleanEqualityRelation(literal,
                                       NegatedRef(previous_other_literal));
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
      VLOG(2) << "Insert lit(" << literal << ") <=> var(" << var
              << ") == " << value;
      const std::pair<int, int64> key{var, var_value};
      eq_half_encoding[key].insert(literal);
      AddImplyInDomain(literal, var, Domain(var_value));
      neq_half_encoding[key].insert(NegatedRef(literal));
      AddImplyInDomain(NegatedRef(literal), var,
                       Domain(var_value).Complement());
    }
  } else {
    const int previous_literal = insert.first->second;
    if (literal != previous_literal) {
      StoreBooleanEqualityRelation(literal, previous_literal);
    }
  }
}

bool PresolveContext::InsertHalfVarValueEncoding(int literal, int var,
                                                 int64 value, bool imply_eq) {
  CHECK(RefIsPositive(var));

  // Creates the linking maps on demand.
  const std::pair<int, int64> key{var, value};
  auto& direct_map = imply_eq ? eq_half_encoding[key] : neq_half_encoding[key];
  auto& other_map = imply_eq ? neq_half_encoding[key] : eq_half_encoding[key];

  // Insert the reference literal in the half encoding map.
  const auto& new_info = direct_map.insert(literal);
  if (new_info.second) {
    VLOG(2) << "Collect lit(" << literal << ") implies var(" << var
            << (imply_eq ? ") == " : ") != ") << value;
    UpdateRuleStats("variables: detect half reified value encoding");

    if (other_map.contains(NegatedRef(literal))) {
      const int imply_eq_literal = imply_eq ? literal : NegatedRef(literal);

      const auto insert_encoding_status =
          encoding.insert(std::make_pair(key, imply_eq_literal));
      if (insert_encoding_status.second) {
        VLOG(2) << "Detect and store lit(" << imply_eq_literal << ") <=> var("
                << var << ") == " << value;
        UpdateRuleStats("variables: detect fully reified value encoding");
        encoding[key] = imply_eq_literal;
      } else if (imply_eq_literal != insert_encoding_status.first->second) {
        const int previous_imply_eq_literal =
            insert_encoding_status.first->second;
        VLOG(2) << "Detect duplicate encoding lit(" << imply_eq_literal
                << ") == lit(" << previous_imply_eq_literal << ") <=> var("
                << var << ") == " << value;
        StoreBooleanEqualityRelation(imply_eq_literal,
                                     previous_imply_eq_literal);

        // Update reference literal.
        const AffineRelation::Relation r =
            GetAffineRelation(previous_imply_eq_literal);
        const int new_ref_lit =
            r.coeff > 0 ? r.representative : NegatedRef(r.representative);
        if (new_ref_lit != previous_imply_eq_literal) {
          VLOG(2) << "Updating reference encoding literal to " << new_ref_lit;
          encoding[key] = new_ref_lit;
        }

        UpdateRuleStats(
            "variables: merge equivalent var value encoding literals");
      }
    }
  }

  return new_info.second;
}

bool PresolveContext::StoreLiteralImpliesVarEqValue(int literal, int var,
                                                    int64 value) {
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/true);
}

bool PresolveContext::StoreLiteralImpliesVarNEqValue(int literal, int var,
                                                     int64 value) {
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/false);
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
    VLOG(2) << "GetOrCreate var(" << var << ") {" << var_min << ", " << var_max
            << "}";
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

void PresolveContext::ReadObjectiveFromProto() {
  const CpObjectiveProto& obj = working_model->objective();

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
    if (!RefIsPositive(ref)) coeff = -coeff;
    int var = PositiveRef(ref);

    objective_map[var] += coeff;
    if (objective_map[var] == 0) {
      objective_map.erase(var);
      var_to_constraints[var].erase(-1);
    } else {
      var_to_constraints[var].insert(-1);
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
  for (const auto& entry : objective_map) {
    tmp_entries.push_back(entry);
  }

  // TODO(user): This is a bit duplicated with the presolve linear code.
  // We also do not propagate back any domain restriction from the objective to
  // the variables if any.
  for (const auto& entry : tmp_entries) {
    const int var = entry.first;
    const auto it = objective_map.find(var);
    if (it == objective_map.end()) continue;
    const int64 coeff = it->second;

    // If a variable only appear in objective, we can fix it!
    if (!keep_all_feasible_solutions && !objective_domain_is_constraining &&
        VariableIsNotRepresentativeOfEquivalenceClass(var) &&
        var_to_constraints[var].size() == 1 &&
        var_to_constraints[var].contains(-1)) {
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
      var_to_constraints[var].erase(-1);
      objective_map.erase(var);
      continue;
    }

    const AffineRelation::Relation r = GetAffineRelation(var);
    if (r.representative == var) continue;

    objective_map.erase(var);
    var_to_constraints[var].erase(-1);

    // Do the substitution.
    offset_change += coeff * r.offset;
    const int64 new_coeff = objective_map[r.representative] += coeff * r.coeff;

    // Process new term.
    if (new_coeff == 0) {
      objective_map.erase(r.representative);
      var_to_constraints[r.representative].erase(-1);
    } else {
      var_to_constraints[r.representative].insert(-1);
      if (IsFixed(r.representative)) {
        offset_change += new_coeff * MinOf(r.representative);
        var_to_constraints[r.representative].erase(-1);
        objective_map.erase(r.representative);
      }
    }
  }

  Domain implied_domain(0);
  int64 gcd(0);

  // We need to sort the entries to be deterministic.
  tmp_entries.clear();
  for (const auto& entry : objective_map) {
    tmp_entries.push_back(entry);
  }
  std::sort(tmp_entries.begin(), tmp_entries.end());
  for (const auto& entry : tmp_entries) {
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
    for (auto& entry : objective_map) {
      entry.second /= gcd;
    }
    objective_domain = objective_domain.InverseMultiplicationBy(gcd);
    objective_offset /= static_cast<double>(gcd);
    objective_scaling_factor *= static_cast<double>(gcd);
  }

  if (objective_domain.IsEmpty()) return false;

  // Detect if the objective domain do not limit the "optimal" objective value.
  // If this is true, then we can apply any reduction that reduce the objective
  // value without any issues.
  objective_domain_is_constraining =
      !implied_domain
           .IntersectionWith(Domain(kint64min, objective_domain.Max()))
           .IsIncludedIn(objective_domain);
  return true;
}

void PresolveContext::SubstituteVariableInObjective(
    int var_in_equality, int64 coeff_in_equality,
    const ConstraintProto& equality, std::vector<int>* new_vars_in_objective) {
  CHECK(equality.enforcement_literal().empty());
  CHECK(RefIsPositive(var_in_equality));

  if (new_vars_in_objective != nullptr) new_vars_in_objective->clear();

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
    if (var == var_in_equality) continue;

    int64& map_ref = objective_map[var];
    if (map_ref == 0 && new_vars_in_objective != nullptr) {
      new_vars_in_objective->push_back(var);
    }
    map_ref -= coeff * multiplier;

    if (map_ref == 0) {
      objective_map.erase(var);
      var_to_constraints[var].erase(-1);
    } else {
      var_to_constraints[var].insert(-1);
    }
  }

  objective_map.erase(var_in_equality);
  var_to_constraints[var_in_equality].erase(-1);

  // Deal with the offset.
  Domain offset = ReadDomainFromProto(equality.linear());
  DCHECK_EQ(offset.Min(), offset.Max());
  bool exact = true;
  offset = offset.MultiplicationBy(multiplier, &exact);
  CHECK(exact);

  // Tricky: The objective domain is without the offset, so we need to shift it
  objective_offset += static_cast<double>(offset.Min());
  objective_domain = objective_domain.AdditionWith(Domain(-offset.Min()));

  // Because we can assume that the constraint we used was constraining
  // (otherwise it would have been removed), the objective domain should be now
  // constraining.
  objective_domain_is_constraining = true;
}

void PresolveContext::WriteObjectiveToProto() {
  if (objective_domain.IsEmpty()) {
    return (void)NotifyThatModelIsUnsat();
  }

  // We need to sort the entries to be deterministic.
  std::vector<std::pair<int, int64>> entries;
  for (const auto& entry : objective_map) {
    entries.push_back(entry);
  }
  std::sort(entries.begin(), entries.end());

  CpObjectiveProto* mutable_obj = working_model->mutable_objective();
  mutable_obj->set_offset(objective_offset);
  mutable_obj->set_scaling_factor(objective_scaling_factor);
  FillDomainInProto(objective_domain, mutable_obj);
  mutable_obj->clear_vars();
  mutable_obj->clear_coeffs();
  for (const auto& entry : entries) {
    mutable_obj->add_vars(entry.first);
    mutable_obj->add_coeffs(entry.second);
  }
}

}  // namespace sat
}  // namespace operations_research
