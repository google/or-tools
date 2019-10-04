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

#include "ortools/sat/cp_model_checker.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/hash.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/port/proto_utils.h"
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
    const std::string error_message = statement;      \
    if (!error_message.empty()) return error_message; \
  } while (false)

template <typename ProtoWithDomain>
bool DomainInProtoIsValid(const ProtoWithDomain& proto) {
  std::vector<ClosedInterval> domain;
  for (int i = 0; i < proto.domain_size(); i += 2) {
    domain.push_back({proto.domain(i), proto.domain(i + 1)});
  }
  return IntervalsAreSortedAndNonAdjacent(domain);
}

bool VariableReferenceIsValid(const CpModelProto& model, int reference) {
  return std::max(-reference - 1, reference) < model.variables_size();
}

bool LiteralReferenceIsValid(const CpModelProto& model, int reference) {
  if (std::max(NegatedRef(reference), reference) >= model.variables_size()) {
    return false;
  }
  const auto& var_proto = model.variables(PositiveRef(reference));
  const int64 min_domain = var_proto.domain(0);
  const int64 max_domain = var_proto.domain(var_proto.domain_size() - 1);
  return min_domain >= 0 && max_domain <= 1;
}

std::string ValidateIntegerVariable(const CpModelProto& model, int v) {
  const IntegerVariableProto& proto = model.variables(v);
  if (proto.domain_size() == 0) {
    return absl::StrCat("var #", v,
                        " has no domain(): ", ProtobufShortDebugString(proto));
  }
  if (proto.domain_size() % 2 != 0) {
    return absl::StrCat("var #", v, " has an odd domain() size: ",
                        ProtobufShortDebugString(proto));
  }
  if (!DomainInProtoIsValid(proto)) {
    return absl::StrCat("var #", v, " has and invalid domain() format: ",
                        ProtobufShortDebugString(proto));
  }

  // Internally, we often take the negation of a domain, and we also want to
  // have sentinel values greater than the min/max of a variable domain, so
  // the domain must fall in [kint64min + 2, kint64max - 1].
  const int64 lb = proto.domain(0);
  const int64 ub = proto.domain(proto.domain_size() - 1);
  if (lb < kint64min + 2 || ub > kint64max - 1) {
    return absl::StrCat(
        "var #", v, " domain do not fall in [kint64min + 2, kint64max - 1]. ",
        ProtobufShortDebugString(proto));
  }

  // We do compute ub - lb in some place in the code and do not want to deal
  // with overflow everywhere. This seems like a reasonable precondition anyway.
  if (lb < 0 && lb + kint64max < ub) {
    return absl::StrCat(
        "var #", v,
        " has a domain that is too large, i.e. |UB - LB| overflow an int64: ",
        ProtobufShortDebugString(proto));
  }

  return "";
}

std::string ValidateArgumentReferencesInConstraint(const CpModelProto& model,
                                                   int c) {
  const ConstraintProto& ct = model.constraints(c);
  IndexReferences references = GetReferencesUsedByConstraint(ct);
  for (const int v : references.variables) {
    if (!VariableReferenceIsValid(model, v)) {
      return absl::StrCat("Out of bound integer variable ", v,
                          " in constraint #", c, " : ",
                          ProtobufShortDebugString(ct));
    }
  }
  for (const int lit : ct.enforcement_literal()) {
    if (!LiteralReferenceIsValid(model, lit)) {
      return absl::StrCat("Invalid enforcement literal ", lit,
                          " in constraint #", c, " : ",
                          ProtobufShortDebugString(ct));
    }
  }
  for (const int lit : references.literals) {
    if (!LiteralReferenceIsValid(model, lit)) {
      return absl::StrCat("Invalid literal ", lit, " in constraint #", c, " : ",
                          ProtobufShortDebugString(ct));
    }
  }
  for (const int i : UsedIntervals(ct)) {
    if (i < 0 || i >= model.constraints_size()) {
      return absl::StrCat("Out of bound interval ", i, " in constraint #", c,
                          " : ", ProtobufShortDebugString(ct));
    }
    if (model.constraints(i).constraint_case() !=
        ConstraintProto::ConstraintCase::kInterval) {
      return absl::StrCat(
          "Interval ", i,
          " does not refer to an interval constraint. Problematic constraint #",
          c, " : ", ProtobufShortDebugString(ct));
    }
  }
  return "";
}

template <class LinearExpressionProto>
bool PossibleIntegerOverflow(const CpModelProto& model,
                             const LinearExpressionProto& proto) {
  int64 sum_min = 0;
  int64 sum_max = 0;
  for (int i = 0; i < proto.vars_size(); ++i) {
    const int ref = proto.vars(i);
    const auto& var_proto = model.variables(PositiveRef(ref));
    const int64 min_domain = var_proto.domain(0);
    const int64 max_domain = var_proto.domain(var_proto.domain_size() - 1);
    const int64 coeff = RefIsPositive(ref) ? proto.coeffs(i) : -proto.coeffs(i);
    const int64 prod1 = CapProd(min_domain, coeff);
    const int64 prod2 = CapProd(max_domain, coeff);

    // Note that we use min/max with zero to disallow "alternative" terms and
    // be sure that we cannot have an overflow if we do the computation in a
    // different order.
    sum_min = CapAdd(sum_min, std::min(int64{0}, std::min(prod1, prod2)));
    sum_max = CapAdd(sum_max, std::max(int64{0}, std::max(prod1, prod2)));
    for (const int64 v : {prod1, prod2, sum_min, sum_max}) {
      if (v == kint64max || v == kint64min) return true;
    }
  }
  return false;
}

std::string ValidateLinearConstraint(const CpModelProto& model,
                                     const ConstraintProto& ct) {
  const LinearConstraintProto& arg = ct.linear();
  if (PossibleIntegerOverflow(model, arg)) {
    return "Possible integer overflow in constraint: " +
           ProtobufDebugString(ct);
  }
  return "";
}

std::string ValidateReservoirConstraint(const CpModelProto& model,
                                        const ConstraintProto& ct) {
  if (ct.enforcement_literal_size() > 0) {
    return "Reservoir does not support enforcement literals.";
  }
  for (const int t : ct.reservoir().times()) {
    const IntegerVariableProto& time = model.variables(t);
    for (const int64 bound : time.domain()) {
      if (bound < 0) {
        return absl::StrCat("Time variables must be >= 0 in constraint ",
                            ProtobufShortDebugString(ct));
      }
    }
  }
  int64 sum_abs = 0;
  for (const int64 demand : ct.reservoir().demands()) {
    sum_abs = CapAdd(sum_abs, std::abs(demand));
    if (sum_abs == kint64max) {
      return "Possible integer overflow in constraint: " +
             ProtobufDebugString(ct);
    }
  }
  if (ct.reservoir().actives_size() > 0 &&
      ct.reservoir().actives_size() != ct.reservoir().times_size()) {
    return "Wrong array length of actives variables";
  }
  if (ct.reservoir().demands_size() > 0 &&
      ct.reservoir().demands_size() != ct.reservoir().times_size()) {
    return "Wrong array length of demands variables";
  }
  return "";
}

std::string ValidateCircuitCoveringConstraint(const ConstraintProto& ct) {
  const int num_nodes = ct.circuit_covering().nexts_size();
  for (const int d : ct.circuit_covering().distinguished_nodes()) {
    if (d < 0 || d >= num_nodes) {
      return absl::StrCat("Distinguished node ", d, " not in [0, ", num_nodes,
                          ").");
    }
  }
  return "";
}

std::string ValidateObjective(const CpModelProto& model,
                              const CpObjectiveProto& obj) {
  if (PossibleIntegerOverflow(model, obj)) {
    return "Possible integer overflow in objective: " +
           ProtobufDebugString(obj);
  }
  return "";
}

std::string ValidateSolutionHint(const CpModelProto& model) {
  if (!model.has_solution_hint()) return "";
  const auto& hint = model.solution_hint();
  if (hint.vars().size() != hint.values().size()) {
    return "Invalid solution hint: vars and values do not have the same size.";
  }
  for (const int ref : hint.vars()) {
    if (!VariableReferenceIsValid(model, ref)) {
      return absl::StrCat("Invalid variable reference in solution hint: ", ref);
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

    // By default, a constraint does not support enforcement literals except if
    // explicitly stated by setting this to true below.
    bool support_enforcement = false;

    // Other non-generic validations.
    // TODO(user): validate all constraints.
    const ConstraintProto& ct = model.constraints(c);
    const ConstraintProto::ConstraintCase type = ct.constraint_case();
    switch (type) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kLinear:
        support_enforcement = true;
        if (!DomainInProtoIsValid(ct.linear())) {
          return absl::StrCat("Invalid domain in constraint #", c, " : ",
                              ProtobufShortDebugString(ct));
        }
        if (ct.linear().coeffs_size() != ct.linear().vars_size()) {
          return absl::StrCat("coeffs_size() != vars_size() in constraint #", c,
                              " : ", ProtobufShortDebugString(ct));
        }
        RETURN_IF_NOT_EMPTY(ValidateLinearConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kInterval:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        if (ct.cumulative().intervals_size() !=
            ct.cumulative().demands_size()) {
          return absl::StrCat(
              "intervals_size() != demands_size() in constraint #", c, " : ",
              ProtobufShortDebugString(ct));
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

    // Because some client set fixed enforcement literal which are supported
    // in the presolve for all constraints, we just check that there is no
    // non-fixed enforcement.
    if (!support_enforcement && !ct.enforcement_literal().empty()) {
      for (const int ref : ct.enforcement_literal()) {
        const int var = PositiveRef(ref);
        const Domain domain = ReadDomainFromProto(model.variables(var));
        if (domain.Size() != 1) {
          return absl::StrCat(
              "Enforcement literal not supported in constraint: ",
              ProtobufShortDebugString(ct));
        }
      }
    }
  }
  if (model.has_objective()) {
    for (const int v : model.objective().vars()) {
      if (!VariableReferenceIsValid(model, v)) {
        return absl::StrCat("Out of bound objective variable ", v, " : ",
                            ProtobufShortDebugString(model.objective()));
      }
    }
    RETURN_IF_NOT_EMPTY(ValidateObjective(model, model.objective()));
  }

  RETURN_IF_NOT_EMPTY(ValidateSolutionHint(model));
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

  bool ConstraintIsEnforced(const ConstraintProto& ct) {
    for (const int lit : ct.enforcement_literal()) {
      if (LiteralIsFalse(lit)) return false;
    }
    return true;
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

  bool AtMostOneConstraintIsFeasible(const ConstraintProto& ct) {
    int num_true_literals = 0;
    for (const int lit : ct.at_most_one().literals()) {
      if (LiteralIsTrue(lit)) ++num_true_literals;
    }
    return num_true_literals <= 1;
  }

  bool BoolXorConstraintIsFeasible(const ConstraintProto& ct) {
    int sum = 0;
    for (const int lit : ct.bool_xor().literals()) {
      sum ^= LiteralIsTrue(lit) ? 1 : 0;
    }
    return sum == 1;
  }

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

  bool IntModConstraintIsFeasible(const ConstraintProto& ct) {
    return Value(ct.int_mod().target()) ==
           Value(ct.int_mod().vars(0)) % Value(ct.int_mod().vars(1));
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
    absl::flat_hash_set<int64> values;
    for (const int v : ct.all_diff().vars()) {
      if (gtl::ContainsKey(values, Value(v))) return false;
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

  bool IntervalIsEmpty(const IntervalConstraintProto& interval) {
    return Value(interval.start()) == Value(interval.end());
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
        if (ConstraintIsEnforced(x) && ConstraintIsEnforced(y) &&
            (!arg.boxes_with_null_area_can_overlap() ||
             (!IntervalIsEmpty(x.interval()) &&
              !IntervalIsEmpty(y.interval())))) {
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
        if (!IntervalsAreDisjoint(xi, xj) && !IntervalsAreDisjoint(yi, yj) &&
            !IntervalIsEmpty(xi) && !IntervalIsEmpty(xj) &&
            !IntervalIsEmpty(yi) && !IntervalIsEmpty(yj)) {
          VLOG(1) << "Interval " << i << "(x=[" << Value(xi.start()) << ", "
                  << Value(xi.end()) << "], y=[" << Value(yi.start()) << ", "
                  << Value(yi.end()) << "]) and " << j << "("
                  << "(x=[" << Value(xj.start()) << ", " << Value(xj.end())
                  << "], y=[" << Value(yj.start()) << ", " << Value(yj.end())
                  << "]) are not disjoint.";
          return false;
        }
      }
    }
    return true;
  }

  bool CumulativeConstraintIsFeasible(const CpModelProto& model,
                                      const ConstraintProto& ct) {
    // TODO(user,user): Improve complexity for large durations.
    const int64 capacity = Value(ct.cumulative().capacity());
    const int num_intervals = ct.cumulative().intervals_size();
    absl::flat_hash_map<int64, int64> usage;
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

  bool AutomatonConstraintIsFeasible(const ConstraintProto& ct) {
    // Build the transition table {tail, label} -> head.
    absl::flat_hash_map<std::pair<int64, int64>, int64> transition_map;
    const int num_transitions = ct.automaton().transition_tail().size();
    for (int i = 0; i < num_transitions; ++i) {
      transition_map[{ct.automaton().transition_tail(i),
                      ct.automaton().transition_label(i)}] =
          ct.automaton().transition_head(i);
    }

    // Walk the automaton.
    int64 current_state = ct.automaton().starting_state();
    const int num_steps = ct.automaton().vars_size();
    for (int i = 0; i < num_steps; ++i) {
      const std::pair<int64, int64> key = {current_state,
                                           Value(ct.automaton().vars(i))};
      if (!gtl::ContainsKey(transition_map, key)) {
        return false;
      }
      current_state = transition_map[key];
    }

    // Check we are now in a final state.
    for (const int64 final : ct.automaton().final_states()) {
      if (current_state == final) return true;
    }
    return false;
  }

  bool CircuitConstraintIsFeasible(const ConstraintProto& ct) {
    // Compute the set of relevant nodes for the constraint and set the next of
    // each of them. This also detects duplicate nexts.
    const int num_arcs = ct.circuit().tails_size();
    absl::flat_hash_set<int> nodes;
    absl::flat_hash_map<int, int> nexts;
    for (int i = 0; i < num_arcs; ++i) {
      const int tail = ct.circuit().tails(i);
      const int head = ct.circuit().heads(i);
      nodes.insert(tail);
      nodes.insert(head);
      if (LiteralIsFalse(ct.circuit().literals(i))) continue;
      if (nexts.contains(tail)) return false;  // Duplicate.
      nexts[tail] = head;
    }

    // All node must have a next.
    int in_cycle;
    int cycle_size = 0;
    for (const int node : nodes) {
      if (!nexts.contains(node)) return false;  // No next.
      if (nexts[node] == node) continue;        // skip self-loop.
      in_cycle = node;
      ++cycle_size;
    }
    if (cycle_size == 0) return true;

    // Check that we have only one cycle. visited is used to not loop forever if
    // we have a "rho" shape instead of a cycle.
    absl::flat_hash_set<int> visited;
    int current = in_cycle;
    int num_visited = 0;
    while (!visited.contains(current)) {
      ++num_visited;
      visited.insert(current);
      current = nexts[current];
    }
    if (current != in_cycle) return false;  // Rho shape.
    return num_visited == cycle_size;       // Another cycle somewhere if false.
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
    const int64 max_level = ct.reservoir().max_level();
    std::map<int64, int64> deltas;
    deltas[0] = 0;
    const bool has_active_variables = ct.reservoir().actives_size() > 0;
    for (int i = 0; i < num_variables; i++) {
      const int64 time = Value(ct.reservoir().times(i));
      if (time < 0) {
        VLOG(1) << "reservoir times(" << i << ") is negative.";
        return false;
      }
      if (!has_active_variables || Value(ct.reservoir().actives(i)) == 1) {
        deltas[time] += ct.reservoir().demands(i);
      }
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
  for (int i = 0; i < model.variables_size(); ++i) {
    if (!DomainInProtoContains(model.variables(i), variable_values[i])) {
      VLOG(1) << "Variable #" << i << " has value " << variable_values[i]
              << " which do not fall in its domain: "
              << ProtobufShortDebugString(model.variables(i));
      return false;
    }
  }

  CHECK_EQ(variable_values.size(), model.variables_size());
  ConstraintChecker checker(variable_values);

  for (int c = 0; c < model.constraints_size(); ++c) {
    const ConstraintProto& ct = model.constraints(c);

    if (!checker.ConstraintIsEnforced(ct)) continue;

    bool is_feasible = true;
    const ConstraintProto::ConstraintCase type = ct.constraint_case();
    switch (type) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        is_feasible = checker.BoolOrConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        is_feasible = checker.BoolAndConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kAtMostOne:
        is_feasible = checker.AtMostOneConstraintIsFeasible(ct);
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
      case ConstraintProto::ConstraintCase::kIntMod:
        is_feasible = checker.IntModConstraintIsFeasible(ct);
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
      case ConstraintProto::ConstraintCase::kAutomaton:
        is_feasible = checker.AutomatonConstraintIsFeasible(ct);
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
              << ProtobufShortDebugString(model.constraints(c));
      return false;
    }
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
