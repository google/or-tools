// Copyright 2010-2014 Google
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

#include <unordered_map>
#include <unordered_set>

#include "ortools/base/join.h"
#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
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
      default:
        break;
    }
  }
  for (const CpObjectiveProto& objective : model.objectives()) {
    const int v = objective.objective_var();
    if (!VariableReferenceIsValid(model, v)) {
      return StrCat("Out of bound objective variable ", v, " : ",
                    objective.ShortDebugString());
    }
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
      const IntervalConstraintProto& interval = model.constraints(i).interval();
      start_durations_pairs.push_back(
          {Value(interval.start()), Value(interval.size())});
    }
    std::sort(start_durations_pairs.begin(), start_durations_pairs.end());
    int64 previous_end = kint64min;
    for (const auto pair : start_durations_pairs) {
      if (pair.first < previous_end) return false;
      previous_end = pair.first + pair.second;
    }
    return true;
  }

  bool IntervalsAreDisjoint(const CpModelProto& model,
                            const IntervalConstraintProto& interval1,
                            const IntervalConstraintProto& interval2) {
    return Value(interval1.end()) <= Value(interval2.start()) ||
           Value(interval2.end()) <= Value(interval1.start());
  }

  bool NoOverlap2DConstraintIsFeasible(const CpModelProto& model,
                                       const ConstraintProto& ct) {
    const auto& arg = ct.no_overlap_2d();
    const int num_intervals = arg.x_intervals_size();
    for (int i = 0; i < num_intervals; ++i) {
      for (int j = i + 1; j < num_intervals; ++j) {
        if (!IntervalsAreDisjoint(
                model, model.constraints(arg.x_intervals(i)).interval(),
                model.constraints(arg.x_intervals(j)).interval()) &&
            !IntervalsAreDisjoint(
                model, model.constraints(arg.y_intervals(i)).interval(),
                model.constraints(arg.y_intervals(j)).interval())) {
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
      const IntervalConstraintProto& interval =
          model.constraints(ct.cumulative().intervals(i)).interval();
      const int64 start = Value(interval.start());
      const int64 duration = Value(interval.size());
      const int64 demand = Value(ct.cumulative().demands(i));
      for (int64 t = start; t < start + duration; ++t) {
        usage[t] += demand;
        if (usage[t] > capacity) return false;
      }
    }
    return true;
  }

  bool ElementConstraintIsFeasible(const CpModelProto& model,
                                   const ConstraintProto& ct) {
    const int index = Value(ct.element().index());
    return Value(ct.element().vars(index)) == Value(ct.element().target());
  }

  bool TableConstraintIsFeasible(const CpModelProto& model,
                                 const ConstraintProto& ct) {
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

  bool AutomataConstraintIsFeasible(const CpModelProto& model,
                                    const ConstraintProto& ct) {
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

  bool CircuitConstraintIsFeasible(const CpModelProto& model,
                                   const ConstraintProto& ct) {
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

 private:
  std::vector<int64> variable_values_;
};

}  // namespace

bool SolutionIsFeasible(const CpModelProto& model,
                        const std::vector<int64>& variable_values) {
  // Check that all values fall in the variable domains.
  for (int i = 0; i < model.variables_size(); ++i) {
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

    // We skip optional constraints that are not present.
    if (ct.enforcement_literal_size() > 0 &&
        checker.LiteralIsFalse(ct.enforcement_literal(0))) {
      continue;
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
        is_feasible = checker.ElementConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kTable:
        is_feasible = checker.TableConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kAutomata:
        is_feasible = checker.AutomataConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kCircuit:
        is_feasible = checker.CircuitConstraintIsFeasible(model, ct);
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
