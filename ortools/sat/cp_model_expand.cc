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

void ExpandReservoir(ConstraintProto* ct, PresolveContext* context) {
  if (ct->reservoir().min_level() > ct->reservoir().max_level()) {
    VLOG(1) << "Empty level domain in reservoir constraint.";
    return (void)context->NotifyThatModelIsUnsat();
  }

  const ReservoirConstraintProto& reservoir = ct->reservoir();
  const int num_events = reservoir.time_exprs_size();

  const int true_literal = context->GetOrCreateConstantVar(1);

  const auto is_active_literal = [&reservoir, true_literal](int index) {
    if (reservoir.active_literals_size() == 0) return true_literal;
    return reservoir.active_literals(index);
  };

  int num_positives = 0;
  int num_negatives = 0;
  for (const int64_t demand : reservoir.level_changes()) {
    if (demand > 0) {
      num_positives++;
    } else if (demand < 0) {
      num_negatives++;
    }
  }

  absl::flat_hash_map<std::pair<int, int>, int> precedence_cache;

  if (num_positives > 0 && num_negatives > 0) {
    // Creates Boolean variables equivalent to (start[i] <= start[j]) i != j
    for (int i = 0; i < num_events - 1; ++i) {
      const int active_i = is_active_literal(i);
      if (context->LiteralIsFalse(active_i)) continue;
      const LinearExpressionProto& time_i = reservoir.time_exprs(i);

      for (int j = i + 1; j < num_events; ++j) {
        const int active_j = is_active_literal(j);
        if (context->LiteralIsFalse(active_j)) continue;
        const LinearExpressionProto& time_j = reservoir.time_exprs(j);

        const int i_lesseq_j = context->GetOrCreateReifiedPrecedenceLiteral(
            time_i, time_j, active_i, active_j);
        context->working_model->mutable_variables(i_lesseq_j)
            ->set_name(absl::StrCat(i, " before ", j));
        precedence_cache[{i, j}] = i_lesseq_j;
        const int j_lesseq_i = context->GetOrCreateReifiedPrecedenceLiteral(
            time_j, time_i, active_j, active_i);
        context->working_model->mutable_variables(j_lesseq_i)
            ->set_name(absl::StrCat(j, " before ", i));
        precedence_cache[{j, i}] = j_lesseq_i;
      }
    }

    // Constrains the running level to be consistent at all time_exprs.
    // For this we only add a constraint at the time a given demand
    // take place. We also have a constraint for time zero if needed
    // (added below).
    for (int i = 0; i < num_events; ++i) {
      const int active_i = is_active_literal(i);
      if (context->LiteralIsFalse(active_i)) continue;

      // Accumulates level_changes of all predecessors.
      ConstraintProto* const level = context->working_model->add_constraints();
      level->add_enforcement_literal(active_i);

      // Add contributions from previous events.
      int64_t offset = 0;
      for (int j = 0; j < num_events; ++j) {
        if (i == j) continue;
        const int active_j = is_active_literal(j);
        if (context->LiteralIsFalse(active_j)) continue;

        const auto prec_it = precedence_cache.find({j, i});
        CHECK(prec_it != precedence_cache.end());
        const int prec_lit = prec_it->second;
        const int64_t demand = reservoir.level_changes(j);
        if (RefIsPositive(prec_lit)) {
          level->mutable_linear()->add_vars(prec_lit);
          level->mutable_linear()->add_coeffs(demand);
        } else {
          level->mutable_linear()->add_vars(prec_lit);
          level->mutable_linear()->add_coeffs(-demand);
          offset -= demand;
        }
      }

      // Accounts for own demand in the domain of the sum.
      const int64_t demand_i = reservoir.level_changes(i);
      level->mutable_linear()->add_domain(
          CapAdd(CapSub(reservoir.min_level(), demand_i), offset));
      level->mutable_linear()->add_domain(
          CapAdd(CapSub(reservoir.max_level(), demand_i), offset));
    }
  } else {
    // If all level_changes have the same sign, we do not care about the order,
    // just the sum.
    auto* const sum =
        context->working_model->add_constraints()->mutable_linear();
    for (int i = 0; i < num_events; ++i) {
      sum->add_vars(is_active_literal(i));
      sum->add_coeffs(reservoir.level_changes(i));
    }
    sum->add_domain(reservoir.min_level());
    sum->add_domain(reservoir.max_level());
  }

  ct->Clear();
  context->UpdateRuleStats("reservoir: expanded");
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

  ct->Clear();
  context->UpdateRuleStats("int_mod: expanded");
}

// TODO(user): Move this into the presolve instead?
void ExpandIntProdWithBoolean(int bool_ref,
                              const LinearExpressionProto& int_expr,
                              const LinearExpressionProto& product_expr,
                              PresolveContext* context) {
  ConstraintProto* const one = context->working_model->add_constraints();
  one->add_enforcement_literal(bool_ref);
  one->mutable_linear()->add_domain(0);
  one->mutable_linear()->add_domain(0);
  AddLinearExpressionToLinearConstraint(int_expr, 1, one->mutable_linear());
  AddLinearExpressionToLinearConstraint(product_expr, -1,
                                        one->mutable_linear());

  ConstraintProto* const zero = context->working_model->add_constraints();
  zero->add_enforcement_literal(NegatedRef(bool_ref));
  zero->mutable_linear()->add_domain(0);
  zero->mutable_linear()->add_domain(0);
  AddLinearExpressionToLinearConstraint(product_expr, 1,
                                        zero->mutable_linear());
}

void ExpandIntProd(ConstraintProto* ct, PresolveContext* context) {
  const LinearArgumentProto& int_prod = ct->int_prod();
  if (int_prod.exprs_size() != 2) return;
  const LinearExpressionProto& a = int_prod.exprs(0);
  const LinearExpressionProto& b = int_prod.exprs(1);
  const LinearExpressionProto& p = int_prod.target();
  int literal;
  const bool a_is_literal = context->ExpressionIsALiteral(a, &literal);
  const bool b_is_literal = context->ExpressionIsALiteral(b, &literal);

  // We expand if exactly one of {a, b} is a literal. If both are literals, it
  // will be presolved into a better version.
  if (a_is_literal && !b_is_literal) {
    ExpandIntProdWithBoolean(literal, b, p, context);
    ct->Clear();
    context->UpdateRuleStats("int_prod: expanded product with Boolean var");
  } else if (b_is_literal) {
    ExpandIntProdWithBoolean(literal, a, p, context);
    ct->Clear();
    context->UpdateRuleStats("int_prod: expanded product with Boolean var");
  }
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

// Adds clauses so that literals[i] true <=> encoding[values[i]] true.
// This also implicitly use the fact that exactly one alternative is true.
void LinkLiteralsAndValues(const std::vector<int>& literals,
                           const std::vector<int64_t>& values,
                           const absl::flat_hash_map<int64_t, int>& encoding,
                           PresolveContext* context) {
  CHECK_EQ(literals.size(), values.size());

  // We use a map to make this method deterministic.
  //
  // TODO(user): Make sure this does not appear in the profile. We could use
  // the same code as in ProcessOneVariable() otherwise.
  std::map<int, std::vector<int>> encoding_lit_to_support;

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
      context->StoreBooleanEqualityRelation(encoding_lit, support[0]);
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
    std::vector<int64_t> labels;
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
      labels.push_back(label);

      // On the last step we don't need to distinguish the output states, so
      // we use zero.
      out_states.push_back(time + 1 == n ? 0 : head);
    }

    // Deal with single tuple.
    const int num_tuples = in_states.size();
    if (num_tuples == 1) {
      if (!context->IntersectDomainWith(vars[time], Domain(labels.front()))) {
        VLOG(1) << "Infeasible automaton.";
        return;
      }
      in_encoding.clear();
      continue;
    }

    // Fully encode vars[time].
    {
      std::vector<int64_t> transitions = labels;
      gtl::STLSortAndRemoveDuplicates(&transitions);

      encoding.clear();
      if (!context->IntersectDomainWith(
              vars[time], Domain::FromValues(transitions), &removed_values)) {
        VLOG(1) << "Infeasible automaton.";
        return;
      }

      // Fully encode the variable.
      // We can leave the encoding empty for fixed vars.
      if (!context->IsFixed(vars[time])) {
        for (const int64_t v : context->DomainOf(vars[time]).Values()) {
          encoding[v] = context->GetOrCreateVarValueEncoding(vars[time], v);
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
        const int var = context->NewBoolVar();
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

          out_encoding[state] = context->NewBoolVar();
        }
      }
    }

    // Simple encoding. This is enough to properly enforce the constraint, but
    // it propagate less. It creates a lot less Booleans though. Note that we
    // use implicit "exactly one" on the encoding and do not add any extra
    // exacly one if the simple encoding is used.
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
      for (const auto [in_value, in_literal] : in_encoding) {
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
      const int bool_var = context->NewBoolVar();
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
          tuple_literal = context->NewBoolVar();
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

// Add the implications and clauses to link one variable of a table to the
// literals controlling if the tuples are possible or not. The parallel vectors
// (tuple_literals, values) contains all valid projected tuples.
//
// The special value "any_value" is used to indicate literal that will support
// any value.
void ProcessOneVariable(const std::vector<int>& tuple_literals,
                        const std::vector<int64_t>& values, int variable,
                        int64_t any_value, PresolveContext* context) {
  VLOG(2) << "Process var(" << variable << ") with domain "
          << context->DomainOf(variable) << " and " << values.size()
          << " tuples.";
  CHECK_EQ(tuple_literals.size(), values.size());

  // Collect pairs of value-literal.
  std::vector<int> tuples_with_any;
  std::vector<std::pair<int64_t, int>> pairs;
  for (int i = 0; i < values.size(); ++i) {
    const int64_t value = values[i];
    if (value == any_value) {
      tuples_with_any.push_back(tuple_literals[i]);
      continue;
    }
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
  const int num_vars = table.vars_size();
  const int num_original_tuples = table.values_size() / num_vars;

  // Read tuples flat array and recreate the vector of tuples.
  const std::vector<int> vars(table.vars().begin(), table.vars().end());
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
  // selected or not.
  std::vector<int> tuple_literals(num_compressed_tuples);
  BoolArgumentProto* exactly_one =
      context->working_model->add_constraints()->mutable_exactly_one();
  for (int i = 0; i < num_compressed_tuples; ++i) {
    tuple_literals[i] = context->NewBoolVar();
    exactly_one->add_literals(tuple_literals[i]);
  }

  std::vector<int64_t> values(num_compressed_tuples);
  for (int var_index = 0; var_index < num_vars; ++var_index) {
    if (values_per_var[var_index].size() == 1) continue;
    for (int i = 0; i < num_compressed_tuples; ++i) {
      values[i] = tuples[i][var_index];
    }
    ProcessOneVariable(tuple_literals, values, vars[var_index], any_value,
                       context);
  }

  context->UpdateRuleStats("table: expanded positive constraint");
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

void ExpandAllDiff(bool force_alldiff_expansion, ConstraintProto* ct,
                   PresolveContext* context) {
  AllDifferentConstraintProto& proto = *ct->mutable_all_diff();
  if (proto.exprs_size() <= 1) return;

  const int num_exprs = proto.exprs_size();
  Domain union_of_domains = context->DomainSuperSetOf(proto.exprs(0));
  for (int i = 1; i < num_exprs; ++i) {
    union_of_domains =
        union_of_domains.UnionWith(context->DomainSuperSetOf(proto.exprs(i)));
  }

  if (!AllDiffShouldBeExpanded(union_of_domains, ct, context) &&
      !force_alldiff_expansion) {
    return;
  }

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
          VLOG(1) << "Empty domain for a variable in ExpandAllDiff()";
          return;
        }
      }
    }

    BoolArgumentProto* at_most_or_equal_one =
        is_a_permutation
            ? context->working_model->add_constraints()->mutable_exactly_one()
            : context->working_model->add_constraints()->mutable_at_most_one();
    for (const LinearExpressionProto& expr : possible_exprs) {
      DCHECK(context->DomainContains(expr, v));
      DCHECK(!context->IsFixed(expr));
      const int encoding = context->GetOrCreateAffineValueEncoding(expr, v);
      at_most_or_equal_one->add_literals(encoding);
    }
  }
  if (is_a_permutation) {
    context->UpdateRuleStats("all_diff: permutation expanded");
  } else {
    context->UpdateRuleStats("all_diff: expanded");
  }
  ct->Clear();
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
      context->DomainOf(var1).MultiplicationBy(coeff1).AdditionWith(
          context->DomainOf(var2).MultiplicationBy(coeff2));

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
    // TODO(user, fdid): Presolve if one or two variables are Boolean.
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

  // First pass: we look at constraints that may fully encode variables.
  for (int i = 0; i < context->working_model->constraints_size(); ++i) {
    ConstraintProto* const ct = context->working_model->mutable_constraints(i);
    bool skip = false;
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintCase::kReservoir:
        ExpandReservoir(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kIntMod:
        ExpandIntMod(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kIntProd:
        ExpandIntProd(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kElement:
        ExpandElement(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        ExpandInverse(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kAutomaton:
        ExpandAutomaton(ct, context);
        break;
      case ConstraintProto::ConstraintCase::kTable:
        if (ct->table().negated()) {
          ExpandNegativeTable(ct, context);
        } else {
          ExpandPositiveTable(ct, context);
        }
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

  // Second pass. We may decide to expand constraints if all their variables
  // are fully encoded.
  for (int i = 0; i < context->working_model->constraints_size(); ++i) {
    ConstraintProto* const ct = context->working_model->mutable_constraints(i);
    bool skip = false;
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintCase::kAllDiff:
        ExpandAllDiff(context->params().expand_alldiff_constraints(), ct,
                      context);
        break;
      case ConstraintProto::ConstraintCase::kLinear:
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

}  // namespace sat
}  // namespace operations_research
