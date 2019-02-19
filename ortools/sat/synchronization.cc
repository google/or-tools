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
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

SharedBoundsManager::SharedBoundsManager(int num_workers, int num_variables)
    : num_workers_(num_workers),
      num_variables_(num_variables),
      changed_variables_per_workers_(num_workers),
      lower_bounds_(num_variables, kint64min),
      upper_bounds_(num_variables, kint64max) {
  for (int i = 0; i < num_workers_; ++i) {
    changed_variables_per_workers_[i].ClearAndResize(num_variables_);
  }
}

void SharedBoundsManager::ReportPotentialNewBounds(
    int worker_id, const std::vector<int>& variables,
    const std::vector<int64>& new_lower_bounds,
    const std::vector<int64>& new_upper_bounds) {
  CHECK_EQ(variables.size(), new_lower_bounds.size());
  CHECK_EQ(variables.size(), new_upper_bounds.size());
  {
    absl::MutexLock mutex_lock(&mutex_);
    int modified_domains = 0;
    int fixed_domains = 0;
    for (int i = 0; i < variables.size(); ++i) {
      const int var = variables[i];
      if (var >= num_variables_) continue;
      const int64 new_lb = new_lower_bounds[i];
      const int64 new_ub = new_upper_bounds[i];
      CHECK_GE(var, 0);
      bool changed = false;
      if (lower_bounds_[var] < new_lb) {
        changed = true;
        lower_bounds_[var] = new_lb;
      }
      if (upper_bounds_[var] > new_ub) {
        changed = true;
        upper_bounds_[var] = new_ub;
      }
      if (changed) {
        if (lower_bounds_[var] == upper_bounds_[var]) {
          fixed_domains++;
        } else {
          modified_domains++;
        }
        for (int j = 0; j < num_workers_; ++j) {
          if (worker_id == j) continue;
          changed_variables_per_workers_[j].Set(var);
        }
      }
    }
    if (fixed_domains > 0 || modified_domains > 0) {
      VLOG(2) << "Worker " << worker_id << ": fixed domains=" << fixed_domains
              << ", modified domains=" << modified_domains << " out of "
              << variables.size() << " events";
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
          const IntegerVariableProto& var_proto =
              model_proto.variables(model_var);
          const int64 new_lb =
              integer_trail->LevelZeroLowerBound(positive_var).value();
          const int64 new_ub =
              integer_trail->LevelZeroUpperBound(positive_var).value();
          // TODO(user): We could imagine an API based on atomic<int64>
          // that could preemptively check if this new bounds are improving.
          model_variables.push_back(model_var);
          new_lower_bounds.push_back(new_lb);
          new_upper_bounds.push_back(new_ub);
          if (!var_proto.name().empty()) {
            VLOG(2) << worker_info->worker_name << " write " << var_proto.name()
                    << "(" << model_var << ")[" << new_lb << ", " << new_ub
                    << "]";
          } else {
            VLOG(2) << worker_info->worker_name << " write anonymous_var("
                    << model_var << ")[" << new_lb << ", " << new_ub << "]";
          }
        }
        if (!model_variables.empty()) {
          shared_bounds_manager->ReportPotentialNewBounds(
              worker_info->worker_id, model_variables, new_lower_bounds,
              new_upper_bounds);
        }
      };

  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(broadcast_level_zero_bounds);
}

void RegisterVariableBoundsLevelZeroImport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model) {
  CHECK(shared_bounds_manager != nullptr);

  const auto& import_lower_bounds = [&model_proto, shared_bounds_manager,
                                     model]() {
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    const WorkerInfo* const worker_info = model->GetOrCreate<WorkerInfo>();
    CpModelMapping* const mapping = model->GetOrCreate<CpModelMapping>();
    std::vector<int> model_variables;
    std::vector<int64> new_lower_bounds;
    std::vector<int64> new_upper_bounds;
    shared_bounds_manager->GetChangedBounds(worker_info->worker_id,
                                            &model_variables, &new_lower_bounds,
                                            &new_upper_bounds);
    for (int i = 0; i < model_variables.size(); ++i) {
      // This can happen if a boolean variables is forced to have an
      // integer view in one thread, and not in another thread.
      if (!mapping->IsInteger(model_variables[i])) continue;
      const IntegerVariable var = mapping->Integer(model_variables[i]);
      const IntegerValue new_lb(new_lower_bounds[i]);
      const IntegerValue new_ub(new_upper_bounds[i]);
      VLOG(2) << worker_info->worker_name << " read "
              << model_proto.variables(model_variables[i]).name() << "["
              << new_lb << ", " << new_ub << "]";
      if (!integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(var, new_lb),
                                  {}, {})) {
        return false;
      }
      if (!integer_trail->Enqueue(IntegerLiteral::LowerOrEqual(var, new_ub), {},
                                  {})) {
        return false;
      }
    }
    if (!model->GetOrCreate<SatSolver>()->FinishPropagation()) {
      return false;
    }
    return true;
  };
  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_lower_bounds);
}

void RegisterObjectiveBestBoundExport(const CpModelProto& model_proto,
                                      bool log_progress,
                                      IntegerVariable objective_var,
                                      WallTimer* wall_timer, Model* model) {
  const auto broadcast_objective_lower_bound =
      [&model_proto, objective_var, wall_timer, model,
       log_progress](const std::vector<IntegerVariable>& unused) {
        auto* integer_trail = model->Get<IntegerTrail>();
        const CpObjectiveProto& obj = model_proto.objective();
        const double new_best_bound = ScaleObjectiveValue(
            obj, integer_trail->LevelZeroLowerBound(objective_var).value());
        const double new_objective_value = ScaleObjectiveValue(
            obj, integer_trail->LevelZeroUpperBound(objective_var).value());

        const ObjectiveSynchronizationHelper* const helper =
            model->GetOrCreate<ObjectiveSynchronizationHelper>();
        const double current_best_bound = helper->get_external_best_bound();
        const double current_objective_value =
            helper->get_external_best_objective();

        // TODO(user): Unit test this lambda.
        if ((helper->scaling_factor >= 0 &&  // Unset -> = 0.0 -> minimize.
             new_best_bound > current_best_bound) ||
            (helper->scaling_factor < 0 &&
             new_best_bound < current_best_bound)) {
          if (log_progress) {
            const WorkerInfo* const worker_info =
                model->GetOrCreate<WorkerInfo>();
            const double reported_objective_value =
                std::isfinite(current_objective_value) ? current_objective_value
                                                       : new_objective_value;
            if (new_best_bound > current_best_bound) {  // minimization.
              LogNewSolution("ObjLb", wall_timer->Get(), new_best_bound,
                             reported_objective_value,
                             worker_info->worker_name);
            } else {
              LogNewSolution("ObjUb", wall_timer->Get(),
                             reported_objective_value, new_best_bound,
                             worker_info->worker_name);
            }
          }
          if (helper->set_external_best_bound) {
            helper->set_external_best_bound(current_objective_value,
                                            new_best_bound);
          }
        }
      };
  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(
          broadcast_objective_lower_bound);
}

void RegisterObjectiveBoundsImport(Model* model) {
  const auto import_objective_bounds = [model]() {
    SatSolver* const solver = model->GetOrCreate<SatSolver>();
    if (solver->AssumptionLevel() != 0) return true;

    const WorkerInfo* const worker_info = model->GetOrCreate<WorkerInfo>();
    const ObjectiveSynchronizationHelper* const helper =
        model->GetOrCreate<ObjectiveSynchronizationHelper>();
    CHECK(helper->get_external_best_bound != nullptr);
    const double external_bound = helper->get_external_best_objective();
    const double external_best_bound = helper->get_external_best_bound();
    IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
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
    if (new_objective_upper_bound < current_objective_upper_bound ||
        new_objective_lower_bound > current_objective_lower_bound) {
      if (new_objective_upper_bound < new_objective_lower_bound) {
        return false;
      }
      VLOG(1) << worker_info->worker_name << " imports objective bounds ["
              << helper->ScaledObjective(
                     std::max(new_objective_lower_bound.value(),
                              current_objective_lower_bound.value()))
              << ", "
              << helper->ScaledObjective(
                     std::min(new_objective_upper_bound.value(),
                              current_objective_upper_bound.value()))
              << "]";
    }
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
