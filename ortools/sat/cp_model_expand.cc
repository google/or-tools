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
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/message.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_table.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

// Different encoding that support general demands. This is usually a pretty bad
// encoding, at least until we improve the solver on such models.
void ExpandReservoirUsingCircuit(int64_t sum_of_positive_demand,
                                 int64_t sum_of_negative_demand,
                                 ConstraintProto* reservoir_ct,
                                 PresolveContext* context) {
  const ReservoirConstraintProto& reservoir = reservoir_ct->reservoir();
  const int num_events = reservoir.time_exprs_size();

  // The encoding will create a circuit constraint, and one integer variable per
  // event (representing the level at that event time).
  CircuitConstraintProto* circuit =
      context->working_model->add_constraints()->mutable_circuit();

  const int64_t var_min =
      std::max(reservoir.min_level(), sum_of_negative_demand);
  const int64_t var_max =
      std::min(reservoir.max_level(), sum_of_positive_demand);
  std::vector<int> level_vars(num_events);
  for (int i = 0; i < num_events; ++i) {
    level_vars[i] = context->NewIntVar(Domain(var_min, var_max));
  }

  // For the corner case where all events are absent, we need a potential
  // self-arc on the start/end circuit node.
  {
    const int all_inactive = context->NewBoolVar("reservoir expansion");
    circuit->add_tails(num_events);
    circuit->add_heads(num_events);
    circuit->add_literals(all_inactive);
  }

  for (int i = 0; i < num_events; ++i) {
    if (!reservoir.active_literals().empty()) {
      // Add self arc to represent absence.
      circuit->add_tails(i);
      circuit->add_heads(i);
      circuit->add_literals(NegatedRef(reservoir.active_literals(i)));
    }

    // We need an extra circuit node for start/end of circuit.
    // We use the available index 'num_events'.
    {
      // Circuit starts at i, level_vars[i] == demand_expr[i].
      const int start_var = context->NewBoolVar("reservoir expansion");
      circuit->add_tails(num_events);
      circuit->add_heads(i);
      circuit->add_literals(start_var);

      // Add enforced linear for demand.
      {
        ConstraintProto* new_ct = context->working_model->add_constraints();
        new_ct->add_enforcement_literal(start_var);
        LinearConstraintProto* lin = new_ct->mutable_linear();
        lin->add_domain(0);
        lin->add_domain(0);
        lin->add_vars(level_vars[i]);
        lin->add_coeffs(1);
        AddLinearExpressionToLinearConstraint(reservoir.level_changes(i), -1,
                                              lin);
        context->CanonicalizeLinearConstraint(new_ct);
      }

      // Circuit ends at i, no extra constraint there.
      const int end_var = context->NewBoolVar("reservoir expansion");
      circuit->add_tails(i);
      circuit->add_heads(num_events);
      circuit->add_literals(end_var);
    }

    for (int j = 0; j < num_events; ++j) {
      if (i == j) continue;

      // If arc_i_j is true then:
      // - active_i is true (enforced by circuit).
      // - active_j is true (enforced by circuit).
      // - time_i <= time_j
      // - level_j == level_i + demand_j
      //
      // TODO(user): Unfortunately we cannot share these literal between
      // reservoir except if the set of time point is exactly the same!
      // otherwise if we miss one, then A "after" B in one circuit do not
      // implies that there is no C in between in another!
      const int arc_i_j = context->NewBoolVar("reservoir expansion");
      circuit->add_tails(i);
      circuit->add_heads(j);
      circuit->add_literals(arc_i_j);

      // Add enforced linear for time.
      {
        ConstraintProto* new_ct = context->working_model->add_constraints();
        new_ct->add_enforcement_literal(arc_i_j);
        LinearConstraintProto* lin = new_ct->mutable_linear();
        lin->add_domain(0);
        lin->add_domain(std::numeric_limits<int64_t>::max());
        AddLinearExpressionToLinearConstraint(reservoir.time_exprs(j), 1, lin);
        AddLinearExpressionToLinearConstraint(reservoir.time_exprs(i), -1, lin);
        context->CanonicalizeLinearConstraint(new_ct);
      }

      // Add enforced linear for demand.
      {
        ConstraintProto* new_ct = context->working_model->add_constraints();
        new_ct->add_enforcement_literal(arc_i_j);
        LinearConstraintProto* lin = new_ct->mutable_linear();
        lin->add_domain(0);
        lin->add_domain(0);
        lin->add_vars(level_vars[j]);
        lin->add_coeffs(1);
        lin->add_vars(level_vars[i]);
        lin->add_coeffs(-1);
        AddLinearExpressionToLinearConstraint(reservoir.level_changes(j), -1,
                                              lin);
        context->CanonicalizeLinearConstraint(new_ct);
      }
    }
  }
  context->solution_crush().SetReservoirCircuitVars(reservoir, var_min, var_max,
                                                    level_vars, *circuit);

  reservoir_ct->Clear();
  context->UpdateRuleStats("reservoir: expanded using circuit.");
}

void ExpandReservoirUsingPrecedences(bool max_level_is_constraining,
                                     bool min_level_is_constraining,
                                     ConstraintProto* reservoir_ct,
                                     PresolveContext* context) {
  const ReservoirConstraintProto& reservoir = reservoir_ct->reservoir();
  const int num_events = reservoir.time_exprs_size();
  const int true_literal = context->GetTrueLiteral();
  const auto is_active_literal = [&reservoir, true_literal](int index) {
    if (reservoir.active_literals_size() == 0) return true_literal;
    return reservoir.active_literals(index);
  };

  // Constrains the running level to be consistent at all time_exprs.
  // For this we only add a constraint at the time a given demand take place.
  for (int i = 0; i < num_events; ++i) {
    const int active_i = is_active_literal(i);
    if (context->LiteralIsFalse(active_i)) continue;

    const int64_t demand_i = context->FixedValue(reservoir.level_changes(i));
    if (demand_i == 0) continue;

    // No need for some constraints if the reservoir is just constrained in
    // one direction.
    if (demand_i > 0 && !max_level_is_constraining) continue;
    if (demand_i < 0 && !min_level_is_constraining) continue;

    ConstraintProto* new_cumul = context->working_model->add_constraints();
    LinearConstraintProto* new_linear = new_cumul->mutable_linear();
    int64_t offset = 0;

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
      AddWeightedLiteralToLinearConstraint(j_lesseq_i, demand_j, new_linear,
                                           &offset);
    }

    // Add contribution from event i.
    //
    // TODO(user): Alternatively we can mark the whole constraint as enforced
    // only if active_i is true. Experiments with both version, right now we
    // miss enough benchmarks to conclude.
    AddWeightedLiteralToLinearConstraint(active_i, demand_i, new_linear,
                                         &offset);

    // Note that according to the sign of demand_i, we only need one side.
    // We apply the offset here to make sure we use int64_t min and max.
    if (demand_i > 0) {
      new_linear->add_domain(std::numeric_limits<int64_t>::min());
      new_linear->add_domain(reservoir.max_level() - offset);
    } else {
      new_linear->add_domain(reservoir.min_level() - offset);
      new_linear->add_domain(std::numeric_limits<int64_t>::max());
    }

    // Canonicalize the newly created constraint.
    context->CanonicalizeLinearConstraint(new_cumul);

    DCHECK(!PossibleIntegerOverflow(*context->working_model, new_linear->vars(),
                                    new_linear->coeffs()));
  }

  reservoir_ct->Clear();
  context->UpdateRuleStats("reservoir: expanded using precedences");
}

void ExpandReservoir(ConstraintProto* reservoir_ct, PresolveContext* context) {
  if (reservoir_ct->reservoir().min_level() >
      reservoir_ct->reservoir().max_level()) {
    VLOG(1) << "Empty level domain in reservoir constraint.";
    return (void)context->NotifyThatModelIsUnsat();
  }

  const ReservoirConstraintProto& reservoir = reservoir_ct->reservoir();
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
    reservoir_ct->Clear();
    return;
  }

  // If all level_changes have the same sign, we do not care about the order,
  // just the sum. We might need to create intermediate variable for quadratic
  // terms though.
  if (num_negatives == 0 || num_positives == 0) {
    const int true_literal = context->GetTrueLiteral();
    ConstraintProto* new_ct = context->working_model->add_constraints();
    LinearConstraintProto* sum = new_ct->mutable_linear();
    for (int i = 0; i < num_events; ++i) {
      const int active = reservoir.active_literals().empty()
                             ? true_literal
                             : reservoir.active_literals(i);
      const LinearExpressionProto& demand = reservoir.level_changes(i);
      if (context->IsFixed(demand)) {
        const int64_t change = context->FixedValue(reservoir.level_changes(i));
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
        const int new_var = context->NewIntVar(
            Domain(context->MinOf(demand), context->MaxOf(demand))
                .UnionWith(Domain(0)));
        sum->add_vars(new_var);
        sum->add_coeffs(1);

        // Active => new_var == demand.
        {
          ConstraintProto* demand_ct =
              context->working_model->add_constraints();
          demand_ct->add_enforcement_literal(active);
          LinearConstraintProto* lin = demand_ct->mutable_linear();
          lin->add_domain(0);
          lin->add_domain(0);
          lin->add_vars(new_var);
          lin->add_coeffs(1);
          AddLinearExpressionToLinearConstraint(demand, -1, lin);
          context->CanonicalizeLinearConstraint(demand_ct);
          context->solution_crush().SetVarToLinearExpressionIf(new_var, demand,
                                                               active);
        }

        // not(active) => new_var == 0.
        context->AddImplyInDomain(NegatedRef(active), new_var, Domain(0));
        context->solution_crush().SetVarToValueIf(new_var, 0,
                                                  NegatedRef(active));
      }
    }
    sum->add_domain(reservoir.min_level());
    sum->add_domain(reservoir.max_level());
    context->CanonicalizeLinearConstraint(new_ct);

    context->UpdateRuleStats("reservoir: simple expansion with sum");
    reservoir_ct->Clear();
    return;
  }

  // Call the correct expansion according to our parameter.
  if (context->params().expand_reservoir_using_circuit()) {
    ExpandReservoirUsingCircuit(sum_of_positive_demand, sum_of_negative_demand,
                                reservoir_ct, context);
  } else {
    // This one is the faster option usually.
    if (all_demands_are_fixed) {
      ExpandReservoirUsingPrecedences(
          sum_of_positive_demand > reservoir_ct->reservoir().max_level(),
          sum_of_negative_demand < reservoir_ct->reservoir().min_level(),
          reservoir_ct, context);
    } else {
      context->UpdateRuleStats(
          "reservoir: skipped expansion due to variable demands");
    }
  }
}

// This is mainly used for testing the reservoir implementation.
void EncodeCumulativeAsReservoir(ConstraintProto* ct,
                                 PresolveContext* context) {
  if (!context->IsFixed(ct->cumulative().capacity())) {
    context->UpdateRuleStats(
        "cumulative -> reservoir: expansion is not supported with variable "
        "capacity.");
    return;
  }

  // Note that we know that the min_level can never go below zero, so we can
  // just ignore this part of the constraint here.
  ConstraintProto reservoir_ct;
  auto* reservoir = reservoir_ct.mutable_reservoir();
  reservoir->set_min_level(std::numeric_limits<int64_t>::min());
  reservoir->set_max_level(context->FixedValue(ct->cumulative().capacity()));

  const int true_literal = context->GetTrueLiteral();
  const int num_intervals = ct->cumulative().intervals().size();
  for (int i = 0; i < num_intervals; ++i) {
    const auto& interval_ct =
        context->working_model->constraints(ct->cumulative().intervals(i));
    const auto& interval = interval_ct.interval();
    *reservoir->add_time_exprs() = interval.start();
    *reservoir->add_time_exprs() = interval.end();

    const LinearExpressionProto& demand = ct->cumulative().demands(i);
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

  // Now expand it and clear the cumulative.
  ct->Clear();
  context->UpdateRuleStats("cumulative: expanded into reservoir");
  ExpandReservoir(&reservoir_ct, context);
}

void ExpandIntMod(ConstraintProto* ct, PresolveContext* context) {
  const LinearArgumentProto& int_mod = ct->int_mod();
  const LinearExpressionProto& mod_expr = int_mod.exprs(1);
  if (context->IsFixed(mod_expr)) return;

  const LinearExpressionProto& expr = int_mod.exprs(0);
  const LinearExpressionProto& target_expr = int_mod.target();

  // We reduce the domain of target_expr to avoid later overflow.
  if (!context->IntersectDomainWith(
          target_expr, context->DomainSuperSetOf(expr).PositiveModuloBySuperset(
                           context->DomainSuperSetOf(mod_expr)))) {
    return;
  }

  // Create a new constraint with the same enforcement as ct.
  auto new_enforced_constraint = [&]() {
    ConstraintProto* new_ct = context->working_model->add_constraints();
    *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    return new_ct;
  };

  // div_expr = expr / mod_expr.
  const int div_var = context->NewIntVar(
      context->DomainSuperSetOf(expr).PositiveDivisionBySuperset(
          context->DomainSuperSetOf(mod_expr)));
  LinearExpressionProto div_expr;
  div_expr.add_vars(div_var);
  div_expr.add_coeffs(1);

  LinearArgumentProto* const div_proto =
      new_enforced_constraint()->mutable_int_div();
  *div_proto->mutable_target() = div_expr;
  *div_proto->add_exprs() = expr;
  *div_proto->add_exprs() = mod_expr;

  // Create prod_expr = div_expr * mod_expr.
  const Domain prod_domain =
      context->DomainOf(div_var)
          .ContinuousMultiplicationBy(context->DomainSuperSetOf(mod_expr))
          .IntersectionWith(context->DomainSuperSetOf(expr).AdditionWith(
              context->DomainSuperSetOf(target_expr).Negation()));
  const int prod_var = context->NewIntVar(prod_domain);
  LinearExpressionProto prod_expr;
  prod_expr.add_vars(prod_var);
  prod_expr.add_coeffs(1);

  LinearArgumentProto* const int_prod =
      new_enforced_constraint()->mutable_int_prod();
  *int_prod->mutable_target() = prod_expr;
  *int_prod->add_exprs() = div_expr;
  *int_prod->add_exprs() = mod_expr;

  // expr - prod_expr = target_expr.
  LinearConstraintProto* const lin =
      new_enforced_constraint()->mutable_linear();
  lin->add_domain(0);
  lin->add_domain(0);
  AddLinearExpressionToLinearConstraint(expr, 1, lin);
  AddLinearExpressionToLinearConstraint(prod_expr, -1, lin);
  AddLinearExpressionToLinearConstraint(target_expr, -1, lin);

  context->solution_crush().SetIntModExpandedVars(*ct, div_var, prod_var,
                                                  context->MinOf(div_var),
                                                  context->MinOf(prod_var));
  ct->Clear();
  context->UpdateRuleStats("int_mod: expanded");
}

void ExpandIntProd(ConstraintProto* ct, PresolveContext* context) {
  if (ct->int_prod().exprs_size() <= 2) return;
  std::deque<LinearExpressionProto> terms(
      {ct->int_prod().exprs().begin(), ct->int_prod().exprs().end()});
  std::vector<int> new_vars;
  while (terms.size() > 2) {
    const LinearExpressionProto& left = terms[0];
    const LinearExpressionProto& right = terms[1];
    const Domain new_domain =
        context->DomainSuperSetOf(left).ContinuousMultiplicationBy(
            context->DomainSuperSetOf(right));
    const int new_var = context->NewIntVar(new_domain);
    new_vars.push_back(new_var);
    LinearArgumentProto* const int_prod =
        context->working_model->add_constraints()->mutable_int_prod();
    *int_prod->add_exprs() = left;
    *int_prod->add_exprs() = right;
    int_prod->mutable_target()->add_vars(new_var);
    int_prod->mutable_target()->add_coeffs(1);
    terms.pop_front();
    terms.front() = int_prod->target();
  }

  LinearArgumentProto* const final_int_prod =
      context->working_model->add_constraints()->mutable_int_prod();
  *final_int_prod->add_exprs() = terms[0];
  *final_int_prod->add_exprs() = terms[1];
  *final_int_prod->mutable_target() = ct->int_prod().target();

  context->solution_crush().SetIntProdExpandedVars(ct->int_prod(), new_vars);
  context->UpdateRuleStats(absl::StrCat(
      "int_prod: expanded int_prod with arity ", ct->int_prod().exprs_size()));
  ct->Clear();
}

void ExpandInverse(ConstraintProto* ct, PresolveContext* context) {
  const auto& f_direct = ct->inverse().f_direct();
  const auto& f_inverse = ct->inverse().f_inverse();
  const int n = f_direct.size();
  CHECK_EQ(n, f_inverse.size());

  // Make sure the domains are included in [0, n - 1).
  // Note that if a variable and its negation appear, the domains will be set to
  // zero here.
  //
  // TODO(user): Add support for UNSAT at expansion. This should create empty
  // domain if UNSAT, so it should still work correctly.
  absl::flat_hash_set<int> used_variables;
  for (const int ref : f_direct) {
    used_variables.insert(PositiveRef(ref));
    if (!context->IntersectDomainWith(ref, Domain(0, n - 1))) {
      VLOG(1) << "Empty domain for a variable in ExpandInverse()";
      return;
    }
  }
  for (const int ref : f_inverse) {
    used_variables.insert(PositiveRef(ref));
    if (!context->IntersectDomainWith(ref, Domain(0, n - 1))) {
      VLOG(1) << "Empty domain for a variable in ExpandInverse()";
      return;
    }
  }

  // If we have duplicate variables, we make sure the domain are reduced
  // as the loop below might not detect incompatibilities.
  if (used_variables.size() != 2 * n) {
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        // Note that if we don't have the same sign, both domain are at zero.
        if (PositiveRef(f_direct[i]) != PositiveRef(f_inverse[j])) continue;

        // We can't have i or j as value if i != j.
        if (i == j) continue;
        if (!context->IntersectDomainWith(
                f_direct[i], Domain::FromValues({i, j}).Complement())) {
          return;
        }
      }
    }
  }

  // Reduce the domains of each variable by checking that the inverse value
  // exists.
  std::vector<int64_t> possible_values;

  // Propagate from one vector to its counterpart.
  const auto filter_inverse_domain =
      [context, n, &possible_values](const auto& direct, const auto& inverse) {
        // Propagate from the inverse vector to the direct vector.
        for (int i = 0; i < n; ++i) {
          possible_values.clear();
          const Domain domain = context->DomainOf(direct[i]);
          bool removed_value = false;
          for (const int64_t j : domain.Values()) {
            if (context->DomainOf(inverse[j]).Contains(i)) {
              possible_values.push_back(j);
            } else {
              removed_value = true;
            }
          }
          if (removed_value) {
            if (!context->IntersectDomainWith(
                    direct[i], Domain::FromValues(possible_values))) {
              VLOG(1) << "Empty domain for a variable in ExpandInverse()";
              return false;
            }
          }
        }
        return true;
      };

  // Note that this should reach the fixed point in one pass.
  // However, if we have duplicate variable, I am not sure.
  if (!filter_inverse_domain(f_direct, f_inverse)) return;
  if (!filter_inverse_domain(f_inverse, f_direct)) return;

  // Expand the inverse constraint by associating literal to var == value
  // and sharing them between the direct and inverse variables.
  //
  // Note that this is only correct because the domain are tight now.
  for (int i = 0; i < n; ++i) {
    const int f_i = f_direct[i];
    for (const int64_t j : context->DomainOf(f_i).Values()) {
      // We have f[i] == j <=> r[j] == i;
      const int r_j = f_inverse[j];
      int r_j_i;
      if (context->HasVarValueEncoding(r_j, i, &r_j_i)) {
        if (!context->InsertVarValueEncoding(r_j_i, f_i, j)) {
          return;
        }
      } else {
        const int f_i_j = context->GetOrCreateVarValueEncoding(f_i, j);
        if (!context->InsertVarValueEncoding(f_i_j, r_j, i)) {
          return;
        }
      }
    }
  }

  ct->Clear();
  context->UpdateRuleStats("inverse: expanded");
}

void ExpandLinMax(ConstraintProto* ct, PresolveContext* context) {
  const int num_exprs = ct->lin_max().exprs().size();
  if (num_exprs < 2) return;

  // We have a special treatment for Abs, Earliness, Tardiness, and all
  // affine_max where there is only one variable present in all the expressions.
  if (ExpressionsContainsOnlyOneVar(ct->lin_max().exprs())) return;

  // We will create 2 * num_exprs constraints for target = max(a1, ..., an).

  // First.
  // - target >= ai
  for (const LinearExpressionProto& expr : ct->lin_max().exprs()) {
    ConstraintProto* new_ct = context->working_model->add_constraints();
    LinearConstraintProto* lin = new_ct->mutable_linear();
    lin->add_domain(0);
    lin->add_domain(std::numeric_limits<int64_t>::max());
    AddLinearExpressionToLinearConstraint(ct->lin_max().target(), 1, lin);
    AddLinearExpressionToLinearConstraint(expr, -1, lin);
    context->CanonicalizeLinearConstraint(new_ct);
  }

  // Second, for each expr, create a new boolean bi, and add bi => target <= ai
  // With exactly_one(bi)
  std::vector<int> enforcement_literals;
  enforcement_literals.reserve(num_exprs);
  if (num_exprs == 2) {
    const int new_bool = context->NewBoolVar("lin max expansion");
    enforcement_literals.push_back(new_bool);
    enforcement_literals.push_back(NegatedRef(new_bool));
  } else {
    ConstraintProto* exactly_one = context->working_model->add_constraints();
    for (int i = 0; i < num_exprs; ++i) {
      const int new_bool = context->NewBoolVar("lin max expansion");
      exactly_one->mutable_exactly_one()->add_literals(new_bool);
      enforcement_literals.push_back(new_bool);
    }
  }

  for (int i = 0; i < num_exprs; ++i) {
    ConstraintProto* new_ct = context->working_model->add_constraints();
    new_ct->add_enforcement_literal(enforcement_literals[i]);
    LinearConstraintProto* lin = new_ct->mutable_linear();
    lin->add_domain(std::numeric_limits<int64_t>::min());
    lin->add_domain(0);
    AddLinearExpressionToLinearConstraint(ct->lin_max().target(), 1, lin);
    AddLinearExpressionToLinearConstraint(ct->lin_max().exprs(i), -1, lin);
    context->CanonicalizeLinearConstraint(new_ct);
  }

  context->solution_crush().SetLinMaxExpandedVars(ct->lin_max(),
                                                  enforcement_literals);
  context->UpdateRuleStats("lin_max: expanded lin_max");
  ct->Clear();
}

// A[V] == V means for all i, V == i => A_i == i
void ExpandElementWhenTargetShareVarWithIndex(ConstraintProto* ct,
                                              PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();

  const LinearExpressionProto& index = element.linear_index();
  DCHECK_EQ(index.vars_size(), 1);
  const int index_var = index.vars(0);
  const LinearExpressionProto& target = element.linear_target();
  DCHECK_EQ(target.vars_size(), 1);
  DCHECK_EQ(target.vars(0), index_var);

  for (const int64_t v : context->DomainOf(index_var).Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    const int64_t target_value = AffineExpressionValueAt(target, v);
    const LinearExpressionProto& expr = element.exprs(index_value);
    ConstraintProto* imply = context->working_model->add_constraints();
    imply->add_enforcement_literal(
        context->GetOrCreateVarValueEncoding(index_var, v));
    imply->mutable_linear()->add_domain(target_value);
    imply->mutable_linear()->add_domain(target_value);
    AddLinearExpressionToLinearConstraint(expr, 1, imply->mutable_linear());
    context->CanonicalizeLinearConstraint(imply);
  }

  context->UpdateRuleStats(
      "element: expanded when the index and the target share the same var");
  ct->Clear();
}

// Special case if the array of the element is filled with constant values.
void ExpandConstantArrayElement(ConstraintProto* ct, PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();
  const LinearExpressionProto& index = element.linear_index();
  DCHECK_EQ(index.vars_size(), 1);
  const int index_var = index.vars(0);
  const LinearExpressionProto& target = element.linear_target();

  // Index and target domain have been reduced before calling this function.
  const Domain index_var_domain = context->DomainOf(index_var);
  const Domain target_domain = context->DomainSuperSetOf(target);

  // This BoolOrs implements the deduction that if all index literals pointing
  // to the same value in the constant array are false, then this value is no
  // no longer valid for the target variable. They are created only for values
  // that have multiples literals supporting them.
  absl::btree_map<int64_t, std::vector<int>> supports;
  for (const int64_t v : index_var_domain.Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    const int64_t expr_value = context->FixedValue(element.exprs(index_value));
    supports[expr_value].push_back(v);
  }

  // While this is not strictly needed since all value in the index will be
  // covered, it allows to easily detect this fact in the presolve.
  //
  // TODO(user): Do we need the support part ? Is this discovered by probing?
  auto* exactly_one =
      context->working_model->add_constraints()->mutable_exactly_one();
  for (const auto& [expr_value, support] : supports) {
    const int target_literal =
        context->GetOrCreateAffineValueEncoding(target, expr_value);
    // not(indices supporting value) -> target != value
    BoolArgumentProto* bool_or =
        context->working_model->add_constraints()->mutable_bool_or();
    bool_or->add_literals(NegatedRef(target_literal));
    for (const int64_t v : support) {
      const int index_literal =
          context->GetOrCreateVarValueEncoding(index_var, v);
      bool_or->add_literals(index_literal);

      // index == v => target == value
      context->AddImplication(index_literal, target_literal);

      // Helps presolve.
      exactly_one->add_literals(index_literal);
    }
  }

  context->UpdateRuleStats("element: expanded value element");
  ct->Clear();
}

// General element when the array contains non fixed variables.
void ExpandVariableElement(ConstraintProto* ct, PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();
  const LinearExpressionProto& index = element.linear_index();
  DCHECK_EQ(index.vars_size(), 1);
  const int index_var = index.vars(0);
  const Domain index_var_domain = context->DomainOf(index_var);
  const LinearExpressionProto& target = element.linear_target();

  BoolArgumentProto* exactly_one =
      context->working_model->add_constraints()->mutable_exactly_one();

  for (const int64_t v : index_var_domain.Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    DCHECK_GE(index_value, 0);
    DCHECK_LT(index_value, element.exprs_size());
    const int index_lit = context->GetOrCreateVarValueEncoding(index_var, v);
    exactly_one->add_literals(index_lit);

    ConstraintProto* const imply = context->working_model->add_constraints();
    imply->add_enforcement_literal(index_lit);
    imply->mutable_linear()->add_domain(0);
    imply->mutable_linear()->add_domain(0);
    AddLinearExpressionToLinearConstraint(target, -1, imply->mutable_linear());
    AddLinearExpressionToLinearConstraint(ct->element().exprs(index_value), 1,
                                          imply->mutable_linear());
    context->CanonicalizeLinearConstraint(imply);

    // Note that this should have been checked at model validation.
    DCHECK(!PossibleIntegerOverflow(*context->working_model,
                                    imply->mutable_linear()->vars(),
                                    imply->mutable_linear()->coeffs()))
        << google::protobuf::ShortFormat(*imply);
  }

  context->UpdateRuleStats("element: expanded");
  ct->Clear();
}

void ExpandElement(ConstraintProto* ct, PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();

  const LinearExpressionProto& index = element.linear_index();
  const LinearExpressionProto& target = element.linear_target();
  const int size = element.exprs_size();

  // Reduce the domain of the index to be compatible with the array of
  // variables. Note that the element constraint is 0 based.
  if (!context->IntersectDomainWith(index, Domain(0, size - 1))) {
    VLOG(1) << "Empty domain for the index variable in ExpandElement()";
    return;
  }

  if (context->IsFixed(index)) {
    ConstraintProto* const eq = context->working_model->add_constraints();
    eq->mutable_linear()->add_domain(0);
    eq->mutable_linear()->add_domain(0);
    AddLinearExpressionToLinearConstraint(target, 1, eq->mutable_linear());
    AddLinearExpressionToLinearConstraint(
        ct->element().exprs(context->FixedValue(index)), -1,
        eq->mutable_linear());
    context->CanonicalizeLinearConstraint(eq);
    context->UpdateRuleStats("element: expanded with fixed index");
    ct->Clear();
    return;
  }

  // Special case when index.var = target.var.
  if (index.vars_size() == 1 && target.vars_size() == 1 &&
      index.vars(0) == target.vars(0)) {
    ExpandElementWhenTargetShareVarWithIndex(ct, context);
    return;
  }

  // Checks if all elements are constant.
  bool all_constants = true;
  for (const int64_t v : context->DomainOf(index.vars(0)).Values()) {
    const int64_t index_value = AffineExpressionValueAt(index, v);
    if (!context->IsFixed(element.exprs(index_value))) {
      all_constants = false;
      break;
    }
  }

  if (all_constants) {
    ExpandConstantArrayElement(ct, context);
  } else {
    ExpandVariableElement(ct, context);
  }
}

// Adds clauses so that literals[i] true <=> encoding[values[i]] true.
// This also implicitly use the fact that exactly one alternative is true.
void LinkLiteralsAndValues(const std::vector<int>& literals,
                           const std::vector<int64_t>& values,
                           const absl::flat_hash_map<int64_t, int>& encoding,
                           PresolveContext* context) {
  CHECK_EQ(literals.size(), values.size());

  // We use a map to make this method deterministic.
  //
  // TODO(user): Make sure this does not appear in the profile.
  absl::btree_map<int, std::vector<int>> encoding_lit_to_support;

  // If a value is false (i.e not possible), then the tuple with this
  // value is false too (i.e not possible). Conversely, if the tuple is
  // selected, the value must be selected.
  for (int i = 0; i < values.size(); ++i) {
    encoding_lit_to_support[encoding.at(values[i])].push_back(literals[i]);
  }

  // If all tuples supporting a value are false, then this value must be
  // false.
  for (const auto& [encoding_lit, support] : encoding_lit_to_support) {
    CHECK(!support.empty());
    if (support.size() == 1) {
      if (!context->StoreBooleanEqualityRelation(encoding_lit, support[0])) {
        return;
      }
    } else {
      BoolArgumentProto* bool_or =
          context->working_model->add_constraints()->mutable_bool_or();
      bool_or->add_literals(NegatedRef(encoding_lit));
      for (const int lit : support) {
        bool_or->add_literals(lit);
        context->AddImplication(lit, encoding_lit);
      }
    }
  }
}

// Add the constraint literal => one_of(encoding[v]), for v in reachable_values.
// Note that all possible values are the ones appearing in encoding.
void AddImplyInReachableValues(int literal,
                               std::vector<int64_t>& reachable_values,
                               const absl::flat_hash_map<int64_t, int> encoding,
                               PresolveContext* context) {
  gtl::STLSortAndRemoveDuplicates(&reachable_values);
  if (reachable_values.size() == encoding.size()) return;  // No constraint.
  if (reachable_values.size() <= encoding.size() / 2) {
    // Bool or encoding.
    ConstraintProto* ct = context->working_model->add_constraints();
    ct->add_enforcement_literal(literal);
    BoolArgumentProto* bool_or = ct->mutable_bool_or();
    for (const int64_t v : reachable_values) {
      bool_or->add_literals(encoding.at(v));
    }
  } else {
    // Bool and encoding.
    absl::flat_hash_set<int64_t> set(reachable_values.begin(),
                                     reachable_values.end());
    ConstraintProto* ct = context->working_model->add_constraints();
    ct->add_enforcement_literal(literal);
    BoolArgumentProto* bool_and = ct->mutable_bool_and();
    for (const auto [value, literal] : encoding) {
      if (!set.contains(value)) {
        bool_and->add_literals(NegatedRef(literal));
      }
    }
  }
}

void ExpandAutomaton(ConstraintProto* ct, PresolveContext* context) {
  AutomatonConstraintProto& proto = *ct->mutable_automaton();

  if (proto.exprs_size() == 0) {
    const int64_t initial_state = proto.starting_state();
    for (const int64_t final_state : proto.final_states()) {
      if (initial_state == final_state) {
        context->UpdateRuleStats("automaton: empty and trivially feasible");
        ct->Clear();
        return;
      }
    }
    return (void)context->NotifyThatModelIsUnsat(
        "automaton: empty with an initial state not in the final states.");
  } else if (proto.transition_label_size() == 0) {
    return (void)context->NotifyThatModelIsUnsat(
        "automaton: non-empty with no transition.");
  }

  std::vector<absl::flat_hash_set<int64_t>> reachable_states;
  std::vector<absl::flat_hash_set<int64_t>> reachable_labels;
  PropagateAutomaton(proto, *context, &reachable_states, &reachable_labels);

  // We will model at each time step the current automaton state using Boolean
  // variables. We will have n+1 time step. At time zero, we start in the
  // initial state, and at time n we should be in one of the final states. We
  // don't need to create Booleans at times when there is just one possible
  // state (like at time zero).
  absl::flat_hash_map<int64_t, int> encoding;
  absl::flat_hash_map<int64_t, int> in_encoding;
  absl::flat_hash_map<int64_t, int> out_encoding;
  bool removed_values = false;

  const int n = proto.exprs_size();
  std::vector<SolutionCrush::StateVar> new_state_vars;
  std::vector<SolutionCrush::TransitionVar> new_transition_vars;
  for (int time = 0; time < n; ++time) {
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
      if (!context->DomainContains(proto.exprs(time), label)) continue;

      still_reachable_after_domain_change.insert(head);
      // TODO(user): if this transition correspond to just one in-state or
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
      if (!context->IntersectDomainWith(proto.exprs(time),
                                        Domain(labels.front()))) {
        VLOG(1) << "Infeasible automaton.";
        return;
      }

      // Tricky: when the same variable is used more than once, the propagation
      // above might not reach the fixed point, so we do need to fix literal
      // at false.
      std::vector<int> at_false;
      for (const auto [value, literal] : in_encoding) {
        if (value != in_states[0]) {
          if (!context->SetLiteralToFalse(literal)) return;
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
      if (!context->IntersectDomainWith(expr, Domain::FromValues(transitions),
                                        &removed_values)) {
        VLOG(1) << "Infeasible automaton.";
        return;
      }

      // Fully encode the variable.
      // We can leave the encoding empty for fixed vars.
      if (!context->IsFixed(expr)) {
        const int var = expr.vars(0);
        for (const int64_t v : context->DomainOf(var).Values()) {
          encoding[AffineExpressionValueAt(expr, v)] =
              context->GetOrCreateVarValueEncoding(var, v);
        }
      }
    }

    // Count how many time each value appear.
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

          // Same if we have an unique transition value that correspond only to
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
    // it propagate less. It creates a lot less Booleans though. Note that we
    // use implicit "exactly one" on the encoding and do not add any extra
    // exactly one if the simple encoding is used.
    //
    // We currently decide which encoding to use depending on the number of new
    // literals needed by the "heavy" encoding compared to the number of states
    // and labels. When the automaton is small, using the full encoding is
    // better, see for instance on rotating-workforce_Example789 were the simple
    // encoding make the problem hard to solve but the full encoding allow the
    // solver to solve it in a couple of seconds!
    //
    // Note that both encoding create about the same number of constraints.
    const int num_involved_variables =
        in_encoding.size() + encoding.size() + out_encoding.size();
    const bool use_light_encoding = (num_tuples > num_involved_variables);
    if (use_light_encoding && !in_encoding.empty() && !encoding.empty() &&
        !out_encoding.empty()) {
      // Part 1: If a in_state is selected, restrict the set of possible labels.
      // We also restrict the set of possible out states, but this is not needed
      // for correctness.
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
        AddImplyInReachableValues(in_literal, in_to_label[in_value], encoding,
                                  context);
        AddImplyInReachableValues(in_literal, in_to_out[in_value], out_encoding,
                                  context);
      }

      // Part2, add all 3-clauses: (in_state, label) => out_state.
      for (int i = 0; i < num_tuples; ++i) {
        auto* bool_or =
            context->working_model->add_constraints()->mutable_bool_or();
        bool_or->add_literals(NegatedRef(in_encoding.at(in_states[i])));
        bool_or->add_literals(NegatedRef(encoding.at(labels[i])));
        bool_or->add_literals(out_encoding.at(out_states[i]));
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
      BoolArgumentProto* exactly_one =
          context->working_model->add_constraints()->mutable_exactly_one();
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
        exactly_one->add_literals(tuple_literal);
      }
    }

    if (!in_encoding.empty()) {
      LinkLiteralsAndValues(tuple_literals, in_states, in_encoding, context);
    }
    if (!encoding.empty()) {
      LinkLiteralsAndValues(tuple_literals, labels, encoding, context);
    }
    if (!out_encoding.empty()) {
      LinkLiteralsAndValues(tuple_literals, out_states, out_encoding, context);
    }

    in_encoding.swap(out_encoding);
    out_encoding.clear();
  }

  context->solution_crush().SetAutomatonExpandedVars(proto, new_state_vars,
                                                     new_transition_vars);
  if (removed_values) {
    context->UpdateRuleStats("automaton: reduced variable domains");
  }
  context->UpdateRuleStats("automaton: expanded");
  ct->Clear();
}

bool TableIsInCanonicalForm(ConstraintProto* ct) {
  TableConstraintProto& table = *ct->mutable_table();
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
      LOG(ERROR) << "Expression contains an non-zero offset.";
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

void ExpandNegativeTable(ConstraintProto* ct, PresolveContext* context) {
  DCHECK(TableIsInCanonicalForm(ct));
  TableConstraintProto& table = *ct->mutable_table();
  if (table.values().empty()) {  // Early exit.
    context->UpdateRuleStats("table: empty negated constraint");
    ct->Clear();
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

  // For each tuple, forbid the variables values to be this tuple.
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
    ConstraintProto* tuple_ct = context->working_model->add_constraints();
    *tuple_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    BoolArgumentProto* bool_or = tuple_ct->mutable_bool_or();
    for (const int lit : clause) {
      bool_or->add_literals(lit);
    }
  }
  context->UpdateRuleStats("table: expanded negated constraint");
  ct->Clear();
}

// Add the implications and clauses to link one variable (i.e. column) of a
// table to the literals controlling if the tuples are possible or not.
//
// We list for each tuple the possible values the variable can take.
// If the list is empty, then this encode "any value".
void ProcessOneCompressedColumn(
    int variable, absl::Span<const int> tuple_literals,
    absl::Span<const absl::InlinedVector<int64_t, 2>> values,
    std::optional<int> table_is_active_literal, PresolveContext* context) {
  DCHECK_EQ(tuple_literals.size(), values.size());

  // Collect pairs of value-literal.
  // Add the constraint literal => one of values.
  //
  // TODO(user): If we have n - 1 values, we could add the constraint that
  // tuple literal => not(last_value) instead?
  std::vector<std::pair<int64_t, int>> pairs;
  std::vector<int> any_values_literals;
  for (int i = 0; i < values.size(); ++i) {
    if (values[i].empty()) {
      any_values_literals.push_back(tuple_literals[i]);
      continue;
    }

    ConstraintProto* ct = context->working_model->add_constraints();
    ct->add_enforcement_literal(tuple_literals[i]);

    // It is slightly better to use a bool_and if size is 1 instead of
    // reconverting it at a later stage.
    auto* literals =
        values[i].size() == 1 ? ct->mutable_bool_and() : ct->mutable_bool_or();
    for (const int64_t v : values[i]) {
      DCHECK(context->DomainContains(variable, v));
      literals->add_literals(context->GetOrCreateVarValueEncoding(variable, v));
      pairs.emplace_back(v, tuple_literals[i]);
    }
  }

  // Regroup literal with the same value and add for each the clause: If all the
  // tuples containing a value are false, then this value must be false too.
  std::vector<int> selected;
  std::sort(pairs.begin(), pairs.end());
  for (int i = 0; i < pairs.size();) {
    selected.clear();
    const int64_t value = pairs[i].first;
    for (; i < pairs.size() && pairs[i].first == value; ++i) {
      selected.push_back(pairs[i].second);
    }

    // A value is supported if one tuple is still active, or a covering 'any'
    // tuple is still active, or the table can still be inactive.
    BoolArgumentProto* no_support =
        context->working_model->add_constraints()->mutable_bool_or();
    for (const int lit : selected) {
      no_support->add_literals(lit);
    }
    for (const int lit : any_values_literals) {
      no_support->add_literals(lit);
    }
    if (table_is_active_literal.has_value()) {
      no_support->add_literals(NegatedRef(table_is_active_literal.value()));
    }

    // And the "value" literal.
    const int value_literal =
        context->GetOrCreateVarValueEncoding(variable, value);
    no_support->add_literals(NegatedRef(value_literal));
  }
}

// Simpler encoding for table constraints with 2 variables.
void AddSizeTwoTable(
    const std::vector<int>& vars,
    const std::vector<std::vector<int64_t>>& tuples,
    const std::vector<absl::flat_hash_set<int64_t>>& values_per_var,
    PresolveContext* context) {
  CHECK_EQ(vars.size(), 2);
  const int left_var = vars[0];
  const int right_var = vars[1];
  if (context->DomainOf(left_var).IsFixed() ||
      context->DomainOf(right_var).IsFixed()) {
    // A table constraint with at most one variable not fixed is trivially
    // enforced after domain reduction.
    return;
  }

  absl::btree_map<int, std::vector<int>> left_to_right;
  absl::btree_map<int, std::vector<int>> right_to_left;

  for (const auto& tuple : tuples) {
    const int64_t left_value(tuple[0]);
    const int64_t right_value(tuple[1]);
    DCHECK(context->DomainContains(left_var, left_value));
    DCHECK(context->DomainContains(right_var, right_value));

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
  auto add_support_constraint =
      [context, &num_clause_added, &num_large_clause_added, &num_implications](
          int lit, absl::Span<const int> support_literals,
          int max_support_size) {
        if (support_literals.size() == max_support_size) return;
        if (support_literals.size() == 1) {
          context->AddImplication(lit, support_literals.front());
          num_implications++;
        } else {
          BoolArgumentProto* bool_or =
              context->working_model->add_constraints()->mutable_bool_or();
          for (const int support_literal : support_literals) {
            bool_or->add_literals(support_literal);
          }
          bool_or->add_literals(NegatedRef(lit));
          num_clause_added++;
          if (support_literals.size() > max_support_size / 2) {
            num_large_clause_added++;
          }
        }
      };

  for (const auto& it : left_to_right) {
    add_support_constraint(it.first, it.second, values_per_var[1].size());
  }
  for (const auto& it : right_to_left) {
    add_support_constraint(it.first, it.second, values_per_var[0].size());
  }
  VLOG(2) << "Table: 2 variables, " << tuples.size() << " tuples encoded using "
          << num_clause_added << " clauses, including "
          << num_large_clause_added << " large clauses, " << num_implications
          << " implications";
}

// A "WCSP" (weighted constraint programming) problem is usually encoded as
// a set of table, with one or more variable only there to carry a cost.
//
// If this is the case, we can do special presolving.
bool ReduceTableInPresenceOfUniqueVariableWithCosts(
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
    // return false if the objective is constraining but we don't care here.
    // Our transformation also do not loose solutions.
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
  if (new_vars.size() == num_vars) return false;

  // Rewrite the tuples.
  // put the cost last.
  int64_t min_cost = std::numeric_limits<int64_t>::max();
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
        new_lin->add_domain((*tuples)[i][new_vars.size() + 1 + j]);
        new_lin->add_domain((*tuples)[i][new_vars.size() + 1 + j]);
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

  // This comes from the WCSP litterature. Basically, if by fixing a variable to
  // a value, we have only tuples with a non-zero cost, we can substract the
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
  return true;
}

// Important: the table and variable domains must be pre-solved before this
// is called. Some checks will fail otherwise.
void CompressAndExpandPositiveTable(ConstraintProto* ct,
                                    bool last_column_is_cost,
                                    absl::Span<const int> vars,
                                    std::vector<std::vector<int64_t>>* tuples,
                                    PresolveContext* context) {
  const int num_tuples_before_compression = tuples->size();

  // If the last column is actually the tuple cost, we compress the table like
  // if this was a normal variable, but afterwards we treat it differently.
  std::vector<int64_t> domain_sizes;
  for (const int var : vars) {
    domain_sizes.push_back(context->DomainOf(var).Size());
  }
  if (last_column_is_cost) {
    domain_sizes.push_back(std::numeric_limits<int64_t>::max());
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
  // TODO(user): Tweak the heuristic, maybe compute the reduction achieve and
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

  VLOG(2) << "Table compression"
          << " var=" << vars.size()
          << " cost=" << domain_sizes.size() - vars.size()
          << " tuples= " << num_tuples_before_compression << " -> "
          << num_tuples_after_first_compression << " -> "
          << compressed_table.size();

  // Affect mznc2017_aes_opt_r10 instance!
  std::sort(compressed_table.begin(), compressed_table.end());

  const int num_vars = vars.size();
  if (compressed_table.size() == 1 && ct->enforcement_literal().empty()) {
    // Domains are propagated. We can remove the constraint.
    context->UpdateRuleStats("table: one tuple");
    if (last_column_is_cost) {
      // TODO(user): Because we transfer the cost, this should always be zero,
      // so not needed.
      context->AddToObjectiveOffset(compressed_table[0].back()[0]);
    }
    return;
  }

  // Optimization. If a value is unique and appear alone in a cell, we can use
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
        DCHECK(context->DomainContains(vars[var_index], v));
        var_index_to_value_count[var_index][v]++;
      }
    }
  }

  // Create one Boolean variable per tuple to indicate if it can still be
  // selected or not. Enforce an exactly one between them.
  BoolArgumentProto* exactly_one =
      context->working_model->add_constraints()->mutable_exactly_one();

  std::optional<int> table_is_active_literal = std::nullopt;
  // Process enforcement literals.
  if (ct->enforcement_literal().size() == 1) {
    table_is_active_literal = ct->enforcement_literal(0);
  } else if (ct->enforcement_literal().size() > 1) {
    table_is_active_literal =
        context->NewBoolVarWithConjunction(ct->enforcement_literal());

    // Adds table_is_active <=> and(enforcement_literals).
    BoolArgumentProto* bool_or =
        context->working_model->add_constraints()->mutable_bool_or();
    bool_or->add_literals(table_is_active_literal.value());
    for (const int lit : ct->enforcement_literal()) {
      context->AddImplication(table_is_active_literal.value(), lit);
      bool_or->add_literals(NegatedRef(lit));
    }
  }
  std::vector<int> existing_row_literals;
  std::vector<SolutionCrush::TableRowLiteral> new_row_literals;
  if (table_is_active_literal.has_value()) {
    const int inactive_lit = NegatedRef(table_is_active_literal.value());
    exactly_one->add_literals(inactive_lit);
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
          !ct->enforcement_literal().empty()) {
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
    exactly_one->add_literals(tuple_literals[i]);
  }
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
void ExpandPositiveTable(ConstraintProto* ct, PresolveContext* context) {
  DCHECK(TableIsInCanonicalForm(ct));
  const TableConstraintProto& table = ct->table();
  if (table.exprs().empty()) {
    CHECK(table.values().empty());
    context->UpdateRuleStats("table: empty trivial");
    ct->Clear();
    return;
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
      if (!context->DomainContains(vars[var_index], value)) {
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
    if (ct->enforcement_literal().empty()) {
      context->UpdateRuleStats("table: empty");
      return (void)context->NotifyThatModelIsUnsat();
    } else {
      context->UpdateRuleStats("table: enforced and empty");
      BoolArgumentProto* bool_or =
          context->working_model->add_constraints()->mutable_bool_or();
      for (const int lit : ct->enforcement_literal()) {
        bool_or->add_literals(NegatedRef(lit));
      }
      ct->Clear();
      return;
    }
  }

  // Update variable domains. It is redundant with presolve, but we could be
  // here with presolve = false.
  // Also counts the number of fixed variables.
  if (ct->enforcement_literal().empty()) {
    int num_fixed_variables = 0;
    for (int var_index = 0; var_index < num_exprs; ++var_index) {
      CHECK(context->IntersectDomainWith(
          vars[var_index],
          Domain::FromValues({values_per_var[var_index].begin(),
                              values_per_var[var_index].end()})));
      if (context->DomainOf(vars[var_index]).IsFixed()) {
        num_fixed_variables++;
      }
    }

    if (num_fixed_variables == num_exprs - 1) {
      context->UpdateRuleStats("table: one variable not fixed");
      ct->Clear();
      return;
    } else if (num_fixed_variables == num_exprs) {
      context->UpdateRuleStats("table: all variables fixed");
      ct->Clear();
      return;
    }
  }

  // Tables with two variables do not need tuple literals.
  //
  // TODO(user): If there is an unique variable with cost, it is better to
  // detect it. But if the detection fail, we should still call
  // AddSizeTwoTable() unlike what happen here.
  if (num_exprs == 2 && !context->params().detect_table_with_cost() &&
      ct->enforcement_literal().empty()) {
    AddSizeTwoTable(vars, tuples, values_per_var, context);
    context->UpdateRuleStats(
        "table: expanded positive constraint with two variables");
    ct->Clear();
    return;
  }

  bool last_column_is_cost = false;
  if (context->params().detect_table_with_cost() &&
      ct->enforcement_literal().empty()) {
    last_column_is_cost =
        ReduceTableInPresenceOfUniqueVariableWithCosts(&vars, &tuples, context);
  }

  CompressAndExpandPositiveTable(ct, last_column_is_cost, vars, &tuples,
                                 context);
  ct->Clear();
}

bool AllDiffShouldBeExpanded(const Domain& union_of_domains,
                             ConstraintProto* ct, PresolveContext* context) {
  const AllDifferentConstraintProto& proto = *ct->mutable_all_diff();
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

  if (num_fully_encoded == num_exprs && union_of_domains.Size() < 256) {
    // All variables fully encoded, and domains are small enough.
    return true;
  }
  return false;
}

// Replaces a constraint literal => ax + by != cte by a set of clauses.
// This is performed if the domains are small enough, and the variables are
// fully encoded.
//
// We do it during the expansion as we want the first pass of the presolve to be
// complete.
void ExpandSomeLinearOfSizeTwo(ConstraintProto* ct, PresolveContext* context) {
  const LinearConstraintProto& arg = ct->linear();
  if (arg.vars_size() != 2) return;

  const int var1 = arg.vars(0);
  const int var2 = arg.vars(1);
  if (context->IsFixed(var1) || context->IsFixed(var2)) return;

  const int64_t coeff1 = arg.coeffs(0);
  const int64_t coeff2 = arg.coeffs(1);
  const Domain reachable_rhs_superset =
      context->DomainOf(var1)
          .MultiplicationBy(coeff1)
          .RelaxIfTooComplex()
          .AdditionWith(context->DomainOf(var2)
                            .MultiplicationBy(coeff2)
                            .RelaxIfTooComplex());
  const Domain infeasible_reachable_values =
      reachable_rhs_superset.IntersectionWith(
          ReadDomainFromProto(arg).Complement());

  // We only deal with != cte constraints.
  if (infeasible_reachable_values.Size() != 1) return;

  // coeff1 * v1 + coeff2 * v2 != cte.
  int64_t a = coeff1;
  int64_t b = coeff2;
  int64_t cte = infeasible_reachable_values.FixedValue();
  int64_t x0 = 0;
  int64_t y0 = 0;
  if (!SolveDiophantineEquationOfSizeTwo(a, b, cte, x0, y0)) {
    // no solution.
    context->UpdateRuleStats("linear: expand always feasible ax + by != cte");
    ct->Clear();
    return;
  }
  const Domain reduced_domain =
      context->DomainOf(var1)
          .AdditionWith(Domain(-x0))
          .InverseMultiplicationBy(b)
          .IntersectionWith(context->DomainOf(var2)
                                .AdditionWith(Domain(-y0))
                                .InverseMultiplicationBy(-a));

  if (reduced_domain.Size() > 16) return;

  // Check if all the needed values are encoded.
  // TODO(user): Do we force encoding for very small domains? Current
  // experiments says no, but revisit later.
  const int64_t size1 = context->DomainOf(var1).Size();
  const int64_t size2 = context->DomainOf(var2).Size();
  for (const int64_t z : reduced_domain.Values()) {
    const int64_t value1 = x0 + b * z;
    const int64_t value2 = y0 - a * z;
    DCHECK(context->DomainContains(var1, value1)) << "value1 = " << value1;
    DCHECK(context->DomainContains(var2, value2)) << "value2 = " << value2;
    DCHECK_EQ(coeff1 * value1 + coeff2 * value2,
              infeasible_reachable_values.FixedValue());
    // TODO(user): Presolve if one or two variables are Boolean.
    if (!context->HasVarValueEncoding(var1, value1, nullptr) || size1 == 2) {
      return;
    }
    if (!context->HasVarValueEncoding(var2, value2, nullptr) || size2 == 2) {
      return;
    }
  }

  // All encoding literals already exist and the number of clauses to create
  // is small enough. We can encode the constraint using just clauses.
  for (const int64_t z : reduced_domain.Values()) {
    const int64_t value1 = x0 + b * z;
    const int64_t value2 = y0 - a * z;
    // We cannot have both lit1 and lit2 true.
    const int lit1 = context->GetOrCreateVarValueEncoding(var1, value1);
    const int lit2 = context->GetOrCreateVarValueEncoding(var2, value2);
    auto* bool_or =
        context->working_model->add_constraints()->mutable_bool_or();
    bool_or->add_literals(NegatedRef(lit1));
    bool_or->add_literals(NegatedRef(lit2));
    for (const int lit : ct->enforcement_literal()) {
      bool_or->add_literals(NegatedRef(lit));
    }
  }

  context->UpdateRuleStats("linear: expand small ax + by != cte");
  ct->Clear();
}

// Note that we used to do that at loading time, but we prefer to do that as
// part of the presolve so that all variables are available for sharing between
// subworkers and also are accessible by the linear relaxation.
//
// TODO(user): Note that currently both encoding introduce extra solutions
// if the constraint has some enforcement literal(). We can either fix this by
// supporting enumeration on a subset of variable. Or add extra constraint to
// fix all new Boolean to false if the initial constraint is not enforced.
void ExpandComplexLinearConstraint(int c, ConstraintProto* ct,
                                   PresolveContext* context) {
  // TODO(user): We treat the linear of size 1 differently because we need them
  // as is to recognize value encoding. Try to still creates needed Boolean now
  // so that we can share more between the different workers. Or revisit how
  // linear1 are propagated.
  if (ct->linear().domain().size() <= 2) return;
  if (ct->linear().vars().size() == 1) return;

  const SatParameters& params = context->params();
  if (params.encode_complex_linear_constraint_with_integer()) {
    // Integer encoding.
    //
    // Here we add a slack with domain equal to rhs and transform
    // expr \in rhs to expr - slack = 0
    const Domain rhs = ReadDomainFromProto(ct->linear());
    const int slack = context->NewIntVar(rhs);
    context->solution_crush().SetVarToLinearExpression(
        slack, ct->linear().vars(), ct->linear().coeffs());
    ct->mutable_linear()->add_vars(slack);
    ct->mutable_linear()->add_coeffs(-1);
    ct->mutable_linear()->clear_domain();
    ct->mutable_linear()->add_domain(0);
    ct->mutable_linear()->add_domain(0);
  } else {
    // Boolean encoding.
    int single_bool;
    BoolArgumentProto* clause = nullptr;
    if (ct->enforcement_literal().empty() && ct->linear().domain_size() == 4) {
      // We cover the special case of no enforcement and two choices by creating
      // a single Boolean.
      single_bool = context->NewBoolVar("complex linear expansion");
    } else {
      clause = context->working_model->add_constraints()->mutable_bool_or();
      for (const int ref : ct->enforcement_literal()) {
        clause->add_literals(NegatedRef(ref));
      }
    }

    // Save enforcement literals for the enumeration.
    const std::vector<int> enforcement_literals(
        ct->enforcement_literal().begin(), ct->enforcement_literal().end());
    ct->mutable_enforcement_literal()->Clear();
    std::vector<int> domain_literals;
    for (int i = 0; i < ct->linear().domain_size(); i += 2) {
      const int64_t lb = ct->linear().domain(i);
      const int64_t ub = ct->linear().domain(i + 1);

      int subdomain_literal;
      if (clause != nullptr) {
        subdomain_literal = context->NewBoolVar("complex linear expansion");
        clause->add_literals(subdomain_literal);
        domain_literals.push_back(subdomain_literal);
      } else {
        if (i == 0) domain_literals.push_back(single_bool);
        subdomain_literal = i == 0 ? single_bool : NegatedRef(single_bool);
      }

      // Create a new constraint which is a copy of the original, but with a
      // simple sub-domain and enforcement literal.
      ConstraintProto* new_ct = context->working_model->add_constraints();
      *new_ct = *ct;
      new_ct->add_enforcement_literal(subdomain_literal);
      FillDomainInProto(Domain(lb, ub), new_ct->mutable_linear());
    }
    context->solution_crush().SetLinearWithComplexDomainExpandedVars(
        ct->linear(), domain_literals);

    // Make sure all booleans are tights when enumerating all solutions.
    if (context->params().enumerate_all_solutions() &&
        !enforcement_literals.empty()) {
      int linear_is_enforced;
      if (enforcement_literals.size() == 1) {
        linear_is_enforced = enforcement_literals[0];
      } else {
        linear_is_enforced = context->NewBoolVar("complex linear expansion");
        BoolArgumentProto* maintain_linear_is_enforced =
            context->working_model->add_constraints()->mutable_bool_or();
        for (const int e_lit : enforcement_literals) {
          context->AddImplication(NegatedRef(e_lit),
                                  NegatedRef(linear_is_enforced));
          maintain_linear_is_enforced->add_literals(NegatedRef(e_lit));
        }
        maintain_linear_is_enforced->add_literals(linear_is_enforced);
        context->solution_crush().SetVarToConjunction(linear_is_enforced,
                                                      enforcement_literals);
      }

      for (const int lit : domain_literals) {
        context->AddImplication(NegatedRef(linear_is_enforced),
                                NegatedRef(lit));
      }
    }
    ct->Clear();
  }

  context->UpdateRuleStats("linear: expanded complex rhs");
  context->InitializeNewDomains();
  context->UpdateNewConstraintsVariableUsage();
  context->UpdateConstraintVariableUsage(c);
}

bool IsVarEqOrNeqValue(PresolveContext* context,
                       const LinearConstraintProto& lin) {
  if (lin.vars_size() != 1) return false;
  const Domain rhs = ReadDomainFromProto(lin);

  // This is literal => var == value.
  if (rhs.IsFixed()) return true;

  // Is it literal => var != value ?
  const Domain not_implied =
      rhs.InverseMultiplicationBy(lin.coeffs(0))
          .Complement()
          .IntersectionWith(context->DomainOf(lin.vars(0)));
  if (not_implied.IsEmpty()) return false;
  return not_implied.IsFixed();
}

// This method will scan all constraints of all variables appearing in an
// all_diff.
// There are 3 outcomes:
//    - maybe expand to Boolean variables (depending on the size)
//    - keep integer all_different constraint (and cuts)
//    - expand and keep
//
// Expand is selected if the variable is fully encoded, or will be when
//   expanding other constraints: index of element, table, automaton.
//   It will check AllDiffShouldBeExpanded() before doing the actual expansion.
// Keep is forced is the variable appears in a linear equation with at least 3
// terms, and with a tight domain ( == cst).
// TODO(user): The above rule is complex. Revisit.
void ScanModelAndDecideAllDiffExpansion(
    ConstraintProto* all_diff_ct, PresolveContext* context,
    absl::flat_hash_set<int>& domain_of_var_is_used,
    absl::flat_hash_set<int>& bounds_of_var_are_used,
    absl::flat_hash_set<int>& processed_variables, bool& expand, bool& keep) {
  CHECK_EQ(all_diff_ct->constraint_case(), ConstraintProto::kAllDiff);

  bool at_least_one_var_domain_is_used = false;
  bool at_least_one_var_bound_is_used = false;

  // Scan variables.
  for (const LinearExpressionProto& expr : all_diff_ct->all_diff().exprs()) {
    // Skip constant expressions.
    if (expr.vars().empty()) continue;
    DCHECK_EQ(1, expr.vars_size());
    const int var = expr.vars(0);
    DCHECK(RefIsPositive(var));
    if (context->IsFixed(var)) continue;

    bool at_least_one_var_domain_is_used = false;
    bool at_least_one_var_bound_is_used = false;

    // Check cache.
    if (!processed_variables.insert(var).second) {
      at_least_one_var_domain_is_used = bounds_of_var_are_used.contains(var);
      at_least_one_var_bound_is_used = domain_of_var_is_used.contains(var);
    } else {
      bool domain_is_used = false;
      bool bounds_are_used = false;

      // Note: Boolean constraints are ignored.
      for (const int ct_index : context->VarToConstraints(var)) {
        // Skip artificial constraints.
        if (ct_index < 0) continue;

        const ConstraintProto& other_ct =
            context->working_model->constraints(ct_index);
        switch (other_ct.constraint_case()) {
          case ConstraintProto::ConstraintCase::kBoolOr:
            break;
          case ConstraintProto::ConstraintCase::kBoolAnd:
            break;
          case ConstraintProto::ConstraintCase::kAtMostOne:
            break;
          case ConstraintProto::ConstraintCase::kExactlyOne:
            break;
          case ConstraintProto::ConstraintCase::kBoolXor:
            break;
          case ConstraintProto::ConstraintCase::kIntDiv:
            break;
          case ConstraintProto::ConstraintCase::kIntMod:
            break;
          case ConstraintProto::ConstraintCase::kLinMax:
            bounds_are_used = true;
            break;
          case ConstraintProto::ConstraintCase::kIntProd:
            break;
          case ConstraintProto::ConstraintCase::kLinear:
            if (IsVarEqOrNeqValue(context, other_ct.linear()) &&
                var == other_ct.linear().vars(0)) {
              // Encoding literals.
              domain_is_used = true;
            } else if (other_ct.linear().vars_size() > 2 &&
                       other_ct.linear().domain_size() == 2 &&
                       other_ct.linear().domain(0) ==
                           other_ct.linear().domain(1)) {
              // We assume all_diff cuts will only be useful if the linear
              // constraint has a fixed domain.
              bounds_are_used = true;
            }
            break;
          case ConstraintProto::ConstraintCase::kAllDiff:
            // We ignore all_diffs as we are trying to decide their expansion
            // from the rest of the model.
            break;
          case ConstraintProto::ConstraintCase::kDummyConstraint:
            break;
          case ConstraintProto::ConstraintCase::kElement:
            // Note: elements should have been expanded.
            if (other_ct.element().index() == var) {
              domain_is_used = true;
            }
            break;
          case ConstraintProto::ConstraintCase::kCircuit:
            break;
          case ConstraintProto::ConstraintCase::kRoutes:
            break;
          case ConstraintProto::ConstraintCase::kInverse:
            domain_is_used = true;
            break;
          case ConstraintProto::ConstraintCase::kReservoir:
            break;
          case ConstraintProto::ConstraintCase::kTable:
            domain_is_used = true;
            break;
          case ConstraintProto::ConstraintCase::kAutomaton:
            domain_is_used = true;
            break;
          case ConstraintProto::ConstraintCase::kInterval:
            bounds_are_used = true;
            break;
          case ConstraintProto::ConstraintCase::kNoOverlap:
            // Will be covered by the interval case.
            break;
          case ConstraintProto::ConstraintCase::kNoOverlap2D:
            // Will be covered by the interval case.
            break;
          case ConstraintProto::ConstraintCase::kCumulative:
            // Will be covered by the interval case.
            break;
          case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
            break;
        }

        // Exit early.
        if (domain_is_used && bounds_are_used) break;
      }  // Loop on other_ct.

      // Update cache.
      if (domain_is_used) domain_of_var_is_used.insert(var);
      if (bounds_are_used) bounds_of_var_are_used.insert(var);

      // Update the usage of the variable.
      at_least_one_var_domain_is_used |= domain_is_used;
      at_least_one_var_bound_is_used |= bounds_are_used;
    }  // End of model scanning.

    if (at_least_one_var_domain_is_used && at_least_one_var_bound_is_used) {
      break;  // No need to scan the rest of the all_diff.
    }
  }  // End of var processing.

  expand = at_least_one_var_domain_is_used;
  keep = at_least_one_var_bound_is_used;
}

void MaybeExpandAllDiff(ConstraintProto* ct, PresolveContext* context,
                        absl::flat_hash_set<int>& domain_of_var_is_used,
                        absl::flat_hash_set<int>& bounds_of_var_are_used,
                        absl::flat_hash_set<int>& processed_variable) {
  const bool expand_all_diff_from_parameters =
      context->params().expand_alldiff_constraints();
  AllDifferentConstraintProto& proto = *ct->mutable_all_diff();
  if (proto.exprs_size() <= 1) return;
  if (context->ModelIsUnsat()) return;

  bool keep_after_expansion = false;
  bool expand_all_diff_from_usage = false;
  ScanModelAndDecideAllDiffExpansion(
      ct, context, domain_of_var_is_used, bounds_of_var_are_used,
      processed_variable, expand_all_diff_from_usage, keep_after_expansion);

  const int num_exprs = proto.exprs_size();
  Domain union_of_domains = context->DomainSuperSetOf(proto.exprs(0));
  for (int i = 1; i < num_exprs; ++i) {
    union_of_domains =
        union_of_domains.UnionWith(context->DomainSuperSetOf(proto.exprs(i)));
  }

  const bool expand_all_diff_from_size =
      AllDiffShouldBeExpanded(union_of_domains, ct, context);

  // Decide expansion:
  //  - always expand if expand_all_diff_from_parameters
  //  - expand if size is compatible (expand_all_diff_from_size) and
  //    expansion is desired:
  //       expand_all_diff_from_usage || !keep_after_expansion
  const bool should_expand =
      expand_all_diff_from_parameters ||
      (expand_all_diff_from_size &&
       (expand_all_diff_from_usage || !keep_after_expansion));
  if (!should_expand) return;

  const bool is_a_permutation = num_exprs == union_of_domains.Size();

  // Collect all possible variables that can take each value, and add one linear
  // equation per value stating that this value can be assigned at most once, or
  // exactly once in case of permutation.
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
      return (void)context->NotifyThatModelIsUnsat();
    } else if (fixed_expression_count == 1) {
      // Remove values from other domains.
      for (const LinearExpressionProto& expr : possible_exprs) {
        if (context->IsFixed(expr)) continue;
        if (!context->IntersectDomainWith(expr, Domain(v).Complement())) {
          VLOG(1) << "Empty domain for a variable in MaybeExpandAllDiff()";
          return;
        }
      }
    }

    BoolArgumentProto* at_most_or_equal_one =
        is_a_permutation
            ? context->working_model->add_constraints()->mutable_exactly_one()
            : context->working_model->add_constraints()->mutable_at_most_one();
    for (const LinearExpressionProto& expr : possible_exprs) {
      // The above propagation can remove a value after the expressions was
      // added to possible_exprs.
      if (!context->DomainContains(expr, v)) continue;

      // If the expression is fixed, the created literal will be the true
      // literal. We still need to fail if two expressions are fixed to the same
      // value.
      const int encoding = context->GetOrCreateAffineValueEncoding(expr, v);
      at_most_or_equal_one->add_literals(encoding);
    }
  }

  context->UpdateRuleStats(
      absl::StrCat("all_diff:", is_a_permutation ? " permutation" : "",
                   " expanded", keep_after_expansion ? " and kept" : ""));
  if (!keep_after_expansion) ct->Clear();
}

}  // namespace

void ExpandCpModel(PresolveContext* context) {
  if (context->params().disable_constraint_expansion()) return;
  if (context->ModelIsUnsat()) return;

  // None of the function here need to be run twice. This is because we never
  // create constraint that need to be expanded during presolve.
  if (context->ModelIsExpanded()) return;

  // Make sure all domains are initialized.
  context->InitializeNewDomains();
  if (context->ModelIsUnsat()) return;

  // Clear the precedence cache.
  context->ClearPrecedenceCache();

  bool has_all_diffs = false;

  // First pass: we look at constraints that may fully encode variables.
  for (int c = 0; c < context->working_model->constraints_size(); ++c) {
    ConstraintProto* const ct = context->working_model->mutable_constraints(c);
    bool skip = false;
    switch (ct->constraint_case()) {
      case ConstraintProto::kLinear:
        // If we only do expansion, we do that as part of the main loop.
        // This way we don't need to call FinalExpansionForLinearConstraint().
        if (ct->linear().domain().size() > 2 &&
            !context->params().cp_model_presolve()) {
          ExpandComplexLinearConstraint(c, ct, context);
        }
        break;
      case ConstraintProto::kReservoir:
        if (context->params().expand_reservoir_constraints()) {
          ExpandReservoir(ct, context);
        }
        break;
      case ConstraintProto::kCumulative:
        if (context->params().encode_cumulative_as_reservoir()) {
          EncodeCumulativeAsReservoir(ct, context);
        }
        break;
      case ConstraintProto::kIntMod:
        ExpandIntMod(ct, context);
        break;
      case ConstraintProto::kIntProd:
        ExpandIntProd(ct, context);
        break;
      case ConstraintProto::kElement:
        ExpandElement(ct, context);
        break;
      case ConstraintProto::kInverse:
        ExpandInverse(ct, context);
        break;
      case ConstraintProto::kAutomaton:
        ExpandAutomaton(ct, context);
        break;
      case ConstraintProto::kTable:
        if (!context->params().cp_model_presolve()) {
          CanonicalizeTable(context, ct);
        }
        if (ct->table().negated()) {
          ExpandNegativeTable(ct, context);
        } else {
          ExpandPositiveTable(ct, context);
        }
        break;
      case ConstraintProto::kLinMax:
        if (ct->lin_max().exprs().size() <=
            context->params().max_lin_max_size_for_expansion()) {
          ExpandLinMax(ct, context);
        }
        break;
      case ConstraintProto::kAllDiff:
        has_all_diffs = true;
        skip = true;
        break;
      default:
        skip = true;
        break;
    }
    if (skip) continue;  // Nothing was done for this constraint.

    // Update variable-constraint graph.
    context->UpdateNewConstraintsVariableUsage();
    if (ct->constraint_case() == ConstraintProto::CONSTRAINT_NOT_SET) {
      context->UpdateConstraintVariableUsage(c);
    }

    // Early exit if the model is unsat.
    if (context->ModelIsUnsat()) {
      SOLVER_LOG(context->logger(), "UNSAT after expansion of ",
                 ProtobufShortDebugString(*ct));
      return;
    }
  }

  // Second pass. We may decide to expand constraints if all their variables
  // are fully encoded.
  //
  // Cache for variable scanning.
  absl::flat_hash_set<int> domain_of_var_is_used;
  absl::flat_hash_set<int> bounds_of_var_are_used;
  absl::flat_hash_set<int> processed_variables;
  for (int i = 0; i < context->working_model->constraints_size(); ++i) {
    ConstraintProto* const ct = context->working_model->mutable_constraints(i);
    bool skip = false;
    switch (ct->constraint_case()) {
      case ConstraintProto::kAllDiff:
        MaybeExpandAllDiff(ct, context, domain_of_var_is_used,
                           bounds_of_var_are_used, processed_variables);
        break;
      case ConstraintProto::kLinear:
        ExpandSomeLinearOfSizeTwo(ct, context);
        break;
      default:
        skip = true;
        break;
    }

    if (skip) continue;  // Nothing was done for this constraint.

    // Update variable-constraint graph.
    context->UpdateNewConstraintsVariableUsage();
    if (ct->constraint_case() == ConstraintProto::CONSTRAINT_NOT_SET) {
      context->UpdateConstraintVariableUsage(i);
    }

    // Early exit if the model is unsat.
    if (context->ModelIsUnsat()) {
      SOLVER_LOG(context->logger(), "UNSAT after expansion of ",
                 ProtobufShortDebugString(*ct));
      return;
    }
  }

  // The precedence cache can become invalid during presolve as it does not
  // handle variable substitution. It is safer just to clear it at the end
  // of the expansion phase.
  context->ClearPrecedenceCache();

  // Make sure the context is consistent.
  context->InitializeNewDomains();

  // Update any changed domain from the context.
  for (int i = 0; i < context->working_model->variables_size(); ++i) {
    FillDomainInProto(context->DomainOf(i),
                      context->working_model->mutable_variables(i));
  }

  context->NotifyThatModelIsExpanded();
}

void FinalExpansionForLinearConstraint(PresolveContext* context) {
  if (context->params().disable_constraint_expansion()) return;
  if (context->ModelIsUnsat()) return;
  for (int c = 0; c < context->working_model->constraints_size(); ++c) {
    ConstraintProto* const ct = context->working_model->mutable_constraints(c);
    switch (ct->constraint_case()) {
      case ConstraintProto::kLinear:
        if (ct->linear().domain().size() > 2) {
          ExpandComplexLinearConstraint(c, ct, context);
        }
        break;
      default:
        break;
    }
  }
}

}  // namespace sat
}  // namespace operations_research
