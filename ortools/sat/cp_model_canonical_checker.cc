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

#include "ortools/sat/cp_model_canonical_checker.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

bool LinearExpressionProtoLessThan(const LinearExpressionProto& a,
                                   const LinearExpressionProto& b) {
  if (absl::MakeSpan(a.vars()) != absl::MakeSpan(b.vars())) {
    return absl::MakeSpan(a.vars()) < absl::MakeSpan(b.vars());
  }
  if (absl::MakeSpan(a.coeffs()) != absl::MakeSpan(b.coeffs())) {
    return absl::MakeSpan(a.coeffs()) < absl::MakeSpan(b.coeffs());
  }
  return a.offset() < b.offset();
}

CanonicalizationChecker::CanonicalizationChecker(
    const std::vector<Domain>* domains, Options options)
    : domains_(domains), options_(std::move(options)) {
  DCHECK(domains_ != nullptr);
}

bool CanonicalizationChecker::IsFixed(int ref) const {
  const int var = PositiveRef(ref);
  DCHECK_LT(var, domains_->size());
  DCHECK(!(*domains_)[var].IsEmpty());
  return (*domains_)[var].IsFixed();
}

bool CanonicalizationChecker::LiteralIsFalse(int lit) const {
  const int var = PositiveRef(lit);
  DCHECK_LT(var, domains_->size());
  DCHECK_GE((*domains_)[var].Min(), 0);
  DCHECK_LE((*domains_)[var].Max(), 1);
  if (RefIsPositive(lit)) {
    return (*domains_)[var].Max() == 0;
  }
  return (*domains_)[var].Min() == 1;
}

bool CanonicalizationChecker::IsRepresentative(int ref) const {
  return options_.var_is_representative == nullptr ||
         options_.var_is_representative(PositiveRef(ref));
}

bool CanonicalizationChecker::LiteralsAreCanonical(absl::Span<const int> lits,
                                                   bool check_enforcements,
                                                   bool check_sorted) const {
  int prev = std::numeric_limits<int>::min();
  if (!check_sorted) tmp_seen_vars_.clear();
  for (const int lit : lits) {
    const int var = PositiveRef(lit);
    if ((check_sorted ? lit <= prev : !tmp_seen_vars_.insert(var).second) ||
        IsFixed(lit) || !IsRepresentative(var)) {
      return false;
    }
    if (check_enforcements && (tmp_enforcements_.contains(var) ||
                               tmp_enforcements_.contains(NegatedRef(var)))) {
      return false;
    }
    prev = lit;
  }
  return !check_sorted || !SortedLiteralsHaveOppositePair(lits);
}

bool CanonicalizationChecker::LinearExpressionIsCanonical(
    const LinearExpressionProto& expr, bool check_sorted) const {
  if (expr.vars_size() != expr.coeffs_size()) return false;
  int prev_var = -1;
  if (!check_sorted) tmp_seen_vars_.clear();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const int var = expr.vars(i);
    if (!RefIsPositive(var) || expr.coeffs(i) == 0 ||
        (check_sorted ? var <= prev_var : !tmp_seen_vars_.insert(var).second) ||
        IsFixed(var) || !IsRepresentative(var)) {
      return false;
    }
    if (tmp_enforcements_.contains(var) ||
        tmp_enforcements_.contains(NegatedRef(var))) {
      return false;
    }
    prev_var = var;
  }
  return true;
}

bool CanonicalizationChecker::ConstraintIsCanonical(const ConstraintProto& ct,
                                                    bool check_sorted) const {
  if (ct.constraint_case() == ConstraintProto::CONSTRAINT_NOT_SET) {
    return ct.enforcement_literal().empty();
  }

  // Unperformed optional interval stubs only exist mid-presolve (when
  // `options_.var_is_representative` is set), before `RemoveEmptyConstraints()`
  // strips them.
  if (options_.var_is_representative != nullptr &&
      ct.constraint_case() == ConstraintProto::kInterval &&
      ct.enforcement_literal_size() == 1 &&
      IsFixed(ct.enforcement_literal(0)) &&
      LiteralIsFalse(ct.enforcement_literal(0))) {
    return IsRepresentative(ct.enforcement_literal(0)) &&
           !ct.interval().has_start() && !ct.interval().has_size() &&
           !ct.interval().has_end();
  }

  if (ct.constraint_case() == ConstraintProto::kBoolOr &&
      !ct.enforcement_literal().empty()) {
    return false;
  }
  if (!LiteralsAreCanonical(ct.enforcement_literal(),
                            /*check_enforcements=*/false, check_sorted)) {
    return false;
  }

  tmp_enforcements_.clear();
  tmp_enforcements_.insert(ct.enforcement_literal().begin(),
                           ct.enforcement_literal().end());
  const auto all_exprs_canonical = [&]() {
    bool ok = true;
    ApplyToAllLinearExpressions(
        [&](const LinearExpressionProto& expr) {
          if (!ok) return;
          if (!LinearExpressionIsCanonical(expr, check_sorted)) {
            ok = false;
          }
        },
        ct);
    return ok;
  };
  const auto has_false_literals = [&](absl::Span<const int> lits) {
    for (const int lit : lits) {
      if (LiteralIsFalse(lit)) return true;
    }
    return false;
  };
  const auto literals_are_resolved = [&](absl::Span<const int> lits) {
    for (const int lit : lits) {
      if (!IsRepresentative(lit) || tmp_enforcements_.contains(lit) ||
          tmp_enforcements_.contains(NegatedRef(lit))) {
        return false;
      }
    }
    return true;
  };

  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      return true;
    case ConstraintProto::ConstraintCase::kLinear: {
      const LinearConstraintProto& lin = ct.linear();
      if (lin.vars().empty() || lin.vars_size() != lin.coeffs_size() ||
          lin.domain().empty()) {
        return false;
      }
      if (!HasEnforcementLiteral(ct) && lin.vars_size() < 2) return false;
      int prev_var = -1;
      if (!check_sorted) tmp_seen_vars_.clear();
      int64_t gcd = 0;
      for (int i = 0; i < lin.vars_size(); ++i) {
        const int var = lin.vars(i);
        if (!RefIsPositive(var) ||
            (check_sorted ? var <= prev_var
                          : !tmp_seen_vars_.insert(var).second) ||
            lin.coeffs(i) == 0 || IsFixed(var) || !IsRepresentative(var)) {
          return false;
        }
        if (tmp_enforcements_.contains(var) ||
            tmp_enforcements_.contains(NegatedRef(var))) {
          return false;
        }
        gcd = std::gcd(gcd, std::abs(lin.coeffs(i)));
        prev_var = var;
      }
      return gcd == 1 && lin.coeffs(0) > 0;
    }
    case ConstraintProto::ConstraintCase::kBoolOr:
      if (options_.lrat_mode) return true;
      return ct.bool_or().literals_size() >= 2 &&
             LiteralsAreCanonical(ct.bool_or().literals(),
                                  /*check_enforcements=*/false, check_sorted);
    case ConstraintProto::ConstraintCase::kBoolAnd:
      if (options_.lrat_mode) return true;
      return HasEnforcementLiteral(ct) && !ct.bool_and().literals().empty() &&
             LiteralsAreCanonical(ct.bool_and().literals(),
                                  /*check_enforcements=*/true, check_sorted);
    case ConstraintProto::ConstraintCase::kAtMostOne:
      return !HasEnforcementLiteral(ct) &&
             ct.at_most_one().literals_size() >= 2 &&
             LiteralsAreCanonical(ct.at_most_one().literals(),
                                  /*check_enforcements=*/false, check_sorted);
    case ConstraintProto::ConstraintCase::kExactlyOne:
      return !HasEnforcementLiteral(ct) &&
             ct.exactly_one().literals_size() >= 2 &&
             LiteralsAreCanonical(ct.exactly_one().literals(),
                                  /*check_enforcements=*/false, check_sorted);
    case ConstraintProto::ConstraintCase::kBoolXor: {
      const auto& lits = ct.bool_xor().literals();
      if (lits.size() < 2) return false;
      int prev_var = -1;
      if (!check_sorted) tmp_seen_vars_.clear();
      for (int i = 0; i < lits.size(); ++i) {
        const int lit = lits[i];
        const int var = PositiveRef(lit);
        if ((i > 0 && !RefIsPositive(lit)) ||
            (check_sorted ? var <= prev_var
                          : !tmp_seen_vars_.insert(var).second) ||
            IsFixed(lit) || !IsRepresentative(var)) {
          return false;
        }
        if (tmp_enforcements_.contains(var) ||
            tmp_enforcements_.contains(NegatedRef(var))) {
          return false;
        }
        prev_var = var;
      }
      return true;
    }
    case ConstraintProto::ConstraintCase::kLinMax:
      return all_exprs_canonical() &&
             (!check_sorted ||
              absl::c_is_sorted(ct.lin_max().exprs(),
                                LinearExpressionProtoLessThan));
    case ConstraintProto::ConstraintCase::kCircuit:
      return !ct.circuit().tails().empty() &&
             !has_false_literals(ct.circuit().literals()) &&
             literals_are_resolved(ct.circuit().literals());
    case ConstraintProto::ConstraintCase::kRoutes:
      return !ct.routes().tails().empty() && all_exprs_canonical() &&
             !has_false_literals(ct.routes().literals()) &&
             literals_are_resolved(ct.routes().literals());
    case ConstraintProto::ConstraintCase::kReservoir:
      return !ct.reservoir().time_exprs().empty() && all_exprs_canonical() &&
             !has_false_literals(ct.reservoir().active_literals()) &&
             literals_are_resolved(ct.reservoir().active_literals());
    case ConstraintProto::ConstraintCase::kDummyConstraint:
    case ConstraintProto::ConstraintCase::kNoOverlap:
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      return true;
    case ConstraintProto::ConstraintCase::kIntProd:
      return all_exprs_canonical() &&
             (!check_sorted ||
              absl::c_is_sorted(ct.int_prod().exprs(),
                                LinearExpressionProtoLessThan));
    case ConstraintProto::ConstraintCase::kAllDiff:
      return all_exprs_canonical() &&
             (!check_sorted ||
              absl::c_is_sorted(ct.all_diff().exprs(),
                                LinearExpressionProtoLessThan));
    case ConstraintProto::ConstraintCase::kElement: {
      const ElementConstraintProto& elem = ct.element();
      return elem.index() == 0 && elem.target() == 0 && elem.vars().empty() &&
             (elem.has_linear_index() || elem.has_linear_target() ||
              !elem.exprs().empty()) &&
             all_exprs_canonical();
    }
    case ConstraintProto::ConstraintCase::kTable:
      return ct.table().vars().empty() && all_exprs_canonical();
    case ConstraintProto::ConstraintCase::kAutomaton:
      return ct.automaton().vars().empty() && all_exprs_canonical();
    case ConstraintProto::ConstraintCase::kInverse:
      return ct.inverse().f_direct().empty() &&
             ct.inverse().f_inverse().empty() && all_exprs_canonical();
    case ConstraintProto::ConstraintCase::kIntDiv:
    case ConstraintProto::ConstraintCase::kIntMod:
    case ConstraintProto::ConstraintCase::kInterval:
    case ConstraintProto::ConstraintCase::kCumulative:
      return all_exprs_canonical();
  }
  return true;
}

bool ModelProtoIsCanonical(const CpModelProto& model, bool lrat_mode,
                           bool check_sorted) {
  std::vector<Domain> domains;
  domains.reserve(model.variables_size());
  for (const IntegerVariableProto& var_proto : model.variables()) {
    Domain d = ReadDomainFromProto(var_proto);
    if (d.IsEmpty()) return true;
    domains.push_back(std::move(d));
  }
  CanonicalizationChecker::Options options;
  options.lrat_mode = lrat_mode;
  CanonicalizationChecker checker(&domains, std::move(options));
  for (const ConstraintProto& ct : model.constraints()) {
    if (!checker.ConstraintIsCanonical(ct, check_sorted)) return false;
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
