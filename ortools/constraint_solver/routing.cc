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

#include "ortools/constraint_solver/routing.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <numeric>
#include <tuple>
#include <utility>

#include "absl/base/casts.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/hash.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/thorough_hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_neighborhoods.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/graph/connectivity.h"
#include "ortools/graph/linear_assignment.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/graph/topologicalsorter.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/bitset.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/stats.h"

namespace operations_research {
class LocalSearchPhaseParameters;
}  // namespace operations_research

DEFINE_int64(sweep_sectors, 1,
             "The number of sectors the space is divided before it is sweeped "
             "by the ray.");

// Trace settings

// TODO(user): Move most of the following settings to a model parameter
// proto.

namespace operations_research {

namespace {
// A decision builder which tries to assign values to variables as close as
// possible to target values first.
// TODO(user): Move to CP solver.
class SetValuesFromTargets : public DecisionBuilder {
 public:
  SetValuesFromTargets(std::vector<IntVar*> variables,
                       std::vector<int64> targets)
      : variables_(std::move(variables)),
        targets_(std::move(targets)),
        index_(0),
        steps_(variables_.size(), 0) {
    DCHECK_EQ(variables_.size(), targets_.size());
  }
  Decision* Next(Solver* const solver) override {
    int index = index_.Value();
    while (index < variables_.size() && variables_[index]->Bound()) {
      ++index;
    }
    index_.SetValue(solver, index);
    if (index >= variables_.size()) return nullptr;
    const int64 variable_min = variables_[index]->Min();
    const int64 variable_max = variables_[index]->Max();
    // Target can be before, inside, or after the variable range.
    // We do a trichotomy on this for clarity.
    if (targets_[index] <= variable_min) {
      return solver->MakeAssignVariableValue(variables_[index], variable_min);
    } else if (targets_[index] >= variable_max) {
      return solver->MakeAssignVariableValue(variables_[index], variable_max);
    } else {
      int64 step = steps_[index];
      int64 value = CapAdd(targets_[index], step);
      // If value is out of variable's range, we can remove the interval of
      // values already explored (which can make the solver fail) and
      // recall Next() to get back into the trichotomy above.
      if (value < variable_min || variable_max < value) {
        step = GetNextStep(step);
        value = CapAdd(targets_[index], step);
        if (step > 0) {
          // Values in [variable_min, value) were already explored.
          variables_[index]->SetMin(value);
        } else {
          // Values in (value, variable_max] were already explored.
          variables_[index]->SetMax(value);
        }
        return Next(solver);
      }
      steps_.SetValue(solver, index, GetNextStep(step));
      return solver->MakeAssignVariableValueOrDoNothing(variables_[index],
                                                        value);
    }
  }

 private:
  int64 GetNextStep(int64 step) const {
    return (step > 0) ? -step : CapSub(1, step);
  }
  const std::vector<IntVar*> variables_;
  const std::vector<int64> targets_;
  Rev<int> index_;
  RevArray<int64> steps_;
};

}  // namespace

DecisionBuilder* MakeSetValuesFromTargets(Solver* solver,
                                          std::vector<IntVar*> variables,
                                          std::vector<int64> targets) {
  return solver->RevAlloc(
      new SetValuesFromTargets(std::move(variables), std::move(targets)));
}

namespace {

bool DimensionFixedTransitsEqualTransitEvaluatorForVehicle(
    const RoutingDimension& dimension, int vehicle) {
  const RoutingModel* const model = dimension.model();
  int node = model->Start(vehicle);
  while (!model->IsEnd(node)) {
    if (!model->NextVar(node)->Bound()) {
      return false;
    }
    const int next = model->NextVar(node)->Value();
    if (dimension.transit_evaluator(vehicle)(node, next) !=
        dimension.FixedTransitVar(node)->Value()) {
      return false;
    }
    node = next;
  }
  return true;
}

bool DimensionFixedTransitsEqualTransitEvaluators(
    const RoutingDimension& dimension) {
  for (int vehicle = 0; vehicle < dimension.model()->vehicles(); vehicle++) {
    if (!DimensionFixedTransitsEqualTransitEvaluatorForVehicle(dimension,
                                                               vehicle)) {
      return false;
    }
  }
  return true;
}

class SetCumulsFromLocalDimensionCosts : public DecisionBuilder {
 public:
  SetCumulsFromLocalDimensionCosts(
      const std::vector<std::unique_ptr<LocalDimensionCumulOptimizer>>*
          local_optimizers,
      const std::vector<std::unique_ptr<LocalDimensionCumulOptimizer>>*
          local_mp_optimizers,
      SearchMonitor* monitor, bool optimize_and_pack = false)
      : local_optimizers_(*local_optimizers),
        local_mp_optimizers_(*local_mp_optimizers),
        monitor_(monitor),
        optimize_and_pack_(optimize_and_pack) {}
  Decision* Next(Solver* const solver) override {
    // The following boolean variable indicates if the solver should fail, in
    // order to postpone the Fail() call until after the internal for loop, so
    // there are no memory leaks related to the cumul_values vector.
    bool should_fail = false;
    for (int i = 0; i < local_optimizers_.size(); ++i) {
      const auto& local_optimizer = local_optimizers_[i];
      const RoutingDimension* const dimension = local_optimizer->dimension();
      RoutingModel* const model = dimension->model();
      const auto next = [model](int64 i) { return model->NextVar(i)->Value(); };
      const auto compute_cumul_values =
          [this, &next](LocalDimensionCumulOptimizer* optimizer, int vehicle,
                        std::vector<int64>* cumul_values,
                        std::vector<int64>* break_start_end_values) {
            if (optimize_and_pack_) {
              return optimizer->ComputePackedRouteCumuls(
                  vehicle, next, cumul_values, break_start_end_values);
            } else {
              return optimizer->ComputeRouteCumuls(vehicle, next, cumul_values,
                                                   break_start_end_values);
            }
          };
      for (int vehicle = 0; vehicle < model->vehicles(); ++vehicle) {
        // TODO(user): Investigate if we should skip unused vehicles.
        DCHECK(DimensionFixedTransitsEqualTransitEvaluatorForVehicle(*dimension,
                                                                     vehicle));
        const bool vehicle_has_break_constraint =
            dimension->HasBreakConstraints() &&
            !dimension->GetBreakIntervalsOfVehicle(vehicle).empty();
        LocalDimensionCumulOptimizer* const optimizer =
            vehicle_has_break_constraint ? local_mp_optimizers_[i].get()
                                         : local_optimizer.get();
        DCHECK_NE(optimizer, nullptr);
        std::vector<int64> cumul_values;
        std::vector<int64> break_start_end_values;
        const DimensionSchedulingStatus status = compute_cumul_values(
            optimizer, vehicle, &cumul_values, &break_start_end_values);
        if (status == DimensionSchedulingStatus::INFEASIBLE) {
          should_fail = true;
          break;
        }
        // If relaxation is not feasible, try the MILP optimizer.
        if (status == DimensionSchedulingStatus::RELAXED_OPTIMAL_ONLY) {
          cumul_values.clear();
          break_start_end_values.clear();
          DCHECK(local_mp_optimizers_[i] != nullptr);
          if (compute_cumul_values(local_mp_optimizers_[i].get(), vehicle,
                                   &cumul_values, &break_start_end_values) ==
              DimensionSchedulingStatus::INFEASIBLE) {
            should_fail = true;
            break;
          }
        } else {
          DCHECK(status == DimensionSchedulingStatus::OPTIMAL);
        }
        // Concatenate cumul_values and break_start_end_values into cp_values,
        // generate corresponding cp_variables vector.
        std::vector<IntVar*> cp_variables;
        std::vector<int64> cp_values;
        std::swap(cp_values, cumul_values);
        {
          int current = model->Start(vehicle);
          while (true) {
            cp_variables.push_back(dimension->CumulVar(current));
            if (!model->IsEnd(current)) {
              current = model->NextVar(current)->Value();
            } else {
              break;
            }
          }
        }
        // Setting the cumuls of path start/end first is more efficient than
        // setting the cumuls in order of path appearance, because setting start
        // and end cumuls gives an opportunity to fix all cumuls with two
        // decisions instead of |path| decisions.
        // To this effect, we put end cumul just after the start cumul.
        std::swap(cp_variables[1], cp_variables.back());
        std::swap(cp_values[1], cp_values.back());
        if (dimension->HasBreakConstraints()) {
          for (IntervalVar* interval :
               dimension->GetBreakIntervalsOfVehicle(vehicle)) {
            cp_variables.push_back(interval->SafeStartExpr(0)->Var());
            cp_variables.push_back(interval->SafeEndExpr(0)->Var());
          }
          cp_values.insert(cp_values.end(), break_start_end_values.begin(),
                           break_start_end_values.end());
        }
        // Value kint64min signals an unoptimized variable, set to min instead.
        for (int i = 0; i < cp_values.size(); ++i) {
          if (cp_values[i] == kint64min) {
            cp_values[i] = cp_variables[i]->Min();
          }
        }
        if (!solver->SolveAndCommit(
                MakeSetValuesFromTargets(solver, std::move(cp_variables),
                                         std::move(cp_values)),
                monitor_)) {
          should_fail = true;
          break;
        }
      }
      if (should_fail) {
        solver->Fail();
      }
    }
    return nullptr;
  }

 private:
  const std::vector<std::unique_ptr<LocalDimensionCumulOptimizer>>&
      local_optimizers_;
  const std::vector<std::unique_ptr<LocalDimensionCumulOptimizer>>&
      local_mp_optimizers_;
  SearchMonitor* const monitor_;
  const bool optimize_and_pack_;
};

class SetCumulsFromGlobalDimensionCosts : public DecisionBuilder {
 public:
  SetCumulsFromGlobalDimensionCosts(
      const std::vector<std::unique_ptr<GlobalDimensionCumulOptimizer>>*
          global_optimizers,
      SearchMonitor* monitor, bool optimize_and_pack = false)
      : global_optimizers_(*global_optimizers),
        monitor_(monitor),
        optimize_and_pack_(optimize_and_pack) {}
  Decision* Next(Solver* const solver) override {
    // The following boolean variable indicates if the solver should fail, in
    // order to postpone the Fail() call until after the for loop, so there are
    // no memory leaks related to the cumul_values vector.
    bool should_fail = false;
    for (const auto& global_optimizer : global_optimizers_) {
      const RoutingDimension* dimension = global_optimizer->dimension();
      RoutingModel* const model = dimension->model();

      const auto next = [model](int64 i) { return model->NextVar(i)->Value(); };

      DCHECK(DimensionFixedTransitsEqualTransitEvaluators(*dimension));

      std::vector<int64> cumul_values;
      std::vector<int64> break_start_end_values;
      const bool cumuls_optimized =
          optimize_and_pack_
              ? global_optimizer->ComputePackedCumuls(next, &cumul_values,
                                                      &break_start_end_values)
              : global_optimizer->ComputeCumuls(next, &cumul_values,
                                                &break_start_end_values);
      if (!cumuls_optimized) {
        should_fail = true;
        break;
      }
      // Concatenate cumul_values and break_start_end_values into cp_values,
      // generate corresponding cp_variables vector.
      std::vector<IntVar*> cp_variables = dimension->cumuls();
      std::vector<int64> cp_values;
      std::swap(cp_values, cumul_values);
      if (dimension->HasBreakConstraints()) {
        const int num_vehicles = model->vehicles();
        for (int vehicle = 0; vehicle < num_vehicles; ++vehicle) {
          for (IntervalVar* interval :
               dimension->GetBreakIntervalsOfVehicle(vehicle)) {
            cp_variables.push_back(interval->SafeStartExpr(0)->Var());
            cp_variables.push_back(interval->SafeEndExpr(0)->Var());
          }
        }
        cp_values.insert(cp_values.end(), break_start_end_values.begin(),
                         break_start_end_values.end());
      }
      // Value kint64min signals an unoptimized variable, set to min instead.
      for (int i = 0; i < cp_values.size(); ++i) {
        if (cp_values[i] == kint64min) {
          cp_values[i] = cp_variables[i]->Min();
        }
      }
      if (!solver->SolveAndCommit(
              MakeSetValuesFromTargets(solver, std::move(cp_variables),
                                       std::move(cp_values)),
              monitor_)) {
        should_fail = true;
        break;
      }
    }
    if (should_fail) {
      solver->Fail();
    }
    return nullptr;
  }

 private:
  const std::vector<std::unique_ptr<GlobalDimensionCumulOptimizer>>&
      global_optimizers_;
  SearchMonitor* const monitor_;
  const bool optimize_and_pack_;
};

}  // namespace

const Assignment* RoutingModel::PackCumulsOfOptimizerDimensionsFromAssignment(
    const Assignment* original_assignment, absl::Duration duration_limit) {
  CHECK(closed_);
  if (original_assignment == nullptr) return nullptr;
  if (duration_limit <= absl::ZeroDuration()) return original_assignment;
  if (global_dimension_optimizers_.empty() &&
      local_dimension_optimizers_.empty()) {
    DCHECK(local_dimension_mp_optimizers_.empty());
    return original_assignment;
  }
  RegularLimit* const limit = GetOrCreateLimit();
  limit->UpdateLimits(duration_limit, kint64max, kint64max, kint64max);

  // Initialize the packed_assignment with the Next values in the
  // original_assignment.
  Assignment* packed_assignment = solver_->MakeAssignment();
  packed_assignment->Add(Nexts());
  packed_assignment->CopyIntersection(original_assignment);

  std::vector<DecisionBuilder*> decision_builders;
  decision_builders.push_back(solver_->MakeRestoreAssignment(preassignment_));
  decision_builders.push_back(
      solver_->MakeRestoreAssignment(packed_assignment));
  decision_builders.push_back(
      solver_->RevAlloc(new SetCumulsFromLocalDimensionCosts(
          &local_dimension_optimizers_, &local_dimension_mp_optimizers_,
          GetOrCreateLargeNeighborhoodSearchLimit(),
          /*optimize_and_pack=*/true)));
  decision_builders.push_back(
      solver_->RevAlloc(new SetCumulsFromGlobalDimensionCosts(
          &global_dimension_optimizers_,
          GetOrCreateLargeNeighborhoodSearchLimit(),
          /*optimize_and_pack=*/true)));
  decision_builders.push_back(
      CreateFinalizerForMinimizedAndMaximizedVariables());

  DecisionBuilder* restore_pack_and_finalize =
      solver_->Compose(decision_builders);
  solver_->Solve(restore_pack_and_finalize,
                 packed_dimensions_assignment_collector_, limit);

  if (packed_dimensions_assignment_collector_->solution_count() != 1) {
    LOG(ERROR) << "The given assignment is not valid for this model, or cannot "
                  "be packed.";
    return nullptr;
  }

  packed_assignment->Copy(original_assignment);
  packed_assignment->CopyIntersection(
      packed_dimensions_assignment_collector_->solution(0));

  return packed_assignment;
}

namespace {
// Constraint which ensures that var != values.
class DifferentFromValues : public Constraint {
 public:
  DifferentFromValues(Solver* solver, IntVar* var, std::vector<int64> values)
      : Constraint(solver), var_(var), values_(std::move(values)) {}
  void Post() override {}
  void InitialPropagate() override { var_->RemoveValues(values_); }
  std::string DebugString() const override { return "DifferentFromValues"; }
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(RoutingModelVisitor::kRemoveValues, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               {var_});
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument, values_);
    visitor->EndVisitConstraint(RoutingModelVisitor::kRemoveValues, this);
  }

 private:
  IntVar* const var_;
  const std::vector<int64> values_;
};

// Set of "light" constraints, well-suited for use within Local Search.
// These constraints are "checking" constraints, only triggered on WhenBound
// events. The provide very little (or no) domain filtering.
// TODO(user): Move to core constraintsolver library.

// Light one-dimension function-based element constraint ensuring:
// var == values(index).
// Doesn't perform bound reduction of the resulting variable until the index
// variable is bound.
// If deep_serialize returns false, the model visitor will not extract all
// possible values from the values function.
template <typename F>
class LightFunctionElementConstraint : public Constraint {
 public:
  LightFunctionElementConstraint(Solver* const solver, IntVar* const var,
                                 IntVar* const index, F values,
                                 std::function<bool()> deep_serialize)
      : Constraint(solver),
        var_(var),
        index_(index),
        values_(std::move(values)),
        deep_serialize_(std::move(deep_serialize)) {}
  ~LightFunctionElementConstraint() override {}

  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &LightFunctionElementConstraint::IndexBound,
        "IndexBound");
    index_->WhenBound(demon);
  }

  void InitialPropagate() override {
    if (index_->Bound()) {
      IndexBound();
    }
  }

  std::string DebugString() const override {
    return "LightFunctionElementConstraint";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(RoutingModelVisitor::kLightElement, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    // Warning: This will expand all values into a vector.
    if (deep_serialize_()) {
      visitor->VisitInt64ToInt64Extension(values_, index_->Min(),
                                          index_->Max());
    }
    visitor->EndVisitConstraint(RoutingModelVisitor::kLightElement, this);
  }

 private:
  void IndexBound() { var_->SetValue(values_(index_->Min())); }

  IntVar* const var_;
  IntVar* const index_;
  F values_;
  std::function<bool()> deep_serialize_;
};

template <typename F>
Constraint* MakeLightElement(Solver* const solver, IntVar* const var,
                             IntVar* const index, F values,
                             std::function<bool()> deep_serialize) {
  return solver->RevAlloc(new LightFunctionElementConstraint<F>(
      solver, var, index, std::move(values), std::move(deep_serialize)));
}

// Light two-dimension function-based element constraint ensuring:
// var == values(index1, index2).
// Doesn't perform bound reduction of the resulting variable until the index
// variables are bound.
// Ownership of the 'values' callback is taken by the constraint.
template <typename F>
class LightFunctionElement2Constraint : public Constraint {
 public:
  LightFunctionElement2Constraint(Solver* const solver, IntVar* const var,
                                  IntVar* const index1, IntVar* const index2,
                                  F values,
                                  std::function<bool()> deep_serialize)
      : Constraint(solver),
        var_(var),
        index1_(index1),
        index2_(index2),
        values_(std::move(values)),
        deep_serialize_(std::move(deep_serialize)) {}
  ~LightFunctionElement2Constraint() override {}
  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &LightFunctionElement2Constraint::IndexBound,
        "IndexBound");
    index1_->WhenBound(demon);
    index2_->WhenBound(demon);
  }
  void InitialPropagate() override { IndexBound(); }

  std::string DebugString() const override {
    return "LightFunctionElement2Constraint";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(RoutingModelVisitor::kLightElement2, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index1_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndex2Argument,
                                            index2_);
    // Warning: This will expand all values into a vector.
    const int64 index1_min = index1_->Min();
    const int64 index1_max = index1_->Max();
    visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, index1_min);
    visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, index1_max);
    if (deep_serialize_()) {
      for (int i = index1_min; i <= index1_max; ++i) {
        visitor->VisitInt64ToInt64Extension(
            [this, i](int64 j) { return values_(i, j); }, index2_->Min(),
            index2_->Max());
      }
    }
    visitor->EndVisitConstraint(RoutingModelVisitor::kLightElement2, this);
  }

 private:
  void IndexBound() {
    if (index1_->Bound() && index2_->Bound()) {
      var_->SetValue(values_(index1_->Min(), index2_->Min()));
    }
  }

  IntVar* const var_;
  IntVar* const index1_;
  IntVar* const index2_;
  Solver::IndexEvaluator2 values_;
  std::function<bool()> deep_serialize_;
};

template <typename F>
Constraint* MakeLightElement2(Solver* const solver, IntVar* const var,
                              IntVar* const index1, IntVar* const index2,
                              F values, std::function<bool()> deep_serialize) {
  return solver->RevAlloc(new LightFunctionElement2Constraint<F>(
      solver, var, index1, index2, std::move(values),
      std::move(deep_serialize)));
}

// Shortcuts to spawn neighborhood operators from ./routing_neighborhoods.h.
// TODO(user): Consider removing all these trivial wrappers and just inlining
// the solver->RevAlloc(new ...Operator()) calls in the client code.

LocalSearchOperator* MakeRelocateNeighbors(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    RoutingModel::TransitCallback2 arc_evaluator) {
  return solver->RevAlloc(new MakeRelocateNeighborsOperator(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(arc_evaluator)));
}

LocalSearchOperator* MakePairActive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new MakePairActiveOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* MakePairInactive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new MakePairInactiveOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* MakePairRelocate(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new PairRelocateOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* MakeLightPairRelocate(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new LightPairRelocateOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* MakePairExchange(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new PairExchangeOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* MakePairExchangeRelocate(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new PairExchangeRelocateOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* SwapIndexPair(Solver* const solver,
                                   const std::vector<IntVar*>& vars,
                                   const std::vector<IntVar*>& secondary_vars,
                                   const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(
      new SwapIndexPairOperator(vars, secondary_vars, pairs));
}

LocalSearchOperator* IndexPairSwapActive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new IndexPairSwapActiveOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* PairNodeSwapActive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->ConcatenateOperators(
      {solver->RevAlloc(new PairNodeSwapActiveOperator<true>(
           vars, secondary_vars, start_empty_path_class, pairs)),
       solver->RevAlloc(new PairNodeSwapActiveOperator<false>(
           vars, secondary_vars, std::move(start_empty_path_class), pairs))});
}

LocalSearchOperator* MakeRelocateSubtrip(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new RelocateSubtrip(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

LocalSearchOperator* MakeExchangeSubtrip(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::IndexPairs& pairs) {
  return solver->RevAlloc(new ExchangeSubtrip(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

// Evaluators
template <class A, class B>
static int64 ReturnZero(A a, B b) {
  return 0;
}

bool TransitCallbackPositive(const RoutingTransitCallback2& callback, int size1,
                             int size2) {
  for (int i = 0; i < size1; i++) {
    for (int j = 0; j < size2; j++) {
      if (callback(i, j) < 0) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

// ----- Routing model -----

static const int kUnassigned = -1;
const int64 RoutingModel::kNoPenalty = -1;

const RoutingModel::DisjunctionIndex RoutingModel::kNoDisjunction(-1);

const RoutingModel::DimensionIndex RoutingModel::kNoDimension(-1);

RoutingModel::RoutingModel(const RoutingIndexManager& index_manager)
    : RoutingModel(index_manager, DefaultRoutingModelParameters()) {}

RoutingModel::RoutingModel(const RoutingIndexManager& index_manager,
                           const RoutingModelParameters& parameters)
    : nodes_(index_manager.num_nodes()),
      vehicles_(index_manager.num_vehicles()),
      max_active_vehicles_(vehicles_),
      fixed_cost_of_vehicle_(vehicles_, 0),
      cost_class_index_of_vehicle_(vehicles_, CostClassIndex(-1)),
      linear_cost_factor_of_vehicle_(vehicles_, 0),
      quadratic_cost_factor_of_vehicle_(vehicles_, 0),
      vehicle_amortized_cost_factors_set_(false),
      consider_empty_route_costs_(vehicles_, false),
      cost_classes_(),
      costs_are_homogeneous_across_vehicles_(
          parameters.reduce_vehicle_cost_model()),
      cache_callbacks_(false),
      vehicle_class_index_of_vehicle_(vehicles_, VehicleClassIndex(-1)),
      vehicle_pickup_delivery_policy_(vehicles_, PICKUP_AND_DELIVERY_NO_ORDER),
      has_hard_type_incompatibilities_(false),
      has_temporal_type_incompatibilities_(false),
      has_same_vehicle_type_requirements_(false),
      has_temporal_type_requirements_(false),
      num_visit_types_(0),
      starts_(vehicles_),
      ends_(vehicles_),
      manager_(index_manager) {
  // Initialize vehicle costs to the zero evaluator.
  vehicle_to_transit_cost_.assign(
      vehicles_, RegisterTransitCallback(ReturnZero<int64, int64>));
  // Active caching after initializing vehicle_to_transit_cost_ to avoid
  // uselessly caching ReturnZero.
  cache_callbacks_ = (nodes_ <= parameters.max_callback_cache_size());

  VLOG(1) << "Model parameters:\n" << parameters.DebugString();
  ConstraintSolverParameters solver_parameters =
      parameters.has_solver_parameters() ? parameters.solver_parameters()
                                         : Solver::DefaultSolverParameters();
  solver_ = absl::make_unique<Solver>("Routing", solver_parameters);
  // TODO(user): Remove when removal of NodeIndex is complete.
  start_end_count_ = index_manager.num_unique_depots();
  Initialize();

  const int64 size = Size();
  index_to_pickup_index_pairs_.resize(size);
  index_to_delivery_index_pairs_.resize(size);
  index_to_visit_type_.resize(index_manager.num_indices(), kUnassigned);
  index_to_type_policy_.resize(index_manager.num_indices());

  index_to_vehicle_.resize(index_manager.num_indices(), kUnassigned);
  for (int v = 0; v < index_manager.num_vehicles(); ++v) {
    starts_[v] = index_manager.GetStartIndex(v);
    index_to_vehicle_[starts_[v]] = v;
    ends_[v] = index_manager.GetEndIndex(v);
    index_to_vehicle_[ends_[v]] = v;
  }

  const std::vector<RoutingIndexManager::NodeIndex>& index_to_node =
      index_manager.GetIndexToNodeMap();
  index_to_equivalence_class_.resize(index_manager.num_indices());
  for (int i = 0; i < index_to_node.size(); ++i) {
    index_to_equivalence_class_[i] = index_to_node[i].value();
  }
  allowed_vehicles_.resize(Size() + vehicles_);
}

void RoutingModel::Initialize() {
  const int size = Size();
  // Next variables
  solver_->MakeIntVarArray(size, 0, size + vehicles_ - 1, "Nexts", &nexts_);
  solver_->AddConstraint(solver_->MakeAllDifferent(nexts_, false));
  index_to_disjunctions_.resize(size + vehicles_);
  // Vehicle variables. In case that node i is not active, vehicle_vars_[i] is
  // bound to -1.
  solver_->MakeIntVarArray(size + vehicles_, -1, vehicles_ - 1, "Vehicles",
                           &vehicle_vars_);
  // Active variables
  solver_->MakeBoolVarArray(size, "Active", &active_);
  // Active vehicle variables
  solver_->MakeBoolVarArray(vehicles_, "ActiveVehicle", &vehicle_active_);
  // Variables representing vehicles contributing to cost.
  solver_->MakeBoolVarArray(vehicles_, "VehicleCostsConsidered",
                            &vehicle_costs_considered_);
  // Is-bound-to-end variables.
  solver_->MakeBoolVarArray(size + vehicles_, "IsBoundToEnd",
                            &is_bound_to_end_);
  // Cost cache
  cost_cache_.clear();
  cost_cache_.resize(size + vehicles_, {kUnassigned, CostClassIndex(-1), 0});
  preassignment_ = solver_->MakeAssignment();
}

RoutingModel::~RoutingModel() {
  gtl::STLDeleteElements(&dimensions_);

  // State dependent transit callbacks.
  absl::flat_hash_set<RangeIntToIntFunction*> value_functions_delete;
  absl::flat_hash_set<RangeMinMaxIndexFunction*> index_functions_delete;
  for (const auto& cache_line : state_dependent_transit_evaluators_cache_) {
    for (const auto& key_transit : *cache_line) {
      value_functions_delete.insert(key_transit.second.transit);
      index_functions_delete.insert(key_transit.second.transit_plus_identity);
    }
  }
  gtl::STLDeleteElements(&value_functions_delete);
  gtl::STLDeleteElements(&index_functions_delete);
}

int RoutingModel::RegisterUnaryTransitCallback(TransitCallback1 callback) {
  const int index = unary_transit_evaluators_.size();
  unary_transit_evaluators_.push_back(std::move(callback));
  return RegisterTransitCallback([this, index](int i, int j) {
    return unary_transit_evaluators_[index](i);
  });
}

int RoutingModel::RegisterPositiveUnaryTransitCallback(
    TransitCallback1 callback) {
  is_transit_evaluator_positive_.push_back(true);
  DCHECK(TransitCallbackPositive(
      [&callback](int i, int) { return callback(i); }, Size() + vehicles(), 1));
  return RegisterUnaryTransitCallback(std::move(callback));
}

int RoutingModel::RegisterTransitCallback(TransitCallback2 callback) {
  if (cache_callbacks_) {
    const int size = Size() + vehicles();
    std::vector<int64> cache(size * size, 0);
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        cache[i * size + j] = callback(i, j);
      }
    }
    transit_evaluators_.push_back(
        [cache, size](int64 i, int64 j) { return cache[i * size + j]; });
  } else {
    transit_evaluators_.push_back(std::move(callback));
  }
  if (transit_evaluators_.size() != unary_transit_evaluators_.size()) {
    DCHECK_EQ(transit_evaluators_.size(), unary_transit_evaluators_.size() + 1);
    unary_transit_evaluators_.push_back(nullptr);
  }
  if (transit_evaluators_.size() != is_transit_evaluator_positive_.size()) {
    DCHECK_EQ(transit_evaluators_.size(),
              is_transit_evaluator_positive_.size() + 1);
    is_transit_evaluator_positive_.push_back(false);
  }
  return transit_evaluators_.size() - 1;
}

int RoutingModel::RegisterPositiveTransitCallback(TransitCallback2 callback) {
  is_transit_evaluator_positive_.push_back(true);
  DCHECK(TransitCallbackPositive(callback, Size() + vehicles(),
                                 Size() + vehicles()));
  return RegisterTransitCallback(std::move(callback));
}

int RoutingModel::RegisterStateDependentTransitCallback(
    VariableIndexEvaluator2 callback) {
  state_dependent_transit_evaluators_cache_.push_back(
      absl::make_unique<StateDependentTransitCallbackCache>());
  StateDependentTransitCallbackCache* const cache =
      state_dependent_transit_evaluators_cache_.back().get();
  state_dependent_transit_evaluators_.push_back(
      [cache, callback](int64 i, int64 j) {
        StateDependentTransit value;
        if (gtl::FindCopy(*cache, CacheKey(i, j), &value)) return value;
        value = callback(i, j);
        cache->insert({CacheKey(i, j), value});
        return value;
      });
  return state_dependent_transit_evaluators_.size() - 1;
}

void RoutingModel::AddNoCycleConstraintInternal() {
  if (no_cycle_constraint_ == nullptr) {
    no_cycle_constraint_ = solver_->MakeNoCycle(nexts_, active_);
    solver_->AddConstraint(no_cycle_constraint_);
  }
}

bool RoutingModel::AddDimension(int evaluator_index, int64 slack_max,
                                int64 capacity, bool fix_start_cumul_to_zero,
                                const std::string& name) {
  const std::vector<int> evaluator_indices(vehicles_, evaluator_index);
  std::vector<int64> capacities(vehicles_, capacity);
  return AddDimensionWithCapacityInternal(evaluator_indices, slack_max,
                                          std::move(capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithVehicleTransits(
    const std::vector<int>& evaluator_indices, int64 slack_max, int64 capacity,
    bool fix_start_cumul_to_zero, const std::string& name) {
  std::vector<int64> capacities(vehicles_, capacity);
  return AddDimensionWithCapacityInternal(evaluator_indices, slack_max,
                                          std::move(capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithVehicleCapacity(
    int evaluator_index, int64 slack_max, std::vector<int64> vehicle_capacities,
    bool fix_start_cumul_to_zero, const std::string& name) {
  const std::vector<int> evaluator_indices(vehicles_, evaluator_index);
  return AddDimensionWithCapacityInternal(evaluator_indices, slack_max,
                                          std::move(vehicle_capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithVehicleTransitAndCapacity(
    const std::vector<int>& evaluator_indices, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  return AddDimensionWithCapacityInternal(evaluator_indices, slack_max,
                                          std::move(vehicle_capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithCapacityInternal(
    const std::vector<int>& evaluator_indices, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  CHECK_EQ(vehicles_, vehicle_capacities.size());
  return InitializeDimensionInternal(
      evaluator_indices, std::vector<int>(), slack_max, fix_start_cumul_to_zero,
      new RoutingDimension(this, std::move(vehicle_capacities), name, nullptr));
}

bool RoutingModel::InitializeDimensionInternal(
    const std::vector<int>& evaluator_indices,
    const std::vector<int>& state_dependent_evaluator_indices, int64 slack_max,
    bool fix_start_cumul_to_zero, RoutingDimension* dimension) {
  CHECK(dimension != nullptr);
  CHECK_EQ(vehicles_, evaluator_indices.size());
  CHECK((dimension->base_dimension_ == nullptr &&
         state_dependent_evaluator_indices.empty()) ||
        vehicles_ == state_dependent_evaluator_indices.size());
  if (!HasDimension(dimension->name())) {
    const DimensionIndex dimension_index(dimensions_.size());
    dimension_name_to_index_[dimension->name()] = dimension_index;
    dimensions_.push_back(dimension);
    dimension->Initialize(evaluator_indices, state_dependent_evaluator_indices,
                          slack_max);
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, dimension->cumuls(), dimension->transits()));
    if (fix_start_cumul_to_zero) {
      for (int i = 0; i < vehicles_; ++i) {
        IntVar* const start_cumul = dimension->CumulVar(Start(i));
        CHECK_EQ(0, start_cumul->Min());
        start_cumul->SetValue(0);
      }
    }
    return true;
  }
  delete dimension;
  return false;
}

namespace {
int RegisterCallback(RoutingTransitCallback2 callback, bool is_positive,
                     RoutingModel* model) {
  if (is_positive) {
    return model->RegisterPositiveTransitCallback(std::move(callback));
  }
  return model->RegisterTransitCallback(std::move(callback));
}

int RegisterUnaryCallback(RoutingTransitCallback1 callback, bool is_positive,
                          RoutingModel* model) {
  if (is_positive) {
    return model->RegisterPositiveUnaryTransitCallback(std::move(callback));
  }
  return model->RegisterUnaryTransitCallback(std::move(callback));
}
}  // namespace

bool RoutingModel::AddConstantDimensionWithSlack(
    int64 value, int64 capacity, int64 slack_max, bool fix_start_cumul_to_zero,
    const std::string& dimension_name) {
  return AddDimension(RegisterUnaryCallback([value](int64) { return value; },
                                            /*is_positive=*/value >= 0, this),
                      slack_max, capacity, fix_start_cumul_to_zero,
                      dimension_name);
}

bool RoutingModel::AddVectorDimension(std::vector<int64> values, int64 capacity,
                                      bool fix_start_cumul_to_zero,
                                      const std::string& dimension_name) {
  return AddDimension(
      RegisterUnaryCallback(
          [this, values](int64 i) {
            return values[manager_.IndexToNode(i).value()];
          },
          /*is_positive=*/
          std::all_of(std::begin(values), std::end(values),
                      [](int64 transit) { return transit >= 0; }),
          this),
      0, capacity, fix_start_cumul_to_zero, dimension_name);
}

bool RoutingModel::AddMatrixDimension(std::vector<std::vector<int64>> values,
                                      int64 capacity,
                                      bool fix_start_cumul_to_zero,
                                      const std::string& dimension_name) {
  bool all_transits_positive = true;
  for (const std::vector<int64>& transit_values : values) {
    all_transits_positive =
        std::all_of(std::begin(transit_values), std::end(transit_values),
                    [](int64 transit) { return transit >= 0; });
    if (!all_transits_positive) {
      break;
    }
  }
  return AddDimension(RegisterCallback(
                          [this, values](int64 i, int64 j) {
                            return values[manager_.IndexToNode(i).value()]
                                         [manager_.IndexToNode(j).value()];
                          },
                          all_transits_positive, this),
                      0, capacity, fix_start_cumul_to_zero, dimension_name);
}

namespace {
// RangeMakeElementExpr is an IntExpr that corresponds to a
// RangeIntToIntFunction indexed by an IntVar.
// Do not create this class dicretly, but rather use MakeRangeMakeElementExpr.
class RangeMakeElementExpr : public BaseIntExpr {
 public:
  RangeMakeElementExpr(const RangeIntToIntFunction* callback, IntVar* index,
                       Solver* s)
      : BaseIntExpr(s), callback_(ABSL_DIE_IF_NULL(callback)), index_(index) {
    CHECK(callback_ != nullptr);
    CHECK(index != nullptr);
  }

  int64 Min() const override {
    // Converting [index_->Min(), index_->Max()] to [idx_min, idx_max).
    const int idx_min = index_->Min();
    const int idx_max = index_->Max() + 1;
    return (idx_min < idx_max) ? callback_->RangeMin(idx_min, idx_max)
                               : kint64max;
  }
  void SetMin(int64 new_min) override {
    const int64 old_min = Min();
    const int64 old_max = Max();
    if (old_min < new_min && new_min <= old_max) {
      const int64 old_idx_min = index_->Min();
      const int64 old_idx_max = index_->Max() + 1;
      if (old_idx_min < old_idx_max) {
        const int64 new_idx_min = callback_->RangeFirstInsideInterval(
            old_idx_min, old_idx_max, new_min, old_max + 1);
        index_->SetMin(new_idx_min);
        if (new_idx_min < old_idx_max) {
          const int64 new_idx_max = callback_->RangeLastInsideInterval(
              new_idx_min, old_idx_max, new_min, old_max + 1);
          index_->SetMax(new_idx_max);
        }
      }
    }
  }
  int64 Max() const override {
    // Converting [index_->Min(), index_->Max()] to [idx_min, idx_max).
    const int idx_min = index_->Min();
    const int idx_max = index_->Max() + 1;
    return (idx_min < idx_max) ? callback_->RangeMax(idx_min, idx_max)
                               : kint64min;
  }
  void SetMax(int64 new_max) override {
    const int64 old_min = Min();
    const int64 old_max = Max();
    if (old_min <= new_max && new_max < old_max) {
      const int64 old_idx_min = index_->Min();
      const int64 old_idx_max = index_->Max() + 1;
      if (old_idx_min < old_idx_max) {
        const int64 new_idx_min = callback_->RangeFirstInsideInterval(
            old_idx_min, old_idx_max, old_min, new_max + 1);
        index_->SetMin(new_idx_min);
        if (new_idx_min < old_idx_max) {
          const int64 new_idx_max = callback_->RangeLastInsideInterval(
              new_idx_min, old_idx_max, old_min, new_max + 1);
          index_->SetMax(new_idx_max);
        }
      }
    }
  }
  void WhenRange(Demon* d) override { index_->WhenRange(d); }

 private:
  const RangeIntToIntFunction* const callback_;
  IntVar* const index_;
};

IntExpr* MakeRangeMakeElementExpr(const RangeIntToIntFunction* callback,
                                  IntVar* index, Solver* s) {
  return s->RegisterIntExpr(
      s->RevAlloc(new RangeMakeElementExpr(callback, index, s)));
}
}  // namespace

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    const std::vector<int>& dependent_transits,
    const RoutingDimension* base_dimension, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  const std::vector<int> pure_transits(vehicles_, /*zero_evaluator*/ 0);
  return AddDimensionDependentDimensionWithVehicleCapacity(
      pure_transits, dependent_transits, base_dimension, slack_max,
      std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    int transit, const RoutingDimension* dimension, int64 slack_max,
    int64 vehicle_capacity, bool fix_start_cumul_to_zero,
    const std::string& name) {
  return AddDimensionDependentDimensionWithVehicleCapacity(
      /*zero_evaluator*/ 0, transit, dimension, slack_max, vehicle_capacity,
      fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacityInternal(
    const std::vector<int>& pure_transits,
    const std::vector<int>& dependent_transits,
    const RoutingDimension* base_dimension, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  CHECK_EQ(vehicles_, vehicle_capacities.size());
  RoutingDimension* new_dimension = nullptr;
  if (base_dimension == nullptr) {
    new_dimension = new RoutingDimension(this, std::move(vehicle_capacities),
                                         name, RoutingDimension::SelfBased());
  } else {
    new_dimension = new RoutingDimension(this, std::move(vehicle_capacities),
                                         name, base_dimension);
  }
  return InitializeDimensionInternal(pure_transits, dependent_transits,
                                     slack_max, fix_start_cumul_to_zero,
                                     new_dimension);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    int pure_transit, int dependent_transit,
    const RoutingDimension* base_dimension, int64 slack_max,
    int64 vehicle_capacity, bool fix_start_cumul_to_zero,
    const std::string& name) {
  std::vector<int> pure_transits(vehicles_, pure_transit);
  std::vector<int> dependent_transits(vehicles_, dependent_transit);
  std::vector<int64> vehicle_capacities(vehicles_, vehicle_capacity);
  return AddDimensionDependentDimensionWithVehicleCapacityInternal(
      pure_transits, dependent_transits, base_dimension, slack_max,
      std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
}

RoutingModel::StateDependentTransit RoutingModel::MakeStateDependentTransit(
    const std::function<int64(int64)>& f, int64 domain_start,
    int64 domain_end) {
  const std::function<int64(int64)> g = [&f](int64 x) { return f(x) + x; };
  // The next line is safe, because MakeCachedIntToIntFunction does not count
  // on keeping the closure of its first argument alive.
  return {MakeCachedIntToIntFunction(f, domain_start, domain_end),
          MakeCachedRangeMinMaxIndexFunction(g, domain_start, domain_end)};
}

std::vector<std::string> RoutingModel::GetAllDimensionNames() const {
  std::vector<std::string> dimension_names;
  for (const auto& dimension_name_index : dimension_name_to_index_) {
    dimension_names.push_back(dimension_name_index.first);
  }
  std::sort(dimension_names.begin(), dimension_names.end());
  return dimension_names;
}

GlobalDimensionCumulOptimizer* RoutingModel::GetMutableGlobalCumulOptimizer(
    const RoutingDimension& dimension) const {
  const DimensionIndex dim_index = GetDimensionIndex(dimension.name());
  if (dim_index < 0 || dim_index >= global_optimizer_index_.size() ||
      global_optimizer_index_[dim_index] < 0) {
    return nullptr;
  }
  const int optimizer_index = global_optimizer_index_[dim_index];
  DCHECK_LT(optimizer_index, global_dimension_optimizers_.size());
  return global_dimension_optimizers_[optimizer_index].get();
}

LocalDimensionCumulOptimizer* RoutingModel::GetMutableLocalCumulOptimizer(
    const RoutingDimension& dimension) const {
  const DimensionIndex dim_index = GetDimensionIndex(dimension.name());
  if (dim_index < 0 || dim_index >= local_optimizer_index_.size() ||
      local_optimizer_index_[dim_index] < 0) {
    return nullptr;
  }
  const int optimizer_index = local_optimizer_index_[dim_index];
  DCHECK_LT(optimizer_index, local_dimension_optimizers_.size());
  return local_dimension_optimizers_[optimizer_index].get();
}

LocalDimensionCumulOptimizer* RoutingModel::GetMutableLocalCumulMPOptimizer(
    const RoutingDimension& dimension) const {
  const DimensionIndex dim_index = GetDimensionIndex(dimension.name());
  if (dim_index < 0 || dim_index >= local_optimizer_index_.size() ||
      local_optimizer_index_[dim_index] < 0) {
    return nullptr;
  }
  const int optimizer_index = local_optimizer_index_[dim_index];
  DCHECK_LT(optimizer_index, local_dimension_mp_optimizers_.size());
  return local_dimension_mp_optimizers_[optimizer_index].get();
}

bool RoutingModel::HasDimension(const std::string& dimension_name) const {
  return gtl::ContainsKey(dimension_name_to_index_, dimension_name);
}

RoutingModel::DimensionIndex RoutingModel::GetDimensionIndex(
    const std::string& dimension_name) const {
  return gtl::FindWithDefault(dimension_name_to_index_, dimension_name,
                              kNoDimension);
}

const RoutingDimension& RoutingModel::GetDimensionOrDie(
    const std::string& dimension_name) const {
  return *dimensions_[gtl::FindOrDie(dimension_name_to_index_, dimension_name)];
}

RoutingDimension* RoutingModel::GetMutableDimension(
    const std::string& dimension_name) const {
  const DimensionIndex index = GetDimensionIndex(dimension_name);
  if (index != kNoDimension) {
    return dimensions_[index];
  }
  return nullptr;
}

void RoutingModel::SetArcCostEvaluatorOfAllVehicles(int evaluator_index) {
  CHECK_LT(0, vehicles_);
  for (int i = 0; i < vehicles_; ++i) {
    SetArcCostEvaluatorOfVehicle(evaluator_index, i);
  }
}

void RoutingModel::SetArcCostEvaluatorOfVehicle(int evaluator_index,
                                                int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  CHECK_LT(evaluator_index, transit_evaluators_.size());
  vehicle_to_transit_cost_[vehicle] = evaluator_index;
}

void RoutingModel::SetFixedCostOfAllVehicles(int64 cost) {
  for (int i = 0; i < vehicles_; ++i) {
    SetFixedCostOfVehicle(cost, i);
  }
}

int64 RoutingModel::GetFixedCostOfVehicle(int vehicle) const {
  CHECK_LT(vehicle, vehicles_);
  return fixed_cost_of_vehicle_[vehicle];
}

void RoutingModel::SetFixedCostOfVehicle(int64 cost, int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  DCHECK_GE(cost, 0);
  fixed_cost_of_vehicle_[vehicle] = cost;
}

void RoutingModel::SetAmortizedCostFactorsOfAllVehicles(
    int64 linear_cost_factor, int64 quadratic_cost_factor) {
  for (int v = 0; v < vehicles_; v++) {
    SetAmortizedCostFactorsOfVehicle(linear_cost_factor, quadratic_cost_factor,
                                     v);
  }
}

void RoutingModel::SetAmortizedCostFactorsOfVehicle(int64 linear_cost_factor,
                                                    int64 quadratic_cost_factor,
                                                    int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  DCHECK_GE(linear_cost_factor, 0);
  DCHECK_GE(quadratic_cost_factor, 0);
  if (linear_cost_factor + quadratic_cost_factor > 0) {
    vehicle_amortized_cost_factors_set_ = true;
  }
  linear_cost_factor_of_vehicle_[vehicle] = linear_cost_factor;
  quadratic_cost_factor_of_vehicle_[vehicle] = quadratic_cost_factor;
}

namespace {
// Some C++ versions used in the open-source export don't support comparison
// functors for STL containers; so we need a comparator class instead.
struct CostClassComparator {
  bool operator()(const RoutingModel::CostClass& a,
                  const RoutingModel::CostClass& b) const {
    return RoutingModel::CostClass::LessThan(a, b);
  }
};

struct VehicleClassComparator {
  bool operator()(const RoutingModel::VehicleClass& a,
                  const RoutingModel::VehicleClass& b) const {
    return RoutingModel::VehicleClass::LessThan(a, b);
  }
};
}  // namespace

// static
const RoutingModel::CostClassIndex RoutingModel::kCostClassIndexOfZeroCost =
    CostClassIndex(0);

void RoutingModel::ComputeCostClasses(
    const RoutingSearchParameters& parameters) {
  // Create and reduce the cost classes.
  cost_classes_.reserve(vehicles_);
  cost_classes_.clear();
  cost_class_index_of_vehicle_.assign(vehicles_, CostClassIndex(-1));
  std::map<CostClass, CostClassIndex, CostClassComparator> cost_class_map;

  // Pre-insert the built-in cost class 'zero cost' with index 0.
  const CostClass zero_cost_class(0);
  cost_classes_.push_back(zero_cost_class);
  DCHECK_EQ(cost_classes_[kCostClassIndexOfZeroCost].evaluator_index, 0);
  cost_class_map[zero_cost_class] = kCostClassIndexOfZeroCost;

  // Determine the canonicalized cost class for each vehicle, and insert it as
  // a new cost class if it doesn't exist already. Building cached evaluators
  // on the way.
  has_vehicle_with_zero_cost_class_ = false;
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    CostClass cost_class(vehicle_to_transit_cost_[vehicle]);

    // Insert the dimension data in a canonical way.
    for (const RoutingDimension* const dimension : dimensions_) {
      const int64 coeff = dimension->vehicle_span_cost_coefficients()[vehicle];
      if (coeff == 0) continue;
      cost_class.dimension_transit_evaluator_class_and_cost_coefficient
          .push_back({dimension->vehicle_to_class(vehicle), coeff, dimension});
    }
    std::sort(cost_class.dimension_transit_evaluator_class_and_cost_coefficient
                  .begin(),
              cost_class.dimension_transit_evaluator_class_and_cost_coefficient
                  .end());
    // Try inserting the CostClass, if it's not already present.
    const CostClassIndex num_cost_classes(cost_classes_.size());
    const CostClassIndex cost_class_index =
        gtl::LookupOrInsert(&cost_class_map, cost_class, num_cost_classes);
    if (cost_class_index == kCostClassIndexOfZeroCost) {
      has_vehicle_with_zero_cost_class_ = true;
    } else if (cost_class_index == num_cost_classes) {  // New cost class.
      cost_classes_.push_back(cost_class);
    }
    cost_class_index_of_vehicle_[vehicle] = cost_class_index;
  }

  // TRICKY:
  // If some vehicle had the "zero" cost class, then we'll have homogeneous
  // vehicles iff they all have that cost class (i.e. cost class count = 1).
  // If none of them have it, then we have homogeneous costs iff there are two
  // cost classes: the unused "zero" cost class and the one used by all
  // vehicles.
  // Note that we always need the zero cost class, even if no vehicle uses it,
  // because we use it in the vehicle_var = -1 scenario (i.e. unperformed).
  //
  // Fixed costs are simply ignored for computing these cost classes. They are
  // attached to start nodes directly.
  costs_are_homogeneous_across_vehicles_ &= has_vehicle_with_zero_cost_class_
                                                ? GetCostClassesCount() == 1
                                                : GetCostClassesCount() <= 2;
}

bool RoutingModel::VehicleClass::LessThan(const VehicleClass& a,
                                          const VehicleClass& b) {
  return std::tie(a.cost_class_index, a.fixed_cost, a.start_equivalence_class,
                  a.end_equivalence_class, a.unvisitable_nodes_fprint,
                  a.dimension_start_cumuls_min, a.dimension_start_cumuls_max,
                  a.dimension_end_cumuls_min, a.dimension_end_cumuls_max,
                  a.dimension_capacities, a.dimension_evaluator_classes) <
         std::tie(b.cost_class_index, b.fixed_cost, b.start_equivalence_class,
                  b.end_equivalence_class, b.unvisitable_nodes_fprint,
                  b.dimension_start_cumuls_min, b.dimension_start_cumuls_max,
                  b.dimension_end_cumuls_min, b.dimension_end_cumuls_max,
                  b.dimension_capacities, b.dimension_evaluator_classes);
}

void RoutingModel::ComputeVehicleClasses() {
  vehicle_classes_.reserve(vehicles_);
  vehicle_classes_.clear();
  vehicle_class_index_of_vehicle_.assign(vehicles_, VehicleClassIndex(-1));
  std::map<VehicleClass, VehicleClassIndex, VehicleClassComparator>
      vehicle_class_map;
  const int nodes_unvisitability_num_bytes = (vehicle_vars_.size() + 7) / 8;
  std::unique_ptr<char[]> nodes_unvisitability_bitmask(
      new char[nodes_unvisitability_num_bytes]);
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    VehicleClass vehicle_class;
    vehicle_class.cost_class_index = cost_class_index_of_vehicle_[vehicle];
    vehicle_class.fixed_cost = fixed_cost_of_vehicle_[vehicle];
    vehicle_class.start_equivalence_class =
        index_to_equivalence_class_[Start(vehicle)];
    vehicle_class.end_equivalence_class =
        index_to_equivalence_class_[End(vehicle)];
    for (const RoutingDimension* const dimension : dimensions_) {
      IntVar* const start_cumul_var = dimension->cumuls()[Start(vehicle)];
      vehicle_class.dimension_start_cumuls_min.push_back(
          start_cumul_var->Min());
      vehicle_class.dimension_start_cumuls_max.push_back(
          start_cumul_var->Max());
      IntVar* const end_cumul_var = dimension->cumuls()[End(vehicle)];
      vehicle_class.dimension_end_cumuls_min.push_back(end_cumul_var->Min());
      vehicle_class.dimension_end_cumuls_max.push_back(end_cumul_var->Max());
      vehicle_class.dimension_capacities.push_back(
          dimension->vehicle_capacities()[vehicle]);
      vehicle_class.dimension_evaluator_classes.push_back(
          dimension->vehicle_to_class(vehicle));
    }
    memset(nodes_unvisitability_bitmask.get(), 0,
           nodes_unvisitability_num_bytes);
    for (int index = 0; index < vehicle_vars_.size(); ++index) {
      IntVar* const vehicle_var = vehicle_vars_[index];
      if (!IsStart(index) && !IsEnd(index) && !vehicle_var->Contains(vehicle)) {
        nodes_unvisitability_bitmask[index / CHAR_BIT] |= 1U
                                                          << (index % CHAR_BIT);
      }
    }
    vehicle_class.unvisitable_nodes_fprint = ThoroughHash(
        nodes_unvisitability_bitmask.get(), nodes_unvisitability_num_bytes);
    const VehicleClassIndex num_vehicle_classes(vehicle_classes_.size());
    const VehicleClassIndex vehicle_class_index = gtl::LookupOrInsert(
        &vehicle_class_map, vehicle_class, num_vehicle_classes);
    if (vehicle_class_index == num_vehicle_classes) {  // New vehicle class
      vehicle_classes_.push_back(vehicle_class);
    }
    vehicle_class_index_of_vehicle_[vehicle] = vehicle_class_index;
  }
}

void RoutingModel::ComputeVehicleTypes() {
  const int nodes_squared = nodes_ * nodes_;
  std::vector<int>& type_index_of_vehicle =
      vehicle_type_container_.type_index_of_vehicle;
  std::vector<std::set<VehicleTypeContainer::VehicleClassEntry>>&
      sorted_vehicle_classes_per_type =
          vehicle_type_container_.sorted_vehicle_classes_per_type;
  std::vector<std::deque<int>>& vehicles_per_vehicle_class =
      vehicle_type_container_.vehicles_per_vehicle_class;

  type_index_of_vehicle.resize(vehicles_);
  sorted_vehicle_classes_per_type.clear();
  sorted_vehicle_classes_per_type.reserve(vehicles_);
  vehicles_per_vehicle_class.clear();
  vehicles_per_vehicle_class.resize(GetVehicleClassesCount());

  absl::flat_hash_map<int64, int> type_to_type_index;

  for (int v = 0; v < vehicles_; v++) {
    const int start = manager_.IndexToNode(Start(v)).value();
    const int end = manager_.IndexToNode(End(v)).value();
    const int cost_class = GetCostClassIndexOfVehicle(v).value();
    const int64 type = cost_class * nodes_squared + start * nodes_ + end;

    const auto& vehicle_type_added = type_to_type_index.insert(
        std::make_pair(type, type_to_type_index.size()));

    const int index = vehicle_type_added.first->second;

    const int vehicle_class = GetVehicleClassIndexOfVehicle(v).value();
    const VehicleTypeContainer::VehicleClassEntry class_entry = {
        vehicle_class, GetFixedCostOfVehicle(v)};

    if (vehicle_type_added.second) {
      // Type was not indexed yet.
      DCHECK_EQ(sorted_vehicle_classes_per_type.size(), index);
      sorted_vehicle_classes_per_type.push_back({class_entry});
    } else {
      // Type already indexed.
      DCHECK_LT(index, sorted_vehicle_classes_per_type.size());
      sorted_vehicle_classes_per_type[index].insert(class_entry);
    }
    vehicles_per_vehicle_class[vehicle_class].push_back(v);
    type_index_of_vehicle[v] = index;
  }
}

void RoutingModel::FinalizeVisitTypes() {
  // NOTE(user): This is necessary if CloseVisitTypes() was not called
  // explicitly before. This will be removed when the TODO regarding this logic
  // is addressed.
  CloseVisitTypes();

  single_nodes_of_type_.clear();
  single_nodes_of_type_.resize(num_visit_types_);
  pair_indices_of_type_.clear();
  pair_indices_of_type_.resize(num_visit_types_);
  std::vector<absl::flat_hash_set<int>> pair_indices_added_for_type(
      num_visit_types_);

  for (int index = 0; index < index_to_visit_type_.size(); index++) {
    const int visit_type = GetVisitType(index);
    if (visit_type < 0) {
      continue;
    }
    const std::vector<std::pair<int, int>>& pickup_index_pairs =
        index_to_pickup_index_pairs_[index];
    const std::vector<std::pair<int, int>>& delivery_index_pairs =
        index_to_delivery_index_pairs_[index];
    if (pickup_index_pairs.empty() && delivery_index_pairs.empty()) {
      single_nodes_of_type_[visit_type].push_back(index);
    }
    for (const std::vector<std::pair<int, int>>* index_pairs :
         {&pickup_index_pairs, &delivery_index_pairs}) {
      for (const std::pair<int, int>& index_pair : *index_pairs) {
        const int pair_index = index_pair.first;
        if (pair_indices_added_for_type[visit_type].insert(pair_index).second) {
          pair_indices_of_type_[visit_type].push_back(pair_index);
        }
      }
    }
  }

  std::vector<std::pair<int, int>> requirement_arcs;
  for (int type = 0; type < num_visit_types_; type++) {
    for (const std::vector<absl::flat_hash_set<int>>*
             required_type_alternatives :
         {&required_type_alternatives_when_adding_type_index_[type],
          &required_type_alternatives_when_removing_type_index_[type],
          &same_vehicle_required_type_alternatives_per_type_index_[type]}) {
      for (const absl::flat_hash_set<int>& alternatives :
           *required_type_alternatives) {
        for (int required_type : alternatives) {
          requirement_arcs.emplace_back(required_type, type);
        }
      }
    }
  }
  if (requirement_arcs.empty()) return;
  if (!util::DenseIntTopologicalSort(num_visit_types_, requirement_arcs,
                                     &topologically_sorted_visit_types_)) {
    topologically_sorted_visit_types_.clear();
  }
}

RoutingModel::DisjunctionIndex RoutingModel::AddDisjunction(
    const std::vector<int64>& indices, int64 penalty, int64 max_cardinality) {
  CHECK_GE(max_cardinality, 1);
  for (int i = 0; i < indices.size(); ++i) {
    CHECK_NE(kUnassigned, indices[i]);
  }

  const DisjunctionIndex disjunction_index(disjunctions_.size());
  disjunctions_.push_back({indices, {penalty, max_cardinality}});
  for (const int64 index : indices) {
    index_to_disjunctions_[index].push_back(disjunction_index);
  }
  return disjunction_index;
}

std::vector<std::pair<int64, int64>>
RoutingModel::GetPerfectBinaryDisjunctions() const {
  std::vector<std::pair<int64, int64>> var_index_pairs;
  for (const Disjunction& disjunction : disjunctions_) {
    const std::vector<int64>& var_indices = disjunction.indices;
    if (var_indices.size() != 2) continue;
    const int64 v0 = var_indices[0];
    const int64 v1 = var_indices[1];
    if (index_to_disjunctions_[v0].size() == 1 &&
        index_to_disjunctions_[v1].size() == 1) {
      // We output sorted pairs.
      var_index_pairs.push_back({std::min(v0, v1), std::max(v0, v1)});
    }
  }
  std::sort(var_index_pairs.begin(), var_index_pairs.end());
  return var_index_pairs;
}

void RoutingModel::IgnoreDisjunctionsAlreadyForcedToZero() {
  CHECK(!closed_);
  for (Disjunction& disjunction : disjunctions_) {
    bool has_one_potentially_active_var = false;
    for (const int64 var_index : disjunction.indices) {
      if (ActiveVar(var_index)->Max() > 0) {
        has_one_potentially_active_var = true;
        break;
      }
    }
    if (!has_one_potentially_active_var) {
      disjunction.value.max_cardinality = 0;
    }
  }
}

IntVar* RoutingModel::CreateDisjunction(DisjunctionIndex disjunction) {
  const std::vector<int64>& indices = disjunctions_[disjunction].indices;
  const int indices_size = indices.size();
  std::vector<IntVar*> disjunction_vars(indices_size);
  for (int i = 0; i < indices_size; ++i) {
    const int64 index = indices[i];
    CHECK_LT(index, Size());
    disjunction_vars[i] = ActiveVar(index);
  }
  const int64 max_cardinality =
      disjunctions_[disjunction].value.max_cardinality;
  IntVar* no_active_var = solver_->MakeBoolVar();
  IntVar* number_active_vars = solver_->MakeIntVar(0, max_cardinality);
  solver_->AddConstraint(
      solver_->MakeSumEquality(disjunction_vars, number_active_vars));
  solver_->AddConstraint(solver_->MakeIsDifferentCstCt(
      number_active_vars, max_cardinality, no_active_var));
  const int64 penalty = disjunctions_[disjunction].value.penalty;
  if (penalty < 0) {
    no_active_var->SetMax(0);
    return nullptr;
  } else {
    return solver_->MakeProd(no_active_var, penalty)->Var();
  }
}

void RoutingModel::AddSoftSameVehicleConstraint(
    const std::vector<int64>& indices, int64 cost) {
  if (!indices.empty()) {
    ValuedNodes<int64> same_vehicle_cost;
    for (const int64 index : indices) {
      same_vehicle_cost.indices.push_back(index);
    }
    same_vehicle_cost.value = cost;
    same_vehicle_costs_.push_back(same_vehicle_cost);
  }
}

void RoutingModel::SetAllowedVehiclesForIndex(const std::vector<int>& vehicles,
                                              int64 index) {
  auto& allowed_vehicles = allowed_vehicles_[index];
  allowed_vehicles.clear();
  for (int vehicle : vehicles) {
    allowed_vehicles.insert(vehicle);
  }
}

void RoutingModel::AddPickupAndDelivery(int64 pickup, int64 delivery) {
  AddPickupAndDeliverySetsInternal({pickup}, {delivery});
  pickup_delivery_disjunctions_.push_back({kNoDisjunction, kNoDisjunction});
}

void RoutingModel::AddPickupAndDeliverySets(
    DisjunctionIndex pickup_disjunction,
    DisjunctionIndex delivery_disjunction) {
  AddPickupAndDeliverySetsInternal(GetDisjunctionIndices(pickup_disjunction),
                                   GetDisjunctionIndices(delivery_disjunction));
  pickup_delivery_disjunctions_.push_back(
      {pickup_disjunction, delivery_disjunction});
}

void RoutingModel::AddPickupAndDeliverySetsInternal(
    const std::vector<int64>& pickups, const std::vector<int64>& deliveries) {
  if (pickups.empty() || deliveries.empty()) {
    return;
  }
  const int64 size = Size();
  const int pair_index = pickup_delivery_pairs_.size();
  for (int pickup_index = 0; pickup_index < pickups.size(); pickup_index++) {
    const int64 pickup = pickups[pickup_index];
    CHECK_LT(pickup, size);
    index_to_pickup_index_pairs_[pickup].emplace_back(pair_index, pickup_index);
  }
  for (int delivery_index = 0; delivery_index < deliveries.size();
       delivery_index++) {
    const int64 delivery = deliveries[delivery_index];
    CHECK_LT(delivery, size);
    index_to_delivery_index_pairs_[delivery].emplace_back(pair_index,
                                                          delivery_index);
  }
  pickup_delivery_pairs_.push_back({pickups, deliveries});
}

const std::vector<std::pair<int, int>>& RoutingModel::GetPickupIndexPairs(
    int64 node_index) const {
  CHECK_LT(node_index, index_to_pickup_index_pairs_.size());
  return index_to_pickup_index_pairs_[node_index];
}

const std::vector<std::pair<int, int>>& RoutingModel::GetDeliveryIndexPairs(
    int64 node_index) const {
  CHECK_LT(node_index, index_to_delivery_index_pairs_.size());
  return index_to_delivery_index_pairs_[node_index];
}

void RoutingModel::SetPickupAndDeliveryPolicyOfVehicle(
    PickupAndDeliveryPolicy policy, int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  vehicle_pickup_delivery_policy_[vehicle] = policy;
}

void RoutingModel::SetPickupAndDeliveryPolicyOfAllVehicles(
    PickupAndDeliveryPolicy policy) {
  CHECK_LT(0, vehicles_);
  for (int i = 0; i < vehicles_; ++i) {
    SetPickupAndDeliveryPolicyOfVehicle(policy, i);
  }
}

RoutingModel::PickupAndDeliveryPolicy
RoutingModel::GetPickupAndDeliveryPolicyOfVehicle(int vehicle) const {
  CHECK_LT(vehicle, vehicles_);
  return vehicle_pickup_delivery_policy_[vehicle];
}

int RoutingModel::GetNumOfSingletonNodes() const {
  int count = 0;
  for (int i = 0; i < Nexts().size(); ++i) {
    // End nodes have no next variables.
    if (!IsStart(i) && GetPickupIndexPairs(i).empty() &&
        GetDeliveryIndexPairs(i).empty()) {
      ++count;
    }
  }
  return count;
}

IntVar* RoutingModel::CreateSameVehicleCost(int vehicle_index) {
  const std::vector<int64>& indices =
      same_vehicle_costs_[vehicle_index].indices;
  CHECK(!indices.empty());
  std::vector<IntVar*> vehicle_counts;
  solver_->MakeIntVarArray(vehicle_vars_.size() + 1, 0, indices.size() + 1,
                           &vehicle_counts);
  std::vector<int64> vehicle_values(vehicle_vars_.size() + 1);
  for (int i = 0; i < vehicle_vars_.size(); ++i) {
    vehicle_values[i] = i;
  }
  vehicle_values[vehicle_vars_.size()] = -1;
  std::vector<IntVar*> vehicle_vars;
  vehicle_vars.reserve(indices.size());
  for (const int64 index : indices) {
    vehicle_vars.push_back(vehicle_vars_[index]);
  }
  solver_->AddConstraint(solver_->MakeDistribute(vehicle_vars, vehicle_counts));
  std::vector<IntVar*> vehicle_used;
  for (int i = 0; i < vehicle_vars_.size() + 1; ++i) {
    vehicle_used.push_back(
        solver_->MakeIsGreaterOrEqualCstVar(vehicle_counts[i], 1));
  }
  vehicle_used.push_back(solver_->MakeIntConst(-1));
  return solver_
      ->MakeProd(solver_->MakeMax(solver_->MakeSum(vehicle_used), 0),
                 same_vehicle_costs_[vehicle_index].value)
      ->Var();
}

void RoutingModel::AddLocalSearchOperator(LocalSearchOperator* ls_operator) {
  extra_operators_.push_back(ls_operator);
}

int64 RoutingModel::GetDepot() const { return vehicles() > 0 ? Start(0) : -1; }

// TODO(user): Remove the need for the homogeneous version once the
// vehicle var to cost class element constraint is fast enough.
void RoutingModel::AppendHomogeneousArcCosts(
    const RoutingSearchParameters& parameters, int node_index,
    std::vector<IntVar*>* cost_elements) {
  CHECK(cost_elements != nullptr);
  const auto arc_cost_evaluator = [this, node_index](int64 next_index) {
    return GetHomogeneousCost(node_index, next_index);
  };
  if (UsesLightPropagation(parameters)) {
    // Only supporting positive costs.
    // TODO(user): Detect why changing lower bound to kint64min stalls
    // the search in GLS in some cases (Solomon instances for instance).
    IntVar* const base_cost_var = solver_->MakeIntVar(0, kint64max);
    solver_->AddConstraint(MakeLightElement(
        solver_.get(), base_cost_var, nexts_[node_index], arc_cost_evaluator,
        [this]() { return enable_deep_serialization_; }));
    IntVar* const var =
        solver_->MakeProd(base_cost_var, active_[node_index])->Var();
    cost_elements->push_back(var);
  } else {
    IntExpr* const expr =
        solver_->MakeElement(arc_cost_evaluator, nexts_[node_index]);
    IntVar* const var = solver_->MakeProd(expr, active_[node_index])->Var();
    cost_elements->push_back(var);
  }
}

void RoutingModel::AppendArcCosts(const RoutingSearchParameters& parameters,
                                  int node_index,
                                  std::vector<IntVar*>* cost_elements) {
  CHECK(cost_elements != nullptr);
  DCHECK_GT(vehicles_, 0);
  if (UsesLightPropagation(parameters)) {
    // Only supporting positive costs.
    // TODO(user): Detect why changing lower bound to kint64min stalls
    // the search in GLS in some cases (Solomon instances for instance).
    IntVar* const base_cost_var = solver_->MakeIntVar(0, kint64max);
    solver_->AddConstraint(MakeLightElement2(
        solver_.get(), base_cost_var, nexts_[node_index],
        vehicle_vars_[node_index],
        [this, node_index](int64 to, int64 vehicle) {
          return GetArcCostForVehicle(node_index, to, vehicle);
        },
        [this]() { return enable_deep_serialization_; }));
    IntVar* const var =
        solver_->MakeProd(base_cost_var, active_[node_index])->Var();
    cost_elements->push_back(var);
  } else {
    IntVar* const vehicle_class_var =
        solver_
            ->MakeElement(
                [this](int64 index) {
                  return SafeGetCostClassInt64OfVehicle(index);
                },
                vehicle_vars_[node_index])
            ->Var();
    IntExpr* const expr = solver_->MakeElement(
        [this, node_index](int64 next, int64 vehicle_class) {
          return GetArcCostForClass(node_index, next, vehicle_class);
        },
        nexts_[node_index], vehicle_class_var);
    IntVar* const var = solver_->MakeProd(expr, active_[node_index])->Var();
    cost_elements->push_back(var);
  }
}

int RoutingModel::GetVehicleStartClass(int64 start_index) const {
  const int vehicle = index_to_vehicle_[start_index];
  if (vehicle != kUnassigned) {
    return GetVehicleClassIndexOfVehicle(vehicle).value();
  }
  return kUnassigned;
}

std::string RoutingModel::FindErrorInSearchParametersForModel(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  if (GetFirstSolutionDecisionBuilder(search_parameters) == nullptr) {
    return absl::StrCat(
        "Undefined first solution strategy: ",
        FirstSolutionStrategy::Value_Name(first_solution_strategy),
        " (int value: ", first_solution_strategy, ")");
  }
  if (search_parameters.first_solution_strategy() ==
          FirstSolutionStrategy::SWEEP &&
      sweep_arranger() == nullptr) {
    return "Undefined sweep arranger for ROUTING_SWEEP strategy.";
  }
  return "";
}

void RoutingModel::QuietCloseModel() {
  QuietCloseModelWithParameters(DefaultRoutingSearchParameters());
}

void RoutingModel::CloseModel() {
  CloseModelWithParameters(DefaultRoutingSearchParameters());
}

class RoutingModelInspector : public ModelVisitor {
 public:
  explicit RoutingModelInspector(RoutingModel* model) : model_(model) {
    same_vehicle_components_.Init(model->Size());
    for (const std::string& name : model->GetAllDimensionNames()) {
      RoutingDimension* const dimension = model->GetMutableDimension(name);
      const std::vector<IntVar*>& cumuls = dimension->cumuls();
      for (int i = 0; i < cumuls.size(); ++i) {
        cumul_to_dim_indices_[cumuls[i]] = {dimension, i};
      }
    }
    const std::vector<IntVar*>& vehicle_vars = model->VehicleVars();
    for (int i = 0; i < vehicle_vars.size(); ++i) {
      vehicle_var_to_indices_[vehicle_vars[i]] = i;
    }
    RegisterInspectors();
  }
  ~RoutingModelInspector() override {}
  void EndVisitModel(const std::string& solver_name) override {
    // Compact same vehicle component indices.
    absl::flat_hash_map<int, int> component_indices;
    int component_index = 0;
    for (int node = 0; node < model_->Size(); ++node) {
      const int component =
          same_vehicle_components_.GetClassRepresentative(node);
      if (gtl::InsertIfNotPresent(&component_indices, component,
                                  component_index)) {
        ++component_index;
      }
    }
    model_->InitSameVehicleGroups(component_indices.size());
    for (int node = 0; node < model_->Size(); ++node) {
      const int component =
          same_vehicle_components_.GetClassRepresentative(node);
      DCHECK(gtl::ContainsKey(component_indices, component));
      model_->SetSameVehicleGroup(
          node, gtl::FindWithDefault(component_indices, component, 0));
    }
    // TODO(user): Perform transitive closure of dimension precedence graphs.
    // TODO(user): Have a single annotated precedence graph.
  }
  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override {
    gtl::FindWithDefault(constraint_inspectors_, type_name, []() {})();
  }
  void VisitIntegerExpressionArgument(const std::string& type_name,
                                      IntExpr* const expr) override {
    gtl::FindWithDefault(expr_inspectors_, type_name,
                         [](const IntExpr* expr) {})(expr);
  }
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64>& values) override {
    gtl::FindWithDefault(array_inspectors_, arg_name,
                         [](const std::vector<int64>& int_array) {})(values);
  }

 private:
  using ExprInspector = std::function<void(const IntExpr*)>;
  using ArrayInspector = std::function<void(const std::vector<int64>&)>;
  using ConstraintInspector = std::function<void()>;

  void RegisterInspectors() {
    expr_inspectors_[kExpressionArgument] = [this](const IntExpr* expr) {
      expr_ = expr;
    };
    expr_inspectors_[kLeftArgument] = [this](const IntExpr* expr) {
      left_ = expr;
    };
    expr_inspectors_[kRightArgument] = [this](const IntExpr* expr) {
      right_ = expr;
    };
    array_inspectors_[kStartsArgument] =
        [this](const std::vector<int64>& int_array) {
          starts_argument_ = int_array;
        };
    array_inspectors_[kEndsArgument] =
        [this](const std::vector<int64>& int_array) {
          ends_argument_ = int_array;
        };
    constraint_inspectors_[kNotMember] = [this]() {
      std::pair<RoutingDimension*, int> dim_index;
      if (gtl::FindCopy(cumul_to_dim_indices_, expr_, &dim_index)) {
        RoutingDimension* const dimension = dim_index.first;
        const int index = dim_index.second;
        dimension->forbidden_intervals_[index].InsertIntervals(starts_argument_,
                                                               ends_argument_);
        VLOG(2) << dimension->name() << " " << index << ": "
                << dimension->forbidden_intervals_[index].DebugString();
      }
      expr_ = nullptr;
      starts_argument_.clear();
      ends_argument_.clear();
    };
    constraint_inspectors_[kEquality] = [this]() {
      int left_index = 0;
      int right_index = 0;
      if (gtl::FindCopy(vehicle_var_to_indices_, left_, &left_index) &&
          gtl::FindCopy(vehicle_var_to_indices_, right_, &right_index)) {
        VLOG(2) << "Vehicle variables for " << left_index << " and "
                << right_index << " are equal.";
        same_vehicle_components_.AddArc(left_index, right_index);
      }
      left_ = nullptr;
      right_ = nullptr;
    };
    constraint_inspectors_[kLessOrEqual] = [this]() {
      std::pair<RoutingDimension*, int> left_index;
      std::pair<RoutingDimension*, int> right_index;
      if (gtl::FindCopy(cumul_to_dim_indices_, left_, &left_index) &&
          gtl::FindCopy(cumul_to_dim_indices_, right_, &right_index)) {
        RoutingDimension* const dimension = left_index.first;
        if (dimension == right_index.first) {
          VLOG(2) << "For dimension " << dimension->name() << ", cumul for "
                  << left_index.second << " is less than " << right_index.second
                  << ".";
          dimension->path_precedence_graph_.AddArc(left_index.second,
                                                   right_index.second);
        }
      }
      left_ = nullptr;
      right_ = nullptr;
    };
  }

  RoutingModel* const model_;
  ConnectedComponents<int, int> same_vehicle_components_;
  absl::flat_hash_map<const IntExpr*, std::pair<RoutingDimension*, int>>
      cumul_to_dim_indices_;
  absl::flat_hash_map<const IntExpr*, int> vehicle_var_to_indices_;
  absl::flat_hash_map<std::string, ExprInspector> expr_inspectors_;
  absl::flat_hash_map<std::string, ArrayInspector> array_inspectors_;
  absl::flat_hash_map<std::string, ConstraintInspector> constraint_inspectors_;
  const IntExpr* expr_ = nullptr;
  const IntExpr* left_ = nullptr;
  const IntExpr* right_ = nullptr;
  std::vector<int64> starts_argument_;
  std::vector<int64> ends_argument_;
};

void RoutingModel::CloseModelWithParameters(
    const RoutingSearchParameters& parameters) {
  std::string error = FindErrorInRoutingSearchParameters(parameters);
  if (!error.empty()) {
    status_ = ROUTING_INVALID;
    LOG(ERROR) << "Invalid RoutingSearchParameters: " << error;
    return;
  }
  if (closed_) {
    LOG(WARNING) << "Model already closed";
    return;
  }
  closed_ = true;

  for (RoutingDimension* const dimension : dimensions_) {
    dimension->CloseModel(UsesLightPropagation(parameters));
  }
  ComputeCostClasses(parameters);
  ComputeVehicleClasses();
  ComputeVehicleTypes();
  FinalizeVisitTypes();
  vehicle_start_class_callback_ = [this](int64 start) {
    return GetVehicleStartClass(start);
  };

  AddNoCycleConstraintInternal();

  const int size = Size();

  // Vehicle variable constraints
  for (int i = 0; i < vehicles_; ++i) {
    const int64 start = starts_[i];
    const int64 end = ends_[i];
    solver_->AddConstraint(
        solver_->MakeEquality(vehicle_vars_[start], solver_->MakeIntConst(i)));
    solver_->AddConstraint(
        solver_->MakeEquality(vehicle_vars_[end], solver_->MakeIntConst(i)));
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(nexts_[start], end, vehicle_active_[i]));
    if (consider_empty_route_costs_[i]) {
      vehicle_costs_considered_[i]->SetMin(1);
    } else {
      solver_->AddConstraint(solver_->MakeEquality(
          vehicle_active_[i], vehicle_costs_considered_[i]));
    }
  }

  // Limit the number of vehicles with non-empty routes.
  if (vehicles_ > max_active_vehicles_) {
    solver_->AddConstraint(
        solver_->MakeSumLessOrEqual(vehicle_active_, max_active_vehicles_));
  }

  // If there is only one vehicle in the model the vehicle variables will have
  // a maximum domain of [-1, 0]. If a node is performed/active then its vehicle
  // variable will be reduced to [0] making the path-cumul constraint below
  // useless. If the node is unperformed/unactive then its vehicle variable will
  // be reduced to [-1] in any case.
  if (vehicles_ > 1) {
    std::vector<IntVar*> zero_transit(size, solver_->MakeIntConst(0));
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, vehicle_vars_, zero_transit));
  }

  // Nodes which are not in a disjunction are mandatory, and those with a
  // trivially infeasible type are necessarily unperformed
  for (int i = 0; i < size; ++i) {
    if (GetDisjunctionIndices(i).empty() && active_[i]->Max() != 0) {
      active_[i]->SetValue(1);
    }
    const int type = GetVisitType(i);
    if (type == kUnassigned) {
      continue;
    }
    const absl::flat_hash_set<VisitTypePolicy>* const infeasible_policies =
        gtl::FindOrNull(trivially_infeasible_visit_types_to_policies_, type);
    if (infeasible_policies != nullptr &&
        gtl::ContainsKey(*infeasible_policies, index_to_type_policy_[i])) {
      active_[i]->SetValue(0);
    }
  }

  // Reduce domains of vehicle variables
  for (int i = 0; i < allowed_vehicles_.size(); ++i) {
    const auto& allowed_vehicles = allowed_vehicles_[i];
    if (!allowed_vehicles.empty()) {
      std::vector<int64> vehicles;
      vehicles.reserve(allowed_vehicles.size() + 1);
      vehicles.push_back(-1);
      for (int vehicle : allowed_vehicles) {
        vehicles.push_back(vehicle);
      }
      solver_->AddConstraint(solver_->MakeMemberCt(VehicleVar(i), vehicles));
    }
  }

  // Reduce domain of next variables.
  for (int i = 0; i < size; ++i) {
    // No variable can point back to a start.
    solver_->AddConstraint(solver_->RevAlloc(
        new DifferentFromValues(solver_.get(), nexts_[i], starts_)));
    // Extra constraint to state an active node can't point to itself.
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(nexts_[i], i, active_[i]));
  }

  // Add constraints to bind vehicle_vars_[i] to -1 in case that node i is not
  // active.
  for (int i = 0; i < size; ++i) {
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(vehicle_vars_[i], -1, active_[i]));
  }

  if (HasTypeRegulations()) {
    solver_->AddConstraint(
        solver_->RevAlloc(new TypeRegulationsConstraint(*this)));
  }

  // Associate first and "logical" last nodes
  for (int i = 0; i < vehicles_; ++i) {
    std::vector<int64> forbidden_ends;
    forbidden_ends.reserve(vehicles_ - 1);
    for (int j = 0; j < vehicles_; ++j) {
      if (i != j) {
        forbidden_ends.push_back(ends_[j]);
      }
    }
    solver_->AddConstraint(solver_->RevAlloc(new DifferentFromValues(
        solver_.get(), nexts_[starts_[i]], std::move(forbidden_ends))));
  }

  // Constraining is_bound_to_end_ variables.
  for (const int64 end : ends_) {
    is_bound_to_end_[end]->SetValue(1);
  }

  std::vector<IntVar*> cost_elements;
  // Arc and dimension costs.
  if (vehicles_ > 0) {
    for (int node_index = 0; node_index < size; ++node_index) {
      if (CostsAreHomogeneousAcrossVehicles()) {
        AppendHomogeneousArcCosts(parameters, node_index, &cost_elements);
      } else {
        AppendArcCosts(parameters, node_index, &cost_elements);
      }
    }
    if (vehicle_amortized_cost_factors_set_) {
      std::vector<IntVar*> route_lengths;
      solver_->MakeIntVarArray(vehicles_, 0, size, &route_lengths);
      solver_->AddConstraint(
          solver_->MakeDistribute(vehicle_vars_, route_lengths));
      std::vector<IntVar*> vehicle_used;
      for (int i = 0; i < vehicles_; i++) {
        // The start/end of the vehicle are always on the route.
        vehicle_used.push_back(
            solver_->MakeIsGreaterCstVar(route_lengths[i], 2));
        IntVar* const var =
            solver_
                ->MakeProd(solver_->MakeOpposite(solver_->MakeSquare(
                               solver_->MakeSum(route_lengths[i], -2))),
                           quadratic_cost_factor_of_vehicle_[i])
                ->Var();
        cost_elements.push_back(var);
      }
      IntVar* const vehicle_usage_cost =
          solver_->MakeScalProd(vehicle_used, linear_cost_factor_of_vehicle_)
              ->Var();
      cost_elements.push_back(vehicle_usage_cost);
    }
  }
  // Dimension span constraints: cost and limits.
  for (const RoutingDimension* dimension : dimensions_) {
    dimension->SetupGlobalSpanCost(&cost_elements);
    dimension->SetupSlackAndDependentTransitCosts();
    const std::vector<int64>& span_costs =
        dimension->vehicle_span_cost_coefficients();
    const std::vector<int64>& span_ubs = dimension->vehicle_span_upper_bounds();
    const bool has_span_constraint =
        std::any_of(span_costs.begin(), span_costs.end(),
                    [](int64 coeff) { return coeff != 0; }) ||
        std::any_of(span_ubs.begin(), span_ubs.end(),
                    [](int64 value) { return value < kint64max; }) ||
        dimension->HasSoftSpanUpperBounds() ||
        dimension->HasQuadraticCostSoftSpanUpperBounds();
    if (has_span_constraint) {
      std::vector<IntVar*> spans(vehicles(), nullptr);
      std::vector<IntVar*> total_slacks(vehicles(), nullptr);
      // Generate variables only where needed.
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (span_ubs[vehicle] < kint64max) {
          spans[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle], "");
        }
        if (span_costs[vehicle] != 0) {
          total_slacks[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle], "");
        }
      }
      if (dimension->HasSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          if (spans[vehicle]) continue;
          const SimpleBoundCosts::BoundCost bound_cost =
              dimension->GetSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0) continue;
          spans[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle]);
        }
      }
      if (dimension->HasQuadraticCostSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          if (spans[vehicle]) continue;
          const SimpleBoundCosts::BoundCost bound_cost =
              dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0) continue;
          spans[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle]);
        }
      }
      solver_->AddConstraint(
          MakePathSpansAndTotalSlacks(dimension, spans, total_slacks));
      // If a vehicle's span is constrained, its start/end cumuls must be
      // instantiated.
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (!spans[vehicle] && !total_slacks[vehicle]) continue;
        if (spans[vehicle]) {
          AddVariableTargetToFinalizer(spans[vehicle], kint64min);
        }
        AddVariableTargetToFinalizer(dimension->CumulVar(End(vehicle)),
                                     kint64min);
        AddVariableTargetToFinalizer(dimension->CumulVar(Start(vehicle)),
                                     kint64max);
      }
      // Add costs of variables.
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (span_costs[vehicle] == 0) continue;
        DCHECK(total_slacks[vehicle] != nullptr);
        IntVar* const slack_amount =
            solver_
                ->MakeProd(vehicle_costs_considered_[vehicle],
                           total_slacks[vehicle])
                ->Var();
        IntVar* const slack_cost =
            solver_->MakeProd(slack_amount, span_costs[vehicle])->Var();
        cost_elements.push_back(slack_cost);
        AddWeightedVariableMinimizedByFinalizer(slack_amount,
                                                span_costs[vehicle]);
      }
      if (dimension->HasSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          const auto bound_cost =
              dimension->GetSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0 || bound_cost.bound == kint64max) continue;
          DCHECK(spans[vehicle] != nullptr);
          // Additional cost is vehicle_cost_considered_[vehicle] *
          // max(0, spans[vehicle] - bound_cost.bound) * bound_cost.cost.
          IntVar* const span_violation_amount =
              solver_
                  ->MakeProd(
                      vehicle_costs_considered_[vehicle],
                      solver_->MakeMax(
                          solver_->MakeSum(spans[vehicle], -bound_cost.bound),
                          0))
                  ->Var();
          IntVar* const span_violation_cost =
              solver_->MakeProd(span_violation_amount, bound_cost.cost)->Var();
          cost_elements.push_back(span_violation_cost);
          AddWeightedVariableMinimizedByFinalizer(span_violation_amount,
                                                  bound_cost.cost);
        }
      }
      if (dimension->HasQuadraticCostSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          const auto bound_cost =
              dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0 || bound_cost.bound == kint64max) continue;
          DCHECK(spans[vehicle] != nullptr);
          // Additional cost is vehicle_cost_considered_[vehicle] *
          // max(0, spans[vehicle] - bound_cost.bound)^2 * bound_cost.cost.
          IntExpr* max0 = solver_->MakeMax(
              solver_->MakeSum(spans[vehicle], -bound_cost.bound), 0);
          IntVar* const squared_span_violation_amount =
              solver_
                  ->MakeProd(vehicle_costs_considered_[vehicle],
                             solver_->MakeSquare(max0))
                  ->Var();
          IntVar* const span_violation_cost =
              solver_->MakeProd(squared_span_violation_amount, bound_cost.cost)
                  ->Var();
          cost_elements.push_back(span_violation_cost);
          AddWeightedVariableMinimizedByFinalizer(squared_span_violation_amount,
                                                  bound_cost.cost);
        }
      }
    }
  }
  // Penalty costs
  for (DisjunctionIndex i(0); i < disjunctions_.size(); ++i) {
    IntVar* penalty_var = CreateDisjunction(i);
    if (penalty_var != nullptr) {
      cost_elements.push_back(penalty_var);
    }
  }
  // Soft cumul lower/upper bound costs
  for (const RoutingDimension* dimension : dimensions_) {
    dimension->SetupCumulVarSoftLowerBoundCosts(&cost_elements);
    dimension->SetupCumulVarSoftUpperBoundCosts(&cost_elements);
    dimension->SetupCumulVarPiecewiseLinearCosts(&cost_elements);
  }
  // Same vehicle costs
  for (int i = 0; i < same_vehicle_costs_.size(); ++i) {
    cost_elements.push_back(CreateSameVehicleCost(i));
  }
  cost_ = solver_->MakeSum(cost_elements)->Var();
  cost_->set_name("Cost");

  // Pickup-delivery precedences
  std::vector<std::pair<int, int>> pickup_delivery_precedences;
  for (const auto& pair : pickup_delivery_pairs_) {
    DCHECK(!pair.first.empty() && !pair.second.empty());
    for (int pickup : pair.first) {
      for (int delivery : pair.second) {
        pickup_delivery_precedences.emplace_back(pickup, delivery);
      }
    }
  }
  std::vector<int> lifo_vehicles;
  std::vector<int> fifo_vehicles;
  for (int i = 0; i < vehicles_; ++i) {
    switch (vehicle_pickup_delivery_policy_[i]) {
      case PICKUP_AND_DELIVERY_NO_ORDER:
        break;
      case PICKUP_AND_DELIVERY_LIFO:
        lifo_vehicles.push_back(Start(i));
        break;
      case PICKUP_AND_DELIVERY_FIFO:
        fifo_vehicles.push_back(Start(i));
        break;
    }
  }
  solver_->AddConstraint(solver_->MakePathPrecedenceConstraint(
      nexts_, pickup_delivery_precedences, lifo_vehicles, fifo_vehicles));

  // Detect constraints
  enable_deep_serialization_ = false;
  std::unique_ptr<RoutingModelInspector> inspector(
      new RoutingModelInspector(this));
  solver_->Accept(inspector.get());
  enable_deep_serialization_ = true;

  for (const RoutingDimension* const dimension : dimensions_) {
    // Dimension path precedences, discovered by model inspection (which must be
    // performed before adding path transit precedences).
    const ReverseArcListGraph<int, int>& graph =
        dimension->GetPathPrecedenceGraph();
    std::vector<std::pair<int, int>> path_precedences;
    for (const auto tail : graph.AllNodes()) {
      for (const auto head : graph[tail]) {
        path_precedences.emplace_back(tail, head);
      }
    }
    if (!path_precedences.empty()) {
      solver_->AddConstraint(solver_->MakePathTransitPrecedenceConstraint(
          nexts_, dimension->transits(), path_precedences));
    }

    // Dimension node precedences.
    for (const RoutingDimension::NodePrecedence& node_precedence :
         dimension->GetNodePrecedences()) {
      const int64 first_node = node_precedence.first_node;
      const int64 second_node = node_precedence.second_node;
      IntExpr* const nodes_are_selected =
          solver_->MakeMin(active_[first_node], active_[second_node]);
      IntExpr* const cumul_difference = solver_->MakeDifference(
          dimension->CumulVar(second_node), dimension->CumulVar(first_node));
      IntVar* const cumul_difference_is_ge_offset =
          solver_->MakeIsGreaterOrEqualCstVar(cumul_difference,
                                              node_precedence.offset);
      // Forces the implication: both nodes are active => cumul difference
      // constraint is active.
      solver_->AddConstraint(solver_->MakeLessOrEqual(
          nodes_are_selected->Var(), cumul_difference_is_ge_offset));
    }
  }

  // Store the local/global cumul optimizers, along with their offsets.
  StoreDimensionCumulOptimizers(parameters);

  // Keep this out of SetupSearch as this contains static search objects.
  // This will allow calling SetupSearch multiple times with different search
  // parameters.
  CreateNeighborhoodOperators(parameters);
  CreateFirstSolutionDecisionBuilders(parameters);
  error = FindErrorInSearchParametersForModel(parameters);
  if (!error.empty()) {
    status_ = ROUTING_INVALID;
    LOG(ERROR) << "Invalid RoutingSearchParameters for this model: " << error;
    return;
  }
  SetupSearch(parameters);
}

struct Link {
  Link(std::pair<int, int> link, double value, int vehicle_class,
       int64 start_depot, int64 end_depot)
      : link(link),
        value(value),
        vehicle_class(vehicle_class),
        start_depot(start_depot),
        end_depot(end_depot) {}
  ~Link() {}

  std::pair<int, int> link;
  int64 value;
  int vehicle_class;
  int64 start_depot;
  int64 end_depot;
};

struct LinkSort {
  bool operator()(const Link& link1, const Link& link2) const {
    return (link1.value > link2.value);
  }
} LinkComparator;

// The RouteConstructor creates the routes of a VRP instance subject to its
// constraints by iterating on a list of arcs appearing in descending order
// of priority.
// TODO(user): Use the dimension class in this class.
// TODO(user): Add support for vehicle-dependent dimension transits.
class RouteConstructor {
 public:
  RouteConstructor(Assignment* const assignment, RoutingModel* const model,
                   bool check_assignment, int64 num_indices,
                   const std::vector<Link>& links_list)
      : assignment_(assignment),
        model_(model),
        check_assignment_(check_assignment),
        solver_(model_->solver()),
        num_indices_(num_indices),
        links_list_(links_list),
        nexts_(model_->Nexts()),
        in_route_(num_indices_, -1),
        final_routes_(),
        index_to_chain_index_(num_indices, -1),
        index_to_vehicle_class_index_(num_indices, -1) {
    {
      const std::vector<std::string> dimension_names =
          model_->GetAllDimensionNames();
      dimensions_.assign(dimension_names.size(), nullptr);
      for (int i = 0; i < dimension_names.size(); ++i) {
        dimensions_[i] = &model_->GetDimensionOrDie(dimension_names[i]);
      }
    }
    cumuls_.resize(dimensions_.size());
    for (std::vector<int64>& cumuls : cumuls_) {
      cumuls.resize(num_indices_);
    }
    new_possible_cumuls_.resize(dimensions_.size());
  }

  ~RouteConstructor() {}

  void Construct() {
    model_->solver()->TopPeriodicCheck();
    // Initial State: Each order is served by its own vehicle.
    for (int index = 0; index < num_indices_; ++index) {
      if (!model_->IsStart(index) && !model_->IsEnd(index)) {
        std::vector<int> route(1, index);
        routes_.push_back(route);
        in_route_[index] = routes_.size() - 1;
      }
    }

    for (const Link& link : links_list_) {
      model_->solver()->TopPeriodicCheck();
      const int index1 = link.link.first;
      const int index2 = link.link.second;
      const int vehicle_class = link.vehicle_class;
      const int64 start_depot = link.start_depot;
      const int64 end_depot = link.end_depot;

      // Initialisation of cumuls_ if the indices are encountered for first time
      if (index_to_vehicle_class_index_[index1] < 0) {
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          cumuls_[dimension_index][index1] =
              std::max(dimensions_[dimension_index]->GetTransitValue(
                           start_depot, index1, 0),
                       dimensions_[dimension_index]->CumulVar(index1)->Min());
        }
      }
      if (index_to_vehicle_class_index_[index2] < 0) {
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          cumuls_[dimension_index][index2] =
              std::max(dimensions_[dimension_index]->GetTransitValue(
                           start_depot, index2, 0),
                       dimensions_[dimension_index]->CumulVar(index2)->Min());
        }
      }

      const int route_index1 = in_route_[index1];
      const int route_index2 = in_route_[index2];
      const bool merge =
          route_index1 >= 0 && route_index2 >= 0 &&
          FeasibleMerge(routes_[route_index1], routes_[route_index2], index1,
                        index2, route_index1, route_index2, vehicle_class,
                        start_depot, end_depot);
      if (Merge(merge, route_index1, route_index2)) {
        index_to_vehicle_class_index_[index1] = vehicle_class;
        index_to_vehicle_class_index_[index2] = vehicle_class;
      }
    }

    model_->solver()->TopPeriodicCheck();
    // Beyond this point not checking limits anymore as the rest of the code is
    // linear and that given we managed to build a solution would be stupid to
    // drop it now.
    for (int chain_index = 0; chain_index < chains_.size(); ++chain_index) {
      if (!gtl::ContainsKey(deleted_chains_, chain_index)) {
        final_chains_.push_back(chains_[chain_index]);
      }
    }
    std::sort(final_chains_.begin(), final_chains_.end(), ChainComparator);
    for (int route_index = 0; route_index < routes_.size(); ++route_index) {
      if (!gtl::ContainsKey(deleted_routes_, route_index)) {
        final_routes_.push_back(routes_[route_index]);
      }
    }
    std::sort(final_routes_.begin(), final_routes_.end(), RouteComparator);

    const int extra_vehicles = std::max(
        0, static_cast<int>(final_chains_.size()) - model_->vehicles());
    // Bind the Start and End of each chain
    int chain_index = 0;
    for (chain_index = extra_vehicles; chain_index < final_chains_.size();
         ++chain_index) {
      if (chain_index - extra_vehicles >= model_->vehicles()) {
        break;
      }
      const int start = final_chains_[chain_index].head;
      const int end = final_chains_[chain_index].tail;
      assignment_->Add(
          model_->NextVar(model_->Start(chain_index - extra_vehicles)));
      assignment_->SetValue(
          model_->NextVar(model_->Start(chain_index - extra_vehicles)), start);
      assignment_->Add(nexts_[end]);
      assignment_->SetValue(nexts_[end],
                            model_->End(chain_index - extra_vehicles));
    }

    // Create the single order routes
    for (int route_index = 0; route_index < final_routes_.size();
         ++route_index) {
      if (chain_index - extra_vehicles >= model_->vehicles()) {
        break;
      }
      DCHECK_LT(route_index, final_routes_.size());
      const int head = final_routes_[route_index].front();
      const int tail = final_routes_[route_index].back();
      if (head == tail && head < model_->Size()) {
        assignment_->Add(
            model_->NextVar(model_->Start(chain_index - extra_vehicles)));
        assignment_->SetValue(
            model_->NextVar(model_->Start(chain_index - extra_vehicles)), head);
        assignment_->Add(nexts_[tail]);
        assignment_->SetValue(nexts_[tail],
                              model_->End(chain_index - extra_vehicles));
        ++chain_index;
      }
    }

    // Unperformed
    for (int index = 0; index < model_->Size(); ++index) {
      IntVar* const next = nexts_[index];
      if (!assignment_->Contains(next)) {
        assignment_->Add(next);
        if (next->Contains(index)) {
          assignment_->SetValue(next, index);
        }
      }
    }
  }

  const std::vector<std::vector<int>>& final_routes() const {
    return final_routes_;
  }

 private:
  enum MergeStatus { FIRST_SECOND, SECOND_FIRST, NO_MERGE };

  struct RouteSort {
    bool operator()(const std::vector<int>& route1,
                    const std::vector<int>& route2) const {
      return (route1.size() < route2.size());
    }
  } RouteComparator;

  struct Chain {
    int head;
    int tail;
    int nodes;
  };

  struct ChainSort {
    bool operator()(const Chain& chain1, const Chain& chain2) const {
      return (chain1.nodes < chain2.nodes);
    }
  } ChainComparator;

  bool Head(int node) const {
    return (node == routes_[in_route_[node]].front());
  }

  bool Tail(int node) const {
    return (node == routes_[in_route_[node]].back());
  }

  bool FeasibleRoute(const std::vector<int>& route, int64 route_cumul,
                     int dimension_index) {
    const RoutingDimension& dimension = *dimensions_[dimension_index];
    std::vector<int>::const_iterator it = route.begin();
    int64 cumul = route_cumul;
    while (it != route.end()) {
      const int previous = *it;
      const int64 cumul_previous = cumul;
      gtl::InsertOrDie(&(new_possible_cumuls_[dimension_index]), previous,
                       cumul_previous);
      ++it;
      if (it == route.end()) {
        return true;
      }
      const int next = *it;
      int64 available_from_previous =
          cumul_previous + dimension.GetTransitValue(previous, next, 0);
      int64 available_cumul_next =
          std::max(cumuls_[dimension_index][next], available_from_previous);

      const int64 slack = available_cumul_next - available_from_previous;
      if (slack > dimension.SlackVar(previous)->Max()) {
        available_cumul_next =
            available_from_previous + dimension.SlackVar(previous)->Max();
      }

      if (available_cumul_next > dimension.CumulVar(next)->Max()) {
        return false;
      }
      if (available_cumul_next <= cumuls_[dimension_index][next]) {
        return true;
      }
      cumul = available_cumul_next;
    }
    return true;
  }

  bool CheckRouteConnection(const std::vector<int>& route1,
                            const std::vector<int>& route2, int dimension_index,
                            int64 start_depot, int64 end_depot) {
    const int tail1 = route1.back();
    const int head2 = route2.front();
    const int tail2 = route2.back();
    const RoutingDimension& dimension = *dimensions_[dimension_index];
    int non_depot_node = -1;
    for (int node = 0; node < num_indices_; ++node) {
      if (!model_->IsStart(node) && !model_->IsEnd(node)) {
        non_depot_node = node;
        break;
      }
    }
    CHECK_GE(non_depot_node, 0);
    const int64 depot_threashold =
        std::max(dimension.SlackVar(non_depot_node)->Max(),
                 dimension.CumulVar(non_depot_node)->Max());

    int64 available_from_tail1 = cumuls_[dimension_index][tail1] +
                                 dimension.GetTransitValue(tail1, head2, 0);
    int64 new_available_cumul_head2 =
        std::max(cumuls_[dimension_index][head2], available_from_tail1);

    const int64 slack = new_available_cumul_head2 - available_from_tail1;
    if (slack > dimension.SlackVar(tail1)->Max()) {
      new_available_cumul_head2 =
          available_from_tail1 + dimension.SlackVar(tail1)->Max();
    }

    bool feasible_route = true;
    if (new_available_cumul_head2 > dimension.CumulVar(head2)->Max()) {
      return false;
    }
    if (new_available_cumul_head2 <= cumuls_[dimension_index][head2]) {
      return true;
    }

    feasible_route =
        FeasibleRoute(route2, new_available_cumul_head2, dimension_index);
    const int64 new_possible_cumul_tail2 =
        gtl::ContainsKey(new_possible_cumuls_[dimension_index], tail2)
            ? new_possible_cumuls_[dimension_index][tail2]
            : cumuls_[dimension_index][tail2];

    if (!feasible_route || (new_possible_cumul_tail2 +
                                dimension.GetTransitValue(tail2, end_depot, 0) >
                            depot_threashold)) {
      return false;
    }
    return true;
  }

  bool FeasibleMerge(const std::vector<int>& route1,
                     const std::vector<int>& route2, int node1, int node2,
                     int route_index1, int route_index2, int vehicle_class,
                     int64 start_depot, int64 end_depot) {
    if ((route_index1 == route_index2) || !(Tail(node1) && Head(node2))) {
      return false;
    }

    // Vehicle Class Check
    if (!((index_to_vehicle_class_index_[node1] == -1 &&
           index_to_vehicle_class_index_[node2] == -1) ||
          (index_to_vehicle_class_index_[node1] == vehicle_class &&
           index_to_vehicle_class_index_[node2] == -1) ||
          (index_to_vehicle_class_index_[node1] == -1 &&
           index_to_vehicle_class_index_[node2] == vehicle_class) ||
          (index_to_vehicle_class_index_[node1] == vehicle_class &&
           index_to_vehicle_class_index_[node2] == vehicle_class))) {
      return false;
    }

    // Check Route1 -> Route2 connection for every dimension
    bool merge = true;
    for (int dimension_index = 0; dimension_index < dimensions_.size();
         ++dimension_index) {
      new_possible_cumuls_[dimension_index].clear();
      merge = merge && CheckRouteConnection(route1, route2, dimension_index,
                                            start_depot, end_depot);
      if (!merge) {
        return false;
      }
    }
    return true;
  }

  bool CheckTempAssignment(Assignment* const temp_assignment,
                           int new_chain_index, int old_chain_index, int head1,
                           int tail1, int head2, int tail2) {
    // TODO(user): If the chain index is greater than the number of vehicles,
    // use another vehicle instead.
    if (new_chain_index >= model_->vehicles()) return false;
    const int start = head1;
    temp_assignment->Add(model_->NextVar(model_->Start(new_chain_index)));
    temp_assignment->SetValue(model_->NextVar(model_->Start(new_chain_index)),
                              start);
    temp_assignment->Add(nexts_[tail1]);
    temp_assignment->SetValue(nexts_[tail1], head2);
    temp_assignment->Add(nexts_[tail2]);
    temp_assignment->SetValue(nexts_[tail2], model_->End(new_chain_index));
    for (int chain_index = 0; chain_index < chains_.size(); ++chain_index) {
      if ((chain_index != new_chain_index) &&
          (chain_index != old_chain_index) &&
          (!gtl::ContainsKey(deleted_chains_, chain_index))) {
        const int start = chains_[chain_index].head;
        const int end = chains_[chain_index].tail;
        temp_assignment->Add(model_->NextVar(model_->Start(chain_index)));
        temp_assignment->SetValue(model_->NextVar(model_->Start(chain_index)),
                                  start);
        temp_assignment->Add(nexts_[end]);
        temp_assignment->SetValue(nexts_[end], model_->End(chain_index));
      }
    }
    return solver_->Solve(solver_->MakeRestoreAssignment(temp_assignment));
  }

  bool UpdateAssignment(const std::vector<int>& route1,
                        const std::vector<int>& route2) {
    bool feasible = true;
    const int head1 = route1.front();
    const int tail1 = route1.back();
    const int head2 = route2.front();
    const int tail2 = route2.back();
    const int chain_index1 = index_to_chain_index_[head1];
    const int chain_index2 = index_to_chain_index_[head2];
    if (chain_index1 < 0 && chain_index2 < 0) {
      const int chain_index = chains_.size();
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible = CheckTempAssignment(temp_assignment, chain_index, -1, head1,
                                       tail1, head2, tail2);
      }
      if (feasible) {
        Chain chain;
        chain.head = head1;
        chain.tail = tail2;
        chain.nodes = 2;
        index_to_chain_index_[head1] = chain_index;
        index_to_chain_index_[tail2] = chain_index;
        chains_.push_back(chain);
      }
    } else if (chain_index1 >= 0 && chain_index2 < 0) {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index1, chain_index2,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        index_to_chain_index_[tail2] = chain_index1;
        chains_[chain_index1].head = head1;
        chains_[chain_index1].tail = tail2;
        ++chains_[chain_index1].nodes;
      }
    } else if (chain_index1 < 0 && chain_index2 >= 0) {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index2, chain_index1,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        index_to_chain_index_[head1] = chain_index2;
        chains_[chain_index2].head = head1;
        chains_[chain_index2].tail = tail2;
        ++chains_[chain_index2].nodes;
      }
    } else {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index1, chain_index2,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        index_to_chain_index_[tail2] = chain_index1;
        chains_[chain_index1].head = head1;
        chains_[chain_index1].tail = tail2;
        chains_[chain_index1].nodes += chains_[chain_index2].nodes;
        deleted_chains_.insert(chain_index2);
      }
    }
    if (feasible) {
      assignment_->Add(nexts_[tail1]);
      assignment_->SetValue(nexts_[tail1], head2);
    }
    return feasible;
  }

  bool Merge(bool merge, int index1, int index2) {
    if (merge) {
      if (UpdateAssignment(routes_[index1], routes_[index2])) {
        // Connection Route1 -> Route2
        for (const int node : routes_[index2]) {
          in_route_[node] = index1;
          routes_[index1].push_back(node);
        }
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          for (const std::pair<int, int64> new_possible_cumul :
               new_possible_cumuls_[dimension_index]) {
            cumuls_[dimension_index][new_possible_cumul.first] =
                new_possible_cumul.second;
          }
        }
        deleted_routes_.insert(index2);
        return true;
      }
    }
    return false;
  }

  Assignment* const assignment_;
  RoutingModel* const model_;
  const bool check_assignment_;
  Solver* const solver_;
  const int64 num_indices_;
  const std::vector<Link> links_list_;
  std::vector<IntVar*> nexts_;
  std::vector<const RoutingDimension*> dimensions_;  // Not owned.
  std::vector<std::vector<int64>> cumuls_;
  std::vector<absl::flat_hash_map<int, int64>> new_possible_cumuls_;
  std::vector<std::vector<int>> routes_;
  std::vector<int> in_route_;
  absl::flat_hash_set<int> deleted_routes_;
  std::vector<std::vector<int>> final_routes_;
  std::vector<Chain> chains_;
  absl::flat_hash_set<int> deleted_chains_;
  std::vector<Chain> final_chains_;
  std::vector<int> index_to_chain_index_;
  std::vector<int> index_to_vehicle_class_index_;
};

#ifndef SWIG
struct SweepIndex {
  SweepIndex(const int64 index, const double angle, const double distance)
      : index(index), angle(angle), distance(distance) {}
  ~SweepIndex() {}

  int64 index;
  double angle;
  double distance;
};

struct SweepIndexSortAngle {
  bool operator()(const SweepIndex& node1, const SweepIndex& node2) const {
    return (node1.angle < node2.angle);
  }
} SweepIndexAngleComparator;

struct SweepIndexSortDistance {
  bool operator()(const SweepIndex& node1, const SweepIndex& node2) const {
    return (node1.distance < node2.distance);
  }
} SweepIndexDistanceComparator;

SweepArranger::SweepArranger(const std::vector<std::pair<int64, int64>>& points)
    : coordinates_(2 * points.size(), 0), sectors_(1) {
  for (int64 i = 0; i < points.size(); ++i) {
    coordinates_[2 * i] = points[i].first;
    coordinates_[2 * i + 1] = points[i].second;
  }
}

// Splits the space of the indices into sectors and sorts the indices of each
// sector with ascending angle from the depot.
void SweepArranger::ArrangeIndices(std::vector<int64>* indices) {
  const double pi_rad = 3.14159265;
  // Suppose that the center is at x0, y0.
  const int x0 = coordinates_[0];
  const int y0 = coordinates_[1];

  std::vector<SweepIndex> sweep_indices;
  for (int64 index = 0; index < static_cast<int>(coordinates_.size()) / 2;
       ++index) {
    const int x = coordinates_[2 * index];
    const int y = coordinates_[2 * index + 1];
    const double x_delta = x - x0;
    const double y_delta = y - y0;
    double square_distance = x_delta * x_delta + y_delta * y_delta;
    double angle = square_distance == 0 ? 0 : std::atan2(y_delta, x_delta);
    angle = angle >= 0 ? angle : 2 * pi_rad + angle;
    SweepIndex sweep_index(index, angle, square_distance);
    sweep_indices.push_back(sweep_index);
  }
  std::sort(sweep_indices.begin(), sweep_indices.end(),
            SweepIndexDistanceComparator);

  const int size = static_cast<int>(sweep_indices.size()) / sectors_;
  for (int sector = 0; sector < sectors_; ++sector) {
    std::vector<SweepIndex> cluster;
    std::vector<SweepIndex>::iterator begin =
        sweep_indices.begin() + sector * size;
    std::vector<SweepIndex>::iterator end =
        sector == sectors_ - 1 ? sweep_indices.end()
                               : sweep_indices.begin() + (sector + 1) * size;
    std::sort(begin, end, SweepIndexAngleComparator);
  }
  for (const SweepIndex& sweep_index : sweep_indices) {
    indices->push_back(sweep_index.index);
  }
}

// Decision Builder building a first solution based on Sweep heuristic for
// Vehicle Routing Problem.
// Suitable only when distance is considered as the cost.
class SweepBuilder : public DecisionBuilder {
 public:
  SweepBuilder(RoutingModel* const model, bool check_assignment)
      : model_(model), check_assignment_(check_assignment) {}
  ~SweepBuilder() override {}

  Decision* Next(Solver* const solver) override {
    // Setup the model of the instance for the Sweep Algorithm
    ModelSetup();

    // Build the assignment routes for the model
    Assignment* const assignment = solver->MakeAssignment();
    route_constructor_ = absl::make_unique<RouteConstructor>(
        assignment, model_, check_assignment_, num_indices_, links_);
    // This call might cause backtracking if the search limit is reached.
    route_constructor_->Construct();
    route_constructor_.reset(nullptr);
    // This call might cause backtracking if the solution is not feasible.
    assignment->Restore();

    return nullptr;
  }

 private:
  void ModelSetup() {
    const int depot = model_->GetDepot();
    num_indices_ = model_->Size() + model_->vehicles();
    if (FLAGS_sweep_sectors > 0 && FLAGS_sweep_sectors < num_indices_) {
      model_->sweep_arranger()->SetSectors(FLAGS_sweep_sectors);
    }
    std::vector<int64> indices;
    model_->sweep_arranger()->ArrangeIndices(&indices);
    for (int i = 0; i < indices.size() - 1; ++i) {
      const int64 first = indices[i];
      const int64 second = indices[i + 1];
      if ((model_->IsStart(first) || !model_->IsEnd(first)) &&
          (model_->IsStart(second) || !model_->IsEnd(second))) {
        if (first != depot && second != depot) {
          Link link(std::make_pair(first, second), 0, 0, depot, depot);
          links_.push_back(link);
        }
      }
    }
  }

  RoutingModel* const model_;
  std::unique_ptr<RouteConstructor> route_constructor_;
  const bool check_assignment_;
  int64 num_indices_;
  std::vector<Link> links_;
};
#endif

namespace {
// Decision builder to build a solution with all nodes inactive. It does no
// branching and may fail if some nodes cannot be made inactive.

class AllUnperformed : public DecisionBuilder {
 public:
  // Does not take ownership of model.
  explicit AllUnperformed(RoutingModel* const model) : model_(model) {}
  ~AllUnperformed() override {}
  Decision* Next(Solver* const solver) override {
    // Solver::(Un)FreezeQueue is private, passing through the public API
    // on PropagationBaseObject.
    model_->CostVar()->FreezeQueue();
    for (int i = 0; i < model_->Size(); ++i) {
      if (!model_->IsStart(i)) {
        model_->ActiveVar(i)->SetValue(0);
      }
    }
    model_->CostVar()->UnfreezeQueue();
    return nullptr;
  }

 private:
  RoutingModel* const model_;
};
}  // namespace

void RoutingModel::AddSearchMonitor(SearchMonitor* const monitor) {
  monitors_.push_back(monitor);
}

namespace {
class AtSolutionCallbackMonitor : public SearchMonitor {
 public:
  AtSolutionCallbackMonitor(Solver* solver, std::function<void()> callback)
      : SearchMonitor(solver), callback_(std::move(callback)) {}
  bool AtSolution() override {
    callback_();
    return false;
  }

 private:
  std::function<void()> callback_;
};
}  // namespace

void RoutingModel::AddAtSolutionCallback(std::function<void()> callback) {
  AddSearchMonitor(solver_->RevAlloc(
      new AtSolutionCallbackMonitor(solver_.get(), std::move(callback))));
}

const Assignment* RoutingModel::Solve(const Assignment* assignment) {
  return SolveFromAssignmentWithParameters(assignment,
                                           DefaultRoutingSearchParameters());
}

const Assignment* RoutingModel::SolveWithParameters(
    const RoutingSearchParameters& parameters,
    std::vector<const Assignment*>* solutions) {
  return SolveFromAssignmentWithParameters(nullptr, parameters, solutions);
}

namespace {
absl::Duration GetTimeLimit(const RoutingSearchParameters& parameters) {
  if (!parameters.has_time_limit()) return absl::InfiniteDuration();
  return util_time::DecodeGoogleApiProto(parameters.time_limit()).ValueOrDie();
}

absl::Duration GetLnsTimeLimit(const RoutingSearchParameters& parameters) {
  if (!parameters.has_lns_time_limit()) return absl::InfiniteDuration();
  return util_time::DecodeGoogleApiProto(parameters.lns_time_limit())
      .ValueOrDie();
}

}  // namespace

namespace {
void MakeAllUnperformed(const RoutingModel* model, Assignment* assignment) {
  assignment->Clear();
  for (int i = 0; i < model->Nexts().size(); ++i) {
    if (!model->IsStart(i)) {
      assignment->Add(model->NextVar(i))->SetValue(i);
    }
  }
  for (int vehicle = 0; vehicle < model->vehicles(); ++vehicle) {
    assignment->Add(model->NextVar(model->Start(vehicle)))
        ->SetValue(model->End(vehicle));
  }
}
}  //  namespace

bool RoutingModel::AppendAssignmentIfFeasible(
    const Assignment& assignment,
    std::vector<std::unique_ptr<Assignment>>* assignments) {
  tmp_assignment_->CopyIntersection(&assignment);
  solver_->Solve(restore_tmp_assignment_, collect_one_assignment_,
                 GetOrCreateLimit());
  if (collect_one_assignment_->solution_count() == 1) {
    assignments->push_back(absl::make_unique<Assignment>(solver_.get()));
    assignments->back()->Copy(collect_one_assignment_->solution(0));
    return true;
  }
  return false;
}

void RoutingModel::LogSolution(const RoutingSearchParameters& parameters,
                               const std::string& description,
                               int64 solution_cost, int64 start_time_ms) {
  const std::string memory_str = MemoryUsage();
  const double cost_scaling_factor = parameters.log_cost_scaling_factor();
  const double cost_offset = parameters.log_cost_offset();
  const std::string cost_string =
      cost_scaling_factor == 1.0 && cost_offset == 0.0
          ? absl::StrCat(solution_cost)
          : absl::StrFormat(
                "%d (%.8lf)", solution_cost,
                cost_scaling_factor * (solution_cost + cost_offset));
  LOG(INFO) << absl::StrFormat(
      "%s (%s, time = %d ms, memory used = %s)", description, cost_string,
      solver_->wall_time() - start_time_ms, memory_str);
}

const Assignment* RoutingModel::SolveFromAssignmentWithParameters(
    const Assignment* assignment, const RoutingSearchParameters& parameters,
    std::vector<const Assignment*>* solutions) {
  const int64 start_time_ms = solver_->wall_time();
  QuietCloseModelWithParameters(parameters);
  VLOG(1) << "Search parameters:\n" << parameters.DebugString();
  if (solutions != nullptr) solutions->clear();
  if (status_ == ROUTING_INVALID) {
    return nullptr;
  }
  limit_->UpdateLimits(GetTimeLimit(parameters), kint64max, kint64max,
                       parameters.solution_limit());
  ls_limit_->UpdateLimits(GetTimeLimit(parameters), kint64max, kint64max, 1);
  lns_limit_->UpdateLimits(GetLnsTimeLimit(parameters), kint64max, kint64max,
                           kint64max);
  // NOTE: Allow more time for the first solution's scheduling, since if it
  // fails, we won't have anything to build upon.
  // We set this time limit based on whether local/global dimension optimizers
  // are used in the finalizer to avoid going over the general time limit.
  // TODO(user): Adapt this when absolute timeouts are given to the model.
  const int time_limit_shares = 1 + !global_dimension_optimizers_.empty() +
                                !local_dimension_optimizers_.empty();
  const absl::Duration first_solution_lns_time_limit =
      std::max(GetTimeLimit(parameters) / time_limit_shares,
               GetLnsTimeLimit(parameters));
  first_solution_lns_limit_->UpdateLimits(first_solution_lns_time_limit,
                                          kint64max, kint64max, kint64max);

  std::vector<std::unique_ptr<Assignment>> solution_pool;
  if (parameters.use_cp() == BOOL_TRUE) {
    if (nullptr == assignment) {
      bool solution_found = false;
      Assignment matching(solver_.get());
      if (IsMatchingModel() && SolveMatchingModel(&matching, parameters) &&
          AppendAssignmentIfFeasible(matching, &solution_pool)) {
        if (parameters.log_search()) {
          LogSolution(parameters, "Min-Cost Flow Solution",
                      solution_pool.back()->ObjectiveValue(), start_time_ms);
        }
        solution_found = true;
      }
      if (!solution_found) {
        // Build trivial solutions to which we can come back too in case the
        // solver does not manage to build something better.
        Assignment unperformed(solver_.get());
        MakeAllUnperformed(this, &unperformed);
        if (AppendAssignmentIfFeasible(unperformed, &solution_pool) &&
            parameters.log_search()) {
          LogSolution(parameters, "All Unperformed Solution",
                      solution_pool.back()->ObjectiveValue(), start_time_ms);
        }
        const absl::Duration elapsed_time =
            absl::Milliseconds(solver_->wall_time() - start_time_ms);
        const absl::Duration time_left =
            GetTimeLimit(parameters) - elapsed_time;
        if (time_left >= absl::ZeroDuration()) {
          limit_->UpdateLimits(time_left, kint64max, kint64max,
                               parameters.solution_limit());
          ls_limit_->UpdateLimits(time_left, kint64max, kint64max, 1);
          solver_->Solve(solve_db_, monitors_);
        }
      }
    } else {
      assignment_->CopyIntersection(assignment);
      solver_->Solve(improve_db_, monitors_);
    }
  }

  if (parameters.use_cp_sat() == BOOL_TRUE) {
    const int solution_count = collect_assignments_->solution_count();
    Assignment* const cp_solution =
        solution_count >= 1 ? collect_assignments_->solution(solution_count - 1)
                            : nullptr;
    Assignment sat_solution(solver_.get());
    if (SolveModelWithSat(*this, parameters, cp_solution, &sat_solution) &&
        AppendAssignmentIfFeasible(sat_solution, &solution_pool) &&
        parameters.log_search()) {
      LogSolution(parameters, "SAT", solution_pool.back()->ObjectiveValue(),
                  start_time_ms);
    }
  }

  const absl::Duration elapsed_time =
      absl::Milliseconds(solver_->wall_time() - start_time_ms);
  const int solution_count = collect_assignments_->solution_count();
  if (solution_count >= 1 || !solution_pool.empty()) {
    status_ = ROUTING_SUCCESS;
    if (solutions != nullptr) {
      for (int i = 0; i < solution_count; ++i) {
        solutions->push_back(
            solver_->MakeAssignment(collect_assignments_->solution(i)));
      }
      for (const auto& solution : solution_pool) {
        if (solutions->empty() ||
            solution->ObjectiveValue() < solutions->back()->ObjectiveValue()) {
          solutions->push_back(solver_->MakeAssignment(solution.get()));
        }
      }
      return solutions->back();
    }
    Assignment* best_assignment =
        solution_count >= 1 ? collect_assignments_->solution(solution_count - 1)
                            : nullptr;
    for (const auto& solution : solution_pool) {
      if (best_assignment == nullptr ||
          solution->ObjectiveValue() < best_assignment->ObjectiveValue()) {
        best_assignment = solution.get();
      }
    }
    return solver_->MakeAssignment(best_assignment);
  } else {
    if (elapsed_time >= GetTimeLimit(parameters)) {
      status_ = ROUTING_FAIL_TIMEOUT;
    } else {
      status_ = ROUTING_FAIL;
    }
    return nullptr;
  }
}

void RoutingModel::SetAssignmentFromOtherModelAssignment(
    Assignment* target_assignment, const RoutingModel* source_model,
    const Assignment* source_assignment) {
  const int size = Size();
  DCHECK_EQ(size, source_model->Size());
  CHECK_EQ(target_assignment->solver(), solver_.get());

  if (CostsAreHomogeneousAcrossVehicles()) {
    SetAssignmentFromAssignment(target_assignment, Nexts(), source_assignment,
                                source_model->Nexts());
  } else {
    std::vector<IntVar*> source_vars(size + size + vehicles_);
    std::vector<IntVar*> target_vars(size + size + vehicles_);
    for (int index = 0; index < size; index++) {
      source_vars[index] = source_model->NextVar(index);
      target_vars[index] = NextVar(index);
    }
    for (int index = 0; index < size + vehicles_; index++) {
      source_vars[size + index] = source_model->VehicleVar(index);
      target_vars[size + index] = VehicleVar(index);
    }
    SetAssignmentFromAssignment(target_assignment, target_vars,
                                source_assignment, source_vars);
  }

  target_assignment->AddObjective(cost_);
}

// Computing a lower bound to the cost of a vehicle routing problem solving a
// a linear assignment problem (minimum-cost perfect bipartite matching).
// A bipartite graph is created with left nodes representing the nodes of the
// routing problem and right nodes representing possible node successors; an
// arc between a left node l and a right node r is created if r can be the
// node folowing l in a route (Next(l) = r); the cost of the arc is the transit
// cost between l and r in the routing problem.
// This is a lower bound given the solution to assignment problem does not
// necessarily produce a (set of) closed route(s) from a starting node to an
// ending node.
int64 RoutingModel::ComputeLowerBound() {
  if (!closed_) {
    LOG(WARNING) << "Non-closed model not supported.";
    return 0;
  }
  if (!CostsAreHomogeneousAcrossVehicles()) {
    LOG(WARNING) << "Non-homogeneous vehicle costs not supported";
    return 0;
  }
  if (!disjunctions_.empty()) {
    LOG(WARNING)
        << "Node disjunction constraints or optional nodes not supported.";
    return 0;
  }
  const int num_nodes = Size() + vehicles_;
  ForwardStarGraph graph(2 * num_nodes, num_nodes * num_nodes);
  LinearSumAssignment<ForwardStarGraph> linear_sum_assignment(graph, num_nodes);
  // Adding arcs for non-end nodes, based on possible values of next variables.
  // Left nodes in the bipartite are indexed from 0 to num_nodes - 1; right
  // nodes are indexed from num_nodes to 2 * num_nodes - 1.
  for (int tail = 0; tail < Size(); ++tail) {
    std::unique_ptr<IntVarIterator> iterator(
        nexts_[tail]->MakeDomainIterator(false));
    for (const int64 head : InitAndGetValues(iterator.get())) {
      // Given there are no disjunction constraints, a node cannot point to
      // itself. Doing this explicitly given that outside the search,
      // propagation hasn't removed this value from next variables yet.
      if (head == tail) {
        continue;
      }
      // The index of a right node in the bipartite graph is the index
      // of the successor offset by the number of nodes.
      const ArcIndex arc = graph.AddArc(tail, num_nodes + head);
      const CostValue cost = GetHomogeneousCost(tail, head);
      linear_sum_assignment.SetArcCost(arc, cost);
    }
  }
  // The linear assignment library requires having as many left and right nodes.
  // Therefore we are creating fake assignments for end nodes, forced to point
  // to the equivalent start node with a cost of 0.
  for (int tail = Size(); tail < num_nodes; ++tail) {
    const ArcIndex arc = graph.AddArc(tail, num_nodes + starts_[tail - Size()]);
    linear_sum_assignment.SetArcCost(arc, 0);
  }
  if (linear_sum_assignment.ComputeAssignment()) {
    return linear_sum_assignment.GetCost();
  }
  return 0;
}

bool RoutingModel::RouteCanBeUsedByVehicle(const Assignment& assignment,
                                           int start_index, int vehicle) const {
  int current_index =
      IsStart(start_index) ? Next(assignment, start_index) : start_index;
  while (!IsEnd(current_index)) {
    const IntVar* const vehicle_var = VehicleVar(current_index);
    if (!vehicle_var->Contains(vehicle)) {
      return false;
    }
    const int next_index = Next(assignment, current_index);
    CHECK_NE(next_index, current_index) << "Inactive node inside a route";
    current_index = next_index;
  }
  return true;
}

bool RoutingModel::ReplaceUnusedVehicle(
    int unused_vehicle, int active_vehicle,
    Assignment* const compact_assignment) const {
  CHECK(compact_assignment != nullptr);
  CHECK(!IsVehicleUsed(*compact_assignment, unused_vehicle));
  CHECK(IsVehicleUsed(*compact_assignment, active_vehicle));
  // Swap NextVars at start nodes.
  const int unused_vehicle_start = Start(unused_vehicle);
  IntVar* const unused_vehicle_start_var = NextVar(unused_vehicle_start);
  const int unused_vehicle_end = End(unused_vehicle);
  const int active_vehicle_start = Start(active_vehicle);
  const int active_vehicle_end = End(active_vehicle);
  IntVar* const active_vehicle_start_var = NextVar(active_vehicle_start);
  const int active_vehicle_next =
      compact_assignment->Value(active_vehicle_start_var);
  compact_assignment->SetValue(unused_vehicle_start_var, active_vehicle_next);
  compact_assignment->SetValue(active_vehicle_start_var, End(active_vehicle));

  // Update VehicleVars along the route, update the last NextVar.
  int current_index = active_vehicle_next;
  while (!IsEnd(current_index)) {
    IntVar* const vehicle_var = VehicleVar(current_index);
    compact_assignment->SetValue(vehicle_var, unused_vehicle);
    const int next_index = Next(*compact_assignment, current_index);
    if (IsEnd(next_index)) {
      IntVar* const last_next_var = NextVar(current_index);
      compact_assignment->SetValue(last_next_var, End(unused_vehicle));
    }
    current_index = next_index;
  }

  // Update dimensions: update transits at the start.
  for (const RoutingDimension* const dimension : dimensions_) {
    const std::vector<IntVar*>& transit_variables = dimension->transits();
    IntVar* const unused_vehicle_transit_var =
        transit_variables[unused_vehicle_start];
    IntVar* const active_vehicle_transit_var =
        transit_variables[active_vehicle_start];
    const bool contains_unused_vehicle_transit_var =
        compact_assignment->Contains(unused_vehicle_transit_var);
    const bool contains_active_vehicle_transit_var =
        compact_assignment->Contains(active_vehicle_transit_var);
    if (contains_unused_vehicle_transit_var !=
        contains_active_vehicle_transit_var) {
      // TODO(user): clarify the expected trigger rate of this LOG.
      LOG(INFO) << "The assignment contains transit variable for dimension '"
                << dimension->name() << "' for some vehicles, but not for all";
      return false;
    }
    if (contains_unused_vehicle_transit_var) {
      const int64 old_unused_vehicle_transit =
          compact_assignment->Value(unused_vehicle_transit_var);
      const int64 old_active_vehicle_transit =
          compact_assignment->Value(active_vehicle_transit_var);
      compact_assignment->SetValue(unused_vehicle_transit_var,
                                   old_active_vehicle_transit);
      compact_assignment->SetValue(active_vehicle_transit_var,
                                   old_unused_vehicle_transit);
    }

    // Update dimensions: update cumuls at the end.
    const std::vector<IntVar*>& cumul_variables = dimension->cumuls();
    IntVar* const unused_vehicle_cumul_var =
        cumul_variables[unused_vehicle_end];
    IntVar* const active_vehicle_cumul_var =
        cumul_variables[active_vehicle_end];
    const int64 old_unused_vehicle_cumul =
        compact_assignment->Value(unused_vehicle_cumul_var);
    const int64 old_active_vehicle_cumul =
        compact_assignment->Value(active_vehicle_cumul_var);
    compact_assignment->SetValue(unused_vehicle_cumul_var,
                                 old_active_vehicle_cumul);
    compact_assignment->SetValue(active_vehicle_cumul_var,
                                 old_unused_vehicle_cumul);
  }
  return true;
}

Assignment* RoutingModel::CompactAssignment(
    const Assignment& assignment) const {
  return CompactAssignmentInternal(assignment, false);
}

Assignment* RoutingModel::CompactAndCheckAssignment(
    const Assignment& assignment) const {
  return CompactAssignmentInternal(assignment, true);
}

Assignment* RoutingModel::CompactAssignmentInternal(
    const Assignment& assignment, bool check_compact_assignment) const {
  CHECK_EQ(assignment.solver(), solver_.get());
  if (!CostsAreHomogeneousAcrossVehicles()) {
    LOG(WARNING)
        << "The costs are not homogeneous, routes cannot be rearranged";
    return nullptr;
  }

  std::unique_ptr<Assignment> compact_assignment(new Assignment(&assignment));
  for (int vehicle = 0; vehicle < vehicles_ - 1; ++vehicle) {
    if (IsVehicleUsed(*compact_assignment, vehicle)) {
      continue;
    }
    const int vehicle_start = Start(vehicle);
    const int vehicle_end = End(vehicle);
    // Find the last vehicle, that can swap routes with this one.
    int swap_vehicle = vehicles_ - 1;
    bool has_more_vehicles_with_route = false;
    for (; swap_vehicle > vehicle; --swap_vehicle) {
      // If a vehicle was already swapped, it will appear in compact_assignment
      // as unused.
      if (!IsVehicleUsed(*compact_assignment, swap_vehicle) ||
          !IsVehicleUsed(*compact_assignment, swap_vehicle)) {
        continue;
      }
      has_more_vehicles_with_route = true;
      const int swap_vehicle_start = Start(swap_vehicle);
      const int swap_vehicle_end = End(swap_vehicle);
      if (manager_.IndexToNode(vehicle_start) !=
              manager_.IndexToNode(swap_vehicle_start) ||
          manager_.IndexToNode(vehicle_end) !=
              manager_.IndexToNode(swap_vehicle_end)) {
        continue;
      }

      // Check that updating VehicleVars is OK.
      if (RouteCanBeUsedByVehicle(*compact_assignment, swap_vehicle_start,
                                  vehicle)) {
        break;
      }
    }

    if (swap_vehicle == vehicle) {
      if (has_more_vehicles_with_route) {
        // No route can be assigned to this vehicle, but there are more vehicles
        // with a route left. This would leave a gap in the indices.
        // TODO(user): clarify the expected trigger rate of this LOG.
        LOG(INFO) << "No vehicle that can be swapped with " << vehicle
                  << " was found";
        return nullptr;
      } else {
        break;
      }
    } else {
      if (!ReplaceUnusedVehicle(vehicle, swap_vehicle,
                                compact_assignment.get())) {
        return nullptr;
      }
    }
  }
  if (check_compact_assignment &&
      !solver_->CheckAssignment(compact_assignment.get())) {
    // TODO(user): clarify the expected trigger rate of this LOG.
    LOG(WARNING) << "The compacted assignment is not a valid solution";
    return nullptr;
  }
  return compact_assignment.release();
}

int RoutingModel::FindNextActive(int index,
                                 const std::vector<int64>& indices) const {
  ++index;
  CHECK_LE(0, index);
  const int size = indices.size();
  while (index < size && ActiveVar(indices[index])->Max() == 0) {
    ++index;
  }
  return index;
}

IntVar* RoutingModel::ApplyLocks(const std::vector<int64>& locks) {
  // TODO(user): Replace calls to this method with calls to
  // ApplyLocksToAllVehicles and remove this method?
  CHECK_EQ(vehicles_, 1);
  preassignment_->Clear();
  IntVar* next_var = nullptr;
  int lock_index = FindNextActive(-1, locks);
  const int size = locks.size();
  if (lock_index < size) {
    next_var = NextVar(locks[lock_index]);
    preassignment_->Add(next_var);
    for (lock_index = FindNextActive(lock_index, locks); lock_index < size;
         lock_index = FindNextActive(lock_index, locks)) {
      preassignment_->SetValue(next_var, locks[lock_index]);
      next_var = NextVar(locks[lock_index]);
      preassignment_->Add(next_var);
    }
  }
  return next_var;
}

bool RoutingModel::ApplyLocksToAllVehicles(
    const std::vector<std::vector<int64>>& locks, bool close_routes) {
  preassignment_->Clear();
  return RoutesToAssignment(locks, true, close_routes, preassignment_);
}

int64 RoutingModel::GetNumberOfDecisionsInFirstSolution(
    const RoutingSearchParameters& parameters) const {
  IntVarFilteredDecisionBuilder* const decision_builder =
      GetFilteredFirstSolutionDecisionBuilderOrNull(parameters);
  return decision_builder != nullptr ? decision_builder->number_of_decisions()
                                     : 0;
}

int64 RoutingModel::GetNumberOfRejectsInFirstSolution(
    const RoutingSearchParameters& parameters) const {
  IntVarFilteredDecisionBuilder* const decision_builder =
      GetFilteredFirstSolutionDecisionBuilderOrNull(parameters);
  return decision_builder != nullptr ? decision_builder->number_of_rejects()
                                     : 0;
}

bool RoutingModel::WriteAssignment(const std::string& file_name) const {
  if (collect_assignments_->solution_count() == 1 && assignment_ != nullptr) {
    assignment_->CopyIntersection(collect_assignments_->solution(0));
    return assignment_->Save(file_name);
  } else {
    return false;
  }
}

Assignment* RoutingModel::ReadAssignment(const std::string& file_name) {
  QuietCloseModel();
  CHECK(assignment_ != nullptr);
  if (assignment_->Load(file_name)) {
    return DoRestoreAssignment();
  }
  return nullptr;
}

Assignment* RoutingModel::RestoreAssignment(const Assignment& solution) {
  QuietCloseModel();
  CHECK(assignment_ != nullptr);
  assignment_->CopyIntersection(&solution);
  return DoRestoreAssignment();
}

Assignment* RoutingModel::DoRestoreAssignment() {
  if (status_ == ROUTING_INVALID) {
    return nullptr;
  }
  solver_->Solve(restore_assignment_, monitors_);
  if (collect_assignments_->solution_count() == 1) {
    status_ = ROUTING_SUCCESS;
    return collect_assignments_->solution(0);
  } else {
    status_ = ROUTING_FAIL;
    return nullptr;
  }
  return nullptr;
}

bool RoutingModel::RoutesToAssignment(
    const std::vector<std::vector<int64>>& routes, bool ignore_inactive_indices,
    bool close_routes, Assignment* const assignment) const {
  CHECK(assignment != nullptr);
  if (!closed_) {
    LOG(ERROR) << "The model is not closed yet";
    return false;
  }
  const int num_routes = routes.size();
  if (num_routes > vehicles_) {
    LOG(ERROR) << "The number of vehicles in the assignment (" << routes.size()
               << ") is greater than the number of vehicles in the model ("
               << vehicles_ << ")";
    return false;
  }

  absl::flat_hash_set<int> visited_indices;
  // Set value to NextVars based on the routes.
  for (int vehicle = 0; vehicle < num_routes; ++vehicle) {
    const std::vector<int64>& route = routes[vehicle];
    int from_index = Start(vehicle);
    std::pair<absl::flat_hash_set<int>::iterator, bool> insert_result =
        visited_indices.insert(from_index);
    if (!insert_result.second) {
      LOG(ERROR) << "Index " << from_index << " (start node for vehicle "
                 << vehicle << ") was already used";
      return false;
    }

    for (const int64 to_index : route) {
      if (to_index < 0 || to_index >= Size()) {
        LOG(ERROR) << "Invalid index: " << to_index;
        return false;
      }

      IntVar* const active_var = ActiveVar(to_index);
      if (active_var->Max() == 0) {
        if (ignore_inactive_indices) {
          continue;
        } else {
          LOG(ERROR) << "Index " << to_index << " is not active";
          return false;
        }
      }

      insert_result = visited_indices.insert(to_index);
      if (!insert_result.second) {
        LOG(ERROR) << "Index " << to_index << " is used multiple times";
        return false;
      }

      const IntVar* const vehicle_var = VehicleVar(to_index);
      if (!vehicle_var->Contains(vehicle)) {
        LOG(ERROR) << "Vehicle " << vehicle << " is not allowed at index "
                   << to_index;
        return false;
      }

      IntVar* const from_var = NextVar(from_index);
      if (!assignment->Contains(from_var)) {
        assignment->Add(from_var);
      }
      assignment->SetValue(from_var, to_index);

      from_index = to_index;
    }

    if (close_routes) {
      IntVar* const last_var = NextVar(from_index);
      if (!assignment->Contains(last_var)) {
        assignment->Add(last_var);
      }
      assignment->SetValue(last_var, End(vehicle));
    }
  }

  // Do not use the remaining vehicles.
  for (int vehicle = num_routes; vehicle < vehicles_; ++vehicle) {
    const int start_index = Start(vehicle);
    // Even if close_routes is false, we still need to add the start index to
    // visited_indices so that deactivating other nodes works correctly.
    std::pair<absl::flat_hash_set<int>::iterator, bool> insert_result =
        visited_indices.insert(start_index);
    if (!insert_result.second) {
      LOG(ERROR) << "Index " << start_index << " is used multiple times";
      return false;
    }
    if (close_routes) {
      IntVar* const start_var = NextVar(start_index);
      if (!assignment->Contains(start_var)) {
        assignment->Add(start_var);
      }
      assignment->SetValue(start_var, End(vehicle));
    }
  }

  // Deactivate other nodes (by pointing them to themselves).
  if (close_routes) {
    for (int index = 0; index < Size(); ++index) {
      if (!gtl::ContainsKey(visited_indices, index)) {
        IntVar* const next_var = NextVar(index);
        if (!assignment->Contains(next_var)) {
          assignment->Add(next_var);
        }
        assignment->SetValue(next_var, index);
      }
    }
  }

  return true;
}

Assignment* RoutingModel::ReadAssignmentFromRoutes(
    const std::vector<std::vector<int64>>& routes,
    bool ignore_inactive_indices) {
  QuietCloseModel();
  if (!RoutesToAssignment(routes, ignore_inactive_indices, true, assignment_)) {
    return nullptr;
  }
  // DoRestoreAssignment() might still fail when checking constraints (most
  // constraints are not verified by RoutesToAssignment) or when filling in
  // dimension variables.
  return DoRestoreAssignment();
}

void RoutingModel::AssignmentToRoutes(
    const Assignment& assignment,
    std::vector<std::vector<int64>>* const routes) const {
  CHECK(closed_);
  CHECK(routes != nullptr);

  const int model_size = Size();
  routes->resize(vehicles_);
  for (int vehicle = 0; vehicle < vehicles_; ++vehicle) {
    std::vector<int64>* const vehicle_route = &routes->at(vehicle);
    vehicle_route->clear();

    int num_visited_indices = 0;
    const int first_index = Start(vehicle);
    const IntVar* const first_var = NextVar(first_index);
    CHECK(assignment.Contains(first_var));
    CHECK(assignment.Bound(first_var));
    int current_index = assignment.Value(first_var);
    while (!IsEnd(current_index)) {
      vehicle_route->push_back(current_index);

      const IntVar* const next_var = NextVar(current_index);
      CHECK(assignment.Contains(next_var));
      CHECK(assignment.Bound(next_var));
      current_index = assignment.Value(next_var);

      ++num_visited_indices;
      CHECK_LE(num_visited_indices, model_size)
          << "The assignment contains a cycle";
    }
  }
}

#ifndef SWIG
std::vector<std::vector<int64>> RoutingModel::GetRoutesFromAssignment(
    const Assignment& assignment) {
  std::vector<std::vector<int64>> route_indices(vehicles());
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    if (!assignment.Bound(NextVar(vehicle))) {
      LOG(DFATAL) << "GetRoutesFromAssignment() called on incomplete solution:"
                  << " NextVar(" << vehicle << ") is unbound.";
    }
  }
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    int64 index = Start(vehicle);
    route_indices[vehicle].push_back(index);
    while (!IsEnd(index)) {
      index = assignment.Value(NextVar(index));
      route_indices[vehicle].push_back(index);
    }
  }
  return route_indices;
}
#endif

int64 RoutingModel::GetArcCostForClassInternal(
    int64 from_index, int64 to_index, CostClassIndex cost_class_index) const {
  DCHECK(closed_);
  DCHECK_GE(cost_class_index, 0);
  DCHECK_LT(cost_class_index, cost_classes_.size());
  CostCacheElement* const cache = &cost_cache_[from_index];
  // See the comment in CostCacheElement in the .h for the int64->int cast.
  if (cache->index == static_cast<int>(to_index) &&
      cache->cost_class_index == cost_class_index) {
    return cache->cost;
  }
  int64 cost = 0;
  const CostClass& cost_class = cost_classes_[cost_class_index];
  const auto& evaluator = transit_evaluators_[cost_class.evaluator_index];
  if (!IsStart(from_index)) {
    cost = CapAdd(evaluator(from_index, to_index),
                  GetDimensionTransitCostSum(from_index, to_index, cost_class));
  } else if (!IsEnd(to_index)) {
    // Apply route fixed cost on first non-first/last node, in other words on
    // the arc from the first node to its next node if it's not the last node.
    cost = CapAdd(
        evaluator(from_index, to_index),
        CapAdd(GetDimensionTransitCostSum(from_index, to_index, cost_class),
               fixed_cost_of_vehicle_[index_to_vehicle_[from_index]]));
  } else {
    // If there's only the first and last nodes on the route, it is considered
    // as an empty route.
    if (consider_empty_route_costs_[index_to_vehicle_[from_index]]) {
      cost =
          CapAdd(evaluator(from_index, to_index),
                 GetDimensionTransitCostSum(from_index, to_index, cost_class));
    } else {
      cost = 0;
    }
  }
  *cache = {static_cast<int>(to_index), cost_class_index, cost};
  return cost;
}

bool RoutingModel::IsStart(int64 index) const {
  return !IsEnd(index) && index_to_vehicle_[index] != kUnassigned;
}

bool RoutingModel::IsVehicleUsed(const Assignment& assignment,
                                 int vehicle) const {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicles_);
  CHECK_EQ(solver_.get(), assignment.solver());
  IntVar* const start_var = NextVar(Start(vehicle));
  CHECK(assignment.Contains(start_var));
  return !IsEnd(assignment.Value(start_var));
}

int64 RoutingModel::Next(const Assignment& assignment, int64 index) const {
  CHECK_EQ(solver_.get(), assignment.solver());
  IntVar* const next_var = NextVar(index);
  CHECK(assignment.Contains(next_var));
  CHECK(assignment.Bound(next_var));
  return assignment.Value(next_var);
}

int64 RoutingModel::GetArcCostForVehicle(int64 from_index, int64 to_index,
                                         int64 vehicle) const {
  if (from_index != to_index && vehicle >= 0) {
    return GetArcCostForClassInternal(from_index, to_index,
                                      GetCostClassIndexOfVehicle(vehicle));
  } else {
    return 0;
  }
}

int64 RoutingModel::GetArcCostForClass(
    int64 from_index, int64 to_index,
    int64 /*CostClassIndex*/ cost_class_index) const {
  if (from_index != to_index) {
    return GetArcCostForClassInternal(from_index, to_index,
                                      CostClassIndex(cost_class_index));
  } else {
    return 0;
  }
}

int64 RoutingModel::GetArcCostForFirstSolution(int64 from_index,
                                               int64 to_index) const {
  // Return high cost if connecting to an end (or bound-to-end) node;
  // this is used in the cost-based first solution strategies to avoid closing
  // routes too soon.
  if (!is_bound_to_end_ct_added_.Switched()) {
    // Lazily adding path-cumul constraint propagating connection to route end,
    // as it can be pretty costly in the general case.
    std::vector<IntVar*> zero_transit(Size(), solver_->MakeIntConst(0));
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, is_bound_to_end_, zero_transit));
    is_bound_to_end_ct_added_.Switch(solver_.get());
  }
  if (is_bound_to_end_[to_index]->Min() == 1) return kint64max;
  // TODO(user): Take vehicle into account.
  return GetHomogeneousCost(from_index, to_index);
}

int64 RoutingModel::GetDimensionTransitCostSum(
    int64 i, int64 j, const CostClass& cost_class) const {
  int64 cost = 0;
  for (const auto& evaluator_and_coefficient :
       cost_class.dimension_transit_evaluator_class_and_cost_coefficient) {
    DCHECK_GT(evaluator_and_coefficient.cost_coefficient, 0);
    cost = CapAdd(
        cost,
        CapProd(evaluator_and_coefficient.cost_coefficient,
                evaluator_and_coefficient.dimension->GetTransitValueFromClass(
                    i, j, evaluator_and_coefficient.transit_evaluator_class)));
  }
  return cost;
}

bool RoutingModel::ArcIsMoreConstrainedThanArc(int64 from, int64 to1,
                                               int64 to2) {
  // Deal with end nodes: never pick an end node over a non-end node.
  if (IsEnd(to1) || IsEnd(to2)) {
    if (IsEnd(to1) != IsEnd(to2)) return IsEnd(to2);
    // If both are end nodes, we don't care; the right end node will be picked
    // by constraint propagation. Break the tie by index.
    return to1 < to2;
  }

  // Look whether they are mandatory (must be performed) or optional.
  const bool mandatory1 = active_[to1]->Min() == 1;
  const bool mandatory2 = active_[to2]->Min() == 1;
  // Always pick a mandatory node over a non-mandatory one.
  if (mandatory1 != mandatory2) return mandatory1;

  // Look at the vehicle variables.
  IntVar* const src_vehicle_var = VehicleVar(from);
  // In case the source vehicle is bound, "src_vehicle" will be it.
  // Otherwise, it'll be set to some possible source vehicle that
  // isn't -1 (if possible).
  const int64 src_vehicle = src_vehicle_var->Max();
  if (src_vehicle_var->Bound()) {
    IntVar* const to1_vehicle_var = VehicleVar(to1);
    IntVar* const to2_vehicle_var = VehicleVar(to2);
    // Subtle: non-mandatory node have kNoVehicle as possible value for
    // their vehicle variable. So they're effectively "bound" when their domain
    // size is 2.
    const bool bound1 =
        mandatory1 ? to1_vehicle_var->Bound() : (to1_vehicle_var->Size() <= 2);
    const bool bound2 =
        mandatory2 ? to2_vehicle_var->Bound() : (to2_vehicle_var->Size() <= 2);
    // Prefer a destination bound to a given vehicle, even if it's not
    // bound to the right one (the propagation will quickly rule it out).
    if (bound1 != bound2) return bound1;
    if (bound1) {  // same as bound1 && bound2.
      // Min() will return kNoVehicle for optional nodes. Thus we use Max().
      const int64 vehicle1 = to1_vehicle_var->Max();
      const int64 vehicle2 = to2_vehicle_var->Max();
      // Prefer a destination bound to the right vehicle.
      // TODO(user): cover this clause in a unit test.
      if ((vehicle1 == src_vehicle) != (vehicle2 == src_vehicle)) {
        return vehicle1 == src_vehicle;
      }
      // If no destination is bound to the right vehicle, whatever we
      // return doesn't matter: both are infeasible. To be consistent, we
      // just break the tie.
      if (vehicle1 != src_vehicle) return to1 < to2;
    }
  }
  // At this point, either both destinations are bound to the source vehicle,
  // or none of them is bound, or the source vehicle isn't bound.
  // We don't bother inspecting the domains of the vehicle variables further.

  // Inspect the primary constrained dimension, if any.
  // TODO(user): try looking at all the dimensions, not just the primary one,
  // and reconsider the need for a "primary" dimension.
  if (!GetPrimaryConstrainedDimension().empty()) {
    const std::vector<IntVar*>& cumul_vars =
        GetDimensionOrDie(GetPrimaryConstrainedDimension()).cumuls();
    IntVar* const dim1 = cumul_vars[to1];
    IntVar* const dim2 = cumul_vars[to2];
    // Prefer the destination that has a lower upper bound for the constrained
    // dimension.
    if (dim1->Max() != dim2->Max()) return dim1->Max() < dim2->Max();
    // TODO(user): evaluate the *actual* Min() of each cumul variable in the
    // scenario where the corresponding arc from->to is performed, and pick
    // the destination with the lowest value.
  }

  // Break ties on equally constrained nodes with the (cost - unperformed
  // penalty).
  {
    const /*CostClassIndex*/ int64 cost_class_index =
        SafeGetCostClassInt64OfVehicle(src_vehicle);
    const int64 cost1 = CapSub(GetArcCostForClass(from, to1, cost_class_index),
                               UnperformedPenalty(to1));
    const int64 cost2 = CapSub(GetArcCostForClass(from, to2, cost_class_index),
                               UnperformedPenalty(to2));
    if (cost1 != cost2) return cost1 < cost2;
  }

  // Further break ties by looking at the size of the VehicleVar.
  {
    const int64 num_vehicles1 = VehicleVar(to1)->Size();
    const int64 num_vehicles2 = VehicleVar(to2)->Size();
    if (num_vehicles1 != num_vehicles2) return num_vehicles1 < num_vehicles2;
  }

  // Break perfect ties by value.
  return to1 < to2;
}

void RoutingModel::SetVisitType(int64 index, int type, VisitTypePolicy policy) {
  CHECK_LT(index, index_to_visit_type_.size());
  DCHECK_EQ(index_to_visit_type_.size(), index_to_type_policy_.size());
  index_to_visit_type_[index] = type;
  index_to_type_policy_[index] = policy;
  num_visit_types_ = std::max(num_visit_types_, type + 1);
}

int RoutingModel::GetVisitType(int64 index) const {
  CHECK_LT(index, index_to_visit_type_.size());
  return index_to_visit_type_[index];
}

const std::vector<int>& RoutingModel::GetSingleNodesOfType(int type) const {
  DCHECK_LT(type, single_nodes_of_type_.size());
  return single_nodes_of_type_[type];
}

const std::vector<int>& RoutingModel::GetPairIndicesOfType(int type) const {
  DCHECK_LT(type, pair_indices_of_type_.size());
  return pair_indices_of_type_[type];
}

RoutingModel::VisitTypePolicy RoutingModel::GetVisitTypePolicy(
    int64 index) const {
  CHECK_LT(index, index_to_type_policy_.size());
  return index_to_type_policy_[index];
}

void RoutingModel::CloseVisitTypes() {
  hard_incompatible_types_per_type_index_.resize(num_visit_types_);
  temporal_incompatible_types_per_type_index_.resize(num_visit_types_);
  same_vehicle_required_type_alternatives_per_type_index_.resize(
      num_visit_types_);
  required_type_alternatives_when_adding_type_index_.resize(num_visit_types_);
  required_type_alternatives_when_removing_type_index_.resize(num_visit_types_);
}

void RoutingModel::AddHardTypeIncompatibility(int type1, int type2) {
  DCHECK_LT(std::max(type1, type2),
            hard_incompatible_types_per_type_index_.size());
  has_hard_type_incompatibilities_ = true;

  hard_incompatible_types_per_type_index_[type1].insert(type2);
  hard_incompatible_types_per_type_index_[type2].insert(type1);
}

void RoutingModel::AddTemporalTypeIncompatibility(int type1, int type2) {
  DCHECK_LT(std::max(type1, type2),
            temporal_incompatible_types_per_type_index_.size());
  has_temporal_type_incompatibilities_ = true;

  temporal_incompatible_types_per_type_index_[type1].insert(type2);
  temporal_incompatible_types_per_type_index_[type2].insert(type1);
}

const absl::flat_hash_set<int>&
RoutingModel::GetHardTypeIncompatibilitiesOfType(int type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, hard_incompatible_types_per_type_index_.size());
  return hard_incompatible_types_per_type_index_[type];
}

const absl::flat_hash_set<int>&
RoutingModel::GetTemporalTypeIncompatibilitiesOfType(int type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, temporal_incompatible_types_per_type_index_.size());
  return temporal_incompatible_types_per_type_index_[type];
}

// TODO(user): Consider if an empty "required_type_alternatives" should mean
// trivially feasible requirement, as there are no required type alternatives?
void RoutingModel::AddSameVehicleRequiredTypeAlternatives(
    int dependent_type, absl::flat_hash_set<int> required_type_alternatives) {
  DCHECK_LT(dependent_type,
            same_vehicle_required_type_alternatives_per_type_index_.size());

  if (required_type_alternatives.empty()) {
    // The dependent_type requires an infeasible (empty) set of types.
    // Nodes of this type and all policies except
    // ADDED_TYPE_REMOVED_FROM_VEHICLE are trivially infeasible.
    absl::flat_hash_set<VisitTypePolicy>& infeasible_policies =
        trivially_infeasible_visit_types_to_policies_[dependent_type];
    infeasible_policies.insert(TYPE_ADDED_TO_VEHICLE);
    infeasible_policies.insert(TYPE_ON_VEHICLE_UP_TO_VISIT);
    infeasible_policies.insert(TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
    return;
  }

  has_same_vehicle_type_requirements_ = true;
  same_vehicle_required_type_alternatives_per_type_index_[dependent_type]
      .push_back(std::move(required_type_alternatives));
}

void RoutingModel::AddRequiredTypeAlternativesWhenAddingType(
    int dependent_type, absl::flat_hash_set<int> required_type_alternatives) {
  DCHECK_LT(dependent_type,
            required_type_alternatives_when_adding_type_index_.size());

  if (required_type_alternatives.empty()) {
    // The dependent_type requires an infeasible (empty) set of types.
    // Nodes of this type and policy TYPE_ADDED_TO_VEHICLE or
    // TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED are trivially infeasible.
    absl::flat_hash_set<VisitTypePolicy>& infeasible_policies =
        trivially_infeasible_visit_types_to_policies_[dependent_type];
    infeasible_policies.insert(TYPE_ADDED_TO_VEHICLE);
    infeasible_policies.insert(TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
    return;
  }

  has_temporal_type_requirements_ = true;
  required_type_alternatives_when_adding_type_index_[dependent_type].push_back(
      std::move(required_type_alternatives));
}

void RoutingModel::AddRequiredTypeAlternativesWhenRemovingType(
    int dependent_type, absl::flat_hash_set<int> required_type_alternatives) {
  DCHECK_LT(dependent_type,
            required_type_alternatives_when_removing_type_index_.size());

  if (required_type_alternatives.empty()) {
    // The dependent_type requires an infeasible (empty) set of types.
    // Nodes of this type and all policies except TYPE_ADDED_TO_VEHICLE are
    // trivially infeasible.
    absl::flat_hash_set<VisitTypePolicy>& infeasible_policies =
        trivially_infeasible_visit_types_to_policies_[dependent_type];
    infeasible_policies.insert(ADDED_TYPE_REMOVED_FROM_VEHICLE);
    infeasible_policies.insert(TYPE_ON_VEHICLE_UP_TO_VISIT);
    infeasible_policies.insert(TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
    return;
  }

  has_temporal_type_requirements_ = true;
  required_type_alternatives_when_removing_type_index_[dependent_type]
      .push_back(std::move(required_type_alternatives));
}

const std::vector<absl::flat_hash_set<int>>&
RoutingModel::GetSameVehicleRequiredTypeAlternativesOfType(int type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type,
            same_vehicle_required_type_alternatives_per_type_index_.size());
  return same_vehicle_required_type_alternatives_per_type_index_[type];
}

const std::vector<absl::flat_hash_set<int>>&
RoutingModel::GetRequiredTypeAlternativesWhenAddingType(int type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, required_type_alternatives_when_adding_type_index_.size());
  return required_type_alternatives_when_adding_type_index_[type];
}

const std::vector<absl::flat_hash_set<int>>&
RoutingModel::GetRequiredTypeAlternativesWhenRemovingType(int type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, required_type_alternatives_when_removing_type_index_.size());
  return required_type_alternatives_when_removing_type_index_[type];
}

int64 RoutingModel::UnperformedPenalty(int64 var_index) const {
  return UnperformedPenaltyOrValue(0, var_index);
}

int64 RoutingModel::UnperformedPenaltyOrValue(int64 default_value,
                                              int64 var_index) const {
  if (active_[var_index]->Min() == 1) return kint64max;  // Forced active.
  const std::vector<DisjunctionIndex>& disjunction_indices =
      GetDisjunctionIndices(var_index);
  if (disjunction_indices.size() != 1) return default_value;
  const DisjunctionIndex disjunction_index = disjunction_indices[0];
  // The disjunction penalty can be kNoPenalty iff there is more than one node
  // in the disjunction; otherwise we would have caught it earlier (the node
  // would be forced active).
  return std::max(int64{0}, disjunctions_[disjunction_index].value.penalty);
}

std::string RoutingModel::DebugOutputAssignment(
    const Assignment& solution_assignment,
    const std::string& dimension_to_print) const {
  for (int i = 0; i < Size(); ++i) {
    if (!solution_assignment.Bound(NextVar(i))) {
      LOG(DFATAL)
          << "DebugOutputVehicleSchedules() called on incomplete solution:"
          << " NextVar(" << i << ") is unbound.";
      return "";
    }
  }
  std::string output;
  absl::flat_hash_set<std::string> dimension_names;
  if (dimension_to_print.empty()) {
    const std::vector<std::string> all_dimension_names = GetAllDimensionNames();
    dimension_names.insert(all_dimension_names.begin(),
                           all_dimension_names.end());
  } else {
    dimension_names.insert(dimension_to_print);
  }
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    int empty_vehicle_range_start = vehicle;
    while (vehicle < vehicles() &&
           IsEnd(solution_assignment.Value(NextVar(Start(vehicle))))) {
      vehicle++;
    }
    if (empty_vehicle_range_start != vehicle) {
      if (empty_vehicle_range_start == vehicle - 1) {
        absl::StrAppendFormat(&output, "Vehicle %d: empty",
                              empty_vehicle_range_start);
      } else {
        absl::StrAppendFormat(&output, "Vehicles %d-%d: empty",
                              empty_vehicle_range_start, vehicle - 1);
      }
      output.append("\n");
    }
    if (vehicle < vehicles()) {
      absl::StrAppendFormat(&output, "Vehicle %d:", vehicle);
      int64 index = Start(vehicle);
      for (;;) {
        const IntVar* vehicle_var = VehicleVar(index);
        absl::StrAppendFormat(&output, "%d Vehicle(%d) ", index,
                              solution_assignment.Value(vehicle_var));
        for (const RoutingDimension* const dimension : dimensions_) {
          if (gtl::ContainsKey(dimension_names, dimension->name())) {
            const IntVar* const var = dimension->CumulVar(index);
            absl::StrAppendFormat(&output, "%s(%d..%d) ", dimension->name(),
                                  solution_assignment.Min(var),
                                  solution_assignment.Max(var));
          }
        }
        if (IsEnd(index)) break;
        index = solution_assignment.Value(NextVar(index));
        if (IsEnd(index)) output.append("Route end ");
      }
      output.append("\n");
    }
  }
  output.append("Unperformed nodes: ");
  bool has_unperformed = false;
  for (int i = 0; i < Size(); ++i) {
    if (!IsEnd(i) && !IsStart(i) &&
        solution_assignment.Value(NextVar(i)) == i) {
      absl::StrAppendFormat(&output, "%d ", i);
      has_unperformed = true;
    }
  }
  if (!has_unperformed) output.append("None");
  output.append("\n");
  return output;
}

#ifndef SWIG
std::vector<std::vector<std::pair<int64, int64>>> RoutingModel::GetCumulBounds(
    const Assignment& solution_assignment, const RoutingDimension& dimension) {
  std::vector<std::vector<std::pair<int64, int64>>> cumul_bounds(vehicles());
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    if (!solution_assignment.Bound(NextVar(vehicle))) {
      LOG(DFATAL) << "GetCumulBounds() called on incomplete solution:"
                  << " NextVar(" << vehicle << ") is unbound.";
    }
  }

  for (int vehicle_id = 0; vehicle_id < vehicles(); ++vehicle_id) {
    int64 index = Start(vehicle_id);
    IntVar* dim_var = dimension.CumulVar(index);
    cumul_bounds[vehicle_id].emplace_back(solution_assignment.Min(dim_var),
                                          solution_assignment.Max(dim_var));
    while (!IsEnd(index)) {
      index = solution_assignment.Value(NextVar(index));
      IntVar* dim_var = dimension.CumulVar(index);
      cumul_bounds[vehicle_id].emplace_back(solution_assignment.Min(dim_var),
                                            solution_assignment.Max(dim_var));
    }
  }
  return cumul_bounds;
}
#endif

Assignment* RoutingModel::GetOrCreateAssignment() {
  if (assignment_ == nullptr) {
    assignment_ = solver_->MakeAssignment();
    assignment_->Add(nexts_);
    if (!CostsAreHomogeneousAcrossVehicles()) {
      assignment_->Add(vehicle_vars_);
    }
    assignment_->AddObjective(cost_);
  }
  return assignment_;
}

Assignment* RoutingModel::GetOrCreateTmpAssignment() {
  if (tmp_assignment_ == nullptr) {
    tmp_assignment_ = solver_->MakeAssignment();
    tmp_assignment_->Add(nexts_);
  }
  return tmp_assignment_;
}

RegularLimit* RoutingModel::GetOrCreateLimit() {
  if (limit_ == nullptr) {
    limit_ = solver_->MakeLimit(absl::InfiniteDuration(), kint64max, kint64max,
                                kint64max, /*smart_time_check=*/true);
  }
  return limit_;
}

RegularLimit* RoutingModel::GetOrCreateLocalSearchLimit() {
  if (ls_limit_ == nullptr) {
    ls_limit_ =
        solver_->MakeLimit(absl::InfiniteDuration(), kint64max, kint64max,
                           /*solutions=*/1, /*smart_time_check=*/true);
  }
  return ls_limit_;
}

RegularLimit* RoutingModel::GetOrCreateLargeNeighborhoodSearchLimit() {
  if (lns_limit_ == nullptr) {
    lns_limit_ =
        solver_->MakeLimit(absl::InfiniteDuration(), kint64max, kint64max,
                           kint64max, /*smart_time_check=*/false);
  }
  return lns_limit_;
}

RegularLimit*
RoutingModel::GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit() {
  if (first_solution_lns_limit_ == nullptr) {
    first_solution_lns_limit_ =
        solver_->MakeLimit(absl::InfiniteDuration(), kint64max, kint64max,
                           kint64max, /*smart_time_check=*/false);
  }
  return first_solution_lns_limit_;
}

LocalSearchOperator* RoutingModel::CreateInsertionOperator() {
  std::vector<IntVar*> empty;
  LocalSearchOperator* insertion_operator =
      MakeLocalSearchOperator<MakeActiveOperator>(
          solver_.get(), nexts_,
          CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
          vehicle_start_class_callback_);
  if (!pickup_delivery_pairs_.empty()) {
    insertion_operator = solver_->ConcatenateOperators(
        {MakePairActive(
             solver_.get(), nexts_,
             CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
             vehicle_start_class_callback_, pickup_delivery_pairs_),
         insertion_operator});
  }
  return insertion_operator;
}

LocalSearchOperator* RoutingModel::CreateMakeInactiveOperator() {
  std::vector<IntVar*> empty;
  LocalSearchOperator* make_inactive_operator =
      MakeLocalSearchOperator<MakeInactiveOperator>(
          solver_.get(), nexts_,
          CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
          vehicle_start_class_callback_);
  if (!pickup_delivery_pairs_.empty()) {
    make_inactive_operator = solver_->ConcatenateOperators(
        {MakePairInactive(
             solver_.get(), nexts_,
             CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
             vehicle_start_class_callback_, pickup_delivery_pairs_),
         make_inactive_operator});
  }
  return make_inactive_operator;
}

#define CP_ROUTING_ADD_OPERATOR(operator_type, cp_operator_type)    \
  if (CostsAreHomogeneousAcrossVehicles()) {                        \
    local_search_operators_[operator_type] =                        \
        solver_->MakeOperator(nexts_, Solver::cp_operator_type);    \
  } else {                                                          \
    local_search_operators_[operator_type] = solver_->MakeOperator( \
        nexts_, vehicle_vars_, Solver::cp_operator_type);           \
  }

#define CP_ROUTING_ADD_OPERATOR2(operator_type, cp_operator_class)     \
  local_search_operators_[operator_type] =                             \
      MakeLocalSearchOperator<cp_operator_class>(                      \
          solver_.get(), nexts_,                                       \
          CostsAreHomogeneousAcrossVehicles() ? std::vector<IntVar*>() \
                                              : vehicle_vars_,         \
          vehicle_start_class_callback_);

#define CP_ROUTING_ADD_CALLBACK_OPERATOR(operator_type, cp_operator_type) \
  if (CostsAreHomogeneousAcrossVehicles()) {                              \
    local_search_operators_[operator_type] = solver_->MakeOperator(       \
        nexts_,                                                           \
        [this](int64 i, int64 j, int64 k) {                               \
          return GetArcCostForVehicle(i, j, k);                           \
        },                                                                \
        Solver::cp_operator_type);                                        \
  } else {                                                                \
    local_search_operators_[operator_type] = solver_->MakeOperator(       \
        nexts_, vehicle_vars_,                                            \
        [this](int64 i, int64 j, int64 k) {                               \
          return GetArcCostForVehicle(i, j, k);                           \
        },                                                                \
        Solver::cp_operator_type);                                        \
  }

void RoutingModel::CreateNeighborhoodOperators(
    const RoutingSearchParameters& parameters) {
  local_search_operators_.clear();
  local_search_operators_.resize(LOCAL_SEARCH_OPERATOR_COUNTER, nullptr);
  CP_ROUTING_ADD_OPERATOR2(RELOCATE, Relocate);
  std::vector<IntVar*> empty;
  local_search_operators_[RELOCATE_PAIR] = MakePairRelocate(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_, pickup_delivery_pairs_);
  local_search_operators_[LIGHT_RELOCATE_PAIR] = MakeLightPairRelocate(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_, pickup_delivery_pairs_);
  local_search_operators_[EXCHANGE_PAIR] = MakePairExchange(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_, pickup_delivery_pairs_);
  local_search_operators_[EXCHANGE_RELOCATE_PAIR] = MakePairExchangeRelocate(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_, pickup_delivery_pairs_);
  local_search_operators_[RELOCATE_NEIGHBORS] = MakeRelocateNeighbors(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_,
      [this](int64 from, int64 to) { return GetHomogeneousCost(from, to); });
  local_search_operators_[NODE_PAIR_SWAP] = solver_->ConcatenateOperators(
      {IndexPairSwapActive(
           solver_.get(), nexts_,
           CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
           vehicle_start_class_callback_, pickup_delivery_pairs_),
       SwapIndexPair(
           solver_.get(), nexts_,
           CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
           pickup_delivery_pairs_),
       PairNodeSwapActive(
           solver_.get(), nexts_,
           CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
           vehicle_start_class_callback_, pickup_delivery_pairs_)});
  const auto arc_cost_for_path_start =
      [this](int64 before_node, int64 after_node, int64 start_index) {
        const int vehicle = index_to_vehicle_[start_index];
        const int64 arc_cost =
            GetArcCostForVehicle(before_node, after_node, vehicle);
        return (before_node != start_index || IsEnd(after_node))
                   ? arc_cost
                   : CapSub(arc_cost, GetFixedCostOfVehicle(vehicle));
      };
  GlobalCheapestInsertionFilteredHeuristic::GlobalCheapestInsertionParameters
      ls_gci_parameters = {
          /* is_sequential */ false,
          /* farthest_seeds_ratio */ 0.0,
          parameters.cheapest_insertion_ls_operator_neighbors_ratio(),
          /* use_neighbors_ratio_for_initialization */ true};
  local_search_operators_[GLOBAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS] =
      solver_->RevAlloc(new FilteredHeuristicCloseNodesLNSOperator(
          absl::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              [this](int64 i) { return UnperformedPenaltyOrValue(0, i); },
              GetOrCreateFeasibilityFilterManager(parameters),
              ls_gci_parameters),
          parameters.heuristic_close_nodes_lns_num_nodes()));

  local_search_operators_[LOCAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS] =
      solver_->RevAlloc(new FilteredHeuristicCloseNodesLNSOperator(
          absl::make_unique<LocalCheapestInsertionFilteredHeuristic>(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              GetOrCreateFeasibilityFilterManager(parameters)),
          parameters.heuristic_close_nodes_lns_num_nodes()));

  local_search_operators_[GLOBAL_CHEAPEST_INSERTION_PATH_LNS] =
      solver_->RevAlloc(new FilteredHeuristicPathLNSOperator(
          absl::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              [this](int64 i) { return UnperformedPenaltyOrValue(0, i); },
              GetOrCreateFeasibilityFilterManager(parameters),
              ls_gci_parameters)));

  local_search_operators_[LOCAL_CHEAPEST_INSERTION_PATH_LNS] =
      solver_->RevAlloc(new FilteredHeuristicPathLNSOperator(
          absl::make_unique<LocalCheapestInsertionFilteredHeuristic>(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              GetOrCreateFeasibilityFilterManager(parameters))));
  local_search_operators_[GLOBAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS] =
      solver_->RevAlloc(new FilteredHeuristicExpensiveChainLNSOperator(
          absl::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              [this](int64 i) { return UnperformedPenaltyOrValue(0, i); },
              GetOrCreateFeasibilityFilterManager(parameters),
              ls_gci_parameters),
          parameters.heuristic_expensive_chain_lns_num_arcs_to_consider(),
          arc_cost_for_path_start));

  local_search_operators_[LOCAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS] =
      solver_->RevAlloc(new FilteredHeuristicExpensiveChainLNSOperator(
          absl::make_unique<LocalCheapestInsertionFilteredHeuristic>(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              GetOrCreateFeasibilityFilterManager(parameters)),
          parameters.heuristic_expensive_chain_lns_num_arcs_to_consider(),
          arc_cost_for_path_start));
  local_search_operators_[RELOCATE_EXPENSIVE_CHAIN] =
      solver_->RevAlloc(new RelocateExpensiveChain(
          nexts_, CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
          vehicle_start_class_callback_,
          parameters.relocate_expensive_chain_num_arcs_to_consider(),
          arc_cost_for_path_start));
  local_search_operators_[RELOCATE_SUBTRIP] = MakeRelocateSubtrip(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_, pickup_delivery_pairs_);
  local_search_operators_[EXCHANGE_SUBTRIP] = MakeExchangeSubtrip(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_, pickup_delivery_pairs_);

  CP_ROUTING_ADD_OPERATOR2(EXCHANGE, Exchange);
  CP_ROUTING_ADD_OPERATOR2(CROSS, Cross);
  CP_ROUTING_ADD_OPERATOR2(TWO_OPT, TwoOpt);
  CP_ROUTING_ADD_OPERATOR(OR_OPT, OROPT);
  CP_ROUTING_ADD_CALLBACK_OPERATOR(LIN_KERNIGHAN, LK);
  local_search_operators_[MAKE_ACTIVE] = CreateInsertionOperator();
  CP_ROUTING_ADD_OPERATOR2(RELOCATE_AND_MAKE_ACTIVE,
                           RelocateAndMakeActiveOperator);
  CP_ROUTING_ADD_OPERATOR2(MAKE_ACTIVE_AND_RELOCATE, MakeActiveAndRelocate);
  local_search_operators_[MAKE_INACTIVE] = CreateMakeInactiveOperator();
  CP_ROUTING_ADD_OPERATOR2(MAKE_CHAIN_INACTIVE, MakeChainInactiveOperator);
  CP_ROUTING_ADD_OPERATOR2(SWAP_ACTIVE, SwapActiveOperator);
  CP_ROUTING_ADD_OPERATOR2(EXTENDED_SWAP_ACTIVE, ExtendedSwapActiveOperator);
  CP_ROUTING_ADD_CALLBACK_OPERATOR(TSP_OPT, TSPOPT);
  CP_ROUTING_ADD_CALLBACK_OPERATOR(TSP_LNS, TSPLNS);
  CP_ROUTING_ADD_OPERATOR(PATH_LNS, PATHLNS);
  CP_ROUTING_ADD_OPERATOR(FULL_PATH_LNS, FULLPATHLNS);
  CP_ROUTING_ADD_OPERATOR(INACTIVE_LNS, UNACTIVELNS);
}

#undef CP_ROUTING_ADD_CALLBACK_OPERATOR
#undef CP_ROUTING_ADD_OPERATOR2
#undef CP_ROUTING_ADD_OPERATOR

#define CP_ROUTING_PUSH_OPERATOR(operator_type, operator_method, operators) \
  if (search_parameters.local_search_operators().use_##operator_method() == \
      BOOL_TRUE) {                                                          \
    operators.push_back(local_search_operators_[operator_type]);            \
  }

LocalSearchOperator* RoutingModel::GetNeighborhoodOperators(
    const RoutingSearchParameters& search_parameters) const {
  std::vector<LocalSearchOperator*> operator_groups;
  std::vector<LocalSearchOperator*> operators = extra_operators_;
  if (!pickup_delivery_pairs_.empty()) {
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_PAIR, relocate_pair, operators);
    // Only add the light version of relocate pair if the normal version has not
    // already been added as it covers a subset of its neighborhood.
    if (search_parameters.local_search_operators().use_relocate_pair() ==
        BOOL_FALSE) {
      CP_ROUTING_PUSH_OPERATOR(LIGHT_RELOCATE_PAIR, light_relocate_pair,
                               operators);
    }
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE_PAIR, exchange_pair, operators);
    CP_ROUTING_PUSH_OPERATOR(NODE_PAIR_SWAP, node_pair_swap_active, operators);
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_SUBTRIP, relocate_subtrip, operators);
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE_SUBTRIP, exchange_subtrip, operators);
  }
  if (vehicles_ > 1) {
    if (GetNumOfSingletonNodes() > 0) {
      // If there are only pairs in the model the only case where Relocate will
      // work is for intra-route moves, already covered by OrOpt.
      // We are not disabling Exchange and Cross because there are no
      // intra-route equivalents.
      CP_ROUTING_PUSH_OPERATOR(RELOCATE, relocate, operators);
    }
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE, exchange, operators);
    CP_ROUTING_PUSH_OPERATOR(CROSS, cross, operators);
  }
  if (!pickup_delivery_pairs_.empty() ||
      search_parameters.local_search_operators().use_relocate_neighbors() ==
          BOOL_TRUE) {
    operators.push_back(local_search_operators_[RELOCATE_NEIGHBORS]);
  }
  const LocalSearchMetaheuristic::Value local_search_metaheuristic =
      search_parameters.local_search_metaheuristic();
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::GENERIC_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(LIN_KERNIGHAN, lin_kernighan, operators);
  }
  CP_ROUTING_PUSH_OPERATOR(TWO_OPT, two_opt, operators);
  CP_ROUTING_PUSH_OPERATOR(OR_OPT, or_opt, operators);
  CP_ROUTING_PUSH_OPERATOR(RELOCATE_EXPENSIVE_CHAIN, relocate_expensive_chain,
                           operators);
  if (!disjunctions_.empty()) {
    CP_ROUTING_PUSH_OPERATOR(MAKE_INACTIVE, make_inactive, operators);
    CP_ROUTING_PUSH_OPERATOR(MAKE_CHAIN_INACTIVE, make_chain_inactive,
                             operators);
    CP_ROUTING_PUSH_OPERATOR(MAKE_ACTIVE, make_active, operators);

    // The relocate_and_make_active parameter activates all neighborhoods
    // relocating a node together with making another active.
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_AND_MAKE_ACTIVE, relocate_and_make_active,
                             operators);
    CP_ROUTING_PUSH_OPERATOR(MAKE_ACTIVE_AND_RELOCATE, relocate_and_make_active,
                             operators);

    CP_ROUTING_PUSH_OPERATOR(SWAP_ACTIVE, swap_active, operators);
    CP_ROUTING_PUSH_OPERATOR(EXTENDED_SWAP_ACTIVE, extended_swap_active,
                             operators);
  }
  operator_groups.push_back(solver_->ConcatenateOperators(operators));

  // Second local search loop: LNS-like operators.
  operators.clear();
  if (vehicles() > 1) {
    // NOTE: The following heuristic path LNS with a single vehicle are
    // equivalent to using the heuristic as first solution strategy, so we only
    // add these moves if we have at least 2 vehicles in the model.
    CP_ROUTING_PUSH_OPERATOR(GLOBAL_CHEAPEST_INSERTION_PATH_LNS,
                             global_cheapest_insertion_path_lns, operators);
    CP_ROUTING_PUSH_OPERATOR(LOCAL_CHEAPEST_INSERTION_PATH_LNS,
                             local_cheapest_insertion_path_lns, operators);
  }
  CP_ROUTING_PUSH_OPERATOR(GLOBAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS,
                           global_cheapest_insertion_expensive_chain_lns,
                           operators);
  CP_ROUTING_PUSH_OPERATOR(LOCAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS,
                           local_cheapest_insertion_expensive_chain_lns,
                           operators);
  CP_ROUTING_PUSH_OPERATOR(GLOBAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS,
                           global_cheapest_insertion_close_nodes_lns,
                           operators);
  CP_ROUTING_PUSH_OPERATOR(LOCAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS,
                           local_cheapest_insertion_close_nodes_lns, operators);
  operator_groups.push_back(solver_->ConcatenateOperators(operators));

  // Third local search loop: Expensive LNS operators.
  operators.clear();
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::GENERIC_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(TSP_OPT, tsp_opt, operators);
  }
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::GENERIC_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(TSP_LNS, tsp_lns, operators);
  }
  CP_ROUTING_PUSH_OPERATOR(FULL_PATH_LNS, full_path_lns, operators);
  CP_ROUTING_PUSH_OPERATOR(PATH_LNS, path_lns, operators);
  if (!disjunctions_.empty()) {
    CP_ROUTING_PUSH_OPERATOR(INACTIVE_LNS, inactive_lns, operators);
  }
  operator_groups.push_back(solver_->ConcatenateOperators(operators));

  return solver_->ConcatenateOperators(operator_groups);
}

#undef CP_ROUTING_PUSH_OPERATOR

std::vector<LocalSearchFilter*> RoutingModel::GetOrCreateLocalSearchFilters(
    const RoutingSearchParameters& parameters) {
  // As of 2013/01, three filters evaluate sub-parts of the objective
  // function:
  // - NodeDisjunctionFilter: takes disjunction penalty costs into account,
  // - PathCumulFilter: takes dimension span costs into account,
  // - ObjectiveFilter:
  //     - VehicleAmortizedCostFilter, which considers the part of the cost
  //       related to amortized linear and quadratic vehicle cost factors.
  //     - LocalSearchObjectiveFilter, which takes dimension "arc" costs into
  //       account.
  std::vector<LocalSearchFilter*> filters;
  // VehicleAmortizedCostFilter can have a negative value, so it must be first.
  if (vehicle_amortized_cost_factors_set_) {
    filters.push_back(MakeVehicleAmortizedCostFilter(*this));
  }

  // The SumObjectiveFilter has the best reject/second ratio in practice,
  // so it is the earliest.
  if (CostsAreHomogeneousAcrossVehicles()) {
    filters.push_back(solver_->MakeSumObjectiveFilter(
        nexts_, [this](int64 i, int64 j) { return GetHomogeneousCost(i, j); },
        Solver::LE));
  } else {
    filters.push_back(solver_->MakeSumObjectiveFilter(
        nexts_, vehicle_vars_,
        [this](int64 i, int64 j, int64 k) {
          return GetArcCostForVehicle(i, j, k);
        },
        Solver::LE));
  }

  filters.push_back(solver_->MakeVariableDomainFilter());

  if (vehicles_ > max_active_vehicles_) {
    filters.push_back(MakeMaxActiveVehiclesFilter(*this));
  }

  if (!disjunctions_.empty()) {
    filters.push_back(MakeNodeDisjunctionFilter(*this));
  }

  if (!pickup_delivery_pairs_.empty()) {
    filters.push_back(MakePickupDeliveryFilter(
        *this, pickup_delivery_pairs_, vehicle_pickup_delivery_policy_));
  }

  if (HasTypeRegulations()) {
    filters.push_back(MakeTypeRegulationsFilter(*this));
  }

  filters.push_back(MakeVehicleVarFilter(*this));

  AppendDimensionCumulFilters(GetDimensions(), parameters,
                              /*filter_objective_cost*/ true, &filters);

  for (const RoutingDimension* dimension : dimensions_) {
    if (!dimension->HasBreakConstraints()) continue;
    filters.push_back(MakeVehicleBreaksFilter(*this, *dimension));
  }
  filters.insert(filters.end(), extra_filters_.begin(), extra_filters_.end());
  return filters;
}

LocalSearchFilterManager* RoutingModel::GetOrCreateLocalSearchFilterManager(
    const RoutingSearchParameters& parameters) {
  if (!local_search_filter_manager_) {
    local_search_filter_manager_ = solver_->MakeLocalSearchFilterManager(
        GetOrCreateLocalSearchFilters(parameters));
  }
  return local_search_filter_manager_;
}

std::vector<LocalSearchFilter*> RoutingModel::GetOrCreateFeasibilityFilters(
    const RoutingSearchParameters& parameters) {
  std::vector<LocalSearchFilter*> filters;
  if (vehicles_ > max_active_vehicles_) {
    filters.push_back(MakeMaxActiveVehiclesFilter(*this));
  }
  if (!disjunctions_.empty()) {
    filters.push_back(MakeNodeDisjunctionFilter(*this));
  }
  filters.push_back(solver_->MakeVariableDomainFilter());
  if (!pickup_delivery_pairs_.empty()) {
    filters.push_back(MakePickupDeliveryFilter(
        *this, pickup_delivery_pairs_, vehicle_pickup_delivery_policy_));
  }
  if (HasTypeRegulations()) {
    filters.push_back(MakeTypeRegulationsFilter(*this));
  }
  filters.push_back(MakeVehicleVarFilter(*this));

  AppendDimensionCumulFilters(GetDimensions(), parameters,
                              /*filter_objective_cost*/ false, &filters);

  for (const RoutingDimension* dimension : dimensions_) {
    if (dimension->HasBreakConstraints()) {
      IntVarLocalSearchFilter* breaks_filter =
          MakeVehicleBreaksFilter(*this, *dimension);
      filters.push_back(breaks_filter);
    }
  }

  filters.insert(filters.end(), extra_filters_.begin(), extra_filters_.end());
  return filters;
}

LocalSearchFilterManager* RoutingModel::GetOrCreateFeasibilityFilterManager(
    const RoutingSearchParameters& parameters) {
  if (!feasibility_filter_manager_) {
    feasibility_filter_manager_ = solver_->MakeLocalSearchFilterManager(
        GetOrCreateFeasibilityFilters(parameters));
  }
  return feasibility_filter_manager_;
}

LocalSearchFilterManager*
RoutingModel::GetOrCreateStrongFeasibilityFilterManager(
    const RoutingSearchParameters& parameters) {
  if (!strong_feasibility_filter_manager_) {
    std::vector<LocalSearchFilter*> filters =
        GetOrCreateFeasibilityFilters(parameters);
    filters.push_back(MakeCPFeasibilityFilter(this));
    strong_feasibility_filter_manager_ =
        solver_->MakeLocalSearchFilterManager(std::move(filters));
  }
  return strong_feasibility_filter_manager_;
}

namespace {
bool AllTransitsPositive(const RoutingDimension& dimension) {
  for (int vehicle = 0; vehicle < dimension.model()->vehicles(); vehicle++) {
    if (!dimension.AreVehicleTransitsPositive(vehicle)) {
      return false;
    }
  }
  return true;
}
}  // namespace

void RoutingModel::StoreDimensionCumulOptimizers(
    const RoutingSearchParameters& parameters) {
  Assignment* packed_dimensions_collector_assignment =
      solver_->MakeAssignment();
  const int num_dimensions = dimensions_.size();
  local_optimizer_index_.resize(num_dimensions, -1);
  global_optimizer_index_.resize(num_dimensions, -1);
  for (DimensionIndex dim = DimensionIndex(0); dim < num_dimensions; dim++) {
    RoutingDimension* dimension = dimensions_[dim];
    if (dimension->global_span_cost_coefficient() > 0 ||
        !dimension->GetNodePrecedences().empty()) {
      // Use global optimizer.
      global_optimizer_index_[dim] = global_dimension_optimizers_.size();
      global_dimension_optimizers_.push_back(
          absl::make_unique<GlobalDimensionCumulOptimizer>(dimension));
      packed_dimensions_collector_assignment->Add(dimension->cumuls());
      if (!AllTransitsPositive(*dimension)) {
        dimension->SetOffsetForGlobalOptimizer(0);
        continue;
      }
      int64 offset = vehicles() == 0 ? 0 : kint64max;
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        DCHECK_GE(dimension->CumulVar(Start(vehicle))->Min(), 0);
        offset =
            std::min(offset, dimension->CumulVar(Start(vehicle))->Min() - 1);
      }
      dimension->SetOffsetForGlobalOptimizer(std::max(Zero(), offset));
    } else {
      bool has_span_cost = false;
      bool has_span_limit = false;
      std::vector<int64> vehicle_offsets(vehicles());
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (dimension->GetSpanCostCoefficientForVehicle(vehicle) > 0) {
          has_span_cost = true;
        }
        if (dimension->GetSpanUpperBoundForVehicle(vehicle) < kint64max) {
          has_span_limit = true;
        }
        DCHECK_GE(dimension->CumulVar(Start(vehicle))->Min(), 0);
        vehicle_offsets[vehicle] =
            dimension->AreVehicleTransitsPositive(vehicle)
                ? std::max(Zero(),
                           dimension->CumulVar(Start(vehicle))->Min() - 1)
                : 0;
      }
      bool has_soft_lower_bound = false;
      bool has_soft_upper_bound = false;
      for (int i = 0; i < dimension->cumuls().size(); ++i) {
        if (dimension->HasCumulVarSoftLowerBound(i)) {
          has_soft_lower_bound = true;
        }
        if (dimension->HasCumulVarSoftUpperBound(i)) {
          has_soft_upper_bound = true;
        }
      }
      int num_linear_constraints = 0;
      if (has_span_cost) ++num_linear_constraints;
      if (has_span_limit) ++num_linear_constraints;
      if (dimension->HasSoftSpanUpperBounds()) ++num_linear_constraints;
      if (has_soft_lower_bound) ++num_linear_constraints;
      if (has_soft_upper_bound) ++num_linear_constraints;
      if (dimension->HasBreakConstraints()) ++num_linear_constraints;
      if (num_linear_constraints >= 2) {
        dimension->SetVehicleOffsetsForLocalOptimizer(
            std::move(vehicle_offsets));
        local_optimizer_index_[dim] = local_dimension_optimizers_.size();
        local_dimension_optimizers_.push_back(
            absl::make_unique<LocalDimensionCumulOptimizer>(
                dimension, parameters.continuous_scheduling_solver()));
        bool has_intervals = false;
        for (const SortedDisjointIntervalList& intervals :
             dimension->forbidden_intervals()) {
          // TODO(user): Change the following test to check intervals within
          // the domain of the corresponding variables.
          if (intervals.NumIntervals() > 0) {
            has_intervals = true;
            break;
          }
        }
        if (dimension->HasBreakConstraints() || has_intervals) {
          local_dimension_mp_optimizers_.push_back(
              absl::make_unique<LocalDimensionCumulOptimizer>(
                  dimension, parameters.mixed_integer_scheduling_solver()));
        } else {
          local_dimension_mp_optimizers_.push_back(nullptr);
        }
        packed_dimensions_collector_assignment->Add(dimension->cumuls());
      }
    }
    DCHECK_EQ(local_dimension_mp_optimizers_.size(),
              local_dimension_optimizers_.size());
  }

  // NOTE(b/129252839): We also add all other extra variables to the
  // packed_dimensions_collector_assignment to make sure the necessary
  // propagations on these variables after packing are correctly stored.
  for (IntVar* const extra_var : extra_vars_) {
    packed_dimensions_collector_assignment->Add(extra_var);
  }
  for (IntervalVar* const extra_interval : extra_intervals_) {
    packed_dimensions_collector_assignment->Add(extra_interval);
  }

  packed_dimensions_assignment_collector_ = solver_->MakeFirstSolutionCollector(
      packed_dimensions_collector_assignment);
}

std::vector<RoutingDimension*> RoutingModel::GetDimensionsWithSoftOrSpanCosts()
    const {
  std::vector<RoutingDimension*> dimensions;
  for (RoutingDimension* dimension : dimensions_) {
    bool has_soft_or_span_cost = false;
    for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
      if (dimension->GetSpanCostCoefficientForVehicle(vehicle) > 0) {
        has_soft_or_span_cost = true;
        break;
      }
    }
    if (!has_soft_or_span_cost) {
      for (int i = 0; i < dimension->cumuls().size(); ++i) {
        if (dimension->HasCumulVarSoftUpperBound(i) ||
            dimension->HasCumulVarSoftLowerBound(i)) {
          has_soft_or_span_cost = true;
          break;
        }
      }
    }
    if (has_soft_or_span_cost) dimensions.push_back(dimension);
  }
  return dimensions;
}

DecisionBuilder*
RoutingModel::CreateFinalizerForMinimizedAndMaximizedVariables() {
  std::stable_sort(finalizer_variable_cost_pairs_.begin(),
                   finalizer_variable_cost_pairs_.end(),
                   [](const std::pair<IntVar*, int64>& var_cost1,
                      const std::pair<IntVar*, int64>& var_cost2) {
                     return var_cost1.second > var_cost2.second;
                   });
  const int num_variables = finalizer_variable_cost_pairs_.size() +
                            finalizer_variable_target_pairs_.size();
  std::vector<IntVar*> variables;
  std::vector<int64> targets;
  variables.reserve(num_variables);
  targets.reserve(num_variables);
  for (const auto& variable_cost : finalizer_variable_cost_pairs_) {
    variables.push_back(variable_cost.first);
    targets.push_back(kint64min);
  }
  for (const auto& variable_target : finalizer_variable_target_pairs_) {
    variables.push_back(variable_target.first);
    targets.push_back(variable_target.second);
  }
  return MakeSetValuesFromTargets(solver(), std::move(variables),
                                  std::move(targets));
}

DecisionBuilder* RoutingModel::CreateSolutionFinalizer(SearchLimit* lns_limit) {
  std::vector<DecisionBuilder*> decision_builders;
  decision_builders.push_back(solver_->MakePhase(
      nexts_, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE));

  if (!local_dimension_optimizers_.empty()) {
    decision_builders.push_back(
        solver_->RevAlloc(new SetCumulsFromLocalDimensionCosts(
            &local_dimension_optimizers_, &local_dimension_mp_optimizers_,
            lns_limit)));
  }
  if (!global_dimension_optimizers_.empty()) {
    decision_builders.push_back(
        solver_->RevAlloc(new SetCumulsFromGlobalDimensionCosts(
            &global_dimension_optimizers_, lns_limit)));
  }
  decision_builders.push_back(
      CreateFinalizerForMinimizedAndMaximizedVariables());

  return solver_->Compose(decision_builders);
}

void RoutingModel::CreateFirstSolutionDecisionBuilders(
    const RoutingSearchParameters& search_parameters) {
  first_solution_decision_builders_.resize(
      FirstSolutionStrategy_Value_Value_ARRAYSIZE, nullptr);
  first_solution_filtered_decision_builders_.resize(
      FirstSolutionStrategy_Value_Value_ARRAYSIZE, nullptr);
  DecisionBuilder* const finalize_solution =
      CreateSolutionFinalizer(GetOrCreateLargeNeighborhoodSearchLimit());
  // Default heuristic
  first_solution_decision_builders_
      [FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE] = finalize_solution;
  // Global cheapest addition heuristic.
  first_solution_decision_builders_
      [FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC] = solver_->MakePhase(
          nexts_,
          [this](int64 i, int64 j) { return GetArcCostForFirstSolution(i, j); },
          Solver::CHOOSE_STATIC_GLOBAL_BEST);
  // Cheapest addition heuristic.
  Solver::IndexEvaluator2 eval = [this](int64 i, int64 j) {
    return GetArcCostForFirstSolution(i, j);
  };
  first_solution_decision_builders_[FirstSolutionStrategy::LOCAL_CHEAPEST_ARC] =
      solver_->MakePhase(nexts_, Solver::CHOOSE_FIRST_UNBOUND, eval);
  // Path-based cheapest addition heuristic.
  first_solution_decision_builders_[FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
      solver_->MakePhase(nexts_, Solver::CHOOSE_PATH, eval);
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
            solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
                absl::make_unique<EvaluatorCheapestAdditionFilteredHeuristic>(
                    this,
                    [this](int64 i, int64 j) {
                      return GetArcCostForFirstSolution(i, j);
                    },
                    GetOrCreateFeasibilityFilterManager(search_parameters))));
    first_solution_decision_builders_
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
            solver_->Try(first_solution_filtered_decision_builders_
                             [FirstSolutionStrategy::PATH_CHEAPEST_ARC],
                         first_solution_decision_builders_
                             [FirstSolutionStrategy::PATH_CHEAPEST_ARC]);
  }
  // Path-based most constrained arc addition heuristic.
  Solver::VariableValueComparator comp = [this](int64 i, int64 j, int64 k) {
    return ArcIsMoreConstrainedThanArc(i, j, k);
  };

  first_solution_decision_builders_
      [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] =
          solver_->MakePhase(nexts_, Solver::CHOOSE_PATH, comp);
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_
        [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] =
            solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
                absl::make_unique<ComparatorCheapestAdditionFilteredHeuristic>(
                    this, comp,
                    GetOrCreateFeasibilityFilterManager(search_parameters))));
    first_solution_decision_builders_
        [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] = solver_->Try(
            first_solution_filtered_decision_builders_
                [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC],
            first_solution_decision_builders_
                [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC]);
  }
  // Evaluator-based path heuristic.
  if (first_solution_evaluator_ != nullptr) {
    first_solution_decision_builders_
        [FirstSolutionStrategy::EVALUATOR_STRATEGY] = solver_->MakePhase(
            nexts_, Solver::CHOOSE_PATH, first_solution_evaluator_);
  } else {
    first_solution_decision_builders_
        [FirstSolutionStrategy::EVALUATOR_STRATEGY] = nullptr;
  }
  // All unperformed heuristic.
  first_solution_decision_builders_[FirstSolutionStrategy::ALL_UNPERFORMED] =
      solver_->RevAlloc(new AllUnperformed(this));
  // Best insertion heuristic.
  RegularLimit* const ls_limit =
      solver_->MakeLimit(GetTimeLimit(search_parameters), kint64max, kint64max,
                         kint64max, /*smart_time_check=*/true);
  DecisionBuilder* const finalize = solver_->MakeSolveOnce(
      finalize_solution, GetOrCreateLargeNeighborhoodSearchLimit());
  LocalSearchPhaseParameters* const insertion_parameters =
      solver_->MakeLocalSearchPhaseParameters(
          nullptr, CreateInsertionOperator(), finalize, ls_limit,
          GetOrCreateLocalSearchFilterManager(search_parameters));
  std::vector<IntVar*> decision_vars = nexts_;
  if (!CostsAreHomogeneousAcrossVehicles()) {
    decision_vars.insert(decision_vars.end(), vehicle_vars_.begin(),
                         vehicle_vars_.end());
  }
  const int64 optimization_step = std::max(
      MathUtil::FastInt64Round(search_parameters.optimization_step()), One());
  first_solution_decision_builders_[FirstSolutionStrategy::BEST_INSERTION] =
      solver_->MakeNestedOptimize(
          solver_->MakeLocalSearchPhase(
              decision_vars, solver_->RevAlloc(new AllUnperformed(this)),
              insertion_parameters),
          GetOrCreateAssignment(), false, optimization_step);
  first_solution_decision_builders_[FirstSolutionStrategy::BEST_INSERTION] =
      solver_->Compose(first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION],
                       finalize);

  // Parallel/Sequential Global cheapest insertion
  GlobalCheapestInsertionFilteredHeuristic::GlobalCheapestInsertionParameters
      gci_parameters = {
          /* is_sequential */ false,
          search_parameters.cheapest_insertion_farthest_seeds_ratio(),
          search_parameters.cheapest_insertion_first_solution_neighbors_ratio(),
          /* use_neighbors_ratio_for_initialization */ false};
  for (bool is_sequential : {false, true}) {
    FirstSolutionStrategy::Value first_solution_strategy =
        is_sequential ? FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION
                      : FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION;
    gci_parameters.is_sequential = is_sequential;

    first_solution_filtered_decision_builders_[first_solution_strategy] =
        solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
            absl::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
                this,
                [this](int64 i, int64 j, int64 vehicle) {
                  return GetArcCostForVehicle(i, j, vehicle);
                },
                [this](int64 i) { return UnperformedPenaltyOrValue(0, i); },
                GetOrCreateFeasibilityFilterManager(search_parameters),
                gci_parameters)));
    IntVarFilteredDecisionBuilder* const strong_gci =
        solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
            absl::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
                this,
                [this](int64 i, int64 j, int64 vehicle) {
                  return GetArcCostForVehicle(i, j, vehicle);
                },
                [this](int64 i) { return UnperformedPenaltyOrValue(0, i); },
                GetOrCreateStrongFeasibilityFilterManager(search_parameters),
                gci_parameters)));
    first_solution_decision_builders_[first_solution_strategy] = solver_->Try(
        first_solution_filtered_decision_builders_[first_solution_strategy],
        solver_->Try(strong_gci, first_solution_decision_builders_
                                     [FirstSolutionStrategy::BEST_INSERTION]));
  }

  // Local cheapest insertion
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] =
          solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
              absl::make_unique<LocalCheapestInsertionFilteredHeuristic>(
                  this,
                  [this](int64 i, int64 j, int64 vehicle) {
                    return GetArcCostForVehicle(i, j, vehicle);
                  },
                  GetOrCreateFeasibilityFilterManager(search_parameters))));
  IntVarFilteredDecisionBuilder* const strong_lci =
      solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
          absl::make_unique<LocalCheapestInsertionFilteredHeuristic>(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              GetOrCreateStrongFeasibilityFilterManager(search_parameters))));
  first_solution_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] = solver_->Try(
          first_solution_filtered_decision_builders_
              [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION],
          solver_->Try(strong_lci,
                       first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION]));
  // Savings
  SavingsFilteredHeuristic::SavingsParameters savings_parameters;
  savings_parameters.neighbors_ratio =
      search_parameters.savings_neighbors_ratio();
  savings_parameters.max_memory_usage_bytes =
      search_parameters.savings_max_memory_usage_bytes();
  savings_parameters.add_reverse_arcs =
      search_parameters.savings_add_reverse_arcs();
  savings_parameters.arc_coefficient =
      search_parameters.savings_arc_coefficient();
  LocalSearchFilterManager* filter_manager = nullptr;
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    filter_manager = GetOrCreateFeasibilityFilterManager(search_parameters);
  }

  if (search_parameters.savings_parallel_routes()) {
    IntVarFilteredDecisionBuilder* savings_db =
        solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
            absl::make_unique<ParallelSavingsFilteredHeuristic>(
                this, &manager_, savings_parameters, filter_manager)));
    if (!search_parameters.use_unfiltered_first_solution_strategy()) {
      first_solution_filtered_decision_builders_
          [FirstSolutionStrategy::SAVINGS] = savings_db;
    }

    first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->Try(savings_db,
                     solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
                         absl::make_unique<ParallelSavingsFilteredHeuristic>(
                             this, &manager_, savings_parameters,
                             GetOrCreateStrongFeasibilityFilterManager(
                                 search_parameters)))));
  } else {
    IntVarFilteredDecisionBuilder* savings_db =
        solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
            absl::make_unique<SequentialSavingsFilteredHeuristic>(
                this, &manager_, savings_parameters, filter_manager)));
    if (!search_parameters.use_unfiltered_first_solution_strategy()) {
      first_solution_filtered_decision_builders_
          [FirstSolutionStrategy::SAVINGS] = savings_db;
    }

    first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->Try(savings_db,
                     solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
                         absl::make_unique<SequentialSavingsFilteredHeuristic>(
                             this, &manager_, savings_parameters,
                             GetOrCreateStrongFeasibilityFilterManager(
                                 search_parameters)))));
  }
  // Sweep
  first_solution_decision_builders_[FirstSolutionStrategy::SWEEP] =
      solver_->RevAlloc(new SweepBuilder(this, true));
  DecisionBuilder* sweep_builder =
      solver_->RevAlloc(new SweepBuilder(this, false));
  first_solution_decision_builders_[FirstSolutionStrategy::SWEEP] =
      solver_->Try(
          sweep_builder,
          first_solution_decision_builders_[FirstSolutionStrategy::SWEEP]);
  // Christofides
  first_solution_decision_builders_[FirstSolutionStrategy::CHRISTOFIDES] =
      solver_->RevAlloc(new IntVarFilteredDecisionBuilder(
          absl::make_unique<ChristofidesFilteredHeuristic>(
              this, GetOrCreateFeasibilityFilterManager(search_parameters),
              search_parameters.christofides_use_minimum_matching())));
  // Automatic
  // TODO(user): make this smarter.
  const bool has_precedences = std::any_of(
      dimensions_.begin(), dimensions_.end(),
      [](RoutingDimension* dim) { return !dim->GetNodePrecedences().empty(); });
  if (pickup_delivery_pairs_.empty() && !has_precedences) {
    automatic_first_solution_strategy_ =
        FirstSolutionStrategy::PATH_CHEAPEST_ARC;
  } else {
    automatic_first_solution_strategy_ =
        FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION;
  }
  first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC] =
      first_solution_decision_builders_[automatic_first_solution_strategy_];
  first_solution_decision_builders_[FirstSolutionStrategy::UNSET] =
      first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC];
}

DecisionBuilder* RoutingModel::GetFirstSolutionDecisionBuilder(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  if (first_solution_strategy < first_solution_decision_builders_.size()) {
    return first_solution_decision_builders_[first_solution_strategy];
  } else {
    return nullptr;
  }
}

IntVarFilteredDecisionBuilder*
RoutingModel::GetFilteredFirstSolutionDecisionBuilderOrNull(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  return first_solution_filtered_decision_builders_[first_solution_strategy];
}

LocalSearchPhaseParameters* RoutingModel::CreateLocalSearchParameters(
    const RoutingSearchParameters& search_parameters) {
  SearchLimit* lns_limit = GetOrCreateLargeNeighborhoodSearchLimit();
  return solver_->MakeLocalSearchPhaseParameters(
      CostVar(), GetNeighborhoodOperators(search_parameters),
      solver_->MakeSolveOnce(CreateSolutionFinalizer(lns_limit), lns_limit),
      GetOrCreateLocalSearchLimit(),
      GetOrCreateLocalSearchFilterManager(search_parameters));
}

DecisionBuilder* RoutingModel::CreateLocalSearchDecisionBuilder(
    const RoutingSearchParameters& search_parameters) {
  const int size = Size();
  DecisionBuilder* first_solution =
      GetFirstSolutionDecisionBuilder(search_parameters);
  LocalSearchPhaseParameters* const parameters =
      CreateLocalSearchParameters(search_parameters);
  SearchLimit* first_solution_lns_limit =
      GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit();
  DecisionBuilder* const first_solution_sub_decision_builder =
      solver_->MakeSolveOnce(CreateSolutionFinalizer(first_solution_lns_limit),
                             first_solution_lns_limit);
  if (CostsAreHomogeneousAcrossVehicles()) {
    return solver_->MakeLocalSearchPhase(nexts_, first_solution,
                                         first_solution_sub_decision_builder,
                                         parameters);
  } else {
    const int all_size = size + size + vehicles_;
    std::vector<IntVar*> all_vars(all_size);
    for (int i = 0; i < size; ++i) {
      all_vars[i] = nexts_[i];
    }
    for (int i = size; i < all_size; ++i) {
      all_vars[i] = vehicle_vars_[i - size];
    }
    return solver_->MakeLocalSearchPhase(all_vars, first_solution,
                                         first_solution_sub_decision_builder,
                                         parameters);
  }
}

void RoutingModel::SetupDecisionBuilders(
    const RoutingSearchParameters& search_parameters) {
  if (search_parameters.use_depth_first_search()) {
    SearchLimit* first_lns_limit =
        GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit();
    solve_db_ = solver_->Compose(
        GetFirstSolutionDecisionBuilder(search_parameters),
        solver_->MakeSolveOnce(CreateSolutionFinalizer(first_lns_limit),
                               first_lns_limit));
  } else {
    solve_db_ = CreateLocalSearchDecisionBuilder(search_parameters);
  }
  CHECK(preassignment_ != nullptr);
  DecisionBuilder* restore_preassignment =
      solver_->MakeRestoreAssignment(preassignment_);
  solve_db_ = solver_->Compose(restore_preassignment, solve_db_);
  improve_db_ =
      solver_->Compose(restore_preassignment,
                       solver_->MakeLocalSearchPhase(
                           GetOrCreateAssignment(),
                           CreateLocalSearchParameters(search_parameters)));
  restore_assignment_ = solver_->Compose(
      solver_->MakeRestoreAssignment(GetOrCreateAssignment()),
      CreateSolutionFinalizer(GetOrCreateLargeNeighborhoodSearchLimit()));
  restore_tmp_assignment_ = solver_->Compose(
      restore_preassignment,
      solver_->MakeRestoreAssignment(GetOrCreateTmpAssignment()),
      CreateSolutionFinalizer(GetOrCreateLargeNeighborhoodSearchLimit()));
}

void RoutingModel::SetupMetaheuristics(
    const RoutingSearchParameters& search_parameters) {
  SearchMonitor* optimize;
  const LocalSearchMetaheuristic::Value metaheuristic =
      search_parameters.local_search_metaheuristic();
  // Some metaheuristics will effectively never terminate; warn
  // user if they fail to set a time limit.
  bool limit_too_long = !search_parameters.has_time_limit() &&
                        search_parameters.solution_limit() == kint64max;
  const int64 optimization_step = std::max(
      MathUtil::FastInt64Round(search_parameters.optimization_step()), One());
  switch (metaheuristic) {
    case LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH:
      if (CostsAreHomogeneousAcrossVehicles()) {
        optimize = solver_->MakeGuidedLocalSearch(
            false, cost_,
            [this](int64 i, int64 j) { return GetHomogeneousCost(i, j); },
            optimization_step, nexts_,
            search_parameters.guided_local_search_lambda_coefficient());
      } else {
        optimize = solver_->MakeGuidedLocalSearch(
            false, cost_,
            [this](int64 i, int64 j, int64 k) {
              return GetArcCostForVehicle(i, j, k);
            },
            optimization_step, nexts_, vehicle_vars_,
            search_parameters.guided_local_search_lambda_coefficient());
      }
      break;
    case LocalSearchMetaheuristic::SIMULATED_ANNEALING:
      optimize =
          solver_->MakeSimulatedAnnealing(false, cost_, optimization_step, 100);
      break;
    case LocalSearchMetaheuristic::TABU_SEARCH:
      optimize = solver_->MakeTabuSearch(false, cost_, optimization_step,
                                         nexts_, 10, 10, .8);
      break;
    case LocalSearchMetaheuristic::GENERIC_TABU_SEARCH: {
      std::vector<operations_research::IntVar*> tabu_vars;
      if (tabu_var_callback_) {
        tabu_vars = tabu_var_callback_(this);
      } else {
        tabu_vars.push_back(cost_);
      }
      optimize = solver_->MakeGenericTabuSearch(false, cost_, optimization_step,
                                                tabu_vars, 100);
      break;
    }
    default:
      limit_too_long = false;
      optimize = solver_->MakeMinimize(cost_, optimization_step);
  }
  if (limit_too_long) {
    LOG(WARNING) << LocalSearchMetaheuristic::Value_Name(metaheuristic)
                 << " specified without sane timeout: solve may run forever.";
  }
  monitors_.push_back(optimize);
}

void RoutingModel::SetTabuVarsCallback(GetTabuVarsCallback tabu_var_callback) {
  tabu_var_callback_ = std::move(tabu_var_callback);
}

void RoutingModel::SetupAssignmentCollector(
    const RoutingSearchParameters& search_parameters) {
  Assignment* full_assignment = solver_->MakeAssignment();
  for (const RoutingDimension* const dimension : dimensions_) {
    full_assignment->Add(dimension->cumuls());
  }
  for (IntVar* const extra_var : extra_vars_) {
    full_assignment->Add(extra_var);
  }
  for (IntervalVar* const extra_interval : extra_intervals_) {
    full_assignment->Add(extra_interval);
  }
  full_assignment->Add(nexts_);
  full_assignment->Add(active_);
  full_assignment->Add(vehicle_vars_);
  full_assignment->AddObjective(cost_);

  collect_assignments_ = solver_->MakeNBestValueSolutionCollector(
      full_assignment, search_parameters.number_of_solutions_to_collect(),
      false);
  collect_one_assignment_ =
      solver_->MakeFirstSolutionCollector(full_assignment);
  monitors_.push_back(collect_assignments_);
}

void RoutingModel::SetupTrace(
    const RoutingSearchParameters& search_parameters) {
  if (search_parameters.log_search()) {
    Solver::SearchLogParameters search_log_parameters;
    search_log_parameters.branch_period = 10000;
    search_log_parameters.objective = nullptr;
    search_log_parameters.variable = cost_;
    search_log_parameters.scaling_factor =
        search_parameters.log_cost_scaling_factor();
    search_log_parameters.offset = search_parameters.log_cost_offset();
    if (!search_parameters.log_tag().empty()) {
      const std::string tag = search_parameters.log_tag();
      search_log_parameters.display_callback = [tag]() { return tag; };
    } else {
      search_log_parameters.display_callback = nullptr;
    }
    monitors_.push_back(solver_->MakeSearchLog(search_log_parameters));
  }
}

void RoutingModel::SetupSearchMonitors(
    const RoutingSearchParameters& search_parameters) {
  monitors_.push_back(GetOrCreateLimit());
  SetupMetaheuristics(search_parameters);
  SetupAssignmentCollector(search_parameters);
  SetupTrace(search_parameters);
}

bool RoutingModel::UsesLightPropagation(
    const RoutingSearchParameters& search_parameters) const {
  return !search_parameters.use_full_propagation() &&
         !search_parameters.use_depth_first_search() &&
         search_parameters.first_solution_strategy() !=
             FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE;
}

void RoutingModel::AddWeightedVariableMinimizedByFinalizer(IntVar* var,
                                                           int64 cost) {
  CHECK(var != nullptr);
  const int index = gtl::LookupOrInsert(&finalizer_variable_cost_index_, var,
                                        finalizer_variable_cost_pairs_.size());
  if (index < finalizer_variable_cost_pairs_.size()) {
    const int64 old_cost = finalizer_variable_cost_pairs_[index].second;
    finalizer_variable_cost_pairs_[index].second = CapAdd(old_cost, cost);
  } else {
    finalizer_variable_cost_pairs_.emplace_back(var, cost);
  }
}

void RoutingModel::AddVariableTargetToFinalizer(IntVar* var, int64 target) {
  CHECK(var != nullptr);
  if (finalizer_variable_target_set_.contains(var)) return;
  finalizer_variable_target_set_.insert(var);
  finalizer_variable_target_pairs_.emplace_back(var, target);
}

void RoutingModel::AddVariableMaximizedByFinalizer(IntVar* var) {
  AddVariableTargetToFinalizer(var, kint64max);
}

void RoutingModel::AddVariableMinimizedByFinalizer(IntVar* var) {
  AddVariableTargetToFinalizer(var, kint64min);
}

void RoutingModel::SetupSearch(
    const RoutingSearchParameters& search_parameters) {
  SetupDecisionBuilders(search_parameters);
  SetupSearchMonitors(search_parameters);
}

void RoutingModel::AddToAssignment(IntVar* const var) {
  extra_vars_.push_back(var);
}

void RoutingModel::AddIntervalToAssignment(IntervalVar* const interval) {
  extra_intervals_.push_back(interval);
}

namespace {

class PathSpansAndTotalSlacks : public Constraint {
 public:
  PathSpansAndTotalSlacks(const RoutingModel* model,
                          const RoutingDimension* dimension,
                          std::vector<IntVar*> spans,
                          std::vector<IntVar*> total_slacks)
      : Constraint(model->solver()),
        model_(model),
        dimension_(dimension),
        spans_(std::move(spans)),
        total_slacks_(std::move(total_slacks)) {
    CHECK_EQ(spans_.size(), model_->vehicles());
    CHECK_EQ(total_slacks_.size(), model_->vehicles());
    vehicle_demons_.resize(model_->vehicles());
  }

  std::string DebugString() const override { return "PathSpansAndTotalSlacks"; }

  void Post() override {
    const int num_nodes = model_->VehicleVars().size();
    const int num_transits = model_->Nexts().size();
    for (int node = 0; node < num_nodes; ++node) {
      auto* demon = MakeConstraintDemon1(
          model_->solver(), this, &PathSpansAndTotalSlacks::PropagateNode,
          "PathSpansAndTotalSlacks::PropagateNode", node);
      dimension_->CumulVar(node)->WhenRange(demon);
      model_->VehicleVar(node)->WhenBound(demon);
      if (node < num_transits) {
        dimension_->TransitVar(node)->WhenRange(demon);
        dimension_->FixedTransitVar(node)->WhenBound(demon);
        model_->NextVar(node)->WhenBound(demon);
      }
    }
    for (int vehicle = 0; vehicle < spans_.size(); ++vehicle) {
      if (!spans_[vehicle] && !total_slacks_[vehicle]) continue;
      auto* demon = MakeDelayedConstraintDemon1(
          solver(), this, &PathSpansAndTotalSlacks::PropagateVehicle,
          "PathSpansAndTotalSlacks::PropagateVehicle", vehicle);
      vehicle_demons_[vehicle] = demon;
      if (spans_[vehicle]) spans_[vehicle]->WhenRange(demon);
      if (total_slacks_[vehicle]) total_slacks_[vehicle]->WhenRange(demon);
      if (dimension_->HasBreakConstraints()) {
        for (IntervalVar* b : dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
          b->WhenAnything(demon);
        }
      }
    }
  }

  // Call propagator on all vehicles.
  void InitialPropagate() override {
    for (int vehicle = 0; vehicle < spans_.size(); ++vehicle) {
      if (!spans_[vehicle] && !total_slacks_[vehicle]) continue;
      PropagateVehicle(vehicle);
    }
  }

 private:
  // Called when a path/dimension variables of the node changes,
  // this delays propagator calls until path variables (Next and VehicleVar)
  // are instantiated, which saves fruitless and multiple identical calls.
  void PropagateNode(int node) {
    if (!model_->VehicleVar(node)->Bound()) return;
    const int vehicle = model_->VehicleVar(node)->Min();
    if (vehicle < 0 || vehicle_demons_[vehicle] == nullptr) return;
    EnqueueDelayedDemon(vehicle_demons_[vehicle]);
  }

  // In order to make reasoning on span and total_slack of a vehicle uniform,
  // we rely on the fact that span == sum_fixed_transits + total_slack
  // to present both span and total_slack in terms of span and fixed transit.
  // This allows to use the same code whether there actually are variables
  // for span and total_slack or not.
  int64 SpanMin(int vehicle, int64 sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    const int64 span_min = spans_[vehicle] ? spans_[vehicle]->Min() : kint64max;
    const int64 total_slack_min =
        total_slacks_[vehicle] ? total_slacks_[vehicle]->Min() : kint64max;
    return std::min(span_min, CapAdd(total_slack_min, sum_fixed_transits));
  }
  int64 SpanMax(int vehicle, int64 sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    const int64 span_max = spans_[vehicle] ? spans_[vehicle]->Max() : kint64min;
    const int64 total_slack_max =
        total_slacks_[vehicle] ? total_slacks_[vehicle]->Max() : kint64min;
    return std::max(span_max, CapAdd(total_slack_max, sum_fixed_transits));
  }
  void SetSpanMin(int vehicle, int64 min, int64 sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    if (spans_[vehicle]) {
      spans_[vehicle]->SetMin(min);
    }
    if (total_slacks_[vehicle]) {
      total_slacks_[vehicle]->SetMin(CapSub(min, sum_fixed_transits));
    }
  }
  void SetSpanMax(int vehicle, int64 max, int64 sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    if (spans_[vehicle]) {
      spans_[vehicle]->SetMax(max);
    }
    if (total_slacks_[vehicle]) {
      total_slacks_[vehicle]->SetMax(CapSub(max, sum_fixed_transits));
    }
  }
  // Propagates span == sum_fixed_transits + total_slack.
  // This should be called at least once during PropagateVehicle().
  void SynchronizeSpanAndTotalSlack(int vehicle, int64 sum_fixed_transits) {
    DCHECK_GE(sum_fixed_transits, 0);
    IntVar* span = spans_[vehicle];
    IntVar* total_slack = total_slacks_[vehicle];
    if (!span || !total_slack) return;
    span->SetMin(CapAdd(total_slack->Min(), sum_fixed_transits));
    span->SetMax(CapAdd(total_slack->Max(), sum_fixed_transits));
    total_slack->SetMin(CapSub(span->Min(), sum_fixed_transits));
    total_slack->SetMax(CapSub(span->Max(), sum_fixed_transits));
  }

  void PropagateVehicle(int vehicle) {
    DCHECK(spans_[vehicle] || total_slacks_[vehicle]);
    const int start = model_->Start(vehicle);
    const int end = model_->End(vehicle);
    // Record path, if it is not fixed from start to end, stop here.
    // TRICKY: do not put end node yet, we look only at transits in the next
    // reasonings, we will append the end when we look at cumuls.
    {
      path_.clear();
      int curr_node = start;
      while (!model_->IsEnd(curr_node)) {
        const IntVar* next_var = model_->NextVar(curr_node);
        if (!next_var->Bound()) return;
        path_.push_back(curr_node);
        curr_node = next_var->Value();
      }
    }
    // Compute the sum of fixed transits. Fixed transit variables should all be
    // fixed, otherwise we wait to get called later when propagation does it.
    int64 sum_fixed_transits = 0;
    for (const int node : path_) {
      const IntVar* fixed_transit_var = dimension_->FixedTransitVar(node);
      if (!fixed_transit_var->Bound()) return;
      sum_fixed_transits =
          CapAdd(sum_fixed_transits, fixed_transit_var->Value());
    }

    SynchronizeSpanAndTotalSlack(vehicle, sum_fixed_transits);

    // The amount of break time that must occur during the route must be smaller
    // than span max - sum_fixed_transits. A break must occur on the route if it
    // must be after the route's start and before the route's end.
    // Propagate lower bound on span, then filter out values
    // that would force more breaks in route than possible.
    if (dimension_->HasBreakConstraints() &&
        !dimension_->GetBreakIntervalsOfVehicle(vehicle).empty()) {
      const int64 vehicle_start_max = dimension_->CumulVar(start)->Max();
      const int64 vehicle_end_min = dimension_->CumulVar(end)->Min();
      // Compute and propagate lower bound.
      int64 min_break_duration = 0;
      for (IntervalVar* br : dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
        if (!br->MustBePerformed()) continue;
        if (vehicle_start_max < br->EndMin() &&
            br->StartMax() < vehicle_end_min) {
          min_break_duration = CapAdd(min_break_duration, br->DurationMin());
        }
      }
      SetSpanMin(vehicle, CapAdd(min_break_duration, sum_fixed_transits),
                 sum_fixed_transits);
      // If a break that is not inside the route may violate slack_max,
      // we can propagate in some cases: when the break must be before or
      // must be after the route.
      // In the other cases, we cannot deduce a better bound on a CumulVar or
      // on a break, so we do nothing.
      const int64 slack_max =
          CapSub(SpanMax(vehicle, sum_fixed_transits), sum_fixed_transits);
      const int64 max_additional_slack = CapSub(slack_max, min_break_duration);
      for (IntervalVar* br : dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
        if (!br->MustBePerformed()) continue;
        // Break must be before end, detect whether it must be before start.
        if (vehicle_start_max >= br->EndMin() &&
            br->StartMax() < vehicle_end_min) {
          if (br->DurationMin() > max_additional_slack) {
            // Having the break inside would violate max_additional_slack..
            // Thus, it must be outside the route, in this case, before.
            br->SetEndMax(vehicle_start_max);
            dimension_->CumulVar(start)->SetMin(br->EndMin());
          }
        }
        // Break must be after start, detect whether it must be after end.
        // Same reasoning, in the case where the break is after.
        if (vehicle_start_max < br->EndMin() &&
            br->StartMax() >= vehicle_end_min) {
          if (br->DurationMin() > max_additional_slack) {
            br->SetStartMin(vehicle_end_min);
            dimension_->CumulVar(end)->SetMax(br->StartMax());
          }
        }
      }
    }

    // Propagate span == cumul(end) - cumul(start).
    {
      IntVar* start_cumul = dimension_->CumulVar(start);
      IntVar* end_cumul = dimension_->CumulVar(end);
      const int64 start_min = start_cumul->Min();
      const int64 start_max = start_cumul->Max();
      const int64 end_min = end_cumul->Min();
      const int64 end_max = end_cumul->Max();
      // Propagate from cumuls to span.
      const int64 span_lb = CapSub(end_min, start_max);
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      const int64 span_ub = CapSub(end_max, start_min);
      SetSpanMax(vehicle, span_ub, sum_fixed_transits);
      // Propagate from span to cumuls.
      const int64 span_min = SpanMin(vehicle, sum_fixed_transits);
      const int64 span_max = SpanMax(vehicle, sum_fixed_transits);
      const int64 slack_from_lb = CapSub(span_max, span_lb);
      const int64 slack_from_ub = CapSub(span_ub, span_min);
      // start >= start_max - (span_max - span_lb).
      start_cumul->SetMin(CapSub(start_max, slack_from_lb));
      // end <= end_min + (span_max - span_lb).
      end_cumul->SetMax(CapAdd(end_min, slack_from_lb));
      // // start <= start_min + (span_ub - span_min)
      start_cumul->SetMax(CapAdd(start_min, slack_from_ub));
      // // end >= end_max - (span_ub - span_min)
      end_cumul->SetMin(CapSub(end_max, slack_from_ub));
    }

    // Propagate sum transits == span.
    {
      // Propagate from transits to span.
      int64 span_lb = 0;
      int64 span_ub = 0;
      for (const int node : path_) {
        span_lb = CapAdd(span_lb, dimension_->TransitVar(node)->Min());
        span_ub = CapAdd(span_ub, dimension_->TransitVar(node)->Max());
      }
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      SetSpanMax(vehicle, span_ub, sum_fixed_transits);
      // Propagate from span to transits.
      // transit[i] <= transit_i_min + (span_max - span_lb)
      // transit[i] >= transit_i_max - (span_ub - span_min)
      const int64 span_min = SpanMin(vehicle, sum_fixed_transits);
      const int64 span_max = SpanMax(vehicle, sum_fixed_transits);
      const int64 slack_from_lb = CapSub(span_max, span_lb);
      const int64 slack_from_ub =
          span_ub < kint64max ? CapSub(span_ub, span_min) : kint64max;
      for (const int node : path_) {
        IntVar* transit_var = dimension_->TransitVar(node);
        const int64 transit_i_min = transit_var->Min();
        const int64 transit_i_max = transit_var->Max();
        // TRICKY: the first propagation might change transit_var->Max(),
        // but we must use the same value of transit_i_max in the computation
        // of transit[i]'s lower bound that was used for span_ub.
        transit_var->SetMax(CapAdd(transit_i_min, slack_from_lb));
        transit_var->SetMin(CapSub(transit_i_max, slack_from_ub));
      }
    }

    // TRICKY: add end node now, we will look at cumuls.
    path_.push_back(end);

    // A stronger bound: from start min of the route, go to node i+1 with time
    // max(cumul[i] + fixed_transit, cumul[i+1].Min()).
    // Record arrival time (should be the same as end cumul min).
    // Then do the reverse route, going to time
    // min(cumul[i+1] - fixed_transit, cumul[i].Max())
    // Record final time as departure time.
    // Then arrival time - departure time is a valid lower bound of span.
    // First reasoning: start - end - start
    {
      int64 arrival_time = dimension_->CumulVar(start)->Min();
      for (int i = 1; i < path_.size(); ++i) {
        arrival_time =
            std::max(CapAdd(arrival_time,
                            dimension_->FixedTransitVar(path_[i - 1])->Min()),
                     dimension_->CumulVar(path_[i])->Min());
      }
      int64 departure_time = arrival_time;
      for (int i = path_.size() - 2; i >= 0; --i) {
        departure_time =
            std::min(CapSub(departure_time,
                            dimension_->FixedTransitVar(path_[i])->Min()),
                     dimension_->CumulVar(path_[i])->Max());
      }
      const int64 span_lb = CapSub(arrival_time, departure_time);
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      const int64 maximum_deviation =
          CapSub(SpanMax(vehicle, sum_fixed_transits), span_lb);
      const int64 start_lb = CapSub(departure_time, maximum_deviation);
      dimension_->CumulVar(start)->SetMin(start_lb);
    }
    // Second reasoning: end - start - end
    {
      int64 departure_time = dimension_->CumulVar(end)->Max();
      for (int i = path_.size() - 2; i >= 0; --i) {
        const int curr_node = path_[i];
        departure_time =
            std::min(CapSub(departure_time,
                            dimension_->FixedTransitVar(curr_node)->Min()),
                     dimension_->CumulVar(curr_node)->Max());
      }
      int arrival_time = departure_time;
      for (int i = 1; i < path_.size(); ++i) {
        arrival_time =
            std::max(CapAdd(arrival_time,
                            dimension_->FixedTransitVar(path_[i - 1])->Min()),
                     dimension_->CumulVar(path_[i])->Min());
      }
      const int64 span_lb = CapSub(arrival_time, departure_time);
      SetSpanMin(vehicle, span_lb, sum_fixed_transits);
      const int64 maximum_deviation =
          CapSub(SpanMax(vehicle, sum_fixed_transits), span_lb);
      dimension_->CumulVar(end)->SetMax(
          CapAdd(arrival_time, maximum_deviation));
    }
  }

  const RoutingModel* const model_;
  const RoutingDimension* const dimension_;
  std::vector<IntVar*> spans_;
  std::vector<IntVar*> total_slacks_;
  std::vector<int> path_;
  std::vector<Demon*> vehicle_demons_;
};

}  // namespace

Constraint* RoutingModel::MakePathSpansAndTotalSlacks(
    const RoutingDimension* dimension, std::vector<IntVar*> spans,
    std::vector<IntVar*> total_slacks) {
  CHECK_EQ(vehicles_, spans.size());
  CHECK_EQ(vehicles_, total_slacks.size());
  return solver()->RevAlloc(
      new PathSpansAndTotalSlacks(this, dimension, spans, total_slacks));
}

const char RoutingModelVisitor::kLightElement[] = "LightElement";
const char RoutingModelVisitor::kLightElement2[] = "LightElement2";
const char RoutingModelVisitor::kRemoveValues[] = "RemoveValues";

RoutingDimension::RoutingDimension(RoutingModel* model,
                                   std::vector<int64> vehicle_capacities,
                                   const std::string& name,
                                   const RoutingDimension* base_dimension)
    : vehicle_capacities_(std::move(vehicle_capacities)),
      base_dimension_(base_dimension),
      global_span_cost_coefficient_(0),
      model_(model),
      name_(name),
      global_optimizer_offset_(0) {
  CHECK(model != nullptr);
  vehicle_span_upper_bounds_.assign(model->vehicles(), kint64max);
  vehicle_span_cost_coefficients_.assign(model->vehicles(), 0);
}

RoutingDimension::RoutingDimension(RoutingModel* model,
                                   std::vector<int64> vehicle_capacities,
                                   const std::string& name, SelfBased)
    : RoutingDimension(model, std::move(vehicle_capacities), name, this) {}

RoutingDimension::~RoutingDimension() {
  cumul_var_piecewise_linear_cost_.clear();
}

void RoutingDimension::Initialize(
    const std::vector<int>& transit_evaluators,
    const std::vector<int>& state_dependent_transit_evaluators,
    int64 slack_max) {
  InitializeCumuls();
  InitializeTransits(transit_evaluators, state_dependent_transit_evaluators,
                     slack_max);
}

namespace {
// Very light version of the RangeLessOrEqual constraint (see ./range_cst.cc).
// Only performs initial propagation and then checks the compatibility of the
// variable domains without domain pruning.
// This is useful when to avoid ping-pong effects with costly constraints
// such as the PathCumul constraint.
// This constraint has not been added to the cp library (in range_cst.cc) given
// it only does checking and no propagation (except the initial propagation)
// and is only fit for local search, in particular in the context of vehicle
// routing.
class LightRangeLessOrEqual : public Constraint {
 public:
  LightRangeLessOrEqual(Solver* const s, IntExpr* const l, IntExpr* const r);
  ~LightRangeLessOrEqual() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override {
    return solver()->MakeIsLessOrEqualVar(left_, right_);
  }
  // TODO(user): introduce a kLightLessOrEqual tag.
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
  }

 private:
  void CheckRange();

  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

LightRangeLessOrEqual::LightRangeLessOrEqual(Solver* const s, IntExpr* const l,
                                             IntExpr* const r)
    : Constraint(s), left_(l), right_(r), demon_(nullptr) {}

void LightRangeLessOrEqual::Post() {
  demon_ = MakeConstraintDemon0(
      solver(), this, &LightRangeLessOrEqual::CheckRange, "CheckRange");
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
}

void LightRangeLessOrEqual::InitialPropagate() {
  left_->SetMax(right_->Max());
  right_->SetMin(left_->Min());
  if (left_->Max() <= right_->Min()) {
    demon_->inhibit(solver());
  }
}

void LightRangeLessOrEqual::CheckRange() {
  if (left_->Min() > right_->Max()) {
    solver()->Fail();
  }
  if (left_->Max() <= right_->Min()) {
    demon_->inhibit(solver());
  }
}

std::string LightRangeLessOrEqual::DebugString() const {
  return left_->DebugString() + " < " + right_->DebugString();
}

}  // namespace

void RoutingDimension::InitializeCumuls() {
  Solver* const solver = model_->solver();
  const int size = model_->Size() + model_->vehicles();
  const auto capacity_range = std::minmax_element(vehicle_capacities_.begin(),
                                                  vehicle_capacities_.end());
  const int64 min_capacity = *capacity_range.first;
  CHECK_GE(min_capacity, 0);
  const int64 max_capacity = *capacity_range.second;
  solver->MakeIntVarArray(size, 0, max_capacity, name_, &cumuls_);
  // Refine the min/max for vehicle start/ends based on vehicle capacities.
  for (int v = 0; v < model_->vehicles(); v++) {
    const int64 vehicle_capacity = vehicle_capacities_[v];
    cumuls_[model_->Start(v)]->SetMax(vehicle_capacity);
    cumuls_[model_->End(v)]->SetMax(vehicle_capacity);
  }

  forbidden_intervals_.resize(size);
  capacity_vars_.clear();
  if (min_capacity != max_capacity) {
    solver->MakeIntVarArray(size, 0, kint64max, &capacity_vars_);
    for (int i = 0; i < size; ++i) {
      IntVar* const capacity_var = capacity_vars_[i];
      if (i < model_->Size()) {
        IntVar* const capacity_active = solver->MakeBoolVar();
        solver->AddConstraint(
            solver->MakeLessOrEqual(model_->ActiveVar(i), capacity_active));
        solver->AddConstraint(solver->MakeIsLessOrEqualCt(
            cumuls_[i], capacity_var, capacity_active));
      } else {
        solver->AddConstraint(
            solver->MakeLessOrEqual(cumuls_[i], capacity_var));
      }
    }
  }
}

namespace {
template <int64 value>
int64 IthElementOrValue(const std::vector<int64>& v, int64 index) {
  return index >= 0 ? v[index] : value;
}

void ComputeTransitClasses(const std::vector<int>& evaluator_indices,
                           std::vector<int>* class_evaluators,
                           std::vector<int64>* vehicle_to_class) {
  CHECK(class_evaluators != nullptr);
  CHECK(vehicle_to_class != nullptr);
  class_evaluators->clear();
  vehicle_to_class->resize(evaluator_indices.size(), -1);
  absl::flat_hash_map<int, int64> evaluator_to_class;
  for (int i = 0; i < evaluator_indices.size(); ++i) {
    const int evaluator_index = evaluator_indices[i];
    int evaluator_class = -1;
    if (!gtl::FindCopy(evaluator_to_class, evaluator_index, &evaluator_class)) {
      evaluator_class = class_evaluators->size();
      evaluator_to_class[evaluator_index] = evaluator_class;
      class_evaluators->push_back(evaluator_index);
    }
    (*vehicle_to_class)[i] = evaluator_class;
  }
}
}  // namespace

void RoutingDimension::InitializeTransitVariables(int64 slack_max) {
  CHECK(!class_evaluators_.empty());
  CHECK(base_dimension_ == nullptr ||
        !state_dependent_class_evaluators_.empty());

  Solver* const solver = model_->solver();
  const int size = model_->Size();
  const Solver::IndexEvaluator1 dependent_vehicle_class_function =
      [this](int index) {
        return (0 <= index && index < state_dependent_vehicle_to_class_.size())
                   ? state_dependent_vehicle_to_class_[index]
                   : state_dependent_class_evaluators_.size();
      };
  const std::string slack_name = name_ + " slack";
  const std::string transit_name = name_ + " fixed transit";

  for (int64 i = 0; i < size; ++i) {
    fixed_transits_[i] =
        solver->MakeIntVar(kint64min, kint64max, absl::StrCat(transit_name, i));
    // Setting dependent_transits_[i].
    if (base_dimension_ != nullptr) {
      if (state_dependent_class_evaluators_.size() == 1) {
        std::vector<IntVar*> transition_variables(cumuls_.size(), nullptr);
        for (int64 j = 0; j < cumuls_.size(); ++j) {
          transition_variables[j] =
              MakeRangeMakeElementExpr(
                  model_
                      ->StateDependentTransitCallback(
                          state_dependent_class_evaluators_[0])(i, j)
                      .transit,
                  base_dimension_->CumulVar(i), solver)
                  ->Var();
        }
        dependent_transits_[i] =
            solver->MakeElement(transition_variables, model_->NextVar(i))
                ->Var();
      } else {
        IntVar* const vehicle_class_var =
            solver
                ->MakeElement(dependent_vehicle_class_function,
                              model_->VehicleVar(i))
                ->Var();
        std::vector<IntVar*> transit_for_vehicle;
        transit_for_vehicle.reserve(state_dependent_class_evaluators_.size() +
                                    1);
        for (int evaluator : state_dependent_class_evaluators_) {
          std::vector<IntVar*> transition_variables(cumuls_.size(), nullptr);
          for (int64 j = 0; j < cumuls_.size(); ++j) {
            transition_variables[j] =
                MakeRangeMakeElementExpr(
                    model_->StateDependentTransitCallback(evaluator)(i, j)
                        .transit,
                    base_dimension_->CumulVar(i), solver)
                    ->Var();
          }
          transit_for_vehicle.push_back(
              solver->MakeElement(transition_variables, model_->NextVar(i))
                  ->Var());
        }
        transit_for_vehicle.push_back(solver->MakeIntConst(0));
        dependent_transits_[i] =
            solver->MakeElement(transit_for_vehicle, vehicle_class_var)->Var();
      }
    } else {
      dependent_transits_[i] = solver->MakeIntConst(0);
    }

    // Summing fixed transits, dependent transits and the slack.
    IntExpr* transit_expr = fixed_transits_[i];
    if (dependent_transits_[i]->Min() != 0 ||
        dependent_transits_[i]->Max() != 0) {
      transit_expr = solver->MakeSum(transit_expr, dependent_transits_[i]);
    }

    if (slack_max == 0) {
      slacks_[i] = solver->MakeIntConst(0);
    } else {
      slacks_[i] =
          solver->MakeIntVar(0, slack_max, absl::StrCat(slack_name, i));
      transit_expr = solver->MakeSum(slacks_[i], transit_expr);
    }
    transits_[i] = transit_expr->Var();
  }
}

void RoutingDimension::InitializeTransits(
    const std::vector<int>& transit_evaluators,
    const std::vector<int>& state_dependent_transit_evaluators,
    int64 slack_max) {
  CHECK_EQ(model_->vehicles(), transit_evaluators.size());
  CHECK(base_dimension_ == nullptr ||
        model_->vehicles() == state_dependent_transit_evaluators.size());
  const int size = model_->Size();
  transits_.resize(size, nullptr);
  fixed_transits_.resize(size, nullptr);
  slacks_.resize(size, nullptr);
  dependent_transits_.resize(size, nullptr);
  ComputeTransitClasses(transit_evaluators, &class_evaluators_,
                        &vehicle_to_class_);
  if (base_dimension_ != nullptr) {
    ComputeTransitClasses(state_dependent_transit_evaluators,
                          &state_dependent_class_evaluators_,
                          &state_dependent_vehicle_to_class_);
  }

  InitializeTransitVariables(slack_max);
}

void FillPathEvaluation(const std::vector<int64>& path,
                        const RoutingModel::TransitCallback2& evaluator,
                        std::vector<int64>* values) {
  const int num_nodes = path.size();
  values->resize(num_nodes - 1);
  for (int i = 0; i < num_nodes - 1; ++i) {
    (*values)[i] = evaluator(path[i], path[i + 1]);
  }
}

TypeRegulationsChecker::TypeRegulationsChecker(const RoutingModel& model)
    : model_(model), occurrences_of_type_(model.GetNumberOfVisitTypes()) {}

bool TypeRegulationsChecker::CheckVehicle(
    int vehicle, const std::function<int64(int64)>& next_accessor) {
  if (!HasRegulationsToCheck()) {
    return true;
  }

  InitializeCheck(vehicle, next_accessor);

  for (int pos = 0; pos < current_route_visits_.size(); pos++) {
    const int64 current_visit = current_route_visits_[pos];
    const int type = model_.GetVisitType(current_visit);
    if (type < 0) {
      continue;
    }
    const VisitTypePolicy policy = model_.GetVisitTypePolicy(current_visit);

    DCHECK_LT(type, occurrences_of_type_.size());
    int& num_type_added = occurrences_of_type_[type].num_type_added_to_vehicle;
    int& num_type_removed =
        occurrences_of_type_[type].num_type_removed_from_vehicle;
    DCHECK_LE(num_type_removed, num_type_added);
    if (policy == RoutingModel::ADDED_TYPE_REMOVED_FROM_VEHICLE &&
        num_type_removed == num_type_added) {
      // The type is not actually being removed as all added types have already
      // been removed.
      continue;
    }

    if (!CheckTypeRegulations(type, policy, pos)) {
      return false;
    }
    // Update count of type based on the visit policy.
    if (policy == VisitTypePolicy::TYPE_ADDED_TO_VEHICLE ||
        policy == VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED) {
      num_type_added++;
    }
    if (policy == VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED ||
        policy == VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
      num_type_removed++;
    }
  }
  return FinalizeCheck();
}

void TypeRegulationsChecker::InitializeCheck(
    int vehicle, const std::function<int64(int64)>& next_accessor) {
  // Accumulates the count of types before the current node.
  // {0, 0, -1} does not compile on or-tools.
  std::fill(occurrences_of_type_.begin(), occurrences_of_type_.end(),
            TypeRegulationsChecker::TypePolicyOccurrence());

  // TODO(user): Optimize the filter to avoid scanning the route an extra
  // time when there are no TYPE_ON_VEHICLE_UP_TO_VISIT policies on the route,
  // by passing a boolean to CheckVehicle() passed to InitializeCheck().
  current_route_visits_.clear();
  for (int64 current = model_.Start(vehicle); !model_.IsEnd(current);
       current = next_accessor(current)) {
    const int type = model_.GetVisitType(current);
    if (type >= 0 && model_.GetVisitTypePolicy(current) ==
                         VisitTypePolicy::TYPE_ON_VEHICLE_UP_TO_VISIT) {
      occurrences_of_type_[type].position_of_last_type_on_vehicle_up_to_visit =
          current_route_visits_.size();
    }
    current_route_visits_.push_back(current);
  }

  OnInitializeCheck();
}

bool TypeRegulationsChecker::TypeOccursOnRoute(int type) const {
  const TypePolicyOccurrence& occurrences = occurrences_of_type_[type];
  return occurrences.num_type_added_to_vehicle > 0 ||
         occurrences.position_of_last_type_on_vehicle_up_to_visit >= 0;
}

bool TypeRegulationsChecker::TypeCurrentlyOnRoute(int type, int pos) const {
  const TypePolicyOccurrence& occurrences = occurrences_of_type_[type];
  return occurrences.num_type_removed_from_vehicle <
             occurrences.num_type_added_to_vehicle ||
         occurrences.position_of_last_type_on_vehicle_up_to_visit >= pos;
}

TypeIncompatibilityChecker::TypeIncompatibilityChecker(
    const RoutingModel& model, bool check_hard_incompatibilities)
    : TypeRegulationsChecker(model),
      check_hard_incompatibilities_(check_hard_incompatibilities) {}

bool TypeIncompatibilityChecker::HasRegulationsToCheck() const {
  return model_.HasTemporalTypeIncompatibilities() ||
         (check_hard_incompatibilities_ &&
          model_.HasHardTypeIncompatibilities());
}

// TODO(user): Remove the check_hard_incompatibilities_ boolean and always
// check both incompatibilities to simplify the code?
// TODO(user): Improve algorithm by only checking a given type if necessary?
// - For temporal incompatibilities, only check if NonDeliveredType(count) == 1.
// - For hard incompatibilities, only if NonDeliveryType(type) == 1.
bool TypeIncompatibilityChecker::CheckTypeRegulations(int type,
                                                      VisitTypePolicy policy,
                                                      int pos) {
  if (policy == VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
    // NOTE: We don't need to check incompatibilities when the type is being
    // removed from the route.
    return true;
  }
  for (int incompatible_type :
       model_.GetTemporalTypeIncompatibilitiesOfType(type)) {
    if (TypeCurrentlyOnRoute(incompatible_type, pos)) {
      return false;
    }
  }
  if (check_hard_incompatibilities_) {
    for (int incompatible_type :
         model_.GetHardTypeIncompatibilitiesOfType(type)) {
      if (TypeOccursOnRoute(incompatible_type)) {
        return false;
      }
    }
  }
  return true;
}

bool TypeRequirementChecker::HasRegulationsToCheck() const {
  return model_.HasTemporalTypeRequirements() ||
         model_.HasSameVehicleTypeRequirements();
}

bool TypeRequirementChecker::CheckRequiredTypesCurrentlyOnRoute(
    const std::vector<absl::flat_hash_set<int>>& required_type_alternatives,
    int pos) {
  for (const absl::flat_hash_set<int>& requirement_alternatives :
       required_type_alternatives) {
    bool has_one_of_alternatives = false;
    for (int type_alternative : requirement_alternatives) {
      if (TypeCurrentlyOnRoute(type_alternative, pos)) {
        has_one_of_alternatives = true;
        break;
      }
    }
    if (!has_one_of_alternatives) {
      return false;
    }
  }
  return true;
}

bool TypeRequirementChecker::CheckTypeRegulations(int type,
                                                  VisitTypePolicy policy,
                                                  int pos) {
  if (policy == RoutingModel::TYPE_ADDED_TO_VEHICLE ||
      policy == RoutingModel::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED) {
    if (!CheckRequiredTypesCurrentlyOnRoute(
            model_.GetRequiredTypeAlternativesWhenAddingType(type), pos)) {
      return false;
    }
  }
  if (policy != RoutingModel::TYPE_ADDED_TO_VEHICLE) {
    if (!CheckRequiredTypesCurrentlyOnRoute(
            model_.GetRequiredTypeAlternativesWhenRemovingType(type), pos)) {
      return false;
    }
  }
  if (policy != RoutingModel::ADDED_TYPE_REMOVED_FROM_VEHICLE &&
      !model_.GetSameVehicleRequiredTypeAlternativesOfType(type).empty()) {
    types_with_same_vehicle_requirements_on_route_.insert(type);
  }
  return true;
}

bool TypeRequirementChecker::FinalizeCheck() const {
  for (int type : types_with_same_vehicle_requirements_on_route_) {
    for (const absl::flat_hash_set<int>& requirement_alternatives :
         model_.GetSameVehicleRequiredTypeAlternativesOfType(type)) {
      bool has_one_of_alternatives = false;
      for (const int type_alternative : requirement_alternatives) {
        if (TypeOccursOnRoute(type_alternative)) {
          has_one_of_alternatives = true;
          break;
        }
      }
      if (!has_one_of_alternatives) {
        return false;
      }
    }
  }
  return true;
}

TypeRegulationsConstraint::TypeRegulationsConstraint(const RoutingModel& model)
    : Constraint(model.solver()),
      model_(model),
      incompatibility_checker_(model, /*check_hard_incompatibilities*/ true),
      requirement_checker_(model),
      vehicle_demons_(model.vehicles()) {}

void TypeRegulationsConstraint::PropagateNodeRegulations(int node) {
  DCHECK_LT(node, model_.Size());
  if (!model_.VehicleVar(node)->Bound() || !model_.NextVar(node)->Bound()) {
    // Vehicle var or Next var not bound.
    return;
  }
  const int vehicle = model_.VehicleVar(node)->Min();
  if (vehicle < 0) return;
  DCHECK(vehicle_demons_[vehicle] != nullptr);
  EnqueueDelayedDemon(vehicle_demons_[vehicle]);
}

void TypeRegulationsConstraint::CheckRegulationsOnVehicle(int vehicle) {
  const auto next_accessor = [this, vehicle](int64 node) {
    if (model_.NextVar(node)->Bound()) {
      return model_.NextVar(node)->Value();
    }
    // Node not bound, skip to the end of the vehicle.
    return model_.End(vehicle);
  };
  if (!incompatibility_checker_.CheckVehicle(vehicle, next_accessor) ||
      !requirement_checker_.CheckVehicle(vehicle, next_accessor)) {
    model_.solver()->Fail();
  }
}

void TypeRegulationsConstraint::Post() {
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    vehicle_demons_[vehicle] = MakeDelayedConstraintDemon1(
        solver(), this, &TypeRegulationsConstraint::CheckRegulationsOnVehicle,
        "CheckRegulationsOnVehicle", vehicle);
  }
  for (int node = 0; node < model_.Size(); node++) {
    Demon* node_demon = MakeConstraintDemon1(
        solver(), this, &TypeRegulationsConstraint::PropagateNodeRegulations,
        "PropagateNodeRegulations", node);
    model_.NextVar(node)->WhenBound(node_demon);
    model_.VehicleVar(node)->WhenBound(node_demon);
  }
}

void TypeRegulationsConstraint::InitialPropagate() {
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    CheckRegulationsOnVehicle(vehicle);
  }
}

void RoutingDimension::CloseModel(bool use_light_propagation) {
  Solver* const solver = model_->solver();
  const auto capacity_lambda = [this](int64 vehicle) {
    return vehicle >= 0 ? vehicle_capacities_[vehicle] : kint64max;
  };
  for (int i = 0; i < capacity_vars_.size(); ++i) {
    IntVar* const vehicle_var = model_->VehicleVar(i);
    IntVar* const capacity_var = capacity_vars_[i];
    if (use_light_propagation) {
      solver->AddConstraint(MakeLightElement(
          solver, capacity_var, vehicle_var, capacity_lambda,
          [this]() { return model_->enable_deep_serialization_; }));
    } else {
      solver->AddConstraint(solver->MakeEquality(
          capacity_var,
          solver->MakeElement(capacity_lambda, vehicle_var)->Var()));
    }
  }
  const Solver::IndexEvaluator1 vehicle_class_function = [this](int index) {
    return IthElementOrValue<-1>(vehicle_to_class_, index);
  };
  for (int i = 0; i < fixed_transits_.size(); ++i) {
    IntVar* const next_var = model_->NextVar(i);
    IntVar* const fixed_transit = fixed_transits_[i];
    const auto transit_vehicle_evaluator = [this, i](int64 to,
                                                     int64 eval_index) {
      return eval_index >= 0 ? transit_evaluator(eval_index)(i, to) : 0;
    };
    if (use_light_propagation) {
      if (class_evaluators_.size() == 1) {
        const int class_evaluator_index = class_evaluators_[0];
        const auto& unary_callback =
            model_->UnaryTransitCallbackOrNull(class_evaluator_index);
        if (unary_callback == nullptr) {
          solver->AddConstraint(MakeLightElement(
              solver, fixed_transit, next_var,
              [this, i](int64 to) {
                return model_->TransitCallback(class_evaluators_[0])(i, to);
              },
              [this]() { return model_->enable_deep_serialization_; }));
        } else {
          fixed_transit->SetValue(unary_callback(i));
        }
      } else {
        solver->AddConstraint(MakeLightElement2(
            solver, fixed_transit, next_var, model_->VehicleVar(i),
            transit_vehicle_evaluator,
            [this]() { return model_->enable_deep_serialization_; }));
      }
    } else {
      if (class_evaluators_.size() == 1) {
        const int class_evaluator_index = class_evaluators_[0];
        const auto& unary_callback =
            model_->UnaryTransitCallbackOrNull(class_evaluator_index);
        if (unary_callback == nullptr) {
          solver->AddConstraint(solver->MakeEquality(
              fixed_transit, solver
                                 ->MakeElement(
                                     [this, i](int64 to) {
                                       return model_->TransitCallback(
                                           class_evaluators_[0])(i, to);
                                     },
                                     model_->NextVar(i))
                                 ->Var()));
        } else {
          fixed_transit->SetValue(unary_callback(i));
        }
      } else {
        IntVar* const vehicle_class_var =
            solver->MakeElement(vehicle_class_function, model_->VehicleVar(i))
                ->Var();
        solver->AddConstraint(solver->MakeEquality(
            fixed_transit, solver
                               ->MakeElement(transit_vehicle_evaluator,
                                             next_var, vehicle_class_var)
                               ->Var()));
      }
    }
  }
  if (HasBreakConstraints()) {
    GlobalVehicleBreaksConstraint* constraint =
        model()->solver()->RevAlloc(new GlobalVehicleBreaksConstraint(this));
    solver->AddConstraint(constraint);
  }
}

int64 RoutingDimension::GetTransitValue(int64 from_index, int64 to_index,
                                        int64 vehicle) const {
  DCHECK(transit_evaluator(vehicle) != nullptr);
  return transit_evaluator(vehicle)(from_index, to_index);
}

SortedDisjointIntervalList RoutingDimension::GetAllowedIntervalsInRange(
    int64 index, int64 min_value, int64 max_value) const {
  SortedDisjointIntervalList allowed;
  const SortedDisjointIntervalList& forbidden = forbidden_intervals_[index];
  IntVar* const cumul_var = cumuls_[index];
  const int64 min = std::max(min_value, cumul_var->Min());
  const int64 max = std::min(max_value, cumul_var->Max());
  int64 next_start = min;
  for (SortedDisjointIntervalList::Iterator interval =
           forbidden.FirstIntervalGreaterOrEqual(min);
       interval != forbidden.end(); ++interval) {
    if (next_start > max) break;
    if (next_start < interval->start) {
      allowed.InsertInterval(next_start, CapSub(interval->start, 1));
    }
    next_start = CapAdd(interval->end, 1);
  }
  if (next_start <= max) {
    allowed.InsertInterval(next_start, max);
  }
  return allowed;
}

void RoutingDimension::SetSpanUpperBoundForVehicle(int64 upper_bound,
                                                   int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_span_upper_bounds_.size());
  CHECK_GE(upper_bound, 0);
  vehicle_span_upper_bounds_[vehicle] = upper_bound;
}

void RoutingDimension::SetSpanCostCoefficientForVehicle(int64 coefficient,
                                                        int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_span_cost_coefficients_.size());
  CHECK_GE(coefficient, 0);
  vehicle_span_cost_coefficients_[vehicle] = coefficient;
}

void RoutingDimension::SetSpanCostCoefficientForAllVehicles(int64 coefficient) {
  CHECK_GE(coefficient, 0);
  vehicle_span_cost_coefficients_.assign(model_->vehicles(), coefficient);
}

void RoutingDimension::SetGlobalSpanCostCoefficient(int64 coefficient) {
  CHECK_GE(coefficient, 0);
  global_span_cost_coefficient_ = coefficient;
}

void RoutingDimension::SetCumulVarPiecewiseLinearCost(
    int64 index, const PiecewiseLinearFunction& cost) {
  if (!cost.IsNonDecreasing()) {
    LOG(WARNING) << "Only non-decreasing cost functions are supported.";
    return;
  }
  if (cost.Value(0) < 0) {
    LOG(WARNING) << "Only positive cost functions are supported.";
    return;
  }
  if (index >= cumul_var_piecewise_linear_cost_.size()) {
    cumul_var_piecewise_linear_cost_.resize(index + 1);
  }
  PiecewiseLinearCost& piecewise_linear_cost =
      cumul_var_piecewise_linear_cost_[index];
  piecewise_linear_cost.var = cumuls_[index];
  piecewise_linear_cost.cost = absl::make_unique<PiecewiseLinearFunction>(cost);
}

bool RoutingDimension::HasCumulVarPiecewiseLinearCost(int64 index) const {
  return (index < cumul_var_piecewise_linear_cost_.size() &&
          cumul_var_piecewise_linear_cost_[index].var != nullptr);
}

const PiecewiseLinearFunction* RoutingDimension::GetCumulVarPiecewiseLinearCost(
    int64 index) const {
  if (index < cumul_var_piecewise_linear_cost_.size() &&
      cumul_var_piecewise_linear_cost_[index].var != nullptr) {
    return cumul_var_piecewise_linear_cost_[index].cost.get();
  }
  return nullptr;
}

namespace {
IntVar* BuildVarFromExprAndIndexActiveState(const RoutingModel* model,
                                            IntExpr* expr, int index) {
  Solver* const solver = model->solver();
  if (model->IsStart(index) || model->IsEnd(index)) {
    const int vehicle = model->VehicleIndex(index);
    DCHECK_GE(vehicle, 0);
    return solver->MakeProd(expr, model->VehicleCostsConsideredVar(vehicle))
        ->Var();
  }
  return solver->MakeProd(expr, model->ActiveVar(index))->Var();
}
}  // namespace

void RoutingDimension::SetupCumulVarPiecewiseLinearCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_piecewise_linear_cost_.size(); ++i) {
    const PiecewiseLinearCost& piecewise_linear_cost =
        cumul_var_piecewise_linear_cost_[i];
    if (piecewise_linear_cost.var != nullptr) {
      IntExpr* const expr = solver->MakePiecewiseLinearExpr(
          piecewise_linear_cost.var, *piecewise_linear_cost.cost);
      IntVar* cost_var = BuildVarFromExprAndIndexActiveState(model_, expr, i);
      cost_elements->push_back(cost_var);
      // TODO(user): Check if it wouldn't be better to minimize
      // piecewise_linear_cost.var here.
      model_->AddWeightedVariableMinimizedByFinalizer(cost_var, 0);
    }
  }
}

void RoutingDimension::SetCumulVarSoftUpperBound(int64 index, int64 upper_bound,
                                                 int64 coefficient) {
  if (index >= cumul_var_soft_upper_bound_.size()) {
    cumul_var_soft_upper_bound_.resize(index + 1, {nullptr, 0, 0});
  }
  cumul_var_soft_upper_bound_[index] = {cumuls_[index], upper_bound,
                                        coefficient};
}

bool RoutingDimension::HasCumulVarSoftUpperBound(int64 index) const {
  return (index < cumul_var_soft_upper_bound_.size() &&
          cumul_var_soft_upper_bound_[index].var != nullptr);
}

int64 RoutingDimension::GetCumulVarSoftUpperBound(int64 index) const {
  if (index < cumul_var_soft_upper_bound_.size() &&
      cumul_var_soft_upper_bound_[index].var != nullptr) {
    return cumul_var_soft_upper_bound_[index].bound;
  }
  return cumuls_[index]->Max();
}

int64 RoutingDimension::GetCumulVarSoftUpperBoundCoefficient(
    int64 index) const {
  if (index < cumul_var_soft_upper_bound_.size() &&
      cumul_var_soft_upper_bound_[index].var != nullptr) {
    return cumul_var_soft_upper_bound_[index].coefficient;
  }
  return 0;
}

void RoutingDimension::SetupCumulVarSoftUpperBoundCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_soft_upper_bound_.size(); ++i) {
    const SoftBound& soft_bound = cumul_var_soft_upper_bound_[i];
    if (soft_bound.var != nullptr) {
      IntExpr* const expr = solver->MakeSemiContinuousExpr(
          solver->MakeSum(soft_bound.var, -soft_bound.bound), 0,
          soft_bound.coefficient);
      IntVar* cost_var = BuildVarFromExprAndIndexActiveState(model_, expr, i);
      cost_elements->push_back(cost_var);
      // NOTE: We minimize the cost here instead of minimizing the cumul
      // variable, to avoid setting the cumul to earlier than necessary.
      model_->AddWeightedVariableMinimizedByFinalizer(cost_var,
                                                      soft_bound.coefficient);
    }
  }
}

void RoutingDimension::SetCumulVarSoftLowerBound(int64 index, int64 lower_bound,
                                                 int64 coefficient) {
  if (index >= cumul_var_soft_lower_bound_.size()) {
    cumul_var_soft_lower_bound_.resize(index + 1, {nullptr, 0, 0});
  }
  cumul_var_soft_lower_bound_[index] = {cumuls_[index], lower_bound,
                                        coefficient};
}

bool RoutingDimension::HasCumulVarSoftLowerBound(int64 index) const {
  return (index < cumul_var_soft_lower_bound_.size() &&
          cumul_var_soft_lower_bound_[index].var != nullptr);
}

int64 RoutingDimension::GetCumulVarSoftLowerBound(int64 index) const {
  if (index < cumul_var_soft_lower_bound_.size() &&
      cumul_var_soft_lower_bound_[index].var != nullptr) {
    return cumul_var_soft_lower_bound_[index].bound;
  }
  return cumuls_[index]->Min();
}

int64 RoutingDimension::GetCumulVarSoftLowerBoundCoefficient(
    int64 index) const {
  if (index < cumul_var_soft_lower_bound_.size() &&
      cumul_var_soft_lower_bound_[index].var != nullptr) {
    return cumul_var_soft_lower_bound_[index].coefficient;
  }
  return 0;
}

void RoutingDimension::SetupCumulVarSoftLowerBoundCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_soft_lower_bound_.size(); ++i) {
    const SoftBound& soft_bound = cumul_var_soft_lower_bound_[i];
    if (soft_bound.var != nullptr) {
      IntExpr* const expr = solver->MakeSemiContinuousExpr(
          solver->MakeDifference(soft_bound.bound, soft_bound.var), 0,
          soft_bound.coefficient);
      IntVar* cost_var = BuildVarFromExprAndIndexActiveState(model_, expr, i);
      cost_elements->push_back(cost_var);
      // NOTE: We minimize the cost here instead of maximizing the cumul
      // variable, to avoid setting the cumul to later than necessary.
      model_->AddWeightedVariableMinimizedByFinalizer(cost_var,
                                                      soft_bound.coefficient);
    }
  }
}

void RoutingDimension::SetupGlobalSpanCost(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  if (global_span_cost_coefficient_ != 0) {
    std::vector<IntVar*> end_cumuls;
    for (int i = 0; i < model_->vehicles(); ++i) {
      end_cumuls.push_back(solver
                               ->MakeProd(model_->vehicle_costs_considered_[i],
                                          cumuls_[model_->End(i)])
                               ->Var());
    }
    IntVar* const max_end_cumul = solver->MakeMax(end_cumuls)->Var();
    model_->AddWeightedVariableMinimizedByFinalizer(
        max_end_cumul, global_span_cost_coefficient_);
    std::vector<IntVar*> start_cumuls;
    for (int i = 0; i < model_->vehicles(); ++i) {
      IntVar* global_span_cost_start_cumul = solver->MakeIntVar(0, kint64max);
      solver->AddConstraint(solver->MakeIfThenElseCt(
          model_->vehicle_costs_considered_[i], cumuls_[model_->Start(i)],
          max_end_cumul, global_span_cost_start_cumul));
      start_cumuls.push_back(global_span_cost_start_cumul);
    }
    IntVar* const min_start_cumul = solver->MakeMin(start_cumuls)->Var();
    model_->AddWeightedVariableMinimizedByFinalizer(
        min_start_cumul, global_span_cost_coefficient_);
    // If there is a single vehicle, model the cost as the sum of its transits
    // to avoid slow (infinite) propagation loops.
    // TODO(user): Avoid slow propagation in the path constraints.
    if (model_->vehicles() == 1) {
      for (int var_index = 0; var_index < model_->Size(); ++var_index) {
        model_->AddWeightedVariableMinimizedByFinalizer(
            slacks_[var_index], global_span_cost_coefficient_);
        cost_elements->push_back(
            solver
                ->MakeProd(
                    model_->vehicle_costs_considered_[0],
                    solver->MakeProd(
                        solver->MakeProd(
                            solver->MakeSum(transits_[var_index],
                                            dependent_transits_[var_index]),
                            global_span_cost_coefficient_),
                        model_->ActiveVar(var_index)))
                ->Var());
      }
    } else {
      IntVar* const end_range =
          solver->MakeDifference(max_end_cumul, min_start_cumul)->Var();
      end_range->SetMin(0);
      cost_elements->push_back(
          solver->MakeProd(end_range, global_span_cost_coefficient_)->Var());
    }
  }
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle,
    std::vector<int64> node_visit_transits) {
  if (breaks.empty()) return;
  const int visit_evaluator = model()->RegisterTransitCallback(
      [node_visit_transits](int64 from, int64 to) {
        return node_visit_transits[from];
      });
  SetBreakIntervalsOfVehicle(std::move(breaks), vehicle, visit_evaluator, -1);
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle,
    std::vector<int64> node_visit_transits,
    std::function<int64(int64, int64)> group_delays) {
  if (breaks.empty()) return;
  const int visit_evaluator = model()->RegisterTransitCallback(
      [node_visit_transits](int64 from, int64 to) {
        return node_visit_transits[from];
      });
  const int delay_evaluator = model()->RegisterTransitCallback(group_delays);
  SetBreakIntervalsOfVehicle(std::move(breaks), vehicle, visit_evaluator,
                             delay_evaluator);
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle, int pre_travel_evaluator,
    int post_travel_evaluator) {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, model_->vehicles());
  if (breaks.empty()) return;
  if (!break_constraints_are_initialized_) InitializeBreaks();
  vehicle_break_intervals_[vehicle] = std::move(breaks);
  vehicle_pre_travel_evaluators_[vehicle] = pre_travel_evaluator;
  vehicle_post_travel_evaluators_[vehicle] = post_travel_evaluator;
  // Breaks intervals must be fixed by search.
  for (IntervalVar* const interval : vehicle_break_intervals_[vehicle]) {
    model_->AddIntervalToAssignment(interval);
    if (interval->MayBePerformed() && !interval->MustBePerformed()) {
      model_->AddVariableTargetToFinalizer(interval->PerformedExpr()->Var(), 0);
    }
    model_->AddVariableTargetToFinalizer(interval->SafeStartExpr(0)->Var(),
                                         kint64min);
    model_->AddVariableTargetToFinalizer(interval->SafeDurationExpr(0)->Var(),
                                         kint64min);
  }
  // When a vehicle has breaks, if its start and end are fixed,
  // then propagation keeps the cumuls min and max on its path feasible.
  model_->AddVariableTargetToFinalizer(CumulVar(model_->End(vehicle)),
                                       kint64min);
  model_->AddVariableTargetToFinalizer(CumulVar(model_->Start(vehicle)),
                                       kint64max);
}

void RoutingDimension::InitializeBreaks() {
  DCHECK(!break_constraints_are_initialized_);
  const int num_vehicles = model_->vehicles();
  vehicle_break_intervals_.resize(num_vehicles);
  vehicle_pre_travel_evaluators_.resize(num_vehicles, -1);
  vehicle_post_travel_evaluators_.resize(num_vehicles, -1);
  vehicle_break_distance_duration_.resize(num_vehicles);
  break_constraints_are_initialized_ = true;
}

bool RoutingDimension::HasBreakConstraints() const {
  return break_constraints_are_initialized_;
}

const std::vector<IntervalVar*>& RoutingDimension::GetBreakIntervalsOfVehicle(
    int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_break_intervals_.size());
  return vehicle_break_intervals_[vehicle];
}

int RoutingDimension::GetPreTravelEvaluatorOfVehicle(int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_pre_travel_evaluators_.size());
  return vehicle_pre_travel_evaluators_[vehicle];
}

int RoutingDimension::GetPostTravelEvaluatorOfVehicle(int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_post_travel_evaluators_.size());
  return vehicle_post_travel_evaluators_[vehicle];
}

void RoutingDimension::SetBreakDistanceDurationOfVehicle(int64 distance,
                                                         int64 duration,
                                                         int vehicle) {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, model_->vehicles());
  if (!break_constraints_are_initialized_) InitializeBreaks();
  vehicle_break_distance_duration_[vehicle].emplace_back(distance, duration);
  // When a vehicle has breaks, if its start and end are fixed,
  // then propagation keeps the cumuls min and max on its path feasible.
  model_->AddVariableTargetToFinalizer(CumulVar(model_->End(vehicle)),
                                       kint64min);
  model_->AddVariableTargetToFinalizer(CumulVar(model_->Start(vehicle)),
                                       kint64max);
}

const std::vector<std::pair<int64, int64>>&
RoutingDimension::GetBreakDistanceDurationOfVehicle(int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_break_distance_duration_.size());
  return vehicle_break_distance_duration_[vehicle];
}

void RoutingDimension::SetPickupToDeliveryLimitFunctionForPair(
    PickupToDeliveryLimitFunction limit_function, int pair_index) {
  CHECK_GE(pair_index, 0);
  if (pair_index >= pickup_to_delivery_limits_per_pair_index_.size()) {
    pickup_to_delivery_limits_per_pair_index_.resize(pair_index + 1);
  }
  pickup_to_delivery_limits_per_pair_index_[pair_index] =
      std::move(limit_function);
}

bool RoutingDimension::HasPickupToDeliveryLimits() const {
  return !pickup_to_delivery_limits_per_pair_index_.empty();
}

int64 RoutingDimension::GetPickupToDeliveryLimitForPair(int pair_index,
                                                        int pickup,
                                                        int delivery) const {
  DCHECK_GE(pair_index, 0);

  if (pair_index >= pickup_to_delivery_limits_per_pair_index_.size()) {
    return kint64max;
  }
  const PickupToDeliveryLimitFunction& pickup_to_delivery_limit_function =
      pickup_to_delivery_limits_per_pair_index_[pair_index];
  if (!pickup_to_delivery_limit_function) {
    // No limit function set for this pair.
    return kint64max;
  }
  DCHECK_GE(pickup, 0);
  DCHECK_GE(delivery, 0);
  return pickup_to_delivery_limit_function(pickup, delivery);
}

void RoutingDimension::SetupSlackAndDependentTransitCosts() const {
  if (model_->vehicles() == 0) return;
  // Figure out whether all vehicles have the same span cost coefficient or not.
  bool all_vehicle_span_costs_are_equal = true;
  for (int i = 1; i < model_->vehicles(); ++i) {
    all_vehicle_span_costs_are_equal &= vehicle_span_cost_coefficients_[i] ==
                                        vehicle_span_cost_coefficients_[0];
  }

  if (all_vehicle_span_costs_are_equal &&
      vehicle_span_cost_coefficients_[0] == 0) {
    return;  // No vehicle span cost.
  }

  // Make sure that the vehicle's start cumul will be maximized in the end;
  // and that the vehicle's end cumul and the node's slacks will be minimized.
  // Note that we don't do that if there was no span cost (see the return
  // clause above), because in that case we want the dimension cumul to
  // remain unconstrained. Since transitions depend on base dimensions, we
  // have to make sure the slacks of base dimensions are taken care of.
  // Also, it makes more sense to make decisions from the root of the tree
  // towards to leaves, and hence the slacks are pushed in reverse order.
  std::vector<const RoutingDimension*> dimensions_with_relevant_slacks = {this};
  while (true) {
    const RoutingDimension* next =
        dimensions_with_relevant_slacks.back()->base_dimension_;
    if (next == nullptr || next == dimensions_with_relevant_slacks.back()) {
      break;
    }
    dimensions_with_relevant_slacks.push_back(next);
  }

  for (auto it = dimensions_with_relevant_slacks.rbegin();
       it != dimensions_with_relevant_slacks.rend(); ++it) {
    for (int i = 0; i < model_->vehicles(); ++i) {
      model_->AddVariableTargetToFinalizer((*it)->cumuls_[model_->End(i)],
                                           kint64min);
      model_->AddVariableTargetToFinalizer((*it)->cumuls_[model_->Start(i)],
                                           kint64max);
    }
    for (IntVar* const slack : (*it)->slacks_) {
      model_->AddVariableTargetToFinalizer(slack, kint64min);
    }
  }
}
}  // namespace operations_research
