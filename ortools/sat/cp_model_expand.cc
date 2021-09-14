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

#include "ortools/sat/cp_model_expand.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/util.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

void AddXEqualYOrXEqualZero(int x_eq_y, int x, int y,
                            PresolveContext* context) {
  ConstraintProto* equality = context->working_model->add_constraints();
  equality->add_enforcement_literal(x_eq_y);
  equality->mutable_linear()->add_vars(x);
  equality->mutable_linear()->add_coeffs(1);
  equality->mutable_linear()->add_vars(y);
  equality->mutable_linear()->add_coeffs(-1);
  equality->mutable_linear()->add_domain(0);
  equality->mutable_linear()->add_domain(0);
  context->AddImplyInDomain(NegatedRef(x_eq_y), x, {0, 0});
}

void ExpandReservoir(ConstraintProto* ct, PresolveContext* context) {
  if (ct->reservoir().min_level() > ct->reservoir().max_level()) {
    VLOG(1) << "Empty level domain in reservoir constraint.";
    return (void)context->NotifyThatModelIsUnsat();
  }

  const ReservoirConstraintProto& reservoir = ct->reservoir();
  const int num_events = reservoir.times_size();

  const int true_literal = context->GetOrCreateConstantVar(1);

  const auto is_active_literal = [&reservoir, true_literal](int index) {
    if (reservoir.actives_size() == 0) return true_literal;
    return reservoir.actives(index);
  };

  int num_positives = 0;
  int num_negatives = 0;
  for (const int64_t demand : reservoir.demands()) {
    if (demand > 0) {
      num_positives++;
    } else if (demand < 0) {
      num_negatives++;
    }
  }

  if (num_positives > 0 && num_negatives > 0) {
    // Creates Boolean variables equivalent to (start[i] <= start[j]) i != j
    for (int i = 0; i < num_events - 1; ++i) {
      const int active_i = is_active_literal(i);
      if (context->LiteralIsFalse(active_i)) continue;
      const int time_i = reservoir.times(i);

      for (int j = i + 1; j < num_events; ++j) {
        const int active_j = is_active_literal(j);
        if (context->LiteralIsFalse(active_j)) continue;
        const int time_j = reservoir.times(j);

        const int i_lesseq_j = context->GetOrCreateReifiedPrecedenceLiteral(
            time_i, time_j, active_i, active_j);
        context->working_model->mutable_variables(i_lesseq_j)
            ->set_name(absl::StrCat(i, " before ", j));
        const int j_lesseq_i = context->GetOrCreateReifiedPrecedenceLiteral(
            time_j, time_i, active_j, active_i);
        context->working_model->mutable_variables(j_lesseq_i)
            ->set_name(absl::StrCat(j, " before ", i));
      }
    }

    // Constrains the running level to be consistent at all times.
    // For this we only add a constraint at the time a given demand
    // take place. We also have a constraint for time zero if needed
    // (added below).
    for (int i = 0; i < num_events; ++i) {
      const int active_i = is_active_literal(i);
      if (context->LiteralIsFalse(active_i)) continue;
      const int time_i = reservoir.times(i);

      // Accumulates demands of all predecessors.
      ConstraintProto* const level = context->working_model->add_constraints();
      level->add_enforcement_literal(active_i);

      // Add contributions from previous events.
      for (int j = 0; j < num_events; ++j) {
        if (i == j) continue;
        const int active_j = is_active_literal(j);
        if (context->LiteralIsFalse(active_j)) continue;

        const int time_j = reservoir.times(j);
        level->mutable_linear()->add_vars(
            context->GetOrCreateReifiedPrecedenceLiteral(time_j, time_i,
                                                         active_j, active_i));
        level->mutable_linear()->add_coeffs(reservoir.demands(j));
      }

      // Accounts for own demand in the domain of the sum.
      const int64_t demand_i = reservoir.demands(i);
      level->mutable_linear()->add_domain(
          CapSub(reservoir.min_level(), demand_i));
      level->mutable_linear()->add_domain(
          CapSub(reservoir.max_level(), demand_i));
    }
  } else {
    // If all demands have the same sign, we do not care about the order, just
    // the sum.
    auto* const sum =
        context->working_model->add_constraints()->mutable_linear();
    for (int i = 0; i < num_events; ++i) {
      sum->add_vars(is_active_literal(i));
      sum->add_coeffs(reservoir.demands(i));
    }
    sum->add_domain(reservoir.min_level());
    sum->add_domain(reservoir.max_level());
  }

  ct->Clear();
  context->UpdateRuleStats("reservoir: expanded");
}

// a_ref spans across 0, b_ref does not.
void ExpandIntDivWithOneAcrossZero(int a_ref, int b_ref, int div_ref,
                                   PresolveContext* context) {
  DCHECK_LT(context->MinOf(a_ref), 0);
  DCHECK_GT(context->MaxOf(a_ref), 0);
  DCHECK_GT(context->MinOf(b_ref), 0);

  // Split the domain of a in two, controlled by a new literal.
  const int a_is_positive = context->NewBoolVar();
  context->AddImplyInDomain(a_is_positive, a_ref,
                            {0, std::numeric_limits<int64_t>::max()});
  context->AddImplyInDomain(NegatedRef(a_is_positive), a_ref,
                            {std::numeric_limits<int64_t>::min(), -1});
  const int pos_a_ref = context->NewIntVar({0, context->MaxOf(a_ref)});
  AddXEqualYOrXEqualZero(a_is_positive, pos_a_ref, a_ref, context);

  const int neg_a_ref = context->NewIntVar({context->MinOf(a_ref), 0});
  AddXEqualYOrXEqualZero(NegatedRef(a_is_positive), neg_a_ref, a_ref, context);

  // Create div with the positive part of a_ref.
  const int pos_a_div = context->NewIntVar({0, context->MaxOf(div_ref)});
  IntegerArgumentProto* pos_div =
      context->working_model->add_constraints()->mutable_int_div();
  pos_div->set_target(pos_a_div);
  pos_div->add_vars(pos_a_ref);
  pos_div->add_vars(b_ref);

  // Create div with the negative part of a_ref.
  const int neg_a_div = context->NewIntVar({context->MinOf(div_ref), 0});
  IntegerArgumentProto* neg_div =
      context->working_model->add_constraints()->mutable_int_div();
  neg_div->set_target(NegatedRef(neg_a_div));
  neg_div->add_vars(NegatedRef(neg_a_ref));
  neg_div->add_vars(b_ref);

  // Link back to the original div.
  LinearConstraintProto* lin =
      context->working_model->add_constraints()->mutable_linear();
  lin->add_vars(div_ref);
  lin->add_coeffs(-1);
  lin->add_vars(pos_a_div);
  lin->add_coeffs(1);
  lin->add_vars(neg_a_div);
  lin->add_coeffs(1);
  lin->add_domain(0);
  lin->add_domain(0);
}

// This is not an "expansion" per say, but just a mandatory presolve step to
// satisfy preconditions assumed by the rest of the code.
void ExpandIntDiv(ConstraintProto* ct, PresolveContext* context) {
  const int numerator = ct->int_div().vars(0);
  const int divisor = ct->int_div().vars(1);
  const int target = ct->int_div().target();
  if (!context->IntersectDomainWith(divisor, Domain(0).Complement())) {
    return (void)context->NotifyThatModelIsUnsat();
  }

  // The checker ensures that the domain of the divisor cannot span across 0,
  // nor take the value 0.
  DCHECK(context->MinOf(divisor) > 0 || context->MaxOf(divisor) < 0);
  if (context->MinOf(numerator) < 0 && context->MaxOf(numerator) > 0) {
    if (context->MinOf(divisor) > 0) {
      ExpandIntDivWithOneAcrossZero(numerator, divisor, target, context);
    } else {
      ExpandIntDivWithOneAcrossZero(NegatedRef(numerator), NegatedRef(divisor),
                                    target, context);
    }
    ct->Clear();
    context->UpdateRuleStats(
        "int_div: expanded division with numerator spanning across zero");
    return;
  }

  // Change the sign of the divisor if negative (by simply negating the target).
  if (context->MinOf(divisor) < 0) {
    ct->mutable_int_div()->set_target(NegatedRef(target));
    ct->mutable_int_div()->set_vars(1, NegatedRef(divisor));
    context->UpdateRuleStats("int_div: inverse the sign of the divisor");
  }

  // Change the sign of the numerator if negative (by simply negating the
  // target). It is okay to negate the target twice.
  // At this stage, the domain of the numerator cannot span across 0.
  if (context->MinOf(numerator) < 0) {
    ct->mutable_int_div()->set_target(NegatedRef(target));
    ct->mutable_int_div()->set_vars(0, NegatedRef(numerator));
    context->UpdateRuleStats("int_div: inverse the sign of the numerator");
  }
}

void ExpandIntMod(ConstraintProto* ct, PresolveContext* context) {
  const IntegerArgumentProto& int_mod = ct->int_mod();
  const int var = int_mod.vars(0);
  const int mod_var = int_mod.vars(1);
  const int target_var = int_mod.target();

  // We reduce the domain of target_var to avoid later overflow.
  if (!context->IntersectDomainWith(
          target_var, context->DomainOf(var).PositiveModuloBySuperset(
                          context->DomainOf(mod_var)))) {
    return;
  }

  // Create a new constraint with the same enforcement as ct.
  auto new_enforced_constraint = [&]() {
    ConstraintProto* new_ct = context->working_model->add_constraints();
    *new_ct->mutable_enforcement_literal() = ct->enforcement_literal();
    return new_ct;
  };

  // div_var = var / mod_var.
  const int div_var =
      context->NewIntVar(context->DomainOf(var).PositiveDivisionBySuperset(
          context->DomainOf(mod_var)));
  IntegerArgumentProto* const div_proto =
      new_enforced_constraint()->mutable_int_div();
  div_proto->set_target(div_var);
  div_proto->add_vars(var);
  div_proto->add_vars(mod_var);

  if (context->IsFixed(mod_var)) {
    // var - div_var * mod = target.
    LinearConstraintProto* const lin =
        new_enforced_constraint()->mutable_linear();
    lin->add_vars(int_mod.vars(0));
    lin->add_coeffs(1);
    lin->add_vars(div_var);
    lin->add_coeffs(-context->MinOf(mod_var));
    lin->add_vars(target_var);
    lin->add_coeffs(-1);
    lin->add_domain(0);
    lin->add_domain(0);
  } else {
    // Create prod_var = div_var * mod.
    const Domain prod_domain =
        context->DomainOf(div_var)
            .ContinuousMultiplicationBy(context->DomainOf(mod_var))
            .IntersectionWith(context->DomainOf(var).AdditionWith(
                context->DomainOf(target_var).Negation()));
    const int prod_var = context->NewIntVar(prod_domain);
    IntegerArgumentProto* const int_prod =
        new_enforced_constraint()->mutable_int_prod();
    int_prod->set_target(prod_var);
    int_prod->add_vars(div_var);
    int_prod->add_vars(mod_var);

    // var - prod_var = target.
    LinearConstraintProto* const lin =
        new_enforced_constraint()->mutable_linear();
    lin->add_vars(var);
    lin->add_coeffs(1);
    lin->add_vars(prod_var);
    lin->add_coeffs(-1);
    lin->add_vars(target_var);
    lin->add_coeffs(-1);
    lin->add_domain(0);
    lin->add_domain(0);
  }

  ct->Clear();
  context->UpdateRuleStats("int_mod: expanded");
}

void ExpandIntProdWithBoolean(int bool_ref, int int_ref, int product_ref,
                              PresolveContext* context) {
  ConstraintProto* const one = context->working_model->add_constraints();
  one->add_enforcement_literal(bool_ref);
  one->mutable_linear()->add_vars(int_ref);
  one->mutable_linear()->add_coeffs(1);
  one->mutable_linear()->add_vars(product_ref);
  one->mutable_linear()->add_coeffs(-1);
  one->mutable_linear()->add_domain(0);
  one->mutable_linear()->add_domain(0);

  ConstraintProto* const zero = context->working_model->add_constraints();
  zero->add_enforcement_literal(NegatedRef(bool_ref));
  zero->mutable_linear()->add_vars(product_ref);
  zero->mutable_linear()->add_coeffs(1);
  zero->mutable_linear()->add_domain(0);
  zero->mutable_linear()->add_domain(0);
}

// a_ref spans across 0, b_ref does not.
void ExpandIntProdWithOneAcrossZero(int a_ref, int b_ref, int product_ref,
                                    PresolveContext* context) {
  DCHECK_LT(context->MinOf(a_ref), 0);
  DCHECK_GT(context->MaxOf(a_ref), 0);
  DCHECK(context->MinOf(b_ref) >= 0 || context->MaxOf(b_ref) <= 0);

  // Split the domain of a in two, controlled by a new literal.
  const int a_is_positive = context->NewBoolVar();
  context->AddImplyInDomain(a_is_positive, a_ref,
                            {0, std::numeric_limits<int64_t>::max()});
  context->AddImplyInDomain(NegatedRef(a_is_positive), a_ref,
                            {std::numeric_limits<int64_t>::min(), -1});
  const int pos_a_ref = context->NewIntVar({0, context->MaxOf(a_ref)});
  AddXEqualYOrXEqualZero(a_is_positive, pos_a_ref, a_ref, context);

  const int neg_a_ref = context->NewIntVar({context->MinOf(a_ref), 0});
  AddXEqualYOrXEqualZero(NegatedRef(a_is_positive), neg_a_ref, a_ref, context);

  // Create product with the positive part ofa_ref.
  const bool b_is_positive = context->MinOf(b_ref) >= 0;
  const Domain pos_a_product_domain =
      b_is_positive ? Domain({0, context->MaxOf(product_ref)})
                    : Domain({context->MinOf(product_ref), 0});
  const int pos_a_product = context->NewIntVar(pos_a_product_domain);
  IntegerArgumentProto* pos_product =
      context->working_model->add_constraints()->mutable_int_prod();
  pos_product->set_target(pos_a_product);
  pos_product->add_vars(pos_a_ref);
  pos_product->add_vars(b_ref);

  // Create product with the negative part of a_ref.
  const Domain neg_a_product_domain =
      b_is_positive ? Domain({context->MinOf(product_ref), 0})
                    : Domain({0, context->MaxOf(product_ref)});
  const int neg_a_product = context->NewIntVar(neg_a_product_domain);
  IntegerArgumentProto* neg_product =
      context->working_model->add_constraints()->mutable_int_prod();
  neg_product->set_target(neg_a_product);
  neg_product->add_vars(neg_a_ref);
  neg_product->add_vars(b_ref);

  // Link back to the original product.
  LinearConstraintProto* lin =
      context->working_model->add_constraints()->mutable_linear();
  lin->add_vars(product_ref);
  lin->add_coeffs(-1);
  lin->add_vars(pos_a_product);
  lin->add_coeffs(1);
  lin->add_vars(neg_a_product);
  lin->add_coeffs(1);
  lin->add_domain(0);
  lin->add_domain(0);
}

void ExpandPositiveIntProdWithTwoAcrossZero(int a_ref, int b_ref,
                                            int product_ref,
                                            PresolveContext* context) {
  const int terms_are_positive = context->NewBoolVar();

  const Domain product_domain = context->DomainOf(product_ref);

  const int64_t max_of_a = context->MaxOf(a_ref);
  const int64_t max_of_b = context->MaxOf(b_ref);
  const Domain positive_vars_domain =
      Domain(0, CapProd(max_of_a, max_of_b)).IntersectionWith(product_domain);
  int both_positive_product_ref = std::numeric_limits<int>::min();
  if (!positive_vars_domain.IsEmpty()) {
    const int pos_a_ref = context->NewIntVar({0, max_of_a});
    AddXEqualYOrXEqualZero(terms_are_positive, pos_a_ref, a_ref, context);
    const int pos_b_ref = context->NewIntVar({0, max_of_b});
    AddXEqualYOrXEqualZero(terms_are_positive, pos_b_ref, b_ref, context);

    // We add 0 to the domain in case this product is not selected.
    both_positive_product_ref =
        context->NewIntVar(positive_vars_domain.UnionWith(Domain(0)));
    IntegerArgumentProto* pos_product =
        context->working_model->add_constraints()->mutable_int_prod();
    pos_product->set_target(both_positive_product_ref);
    pos_product->add_vars(pos_a_ref);
    pos_product->add_vars(pos_b_ref);
  }

  const int64_t min_of_a = context->MinOf(a_ref);
  const int64_t min_of_b = context->MinOf(b_ref);
  const Domain negative_vars_domain =
      Domain(0, CapProd(min_of_a, min_of_b)).IntersectionWith(product_domain);
  int both_negative_product_ref = std::numeric_limits<int>::min();
  if (!negative_vars_domain.IsEmpty()) {
    const int neg_a_ref = context->NewIntVar({min_of_a, 0});
    AddXEqualYOrXEqualZero(NegatedRef(terms_are_positive), neg_a_ref, a_ref,
                           context);
    const int neg_b_ref = context->NewIntVar({min_of_b, 0});
    AddXEqualYOrXEqualZero(NegatedRef(terms_are_positive), neg_b_ref, b_ref,
                           context);
    // We add 0 to the domain in case this product is not selected.
    both_negative_product_ref =
        context->NewIntVar(negative_vars_domain.UnionWith(Domain(0)));
    IntegerArgumentProto* neg_product =
        context->working_model->add_constraints()->mutable_int_prod();
    neg_product->set_target(both_negative_product_ref);
    neg_product->add_vars(neg_a_ref);
    neg_product->add_vars(neg_b_ref);
  }

  // Link back to the original product.
  LinearConstraintProto* lin =
      context->working_model->add_constraints()->mutable_linear();
  lin->add_vars(product_ref);
  lin->add_coeffs(-1);
  if (both_positive_product_ref != std::numeric_limits<int>::min()) {
    lin->add_vars(both_positive_product_ref);
    lin->add_coeffs(1);
  }
  if (both_negative_product_ref != std::numeric_limits<int>::min()) {
    lin->add_vars(both_negative_product_ref);
    lin->add_coeffs(1);
  }
  lin->add_domain(0);
  lin->add_domain(0);
}

void ExpandIntProdWithTwoAcrossZero(int a_ref, int b_ref, int product_ref,
                                    PresolveContext* context) {
  if (context->MinOf(product_ref) >= 0) {
    ExpandPositiveIntProdWithTwoAcrossZero(a_ref, b_ref, product_ref, context);
    return;
  } else if (context->MaxOf(product_ref) <= 0) {
    ExpandPositiveIntProdWithTwoAcrossZero(a_ref, NegatedRef(b_ref),
                                           NegatedRef(product_ref), context);
    return;
  }
  // Split a_ref domain in two, controlled by a new literal.
  const int a_is_positive = context->NewBoolVar();
  context->AddImplyInDomain(a_is_positive, a_ref,
                            {0, std::numeric_limits<int64_t>::max()});
  context->AddImplyInDomain(NegatedRef(a_is_positive), a_ref,
                            {std::numeric_limits<int64_t>::min(), -1});
  const int64_t min_of_a = context->MinOf(a_ref);
  const int64_t max_of_a = context->MaxOf(a_ref);

  const int pos_a_ref = context->NewIntVar({0, max_of_a});
  AddXEqualYOrXEqualZero(a_is_positive, pos_a_ref, a_ref, context);

  const int neg_a_ref = context->NewIntVar({min_of_a, 0});
  AddXEqualYOrXEqualZero(NegatedRef(a_is_positive), neg_a_ref, a_ref, context);

  // Create product with two sub parts of a_ref.
  const int pos_product_ref =
      context->NewIntVar(context->DomainOf(product_ref));
  ExpandIntProdWithOneAcrossZero(b_ref, pos_a_ref, pos_product_ref, context);
  const int neg_product_ref =
      context->NewIntVar(context->DomainOf(product_ref));
  ExpandIntProdWithOneAcrossZero(b_ref, neg_a_ref, neg_product_ref, context);

  // Link back to the original product.
  LinearConstraintProto* lin =
      context->working_model->add_constraints()->mutable_linear();
  lin->add_vars(product_ref);
  lin->add_coeffs(-1);
  lin->add_vars(pos_product_ref);
  lin->add_coeffs(1);
  lin->add_vars(neg_product_ref);
  lin->add_coeffs(1);
  lin->add_domain(0);
  lin->add_domain(0);
}

void ExpandIntProd(ConstraintProto* ct, PresolveContext* context) {
  const IntegerArgumentProto& int_prod = ct->int_prod();
  if (int_prod.vars_size() != 2) return;
  const int a = int_prod.vars(0);
  const int b = int_prod.vars(1);
  const int p = int_prod.target();
  const bool a_is_boolean =
      RefIsPositive(a) && context->MinOf(a) == 0 && context->MaxOf(a) == 1;
  const bool b_is_boolean =
      RefIsPositive(b) && context->MinOf(b) == 0 && context->MaxOf(b) == 1;

  // We expand if exactly one of {a, b} is Boolean. If both are Boolean, it
  // will be presolved into a better version.
  if (a_is_boolean && !b_is_boolean) {
    ExpandIntProdWithBoolean(a, b, p, context);
    ct->Clear();
    context->UpdateRuleStats("int_prod: expanded product with Boolean var");
    return;
  }
  if (b_is_boolean && !a_is_boolean) {
    ExpandIntProdWithBoolean(b, a, p, context);
    ct->Clear();
    context->UpdateRuleStats("int_prod: expanded product with Boolean var");
    return;
  }

  const bool a_span_across_zero =
      context->MinOf(a) < 0 && context->MaxOf(a) > 0;
  const bool b_span_across_zero =
      context->MinOf(b) < 0 && context->MaxOf(b) > 0;
  if (a_span_across_zero && !b_span_across_zero) {
    ExpandIntProdWithOneAcrossZero(a, b, p, context);
    ct->Clear();
    context->UpdateRuleStats(
        "int_prod: expanded product with general integer variables");
    return;
  }
  if (!a_span_across_zero && b_span_across_zero) {
    ExpandIntProdWithOneAcrossZero(b, a, p, context);
    ct->Clear();
    context->UpdateRuleStats(
        "int_prod: expanded product with general integer variables");
    return;
  }
  if (a_span_across_zero && b_span_across_zero) {
    ExpandIntProdWithTwoAcrossZero(a, b, p, context);
    ct->Clear();
    context->UpdateRuleStats(
        "int_prod: expanded product with general integer variables");
    return;
  }
}

void ExpandInverse(ConstraintProto* ct, PresolveContext* context) {
  const int size = ct->inverse().f_direct().size();
  CHECK_EQ(size, ct->inverse().f_inverse().size());

  // Make sure the domains are included in [0, size - 1).
  //
  // TODO(user): Add support for UNSAT at expansion. This should create empty
  // domain if UNSAT, so it should still work correctly.
  for (const int ref : ct->inverse().f_direct()) {
    if (!context->IntersectDomainWith(ref, Domain(0, size - 1))) {
      VLOG(1) << "Empty domain for a variable in ExpandInverse()";
      return;
    }
  }
  for (const int ref : ct->inverse().f_inverse()) {
    if (!context->IntersectDomainWith(ref, Domain(0, size - 1))) {
      VLOG(1) << "Empty domain for a variable in ExpandInverse()";
      return;
    }
  }

  // Reduce the domains of each variable by checking that the inverse value
  // exists.
  std::vector<int64_t> possible_values;
  // Propagate from one vector to its counterpart.
  // Note this reaches the fixpoint as there is a one to one mapping between
  // (variable-value) pairs in each vector.
  const auto filter_inverse_domain = [context, size, &possible_values](
                                         const auto& direct,
                                         const auto& inverse) {
    // Propagate for the inverse vector to the direct vector.
    for (int i = 0; i < size; ++i) {
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

  if (!filter_inverse_domain(ct->inverse().f_direct(),
                             ct->inverse().f_inverse())) {
    return;
  }

  if (!filter_inverse_domain(ct->inverse().f_inverse(),
                             ct->inverse().f_direct())) {
    return;
  }

  // Expand the inverse constraint by associating literal to var == value
  // and sharing them between the direct and inverse variables.
  for (int i = 0; i < size; ++i) {
    const int f_i = ct->inverse().f_direct(i);
    for (const int64_t j : context->DomainOf(f_i).Values()) {
      // We have f[i] == j <=> r[j] == i;
      const int r_j = ct->inverse().f_inverse(j);
      int r_j_i;
      if (context->HasVarValueEncoding(r_j, i, &r_j_i)) {
        context->InsertVarValueEncoding(r_j_i, f_i, j);
      } else {
        const int f_i_j = context->GetOrCreateVarValueEncoding(f_i, j);
        context->InsertVarValueEncoding(f_i_j, r_j, i);
      }
    }
  }

  ct->Clear();
  context->UpdateRuleStats("inverse: expanded");
}

// A[V] == V means for all i, V == i => A_i == i
void ExpandElementWithTargetEqualIndex(ConstraintProto* ct,
                                       PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();
  DCHECK_EQ(element.index(), element.target());

  const int index_ref = element.index();
  std::vector<int64_t> valid_indices;
  for (const int64_t v : context->DomainOf(index_ref).Values()) {
    if (!context->DomainContains(element.vars(v), v)) continue;
    valid_indices.push_back(v);
  }
  if (valid_indices.size() < context->DomainOf(index_ref).Size()) {
    if (!context->IntersectDomainWith(index_ref,
                                      Domain::FromValues(valid_indices))) {
      VLOG(1) << "No compatible variable domains in "
                 "ExpandElementWithTargetEqualIndex()";
      return;
    }
    context->UpdateRuleStats("element: reduced index domain");
  }

  for (const int64_t v : context->DomainOf(index_ref).Values()) {
    const int var = element.vars(v);
    if (context->MinOf(var) == v && context->MaxOf(var) == v) continue;
    context->AddImplyInDomain(
        context->GetOrCreateVarValueEncoding(index_ref, v), var, Domain(v));
  }
  context->UpdateRuleStats(
      "element: expanded with special case target = index");
  ct->Clear();
}

// Special case if the array of the element is filled with constant values.
void ExpandConstantArrayElement(ConstraintProto* ct, PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();
  const int index_ref = element.index();
  const int target_ref = element.target();

  // Index and target domain have been reduced before calling this function.
  const Domain index_domain = context->DomainOf(index_ref);
  const Domain target_domain = context->DomainOf(target_ref);

  // This BoolOrs implements the deduction that if all index literals pointing
  // to the same value in the constant array are false, then this value is no
  // no longer valid for the target variable. They are created only for values
  // that have multiples literals supporting them.
  // Order is not important.
  absl::flat_hash_map<int64_t, BoolArgumentProto*> supports;
  {
    absl::flat_hash_map<int64_t, int> constant_var_values_usage;
    for (const int64_t v : index_domain.Values()) {
      DCHECK(context->IsFixed(element.vars(v)));
      const int64_t value = context->MinOf(element.vars(v));
      if (++constant_var_values_usage[value] == 2) {
        // First time we cross > 1.
        BoolArgumentProto* const support =
            context->working_model->add_constraints()->mutable_bool_or();
        const int target_literal =
            context->GetOrCreateVarValueEncoding(target_ref, value);
        support->add_literals(NegatedRef(target_literal));
        supports[value] = support;
      }
    }
  }

  {
    // While this is not stricly needed since all value in the index will be
    // covered, it allows to easily detect this fact in the presolve.
    auto* exactly_one =
        context->working_model->add_constraints()->mutable_exactly_one();
    for (const int64_t v : index_domain.Values()) {
      const int index_literal =
          context->GetOrCreateVarValueEncoding(index_ref, v);
      exactly_one->add_literals(index_literal);

      const int64_t value = context->MinOf(element.vars(v));
      const auto& it = supports.find(value);
      if (it != supports.end()) {
        // The encoding literal for 'value' of the target_ref has been
        // created before.
        const int target_literal =
            context->GetOrCreateVarValueEncoding(target_ref, value);
        context->AddImplication(index_literal, target_literal);
        it->second->add_literals(index_literal);
      } else {
        // Try to reuse the literal of the index.
        context->InsertVarValueEncoding(index_literal, target_ref, value);
      }
    }
  }

  context->UpdateRuleStats("element: expanded value element");
  ct->Clear();
}

// General element when the array contains non fixed variables.
void ExpandVariableElement(ConstraintProto* ct, PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();
  const int index_ref = element.index();
  const int target_ref = element.target();
  const Domain index_domain = context->DomainOf(index_ref);

  BoolArgumentProto* bool_or =
      context->working_model->add_constraints()->mutable_bool_or();

  for (const int64_t v : index_domain.Values()) {
    const int var = element.vars(v);
    const Domain var_domain = context->DomainOf(var);
    const int index_lit = context->GetOrCreateVarValueEncoding(index_ref, v);
    bool_or->add_literals(index_lit);

    if (var_domain.IsFixed()) {
      context->AddImplyInDomain(index_lit, target_ref, var_domain);
    } else {
      ConstraintProto* const ct = context->working_model->add_constraints();
      ct->add_enforcement_literal(index_lit);
      ct->mutable_linear()->add_vars(var);
      ct->mutable_linear()->add_coeffs(1);
      ct->mutable_linear()->add_vars(target_ref);
      ct->mutable_linear()->add_coeffs(-1);
      ct->mutable_linear()->add_domain(0);
      ct->mutable_linear()->add_domain(0);
    }
  }

  context->UpdateRuleStats("element: expanded");
  ct->Clear();
}

void ExpandElement(ConstraintProto* ct, PresolveContext* context) {
  const ElementConstraintProto& element = ct->element();

  const int index_ref = element.index();
  const int target_ref = element.target();
  const int size = element.vars_size();

  // Reduce the domain of the index to be compatible with the array of
  // variables. Note that the element constraint is 0 based.
  if (!context->IntersectDomainWith(index_ref, Domain(0, size - 1))) {
    VLOG(1) << "Empty domain for the index variable in ExpandElement()";
    return;
  }

  // Special case when index = target.
  if (index_ref == target_ref) {
    ExpandElementWithTargetEqualIndex(ct, context);
    return;
  }

  // Reduces the domain of the index and the target.
  bool all_constants = true;
  std::vector<int64_t> valid_indices;
  const Domain index_domain = context->DomainOf(index_ref);
  const Domain target_domain = context->DomainOf(target_ref);
  Domain reached_domain;
  for (const int64_t v : index_domain.Values()) {
    const Domain var_domain = context->DomainOf(element.vars(v));
    if (var_domain.IntersectionWith(target_domain).IsEmpty()) continue;

    valid_indices.push_back(v);
    reached_domain = reached_domain.UnionWith(var_domain);
    if (var_domain.Min() != var_domain.Max()) {
      all_constants = false;
    }
  }

  if (valid_indices.size() < index_domain.Size()) {
    if (!context->IntersectDomainWith(index_ref,
                                      Domain::FromValues(valid_indices))) {
      VLOG(1) << "No compatible variable domains in ExpandElement()";
      return;
    }

    context->UpdateRuleStats("element: reduced index domain");
  }

  // We know the target_domain is not empty as this would have triggered the
  // above check.
  bool target_domain_changed = false;
  if (!context->IntersectDomainWith(target_ref, reached_domain,
                                    &target_domain_changed)) {
    return;
  }

  if (target_domain_changed) {
    context->UpdateRuleStats("element: reduced target domain");
  }

  if (all_constants) {
    ExpandConstantArrayElement(ct, context);
    return;
  }

  ExpandVariableElement(ct, context);
}

// Adds clauses so that literals[i] true <=> encoding[value[i]] true.
// This also implicitly use the fact that exactly one alternative is true.
void LinkLiteralsAndValues(
    const std::vector<int>& value_literals, const std::vector<int64_t>& values,
    const absl::flat_hash_map<int64_t, int>& target_encoding,
    PresolveContext* context) {
  CHECK_EQ(value_literals.size(), values.size());

  // TODO(user): Make sure this does not appear in the profile.
  // We use a map to make this method deterministic.
  std::map<int, std::vector<int>> value_literals_per_target_literal;

  // If a value is false (i.e not possible), then the tuple with this
  // value is false too (i.e not possible). Conversely, if the tuple is
  // selected, the value must be selected.
  for (int i = 0; i < values.size(); ++i) {
    const int64_t v = values[i];
    CHECK(target_encoding.contains(v));
    const int lit = gtl::FindOrDie(target_encoding, v);
    value_literals_per_target_literal[lit].push_back(value_literals[i]);
  }

  // If all tuples supporting a value are false, then this value must be
  // false.
  for (const auto& it : value_literals_per_target_literal) {
    const int target_literal = it.first;
    switch (it.second.size()) {
      case 0: {
        if (!context->SetLiteralToFalse(target_literal)) {
          return;
        }
        break;
      }
      case 1: {
        context->StoreBooleanEqualityRelation(target_literal,
                                              it.second.front());
        break;
      }
      default: {
        BoolArgumentProto* const bool_or =
            context->working_model->add_constraints()->mutable_bool_or();
        bool_or->add_literals(NegatedRef(target_literal));
        for (const int value_literal : it.second) {
          bool_or->add_literals(value_literal);
          context->AddImplication(value_literal, target_literal);
        }
      }
    }
  }
}

void ExpandAutomaton(ConstraintProto* ct, PresolveContext* context) {
  AutomatonConstraintProto& proto = *ct->mutable_automaton();

  if (proto.vars_size() == 0) {
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

  const int n = proto.vars_size();
  const std::vector<int> vars = {proto.vars().begin(), proto.vars().end()};

  // Compute the set of reachable state at each time point.
  const absl::flat_hash_set<int64_t> final_states(
      {proto.final_states().begin(), proto.final_states().end()});
  std::vector<absl::flat_hash_set<int64_t>> reachable_states(n + 1);
  reachable_states[0].insert(proto.starting_state());

  // Forward pass.
  for (int time = 0; time < n; ++time) {
    for (int t = 0; t < proto.transition_tail_size(); ++t) {
      const int64_t tail = proto.transition_tail(t);
      const int64_t label = proto.transition_label(t);
      const int64_t head = proto.transition_head(t);
      if (!reachable_states[time].contains(tail)) continue;
      if (!context->DomainContains(vars[time], label)) continue;
      if (time == n - 1 && !final_states.contains(head)) continue;
      reachable_states[time + 1].insert(head);
    }
  }

  // Backward pass.
  for (int time = n - 1; time >= 0; --time) {
    absl::flat_hash_set<int64_t> new_set;
    for (int t = 0; t < proto.transition_tail_size(); ++t) {
      const int64_t tail = proto.transition_tail(t);
      const int64_t label = proto.transition_label(t);
      const int64_t head = proto.transition_head(t);

      if (!reachable_states[time].contains(tail)) continue;
      if (!context->DomainContains(vars[time], label)) continue;
      if (!reachable_states[time + 1].contains(head)) continue;
      new_set.insert(tail);
    }
    reachable_states[time].swap(new_set);
  }

  // We will model at each time step the current automaton state using Boolean
  // variables. We will have n+1 time step. At time zero, we start in the
  // initial state, and at time n we should be in one of the final states. We
  // don't need to create Booleans at at time when there is just one possible
  // state (like at time zero).
  absl::flat_hash_map<int64_t, int> encoding;
  absl::flat_hash_map<int64_t, int> in_encoding;
  absl::flat_hash_map<int64_t, int> out_encoding;
  bool removed_values = false;

  for (int time = 0; time < n; ++time) {
    // All these vector have the same size. We will use them to enforce a
    // local table constraint representing one step of the automaton at the
    // given time.
    std::vector<int64_t> in_states;
    std::vector<int64_t> transition_values;
    std::vector<int64_t> out_states;
    for (int i = 0; i < proto.transition_label_size(); ++i) {
      const int64_t tail = proto.transition_tail(i);
      const int64_t label = proto.transition_label(i);
      const int64_t head = proto.transition_head(i);

      if (!reachable_states[time].contains(tail)) continue;
      if (!reachable_states[time + 1].contains(head)) continue;
      if (!context->DomainContains(vars[time], label)) continue;

      // TODO(user): if this transition correspond to just one in-state or
      // one-out state or one variable value, we could reuse the corresponding
      // Boolean variable instead of creating a new one!
      in_states.push_back(tail);
      transition_values.push_back(label);

      // On the last step we don't need to distinguish the output states, so
      // we use zero.
      out_states.push_back(time + 1 == n ? 0 : head);
    }

    std::vector<int> tuple_literals;
    if (transition_values.size() == 1) {
      tuple_literals.push_back(context->GetOrCreateConstantVar(1));
      if (!context->IntersectDomainWith(vars[time],
                                        Domain(transition_values.front()))) {
        VLOG(1) << "Infeasible automaton.";
        return;
      }
      in_encoding.clear();
      continue;
    } else if (transition_values.size() == 2) {
      const int bool_var = context->NewBoolVar();
      tuple_literals.push_back(bool_var);
      tuple_literals.push_back(NegatedRef(bool_var));
    } else {
      // Note that we do not need the ExactlyOneConstraint(tuple_literals)
      // because it is already implicitly encoded since we have exactly one
      // transition value.
      LinearConstraintProto* const exactly_one =
          context->working_model->add_constraints()->mutable_linear();
      exactly_one->add_domain(1);
      exactly_one->add_domain(1);
      for (int i = 0; i < transition_values.size(); ++i) {
        const int tuple_literal = context->NewBoolVar();
        tuple_literals.push_back(tuple_literal);
        exactly_one->add_vars(tuple_literal);
        exactly_one->add_coeffs(1);
      }
    }

    // Fully encode vars[time].
    {
      std::vector<int64_t> s = transition_values;
      gtl::STLSortAndRemoveDuplicates(&s);

      encoding.clear();
      if (!context->IntersectDomainWith(vars[time], Domain::FromValues(s),
                                        &removed_values)) {
        VLOG(1) << "Infeasible automaton.";
        return;
      }

      // Fully encode the variable.
      for (const int64_t v : context->DomainOf(vars[time]).Values()) {
        encoding[v] = context->GetOrCreateVarValueEncoding(vars[time], v);
      }
    }

    // For each possible out states, create one Boolean variable.
    {
      std::vector<int64_t> s = out_states;
      gtl::STLSortAndRemoveDuplicates(&s);

      out_encoding.clear();
      if (s.size() == 2) {
        const int var = context->NewBoolVar();
        out_encoding[s.front()] = var;
        out_encoding[s.back()] = NegatedRef(var);
      } else if (s.size() > 2) {
        for (const int64_t state : s) {
          out_encoding[state] = context->NewBoolVar();
        }
      }
    }

    if (!in_encoding.empty()) {
      LinkLiteralsAndValues(tuple_literals, in_states, in_encoding, context);
    }
    if (!encoding.empty()) {
      LinkLiteralsAndValues(tuple_literals, transition_values, encoding,
                            context);
    }
    if (!out_encoding.empty()) {
      LinkLiteralsAndValues(tuple_literals, out_states, out_encoding, context);
    }
    in_encoding.swap(out_encoding);
    out_encoding.clear();
  }

  if (removed_values) {
    context->UpdateRuleStats("automaton: reduced variable domains");
  }
  context->UpdateRuleStats("automaton: expanded");
  ct->Clear();
}

void ExpandNegativeTable(ConstraintProto* ct, PresolveContext* context) {
  TableConstraintProto& table = *ct->mutable_table();
  const int num_vars = table.vars_size();
  const int num_original_tuples = table.values_size() / num_vars;
  std::vector<std::vector<int64_t>> tuples(num_original_tuples);
  int count = 0;
  for (int i = 0; i < num_original_tuples; ++i) {
    for (int j = 0; j < num_vars; ++j) {
      tuples[i].push_back(table.values(count++));
    }
  }

  if (tuples.empty()) {  // Early exit.
    context->UpdateRuleStats("table: empty negated constraint");
    ct->Clear();
    return;
  }

  // Compress tuples.
  const int64_t any_value = std::numeric_limits<int64_t>::min();
  std::vector<int64_t> domain_sizes;
  for (int i = 0; i < num_vars; ++i) {
    domain_sizes.push_back(context->DomainOf(table.vars(i)).Size());
  }
  CompressTuples(domain_sizes, any_value, &tuples);

  // For each tuple, forbid the variables values to be this tuple.
  std::vector<int> clause;
  for (const std::vector<int64_t>& tuple : tuples) {
    clause.clear();
    for (int i = 0; i < num_vars; ++i) {
      const int64_t value = tuple[i];
      if (value == any_value) continue;

      const int literal =
          context->GetOrCreateVarValueEncoding(table.vars(i), value);
      clause.push_back(NegatedRef(literal));
    }

    // Note: if the clause is empty, then the model is infeasible.
    BoolArgumentProto* bool_or =
        context->working_model->add_constraints()->mutable_bool_or();
    for (const int lit : clause) {
      bool_or->add_literals(lit);
    }
  }
  context->UpdateRuleStats("table: expanded negated constraint");
  ct->Clear();
}

void ExpandLinMin(ConstraintProto* ct, PresolveContext* context) {
  ConstraintProto* const lin_max = context->working_model->add_constraints();
  for (int i = 0; i < ct->enforcement_literal_size(); ++i) {
    lin_max->add_enforcement_literal(ct->enforcement_literal(i));
  }

  // Target
  SetToNegatedLinearExpression(ct->lin_min().target(),
                               lin_max->mutable_lin_max()->mutable_target());

  for (int i = 0; i < ct->lin_min().exprs_size(); ++i) {
    LinearExpressionProto* const expr = lin_max->mutable_lin_max()->add_exprs();
    SetToNegatedLinearExpression(ct->lin_min().exprs(i), expr);
  }
  ct->Clear();
}

// Add the implications and clauses to link one variable of a table to the
// literals controlling if the tuples are possible or not. The parallel vectors
// (tuple_literals, values) contains all valid projected tuples. The
// tuples_with_any vector provides a list of tuple_literals that will support
// any value.
void ProcessOneVariable(const std::vector<int>& tuple_literals,
                        const std::vector<int64_t>& values, int variable,
                        const std::vector<int>& tuples_with_any,
                        PresolveContext* context) {
  VLOG(2) << "Process var(" << variable << ") with domain "
          << context->DomainOf(variable) << " and " << values.size()
          << " active tuples, and " << tuples_with_any.size() << " any tuples";
  CHECK_EQ(tuple_literals.size(), values.size());
  std::vector<std::pair<int64_t, int>> pairs;

  // Collect pairs of value-literal.
  for (int i = 0; i < values.size(); ++i) {
    const int64_t value = values[i];
    CHECK(context->DomainContains(variable, value));
    pairs.emplace_back(value, tuple_literals[i]);
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

    CHECK(!selected.empty() || !tuples_with_any.empty());
    if (selected.size() == 1 && tuples_with_any.empty()) {
      context->InsertVarValueEncoding(selected.front(), variable, value);
    } else {
      const int value_literal =
          context->GetOrCreateVarValueEncoding(variable, value);
      BoolArgumentProto* no_support =
          context->working_model->add_constraints()->mutable_bool_or();
      for (const int lit : selected) {
        no_support->add_literals(lit);
        context->AddImplication(lit, value_literal);
      }
      for (const int lit : tuples_with_any) {
        no_support->add_literals(lit);
      }

      // And the "value" literal.
      no_support->add_literals(NegatedRef(value_literal));
    }
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

  std::map<int, std::vector<int>> left_to_right;
  std::map<int, std::vector<int>> right_to_left;

  for (const auto& tuple : tuples) {
    const int64_t left_value(tuple[0]);
    const int64_t right_value(tuple[1]);
    CHECK(context->DomainContains(left_var, left_value));
    CHECK(context->DomainContains(right_var, right_value));

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
          int lit, const std::vector<int>& support_literals,
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

void ExpandPositiveTable(ConstraintProto* ct, PresolveContext* context) {
  const TableConstraintProto& table = ct->table();
  const std::vector<int> vars(table.vars().begin(), table.vars().end());
  const int num_vars = table.vars_size();
  const int num_original_tuples = table.values_size() / num_vars;

  // Read tuples flat array and recreate the vector of tuples.
  std::vector<std::vector<int64_t>> tuples(num_original_tuples);
  int count = 0;
  for (int tuple_index = 0; tuple_index < num_original_tuples; ++tuple_index) {
    for (int var_index = 0; var_index < num_vars; ++var_index) {
      tuples[tuple_index].push_back(table.values(count++));
    }
  }

  // Compute the set of possible values for each variable (from the table).
  // Remove invalid tuples along the way.
  std::vector<absl::flat_hash_set<int64_t>> values_per_var(num_vars);
  int new_size = 0;
  for (int tuple_index = 0; tuple_index < num_original_tuples; ++tuple_index) {
    bool keep = true;
    for (int var_index = 0; var_index < num_vars; ++var_index) {
      const int64_t value = tuples[tuple_index][var_index];
      if (!context->DomainContains(vars[var_index], value)) {
        keep = false;
        break;
      }
    }
    if (keep) {
      for (int var_index = 0; var_index < num_vars; ++var_index) {
        values_per_var[var_index].insert(tuples[tuple_index][var_index]);
      }
      std::swap(tuples[tuple_index], tuples[new_size]);
      new_size++;
    }
  }
  tuples.resize(new_size);
  const int num_valid_tuples = tuples.size();

  if (tuples.empty()) {
    context->UpdateRuleStats("table: empty");
    return (void)context->NotifyThatModelIsUnsat();
  }

  // Update variable domains. It is redundant with presolve, but we could be
  // here with presolve = false.
  // Also counts the number of fixed variables.
  int num_fixed_variables = 0;
  for (int var_index = 0; var_index < num_vars; ++var_index) {
    CHECK(context->IntersectDomainWith(
        vars[var_index],
        Domain::FromValues({values_per_var[var_index].begin(),
                            values_per_var[var_index].end()})));
    if (context->DomainOf(vars[var_index]).IsFixed()) {
      num_fixed_variables++;
    }
  }

  if (num_fixed_variables == num_vars - 1) {
    context->UpdateRuleStats("table: one variable not fixed");
    ct->Clear();
    return;
  } else if (num_fixed_variables == num_vars) {
    context->UpdateRuleStats("table: all variables fixed");
    ct->Clear();
    return;
  }

  // Tables with two variables do not need tuple literals.
  if (num_vars == 2) {
    AddSizeTwoTable(vars, tuples, values_per_var, context);
    context->UpdateRuleStats(
        "table: expanded positive constraint with two variables");
    ct->Clear();
    return;
  }

  // It is easier to compute this before compression, as compression will merge
  // tuples.
  int num_prefix_tuples = 0;
  {
    absl::flat_hash_set<absl::Span<const int64_t>> prefixes;
    for (const std::vector<int64_t>& tuple : tuples) {
      prefixes.insert(absl::MakeSpan(tuple.data(), num_vars - 1));
    }
    num_prefix_tuples = prefixes.size();
  }

  // TODO(user): reinvestigate ExploreSubsetOfVariablesAndAddNegatedTables.

  // Compress tuples.
  const int64_t any_value = std::numeric_limits<int64_t>::min();
  std::vector<int64_t> domain_sizes;
  for (int i = 0; i < num_vars; ++i) {
    domain_sizes.push_back(values_per_var[i].size());
  }
  const int num_tuples_before_compression = tuples.size();
  CompressTuples(domain_sizes, any_value, &tuples);
  const int num_compressed_tuples = tuples.size();
  if (num_compressed_tuples < num_tuples_before_compression) {
    context->UpdateRuleStats("table: compress tuples");
  }

  if (num_compressed_tuples == 1) {
    // Domains are propagated. We can remove the constraint.
    context->UpdateRuleStats("table: one tuple");
    ct->Clear();
    return;
  }

  // Detect if prefix tuples are all different.
  const bool prefixes_are_all_different = num_prefix_tuples == num_valid_tuples;
  if (prefixes_are_all_different) {
    context->UpdateRuleStats(
        "TODO table: last value implied by previous values");
  }
  // TODO(user): if 2 table constraints share the same valid prefix, the
  // tuple literals can be reused.
  // TODO(user): investigate different encoding for prefix tables. Maybe
  // we can remove the need to create tuple literals.

  // Debug message to log the status of the expansion.
  if (VLOG_IS_ON(2)) {
    // Compute the maximum number of prefix tuples.
    int64_t max_num_prefix_tuples = 1;
    for (int var_index = 0; var_index + 1 < num_vars; ++var_index) {
      max_num_prefix_tuples =
          CapProd(max_num_prefix_tuples, values_per_var[var_index].size());
    }

    std::string message =
        absl::StrCat("Table: ", num_vars,
                     " variables, original tuples = ", num_original_tuples);
    if (num_valid_tuples != num_original_tuples) {
      absl::StrAppend(&message, ", valid tuples = ", num_valid_tuples);
    }
    if (prefixes_are_all_different) {
      if (num_prefix_tuples < max_num_prefix_tuples) {
        absl::StrAppend(&message, ", partial prefix = ", num_prefix_tuples, "/",
                        max_num_prefix_tuples);
      } else {
        absl::StrAppend(&message, ", full prefix = true");
      }
    } else {
      absl::StrAppend(&message, ", num prefix tuples = ", num_prefix_tuples);
    }
    if (num_compressed_tuples != num_valid_tuples) {
      absl::StrAppend(&message,
                      ", compressed tuples = ", num_compressed_tuples);
    }
    VLOG(2) << message;
  }

  // Log if we have only two tuples.
  if (num_compressed_tuples == 2) {
    context->UpdateRuleStats("TODO table: two tuples");
  }

  // Create one Boolean variable per tuple to indicate if it can still be
  // selected or not. Note that we don't enforce exactly one tuple to be
  // selected as this is costly.
  std::vector<int> tuple_literals(num_compressed_tuples);
  BoolArgumentProto* exactly_one =
      context->working_model->add_constraints()->mutable_exactly_one();

  for (int var_index = 0; var_index < num_compressed_tuples; ++var_index) {
    tuple_literals[var_index] = context->NewBoolVar();
    exactly_one->add_literals(tuple_literals[var_index]);
  }

  std::vector<int> active_tuple_literals;
  std::vector<int64_t> active_values;
  std::vector<int> any_tuple_literals;
  for (int var_index = 0; var_index < num_vars; ++var_index) {
    if (values_per_var[var_index].size() == 1) continue;

    active_tuple_literals.clear();
    active_values.clear();
    any_tuple_literals.clear();
    for (int tuple_index = 0; tuple_index < tuple_literals.size();
         ++tuple_index) {
      const int64_t value = tuples[tuple_index][var_index];
      const int tuple_literal = tuple_literals[tuple_index];

      if (value == any_value) {
        any_tuple_literals.push_back(tuple_literal);
      } else {
        active_tuple_literals.push_back(tuple_literal);
        active_values.push_back(value);
      }
    }

    if (!active_tuple_literals.empty()) {
      ProcessOneVariable(active_tuple_literals, active_values, vars[var_index],
                         any_tuple_literals, context);
    }
  }

  context->UpdateRuleStats("table: expanded positive constraint");
  ct->Clear();
}

void ExpandAllDiff(bool expand_non_permutations, ConstraintProto* ct,
                   PresolveContext* context) {
  AllDifferentConstraintProto& proto = *ct->mutable_all_diff();
  if (proto.vars_size() <= 2) return;

  const int num_vars = proto.vars_size();

  Domain union_of_domains = context->DomainOf(proto.vars(0));
  for (int i = 1; i < num_vars; ++i) {
    union_of_domains =
        union_of_domains.UnionWith(context->DomainOf(proto.vars(i)));
  }

  const bool is_a_permutation = proto.vars_size() == union_of_domains.Size();
  const bool has_small_domains =
      (union_of_domains.Size() <= 2 * proto.vars_size()) ||
      (union_of_domains.Size() <= 32);

  if (!is_a_permutation && !has_small_domains && !expand_non_permutations) {
    return;
  }

  // Collect all possible variables that can take each value, and add one linear
  // equation per value stating that this value can be assigned at most once, or
  // exactly once in case of permutation.
  for (const int64_t v : union_of_domains.Values()) {
    // Collect references which domain contains v.
    std::vector<int> possible_refs;
    int fixed_variable_count = 0;
    for (const int ref : proto.vars()) {
      if (!context->DomainContains(ref, v)) continue;
      possible_refs.push_back(ref);
      if (context->DomainOf(ref).IsFixed()) {
        fixed_variable_count++;
      }
    }

    if (fixed_variable_count > 1) {
      // Violates the definition of AllDifferent.
      return (void)context->NotifyThatModelIsUnsat();
    } else if (fixed_variable_count == 1) {
      // Remove values from other domains.
      for (const int ref : possible_refs) {
        if (context->DomainOf(ref).IsFixed()) continue;
        if (!context->IntersectDomainWith(ref, Domain(v).Complement())) {
          VLOG(1) << "Empty domain for a variable in ExpandAllDiff()";
          return;
        }
      }
    }

    LinearConstraintProto* at_most_or_equal_one =
        context->working_model->add_constraints()->mutable_linear();
    int lb = is_a_permutation ? 1 : 0;
    int ub = 1;
    for (const int ref : possible_refs) {
      DCHECK(context->DomainContains(ref, v));
      DCHECK_GT(context->DomainOf(ref).Size(), 1);
      const int encoding = context->GetOrCreateVarValueEncoding(ref, v);
      if (RefIsPositive(encoding)) {
        at_most_or_equal_one->add_vars(encoding);
        at_most_or_equal_one->add_coeffs(1);
      } else {
        at_most_or_equal_one->add_vars(PositiveRef(encoding));
        at_most_or_equal_one->add_coeffs(-1);
        lb--;
        ub--;
      }
    }
    at_most_or_equal_one->add_domain(lb);
    at_most_or_equal_one->add_domain(ub);
  }
  if (is_a_permutation) {
    context->UpdateRuleStats("alldiff: permutation expanded");
  } else {
    context->UpdateRuleStats("alldiff: expanded");
  }
  ct->Clear();
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

  // Clear the precedence cache.
  context->ClearPrecedenceCache();

  for (int i = 0; i < context->working_model->constraints_size(); ++i) {
    ConstraintProto* const ct = context->working_model->mutable_constraints(i);
    bool skip = false;
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintCase::kReservoir:
        if (context->params().expand_reservoir_constraints()) {
          ExpandReservoir(ct, context);
        }
        break;
      case ConstraintProto::ConstraintCase::kIntMod:
        ExpandIntMod(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kIntDiv:
        ExpandIntDiv(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kIntProd:
        ExpandIntProd(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kLinMin:
        ExpandLinMin(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kElement:
        if (context->params().expand_element_constraints()) {
          ExpandElement(ct, context);
        }
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        ExpandInverse(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kAutomaton:
        if (context->params().expand_automaton_constraints()) {
          ExpandAutomaton(ct, context);
        }
        break;
      case ConstraintProto::ConstraintCase::kTable:
        if (ct->table().negated()) {
          ExpandNegativeTable(ct, context);
        } else if (context->params().expand_table_constraints()) {
          ExpandPositiveTable(ct, context);
        }
        break;
      case ConstraintProto::ConstraintCase::kAllDiff:
        ExpandAllDiff(context->params().expand_alldiff_constraints(), ct,
                      context);
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

}  // namespace sat
}  // namespace operations_research
