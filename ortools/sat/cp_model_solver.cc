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

#include "ortools/sat/cp_model_solver.h"

#include <functional>

#include "ortools/base/timer.h"
#include "ortools/base/join.h"
#include "ortools/base/join.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/connectivity.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/table.h"

namespace operations_research {
namespace sat {

namespace {

// =============================================================================
// Helper classes.
// =============================================================================

// List all the CpModelProto references used.
struct VariableUsage {
  const std::vector<int> integers;
  const std::vector<int> intervals;
  const std::vector<int> booleans;
};

VariableUsage ComputeVariableUsage(const CpModelProto& model_proto) {
  // Since an interval is a constraint by itself, this will just list all
  // the interval constraint in order.
  std::vector<int> used_intervals;

  // TODO(user): use std::vector<bool> instead of unordered_set<int> + sort if
  // efficiency become an issue. Note that we need these to be sorted.
  IndexReferences references;
  for (int c = 0; c < model_proto.constraints_size(); ++c) {
    const ConstraintProto& ct = model_proto.constraints(c);
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kInterval) {
      used_intervals.push_back(c);
    }
    if (HasEnforcementLiteral(ct)) {
      references.literals.insert(ct.enforcement_literal(0));
    }
    AddReferencesUsedByConstraint(ct, &references);
  }

  // Add the objectives and search heuristics variables that needs to be
  // referenceable as integer even if they are only used as Booleans.
  for (const CpObjectiveProto& objective : model_proto.objectives()) {
    references.variables.insert(objective.objective_var());
  }
  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    for (const int var : strategy.variables()) {
      references.variables.insert(var);
    }
  }

  std::vector<int> used_integers;
  for (const int var : references.variables) {
    used_integers.push_back(PositiveRef(var));
  }
  STLSortAndRemoveDuplicates(&used_integers);

  std::vector<int> used_booleans;
  for (const int lit : references.literals) {
    used_booleans.push_back(PositiveRef(lit));
  }
  STLSortAndRemoveDuplicates(&used_booleans);

  return VariableUsage{used_integers, used_intervals, used_booleans};
}

// Holds the sat::model and the mapping between the proto indices and the
// sat::model ones.
class ModelWithMapping {
 public:
  ModelWithMapping(const CpModelProto& model_proto, const VariableUsage& usage,
                   Model* model);

  // Shortcuts for the underlying model_ functions.
  template <typename T>
  T Add(std::function<T(Model*)> f) {
    return f(model_);
  }
  template <typename T>
  T Get(std::function<T(const Model&)> f) const {
    return f(*model_);
  }
  template <typename T>
  T* GetOrCreate() {
    return model_->GetOrCreate<T>();
  }
  template <typename T>
  void TakeOwnership(T* t) {
    return model_->TakeOwnership<T>(t);
  }

  bool IsInteger(int i) const {
    CHECK_LT(PositiveRef(i), integers_.size());
    return integers_[PositiveRef(i)] != kNoIntegerVariable;
  }

  IntegerVariable Integer(int i) const {
    CHECK_LT(PositiveRef(i), integers_.size());
    const IntegerVariable var = integers_[PositiveRef(i)];
    CHECK_NE(var, kNoIntegerVariable);
    return RefIsPositive(i) ? var : NegationOf(var);
  }

  BooleanVariable Boolean(int i) const {
    CHECK_GE(i, 0);
    CHECK_LT(i, booleans_.size());
    CHECK_NE(booleans_[i], kNoBooleanVariable);
    return booleans_[i];
  }

  IntervalVariable Interval(int i) const {
    CHECK_GE(i, 0);
    CHECK_LT(i, intervals_.size());
    CHECK_NE(intervals_[i], kNoIntervalVariable);
    return intervals_[i];
  }

  sat::Literal Literal(int i) const {
    CHECK_LT(PositiveRef(i), integers_.size());
    return sat::Literal(booleans_[PositiveRef(i)], RefIsPositive(i));
  }

  template <typename List>
  std::vector<IntegerVariable> Integers(const List& list) const {
    std::vector<IntegerVariable> result;
    for (const auto i : list) result.push_back(Integer(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<sat::Literal> Literals(const ProtoIndices& indices) const {
    std::vector<sat::Literal> result;
    for (const int i : indices) result.push_back(ModelWithMapping::Literal(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<IntervalVariable> Intervals(const ProtoIndices& indices) const {
    std::vector<IntervalVariable> result;
    for (const int i : indices) result.push_back(Interval(i));
    return result;
  }

  const IntervalsRepository& GetIntervalsRepository() const {
    const IntervalsRepository* repository = model_->Get<IntervalsRepository>();
    return *repository;
  }

  std::vector<int64> ExtractFullAssignment() const {
    std::vector<int64> result;
    const int num_variables = integers_.size();
    for (int i = 0; i < num_variables; ++i) {
      if (integers_[i] != kNoIntegerVariable) {
        if (model_->Get(LowerBound(integers_[i])) !=
            model_->Get(UpperBound(integers_[i]))) {
          // Notify that everything is not fixed.
          result.clear();
          return {};
        }
        if (model_->GetOrCreate<IntegerTrail>()->IsCurrentlyIgnored(
                integers_[i])) {
          // This variable is "ignored" so it may not be fixed, simply use
          // the current lower bound. Any value in its domain should lead to
          // a feasible solution.
          result.push_back(model_->Get(LowerBound(integers_[i])));
        } else {
          result.push_back(model_->Get(Value(integers_[i])));
        }
      } else if (booleans_[i] != kNoBooleanVariable) {
        result.push_back(model_->Get(Value(booleans_[i])));
      } else {
        // This variable is not used anywhere, fix it to its lower_bound.
        // TODO(user): maybe it is better to fix it to its lowest possible
        // magnitude.
        result.push_back(lower_bounds_[i]);
      }
    }
    return result;
  }

 private:
  Model* model_;

  // Note that only the variables used by at leat one constraint will be
  // created, the other will have a kNo[Integer,Interval,Boolean]VariableValue.
  std::vector<IntegerVariable> integers_;
  std::vector<IntervalVariable> intervals_;
  std::vector<BooleanVariable> booleans_;

  // Used to return a feasible solution for the unused variables.
  std::vector<int64> lower_bounds_;
};

template <typename Values>
std::vector<int64> ValuesFromProto(const Values& values) {
  return std::vector<int64>(values.begin(), values.end());
}

// Extracts all the used variables in the CpModelProto and creates a sat::Model
// representation for them.
ModelWithMapping::ModelWithMapping(const CpModelProto& model_proto,
                                   const VariableUsage& usage, Model* sat_model)
    : model_(sat_model) {
  integers_.resize(model_proto.variables_size(), kNoIntegerVariable);
  booleans_.resize(model_proto.variables_size(), kNoBooleanVariable);
  intervals_.resize(model_proto.constraints_size(), kNoIntervalVariable);
  lower_bounds_.resize(model_proto.variables_size(), 0);

  // Fills lower_bounds_, this is only used in ExtractFullAssignment().
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    lower_bounds_[i] = model_proto.variables(i).domain(0);
  }

  // TODO(user): Detect integers that are the negation of other variable. This
  // cannot be simplified by the presolve in the current proto format.
  std::vector<ClosedInterval> domain;
  std::vector<bool> domain_is_boolean;
  for (const int i : usage.integers) {
    const auto& var_proto = model_proto.variables(i);
    integers_[i] = Add(NewIntegerVariable(ReadDomain(var_proto)));
  }

  for (const int i : usage.intervals) {
    const ConstraintProto& ct = model_proto.constraints(i);
    CHECK(!HasEnforcementLiteral(ct)) << "Optional interval not yet supported.";
    intervals_[i] = Add(NewInterval(Integer(ct.interval().start()),
                                    Integer(ct.interval().end()),
                                    Integer(ct.interval().size())));
  }

  for (const int i : usage.booleans) {
    booleans_[i] = Add(NewBooleanVariable());

    // We need to fix the Boolean if the domain of the integer variable do not
    // contain 0 or contains only zero! Note that this case should not appear
    // once the model is presolved.
    std::vector<int64> domain =
        ValuesFromProto(model_proto.variables(i).domain());
    if (domain[0] == 0 && domain[1] == 0) {
      // Fix to false.
      Add(ClauseConstraint({sat::Literal(booleans_[i], false)}));
    } else if (!DomainInProtoContains(model_proto.variables(i), 0)) {
      // Fix to true.
      Add(ClauseConstraint({sat::Literal(booleans_[i], true)}));
    } else if (integers_[i] != kNoIntegerVariable) {
      Add(ReifiedInInterval(integers_[i], 0, 0,
                            sat::Literal(booleans_[i], false)));
    }
  }
}

// =============================================================================
// Constraint loading functions.
// =============================================================================

void LoadBoolOrConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  std::vector<Literal> literals = m->Literals(ct.bool_or().literals());
  if (HasEnforcementLiteral(ct)) {
    literals.push_back(m->Literal(ct.enforcement_literal(0)).Negated());
  }
  m->Add(ClauseConstraint(literals));
}

void LoadBoolAndConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<Literal> literals = m->Literals(ct.bool_and().literals());
  if (HasEnforcementLiteral(ct)) {
    const Literal is_true = m->Literal(ct.enforcement_literal(0));
    for (const Literal lit : literals) m->Add(Implication(is_true, lit));
  } else {
    for (const Literal lit : literals) m->Add(ClauseConstraint({lit}));
  }
}

void LoadBoolXorConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  CHECK(!HasEnforcementLiteral(ct)) << "Not supported.";
  m->Add(LiteralXorIs(m->Literals(ct.bool_xor().literals()), true));
}

void LoadLinearConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.linear().vars());
  const std::vector<int64> coeffs = ValuesFromProto(ct.linear().coeffs());
  if (ct.linear().domain_size() == 2) {
    const int64 lb = ct.linear().domain(0);
    const int64 ub = ct.linear().domain(1);
    if (!HasEnforcementLiteral(ct)) {
      if (lb != kint64min) m->Add(WeightedSumGreaterOrEqual(vars, coeffs, lb));
      if (ub != kint64max) m->Add(WeightedSumLowerOrEqual(vars, coeffs, ub));
    } else {
      const Literal is_true = m->Literal(ct.enforcement_literal(0));
      if (lb != kint64min) {
        m->Add(ConditionalWeightedSumGreaterOrEqual(is_true, vars, coeffs, lb));
      }
      if (ub != kint64max) {
        m->Add(ConditionalWeightedSumLowerOrEqual(is_true, vars, coeffs, ub));
      }
    }
  } else {
    std::vector<Literal> clause;
    for (int i = 0; i < ct.linear().domain_size(); i += 2) {
      const int64 lb = ct.linear().domain(i);
      const int64 ub = ct.linear().domain(i + 1);
      const Literal literal(m->Add(NewBooleanVariable()), true);
      clause.push_back(literal);
      if (lb != kint64min) {
        m->Add(ConditionalWeightedSumGreaterOrEqual(literal, vars, coeffs, lb));
      }
      if (ub != kint64max) {
        m->Add(ConditionalWeightedSumLowerOrEqual(literal, vars, coeffs, ub));
      }
    }
    if (HasEnforcementLiteral(ct)) {
      clause.push_back(m->Literal(ct.enforcement_literal(0)).Negated());
    }

    // TODO(user): In the cases where this clause only contains two literals,
    // then we could have only used one literal and its negation above.
    m->Add(ClauseConstraint(clause));
  }
}

void LoadAllDiffConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.all_diff().vars());
  // TODO(user): Find out which alldifferent to use depending on model.
  // If some domain is too large, use bounds reasoning.
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  int64 max_domain_size = 0;
  for (const IntegerVariable var : vars) {
    IntegerValue lb = integer_trail->LowerBound(var);
    IntegerValue ub = integer_trail->UpperBound(var);
    int64 domain_size = ub.value() - lb.value();
    max_domain_size = std::max(max_domain_size, domain_size);
  }

  if (max_domain_size < 1024) {
    m->Add(AllDifferentBinary(vars));
    m->Add(AllDifferentAC(vars));
  } else {
    m->Add(AllDifferentOnBounds(vars));
  }
}

void LoadIntProdConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable prod = m->Integer(ct.int_prod().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_prod().vars());
  CHECK_EQ(vars.size(), 2) << "General int_prod not supported yet.";
  m->Add(ProductConstraint(vars[0], vars[1], prod));
}

void LoadIntDivConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable div = m->Integer(ct.int_div().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_div().vars());
  m->Add(DivisionConstraint(vars[0], vars[1], div));
}

void LoadIntMinConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable min = m->Integer(ct.int_min().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_min().vars());
  m->Add(IsEqualToMinOf(min, vars));
}

void LoadIntMaxConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable max = m->Integer(ct.int_max().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.int_max().vars());
  m->Add(IsEqualToMaxOf(max, vars));
}

void LoadNoOverlapConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  m->Add(Disjunctive(m->Intervals(ct.no_overlap().intervals())));
}

void LoadNoOverlap2dConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntervalVariable> x_intervals =
      m->Intervals(ct.no_overlap_2d().x_intervals());
  const std::vector<IntervalVariable> y_intervals =
      m->Intervals(ct.no_overlap_2d().y_intervals());

  const IntervalsRepository& repository = m->GetIntervalsRepository();
  std::vector<IntegerVariable> x;
  std::vector<IntegerVariable> y;
  std::vector<IntegerVariable> dx;
  std::vector<IntegerVariable> dy;
  for (int i = 0; i < x_intervals.size(); ++i) {
    x.push_back(repository.StartVar(x_intervals[i]));
    y.push_back(repository.StartVar(y_intervals[i]));
    dx.push_back(repository.SizeVar(x_intervals[i]));
    dy.push_back(repository.SizeVar(y_intervals[i]));
  }
  m->Add(StrictNonOverlappingRectangles(x, y, dx, dy));
}

void LoadCumulativeConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntervalVariable> intervals =
      m->Intervals(ct.cumulative().intervals());
  const IntegerVariable capacity = m->Integer(ct.cumulative().capacity());
  const std::vector<IntegerVariable> demands =
      m->Integers(ct.cumulative().demands());
  m->Add(Cumulative(intervals, demands, capacity));
}

// TODO(user): Be more efficient when the element().vars() are constants.
// Ideally we should avoid creating them as integer variable...
void LoadElementConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const IntegerVariable index = m->Integer(ct.element().index());
  const IntegerVariable target = m->Integer(ct.element().target());
  const std::vector<IntegerVariable> vars = m->Integers(ct.element().vars());

  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  if (integer_trail->LowerBound(index) == integer_trail->UpperBound(index)) {
    const int64 value = integer_trail->LowerBound(index).value();
    m->Add(Equality(target, vars[value]));
    return;
  }

  // We always fully encode the index on an element constraint.
  const auto encoding = m->Add(FullyEncodeVariable((index)));
  std::vector<Literal> selectors;
  std::vector<IntegerVariable> possible_vars;
  for (const auto literal_value : encoding) {
    const int i = literal_value.value.value();
    CHECK_GE(i, 0) << "Should be presolved.";
    CHECK_LT(i, vars.size()) << "Should be presolved.";
    possible_vars.push_back(vars[i]);
    selectors.push_back(literal_value.literal);
    const Literal r = literal_value.literal;

    // TODO(user): Be more efficient if one of the two is a constant. Or handle
    // that directly in the model function.
    if (vars[i] == target) continue;
    m->Add(ConditionalLowerOrEqualWithOffset(vars[i], target, 0, r));
    m->Add(ConditionalLowerOrEqualWithOffset(target, vars[i], 0, r));
  }
  m->Add(PartialIsOneOfVar(target, possible_vars, selectors));
}

void LoadTableConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.table().vars());
  const std::vector<int64> values = ValuesFromProto(ct.table().values());
  const int num_vars = vars.size();
  const int num_tuples = values.size() / num_vars;
  std::vector<std::vector<int64>> tuples(num_tuples);
  int count = 0;
  for (int i = 0; i < num_tuples; ++i) {
    for (int j = 0; j < num_vars; ++j) {
      tuples[i].push_back(values[count++]);
    }
  }
  if (ct.table().negated()) {
    m->Add(NegatedTableConstraintWithoutFullEncoding(vars, tuples));
  } else {
    m->Add(TableConstraint(vars, tuples));
  }
}

void LoadAutomataConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const std::vector<IntegerVariable> vars = m->Integers(ct.automata().vars());

  const int num_transitions = ct.automata().transition_tail_size();
  std::vector<std::vector<int64>> transitions;
  for (int i = 0; i < num_transitions; ++i) {
    transitions.push_back({ct.automata().transition_tail(i),
                           ct.automata().transition_label(i),
                           ct.automata().transition_head(i)});
  }

  const int64 starting_state = ct.automata().starting_state();
  const std::vector<int64> final_states =
      ValuesFromProto(ct.automata().final_states());
  m->Add(TransitionConstraint(vars, transitions, starting_state, final_states));
}

void LoadCircuitConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  const int num_nodes = ct.circuit().nexts_size();
  const std::vector<IntegerVariable> nexts = m->Integers(ct.circuit().nexts());
  std::vector<std::vector<LiteralIndex>> graph(
      num_nodes, std::vector<LiteralIndex>(num_nodes, kFalseLiteralIndex));
  for (int i = 0; i < num_nodes; ++i) {
    if (m->Get(IsFixed(nexts[i]))) {
      // This is just an optimization. Note that if nexts[i] is not used in
      // other places, we didn't even need to create this constant variable in
      // the IntegerTrail...
      graph[i][m->Get(Value(nexts[i]))] = kTrueLiteralIndex;
      continue;
    } else {
      const auto encoding = m->Add(FullyEncodeVariable((nexts[i])));
      for (const auto& entry : encoding) {
        graph[i][entry.value.value()] = entry.literal.Index();
      }
    }
  }
  m->Add(SubcircuitConstraint(graph));
}

// Makes the std::string fit in one line by cutting it in the middle if necessary.
std::string Summarize(const std::string& input) {
  if (input.size() < 105) return input;
  const int half = 50;
  return StrCat(input.substr(0, half), " ... ",
                input.substr(input.size() - half, half));
}

}  // namespace.

// =============================================================================
// Public API.
// =============================================================================

std::string CpModelStats(const CpModelProto& model_proto) {
  std::map<ConstraintProto::ConstraintCase, int> num_constraints_by_type;
  std::map<ConstraintProto::ConstraintCase, int> num_reif_constraints_by_type;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    num_constraints_by_type[ct.constraint_case()]++;
    if (!ct.enforcement_literal().empty()) {
      num_reif_constraints_by_type[ct.constraint_case()]++;
    }
  }
  const VariableUsage usage = ComputeVariableUsage(model_proto);

  int num_constants = 0;
  std::set<int64> constant_values;
  std::map<std::vector<ClosedInterval>, int> num_vars_per_domains;
  for (const IntegerVariableProto& var : model_proto.variables()) {
    if (var.domain_size() == 2 && var.domain(0) == var.domain(1)) {
      ++num_constants;
      constant_values.insert(var.domain(0));
    } else {
      num_vars_per_domains[ReadDomain(var)]++;
    }
  }

  std::string result;
  StrAppend(&result, "Model '", model_proto.name(), "':\n");

  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    StrAppend(&result, "Search strategy: on ", strategy.variables_size(),
                    " variables, ",
                    DecisionStrategyProto::VariableSelectionStrategy_Name(
                        strategy.variable_selection_strategy()),
                    ", ",
                    DecisionStrategyProto::DomainReductionStrategy_Name(
                        strategy.domain_reduction_strategy()),
                    "\n");
  }

  StrAppend(&result, "#Variables: ", model_proto.variables_size(), "\n");
  if (num_vars_per_domains.size() < 20) {
    for (const auto& entry : num_vars_per_domains) {
      const std::string temp = StrCat(" - ", entry.second, " in ",
                                       IntervalsAsString(entry.first), "\n");
      StrAppend(&result, Summarize(temp));
    }
  } else {
    size_t max_complexity = 0;
    int64 min = kint64max;
    int64 max = kint64min;
    for (const auto& entry : num_vars_per_domains) {
      min = std::min(min, entry.first.front().start);
      max = std::max(max, entry.first.back().end);
      max_complexity = std::max(max_complexity, entry.first.size());
    }
    StrAppend(&result, " - ", num_vars_per_domains.size(),
                    " different domains in [", min, ",", max,
                    "] with a largest complexity of ", max_complexity, ".\n");
  }

  if (num_constants > 0) {
    const std::string temp =
        StrCat(" - ", num_constants, " constants in {",
                     strings::Join(constant_values, ","), "} \n");
    StrAppend(&result, Summarize(temp));
  }

  StrAppend(&result, "#Booleans: ", usage.booleans.size(), "\n");
  StrAppend(&result, "#Integers: ", usage.integers.size(), "\n");

  std::vector<std::string> constraints;
  for (const auto entry : num_constraints_by_type) {
    constraints.push_back(
        StrCat("#", ConstraintCaseName(entry.first), ": ", entry.second,
                     " (", num_reif_constraints_by_type[entry.first],
                     " with enforcement literal)"));
  }
  std::sort(constraints.begin(), constraints.end());
  StrAppend(&result, strings::Join(constraints, "\n"));

  return result;
}

std::string CpSolverResponseStats(const CpSolverResponse& response) {
  std::string result;
  StrAppend(&result, "CpSolverResponse:");
  StrAppend(&result,
                  "\nstatus: ", CpSolverStatus_Name(response.status()));

  // We special case the pure-decision problem for clarity.
  //
  // TODO(user): This test is not ideal for the corner case where the status is
  // still UNKNOWN yet we already know that if there is a solution, then its
  // objective is zero...
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.objective_value() == 0 && response.best_objective_bound() == 0) {
    StrAppend(&result, "\nobjective: NA");
    StrAppend(&result, "\nbest_bound: NA");
  } else {
    StrAppend(&result, "\nobjective: ", response.objective_value());
    StrAppend(&result, "\nbest_bound: ", response.best_objective_bound());
  }

  StrAppend(&result, "\nbooleans: ", response.num_booleans());
  StrAppend(&result, "\nconflicts: ", response.num_conflicts());
  StrAppend(&result, "\nbranches: ", response.num_branches());

  // TODO(user): This is probably better named "binary_propagation", but we just
  // output "propagations" to be consistent with sat/analyze.sh.
  StrAppend(&result,
                  "\npropagations: ", response.num_binary_propagations());
  StrAppend(
      &result, "\ninteger_propagations: ", response.num_integer_propagations());
  StrAppend(&result, "\nwalltime: ", response.wall_time());
  StrAppend(&result, "\nusertime: ", response.user_time());
  StrAppend(&result,
                  "\ndeterministic_time: ", response.deterministic_time());
  StrAppend(&result, "\n");
  return result;
}

namespace {

double ScaleObjectiveValue(const CpObjectiveProto& proto, int64 value) {
  double result = value + proto.offset();
  if (proto.scaling_factor() == 0) return result;
  return proto.scaling_factor() * result;
}

bool LoadConstraint(const ConstraintProto& ct, ModelWithMapping* m) {
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      return true;
    case ConstraintProto::ConstraintCase::kBoolOr:
      LoadBoolOrConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      LoadBoolAndConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintCase::kBoolXor:
      LoadBoolXorConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kLinear:
      LoadLinearConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kAllDiff:
      LoadAllDiffConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntProd:
      LoadIntProdConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntDiv:
      LoadIntDivConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntMin:
      LoadIntMinConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kIntMax:
      LoadIntMaxConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kInterval:
      // Already dealt with.
      return true;
    case ConstraintProto::ConstraintProto::kNoOverlap:
      LoadNoOverlapConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kNoOverlap2D:
      LoadNoOverlap2dConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kCumulative:
      LoadCumulativeConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kElement:
      LoadElementConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kTable:
      LoadTableConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kAutomata:
      LoadAutomataConstraint(ct, m);
      return true;
    case ConstraintProto::ConstraintProto::kCircuit:
      LoadCircuitConstraint(ct, m);
      return true;
    default:
      return false;
  }
}

// TODO(user): In full generality, we could encode all the constraint as an LP.
void LoadConstraintInGlobalLp(const ConstraintProto& ct, ModelWithMapping* m,
                              LinearProgrammingConstraint* lp) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (HasEnforcementLiteral(ct)) return;
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
    // TODO(user): Support this when the LinearProgrammingConstraint support
    // SetCoefficient() with literals.
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMax) {
    const int target = ct.int_max().target();
    for (const int var : ct.int_max().vars()) {
      // This deal with the corner case X = std::max(X, Y, Z, ..) !
      // Note that this can be presolved into X >= Y, X >= Z, ...
      if (target == var) continue;
      const auto lp_constraint = lp->CreateNewConstraint(-kInfinity, 0.0);
      lp->SetCoefficient(lp_constraint, m->Integer(var), 1.0);
      lp->SetCoefficient(lp_constraint, m->Integer(target), -1.0);
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMin) {
    const int target = ct.int_min().target();
    for (const int var : ct.int_min().vars()) {
      if (target == var) continue;
      const auto lp_constraint = lp->CreateNewConstraint(-kInfinity, 0.0);
      lp->SetCoefficient(lp_constraint, m->Integer(target), 1.0);
      lp->SetCoefficient(lp_constraint, m->Integer(var), -1.0);
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
    // Note that we ignore the holes in the domain...
    const int64 min = ct.linear().domain(0);
    const int64 max = ct.linear().domain(ct.linear().domain_size() - 1);
    if (min == kint64min && max == kint64max) return;

    // This is needed in case of duplicate variables in the linear constraint.
    std::unordered_map<IntegerVariable, double> terms;
    for (int i = 0; i < ct.linear().vars_size(); i++) {
      terms[m->Integer(ct.linear().vars(i))] += ct.linear().coeffs(i);
    }

    const double lb = (min == kint64min) ? -kInfinity : min;
    const double ub = (max == kint64max) ? kInfinity : max;
    const auto lp_constraint = lp->CreateNewConstraint(lb, ub);
    for (const auto entry : terms) {
      lp->SetCoefficient(lp_constraint, entry.first, entry.second);
    }
  }
}

void FillSolutionInResponse(const CpModelProto& model_proto,
                            const ModelWithMapping& m,
                            CpSolverResponse* response) {
  const std::vector<int64> solution = m.ExtractFullAssignment();
  if (!solution.empty()) {
    CHECK(SolutionIsFeasible(model_proto, solution));
    response->clear_solution();
    for (const int64 value : solution) response->add_solution(value);
  } else {
    // Not all variables are fixed.
    // We fill instead the lb/ub of each variables.
    response->clear_solution_lower_bounds();
    response->clear_solution_upper_bounds();
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      if (m.IsInteger(i)) {
        response->add_solution_lower_bounds(m.Get(LowerBound(m.Integer(i))));
        response->add_solution_upper_bounds(m.Get(UpperBound(m.Integer(i))));
      } else {
        const int value = m.Get(Value(m.Boolean(i)));
        response->add_solution_lower_bounds(value);
        response->add_solution_upper_bounds(value);
      }
    }
  }
}

// Adds one LinearProgrammingConstraint per connected component of the model.
void AddLPConstraints(const CpModelProto& model_proto, ModelWithMapping* m) {
  const int num_constraints = model_proto.constraints().size();
  const int num_variables = model_proto.variables().size();

  // The bipartite graph of LP constraints might be disconnected:
  // make a partition of the variables into connected components.
  // Constraint nodes are indexed by [0..num_constraints),
  // variable nodes by [num_constraints..num_constraints+num_variables).
  // TODO(user): look into biconnected components.
  ConnectedComponents<int, int> components;
  components.Init(num_constraints + num_variables);
  std::vector<bool> constraint_has_lp_representation(num_constraints);
  auto get_var_index = [num_constraints](int proto_var_index) {
    return num_constraints + PositiveRef(proto_var_index);
  };

  for (int i = 0; i < num_constraints; i++) {
    const auto& ct = model_proto.constraints(i);
    // Skip reified constraints.
    if (HasEnforcementLiteral(ct)) continue;

    constraint_has_lp_representation[i] = true;
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMax) {
      components.AddArc(i, get_var_index(ct.int_max().target()));
      for (const int var : ct.int_max().vars()) {
        components.AddArc(i, get_var_index(var));
      }
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kIntMin) {
      components.AddArc(i, get_var_index(ct.int_min().target()));
      for (const int var : ct.int_min().vars()) {
        components.AddArc(i, get_var_index(var));
      }
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kLinear) {
      for (const int var : ct.linear().vars()) {
        components.AddArc(i, get_var_index(var));
      }
    } else {
      constraint_has_lp_representation[i] = false;
    }
  }

  std::unordered_map<int, int> components_to_size;
  for (int i = 0; i < num_constraints; i++) {
    if (constraint_has_lp_representation[i]) {
      const int id = components.GetClassRepresentative(i);
      components_to_size[id] += 1;
    }
  }

  // Dispatch every constraint to its LinearProgrammingConstraint.
  std::unordered_map<int, LinearProgrammingConstraint*> representative_to_lp_constraint;
  std::vector<LinearProgrammingConstraint*> lp_constraints;
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  for (int i = 0; i < num_constraints; i++) {
    if (constraint_has_lp_representation[i]) {
      const auto& ct = model_proto.constraints(i);
      const int id = components.GetClassRepresentative(i);
      if (components_to_size[id] <= 1) continue;
      if (!ContainsKey(representative_to_lp_constraint, id)) {
        auto* lp = new LinearProgrammingConstraint(integer_trail);
        representative_to_lp_constraint[id] = lp;
        lp_constraints.push_back(lp);
      }
      LoadConstraintInGlobalLp(ct, m, representative_to_lp_constraint[id]);
    }
  }

  // Add the objective.
  if (model_proto.objectives_size() != 0) {
    const int var = model_proto.objectives(0).objective_var();
    const int id = components.GetClassRepresentative(get_var_index(var));
    if (ContainsKey(representative_to_lp_constraint, id)) {
      representative_to_lp_constraint[id]->SetObjective(m->Integer(var), true);
    }
  }

  // Register LP constraints and transfer their ownership to the CP model.
  for (auto* lp_constraint : lp_constraints) {
    m->TakeOwnership(lp_constraint);
    lp_constraint->RegisterWith(m->GetOrCreate<GenericLiteralWatcher>());
  }

  VLOG_IF(1, !lp_constraints.empty())
      << "Added " << lp_constraints.size() << " LP constraints.";
}

// The function responsible for implementing the choosen search strategy.
//
// TODO(user): expose and unit-test, it seems easy to get the order wrong, and
// that would not change the correctness.
struct Strategy {
  std::vector<IntegerVariable> variables;
  DecisionStrategyProto::VariableSelectionStrategy var_strategy;
  DecisionStrategyProto::DomainReductionStrategy domain_strategy;
};
const std::function<LiteralIndex()> ConstructSearchStrategy(
    const std::unordered_map<int, std::pair<int64, int64>>&
        var_to_coeff_offset_pair,
    const std::vector<Strategy>& strategies, Model* model) {
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();

  // Note that we copy strategies to keep the return function validity
  // independently of the life of the passed vector.
  return [integer_encoder, integer_trail, strategies,
          var_to_coeff_offset_pair]() {
    for (const Strategy& strategy : strategies) {
      IntegerVariable candidate = kNoIntegerVariable;
      IntegerValue candidate_value = kMaxIntegerValue;
      IntegerValue candidate_lb;
      IntegerValue candidate_ub;

      // TODO(user): Improve the complexity if this becomes an issue which
      // may be the case if we do a fixed_search.
      for (const IntegerVariable var : strategy.variables) {
        if (integer_trail->IsCurrentlyIgnored(var)) continue;
        const IntegerValue lb = integer_trail->LowerBound(var);
        const IntegerValue ub = integer_trail->UpperBound(var);
        if (lb == ub) continue;
        IntegerValue value(0);
        IntegerValue coeff(1);
        IntegerValue offset(0);
        if (ContainsKey(var_to_coeff_offset_pair, var.value())) {
          const auto coeff_offset =
              FindOrDie(var_to_coeff_offset_pair, var.value());
          coeff = coeff_offset.first;
          offset = coeff_offset.second;
        }
        DCHECK_GT(coeff, 0);
        switch (strategy.var_strategy) {
          case DecisionStrategyProto::CHOOSE_FIRST:
            break;
          case DecisionStrategyProto::CHOOSE_LOWEST_MIN:
            value = coeff * lb + offset;
            break;
          case DecisionStrategyProto::CHOOSE_HIGHEST_MAX:
            value = -(coeff * ub + offset);
            break;
          case DecisionStrategyProto::CHOOSE_MIN_DOMAIN_SIZE:
            value = coeff * (ub - lb);
            break;
          case DecisionStrategyProto::CHOOSE_MAX_DOMAIN_SIZE:
            value = -coeff * (ub - lb);
            break;
          default:
            LOG(FATAL) << "Unknown VariableSelectionStrategy "
                       << strategy.var_strategy;
        }
        if (value < candidate_value) {
          candidate = var;
          candidate_lb = lb;
          candidate_ub = ub;
          candidate_value = value;
        }
        if (strategy.var_strategy == DecisionStrategyProto::CHOOSE_FIRST) break;
      }
      if (candidate == kNoIntegerVariable) continue;

      IntegerLiteral literal;
      switch (strategy.domain_strategy) {
        case DecisionStrategyProto::SELECT_MIN_VALUE:
          literal = IntegerLiteral::LowerOrEqual(candidate, candidate_lb);
          break;
        case DecisionStrategyProto::SELECT_MAX_VALUE:
          literal = IntegerLiteral::GreaterOrEqual(candidate, candidate_ub);
          break;
        case DecisionStrategyProto::SELECT_LOWER_HALF:
          literal = IntegerLiteral::LowerOrEqual(
              candidate, candidate_lb + (candidate_ub - candidate_lb) / 2);
          break;
        case DecisionStrategyProto::SELECT_UPPER_HALF:
          literal = IntegerLiteral::GreaterOrEqual(
              candidate, candidate_ub - (candidate_ub - candidate_lb) / 2);
          break;
        default:
          LOG(FATAL) << "Unknown DomainReductionStrategy "
                     << strategy.domain_strategy;
      }
      return integer_encoder->GetOrCreateAssociatedLiteral(literal).Index();
    }
    return kNoLiteralIndex;
  };
}

void ExtractLinearObjective(const CpModelProto& model_proto,
                            ModelWithMapping* m,
                            std::vector<IntegerVariable>* linear_vars,
                            std::vector<IntegerValue>* linear_coeffs) {
  CHECK(!model_proto.objectives().empty());
  const CpObjectiveProto obj = model_proto.objectives(0);
  const IntegerVariable objective_var = m->Integer(obj.objective_var());

  // Default linear objective if we don't find any linear equality defining it.
  *linear_vars = {objective_var};
  *linear_coeffs = {IntegerValue(1)};

  // TODO(user): Expand the linear equation recursively in order to have
  // as much term as possible?
  for (const ConstraintProto& ct : model_proto.constraints()) {
    // Skip everything that is not a linear equality constraint.
    if (!ct.enforcement_literal().empty()) continue;
    if (ct.constraint_case() != ConstraintProto::ConstraintCase::kLinear) {
      continue;
    }
    if (ct.linear().domain().size() != 2) continue;
    if (ct.linear().domain(0) != ct.linear().domain(1)) continue;

    // Find out if objective_var appear in this constraint.
    bool present = false;
    int64 objective_coeff;
    const int num_terms = ct.linear().vars_size();
    for (int i = 0; i < num_terms; ++i) {
      const int ref = ct.linear().vars(i);
      const int64 coeff = ct.linear().coeffs(i);
      if (PositiveRef(ref) == PositiveRef(obj.objective_var())) {
        CHECK(!present) << "Duplicate variables not supported";
        present = true;
        objective_coeff = (ref == obj.objective_var()) ? coeff : -coeff;
      }
    }

    // We use the longest equality we can find.
    // TODO(user): Deal with objective_coeff with a magnitude greater than 1?
    if (present && std::abs(objective_coeff) == 1 &&
        num_terms > linear_vars->size() + 1) {
      linear_vars->clear();
      linear_coeffs->clear();
      const int64 rhs = ct.linear().domain(0);
      if (rhs != 0) {
        linear_vars->push_back(m->Add(NewIntegerVariable(rhs, rhs)));
        linear_coeffs->push_back(IntegerValue(objective_coeff == 1 ? 1 : -1));
      }
      for (int i = 0; i < num_terms; ++i) {
        const int ref = ct.linear().vars(i);
        if (PositiveRef(ref) != PositiveRef(obj.objective_var())) {
          linear_vars->push_back(m->Integer(ref));
          const IntegerValue coeff(ct.linear().coeffs(i));
          linear_coeffs->push_back(objective_coeff == 1 ? -coeff : coeff);
        }
      }
    }
  }
}

}  // namespace

CpSolverResponse SolveCpModelWithoutPresolve(const CpModelProto& model_proto,
                                             Model* model) {
  // Timing.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  // Initialize a default invalid response.
  CpSolverResponse response;
  response.set_status(CpSolverStatus::MODEL_INVALID);

  // Instanciate all the needed variables.
  const VariableUsage usage = ComputeVariableUsage(model_proto);
  ModelWithMapping m(model_proto, usage, model);

  const SatParameters& parameters =
      model->GetOrCreate<SatSolver>()->parameters();

  // Load the constraints.
  std::set<std::string> unsupported_types;
  Trail* trail = model->GetOrCreate<Trail>();
  for (const ConstraintProto& ct : model_proto.constraints()) {
    const int old_num_fixed = trail->Index();
    if (!LoadConstraint(ct, &m)) {
      unsupported_types.insert(ConstraintCaseName(ct.constraint_case()));
      continue;
    }

    // We propagate after each new Boolean constraint but not the integer
    // ones. So we call Propagate() manually here. TODO(user): Do that
    // automatically?
    model->GetOrCreate<SatSolver>()->Propagate();
    if (trail->Index() > old_num_fixed) {
      VLOG(1) << "Constraint fixed " << trail->Index() - old_num_fixed
              << " Boolean variable(s): " << ct.DebugString();
    }
    if (model->GetOrCreate<SatSolver>()->IsModelUnsat()) {
      VLOG(1) << "UNSAT during extraction (after adding '"
              << ConstraintCaseName(ct.constraint_case()) << "'). "
              << ct.DebugString();
      break;
    }
  }
  if (!unsupported_types.empty()) {
    VLOG(1) << "There is unsuported constraints types in this model: ";
    for (const std::string& type : unsupported_types) {
      VLOG(1) << " - " << type;
    }
    return response;
  }

  // Linearize some part of the problem and register LP constraint(s).
  if (parameters.use_global_lp_constraint()) {
    AddLPConstraints(model_proto, &m);
  }

  // Initialize the search strategy function.
  std::function<LiteralIndex()> next_decision;
  if (model_proto.search_strategy().empty()) {
    std::vector<IntegerVariable> decisions;
    for (const int i : usage.integers) {
      decisions.push_back(m.Integer(i));
    }
    next_decision = FirstUnassignedVarAtItsMinHeuristic(decisions, model);
  } else {
    std::vector<Strategy> strategies;
    std::unordered_map<int, std::pair<int64, int64>> var_to_coeff_offset_pair;
    for (const DecisionStrategyProto& proto : model_proto.search_strategy()) {
      strategies.push_back(Strategy());
      Strategy& strategy = strategies.back();
      for (const int ref : proto.variables()) {
        strategy.variables.push_back(m.Integer(ref));
      }
      strategy.var_strategy = proto.variable_selection_strategy();
      strategy.domain_strategy = proto.domain_reduction_strategy();
      for (const auto& tranform : proto.transformations()) {
        const IntegerVariable var = m.Integer(tranform.var());
        if (!ContainsKey(var_to_coeff_offset_pair, var.value())) {
          var_to_coeff_offset_pair[var.value()] = {tranform.positive_coeff(),
                                                   tranform.offset()};
        }
      }
    }
    next_decision =
        ConstructSearchStrategy(var_to_coeff_offset_pair, strategies, model);
  }

  // Solve.
  int num_solutions = 0;
  SatSolver::Status status;
  if (model_proto.objectives_size() == 0) {
    status = SolveIntegerProblemWithLazyEncoding(
        /*assumptions=*/{}, next_decision, model);
    if (status == SatSolver::MODEL_SAT) {
      FillSolutionInResponse(model_proto, m, &response);
    }
  } else {
    // Optimization problem.
    CHECK_EQ(model_proto.objectives_size(), 1);
    const CpObjectiveProto obj = model_proto.objectives(0);
    const IntegerVariable objective_var = m.Integer(obj.objective_var());
    const auto solution_observer =
        [&model_proto, &response, &num_solutions, &obj, &m,
         objective_var](const Model& sat_model) {
          num_solutions++;
          FillSolutionInResponse(model_proto, m, &response);
          response.set_objective_value(
              ScaleObjectiveValue(obj, sat_model.Get(Value(objective_var))));
          VLOG(1) << "Solution #" << num_solutions
                  << " obj:" << response.objective_value()
                  << " num_bool:" << sat_model.Get<SatSolver>()->NumVariables();
        };

    if (parameters.optimize_with_core()) {
      std::vector<IntegerVariable> linear_vars;
      std::vector<IntegerValue> linear_coeffs;
      ExtractLinearObjective(model_proto, &m, &linear_vars, &linear_coeffs);
      status = MinimizeWithCoreAndLazyEncoding(
          VLOG_IS_ON(1), objective_var, linear_vars, linear_coeffs,
          next_decision, solution_observer, model);
    } else {
      status = MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          /*log_info=*/false, objective_var, next_decision, solution_observer,
          model);
    }

    if (status == SatSolver::LIMIT_REACHED) {
      model->GetOrCreate<SatSolver>()->Backtrack(0);
      if (num_solutions == 0) {
        response.set_objective_value(
            ScaleObjectiveValue(obj, model->Get(UpperBound(objective_var))));
      }
      response.set_best_objective_bound(
          ScaleObjectiveValue(obj, model->Get(LowerBound(objective_var))));
    } else if (status == SatSolver::MODEL_SAT) {
      // Optimal!
      response.set_best_objective_bound(response.objective_value());
    }
  }

  // Fill response.
  switch (status) {
    case SatSolver::LIMIT_REACHED: {
      response.set_status(num_solutions != 0 ? CpSolverStatus::MODEL_SAT
                                             : CpSolverStatus::UNKNOWN);
      break;
    }
    case SatSolver::MODEL_SAT: {
      response.set_status(model_proto.objectives_size() != 0
                              ? CpSolverStatus::OPTIMAL
                              : CpSolverStatus::MODEL_SAT);
      break;
    }
    case SatSolver::MODEL_UNSAT: {
      response.set_status(CpSolverStatus::MODEL_UNSAT);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected SatSolver::Status " << status;
  }
  response.set_num_booleans(model->Get<SatSolver>()->NumVariables());
  response.set_num_branches(model->Get<SatSolver>()->num_branches());
  response.set_num_conflicts(model->Get<SatSolver>()->num_failures());
  response.set_num_binary_propagations(
      model->Get<SatSolver>()->num_propagations());
  response.set_num_integer_propagations(
      model->Get<IntegerTrail>() == nullptr
          ? 0
          : model->Get<IntegerTrail>()->num_enqueues());
  response.set_wall_time(wall_timer.Get());
  response.set_user_time(user_timer.Get());
  response.set_deterministic_time(
      model->Get<SatSolver>()->deterministic_time());
  return response;
}

CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model) {
  // Validate model_proto.
  // TODO(user): provide an option to skip this step for speed?
  {
    const std::string error = ValidateCpModel(model_proto);
    if (!error.empty()) {
      VLOG(1) << error;
      CpSolverResponse response;
      response.set_status(CpSolverStatus::MODEL_INVALID);
      return response;
    }
  }

  CpModelProto presolved_proto;
  CpModelProto mapping_proto;
  std::vector<int> postsolve_mapping;
  PresolveCpModel(model_proto, &presolved_proto, &mapping_proto,
                  &postsolve_mapping);

  VLOG(1) << CpModelStats(presolved_proto);

  CpSolverResponse response =
      SolveCpModelWithoutPresolve(presolved_proto, model);
  if (response.status() != CpSolverStatus::MODEL_SAT &&
      response.status() != CpSolverStatus::OPTIMAL) {
    return response;
  }

  // Postsolve.
  for (int i = 0; i < response.solution_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    var_proto->clear_domain();
    var_proto->add_domain(response.solution(i));
    var_proto->add_domain(response.solution(i));
  }
  for (int i = 0; i < response.solution_lower_bounds_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    FillDomain(
        IntersectionOfSortedDisjointIntervals(
            ReadDomain(*var_proto), {{response.solution_lower_bounds(i),
                                      response.solution_upper_bounds(i)}}),
        var_proto);
  }
  Model postsolve_model;

  // Postosolve parameters.
  // TODO(user): this problem is usually trivial, but we may still want to
  // impose a time limit or copy some of the parameters passed by the user.
  {
    SatParameters params;
    params.set_use_global_lp_constraint(false);
    postsolve_model.Add(operations_research::sat::NewSatParameters(params));
  }
  const CpSolverResponse postsolve_response =
      SolveCpModelWithoutPresolve(mapping_proto, &postsolve_model);
  CHECK_EQ(postsolve_response.status(), CpSolverStatus::MODEL_SAT);
  response.clear_solution();
  response.clear_solution_lower_bounds();
  response.clear_solution_upper_bounds();
  if (!postsolve_response.solution().empty()) {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response.add_solution(postsolve_response.solution(i));
    }
    CHECK(SolutionIsFeasible(model_proto,
                             std::vector<int64>(response.solution().begin(),
                                                response.solution().end())));
  } else {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response.add_solution_lower_bounds(
          postsolve_response.solution_lower_bounds(i));
      response.add_solution_upper_bounds(
          postsolve_response.solution_upper_bounds(i));
    }
  }
  return response;
}

}  // namespace sat
}  // namespace operations_research
