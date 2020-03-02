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

#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/file.h"
#include "ortools/sat/cp_model_loader.h"
#endif  // __PORTABLE_PLATFORM__

#include "absl/container/flat_hash_set.h"
#include "absl/random/random.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/time_limit.h"

DEFINE_bool(cp_model_dump_solutions, false,
            "DEBUG ONLY. If true, all the intermediate solution will be dumped "
            "under '/tmp/solution_xxx.pb.txt'.");
DEFINE_string(
    cp_model_load_debug_solution, "",
    "DEBUG ONLY. When this is set to a non-empty file name, "
    "we will interpret this as an internal solution which can be used for "
    "debugging. For instance we use it to identify wrong cuts/reasons.");

namespace operations_research {
namespace sat {

int SharedSolutionRepository::NumSolutions() const {
  absl::MutexLock mutex_lock(&mutex_);
  return solutions_.size();
}

SharedSolutionRepository::Solution SharedSolutionRepository::GetSolution(
    int i) const {
  absl::MutexLock mutex_lock(&mutex_);
  return solutions_[i];
}

// TODO(user): Experiments on the best distribution.
SharedSolutionRepository::Solution
SharedSolutionRepository::GetRandomBiasedSolution(
    random_engine_t* random) const {
  absl::MutexLock mutex_lock(&mutex_);
  const int64 best_objective = solutions_[0].internal_objective;

  // As long as we have solution with the best objective that haven't been
  // explored too much, we select one uniformly. Otherwise, we select a solution
  // from the pool uniformly.
  //
  // Note(user): Because of the increase of num_selected, this is dependent on
  // the order of call. It should be fine for "determinism" because we do
  // generate the task of a batch always in the same order.
  const int kExplorationThreshold = 100;

  // Select all the best solution with a low enough selection count.
  tmp_indices_.clear();
  for (int i = 0; i < solutions_.size(); ++i) {
    const auto& solution = solutions_[i];
    if (solution.internal_objective == best_objective &&
        solution.num_selected <= kExplorationThreshold) {
      tmp_indices_.push_back(i);
    }
  }

  int index = 0;
  if (tmp_indices_.empty()) {
    index = tmp_indices_[absl::Uniform<int>(*random, 0, tmp_indices_.size())];
  } else {
    index = absl::Uniform<int>(*random, 0, solutions_.size());
  }
  solutions_[index].num_selected++;
  return solutions_[index];
}

void SharedSolutionRepository::Add(const Solution& solution) {
  absl::MutexLock mutex_lock(&mutex_);
  int worse_solution_index = 0;
  for (int i = 0; i < new_solutions_.size(); ++i) {
    // Do not add identical solution.
    if (new_solutions_[i] == solution) return;
    if (new_solutions_[worse_solution_index] < new_solutions_[i]) {
      worse_solution_index = i;
    }
  }
  if (new_solutions_.size() < num_solutions_to_keep_) {
    new_solutions_.push_back(solution);
  } else if (solution < new_solutions_[worse_solution_index]) {
    new_solutions_[worse_solution_index] = solution;
  }
}

void SharedSolutionRepository::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  solutions_.insert(solutions_.end(), new_solutions_.begin(),
                    new_solutions_.end());
  new_solutions_.clear();

  // We use a stable sort to keep the num_selected count for the already
  // existing solutions.
  //
  // TODO(user): Intoduce a notion of orthogonality to diversify the pool?
  gtl::STLStableSortAndRemoveDuplicates(&solutions_);
  if (solutions_.size() > num_solutions_to_keep_) {
    solutions_.resize(num_solutions_to_keep_);
  }
}

// TODO(user): Experiments and play with the num_solutions_to_keep parameter.
SharedResponseManager::SharedResponseManager(
    bool log_updates, bool enumerate_all_solutions, const CpModelProto* proto,
    const WallTimer* wall_timer, const SharedTimeLimit* shared_time_limit)
    : log_updates_(log_updates),
      enumerate_all_solutions_(enumerate_all_solutions),
      model_proto_(*proto),
      wall_timer_(*wall_timer),
      shared_time_limit_(*shared_time_limit),
      solutions_(/*num_solutions_to_keep=*/3) {}

namespace {

void LogNewSolution(const std::string& event_or_solution_count,
                    double time_in_seconds, double obj_best, double obj_lb,
                    double obj_ub, const std::string& solution_info) {
  const std::string obj_next =
      absl::StrFormat("next:[%.9g,%.9g]", obj_lb, obj_ub);
  LOG(INFO) << absl::StrFormat("#%-5s %6.2fs best:%-5.9g %-15s %s",
                               event_or_solution_count, time_in_seconds,
                               obj_best, obj_next, solution_info);
}

void LogNewSatSolution(const std::string& event_or_solution_count,
                       double time_in_seconds,
                       const std::string& solution_info) {
  LOG(INFO) << absl::StrFormat("#%-5s %6.2fs  %s", event_or_solution_count,
                               time_in_seconds, solution_info);
}

}  // namespace

void SharedResponseManager::UpdatePrimalIntegral() {
  absl::MutexLock mutex_lock(&mutex_);
  if (!model_proto_.has_objective()) return;

  const double current_time = shared_time_limit_.GetElapsedDeterministicTime();
  const double time_delta = current_time - last_primal_integral_time_stamp_;
  last_primal_integral_time_stamp_ = current_time;

  // We use the log of the absolute objective gap.
  //
  // Using the log should count no solution as just log(2*64) = 18, and
  // otherwise just compare order of magnitude which seems nice. Also, It is
  // more easy to compare the primal integral with the total time.
  const CpObjectiveProto& obj = model_proto_.objective();
  const double factor =
      obj.scaling_factor() != 0.0 ? std::abs(obj.scaling_factor()) : 1.0;
  const double bounds_delta = std::log(
      1 + factor * std::abs(static_cast<double>(inner_objective_upper_bound_) -
                            static_cast<double>(inner_objective_lower_bound_)));
  primal_integral_ += time_delta * bounds_delta;
}

void SharedResponseManager::UpdateInnerObjectiveBounds(
    const std::string& worker_info, IntegerValue lb, IntegerValue ub) {
  absl::MutexLock mutex_lock(&mutex_);
  CHECK(model_proto_.has_objective());

  // The problem is already solved!
  //
  // TODO(user): A thread might not be notified right away that the new bounds
  // that it is pushing make the problem infeasible. Fix that. For now we just
  // abort early here to avoid logging the "#Done" message multiple times.
  if (inner_objective_lower_bound_ > inner_objective_upper_bound_) {
    return;
  }

  const bool change =
      (lb > inner_objective_lower_bound_ || ub < inner_objective_upper_bound_);
  if (lb > inner_objective_lower_bound_) {
    inner_objective_lower_bound_ = lb.value();
  }
  if (ub < inner_objective_upper_bound_) {
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
    const double best =
        ScaleObjectiveValue(obj, best_solution_objective_value_);
    double new_lb = ScaleObjectiveValue(obj, inner_objective_lower_bound_);
    double new_ub = ScaleObjectiveValue(obj, inner_objective_upper_bound_);
    if (model_proto_.objective().scaling_factor() < 0) {
      std::swap(new_lb, new_ub);
    }
    LogNewSolution("Bound", wall_timer_.Get(), best, new_lb, new_ub,
                   worker_info);
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
    CHECK_EQ(num_solutions_, 0);
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

IntegerValue SharedResponseManager::BestSolutionInnerObjectiveValue() {
  absl::MutexLock mutex_lock(&mutex_);
  return IntegerValue(best_solution_objective_value_);
}

double SharedResponseManager::PrimalIntegral() const {
  absl::MutexLock mutex_lock(&mutex_);
  return primal_integral_;
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
  best_response_.set_primal_integral(primal_integral_);
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

  if (model_proto_.has_objective()) {
    const int64 objective_value =
        ComputeInnerObjective(model_proto_.objective(), response);

    // Add this solution to the pool, even if it is not improving.
    if (!response.solution().empty()) {
      SharedSolutionRepository::Solution solution;
      solution.variable_values.assign(response.solution().begin(),
                                      response.solution().end());
      solution.internal_objective = objective_value;
      solutions_.Add(solution);
    }

    // Ignore any non-strictly improving solution.
    if (objective_value > inner_objective_upper_bound_) return;

    // Our inner_objective_upper_bound_ should be a globaly valid bound, until
    // the problem become infeasible (i.e the lb > ub) in which case the bound
    // is no longer globally valid. Here, because we have a strictly improving
    // solution, we shouldn't be in the infeasible setting yet.
    DCHECK_GE(objective_value, inner_objective_lower_bound_);

    DCHECK_LT(objective_value, best_solution_objective_value_);
    DCHECK_NE(best_response_.status(), CpSolverStatus::OPTIMAL);
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
                      " num_bool:", model->Get<Trail>()->NumVariables());
    }

    if (model_proto_.has_objective()) {
      const CpObjectiveProto& obj = model_proto_.objective();
      const double best =
          ScaleObjectiveValue(obj, best_solution_objective_value_);
      double lb = ScaleObjectiveValue(obj, inner_objective_lower_bound_);
      double ub = ScaleObjectiveValue(obj, inner_objective_upper_bound_);
      if (model_proto_.objective().scaling_factor() < 0) {
        std::swap(lb, ub);
      }
      LogNewSolution(absl::StrCat(num_solutions_), wall_timer_.Get(), best, lb,
                     ub, solution_info);
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

#if !defined(__PORTABLE_PLATFORM__)
  if (FLAGS_cp_model_dump_solutions) {
    const std::string file =
        absl::StrCat("/tmp/solution_", num_solutions_, ".pb.txt");
    LOG(INFO) << "Dumping solution to '" << file << "'.";
    CHECK_OK(file::SetTextProto(file, best_response_, file::Defaults()));
  }
#endif  // __PORTABLE_PLATFORM__
}

void SharedResponseManager::LoadDebugSolution(Model* model) {
#if !defined(__PORTABLE_PLATFORM__)
  if (FLAGS_cp_model_load_debug_solution.empty()) return;
  if (model->Get<DebugSolution>() != nullptr) return;  // Already loaded.

  CpSolverResponse response;
  LOG(INFO) << "Reading solution from '" << FLAGS_cp_model_load_debug_solution
            << "'.";
  CHECK_OK(file::GetTextProto(FLAGS_cp_model_load_debug_solution, &response,
                              file::Defaults()));

  const auto& mapping = *model->GetOrCreate<CpModelMapping>();
  auto& debug_solution = *model->GetOrCreate<DebugSolution>();
  debug_solution.resize(
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value());
  for (int i = 0; i < response.solution().size(); ++i) {
    if (!mapping.IsInteger(i)) continue;
    const IntegerVariable var = mapping.Integer(i);
    debug_solution[var] = response.solution(i);
    debug_solution[NegationOf(var)] = -response.solution(i);
  }

  // The objective variable is usually not part of the proto, but it is still
  // nice to have it, so we recompute it here.
  auto* objective_def = model->Get<ObjectiveDefinition>();
  if (objective_def == nullptr) return;

  const IntegerVariable objective_var = objective_def->objective_var;
  const int64 objective_value =
      ComputeInnerObjective(model_proto_.objective(), response);
  debug_solution[objective_var] = objective_value;
  debug_solution[NegationOf(objective_var)] = -objective_value;
#endif  // __PORTABLE_PLATFORM__
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

bool SharedResponseManager::ProblemIsSolved() const {
  absl::MutexLock mutex_lock(&mutex_);

  if (!model_proto_.has_objective() &&
      best_response_.status() == CpSolverStatus::FEASIBLE &&
      !enumerate_all_solutions_) {
    return true;
  }

  return best_response_.status() == CpSolverStatus::OPTIMAL ||
         best_response_.status() == CpSolverStatus::INFEASIBLE;
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

}  // namespace sat
}  // namespace operations_research
