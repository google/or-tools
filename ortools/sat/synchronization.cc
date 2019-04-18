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

#include "ortools/sat/synchronization.h"

#include "absl/container/flat_hash_set.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

SharedResponseManager::SharedResponseManager(bool log_updates,
                                             const CpModelProto* proto,
                                             const WallTimer* wall_timer)
    : log_updates_(log_updates),
      model_proto_(*proto),
      wall_timer_(*wall_timer) {}

void SharedResponseManager::UpdateInnerObjectiveBounds(
    const std::string& worker_info, IntegerValue lb, IntegerValue ub) {
  CHECK(model_proto_.has_objective());

  absl::MutexLock mutex_lock(&mutex_);
  bool change = false;
  if (lb > inner_objective_lower_bound_) {
    change = true;
    inner_objective_lower_bound_ = lb.value();
  }
  if (ub < inner_objective_upper_bound_) {
    change = true;
    inner_objective_upper_bound_ = ub.value();
  }
  if (inner_objective_lower_bound_ > inner_objective_upper_bound_) {
    if (best_response_.status() == CpSolverStatus::FEASIBLE ||
        best_response_.status() == CpSolverStatus::OPTIMAL) {
      best_response_.set_status(CpSolverStatus::OPTIMAL);
    } else {
      best_response_.set_status(CpSolverStatus::INFEASIBLE);
    }
    if (log_updates_) LogNewSatSolution("Done", wall_timer_.Get(), worker_info);
    return;
  }
  if (log_updates_ && change) {
    const CpObjectiveProto& obj = model_proto_.objective();
    double new_lb = ScaleObjectiveValue(obj, inner_objective_lower_bound_);
    double new_ub = ScaleObjectiveValue(obj, inner_objective_upper_bound_);
    if (model_proto_.objective().scaling_factor() < 0) {
      std::swap(new_lb, new_ub);
    }
    LogNewSolution("Bound", wall_timer_.Get(), new_lb, new_ub, worker_info);
  }
}

// Invariant: the status always start at UNKNOWN and can only evolve as follow:
// UNKNOWN -> FEASIBLE -> OPTIMAL
// UNKNOWN -> INFEASIBLE
void SharedResponseManager::NotifyThatImprovingProblemIsInfeasible(
    const std::string& worker_info) {
  absl::MutexLock mutex_lock(&mutex_);
  if (best_response_.status() == CpSolverStatus::FEASIBLE ||
      best_response_.status() == CpSolverStatus::OPTIMAL) {
    // We also use this status to indicate that we enumerated all solutions to
    // a feasible problem.
    best_response_.set_status(CpSolverStatus::OPTIMAL);
    if (!model_proto_.has_objective()) {
      best_response_.set_all_solutions_were_found(true);
    }
  } else {
    best_response_.set_status(CpSolverStatus::INFEASIBLE);
  }
  if (log_updates_) LogNewSatSolution("Done", wall_timer_.Get(), worker_info);
}

IntegerValue SharedResponseManager::GetInnerObjectiveLowerBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return IntegerValue(inner_objective_lower_bound_);
}

IntegerValue SharedResponseManager::GetInnerObjectiveUpperBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return IntegerValue(inner_objective_upper_bound_);
}

int SharedResponseManager::AddSolutionCallback(
    std::function<void(const CpSolverResponse&)> callback) {
  absl::MutexLock mutex_lock(&mutex_);
  const int id = next_callback_id_++;
  callbacks_.emplace_back(id, std::move(callback));
  return id;
}

void SharedResponseManager::UnregisterCallback(int callback_id) {
  absl::MutexLock mutex_lock(&mutex_);
  for (int i = 0; i < callbacks_.size(); ++i) {
    if (callbacks_[i].first == callback_id) {
      callbacks_.erase(callbacks_.begin() + i);
      return;
    }
  }
  LOG(DFATAL) << "Callback id " << callback_id << " not registered.";
}

CpSolverResponse SharedResponseManager::GetResponse() {
  absl::MutexLock mutex_lock(&mutex_);
  FillObjectiveValuesInBestResponse();
  return best_response_;
}

void SharedResponseManager::FillObjectiveValuesInBestResponse() {
  if (!model_proto_.has_objective()) return;
  const CpObjectiveProto& obj = model_proto_.objective();

  if (best_response_.status() == CpSolverStatus::INFEASIBLE) {
    best_response_.clear_objective_value();
    best_response_.clear_best_objective_bound();
    return;
  }

  // Set the objective value.
  // If we don't have any solution, we use our inner bound.
  if (best_response_.status() == CpSolverStatus::UNKNOWN) {
    best_response_.set_objective_value(
        ScaleObjectiveValue(obj, inner_objective_upper_bound_));
  } else {
    best_response_.set_objective_value(
        ScaleObjectiveValue(obj, best_solution_objective_value_));
  }

  // Update the best bound in the response.
  // If we are at optimal, we set it to the objective value.
  if (best_response_.status() == CpSolverStatus::OPTIMAL) {
    best_response_.set_best_objective_bound(best_response_.objective_value());
  } else {
    best_response_.set_best_objective_bound(
        ScaleObjectiveValue(obj, inner_objective_lower_bound_));
  }
}

void SharedResponseManager::NewSolution(const CpSolverResponse& response,
                                        Model* model) {
  absl::MutexLock mutex_lock(&mutex_);
  CHECK_NE(best_response_.status(), CpSolverStatus::INFEASIBLE);

  int64 objective_value = 0;
  if (model_proto_.has_objective()) {
    const CpObjectiveProto& obj = model_proto_.objective();
    auto& repeated_field_values = response.solution().empty()
                                      ? response.solution_lower_bounds()
                                      : response.solution();
    for (int i = 0; i < obj.vars_size(); ++i) {
      int64 coeff = obj.coeffs(i);
      const int ref = obj.vars(i);
      const int var = PositiveRef(ref);
      if (!RefIsPositive(ref)) coeff = -coeff;
      objective_value += coeff * repeated_field_values[var];
    }

    // Ignore any non-strictly improving solution.
    // We also perform some basic checks on the inner bounds.
    CHECK_GE(objective_value, inner_objective_lower_bound_);
    if (objective_value > inner_objective_upper_bound_) return;

    CHECK_LT(objective_value, best_solution_objective_value_);
    CHECK_NE(best_response_.status(), CpSolverStatus::OPTIMAL);
    best_solution_objective_value_ = objective_value;

    // Update the new bound.
    inner_objective_upper_bound_ = objective_value - 1;
  }

  // Note that the objective will be filled by
  // FillObjectiveValuesInBestResponse().
  best_response_.set_status(CpSolverStatus::FEASIBLE);
  best_response_.set_solution_info(response.solution_info());
  *best_response_.mutable_solution() = response.solution();
  *best_response_.mutable_solution_lower_bounds() =
      response.solution_lower_bounds();
  *best_response_.mutable_solution_upper_bounds() =
      response.solution_upper_bounds();

  // Mark model as OPTIMAL if the inner bound crossed.
  if (model_proto_.has_objective() &&
      inner_objective_lower_bound_ > inner_objective_upper_bound_) {
    best_response_.set_status(CpSolverStatus::OPTIMAL);
  }

  // Logging.
  ++num_solutions_;
  if (log_updates_) {
    std::string solution_info = response.solution_info();
    if (model != nullptr) {
      absl::StrAppend(&solution_info,
                      " num_bool:", model->Get<SatSolver>()->NumVariables());
    }

    if (model_proto_.has_objective()) {
      const CpObjectiveProto& obj = model_proto_.objective();
      double lb = ScaleObjectiveValue(obj, inner_objective_lower_bound_);
      double ub = ScaleObjectiveValue(obj, objective_value);
      if (model_proto_.objective().scaling_factor() < 0) {
        std::swap(lb, ub);
      }
      LogNewSolution(absl::StrCat(num_solutions_), wall_timer_.Get(), lb, ub,
                     solution_info);
    } else {
      LogNewSatSolution(absl::StrCat(num_solutions_), wall_timer_.Get(),
                        solution_info);
    }
  }

  // Call callbacks.
  // Note that we cannot call function that try to get the mutex_ here.
  if (!callbacks_.empty()) {
    FillObjectiveValuesInBestResponse();
    SetStatsFromModelInternal(model);
    for (const auto& pair : callbacks_) {
      pair.second(best_response_);
    }
  }
}

void SharedResponseManager::SetStatsFromModel(Model* model) {
  absl::MutexLock mutex_lock(&mutex_);
  SetStatsFromModelInternal(model);
}

void SharedResponseManager::SetStatsFromModelInternal(Model* model) {
  if (model == nullptr) return;
  auto* sat_solver = model->Get<SatSolver>();
  auto* integer_trail = model->Get<IntegerTrail>();
  best_response_.set_num_booleans(sat_solver->NumVariables());
  best_response_.set_num_branches(sat_solver->num_branches());
  best_response_.set_num_conflicts(sat_solver->num_failures());
  best_response_.set_num_binary_propagations(sat_solver->num_propagations());
  best_response_.set_num_integer_propagations(
      integer_trail == nullptr ? 0 : integer_trail->num_enqueues());
  auto* time_limit = model->Get<TimeLimit>();
  best_response_.set_wall_time(time_limit->GetElapsedTime());
  best_response_.set_deterministic_time(
      time_limit->GetElapsedDeterministicTime());
}

SharedBoundsManager::SharedBoundsManager(int num_workers,
                                         const CpModelProto& model_proto)
    : num_workers_(num_workers),
      num_variables_(model_proto.variables_size()),
      changed_variables_per_workers_(num_workers),
      lower_bounds_(num_variables_, kint64min),
      upper_bounds_(num_variables_, kint64max) {
  for (int i = 0; i < num_workers_; ++i) {
    changed_variables_per_workers_[i].ClearAndResize(num_variables_);
  }
  for (int i = 0; i < num_variables_; ++i) {
    lower_bounds_[i] = model_proto.variables(i).domain(0);
    const int domain_size = model_proto.variables(i).domain_size();
    upper_bounds_[i] = model_proto.variables(i).domain(domain_size - 1);
  }
}

void SharedBoundsManager::ReportPotentialNewBounds(
    const CpModelProto& model_proto, int worker_id,
    const std::string& worker_name, const std::vector<int>& variables,
    const std::vector<int64>& new_lower_bounds,
    const std::vector<int64>& new_upper_bounds) {
  CHECK_EQ(variables.size(), new_lower_bounds.size());
  CHECK_EQ(variables.size(), new_upper_bounds.size());
  {
    absl::MutexLock mutex_lock(&mutex_);
    for (int i = 0; i < variables.size(); ++i) {
      const int var = variables[i];
      if (var >= num_variables_) continue;
      const int64 old_lb = lower_bounds_[var];
      const int64 old_ub = upper_bounds_[var];
      const int64 new_lb = new_lower_bounds[i];
      const int64 new_ub = new_upper_bounds[i];
      const bool changed_lb = new_lb > old_lb;
      const bool changed_ub = new_ub < old_ub;
      CHECK_GE(var, 0);
      if (!changed_lb && !changed_ub) continue;

      if (changed_lb) {
        lower_bounds_[var] = new_lb;
      }
      if (changed_ub) {
        upper_bounds_[var] = new_ub;
      }

      for (int j = 0; j < num_workers_; ++j) {
        if (worker_id == j) continue;
        changed_variables_per_workers_[j].Set(var);
      }
      if (VLOG_IS_ON(2)) {
        const IntegerVariableProto& var_proto = model_proto.variables(var);
        const std::string& var_name =
            var_proto.name().empty() ? absl::StrCat("anonymous_var(", var, ")")
                                     : var_proto.name();
        LOG(INFO) << "  '" << worker_name << "' exports new bounds for "
                  << var_name << ": from [" << old_lb << ", " << old_ub
                  << "] to [" << new_lb << ", " << new_ub << "]";
      }
    }
  }
}

// When called, returns the set of bounds improvements since
// the last time this method was called by the same worker.
void SharedBoundsManager::GetChangedBounds(
    int worker_id, std::vector<int>* variables,
    std::vector<int64>* new_lower_bounds,
    std::vector<int64>* new_upper_bounds) {
  variables->clear();
  new_lower_bounds->clear();
  new_upper_bounds->clear();
  {
    absl::MutexLock mutex_lock(&mutex_);
    for (const int var :
         changed_variables_per_workers_[worker_id].PositionsSetAtLeastOnce()) {
      variables->push_back(var);
      new_lower_bounds->push_back(lower_bounds_[var]);
      new_upper_bounds->push_back(upper_bounds_[var]);
    }
    changed_variables_per_workers_[worker_id].ClearAll();
  }
}

void RegisterVariableBoundsLevelZeroExport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model) {
  CHECK(shared_bounds_manager != nullptr);
  const auto broadcast_level_zero_bounds =
      [&model_proto, model, shared_bounds_manager](
          const std::vector<IntegerVariable>& modified_vars) {
        auto* integer_trail = model->Get<IntegerTrail>();
        const WorkerInfo* const worker_info = model->GetOrCreate<WorkerInfo>();
        CpModelMapping* const mapping = model->GetOrCreate<CpModelMapping>();

        std::vector<int> model_variables;
        std::vector<int64> new_lower_bounds;
        std::vector<int64> new_upper_bounds;
        absl::flat_hash_set<int> visited_variables;
        for (const IntegerVariable& var : modified_vars) {
          const IntegerVariable positive_var = PositiveVariable(var);
          const int model_var =
              mapping->GetProtoVariableFromIntegerVariable(positive_var);
          if (model_var == -1 ||
              gtl::ContainsKey(visited_variables, model_var)) {
            continue;
          } else {
            visited_variables.insert(model_var);
          }
          const int64 new_lb =
              integer_trail->LevelZeroLowerBound(positive_var).value();
          const int64 new_ub =
              integer_trail->LevelZeroUpperBound(positive_var).value();
          // TODO(user): We could imagine an API based on atomic<int64>
          // that could preemptively check if this new bounds are improving.
          model_variables.push_back(model_var);
          new_lower_bounds.push_back(new_lb);
          new_upper_bounds.push_back(new_ub);
        }
        if (!model_variables.empty()) {
          shared_bounds_manager->ReportPotentialNewBounds(
              model_proto, worker_info->worker_id, worker_info->worker_name,
              model_variables, new_lower_bounds, new_upper_bounds);
        }
      };

  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(broadcast_level_zero_bounds);
}

void RegisterVariableBoundsLevelZeroImport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model) {
  CHECK(shared_bounds_manager != nullptr);
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const WorkerInfo* const worker_info = model->GetOrCreate<WorkerInfo>();
  CpModelMapping* const mapping = model->GetOrCreate<CpModelMapping>();

  const auto& import_level_zero_bounds = [&model_proto, shared_bounds_manager,
                                          model, integer_trail, worker_info,
                                          mapping]() {
    std::vector<int> model_variables;
    std::vector<int64> new_lower_bounds;
    std::vector<int64> new_upper_bounds;
    shared_bounds_manager->GetChangedBounds(worker_info->worker_id,
                                            &model_variables, &new_lower_bounds,
                                            &new_upper_bounds);
    bool new_bounds_have_been_imported = false;
    for (int i = 0; i < model_variables.size(); ++i) {
      const int model_var = model_variables[i];
      // This can happen if a boolean variables is forced to have an
      // integer view in one thread, and not in another thread.
      if (!mapping->IsInteger(model_var)) continue;
      const IntegerVariable var = mapping->Integer(model_var);
      const IntegerValue new_lb(new_lower_bounds[i]);
      const IntegerValue new_ub(new_upper_bounds[i]);
      const IntegerValue old_lb = integer_trail->LowerBound(var);
      const IntegerValue old_ub = integer_trail->UpperBound(var);
      const bool changed_lb = new_lb > old_lb;
      const bool changed_ub = new_ub < old_ub;
      if (!changed_lb && !changed_ub) continue;

      new_bounds_have_been_imported = true;
      if (VLOG_IS_ON(2)) {
        const IntegerVariableProto& var_proto =
            model_proto.variables(model_var);
        const std::string& var_name =
            var_proto.name().empty()
                ? absl::StrCat("anonymous_var(", model_var, ")")
                : var_proto.name();
        LOG(INFO) << "  '" << worker_info->worker_name
                  << "' imports new bounds for " << var_name << ": from ["
                  << old_lb << ", " << old_ub << "] to [" << new_lb << ", "
                  << new_ub << "]";
      }

      if (changed_lb &&
          !integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(var, new_lb),
                                  {}, {})) {
        return false;
      }
      if (changed_ub &&
          !integer_trail->Enqueue(IntegerLiteral::LowerOrEqual(var, new_ub), {},
                                  {})) {
        return false;
      }
    }
    if (new_bounds_have_been_imported &&
        !model->GetOrCreate<SatSolver>()->FinishPropagation()) {
      return false;
    }
    return true;
  };
  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_level_zero_bounds);
}

void RegisterObjectiveBestBoundExport(
    IntegerVariable objective_var,
    SharedResponseManager* shared_response_manager, Model* model) {
  std::string worker_name = model->GetOrCreate<WorkerInfo>()->worker_name;
  auto* integer_trail = model->Get<IntegerTrail>();
  const auto broadcast_objective_lower_bound =
      [worker_name, objective_var, integer_trail,
       shared_response_manager](const std::vector<IntegerVariable>& unused) {
        shared_response_manager->UpdateInnerObjectiveBounds(
            worker_name, integer_trail->LevelZeroLowerBound(objective_var),
            integer_trail->LevelZeroUpperBound(objective_var));
      };
  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(
          broadcast_objective_lower_bound);
}

void RegisterObjectiveBoundsImport(
    SharedResponseManager* shared_response_manager, Model* model) {
  auto* solver = model->GetOrCreate<SatSolver>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* worker_info = model->GetOrCreate<WorkerInfo>();
  auto* objective = model->GetOrCreate<ObjectiveDefinition>();
  const auto import_objective_bounds = [solver, integer_trail, worker_info,
                                        objective, shared_response_manager]() {
    if (solver->AssumptionLevel() != 0) return true;
    bool propagate = false;

    const IntegerValue external_lb =
        shared_response_manager->GetInnerObjectiveLowerBound();
    const IntegerValue current_lb =
        integer_trail->LowerBound(objective->objective_var);
    if (external_lb > current_lb) {
      if (!integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(
                                      objective->objective_var, external_lb),
                                  {}, {})) {
        return false;
      }
      propagate = true;
    }

    const IntegerValue external_ub =
        shared_response_manager->GetInnerObjectiveUpperBound();
    const IntegerValue current_ub =
        integer_trail->UpperBound(objective->objective_var);
    if (external_ub < current_ub) {
      if (!integer_trail->Enqueue(IntegerLiteral::LowerOrEqual(
                                      objective->objective_var, external_ub),
                                  {}, {})) {
        return false;
      }
      propagate = true;
    }

    if (!propagate) return true;

    VLOG(1) << "'" << worker_info->worker_name
            << "' imports objective bounds: external ["
            << objective->ScaleIntegerObjective(external_lb) << ", "
            << objective->ScaleIntegerObjective(external_ub) << "], current ["
            << objective->ScaleIntegerObjective(current_lb) << ", "
            << objective->ScaleIntegerObjective(current_ub) << "]";

    return solver->FinishPropagation();
  };

  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_objective_bounds);
}

}  // namespace sat
}  // namespace operations_research
