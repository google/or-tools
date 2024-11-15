// Copyright 2010-2024 Google LLC
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

#include <sys/types.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/time/time.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#endif  // __PORTABLE_PLATFORM__
#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/symmetry_util.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

ABSL_FLAG(bool, cp_model_dump_solutions, false,
          "DEBUG ONLY. If true, all the intermediate solution will be dumped "
          "under '\"FLAGS_cp_model_dump_prefix\" + \"solution_xxx.pb.txt\"'.");
ABSL_FLAG(bool, cp_model_dump_tightened_models, false,
          "DEBUG ONLY. If true, dump tightened models incoporating all bounds "
          "changes under '\"FLAGS_cp_model_dump_prefix\" + "
          "\"tight_model_xxx.pb.txt\"'.");

namespace operations_research {
namespace sat {

void SharedLPSolutionRepository::NewLPSolution(
    std::vector<double> lp_solution) {
  if (lp_solution.empty()) return;

  // Add this solution to the pool.
  SharedSolutionRepository<double>::Solution solution;
  solution.variable_values = std::move(lp_solution);

  // We always prefer to keep the solution from the last synchronize batch.
  absl::MutexLock mutex_lock(&mutex_);
  solution.rank = -num_synchronization_;
  ++num_added_;
  new_solutions_.push_back(solution);
}

void SharedIncompleteSolutionManager::AddSolution(
    const std::vector<double>& lp_solution) {
  absl::MutexLock mutex_lock(&mutex_);
  ++num_added_;
  solutions_.push_back(lp_solution);
  if (solutions_.size() > 100) solutions_.pop_front();
}

bool SharedIncompleteSolutionManager::HasSolution() const {
  absl::MutexLock mutex_lock(&mutex_);
  return !solutions_.empty();
}

std::vector<double> SharedIncompleteSolutionManager::PopLast() {
  absl::MutexLock mutex_lock(&mutex_);
  if (solutions_.empty()) return {};

  ++num_queried_;
  std::vector<double> solution = std::move(solutions_.back());
  solutions_.pop_back();
  return solution;
}

SharedResponseManager::SharedResponseManager(Model* model)
    : parameters_(*model->GetOrCreate<SatParameters>()),
      wall_timer_(*model->GetOrCreate<WallTimer>()),
      shared_time_limit_(model->GetOrCreate<ModelSharedTimeLimit>()),
      solutions_(parameters_.solution_pool_size(), "feasible solutions"),
      logger_(model->GetOrCreate<SolverLogger>()) {
  bounds_logging_id_ = logger_->GetNewThrottledId();
}

namespace {

std::string ProgressMessage(absl::string_view event_or_solution_count,
                            double time_in_seconds, double obj_best,
                            double obj_lb, double obj_ub,
                            absl::string_view solution_info) {
  const std::string obj_next =
      obj_lb <= obj_ub ? absl::StrFormat("next:[%.9g,%.9g]", obj_lb, obj_ub)
                       : "next:[]";
  return absl::StrFormat("#%-5s %6.2fs best:%-5.9g %-15s %s",
                         event_or_solution_count, time_in_seconds, obj_best,
                         obj_next, solution_info);
}

std::string SatProgressMessage(const std::string& event_or_solution_count,
                               double time_in_seconds,
                               const std::string& solution_info) {
  return absl::StrFormat("#%-5s %6.2fs %s", event_or_solution_count,
                         time_in_seconds, solution_info);
}

}  // namespace

void FillSolveStatsInResponse(Model* model, CpSolverResponse* response) {
  if (model == nullptr) return;
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* integer_trail = model->Get<IntegerTrail>();
  response->set_num_booleans(sat_solver->NumVariables());
  response->set_num_branches(sat_solver->num_branches());
  response->set_num_conflicts(sat_solver->num_failures());
  response->set_num_binary_propagations(sat_solver->num_propagations());
  response->set_num_restarts(sat_solver->num_restarts());

  response->set_num_integers(
      integer_trail == nullptr
          ? 0
          : integer_trail->NumIntegerVariables().value() / 2);
  response->set_num_integer_propagations(
      integer_trail == nullptr ? 0 : integer_trail->num_enqueues());

  // TODO(user): find a way to clear all stats fields that might be set by
  // one of the callback.
  response->set_num_lp_iterations(0);
  for (const auto& set_stats :
       model->GetOrCreate<CpSolverResponseStatisticCallbacks>()->callbacks) {
    set_stats(response);
  }
}

void SharedResponseManager::LogMessage(absl::string_view prefix,
                                       absl::string_view message) {
  absl::MutexLock mutex_lock(&mutex_);
  SOLVER_LOG(logger_, absl::StrFormat("#%-5s %6.2fs %s", prefix,
                                      wall_timer_.Get(), message));
}

void SharedResponseManager::LogMessageWithThrottling(
    const std::string& prefix, const std::string& message) {
  absl::MutexLock mutex_lock(&mutex_);

  int id;
  auto it = throttling_ids_.find(prefix);
  if (it == throttling_ids_.end()) {
    id = throttling_ids_[prefix] = logger_->GetNewThrottledId();
  } else {
    id = it->second;
  }
  logger_->ThrottledLog(id, absl::StrFormat("#%-5s %6.2fs %s", prefix,
                                            wall_timer_.Get(), message));
}

bool SharedResponseManager::LoggingIsEnabled() const {
  absl::MutexLock mutex_lock(&mutex_);

  return logger_->LoggingIsEnabled();
}

void SharedResponseManager::InitializeObjective(const CpModelProto& cp_model) {
  if (cp_model.has_objective()) {
    objective_or_null_ = &cp_model.objective();
    const Domain domain = ReadDomainFromProto(cp_model.objective());
    if (!domain.IsEmpty()) {
      UpdateInnerObjectiveBounds("initial_domain", IntegerValue(domain.Min()),
                                 IntegerValue(domain.Max()));
    }
  } else {
    objective_or_null_ = nullptr;
  }
}

void SharedResponseManager::SetSynchronizationMode(bool always_synchronize) {
  absl::MutexLock mutex_lock(&mutex_);
  always_synchronize_ = always_synchronize;
}

void SharedResponseManager::SetUpdateGapIntegralOnEachChange(bool set) {
  absl::MutexLock mutex_lock(&mutex_);
  update_integral_on_each_change_ = set;
}

void SharedResponseManager::UpdateGapIntegral() {
  absl::MutexLock mutex_lock(&mutex_);
  UpdateGapIntegralInternal();
}

void SharedResponseManager::UpdateGapIntegralInternal() {
  if (objective_or_null_ == nullptr) return;

  const double current_time = shared_time_limit_->GetElapsedDeterministicTime();
  const double time_delta = current_time - last_gap_integral_time_stamp_;

  // We use the log of the absolute objective gap.
  //
  // Using the log should count no solution as just log(2*64) = 18, and
  // otherwise just compare order of magnitude which seems nice. Also, It is
  // more easy to compare the primal integral with the total time.
  const CpObjectiveProto& obj = *objective_or_null_;
  const double factor =
      obj.scaling_factor() != 0.0 ? std::abs(obj.scaling_factor()) : 1.0;
  const double bounds_delta = std::log(1 + factor * last_absolute_gap_);
  gap_integral_ += time_delta * bounds_delta;

  // Update with new value.
  last_gap_integral_time_stamp_ = current_time;
  last_absolute_gap_ =
      std::max(0.0, static_cast<double>(inner_objective_upper_bound_) -
                        static_cast<double>(inner_objective_lower_bound_));
}

void SharedResponseManager::SetGapLimitsFromParameters(
    const SatParameters& parameters) {
  absl::MutexLock mutex_lock(&mutex_);
  if (objective_or_null_ == nullptr) return;
  absolute_gap_limit_ = parameters.absolute_gap_limit();
  relative_gap_limit_ = parameters.relative_gap_limit();
}

void SharedResponseManager::TestGapLimitsIfNeeded() {
  // This is called on each internal limit change, so it is a good place to
  // update the integral. Note that this is not called at the end of the search
  // though.
  if (update_integral_on_each_change_) UpdateGapIntegralInternal();

  // Abort if there is not limit set, if the gap is not defined or if we already
  // proved optimality or infeasibility.
  if (absolute_gap_limit_ == 0 && relative_gap_limit_ == 0) return;
  if (best_solution_objective_value_ >= kMaxIntegerValue) return;
  if (inner_objective_lower_bound_ <= kMinIntegerValue) return;
  if (inner_objective_lower_bound_ > inner_objective_upper_bound_) return;

  const CpObjectiveProto& obj = *objective_or_null_;
  const double user_best =
      ScaleObjectiveValue(obj, best_solution_objective_value_);
  const double user_bound =
      ScaleObjectiveValue(obj, inner_objective_lower_bound_);
  const double gap = std::abs(user_best - user_bound);
  if (gap <= absolute_gap_limit_) {
    SOLVER_LOG(logger_, "Absolute gap limit of ", absolute_gap_limit_,
               " reached.");
    UpdateBestStatus(CpSolverStatus::OPTIMAL);

    // Note(user): Some code path in single-thread assumes that the problem
    // can only be solved when they have proven infeasibility and do not check
    // the ProblemIsSolved() method. So we force a stop here.
    if (always_synchronize_) shared_time_limit_->Stop();
  }
  if (gap / std::max(1.0, std::abs(user_best)) < relative_gap_limit_) {
    SOLVER_LOG(logger_, "Relative gap limit of ", relative_gap_limit_,
               " reached.");
    UpdateBestStatus(CpSolverStatus::OPTIMAL);

    // Same as above.
    if (always_synchronize_) shared_time_limit_->Stop();
  }
}

void SharedResponseManager::UpdateInnerObjectiveBounds(
    const std::string& update_info, IntegerValue lb, IntegerValue ub) {
  absl::MutexLock mutex_lock(&mutex_);
  CHECK(objective_or_null_ != nullptr);

  // The problem is already solved!
  //
  // TODO(user): A thread might not be notified right away that the new bounds
  // that it is pushing make the problem infeasible. Fix that. For now we just
  // abort early here to avoid logging the "#Done" message multiple times.
  if (inner_objective_lower_bound_ > inner_objective_upper_bound_) {
    return;
  }

  const bool ub_change = ub < inner_objective_upper_bound_;
  const bool lb_change = lb > inner_objective_lower_bound_;
  if (!lb_change && !ub_change) return;

  if (lb_change) {
    // When the improving problem is infeasible, it is possible to report
    // arbitrary high inner_objective_lower_bound_. We make sure it never cross
    // the current best solution, so that we always report globally valid lower
    // bound.
    DCHECK_LE(inner_objective_upper_bound_, best_solution_objective_value_);
    inner_objective_lower_bound_ =
        std::min(best_solution_objective_value_, lb.value());
  }
  if (ub_change) {
    inner_objective_upper_bound_ = ub.value();
  }

  if (always_synchronize_) {
    synchronized_inner_objective_lower_bound_ =
        IntegerValue(inner_objective_lower_bound_);
    synchronized_inner_objective_upper_bound_ =
        IntegerValue(inner_objective_upper_bound_);
  }

  if (inner_objective_lower_bound_ > inner_objective_upper_bound_) {
    if (best_status_ == CpSolverStatus::FEASIBLE ||
        best_status_ == CpSolverStatus::OPTIMAL) {
      UpdateBestStatus(CpSolverStatus::OPTIMAL);
    } else {
      UpdateBestStatus(CpSolverStatus::INFEASIBLE);
    }
    if (update_integral_on_each_change_) UpdateGapIntegralInternal();
    SOLVER_LOG(logger_,
               SatProgressMessage("Done", wall_timer_.Get(), update_info));
    return;
  }
  if (logger_->LoggingIsEnabled() || !best_bound_callbacks_.empty()) {
    const CpObjectiveProto& obj = *objective_or_null_;
    const double best =
        ScaleObjectiveValue(obj, best_solution_objective_value_);
    double new_lb = ScaleObjectiveValue(obj, inner_objective_lower_bound_);
    if (lb_change) {
      for (const auto& callback_entry : best_bound_callbacks_) {
        callback_entry.second(new_lb);
      }
    }
    if (logger_->LoggingIsEnabled()) {
      double new_ub = ScaleObjectiveValue(obj, inner_objective_upper_bound_);
      if (obj.scaling_factor() < 0) {
        std::swap(new_lb, new_ub);
      }
      RegisterObjectiveBoundImprovement(update_info);
      logger_->ThrottledLog(bounds_logging_id_,
                            ProgressMessage("Bound", wall_timer_.Get(), best,
                                            new_lb, new_ub, update_info));
    }
  }
  TestGapLimitsIfNeeded();
}

// Invariant: the status always start at UNKNOWN and can only evolve as follow:
// UNKNOWN -> FEASIBLE -> OPTIMAL
// UNKNOWN -> INFEASIBLE
void SharedResponseManager::NotifyThatImprovingProblemIsInfeasible(
    const std::string& worker_info) {
  absl::MutexLock mutex_lock(&mutex_);
  if (best_status_ == CpSolverStatus::FEASIBLE ||
      best_status_ == CpSolverStatus::OPTIMAL) {
    // We also use this status to indicate that we enumerated all solutions to
    // a feasible problem.
    UpdateBestStatus(CpSolverStatus::OPTIMAL);

    // We just proved that the best solution cannot be improved uppon, so we
    // have a new lower bound.
    inner_objective_lower_bound_ = best_solution_objective_value_;
    if (update_integral_on_each_change_) UpdateGapIntegralInternal();
  } else {
    CHECK_EQ(num_solutions_, 0);
    UpdateBestStatus(CpSolverStatus::INFEASIBLE);
  }
  SOLVER_LOG(logger_,
             SatProgressMessage("Done", wall_timer_.Get(), worker_info));
}

void SharedResponseManager::AddUnsatCore(const std::vector<int>& core) {
  absl::MutexLock mutex_lock(&mutex_);
  unsat_cores_ = core;
}

IntegerValue SharedResponseManager::GetInnerObjectiveLowerBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return synchronized_inner_objective_lower_bound_;
}

IntegerValue SharedResponseManager::GetInnerObjectiveUpperBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return synchronized_inner_objective_upper_bound_;
}

void SharedResponseManager::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  synchronized_inner_objective_lower_bound_ =
      IntegerValue(inner_objective_lower_bound_);
  synchronized_inner_objective_upper_bound_ =
      IntegerValue(inner_objective_upper_bound_);
  synchronized_best_status_ = best_status_;
  if (solutions_.NumSolutions() > 0) {
    first_solution_solvers_should_stop_ = true;
  }
  logger_->FlushPendingThrottledLogs();
}

IntegerValue SharedResponseManager::BestSolutionInnerObjectiveValue() {
  absl::MutexLock mutex_lock(&mutex_);
  return IntegerValue(best_solution_objective_value_);
}

double SharedResponseManager::GapIntegral() const {
  absl::MutexLock mutex_lock(&mutex_);
  return gap_integral_;
}

void SharedResponseManager::AddSolutionPostprocessor(
    std::function<void(std::vector<int64_t>*)> postprocessor) {
  absl::MutexLock mutex_lock(&mutex_);
  solution_postprocessors_.push_back(postprocessor);
}

void SharedResponseManager::AddResponsePostprocessor(
    std::function<void(CpSolverResponse*)> postprocessor) {
  absl::MutexLock mutex_lock(&mutex_);
  postprocessors_.push_back(postprocessor);
}

void SharedResponseManager::AddFinalResponsePostprocessor(
    std::function<void(CpSolverResponse*)> postprocessor) {
  absl::MutexLock mutex_lock(&mutex_);
  final_postprocessors_.push_back(postprocessor);
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

int SharedResponseManager::AddLogCallback(
    std::function<std::string(const CpSolverResponse&)> callback) {
  absl::MutexLock mutex_lock(&mutex_);
  const int id = next_search_log_callback_id_++;
  search_log_callbacks_.emplace_back(id, std::move(callback));
  return id;
}

void SharedResponseManager::UnregisterLogCallback(int callback_id) {
  absl::MutexLock mutex_lock(&mutex_);
  for (int i = 0; i < search_log_callbacks_.size(); ++i) {
    if (search_log_callbacks_[i].first == callback_id) {
      search_log_callbacks_.erase(search_log_callbacks_.begin() + i);
      return;
    }
  }
  LOG(DFATAL) << "Callback id " << callback_id << " not registered.";
}

int SharedResponseManager::AddBestBoundCallback(
    std::function<void(double)> callback) {
  absl::MutexLock mutex_lock(&mutex_);
  const int id = next_best_bound_callback_id_++;
  best_bound_callbacks_.emplace_back(id, std::move(callback));
  return id;
}

void SharedResponseManager::UnregisterBestBoundCallback(int callback_id) {
  absl::MutexLock mutex_lock(&mutex_);
  for (int i = 0; i < best_bound_callbacks_.size(); ++i) {
    if (best_bound_callbacks_[i].first == callback_id) {
      best_bound_callbacks_.erase(best_bound_callbacks_.begin() + i);
      return;
    }
  }
  LOG(DFATAL) << "Callback id " << callback_id << " not registered.";
}

CpSolverResponse SharedResponseManager::GetResponseInternal(
    absl::Span<const int64_t> variable_values,
    const std::string& solution_info) {
  CpSolverResponse result;
  result.set_status(best_status_);
  if (!unsat_cores_.empty()) {
    DCHECK_EQ(best_status_, CpSolverStatus::INFEASIBLE);
    result.mutable_sufficient_assumptions_for_infeasibility()->Assign(
        unsat_cores_.begin(), unsat_cores_.end());
  }
  FillObjectiveValuesInResponse(&result);
  result.set_solution_info(solution_info);

  // Tricky: We copy the solution now for the case where MergeFrom() belows
  // override it!
  //
  // TODO(user): Fix. This is messy, we should really just override stats not
  // important things like solution or status with the MergeFrom() below.
  if (best_status_ == CpSolverStatus::FEASIBLE ||
      best_status_ == CpSolverStatus::OPTIMAL) {
    result.mutable_solution()->Assign(variable_values.begin(),
                                      variable_values.end());
  }

  // Note that we allow subsolver_responses_ to override the fields set above.
  // That is the status, solution_info and objective values...
  if (!subsolver_responses_.empty()) {
    result.MergeFrom(subsolver_responses_.front());
  }

  if (result.status() == CpSolverStatus::FEASIBLE ||
      result.status() == CpSolverStatus::OPTIMAL) {
    // We need to copy the solution before we postsolve it.
    std::vector<int64_t> solution(result.solution().begin(),
                                  result.solution().end());
    for (int i = solution_postprocessors_.size(); --i >= 0;) {
      solution_postprocessors_[i](&solution);
    }
    result.mutable_solution()->Assign(solution.begin(), solution.end());
  }

  // Apply response postprocessor to set things like timing information.
  for (int i = postprocessors_.size(); --i >= 0;) {
    postprocessors_[i](&result);
  }
  return result;
}

CpSolverResponse SharedResponseManager::GetResponse() {
  absl::MutexLock mutex_lock(&mutex_);
  CpSolverResponse result =
      solutions_.NumSolutions() == 0
          ? GetResponseInternal({}, "")
          : GetResponseInternal(solutions_.GetSolution(0).variable_values,
                                solutions_.GetSolution(0).info);

  // If this is true, we postsolve and copy all of our solutions.
  if (parameters_.fill_additional_solutions_in_response()) {
    std::vector<int64_t> temp;
    for (int i = 0; i < solutions_.NumSolutions(); ++i) {
      temp = solutions_.GetSolution(i).variable_values;
      for (int i = solution_postprocessors_.size(); --i >= 0;) {
        solution_postprocessors_[i](&temp);
      }
      result.add_additional_solutions()->mutable_values()->Assign(temp.begin(),
                                                                  temp.end());
    }
  }

  // final postprocessors will print out the final log. They must be called
  // last.
  for (int i = final_postprocessors_.size(); --i >= 0;) {
    final_postprocessors_[i](&result);
  }

  return result;
}

void SharedResponseManager::AppendResponseToBeMerged(
    const CpSolverResponse& response) {
  absl::MutexLock mutex_lock(&mutex_);
  return subsolver_responses_.push_back(response);
}

void SharedResponseManager::FillObjectiveValuesInResponse(
    CpSolverResponse* response) const {
  if (objective_or_null_ == nullptr) return;
  const CpObjectiveProto& obj = *objective_or_null_;

  if (best_status_ == CpSolverStatus::INFEASIBLE) {
    response->clear_objective_value();
    response->clear_best_objective_bound();
    response->clear_inner_objective_lower_bound();
    return;
  }

  // Set the objective value.
  // If we don't have any solution, we use our inner bound.
  if (best_status_ == CpSolverStatus::UNKNOWN) {
    response->set_objective_value(
        ScaleObjectiveValue(obj, inner_objective_upper_bound_));
  } else {
    response->set_objective_value(
        ScaleObjectiveValue(obj, best_solution_objective_value_));
  }

  // Update the best bound in the response.
  response->set_inner_objective_lower_bound(
      ScaleInnerObjectiveValue(obj, inner_objective_lower_bound_));
  response->set_best_objective_bound(
      ScaleObjectiveValue(obj, inner_objective_lower_bound_));

  // Update the primal integral.
  response->set_gap_integral(gap_integral_);
}

void SharedResponseManager::NewSolution(
    absl::Span<const int64_t> solution_values, const std::string& solution_info,
    Model* model) {
  absl::MutexLock mutex_lock(&mutex_);

  // For SAT problems, we add the solution to the solution pool for retrieval
  // later.
  if (objective_or_null_ == nullptr) {
    SharedSolutionRepository<int64_t>::Solution solution;
    solution.variable_values.assign(solution_values.begin(),
                                    solution_values.end());
    solution.info = solution_info;
    solutions_.Add(solution);
  } else {
    const int64_t objective_value =
        ComputeInnerObjective(*objective_or_null_, solution_values);

    // Add this solution to the pool, even if it is not improving.
    SharedSolutionRepository<int64_t>::Solution solution;
    solution.variable_values.assign(solution_values.begin(),
                                    solution_values.end());
    solution.rank = objective_value;
    solution.info = solution_info;
    solutions_.Add(solution);

    // Ignore any non-strictly improving solution.
    if (objective_value > inner_objective_upper_bound_) return;

    // Our inner_objective_lower_bound_ should be a globaly valid bound, until
    // the problem become infeasible (i.e the lb > ub) in which case the bound
    // is no longer globally valid. Here, because we have a strictly improving
    // solution, we shouldn't be in the infeasible setting yet.
    DCHECK_GE(objective_value, inner_objective_lower_bound_);

    DCHECK_LT(objective_value, best_solution_objective_value_);
    best_solution_objective_value_ = objective_value;

    // Update the new bound.
    inner_objective_upper_bound_ = objective_value - 1;
  }

  // In single thread, no one is synchronizing the solution manager, so we
  // should do it from here.
  if (always_synchronize_) {
    solutions_.Synchronize();
    first_solution_solvers_should_stop_ = true;
  }

  // Note that the objective will be filled by
  // FillObjectiveValuesInResponse().
  if (objective_or_null_ == nullptr && !parameters_.enumerate_all_solutions()) {
    UpdateBestStatus(CpSolverStatus::OPTIMAL);
  } else {
    UpdateBestStatus(CpSolverStatus::FEASIBLE);
  }

  // Mark model as OPTIMAL if the inner bound crossed.
  if (objective_or_null_ != nullptr &&
      inner_objective_lower_bound_ > inner_objective_upper_bound_) {
    UpdateBestStatus(CpSolverStatus::OPTIMAL);
  }

  // Logging.
  ++num_solutions_;

  // Compute the post-solved response once.
  CpSolverResponse tmp_postsolved_response;
  if ((!search_log_callbacks_.empty() && logger_->LoggingIsEnabled()) ||
      !callbacks_.empty()) {
    tmp_postsolved_response =
        GetResponseInternal(solution_values, solution_info);
    FillSolveStatsInResponse(model, &tmp_postsolved_response);
  }

  // TODO(user): Remove this code and the need for model in this function.
  // Use search log callbacks instead.
  if (logger_->LoggingIsEnabled()) {
    std::string solution_message = solution_info;
    if (model != nullptr) {
      const int64_t num_bool = model->Get<Trail>()->NumVariables();
      const int64_t num_fixed = model->Get<SatSolver>()->NumFixedVariables();
      absl::StrAppend(&solution_message, " (fixed_bools=", num_fixed, "/",
                      num_bool, ")");
    }

    if (!search_log_callbacks_.empty()) {
      for (const auto& pair : search_log_callbacks_) {
        absl::StrAppend(&solution_message, " ",
                        pair.second(tmp_postsolved_response));
      }
    }

    if (objective_or_null_ != nullptr) {
      const CpObjectiveProto& obj = *objective_or_null_;
      const double best =
          ScaleObjectiveValue(obj, best_solution_objective_value_);
      double lb = ScaleObjectiveValue(obj, inner_objective_lower_bound_);
      double ub = ScaleObjectiveValue(obj, inner_objective_upper_bound_);
      if (obj.scaling_factor() < 0) {
        std::swap(lb, ub);
      }
      RegisterSolutionFound(solution_message, num_solutions_);
      SOLVER_LOG(logger_, ProgressMessage(absl::StrCat(num_solutions_),
                                          wall_timer_.Get(), best, lb, ub,
                                          solution_message));
    } else {
      SOLVER_LOG(logger_,
                 SatProgressMessage(absl::StrCat(num_solutions_),
                                    wall_timer_.Get(), solution_message));
    }
  }

  // Call callbacks.
  // Note that we cannot call function that try to get the mutex_ here.
  TestGapLimitsIfNeeded();
  for (const auto& pair : callbacks_) {
    pair.second(tmp_postsolved_response);
  }

#if !defined(__PORTABLE_PLATFORM__)
  // We protect solution dumping with log_updates as LNS subsolvers share
  // another solution manager, and we do not want to dump those.
  if (logger_->LoggingIsEnabled() &&
      absl::GetFlag(FLAGS_cp_model_dump_solutions)) {
    const std::string file =
        absl::StrCat(dump_prefix_, "solution_", num_solutions_, ".pb.txt");
    LOG(INFO) << "Dumping solution to '" << file << "'.";

    // Note that here we only want to dump the non-post-solved solution.
    // This is only used for debugging.
    CpSolverResponse response;
    response.mutable_solution()->Assign(solution_values.begin(),
                                        solution_values.end());
    CHECK_OK(file::SetTextProto(file, response, file::Defaults()));
  }
#endif  // __PORTABLE_PLATFORM__
}

bool SharedResponseManager::ProblemIsSolved() const {
  absl::MutexLock mutex_lock(&mutex_);
  return synchronized_best_status_ == CpSolverStatus::OPTIMAL ||
         synchronized_best_status_ == CpSolverStatus::INFEASIBLE;
}

void SharedResponseManager::UpdateBestStatus(const CpSolverStatus& status) {
  best_status_ = status;
  if (always_synchronize_) {
    synchronized_best_status_ = status;
  }
}

std::string ExtractSubSolverName(const std::string& improvement_info) {
  if (improvement_info.empty()) return "";

  // We assume the subsolver name is always first.
  for (int i = 0; i < improvement_info.size(); ++i) {
    if (!std::isalnum(improvement_info[i]) && improvement_info[i] != '_') {
      return improvement_info.substr(0, i);
    }
  }

  return improvement_info;
}

void SharedResponseManager::RegisterSolutionFound(
    const std::string& improvement_info, int solution_rank) {
  if (improvement_info.empty()) return;
  const std::string subsolver_name = ExtractSubSolverName(improvement_info);
  primal_improvements_count_[subsolver_name]++;
  primal_improvements_min_rank_.insert({subsolver_name, solution_rank});
  primal_improvements_max_rank_[subsolver_name] = solution_rank;
}

void SharedResponseManager::RegisterObjectiveBoundImprovement(
    const std::string& improvement_info) {
  if (improvement_info.empty() || improvement_info == "initial domain") return;
  dual_improvements_count_[ExtractSubSolverName(improvement_info)]++;
}

void SharedResponseManager::DisplayImprovementStatistics() {
  absl::MutexLock mutex_lock(&mutex_);
  if (!primal_improvements_count_.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back(
        {absl::StrCat("Solutions (", num_solutions_, ")"), "Num", "Rank"});
    for (const auto& entry : primal_improvements_count_) {
      const int min_rank = primal_improvements_min_rank_[entry.first];
      const int max_rank = primal_improvements_max_rank_[entry.first];
      table.push_back({FormatName(entry.first), FormatCounter(entry.second),
                       absl::StrCat("[", min_rank, ",", max_rank, "]")});
    }
    SOLVER_LOG(logger_, FormatTable(table));
  }
  if (!dual_improvements_count_.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back({"Objective bounds", "Num"});
    for (const auto& entry : dual_improvements_count_) {
      table.push_back({FormatName(entry.first), FormatCounter(entry.second)});
    }
    SOLVER_LOG(logger_, FormatTable(table));
  }
}

SharedBoundsManager::SharedBoundsManager(const CpModelProto& model_proto)
    : num_variables_(model_proto.variables_size()),
      model_proto_(model_proto),
      lower_bounds_(num_variables_, std::numeric_limits<int64_t>::min()),
      upper_bounds_(num_variables_, std::numeric_limits<int64_t>::max()),
      synchronized_lower_bounds_(num_variables_,
                                 std::numeric_limits<int64_t>::min()),
      synchronized_upper_bounds_(num_variables_,
                                 std::numeric_limits<int64_t>::max()) {
  changed_variables_since_last_synchronize_.ClearAndResize(num_variables_);
  for (int i = 0; i < num_variables_; ++i) {
    lower_bounds_[i] = model_proto.variables(i).domain(0);
    const int domain_size = model_proto.variables(i).domain_size();
    upper_bounds_[i] = model_proto.variables(i).domain(domain_size - 1);
    synchronized_lower_bounds_[i] = lower_bounds_[i];
    synchronized_upper_bounds_[i] = upper_bounds_[i];
  }

  // Fill symmetry data.
  if (model_proto.has_symmetry()) {
    const int num_vars = model_proto.variables().size();
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    for (const SparsePermutationProto& perm :
         model_proto.symmetry().permutations()) {
      generators.emplace_back(CreateSparsePermutationFromProto(num_vars, perm));
    }
    if (generators.empty()) return;

    // Get orbits in term of IntegerVariable.
    var_to_orbit_index_ = GetOrbits(num_vars, generators);

    // Fill orbits_.
    std::vector<int> keys;
    std::vector<int> values;
    for (int var = 0; var < num_vars; ++var) {
      const int orbit_index = var_to_orbit_index_[var];
      if (orbit_index == -1) continue;
      keys.push_back(orbit_index);
      values.push_back(var);
    }
    if (keys.empty()) return;

    has_symmetry_ = true;
    orbits_.ResetFromFlatMapping(keys, values);

    // Fill representative.
    var_to_representative_.resize(num_vars);
    for (int var = 0; var < num_vars; ++var) {
      const int orbit_index = var_to_orbit_index_[var];
      if (orbit_index == -1) {
        var_to_representative_[var] = var;
      } else {
        var_to_representative_[var] = orbits_[orbit_index][0];
      }
    }
  }
}

void SharedBoundsManager::ReportPotentialNewBounds(
    const std::string& worker_name, const std::vector<int>& variables,
    const std::vector<int64_t>& new_lower_bounds,
    const std::vector<int64_t>& new_upper_bounds) {
  CHECK_EQ(variables.size(), new_lower_bounds.size());
  CHECK_EQ(variables.size(), new_upper_bounds.size());
  int num_improvements = 0;
  int num_symmetric_improvements = 0;

  absl::MutexLock mutex_lock(&mutex_);
  for (int i = 0; i < variables.size(); ++i) {
    int var = variables[i];
    if (var >= num_variables_) continue;

    // In the presence of symmetry we only update the representative.
    if (has_symmetry_) {
      var = var_to_representative_[var];
    }
    const int64_t old_lb = lower_bounds_[var];
    const int64_t old_ub = upper_bounds_[var];
    const int64_t new_lb = new_lower_bounds[i];
    const int64_t new_ub = new_upper_bounds[i];
    const bool changed_lb = new_lb > old_lb;
    const bool changed_ub = new_ub < old_ub;
    if (!changed_lb && !changed_ub) continue;

    VLOG(3) << worker_name << " var=" << var << " [" << old_lb << "," << old_ub
            << "] -> [" << new_lb << "," << new_ub << "]";

    if (changed_lb) {
      if (DEBUG_MODE && !debug_solution_.empty()) {
        CHECK_LE(new_lb, debug_solution_[var]) << worker_name << " var=" << var;
      }
      lower_bounds_[var] = new_lb;
    }
    if (changed_ub) {
      if (DEBUG_MODE && !debug_solution_.empty()) {
        CHECK_GE(new_ub, debug_solution_[var]) << worker_name << " var=" << var;
      }
      upper_bounds_[var] = new_ub;
    }
    changed_variables_since_last_synchronize_.Set(var);
    num_improvements++;

    if (has_symmetry_ && variables[i] != var) {
      // We count -1 so that num_improvements + num_symmetric_improvements
      // corresponds to the number of actual bound improvement.
      num_symmetric_improvements +=
          orbits_[var_to_orbit_index_[var]].size() - 1;
    }
  }
  if (num_improvements > 0) {
    total_num_improvements_ += num_improvements;
    VLOG(3) << total_num_improvements_ << "/" << num_variables_;
    bounds_exported_[worker_name].num_exported += num_improvements;
    bounds_exported_[worker_name].num_symmetric += num_symmetric_improvements;
    if (absl::GetFlag(FLAGS_cp_model_dump_tightened_models)) {
      CpModelProto tight_model = model_proto_;
      for (int i = 0; i < num_variables_; ++i) {
        IntegerVariableProto* var_proto = tight_model.mutable_variables(i);

        int rep = i;
        if (has_symmetry_) rep = var_to_representative_[i];
        const Domain domain = ReadDomainFromProto(*var_proto)
                                  .IntersectionWith(Domain(lower_bounds_[rep],
                                                           upper_bounds_[rep]));
        FillDomainInProto(domain, var_proto);
      }
      const std::string filename = absl::StrCat(dump_prefix_, "tighened_model_",
                                                export_counter_, ".pb.txt");
      LOG(INFO) << "Dumping tightened model proto to '" << filename << "'.";
      export_counter_++;
      CHECK(WriteModelProtoToFile(tight_model, filename));
    }
  }
}

// TODO(user): Because we look at the non-synchronized and up to date bounds,
// this break determinism if two solution for the same subpart comes at the same
// time.
void SharedBoundsManager::FixVariablesFromPartialSolution(
    absl::Span<const int64_t> solution,
    absl::Span<const int> variables_to_fix) {
  // This function shouldn't be called if we has symmetry.
  CHECK(!has_symmetry_);
  absl::MutexLock mutex_lock(&mutex_);

  // Abort if incompatible. Note that we only check the position that we are
  // about to fix. This should be enough. Otherwise we might never accept any
  // solution because the base LNS solution was not the same in some of the
  // variables that we fixed here.
  for (const int var : variables_to_fix) {
    const int64_t value = solution[var];
    if (value < lower_bounds_[var] || value > upper_bounds_[var]) {
      VLOG(1) << "Incompatibility in FixVariablesFromPartialSolution() "
              << "var: " << var << " value: " << value << " bounds: ["
              << lower_bounds_[var] << "," << upper_bounds_[var] << "]";
      return;
    }
  }

  // Fix the variables.
  for (const int var : variables_to_fix) {
    const int64_t old_lb = lower_bounds_[var];
    const int64_t old_ub = upper_bounds_[var];
    const bool changed_lb = solution[var] > old_lb;
    const bool changed_ub = solution[var] < old_ub;
    if (!changed_lb && !changed_ub) continue;

    lower_bounds_[var] = solution[var];
    upper_bounds_[var] = solution[var];
    changed_variables_since_last_synchronize_.Set(var);

    // This is problematic as we might find a different partial solution.
    // To allow for further investigation, we currently fix it to the debug
    // solution instead.
    if (DEBUG_MODE && !debug_solution_.empty()) {
      if (solution[var] != debug_solution_[var]) {
        LOG(INFO) << "Fixing to a different solution for var=" << var
                  << " debug=" << debug_solution_[var]
                  << " partial=" << solution[var];
        lower_bounds_[var] = debug_solution_[var];
        upper_bounds_[var] = debug_solution_[var];
      }
    }
  }
}

void SharedBoundsManager::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  for (const int var :
       changed_variables_since_last_synchronize_.PositionsSetAtLeastOnce()) {
    DCHECK(!has_symmetry_ || var_to_representative_[var] == var);
    synchronized_lower_bounds_[var] = lower_bounds_[var];
    synchronized_upper_bounds_[var] = upper_bounds_[var];
    for (int j = 0; j < id_to_changed_variables_.size(); ++j) {
      id_to_changed_variables_[j].Set(var);
    }
  }
  changed_variables_since_last_synchronize_.ClearAll();
}

int SharedBoundsManager::RegisterNewId() {
  absl::MutexLock mutex_lock(&mutex_);
  const int id = id_to_changed_variables_.size();
  id_to_changed_variables_.resize(id + 1);
  id_to_changed_variables_[id].ClearAndResize(num_variables_);
  for (int var = 0; var < num_variables_; ++var) {
    const int64_t lb = model_proto_.variables(var).domain(0);
    const int domain_size = model_proto_.variables(var).domain_size();
    const int64_t ub = model_proto_.variables(var).domain(domain_size - 1);
    if (lb != synchronized_lower_bounds_[var] ||
        ub != synchronized_upper_bounds_[var]) {
      DCHECK(!has_symmetry_ || var_to_representative_[var] == var);
      id_to_changed_variables_[id].Set(var);
    }
  }
  return id;
}

void SharedBoundsManager::GetChangedBounds(
    int id, std::vector<int>* variables, std::vector<int64_t>* new_lower_bounds,
    std::vector<int64_t>* new_upper_bounds) {
  variables->clear();
  new_lower_bounds->clear();
  new_upper_bounds->clear();

  {
    absl::MutexLock mutex_lock(&mutex_);
    for (const int var :
         id_to_changed_variables_[id].PositionsSetAtLeastOnce()) {
      DCHECK(!has_symmetry_ || var_to_representative_[var] == var);
      variables->push_back(var);
    }
    id_to_changed_variables_[id].ClearAll();

    // We need to report the bounds in a deterministic order as it is difficult
    // to guarantee that nothing depend on the order in which the new bounds are
    // processed.
    absl::c_sort(*variables);
    for (const int var : *variables) {
      new_lower_bounds->push_back(synchronized_lower_bounds_[var]);
      new_upper_bounds->push_back(synchronized_upper_bounds_[var]);
    }
  }

  // Now that the mutex is released, we can add all symmetric version if any.
  // Note that alternatively we could do that in the client side, but the
  // complexity will be the same, we will just save some memory that is usually
  // just reused.
  if (has_symmetry_) {
    const int old_size = variables->size();
    for (int i = 0; i < old_size; ++i) {
      const int var = (*variables)[i];
      const int orbit_index = var_to_orbit_index_[var];
      if (orbit_index == -1) continue;

      const int64_t lb = (*new_lower_bounds)[i];
      const int64_t ub = (*new_upper_bounds)[i];
      const auto orbit = orbits_[orbit_index];
      CHECK_EQ(var, orbit[0]);
      for (const int other : orbit.subspan(1)) {
        variables->push_back(other);
        new_lower_bounds->push_back(lb);
        new_upper_bounds->push_back(ub);
      }
    }
  }
}

void SharedBoundsManager::UpdateDomains(std::vector<Domain>* domains) {
  absl::MutexLock mutex_lock(&mutex_);
  CHECK_EQ(domains->size(), synchronized_lower_bounds_.size());
  for (int var = 0; var < domains->size(); ++var) {
    (*domains)[var] = (*domains)[var].IntersectionWith(Domain(
        synchronized_lower_bounds_[var], synchronized_upper_bounds_[var]));
  }
}

void SharedBoundsManager::LogStatistics(SolverLogger* logger) {
  absl::MutexLock mutex_lock(&mutex_);
  if (!bounds_exported_.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back({"Improving bounds shared", "Num", "Sym"});
    for (const auto& entry : bounds_exported_) {
      table.push_back({FormatName(entry.first),
                       FormatCounter(entry.second.num_exported),
                       FormatCounter(entry.second.num_symmetric)});
    }
    SOLVER_LOG(logger, FormatTable(table));
  }
}

int SharedBoundsManager::NumBoundsExported(absl::string_view worker_name) {
  absl::MutexLock mutex_lock(&mutex_);
  const auto it = bounds_exported_.find(worker_name);
  if (it == bounds_exported_.end()) return 0;
  return it->second.num_exported;
}

UniqueClauseStream::UniqueClauseStream() {
  for (auto& buffer : clauses_by_size_) {
    buffer.reserve(kMaxBufferedLiterals);
  }
}

bool UniqueClauseStream::Add(absl::Span<const int> clause) {
  absl::MutexLock mutex_lock(&mutex_);
  if (clause.size() > kMaxClauseSize || clause.size() <= 2) return false;
  // This is just a safety check, the caller should have called CanAccept().
  if (NumLiteralsOfSize(clause.size()) + clause.size() > kMaxBufferedLiterals) {
    return false;
  }
  if (BlockClause(clause)) {
    std::vector<int>* buffer = MutableBufferForSize(clause.size());
    buffer->insert(buffer->end(), clause.begin(), clause.end());
    return true;
  }
  return false;
}

bool UniqueClauseStream::BlockClause(absl::Span<const int> clause) {
  if (clause.size() > kMaxClauseSize) return false;
  if (clause.size() <= 2) return false;
  return fingerprints_.emplace(HashClause(clause)).second;
}

bool UniqueClauseStream::Delete(absl::Span<const int> clause) {
  const size_t fingerprint = HashClause(clause);
  absl::MutexLock mutex_lock(&mutex_);
  // Note a clause with this hash may be buffered, but not yet exported.
  return fingerprints_.erase(fingerprint) == 1;
}

CompactVectorVector<int> UniqueClauseStream::NextBatch() {
  CompactVectorVector<int> buffer;
  buffer.reserve(kMaxLiteralsPerBatch / kMinClauseSize, kMaxLiteralsPerBatch);
  int to_fill = kMaxLiteralsPerBatch;
  absl::MutexLock mutex_lock(&mutex_);
  for (int size = kMinClauseSize; size <= kMaxClauseSize; ++size) {
    CHECK_EQ(NumLiteralsOfSize(size) % size, 0);
    while (to_fill >= size && NumLiteralsOfSize(size) > 0) {
      absl::Span<const int> clause = NextClause(size);
      if (fingerprints_.contains(HashClause(clause))) {
        buffer.Add(NextClause(size));
        to_fill -= size;
      }
      PopClause(size);
    }
  }
  return buffer;
}

int UniqueClauseStream::FillUpstreamBuffer(UniqueClauseStream& upstream,
                                           int size,
                                           int max_clauses_to_export) {
  int num_exported_clauses = 0;
  absl::MutexLock mutex_lock(&mutex_);
  while (NumLiteralsOfSize(size) > 0 &&
         num_exported_clauses < max_clauses_to_export) {
    absl::Span<const int> clause = NextClause(size);
    // Don't emit deleted clauses.
    if (fingerprints_.contains(HashClause(clause)) && upstream.Add(clause)) {
      ++num_exported_clauses;
    }
    PopClause(size);
  }
  return num_exported_clauses;
}

int UniqueClauseStream::NumBufferedLiterals() const {
  absl::MutexLock mutex_lock(&mutex_);
  int result = 0;
  for (const auto& buffer : clauses_by_size_) {
    result += buffer.size();
  }
  return result;
}

bool UniqueClauseStream::CanAccept(int size, int lbd) const {
  if (size <= 2 || size > kMaxClauseSize) return false;
  absl::MutexLock mutex_lock(&mutex_);
  if (lbd > lbd_threshold_) return false;
  int num_literals_up_to_size = 0;
  for (int i = kMinClauseSize; i <= size; ++i) {
    num_literals_up_to_size += NumLiteralsOfSize(i);
  }
  return num_literals_up_to_size + size <= kMaxBufferedLiterals;
}

void UniqueClauseStream::RemoveWorstClauses() {
  absl::MutexLock mutex_lock(&mutex_);
  int literals_to_remove = 0;
  for (const auto& buffer : clauses_by_size_) {
    literals_to_remove += buffer.size();
  }
  literals_to_remove -= kMaxBufferedLiterals;
  for (int size = kMaxClauseSize; size >= kMinClauseSize; --size) {
    while (NumLiteralsOfSize(size) > 0) {
      // Stop if removing one more clause of the current size would
      // leave the buffer under full. Otherwise we might remove a shorter
      // clause later!
      if (literals_to_remove < size) return;
      fingerprints_.erase(HashClause(NextClause(size)));
      PopClause(size);
      literals_to_remove -= size;
    }
  }
}

void UniqueClauseStream::set_lbd_threshold(int lbd) {
  absl::MutexLock mutex_lock(&mutex_);
  lbd_threshold_ = lbd;
}

size_t UniqueClauseStream::HashClause(absl::Span<const int> clause,
                                      size_t hash_seed) {
  size_t hash = absl::HashOf(hash_seed, clause.size());
  for (int i = 0; i < clause.size(); ++i) {
    hash ^= absl::HashOf(clause[i], hash_seed);
  }
  return hash;
}

absl::Span<const int> UniqueClauseStream::NextClause(int size) const {
  absl::Span<const int> buffer = BufferForSize(size);
  return buffer.subspan(buffer.size() - size, size);
}

void UniqueClauseStream::PopClause(int size) {
  std::vector<int>* buffer = MutableBufferForSize(size);
  buffer->erase(buffer->end() - size, buffer->end());
}

int UniqueClauseStream::NumClausesOfSize(int size) const {
  return NumLiteralsOfSize(size) / size;
}

int UniqueClauseStream::NumLiteralsOfSize(int size) const {
  return BufferForSize(size).size();
}

SharedClausesManager::SharedClausesManager(bool always_synchronize,
                                           absl::Duration share_frequency)
    : always_synchronize_(always_synchronize),
      share_frequency_(share_frequency) {}

int SharedClausesManager::RegisterNewId() {
  absl::MutexLock mutex_lock(&mutex_);
  const int id = id_to_last_processed_binary_clause_.size();
  id_to_last_processed_binary_clause_.resize(id + 1, 0);
  id_to_last_returned_batch_.resize(id + 1, 0);
  id_to_last_finished_batch_.resize(id + 1, 0);
  id_to_clauses_exported_.resize(id + 1, 0);
  id_to_clause_stream_.emplace_back();
  return id;
}

void SharedClausesManager::SetWorkerNameForId(int id,
                                              absl::string_view worker_name) {
  absl::MutexLock mutex_lock(&mutex_);
  id_to_worker_name_[id] = worker_name;
}

void SharedClausesManager::AddBinaryClause(int id, int lit1, int lit2) {
  if (lit2 < lit1) std::swap(lit1, lit2);
  const auto p = std::make_pair(lit1, lit2);

  absl::MutexLock mutex_lock(&mutex_);
  const auto [unused_it, inserted] = added_binary_clauses_set_.insert(p);
  if (inserted) {
    added_binary_clauses_.push_back(p);
    if (always_synchronize_) ++last_visible_binary_clause_;
    id_to_clauses_exported_[id]++;

    // Small optim. If the worker is already up to date with clauses to import,
    // we can mark this new clause as already seen.
    if (id_to_last_processed_binary_clause_[id] ==
        added_binary_clauses_.size() - 1) {
      id_to_last_processed_binary_clause_[id]++;
    }
  }
}

std::vector<absl::Span<const int>> SharedClausesManager::GetUnseenClauses(
    int id) {
  std::vector<absl::Span<const int>> result;
  absl::MutexLock mutex_lock(&mutex_);
  for (int i = id_to_last_returned_batch_[id]; i < batches_.size(); ++i) {
    for (int j = 0; j < batches_[i].size(); ++j) {
      result.push_back(batches_[i][j]);
    }
  }
  id_to_last_finished_batch_[id] = id_to_last_returned_batch_[id];
  id_to_last_returned_batch_[id] = batches_.size();
  return result;
}

void SharedClausesManager::GetUnseenBinaryClauses(
    int id, std::vector<std::pair<int, int>>* new_clauses) {
  new_clauses->clear();
  absl::MutexLock mutex_lock(&mutex_);
  const int last_binary_clause_seen = id_to_last_processed_binary_clause_[id];
  if (last_binary_clause_seen >= last_visible_binary_clause_) return;

  new_clauses->assign(
      added_binary_clauses_.begin() + last_binary_clause_seen,
      added_binary_clauses_.begin() + last_visible_binary_clause_);
  id_to_last_processed_binary_clause_[id] = last_visible_binary_clause_;
}

void SharedClausesManager::LogStatistics(SolverLogger* logger) {
  absl::MutexLock mutex_lock(&mutex_);
  absl::btree_map<std::string, int64_t> name_to_clauses;
  for (int id = 0; id < id_to_clauses_exported_.size(); ++id) {
    if (id_to_clauses_exported_[id] == 0) continue;
    name_to_clauses[id_to_worker_name_[id]] = id_to_clauses_exported_[id];
  }
  if (!name_to_clauses.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back({"Clauses shared", "Num"});
    for (const auto& entry : name_to_clauses) {
      table.push_back({FormatName(entry.first), FormatCounter(entry.second)});
    }
    SOLVER_LOG(logger, FormatTable(table));
  }
}

void SharedClausesManager::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  last_visible_binary_clause_ = added_binary_clauses_.size();
  const int num_workers = id_to_clause_stream_.size();
  if (num_workers <= 1) return;
  if (!share_timer_.IsRunning()) share_timer_.Start();
  if (share_timer_.GetDuration() < share_frequency_) return;
  share_timer_.Restart();

  // Tune LBD threshold for individual workers based on how the worker's buffer
  // is. We aim to ensure workers can always export their fair share of clauses.
  for (int id = 0; id < num_workers; ++id) {
    UniqueClauseStream& stream = id_to_clause_stream_[id];
    const int lbd_threshold = stream.lbd_threshold();
    const int num_buffered_literals = stream.NumBufferedLiterals();
    const bool underfull =
        num_buffered_literals <
        UniqueClauseStream::kMaxLiteralsPerBatch / num_workers;
    const bool overfull =
        num_buffered_literals >
        2 * UniqueClauseStream::kMaxLiteralsPerBatch / num_workers;
    const int new_lbd = std::clamp(lbd_threshold + underfull - overfull, 2,
                                   UniqueClauseStream::kMaxClauseSize);
    if (new_lbd != lbd_threshold) {
      VLOG(2) << id_to_worker_name_[id]
              << " sharing clauses with lbd <= " << new_lbd;
      stream.set_lbd_threshold(new_lbd);
    }
  }

  std::vector<int> ids(num_workers);
  int literals_to_fill = UniqueClauseStream::kMaxLiteralsPerBatch;
  for (int size = UniqueClauseStream::kMinClauseSize;
       size <= UniqueClauseStream::kMaxClauseSize; ++size) {
    ids.clear();
    for (int id = 0; id < num_workers; ++id) {
      if (id_to_clause_stream_[id].NumBufferedLiteralsOfSize(size) > 0) {
        ids.push_back(id);
      }
    }
    // Use progressive filling to attempt to fill the batch with clauses of
    // minimum size, this is max-min fair.
    while (!ids.empty()) {
      const int clauses_to_fill = literals_to_fill / size;
      if (clauses_to_fill == 0) break;
      // Some workers need to export more clauses to fill the batch due to
      // rounding, but we don't want all workers to round up.
      const int num_to_round_up = clauses_to_fill % ids.size();
      for (int i = 0; i < ids.size(); ++i) {
        const bool round_up = i < num_to_round_up;
        const int id = ids[i];
        const int shared = id_to_clause_stream_[id].FillUpstreamBuffer(
            all_clauses_, size, clauses_to_fill / ids.size() + round_up);
        id_to_clauses_exported_[id] += shared;
        if (shared == 0 ||
            id_to_clause_stream_[id].NumBufferedLiteralsOfSize(size) == 0) {
          ids[i] = ids.back();
          ids.pop_back();
          --i;
        }
      }
    }
  }
  if (all_clauses_.NumBufferedLiterals() > 0) {
    batches_.push_back(all_clauses_.NextBatch());
    VLOG(2) << "Batch #" << batches_.size() << " w/ " << batches_.back().size()
            << " clauses max size = "
            << batches_.back()[batches_.back().size() - 1].size();
  }
  // Delete batches that have been consumed by all workers.
  // Keep a few batches around for startup (min finished batch doesn't count
  // workers that haven't registered yet).
  // This also ensures that our fingerprint table always contains the last few
  // batches, so we reduce the chance of an old buffered duplicate clause on
  // a worker being emitted from the global stream multiple times.
  if (batches_.size() < kMinBatches) return;
  const int min_finished_batch =
      std::min<int>(batches_.size() - kMinBatches,
                    *absl::c_min_element(id_to_last_finished_batch_));
  for (int i = 0; i < min_finished_batch; ++i) {
    VLOG(2) << "Erasing batch";
    for (int i = 0; i < batches_.front().size(); ++i) {
      all_clauses_.Delete(batches_.front()[i]);
    }
    batches_.pop_front();
  }
  for (int id = 0; id < id_to_last_finished_batch_.size(); ++id) {
    id_to_last_returned_batch_[id] -= min_finished_batch;
    id_to_last_finished_batch_[id] -= min_finished_batch;
  }
  // TODO(user): We could cleanup binary clauses that have been consumed.
}

void SharedStatistics::AddStats(
    absl::Span<const std::pair<std::string, int64_t>> stats) {
  absl::MutexLock mutex_lock(&mutex_);
  for (const auto& [key, count] : stats) {
    stats_[key] += count;
  }
}

void SharedStatistics::Log(SolverLogger* logger) {
  absl::MutexLock mutex_lock(&mutex_);
  if (stats_.empty()) return;

  SOLVER_LOG(logger, "Stats across workers (summed):");
  std::vector<std::pair<std::string, int64_t>> to_sort_;
  for (const auto& [key, count] : stats_) {
    to_sort_.push_back({key, count});
  }
  std::sort(to_sort_.begin(), to_sort_.end());
  for (const auto& [key, count] : to_sort_) {
    SOLVER_LOG(logger, "  ", key, ": ", FormatCounter(count));
  }
  SOLVER_LOG(logger, "");
}

}  // namespace sat
}  // namespace operations_research
