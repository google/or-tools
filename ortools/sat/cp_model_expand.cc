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

#include "ortools/sat/cp_model_expand.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "google/protobuf/message.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/types.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_table.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_status.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/util/deterministic_time.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

FeasibilityStatus ExpandAlwaysFalseConstraint(int c, PresolveContext* context,
                                              absl::string_view message = "") {
  const ConstraintProto& ct = context->Constraint(c);
  if (ct.enforcement_literal().empty()) {
    return context->NotifyUnsat(message);
  }
  context->AddConstraint([&ct](ConstraintProto* new_ct) {
    BoolArgumentProto* bool_or = new_ct->mutable_bool_or();
    for (const int literal : ct.enforcement_literal()) {
      bool_or->add_literals(NegatedRef(literal));
    }
  });
  context->ClearConstraint(c);
  return kOk;
}

// Returned by `EnforcedDomains` domain-filtering operations: `Ok()` when the
// domain remains non-empty, `kAlwaysFalse` when empty and enforced, and
// `kUnsat` when empty and unenforced.
class [[nodiscard]] EnforcedStatus {
 public:
  constexpr EnforcedStatus(UnsatStatus) : state_(State::kUnsat) {}
  explicit constexpr EnforcedStatus(FeasibilityStatus status)
      : state_(status.unsat() ? State::kUnsat : State::kAlwaysFalse) {}
  static constexpr EnforcedStatus Ok() { return EnforcedStatus(State::kOk); }
  constexpr bool ok() const { return state_ == State::kOk; }
  constexpr FeasibilityStatus feasibility() const {
    return state_ == State::kUnsat ? FeasibilityStatus(kUnsat) : kOk;
  }

 private:
  enum class State : uint8_t { kOk, kUnsat, kAlwaysFalse };
  explicit constexpr EnforcedStatus(State state) : state_(state) {}
  State state_;
};

// Keeps track of the domains of the variables used in a constraint when this
// constraint is enforced. These domains cannot be modified in the presolve
// context because they are not valid when the constraint is not enforced.
class EnforcedDomains {
 public:
  EnforcedDomains(int c, PresolveContext* context) : c_(c), context_(context) {}

  int size() { return domains_.size(); }

  bool IsFixed(const LinearExpressionProto& expr) const {
    CHECK_LE(expr.vars_size(), 1);
    return expr.vars().empty() || DomainOf(expr.vars(0)).IsFixed();
  }

  Domain DomainOf(int ref) const {
    auto it = domains_.find(PositiveRef(ref));
    if (it != domains_.end()) {
      return RefIsPositive(ref) ? it->second : it->second.Negation();
    }
    return context_->DomainOf(ref);
  }

  bool DomainContains(int ref, int64_t value) const {
    auto it = domains_.find(PositiveRef(ref));
    if (it != domains_.end()) {
      return RefIsPositive(ref) ? it->second.Contains(value)
                                : it->second.Contains(-value);
    }
    return context_->DomainOf(ref).Contains(value);
  }

  bool DomainContains(const LinearExpressionProto& expr, int64_t value) const {
    CHECK_LE(expr.vars_size(), 1);
    const int64_t offset = expr.offset();
    if (expr.vars().empty()) {
      return offset == value;
    }
    const Domain domain = DomainOf(expr.vars(0));
    const int64_t coeff = expr.coeffs(0);
    if (value > coeff * (coeff > 0 ? domain.Max() : domain.Min()) + offset) {
      return false;
    }
    if (value < coeff * (coeff > 0 ? domain.Min() : domain.Max()) + offset) {
      return false;
    }
    // We assume expression is validated for overflow initially, and the code
    // below should be overflow safe.
    if ((value - expr.offset()) % expr.coeffs(0) != 0) return false;
    return DomainOf(expr.vars(0))
        .Contains((value - expr.offset()) / expr.coeffs(0));
  }

  // Returns `EnforcedStatus::Ok()` if the new domain is non-empty,
  // `kAlwaysFalse` when empty and enforced, and `kUnsat` when empty and
  // unenforced.
  EnforcedStatus IntersectDomainWith(int ref, const Domain& domain,
                                     absl::string_view message = "",
                                     bool* domain_modified = nullptr) {
    if (context_->Constraint(c_).enforcement_literal().empty() &&
        context_->IntersectDomainWith(ref, domain, domain_modified).unsat()) {
      return context_->NotifyUnsat(message);
    }
    const Domain current_domain = DomainOf(ref);
    if (current_domain.IsIncludedIn(domain)) return EnforcedStatus::Ok();
    if (domain_modified != nullptr) {
      *domain_modified = true;
    }
    const Domain new_domain = current_domain.IntersectionWith(domain);
    if (new_domain.IsEmpty()) {
      return EnforcedStatus(ExpandAlwaysFalseConstraint(c_, context_, message));
    }
    domains_[PositiveRef(ref)] = new_domain;
    return EnforcedStatus::Ok();
  }

  EnforcedStatus SetLiteralToFalse(int lit) {
    const int var = PositiveRef(lit);
    const int64_t value = RefIsPositive(lit) ? 0 : 1;
    return IntersectDomainWith(var, Domain(value));
  }

  EnforcedStatus IntersectDomainWith(const LinearExpressionProto& expr,
                                     const Domain& domain,
                                     absl::string_view message = "",
                                     bool* domain_modified = nullptr) {
    CHECK_LE(expr.vars_size(), 1);
    if (context_->Constraint(c_).enforcement_literal().empty() &&
        context_->IntersectDomainWith(expr, domain, domain_modified).unsat()) {
      return context_->NotifyUnsat(message);
    }
    if (expr.vars().empty()) {
      if (domain.Contains(expr.offset())) return EnforcedStatus::Ok();
      return EnforcedStatus(ExpandAlwaysFalseConstraint(c_, context_, message));
    }
    return IntersectDomainWith(expr.vars(0),
                               domain.AdditionWith(Domain(-expr.offset()))
                                   .InverseMultiplicationBy(expr.coeffs(0)),
                               message, domain_modified);
  }

  void MaybeAddEnforcedDomainConstraints() const {
    if (context_->Constraint(c_).enforcement_literal().empty()) return;
    std::vector<int> vars;
    for (const auto& [var, _] : domains_) {
      vars.push_back(var);
    }
    std::sort(vars.begin(), vars.end());
    const ConstraintProto& ct = context_->Constraint(c_);
    for (const int var : vars) {
      // enforcement_literal => var in domain
      const Domain& domain = domains_.at(var);
      context_->AddEnforcedConstraint(
          ct, [var, &domain](ConstraintProto* new_ct) {
            LinearConstraintProto* const lin = new_ct->mutable_linear();
            lin->add_vars(var);
            lin->add_coeffs(1);
            FillDomainInProto(domain, lin);
          });
    }
  }

 private:
  const int c_;
  PresolveContext* context_;
  absl::flat_hash_map<int, Domain> domains_;
};

void ExpandEnforcedAtMostOneOrExactlyOneConstraint(
    int c, PresolveContext* context, std::vector<int>* tmp_literals) {
  const ConstraintProto& ct = context->Constraint(c);
  if (ct.enforcement_literal().empty()) {
    return;
  }
  const auto& src = ct.has_at_most_one() ? ct.at_most_one().literals()
                                         : ct.exactly_one().literals();
  tmp_literals->assign(src.begin(), src.end());
  const int64_t lb = ct.has_at_most_one() ? 0 : 1;
  context->ModifyConstraint(c, [&](ConstraintProto* mutable_ct) {
    LiteralsToLinear(*tmp_literals, lb, /*ub=*/1, mutable_ct->mutable_linear());
  });
}

void ExpandAllEnforcedAtMostOneAndExactlyOne(PresolveContext* context,
                                             std::vector<int>* tmp_literals) {
  DeterministicTimer<{.scale = 1.3e-08, .offset = 1.6e-08}> timer(
      context->time_limit(), context->NumConstraints());
  for (int c = 0; c < context->NumConstraints(); ++c) {
    const ConstraintProto& ct = context->Constraint(c);
    if (!ct.enforcement_literal().empty() &&
        (ct.has_at_most_one() || ct.has_exactly_one())) {
      ExpandEnforcedAtMostOneOrExactlyOneConstraint(c, context, tmp_literals);
    }
  }
}

// Different encoding that supports general demands. This is usually a pretty
// bad encoding, at least until we improve the solver on such models.
FeasibilityStatus ExpandReservoirUsingCircuit(int64_t sum_of_positive_demand,
                                              int64_t sum_of_negative_demand,
                                              int c, PresolveContext* context) {
  const ConstraintProto& reservoir_ct = context->Constraint(c);
  const ReservoirConstraintProto& reservoir = reservoir_ct.reservoir();
  const int num_events = reservoir.time_exprs_size();

  const int64_t var_min =
      std::max(reservoir.min_level(), sum_of_negative_demand);
  const int64_t var_max =
      std::min(reservoir.max_level(), sum_of_positive_demand);
  if (var_min > var_max) {
    return ExpandAlwaysFalseConstraint(
        c, context, "Empty level domain in reservoir constraint.");
  }

  // The encoding will create a circuit constraint, and one integer variable per
  // event (representing the level at that event time).
  std::vector<int> circuit_tails;
  std::vector<int> circuit_heads;
  std::vector<int> circuit_literals;

  std::vector<int> level_vars(num_events);
  for (int i = 0; i < num_events; ++i) {
    level_vars[i] = context->NewIntVar(Domain(var_min, var_max));
  }

  // For the corner case where all events are absent, we need a potential
  // self-arc on the start/end circuit node.
  {
    const int all_inactive = context->NewBoolVar("reservoir expansion");
    circuit_tails.push_back(num_events);
    circuit_heads.push_back(num_events);
    circuit_literals.push_back(all_inactive);
  }

  for (int i = 0; i < num_events; ++i) {
    if (!reservoir.active_literals().empty()) {
      // Add self arc to represent absence.
      circuit_tails.push_back(i);
      circuit_heads.push_back(i);
      circuit_literals.push_back(NegatedRef(reservoir.active_literals(i)));
    }

    // We need an extra circuit node for start/end of circuit.
    // We use the available index 'num_events'.
    {
      // Circuit starts at i, level_vars[i] == demand_expr[i].
      const int start_var = context->NewBoolVar("reservoir expansion");
      circuit_tails.push_back(num_events);
      circuit_heads.push_back(i);
      circuit_literals.push_back(start_var);

      // Add enforced linear for demand.
      context->AddEnforcedConstraint(
          reservoir_ct, [&](ConstraintProto* new_ct) {
            new_ct->add_enforcement_literal(start_var);
            LinearConstraintProto* lin = new_ct->mutable_linear();
            FillDomainInProto(0, lin);
            lin->add_vars(level_vars[i]);
            lin->add_coeffs(1);
            AddLinearExpressionToLinearConstraint(reservoir.level_changes(i),
                                                  -1, lin);
          });

      // Circuit ends at i, no extra constraint there.
      const int end_var = context->NewBoolVar("reservoir expansion");
      circuit_tails.push_back(i);
      circuit_heads.push_back(num_events);
      circuit_literals.push_back(end_var);
    }

    for (int j = 0; j < num_events; ++j) {
      if (i == j) continue;

      // If arc_i_j is true then:
      // - active_i is true (enforced by circuit).
      // - active_j is true (enforced by circuit).
      // - time_i <= time_j
      // - level_j == level_i + demand_j
      //
      // TODO(user): Unfortunately we cannot share these literals between
      // reservoirs except if the set of time points is exactly the same!
      // Otherwise if we miss one, then A "after" B in one circuit does not
      // imply that there is no C in between in another!
      const int arc_i_j = context->NewBoolVar("reservoir expansion");
      circuit_tails.push_back(i);
      circuit_heads.push_back(j);
      circuit_literals.push_back(arc_i_j);

      // Add enforced linear for time.
      context->AddEnforcedConstraint(
          reservoir_ct, [&](ConstraintProto* new_ct) {
            new_ct->add_enforcement_literal(arc_i_j);
            LinearConstraintProto* lin = new_ct->mutable_linear();
            FillDomainInProto(0, kint64max, lin);
            AddLinearExpressionToLinearConstraint(reservoir.time_exprs(j), 1,
                                                  lin);
            AddLinearExpressionToLinearConstraint(reservoir.time_exprs(i), -1,
                                                  lin);
          });

      // Add enforced linear for demand.
      context->AddEnforcedConstraint(
          reservoir_ct, [&](ConstraintProto* new_ct) {
            new_ct->add_enforcement_literal(arc_i_j);
            LinearConstraintProto* lin = new_ct->mutable_linear();
            FillDomainInProto(0, lin);
            lin->add_vars(level_vars[j]);
            lin->add_coeffs(1);
            lin->add_vars(level_vars[i]);
            lin->add_coeffs(-1);
            AddLinearExpressionToLinearConstraint(reservoir.level_changes(j),
                                                  -1, lin);
          });
    }
  }
  const int circuit_c = context->AddConstraint([&](ConstraintProto* new_ct) {
    CircuitConstraintProto* circuit = new_ct->mutable_circuit();
    *circuit->mutable_tails() = {circuit_tails.begin(), circuit_tails.end()};
    *circuit->mutable_heads() = {circuit_heads.begin(), circuit_heads.end()};
    *circuit->mutable_literals() = {circuit_literals.begin(),
                                    circuit_literals.end()};
  });
  context->solution_crush().SetReservoirCircuitVars(
      reservoir, var_min, var_max, level_vars,
      context->Constraint(circuit_c).circuit());

  context->ClearConstraint(c);
  context->UpdateRuleStats("reservoir: expanded using circuit.");
  return kOk;
}

void ExpandReservoirUsingPrecedences(bool max_level_is_constraining,
                                     bool min_level_is_constraining, int c,
                                     PresolveContext* context) {
  const ConstraintProto& reservoir_ct = context->Constraint(c);
  const ReservoirConstraintProto& reservoir = reservoir_ct.reservoir();
  const int num_events = reservoir.time_exprs_size();
  const int true_literal = context->GetTrueLiteral();
  const auto is_active_literal = [&reservoir, true_literal](int index) {
    if (reservoir.active_literals_size() == 0) return true_literal;
    return reservoir.active_literals(index);
  };

  // Constrains the running level to be consistent at all time_exprs.
  // For this we only add a constraint at the time a given demand takes place.
  std::vector<std::pair<int, int64_t>> terms;
  for (int i = 0; i < num_events; ++i) {
    const int active_i = is_active_literal(i);
    if (context->LiteralIsFalse(active_i)) continue;

    const int64_t demand_i = context->FixedValue(reservoir.level_changes(i));
    if (demand_i == 0) continue;

    // No need for some constraints if the reservoir is just constrained in
    // one direction.
    if (demand_i > 0 && !max_level_is_constraining) continue;
    if (demand_i < 0 && !min_level_is_constraining) continue;

    terms.clear();
    // Add contributions from events that happened at time_j <= time_i.
    const LinearExpressionProto& time_i = reservoir.time_exprs(i);
    for (int j = 0; j < num_events; ++j) {
      if (i == j) continue;
      const int active_j = is_active_literal(j);
      if (context->LiteralIsFalse(active_j)) continue;
      const int64_t demand_j = context->FixedValue(reservoir.level_changes(j));
      if (demand_j == 0) continue;

      // Get or create the literal equivalent to
      // active_i && active_j && time[j] <= time[i].
      //
      // TODO(user): we could get rid of active_i in the equivalence above.
      // Experiments when we have enough benchmarks.
      const LinearExpressionProto& time_j = reservoir.time_exprs(j);
      const int j_lesseq_i = context->GetOrCreateReifiedPrecedenceLiteral(
          time_j, time_i, active_j, active_i);
      terms.push_back({j_lesseq_i, demand_j});
    }

    // Add contribution from event i.
    //
    // TODO(user): Alternatively we can mark the whole constraint as enforced
    // only if active_i is true. Experiment with both versions; right now we
    // lack enough benchmarks to conclude.
    terms.push_back({active_i, demand_i});

    context->AddEnforcedConstraint(reservoir_ct, [&](ConstraintProto* new_ct) {
      LinearConstraintProto* new_linear = new_ct->mutable_linear();
      int64_t offset = 0;
      for (const auto [lit, demand] : terms) {
        AddWeightedLiteralToLinearConstraint(lit, demand, new_linear, &offset);
      }

      // Note that according to the sign of demand_i, we only need one side.
      // We apply the offset here to make sure we use int64_t min and max.
      if (demand_i > 0) {
        FillDomainInProto(kint64min, reservoir.max_level() - offset,
                          new_linear);
      } else {
        FillDomainInProto(reservoir.min_level() - offset, kint64max,
                          new_linear);
      }
    });
  }

  context->ClearConstraint(c);
  context->UpdateRuleStats("reservoir: expanded using precedences");
}

// Checks that single-sign reservoir expansion into a sum linear constraint
// (plus optional per-event `new_var - demand == 0` constraints) cannot overflow
// LinearOverflowChecker or saturate the sum constraint's domain bounds.
bool CanExpandReservoirWithSum(const ReservoirConstraintProto& reservoir,
                               PresolveContext* context) {
  const int true_literal = context->GetTrueLiteral();
  LinearOverflowChecker sum_checker;
  int64_t min_sum_domain = reservoir.min_level();
  int64_t max_sum_domain = reservoir.max_level();
  for (int i = 0; i < reservoir.time_exprs_size(); ++i) {
    const int active = reservoir.active_literals().empty()
                           ? true_literal
                           : reservoir.active_literals(i);
    if (context->LiteralIsFalse(active)) continue;
    const LinearExpressionProto& demand = reservoir.level_changes(i);
    const int64_t min_demand = context->MinOf(demand);
    const int64_t max_demand = context->MaxOf(demand);
    if (context->IsFixed(demand)) {
      if (RefIsPositive(active)) {
        if (!sum_checker.AddTerm(min_demand, 0, 1)) return false;
      } else {
        // Negated active adds true_literal * change + not(active) * (-change).
        if (!sum_checker.AddTerm(min_demand, 1, 1) ||
            !sum_checker.AddTerm(-min_demand, 0, 1)) {
          return false;
        }
      }
    } else if (context->LiteralIsTrue(active)) {
      // Non-zero offsets shift sum's domain; CapSub is not sticky at min/max.
      if (demand.offset() != 0) {
        if (AtMinOrMaxInt64(min_sum_domain) ||
            AtMinOrMaxInt64(max_sum_domain)) {
          return false;
        }
        min_sum_domain = CapSub(min_sum_domain, demand.offset());
        max_sum_domain = CapSub(max_sum_domain, demand.offset());
        if (AtMinOrMaxInt64(min_sum_domain) ||
            AtMinOrMaxInt64(max_sum_domain)) {
          return false;
        }
      }
      for (int j = 0; j < demand.vars_size(); ++j) {
        const int var = demand.vars(j);
        if (!sum_checker.AddTerm(demand.coeffs(j), context->MinOf(var),
                                 context->MaxOf(var))) {
          return false;
        }
      }
    } else {
      if (!sum_checker.AddTerm(1, std::min(int64_t{0}, min_demand),
                               std::max(int64_t{0}, max_demand))) {
        return false;
      }
      // Also guard the enforced constraint `new_var - demand == 0`.
      LinearOverflowChecker checker;
      checker.sum_min = std::min(int64_t{0}, min_demand);
      checker.sum_max = std::max(int64_t{0}, max_demand);
      for (int j = 0; j < demand.vars_size(); ++j) {
        const int var = demand.vars(j);
        if (!checker.AddTerm(-demand.coeffs(j), context->MinOf(var),
                             context->MaxOf(var))) {
          return false;
        }
      }
    }
  }
  return true;
}

FeasibilityStatus ExpandReservoir(int c, PresolveContext* context) {
  const ConstraintProto& reservoir_ct = context->Constraint(c);
  if (reservoir_ct.reservoir().min_level() >
      reservoir_ct.reservoir().max_level()) {
    return ExpandAlwaysFalseConstraint(
        c, context, "Empty level domain in reservoir constraint.");
  }

  const ReservoirConstraintProto& reservoir = reservoir_ct.reservoir();
  const int num_events = reservoir.time_exprs_size();

  int num_positives = 0;
  int num_negatives = 0;
  bool all_demands_are_fixed = true;
  int64_t sum_of_positive_demand = 0;
  int64_t sum_of_negative_demand = 0;
  for (const LinearExpressionProto& demand_expr : reservoir.level_changes()) {
    if (!context->IsFixed(demand_expr)) {
      all_demands_are_fixed = false;
    }
    const int64_t max_demand = context->MaxOf(demand_expr);
    if (max_demand > 0) {
      num_positives++;
      sum_of_positive_demand += max_demand;
    }
    const int64_t min_demand = context->MinOf(demand_expr);
    if (min_demand < 0) {
      num_negatives++;
      sum_of_negative_demand += min_demand;
    }
  }

  if (sum_of_negative_demand >= reservoir.min_level() &&
      sum_of_positive_demand <= reservoir.max_level()) {
    context->UpdateRuleStats("reservoir: always true");
    context->ClearConstraint(c);
    return kOk;
  }

  // If all level_changes have the same sign, we do not care about the order,
  // just the sum. We might need to create intermediate variables for quadratic
  // terms though.
  if ((num_negatives == 0 || num_positives == 0) &&
      CanExpandReservoirWithSum(reservoir, context)) {
    const int true_literal = context->GetTrueLiteral();
    std::vector<int> optional_demand_vars(num_events, -1);
    for (int i = 0; i < num_events; ++i) {
      const int active = reservoir.active_literals().empty()
                             ? true_literal
                             : reservoir.active_literals(i);
      if (context->LiteralIsFalse(active)) continue;
      const LinearExpressionProto& demand = reservoir.level_changes(i);
      if (context->IsFixed(demand) || context->LiteralIsTrue(active)) continue;

      const int new_var = context->NewIntVar(
          Domain(context->MinOf(demand), context->MaxOf(demand))
              .UnionWith(Domain(0)));
      optional_demand_vars[i] = new_var;

      // Active => new_var == demand.
      context->AddEnforcedConstraint(
          {active}, [new_var, &demand](ConstraintProto* new_ct) {
            LinearConstraintProto* lin = new_ct->mutable_linear();
            FillDomainInProto(0, lin);
            lin->add_vars(new_var);
            lin->add_coeffs(1);
            AddLinearExpressionToLinearConstraint(demand, -1, lin);
          });
      context->solution_crush().SetVarToLinearExpressionIf(new_var, demand,
                                                           {active});

      // not(active) => new_var == 0.
      context->AddImplyInDomain(NegatedRef(active), new_var, Domain(0));
      context->solution_crush().SetVarToValueIf(new_var, 0, NegatedRef(active));
    }

    context->AddEnforcedConstraint(reservoir_ct, [&](ConstraintProto* new_ct) {
      LinearConstraintProto* sum = new_ct->mutable_linear();
      FillDomainInProto(reservoir.min_level(), reservoir.max_level(), sum);
      for (int i = 0; i < num_events; ++i) {
        const int active = reservoir.active_literals().empty()
                               ? true_literal
                               : reservoir.active_literals(i);
        if (context->LiteralIsFalse(active)) continue;
        const LinearExpressionProto& demand = reservoir.level_changes(i);
        if (context->IsFixed(demand)) {
          const int64_t change =
              context->FixedValue(reservoir.level_changes(i));
          if (RefIsPositive(active)) {
            sum->add_vars(active);
            sum->add_coeffs(change);
          } else {
            // Add (1 - not(active)) * level_change.
            sum->add_vars(true_literal);
            sum->add_coeffs(change);
            sum->add_vars(NegatedRef(active));
            sum->add_coeffs(-change);
          }
        } else if (context->LiteralIsTrue(active)) {
          AddLinearExpressionToLinearConstraint(demand, 1, sum);
        } else {
          sum->add_vars(optional_demand_vars[i]);
          sum->add_coeffs(1);
        }
      }
    });

    context->UpdateRuleStats("reservoir: simple expansion with sum");
    context->ClearConstraint(c);
    return kOk;
  }

  // Call the correct expansion according to our parameter.
  if (context->params().expand_reservoir_using_circuit()) {
    return ExpandReservoirUsingCircuit(sum_of_positive_demand,
                                       sum_of_negative_demand, c, context);
  } else {
    // This one is the faster option usually.
    if (all_demands_are_fixed) {
      ExpandReservoirUsingPrecedences(
          sum_of_positive_demand > reservoir_ct.reservoir().max_level(),
          sum_of_negative_demand < reservoir_ct.reservoir().min_level(), c,
          context);
    } else {
      context->UpdateRuleStats(
          "reservoir: skipped expansion due to variable demands");
    }
  }
  return kOk;
}

// This is mainly used for testing the reservoir implementation.
FeasibilityStatus EncodeCumulativeAsReservoir(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  if (!context->IsFixed(ct.cumulative().capacity())) {
    context->UpdateRuleStats(
        "cumulative -> reservoir: expansion is not supported with variable "
        "capacity.");
    return kOk;
  }

  // ExpandReservoir() leaves a reservoir with variable level changes unexpanded
  // unless expand_reservoir_using_circuit is true. We keep the cumulative in
  // this case (the reservoir below has a min_level of kint64min).
  if (!context->params().expand_reservoir_using_circuit()) {
    for (const LinearExpressionProto& demand : ct.cumulative().demands()) {
      if (!context->IsFixed(demand)) {
        context->UpdateRuleStats(
            "cumulative -> reservoir: expansion is not supported with variable "
            "demands.");
        return kOk;
      }
    }
  }

  // Note that we know that the min_level can never go below zero, so we can
  // just ignore this part of the constraint here.
  ConstraintProto reservoir_ct;
  *reservoir_ct.mutable_enforcement_literal() = ct.enforcement_literal();
  auto* reservoir = reservoir_ct.mutable_reservoir();
  reservoir->set_min_level(kint64min);
  reservoir->set_max_level(context->FixedValue(ct.cumulative().capacity()));

  const int true_literal = context->GetTrueLiteral();
  const int num_intervals = ct.cumulative().intervals().size();
  for (int i = 0; i < num_intervals; ++i) {
    const auto& interval_ct = context->Constraint(ct.cumulative().intervals(i));
    const auto& interval = interval_ct.interval();
    *reservoir->add_time_exprs() = interval.start();
    *reservoir->add_time_exprs() = interval.end();

    const LinearExpressionProto& demand = ct.cumulative().demands(i);
    *reservoir->add_level_changes() = demand;
    LinearExpressionProto& negated = *reservoir->add_level_changes();
    negated.set_offset(-demand.offset());
    for (int j = 0; j < demand.vars().size(); ++j) {
      negated.add_vars(demand.vars(j));
      negated.add_coeffs(-demand.coeffs(j));
    }

    if (interval_ct.enforcement_literal().empty()) {
      reservoir->add_active_literals(true_literal);
      reservoir->add_active_literals(true_literal);
    } else {
      CHECK_EQ(interval_ct.enforcement_literal().size(), 1);
      reservoir->add_active_literals(interval_ct.enforcement_literal(0));
      reservoir->add_active_literals(interval_ct.enforcement_literal(0));
    }
  }

  // Now replace the cumulative with the reservoir and expand it.
  context->ReplaceConstraint(c, std::move(reservoir_ct));
  context->UpdateRuleStats("cumulative: expanded into reservoir");
  return ExpandReservoir(c, context);
}

FeasibilityStatus ExpandIntMod(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  const LinearArgumentProto& int_mod = ct.int_mod();
  const LinearExpressionProto& mod_expr = int_mod.exprs(1);
  if (context->IsFixed(mod_expr)) return kOk;

  const LinearExpressionProto& expr = int_mod.exprs(0);
  const LinearExpressionProto& target_expr = int_mod.target();
  EnforcedDomains enforced_domains(c, context);

  // We reduce the domain of target_expr to avoid later overflow.
  const EnforcedStatus status = enforced_domains.IntersectDomainWith(
      target_expr, context->DomainSuperSetOf(expr).PositiveModuloBySuperset(
                       context->DomainSuperSetOf(mod_expr)));
  if (!status.ok()) {
    return status.feasibility();
  }

  // div_expr = expr / mod_expr.
  const int div_var =
      context->NewIntVar(context->DomainSuperSetOf(expr).DivisionBySuperset(
          context->DomainSuperSetOf(mod_expr)));
  LinearExpressionProto div_expr;
  div_expr.add_vars(div_var);
  div_expr.add_coeffs(1);

  context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
    LinearArgumentProto* const div_proto = new_ct->mutable_int_div();
    *div_proto->mutable_target() = div_expr;
    *div_proto->add_exprs() = expr;
    *div_proto->add_exprs() = mod_expr;
  });

  // Create prod_expr = div_expr * mod_expr.
  const Domain prod_domain =
      context->DomainOf(div_var)
          .ContinuousMultiplicationBy(context->DomainSuperSetOf(mod_expr))
          .IntersectionWith(context->DomainSuperSetOf(expr).AdditionWith(
              context->DomainSuperSetOf(target_expr).Negation()));
  if (prod_domain.IsEmpty()) {
    return ExpandAlwaysFalseConstraint(c, context,
                                       "int_mod: empty target domain");
  }
  const int prod_var = context->NewIntVar(prod_domain);
  LinearExpressionProto prod_expr;
  prod_expr.add_vars(prod_var);
  prod_expr.add_coeffs(1);

  context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
    LinearArgumentProto* const int_prod = new_ct->mutable_int_prod();
    *int_prod->mutable_target() = prod_expr;
    *int_prod->add_exprs() = div_expr;
    *int_prod->add_exprs() = mod_expr;
  });

  // expr - prod_expr = target_expr.
  context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
    LinearConstraintProto* const lin = new_ct->mutable_linear();
    FillDomainInProto(0, lin);
    AddLinearExpressionToLinearConstraint(expr, 1, lin);
    AddLinearExpressionToLinearConstraint(prod_expr, -1, lin);
    AddLinearExpressionToLinearConstraint(target_expr, -1, lin);
  });

  context->solution_crush().SetIntModExpandedVars(
      ct, div_var, prod_var, context->MinOf(div_var), context->MinOf(prod_var));
  context->ClearConstraint(c);
  context->UpdateRuleStats("int_mod: expanded");
  return kOk;
}

void ExpandIntProd(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  if (ct.int_prod().exprs_size() <= 2) return;
  std::deque<LinearExpressionProto> terms(
      {ct.int_prod().exprs().begin(), ct.int_prod().exprs().end()});
  std::vector<int> new_vars;
  while (terms.size() > 2) {
    const LinearExpressionProto& left = terms[0];
    const LinearExpressionProto& right = terms[1];
    const Domain new_domain =
        context->DomainSuperSetOf(left).ContinuousMultiplicationBy(
            context->DomainSuperSetOf(right));
    const int new_var = context->NewIntVar(new_domain);
    new_vars.push_back(new_var);
    // TODO(user): since we copy the enforcement literals in the final int
    // prod constraint below, this is not strictly necessary. Is it better with
    // or without?
    LinearExpressionProto target_expr;
    target_expr.add_vars(new_var);
    target_expr.add_coeffs(1);
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      LinearArgumentProto* const int_prod = new_ct->mutable_int_prod();
      *int_prod->add_exprs() = left;
      *int_prod->add_exprs() = right;
      *int_prod->mutable_target() = target_expr;
    });
    terms.pop_front();
    terms.front() = std::move(target_expr);
  }

  context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
    LinearArgumentProto* const final_int_prod = new_ct->mutable_int_prod();
    *final_int_prod->add_exprs() = terms[0];
    *final_int_prod->add_exprs() = terms[1];
    *final_int_prod->mutable_target() = ct.int_prod().target();
  });

  context->solution_crush().SetIntProdExpandedVars(ct.int_prod(), new_vars);
  context->UpdateRuleStats(absl::StrCat(
      "int_prod: expanded int_prod with arity ", ct.int_prod().exprs_size()));
  context->ClearConstraint(c);
}

FeasibilityStatus ExpandInverse(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  DCHECK(ct.inverse().f_direct().empty());
  const auto& f_expr_direct = ct.inverse().f_expr_direct();
  const auto& f_expr_inverse = ct.inverse().f_expr_inverse();
  const int n = f_expr_direct.size();
  CHECK_EQ(n, f_expr_inverse.size());

  // Make sure the variable domains give expression domains in [0, n - 1].
  // Note that if a variable appears in several expressions, its domain will be
  // the intersection of the domains implied by each expression. In particular,
  // if there are two expressions "v" and "-v", the domain of v will be {0}.
  EnforcedDomains enforced_domains(c, context);
  for (const LinearExpressionProto& expr : f_expr_direct) {
    const EnforcedStatus status = enforced_domains.IntersectDomainWith(
        expr, Domain(0, n - 1),
        "Empty domain for a variable in ExpandInverse()");
    if (!status.ok()) {
      return status.feasibility();
    }
  }
  for (const LinearExpressionProto& expr : f_expr_inverse) {
    const EnforcedStatus status = enforced_domains.IntersectDomainWith(
        expr, Domain(0, n - 1),
        "Empty domain for a variable in ExpandInverse()");
    if (!status.ok()) {
      return status.feasibility();
    }
  }

  // Reduce the domains of each variable by checking that the inverse value
  // exists.
  std::vector<int64_t> possible_values;

  // Propagate from one vector to its counterpart.
  const auto filter_inverse_domain = [&enforced_domains, n, &possible_values](
                                         const auto& direct,
                                         const auto& inverse,
                                         bool& changed) -> EnforcedStatus {
    // Propagate from the inverse vector to the direct vector.
    for (int i = 0; i < n; ++i) {
      if (direct[i].vars().empty()) {
        const int64_t j = direct[i].offset();
        const EnforcedStatus status = enforced_domains.IntersectDomainWith(
            inverse[j], Domain(i),
            "Empty domain for a variable in ExpandInverse()", &changed);
        if (!status.ok()) {
          return status;
        }
      } else {
        possible_values.clear();
        bool removed_value = false;
        for (const int64_t i_var_value :
             enforced_domains.DomainOf(direct[i].vars(0)).Values()) {
          const int64_t j =
              direct[i].coeffs(0) * i_var_value + direct[i].offset();
          if (enforced_domains.DomainContains(inverse[j], i)) {
            possible_values.push_back(j);
          } else {
            removed_value = true;
          }
        }
        if (removed_value) {
          changed = true;
          const EnforcedStatus status = enforced_domains.IntersectDomainWith(
              direct[i], Domain::FromValues(possible_values),
              "Empty domain for a variable in ExpandInverse()");
          if (!status.ok()) {
            return status;
          }
        }
      }
    }
    return EnforcedStatus::Ok();
  };

  // Loop until fix point (several iterations might be needed if some
  // expressions share the same variable).
  for (int i = 0; i < 100; ++i) {  // Just to avoid potentially bad cases.
    bool changed = false;
    const EnforcedStatus direct_status =
        filter_inverse_domain(f_expr_direct, f_expr_inverse, changed);
    if (!direct_status.ok()) {
      return direct_status.feasibility();
    }
    const EnforcedStatus inverse_status =
        filter_inverse_domain(f_expr_inverse, f_expr_direct, changed);
    if (!inverse_status.ok()) {
      return inverse_status.feasibility();
    }
    if (!changed) break;
  }

  enforced_domains.MaybeAddEnforcedDomainConstraints();

  // Expand the inverse constraint by associating literal to var == value
  // and sharing them between the direct and inverse variables.
  //
  // Note that this is only correct because the domains are tight now.
  for (int i = 0; i < n; ++i) {
    const LinearExpressionProto& f_i = f_expr_direct[i];
    // Fixed expressions are already handled by the propagation above.
    if (f_i.vars().empty()) continue;
    const int f_i_var = f_i.vars(0);
    const int64_t f_i_coeff = f_i.coeffs(0);
    const int64_t f_i_offset = f_i.offset();
    for (const int64_t f_i_var_value :
         enforced_domains.DomainOf(f_i_var).Values()) {
      const int64_t j = f_i_coeff * f_i_var_value + f_i_offset;
      const LinearExpressionProto& r_j = f_expr_inverse[j];
      if (r_j.vars().empty()) continue;
      const int r_j_var = r_j.vars(0);
      const int64_t r_j_coeff = r_j.coeffs(0);
      const int64_t r_j_offset = r_j.offset();
      // We have f_i_var * f_i_coeff + f_i_offset == j <=>
      //         r_j_var * r_j_coeff + r_j_offset == i
      // which is equivalent to
      //         f_i_var == f_i_var_value <=>
      //         r_j_var == r_j_var_value = (i - r_j_offset) / r_j_coeff
      DCHECK_EQ((i - r_j_offset) % r_j_coeff, 0);
      const int64_t r_j_var_value = (i - r_j_offset) / r_j_coeff;
      if (ct.enforcement_literal().empty()) {
        int r_j_i;
        if (enforced_domains.DomainContains(r_j, i) &&
            context->VarCanTakeValue(r_j_var, r_j_var_value) &&
            context->HasVarValueEncoding(r_j_var, r_j_var_value, &r_j_i)) {
          if (context->InsertVarValueEncoding(r_j_i, f_i_var, f_i_var_value)
                  .unsat()) {
            return kUnsat;
          }
        } else {
          const int f_i_j =
              context->GetOrCreateVarValueEncoding(f_i_var, f_i_var_value);
          if (context->InsertVarValueEncoding(f_i_j, r_j_var, r_j_var_value)
                  .unsat()) {
            return kUnsat;
          }
        }
      } else {
        // enforcement_literal && f_i_var == f_i_var_value =>
        //     r_j_var == r_j_var_value
        // enforcement_literal && r_j_var == r_j_var_value =>
        //     f_i_var == f_i_var_value
        const int f_i_j =
            context->GetOrCreateVarValueEncoding(f_i_var, f_i_var_value);
        const int r_j_i =
            context->GetOrCreateVarValueEncoding(r_j_var, r_j_var_value);
        if (f_i_j != r_j_i) {
          context->AddEnforcedConstraint(
              ct, [f_i_j, r_j_i](ConstraintProto* new_ct) {
                new_ct->add_enforcement_literal(f_i_j);
                new_ct->mutable_bool_and()->add_literals(r_j_i);
              });
          context->AddEnforcedConstraint(
              ct, [r_j_i, f_i_j](ConstraintProto* new_ct) {
                new_ct->add_enforcement_literal(r_j_i);
                new_ct->mutable_bool_and()->add_literals(f_i_j);
              });
        }
      }
    }
  }

  context->ClearConstraint(c);
  context->UpdateRuleStats("inverse: expanded");
  return kOk;
}

void ExpandLinMax(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  const int num_exprs = ct.lin_max().exprs().size();
  if (num_exprs < 2) return;

  // We have a special treatment for Abs, Earliness, Tardiness, and all
  // affine_max where there is only one variable present in all the expressions.
  if (ExpressionsContainsOnlyOneVar(ct.lin_max().exprs())) return;

  // We will create 2 * num_exprs constraints for target = max(a1, ..., an).

  // First.
  // - enforcement literals => target >= ai
  for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      LinearConstraintProto* lin = new_ct->mutable_linear();
      FillDomainInProto(0, kint64max, lin);
      AddLinearExpressionToLinearConstraint(ct.lin_max().target(), 1, lin);
      AddLinearExpressionToLinearConstraint(expr, -1, lin);
    });
  }

  // Second, for each expr, create a new boolean bi, and add bi => target <= ai
  // With enforcement literals => exactly_one(bi)
  std::vector<int> enforcement_literals;
  enforcement_literals.reserve(num_exprs);
  if (num_exprs == 2 && ct.enforcement_literal().empty()) {
    const int new_bool = context->NewBoolVar("lin max expansion");
    enforcement_literals.push_back(new_bool);
    enforcement_literals.push_back(NegatedRef(new_bool));
  } else {
    for (int i = 0; i < num_exprs; ++i) {
      enforcement_literals.push_back(context->NewBoolVar("lin max expansion"));
    }
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      *new_ct->mutable_exactly_one()->mutable_literals() = {
          enforcement_literals.begin(), enforcement_literals.end()};
    });
  }

  for (int i = 0; i < num_exprs; ++i) {
    context->AddEnforcedConstraint(
        {enforcement_literals[i]}, [&](ConstraintProto* new_ct) {
          LinearConstraintProto* lin = new_ct->mutable_linear();
          FillDomainInProto(kint64min, 0, lin);
          AddLinearExpressionToLinearConstraint(ct.lin_max().target(), 1, lin);
          AddLinearExpressionToLinearConstraint(ct.lin_max().exprs(i), -1, lin);
        });
  }

  context->solution_crush().SetLinMaxExpandedVars(ct.lin_max(),
                                                  enforcement_literals);
  context->UpdateRuleStats("lin_max: expanded lin_max");
  context->ClearConstraint(c);
}

// A[V] == V means for all i, V == i => A_i == i
void ExpandElementWhenTargetShareVarWithIndex(
    int c, PresolveContext* context, const Domain& reduced_index_var_domain) {
  const ConstraintProto& ct = context->Constraint(c);
  const ElementConstraintProto& element = ct.element();

  const LinearExpressionProto& index = element.linear_index();
  DCHECK_EQ(index.vars_size(), 1);
  const int index_var = index.vars(0);
  const LinearExpressionProto& target = element.linear_target();
  DCHECK_EQ(target.vars_size(), 1);
  DCHECK_EQ(target.vars(0), index_var);

  for (const int64_t v : reduced_index_var_domain.Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    const int64_t target_value = AffineExpressionValueAt(target, v);
    const LinearExpressionProto& expr = element.exprs(index_value);
    const int index_lit = context->GetOrCreateVarValueEncoding(index_var, v);
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      new_ct->add_enforcement_literal(index_lit);
      FillDomainInProto(target_value, new_ct->mutable_linear());
      AddLinearExpressionToLinearConstraint(expr, 1, new_ct->mutable_linear());
    });
  }

  context->UpdateRuleStats(
      "element: expanded when the index and the target share the same var");
  context->ClearConstraint(c);
}

// Special case if the array of the element is filled with constant values.
FeasibilityStatus ExpandConstantArrayElement(
    int c, PresolveContext* context, const Domain& reduced_index_var_domain) {
  const ConstraintProto& ct = context->Constraint(c);
  const ElementConstraintProto& element = ct.element();
  const LinearExpressionProto& index = element.linear_index();
  DCHECK_EQ(index.vars_size(), 1);
  const int index_var = index.vars(0);
  const LinearExpressionProto& target = element.linear_target();

  absl::btree_map<int64_t, std::vector<int>> support_literals;
  std::vector<int64_t> invalid_index_values;
  for (const int64_t v : reduced_index_var_domain.Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    const int64_t expr_value = context->FixedValue(element.exprs(index_value));
    if (!context->DomainContains(target, expr_value)) {
      if (ct.enforcement_literal().empty()) {
        invalid_index_values.push_back(v);
      } else {
        const int index_lit =
            context->GetOrCreateVarValueEncoding(index_var, v);
        context->AddEnforcedConstraint(
            ct, [index_lit](ConstraintProto* new_ct) {
              new_ct->mutable_bool_and()->add_literals(NegatedRef(index_lit));
            });
        context->UpdateRuleStats("element: value not in target domain");
      }
    } else {
      const int index_lit = context->GetOrCreateVarValueEncoding(index_var, v);
      support_literals[expr_value].push_back(index_lit);
    }
  }

  if (!invalid_index_values.empty()) {
    if (context
            ->IntersectDomainWith(
                index_var,
                Domain::FromValues(invalid_index_values).Complement())
            .unsat()) {
      VLOG(1) << "Empty domain for the index variable in "
                 "ExpandConstantArrayElement()";
      return kUnsat;
    }
    context->UpdateRuleStats("element: reduce index domain during expansion");
  }

  for (const auto& [expr_value, index_var_literals] : support_literals) {
    const int target_literal =
        context->GetOrCreateAffineValueEncoding(target, expr_value);
    if (index_var_literals.size() == 1 && ct.enforcement_literal().empty()) {
      if (context
              ->StoreBooleanEqualityRelation(target_literal,
                                             index_var_literals[0])
              .unsat()) {
        return kUnsat;
      }
    } else {
      // enforcement => exactly_one(target != expr_value, index in support)
      context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
        new_ct->mutable_exactly_one()->add_literals(NegatedRef(target_literal));
        for (const int64_t lit : index_var_literals) {
          new_ct->mutable_exactly_one()->add_literals(lit);
        }
      });
    }
  }

  context->UpdateRuleStats("element: expanded value element");
  context->ClearConstraint(c);
  return kOk;
}

// General element when the array contains non fixed variables.
FeasibilityStatus ExpandVariableElement(
    int c, PresolveContext* context, const Domain& reduced_index_var_domain) {
  DeterministicTimer<{.scale = 0.0}> timer(context->time_limit(),
                                           reduced_index_var_domain.Size());
  const ConstraintProto& ct = context->Constraint(c);
  const ElementConstraintProto& element = ct.element();
  const LinearExpressionProto& index = element.linear_index();
  DCHECK_EQ(index.vars_size(), 1);
  const int index_var = index.vars(0);
  const LinearExpressionProto& target = element.linear_target();

  // Uniqueness is only valid if presolve is on.
  const bool presolve_is_on = context->params().cp_model_presolve();

  if (presolve_is_on && ct.enforcement_literal().empty() &&
      context->VariableIsUniqueAndRemovable(index_var)) {
    // Check index_var does not appear in the array of expressions.
    bool is_unique = true;
    for (int i = 0; i < element.exprs_size(); ++i) {
      if (AffineExpressionContainsVar(element.exprs(i), index_var)) {
        is_unique = false;
        break;
      }
    }
    if (is_unique) {
      const int64_t min_index = reduced_index_var_domain.Min();
      ConstraintProto* mapping_ct =
          context->NewMappingConstraint(__FILE__, __LINE__);
      mapping_ct->mutable_linear()->add_vars(index_var);
      mapping_ct->mutable_linear()->add_coeffs(1);
      int64_t mapping_offset = min_index;

      std::vector<int> exo_literals;
      exo_literals.reserve(reduced_index_var_domain.Size());
      for (int i = 0; i < reduced_index_var_domain.Size(); ++i) {
        exo_literals.push_back(
            context->NewBoolVar("element with unused index expansion"));
      }
      context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
        *new_ct->mutable_exactly_one()->mutable_literals() = {
            exo_literals.begin(), exo_literals.end()};
      });

      int exo_idx = 0;
      for (const int64_t v : reduced_index_var_domain.Values()) {
        const int literal = exo_literals[exo_idx++];
        context->solution_crush().MaybeSetLiteralToValueEncoding(literal,
                                                                 index_var, v);

        // Mapping constraint.
        const int64_t mapping_coeff = (v - min_index);
        if (mapping_coeff != 0) {
          if (RefIsPositive(literal)) {
            mapping_ct->mutable_linear()->add_vars(literal);
            mapping_ct->mutable_linear()->add_coeffs(-mapping_coeff);
          } else {
            mapping_offset += mapping_coeff;
            mapping_ct->mutable_linear()->add_vars(PositiveRef(literal));
            mapping_ct->mutable_linear()->add_coeffs(mapping_coeff);
          }
        }

        // Element expansion.
        const int64_t index_value = AffineExpressionValueAt(index, v);
        DCHECK_GE(index_value, 0);
        DCHECK_LT(index_value, element.exprs_size());
        const LinearExpressionProto& expr = element.exprs(index_value);
        context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
          new_ct->add_enforcement_literal(literal);
          FillDomainInProto(0, new_ct->mutable_linear());
          AddLinearExpressionToLinearConstraint(target, -1,
                                                new_ct->mutable_linear());
          AddLinearExpressionToLinearConstraint(expr, 1,
                                                new_ct->mutable_linear());
        });
      }

      mapping_ct->mutable_linear()->add_domain(mapping_offset);
      mapping_ct->mutable_linear()->add_domain(mapping_offset);

      context->MarkVariableAsRemoved(index_var);
      context->UpdateRuleStats(
          "element: expanded variable element with unused index");
      context->ClearConstraint(c);
      return kOk;
    }
  }

  if (presolve_is_on && !context->IsFixed(target) &&
      context->VariableIsUniqueAndRemovable(target.vars(0))) {
    DCHECK_EQ(target.vars_size(), 1);
    const int target_var = target.vars(0);
    bool domain_is_exact = true;
    const Domain target_domain =
        context->DomainOf(target_var)
            .MultiplicationBy(target.coeffs(0), &domain_is_exact)
            .AdditionWith(Domain(target.offset()));
    // Check target_var does not appear in the array of expressions.
    bool is_unique = true;
    for (int i = 0; i < element.exprs_size(); ++i) {
      if (AffineExpressionContainsVar(element.exprs(i), target_var)) {
        is_unique = false;
        break;
      }
    }

    if (domain_is_exact && is_unique) {
      for (const int64_t v : reduced_index_var_domain.Values()) {
        const int64_t index_value = AffineExpressionValueAt(index, v);
        DCHECK_GE(index_value, 0);
        DCHECK_LT(index_value, element.exprs_size());
        const int index_lit =
            context->GetOrCreateVarValueEncoding(index_var, v);
        const LinearExpressionProto& expr = element.exprs(index_value);

        context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
          new_ct->add_enforcement_literal(index_lit);
          FillDomainInProto(target_domain, new_ct->mutable_linear());
          AddLinearExpressionToLinearConstraint(expr, 1,
                                                new_ct->mutable_linear());
        });
      }
      context->UpdateRuleStats(
          "element: expanded variable element with unused target");
      context->MarkVariableAsRemoved(target_var);
      context->NewMappingConstraint(ct, __FILE__, __LINE__);
      context->ClearConstraint(c);
      return kOk;
    } else {
      context->UpdateRuleStats("TODO element: target not used elsewhere");
    }
  }

  const auto add_imply_eq_value_ct = [&ct, context](
                                         const LinearExpressionProto& expr,
                                         int64_t value, int literal) {
    int expr_lit;
    const bool has_encoding =
        context->HasAffineValueEncoding(expr, value, &expr_lit);
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      new_ct->add_enforcement_literal(literal);
      if (has_encoding) {
        new_ct->mutable_bool_and()->add_literals(expr_lit);
      } else {
        FillDomainInProto(value, new_ct->mutable_linear());
        AddLinearExpressionToLinearConstraint(expr, 1,
                                              new_ct->mutable_linear());
      }
    });
  };

  std::vector<int64_t> invalid_index_values;
  for (const int64_t v : reduced_index_var_domain.Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    DCHECK_GE(index_value, 0);
    DCHECK_LT(index_value, element.exprs_size());

    const LinearExpressionProto& expr = element.exprs(index_value);
    const bool index_is_valid =
        context->IntersectionOfAffineExprsIsNotEmpty(target, expr);

    if (!index_is_valid) {
      if (ct.enforcement_literal().empty()) {
        invalid_index_values.push_back(v);
      } else {
        const int index_lit =
            context->GetOrCreateVarValueEncoding(index_var, v);
        context->AddEnforcedConstraint(
            ct, [index_lit](ConstraintProto* new_ct) {
              new_ct->mutable_bool_and()->add_literals(NegatedRef(index_lit));
            });
        context->UpdateRuleStats("element: value not in target domain");
      }
    } else {
      const int index_lit = context->GetOrCreateVarValueEncoding(index_var, v);
      if (context->IsFixed(target)) {
        if (context->IsFixed(expr)) {
          DCHECK_EQ(context->FixedValue(target), context->FixedValue(expr));
        } else {
          add_imply_eq_value_ct(expr, context->FixedValue(target), index_lit);
        }
      } else if (context->IsFixed(expr)) {
        add_imply_eq_value_ct(target, context->FixedValue(expr), index_lit);
      } else {  // target and expr are not fixed.
        context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
          new_ct->add_enforcement_literal(index_lit);
          FillDomainInProto(0, new_ct->mutable_linear());
          AddLinearExpressionToLinearConstraint(target, -1,
                                                new_ct->mutable_linear());
          AddLinearExpressionToLinearConstraint(expr, 1,
                                                new_ct->mutable_linear());
        });
      }
    }
  }

  if (!invalid_index_values.empty()) {
    if (context
            ->IntersectDomainWith(
                index_var,
                Domain::FromValues(invalid_index_values).Complement())
            .unsat()) {
      return ExpandAlwaysFalseConstraint(
          c, context,
          "Empty domain for the index variable in ExpandArrayElement()");
    }
    context->UpdateRuleStats("element: reduce index domain during expansion");
  }
  VLOG(3) << "Expanded element: |index| = " << reduced_index_var_domain.Size()
          << " |target| = "
          << (target.vars().empty() ? 1
                                    : context->DomainOf(target.vars(0)).Size());

  context->UpdateRuleStats("element: expanded");
  context->ClearConstraint(c);
  return kOk;
}

FeasibilityStatus ExpandElement(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  const ElementConstraintProto& element = ct.element();

  const LinearExpressionProto& index = element.linear_index();
  const LinearExpressionProto& target = element.linear_target();
  const int size = element.exprs_size();

  // Reduce the domain of the index to be compatible with the array of
  // variables. Note that the element constraint is 0 based.
  const Domain reduced_index_var_domain =
      index.vars_size() == 1
          ? context->DomainOf(index.vars(0))
                .IntersectionWith(Domain(0, size - 1)
                                      .AdditionWith(Domain(-index.offset()))
                                      .InverseMultiplicationBy(index.coeffs(0)))
          : Domain();

  if (ct.enforcement_literal().empty()) {
    if (context->IntersectDomainWith(index, Domain(0, size - 1)).unsat()) {
      VLOG(1) << "Empty domain for the index variable in ExpandElement()";
      return kUnsat;
    }
  } else {
    const bool reduced_index_domain_is_empty =
        index.vars_size() == 1 ? reduced_index_var_domain.IsEmpty()
                               : (index.offset() < 0 || index.offset() >= size);
    if (reduced_index_domain_is_empty) {
      return ExpandAlwaysFalseConstraint(c, context);
    }
    // enforcement_literal => index in [0, size - 1]
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      FillDomainInProto(0, size - 1, new_ct->mutable_linear());
      AddLinearExpressionToLinearConstraint(index, 1, new_ct->mutable_linear());
    });
  }

  const bool reduced_index_domain_is_fixed =
      index.vars_size() == 0 || reduced_index_var_domain.IsFixed();
  if (reduced_index_domain_is_fixed) {
    DCHECK(!ct.enforcement_literal().empty() || context->IsFixed(index));
    const int64_t reduced_index_fixed_value =
        index.vars_size() == 0
            ? index.offset()
            : AffineExpressionValueAt(index,
                                      reduced_index_var_domain.FixedValue());
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      FillDomainInProto(0, new_ct->mutable_linear());
      AddLinearExpressionToLinearConstraint(target, 1,
                                            new_ct->mutable_linear());
      AddLinearExpressionToLinearConstraint(
          ct.element().exprs(reduced_index_fixed_value), -1,
          new_ct->mutable_linear());
    });
    context->UpdateRuleStats("element: expanded with fixed index");
    context->ClearConstraint(c);
    return kOk;
  }

  // Special case when index.var = target.var.
  if (index.vars_size() == 1 && target.vars_size() == 1 &&
      index.vars(0) == target.vars(0)) {
    ExpandElementWhenTargetShareVarWithIndex(c, context,
                                             reduced_index_var_domain);
    return kOk;
  }

  // Checks if all elements are constant.
  bool all_constants = true;
  for (const int64_t v : reduced_index_var_domain.Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    if (!context->IsFixed(element.exprs(index_value))) {
      all_constants = false;
      break;
    }
  }

  if (all_constants) {
    return ExpandConstantArrayElement(c, context, reduced_index_var_domain);
  } else {
    return ExpandVariableElement(c, context, reduced_index_var_domain);
  }
}

// Adds clauses so that:
//  enforcement_literals && literals[i] true => encoding[values[i]] true
//  enforcement_literals => one of literals[i in I(j)] true || encoding[j] false
// where I(j) = {i | values[i] = j}. This also implicitly uses the fact that
// exactly one literal is true. Note that we will use exactly_one in the
// encoding if possible.
FeasibilityStatus LinkLiteralsAndValues(
    absl::Span<const int> enforcement_literals, absl::Span<const int> literals,
    absl::Span<const int64_t> values,
    const absl::flat_hash_map<int64_t, int>& encoding,
    PresolveContext* context) {
  CHECK_EQ(literals.size(), values.size());

  // We use a map to make this method deterministic.
  //
  // TODO(user): Make sure this does not appear in the profile.
  absl::btree_map<int, std::vector<int>> encoding_lit_to_support;

  for (int i = 0; i < values.size(); ++i) {
    encoding_lit_to_support[encoding.at(values[i])].push_back(literals[i]);
  }

  // Using an exactly one conveys more structure and has a better linear
  // relaxation. Even if we could theoretically infer it back from the other
  // encoding.
  for (const auto& [encoding_lit, support] : encoding_lit_to_support) {
    CHECK(!support.empty());
    if (support.size() == 1 && enforcement_literals.empty()) {
      if (context->StoreBooleanEqualityRelation(encoding_lit, support[0])
              .unsat()) {
        return kUnsat;
      }
    } else {
      context->AddEnforcedConstraint(
          enforcement_literals, [&](ConstraintProto* new_ct) {
            BoolArgumentProto* exo = new_ct->mutable_exactly_one();
            exo->add_literals(NegatedRef(encoding_lit));
            for (const int lit : support) {
              exo->add_literals(lit);
            }
          });
    }
  }
  return kOk;
}

// Add the constraint enforcement_literals && literal => one_of(encoding[v]),
// for v in reachable_values. Note that all possible values are the ones
// appearing in encoding.
void AddImplyInReachableValues(
    absl::Span<const int> enforcement_literals, int literal,
    std::vector<int64_t>& reachable_values,
    const absl::flat_hash_map<int64_t, int>& encoding,
    PresolveContext* context) {
  gtl::STLSortAndRemoveDuplicates(&reachable_values);
  if (reachable_values.size() == encoding.size()) return;  // No constraint.
  if (reachable_values.size() <= encoding.size() / 2) {
    // Bool or encoding.
    context->AddEnforcedConstraint(
        enforcement_literals, [&](ConstraintProto* new_ct) {
          new_ct->add_enforcement_literal(literal);
          BoolArgumentProto* bool_or = new_ct->mutable_bool_or();
          for (const int64_t v : reachable_values) {
            bool_or->add_literals(encoding.at(v));
          }
        });
  } else {
    // Bool and encoding.
    absl::flat_hash_set<int64_t> set(reachable_values.begin(),
                                     reachable_values.end());
    context->AddEnforcedConstraint(
        enforcement_literals, [&](ConstraintProto* new_ct) {
          new_ct->add_enforcement_literal(literal);
          BoolArgumentProto* bool_and = new_ct->mutable_bool_and();
          for (const auto [value, enc_lit] : encoding) {
            if (!set.contains(value)) {
              bool_and->add_literals(NegatedRef(enc_lit));
            }
          }
        });
  }
}

FeasibilityStatus ExpandAutomaton(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  const AutomatonConstraintProto& proto = ct.automaton();

  if (proto.exprs_size() == 0) {
    const int64_t initial_state = proto.starting_state();
    for (const int64_t final_state : proto.final_states()) {
      if (initial_state == final_state) {
        context->UpdateRuleStats("automaton: empty and trivially feasible");
        context->ClearConstraint(c);
        return kOk;
      }
    }

    return ExpandAlwaysFalseConstraint(
        c, context,
        "automaton: empty with an initial state not in the final states.");
  } else if (proto.transition_label_size() == 0) {
    return ExpandAlwaysFalseConstraint(
        c, context, "automaton: non-empty with no transition.");
  }

  std::vector<absl::flat_hash_set<int64_t>> reachable_states;
  std::vector<absl::flat_hash_set<int64_t>> reachable_labels;
  PropagateAutomaton(proto, *context, &reachable_states, &reachable_labels);

  // We will model at each time step the current automaton state using Boolean
  // variables. We will have n+1 time steps. At time zero, we start in the
  // initial state, and at time n we should be in one of the final states. We
  // don't need to create Booleans at times when there is just one possible
  // state (like at time zero).
  absl::flat_hash_map<int64_t, int> encoding;
  absl::flat_hash_map<int64_t, int> in_encoding;
  absl::flat_hash_map<int64_t, int> out_encoding;
  bool removed_values = false;
  EnforcedDomains enforced_domains(c, context);

  const int n = proto.exprs_size();
  std::vector<SolutionCrush::StateVar> new_state_vars;
  std::vector<SolutionCrush::TransitionVar> new_transition_vars;
  for (int time = 0; time < n; ++time) {
    if (context->time_limit()->LimitReached()) return kOk;
    // All these vectors have the same size. We will use them to enforce a
    // local table constraint representing one step of the automaton at the
    // given time.
    std::vector<int64_t> in_states;
    std::vector<int64_t> labels;
    std::vector<int64_t> out_states;
    absl::flat_hash_set<int64_t> still_reachable_after_domain_change;
    for (int i = 0; i < proto.transition_label_size(); ++i) {
      const int64_t tail = proto.transition_tail(i);
      const int64_t label = proto.transition_label(i);
      const int64_t head = proto.transition_head(i);

      if (!reachable_states[time].contains(tail)) continue;
      if (!reachable_states[time + 1].contains(head)) continue;
      if (!enforced_domains.DomainContains(proto.exprs(time), label)) continue;

      still_reachable_after_domain_change.insert(head);
      // TODO(user): if this transition corresponds to just one in-state or
      // one-out state or one variable value, we could reuse the corresponding
      // Boolean variable instead of creating a new one!
      in_states.push_back(tail);
      labels.push_back(label);

      // On the last step we don't need to distinguish the output states, so
      // we use zero.
      out_states.push_back(time + 1 == n ? 0 : head);
    }

    reachable_states[time + 1] = still_reachable_after_domain_change;

    // Deal with single tuple.
    const int num_tuples = in_states.size();
    if (num_tuples == 1) {
      const EnforcedStatus expr_status = enforced_domains.IntersectDomainWith(
          proto.exprs(time), Domain(labels.front()), "Infeasible automaton.");
      if (!expr_status.ok()) {
        return expr_status.feasibility();
      }

      // Tricky: when the same variable is used more than once, the propagation
      // above might not reach the fixed point, so we do need to fix literal
      // at false.
      std::vector<int> at_false;
      for (const auto [value, literal] : in_encoding) {
        if (value != in_states[0]) {
          const EnforcedStatus lit_status =
              enforced_domains.SetLiteralToFalse(literal);
          if (!lit_status.ok()) {
            return lit_status.feasibility();
          }
        }
      }

      in_encoding.clear();
      continue;
    }

    // Fully encode vars[time].
    {
      std::vector<int64_t> transitions = labels;
      const LinearExpressionProto& expr = proto.exprs(time);
      gtl::STLSortAndRemoveDuplicates(&transitions);

      encoding.clear();
      const EnforcedStatus status = enforced_domains.IntersectDomainWith(
          expr, Domain::FromValues(transitions), "Infeasible automaton.",
          &removed_values);
      if (!status.ok()) {
        return status.feasibility();
      }

      // Fully encode the variable.
      // We can leave the encoding empty for fixed vars.
      if (!enforced_domains.IsFixed(expr)) {
        const int var = expr.vars(0);
        for (const int64_t v : enforced_domains.DomainOf(var).Values()) {
          encoding[AffineExpressionValueAt(expr, v)] =
              context->GetOrCreateVarValueEncoding(var, v);
        }
      }
    }

    // Count how many times each value appears.
    // We use this to reuse literals if possible.
    absl::flat_hash_map<int64_t, int> in_count;
    absl::flat_hash_map<int64_t, int> transition_count;
    absl::flat_hash_map<int64_t, int> out_count;
    for (int i = 0; i < num_tuples; ++i) {
      in_count[in_states[i]]++;
      transition_count[labels[i]]++;
      out_count[out_states[i]]++;
    }

    // For each possible out states, create one Boolean variable.
    //
    // TODO(user): Add exactly one?
    {
      std::vector<int64_t> states = out_states;
      gtl::STLSortAndRemoveDuplicates(&states);

      out_encoding.clear();
      if (states.size() == 2) {
        const int var = context->NewBoolVar("automaton expansion");
        new_state_vars.push_back({var, time + 1, states[0]});
        out_encoding[states[0]] = var;
        out_encoding[states[1]] = NegatedRef(var);
      } else if (states.size() > 2) {
        struct UniqueDetector {
          void Set(int64_t v) {
            if (!is_unique) return;
            if (is_set) {
              if (v != value) is_unique = false;
            } else {
              is_set = true;
              value = v;
            }
          }
          bool is_set = false;
          bool is_unique = true;
          int64_t value = 0;
        };

        // Optimization to detect if we have an in state that is only matched to
        // a single out state. Same with transition.
        absl::flat_hash_map<int64_t, UniqueDetector> out_to_in;
        absl::flat_hash_map<int64_t, UniqueDetector> out_to_transition;
        for (int i = 0; i < num_tuples; ++i) {
          out_to_in[out_states[i]].Set(in_states[i]);
          out_to_transition[out_states[i]].Set(labels[i]);
        }

        for (const int64_t state : states) {
          // If we have a relation in_state <=> out_state, then we can reuse
          // the in Boolean and do not need to create a new one.
          if (!in_encoding.empty() && out_to_in[state].is_unique) {
            const int64_t unique_in = out_to_in[state].value;
            if (in_count[unique_in] == out_count[state]) {
              out_encoding[state] = in_encoding[unique_in];
              continue;
            }
          }

          // Same if we have a unique transition value that corresponds only to
          // this state.
          if (!encoding.empty() && out_to_transition[state].is_unique) {
            const int64_t unique_transition = out_to_transition[state].value;
            if (transition_count[unique_transition] == out_count[state]) {
              out_encoding[state] = encoding[unique_transition];
              continue;
            }
          }

          out_encoding[state] = context->NewBoolVar("automaton expansion");
          new_state_vars.push_back({out_encoding[state], time + 1, state});
        }
      }
    }

    // Simple encoding. This is enough to properly enforce the constraint, but
    // it propagates less. It creates a lot fewer Booleans though. Note that we
    // use implicit "exactly one" on the encoding and do not add any extra
    // exactly one if the simple encoding is used.
    //
    // We currently decide which encoding to use depending on the number of new
    // literals needed by the "heavy" encoding compared to the number of states
    // and labels. When the automaton is small, using the full encoding is
    // better, see for instance on rotating-workforce_Example789 where the
    // simple encoding makes the problem hard to solve but the full encoding
    // allows the solver to solve it in a couple of seconds!
    //
    // Note that both encoding create about the same number of constraints.
    const int num_involved_variables =
        in_encoding.size() + encoding.size() + out_encoding.size();
    const bool use_light_encoding = (num_tuples > num_involved_variables);
    if (use_light_encoding && !in_encoding.empty() && !encoding.empty() &&
        !out_encoding.empty()) {
      // Part 1: If an in_state is selected, restrict the set of possible
      // labels. We also restrict the set of possible out states, but this is
      // not needed for correctness.
      absl::flat_hash_map<int64_t, std::vector<int64_t>> in_to_label;
      absl::flat_hash_map<int64_t, std::vector<int64_t>> in_to_out;
      for (int i = 0; i < num_tuples; ++i) {
        in_to_label[in_states[i]].push_back(labels[i]);
        in_to_out[in_states[i]].push_back(out_states[i]);
      }
      // Sort the pairs to make the order deterministic.
      std::vector<std::pair<int64_t, int>> in_to_label_pairs(
          in_encoding.begin(), in_encoding.end());
      absl::c_sort(in_to_label_pairs);
      for (const auto [in_value, in_literal] : in_to_label_pairs) {
        AddImplyInReachableValues(ct.enforcement_literal(), in_literal,
                                  in_to_label[in_value], encoding, context);
        AddImplyInReachableValues(ct.enforcement_literal(), in_literal,
                                  in_to_out[in_value], out_encoding, context);
      }

      // Part2, add all 3-clauses: enforcement_literal && (in_state, label) =>
      // out_state.
      for (int i = 0; i < num_tuples; ++i) {
        context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
          auto* bool_or = new_ct->mutable_bool_or();
          bool_or->add_literals(NegatedRef(in_encoding.at(in_states[i])));
          bool_or->add_literals(NegatedRef(encoding.at(labels[i])));
          bool_or->add_literals(out_encoding.at(out_states[i]));
        });
      }

      in_encoding.swap(out_encoding);
      out_encoding.clear();
      continue;
    }

    // Create the tuple literals.
    //
    // TODO(user): Call and use the same heuristics as the table constraint to
    // expand this small table with 3 columns (i.e. compress, negate, etc...).
    std::vector<int> tuple_literals;
    if (num_tuples == 2) {
      const int bool_var = context->NewBoolVar("automaton expansion");
      new_transition_vars.push_back({bool_var, time, in_states[0], labels[0]});
      tuple_literals.push_back(bool_var);
      tuple_literals.push_back(NegatedRef(bool_var));
    } else {
      // Note that we do not need the ExactlyOneConstraint(tuple_literals)
      // because it is already implicitly encoded since we have exactly one
      // transition value. But adding one seems to help.
      tuple_literals.reserve(num_tuples);
      for (int i = 0; i < num_tuples; ++i) {
        int tuple_literal;
        if (in_count[in_states[i]] == 1 && !in_encoding.empty()) {
          tuple_literal = in_encoding[in_states[i]];
        } else if (transition_count[labels[i]] == 1 && !encoding.empty()) {
          tuple_literal = encoding[labels[i]];
        } else if (out_count[out_states[i]] == 1 && !out_encoding.empty()) {
          tuple_literal = out_encoding[out_states[i]];
        } else {
          tuple_literal = context->NewBoolVar("automaton expansion");
          new_transition_vars.push_back(
              {tuple_literal, time, in_states[i], labels[i]});
        }

        tuple_literals.push_back(tuple_literal);
      }
      context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
        *new_ct->mutable_exactly_one()->mutable_literals() = {
            tuple_literals.begin(), tuple_literals.end()};
      });
    }

    if (!in_encoding.empty()) {
      if (LinkLiteralsAndValues(ct.enforcement_literal(), tuple_literals,
                                in_states, in_encoding, context)
              .unsat()) {
        return kUnsat;
      }
    }
    if (!encoding.empty()) {
      if (LinkLiteralsAndValues(ct.enforcement_literal(), tuple_literals,
                                labels, encoding, context)
              .unsat()) {
        return kUnsat;
      }
    }
    if (!out_encoding.empty()) {
      if (LinkLiteralsAndValues(ct.enforcement_literal(), tuple_literals,
                                out_states, out_encoding, context)
              .unsat()) {
        return kUnsat;
      }
    }

    in_encoding.swap(out_encoding);
    out_encoding.clear();
  }

  enforced_domains.MaybeAddEnforcedDomainConstraints();

  context->solution_crush().SetAutomatonExpandedVars(proto, new_state_vars,
                                                     new_transition_vars);
  if (removed_values) {
    context->UpdateRuleStats("automaton: reduced variable domains");
  }
  context->UpdateRuleStats("automaton: expanded");
  context->ClearConstraint(c);
  return kOk;
}

bool TableIsInCanonicalForm(const ConstraintProto& ct) {
  const TableConstraintProto& table = ct.table();
  if (!table.vars().empty()) {
    LOG(ERROR) << "Table is in the legacy format.";
    return false;
  }
  if (table.values().empty()) {
    if (table.exprs().empty()) {
      return true;
    }
    if (table.exprs_size() != 1) {
      LOG(ERROR) << "Table is empty but has more than one expression.";
      return false;
    }
    if (table.exprs(0).offset() != 0) {
      LOG(ERROR) << "Table is empty but has an expression with a non-zero "
                    "offset.";
      return false;
    }
    if (!table.exprs(0).vars().empty()) {
      LOG(ERROR) << "Table is empty but has an expression with a non-constant "
                    "coefficient.";
      return false;
    }
    return true;
  }
  for (const LinearExpressionProto& expr : table.exprs()) {
    if (expr.offset() != 0) {
      LOG(ERROR) << "Expression contains a non-zero offset.";
      return false;
    }
    if (expr.coeffs().size() == 1 && expr.coeffs(0) != 1) {
      LOG(ERROR) << "Expression contains a single variable with a coefficient "
                    "different from 1.";
      return false;
    }
    if (expr.vars().empty()) {
      LOG(ERROR) << "Constant expression.";
      return false;
    }
  }
  return true;
}

void ExpandNegativeTable(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  DCHECK(TableIsInCanonicalForm(ct));
  const TableConstraintProto& table = ct.table();
  if (table.values().empty()) {  // Early exit.
    context->UpdateRuleStats("table: empty negated constraint");
    context->ClearConstraint(c);
    return;
  }

  const int num_exprs = table.exprs_size();
  DCHECK_GT(num_exprs, 0);
  const int num_original_tuples = table.values_size() / num_exprs;
  std::vector<std::vector<int64_t>> tuples(num_original_tuples);
  int count = 0;
  for (int i = 0; i < num_original_tuples; ++i) {
    for (int j = 0; j < num_exprs; ++j) {
      tuples[i].push_back(table.values(count++));
    }
  }

  // Compress tuples.
  std::vector<int64_t> domain_sizes;
  for (int i = 0; i < num_exprs; ++i) {
    domain_sizes.push_back(context->DomainOf(table.exprs(i).vars(0)).Size());
  }
  CompressTuples(domain_sizes, &tuples);

  // For each tuple, forbid the variable values to be this tuple.
  std::vector<int> clause;
  for (const std::vector<int64_t>& tuple : tuples) {
    clause.clear();
    for (int i = 0; i < num_exprs; ++i) {
      const int64_t value = tuple[i];
      if (value == kTableAnyValue) continue;

      const int literal =
          context->GetOrCreateVarValueEncoding(table.exprs(i).vars(0), value);
      clause.push_back(NegatedRef(literal));
    }

    // Note: if the clause is empty, then the model is infeasible.
    context->AddEnforcedConstraint(ct, [&clause](ConstraintProto* new_ct) {
      *new_ct->mutable_bool_or()->mutable_literals() = {clause.begin(),
                                                        clause.end()};
    });
  }
  context->UpdateRuleStats("table: expanded negated constraint");
  context->ClearConstraint(c);
}

// Add the implications and clauses to link one variable (i.e. column) of a
// table to the literals controlling if the tuples are possible or not.
//
// We list for each tuple the possible values the variable can take.
// If the list is empty, then this encodes "any value".
void ProcessOneCompressedColumn(
    int variable, absl::Span<const int> tuple_literals,
    absl::Span<const absl::InlinedVector<int64_t, 2>> values,
    std::optional<int> table_is_active_literal, PresolveContext* context) {
  DCHECK_EQ(tuple_literals.size(), values.size());

  // Some pre-computations.
  // Collect pairs of value-literal.
  absl::flat_hash_set<int64_t> value_is_multiple;
  std::vector<int> any_values_literals;
  std::vector<std::pair<int64_t, int>> pairs;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i].empty()) {
      any_values_literals.push_back(tuple_literals[i]);
      continue;
    }
    for (const int64_t v : values[i]) {
      pairs.emplace_back(v, tuple_literals[i]);
    }
    if (values[i].size() > 1) {
      value_is_multiple.insert(values[i].begin(), values[i].end());
    }
  }

  // Try to use exactly one in the encoding if we can.
  bool use_exo = true;
  if (table_is_active_literal.has_value()) use_exo = false;
  if (!any_values_literals.empty()) use_exo = false;

  // Add the constraint literal => one of values.
  std::vector<int> encoded_lits;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i].empty()) continue;

    if (use_exo && values[i].size() == 1 &&
        !value_is_multiple.contains(values[i][0])) {
      // nothing to do here since the implication is covered by the exactly one.
      continue;
    }

    if (values[i].size() == 1) {
      // It is slightly better to use a bool_and if size is 1 instead of
      // reconverting it at a later stage.
      const int lit =
          context->GetOrCreateVarValueEncoding(variable, values[i][0]);
      context->AddEnforcedConstraint(
          {tuple_literals[i]}, [lit](ConstraintProto* new_ct) {
            new_ct->mutable_bool_and()->add_literals(lit);
          });
      continue;
    }

    // TODO(user): If we have n - 1 values, we could add the constraint that
    // tuple literal => not(last_value) instead?
    encoded_lits.clear();
    encoded_lits.reserve(values[i].size());
    for (const int64_t v : values[i]) {
      DCHECK(context->DomainOf(variable).Contains(v));
      encoded_lits.push_back(context->GetOrCreateVarValueEncoding(variable, v));
    }
    context->AddEnforcedConstraint(
        {tuple_literals[i]}, [&encoded_lits](ConstraintProto* new_ct) {
          *new_ct->mutable_bool_or()->mutable_literals() = {
              encoded_lits.begin(), encoded_lits.end()};
        });
  }

  // Regroup literal with the same value and add for each the clause: If all the
  // tuples containing a value are false, then this value must be false too.
  std::sort(pairs.begin(), pairs.end());
  for (int i = 0; i < pairs.size();) {
    const int64_t value = pairs[i].first;
    const int start_i = i;
    while (i < pairs.size() && pairs[i].first == value) ++i;
    const int end_i = i;
    const int value_literal =
        context->GetOrCreateVarValueEncoding(variable, value);

    // A value is supported if one tuple is still active, or a covering 'any'
    // tuple is still active, or the table can still be inactive.
    //
    // Note that if a value only appears individually in each tuple, and the
    // table is not enforced, then we have an exactly one. This seems to help a
    // bit, especially the linear relaxation.
    const bool make_exo = use_exo && !value_is_multiple.contains(value);
    context->AddConstraint([&](ConstraintProto* new_ct) {
      BoolArgumentProto* no_support =
          make_exo ? new_ct->mutable_exactly_one() : new_ct->mutable_bool_or();
      for (int k = start_i; k < end_i; ++k) {
        no_support->add_literals(pairs[k].second);
      }
      for (const int lit : any_values_literals) {
        no_support->add_literals(lit);
      }
      if (table_is_active_literal.has_value()) {
        no_support->add_literals(NegatedRef(table_is_active_literal.value()));
      }
      no_support->add_literals(NegatedRef(value_literal));
    });
  }
}

// Simpler encoding for table constraints with 2 variables.
FeasibilityStatus AddSizeTwoTable(
    absl::Span<const int> vars, absl::Span<const std::vector<int64_t>> tuples,
    absl::Span<const absl::flat_hash_set<int64_t>> values_per_var,
    PresolveContext* context) {
  CHECK_EQ(vars.size(), 2);
  const int left_var = vars[0];
  const int right_var = vars[1];
  if (context->DomainOf(left_var).IsFixed() ||
      context->DomainOf(right_var).IsFixed()) {
    // A table constraint with at most one variable not fixed is trivially
    // enforced after domain reduction.
    return kOk;
  }

  absl::btree_map<int, std::vector<int>> left_to_right;
  absl::btree_map<int, std::vector<int>> right_to_left;

  for (const auto& tuple : tuples) {
    const int64_t left_value(tuple[0]);
    const int64_t right_value(tuple[1]);
    DCHECK(context->DomainOf(left_var).Contains(left_value));
    DCHECK(context->DomainOf(right_var).Contains(right_value));

    const int left_literal =
        context->GetOrCreateVarValueEncoding(left_var, left_value);
    const int right_literal =
        context->GetOrCreateVarValueEncoding(right_var, right_value);
    left_to_right[left_literal].push_back(right_literal);
    right_to_left[right_literal].push_back(left_literal);
  }

  int num_implications = 0;
  int num_clause_added = 0;
  int num_large_clause_added = 0;
  int num_exo_added = 0;
  int num_equivalences_added = 0;
  auto add_support_constraint =
      [context, &num_clause_added, &num_large_clause_added, &num_implications,
       &num_exo_added, &num_equivalences_added](
          int lit, absl::Span<const int> support_literals, int max_support_size,
          const absl::btree_map<int, std::vector<int>>& other_map)
      -> FeasibilityStatus {
    if (support_literals.size() == max_support_size) return kOk;
    if (support_literals.size() == 1) {
      const int support_literal = support_literals.front();
      const auto& it = other_map.find(support_literal);
      CHECK(it != other_map.end());
      if (it->second.size() > 1) {
        context->AddImplication(lit, support_literal);
        num_implications++;
      } else {
        if (context->StoreBooleanEqualityRelation(lit, support_literal)
                .unsat()) {
          return kUnsat;
        }
        ++num_equivalences_added;
      }
    } else {
      bool exclusive = true;
      for (const int support_literal : support_literals) {
        const auto& it = other_map.find(support_literal);
        CHECK(it != other_map.end());
        if (it->second.size() > 1) {
          exclusive = false;
          break;
        }
      }
      if (exclusive) {
        context->AddConstraint([&](ConstraintProto* new_ct) {
          BoolArgumentProto* exo = new_ct->mutable_exactly_one();
          for (const int support_literal : support_literals) {
            exo->add_literals(support_literal);
          }
          exo->add_literals(NegatedRef(lit));
        });
        ++num_exo_added;
      } else {
        context->AddConstraint([&](ConstraintProto* new_ct) {
          BoolArgumentProto* bool_or = new_ct->mutable_bool_or();
          for (const int support_literal : support_literals) {
            bool_or->add_literals(support_literal);
          }
          bool_or->add_literals(NegatedRef(lit));
        });
        num_clause_added++;
        if (support_literals.size() > max_support_size / 2) {
          num_large_clause_added++;
        }
      }
    }
    return kOk;
  };

  for (const auto& it : left_to_right) {
    if (add_support_constraint(it.first, it.second, values_per_var[1].size(),
                               right_to_left)
            .unsat()) {
      return kUnsat;
    }
  }
  for (const auto& it : right_to_left) {
    if (add_support_constraint(it.first, it.second, values_per_var[0].size(),
                               left_to_right)
            .unsat()) {
      return kUnsat;
    }
  }
  VLOG(3) << "Table: 2 variables, " << tuples.size() << " tuples encoded using "
          << num_clause_added << " clauses, including "
          << num_large_clause_added << " large clauses, " << num_implications
          << " implications, " << num_exo_added << " exactly ones, "
          << num_equivalences_added << " equivalences.";
  return kOk;
}

// A "WCSP" (weighted constraint programming) problem is usually encoded as
// a set of tables, with one or more variables only there to carry a cost.
//
// If this is the case, we can do special presolving.
ModificationStatus ReduceTableInPresenceOfUniqueVariableWithCosts(
    std::vector<int>* vars, std::vector<std::vector<int64_t>>* tuples,
    PresolveContext* context) {
  const int num_vars = vars->size();

  std::vector<bool> only_here_and_in_objective(num_vars, false);
  std::vector<int64_t> objective_coeffs(num_vars, 0.0);
  std::vector<int> new_vars;
  std::vector<int> deleted_vars;
  for (int var_index = 0; var_index < num_vars; ++var_index) {
    const int var = (*vars)[var_index];

    // We do not use VariableWithCostIsUniqueAndRemovable() since this one
    // returns false if the objective is constraining but we don't care here.
    // Our transformation also does not lose solutions.
    if (context->VariableWithCostIsUnique(var)) {
      context->UpdateRuleStats("table: removed unused column with cost");
      only_here_and_in_objective[var_index] = true;
      objective_coeffs[var_index] =
          RefIsPositive(var) ? context->ObjectiveMap().at(var)
                             : -context->ObjectiveMap().at(PositiveRef(var));
      context->RemoveVariableFromObjective(var);
      context->MarkVariableAsRemoved(var);
      deleted_vars.push_back(var);
    } else if (context->VarToConstraints(var).size() == 1) {
      // If there is no cost, we can remove that variable using the same code by
      // just setting the cost to zero.
      context->UpdateRuleStats("table: removed unused column");
      only_here_and_in_objective[var_index] = true;
      objective_coeffs[var_index] = 0;
      context->MarkVariableAsRemoved(var);
      deleted_vars.push_back(var);
    } else {
      new_vars.push_back(var);
    }
  }
  if (new_vars.size() == num_vars) return kUnchanged;

  // Rewrite the tuples.
  // put the cost last.
  int64_t min_cost = kint64max;
  std::vector<int64_t> temp;
  for (int i = 0; i < tuples->size(); ++i) {
    int64_t cost = 0;
    int new_size = 0;
    temp.clear();
    for (int var_index = 0; var_index < num_vars; ++var_index) {
      const int64_t value = (*tuples)[i][var_index];
      if (only_here_and_in_objective[var_index]) {
        temp.push_back(value);
        const int64_t objective_coeff = objective_coeffs[var_index];
        cost += value * objective_coeff;
      } else {
        (*tuples)[i][new_size++] = value;
      }
    }
    (*tuples)[i].resize(new_size);
    (*tuples)[i].push_back(cost);
    min_cost = std::min(min_cost, cost);

    // Hack: we store the deleted value here so that we can properly encode
    // the postsolve constraints below.
    (*tuples)[i].insert((*tuples)[i].end(), temp.begin(), temp.end());
  }

  // Remove tuples that only differ by their cost.
  // Make sure we will assign the proper value of the removed variable at
  // postsolve.
  {
    int new_size = 0;
    const int old_size = tuples->size();
    std::sort(tuples->begin(), tuples->end());
    for (int i = 0; i < tuples->size(); ++i) {
      // If the prefix (up to new_vars.size()) is the same, skip this tuple.
      if (new_size > 0) {
        bool skip = true;
        for (int var_index = 0; var_index < new_vars.size(); ++var_index) {
          if ((*tuples)[i][var_index] != (*tuples)[new_size - 1][var_index]) {
            skip = false;
            break;
          }
        }
        if (skip) continue;
      }

      // If this tuple is selected, then fix the removed variable value in the
      // mapping model.
      for (int j = 0; j < deleted_vars.size(); ++j) {
        ConstraintProto* mapping_ct =
            context->NewMappingConstraint(__FILE__, __LINE__);
        for (int var_index = 0; var_index < new_vars.size(); ++var_index) {
          mapping_ct->add_enforcement_literal(
              context->GetOrCreateVarValueEncoding(new_vars[var_index],
                                                   (*tuples)[i][var_index]));
        }
        LinearConstraintProto* new_lin = mapping_ct->mutable_linear();
        new_lin->add_vars(deleted_vars[j]);
        new_lin->add_coeffs(1);
        FillDomainInProto((*tuples)[i][new_vars.size() + 1 + j], new_lin);
      }
      (*tuples)[i].resize(new_vars.size() + 1);
      (*tuples)[new_size++] = (*tuples)[i];
    }
    tuples->resize(new_size);
    if (new_size < old_size) {
      context->UpdateRuleStats(
          "table: removed duplicate tuples with different costs");
    }
  }

  if (min_cost > 0) {
    context->AddToObjectiveOffset(min_cost);
    context->UpdateRuleStats("table: transferred min_cost to objective offset");
    for (int i = 0; i < tuples->size(); ++i) {
      (*tuples)[i].back() -= min_cost;
    }
  }

  // This comes from the WCSP literature. Basically, if by fixing a variable to
  // a value, we have only tuples with a non-zero cost, we can subtract the
  // minimum cost of these tuples and transfer it to the variable cost.
  //
  // TODO(user): Doing this before table compression can prevent good
  // compression. We should probably exploit this during compression to make
  // sure we compress as much as possible, and once compressed, do it again. Or
  // do it in a more general IP settings when one literal implies that a set of
  // literals with >0 cost are in EXO. We can transfer the min of their cost to
  // that Boolean.
  if (/*DISABLES CODE*/ (false)) {
    for (int var_index = 0; var_index < new_vars.size(); ++var_index) {
      absl::flat_hash_map<int64_t, int64_t> value_to_min_cost;
      const int num_tuples = tuples->size();
      for (int i = 0; i < num_tuples; ++i) {
        const int64_t v = (*tuples)[i][var_index];
        const int64_t cost = (*tuples)[i].back();
        auto insert = value_to_min_cost.insert({v, cost});
        if (!insert.second) {
          insert.first->second = std::min(insert.first->second, cost);
        }
      }
      for (int i = 0; i < num_tuples; ++i) {
        const int64_t v = (*tuples)[i][var_index];
        (*tuples)[i].back() -= value_to_min_cost.at(v);
      }
      for (const auto entry : value_to_min_cost) {
        if (entry.second == 0) continue;
        context->UpdateRuleStats("table: transferred cost to encoding");
        const int value_literal = context->GetOrCreateVarValueEncoding(
            new_vars[var_index], entry.first);
        context->AddLiteralToObjective(value_literal, entry.second);
      }
    }
  }

  context->UpdateRuleStats(absl::StrCat(
      "table: expansion with column(s) only in objective. Arity = ",
      new_vars.size()));

  *vars = new_vars;
  return kModified;
}

// Important: the table and variable domains must be pre-solved before this
// is called. Some checks will fail otherwise.
void CompressAndExpandPositiveTable(int c, bool last_column_is_cost,
                                    absl::Span<const int> vars,
                                    std::vector<std::vector<int64_t>>* tuples,
                                    PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  const int num_tuples_before_compression = tuples->size();

  // If the last column is actually the tuple cost, we compress the table like
  // if this was a normal variable, but afterwards we treat it differently.
  std::vector<int64_t> domain_sizes;
  for (const int var : vars) {
    domain_sizes.push_back(context->DomainOf(var).Size());
  }
  if (last_column_is_cost) {
    domain_sizes.push_back(kint64max);
  }

  // We start by compressing the table with kTableAnyValue only.
  const int compression_level = context->params().table_compression_level();
  if (compression_level > 0) {
    CompressTuples(domain_sizes, tuples);
  }
  const int num_tuples_after_first_compression = tuples->size();

  // Tricky: If the table is big, it is better to compress it as much as
  // possible to reduce the number of created booleans. Otherwise, the more
  // verbose encoding can lead to better linear relaxation. Probably because the
  // tuple literal can encode each variable as sum literal * value. Also because
  // we have more direct implied bounds, which might lead to better cuts.
  //
  // For instance, on lot_sizing_cp_pigment15c.psp, compressing the table more
  // is a lot worse (at least until we can produce better cut).
  //
  // TODO(user): Tweak the heuristic, maybe compute the reduction achieved and
  // decide based on that.
  std::vector<std::vector<absl::InlinedVector<int64_t, 2>>> compressed_table;
  if (compression_level > 2 ||
      (compression_level == 2 && num_tuples_after_first_compression > 1000)) {
    compressed_table = FullyCompressTuples(domain_sizes, tuples);
    if (compressed_table.size() < num_tuples_before_compression) {
      context->UpdateRuleStats("table: fully compress tuples");
    }
  } else {
    // Convert the kTableAnyValue to an empty list format.
    for (int i = 0; i < tuples->size(); ++i) {
      compressed_table.push_back({});
      for (const int64_t v : (*tuples)[i]) {
        if (v == kTableAnyValue) {
          compressed_table.back().push_back({});
        } else {
          compressed_table.back().push_back({v});
        }
      }
    }
    if (compressed_table.size() < num_tuples_before_compression) {
      context->UpdateRuleStats("table: compress tuples");
    }
  }

  VLOG(3) << "Table compression"
          << " var=" << vars.size()
          << " cost=" << domain_sizes.size() - vars.size()
          << " tuples= " << num_tuples_before_compression << " -> "
          << num_tuples_after_first_compression << " -> "
          << compressed_table.size();

  // Affect mznc2017_aes_opt_r10 instance!
  std::sort(compressed_table.begin(), compressed_table.end());

  const int num_vars = vars.size();
  if (compressed_table.size() == 1 && ct.enforcement_literal().empty()) {
    // Domains are propagated. We can remove the constraint.
    context->UpdateRuleStats("table: one tuple");
    if (last_column_is_cost) {
      // TODO(user): Because we transfer the cost, this should always be zero,
      // so not needed.
      context->AddToObjectiveOffset(compressed_table[0].back()[0]);
    }
    return;
  }

  // Optimization. If a value is unique and appears alone in a cell, we can use
  // the encoding literal for this line tuple literal instead of creating a new
  // one.
  std::vector<bool> has_any(num_vars, false);
  std::vector<absl::flat_hash_map<int64_t, int>> var_index_to_value_count(
      num_vars);
  for (int i = 0; i < compressed_table.size(); ++i) {
    for (int var_index = 0; var_index < num_vars; ++var_index) {
      if (compressed_table[i][var_index].empty()) {
        has_any[var_index] = true;
        continue;
      }
      for (const int64_t v : compressed_table[i][var_index]) {
        DCHECK_NE(v, kTableAnyValue);
        DCHECK(context->DomainOf(vars[var_index]).Contains(v));
        var_index_to_value_count[var_index][v]++;
      }
    }
  }

  // Create one Boolean variable per tuple to indicate if it can still be
  // selected or not. Enforce an exactly one between them.
  std::vector<int> exo_literals;
  exo_literals.reserve(compressed_table.size() + 1);

  std::optional<int> table_is_active_literal = std::nullopt;
  // Process enforcement literals.
  if (ct.enforcement_literal().size() == 1) {
    table_is_active_literal = ct.enforcement_literal(0);
  } else if (ct.enforcement_literal().size() > 1) {
    table_is_active_literal =
        context->NewBoolVarWithConjunction(ct.enforcement_literal());

    // Adds table_is_active <=> and(enforcement_literals).
    context->AddConstraint([&](ConstraintProto* new_ct) {
      BoolArgumentProto* bool_or = new_ct->mutable_bool_or();
      bool_or->add_literals(table_is_active_literal.value());
      for (const int lit : ct.enforcement_literal()) {
        bool_or->add_literals(NegatedRef(lit));
      }
    });
    for (const int lit : ct.enforcement_literal()) {
      context->AddImplication(table_is_active_literal.value(), lit);
    }
  }
  std::vector<int> existing_row_literals;
  std::vector<SolutionCrush::TableRowLiteral> new_row_literals;
  if (table_is_active_literal.has_value()) {
    const int inactive_lit = NegatedRef(table_is_active_literal.value());
    exo_literals.push_back(inactive_lit);
    existing_row_literals.push_back(inactive_lit);
  }

  int num_reused_variables = 0;
  std::vector<int> tuples_with_new_variable;
  std::vector<int> tuple_literals(compressed_table.size());
  for (int i = 0; i < compressed_table.size(); ++i) {
    bool create_new_var = true;
    for (int var_index = 0; var_index < num_vars; ++var_index) {
      if (has_any[var_index]) continue;
      if (compressed_table[i][var_index].size() != 1 ||
          !ct.enforcement_literal().empty()) {
        continue;
      }
      const int64_t v = compressed_table[i][var_index][0];
      if (var_index_to_value_count[var_index][v] != 1) continue;

      ++num_reused_variables;
      create_new_var = false;
      tuple_literals[i] =
          context->GetOrCreateVarValueEncoding(vars[var_index], v);
      existing_row_literals.push_back(tuple_literals[i]);
      break;
    }
    if (create_new_var) {
      tuple_literals[i] = context->NewBoolVar("table expansion");
      new_row_literals.push_back({tuple_literals[i], compressed_table[i]});
    }
    exo_literals.push_back(tuple_literals[i]);
  }
  context->AddConstraint([&exo_literals](ConstraintProto* new_ct) {
    *new_ct->mutable_exactly_one()->mutable_literals() = {exo_literals.begin(),
                                                          exo_literals.end()};
  });
  if (num_reused_variables > 0) {
    context->UpdateRuleStats("table: reused literals");
  }

  // Set the cost to the corresponding tuple literal. If there is more than one
  // cost, we just choose the first one which is the smallest one.
  if (last_column_is_cost) {
    for (int i = 0; i < tuple_literals.size(); ++i) {
      context->AddLiteralToObjective(tuple_literals[i],
                                     compressed_table[i].back()[0]);
    }
  }

  std::vector<absl::InlinedVector<int64_t, 2>> column;
  for (int var_index = 0; var_index < num_vars; ++var_index) {
    if (context->IsFixed(vars[var_index])) continue;

    column.clear();
    for (int i = 0; i < tuple_literals.size(); ++i) {
      column.push_back(compressed_table[i][var_index]);
    }
    ProcessOneCompressedColumn(vars[var_index], tuple_literals, column,
                               table_is_active_literal, context);
  }

  context->solution_crush().SetTableExpandedVars(vars, existing_row_literals,
                                                 new_row_literals);
  context->UpdateRuleStats("table: expanded positive constraint");
}

// TODO(user): reinvestigate ExploreSubsetOfVariablesAndAddNegatedTables.
//
// TODO(user): if 2 table constraints share the same valid prefix, the
// tuple literals can be reused.
//
// TODO(user): investigate different encoding for prefix tables. Maybe
// we can remove the need to create tuple literals.
FeasibilityStatus ExpandPositiveTable(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  DCHECK(TableIsInCanonicalForm(ct));
  const TableConstraintProto& table = ct.table();
  if (table.exprs().empty()) {
    CHECK(table.values().empty());
    context->UpdateRuleStats("table: empty trivial");
    context->ClearConstraint(c);
    return kOk;
  }
  const int num_exprs = table.exprs_size();
  const int num_original_tuples = table.values_size() / num_exprs;

  // Read tuples flat array and recreate the vector of tuples.
  std::vector<int> vars;
  vars.reserve(table.exprs_size());
  if (table.values().empty()) {
    DCHECK(table.exprs_size() == 1 && table.exprs(0).vars().empty());
  } else {
    for (const LinearExpressionProto& expr : table.exprs()) {
      vars.push_back(expr.vars(0));
    }
  }
  std::vector<std::vector<int64_t>> tuples(num_original_tuples);
  int count = 0;
  for (int tuple_index = 0; tuple_index < num_original_tuples; ++tuple_index) {
    for (int var_index = 0; var_index < num_exprs; ++var_index) {
      tuples[tuple_index].push_back(table.values(count++));
    }
  }

  // Compute the set of possible values for each variable (from the table).
  // Remove invalid tuples along the way.
  std::vector<absl::flat_hash_set<int64_t>> values_per_var(num_exprs);
  int new_size = 0;
  for (int tuple_index = 0; tuple_index < num_original_tuples; ++tuple_index) {
    bool keep = true;
    for (int var_index = 0; var_index < num_exprs; ++var_index) {
      const int64_t value = tuples[tuple_index][var_index];
      if (!context->VarCanTakeValue(vars[var_index], value)) {
        keep = false;
        break;
      }
    }
    if (keep) {
      for (int var_index = 0; var_index < num_exprs; ++var_index) {
        values_per_var[var_index].insert(tuples[tuple_index][var_index]);
      }
      std::swap(tuples[tuple_index], tuples[new_size]);
      new_size++;
    }
  }
  tuples.resize(new_size);

  if (tuples.empty()) {
    if (ct.enforcement_literal().empty()) {
      context->UpdateRuleStats("table: empty");
      return context->NotifyUnsat();
    } else {
      context->UpdateRuleStats("table: enforced and empty");
      context->AddConstraint([&ct](ConstraintProto* new_ct) {
        BoolArgumentProto* bool_or = new_ct->mutable_bool_or();
        for (const int lit : ct.enforcement_literal()) {
          bool_or->add_literals(NegatedRef(lit));
        }
      });
      context->ClearConstraint(c);
      return kOk;
    }
  }

  // Update variable domains. It is redundant with presolve, but we could be
  // here with presolve = false.
  // Also counts the number of fixed variables.
  if (ct.enforcement_literal().empty()) {
    int num_fixed_variables = 0;
    for (int var_index = 0; var_index < num_exprs; ++var_index) {
      if (context
              ->IntersectDomainWith(
                  vars[var_index],
                  Domain::FromValues({values_per_var[var_index].begin(),
                                      values_per_var[var_index].end()}))
              .unsat()) {
        // Normally this would not happen since we canonicalize the table before
        // calling ExpandCpModel. But the process of expand itself (for example
        // of a previous constraint) can fix variables and/or create affine
        // relationships that make the constraint not canonical anymore.
        return kUnsat;
      }
      if (context->DomainOf(vars[var_index]).IsFixed()) {
        num_fixed_variables++;
      }
    }

    if (num_fixed_variables == num_exprs - 1) {
      context->UpdateRuleStats("table: one variable not fixed");
      context->ClearConstraint(c);
      return kOk;
    } else if (num_fixed_variables == num_exprs) {
      context->UpdateRuleStats("table: all variables fixed");
      context->ClearConstraint(c);
      return kOk;
    }
  }

  // Tables with two variables do not need tuple literals.
  //
  // TODO(user): If there is a unique variable with cost, it is better to
  // detect it. But if the detection fails, we should still call
  // AddSizeTwoTable() unlike what happens here.
  if (num_exprs == 2 && !context->params().detect_table_with_cost() &&
      ct.enforcement_literal().empty()) {
    if (AddSizeTwoTable(vars, tuples, values_per_var, context).unsat()) {
      return kUnsat;
    }
    context->UpdateRuleStats(
        "table: expanded positive constraint with two variables");
    context->ClearConstraint(c);
    return kOk;
  }

  bool last_column_is_cost = false;
  if (context->params().detect_table_with_cost() &&
      ct.enforcement_literal().empty()) {
    last_column_is_cost =
        ReduceTableInPresenceOfUniqueVariableWithCosts(&vars, &tuples, context)
            .modified();
  }

  CompressAndExpandPositiveTable(c, last_column_is_cost, vars, &tuples,
                                 context);
  context->ClearConstraint(c);
  return kOk;
}

bool AllDiffShouldBeExpanded(const Domain& union_of_domains,
                             const ConstraintProto& ct,
                             PresolveContext* context) {
  if (union_of_domains.Size() > context->params().max_alldiff_domain_size()) {
    return false;
  }

  const AllDifferentConstraintProto& proto = ct.all_diff();
  const int num_exprs = proto.exprs_size();
  int num_fully_encoded = 0;
  for (int i = 0; i < num_exprs; ++i) {
    if (context->IsFullyEncoded(proto.exprs(i))) {
      num_fully_encoded++;
    }
  }

  if ((union_of_domains.Size() <= 2 * proto.exprs_size()) ||
      (union_of_domains.Size() <= 32)) {
    // Small domains.
    return true;
  }

  if (num_fully_encoded == num_exprs) {
    // All variables fully encoded, and domains are small enough.
    return true;
  }
  return false;
}

// Note that we used to do that at loading time, but we prefer to do that as
// part of the presolve so that all variables are available for sharing between
// subworkers and also are accessible by the linear relaxation.
//
// TODO(user): Note that currently both encoding introduce extra solutions
// if the constraint has some enforcement literal(). We can either fix this by
// supporting enumeration on a subset of variables. Or add an extra constraint
// to fix all new Booleans to false if the initial constraint is not enforced.
void ExpandComplexLinearConstraint(int c, PresolveContext* context) {
  const ConstraintProto& ct = context->Constraint(c);
  // TODO(user): We treat the linear of size 1 differently because we need them
  // as is to recognize value encoding. Try to still create needed Booleans now
  // so that we can share more between the different workers. Or revisit how
  // linear1 are propagated.
  if (ct.linear().domain().size() <= 2) return;
  if (ct.linear().vars().size() == 1) return;

  const SatParameters& params = context->params();
  if (params.encode_complex_linear_constraint_with_integer()) {
    // Integer encoding.
    //
    // Here we add a slack with domain equal to rhs and transform
    // expr \in rhs to expr - slack = 0
    const Domain rhs = ReadDomainFromProto(ct.linear());
    const int slack = context->NewIntVar(rhs);
    context->solution_crush().SetVarToLinearExpression(
        slack, ct.linear().vars(), ct.linear().coeffs());
    context->ModifyConstraint(c, [slack](ConstraintProto* mutable_ct) {
      LinearConstraintProto* lin = mutable_ct->mutable_linear();
      lin->add_vars(slack);
      lin->add_coeffs(-1);
      lin->clear_domain();
      FillDomainInProto(0, lin);
    });
  } else {
    // Boolean encoding.
    const bool use_single_bool =
        ct.enforcement_literal().empty() && ct.linear().domain_size() == 4;
    // Save enforcement literals for the enumeration.
    const std::vector<int> enforcement_literals(
        ct.enforcement_literal().begin(), ct.enforcement_literal().end());
    std::vector<int> domain_literals;
    int single_bool = 0;
    if (use_single_bool) {
      // We cover the special case of no enforcement and two choices by creating
      // a single Boolean.
      single_bool = context->NewBoolVar("complex linear expansion");
      domain_literals.push_back(single_bool);
    } else {
      domain_literals.reserve(ct.linear().domain_size() / 2);
      for (int i = 0; i < ct.linear().domain_size(); i += 2) {
        domain_literals.push_back(
            context->NewBoolVar("complex linear expansion"));
      }
      context->AddConstraint([&](ConstraintProto* new_ct) {
        BoolArgumentProto* clause = new_ct->mutable_bool_or();
        for (const int ref : enforcement_literals) {
          clause->add_literals(NegatedRef(ref));
        }
        for (const int subdomain_literal : domain_literals) {
          clause->add_literals(subdomain_literal);
        }
      });
    }

    for (int i = 0; i < ct.linear().domain_size(); i += 2) {
      const int64_t lb = ct.linear().domain(i);
      const int64_t ub = ct.linear().domain(i + 1);
      const int subdomain_literal =
          use_single_bool ? (i == 0 ? single_bool : NegatedRef(single_bool))
                          : domain_literals[i / 2];

      // Create a new constraint which is a copy of the original, but with a
      // simple sub-domain and enforcement literal.
      context->AddConstraint([&](ConstraintProto* new_ct) {
        new_ct->set_name(ct.name());
        *new_ct->mutable_linear() = ct.linear();
        new_ct->add_enforcement_literal(subdomain_literal);
        FillDomainInProto(Domain(lb, ub), new_ct->mutable_linear());
      });
    }
    context->solution_crush().SetLinearWithComplexDomainExpandedVars(
        ct.linear(), domain_literals);

    // Make sure all booleans are tight when enumerating all solutions.
    if (context->params().enumerate_all_solutions() &&
        !enforcement_literals.empty()) {
      int linear_is_enforced;
      if (enforcement_literals.size() == 1) {
        linear_is_enforced = enforcement_literals[0];
      } else {
        linear_is_enforced = context->NewBoolVar("complex linear expansion");
        context->AddConstraint([&](ConstraintProto* new_ct) {
          BoolArgumentProto* maintain_linear_is_enforced =
              new_ct->mutable_bool_or();
          for (const int e_lit : enforcement_literals) {
            maintain_linear_is_enforced->add_literals(NegatedRef(e_lit));
          }
          maintain_linear_is_enforced->add_literals(linear_is_enforced);
        });
        for (const int e_lit : enforcement_literals) {
          context->AddImplication(NegatedRef(e_lit),
                                  NegatedRef(linear_is_enforced));
        }
        context->solution_crush().SetVarToConjunction(linear_is_enforced,
                                                      enforcement_literals);
      }

      for (const int lit : domain_literals) {
        context->AddImplication(NegatedRef(linear_is_enforced),
                                NegatedRef(lit));
      }
    }
    context->ClearConstraint(c);
  }

  context->UpdateRuleStats("linear: expanded complex rhs");
  CHECK_EQ(context->InitializeNewDomains(), kOk);
}

bool IsVarEqOrNeqValue(const PresolveContext& context,
                       const LinearConstraintProto& lin) {
  if (lin.vars_size() != 1) return false;
  const Domain rhs = ReadDomainFromProto(lin);

  // This is literal => var == value.
  if (rhs.IsFixed()) return true;

  // Is it literal => var != value ?
  const Domain not_implied =
      rhs.InverseMultiplicationBy(lin.coeffs(0))
          .Complement()
          .IntersectionWith(context.DomainOf(lin.vars(0)));
  if (not_implied.IsEmpty()) return false;
  return not_implied.IsFixed();
}

// Scans the unexpanded model (only when at least one `all_diff` constraint is
// present) to record, for each variable:
//   - `var_will_be_fully_encoded`: another constraint (`table`, `inverse`,
//     `automaton`, `element` index or constant-array target) or >= 2
//     permutation/near-permutation `all_diff` constraints will fully encode
//     this variable.
//   - `var_to_num_eq_neq_encoding`: the number of 1-var `==`/`!=` linear
//     constraints on this variable, each of which will encode one value.
//
// Running this before Pass 1 expansion ensures we still see `table`, `element`,
// `automaton`, and `inverse` before they are expanded and cleared.
void ScanModelForAllDiffExpansion(
    const PresolveContext& context,
    std::vector<bool>& var_will_be_fully_encoded,
    std::vector<int64_t>& var_to_num_eq_neq_encoding) {
  const CpModelProto& model_proto = context.WorkingModel();
  const int num_vars = model_proto.variables_size();
  std::vector<bool> in_all_diff(num_vars, false);
  std::vector<int> perm_or_near_perm_count(num_vars, 0);
  bool has_all_diff = false;
  for (int c = 0; c < context.NumConstraints(); ++c) {
    const ConstraintProto& ct = context.Constraint(c);
    if (ct.constraint_case() != ConstraintProto::kAllDiff) continue;
    has_all_diff = true;
    const AllDifferentConstraintProto& proto = ct.all_diff();
    for (const LinearExpressionProto& expr : proto.exprs()) {
      for (const int var : expr.vars()) {
        DCHECK(RefIsPositive(var));
        in_all_diff[var] = true;
      }
    }
    if (proto.exprs_size() > 1 && ct.enforcement_literal().empty()) {
      Domain union_of_domains = context.DomainSuperSetOf(proto.exprs(0));
      for (int i = 1; i < proto.exprs_size(); ++i) {
        union_of_domains = union_of_domains.UnionWith(
            context.DomainSuperSetOf(proto.exprs(i)));
      }
      const int64_t num_slacks = union_of_domains.Size() - proto.exprs_size();
      if (union_of_domains.Size() <=
              context.params().max_alldiff_domain_size() &&
          num_slacks >= 0 && num_slacks <= 8) {
        for (const LinearExpressionProto& expr : proto.exprs()) {
          for (const int var : expr.vars()) {
            ++perm_or_near_perm_count[var];
          }
        }
      }
    }
  }
  if (!has_all_diff) return;

  var_will_be_fully_encoded.assign(num_vars, false);
  var_to_num_eq_neq_encoding.assign(num_vars, 0);

  auto mark_exprs_fully_encoded = [&](const auto& exprs) {
    for (const LinearExpressionProto& expr : exprs) {
      for (const int v : expr.vars()) {
        var_will_be_fully_encoded[v] = true;
      }
    }
  };

  std::vector<bool> visited_constraints(context.NumConstraints(), false);
  for (int var = 0; var < num_vars; ++var) {
    if (!in_all_diff[var] || context.IsFixed(var)) continue;
    if (perm_or_near_perm_count[var] >= 2) {
      var_will_be_fully_encoded[var] = true;
      continue;
    }
    if (var_will_be_fully_encoded[var]) continue;

    // Note: Boolean constraints are ignored.
    for (const int ct_index : context.VarToConstraints(var)) {
      // Skip artificial or already-visited constraints.
      if (ct_index < 0 || visited_constraints[ct_index]) continue;
      visited_constraints[ct_index] = true;

      const ConstraintProto& other_ct = context.Constraint(ct_index);
      switch (other_ct.constraint_case()) {
        case ConstraintProto::ConstraintCase::kLinear:
          if (other_ct.linear().vars_size() == 1 &&
              IsVarEqOrNeqValue(context, other_ct.linear())) {
            const int lin_var = other_ct.linear().vars(0);
            DCHECK(RefIsPositive(lin_var));
            ++var_to_num_eq_neq_encoding[lin_var];
          }
          break;
        case ConstraintProto::ConstraintCase::kElement: {
          const ElementConstraintProto& element = other_ct.element();
          if (element.linear_index().vars_size() == 1) {
            var_will_be_fully_encoded[element.linear_index().vars(0)] = true;
          }
          if (element.linear_target().vars_size() == 1) {
            const int target_var = element.linear_target().vars(0);
            if (!var_will_be_fully_encoded[target_var]) {
              const bool all_constants = absl::c_all_of(
                  element.exprs(),
                  [](const auto& e) { return e.vars().empty(); });
              if (all_constants) {
                var_will_be_fully_encoded[target_var] = true;
              }
            }
          }
          break;
        }
        case ConstraintProto::ConstraintCase::kInverse:
          mark_exprs_fully_encoded(other_ct.inverse().f_expr_direct());
          mark_exprs_fully_encoded(other_ct.inverse().f_expr_inverse());
          break;
        case ConstraintProto::ConstraintCase::kTable:
          mark_exprs_fully_encoded(other_ct.table().exprs());
          break;
        case ConstraintProto::ConstraintCase::kAutomaton:
          mark_exprs_fully_encoded(other_ct.automaton().exprs());
          break;
        default:
          break;
      }

      if (var_will_be_fully_encoded[var]) break;
    }
  }
}

FeasibilityStatus MaybeExpandAllDiff(
    int c, PresolveContext* context,
    const std::vector<bool>& var_will_be_fully_encoded,
    const std::vector<int64_t>& var_to_num_eq_neq_encoding) {
  const bool expand_all_diff_from_parameters =
      context->params().expand_alldiff_constraints();
  const ConstraintProto& ct = context->Constraint(c);
  const AllDifferentConstraintProto& proto = ct.all_diff();
  const int num_exprs = proto.exprs_size();
  if (num_exprs <= 1) return kOk;

  Domain union_of_domains = context->DomainSuperSetOf(proto.exprs(0));
  for (int i = 1; i < num_exprs; ++i) {
    union_of_domains =
        union_of_domains.UnionWith(context->DomainSuperSetOf(proto.exprs(i)));
  }

  const bool expand_all_diff_from_size =
      AllDiffShouldBeExpanded(union_of_domains, ct, context);
  if (!expand_all_diff_from_parameters && !expand_all_diff_from_size) {
    return kOk;
  }

  const int64_t num_slacks = union_of_domains.Size() - num_exprs;
  const bool is_a_permutation = num_slacks == 0;
  const bool is_near_permutation =
      ct.enforcement_literal().empty() && num_slacks > 0 &&
      num_slacks <= std::min<int64_t>(8, num_exprs);  // If num_exprs < 8.

  if (!expand_all_diff_from_parameters) {
    int num_unfixed = 0;
    int num_domain_used = 0;
    int64_t sum_domain_sizes = 0;
    int64_t sum_encoded_values = 0;
    for (const LinearExpressionProto& expr : proto.exprs()) {
      if (expr.vars().empty()) continue;
      DCHECK_EQ(1, expr.vars_size());
      const int var = expr.vars(0);
      DCHECK(RefIsPositive(var));
      if (context->IsFixed(var)) continue;
      ++num_unfixed;
      const int64_t domain_size = context->DomainOf(var).Size();
      sum_domain_sizes += domain_size;
      if ((var < var_will_be_fully_encoded.size() &&
           var_will_be_fully_encoded[var]) ||
          domain_size <= 2) {
        sum_encoded_values += domain_size;
        ++num_domain_used;
      } else {
        const int64_t extra_encodings = var < var_to_num_eq_neq_encoding.size()
                                            ? var_to_num_eq_neq_encoding[var]
                                            : 0;
        const int64_t num_encoded = std::min(
            domain_size, context->GetValueEncodingSize(var) + extra_encodings);
        sum_encoded_values += num_encoded;
        if (num_encoded * 2 >= domain_size) {
          ++num_domain_used;
        }
      }
    }

    const bool should_expand = (is_a_permutation || is_near_permutation)
                                   ? sum_encoded_values > sum_domain_sizes / 2
                                   : num_domain_used * 2 >= num_unfixed;
    if (!should_expand) return kOk;
  }

  const bool keep_after_expansion = false;

  // Collect all possible variables that can take each value, and add one linear
  // equation per value stating that this value can be assigned at most once, or
  // exactly once in case of permutation (or near-permutation with a slack).
  int64_t remaining_slacks = num_slacks;
  std::vector<int> slack_vars;
  std::vector<int> negated_lits;
  for (const int64_t v : union_of_domains.Values()) {
    // Collect references which domain contains v.
    std::vector<LinearExpressionProto> possible_exprs;
    int fixed_expression_count = 0;
    for (const LinearExpressionProto& expr : proto.exprs()) {
      if (!context->DomainContains(expr, v)) continue;
      possible_exprs.push_back(expr);
      if (context->IsFixed(expr)) {
        fixed_expression_count++;
      }
    }

    if (fixed_expression_count > 1) {
      // Violates the definition of AllDifferent.
      return ExpandAlwaysFalseConstraint(c, context);
    } else if (fixed_expression_count == 1 &&
               ct.enforcement_literal().empty()) {
      // Remove values from other domains.
      for (const LinearExpressionProto& expr : possible_exprs) {
        if (context->IsFixed(expr)) continue;
        if (context->IntersectDomainWith(expr, Domain(v).Complement())
                .unsat()) {
          VLOG(1) << "Empty domain for a variable in MaybeExpandAllDiff()";
          return kUnsat;
        }
      }
    }

    if (is_near_permutation && possible_exprs.empty()) {
      --remaining_slacks;
      if (remaining_slacks < 0) return kUnsat;
    }

    const bool add_slack = is_near_permutation && fixed_expression_count == 0 &&
                           !possible_exprs.empty();
    std::vector<int> lits;
    lits.reserve(possible_exprs.size() + 1);
    negated_lits.clear();
    for (const LinearExpressionProto& expr : possible_exprs) {
      // The above propagation can remove a value after the expression was
      // added to possible_exprs.
      if (!context->DomainContains(expr, v)) continue;

      // If the expression is fixed, the created literal will be the true
      // literal. We still need to fail if two expressions are fixed to the same
      // value.
      const int encoding = context->GetOrCreateAffineValueEncoding(expr, v);
      lits.push_back(encoding);
      if (add_slack) negated_lits.push_back(NegatedRef(encoding));
    }
    if (add_slack) {
      const int slack = context->NewBoolVar("all_diff slack");
      lits.push_back(slack);
      context->solution_crush().SetVarToConjunction(slack, negated_lits);
      slack_vars.push_back(slack);
    }
    context->AddEnforcedConstraint(ct, [&](ConstraintProto* new_ct) {
      BoolArgumentProto* at_most_or_equal_one =
          (is_a_permutation || add_slack) ? new_ct->mutable_exactly_one()
                                          : new_ct->mutable_at_most_one();
      *at_most_or_equal_one->mutable_literals() = {lits.begin(), lits.end()};
    });
  }

  if (!slack_vars.empty()) {
    if (remaining_slacks == 0) {
      for (const int slack : slack_vars) {
        if (context->SetLiteralToFalse(slack).unsat()) return kUnsat;
      }
    } else if (remaining_slacks == 1) {
      context->AddConstraint([&slack_vars](ConstraintProto* new_ct) {
        *new_ct->mutable_exactly_one()->mutable_literals() = {
            slack_vars.begin(), slack_vars.end()};
      });
    } else {
      context->AddConstraint(
          [&slack_vars, remaining_slacks](ConstraintProto* new_ct) {
            LinearConstraintProto* lin = new_ct->mutable_linear();
            for (const int slack : slack_vars) {
              lin->add_vars(slack);
              lin->add_coeffs(1);
            }
            lin->add_domain(remaining_slacks);
            lin->add_domain(remaining_slacks);
          });
    }
  }

  context->UpdateRuleStats(
      absl::StrCat("all_diff:",
                   is_a_permutation      ? " permutation"
                   : !slack_vars.empty() ? " near_permutation"
                                         : "",
                   " expanded", keep_after_expansion ? " and kept" : ""));
  if (!keep_after_expansion) context->ClearConstraint(c);
  return kOk;
}

}  // namespace

FeasibilityStatus ExpandCpModel(PresolveContext* context) {
  if (context->params().disable_constraint_expansion()) return kOk;

  // None of the functions here need to be run twice. This is because we never
  // create constraints that need to be expanded during presolve.
  if (context->ModelIsExpanded()) return kOk;

  // Make sure all domains are initialized and constraints are canonical.
  if (context->InitializeNewDomains().unsat()) return kUnsat;
  std::vector<int> tmp_literals;
  ExpandAllEnforcedAtMostOneAndExactlyOne(context, &tmp_literals);
  if (context->CanonicalizeConstraintsLazily().unsat()) return kUnsat;

  // Clear the precedence cache.
  context->ClearPrecedenceCache();

  // Scan variable usage before Pass 1 so we still see unexpanded constraints
  // (`table`, `element`, `automaton`, `inverse`).
  std::vector<bool> var_will_be_fully_encoded;
  std::vector<int64_t> var_to_num_eq_neq_encoding;
  ScanModelForAllDiffExpansion(*context, var_will_be_fully_encoded,
                               var_to_num_eq_neq_encoding);

  // First pass: we look at constraints that may fully encode variables.
  {
    DeterministicTimer<{.scale = 1.3e-08, .offset = 1.9e-07}> timer(
        context->time_limit(), context->NumConstraints());
    for (int c = 0; c < context->NumConstraints(); ++c) {
      const ConstraintProto& ct = context->Constraint(c);
      bool skip = false;
      FeasibilityStatus status = kOk;
      switch (ct.constraint_case()) {
        case ConstraintProto::kLinear:
          // If we only do expansion, we do that as part of the main loop.
          // This way we don't need to call FinalExpansionForLinearConstraint().
          if (ct.linear().domain().size() > 2 &&
              !context->params().cp_model_presolve()) {
            ExpandComplexLinearConstraint(c, context);
          }
          break;
        case ConstraintProto::kReservoir:
          if (context->params().expand_reservoir_constraints()) {
            status = ExpandReservoir(c, context);
          }
          break;
        case ConstraintProto::kCumulative:
          if (context->params().encode_cumulative_as_reservoir()) {
            status = EncodeCumulativeAsReservoir(c, context);
          }
          break;
        case ConstraintProto::kIntMod:
          status = ExpandIntMod(c, context);
          break;
        case ConstraintProto::kIntProd:
          ExpandIntProd(c, context);
          break;
        case ConstraintProto::kElement:
          status = ExpandElement(c, context);
          break;
        case ConstraintProto::kInverse:
          status = ExpandInverse(c, context);
          break;
        case ConstraintProto::kAutomaton:
          status = ExpandAutomaton(c, context);
          break;
        case ConstraintProto::kTable:
          if (!context->params().cp_model_presolve() ||
              context->time_limit()->LimitReached()) {
            context->ModifyConstraint(c,
                                      [context](ConstraintProto* mutable_ct) {
                                        CanonicalizeTable(context, mutable_ct);
                                      });
          }
          if (ct.table().negated()) {
            ExpandNegativeTable(c, context);
          } else {
            status = ExpandPositiveTable(c, context);
          }
          break;
        case ConstraintProto::kLinMax:
          if (ct.lin_max().exprs().size() <=
              context->params().max_lin_max_size_for_expansion()) {
            ExpandLinMax(c, context);
          } else {
            skip = true;
          }
          break;
        default:
          skip = true;
          break;
      }
      if (skip) continue;  // Nothing was done for this constraint.

      // Early exit if the model is unsat.
      if (status.unsat()) {
        SOLVER_LOG(context->logger(), "UNSAT after expansion of ",
                   ProtobufShortDebugString(ct));
        return kUnsat;
      }

      // Update variable-constraint graph.
      context->MarkConstraintForCanonicalization(c);
    }
  }

  if (context->InitializeNewDomains().unsat()) return kUnsat;
  ExpandAllEnforcedAtMostOneAndExactlyOne(context, &tmp_literals);
  if (context->CanonicalizeConstraintsLazily().unsat()) return kUnsat;

  // Second pass. We may decide to expand constraints if all their variables
  // are fully encoded.
  {
    DeterministicTimer<{.scale = 1.4e-08, .offset = 1.4e-07}> timer(
        context->time_limit(), context->NumConstraints());
    for (int c = 0; c < context->NumConstraints(); ++c) {
      bool skip = false;
      FeasibilityStatus status = kOk;
      const ConstraintProto& ct = context->Constraint(c);
      switch (ct.constraint_case()) {
        case ConstraintProto::kAtMostOne:
        case ConstraintProto::kExactlyOne:
          // We do those in the second pass since MaybeExpandAllDiff() below may
          // create such constraints.
          ExpandEnforcedAtMostOneOrExactlyOneConstraint(c, context,
                                                        &tmp_literals);
          break;
        case ConstraintProto::kAllDiff:
          status = MaybeExpandAllDiff(c, context, var_will_be_fully_encoded,
                                      var_to_num_eq_neq_encoding);
          break;
        default:
          skip = true;
          break;
      }

      if (skip) continue;  // Nothing was done for this constraint.

      // Early exit if the model is unsat.
      if (status.unsat()) {
        SOLVER_LOG(context->logger(), "UNSAT after expansion of ",
                   ProtobufShortDebugString(ct));
        return kUnsat;
      }

      // Update variable-constraint graph.
      context->MarkConstraintForCanonicalization(c);
    }
  }

  // The precedence cache can become invalid during presolve as it does not
  // handle variable substitution. It is safer just to clear it at the end
  // of the expansion phase.
  context->ClearPrecedenceCache();

  // Make sure the context is consistent and the proto domains are up to date.
  if (context->InitializeNewDomains().unsat()) return kUnsat;
  if (context->CanonicalizeConstraintsLazily().unsat()) return kUnsat;
  context->WriteVariableDomainsToProto();
  context->NotifyThatModelIsExpanded();
  return kOk;
}

void FinalExpansionForLinearConstraint(PresolveContext* context) {
  if (context->params().disable_constraint_expansion()) return;
  DeterministicTimer<{.scale = 1.4e-08}> timer(context->time_limit(),
                                               context->NumConstraints());
  for (int c = 0; c < context->NumConstraints(); ++c) {
    const ConstraintProto& ct = context->Constraint(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::kLinear:
        if (ct.linear().domain().size() > 2) {
          ExpandComplexLinearConstraint(c, context);
        }
        break;
      default:
        break;
    }
  }
}

}  // namespace sat
}  // namespace operations_research
