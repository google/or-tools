// Copyright 2010-2022 Google LLC
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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/helpers.h"
#endif  // __PORTABLE_PLATFORM__
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

ABSL_FLAG(bool, cp_model_dump_solutions, false,
          "DEBUG ONLY. If true, all the intermediate solution will be dumped "
          "under '\"FLAGS_cp_model_dump_prefix\" + \"solution_xxx.pb.txt\"'.");

ABSL_FLAG(
    std::string, cp_model_load_debug_solution, "",
    "DEBUG ONLY. When this is set to a non-empty file name, "
    "we will interpret this as an internal solution which can be used for "
    "debugging. For instance we use it to identify wrong cuts/reasons.");

namespace operations_research {
namespace sat {

void SharedRelaxationSolutionRepository::NewRelaxationSolution(
    const CpSolverResponse& response) {
  // Note that the Add() method already applies mutex lock. So we don't need it
  // here.
  if (response.solution().empty()) return;

  // Add this solution to the pool.
  SharedSolutionRepository<int64_t>::Solution solution;
  solution.variable_values.assign(response.solution().begin(),
                                  response.solution().end());
  // For now we use the negated lower bound as the "internal objective" to
  // prefer solution with an higher bound.
  //
  // Note: If the model doesn't have objective, the best_objective_bound is set
  // to default value 0.
  solution.rank = -response.best_objective_bound();

  Add(solution);
}

void SharedLPSolutionRepository::NewLPSolution(
    std::vector<double> lp_solution) {
  if (lp_solution.empty()) return;

  // Add this solution to the pool.
  SharedSolutionRepository<double>::Solution solution;
  solution.variable_values = std::move(lp_solution);

  // We always prefer to keep the solution from the last synchronize batch.
  absl::MutexLock mutex_lock(&mutex_);
  solution.rank = -num_synchronization_;
  AddInternal(solution);
}

bool SharedIncompleteSolutionManager::HasNewSolution() const {
  absl::MutexLock mutex_lock(&mutex_);
  return !solutions_.empty();
}

std::vector<double> SharedIncompleteSolutionManager::GetNewSolution() {
  absl::MutexLock mutex_lock(&mutex_);
  std::vector<double> solution;
  if (solutions_.empty()) return solution;

  solution = std::move(solutions_.back());
  solutions_.pop_back();
  return solution;
}

void SharedIncompleteSolutionManager::AddNewSolution(
    const std::vector<double>& lp_solution) {
  absl::MutexLock mutex_lock(&mutex_);
  solutions_.push_back(lp_solution);
}

SharedResponseManager::SharedResponseManager(Model* model)
    : parameters_(*model->GetOrCreate<SatParameters>()),
      wall_timer_(*model->GetOrCreate<WallTimer>()),
      shared_time_limit_(model->GetOrCreate<ModelSharedTimeLimit>()),
      solutions_(parameters_.solution_pool_size()),
      logger_(model->GetOrCreate<SolverLogger>()) {}

namespace {

std::string ProgressMessage(const std::string& event_or_solution_count,
                            double time_in_seconds, double obj_best,
                            double obj_lb, double obj_ub,
                            const std::string& solution_info) {
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

void SharedResponseManager::LogMessage(const std::string& prefix,
                                       const std::string& message) {
  absl::MutexLock mutex_lock(&mutex_);
  SOLVER_LOG(logger_, absl::StrFormat("#%-5s %6.2fs %s", prefix,
                                      wall_timer_.Get(), message));
}

void SharedResponseManager::LogPeriodicMessage(const std::string& prefix,
                                               const std::string& message,
                                               double frequency_seconds,
                                               absl::Time* last_logging_time) {
  if (frequency_seconds < 0.0 || last_logging_time == nullptr) return;
  const absl::Time now = absl::Now();
  if (now - *last_logging_time < absl::Seconds(frequency_seconds)) {
    return;
  }

  absl::MutexLock mutex_lock(&mutex_);
  *last_logging_time = now;
  SOLVER_LOG(logger_, absl::StrFormat("#%-5s %6.2fs %s", prefix,
                                      wall_timer_.Get(), message));
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
    best_response_.set_status(CpSolverStatus::OPTIMAL);

    // Note(user): Some code path in single-thread assumes that the problem
    // can only be solved when they have proven infeasibility and do not check
    // the ProblemIsSolved() method. So we force a stop here.
    shared_time_limit_->Stop();
  }
  if (gap / std::max(1.0, std::abs(user_best)) < relative_gap_limit_) {
    SOLVER_LOG(logger_, "Relative gap limit of ", relative_gap_limit_,
               " reached.");
    best_response_.set_status(CpSolverStatus::OPTIMAL);

    // Same as above.
    shared_time_limit_->Stop();
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

  const bool change =
      (lb > inner_objective_lower_bound_ || ub < inner_objective_upper_bound_);
  if (lb > inner_objective_lower_bound_) {
    // When the improving problem is infeasible, it is possible to report
    // arbitrary high inner_objective_lower_bound_. We make sure it never cross
    // the current best solution, so that we always report globablly valid lower
    // bound.
    DCHECK_LE(inner_objective_upper_bound_, best_solution_objective_value_);
    inner_objective_lower_bound_ =
        std::min(best_solution_objective_value_, lb.value());
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
    if (update_integral_on_each_change_) UpdateGapIntegralInternal();
    SOLVER_LOG(logger_,
               SatProgressMessage("Done", wall_timer_.Get(), update_info));
    return;
  }
  if (logger_->LoggingIsEnabled() && change) {
    const CpObjectiveProto& obj = *objective_or_null_;
    const double best =
        ScaleObjectiveValue(obj, best_solution_objective_value_);
    double new_lb = ScaleObjectiveValue(obj, inner_objective_lower_bound_);
    double new_ub = ScaleObjectiveValue(obj, inner_objective_upper_bound_);
    if (obj.scaling_factor() < 0) {
      std::swap(new_lb, new_ub);
    }
    RegisterObjectiveBoundImprovement(update_info);
    SOLVER_LOG(logger_, ProgressMessage("Bound", wall_timer_.Get(), best,
                                        new_lb, new_ub, update_info));
  }
  if (change) TestGapLimitsIfNeeded();
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

    // We just proved that the best solution cannot be improved uppon, so we
    // have a new lower bound.
    inner_objective_lower_bound_ = best_solution_objective_value_;
    if (update_integral_on_each_change_) UpdateGapIntegralInternal();
  } else {
    CHECK_EQ(num_solutions_, 0);
    best_response_.set_status(CpSolverStatus::INFEASIBLE);
  }
  SOLVER_LOG(logger_,
             SatProgressMessage("Done", wall_timer_.Get(), worker_info));
}

void SharedResponseManager::AddUnsatCore(const std::vector<int>& core) {
  absl::MutexLock mutex_lock(&mutex_);
  best_response_.clear_sufficient_assumptions_for_infeasibility();
  for (const int ref : core) {
    best_response_.add_sufficient_assumptions_for_infeasibility(ref);
  }
}

IntegerValue SharedResponseManager::GetInnerObjectiveLowerBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return IntegerValue(inner_objective_lower_bound_);
}

IntegerValue SharedResponseManager::GetInnerObjectiveUpperBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return IntegerValue(inner_objective_upper_bound_);
}

void SharedResponseManager::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  synchronized_inner_objective_lower_bound_ =
      IntegerValue(inner_objective_lower_bound_);
  synchronized_inner_objective_upper_bound_ =
      IntegerValue(inner_objective_upper_bound_);
}

IntegerValue SharedResponseManager::SynchronizedInnerObjectiveLowerBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return synchronized_inner_objective_lower_bound_;
}

IntegerValue SharedResponseManager::SynchronizedInnerObjectiveUpperBound() {
  absl::MutexLock mutex_lock(&mutex_);
  return synchronized_inner_objective_upper_bound_;
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

CpSolverResponse SharedResponseManager::GetResponseInternal() {
  FillObjectiveValuesInBestResponse();

  // We need to copy the response before we postsolve it.
  CpSolverResponse result = best_response_;
  if (result.status() == CpSolverStatus::FEASIBLE ||
      result.status() == CpSolverStatus::OPTIMAL) {
    std::vector<int64_t> solution(result.solution().begin(),
                                  result.solution().end());
    for (int i = solution_postprocessors_.size(); --i >= 0;) {
      solution_postprocessors_[i](&solution);
    }
    result.mutable_solution()->Assign(solution.begin(), solution.end());
  }
  for (int i = postprocessors_.size(); --i >= 0;) {
    postprocessors_[i](&result);
  }
  return result;
}

CpSolverResponse SharedResponseManager::GetResponse(bool full_response) {
  absl::MutexLock mutex_lock(&mutex_);
  CpSolverResponse result = GetResponseInternal();
  if (full_response) {
    // If this is true, we postsolve and copy all of our solutions.
    if (parameters_.fill_additional_solutions_in_response()) {
      std::vector<int64_t> temp;
      for (int i = 0; i < solutions_.NumSolutions(); ++i) {
        temp = solutions_.GetSolution(i).variable_values;
        for (int i = solution_postprocessors_.size(); --i >= 0;) {
          solution_postprocessors_[i](&temp);
        }
        result.add_additional_solutions()->mutable_values()->Assign(
            temp.begin(), temp.end());
      }
    }
    for (int i = final_postprocessors_.size(); --i >= 0;) {
      final_postprocessors_[i](&result);
    }
  }
  return result;
}

void SharedResponseManager::FillObjectiveValuesInBestResponse() {
  if (objective_or_null_ == nullptr) return;
  const CpObjectiveProto& obj = *objective_or_null_;

  if (best_response_.status() == CpSolverStatus::INFEASIBLE) {
    best_response_.clear_objective_value();
    best_response_.clear_best_objective_bound();
    best_response_.clear_inner_objective_lower_bound();
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
  best_response_.set_inner_objective_lower_bound(
      ScaleInnerObjectiveValue(obj, inner_objective_lower_bound_));
  best_response_.set_best_objective_bound(
      ScaleObjectiveValue(obj, inner_objective_lower_bound_));

  // Update the primal integral.
  best_response_.set_gap_integral(gap_integral_);
}

void SharedResponseManager::NewSolution(const CpSolverResponse& response,
                                        Model* model) {
  absl::MutexLock mutex_lock(&mutex_);

  // Special case if the user asked to keep solutions in the pool.
  if (objective_or_null_ == nullptr && parameters_.enumerate_all_solutions() &&
      parameters_.fill_additional_solutions_in_response()) {
    SharedSolutionRepository<int64_t>::Solution solution;
    solution.variable_values.assign(response.solution().begin(),
                                    response.solution().end());
    solutions_.Add(solution);
  }

  if (objective_or_null_ != nullptr) {
    const int64_t objective_value =
        ComputeInnerObjective(*objective_or_null_, response);

    // Add this solution to the pool, even if it is not improving.
    if (!response.solution().empty()) {
      SharedSolutionRepository<int64_t>::Solution solution;
      solution.variable_values.assign(response.solution().begin(),
                                      response.solution().end());
      solution.rank = objective_value;
      solutions_.Add(solution);
    }

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

  // TODO(user): Hack. In single thread, no one is synchronizing the solution,
  // so we should do it from here. We currently "reuse"
  // update_integral_on_each_change_ which should probably just change name.
  if (update_integral_on_each_change_) {
    solutions_.Synchronize();
  }

  // Note that the objective will be filled by
  // FillObjectiveValuesInBestResponse().
  if (objective_or_null_ == nullptr && !parameters_.enumerate_all_solutions()) {
    best_response_.set_status(CpSolverStatus::OPTIMAL);
  } else {
    best_response_.set_status(CpSolverStatus::FEASIBLE);
  }

  best_response_.set_solution_info(response.solution_info());
  *best_response_.mutable_solution() = response.solution();

  // Mark model as OPTIMAL if the inner bound crossed.
  if (objective_or_null_ != nullptr &&
      inner_objective_lower_bound_ > inner_objective_upper_bound_) {
    best_response_.set_status(CpSolverStatus::OPTIMAL);
  }

  // Logging.
  ++num_solutions_;
  if (logger_->LoggingIsEnabled()) {
    std::string solution_info = response.solution_info();
    if (model != nullptr) {
      const int64_t num_bool = model->Get<Trail>()->NumVariables();
      const int64_t num_fixed = model->Get<SatSolver>()->NumFixedVariables();
      absl::StrAppend(&solution_info, " fixed_bools:", num_fixed, "/",
                      num_bool);
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
      RegisterSolutionFound(solution_info);
      SOLVER_LOG(logger_, ProgressMessage(absl::StrCat(num_solutions_),
                                          wall_timer_.Get(), best, lb, ub,
                                          solution_info));
    } else {
      SOLVER_LOG(logger_, SatProgressMessage(absl::StrCat(num_solutions_),
                                             wall_timer_.Get(), solution_info));
    }
  }

  // Call callbacks.
  // Note that we cannot call function that try to get the mutex_ here.
  TestGapLimitsIfNeeded();
  if (!callbacks_.empty()) {
    SetStatsFromModelInternal(model);
    const CpSolverResponse copy = GetResponseInternal();
    for (const auto& pair : callbacks_) {
      pair.second(copy);
    }
  }

#if !defined(__PORTABLE_PLATFORM__)
  // We protect solution dumping with log_updates as LNS subsolvers share
  // another solution manager, and we do not want to dump those.
  if (absl::GetFlag(FLAGS_cp_model_dump_solutions)) {
    const std::string file =
        absl::StrCat(dump_prefix_, "solution_", num_solutions_, ".pb.txt");
    LOG(INFO) << "Dumping solution to '" << file << "'.";
    CHECK_OK(file::SetTextProto(file, best_response_, file::Defaults()));
  }
#endif  // __PORTABLE_PLATFORM__
}

void SharedResponseManager::LoadDebugSolution(Model* model) {
#if !defined(__PORTABLE_PLATFORM__)
  if (absl::GetFlag(FLAGS_cp_model_load_debug_solution).empty()) return;
  if (model->Get<DebugSolution>() != nullptr) return;  // Already loaded.

  CpSolverResponse response;
  LOG(INFO) << "Reading solution from '"
            << absl::GetFlag(FLAGS_cp_model_load_debug_solution) << "'.";
  CHECK_OK(file::GetTextProto(absl::GetFlag(FLAGS_cp_model_load_debug_solution),
                              &response, file::Defaults()));

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
  const int64_t objective_value =
      ComputeInnerObjective(*objective_or_null_, response);
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
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* integer_trail = model->Get<IntegerTrail>();
  best_response_.set_num_booleans(sat_solver->NumVariables());
  best_response_.set_num_branches(sat_solver->num_branches());
  best_response_.set_num_conflicts(sat_solver->num_failures());
  best_response_.set_num_binary_propagations(sat_solver->num_propagations());
  best_response_.set_num_restarts(sat_solver->num_restarts());
  best_response_.set_num_integer_propagations(
      integer_trail == nullptr ? 0 : integer_trail->num_enqueues());
  auto* time_limit = model->Get<TimeLimit>();
  best_response_.set_wall_time(time_limit->GetElapsedTime());
  best_response_.set_deterministic_time(
      time_limit->GetElapsedDeterministicTime());

  int64_t num_lp_iters = 0;
  for (const LinearProgrammingConstraint* lp :
       *model->GetOrCreate<LinearProgrammingConstraintCollection>()) {
    num_lp_iters += lp->total_num_simplex_iterations();
  }
  best_response_.set_num_lp_iterations(num_lp_iters);
}

bool SharedResponseManager::ProblemIsSolved() const {
  absl::MutexLock mutex_lock(&mutex_);
  return best_response_.status() == CpSolverStatus::OPTIMAL ||
         best_response_.status() == CpSolverStatus::INFEASIBLE;
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
    const std::string& improvement_info) {
  if (improvement_info.empty()) return;
  primal_improvements_count_[ExtractSubSolverName(improvement_info)]++;
}

void SharedResponseManager::RegisterObjectiveBoundImprovement(
    const std::string& improvement_info) {
  if (improvement_info.empty() || improvement_info == "initial domain") return;
  dual_improvements_count_[ExtractSubSolverName(improvement_info)]++;
}

void SharedResponseManager::DisplayImprovementStatistics() {
  absl::MutexLock mutex_lock(&mutex_);
  if (!primal_improvements_count_.empty()) {
    SOLVER_LOG(logger_, "");
    SOLVER_LOG(logger_, "Solutions found per subsolver:");
    for (const auto& entry : primal_improvements_count_) {
      SOLVER_LOG(logger_, "  '", entry.first, "': ", entry.second);
    }
  }
  if (!dual_improvements_count_.empty()) {
    SOLVER_LOG(logger_, "");
    SOLVER_LOG(logger_, "Objective bounds found per subsolver:");
    for (const auto& entry : dual_improvements_count_) {
      SOLVER_LOG(logger_, "  '", entry.first, "': ", entry.second);
    }
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
}

void SharedBoundsManager::ReportPotentialNewBounds(
    const CpModelProto& model_proto, const std::string& worker_name,
    const std::vector<int>& variables,
    const std::vector<int64_t>& new_lower_bounds,
    const std::vector<int64_t>& new_upper_bounds) {
  CHECK_EQ(variables.size(), new_lower_bounds.size());
  CHECK_EQ(variables.size(), new_upper_bounds.size());
  int num_improvements = 0;

  absl::MutexLock mutex_lock(&mutex_);
  for (int i = 0; i < variables.size(); ++i) {
    const int var = variables[i];
    if (var >= num_variables_) continue;
    const int64_t old_lb = lower_bounds_[var];
    const int64_t old_ub = upper_bounds_[var];
    const int64_t new_lb = new_lower_bounds[i];
    const int64_t new_ub = new_upper_bounds[i];
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
    changed_variables_since_last_synchronize_.Set(var);
    num_improvements++;
  }
  if (num_improvements > 0) {
    bounds_exported_[worker_name] += num_improvements;
  }
}

// TODO(user): Because we look at the non-synchronized and up to date bounds,
// this break determinism if two solution for the same subpart comes at the same
// time.
void SharedBoundsManager::FixVariablesFromPartialSolution(
    const std::vector<int64_t>& solution,
    const std::vector<int>& variables_to_fix) {
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
  }
}

void SharedBoundsManager::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  for (const int var :
       changed_variables_since_last_synchronize_.PositionsSetAtLeastOnce()) {
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

  absl::MutexLock mutex_lock(&mutex_);
  for (const int var : id_to_changed_variables_[id].PositionsSetAtLeastOnce()) {
    variables->push_back(var);
    new_lower_bounds->push_back(synchronized_lower_bounds_[var]);
    new_upper_bounds->push_back(synchronized_upper_bounds_[var]);
  }
  id_to_changed_variables_[id].ClearAll();
}

void SharedBoundsManager::LogStatistics(SolverLogger* logger) {
  absl::MutexLock mutex_lock(&mutex_);
  if (!bounds_exported_.empty()) {
    SOLVER_LOG(logger, "");
    SOLVER_LOG(logger, "Improving variable bounds shared per subsolver:");
    for (const auto& entry : bounds_exported_) {
      SOLVER_LOG(logger, "  '", entry.first, "': ", entry.second);
    }
  }
}

int SharedBoundsManager::NumBoundsExported(const std::string& worker_name) {
  absl::MutexLock mutex_lock(&mutex_);
  const auto it = bounds_exported_.find(worker_name);
  if (it == bounds_exported_.end()) return 0;
  return it->second;
}

int SharedClausesManager::RegisterNewId() {
  absl::MutexLock mutex_lock(&mutex_);
  const int id = id_to_last_processed_binary_clause_.size();
  id_to_last_processed_binary_clause_.resize(id + 1, 0);
  id_to_clauses_exported_.resize(id + 1, 0);
  return id;
}

void SharedClausesManager::SetWorkerNameForId(int id,
                                              const std::string& worker_name) {
  absl::MutexLock mutex_lock(&mutex_);
  id_to_worker_name_[id] = worker_name;
}

void SharedClausesManager::AddBinaryClause(int id, int lit1, int lit2) {
  absl::MutexLock mutex_lock(&mutex_);
  if (lit2 < lit1) std::swap(lit1, lit2);

  const auto p = std::make_pair(lit1, lit2);
  const auto [unused_it, inserted] = added_binary_clauses_set_.insert(p);
  if (inserted) {
    added_binary_clauses_.push_back(p);
    id_to_clauses_exported_[id]++;
    // Small optim. If the worker is already up to date with clauses to import,
    // we can mark this new clause as already seen.
    if (id_to_last_processed_binary_clause_[id] ==
        added_binary_clauses_.size() - 1) {
      id_to_last_processed_binary_clause_[id]++;
    }
  }
}

void SharedClausesManager::GetUnseenBinaryClauses(
    int id, std::vector<std::pair<int, int>>* new_clauses) {
  new_clauses->clear();
  absl::MutexLock mutex_lock(&mutex_);
  const int last_binary_clause_seen = id_to_last_processed_binary_clause_[id];
  new_clauses->assign(added_binary_clauses_.begin() + last_binary_clause_seen,
                      added_binary_clauses_.end());
  id_to_last_processed_binary_clause_[id] = added_binary_clauses_.size();
}

void SharedClausesManager::LogStatistics(SolverLogger* logger) {
  absl::MutexLock mutex_lock(&mutex_);
  absl::btree_map<std::string, int64_t> name_to_clauses;
  for (int id = 0; id < id_to_clauses_exported_.size(); ++id) {
    if (id_to_clauses_exported_[id] == 0) continue;
    name_to_clauses[id_to_worker_name_[id]] = id_to_clauses_exported_[id];
  }
  if (!name_to_clauses.empty()) {
    SOLVER_LOG(logger, "");
    SOLVER_LOG(logger, "Clauses shared per subsolver:");
    for (const auto& entry : name_to_clauses) {
      SOLVER_LOG(logger, "  '", entry.first, "': ", entry.second);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
