// Copyright 2010-2017 Google
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

#include "ortools/sat/cp_model_checker.h"

#include <algorithm>
#include <unordered_map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "ortools/base/logging.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

// =============================================================================
// CpModelProto validation.
// =============================================================================

// If the std::string returned by "statement" is not empty, returns it.
#define RETURN_IF_NOT_EMPTY(statement)                \
  do {                                                \
    const std::string error_message = statement;           \
    if (!error_message.empty()) return error_message; \
  } while (false)

template <typename ProtoWithDomain>
bool DomainInProtoIsValid(const ProtoWithDomain& proto) {
  std::vector<ClosedInterval> domain;
  for (int i = 0; i < proto.domain_size(); i += 2) {
    domain.push_back({proto.domain(i), proto.domain(i + 1)});
  }
  return IntervalsAreSortedAndDisjoint(domain);
}

std::string ValidateIntegerVariable(const CpModelProto& model, int v) {
  const IntegerVariableProto& proto = model.variables(v);
  if (proto.domain_size() == 0) {
    return StrCat("var #", v,
                        " has no domain(): ", proto.ShortDebugString());
  }
  if (proto.domain_size() % 2 != 0) {
    return StrCat(
        "var #", v, " has an odd domain() size: ", proto.ShortDebugString());
  }
  if (!DomainInProtoIsValid(proto)) {
    return StrCat("var #", v, " has and invalid domain() format: ",
                        proto.ShortDebugString());
  }
  return "";
}

bool VariableReferenceIsValid(const CpModelProto& model, int reference) {
  return std::max(-reference - 1, reference) < model.variables_size();
}

bool LiteralReferenceIsValid(const CpModelProto& model, int reference) {
  if (std::max(-reference - 1, reference) >= model.variables_size()) {
    return false;
  }
  const auto& var_proto = model.variables(PositiveRef(reference));
  const int64 min_domain = var_proto.domain(0);
  const int64 max_domain = var_proto.domain(var_proto.domain_size() - 1);
  return min_domain >= 0 && max_domain <= 1;
}

std::string ValidateArgumentReferencesInConstraint(const CpModelProto& model,
                                              int c) {
  const ConstraintProto& ct = model.constraints(c);
  IndexReferences references;
  AddReferencesUsedByConstraint(ct, &references);
  for (const int v : references.variables) {
    if (!VariableReferenceIsValid(model, v)) {
      return StrCat("Out of bound integer variable ", v,
                          " in constraint #", c, " : ", ct.ShortDebugString());
    }
  }
  if (ct.enforcement_literal_size() > 1) {
    return StrCat("More than one enforcement_literal in constraint #", c,
                        " : ", ct.ShortDebugString());
  }
  if (ct.enforcement_literal_size() == 1) {
    const int lit = ct.enforcement_literal(0);
    if (!LiteralReferenceIsValid(model, lit)) {
      return StrCat("Invalid enforcement literal ", lit,
                          " in constraint #", c, " : ", ct.ShortDebugString());
    }
  }
  for (const int lit : references.literals) {
    if (!LiteralReferenceIsValid(model, lit)) {
      return StrCat("Invalid literal ", lit, " in constraint #", c, " : ",
                          ct.ShortDebugString());
    }
  }
  for (const int i : references.intervals) {
    if (i < 0 || i >= model.constraints_size()) {
      return StrCat("Out of bound interval ", i, " in constraint #", c,
                          " : ", ct.ShortDebugString());
    }
    if (model.constraints(i).constraint_case() !=
        ConstraintProto::ConstraintCase::kInterval) {
      return StrCat(
          "Interval ", i,
          " does not refer to an interval constraint. Problematic constraint #",
          c, " : ", ct.ShortDebugString());
    }
  }
  return "";
}

std::string ValidateLinearConstraint(const CpModelProto& model,
                                const ConstraintProto& ct) {
  const LinearConstraintProto& arg = ct.linear();
  int64 sum_min = 0;
  int64 sum_max = 0;
  for (int i = 0; i < arg.vars_size(); ++i) {
    const int ref = arg.vars(i);
    const auto& var_proto = model.variables(PositiveRef(ref));
    const int64 min_domain = var_proto.domain(0);
    const int64 max_domain = var_proto.domain(var_proto.domain_size() - 1);
    const int64 coeff = RefIsPositive(ref) ? arg.coeffs(i) : -arg.coeffs(i);
    const int64 prod1 = CapProd(min_domain, coeff);
    const int64 prod2 = CapProd(max_domain, coeff);

    // Note that we use min/max with zero to disallow "alternative" terms and
    // be sure that we cannot have an overflow if we do the computation in a
    // different order.
    sum_min = CapAdd(sum_min, std::min(0ll, std::min(prod1, prod2)));
    sum_max = CapAdd(sum_max, std::max(0ll, std::max(prod1, prod2)));
    for (const int64 v : {prod1, prod2, sum_min, sum_max}) {
      if (v == kint64max || v == kint64min) {
        return "Possible integer overflow in constraint: " + ct.DebugString();
      }
    }
  }
  return "";
}

std::string ValidateReservoirConstraint(const CpModelProto& model,
                                   const ConstraintProto& ct) {
  for (const int t : ct.reservoir().times()) {
    const IntegerVariableProto& time = model.variables(t);
    for (const int64 bound : time.domain()) {
      if (bound < 0) {
        return StrCat("Time variables must be >= 0 in constraint ",
                            ct.ShortDebugString());
      }
    }
  }
  int64 sum_abs = 0;
  for (const int64 demand : ct.reservoir().demands()) {
    sum_abs = CapAdd(sum_abs, std::abs(demand));
    if (sum_abs == kint64max) {
      return "Possible integer overflow in constraint: " + ct.DebugString();
    }
  }
  return "";
}

std::string ValidateCircuitCoveringConstraint(const ConstraintProto& ct) {
  const int num_nodes = ct.circuit_covering().nexts_size();
  for (const int d : ct.circuit_covering().distinguished_nodes()) {
    if (d < 0 || d >= num_nodes) {
      return StrCat("Distinguished node ", d, " not in [0, ", num_nodes,
                          ").");
    }
  }
  return "";
}

std::string ValidateObjective(const CpModelProto& model,
                         const CpObjectiveProto& obj) {
  // TODO(user): share the code with ValidateLinearConstraint().
  if (obj.vars_size() == 1 && obj.coeffs(0) == 1) return "";
  int64 sum_min = 0;
  int64 sum_max = 0;
  for (int i = 0; i < obj.vars_size(); ++i) {
    const int ref = obj.vars(i);
    const auto& var_proto = model.variables(PositiveRef(ref));
    const int64 min_domain = var_proto.domain(0);
    const int64 max_domain = var_proto.domain(var_proto.domain_size() - 1);
    const int64 coeff = RefIsPositive(ref) ? obj.coeffs(i) : -obj.coeffs(i);
    const int64 prod1 = CapProd(min_domain, coeff);
    const int64 prod2 = CapProd(max_domain, coeff);

    // Note that we use min/max with zero to disallow "alternative" terms and
    // be sure that we cannot have an overflow if we do the computation in a
    // different order.
    sum_min = CapAdd(sum_min, std::min(0ll, std::min(prod1, prod2)));
    sum_max = CapAdd(sum_max, std::max(0ll, std::max(prod1, prod2)));
    for (const int64 v : {prod1, prod2, sum_min, sum_max}) {
      // When introducing the objective variable, we use a [...] domain so we
      // need to be more defensive here to make sure no overflow can happen in
      // linear constraint propagator.
      if (v == kint64max / 2 || v == kint64min / 2) {
        return "Possible integer overflow in objective: " + obj.DebugString();
      }
    }
  }
  return "";
}

}  // namespace

std::string ValidateCpModel(const CpModelProto& model) {
  for (int v = 0; v < model.variables_size(); ++v) {
    RETURN_IF_NOT_EMPTY(ValidateIntegerVariable(model, v));
  }
  for (int c = 0; c < model.constraints_size(); ++c) {
    RETURN_IF_NOT_EMPTY(ValidateArgumentReferencesInConstraint(model, c));

    // Other non-generic validations.
    // TODO(user): validate all constraints.
    const ConstraintProto& ct = model.constraints(c);
    const ConstraintProto::ConstraintCase type = ct.constraint_case();
    switch (type) {
      case ConstraintProto::ConstraintCase::kLinear:
        if (!DomainInProtoIsValid(ct.linear())) {
          return StrCat("Invalid domain in constraint #", c, " : ",
                              ct.ShortDebugString());
        }
        if (ct.linear().coeffs_size() != ct.linear().vars_size()) {
          return StrCat("coeffs_size() != vars_size() in constraint #", c,
                              " : ", ct.ShortDebugString());
        }
        RETURN_IF_NOT_EMPTY(ValidateLinearConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        if (ct.cumulative().intervals_size() !=
            ct.cumulative().demands_size()) {
          return StrCat(
              "intervals_size() != demands_size() in constraint #", c, " : ",
              ct.ShortDebugString());
        }
        break;
      case ConstraintProto::ConstraintCase::kReservoir:
        RETURN_IF_NOT_EMPTY(ValidateReservoirConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kCircuitCovering:
        RETURN_IF_NOT_EMPTY(ValidateCircuitCoveringConstraint(ct));
        break;
      default:
        break;
    }
  }
  if (model.has_objective()) {
    for (const int v : model.objective().vars()) {
      if (!VariableReferenceIsValid(model, v)) {
        return StrCat("Out of bound objective variable ", v, " : ",
                      model.objective().ShortDebugString());
      }
    }
    RETURN_IF_NOT_EMPTY(ValidateObjective(model, model.objective()));
  }

  return "";
}

#undef RETURN_IF_NOT_EMPTY

// =============================================================================
// Solution Feasibility.
// =============================================================================

namespace {

class ConstraintChecker {
 public:
  explicit ConstraintChecker(const std::vector<int64>& variable_values)
      : variable_values_(variable_values) {}

  bool LiteralIsTrue(int l) const {
    if (l >= 0) return variable_values_[l] != 0;
    return variable_values_[-l - 1] == 0;
  }

  bool LiteralIsFalse(int l) const { return !LiteralIsTrue(l); }

  int64 Value(int var) const {
    if (var >= 0) return variable_values_[var];
    return -variable_values_[-var - 1];
  }

  // Note that this does not check the variables like
  // ConstraintHasNonEnforcedVariables() does.
  bool ConstraintIsEnforced(const ConstraintProto& ct) {
    return !HasEnforcementLiteral(ct) ||
           LiteralIsTrue(ct.enforcement_literal(0));
  }

  bool ConstraintHasNonEnforcedVariables(const CpModelProto& model,
                                         const ConstraintProto& ct) {
    IndexReferences references;
    AddReferencesUsedByConstraint(ct, &references);
    for (const int ref : references.variables) {
      const auto& var_proto = model.variables(PositiveRef(ref));
      if (var_proto.enforcement_literal().empty()) continue;
      if (LiteralIsFalse(var_proto.enforcement_literal(0))) return true;
    }
    return false;
  }

  bool BoolOrConstraintIsFeasible(const ConstraintProto& ct) {
    for (const int lit : ct.bool_or().literals()) {
      if (LiteralIsTrue(lit)) return true;
    }
    return false;
  }

  bool BoolAndConstraintIsFeasible(const ConstraintProto& ct) {
    for (const int lit : ct.bool_and().literals()) {
      if (LiteralIsFalse(lit)) return false;
    }
    return true;
  }

  bool BoolXorConstraintIsFeasible(const ConstraintProto& ct) {
    int sum = 0;
    for (const int lit : ct.bool_xor().literals()) {
      sum ^= LiteralIsTrue(lit) ? 1 : 0;
    }
    return sum == 1;
  }

  // TODO(user): deal with integer overflows.
  bool LinearConstraintIsFeasible(const ConstraintProto& ct) {
    int64 sum = 0;
    const int num_variables = ct.linear().coeffs_size();
    for (int i = 0; i < num_variables; ++i) {
      sum += Value(ct.linear().vars(i)) * ct.linear().coeffs(i);
    }
    return DomainInProtoContains(ct.linear(), sum);
  }

  bool IntMaxConstraintIsFeasible(const ConstraintProto& ct) {
    const int64 max = Value(ct.int_max().target());
    int64 actual_max = kint64min;
    for (int i = 0; i < ct.int_max().vars_size(); ++i) {
      actual_max = std::max(actual_max, Value(ct.int_max().vars(i)));
    }
    return max == actual_max;
  }

  bool IntProdConstraintIsFeasible(const ConstraintProto& ct) {
    const int64 prod = Value(ct.int_prod().target());
    int64 actual_prod = 1;
    for (int i = 0; i < ct.int_prod().vars_size(); ++i) {
      actual_prod *= Value(ct.int_prod().vars(i));
    }
    return prod == actual_prod;
  }

  bool IntDivConstraintIsFeasible(const ConstraintProto& ct) {
    return Value(ct.int_div().target()) ==
           Value(ct.int_div().vars(0)) / Value(ct.int_div().vars(1));
  }

  bool IntMinConstraintIsFeasible(const ConstraintProto& ct) {
    const int64 min = Value(ct.int_min().target());
    int64 actual_min = kint64max;
    for (int i = 0; i < ct.int_min().vars_size(); ++i) {
      actual_min = std::min(actual_min, Value(ct.int_min().vars(i)));
    }
    return min == actual_min;
  }

  bool AllDiffConstraintIsFeasible(const ConstraintProto& ct) {
    std::unordered_set<int64> values;
    for (const int v : ct.all_diff().vars()) {
      if (ContainsKey(values, Value(v))) return false;
      values.insert(Value(v));
    }
    return true;
  }

  bool IntervalConstraintIsFeasible(const ConstraintProto& ct) {
    const int64 size = Value(ct.interval().size());
    if (size < 0) return false;
    return Value(ct.interval().start()) + size == Value(ct.interval().end());
  }

  bool NoOverlapConstraintIsFeasible(const CpModelProto& model,
                                     const ConstraintProto& ct) {
    std::vector<std::pair<int64, int64>> start_durations_pairs;
    for (const int i : ct.no_overlap().intervals()) {
      const ConstraintProto& interval_constraint = model.constraints(i);
      if (ConstraintIsEnforced(interval_constraint)) {
        const IntervalConstraintProto& interval =
            interval_constraint.interval();
        start_durations_pairs.push_back(
            {Value(interval.start()), Value(interval.size())});
      }
    }
    std::sort(start_durations_pairs.begin(), start_durations_pairs.end());
    int64 previous_end = kint64min;
    for (const auto pair : start_durations_pairs) {
      if (pair.first < previous_end) return false;
      previous_end = pair.first + pair.second;
    }
    return true;
  }

  bool IntervalsAreDisjoint(const IntervalConstraintProto& interval1,
                            const IntervalConstraintProto& interval2) {
    return Value(interval1.end()) <= Value(interval2.start()) ||
           Value(interval2.end()) <= Value(interval1.start());
  }

  bool NoOverlap2DConstraintIsFeasible(const CpModelProto& model,
                                       const ConstraintProto& ct) {
    const auto& arg = ct.no_overlap_2d();
    // Those intervals from arg.x_intervals and arg.y_intervals where both
    // the x and y intervals are enforced.
    std::vector<std::pair<const IntervalConstraintProto* const,
                          const IntervalConstraintProto* const>>
        enforced_intervals_xy;
    {
      const int num_intervals = arg.x_intervals_size();
      CHECK_EQ(arg.y_intervals_size(), num_intervals);
      for (int i = 0; i < num_intervals; ++i) {
        const ConstraintProto& x = model.constraints(arg.x_intervals(i));
        const ConstraintProto& y = model.constraints(arg.y_intervals(i));
        if (ConstraintIsEnforced(x) && ConstraintIsEnforced(y)) {
          enforced_intervals_xy.push_back({&x.interval(), &y.interval()});
        }
      }
    }
    const int num_enforced_intervals = enforced_intervals_xy.size();
    for (int i = 0; i < num_enforced_intervals; ++i) {
      for (int j = i + 1; j < num_enforced_intervals; ++j) {
        const auto& xi = *enforced_intervals_xy[i].first;
        const auto& yi = *enforced_intervals_xy[i].second;
        const auto& xj = *enforced_intervals_xy[j].first;
        const auto& yj = *enforced_intervals_xy[j].second;
        if (!IntervalsAreDisjoint(xi, xj) && !IntervalsAreDisjoint(yi, yj)) {
          return false;
        }
      }
    }
    return true;
  }

  bool CumulativeConstraintIsFeasible(const CpModelProto& model,
                                      const ConstraintProto& ct) {
    // TODO(user, fdid): Improve complexity for large durations.
    const int64 capacity = Value(ct.cumulative().capacity());
    const int num_intervals = ct.cumulative().intervals_size();
    std::unordered_map<int64, int64> usage;
    for (int i = 0; i < num_intervals; ++i) {
      const ConstraintProto interval_constraint =
          model.constraints(ct.cumulative().intervals(i));
      if (ConstraintIsEnforced(interval_constraint)) {
        const IntervalConstraintProto& interval =
            interval_constraint.interval();
        const int64 start = Value(interval.start());
        const int64 duration = Value(interval.size());
        const int64 demand = Value(ct.cumulative().demands(i));
        for (int64 t = start; t < start + duration; ++t) {
          usage[t] += demand;
          if (usage[t] > capacity) return false;
        }
      }
    }
    return true;
  }

  bool ElementConstraintIsFeasible(const ConstraintProto& ct) {
    const int index = Value(ct.element().index());
    return Value(ct.element().vars(index)) == Value(ct.element().target());
  }

  bool TableConstraintIsFeasible(const ConstraintProto& ct) {
    const int size = ct.table().vars_size();
    if (size == 0) return true;
    for (int row_start = 0; row_start < ct.table().values_size();
         row_start += size) {
      int i = 0;
      while (Value(ct.table().vars(i)) == ct.table().values(row_start + i)) {
        ++i;
        if (i == size) return !ct.table().negated();
      }
    }
    return ct.table().negated();
  }

  bool AutomataConstraintIsFeasible(const ConstraintProto& ct) {
    // Build the transition table {tail, label} -> head.
    std::unordered_map<std::pair<int64, int64>, int64> transition_map;
    const int num_transitions = ct.automata().transition_tail().size();
    for (int i = 0; i < num_transitions; ++i) {
      transition_map[{ct.automata().transition_tail(i),
                      ct.automata().transition_label(i)}] =
          ct.automata().transition_head(i);
    }

    // Walk the automata.
    int64 current_state = ct.automata().starting_state();
    const int num_steps = ct.automata().vars_size();
    for (int i = 0; i < num_steps; ++i) {
      const std::pair<int64, int64> key = {current_state,
                                           Value(ct.automata().vars(i))};
      CHECK(ContainsKey(transition_map, key));
      current_state = transition_map[key];
    }

    // Check we are now in a final state.
    for (const int64 final : ct.automata().final_states()) {
      if (current_state == final) return true;
    }
    return false;
  }

  bool CircuitConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_nodes = ct.circuit().nexts_size();
    int num_inactive = 0;
    int last_active = 0;
    for (int i = 0; i < num_nodes; ++i) {
      const int value = Value(ct.circuit().nexts(i));
      if (value < 0 || value == i || value >= num_nodes) {
        ++num_inactive;
      } else {
        last_active = i;
      }
    }
    if (num_inactive == num_nodes) return true;

    std::vector<bool> visited(num_nodes, false);
    int current = last_active;
    int num_visited = 0;
    while (!visited[current]) {
      ++num_visited;
      visited[current] = true;
      current = Value(ct.circuit().nexts(current));
    }
    return num_visited + num_inactive == num_nodes;
  }

  bool RoutesConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_arcs = ct.routes().tails_size();
    int num_used_arcs = 0;
    int num_self_arcs = 0;
    int num_nodes = 0;
    std::vector<int> tail_to_head;
    std::vector<int> depot_nexts;
    for (int i = 0; i < num_arcs; ++i) {
      const int tail = ct.routes().tails(i);
      const int head = ct.routes().heads(i);
      num_nodes = std::max(num_nodes, 1 + tail);
      num_nodes = std::max(num_nodes, 1 + head);
      tail_to_head.resize(num_nodes, -1);
      if (LiteralIsTrue(ct.routes().literals(i))) {
        if (tail == head) {
          if (tail == 0) return false;
          ++num_self_arcs;
          continue;
        }
        ++num_used_arcs;
        if (tail == 0) {
          depot_nexts.push_back(head);
        } else {
          if (tail_to_head[tail] != -1) return false;
          tail_to_head[tail] = head;
        }
      }
    }

    // Make sure each routes from the depot go back to it, and count such arcs.
    int count = 0;
    for (int start : depot_nexts) {
      ++count;
      while (start != 0) {
        if (tail_to_head[start] == -1) return false;
        start = tail_to_head[start];
        ++count;
      }
    }

    if (count != num_used_arcs) {
      VLOG(1) << "count: " << count << " != num_used_arcs:" << num_used_arcs;
      return false;
    }

    // Each routes cover as many node as there is arcs, but this way we count
    // multiple times the depot. So the number of nodes covered are:
    //     count - depot_nexts.size() + 1.
    // And this number + the self arcs should be num_nodes.
    if (count - depot_nexts.size() + 1 + num_self_arcs != num_nodes) {
      VLOG(1) << "Not all nodes are covered!";
      return false;
    }

    return true;
  }

  bool CircuitCoveringConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_nodes = ct.circuit_covering().nexts_size();
    std::vector<bool> distinguished(num_nodes, false);
    std::vector<bool> visited(num_nodes, false);
    for (const int node : ct.circuit_covering().distinguished_nodes()) {
      distinguished[node] = true;
    }

    // By design, every node has exactly one neighbour.
    // Check that distinguished nodes do not share a circuit,
    // mark nodes visited during the process.
    std::vector<int> next(num_nodes, -1);
    for (const int d : ct.circuit_covering().distinguished_nodes()) {
      visited[d] = true;
      for (int node = Value(ct.circuit_covering().nexts(d)); node != d;
           node = Value(ct.circuit_covering().nexts(node))) {
        if (distinguished[node]) return false;
        CHECK(!visited[node]);
        visited[node] = true;
      }
    }

    // Check that nodes that were not visited are all loops.
    for (int node = 0; node < num_nodes; node++) {
      if (!visited[node] && Value(ct.circuit_covering().nexts(node)) != node) {
        return false;
      }
    }
    return true;
  }

  bool InverseConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_variables = ct.inverse().f_direct_size();
    if (num_variables != ct.inverse().f_inverse_size()) return false;
    // Check that f_inverse(f_direct(i)) == i; this is sufficient.
    for (int i = 0; i < num_variables; i++) {
      const int fi = Value(ct.inverse().f_direct(i));
      if (fi < 0 || num_variables <= fi) return false;
      if (i != Value(ct.inverse().f_inverse(fi))) return false;
    }
    return true;
  }

  bool ReservoirConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_variables = ct.reservoir().times_size();
    const int64 min_level = ct.reservoir().min_level();
    const int64 max_level = ct.reservoir().min_level();
    std::map<int64, int64> deltas;
    deltas[0] = 0;
    for (int i = 0; i < num_variables; i++) {
      const int64 time = Value(ct.reservoir().times(i));
      if (time < 0) {
        VLOG(1) << "reservoir times(" << i << ") is negative.";
        return false;
      }
      deltas[time] += ct.reservoir().demands(i);
    }
    int64 current_level = 0;
    for (const auto& delta : deltas) {
      current_level += delta.second;
      if (current_level < min_level || current_level > max_level) {
        VLOG(1) << "Reservoir level " << current_level
                << " is out of bounds at time" << delta.first;
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<int64> variable_values_;
};

}  // namespace

bool SolutionIsFeasible(const CpModelProto& model,
                        const std::vector<int64>& variable_values) {
  if (variable_values.size() != model.variables_size()) {
    VLOG(1) << "Wrong number of variables in the solution vector";
    return false;
  }

  // Check that all values fall in the variable domains.
  int num_optional_vars = 0;
  for (int i = 0; i < model.variables_size(); ++i) {
    if (!model.variables(i).enforcement_literal().empty()) {
      ++num_optional_vars;
    }
    if (!DomainInProtoContains(model.variables(i), variable_values[i])) {
      VLOG(1) << "Variable #" << i << " has value " << variable_values[i]
              << " which do not fall in its domain: "
              << model.variables(i).ShortDebugString();
      return false;
    }
  }

  CHECK_EQ(variable_values.size(), model.variables_size());
  ConstraintChecker checker(variable_values);

  for (int c = 0; c < model.constraints_size(); ++c) {
    const ConstraintProto& ct = model.constraints(c);

    if (!checker.ConstraintIsEnforced(ct)) continue;
    if (num_optional_vars > 0) {
      // This function can be slow because it uses reflection. So we only
      // call it if there is any optional variables.
      if (checker.ConstraintHasNonEnforcedVariables(model, ct)) continue;
    }

    bool is_feasible = true;
    const ConstraintProto::ConstraintCase type = ct.constraint_case();
    switch (type) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        is_feasible = checker.BoolOrConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        is_feasible = checker.BoolAndConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kBoolXor:
        is_feasible = checker.BoolAndConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kLinear:
        is_feasible = checker.LinearConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntProd:
        is_feasible = checker.IntProdConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntDiv:
        is_feasible = checker.IntDivConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntMin:
        is_feasible = checker.IntMinConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntMax:
        is_feasible = checker.IntMaxConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kAllDiff:
        is_feasible = checker.AllDiffConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kInterval:
        is_feasible = checker.IntervalConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap:
        is_feasible = checker.NoOverlapConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        is_feasible = checker.NoOverlap2DConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        is_feasible = checker.CumulativeConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kElement:
        is_feasible = checker.ElementConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kTable:
        is_feasible = checker.TableConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kAutomata:
        is_feasible = checker.AutomataConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kCircuit:
        is_feasible = checker.CircuitConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kRoutes:
        is_feasible = checker.RoutesConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kCircuitCovering:
        is_feasible = checker.CircuitCoveringConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        is_feasible = checker.InverseConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kReservoir:
        is_feasible = checker.ReservoirConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
        // Empty constraint is always feasible.
        break;
      default:
        LOG(FATAL) << "Unuspported constraint: " << ConstraintCaseName(type);
    }
    if (!is_feasible) {
      VLOG(1) << "Failing constraint #" << c << " : "
              << model.constraints(c).ShortDebugString();
      return false;
    }
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
