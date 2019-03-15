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
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_neighborhoods.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/graph/connectivity.h"
#include "ortools/graph/linear_assignment.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
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
        steps_(variables_.size(), 0) {}
  Decision* Next(Solver* const solver) override {
    int index = index_.Value();
    while (index < variables_.size() && variables_[index]->Bound()) {
      ++index;
    }
    index_.SetValue(solver, index);
    if (index >= variables_.size()) return nullptr;
    if (!HasTargetsInBounds(index)) {
      // Both bounds were hit, exiting search.
      solver->Fail();
    }
    const int step = steps_[index];
    steps_.SetValue(solver, index, GetNextStep(step));
    return solver->MakeAssignVariableValue(variables_[index],
                                           CapAdd(targets_[index], step));
  }

 private:
  int GetNextStep(int step) const {
    return (step > 0) ? -step : CapSub(1, step);
  }
  bool HasTargetsInBounds(int index) const {
    IntVar* const variable = variables_[index];
    const int64 target = targets_[index];
    const int64 step = steps_[index];
    const int64 value = CapAdd(target, step);
    const int64 next_value = CapAdd(target, GetNextStep(step));
    return std::max(value, next_value) <= variable->Max() ||
           std::min(value, next_value) >= variable->Min();
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
      const std::vector<RoutingDimension*>& dimensions_for_local_optimizer,
      SearchMonitor* monitor)
      : monitor_(monitor) {
    local_optimizers_.reserve(dimensions_for_local_optimizer.size());
    for (RoutingDimension* const dimension : dimensions_for_local_optimizer) {
      local_optimizers_.emplace_back(dimension);
    }
  }
  Decision* Next(Solver* const solver) override {
    // The following boolean variable indicates if the solver should fail, in
    // order to postpone the Fail() call until after the internal for loop, so
    // there are no memory leaks related to the cumul_values vector.
    bool should_fail = false;
    for (LocalDimensionCumulOptimizer& optimizer : local_optimizers_) {
      const RoutingDimension* const dimension = optimizer.dimension();
      RoutingModel* const model = dimension->model();
      const auto next = [model](int64 i) { return model->NextVar(i)->Value(); };
      for (int vehicle = 0; vehicle < model->vehicles(); ++vehicle) {
        // TODO(user): Investigate if we should skip unused vehicles.
        if (dimension->VehicleHasBreakIntervals(vehicle)) continue;
        if (dimension->GetSpanCostCoefficientForVehicle(vehicle) <= 0) continue;

        DCHECK(DimensionFixedTransitsEqualTransitEvaluatorForVehicle(*dimension,
                                                                     vehicle));
        std::vector<int64> cumul_values;
        if (!optimizer.ComputeRouteCumuls(vehicle, next, &cumul_values)) {
          should_fail = true;
          break;
        }
        std::vector<IntVar*> cumuls;
        int current = model->Start(vehicle);
        while (true) {
          cumuls.push_back(dimension->CumulVar(current));
          if (!model->IsEnd(current)) {
            current = model->NextVar(current)->Value();
          } else {
            break;
          }
        }
        // TODO(user): Use SetValuesFromTargets to return a Decision instead
        // of the nested Solve.
        if (!solver->SolveAndCommit(
                MakeSetValuesFromTargets(solver, std::move(cumuls),
                                         std::move(cumul_values)),
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
  std::vector<LocalDimensionCumulOptimizer> local_optimizers_;
  SearchMonitor* const monitor_;
};

class SetCumulsFromGlobalDimensionCosts : public DecisionBuilder {
 public:
  SetCumulsFromGlobalDimensionCosts(
      const std::vector<RoutingDimension*>& dimensions_for_global_optimizer,
      SearchMonitor* monitor, bool optimize_and_pack = false)
      : monitor_(monitor), optimize_and_pack_(optimize_and_pack) {
    global_optimizers_.reserve(dimensions_for_global_optimizer.size());
    for (const RoutingDimension* dimension : dimensions_for_global_optimizer) {
      global_optimizers_.emplace_back(
          absl::make_unique<GlobalDimensionCumulOptimizer>(dimension));
    }
  }
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
      const bool cumuls_optimized =
          optimize_and_pack_
              ? global_optimizer->ComputePackedCumuls(next, &cumul_values)
              : global_optimizer->ComputeCumuls(next, &cumul_values);
      // TODO(user): Use SetValuesFromTargets to return a Decision instead
      // of the nested Solve.
      if (!cumuls_optimized ||
          !solver->SolveAndCommit(
              MakeSetValuesFromTargets(solver, dimension->cumuls(),
                                       std::move(cumul_values)),
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
  std::vector<std::unique_ptr<GlobalDimensionCumulOptimizer>>
      global_optimizers_;
  SearchMonitor* const monitor_;
  const bool optimize_and_pack_;
};

}  // namespace

const Assignment*
RoutingModel::PackCumulsOfGlobalOptimizerDimensionsFromAssignment(
    const Assignment* original_assignment) {
  CHECK(closed_);
  if (original_assignment == nullptr ||
      dimensions_for_global_optimizer_.empty()) {
    return original_assignment;
  }

  // Initialize the packed_assignment with the Next values in the
  // original_assignment.
  Assignment* packed_assignment = solver_->MakeAssignment();
  packed_assignment->Add(Nexts());
  packed_assignment->CopyIntersection(original_assignment);

  DecisionBuilder* restore_and_pack =
      solver_->Compose(solver_->MakeRestoreAssignment(packed_assignment),
                       solver_->RevAlloc(new SetCumulsFromGlobalDimensionCosts(
                           dimensions_for_global_optimizer_, nullptr,
                           /*optimize_and_pack=*/true)));
  solver_->Solve(restore_and_pack, packed_dimensions_assignment_collector_);

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

// Evaluators
template <class A, class B>
static int64 ReturnZero(A a, B b) {
  return 0;
}

bool TransitCallbackPositive(const RoutingTransitCallback2& callback,
                             int size) {
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
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
      fixed_cost_of_vehicle_(vehicles_, 0),
      cost_class_index_of_vehicle_(vehicles_, CostClassIndex(-1)),
      linear_cost_factor_of_vehicle_(vehicles_, 0),
      quadratic_cost_factor_of_vehicle_(vehicles_, 0),
      vehicle_amortized_cost_factors_set_(false),
      cost_classes_(),
      costs_are_homogeneous_across_vehicles_(
          parameters.reduce_vehicle_cost_model()),
      cache_callbacks_(false),
      vehicle_class_index_of_vehicle_(vehicles_, VehicleClassIndex(-1)),
      vehicle_pickup_delivery_policy_(vehicles_, PICKUP_AND_DELIVERY_NO_ORDER),
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
  DCHECK(TransitCallbackPositive(callback, Size() + vehicles()));
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
}  // namespace

bool RoutingModel::AddConstantDimensionWithSlack(
    int64 value, int64 capacity, int64 slack_max, bool fix_start_cumul_to_zero,
    const std::string& dimension_name) {
  return AddDimension(RegisterCallback([value](int64, int64) { return value; },
                                       /*is_positive=*/value >= 0, this),
                      slack_max, capacity, fix_start_cumul_to_zero,
                      dimension_name);
}

bool RoutingModel::AddVectorDimension(std::vector<int64> values, int64 capacity,
                                      bool fix_start_cumul_to_zero,
                                      const std::string& dimension_name) {
  return AddDimension(
      RegisterCallback(
          [this, values](int64 i, int64 j) {
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
  return dimension_names;
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
  if (a.cost_class_index != b.cost_class_index) {
    return a.cost_class_index < b.cost_class_index;
  }
  if (a.fixed_cost != b.fixed_cost) {
    return a.fixed_cost < b.fixed_cost;
  }
  if (a.start_equivalence_class != b.start_equivalence_class) {
    return a.start_equivalence_class < b.start_equivalence_class;
  }
  if (a.end_equivalence_class != b.end_equivalence_class) {
    return a.end_equivalence_class < b.end_equivalence_class;
  }
  if (a.unvisitable_nodes_fprint != b.unvisitable_nodes_fprint) {
    return a.unvisitable_nodes_fprint < b.unvisitable_nodes_fprint;
  }
  if (a.dimension_start_cumuls_min != b.dimension_start_cumuls_min) {
    return a.dimension_start_cumuls_min < b.dimension_start_cumuls_min;
  }
  if (a.dimension_start_cumuls_max != b.dimension_start_cumuls_max) {
    return a.dimension_start_cumuls_max < b.dimension_start_cumuls_max;
  }
  if (a.dimension_end_cumuls_min != b.dimension_end_cumuls_min) {
    return a.dimension_end_cumuls_min < b.dimension_end_cumuls_min;
  }
  if (a.dimension_end_cumuls_max != b.dimension_end_cumuls_max) {
    return a.dimension_end_cumuls_max < b.dimension_end_cumuls_max;
  }
  if (a.dimension_capacities != b.dimension_capacities) {
    return a.dimension_capacities < b.dimension_capacities;
  }
  return a.dimension_evaluator_classes < b.dimension_evaluator_classes;
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
  vehicle_start_class_callback_ = [this](int64 start) {
    return GetVehicleStartClass(start);
  };

  AddNoCycleConstraintInternal();

  const int size = Size();

  // Vehicle variable constraints
  for (int i = 0; i < vehicles_; ++i) {
    solver_->AddConstraint(solver_->MakeEquality(vehicle_vars_[starts_[i]],
                                                 solver_->MakeIntConst(i)));
    solver_->AddConstraint(solver_->MakeEquality(vehicle_vars_[ends_[i]],
                                                 solver_->MakeIntConst(i)));
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

  // Nodes which are not in a disjunction are mandatory.
  for (int i = 0; i < size; ++i) {
    if (GetDisjunctionIndices(i).empty() && active_[i]->Max() != 0) {
      active_[i]->SetValue(1);
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

  if (HasTemporalTypeIncompatibilities() || HasHardTypeIncompatibilities()) {
    solver_->AddConstraint(
        solver_->RevAlloc(new TypeIncompatibilityConstraint(*this)));
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
    dimension->SetupSlackAndDependentTransitCosts(&cost_elements);
    bool has_span_constraint = false;
    for (const int64 coeff : dimension->vehicle_span_cost_coefficients()) {
      if (coeff != 0) {
        has_span_constraint = true;
        break;
      }
    }
    if (!has_span_constraint) {
      for (const int64 value : dimension->vehicle_span_upper_bounds()) {
        if (value != 0) {
          has_span_constraint = true;
          break;
        }
      }
    }
    if (has_span_constraint) {
      std::vector<IntVar*> total_slack(vehicles(), nullptr);
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        const int64 cost = dimension->vehicle_span_cost_coefficients()[vehicle];
        const int64 limit = dimension->vehicle_span_upper_bounds()[vehicle];
        if (cost != 0 || limit < kint64max) {
          total_slack[vehicle] = solver_->MakeIntVar(0, limit, "");
          if (cost != 0) {
            cost_elements.push_back(
                solver_->MakeProd(total_slack[vehicle], cost)->Var());
          }
        }
      }
      solver_->AddConstraint(MakePathSlacks(dimension, total_slack));
    }
  }
  // Penalty costs
  for (DisjunctionIndex i(0); i < disjunctions_.size(); ++i) {
    IntVar* penalty_var = CreateDisjunction(i);
    if (penalty_var != nullptr) {
      cost_elements.push_back(penalty_var);
    }
  }
  // Soft cumul upper bound costs
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

  // Store the dimensions for local/global cumul optimizers, along with their
  // offsets.
  StoreDimensionsForDimensionCumulOptimizers(parameters);

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
// The CP solver engine uses int64 time limits in milliseconds, with kint64max
// for "no limit". This converter helps several times.
int64 GetTimeLimitMs(const RoutingSearchParameters& parameters) {
  if (!parameters.has_time_limit()) return kint64max;
  return absl::ToInt64Milliseconds(
      util_time::DecodeGoogleApiProto(parameters.time_limit()).ValueOrDie());
}

// Ditto, for the LNS time limit.
int64 GetLnsTimeLimitMs(const RoutingSearchParameters& parameters) {
  if (!parameters.has_lns_time_limit()) return kint64max;
  return absl::ToInt64Milliseconds(
      util_time::DecodeGoogleApiProto(parameters.lns_time_limit())
          .ValueOrDie());
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
  const std::string cost_string =
      cost_scaling_factor == 1.0
          ? absl::StrCat(solution_cost)
          : absl::StrFormat("%d (%.8lf)", solution_cost,
                            solution_cost / cost_scaling_factor);
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
  solver_->UpdateLimits(GetTimeLimitMs(parameters), kint64max, kint64max,
                        parameters.solution_limit(), limit_);
  solver_->UpdateLimits(GetTimeLimitMs(parameters), kint64max, kint64max, 1,
                        ls_limit_);
  solver_->UpdateLimits(GetLnsTimeLimitMs(parameters), kint64max, kint64max,
                        kint64max, lns_limit_);
  // NOTE: Allow more time for the first solution's scheduling, since if it
  // fails, we won't have anything to build upon.
  // We set this time limit based on whether local/global dimension optimizers
  // are used in the finalizer to avoid going over the general time limit.
  // TODO(user): Adapt this when absolute timeouts are given to the model.
  const int time_limit_shares = 1 + !dimensions_for_global_optimizer_.empty() +
                                !dimensions_for_local_optimizer_.empty();
  const int64 first_solution_lns_time_limit =
      std::max(GetTimeLimitMs(parameters) / time_limit_shares,
               GetLnsTimeLimitMs(parameters));
  solver_->UpdateLimits(first_solution_lns_time_limit, kint64max, kint64max,
                        kint64max, first_solution_lns_limit_);

  std::vector<std::unique_ptr<Assignment>> solution_pool;
  if (nullptr == assignment) {
    bool solution_found = false;
    Assignment matching(solver_.get());
    if (IsMatchingModel() && SolveMatchingModel(&matching) &&
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
      const int64 elapsed_time_ms = solver_->wall_time() - start_time_ms;
      const int64 time_left_ms =
          GetTimeLimitMs(parameters) != kint64max
              ? GetTimeLimitMs(parameters) - elapsed_time_ms
              : kint64max;
      if (time_left_ms >= 0) {
        solver_->UpdateLimits(time_left_ms, kint64max, kint64max,
                              parameters.solution_limit(), limit_);
        solver_->UpdateLimits(time_left_ms, kint64max, kint64max, 1, ls_limit_);
        solver_->Solve(solve_db_, monitors_);
      }
    }
  } else {
    assignment_->CopyIntersection(assignment);
    solver_->Solve(improve_db_, monitors_);
  }
  const int64 elapsed_time_ms = solver_->wall_time() - start_time_ms;
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
    if (elapsed_time_ms >= GetTimeLimitMs(parameters)) {
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

int64 RoutingModel::GetArcCostForClassInternal(
    int64 from_index, int64 to_index, CostClassIndex cost_class_index) {
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
    // as an empty route thus the cost of 0.
    cost = 0;
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
                                         int64 vehicle) {
  if (from_index != to_index && vehicle >= 0) {
    return GetArcCostForClassInternal(from_index, to_index,
                                      GetCostClassIndexOfVehicle(vehicle));
  } else {
    return 0;
  }
}

int64 RoutingModel::GetArcCostForClass(
    int64 from_index, int64 to_index,
    int64 /*CostClassIndex*/ cost_class_index) {
  if (from_index != to_index) {
    return GetArcCostForClassInternal(from_index, to_index,
                                      CostClassIndex(cost_class_index));
  } else {
    return 0;
  }
}

int64 RoutingModel::GetArcCostForFirstSolution(int64 from_index,
                                               int64 to_index) {
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

void RoutingModel::SetVisitType(int64 index, int type) {
  CHECK_LT(index, index_to_visit_type_.size());
  index_to_visit_type_[index] = type;
  num_visit_types_ = std::max(num_visit_types_, type + 1);
}

int RoutingModel::GetVisitType(int64 index) const {
  CHECK_LT(index, index_to_visit_type_.size());
  return index_to_visit_type_[index];
}

void RoutingModel::AddTypeIncompatibilityInternal(
    int type1, int type2,
    std::vector<absl::flat_hash_set<int>>* incompatible_types_per_type_index) {
  // Resizing incompatible_types_per_type_index if necessary (first
  // incompatibility or new type).
  num_visit_types_ = std::max(num_visit_types_, std::max(type1, type2) + 1);
  if (incompatible_types_per_type_index->size() < num_visit_types_) {
    incompatible_types_per_type_index->resize(num_visit_types_);
  }
  (*incompatible_types_per_type_index)[type1].insert(type2);
  (*incompatible_types_per_type_index)[type2].insert(type1);
}

void RoutingModel::AddHardTypeIncompatibility(int type1, int type2) {
  AddTypeIncompatibilityInternal(type1, type2,
                                 &hard_incompatible_types_per_type_index_);
}

void RoutingModel::AddTemporalTypeIncompatibility(int type1, int type2) {
  AddTypeIncompatibilityInternal(type1, type2,
                                 &temporal_incompatible_types_per_type_index_);
}

const absl::flat_hash_set<int>&
RoutingModel::GetTypeIncompatibilitiesOfTypeInternal(
    int type, const std::vector<absl::flat_hash_set<int>>&
                  incompatible_types_per_type_index) const {
  CHECK_GE(type, 0);
  if (type >= incompatible_types_per_type_index.size()) {
    // No incompatibilities added for this type.
    return empty_incompatibilities_;
  }
  return incompatible_types_per_type_index[type];
}

const absl::flat_hash_set<int>&
RoutingModel::GetHardTypeIncompatibilitiesOfType(int type) const {
  return GetTypeIncompatibilitiesOfTypeInternal(
      type, hard_incompatible_types_per_type_index_);
}

const absl::flat_hash_set<int>&
RoutingModel::GetTemporalTypeIncompatibilitiesOfType(int type) const {
  return GetTypeIncompatibilitiesOfTypeInternal(
      type, temporal_incompatible_types_per_type_index_);
}

const absl::flat_hash_set<int>&
RoutingModel::GetHardTypeIncompatibilitiesOfNode(int64 index) const {
  const int type = GetVisitType(index);
  return type == kUnassigned
             ? empty_incompatibilities_
             : GetTypeIncompatibilitiesOfTypeInternal(
                   type, hard_incompatible_types_per_type_index_);
}

const absl::flat_hash_set<int>&
RoutingModel::GetTemporalTypeIncompatibilitiesOfNode(int64 index) const {
  const int type = GetVisitType(index);
  return type == kUnassigned
             ? empty_incompatibilities_
             : GetTypeIncompatibilitiesOfTypeInternal(
                   type, temporal_incompatible_types_per_type_index_);
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
  for (int i = 0; i < Size(); ++i) {
    if (!IsEnd(i) && !IsStart(i) &&
        solution_assignment.Value(NextVar(i)) == i) {
      absl::StrAppendFormat(&output, "%d ", i);
    }
  }
  output.append("\n");
  return output;
}

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

SearchLimit* RoutingModel::GetOrCreateLimit() {
  if (limit_ == nullptr) {
    limit_ =
        solver_->MakeLimit(kint64max, kint64max, kint64max, kint64max, true);
  }
  return limit_;
}

SearchLimit* RoutingModel::GetOrCreateLocalSearchLimit() {
  if (ls_limit_ == nullptr) {
    ls_limit_ = solver_->MakeLimit(kint64max, kint64max, kint64max, 1, true);
  }
  return ls_limit_;
}

SearchLimit* RoutingModel::GetOrCreateLargeNeighborhoodSearchLimit() {
  if (lns_limit_ == nullptr) {
    lns_limit_ = solver_->MakeLimit(kint64max, kint64max, kint64max, kint64max);
  }
  return lns_limit_;
}

SearchLimit*
RoutingModel::GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit() {
  if (first_solution_lns_limit_ == nullptr) {
    first_solution_lns_limit_ =
        solver_->MakeLimit(kint64max, kint64max, kint64max, kint64max);
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
        return GetArcCostForVehicle(before_node, after_node,
                                    index_to_vehicle_[start_index]);
      };
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
  // TODO(user): move the following operators to a second local search
  // loop.
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
  return solver_->ConcatenateOperators(operators);
}

#undef CP_ROUTING_PUSH_OPERATOR

const std::vector<LocalSearchFilter*>&
RoutingModel::GetOrCreateLocalSearchFilters() {
  // Note on objective injection from one filter to another.
  // As of 2013/01, three filters evaluate sub-parts of the objective
  // function:
  // - NodeDisjunctionFilter: takes disjunction penalty costs into account,
  // - PathCumulFilter: takes dimension span costs into account,
  // - ObjectiveFilter:
  //     - VehicleAmortizedCostFilter, which considers the part of the cost
  //       related to amortized linear and quadratic vehicle cost factors.
  //     - LocalSearchObjectiveFilter, which takes dimension "arc" costs into
  //       account.
  // To be able to filter cost values properly, a filter needs to be aware of
  // cost bounds computed by other filters before it (for the same delta).
  // Communication of cost between filters is done through callbacks,
  // VehicleAmortizedCostFilter sending the total "vehicle cost" to
  // LocalSearchObjectiveFilter, itself sending this vehicle cost + total arc
  // costs to NodeDisjunctionFilter, in turn sending this cost + total penalty
  // cost to PathCumulFilters (if you have several of these, they send updated
  // costs to each other too).
  // Note that since the VehicleAmortizedCostFilter is the only filter with a
  // possible negative contribution to the objective, it has to be the first
  // filter called so it propagates this value to all other filters.
  // Callbacks are called on OnSynchronize to send the cost of the current
  // solution and on Accept to send the cost of solution deltas.
  if (filters_.empty()) {
    // NOTE: We first sort the dimensions by decreasing complexity of filtering:
    // Dimensions with a global span cost coefficient and/or precedences will
    // have a global LP created to filter feasibility and costs.
    // We want these filters to be called last, hence putting them in the
    // beginning of the vector, since it will be reversed.
    std::vector<RoutingDimension*> sorted_dimensions = dimensions_.get();
    std::sort(sorted_dimensions.begin(), sorted_dimensions.end(),
              [](const RoutingDimension* d1, const RoutingDimension* d2) {
                return (d1->global_span_cost_coefficient() >
                        d2->global_span_cost_coefficient()) ||
                       (d1->GetNodePrecedences().size() >
                        d2->GetNodePrecedences().size());
              });

    std::vector<IntVarLocalSearchFilter*> cumul_filters;
    IntVarLocalSearchFilter* path_cumul_filter = nullptr;
    for (const RoutingDimension* dimension : sorted_dimensions) {
      Solver::ObjectiveWatcher objective_callback = nullptr;
      if (path_cumul_filter != nullptr) {
        objective_callback = [path_cumul_filter](int64 value) {
          return path_cumul_filter->InjectObjectiveValue(value);
        };
      }
      const std::vector<IntVarLocalSearchFilter*> dimension_cumul_filters =
          MakeCumulFilters(*dimension, objective_callback,
                           /*filtering_objective*/ true);
      cumul_filters.insert(cumul_filters.end(), dimension_cumul_filters.begin(),
                           dimension_cumul_filters.end());
      // NOTE: We use the last element of cumul_filters to inject later costs
      // because MakeCumulFilter() sets the internal injections between the
      // LPCumulFilter and PathCumulFilter when necessary.
      path_cumul_filter = cumul_filters.back();
    }
    // Due to the way cost injection is setup, path filters have to be
    // called in reverse order.
    std::reverse(cumul_filters.begin(), cumul_filters.end());
    IntVarLocalSearchFilter* node_disjunction_filter = nullptr;
    if (!disjunctions_.empty()) {
      Solver::ObjectiveWatcher objective_callback = nullptr;
      if (path_cumul_filter != nullptr) {
        objective_callback = [path_cumul_filter](int64 value) {
          return path_cumul_filter->InjectObjectiveValue(value);
        };
      }
      node_disjunction_filter =
          MakeNodeDisjunctionFilter(*this, objective_callback);
    }
    Solver::ObjectiveWatcher objective_callback = nullptr;
    if (node_disjunction_filter != nullptr) {
      objective_callback = [node_disjunction_filter](int64 value) {
        return node_disjunction_filter->InjectObjectiveValue(value);
      };
    } else if (path_cumul_filter != nullptr) {
      objective_callback = [path_cumul_filter](int64 value) {
        return path_cumul_filter->InjectObjectiveValue(value);
      };
    }

    IntVarLocalSearchFilter* local_search_objective_filter = nullptr;
    if (CostsAreHomogeneousAcrossVehicles()) {
      local_search_objective_filter = solver_->MakeSumObjectiveFilter(
          nexts_, [this](int64 i, int64 j) { return GetHomogeneousCost(i, j); },
          objective_callback, cost_, Solver::LE);
    } else {
      local_search_objective_filter = solver_->MakeSumObjectiveFilter(
          nexts_, vehicle_vars_,
          [this](int64 i, int64 j, int64 k) {
            return GetArcCostForVehicle(i, j, k);
          },
          objective_callback, cost_, Solver::LE);
    }

    if (vehicle_amortized_cost_factors_set_) {
      objective_callback = [local_search_objective_filter](int64 value) {
        return local_search_objective_filter->InjectObjectiveValue(value);
      };
      filters_.push_back(
          MakeVehicleAmortizedCostFilter(*this, objective_callback));
    }

    // Must be added after VehicleAmortizedCostFilter.
    filters_.push_back(local_search_objective_filter);

    filters_.push_back(solver_->MakeVariableDomainFilter());
    if (node_disjunction_filter != nullptr) {
      // Must be added after ObjectiveFilter.
      filters_.push_back(node_disjunction_filter);
    }
    if (!pickup_delivery_pairs_.empty()) {
      filters_.push_back(MakePickupDeliveryFilter(
          *this, pickup_delivery_pairs_, vehicle_pickup_delivery_policy_));
    }
    if (HasTemporalTypeIncompatibilities() || HasHardTypeIncompatibilities()) {
      filters_.push_back(MakeTypeIncompatibilityFilter(*this));
    }
    filters_.push_back(MakeVehicleVarFilter(*this));

    // Must be added after NodeDisjunctionFilter and ObjectiveFilter.
    filters_.insert(filters_.end(), cumul_filters.begin(), cumul_filters.end());

    for (const RoutingDimension* dimension : dimensions_) {
      if (!dimension->vehicle_break_intervals_.empty()) {
        IntVarLocalSearchFilter* breaks_filter =
            MakeVehicleBreaksFilter(*this, *dimension);
        filters_.push_back(breaks_filter);
      }
    }
    filters_.insert(filters_.end(), extra_filters_.begin(),
                    extra_filters_.end());
  }
  return filters_;
}

const std::vector<LocalSearchFilter*>&
RoutingModel::GetOrCreateFeasibilityFilters() {
  if (feasibility_filters_.empty()) {
    if (!disjunctions_.empty()) {
      feasibility_filters_.push_back(MakeNodeDisjunctionFilter(*this, nullptr));
    }
    feasibility_filters_.push_back(solver_->MakeVariableDomainFilter());
    if (!pickup_delivery_pairs_.empty()) {
      feasibility_filters_.push_back(MakePickupDeliveryFilter(
          *this, pickup_delivery_pairs_, vehicle_pickup_delivery_policy_));
    }
    if (HasTemporalTypeIncompatibilities() || HasHardTypeIncompatibilities()) {
      feasibility_filters_.push_back(MakeTypeIncompatibilityFilter(*this));
    }
    feasibility_filters_.push_back(MakeVehicleVarFilter(*this));

    // NOTE: We sort the dimensions by decreasing complexity of filtering:
    // Dimensions with precedences will have a global LP created to filter
    // feasibility, so we want these filters to be called last.
    std::vector<RoutingDimension*> sorted_dimensions = dimensions_.get();
    std::sort(sorted_dimensions.begin(), sorted_dimensions.end(),
              [](const RoutingDimension* d1, const RoutingDimension* d2) {
                return (d1->GetNodePrecedences().size() <
                        d2->GetNodePrecedences().size());
              });
    for (const RoutingDimension* const dimension : sorted_dimensions) {
      const std::vector<IntVarLocalSearchFilter*> dimension_cumul_filters =
          MakeCumulFilters(*dimension, nullptr, /*filtering_objective*/ false);
      feasibility_filters_.insert(feasibility_filters_.end(),
                                  dimension_cumul_filters.begin(),
                                  dimension_cumul_filters.end());
    }

    for (const RoutingDimension* dimension : dimensions_) {
      if (!dimension->vehicle_break_intervals_.empty()) {
        IntVarLocalSearchFilter* breaks_filter =
            MakeVehicleBreaksFilter(*this, *dimension);
        feasibility_filters_.push_back(breaks_filter);
      }
    }

    feasibility_filters_.insert(feasibility_filters_.end(),
                                extra_filters_.begin(), extra_filters_.end());
  }
  return feasibility_filters_;
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

void RoutingModel::StoreDimensionsForDimensionCumulOptimizers(
    const RoutingSearchParameters& parameters) {
  if (parameters.has_lns_time_limit()) {
    lp_scheduling_time_limit_seconds_ = absl::ToDoubleSeconds(
        util_time::DecodeGoogleApiProto(parameters.lns_time_limit())
            .ValueOrDie());
  }

  Assignment* packed_dimensions_collector_assignment =
      solver_->MakeAssignment();
  for (RoutingDimension* dimension : dimensions_) {
    if (dimension->global_span_cost_coefficient() > 0 ||
        !dimension->GetNodePrecedences().empty()) {
      // Use global optimizer.
      dimensions_for_global_optimizer_.push_back(dimension);
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
      std::vector<int64> vehicle_offsets(vehicles());
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (dimension->GetSpanCostCoefficientForVehicle(vehicle) > 0) {
          has_span_cost = true;
        }
        DCHECK_GE(dimension->CumulVar(Start(vehicle))->Min(), 0);
        vehicle_offsets[vehicle] =
            dimension->AreVehicleTransitsPositive(vehicle)
                ? std::max(Zero(),
                           dimension->CumulVar(Start(vehicle))->Min() - 1)
                : 0;
      }
      if (has_span_cost) {
        for (int i = 0; i < dimension->cumuls().size(); ++i) {
          if (dimension->HasCumulVarSoftUpperBound(i) ||
              dimension->HasCumulVarSoftLowerBound(i)) {
            dimension->SetVehicleOffsetsForLocalOptimizer(
                std::move(vehicle_offsets));
            dimensions_for_local_optimizer_.push_back(dimension);
            break;
          }
        }
      }
    }
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

DecisionBuilder* RoutingModel::CreateSolutionFinalizer(SearchLimit* lns_limit) {
  std::vector<DecisionBuilder*> decision_builders;
  decision_builders.push_back(solver_->MakePhase(
      nexts_, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE));

  if (!dimensions_for_local_optimizer_.empty()) {
    decision_builders.push_back(
        solver_->RevAlloc(new SetCumulsFromLocalDimensionCosts(
            dimensions_for_local_optimizer_, lns_limit)));
  }
  if (!dimensions_for_global_optimizer_.empty()) {
    decision_builders.push_back(
        solver_->RevAlloc(new SetCumulsFromGlobalDimensionCosts(
            dimensions_for_global_optimizer_, lns_limit)));
  }

  for (IntVar* const variable : variables_minimized_by_finalizer_) {
    decision_builders.push_back(solver_->MakePhase(
        variable, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE));
  }
  for (IntVar* const variable : variables_maximized_by_finalizer_) {
    decision_builders.push_back(solver_->MakePhase(
        variable, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MAX_VALUE));
  }
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
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] = solver_->RevAlloc(
            new EvaluatorCheapestAdditionFilteredDecisionBuilder(
                this,
                [this](int64 i, int64 j) {
                  return GetArcCostForFirstSolution(i, j);
                },
                GetOrCreateFeasibilityFilters()));
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
        [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] = solver_->RevAlloc(
            new ComparatorCheapestAdditionFilteredDecisionBuilder(
                this, comp, GetOrCreateFeasibilityFilters()));
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
  SearchLimit* const ls_limit = solver_->MakeLimit(
      GetTimeLimitMs(search_parameters), kint64max, kint64max, kint64max, true);
  DecisionBuilder* const finalize = solver_->MakeSolveOnce(
      finalize_solution, GetOrCreateLargeNeighborhoodSearchLimit());
  LocalSearchPhaseParameters* const insertion_parameters =
      solver_->MakeLocalSearchPhaseParameters(CreateInsertionOperator(),
                                              finalize, ls_limit,
                                              GetOrCreateLocalSearchFilters());
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
  // Global cheapest insertion
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION] =
          solver_->RevAlloc(new GlobalCheapestInsertionFilteredDecisionBuilder(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              [this](int64 i) { return UnperformedPenaltyOrValue(0, i); },
              GetOrCreateFeasibilityFilters(), /* is_sequential */ false,
              search_parameters.cheapest_insertion_farthest_seeds_ratio(),
              search_parameters.cheapest_insertion_neighbors_ratio()));
  first_solution_decision_builders_
      [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION] =
          solver_->Try(first_solution_filtered_decision_builders_
                           [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION],
                       first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION]);

  // Sequential global cheapest insertion
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION] =
          solver_->RevAlloc(new GlobalCheapestInsertionFilteredDecisionBuilder(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              [this](int64 i) { return UnperformedPenaltyOrValue(0, i); },
              GetOrCreateFeasibilityFilters(), /* is_sequential */ true,
              search_parameters.cheapest_insertion_farthest_seeds_ratio(),
              search_parameters.cheapest_insertion_neighbors_ratio()));
  first_solution_decision_builders_
      [FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION] = solver_->Try(
          first_solution_filtered_decision_builders_
              [FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION],
          first_solution_decision_builders_
              [FirstSolutionStrategy::BEST_INSERTION]);

  // Local cheapest insertion
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] =
          solver_->RevAlloc(new LocalCheapestInsertionFilteredDecisionBuilder(
              this,
              [this](int64 i, int64 j, int64 vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              GetOrCreateFeasibilityFilters()));
  first_solution_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] =
          solver_->Try(first_solution_filtered_decision_builders_
                           [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION],
                       first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION]);
  // Savings
  SavingsFilteredDecisionBuilder::SavingsParameters savings_parameters;
  savings_parameters.neighbors_ratio =
      search_parameters.savings_neighbors_ratio();
  savings_parameters.max_memory_usage_bytes =
      search_parameters.savings_max_memory_usage_bytes();
  savings_parameters.add_reverse_arcs =
      search_parameters.savings_add_reverse_arcs();
  savings_parameters.arc_coefficient =
      search_parameters.savings_arc_coefficient();
  std::vector<LocalSearchFilter*> filters;
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    filters = GetOrCreateFeasibilityFilters();
  }

  if (search_parameters.savings_parallel_routes()) {
    IntVarFilteredDecisionBuilder* savings_db =
        solver_->RevAlloc(new ParallelSavingsFilteredDecisionBuilder(
            this, &manager_, savings_parameters, filters));
    if (!search_parameters.use_unfiltered_first_solution_strategy()) {
      first_solution_filtered_decision_builders_
          [FirstSolutionStrategy::SAVINGS] = savings_db;
    }

    filters.push_back(MakeCPFeasibilityFilter(this));
    first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->Try(
            savings_db,
            solver_->RevAlloc(new ParallelSavingsFilteredDecisionBuilder(
                this, &manager_, savings_parameters, filters)));
  } else {
    IntVarFilteredDecisionBuilder* savings_db =
        solver_->RevAlloc(new SequentialSavingsFilteredDecisionBuilder(
            this, &manager_, savings_parameters, filters));
    if (!search_parameters.use_unfiltered_first_solution_strategy()) {
      first_solution_filtered_decision_builders_
          [FirstSolutionStrategy::SAVINGS] = savings_db;
    }

    filters.push_back(MakeCPFeasibilityFilter(this));
    first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->Try(
            savings_db,
            solver_->RevAlloc(new SequentialSavingsFilteredDecisionBuilder(
                this, &manager_, savings_parameters, filters)));
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
      solver_->RevAlloc(new ChristofidesFilteredDecisionBuilder(
          this, GetOrCreateFeasibilityFilters()));
  // Automatic
  // TODO(user): make this smarter.
  if (pickup_delivery_pairs_.empty()) {
    first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC] =
        first_solution_decision_builders_
            [FirstSolutionStrategy::PATH_CHEAPEST_ARC];
  } else {
    first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC] =
        first_solution_decision_builders_
            [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION];
  }
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
      GetNeighborhoodOperators(search_parameters),
      solver_->MakeSolveOnce(CreateSolutionFinalizer(lns_limit), lns_limit),
      GetOrCreateLocalSearchLimit(),
      {solver_->RevAlloc(new LocalSearchFilterManager(
          solver_.get(), GetOrCreateLocalSearchFilters(), CostVar()))});
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
    search_log_parameters.display_callback = nullptr;
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

void RoutingModel::AddVariableMinimizedByFinalizer(IntVar* var) {
  CHECK(var != nullptr);
  variables_minimized_by_finalizer_.push_back(var);
}

void RoutingModel::AddVariableMaximizedByFinalizer(IntVar* var) {
  CHECK(var != nullptr);
  variables_maximized_by_finalizer_.push_back(var);
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

class PathSlacks : public Constraint {
 public:
  PathSlacks(const RoutingModel* model, const RoutingDimension* dimension,
             std::vector<IntVar*> total_slacks)
      : Constraint(model->solver()),
        model_(model),
        dimension_(dimension),
        total_slacks_(std::move(total_slacks)) {
    DCHECK_LE(total_slacks_.size(), model_->vehicles());
    vehicle_demons_.resize(model_->vehicles());
  }

  void Post() override {
    const int num_nodes = model_->VehicleVars().size();
    const int num_transits = model_->Nexts().size();
    for (int node = 0; node < num_nodes; ++node) {
      auto* demon = MakeConstraintDemon1(model_->solver(), this,
                                         &PathSlacks::PropagateNode,
                                         "PathSlacks::PropagateNode", node);
      dimension_->CumulVar(node)->WhenRange(demon);
      model_->VehicleVar(node)->WhenBound(demon);
      if (node < num_transits) {
        dimension_->TransitVar(node)->WhenRange(demon);
        dimension_->FixedTransitVar(node)->WhenBound(demon);
        model_->NextVar(node)->WhenBound(demon);
      }
    }
    for (int vehicle = 0; vehicle < total_slacks_.size(); ++vehicle) {
      if (total_slacks_[vehicle] == nullptr) continue;
      auto* demon = MakeDelayedConstraintDemon1(
          solver(), this, &PathSlacks::PropagateVehicle,
          "PathSlacks::PropagateVehicle", vehicle);
      vehicle_demons_[vehicle] = demon;
      total_slacks_[vehicle]->WhenRange(demon);
      if (dimension_->VehicleHasBreakIntervals(vehicle)) {
        for (IntervalVar* br :
             dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
          br->WhenAnything(demon);
        }
      }
    }
  }

  // Call propagator on all vehicles.
  void InitialPropagate() override {
    for (int vehicle = 0; vehicle < total_slacks_.size(); ++vehicle) {
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

  void PropagateVehicle(int vehicle) {
    if (total_slacks_[vehicle] == nullptr) return;
    const int start = model_->Start(vehicle);
    const int end = model_->End(vehicle);

    // The amount of break time that must occur during the route must be smaller
    // than total_slack_max. A break must occur on the route if it must be
    // after the route's start and before the route's end.
    // Propagate lower bound on total_slack, then filter out values
    // that would force more breaks in route than possible.
    if (dimension_->VehicleHasBreakIntervals(vehicle)) {
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
      total_slacks_[vehicle]->SetMin(min_break_duration);
      // If a break that is not inside the route may violate total_slack_max,
      // we can propagate in some cases: when the break must be before or
      // must be after the route.
      // In the other cases, we cannot deduce a better bound on a CumulVar or
      // on a break, so we do nothing.
      const int64 total_slack_max = total_slacks_[vehicle]->Max();
      for (IntervalVar* br : dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
        if (!br->MustBePerformed()) continue;
        // Break must be before end, detect whether it must be before start.
        if (vehicle_start_max >= br->EndMin() &&
            br->StartMax() < vehicle_end_min) {
          if (CapAdd(min_break_duration, br->DurationMin()) > total_slack_max) {
            // Having the break inside would violate total_slack_max.
            // Thus, it must be outside the route, in this case, before.
            br->SetEndMax(vehicle_start_max);
            dimension_->CumulVar(start)->SetMin(br->EndMin());
          }
        }
        // Break must be after start, detect whether it must be after end.
        // Same reasoning, in the case where the break is after.
        if (vehicle_start_max < br->EndMin() &&
            br->StartMax() >= vehicle_end_min) {
          if (CapAdd(min_break_duration, br->DurationMin()) > total_slack_max) {
            br->SetStartMin(vehicle_end_min);
            dimension_->CumulVar(end)->SetMax(br->StartMax());
          }
        }
      }
    }

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
    if (dimension_->GetSpanUpperBoundForVehicle(vehicle) < kint64max) {
      const int64 total_slack_ub = CapSub(
          dimension_->GetSpanUpperBoundForVehicle(vehicle), sum_fixed_transits);
      total_slacks_[vehicle]->SetMax(total_slack_ub);
    }

    // Propagate total_slack = cumul(end) - cumul(start) - sum_fixed_transits.
    {
      IntVar* start_cumul_var = dimension_->CumulVar(start);
      IntVar* end_cumul_var = dimension_->CumulVar(end);
      // Propagate from cumuls to total_slack.
      const int64 total_slack_lb =
          CapSub(CapSub(end_cumul_var->Min(), start_cumul_var->Max()),
                 sum_fixed_transits);
      total_slacks_[vehicle]->SetMin(total_slack_lb);
      const int64 total_slack_ub =
          CapSub(CapSub(end_cumul_var->Max(), start_cumul_var->Min()),
                 sum_fixed_transits);
      total_slacks_[vehicle]->SetMax(total_slack_ub);
      // Propagate from total_slack to cumuls.
      const int64 maximum_deviation =
          CapSub(total_slacks_[vehicle]->Max(), total_slack_lb);
      start_cumul_var->SetMin(
          CapSub(start_cumul_var->Max(), maximum_deviation));
      end_cumul_var->SetMax(CapAdd(end_cumul_var->Min(), maximum_deviation));
    }

    // Propagate total_slack == sum transits.
    {
      // Propagate from transits to total_slack.
      int64 total_slack_lb = 0;
      int64 total_slack_ub = 0;
      {
        for (const int curr_node : path_) {
          total_slack_lb =
              CapAdd(total_slack_lb, dimension_->TransitVar(curr_node)->Min());
          total_slack_ub =
              CapAdd(total_slack_ub, dimension_->TransitVar(curr_node)->Max());
        }
        total_slack_lb = CapSub(total_slack_lb, sum_fixed_transits);
        total_slack_ub = CapSub(total_slack_ub, sum_fixed_transits);
      }
      total_slacks_[vehicle]->SetMin(total_slack_lb);
      total_slacks_[vehicle]->SetMax(total_slack_ub);
      // Propagate from total_slack to transits.
      const int64 maximum_deviation =
          CapSub(total_slacks_[vehicle]->Max(), total_slack_lb);
      for (const int curr_node : path_) {
        IntVar* transit_var = dimension_->TransitVar(curr_node);
        transit_var->SetMax(CapAdd(transit_var->Min(), maximum_deviation));
      }
    }

    // TRICKY: add end node now, we will look at cumuls.
    path_.push_back(end);

    // A stronger bound: from start min of the route,
    // go to next node with time max(prev_cumul + fixed_transit,
    // curr_cumul.Min()) Record arrival time (should be the same as end cumul
    // min). Then do the reverse route, going to time min(cumul - fixed_transit,
    // prev_cumul.Max()) Record final time as departure time. arrival time -
    // departure time is a valid lower bound of total_slack.
    {
      // start - end - start
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
        const int64 total_slack_lb =
            CapSub(CapSub(arrival_time, departure_time), sum_fixed_transits);
        total_slacks_[vehicle]->SetMin(total_slack_lb);
        const int64 maximum_deviation =
            CapSub(total_slacks_[vehicle]->Max(), total_slack_lb);
        const int64 start_lb = CapSub(departure_time, maximum_deviation);
        dimension_->CumulVar(start)->SetMin(start_lb);
      }

      // end - start - end
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

        // total_slack_lb will be the same value as in the start - end - start
        // scenario, we don't need to propagate it.
        const int64 total_slack_lb =
            CapSub(CapSub(arrival_time, departure_time), sum_fixed_transits);
        const int64 maximum_deviation =
            CapSub(total_slacks_[vehicle]->Max(), total_slack_lb);
        dimension_->CumulVar(end)->SetMax(
            CapAdd(arrival_time, maximum_deviation));
      }
    }
  }

  const RoutingModel* const model_;
  const RoutingDimension* const dimension_;
  std::vector<IntVar*> total_slacks_;
  std::vector<int> path_;
  std::vector<Demon*> vehicle_demons_;
};

}  // namespace

Constraint* RoutingModel::MakePathSlacks(const RoutingDimension* dimension,
                                         std::vector<IntVar*> total_slacks) {
  CHECK_EQ(vehicles_, total_slacks.size());
  return solver()->RevAlloc(new PathSlacks(this, dimension, total_slacks));
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
    fixed_transits_[i] = solver->MakeIntVar(kint64min, kint64max, transit_name);
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
      slacks_[i] = solver->MakeIntVar(0, slack_max, slack_name);
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

GlobalVehicleBreaksConstraint::GlobalVehicleBreaksConstraint(
    const RoutingDimension* dimension)
    : Constraint(dimension->model()->solver()),
      model_(dimension->model()),
      dimension_(dimension) {
  vehicle_demons_.resize(model_->vehicles());
}

void GlobalVehicleBreaksConstraint::Post() {
  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    if (!dimension_->VehicleHasBreakIntervals(vehicle)) continue;
    vehicle_demons_[vehicle] = MakeDelayedConstraintDemon1(
        solver(), this, &GlobalVehicleBreaksConstraint::PropagateVehicle,
        "PropagateVehicle", vehicle);
    for (IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      interval->WhenAnything(vehicle_demons_[vehicle]);
    }
  }
  const int num_nodes = model_->Nexts().size();
  for (int node = 0; node < num_nodes; node++) {
    Demon* dimension_demon = MakeConstraintDemon1(
        solver(), this, &GlobalVehicleBreaksConstraint::PropagateNode,
        "PropagateNode", node);
    model_->NextVar(node)->WhenBound(dimension_demon);
    model_->VehicleVar(node)->WhenBound(dimension_demon);
    dimension_->CumulVar(node)->WhenDomain(dimension_demon);
    dimension_->SlackVar(node)->WhenDomain(dimension_demon);
  }
}

void GlobalVehicleBreaksConstraint::InitialPropagate() {
  for (int vehicle = 0; vehicle < model_->vehicles(); vehicle++) {
    if (dimension_->VehicleHasBreakIntervals(vehicle)) {
      PropagateVehicle(vehicle);
    }
  }
}

// This dispatches node events to the right vehicle propagator.
// It also filters out a part of uninteresting events, on which the vehicle
// propagator will not find anything new.
void GlobalVehicleBreaksConstraint::PropagateNode(int node) {
  if (!model_->VehicleVar(node)->Bound()) return;
  const int vehicle = model_->VehicleVar(node)->Min();
  if (vehicle < 0 || vehicle_demons_[vehicle] == nullptr) return;
  EnqueueDelayedDemon(vehicle_demons_[vehicle]);
}

// Naive filtering algorithm: for every arc on the path from start of vehicle,
// for every break B, either do pairwise disjunctive reasoning between
// [CumulVar(node), CumulVar(node) + node_transits(node)) and B
// or [CumulVar(node), CumulVar(Next(node))) and B,
// depending on whether B can fit inside the arc's slack or not.
// The pairwise disjunctive reasoning rule for two performed intervals is:
// StartMax(A) < EndMin(B) => End(A) < StartMax(B) and
// StartMax(A) < EndMin(B) => EndMin(A) < Start(B).
void GlobalVehicleBreaksConstraint::PropagateVehicle(int vehicle) {
  const std::vector<IntervalVar*>& break_intervals =
      dimension_->GetBreakIntervalsOfVehicle(vehicle);
  const std::vector<int64>& node_visit_transit =
      dimension_->GetNodeVisitTransitsOfVehicle(vehicle);
  // External loop: browse all arcs from start of vehicle.
  int64 current = model_->Start(vehicle);
  while (!model_->IsEnd(current)) {
    if (!model_->NextVar(current)->Bound()) break;
    const int64 next = model_->NextVar(current)->Min();
    const int64 current_start_max = dimension_->CumulVar(current)->Max();
    const int64 current_end_min =
        dimension_->CumulVar(current)->Min() + node_visit_transit[current];
    const int64 next_start_min = dimension_->CumulVar(next)->Min();
    const int64 current_slack_max = dimension_->SlackVar(current)->Max();

    int64 total_break_inside_arc = 0;
    for (IntervalVar* interval : break_intervals) {
      if (!interval->MayBePerformed()) continue;

      const bool interval_is_performed = interval->MustBePerformed();
      const int64 interval_start_max = interval->StartMax();
      const int64 interval_end_min = interval->EndMin();
      const int64 interval_duration_min = interval->DurationMin();

      // When interval cannot fit inside current arc,
      // do disjunctive reasoning on full arc.
      if (interval_duration_min > current_slack_max) {
        if (current_start_max < interval_end_min) {
          interval->SetStartMin(next_start_min);
          if (interval_is_performed) {
            dimension_->CumulVar(next)->SetMax(interval_start_max);
          }
        }
        if (interval_start_max < next_start_min) {
          interval->SetEndMax(current_start_max);
          if (interval_is_performed) {
            dimension_->CumulVar(current)->SetMin(interval_end_min);
          }
        }
        continue;
      }

      // Interval could fit inside arc: do disjunctive reasoning between
      // interval and service task only.
      if (current_start_max < interval_end_min) {
        interval->SetStartMin(current_end_min);
        if (interval_is_performed) {
          dimension_->CumulVar(current)->SetMax(interval_start_max);
        }
      }
      if (interval_start_max < current_end_min) {
        interval->SetEndMax(current_start_max);
        if (interval_is_performed) {
          dimension_->CumulVar(current)->SetMin(interval_end_min);
        }
      }

      const int64 actual_transit = dimension_->FixedTransitVar(current)->Min();
      if (interval_start_max < next_start_min) {  // interval not after.
        if (interval_is_performed) {
          const int64 bound_interval_before = interval_end_min + actual_transit;
          const int64 bound_interval_inside =
              dimension_->CumulVar(current)->Min() + actual_transit +
              interval_duration_min;
          dimension_->CumulVar(next)->SetMin(
              std::min(bound_interval_before, bound_interval_inside));
        }
      }

      if (interval_end_min > current_start_max) {  // interval not before
        if (interval_is_performed) {
          const int64 bound_interval_after =
              interval_start_max - actual_transit;
          const int64 bound_interval_inside =
              dimension_->CumulVar(next)->Max() - actual_transit -
              interval_duration_min;
          dimension_->CumulVar(current)->SetMax(
              std::max(bound_interval_after, bound_interval_inside));
        }
      }

      // If interval must be inside the arc, the slack must contain it.
      if (current_start_max < interval_end_min &&
          interval_start_max < next_start_min) {
        if (interval_is_performed) {
          total_break_inside_arc += interval_duration_min;
        }
      }
    }
    dimension_->SlackVar(current)->SetMin(total_break_inside_arc);
    current = next;
  }

  // Energy-based reasoning.
  // Fill internal vectors with route and breaks information.
  tasks_.start_min.clear();
  tasks_.duration_min.clear();
  tasks_.end_max.clear();
  tasks_.is_preemptible.clear();
  tasks_.forbidden_intervals.clear();
  task_translators_.clear();
  // Translate route to tasks: visits are nonpreemptible, transits are.
  int64 group_delay = 0;
  current = model_->Start(vehicle);
  while (true) {
    // Tasks from visits.
    const bool node_is_last = current == model_->End(vehicle);
    const int64 visit_duration = node_is_last ? 0 : node_visit_transit[current];
    tasks_.start_min.push_back(
        CapSub(dimension_->CumulVar(current)->Min(), group_delay));
    tasks_.duration_min.push_back(CapAdd(group_delay, visit_duration));
    tasks_.end_max.push_back(
        CapAdd(dimension_->CumulVar(current)->Max(), visit_duration));
    tasks_.is_preemptible.push_back(false);
    task_translators_.emplace_back(dimension_->CumulVar(current),
                                   visit_duration, group_delay);
    if (node_is_last) break;

    // Tasks from transits.
    const bool next_is_bound = model_->NextVar(current)->Bound();
    const int next =
        next_is_bound ? model_->NextVar(current)->Min() : model_->End(vehicle);
    tasks_.start_min.push_back(
        CapAdd(CapAdd(tasks_.start_min.back(), group_delay), visit_duration));
    group_delay =
        next_is_bound ? dimension_->GetGroupDelay(vehicle, current, next) : 0;
    DCHECK_GE(group_delay, 0);
    tasks_.duration_min.push_back(std::max<int64>(
        0, CapSub(CapSub(dimension_->FixedTransitVar(current)->Min(),
                         visit_duration),
                  group_delay)));
    DCHECK_GE(tasks_.duration_min.back(), 0);
    tasks_.end_max.push_back(
        CapSub(dimension_->CumulVar(next)->Max(), group_delay));
    tasks_.is_preemptible.push_back(true);
    task_translators_.emplace_back();  // Dummy translator, prunes nothing.
    current = next;
  }
  tasks_.num_chain_tasks = tasks_.start_min.size();
  // Tasks from breaks.
  for (IntervalVar* interval : break_intervals) {
    if (!interval->MustBePerformed()) continue;
    tasks_.start_min.push_back(interval->StartMin());
    tasks_.duration_min.push_back(interval->DurationMin());
    tasks_.end_max.push_back(interval->EndMax());
    tasks_.is_preemptible.push_back(false);
    task_translators_.emplace_back(interval);
  }
  if (!disjunctive_propagator_.Propagate(&tasks_)) solver()->Fail();

  // Push new bounds to variables.
  const int num_tasks = tasks_.start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    task_translators_[task].SetStartMin(tasks_.start_min[task]);
    task_translators_[task].SetEndMax(tasks_.end_max[task]);
  }
}

bool DisjunctivePropagator::Propagate(Tasks* tasks) {
  DCHECK_LE(tasks->num_chain_tasks, tasks->start_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->duration_min.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->end_max.size());
  DCHECK_EQ(tasks->start_min.size(), tasks->is_preemptible.size());
  // Do forward deductions, then backward deductions.
  // Interleave Precedences() that is O(n).
  return Precedences(tasks) && EdgeFinding(tasks) && Precedences(tasks) &&
         DetectablePrecedencesWithChain(tasks) && ForbiddenIntervals(tasks) &&
         Precedences(tasks) && MirrorTasks(tasks) && EdgeFinding(tasks) &&
         Precedences(tasks) && DetectablePrecedencesWithChain(tasks) &&
         MirrorTasks(tasks);
}

bool DisjunctivePropagator::Precedences(Tasks* tasks) {
  const int num_chain_tasks = tasks->num_chain_tasks;
  if (num_chain_tasks == 0) return true;
  // Propagate forwards.
  int64 time = tasks->start_min[0];
  for (int task = 0; task < num_chain_tasks; ++task) {
    time = std::max(tasks->start_min[task], time);
    tasks->start_min[task] = time;
    time = CapAdd(time, tasks->duration_min[task]);
    if (tasks->end_max[task] < time) return false;
  }
  // Propagate backwards.
  time = tasks->end_max[num_chain_tasks - 1];
  for (int task = num_chain_tasks - 1; task >= 0; --task) {
    time = std::min(tasks->end_max[task], time);
    tasks->end_max[task] = time;
    time = CapSub(time, tasks->duration_min[task]);
    if (time < tasks->start_min[task]) return false;
  }
  return true;
}

bool DisjunctivePropagator::MirrorTasks(Tasks* tasks) {
  // For all tasks, start_min := -end_max and end_max := -start_min.
  const int num_tasks = tasks->start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    const int64 t = -tasks->start_min[task];
    tasks->start_min[task] = -tasks->end_max[task];
    tasks->end_max[task] = t;
  }
  // In the mirror problem, tasks linked by precedences are in reversed order.
  const int num_chain_tasks = tasks->num_chain_tasks;
  if (num_chain_tasks > 0) {
    std::reverse(tasks->start_min.begin(),
                 tasks->start_min.begin() + num_chain_tasks);
    std::reverse(tasks->duration_min.begin(),
                 tasks->duration_min.begin() + num_chain_tasks);
    std::reverse(tasks->end_max.begin(),
                 tasks->end_max.begin() + num_chain_tasks);
    std::reverse(tasks->is_preemptible.begin(),
                 tasks->is_preemptible.begin() + num_chain_tasks);
  }
  return true;
}

bool DisjunctivePropagator::EdgeFinding(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // Prepare start_min events for tree.
  tasks_by_start_min_.resize(num_tasks);
  std::iota(tasks_by_start_min_.begin(), tasks_by_start_min_.end(), 0);
  std::sort(
      tasks_by_start_min_.begin(), tasks_by_start_min_.end(),
      [&](int i, int j) { return tasks->start_min[i] < tasks->start_min[j]; });
  event_of_task_.resize(num_tasks);
  for (int event = 0; event < num_tasks; ++event) {
    event_of_task_[tasks_by_start_min_[event]] = event;
  }
  // Tasks will be browsed according to end_max order.
  tasks_by_end_max_.resize(num_tasks);
  std::iota(tasks_by_end_max_.begin(), tasks_by_end_max_.end(), 0);
  std::sort(
      tasks_by_end_max_.begin(), tasks_by_end_max_.end(),
      [&](int i, int j) { return tasks->end_max[i] < tasks->end_max[j]; });

  // Generic overload checking: insert tasks by end_max,
  // fail if envelope > end_max.
  theta_lambda_tree_.Reset(num_tasks);
  for (const int task : tasks_by_end_max_) {
    theta_lambda_tree_.AddOrUpdateEvent(
        event_of_task_[task], tasks->start_min[task], tasks->duration_min[task],
        tasks->duration_min[task]);
    if (theta_lambda_tree_.GetEnvelope() > tasks->end_max[task]) {
      return false;
    }
  }

  // Generic edge finding: from full set of tasks, at each end_max event in
  // decreasing order, check lambda feasibility, then move end_max task from
  // theta to lambda.
  for (int i = num_tasks - 1; i >= 0; --i) {
    const int task = tasks_by_end_max_[i];
    const int64 envelope = theta_lambda_tree_.GetEnvelope();
    // If a nonpreemptible optional would overload end_max, push to envelope.
    while (theta_lambda_tree_.GetOptionalEnvelope() > tasks->end_max[task]) {
      int critical_event;  // Dummy value.
      int optional_event;
      int64 available_energy;  // Dummy value.
      theta_lambda_tree_.GetEventsWithOptionalEnvelopeGreaterThan(
          tasks->end_max[task], &critical_event, &optional_event,
          &available_energy);
      const int optional_task = tasks_by_start_min_[optional_event];
      tasks->start_min[optional_task] =
          std::max(tasks->start_min[optional_task], envelope);
      theta_lambda_tree_.RemoveEvent(optional_event);
    }
    if (!tasks->is_preemptible[task]) {
      theta_lambda_tree_.AddOrUpdateOptionalEvent(event_of_task_[task],
                                                  tasks->start_min[task],
                                                  tasks->duration_min[task]);
    } else {
      theta_lambda_tree_.RemoveEvent(event_of_task_[task]);
    }
  }
  return true;
}

bool DisjunctivePropagator::DetectablePrecedencesWithChain(Tasks* tasks) {
  const int num_tasks = tasks->start_min.size();
  // Prepare start_min events for tree.
  tasks_by_start_min_.resize(num_tasks);
  std::iota(tasks_by_start_min_.begin(), tasks_by_start_min_.end(), 0);
  std::sort(
      tasks_by_start_min_.begin(), tasks_by_start_min_.end(),
      [&](int i, int j) { return tasks->start_min[i] < tasks->start_min[j]; });
  event_of_task_.resize(num_tasks);
  for (int event = 0; event < num_tasks; ++event) {
    event_of_task_[tasks_by_start_min_[event]] = event;
  }
  theta_lambda_tree_.Reset(num_tasks);

  // Sort nonchain tasks by start max = end_max - duration_min.
  const int num_chain_tasks = tasks->num_chain_tasks;
  nonchain_tasks_by_start_max_.resize(num_tasks - num_chain_tasks);
  std::iota(nonchain_tasks_by_start_max_.begin(),
            nonchain_tasks_by_start_max_.end(), num_chain_tasks);
  std::sort(nonchain_tasks_by_start_max_.begin(),
            nonchain_tasks_by_start_max_.end(), [&tasks](int i, int j) {
              return tasks->end_max[i] - tasks->duration_min[i] <
                     tasks->end_max[j] - tasks->duration_min[j];
            });

  // Detectable precedences, specialized for routes: for every task on route,
  // put all tasks before it in the tree, then push with envelope.
  int index_nonchain = 0;
  for (int i = 0; i < num_chain_tasks; ++i) {
    if (!tasks->is_preemptible[i]) {
      // Add all nonchain tasks detected before i.
      while (index_nonchain < nonchain_tasks_by_start_max_.size()) {
        const int task = nonchain_tasks_by_start_max_[index_nonchain];
        if (tasks->end_max[task] - tasks->duration_min[task] >=
            tasks->start_min[i] + tasks->duration_min[i])
          break;
        theta_lambda_tree_.AddOrUpdateEvent(
            event_of_task_[task], tasks->start_min[task],
            tasks->duration_min[task], tasks->duration_min[task]);
        index_nonchain++;
      }
    }
    // All chain and nonchain tasks before i are now in the tree, push i.
    const int64 new_start_min = theta_lambda_tree_.GetEnvelope();
    // Add i to the tree before updating it.
    theta_lambda_tree_.AddOrUpdateEvent(event_of_task_[i], tasks->start_min[i],
                                        tasks->duration_min[i],
                                        tasks->duration_min[i]);
    tasks->start_min[i] = std::max(tasks->start_min[i], new_start_min);
  }
  return true;
}

bool DisjunctivePropagator::ForbiddenIntervals(Tasks* tasks) {
  if (tasks->forbidden_intervals.empty()) return true;
  const int num_tasks = tasks->start_min.size();
  for (int task = 0; task < num_tasks; ++task) {
    if (tasks->duration_min[task] == 0) continue;
    if (tasks->forbidden_intervals[task] == nullptr) continue;
    // If start_min forbidden, push to next feasible value.
    {
      const auto& interval =
          tasks->forbidden_intervals[task]->FirstIntervalGreaterOrEqual(
              tasks->start_min[task]);
      if (interval == tasks->forbidden_intervals[task]->end()) continue;
      if (interval->start <= tasks->start_min[task]) {
        tasks->start_min[task] = CapAdd(interval->end, 1);
      }
    }
    // If end_max forbidden, push to next feasible value.
    {
      const int64 start_max =
          CapSub(tasks->end_max[task], tasks->duration_min[task]);
      const auto& interval =
          tasks->forbidden_intervals[task]->LastIntervalLessOrEqual(start_max);
      if (interval == tasks->forbidden_intervals[task]->end()) continue;
      if (interval->end >= start_max) {
        tasks->end_max[task] =
            CapAdd(interval->start, tasks->duration_min[task] - 1);
      }
    }
    if (CapAdd(tasks->start_min[task], tasks->duration_min[task]) >
        tasks->end_max[task]) {
      return false;
    }
  }
  return true;
}

TypeIncompatibilityChecker::TypeIncompatibilityChecker(
    const RoutingModel& model)
    : model_(model) {
  if (!model.HasTemporalTypeIncompatibilities() &&
      !model.HasHardTypeIncompatibilities()) {
    return;
  }
  pickup_delivery_status_of_node_.resize(model.Size());
  for (int node_index = 0; node_index < model.Size(); node_index++) {
    const std::vector<std::pair<int, int>>& pickup_index_pairs =
        model.GetPickupIndexPairs(node_index);
    const std::vector<std::pair<int, int>>& delivery_index_pairs =
        model.GetDeliveryIndexPairs(node_index);
    if (!pickup_index_pairs.empty()) {
      // Pickup node. We verify that it's not a delivery and that it appears in
      // a single pickup index pair.
      CHECK(delivery_index_pairs.empty());
      CHECK_EQ(pickup_index_pairs.size(), 1);
      pickup_delivery_status_of_node_[node_index] = PICKUP;
    } else if (!delivery_index_pairs.empty()) {
      // Delivery node. Check that it appears in a single delivery index pair.
      CHECK_EQ(delivery_index_pairs.size(), 1);
      pickup_delivery_status_of_node_[node_index] = DELIVERY;
    } else {
      // Neither pickup nor delivery.
      pickup_delivery_status_of_node_[node_index] = NONE;
    }
  }
}

bool TypeIncompatibilityChecker::TemporalIncompatibilitiesRespectedOnVehicle(
    int vehicle, const std::function<int64(int64)>& next_accessor) const {
  return IncompatibilitiesRespectedOnVehicle(
      vehicle, next_accessor, /*check_hard_incompatibilities=*/true);
}

bool TypeIncompatibilityChecker::AllIncompatibilitiesRespectedOnVehicle(
    int vehicle, const std::function<int64(int64)>& next_accessor) const {
  return IncompatibilitiesRespectedOnVehicle(
      vehicle, next_accessor, /*check_hard_incompatibilities=*/true);
}

// TODO(user): Remove the check_hard_incompatibilities boolean and always
// check both incompatibilities to simplify the code.
bool TypeIncompatibilityChecker::IncompatibilitiesRespectedOnVehicle(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    bool check_hard_incompatibilities) const {
  if (!model_.HasTemporalTypeIncompatibilities() &&
      (!check_hard_incompatibilities ||
       !model_.HasHardTypeIncompatibilities())) {
    return true;
  }
  struct NodeCount {
    int non_pickup_delivery = 0;
    int pickup = 0;
    int delivery = 0;
  };
  // Accumulates the count of types before the current node.
  std::vector<NodeCount> counts_of_type(model_.GetNumberOfVisitTypes());

  for (int64 current = model_.Start(vehicle); !model_.IsEnd(current);
       current = next_accessor(current)) {
    if (model_.GetTemporalTypeIncompatibilitiesOfNode(current).empty() &&
        (!check_hard_incompatibilities ||
         model_.GetHardTypeIncompatibilitiesOfNode(current).empty())) {
      continue;
    }
    const int type = model_.GetVisitType(current);
    // If the node had no type, its incompatibilities would have been empty.
    DCHECK_GE(type, 0);
    DCHECK_LT(type, counts_of_type.size());
    const PickupDeliveryStatus pickup_delivery_status =
        pickup_delivery_status_of_node_[current];
    NodeCount& counts = counts_of_type[type];
    if (pickup_delivery_status == DELIVERY) {
      // The node is a delivery.
      if (counts.pickup <= counts.delivery) {
        // All pickups for this type have already been delivered, so we're
        // missing a pickup for this delivery.
        return false;
      }
      counts.delivery++;
      continue;
    }
    // The node is either a pickup or a "fixed" (non-pickup/delivery) node.
    // Verify incompatibilities.

    for (int incompatible_type :
         model_.GetTemporalTypeIncompatibilitiesOfNode(current)) {
      const NodeCount& incompatible_counts = counts_of_type[incompatible_type];
      const int non_delivered_count =
          incompatible_counts.pickup - incompatible_counts.delivery;
      if (non_delivered_count + incompatible_counts.non_pickup_delivery > 0) {
        return false;
      }
    }
    if (check_hard_incompatibilities) {
      for (int incompatible_type :
           model_.GetHardTypeIncompatibilitiesOfNode(current)) {
        const NodeCount& incompatible_counts =
            counts_of_type[incompatible_type];
        if (incompatible_counts.non_pickup_delivery +
                incompatible_counts.pickup >
            0) {
          return false;
        }
      }
    }
    // Update count of type based on whether it is a pickup or not.
    int& count = pickup_delivery_status == NONE ? counts.non_pickup_delivery
                                                : counts.pickup;
    count++;
  }
  return true;
}

TypeIncompatibilityConstraint::TypeIncompatibilityConstraint(
    const RoutingModel& model)
    : Constraint(model.solver()),
      model_(model),
      incompatibility_checker_(model),
      vehicle_demons_(model.vehicles()) {}

void TypeIncompatibilityConstraint::PropagateNodeIncompatibilities(int node) {
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

void TypeIncompatibilityConstraint::CheckIncompatibilitiesOnVehicle(
    int vehicle) {
  if (!incompatibility_checker_.AllIncompatibilitiesRespectedOnVehicle(
          vehicle, /*next_accessor*/ [this, vehicle](int64 node) {
            if (model_.NextVar(node)->Bound()) {
              return model_.NextVar(node)->Value();
            }
            // Node not bound, skip to the end of the vehicle.
            return model_.End(vehicle);
          })) {
    model_.solver()->Fail();
  }
}

void TypeIncompatibilityConstraint::Post() {
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    vehicle_demons_[vehicle] = MakeDelayedConstraintDemon1(
        solver(), this,
        &TypeIncompatibilityConstraint::CheckIncompatibilitiesOnVehicle,
        "CheckIncompatibilitiesOnVehicle", vehicle);
  }
  for (int node = 0; node < model_.Size(); node++) {
    Demon* node_demon = MakeConstraintDemon1(
        solver(), this,
        &TypeIncompatibilityConstraint::PropagateNodeIncompatibilities,
        "PropagateNodeIncompatibilities", node);
    model_.NextVar(node)->WhenBound(node_demon);
    model_.VehicleVar(node)->WhenBound(node_demon);
  }
}

void TypeIncompatibilityConstraint::InitialPropagate() {
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    CheckIncompatibilitiesOnVehicle(vehicle);
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
  if (!vehicle_break_intervals_.empty()) {
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

void RoutingDimension::SetSpanUpperBoundForVehicle(int64 upper_bound,
                                                   int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_span_upper_bounds_.size());
  CHECK_GE(upper_bound, 0);
  vehicle_span_upper_bounds_[vehicle] = upper_bound;
  Solver* const solver = model_->solver();
  IntVar* const start = cumuls_[model_->Start(vehicle)];
  IntVar* const end = cumuls_[model_->End(vehicle)];
  solver->AddConstraint(
      solver->MakeLessOrEqual(solver->MakeDifference(end, start), upper_bound));
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
      IntVar* cost_var = nullptr;
      if (model_->IsEnd(i)) {
        // No active variable for end nodes, always active.
        cost_var = expr->Var();
      } else {
        cost_var = solver->MakeProd(expr, model_->ActiveVar(i))->Var();
      }
      cost_elements->push_back(cost_var);
      // TODO(user): Check if it wouldn't be better to minimize
      // piecewise_linear_cost.var here.
      model_->AddVariableMinimizedByFinalizer(cost_var);
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
      IntVar* cost_var = nullptr;
      if (model_->IsEnd(i)) {
        // No active variable for end nodes, always active.
        cost_var = expr->Var();
      } else {
        cost_var = solver->MakeProd(expr, model_->ActiveVar(i))->Var();
      }
      cost_elements->push_back(cost_var);
      // TODO(user): Check if it wouldn't be better to minimize
      // soft_bound.var here.
      model_->AddVariableMinimizedByFinalizer(cost_var);
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
      IntVar* cost_var = nullptr;
      if (model_->IsEnd(i)) {
        // No active variable for end nodes, always active.
        cost_var = expr->Var();
      } else {
        cost_var = solver->MakeProd(expr, model_->ActiveVar(i))->Var();
      }
      cost_elements->push_back(cost_var);
      model_->AddVariableMaximizedByFinalizer(soft_bound.var);
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
      end_cumuls.push_back(cumuls_[model_->End(i)]);
    }
    IntVar* const max_end_cumul = solver->MakeMax(end_cumuls)->Var();
    model_->AddVariableMinimizedByFinalizer(max_end_cumul);
    std::vector<IntVar*> start_cumuls;
    for (int i = 0; i < model_->vehicles(); ++i) {
      start_cumuls.push_back(cumuls_[model_->Start(i)]);
    }
    IntVar* const min_start_cumul = solver->MakeMin(start_cumuls)->Var();
    model_->AddVariableMaximizedByFinalizer(min_start_cumul);
    // If there is a single vehicle, model the cost as the sum of its transits
    // to avoid slow (infinite) propagation loops.
    // TODO(user): Avoid slow propagation in the path constraints.
    if (model_->vehicles() == 1) {
      for (int var_index = 0; var_index < model_->Size(); ++var_index) {
        model_->AddVariableMinimizedByFinalizer(slacks_[var_index]);
        cost_elements->push_back(
            solver
                ->MakeProd(solver->MakeProd(
                               solver->MakeSum(transits_[var_index],
                                               dependent_transits_[var_index]),
                               global_span_cost_coefficient_),
                           model_->ActiveVar(var_index))
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
  SetBreakIntervalsOfVehicle(std::move(breaks), vehicle,
                             std::move(node_visit_transits),
                             [](int64, int64) { return 0; });
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle,
    std::vector<int64> node_visit_transits,
    std::function<int64(int64 from_index, int64 to_index)> group_delay) {
  for (IntervalVar* const interval : breaks) {
    model_->AddIntervalToAssignment(interval);
    model_->AddVariableMinimizedByFinalizer(interval->SafeStartExpr(0)->Var());
  }
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, model_->vehicles());
  if (vehicle_node_visit_transits_.empty()) {
    vehicle_node_visit_transits_.resize(model_->vehicles());
    vehicle_break_intervals_.resize(model_->vehicles());
    vehicle_group_delays_.resize(model_->vehicles(),
                                 [](int64, int64) { return 0; });
  }
  DCHECK_EQ(0, vehicle_node_visit_transits_[vehicle].size());
  vehicle_node_visit_transits_[vehicle] = std::move(node_visit_transits);
  vehicle_break_intervals_[vehicle] = std::move(breaks);
  vehicle_group_delays_[vehicle] = std::move(group_delay);
}

bool RoutingDimension::VehicleHasBreakIntervals(int vehicle) const {
  DCHECK_LE(0, vehicle);
  return (vehicle < vehicle_break_intervals_.size()) &&
         !vehicle_break_intervals_[vehicle].empty();
}

const std::vector<IntervalVar*>& RoutingDimension::GetBreakIntervalsOfVehicle(
    int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_break_intervals_.size());
  return vehicle_break_intervals_[vehicle];
}

const std::vector<int64>& RoutingDimension::GetNodeVisitTransitsOfVehicle(
    int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_node_visit_transits_.size());
  return vehicle_node_visit_transits_[vehicle];
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

void RoutingDimension::SetupSlackAndDependentTransitCosts(
    std::vector<IntVar*>* cost_elements) const {
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
      model_->AddVariableMaximizedByFinalizer((*it)->cumuls_[model_->Start(i)]);
      model_->AddVariableMinimizedByFinalizer((*it)->cumuls_[model_->End(i)]);
    }
    for (IntVar* const slack : (*it)->slacks_) {
      model_->AddVariableMinimizedByFinalizer(slack);
    }
  }
}
}  // namespace operations_research
