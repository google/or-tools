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

SharedResponseManager::SharedResponseManager(const CpModelProto& proto) {
  best_response_.set_status(CpSolverStatus::UNKNOWN);
  if (proto.has_objective()) {
    const double kInfinity = std::numeric_limits<double>::infinity();
    is_maximize_ = proto.objective().scaling_factor() < 0.0;
    if (is_maximize_) {
      best_response_.set_objective_value(-kInfinity);
      best_response_.set_best_objective_bound(kInfinity);
    } else {
      best_response_.set_objective_value(kInfinity);
      best_response_.set_best_objective_bound(-kInfinity);
    }
  }
}

double SharedResponseManager::GetObjectiveValue() {
  absl::MutexLock mutex_lock(&mutex_);
  return best_response_.objective_value();
}

double SharedResponseManager::GetObjectiveBestBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return best_response_.best_objective_bound();
}

CpSolverResponse SharedResponseManager::GetBestResponse() {
  absl::MutexLock mutex_lock(&mutex_);
  return best_response_;
}

bool SharedResponseManager::MergeIntoBestResponse(
    const CpSolverResponse& response) {
  absl::MutexLock mutex_lock(&mutex_);
  return MergeOptimizationSolution(response, is_maximize_, &best_response_);
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
    const CpModelProto& model_proto, bool log_progress,
    IntegerVariable objective_var, WallTimer* wall_timer,
    SharedResponseManager* shared_response_manager, Model* model) {
  auto* integer_trail = model->Get<IntegerTrail>();
  auto* worker_info = model->GetOrCreate<WorkerInfo>();
  const CpObjectiveProto& obj = model_proto.objective();
  const auto broadcast_objective_lower_bound =
      [obj, objective_var, wall_timer, integer_trail, worker_info, log_progress,
       shared_response_manager](const std::vector<IntegerVariable>& unused) {
        const double new_best_bound = ScaleObjectiveValue(
            obj, integer_trail->LevelZeroLowerBound(objective_var).value());
        const double current_best_bound =
            shared_response_manager->GetObjectiveBestBound();
        const double current_objective_value =
            shared_response_manager->GetObjectiveValue();

        // TODO(user): we currently display "inf" for the objective if the first
        // update is a bound update. This will go away when I refactor the code
        // to not depend on the objective scaling/offset and stay with int64
        // internally.
        CpSolverResponse response;
        response.set_status(CpSolverStatus::UNKNOWN);
        const double kInfinity = std::numeric_limits<double>::infinity();
        if (shared_response_manager->IsMaximize() &&
            new_best_bound < current_best_bound) {
          response.set_objective_value(-kInfinity);
          response.set_best_objective_bound(new_best_bound);
          shared_response_manager->MergeIntoBestResponse(response);
          if (log_progress) {
            LogNewSolution("ObjUb", wall_timer->Get(), current_objective_value,
                           new_best_bound, worker_info->worker_name);
          }
        } else if (new_best_bound > current_best_bound) {
          response.set_objective_value(kInfinity);
          response.set_best_objective_bound(new_best_bound);
          shared_response_manager->MergeIntoBestResponse(response);
          if (log_progress) {
            LogNewSolution("ObjLb", wall_timer->Get(), new_best_bound,
                           current_objective_value, worker_info->worker_name);
          }
        }
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
  auto* helper = model->GetOrCreate<ObjectiveSynchronizationHelper>();
  const auto import_objective_bounds = [solver, integer_trail, worker_info,
                                        helper, shared_response_manager]() {
    if (solver->AssumptionLevel() != 0) return true;
    const double external_bound = shared_response_manager->GetObjectiveValue();
    const double external_best_bound =
        shared_response_manager->GetObjectiveBestBound();

    const IntegerValue current_objective_upper_bound(
        integer_trail->UpperBound(helper->objective_var));
    const IntegerValue current_objective_lower_bound(
        integer_trail->LowerBound(helper->objective_var));
    const IntegerValue new_objective_upper_bound(
        std::isfinite(external_bound)
            ? helper->UnscaledObjective(external_bound) - 1
            : current_objective_upper_bound.value());
    const IntegerValue new_objective_lower_bound(
        std::isfinite(external_best_bound)
            ? helper->UnscaledObjective(external_best_bound)
            : current_objective_lower_bound.value());
    if (new_objective_upper_bound < new_objective_lower_bound) {
      return false;
    }
    if (new_objective_upper_bound >= current_objective_upper_bound &&
        new_objective_lower_bound <= current_objective_lower_bound) {
      return true;
    }
    VLOG(1) << "  '" << worker_info->worker_name
            << "' imports objective bounds: external ["
            << helper->ScaledObjective(new_objective_lower_bound.value())
            << ", "
            << helper->ScaledObjective(new_objective_upper_bound.value())
            << "], internal ["
            << helper->ScaledObjective(current_objective_lower_bound.value())
            << ", "
            << helper->ScaledObjective(current_objective_upper_bound.value())
            << "]";
    if (new_objective_upper_bound < current_objective_upper_bound &&
        !integer_trail->Enqueue(
            IntegerLiteral::LowerOrEqual(helper->objective_var,
                                         new_objective_upper_bound),
            {}, {})) {
      return false;
    }
    if (new_objective_lower_bound > current_objective_lower_bound &&
        !integer_trail->Enqueue(
            IntegerLiteral::GreaterOrEqual(helper->objective_var,
                                           new_objective_lower_bound),
            {}, {})) {
      return false;
    }
    if (!solver->FinishPropagation()) {
      return false;
    }

    return true;
  };

  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_objective_bounds);
}

}  // namespace sat
}  // namespace operations_research
