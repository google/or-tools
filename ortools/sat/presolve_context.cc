// Copyright 2010-2024 Google LLC
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
#include <cstdlib>
#include <limits>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/logging.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

ABSL_FLAG(bool, cp_model_debug_postsolve, false,
          "DEBUG ONLY. When set to true, the mapping_model.proto will contains "
          "file:line of the code that created that constraint. This is helpful "
          "for debugging postsolve");

namespace operations_research {
namespace sat {

int SavedLiteral::Get(PresolveContext* context) const {
  return context->GetLiteralRepresentative(ref_);
}

int SavedVariable::Get() const { return ref_; }

void PresolveContext::ClearStats() { stats_by_rule_name_.clear(); }

int PresolveContext::NewIntVar(const Domain& domain) {
  IntegerVariableProto* const var = working_model->add_variables();
  FillDomainInProto(domain, var);
  InitializeNewDomains();
  return working_model->variables_size() - 1;
}

int PresolveContext::NewIntVarWithDefinition(
    const Domain& domain, absl::Span<const std::pair<int, int64_t>> definition,
    bool append_constraint_to_mapping_model) {
  if (domain.Size() == 1) {
    UpdateRuleStats("TODO new_var_definition : use boolean equation");
  }

  const int new_var = NewIntVar(domain);

  // Create new linear constraint new_var = definition.
  // TODO(user): When we encounter overflow (rare), we still create a variable.
  auto* new_linear = append_constraint_to_mapping_model
                         ? mapping_model->add_constraints()->mutable_linear()
                         : working_model->add_constraints()->mutable_linear();
  for (const auto [var, coeff] : definition) {
    new_linear->add_vars(var);
    new_linear->add_coeffs(coeff);
  }
  new_linear->add_vars(new_var);
  new_linear->add_coeffs(-1);
  new_linear->add_domain(0);
  new_linear->add_domain(0);
  if (PossibleIntegerOverflow(*working_model, new_linear->vars(),
                              new_linear->coeffs())) {
    UpdateRuleStats("TODO new_var_definition : possible overflow.");
    if (append_constraint_to_mapping_model) {
      mapping_model->mutable_constraints()->RemoveLast();
    } else {
      working_model->mutable_constraints()->RemoveLast();
    }
    return -1;
  }
  if (!append_constraint_to_mapping_model) {
    UpdateNewConstraintsVariableUsage();
  }

  // We only fill the hint of the new variable if all the variable involved
  // in its definition have a value.
  if (hint_is_loaded_) {
    int64_t new_value = 0;
    for (const auto [var, coeff] : definition) {
      CHECK_GE(var, 0);
      CHECK_LE(var, hint_.size());
      if (!hint_has_value_[var]) return new_var;
      new_value += coeff * hint_[var];
    }
    hint_has_value_[new_var] = true;
    hint_[new_var] = new_value;
  }
  return new_var;
}

int PresolveContext::NewBoolVar(absl::string_view source) {
  UpdateRuleStats(absl::StrCat("new_bool: ", source));
  return NewIntVar(Domain(0, 1));
}

int PresolveContext::NewBoolVarWithClause(absl::Span<const int> clause) {
  const int new_var = NewBoolVar("with clause");
  if (hint_is_loaded_) {
    bool all_have_hint = true;
    for (const int literal : clause) {
      const int var = PositiveRef(literal);
      if (!hint_has_value_[var]) {
        all_have_hint = false;
        continue;
      }
      if (RefIsPositive(literal)) {
        if (hint_[var] == 1) {
          hint_has_value_[new_var] = true;
          hint_[new_var] = 1;
          break;
        }
      } else {
        if (hint_[var] == 0) {
          hint_has_value_[new_var] = true;
          hint_[new_var] = 1;
          break;
        }
      }
    }

    // If all literals where hinted and at zero, we set the hint of
    // new_var to zero, otherwise we leave it unassigned.
    if (all_have_hint && !hint_has_value_[new_var]) {
      hint_has_value_[new_var] = true;
      hint_[new_var] = 0;
    }
  }
  return new_var;
}

int PresolveContext::GetTrueLiteral() {
  if (!true_literal_is_defined_) {
    true_literal_is_defined_ = true;
    true_literal_ = NewIntVar(Domain(1));
  }
  return true_literal_;
}

int PresolveContext::GetFalseLiteral() { return NegatedRef(GetTrueLiteral()); }

// a => b.
void PresolveContext::AddImplication(int a, int b) {
  if (a == b) return;
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
  if (!IsFixed(proto.interval().start())) return false;
  if (!IsFixed(proto.interval().size())) return false;
  if (!IsFixed(proto.interval().end())) return false;
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

// Tricky: If this variable is equivalent to another one (but not the
// representative) and appear in just one constraint, then this constraint must
// be the affine defining one. And in this case the code using this function
// should do the proper stuff.
bool PresolveContext::VariableIsUnique(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  const int var = PositiveRef(ref);
  return var_to_constraints_[var].size() == 1;
}

bool PresolveContext::VariableIsUniqueAndRemovable(int ref) const {
  return !params_.keep_all_feasible_solutions_in_presolve() &&
         VariableIsUnique(ref);
}

bool PresolveContext::VariableWithCostIsUnique(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  const int var = PositiveRef(ref);
  return var_to_constraints_[var].size() == 2 &&
         var_to_constraints_[var].contains(kObjectiveConstraint);
}

// Tricky: Same remark as for VariableIsUniqueAndRemovable().
//
// Also if the objective domain is constraining, we can't have a preferred
// direction, so we cannot easily remove such variable.
bool PresolveContext::VariableWithCostIsUniqueAndRemovable(int ref) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  const int var = PositiveRef(ref);
  return !params_.keep_all_feasible_solutions_in_presolve() &&
         !objective_domain_is_constraining_ && VariableWithCostIsUnique(var);
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
      SOLVER_LOG(logger_, "constraint #", c, " : ",
                 c >= 0
                     ? ProtobufShortDebugString(working_model->constraints(c))
                     : "");
    }
  }
  return true;
}

bool PresolveContext::VariableIsOnlyUsedInEncodingAndMaybeInObjective(
    int var) const {
  CHECK(RefIsPositive(var));
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  if (var_to_num_linear1_[var] == 0) return false;
  return var_to_num_linear1_[var] == var_to_constraints_[var].size() ||
         (var_to_constraints_[var].contains(kObjectiveConstraint) &&
          var_to_num_linear1_[var] + 1 == var_to_constraints_[var].size());
}

bool PresolveContext::VariableIsOnlyUsedInLinear1AndOneExtraConstraint(
    int var) const {
  if (!ConstraintVariableGraphIsUpToDate()) return false;
  if (var_to_num_linear1_[var] == 0) return false;
  CHECK(RefIsPositive(var));
  return var_to_num_linear1_[var] + 1 == var_to_constraints_[var].size();
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
  if (value > MaxOf(expr)) return false;
  if (value < MinOf(expr)) return false;

  // We assume expression is validated for overflow initially, and the code
  // below should be overflow safe.
  if ((value - expr.offset()) % expr.coeffs(0) != 0) return false;
  return DomainContains(expr.vars(0), (value - expr.offset()) / expr.coeffs(0));
}

ABSL_MUST_USE_RESULT bool PresolveContext::IntersectDomainWithInternal(
    int ref, const Domain& domain, bool* domain_modified, bool update_hint) {
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
    return NotifyThatModelIsUnsat(
        absl::StrCat("var #", ref, " as empty domain after intersecting with ",
                     domain.ToString()));
  }

  if (update_hint && VarHasSolutionHint(var)) {
    UpdateVarSolutionHint(var, domains[var].ClosestValue(SolutionHint(var)));
  }

#ifdef CHECK_HINT
  if (working_model->has_solution_hint() && HintIsLoaded() &&
      !domains[var].Contains(hint_[var])) {
    LOG(FATAL) << "Hint with value " << hint_[var]
               << " infeasible when changing domain of " << var << " to "
               << domains[var];
  }
#endif

  // Propagate the domain of the representative right away.
  // Note that the recursive call should only by one level deep.
  const AffineRelation::Relation r = GetAffineRelation(var);
  if (r.representative == var) return true;
  return IntersectDomainWithInternal(r.representative,
                                     DomainOf(var)
                                         .AdditionWith(Domain(-r.offset))
                                         .InverseMultiplicationBy(r.coeff),
                                     /*domain_modified=*/nullptr, update_hint);
}

ABSL_MUST_USE_RESULT bool PresolveContext::IntersectDomainWith(
    const LinearExpressionProto& expr, const Domain& domain,
    bool* domain_modified) {
  if (expr.vars().empty()) {
    if (domain.Contains(expr.offset())) {
      return true;
    } else {
      return NotifyThatModelIsUnsat(absl::StrCat(
          ProtobufShortDebugString(expr),
          " as empty domain after intersecting with ", domain.ToString()));
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
  // Hack: we don't want to count TODO rules as this is used to decide if
  // we loop again.
  const bool is_todo = name.size() >= 4 && name.substr(0, 4) == "TODO";
  if (!is_todo) num_presolve_operations += num_times;

  if (logger_->LoggingIsEnabled()) {
    if (VLOG_IS_ON(1)) {
      int level = is_todo ? 3 : 2;
      if (std::abs(num_presolve_operations -
                   params_.debug_max_num_presolve_operations()) <= 100) {
        level = 1;
      }
      VLOG(level) << num_presolve_operations << " : " << name;
    }

    stats_by_rule_name_[name] += num_times;
  }
}

void PresolveContext::UpdateLinear1Usage(const ConstraintProto& ct, int c) {
  const int old_var = constraint_to_linear1_var_[c];
  if (old_var >= 0) {
    var_to_num_linear1_[old_var]--;
    DCHECK_GE(var_to_num_linear1_[old_var], 0);
  }
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear &&
      ct.linear().vars().size() == 1) {
    const int var = PositiveRef(ct.linear().vars(0));
    constraint_to_linear1_var_[c] = var;
    var_to_num_linear1_[var]++;
  } else {
    constraint_to_linear1_var_[c] = -1;
  }
}

void PresolveContext::MaybeResizeIntervalData() {
  // Lazy allocation so that we only do that if there are some interval.
  const int num_constraints = constraint_to_vars_.size();
  if (constraint_to_intervals_.size() != num_constraints) {
    constraint_to_intervals_.resize(num_constraints);
    interval_usage_.resize(num_constraints);
  }
}

void PresolveContext::AddVariableUsage(int c) {
  const ConstraintProto& ct = working_model->constraints(c);

  constraint_to_vars_[c] = UsedVariables(ct);
  for (const int v : constraint_to_vars_[c]) {
    DCHECK_LT(v, var_to_constraints_.size());
    DCHECK(!VariableWasRemoved(v));
    var_to_constraints_[v].insert(c);
  }

  std::vector<int> used_interval = UsedIntervals(ct);
  if (!used_interval.empty()) {
    MaybeResizeIntervalData();
    constraint_to_intervals_[c].swap(used_interval);
    for (const int i : constraint_to_intervals_[c]) interval_usage_[i]++;
  }

  UpdateLinear1Usage(ct, c);

#ifdef CHECK_HINT
  // Crash if the loaded hint is infeasible for this constraint.
  // This is helpful to debug a wrong presolve that kill a feasible solution.
  if (working_model->has_solution_hint() && HintIsLoaded() &&
      !ConstraintIsFeasible(*working_model, ct, hint_)) {
    LOG(FATAL) << "Hint infeasible for constraint #" << c << " : "
               << ct.ShortDebugString();
  }
#endif
}

void PresolveContext::EraseFromVarToConstraint(int var, int c) {
  var_to_constraints_[var].erase(c);
  if (var_to_constraints_[var].size() <= 3) {
    var_with_reduced_small_degree.Set(var);
  }
}

void PresolveContext::UpdateConstraintVariableUsage(int c) {
  if (is_unsat_) return;
  DCHECK_EQ(constraint_to_vars_.size(), working_model->constraints_size());
  const ConstraintProto& ct = working_model->constraints(c);

  // We don't optimize the interval usage as this is not super frequent.
  std::vector<int> used_interval = UsedIntervals(ct);
  if (c < constraint_to_intervals_.size() || !used_interval.empty()) {
    MaybeResizeIntervalData();
    for (const int i : constraint_to_intervals_[c]) interval_usage_[i]--;
    constraint_to_intervals_[c].swap(used_interval);
    for (const int i : constraint_to_intervals_[c]) interval_usage_[i]++;
  }

  // For the variables, we avoid an erase() followed by an insert() for the
  // variables that didn't change.
  std::vector<int> new_usage = UsedVariables(ct);
  const absl::Span<const int> old_usage = constraint_to_vars_[c];
  const int old_size = old_usage.size();
  int i = 0;
  for (const int var : new_usage) {
    DCHECK(!VariableWasRemoved(var));
    while (i < old_size && old_usage[i] < var) {
      EraseFromVarToConstraint(old_usage[i], c);
      ++i;
    }
    if (i < old_size && old_usage[i] == var) {
      ++i;
    } else {
      var_to_constraints_[var].insert(c);
    }
  }
  for (; i < old_size; ++i) {
    EraseFromVarToConstraint(old_usage[i], c);
  }
  constraint_to_vars_[c].swap(new_usage);

  UpdateLinear1Usage(ct, c);

#ifdef CHECK_HINT
  // Crash if the loaded hint is infeasible for this constraint.
  // This is helpful to debug a wrong presolve that kill a feasible solution.
  if (working_model->has_solution_hint() && HintIsLoaded() &&
      !ConstraintIsFeasible(*working_model, ct, hint_)) {
    LOG(FATAL) << "Hint infeasible for constraint #" << c << " : "
               << ct.ShortDebugString();
  }
#endif
}

bool PresolveContext::ConstraintVariableGraphIsUpToDate() const {
  if (is_unsat_) return true;  // We do not care in this case.
  return constraint_to_vars_.size() == working_model->constraints_size();
}

void PresolveContext::UpdateNewConstraintsVariableUsage() {
  if (is_unsat_) return;
  const int old_size = constraint_to_vars_.size();
  const int new_size = working_model->constraints_size();
  DCHECK_LE(old_size, new_size);
  constraint_to_vars_.resize(new_size);
  constraint_to_linear1_var_.resize(new_size, -1);
  for (int c = old_size; c < new_size; ++c) {
    AddVariableUsage(c);
  }
}

bool PresolveContext::HasUnusedAffineVariable() const {
  if (is_unsat_) return false;  // We do not care in this case.
  if (params_.keep_all_feasible_solutions_in_presolve()) return false;

  // We can leave non-optimal stuff around if we reach the time limit.
  if (time_limit_->LimitReached()) return false;

  for (int var = 0; var < working_model->variables_size(); ++var) {
    if (VariableIsNotUsedAnymore(var)) continue;
    if (IsFixed(var)) continue;
    const auto& constraints = VarToConstraints(var);
    if (constraints.size() == 1 &&
        constraints.contains(kAffineRelationConstraint) &&
        GetAffineRelation(var).representative != var) {
      return true;
    }
  }
  return false;
}

// TODO(user): Also test var_to_constraints_ !!
bool PresolveContext::ConstraintVariableUsageIsConsistent() {
  if (is_unsat_) return true;  // We do not care in this case.
  if (var_to_constraints_.size() != working_model->variables_size()) {
    LOG(INFO) << "Wrong var_to_constraints_ size!";
    return false;
  }
  if (constraint_to_vars_.size() != working_model->constraints_size()) {
    LOG(INFO) << "Wrong constraint_to_vars size!";
    return false;
  }
  std::vector<int> linear1_count(var_to_constraints_.size(), 0);
  for (int c = 0; c < constraint_to_vars_.size(); ++c) {
    const ConstraintProto& ct = working_model->constraints(c);
    if (constraint_to_vars_[c] != UsedVariables(ct)) {
      LOG(INFO) << "Wrong variables usage for constraint: \n"
                << ProtobufDebugString(ct)
                << " old_size: " << constraint_to_vars_[c].size();
      return false;
    }
    if (ct.constraint_case() == ConstraintProto::kLinear &&
        ct.linear().vars().size() == 1) {
      linear1_count[PositiveRef(ct.linear().vars(0))]++;
      if (constraint_to_linear1_var_[c] != PositiveRef(ct.linear().vars(0))) {
        LOG(INFO) << "Wrong variables for linear1: \n"
                  << ProtobufDebugString(ct)
                  << " saved_var: " << constraint_to_linear1_var_[c];
        return false;
      }
    }
  }
  int num_in_objective = 0;
  for (int v = 0; v < var_to_constraints_.size(); ++v) {
    if (linear1_count[v] != var_to_num_linear1_[v]) {
      LOG(INFO) << "Variable " << v << " has wrong linear1 count!"
                << " stored: " << var_to_num_linear1_[v]
                << " actual: " << linear1_count[v];
      return false;
    }
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

bool PresolveContext::PropagateAffineRelation(int var) {
  DCHECK(RefIsPositive(var));
  const AffineRelation::Relation r = GetAffineRelation(var);
  if (r.representative == var) return true;
  return PropagateAffineRelation(var, r.representative, r.coeff, r.offset);
}

bool PresolveContext::PropagateAffineRelation(int var, int rep, int64_t coeff,
                                              int64_t offset) {
  DCHECK(RefIsPositive(var));
  DCHECK(RefIsPositive(rep));
  DCHECK(!DomainIsEmpty(var));
  DCHECK(!DomainIsEmpty(rep));

  // Propagate domains both ways.
  // var = coeff * rep + offset
  if (!IntersectDomainWith(rep, DomainOf(var)
                                    .AdditionWith(Domain(-offset))
                                    .InverseMultiplicationBy(coeff))) {
    return false;
  }
  if (!IntersectDomainWith(
          var,
          DomainOf(rep).MultiplicationBy(coeff).AdditionWith(Domain(offset)))) {
    return false;
  }

  return true;
}

void PresolveContext::RemoveAllVariablesFromAffineRelationConstraint() {
  for (auto& ref_map : var_to_constraints_) {
    ref_map.erase(kAffineRelationConstraint);
  }
}

void PresolveContext::RemoveNonRepresentativeAffineVariableIfUnused(int var) {
  if (!VariableIsUnique(var)) {
    return;
  }
  const AffineRelation::Relation r = GetAffineRelation(var);
  if (var == r.representative) {
    return;
  }
  DCHECK(VarToConstraints(var).contains(kAffineRelationConstraint));
  DCHECK(!VariableIsNotUsedAnymore(r.representative));
  // Add relation with current representative to the mapping model.
  ConstraintProto* ct = NewMappingConstraint(__FILE__, __LINE__);
  auto* arg = ct->mutable_linear();
  arg->add_vars(var);
  arg->add_coeffs(1);
  arg->add_vars(r.representative);
  arg->add_coeffs(-r.coeff);
  arg->add_domain(r.offset);
  arg->add_domain(r.offset);
  RemoveVariableFromAffineRelation(var);
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

  // We do not call EraseFromVarToConstraint() on purpose here since the
  // variable is removed.
  var_to_constraints_[var].erase(kAffineRelationConstraint);
  affine_relations_.IgnoreFromClassSize(var);

  // If the representative is left alone, we can remove it from the special
  // affine relation constraint too.
  if (affine_relations_.ClassSize(rep) == 1) {
    EraseFromVarToConstraint(rep, kAffineRelationConstraint);
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
  if (!working_model->variables(var).name().empty()) {
    working_model->mutable_variables(new_var)->set_name(
        working_model->variables(var).name());
  }
  CHECK(StoreAffineRelation(var, new_var, mod, offset + mod * min_value,
                            /*debug_no_recursion=*/true));
  UpdateRuleStats("variables: canonicalize affine domain");
  UpdateNewConstraintsVariableUsage();
  return true;
}

void PresolveContext::PermuteHintValues(const SparsePermutation& perm) {
  CHECK(hint_is_loaded_);
  perm.ApplyToDenseCollection(hint_);
  perm.ApplyToDenseCollection(hint_has_value_);
}

bool PresolveContext::StoreAffineRelation(int var_x, int var_y, int64_t coeff,
                                          int64_t offset,
                                          bool debug_no_recursion) {
  DCHECK(RefIsPositive(var_x));
  DCHECK(RefIsPositive(var_y));
  DCHECK_NE(coeff, 0);
  if (is_unsat_) return false;

  if (hint_is_loaded_) {
    if (!hint_has_value_[var_y] && hint_has_value_[var_x]) {
      hint_has_value_[var_y] = true;
      hint_[var_y] = (hint_[var_x] - offset) / coeff;
      if (hint_[var_y] * coeff + offset != hint_[var_x]) {
        // TODO(user): Do we implement a rounding to closest instead of
        // routing towards 0.
        UpdateRuleStats(
            "Warning: hint didn't satisfy affine relation and was corrected");
      }
    }
  }

#ifdef CHECK_HINT
  const int64_t vx = hint_[var_x];
  const int64_t vy = hint_[var_y];
  if (working_model->has_solution_hint() && HintIsLoaded() &&
      vx != vy * coeff + offset) {
    LOG(FATAL) << "Affine relation incompatible with hint: " << vx
               << " != " << vy << " * " << coeff << " + " << offset;
  }
#endif

  // TODO(user): I am not 100% sure why, but sometimes the representative is
  // fixed but that is not propagated to var_x or var_y and this causes issues.
  if (!PropagateAffineRelation(var_x)) return false;
  if (!PropagateAffineRelation(var_y)) return false;
  if (!PropagateAffineRelation(var_x, var_y, coeff, offset)) return false;

  if (IsFixed(var_x)) {
    const int64_t lhs = DomainOf(var_x).FixedValue() - offset;
    if (lhs % std::abs(coeff) != 0) {
      return NotifyThatModelIsUnsat();
    }
    UpdateRuleStats("affine: fixed");
    return IntersectDomainWith(var_y, Domain(lhs / coeff));
  }

  if (IsFixed(var_y)) {
    const int64_t value_x = DomainOf(var_y).FixedValue() * coeff + offset;
    UpdateRuleStats("affine: fixed");
    return IntersectDomainWith(var_x, Domain(value_x));
  }

  // If both are already in the same class, we need to make sure the relations
  // are compatible.
  const AffineRelation::Relation rx = GetAffineRelation(var_x);
  const AffineRelation::Relation ry = GetAffineRelation(var_y);
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
    if (!IntersectDomainWith(var_x,
                             Domain(unique_value * rx.coeff + rx.offset))) {
      return false;
    }
    if (!IntersectDomainWith(var_y,
                             Domain(unique_value * ry.coeff + ry.offset))) {
      return false;
    }
    return true;
  }

  // var_x = coeff * var_y + offset;
  // rx.coeff * rep_x + rx.offset =
  //    coeff * (ry.coeff * rep_y + ry.offset) + offset
  //
  // We have a * rep_x + b * rep_y == o
  int64_t a = rx.coeff;
  int64_t b = -coeff * ry.coeff;
  int64_t o = coeff * ry.offset + offset - rx.offset;
  CHECK_NE(a, 0);
  CHECK_NE(b, 0);
  {
    const int64_t gcd = std::gcd(std::abs(a), std::abs(b));
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
    return StoreAffineRelation(var_x, var_y, coeff, offset,
                               /*debug_no_recursion=*/true);
  }

  // Canonicalize from (a * rep_x + b * rep_y = o) to (x = c * y + o).
  int x, y;
  int64_t c;
  bool negate = false;
  if (std::abs(a) == 1) {
    x = rx.representative;
    y = ry.representative;
    c = -b;
    negate = a < 0;
  } else {
    CHECK_EQ(std::abs(b), 1);
    x = ry.representative;
    y = rx.representative;
    c = -a;
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
  UpdateRuleStats("affine: new relation");

  // Lets propagate again the new relation. We might as well do it as early
  // as possible and not all call site do it.
  //
  // TODO(user): I am not sure this is needed given the propagation above.
  if (!PropagateAffineRelation(var_x)) return false;
  if (!PropagateAffineRelation(var_y)) return false;

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

// This makes sure that the affine relation only uses one of the
// representative from the var_equiv_relations_.
AffineRelation::Relation PresolveContext::GetAffineRelation(int ref) const {
  AffineRelation::Relation r = affine_relations_.Get(PositiveRef(ref));
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
  const int new_size = working_model->variables().size();
  DCHECK_GE(new_size, domains.size());
  if (domains.size() == new_size) return;

  modified_domains.Resize(new_size);
  var_with_reduced_small_degree.Resize(new_size);
  var_to_constraints_.resize(new_size);
  var_to_num_linear1_.resize(new_size);

  // We mark the domain as modified so we will look at these new variable during
  // our presolve loop.
  for (int i = domains.size(); i < new_size; ++i) {
    modified_domains.Set(i);
    domains.emplace_back(ReadDomainFromProto(working_model->variables(i)));
    if (domains.back().IsEmpty()) {
      is_unsat_ = true;
      return;
    }
  }

  // We resize the hint too even if not loaded.
  hint_.resize(new_size, 0);
  hint_has_value_.resize(new_size, false);
}

void PresolveContext::LoadSolutionHint() {
  CHECK(!hint_is_loaded_);
  hint_is_loaded_ = true;
  if (working_model->has_solution_hint()) {
    const auto hint_proto = working_model->solution_hint();
    const int num_terms = hint_proto.vars().size();
    int num_changes = 0;
    for (int i = 0; i < num_terms; ++i) {
      const int var = hint_proto.vars(i);
      if (!RefIsPositive(var)) break;  // Abort. Shouldn't happen.
      if (var < hint_.size()) {
        hint_has_value_[var] = true;
        const int64_t hint_value = hint_proto.values(i);
        hint_[var] = DomainOf(var).ClosestValue(hint_value);
        if (hint_[var] != hint_value) {
          ++num_changes;
        }
      }
    }
    if (num_changes > 0) {
      UpdateRuleStats("hint: moved var hint within its domain.", num_changes);
    }
    for (int i = 0; i < hint_.size(); ++i) {
      if (hint_has_value_[i]) continue;
      if (IsFixed(i)) {
        hint_has_value_[i] = true;
        hint_[i] = FixedValue(i);
      }
    }
  }
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
      if (!StoreBooleanEqualityRelation(min_literal, NegatedRef(max_literal))) {
        return;
      }
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
    max_literal = NewBoolVar("var with 2 values");
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

bool PresolveContext::InsertVarValueEncodingInternal(int literal, int var,
                                                     int64_t value,
                                                     bool add_constraints) {
  DCHECK(RefIsPositive(var));
  DCHECK(!VariableWasRemoved(literal));
  DCHECK(!VariableWasRemoved(var));
  if (is_unsat_) return false;
  absl::flat_hash_map<int64_t, SavedLiteral>& var_map = encoding_[var];

  // The code below is not 100% correct if this is not the case.
  CHECK(DomainOf(var).Contains(value));
  if (DomainOf(var).IsFixed()) {
    return SetLiteralToTrue(literal);
  }
  if (LiteralIsTrue(literal)) {
    return IntersectDomainWith(var, Domain(value));
  }
  if (LiteralIsFalse(literal)) {
    return IntersectDomainWith(var, Domain(value).Complement());
  }

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
        if (!StoreBooleanEqualityRelation(literal, previous_literal)) {
          return false;
        }
      }
    }
    return true;
  }

  if (DomainOf(var).Size() == 2) {
    if (!CanBeUsedAsLiteral(var)) {
      // TODO(user): There is a bug here if the var == value was not in the
      // domain, it will just be ignored.
      CanonicalizeDomainOfSizeTwo(var);
      if (is_unsat_) return false;

      if (IsFixed(var)) {
        if (FixedValue(var) == value) {
          return SetLiteralToTrue(literal);
        } else {
          return SetLiteralToFalse(literal);
        }
      }

      // We should have a Boolean now.
      CanonicalizeEncoding(&var, &value);
    }

    CHECK(CanBeUsedAsLiteral(var));
    if (value == 0) {
      if (!StoreBooleanEqualityRelation(literal, NegatedRef(var))) {
        return false;
      }
    } else {
      CHECK_EQ(value, 1);
      if (!StoreBooleanEqualityRelation(literal, var)) return false;
    }
  } else if (add_constraints) {
    UpdateRuleStats("variables: add encoding constraint");
    AddImplyInDomain(literal, var, Domain(value));
    AddImplyInDomain(NegatedRef(literal), var, Domain(value).Complement());
  }

  // The canonicalization might have proven UNSAT.
  return !ModelIsUnsat();
}

bool PresolveContext::InsertHalfVarValueEncoding(int literal, int var,
                                                 int64_t value, bool imply_eq) {
  if (is_unsat_) return false;
  DCHECK(RefIsPositive(var));

  // Creates the linking sets on demand.
  // Insert the enforcement literal in the half encoding map.
  auto& direct_set = imply_eq ? eq_half_encoding_ : neq_half_encoding_;
  auto insert_result = direct_set.insert({{literal, var}, value});
  if (!insert_result.second) {
    if (insert_result.first->second != value && imply_eq) {
      UpdateRuleStats("variables: detect half reified incompatible value");
      return SetLiteralToFalse(literal);
    }
    return false;  // Already there.
  }
  if (imply_eq) {
    // We are adding b => x=v. Check if we already have ~b => x=u.
    auto negated_encoding = direct_set.find({NegatedRef(literal), var});
    if (negated_encoding != direct_set.end()) {
      if (negated_encoding->second == value) {
        UpdateRuleStats(
            "variables: both boolean and its negation imply same equality");
        if (!IntersectDomainWith(var, Domain(value))) {
          return false;
        }
      } else {
        const int64_t other_value = negated_encoding->second;
        // b => var == value
        // !b => var == other_value
        // var = (value - other_value) * b + other_value
        UpdateRuleStats(
            "variables: both boolean and its negation fix the same variable");
        if (RefIsPositive(literal)) {
          StoreAffineRelation(var, literal, value - other_value, other_value);
        } else {
          StoreAffineRelation(var, NegatedRef(literal), other_value - value,
                              value);
        }
      }
    }
  }
  VLOG(2) << "Collect lit(" << literal << ") implies var(" << var
          << (imply_eq ? ") == " : ") != ") << value;
  UpdateRuleStats("variables: detect half reified value encoding");

  // Note(user): We don't expect a lot of literals in these sets, so doing
  // a scan should be okay.
  auto& other_set = imply_eq ? neq_half_encoding_ : eq_half_encoding_;
  auto it = other_set.find({NegatedRef(literal), var});
  if (it != other_set.end() && it->second == value) {
    UpdateRuleStats("variables: detect fully reified value encoding");
    const int imply_eq_literal = imply_eq ? literal : NegatedRef(literal);
    if (!InsertVarValueEncodingInternal(imply_eq_literal, var, value,
                                        /*add_constraints=*/false)) {
      return false;
    }
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

bool PresolveContext::InsertVarValueEncoding(int literal, int var,
                                             int64_t value) {
  if (!CanonicalizeEncoding(&var, &value) || !DomainOf(var).Contains(value)) {
    return SetLiteralToFalse(literal);
  }
  literal = GetLiteralRepresentative(literal);
  if (!InsertVarValueEncodingInternal(literal, var, value,
                                      /*add_constraints=*/true)) {
    return false;
  }

  eq_half_encoding_.insert({{literal, var}, value});
  neq_half_encoding_.insert({{NegatedRef(literal), var}, value});

  if (hint_is_loaded_) {
    const int bool_var = PositiveRef(literal);
    DCHECK(RefIsPositive(var));
    if (!hint_has_value_[bool_var] && hint_has_value_[var]) {
      const int64_t bool_value = hint_[var] == value ? 1 : 0;
      hint_has_value_[bool_var] = true;
      hint_[bool_var] = RefIsPositive(literal) ? bool_value : 1 - bool_value;
    }
  }
  return true;
}

bool PresolveContext::StoreLiteralImpliesVarEqValue(int literal, int var,
                                                    int64_t value) {
  if (!CanonicalizeEncoding(&var, &value) || !DomainOf(var).Contains(value)) {
    // The literal cannot be true.
    return SetLiteralToFalse(literal);
  }
  literal = GetLiteralRepresentative(literal);
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/true);
}

bool PresolveContext::StoreLiteralImpliesVarNEqValue(int literal, int var,
                                                     int64_t value) {
  if (!CanonicalizeEncoding(&var, &value) || !DomainOf(var).Contains(value)) {
    // The constraint is trivial.
    return false;
  }
  literal = GetLiteralRepresentative(literal);
  return InsertHalfVarValueEncoding(literal, var, value, /*imply_eq=*/false);
}

bool PresolveContext::HasVarValueEncoding(int ref, int64_t value,
                                          int* literal) {
  CHECK(!VariableWasRemoved(ref));
  if (!CanonicalizeEncoding(&ref, &value)) return false;
  DCHECK(RefIsPositive(ref));

  if (!DomainOf(ref).Contains(value)) {
    if (literal != nullptr) *literal = GetFalseLiteral();
    return true;
  }

  if (CanBeUsedAsLiteral(ref)) {
    if (literal != nullptr) {
      *literal = value == 1 ? ref : NegatedRef(ref);
    }
    return true;
  }

  const auto first_it = encoding_.find(ref);
  if (first_it == encoding_.end()) return false;
  const auto it = first_it->second.find(value);
  if (it == first_it->second.end()) return false;

  if (VariableWasRemoved(it->second.Get(this))) return false;
  if (literal != nullptr) {
    *literal = it->second.Get(this);
  }
  return true;
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
  if (!CanonicalizeEncoding(&ref, &value)) return GetFalseLiteral();

  // Positive after CanonicalizeEncoding().
  const int var = ref;

  // Returns the false literal if the value is not in the domain.
  if (!domains[var].Contains(value)) {
    return GetFalseLiteral();
  }

  // Return the literal itself if this was called or canonicalized to a Boolean.
  if (CanBeUsedAsLiteral(ref)) {
    return value == 1 ? ref : NegatedRef(ref);
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
    const int true_literal = GetTrueLiteral();
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
      const int literal = NewBoolVar("integer encoding");
      InsertVarValueEncoding(literal, var, var_max);
      const int representative = GetLiteralRepresentative(literal);
      return value == var_max ? representative : NegatedRef(representative);
    }
  }

  const int literal = NewBoolVar("integer encoding");
  InsertVarValueEncoding(literal, var, value);
  return GetLiteralRepresentative(literal);
}

int PresolveContext::GetOrCreateAffineValueEncoding(
    const LinearExpressionProto& expr, int64_t value) {
  DCHECK_LE(expr.vars_size(), 1);
  if (IsFixed(expr)) {
    if (FixedValue(expr) == value) {
      return GetTrueLiteral();
    } else {
      return GetFalseLiteral();
    }
  }

  if ((value - expr.offset()) % expr.coeffs(0) != 0) {
    return GetFalseLiteral();
  }

  return GetOrCreateVarValueEncoding(expr.vars(0),
                                     (value - expr.offset()) / expr.coeffs(0));
}

void PresolveContext::ReadObjectiveFromProto() {
  const CpObjectiveProto& obj = working_model->objective();

  // We do some small canonicalization here
  objective_proto_is_up_to_date_ = false;

  objective_offset_ = obj.offset();
  objective_scaling_factor_ = obj.scaling_factor();
  if (objective_scaling_factor_ == 0.0) {
    objective_scaling_factor_ = 1.0;
  }

  objective_integer_before_offset_ = obj.integer_before_offset();
  objective_integer_after_offset_ = obj.integer_after_offset();
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
  objective_overflow_detection_ = std::abs(objective_integer_before_offset_);

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
      RemoveVariableFromObjective(var);
    } else {
      var_to_constraints_[var].insert(kObjectiveConstraint);
    }
  }
}

bool PresolveContext::CanonicalizeOneObjectiveVariable(int var) {
  const auto it = objective_map_.find(var);
  if (it == objective_map_.end()) return true;
  const int64_t coeff = it->second;

  // If a variable only appear in objective, we can fix it!
  // Note that we don't care if it was in affine relation, because if none
  // of the relations are left, then we can still fix it.
  if (params_.cp_model_presolve() &&
      !params_.keep_all_feasible_solutions_in_presolve() &&
      !objective_domain_is_constraining_ &&
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
    AddToObjectiveOffset(coeff * MinOf(var));
    RemoveVariableFromObjective(var);
    return true;
  }

  const AffineRelation::Relation r = GetAffineRelation(var);
  if (r.representative == var) return true;

  RemoveVariableFromObjective(var);

  // After we removed the variable from the objective it might have become a
  // unused affine. Add it to the list of variables to check so we reprocess it.
  modified_domains.Set(var);

  // Do the substitution.
  AddToObjectiveOffset(coeff * r.offset);
  const int64_t new_coeff = objective_map_[r.representative] += coeff * r.coeff;

  // Process new term.
  if (new_coeff == 0) {
    RemoveVariableFromObjective(r.representative);
  } else {
    var_to_constraints_[r.representative].insert(kObjectiveConstraint);
    if (IsFixed(r.representative)) {
      RemoveVariableFromObjective(r.representative);
      AddToObjectiveOffset(new_coeff * MinOf(r.representative));
    }
  }
  return true;
}

bool PresolveContext::CanonicalizeObjective(bool simplify_domain) {
  objective_proto_is_up_to_date_ = false;

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
    if (!CanonicalizeOneObjectiveVariable(entry.first)) {
      return NotifyThatModelIsUnsat("canonicalize objective one term");
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
    gcd = std::gcd(gcd, std::abs(coeff));
    implied_domain =
        implied_domain.AdditionWith(DomainOf(var).MultiplicationBy(coeff))
            .RelaxIfTooComplex();
  }

  // This is the new domain.
  // Note that the domain never include the offset.
  objective_domain_ = objective_domain_.IntersectionWith(implied_domain);

  // Depending on the use case, we cannot do that.
  if (simplify_domain) {
    objective_domain_ =
        objective_domain_.SimplifyUsingImpliedDomain(implied_domain);
  }

  // Maybe divide by GCD.
  if (gcd > 1) {
    for (auto& entry : objective_map_) {
      entry.second /= gcd;
    }
    objective_domain_ = objective_domain_.InverseMultiplicationBy(gcd);
    if (objective_domain_.IsEmpty()) {
      return NotifyThatModelIsUnsat("empty objective domain");
    }

    objective_offset_ /= static_cast<double>(gcd);
    objective_scaling_factor_ *= static_cast<double>(gcd);

    // We update the offset accordingly.
    absl::int128 offset = absl::int128(objective_integer_before_offset_) *
                              absl::int128(objective_integer_scaling_factor_) +
                          absl::int128(objective_integer_after_offset_);

    if (objective_domain_.IsFixed()) {
      // To avoid overflow in (fixed_value * gcd + before_offset) * factor +
      // after_offset because the objective is constant (and should fit on an
      // int64_t), we can rewrite it as fixed_value + offset.
      objective_integer_scaling_factor_ = 1;
      offset +=
          absl::int128(gcd - 1) * absl::int128(objective_domain_.FixedValue());
    } else {
      objective_integer_scaling_factor_ *= gcd;
    }

    objective_integer_before_offset_ = static_cast<int64_t>(
        offset / absl::int128(objective_integer_scaling_factor_));
    objective_integer_after_offset_ = static_cast<int64_t>(
        offset % absl::int128(objective_integer_scaling_factor_));

    // It is important to update the implied_domain for the "is constraining"
    // test below.
    implied_domain = implied_domain.InverseMultiplicationBy(gcd);
  }

  if (objective_domain_.IsEmpty()) {
    return NotifyThatModelIsUnsat("empty objective domain");
  }

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

bool PresolveContext::RecomputeSingletonObjectiveDomain() {
  CHECK_EQ(objective_map_.size(), 1);
  const int var = objective_map_.begin()->first;
  const int64_t coeff = objective_map_.begin()->second;

  // Transfer all the info to the domain of var.
  if (!IntersectDomainWith(var,
                           objective_domain_.InverseMultiplicationBy(coeff))) {
    return false;
  }

  // Recompute a correct and non-constraining objective domain.
  objective_proto_is_up_to_date_ = false;
  objective_domain_ = DomainOf(var).ContinuousMultiplicationBy(coeff);
  objective_domain_is_constraining_ = false;
  return true;
}

void PresolveContext::RemoveVariableFromObjective(int ref) {
  objective_proto_is_up_to_date_ = false;
  const int var = PositiveRef(ref);
  objective_map_.erase(var);
  EraseFromVarToConstraint(var, kObjectiveConstraint);
}

void PresolveContext::AddToObjective(int var, int64_t value) {
  CHECK(RefIsPositive(var));
  objective_proto_is_up_to_date_ = false;
  int64_t& map_ref = objective_map_[var];
  map_ref += value;
  if (map_ref == 0) {
    RemoveVariableFromObjective(var);
  } else {
    var_to_constraints_[var].insert(kObjectiveConstraint);
  }
}

void PresolveContext::AddLiteralToObjective(int ref, int64_t value) {
  objective_proto_is_up_to_date_ = false;
  const int var = PositiveRef(ref);
  int64_t& map_ref = objective_map_[var];
  if (RefIsPositive(ref)) {
    map_ref += value;
  } else {
    AddToObjectiveOffset(value);
    map_ref -= value;
  }
  if (map_ref == 0) {
    RemoveVariableFromObjective(var);
  } else {
    var_to_constraints_[var].insert(kObjectiveConstraint);
  }
}

bool PresolveContext::AddToObjectiveOffset(int64_t delta) {
  objective_proto_is_up_to_date_ = false;
  const int64_t temp = CapAdd(objective_integer_before_offset_, delta);
  if (temp == std::numeric_limits<int64_t>::min()) return false;
  if (temp == std::numeric_limits<int64_t>::max()) return false;
  objective_integer_before_offset_ = temp;

  // Tricky: The objective domain is without the offset, so we need to shift it.
  objective_offset_ += static_cast<double>(delta);
  objective_domain_ = objective_domain_.AdditionWith(Domain(-delta));
  return true;
}

bool PresolveContext::SubstituteVariableInObjective(
    int var_in_equality, int64_t coeff_in_equality,
    const ConstraintProto& equality) {
  objective_proto_is_up_to_date_ = false;
  CHECK(equality.enforcement_literal().empty());
  CHECK(RefIsPositive(var_in_equality));

  // We can only "easily" substitute if the objective coefficient is a multiple
  // of the one in the constraint.
  const int64_t coeff_in_objective = objective_map_.at(var_in_equality);
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
  if (!AddToObjectiveOffset(offset.Min())) return false;

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
    map_ref -= coeff * multiplier;

    if (map_ref == 0) {
      RemoveVariableFromObjective(var);
    } else {
      var_to_constraints_[var].insert(kObjectiveConstraint);
    }
  }

  RemoveVariableFromObjective(var_in_equality);

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
  if (exactly_one.empty()) return false;

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

  return ShiftCostInExactlyOne(exactly_one, min_coeff);
}

bool PresolveContext::ShiftCostInExactlyOne(absl::Span<const int> exactly_one,
                                            int64_t shift) {
  if (shift == 0) return true;

  // We have to be careful because shifting cost like this might increase the
  // min/max possible activity of the sum.
  //
  // TODO(user): Be more precise with this objective_overflow_detection_ and
  // always keep it up to date on each offset / coeff change.
  int64_t sum = 0;
  int64_t new_sum = 0;
  for (const int ref : exactly_one) {
    const int var = PositiveRef(ref);
    const int64_t obj = ObjectiveCoeff(var);
    sum = CapAdd(sum, std::abs(obj));

    const int64_t new_obj = RefIsPositive(ref) ? obj - shift : obj + shift;
    new_sum = CapAdd(new_sum, std::abs(new_obj));
  }
  if (AtMinOrMaxInt64(new_sum)) return false;
  if (new_sum > sum) {
    const int64_t new_value =
        CapAdd(objective_overflow_detection_, new_sum - sum);
    if (AtMinOrMaxInt64(new_value)) return false;
    objective_overflow_detection_ = new_value;
  }

  int64_t offset = shift;
  objective_proto_is_up_to_date_ = false;
  for (const int ref : exactly_one) {
    const int var = PositiveRef(ref);

    // The value will be zero if it wasn't present.
    int64_t& map_ref = objective_map_[var];
    if (map_ref == 0) {
      var_to_constraints_[var].insert(kObjectiveConstraint);
    }
    if (RefIsPositive(ref)) {
      map_ref -= shift;
      if (map_ref == 0) {
        RemoveVariableFromObjective(var);
      }
    } else {
      // Term = coeff * (1 - X) = coeff  - coeff * X;
      // So -coeff -> -coeff  -shift
      // And Term = coeff + shift - shift - (coeff + shift) * X
      //          = (coeff + shift) * (1 - X) - shift;
      map_ref += shift;
      if (map_ref == 0) {
        RemoveVariableFromObjective(var);
      }
      offset -= shift;
    }
  }

  // Note that the domain never include the offset, so we need to update it.
  if (offset != 0) AddToObjectiveOffset(offset);

  // When we shift the cost using an exactly one, our objective implied bounds
  // might be more or less precise. If the objective domain is not constraining
  // (and thus just constraining the upper bound), we relax it to make sure its
  // stay "non constraining".
  //
  // TODO(user): This is a bit hacky, find a nicer way.
  if (!objective_domain_is_constraining_) {
    objective_domain_ =
        Domain(std::numeric_limits<int64_t>::min(), objective_domain_.Max());
  }

  return true;
}

void PresolveContext::WriteObjectiveToProto() const {
  if (objective_proto_is_up_to_date_) return;
  objective_proto_is_up_to_date_ = true;

  // We need to sort the entries to be deterministic.
  // Note that --cpu_profile shows it is slightly faster to only compare key.
  tmp_entries_.clear();
  tmp_entries_.reserve(objective_map_.size());
  for (const auto& entry : objective_map_) {
    tmp_entries_.push_back(entry);
  }
  std::sort(tmp_entries_.begin(), tmp_entries_.end(),
            [](const std::pair<int, int64_t>& a,
               const std::pair<int, int64_t>& b) { return a.first < b.first; });

  CpObjectiveProto* mutable_obj = working_model->mutable_objective();
  mutable_obj->set_offset(objective_offset_);
  mutable_obj->set_scaling_factor(objective_scaling_factor_);
  mutable_obj->set_integer_before_offset(objective_integer_before_offset_);
  mutable_obj->set_integer_after_offset(objective_integer_after_offset_);
  if (objective_integer_scaling_factor_ == 1) {
    mutable_obj->set_integer_scaling_factor(0);  // Default.
  } else {
    mutable_obj->set_integer_scaling_factor(objective_integer_scaling_factor_);
  }
  FillDomainInProto(objective_domain_, mutable_obj);
  mutable_obj->clear_vars();
  mutable_obj->clear_coeffs();
  for (const auto& entry : tmp_entries_) {
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

  const int result = NewBoolVar("reified precedence");
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
  CanonicalizeLinearConstraint(lesseq);

  if (!LiteralIsTrue(active_i)) {
    AddImplication(result, active_i);
  }
  if (!LiteralIsTrue(active_j) && active_i != active_j) {
    AddImplication(result, active_j);
  }

  // Not(result) && active_i && active_j => (time_i > time_j)
  {
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

    greater->add_enforcement_literal(NegatedRef(result));
    if (!LiteralIsTrue(active_i)) {
      greater->add_enforcement_literal(active_i);
    }
    if (!LiteralIsTrue(active_j) && active_i != active_j) {
      greater->add_enforcement_literal(active_j);
    }
    CanonicalizeLinearConstraint(greater);
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
    if (!LiteralIsTrue(active_i)) {
      bool_or->add_literals(NegatedRef(active_i));
    }
    if (!LiteralIsTrue(active_j)) {
      bool_or->add_literals(NegatedRef(active_j));
    }
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
  absl::btree_map<std::string, int> sorted_rules(stats_by_rule_name_.begin(),
                                                 stats_by_rule_name_.end());
  for (const auto& entry : sorted_rules) {
    if (entry.second == 1) {
      SOLVER_LOG(logger_, "  - rule '", entry.first, "' was applied 1 time.");
    } else {
      SOLVER_LOG(logger_, "  - rule '", entry.first, "' was applied ",
                 FormatCounter(entry.second), " times.");
    }
  }
}

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
bool LoadModelForProbing(PresolveContext* context, Model* local_model) {
  if (context->ModelIsUnsat()) return false;

  // Update the domain in the current CpModelProto.
  context->WriteVariableDomainsToProto();
  const CpModelProto& model_proto = *(context->working_model);
  // Adapt some of the parameters during this probing phase.
  SatParameters local_params = context->params();
  local_params.set_use_implied_bounds(false);
  return LoadModelForPresolve(model_proto, std::move(local_params), context,
                              local_model, "probing");
}

bool LoadModelForPresolve(const CpModelProto& model_proto, SatParameters params,
                          PresolveContext* context, Model* local_model,
                          absl::string_view name_for_logging) {
  *local_model->GetOrCreate<SatParameters>() = std::move(params);
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
  if (sat_solver->ModelIsUnsat()) {
    return context->NotifyThatModelIsUnsat(
        absl::StrCat("Initial loading for ", name_for_logging));
  }
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) continue;
    CHECK(LoadConstraint(ct, local_model));
    if (sat_solver->ModelIsUnsat()) {
      return context->NotifyThatModelIsUnsat(
          absl::StrCat("after loading constraint during ", name_for_logging,
                       " ", ProtobufShortDebugString(ct)));
    }
  }
  encoder->AddAllImplicationsBetweenAssociatedLiterals();
  if (sat_solver->ModelIsUnsat()) return false;
  if (!sat_solver->Propagate()) {
    return context->NotifyThatModelIsUnsat(
        "during probing initial propagation");
  }

  return true;
}

template <typename ProtoWithVarsAndCoeffs, typename PresolveContextT>
bool CanonicalizeLinearExpressionInternal(
    absl::Span<const int> enforcements, ProtoWithVarsAndCoeffs* proto,
    int64_t* offset, std::vector<std::pair<int, int64_t>>* tmp_terms,
    PresolveContextT* context) {
  // First regroup the terms on the same variables and sum the fixed ones.
  //
  // TODO(user): Add a quick pass to skip most of the work below if the
  // constraint is already in canonical form?
  tmp_terms->clear();
  int64_t sum_of_fixed_terms = 0;
  bool remapped = false;
  const int old_size = proto->vars().size();
  DCHECK_EQ(old_size, proto->coeffs().size());
  for (int i = 0; i < old_size; ++i) {
    // Remove fixed variable and take affine representative.
    //
    // Note that we need to do that before we test for equality with an
    // enforcement (they should already have been mapped).
    int new_var;
    int64_t new_coeff;
    {
      const int ref = proto->vars(i);
      const int var = PositiveRef(ref);
      const int64_t coeff =
          RefIsPositive(ref) ? proto->coeffs(i) : -proto->coeffs(i);
      if (coeff == 0) continue;

      if (context->IsFixed(var)) {
        sum_of_fixed_terms += coeff * context->FixedValue(var);
        continue;
      }

      const AffineRelation::Relation r = context->GetAffineRelation(var);
      if (r.representative != var) {
        remapped = true;
        sum_of_fixed_terms += coeff * r.offset;
      }

      new_var = r.representative;
      new_coeff = coeff * r.coeff;
    }

    // TODO(user): Avoid the quadratic loop for the corner case of many
    // enforcement literal (this should be pretty rare though).
    bool removed = false;
    for (const int enf : enforcements) {
      if (new_var == PositiveRef(enf)) {
        if (RefIsPositive(enf)) {
          // If the constraint is enforced, we can assume the variable is at 1.
          sum_of_fixed_terms += new_coeff;
        } else {
          // We can assume the variable is at zero.
        }
        removed = true;
        break;
      }
    }
    if (removed) {
      context->UpdateRuleStats("linear: enforcement literal in expression");
      continue;
    }

    tmp_terms->push_back({new_var, new_coeff});
  }
  proto->clear_vars();
  proto->clear_coeffs();
  std::sort(tmp_terms->begin(), tmp_terms->end());
  int current_var = 0;
  int64_t current_coeff = 0;
  for (const auto& entry : *tmp_terms) {
    CHECK(RefIsPositive(entry.first));
    if (entry.first == current_var) {
      current_coeff += entry.second;
    } else {
      if (current_coeff != 0) {
        proto->add_vars(current_var);
        proto->add_coeffs(current_coeff);
      }
      current_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    proto->add_vars(current_var);
    proto->add_coeffs(current_coeff);
  }
  if (remapped) {
    context->UpdateRuleStats("linear: remapped using affine relations");
  }
  if (proto->vars().size() < old_size) {
    context->UpdateRuleStats("linear: fixed or dup variables");
  }
  *offset = sum_of_fixed_terms;
  return remapped || proto->vars().size() < old_size;
}

namespace {
bool CanonicalizeLinearExpressionNoContext(absl::Span<const int> enforcements,
                                           LinearConstraintProto* proto) {
  struct DummyContext {
    bool IsFixed(int /*var*/) const { return false; }
    int64_t FixedValue(int /*var*/) const { return 0; }
    AffineRelation::Relation GetAffineRelation(int var) const {
      return {var, 1, 0};
    }
    void UpdateRuleStats(absl::string_view /*rule*/) const {}
  } dummy_context;
  int64_t offset = 0;
  std::vector<std::pair<int, int64_t>> tmp_terms;
  const bool result = CanonicalizeLinearExpressionInternal(
      enforcements, proto, &offset, &tmp_terms, &dummy_context);
  if (offset != 0) {
    FillDomainInProto(ReadDomainFromProto(*proto).AdditionWith(Domain(-offset)),
                      proto);
  }
  return result;
}
}  // namespace

bool PresolveContext::CanonicalizeLinearConstraint(ConstraintProto* ct) {
  int64_t offset = 0;
  const bool result = CanonicalizeLinearExpressionInternal(
      ct->enforcement_literal(), ct->mutable_linear(), &offset, &tmp_terms_,
      this);
  if (offset != 0) {
    FillDomainInProto(
        ReadDomainFromProto(ct->linear()).AdditionWith(Domain(-offset)),
        ct->mutable_linear());
  }
  return result;
}

bool PresolveContext::CanonicalizeLinearExpression(
    absl::Span<const int> enforcements, LinearExpressionProto* expr) {
  int64_t offset = 0;
  const bool result = CanonicalizeLinearExpressionInternal(
      enforcements, expr, &offset, &tmp_terms_, this);
  expr->set_offset(expr->offset() + offset);
  return result;
}

ConstraintProto* PresolveContext::NewMappingConstraint(absl::string_view file,
                                                       int line) {
  const int c = mapping_model->constraints().size();
  ConstraintProto* new_ct = mapping_model->add_constraints();
  if (absl::GetFlag(FLAGS_cp_model_debug_postsolve)) {
    new_ct->set_name(absl::StrCat("#", c, " ", file, ":", line));
  }
  return new_ct;
}

ConstraintProto* PresolveContext::NewMappingConstraint(
    const ConstraintProto& base_ct, absl::string_view file, int line) {
  const int c = mapping_model->constraints().size();
  ConstraintProto* new_ct = mapping_model->add_constraints();
  *new_ct = base_ct;
  if (absl::GetFlag(FLAGS_cp_model_debug_postsolve)) {
    new_ct->set_name(absl::StrCat("#c", c, " ", file, ":", line));
  }
  return new_ct;
}

void CreateValidModelWithSingleConstraint(const ConstraintProto& ct,
                                          const PresolveContext* context,
                                          std::vector<int>* variable_mapping,
                                          CpModelProto* mini_model) {
  mini_model->Clear();

  *mini_model->add_constraints() = ct;

  absl::flat_hash_map<int, int> inverse_interval_map;
  for (const int i : UsedIntervals(ct)) {
    auto [it, inserted] =
        inverse_interval_map.insert({i, mini_model->constraints_size()});
    if (inserted) {
      *mini_model->add_constraints() = context->working_model->constraints(i);

      // Now add end = start + size for the interval. This is not strictly
      // necessary but it makes the presolve more powerful.
      ConstraintProto* linear = mini_model->add_constraints();
      *linear->mutable_enforcement_literal() = ct.enforcement_literal();
      LinearConstraintProto* mutable_linear = linear->mutable_linear();
      const IntervalConstraintProto& itv =
          context->working_model->constraints(i).interval();

      mutable_linear->add_domain(0);
      mutable_linear->add_domain(0);
      AddLinearExpressionToLinearConstraint(itv.start(), 1, mutable_linear);
      AddLinearExpressionToLinearConstraint(itv.size(), 1, mutable_linear);
      AddLinearExpressionToLinearConstraint(itv.end(), -1, mutable_linear);
      CanonicalizeLinearExpressionNoContext(ct.enforcement_literal(),
                                            mutable_linear);
    }
  }

  absl::flat_hash_map<int, int> inverse_variable_map;
  for (const ConstraintProto& cur_ct : mini_model->constraints()) {
    for (const int v : UsedVariables(cur_ct)) {
      auto [it, inserted] =
          inverse_variable_map.insert({v, mini_model->variables_size()});
      if (inserted) {
        FillDomainInProto(context->DomainOf(v), mini_model->add_variables());
      }
    }
  }

  variable_mapping->resize(inverse_variable_map.size());
  for (const auto& [k, v] : inverse_variable_map) {
    (*variable_mapping)[v] = k;
  }
  const auto mapping_function = [&inverse_variable_map](int* i) {
    const bool is_positive = RefIsPositive(*i);
    const int positive_ref = is_positive ? *i : NegatedRef(*i);

    const auto it = inverse_variable_map.find(positive_ref);
    DCHECK(it != inverse_variable_map.end());
    *i = is_positive ? it->second : NegatedRef(it->second);
  };
  const auto interval_mapping_function = [&inverse_interval_map](int* i) {
    const auto it = inverse_interval_map.find(*i);
    DCHECK(it != inverse_interval_map.end());
    *i = it->second;
  };
  for (ConstraintProto& ct : *mini_model->mutable_constraints()) {
    ApplyToAllVariableIndices(mapping_function, &ct);
    ApplyToAllLiteralIndices(mapping_function, &ct);
    ApplyToAllIntervalIndices(interval_mapping_function, &ct);
  }
}

bool PresolveContext::DebugTestHintFeasibility() {
  WriteVariableDomainsToProto();
  if (hint_.size() != working_model->variables().size()) return false;
  return SolutionIsFeasible(*working_model, hint_);
}

}  // namespace sat
}  // namespace operations_research
