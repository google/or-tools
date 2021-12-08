// Copyright 2010-2021 Google LLC
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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

int SavedLiteral::Get(PresolveContext* context) const {
  return context->GetLiteralRepresentative(ref_);
}

int SavedVariable::Get(PresolveContext* context) const {
  return context->GetVariableRepresentative(ref_);
}

void PresolveContext::ClearStats() { stats_by_rule_name_.clear(); }

int PresolveContext::NewIntVar(const Domain& domain) {
  IntegerVariableProto* const var = working_model->add_variables();
  FillDomainInProto(domain, var);
  InitializeNewDomains();
  return working_model->variables_size() - 1;
}

int PresolveContext::NewBoolVar() { return NewIntVar(Domain(0, 1)); }

int PresolveContext::GetOrCreateConstantVar(int64_t cst) {
  if (!constant_to_ref_.contains(cst)) {
    constant_to_ref_[cst] = SavedVariable(working_model->variables_size());
    IntegerVariableProto* const var_proto = working_model->add_variables();
    var_proto->add_domain(cst);
    var_proto->add_domain(cst);
    InitializeNewDomains();
  }
  return constant_to_ref_[cst].Get(this);
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

int64_t PresolveContext::MinOf(int ref) const {
  DCHECK(!DomainIsEmpty(ref));
  return RefIsPositive(ref) ? domains[PositiveRef(ref)].Min()
                            : -domains[PositiveRef(ref)].Max();
}

int64_t PresolveContext::MaxOf(int ref) const {
  DCHECK(!DomainIsEmpty(ref));
  return RefIsPositive(ref) ? domains[PositiveRef(ref)].Max()
                            : -domains[PositiveRef(ref)].Min();
}

int64_t PresolveContext::FixedValue(int ref) const {
  DCHECK(!DomainIsEmpty(ref));
  CHECK(IsFixed(ref));
  return RefIsPositive(ref) ? domains[PositiveRef(ref)].Min()
                            : -domains[PositiveRef(ref)].Min();
}

int64_t PresolveContext::MinOf(const LinearExpressionProto& expr) const {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int64_t coeff = expr.coeffs(i);
    if (coeff > 0) {
      result += coeff * MinOf(expr.vars(i));
    } else {
      result += coeff * MaxOf(expr.vars(i));
    }
  }
  return result;
}

int64_t PresolveContext::MaxOf(const LinearExpressionProto& expr) const {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int64_t coeff = expr.coeffs(i);
    if (coeff > 0) {
      result += coeff * MaxOf(expr.vars(i));
    } else {
      result += coeff * MinOf(expr.vars(i));
    }
  }
  return result;
}

bool PresolveContext::IsFixed(const LinearExpressionProto& expr) const {
  for (int i = 0; i < expr.vars_size(); ++i) {
    if (expr.coeffs(i) != 0 && !IsFixed(expr.vars(i))) return false;
  }
  return true;
}

int64_t PresolveContext::FixedValue(const LinearExpressionProto& expr) const {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    if (expr.coeffs(i) == 0) continue;
    result += expr.coeffs(i) * FixedValue(expr.vars(i));
  }
  return result;
}

Domain PresolveContext::DomainSuperSetOf(
    const LinearExpressionProto& expr) const {
  Domain result(expr.offset());
  for (int i = 0; i < expr.vars_size(); ++i) {
    result = result.AdditionWith(
        DomainOf(expr.vars(i)).MultiplicationBy(expr.coeffs(i)));
  }
  return result;
}

bool PresolveContext::ExpressionIsAffineBoolean(
    const LinearExpressionProto& expr) const {
  if (expr.vars().size() != 1) return false;
  return CanBeUsedAsLiteral(expr.vars(0));
}

int PresolveContext::LiteralForExpressionMax(
    const LinearExpressionProto& expr) const {
  const int ref = expr.vars(0);
  return RefIsPositive(ref) == (expr.coeffs(0) > 0) ? ref : NegatedRef(ref);
}

bool PresolveContext::ExpressionIsSingleVariable(
    const LinearExpressionProto& expr) const {
  return expr.offset() == 0 && expr.vars_size() == 1 && expr.coeffs(0) == 1;
}

bool PresolveContext::ExpressionIsALiteral(const LinearExpressionProto& expr,
                                           int* literal) const {
  if (expr.vars_size() != 1) return false;
  const int ref = expr.vars(0);
  const int var = PositiveRef(ref);
  if (MinOf(var) < 0 || MaxOf(var) > 1) return false;

  if (expr.offset() == 0 && expr.coeffs(0) == 1 && RefIsPositive(ref)) {
    if (literal != nullptr) *literal = ref;
    return true;
  }
  if (expr.offset() == 1 && expr.coeffs(0) == -1 && RefIsPositive(ref)) {
    if (literal != nullptr) *literal = NegatedRef(ref);
    return true;
  }
  if (expr.offset() == 1 && expr.coeffs(0) == 1 && !RefIsPositive(ref)) {
    if (literal != nullptr) *literal = ref;
    return true;
  }
  return false;
}

// Note that we only support converted intervals.
bool PresolveContext::IntervalIsConstant(int ct_ref) const {
  const ConstraintProto& proto = working_model->constraints(ct_ref);
  if (!proto.enforcement_literal().empty()) return false;
  if (!proto.interval().has_start()) return false;
  for (const int var : proto.interval().start().vars()) {
    if (!IsFixed(var)) return false;
  }
  for (const int var : proto.interval().size().vars()) {
    if (!IsFixed(var)) return false;
  }
  for (const int var : proto.interval().end().vars()) {
    if (!IsFixed(var)) return false;
  }
  return true;
}

std::string PresolveContext::IntervalDebugString(int ct_ref) const {
  if (IntervalIsConstant(ct_ref)) {
    return absl::StrCat("interval_", ct_ref, "(", StartMin(ct_ref), "..",
                        EndMax(ct_ref), ")");
  } else if (ConstraintIsOptional(ct_ref)) {
    const int literal =
        working_model->constraints(ct_ref).enforcement_literal(0);
    if (SizeMin(ct_ref) == SizeMax(ct_ref)) {
      return absl::StrCat("interval_", ct_ref, "(lit=", literal, ", ",
                          StartMin(ct_ref), " --(", SizeMin(ct_ref), ")--> ",
                          EndMax(ct_ref), ")");
    } else {
      return absl::StrCat("interval_", ct_ref, "(lit=", literal, ", ",
                          StartMin(ct_ref), " --(", SizeMin(ct_ref), "..",
                          SizeMax(ct_ref), ")--> ", EndMax(ct_ref), ")");
    }
  } else if (SizeMin(ct_ref) == SizeMax(ct_ref)) {
    return absl::StrCat("interval_", ct_ref, "(", StartMin(ct_ref), " --(",
                        SizeMin(ct_ref), ")--> ", EndMax(ct_ref), ")");
  } else {
    return absl::StrCat("interval_", ct_ref, "(", StartMin(ct_ref), " --(",
                        SizeMin(ct_ref), "..", SizeMax(ct_ref), ")--> ",
                        EndMax(ct_ref), ")");
  }
}

int64_t PresolveContext::StartMin(int ct_ref) const {
  const IntervalConstraintProto& interval =
      working_model->constraints(ct_ref).interval();
  return MinOf(interval.start());
}

int64_t PresolveContext::StartMax(int ct_ref) const {
  const IntervalConstraintProto& interval =
      working_model->constraints(ct_ref).interval();
  return MaxOf(interval.start());
}

int64_t PresolveContext::EndMin(int ct_ref) const {
  const IntervalConstraintProto& interval =
      working_model->constraints(ct_ref).interval();
  return MinOf(interval.end());
}

int64_t PresolveContext::EndMax(int ct_ref) const {
  const IntervalConstraintProto& interval =
      working_model->constraints(ct_ref).interval();
  return MaxOf(interval.end());
}

int64_t PresolveContext::SizeMin(int ct_ref) const {
  const IntervalConstraintProto& interval =
      working_model->constraints(ct_ref).interval();
  return MinOf(interval.size());
}

int64_t PresolveContext::SizeMax(int ct_ref) const {
  const IntervalConstraintProto& interval =
      working_model->constraints(ct_ref).interval();
  return MaxOf(interval.size());
}

// Important: To be sure a variable can be removed, we need it to not be a
// representative of both affine and equivalence relation.
bool PresolveContext::VariableIsNotRepresentativeOfEquivalenceClass(
    int var) const {
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

bool PresolveContext::VariableIsRemovable(int ref) const {
  const int var = PositiveRef(ref);
  return VariableIsNotRepresentativeOfEquivalenceClass(var) &&
         !keep_all_feasible_solutions;
}

// Tricky: If this variable is equivalent to another one (but not the
// representative) and appear in just one constraint, then this constraint must
// be the affine defining one. And in this case the code using this function
// should do the proper stuff.
bool PresolveContext::VariableIsUniqueAndRemovable(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  const int var = PositiveRef(ref);
  return var_to_constraints_[var].size() == 1 && VariableIsRemovable(var);
}

bool PresolveContext::VariableWithCostIsUnique(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  const int var = PositiveRef(ref);
  return VariableIsNotRepresentativeOfEquivalenceClass(var) &&
         var_to_constraints_[var].contains(kObjectiveConstraint) &&
         var_to_constraints_[var].size() == 2;
}

// Tricky: Same remark as for VariableIsUniqueAndRemovable().
//
// Also if the objective domain is constraining, we can't have a preferred
// direction, so we cannot easily remove such variable.
bool PresolveContext::VariableWithCostIsUniqueAndRemovable(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  const int var = PositiveRef(ref);
  return VariableIsRemovable(var) && !objective_domain_is_constraining_ &&
         VariableWithCostIsUnique(var);
}

// Here, even if the variable is equivalent to others, if its affine defining
// constraints where removed, then it is not needed anymore.
bool PresolveContext::VariableIsNotUsedAnymore(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
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
  if (IsFixed(ref)) return false;
  if (!removed_variables_.contains(PositiveRef(ref))) return false;
  if (!var_to_constraints_[PositiveRef(ref)].empty()) {
    SOLVER_LOG(logger_, "Variable ", PositiveRef(ref),
               " was removed, yet it appears in some constraints!");
    SOLVER_LOG(logger_, "affine relation: ",
               AffineRelationDebugString(PositiveRef(ref)));
    for (const int c : var_to_constraints_[PositiveRef(ref)]) {
      SOLVER_LOG(
          logger_, "constraint #", c, " : ",
          c >= 0 ? working_model->constraints(c).ShortDebugString() : "");
    }
  }
  return true;
}

bool PresolveContext::VariableIsOnlyUsedInEncodingAndMaybeInObjective(
    int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  const int var = PositiveRef(ref);
  return var_to_num_linear1_[var] == var_to_constraints_[var].size() ||
         (var_to_constraints_[var].contains(kObjectiveConstraint) &&
          var_to_num_linear1_[var] + 1 == var_to_constraints_[var].size());
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

bool PresolveContext::DomainContains(int ref, int64_t value) const {
  if (!RefIsPositive(ref)) {
    return domains[PositiveRef(ref)].Contains(-value);
  }
  return domains[ref].Contains(value);
}

bool PresolveContext::DomainContains(const LinearExpressionProto& expr,
                                     int64_t value) const {
  CHECK_LE(expr.vars_size(), 1);
  if (IsFixed(expr)) {
    return FixedValue(expr) == value;
  }
  if ((value - expr.offset()) % expr.coeffs(0) != 0) return false;
  return DomainContains(expr.vars(0), (value - expr.offset()) / expr.coeffs(0));
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
    is_unsat_ = true;
    return false;
  }

  // Propagate the domain of the representative right away.
  // Note that the recursive call should only by one level deep.
  const AffineRelation::Relation r = GetAffineRelation(var);
  if (r.representative == var) return true;
  return IntersectDomainWith(r.representative,
                             DomainOf(var)
                                 .AdditionWith(Domain(-r.offset))
                                 .InverseMultiplicationBy(r.coeff));
}

ABSL_MUST_USE_RESULT bool PresolveContext::IntersectDomainWith(
    const LinearExpressionProto& expr, const Domain& domain,
    bool* domain_modified) {
  if (expr.vars().empty()) {
    if (domain.Contains(expr.offset())) {
      return true;
    } else {
      is_unsat_ = true;
      return false;
    }
  }
  if (expr.vars().size() == 1) {  // Affine
    return IntersectDomainWith(expr.vars(0),
                               domain.AdditionWith(Domain(-expr.offset()))
                                   .InverseMultiplicationBy(expr.coeffs(0)),
                               domain_modified);
  }

  // We don't do anything for longer expression for now.
  return true;
}

ABSL_MUST_USE_RESULT bool PresolveContext::SetLiteralToFalse(int lit) {
  const int var = PositiveRef(lit);
  const int64_t value = RefIsPositive(lit) ? 0 : 1;
  return IntersectDomainWith(var, Domain(value));
}

ABSL_MUST_USE_RESULT bool PresolveContext::SetLiteralToTrue(int lit) {
  return SetLiteralToFalse(NegatedRef(lit));
}

bool PresolveContext::ConstraintIsInactive(int index) const {
  const ConstraintProto& ct = working_model->constraints(index);
  if (ct.constraint_case() ==
      ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET) {
    return true;
  }
  for (const int literal : ct.enforcement_literal()) {
    if (LiteralIsFalse(literal)) return true;
  }
  return false;
}

bool PresolveContext::ConstraintIsOptional(int ct_ref) const {
  const ConstraintProto& ct = working_model->constraints(ct_ref);
  bool contains_one_free_literal = false;
  for (const int literal : ct.enforcement_literal()) {
    if (LiteralIsFalse(literal)) return false;
    if (!LiteralIsTrue(literal)) contains_one_free_literal = true;
  }
  return contains_one_free_literal;
}

void PresolveContext::UpdateRuleStats(const std::string& name, int num_times) {
  // We only count if we are going to display it.
  if (logger_->LoggingIsEnabled()) {
    VLOG(2) << num_presolve_operations << " : " << name;
    stats_by_rule_name_[name] += num_times;
  }
  num_presolve_operations += num_times;
}

void PresolveContext::UpdateLinear1Usage(const ConstraintProto& ct, int c) {
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
  const ConstraintProto& ct = working_model->constraints(c);
  constraint_to_vars_[c] = UsedVariables(ct);
  constraint_to_intervals_[c] = UsedIntervals(ct);
  for (const int v : constraint_to_vars_[c]) {
    DCHECK(!VariableWasRemoved(v));
    var_to_constraints_[v].insert(c);
  }
  for (const int i : constraint_to_intervals_[c]) interval_usage_[i]++;
  UpdateLinear1Usage(ct, c);
}

void PresolveContext::UpdateConstraintVariableUsage(int c) {
  if (is_unsat_) return;
  DCHECK_EQ(constraint_to_vars_.size(), working_model->constraints_size());
  const ConstraintProto& ct = working_model->constraints(c);

  // We don't optimize the interval usage as this is not super frequent.
  for (const int i : constraint_to_intervals_[c]) interval_usage_[i]--;
  constraint_to_intervals_[c] = UsedIntervals(ct);
  for (const int i : constraint_to_intervals_[c]) interval_usage_[i]++;

  // For the variables, we avoid an erase() followed by an insert() for the
  // variables that didn't change.
  tmp_new_usage_ = UsedVariables(ct);
  const std::vector<int>& old_usage = constraint_to_vars_[c];
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
  for (; i < old_size; ++i) var_to_constraints_[old_usage[i]].erase(c);
  constraint_to_vars_[c] = tmp_new_usage_;

  UpdateLinear1Usage(ct, c);
}

bool PresolveContext::ConstraintVariableGraphIsUpToDate() const {
  return constraint_to_vars_.size() == working_model->constraints_size();
}

void PresolveContext::UpdateNewConstraintsVariableUsage() {
  if (is_unsat_) return;
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
  if (is_unsat_) return true;  // We do not care in this case.
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
      if (!objective_map_.contains(v)) {
        LOG(INFO) << "Variable " << v
                  << " is marked as part of the objective but isn't.";
        return false;
      }
    }
  }
  if (num_in_objective != objective_map_.size()) {
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
bool PresolveContext::AddRelation(int x, int y, int64_t c, int64_t o,
                                  AffineRelation* repo) {
  // When the coefficient is larger than one, then if later one variable becomes
  // Boolean, it must be the representative.
  if (std::abs(c) != 1) return repo->TryAdd(x, y, c, o);

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
  const int64_t m_x = std::max(std::abs(MinOf(rep_x)), std::abs(MaxOf(rep_x)));
  const int64_t m_y = std::max(std::abs(MinOf(rep_y)), std::abs(MaxOf(rep_y)));
  bool allow_rep_x = m_x < m_y;
  bool allow_rep_y = m_y < m_x;
  if (m_x == m_y) {
    // If both magnitude are the same, we prefer a positive domain.
    // This is important so we don't use [-1, 0] as a representative for [0, 1].
    allow_rep_x = MinOf(rep_x) >= MinOf(rep_y);
    allow_rep_y = MinOf(rep_y) >= MinOf(rep_x);
  }
  if (allow_rep_x && allow_rep_y) {
    // If both representative are okay, we force the choice to the variable
    // with lower index. This is needed because we have two "equivalence"
    // relations, and we want the same representative in both.
    if (rep_x < rep_y) {
      allow_rep_y = false;
    } else {
      allow_rep_x = false;
    }
  }
  return repo->TryAdd(x, y, c, o, allow_rep_x, allow_rep_y);
}

// Note that we just add the relation to the var_equiv_relations_, not to the
// affine one. This is enough, and should prevent overflow in the affine
// relation class: if we keep chaining variable fixed to zero, the coefficient
// in the relation can overflow. For instance if x = 200 y and z = 200 t,
// nothing prevent us if all end up being zero, to say y = z, which will result
// in x = 200^2 t. If we make a few bad choices like this, then we can have an
// overflow.
void PresolveContext::ExploitFixedDomain(int var) {
  DCHECK(RefIsPositive(var));
  DCHECK(IsFixed(var));
  const int64_t min = MinOf(var);
  if (constant_to_ref_.contains(min)) {
    const int rep = constant_to_ref_[min].Get(this);
    if (RefIsPositive(rep)) {
      if (rep != var) {
        AddRelation(var, rep, 1, 0, &var_equiv_relations_);
      }
    } else {
      if (PositiveRef(rep) == var) {
        CHECK_EQ(min, 0);
      } else {
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
  if (r.representative == var) return true;

  // Propagate domains both ways.
  // var = coeff * rep + offset
  if (!IntersectDomainWith(r.representative,
                           DomainOf(var)
                               .AdditionWith(Domain(-r.offset))
                               .InverseMultiplicationBy(r.coeff))) {
    return false;
  }
  if (!IntersectDomainWith(var, DomainOf(r.representative)
                                    .MultiplicationBy(r.coeff)
                                    .AdditionWith(Domain(r.offset)))) {
    return false;
  }

  return true;
}

void PresolveContext::RemoveAllVariablesFromAffineRelationConstraint() {
  for (auto& ref_map : var_to_constraints_) {
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
    LOG(INFO) << "Removing affine relation: " << AffineRelationDebugString(var);
  }
}

void PresolveContext::CanonicalizeVariable(int ref) {
  const int var = GetAffineRelation(ref).representative;
  const int64_t min = MinOf(var);
  if (min == 0 || IsFixed(var)) return;  // Nothing to do.

  const int new_var = NewIntVar(DomainOf(var).AdditionWith(Domain(-min)));
  CHECK(StoreAffineRelation(var, new_var, 1, min, /*debug_no_recursion=*/true));
  UpdateRuleStats("variables: canonicalize domain");
  UpdateNewConstraintsVariableUsage();
}

bool PresolveContext::ScaleFloatingPointObjective() {
  DCHECK(working_model->has_floating_point_objective());
  DCHECK(!working_model->has_objective());
  const auto& objective = working_model->floating_point_objective();
  std::vector<std::pair<int, double>> terms;
  for (int i = 0; i < objective.vars_size(); ++i) {
    DCHECK(RefIsPositive(objective.vars(i)));
    terms.push_back({objective.vars(i), objective.coeffs(i)});
  }
  const double offset = objective.offset();
  const bool maximize = objective.maximize();
  working_model->clear_floating_point_objective();

  // We need the domains up to date before scaling.
  WriteVariableDomainsToProto();
  return ScaleAndSetObjective(params_, terms, offset, maximize, working_model,
                              logger_);
}

bool PresolveContext::CanonicalizeAffineVariable(int ref, int64_t coeff,
                                                 int64_t mod, int64_t rhs) {
  CHECK_NE(mod, 0);
  CHECK_NE(coeff, 0);

  const int64_t gcd = std::gcd(coeff, mod);
  if (gcd != 1) {
    if (rhs % gcd != 0) {
      return NotifyThatModelIsUnsat(
          absl::StrCat("Infeasible ", coeff, " * X = ", rhs, " % ", mod));
    }
    coeff /= gcd;
    mod /= gcd;
    rhs /= gcd;
  }

  // We just abort in this case as there is no point introducing a new variable.
  if (std::abs(mod) == 1) return true;

  int var = ref;
  if (!RefIsPositive(var)) {
    var = NegatedRef(ref);
    coeff = -coeff;
    rhs = -rhs;
  }

  // From var * coeff % mod = rhs
  // We have var = mod * X + offset.
  const int64_t offset = ProductWithModularInverse(coeff, mod, rhs);

  // Lets create a new integer variable and add the affine relation.
  const Domain new_domain =
      DomainOf(var).AdditionWith(Domain(-offset)).InverseMultiplicationBy(mod);
  if (new_domain.IsEmpty()) {
    return NotifyThatModelIsUnsat(
        "Empty domain in CanonicalizeAffineVariable()");
  }
  if (new_domain.IsFixed()) {
    UpdateRuleStats("variables: fixed value due to affine relation");
    return IntersectDomainWith(
        var, new_domain.ContinuousMultiplicationBy(mod).AdditionWith(
                 Domain(offset)));
  }

  // We make sure the new variable has a domain starting at zero to minimize
  // future overflow issues. If it end up Boolean, it is also nice to be able to
  // use it as such.
  //
  // A potential problem with this is that it messes up the natural variable
  // order chosen by the modeler. We try to correct that when mapping variables
  // at the end of the presolve.
  const int64_t min_value = new_domain.Min();
  const int new_var = NewIntVar(new_domain.AdditionWith(Domain(-min_value)));
  CHECK(StoreAffineRelation(var, new_var, mod, offset + mod * min_value,
                            /*debug_no_recursion=*/true));
  UpdateRuleStats("variables: canonicalize affine domain");
  UpdateNewConstraintsVariableUsage();
  return true;
}

bool PresolveContext::StoreAffineRelation(int ref_x, int ref_y, int64_t coeff,
                                          int64_t offset,
                                          bool debug_no_recursion) {
  CHECK_NE(coeff, 0);
  if (is_unsat_) return false;

  // TODO(user): I am not 100% sure why, but sometimes the representative is
  // fixed but that is not propagated to ref_x or ref_y and this causes issues.
  if (!PropagateAffineRelation(ref_x)) return false;
  if (!PropagateAffineRelation(ref_y)) return false;

  if (IsFixed(ref_x)) {
    const int64_t lhs = DomainOf(ref_x).FixedValue() - offset;
    if (lhs % std::abs(coeff) != 0) {
      return NotifyThatModelIsUnsat();
    }
    UpdateRuleStats("affine: fixed");
    return IntersectDomainWith(ref_y, Domain(lhs / coeff));
  }

  if (IsFixed(ref_y)) {
    const int64_t value_x = DomainOf(ref_y).FixedValue() * coeff + offset;
    UpdateRuleStats("affine: fixed");
    return IntersectDomainWith(ref_x, Domain(value_x));
  }

  // If both are already in the same class, we need to make sure the relations
  // are compatible.
  const AffineRelation::Relation rx = GetAffineRelation(ref_x);
  const AffineRelation::Relation ry = GetAffineRelation(ref_y);
  if (rx.representative == ry.representative) {
    // x = rx.coeff * rep + rx.offset;
    // y = ry.coeff * rep + ry.offset;
    // And x == coeff * ry.coeff * rep + (coeff * ry.offset + offset).
    //
    // So we get the relation a * rep == b with a and b defined here:
    const int64_t a = coeff * ry.coeff - rx.coeff;
    const int64_t b = coeff * ry.offset + offset - rx.offset;
    if (a == 0) {
      if (b != 0) return NotifyThatModelIsUnsat();
      return true;
    }
    if (b % a != 0) {
      return NotifyThatModelIsUnsat();
    }
    UpdateRuleStats("affine: unique solution");
    const int64_t unique_value = -b / a;
    if (!IntersectDomainWith(rx.representative, Domain(unique_value))) {
      return false;
    }
    if (!IntersectDomainWith(ref_x,
                             Domain(unique_value * rx.coeff + rx.offset))) {
      return false;
    }
    if (!IntersectDomainWith(ref_y,
                             Domain(unique_value * ry.coeff + ry.offset))) {
      return false;
    }
    return true;
  }

  // ref_x = coeff * ref_y + offset;
  // rx.coeff * rep_x + rx.offset =
  //    coeff * (ry.coeff * rep_y + ry.offset) + offset
  //
  // We have a * rep_x + b * rep_y == o
  int64_t a = rx.coeff;
  int64_t b = coeff * ry.coeff;
  int64_t o = coeff * ry.offset + offset - rx.offset;
  CHECK_NE(a, 0);
  CHECK_NE(b, 0);
  {
    const int64_t gcd = MathUtil::GCD64(std::abs(a), std::abs(b));
    if (gcd != 1) {
      a /= gcd;
      b /= gcd;
      if (o % gcd != 0) return NotifyThatModelIsUnsat();
      o /= gcd;
    }
  }

  // In this (rare) case, we need to canonicalize one of the variable that will
  // become the representative for both.
  if (std::abs(a) > 1 && std::abs(b) > 1) {
    UpdateRuleStats("affine: created common representative");
    if (!CanonicalizeAffineVariable(rx.representative, a, std::abs(b),
                                    offset)) {
      return false;
    }

    // Re-add the relation now that a will resolve to a multiple of b.
    return StoreAffineRelation(ref_x, ref_y, coeff, offset,
                               /*debug_no_recursion=*/true);
  }

  // Canonicalize to x = c * y + o
  int x, y;
  int64_t c;
  bool negate = false;
  if (std::abs(a) == 1) {
    x = rx.representative;
    y = ry.representative;
    c = b;
    negate = a < 0;
  } else {
    CHECK_EQ(std::abs(b), 1);
    x = ry.representative;
    y = rx.representative;
    c = a;
    negate = b < 0;
  }
  if (negate) {
    c = -c;
    o = -o;
  }
  CHECK(RefIsPositive(x));
  CHECK(RefIsPositive(y));

  // Lets propagate domains first.
  if (!IntersectDomainWith(
          y, DomainOf(x).AdditionWith(Domain(-o)).InverseMultiplicationBy(c))) {
    return false;
  }
  if (!IntersectDomainWith(
          x,
          DomainOf(y).ContinuousMultiplicationBy(c).AdditionWith(Domain(o)))) {
    return false;
  }

  // To avoid corner cases where replacing x by y in a linear expression
  // can cause overflow, we might want to canonicalize y first to avoid
  // cases like x = c * [large_value, ...] - large_value.
  //
  // TODO(user): we can do better for overflow by not always choosing the
  // min at zero, do the best things if it becomes needed.
  if (std::abs(o) > std::max(std::abs(MinOf(x)), std::abs(MaxOf(x)))) {
    // Both these function recursively call StoreAffineRelation() but shouldn't
    // be able to cascade (CHECKED).
    CHECK(!debug_no_recursion);
    CanonicalizeVariable(y);
    return StoreAffineRelation(x, y, c, o, /*debug_no_recursion=*/true);
  }

  // TODO(user): can we force the rep and remove GetAffineRelation()?
  CHECK(AddRelation(x, y, c, o, &affine_relations_));
  if ((c == 1 || c == -1) && o == 0) {
    CHECK(AddRelation(x, y, c, o, &var_equiv_relations_));
  }

  UpdateRuleStats("affine: new relation");

  // Lets propagate again the new relation. We might as well do it as early
  // as possible and not all call site do it.
  //
  // TODO(user): I am not sure this is needed given the propagation above.
  if (!PropagateAffineRelation(ref_x)) return false;
  if (!PropagateAffineRelation(ref_y)) return false;

  // These maps should only contains representative, so only need to remap
  // either x or y.
  const int rep = GetAffineRelation(x).representative;

  // The domain didn't change, but this notification allows to re-process any
  // constraint containing these variables. Note that we do not need to
  // retrigger a propagation of the constraint containing a variable whose
  // representative didn't change.
  if (x != rep) modified_domains.Set(x);
  if (y != rep) modified_domains.Set(y);

  var_to_constraints_[x].insert(kAffineRelationConstraint);
  var_to_constraints_[y].insert(kAffineRelationConstraint);
  return true;
}

bool PresolveContext::StoreBooleanEqualityRelation(int ref_a, int ref_b) {
  if (is_unsat_) return false;

  CHECK(!VariableWasRemoved(ref_a));
  CHECK(!VariableWasRemoved(ref_b));
  CHECK(!DomainOf(ref_a).IsEmpty());
  CHECK(!DomainOf(ref_b).IsEmpty());
  CHECK(CanBeUsedAsLiteral(ref_a));
  CHECK(CanBeUsedAsLiteral(ref_b));

  if (ref_a == ref_b) return true;
  if (ref_a == NegatedRef(ref_b)) return IntersectDomainWith(ref_a, Domain(0));

  const int var_a = PositiveRef(ref_a);
  const int var_b = PositiveRef(ref_b);
  if (RefIsPositive(ref_a) == RefIsPositive(ref_b)) {
    // a = b
    return StoreAffineRelation(var_a, var_b, /*coeff=*/1, /*offset=*/0);
  }
  // a = 1 - b
  return StoreAffineRelation(var_a, var_b, /*coeff=*/-1, /*offset=*/1);
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

bool PresolveContext::GetAbsRelation(int target_ref, int* ref) {
  auto it = abs_relations_.find(target_ref);
  if (it == abs_relations_.end()) return false;

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
  CHECK(!VariableWasRemoved(*ref));
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
  if (o.coeff == -1) r.coeff = -r.coeff;
  if (!RefIsPositive(ref)) {
    r.coeff *= -1;
    r.offset *= -1;
  }
  return r;
}

std::string PresolveContext::RefDebugString(int ref) const {
  return absl::StrCat(RefIsPositive(ref) ? "X" : "-X", PositiveRef(ref),
                      DomainOf(ref).ToString());
}

std::string PresolveContext::AffineRelationDebugString(int ref) const {
  const AffineRelation::Relation r = GetAffineRelation(ref);
  return absl::StrCat(RefDebugString(ref), " = ", r.coeff, " * ",
                      RefDebugString(r.representative), " + ", r.offset);
}

// Create the internal structure for any new variables in working_model.
void PresolveContext::InitializeNewDomains() {
  for (int i = domains.size(); i < working_model->variables_size(); ++i) {
    domains.emplace_back(ReadDomainFromProto(working_model->variables(i)));
    if (domains.back().IsEmpty()) {
      is_unsat_ = true;
      return;
    }
    if (IsFixed(i)) ExploitFixedDomain(i);
  }
  modified_domains.Resize(domains.size());
  var_to_constraints_.resize(domains.size());
  var_to_num_linear1_.resize(domains.size());
  var_to_ub_only_constraints.resize(domains.size());
  var_to_lb_only_constraints.resize(domains.size());
}

void PresolveContext::CanonicalizeDomainOfSizeTwo(int var) {
  CHECK(RefIsPositive(var));
  CHECK_EQ(DomainOf(var).Size(), 2);
  const int64_t var_min = MinOf(var);
  const int64_t var_max = MaxOf(var);

  if (is_unsat_) return;

  absl::flat_hash_map<int64_t, SavedLiteral>& var_map = encoding_[var];

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
      if (is_unsat_) return;
    }
    min_literal = GetLiteralRepresentative(min_literal);
    max_literal = GetLiteralRepresentative(max_literal);
    if (!IsFixed(min_literal)) CHECK_EQ(min_literal, NegatedRef(max_literal));
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
      (void)StoreAffineRelation(var, PositiveRef(max_literal),
                                var_max - var_min, var_min);
    } else {
      (void)StoreAffineRelation(var, PositiveRef(max_literal),
                                var_min - var_max, var_max);
    }
  }
}

void PresolveContext::InsertVarValueEncodingInternal(int literal, int var,
                                                     int64_t value,
                                                     bool add_constraints) {
  CHECK(RefIsPositive(var));
  CHECK(!VariableWasRemoved(literal));
  CHECK(!VariableWasRemoved(var));
  absl::flat_hash_map<int64_t, SavedLiteral>& var_map = encoding_[var];

  // The code below is not 100% correct if this is not the case.
  DCHECK(DomainOf(var).Contains(value));

  // If an encoding already exist, make the two Boolean equals.
  const auto [it, inserted] =
      var_map.insert(std::make_pair(value, SavedLiteral(literal)));
  if (!inserted) {
    const int previous_literal = it->second.Get(this);

    // Ticky and rare: I have only observed this on the LNS of
    // radiation_m18_12_05_sat.fzn. The value was encoded, but maybe we never
    // used the involved variables / constraints, so it was removed (with the
    // encoding constraints) from the model already! We have to be careful.
    if (VariableWasRemoved(previous_literal)) {
      it->second = SavedLiteral(literal);
    } else {
      if (literal != previous_literal) {
        UpdateRuleStats(
            "variables: merge equivalent var value encoding literals");
        StoreBooleanEqualityRelation(literal, previous_literal);
      }
    }
    return;
  }

  if (DomainOf(var).Size() == 2) {
    // TODO(user): There is a bug here if the var == value was not in the
    // domain, it will just be ignored.
    CanonicalizeDomainOfSizeTwo(var);
  } else {
    VLOG(2) << "Insert lit(" << literal << ") <=> var(" << var
            << ") == " << value;
    eq_half_encoding_[var][value].insert(literal);
    neq_half_encoding_[var][value].insert(NegatedRef(literal));
    if (add_constraints) {
      UpdateRuleStats("variables: add encoding constraint");
      AddImplyInDomain(literal, var, Domain(value));
      AddImplyInDomain(NegatedRef(literal), var, Domain(value).Complement());
    }
  }
}

bool PresolveContext::InsertHalfVarValueEncoding(int literal, int var,
                                                 int64_t value, bool imply_eq) {
  if (is_unsat_) return false;
  CHECK(RefIsPositive(var));

  // Creates the linking sets on demand.
  // Insert the enforcement literal in the half encoding map.
  auto& direct_set =
      imply_eq ? eq_half_encoding_[var][value] : neq_half_encoding_[var][value];
  if (!direct_set.insert(literal).second) return false;  // Already there.

  VLOG(2) << "Collect lit(" << literal << ") implies var(" << var
          << (imply_eq ? ") == " : ") != ") << value;
  UpdateRuleStats("variables: detect half reified value encoding");

  // Note(user): We don't expect a lot of literals in these sets, so doing
  // a scan should be okay.
  auto& other_set =
      imply_eq ? neq_half_encoding_[var][value] : eq_half_encoding_[var][value];
  for (const int other : other_set) {
    if (GetLiteralRepresentative(other) != NegatedRef(literal)) continue;

    UpdateRuleStats("variables: detect fully reified value encoding");
    const int imply_eq_literal = imply_eq ? literal : NegatedRef(literal);
    InsertVarValueEncodingInternal(imply_eq_literal, var, value,
                                   /*add_constraints=*/false);
    break;
  }

  return true;
}

bool PresolveContext::CanonicalizeEncoding(int* ref, int64_t* value) {
  const AffineRelation::Relation r = GetAffineRelation(*ref);
  if ((*value - r.offset) % r.coeff != 0) return false;
  *ref = r.representative;
  *value = (*value - r.offset) / r.coeff;
  return true;
}

bool PresolveContext::InsertVarValueEncoding(int literal, int ref,
                                             int64_t value) {
  if (!CanonicalizeEncoding(&ref, &value)) {
    return SetLiteralToFalse(literal);
  }
  literal = GetLiteralRepresentative(literal);
  InsertVarValueEncodingInternal(literal, ref, value, /*add_constraints=*/true);
  return true;
}

bool PresolveContext::StoreLiteralImpliesVarEqValue(int literal, int var,
                                                    int64_t value) {
  if (!CanonicalizeEncoding(&var, &value)) return false;
  literal = GetLiteralRepresentative(literal);
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/true);
}

bool PresolveContext::StoreLiteralImpliesVarNEqValue(int literal, int var,
                                                     int64_t value) {
  if (!CanonicalizeEncoding(&var, &value)) return false;
  literal = GetLiteralRepresentative(literal);
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/false);
}

bool PresolveContext::HasVarValueEncoding(int ref, int64_t value,
                                          int* literal) {
  CHECK(!VariableWasRemoved(ref));
  if (!CanonicalizeEncoding(&ref, &value)) return false;
  const absl::flat_hash_map<int64_t, SavedLiteral>& var_map = encoding_[ref];
  const auto it = var_map.find(value);
  if (it != var_map.end()) {
    if (literal != nullptr) {
      *literal = it->second.Get(this);
    }
    return true;
  }
  return false;
}

bool PresolveContext::IsFullyEncoded(int ref) const {
  const int var = PositiveRef(ref);
  const int64_t size = domains[var].Size();
  if (size <= 2) return true;
  const auto& it = encoding_.find(var);
  return it == encoding_.end() ? false : size <= it->second.size();
}

bool PresolveContext::IsFullyEncoded(const LinearExpressionProto& expr) const {
  CHECK_LE(expr.vars_size(), 1);
  if (IsFixed(expr)) return true;
  return IsFullyEncoded(expr.vars(0));
}

int PresolveContext::GetOrCreateVarValueEncoding(int ref, int64_t value) {
  CHECK(!VariableWasRemoved(ref));
  if (!CanonicalizeEncoding(&ref, &value)) return GetOrCreateConstantVar(0);

  // Positive after CanonicalizeEncoding().
  const int var = ref;

  // Returns the false literal if the value is not in the domain.
  if (!domains[var].Contains(value)) {
    return GetOrCreateConstantVar(0);
  }

  // Returns the associated literal if already present.
  absl::flat_hash_map<int64_t, SavedLiteral>& var_map = encoding_[var];
  auto it = var_map.find(value);
  if (it != var_map.end()) {
    const int lit = it->second.Get(this);
    if (VariableWasRemoved(lit)) {
      // If the variable was already removed, for now we create a new one.
      // This should be rare hopefully.
      var_map.erase(value);
    } else {
      return lit;
    }
  }

  // Special case for fixed domains.
  if (domains[var].Size() == 1) {
    const int true_literal = GetOrCreateConstantVar(1);
    var_map[value] = SavedLiteral(true_literal);
    return true_literal;
  }

  // Special case for domains of size 2.
  const int64_t var_min = MinOf(var);
  const int64_t var_max = MaxOf(var);
  if (domains[var].Size() == 2) {
    // Checks if the other value is already encoded.
    const int64_t other_value = value == var_min ? var_max : var_min;
    auto other_it = var_map.find(other_value);
    if (other_it != var_map.end()) {
      const int literal = NegatedRef(other_it->second.Get(this));
      if (VariableWasRemoved(literal)) {
        // If the variable was already removed, for now we create a new one.
        // This should be rare hopefully.
        var_map.erase(other_value);
      } else {
        // Update the encoding map. The domain could have been reduced to size
        // two after the creation of the first literal.
        var_map[value] = SavedLiteral(literal);
        return literal;
      }
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

int PresolveContext::GetOrCreateAffineValueEncoding(
    const LinearExpressionProto& expr, int64_t value) {
  DCHECK_LE(expr.vars_size(), 1);
  if (IsFixed(expr)) {
    if (FixedValue(expr) == value) {
      return GetOrCreateConstantVar(1);
    } else {
      return GetOrCreateConstantVar(0);
    }
  }

  if ((value - expr.offset()) % expr.coeffs(0) != 0) {
    return GetOrCreateConstantVar(0);
  }

  return GetOrCreateVarValueEncoding(expr.vars(0),
                                     (value - expr.offset()) / expr.coeffs(0));
}

void PresolveContext::ReadObjectiveFromProto() {
  const CpObjectiveProto& obj = working_model->objective();

  objective_offset_ = obj.offset();
  objective_scaling_factor_ = obj.scaling_factor();
  if (objective_scaling_factor_ == 0.0) {
    objective_scaling_factor_ = 1.0;
  }

  objective_integer_offset_ = obj.integer_offset();
  objective_integer_scaling_factor_ = obj.integer_scaling_factor();
  if (objective_integer_scaling_factor_ == 0) {
    objective_integer_scaling_factor_ = 1;
  }

  if (!obj.domain().empty()) {
    // We might relax this in CanonicalizeObjective() when we will compute
    // the possible objective domain from the domains of the variables.
    objective_domain_is_constraining_ = true;
    objective_domain_ = ReadDomainFromProto(obj);
  } else {
    objective_domain_is_constraining_ = false;
    objective_domain_ = Domain::AllValues();
  }

  // This is an upper bound of the higher magnitude that can be reach by
  // summing an objective partial sum. Because of the model validation, this
  // shouldn't overflow, and we make sure it stays this way.
  objective_overflow_detection_ = 0;

  objective_map_.clear();
  for (int i = 0; i < obj.vars_size(); ++i) {
    const int ref = obj.vars(i);
    const int64_t var_max_magnitude =
        std::max(std::abs(MinOf(ref)), std::abs(MaxOf(ref)));

    // Skipping var fixed to zero allow to avoid some overflow in situation
    // were we can deal with it.
    if (var_max_magnitude == 0) continue;

    const int64_t coeff = obj.coeffs(i);
    objective_overflow_detection_ += var_max_magnitude * std::abs(coeff);

    const int var = PositiveRef(ref);
    objective_map_[var] += RefIsPositive(ref) ? coeff : -coeff;
    if (objective_map_[var] == 0) {
      objective_map_.erase(var);
      var_to_constraints_[var].erase(kObjectiveConstraint);
    } else {
      var_to_constraints_[var].insert(kObjectiveConstraint);
    }
  }
}

bool PresolveContext::CanonicalizeObjective(bool simplify_domain) {
  int64_t offset_change = 0;

  // We replace each entry by its affine representative.
  // Note that the non-deterministic loop is fine, but because we iterate
  // one the map while modifying it, it is safer to do a copy rather than to
  // try to handle that in one pass.
  tmp_entries_.clear();
  for (const auto& entry : objective_map_) {
    tmp_entries_.push_back(entry);
  }

  // TODO(user): This is a bit duplicated with the presolve linear code.
  // We also do not propagate back any domain restriction from the objective to
  // the variables if any.
  for (const auto& entry : tmp_entries_) {
    const int var = entry.first;
    const auto it = objective_map_.find(var);
    if (it == objective_map_.end()) continue;
    const int64_t coeff = it->second;

    // If a variable only appear in objective, we can fix it!
    // Note that we don't care if it was in affine relation, because if none
    // of the relations are left, then we can still fix it.
    if (!keep_all_feasible_solutions && !objective_domain_is_constraining_ &&
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
      objective_map_.erase(var);
      continue;
    }

    const AffineRelation::Relation r = GetAffineRelation(var);
    if (r.representative == var) continue;

    objective_map_.erase(var);
    var_to_constraints_[var].erase(kObjectiveConstraint);

    // Do the substitution.
    offset_change += coeff * r.offset;
    const int64_t new_coeff = objective_map_[r.representative] +=
        coeff * r.coeff;

    // Process new term.
    if (new_coeff == 0) {
      objective_map_.erase(r.representative);
      var_to_constraints_[r.representative].erase(kObjectiveConstraint);
    } else {
      var_to_constraints_[r.representative].insert(kObjectiveConstraint);
      if (IsFixed(r.representative)) {
        offset_change += new_coeff * MinOf(r.representative);
        var_to_constraints_[r.representative].erase(kObjectiveConstraint);
        objective_map_.erase(r.representative);
      }
    }
  }

  Domain implied_domain(0);
  int64_t gcd(0);

  // We need to sort the entries to be deterministic.
  tmp_entries_.clear();
  for (const auto& entry : objective_map_) {
    tmp_entries_.push_back(entry);
  }
  std::sort(tmp_entries_.begin(), tmp_entries_.end());
  for (const auto& entry : tmp_entries_) {
    const int var = entry.first;
    const int64_t coeff = entry.second;
    gcd = MathUtil::GCD64(gcd, std::abs(coeff));
    implied_domain =
        implied_domain.AdditionWith(DomainOf(var).MultiplicationBy(coeff))
            .RelaxIfTooComplex();
  }

  // This is the new domain.
  // Note that the domain never include the offset.
  objective_domain_ = objective_domain_.AdditionWith(Domain(-offset_change))
                          .IntersectionWith(implied_domain);

  // Depending on the use case, we cannot do that.
  if (simplify_domain) {
    objective_domain_ =
        objective_domain_.SimplifyUsingImpliedDomain(implied_domain);
  }

  // Update the offset.
  objective_offset_ += offset_change;
  objective_integer_offset_ +=
      offset_change * objective_integer_scaling_factor_;

  // Maybe divide by GCD.
  if (gcd > 1) {
    for (auto& entry : objective_map_) {
      entry.second /= gcd;
    }
    objective_domain_ = objective_domain_.InverseMultiplicationBy(gcd);
    objective_offset_ /= static_cast<double>(gcd);
    objective_scaling_factor_ *= static_cast<double>(gcd);
    objective_integer_scaling_factor_ *= gcd;
  }

  if (objective_domain_.IsEmpty()) return false;

  // Detect if the objective domain do not limit the "optimal" objective value.
  // If this is true, then we can apply any reduction that reduce the objective
  // value without any issues.
  objective_domain_is_constraining_ =
      !implied_domain
           .IntersectionWith(Domain(std::numeric_limits<int64_t>::min(),
                                    objective_domain_.Max()))
           .IsIncludedIn(objective_domain_);
  return true;
}

void PresolveContext::RemoveVariableFromObjective(int var) {
  objective_map_.erase(var);
  var_to_constraints_[var].erase(kObjectiveConstraint);
}

void PresolveContext::AddToObjective(int var, int64_t value) {
  int64_t& map_ref = objective_map_[var];
  map_ref += value;
  if (map_ref == 0) {
    objective_map_.erase(var);
    var_to_constraints_[var].erase(kObjectiveConstraint);
  } else {
    var_to_constraints_[var].insert(kObjectiveConstraint);
  }
}

void PresolveContext::AddToObjectiveOffset(int64_t value) {
  // Tricky: The objective domain is without the offset, so we need to shift it.
  objective_offset_ += static_cast<double>(value);
  objective_integer_offset_ += value * objective_integer_scaling_factor_;
  objective_domain_ = objective_domain_.AdditionWith(Domain(-value));
}

bool PresolveContext::SubstituteVariableInObjective(
    int var_in_equality, int64_t coeff_in_equality,
    const ConstraintProto& equality, std::vector<int>* new_vars_in_objective) {
  CHECK(equality.enforcement_literal().empty());
  CHECK(RefIsPositive(var_in_equality));

  if (new_vars_in_objective != nullptr) new_vars_in_objective->clear();

  // We can only "easily" substitute if the objective coefficient is a multiple
  // of the one in the constraint.
  const int64_t coeff_in_objective =
      gtl::FindOrDie(objective_map_, var_in_equality);
  CHECK_NE(coeff_in_equality, 0);
  CHECK_EQ(coeff_in_objective % coeff_in_equality, 0);

  const int64_t multiplier = coeff_in_objective / coeff_in_equality;

  // Abort if the new objective seems to violate our overflow preconditions.
  int64_t change = 0;
  for (int i = 0; i < equality.linear().vars().size(); ++i) {
    int var = equality.linear().vars(i);
    if (PositiveRef(var) == var_in_equality) continue;
    int64_t coeff = equality.linear().coeffs(i);
    change +=
        std::abs(coeff) * std::max(std::abs(MinOf(var)), std::abs(MaxOf(var)));
  }
  const int64_t new_value =
      CapAdd(CapProd(std::abs(multiplier), change),
             objective_overflow_detection_ -
                 std::abs(coeff_in_equality) *
                     std::max(std::abs(MinOf(var_in_equality)),
                              std::abs(MaxOf(var_in_equality))));
  if (new_value == std::numeric_limits<int64_t>::max()) return false;
  objective_overflow_detection_ = new_value;

  // Compute the objective offset change.
  Domain offset = ReadDomainFromProto(equality.linear());
  DCHECK_EQ(offset.Min(), offset.Max());
  bool exact = true;
  offset = offset.MultiplicationBy(multiplier, &exact);
  CHECK(exact);
  CHECK(!offset.IsEmpty());

  // We also need to make sure the integer_offset will not overflow.
  {
    int64_t temp = CapProd(offset.Min(), objective_integer_scaling_factor_);
    if (temp == std::numeric_limits<int64_t>::max()) return false;
    if (temp == std::numeric_limits<int64_t>::min()) return false;
    temp = CapAdd(temp, objective_integer_offset_);
    if (temp == std::numeric_limits<int64_t>::max()) return false;
    if (temp == std::numeric_limits<int64_t>::min()) return false;
  }

  // Perform the substitution.
  for (int i = 0; i < equality.linear().vars().size(); ++i) {
    int var = equality.linear().vars(i);
    int64_t coeff = equality.linear().coeffs(i);
    if (!RefIsPositive(var)) {
      var = NegatedRef(var);
      coeff = -coeff;
    }
    if (var == var_in_equality) continue;

    int64_t& map_ref = objective_map_[var];
    if (map_ref == 0 && new_vars_in_objective != nullptr) {
      new_vars_in_objective->push_back(var);
    }
    map_ref -= coeff * multiplier;

    if (map_ref == 0) {
      objective_map_.erase(var);
      var_to_constraints_[var].erase(kObjectiveConstraint);
    } else {
      var_to_constraints_[var].insert(kObjectiveConstraint);
    }
  }

  objective_map_.erase(var_in_equality);
  var_to_constraints_[var_in_equality].erase(kObjectiveConstraint);

  // Tricky: The objective domain is without the offset, so we need to shift it.
  objective_offset_ += static_cast<double>(offset.Min());
  objective_integer_offset_ += offset.Min() * objective_integer_scaling_factor_;
  objective_domain_ = objective_domain_.AdditionWith(Domain(-offset.Min()));

  // Because we can assume that the constraint we used was constraining
  // (otherwise it would have been removed), the objective domain should be now
  // constraining.
  objective_domain_is_constraining_ = true;

  if (objective_domain_.IsEmpty()) {
    return NotifyThatModelIsUnsat();
  }
  return true;
}

bool PresolveContext::ExploitExactlyOneInObjective(
    absl::Span<const int> exactly_one) {
  if (objective_map_.empty()) return false;

  int64_t min_coeff = std::numeric_limits<int64_t>::max();
  for (const int ref : exactly_one) {
    const auto it = objective_map_.find(PositiveRef(ref));
    if (it == objective_map_.end()) return false;

    const int64_t coeff = it->second;
    if (RefIsPositive(ref)) {
      min_coeff = std::min(min_coeff, coeff);
    } else {
      // Objective = coeff * var = coeff * (1 - ref);
      min_coeff = std::min(min_coeff, -coeff);
    }
  }

  int64_t offset = min_coeff;
  for (const int ref : exactly_one) {
    const int var = PositiveRef(ref);
    int64_t& map_ref = objective_map_.at(var);
    if (RefIsPositive(ref)) {
      map_ref -= min_coeff;
      if (map_ref == 0) {
        objective_map_.erase(var);
        var_to_constraints_[var].erase(kObjectiveConstraint);
      }
    } else {
      // Term = coeff * (1 - X) = coeff  - coeff * X;
      // So -coeff -> -coeff  -min_coeff
      // And Term = coeff + min_coeff - min_coeff - (coeff + min_coeff) * X
      //          = (coeff + min_coeff) * (1 - X) - min_coeff;
      map_ref += min_coeff;
      if (map_ref == 0) {
        objective_map_.erase(var);
        var_to_constraints_[var].erase(kObjectiveConstraint);
      }
      offset -= min_coeff;
    }
  }

  // Note that the domain never include the offset, so we need to update it.
  if (offset != 0) {
    objective_offset_ += offset;
    objective_integer_offset_ += offset * objective_integer_scaling_factor_;
    objective_domain_ = objective_domain_.AdditionWith(Domain(-offset));
  }

  return true;
}

void PresolveContext::WriteObjectiveToProto() const {
  // We need to sort the entries to be deterministic.
  std::vector<std::pair<int, int64_t>> entries;
  for (const auto& entry : objective_map_) {
    entries.push_back(entry);
  }
  std::sort(entries.begin(), entries.end());

  CpObjectiveProto* mutable_obj = working_model->mutable_objective();
  mutable_obj->set_offset(objective_offset_);
  mutable_obj->set_scaling_factor(objective_scaling_factor_);
  mutable_obj->set_integer_offset(objective_integer_offset_);
  if (objective_integer_scaling_factor_ == 1) {
    mutable_obj->set_integer_scaling_factor(0);  // Default.
  } else {
    mutable_obj->set_integer_scaling_factor(objective_integer_scaling_factor_);
  }
  FillDomainInProto(objective_domain_, mutable_obj);
  mutable_obj->clear_vars();
  mutable_obj->clear_coeffs();
  for (const auto& entry : entries) {
    mutable_obj->add_vars(entry.first);
    mutable_obj->add_coeffs(entry.second);
  }
}

void PresolveContext::WriteVariableDomainsToProto() const {
  for (int i = 0; i < working_model->variables_size(); ++i) {
    FillDomainInProto(DomainOf(i), working_model->mutable_variables(i));
  }
}

int PresolveContext::GetOrCreateReifiedPrecedenceLiteral(
    const LinearExpressionProto& time_i, const LinearExpressionProto& time_j,
    int active_i, int active_j) {
  CHECK(!LiteralIsFalse(active_i));
  CHECK(!LiteralIsFalse(active_j));
  DCHECK(ExpressionIsAffine(time_i));
  DCHECK(ExpressionIsAffine(time_j));

  const std::tuple<int, int64_t, int, int64_t, int64_t, int, int> key =
      GetReifiedPrecedenceKey(time_i, time_j, active_i, active_j);
  const auto& it = reified_precedences_cache_.find(key);
  if (it != reified_precedences_cache_.end()) return it->second;

  const int result = NewBoolVar();
  reified_precedences_cache_[key] = result;

  // result => (time_i <= time_j) && active_i && active_j.
  ConstraintProto* const lesseq = working_model->add_constraints();
  lesseq->add_enforcement_literal(result);
  if (!IsFixed(time_i)) {
    lesseq->mutable_linear()->add_vars(time_i.vars(0));
    lesseq->mutable_linear()->add_coeffs(-time_i.coeffs(0));
  }
  if (!IsFixed(time_j)) {
    lesseq->mutable_linear()->add_vars(time_j.vars(0));
    lesseq->mutable_linear()->add_coeffs(time_j.coeffs(0));
  }

  const int64_t offset =
      (IsFixed(time_i) ? FixedValue(time_i) : time_i.offset()) -
      (IsFixed(time_j) ? FixedValue(time_j) : time_j.offset());
  lesseq->mutable_linear()->add_domain(offset);
  lesseq->mutable_linear()->add_domain(std::numeric_limits<int64_t>::max());
  if (!LiteralIsTrue(active_i)) {
    AddImplication(result, active_i);
  }
  if (!LiteralIsTrue(active_j)) {
    AddImplication(result, active_j);
  }

  // Not(result) && active_i && active_j => (time_i > time_j)
  ConstraintProto* const greater = working_model->add_constraints();
  if (!IsFixed(time_i)) {
    greater->mutable_linear()->add_vars(time_i.vars(0));
    greater->mutable_linear()->add_coeffs(-time_i.coeffs(0));
  }
  if (!IsFixed(time_j)) {
    greater->mutable_linear()->add_vars(time_j.vars(0));
    greater->mutable_linear()->add_coeffs(time_j.coeffs(0));
  }
  greater->mutable_linear()->add_domain(std::numeric_limits<int64_t>::min());
  greater->mutable_linear()->add_domain(offset - 1);

  // Manages enforcement literal.
  greater->add_enforcement_literal(NegatedRef(result));
  if (!LiteralIsTrue(active_i)) {
    greater->add_enforcement_literal(active_i);
  }
  if (!LiteralIsTrue(active_j)) {
    greater->add_enforcement_literal(active_j);
  }

  // This is redundant but should improves performance.
  //
  // If GetOrCreateReifiedPrecedenceLiteral(time_j, time_i, active_j, active_i)
  // (the reverse precedence) has been called too, then we can link the two
  // precedence literals, and the two active literals together.
  const auto& rev_it = reified_precedences_cache_.find(
      GetReifiedPrecedenceKey(time_j, time_i, active_j, active_i));
  if (rev_it != reified_precedences_cache_.end()) {
    auto* const bool_or = working_model->add_constraints()->mutable_bool_or();
    bool_or->add_literals(result);
    bool_or->add_literals(rev_it->second);
    bool_or->add_literals(NegatedRef(active_i));
    bool_or->add_literals(NegatedRef(active_j));
  }

  return result;
}

std::tuple<int, int64_t, int, int64_t, int64_t, int, int>
PresolveContext::GetReifiedPrecedenceKey(const LinearExpressionProto& time_i,
                                         const LinearExpressionProto& time_j,
                                         int active_i, int active_j) {
  const int var_i =
      IsFixed(time_i) ? std::numeric_limits<int>::min() : time_i.vars(0);
  const int64_t coeff_i = IsFixed(time_i) ? 0 : time_i.coeffs(0);
  const int var_j =
      IsFixed(time_j) ? std::numeric_limits<int>::min() : time_j.vars(0);
  const int64_t coeff_j = IsFixed(time_j) ? 0 : time_j.coeffs(0);
  const int64_t offset =
      (IsFixed(time_i) ? FixedValue(time_i) : time_i.offset()) -
      (IsFixed(time_j) ? FixedValue(time_j) : time_j.offset());
  // In all formulas, active_i and active_j are symmetrical, we can sort the
  // active literals.
  if (active_j < active_i) std::swap(active_i, active_j);
  return std::make_tuple(var_i, coeff_i, var_j, coeff_j, offset, active_i,
                         active_j);
}

void PresolveContext::ClearPrecedenceCache() {
  reified_precedences_cache_.clear();
}

void PresolveContext::LogInfo() {
  SOLVER_LOG(logger_, "");
  SOLVER_LOG(logger_, "Presolve summary:");
  SOLVER_LOG(logger_, "  - ", NumAffineRelations(),
             " affine relations were detected.");
  SOLVER_LOG(logger_, "  - ", NumEquivRelations(),
             " variable equivalence relations were detected.");
  std::map<std::string, int> sorted_rules(stats_by_rule_name_.begin(),
                                          stats_by_rule_name_.end());
  for (const auto& entry : sorted_rules) {
    if (entry.second == 1) {
      SOLVER_LOG(logger_, "  - rule '", entry.first, "' was applied 1 time.");
    } else {
      SOLVER_LOG(logger_, "  - rule '", entry.first, "' was applied ",
                 entry.second, " times.");
    }
  }
}

bool LoadModelForProbing(PresolveContext* context, Model* local_model) {
  if (context->ModelIsUnsat()) return false;

  // Update the domain in the current CpModelProto.
  context->WriteVariableDomainsToProto();
  const CpModelProto& model_proto = *(context->working_model);

  // Load the constraints in a local model.
  //
  // TODO(user): The model we load does not contain affine relations! But
  // ideally we should be able to remove all of them once we allow more complex
  // constraints to contains linear expression.
  //
  // TODO(user): remove code duplication with cp_model_solver. Here we also do
  // not run the heuristic to decide which variable to fully encode.
  //
  // TODO(user): Maybe do not load slow to propagate constraints? for instance
  // we do not use any linear relaxation here.
  Model model;
  local_model->Register<SolverLogger>(context->logger());

  // Adapt some of the parameters during this probing phase.
  auto* local_param = local_model->GetOrCreate<SatParameters>();
  *local_param = context->params();
  local_param->set_use_implied_bounds(false);

  local_model->GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(
      context->time_limit());
  local_model->Register<ModelRandomGenerator>(context->random());
  auto* encoder = local_model->GetOrCreate<IntegerEncoder>();
  encoder->DisableImplicationBetweenLiteral();
  auto* mapping = local_model->GetOrCreate<CpModelMapping>();

  // Important: Because the model_proto do not contains affine relation or the
  // objective, we cannot call DetectOptionalVariables() ! This might wrongly
  // detect optionality and derive bad conclusion.
  LoadVariables(model_proto, /*view_all_booleans_as_integers=*/false,
                local_model);
  ExtractEncoding(model_proto, local_model);
  auto* sat_solver = local_model->GetOrCreate<SatSolver>();
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) continue;
    CHECK(LoadConstraint(ct, local_model));
    if (sat_solver->IsModelUnsat()) {
      return context->NotifyThatModelIsUnsat(absl::StrCat(
          "after loading constraint during probing ", ct.ShortDebugString()));
    }
  }
  encoder->AddAllImplicationsBetweenAssociatedLiterals();
  if (!sat_solver->Propagate()) {
    return context->NotifyThatModelIsUnsat(
        "during probing initial propagation");
  }

  return true;
}

}  // namespace sat
}  // namespace operations_research
