// Copyright 2010-2025 Google LLC
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

#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#endif  // __PORTABLE_PLATFORM__
#include "absl/algorithm/container.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/int128.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
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

std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
SharedSolutionPool::Add(SharedSolutionRepository<int64_t>::Solution solution) {
  // Only add to the alternative path if it has the correct source id.
  if (alternative_path_.num_solutions_to_keep() > 0 &&
      solution.source_id == alternative_path_.source_id()) {
    alternative_path_.Add(solution);
    if (solution.rank < best_solutions_.GetBestRank()) {
      VLOG(2) << "ALTERNATIVE WIN !";
    }
  }

  // For now we only return a solution if it was stored in best_solutions_.
  return best_solutions_.Add(std::move(solution));
}

void SharedSolutionPool::Synchronize(absl::BitGenRef random) {
  // Update the "seeds" for the aternative path.
  if (alternative_path_.num_solutions_to_keep() > 0) {
    absl::MutexLock mutex_lock(mutex_);

    auto process_solution =
        [this](const SharedSolutionRepository<int64_t>::Solution& solution)
            ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
              if (solution.variable_values.empty()) return;
              if (solution.rank < min_rank_ || solution.rank > max_rank_) {
                // Recompute buckets.
                min_rank_ = std::min(min_rank_, solution.rank);
                max_rank_ = std::max(max_rank_, solution.rank);

                // We want to store around 100 MB max.
                int num_solutions = std::max<int>(
                    10, 100'000'000 / solution.variable_values.size());
                const int64_t range = max_rank_ - min_rank_ + 1;
                if (num_solutions > range) {
                  num_solutions = range;
                }

                // But if the number of variables is low, we do not want
                // to use a lot of space/time just iterating over num_solutions.
                //
                // TODO(user): Rework the algo to be in
                // O(num_different_solutions) rather than initializing the
                // maximum amount right away.
                num_solutions = std::min(num_solutions, 1'000);

                // Resize and recompute rank_.
                //
                // seeds_[i] should contains solution in [ranks_[i],
                // rank_[i+1]). rank_[0] is always min_rank_. As long as we have
                // room, we should have exactly one bucket per rank.
                ranks_.resize(num_solutions);
                seeds_.resize(num_solutions);

                int64_t offset = (max_rank_ - min_rank_ + 1) / num_solutions;
                CHECK_GT(offset, 0);
                for (int i = 0; i < num_solutions; ++i) {
                  ranks_[i] = min_rank_ +
                              static_cast<int64_t>(absl::int128(i) *
                                                   absl::int128(range) /
                                                   absl::int128(num_solutions));
                }

                // Move existing solutions to their new bucket.
                int to_index = seeds_.size() - 1;
                for (int i = seeds_.size(); --i >= 0;) {
                  if (seeds_[i] == nullptr) continue;
                  while (to_index >= 0 && ranks_[to_index] > seeds_[i]->rank) {
                    --to_index;
                  }
                  seeds_[to_index] = std::move(seeds_[i]);
                }
              }

              // rank[limit] is the first > solution.rank.
              const int limit = std::upper_bound(ranks_.begin(), ranks_.end(),
                                                 solution.rank) -
                                ranks_.begin();
              CHECK_GT(limit, 0);
              seeds_[limit - 1] =
                  std::make_shared<SharedSolutionRepository<int64_t>::Solution>(
                      solution);
            };

    // All solution go through best_solutions_.Add(), so we only need
    // to process these here.
    best_solutions_.Synchronize(process_solution);
  } else {
    best_solutions_.Synchronize();
  }
  alternative_path_.Synchronize();

  // If we try to improve the alternate path without success, reset it
  // from a random path_seeds_.
  //
  // TODO(user): find a way to generate random solution and update the seeds
  // with them. Shall we do that in a continuous way or only when needed?
  if (alternative_path_.num_solutions_to_keep() > 0) {
    // Restart the alternative path ?
    const int threshold = std::max(
        100, static_cast<int>(std::sqrt(best_solutions_.num_queried())));
    if (alternative_path_.NumRecentlyNonImproving() > threshold) {
      VLOG(2) << "Done. num_non_improving: "
              << alternative_path_.NumRecentlyNonImproving()
              << " achieved: " << alternative_path_.GetBestRank() << " / "
              << best_solutions_.GetBestRank();
      alternative_path_.ClearSolutionsAndIncreaseSourceId();
    }

    // If we restarted, or we are at the beginning, pick a seed for the path.
    if (alternative_path_.NumSolutions() == 0) {
      absl::MutexLock mutex_lock(mutex_);

      // Pick random bucket with bias. If the bucket is empty, we will scan
      // "worse" bucket until we find a solution. We never pick bucket 0.
      if (seeds_.size() > 1) {
        // Note that LogUniform() is always inclusive.
        // TODO(user): Shall we bias even more?
        int index = 1 + absl::LogUniform<int>(random, 0, seeds_.size() - 2);
        for (; index < seeds_.size(); ++index) {
          if (seeds_[index] != nullptr) {
            alternative_path_.Add(*seeds_[index]);
            alternative_path_.Synchronize();
            VLOG(2) << "RESTART bucket=" << index << "/" << seeds_.size()
                    << " rank=" << alternative_path_.GetSolution(0)->rank
                    << " from_optimal="
                    << alternative_path_.GetSolution(0)->rank - min_rank_;
            break;
          }
        }

        // The last bucket should never be empty.
        CHECK(seeds_.back() != nullptr);
        CHECK_LT(index, seeds_.size());
      }
    }
  }
}

void SharedLPSolutionRepository::NewLPSolution(
    std::vector<double> lp_solution) {
  if (lp_solution.empty()) return;

  // Add this solution to the pool.
  auto solution =
      std::make_shared<SharedSolutionRepository<double>::Solution>();
  solution->variable_values = std::move(lp_solution);

  // We always prefer to keep the solution from the last synchronize batch.
  {
    absl::MutexLock mutex_lock(mutex_);
    solution->rank = -num_synchronization_;
    ++num_added_;
    new_solutions_.push_back(solution);
  }
}

void SharedIncompleteSolutionManager::AddSolution(
    const std::vector<double>& lp_solution) {
  absl::MutexLock mutex_lock(mutex_);
  ++num_added_;
  solutions_.push_back(lp_solution);
  if (solutions_.size() > 100) solutions_.pop_front();
}

bool SharedIncompleteSolutionManager::HasSolution() const {
  absl::MutexLock mutex_lock(mutex_);
  return !solutions_.empty();
}

std::vector<double> SharedIncompleteSolutionManager::PopLast() {
  absl::MutexLock mutex_lock(mutex_);
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
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      solution_pool_(parameters_),
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

std::string SatProgressMessage(absl::string_view event_or_solution_count,
                               double time_in_seconds,
                               absl::string_view solution_info) {
  return absl::StrFormat("#%-5s %6.2fs %s", event_or_solution_count,
                         time_in_seconds, solution_info);
}

}  // namespace

void SharedResponseManager::FillSolveStatsInResponse(
    Model* model, CpSolverResponse* response) {
  if (model == nullptr) return;
  absl::MutexLock mutex_lock(mutex_);
  for (const auto& set_stats : statistics_postprocessors_) {
    set_stats(model, response);
  }
}

void SharedResponseManager::LogMessage(absl::string_view prefix,
                                       absl::string_view message) {
  absl::MutexLock mutex_lock(mutex_);
  SOLVER_LOG(logger_, absl::StrFormat("#%-5s %6.2fs %s", prefix,
                                      wall_timer_.Get(), message));
}

void SharedResponseManager::LogMessageWithThrottling(
    absl::string_view prefix, absl::string_view message) {
  absl::MutexLock mutex_lock(mutex_);

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
  absl::MutexLock mutex_lock(mutex_);

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
  absl::MutexLock mutex_lock(mutex_);
  always_synchronize_ = always_synchronize;
}

void SharedResponseManager::SetUpdateGapIntegralOnEachChange(bool set) {
  absl::MutexLock mutex_lock(mutex_);
  update_integral_on_each_change_ = set;
}

void SharedResponseManager::UpdateGapIntegral() {
  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);
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
    absl::string_view worker_info) {
  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);
  unsat_cores_ = core;
}

IntegerValue SharedResponseManager::GetInnerObjectiveLowerBound() {
  absl::MutexLock mutex_lock(mutex_);
  return synchronized_inner_objective_lower_bound_;
}

IntegerValue SharedResponseManager::GetInnerObjectiveUpperBound() {
  absl::MutexLock mutex_lock(mutex_);
  return synchronized_inner_objective_upper_bound_;
}

void SharedResponseManager::Synchronize() {
  solution_pool_.Synchronize(*random_);

  absl::MutexLock mutex_lock(mutex_);
  synchronized_inner_objective_lower_bound_ =
      IntegerValue(inner_objective_lower_bound_);
  synchronized_inner_objective_upper_bound_ =
      IntegerValue(inner_objective_upper_bound_);
  synchronized_best_status_ = best_status_;
  if (solution_pool_.BestSolutions().NumSolutions() > 0) {
    first_solution_solvers_should_stop_ = true;
  }
  logger_->FlushPendingThrottledLogs();
}

IntegerValue SharedResponseManager::BestSolutionInnerObjectiveValue() {
  absl::MutexLock mutex_lock(mutex_);
  return IntegerValue(best_solution_objective_value_);
}

double SharedResponseManager::GapIntegral() const {
  absl::MutexLock mutex_lock(mutex_);
  return gap_integral_;
}

void SharedResponseManager::AddSolutionPostprocessor(
    std::function<void(std::vector<int64_t>*)> postprocessor) {
  absl::MutexLock mutex_lock(mutex_);
  solution_postprocessors_.push_back(postprocessor);
}

void SharedResponseManager::AddResponsePostprocessor(
    std::function<void(CpSolverResponse*)> postprocessor) {
  absl::MutexLock mutex_lock(mutex_);
  postprocessors_.push_back(postprocessor);
}

void SharedResponseManager::AddFinalResponsePostprocessor(
    std::function<void(CpSolverResponse*)> postprocessor) {
  absl::MutexLock mutex_lock(mutex_);
  final_postprocessors_.push_back(postprocessor);
}

void SharedResponseManager::AddStatisticsPostprocessor(
    std::function<void(Model*, CpSolverResponse*)> postprocessor) {
  absl::MutexLock mutex_lock(mutex_);
  statistics_postprocessors_.push_back(postprocessor);
}

int SharedResponseManager::AddSolutionCallback(
    std::function<void(const CpSolverResponse&)> callback) {
  absl::MutexLock mutex_lock(mutex_);
  const int id = next_callback_id_++;
  callbacks_.emplace_back(id, std::move(callback));
  return id;
}

void SharedResponseManager::UnregisterCallback(int callback_id) {
  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);
  const int id = next_search_log_callback_id_++;
  search_log_callbacks_.emplace_back(id, std::move(callback));
  return id;
}

void SharedResponseManager::UnregisterLogCallback(int callback_id) {
  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);
  const int id = next_best_bound_callback_id_++;
  best_bound_callbacks_.emplace_back(id, std::move(callback));
  return id;
}

void SharedResponseManager::UnregisterBestBoundCallback(int callback_id) {
  absl::MutexLock mutex_lock(mutex_);
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
    absl::string_view solution_info) {
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
  absl::MutexLock mutex_lock(mutex_);
  CpSolverResponse result;
  if (solution_pool_.BestSolutions().NumSolutions() == 0) {
    result = GetResponseInternal({}, "");
  } else {
    std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
        solution = solution_pool_.BestSolutions().GetSolution(0);
    result = GetResponseInternal(solution->variable_values, solution->info);
  }
  // If this is true, we postsolve and copy all of our solutions.
  if (parameters_.fill_additional_solutions_in_response()) {
    std::vector<int64_t> temp;
    const int size = solution_pool_.BestSolutions().NumSolutions();
    for (int i = 0; i < size; ++i) {
      const auto solution = solution_pool_.BestSolutions().GetSolution(i);
      temp = solution->variable_values;
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
  absl::MutexLock mutex_lock(mutex_);
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

std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
SharedResponseManager::NewSolution(absl::Span<const int64_t> solution_values,
                                   absl::string_view solution_info,
                                   Model* model, int source_id) {
  absl::MutexLock mutex_lock(mutex_);
  std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution> ret;

  // For SAT problems, we add the solution to the solution pool for retrieval
  // later.
  if (objective_or_null_ == nullptr) {
    SharedSolutionRepository<int64_t>::Solution solution;
    solution.variable_values.assign(solution_values.begin(),
                                    solution_values.end());
    solution.info = solution_info;
    solution.source_id = source_id;
    ret = solution_pool_.Add(solution);
  } else {
    const int64_t objective_value =
        ComputeInnerObjective(*objective_or_null_, solution_values);

    // Add this solution to the pool, even if it is not improving.
    SharedSolutionRepository<int64_t>::Solution solution;
    solution.variable_values.assign(solution_values.begin(),
                                    solution_values.end());
    solution.rank = objective_value;
    solution.info = solution_info;
    solution.source_id = source_id;
    ret = solution_pool_.Add(solution);

    // Ignore any non-strictly improving solution.
    if (objective_value > inner_objective_upper_bound_) return ret;

    // Our inner_objective_lower_bound_ should be a globally valid bound, until
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
    solution_pool_.Synchronize(*random_);
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

    // Same as FillSolveStatsInResponse() but since we already hold the mutex...
    if (model != nullptr && !statistics_postprocessors_.empty()) {
      for (const auto& set_stats : statistics_postprocessors_) {
        set_stats(model, &tmp_postsolved_response);
      }
    }
  }

  if (logger_->LoggingIsEnabled()) {
    std::string solution_message(solution_info);
    if (tmp_postsolved_response.num_booleans() > 0) {
      absl::StrAppend(&solution_message, " (fixed_bools=",
                      tmp_postsolved_response.num_fixed_booleans(), "/",
                      tmp_postsolved_response.num_booleans(), ")");
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

  return ret;
}

bool SharedResponseManager::ProblemIsSolved() const {
  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);
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
    const std::string& worker_name, absl::Span<const int> variables,
    absl::Span<const int64_t> new_lower_bounds,
    absl::Span<const int64_t> new_upper_bounds) {
  CHECK_EQ(variables.size(), new_lower_bounds.size());
  CHECK_EQ(variables.size(), new_upper_bounds.size());
  int num_improvements = 0;
  int num_symmetric_improvements = 0;

  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);

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
  absl::MutexLock mutex_lock(mutex_);
  for (const int var :
       changed_variables_since_last_synchronize_.PositionsSetAtLeastOnce()) {
    DCHECK(!has_symmetry_ || var_to_representative_[var] == var);
    synchronized_lower_bounds_[var] = lower_bounds_[var];
    synchronized_upper_bounds_[var] = upper_bounds_[var];
    for (int j = 0; j < id_to_changed_variables_.size(); ++j) {
      id_to_changed_variables_[j].Set(var);
    }
  }
  changed_variables_since_last_synchronize_.ResetAllToFalse();
}

int SharedBoundsManager::RegisterNewId() {
  absl::MutexLock mutex_lock(mutex_);
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
    absl::MutexLock mutex_lock(mutex_);
    for (const int var :
         id_to_changed_variables_[id].PositionsSetAtLeastOnce()) {
      DCHECK(!has_symmetry_ || var_to_representative_[var] == var);
      variables->push_back(var);
    }
    id_to_changed_variables_[id].ResetAllToFalse();

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
  absl::MutexLock mutex_lock(mutex_);
  CHECK_EQ(domains->size(), synchronized_lower_bounds_.size());
  for (int var = 0; var < domains->size(); ++var) {
    (*domains)[var] = (*domains)[var].IntersectionWith(Domain(
        synchronized_lower_bounds_[var], synchronized_upper_bounds_[var]));
  }
}

void SharedBoundsManager::LogStatistics(SolverLogger* logger) {
  absl::MutexLock mutex_lock(mutex_);
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
  absl::MutexLock mutex_lock(mutex_);
  const auto it = bounds_exported_.find(worker_name);
  if (it == bounds_exported_.end()) return 0;
  return it->second.num_exported;
}

UniqueClauseStream::UniqueClauseStream() {
  for (auto& buffer : clauses_by_size_) {
    buffer.reserve(kMaxLiteralsPerBatch);
  }
  fingerprints_.reserve(kMaxFingerprints);
}

bool UniqueClauseStream::Add(absl::Span<const int> clause, int lbd) {
  if (!BlockClause(clause) || lbd > lbd_threshold_) return false;
  std::vector<int>* buffer = MutableBufferForSize(clause.size());
  CHECK_NE(buffer, nullptr);
  if (buffer->size() + clause.size() <= kMaxLiteralsPerBatch) {
    buffer->insert(buffer->end(), clause.begin(), clause.end());
  } else {
    // Maybe replace an old buffered clause of the same size if it has a smaller
    // hash value. This means that the buffer will contain a deterministic
    // sample of the clauses added independent of insertion order.
    const int64_t replaced_clause_id =
        HashClause(clause, 1) % NumClausesOfSize(clause.size());
    absl::Span<int> replaced_clause = absl::MakeSpan(*buffer).subspan(
        replaced_clause_id * clause.size(), clause.size());
    dropped_literals_since_last_batch_ += clause.size();
    if (HashClause(clause, 2) < HashClause(replaced_clause, 2)) {
      std::copy(clause.begin(), clause.end(), replaced_clause.begin());
    }
  }
  return true;
}

bool UniqueClauseStream::BlockClause(absl::Span<const int> clause) {
  if (clause.size() > kMaxClauseSize) return false;
  if (clause.size() <= 2) return false;
  const auto hash = HashClause(clause);
  return fingerprints_.emplace(hash).second &&
         !old_fingerprints_.contains(hash);
}

CompactVectorVector<int> UniqueClauseStream::NextBatch() {
  CompactVectorVector<int> batch;
  batch.reserve(kMaxLiteralsPerBatch / kMinClauseSize, kMaxLiteralsPerBatch);
  int to_fill = kMaxLiteralsPerBatch;
  for (int size = kMinClauseSize; size <= kMaxClauseSize; ++size) {
    CHECK_EQ(NumLiteralsOfSize(size) % size, 0);
    std::vector<int>* buffer = MutableBufferForSize(size);
    while (to_fill >= size && !buffer->empty()) {
      batch.Add(NextClause(size));
      to_fill -= size;
      PopClause(size);
    }
    if (to_fill < size) {
      dropped_literals_since_last_batch_ += buffer->size();
      buffer->clear();
    }
  }

  if (fingerprints_.size() >= kMaxFingerprints / 2) {
    VLOG(2) << "Clearing fingerprints: " << fingerprints_.size() / 1024 << "Ki";
    std::swap(fingerprints_, old_fingerprints_);
    fingerprints_.clear();
    fingerprints_.reserve(kMaxFingerprints);
  }

  if (to_fill > kMaxLiteralsPerBatch / 2 && lbd_threshold_ < kMaxLbd) {
    lbd_threshold_ += 1;
    VLOG(2) << "Inc lbd: " << lbd_threshold_;
  } else if (dropped_literals_since_last_batch_ > 0 &&
             lbd_threshold_ > kMinLbd) {
    lbd_threshold_ -= 1;
    VLOG(2) << "Dec lbd: " << lbd_threshold_;
  }
  dropped_literals_since_last_batch_ = 0;
  return batch;
}

int UniqueClauseStream::NumBufferedLiterals() const {
  int result = 0;
  for (const auto& buffer : clauses_by_size_) {
    result += buffer.size();
  }
  return result;
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

SharedClausesManager::SharedClausesManager(bool always_synchronize)
    : always_synchronize_(always_synchronize) {}

int SharedClausesManager::RegisterNewId(absl::string_view worker_name,
                                        bool may_terminate_early) {
  absl::MutexLock mutex_lock(mutex_);
  num_full_workers_ += may_terminate_early ? 0 : 1;
  const int id = id_to_last_processed_binary_clause_.size();
  id_to_last_processed_binary_clause_.resize(id + 1, 0);
  id_to_last_returned_batch_.resize(id + 1, -1);
  id_to_last_finished_batch_.resize(id + 1, -1);
  id_to_num_exported_.resize(id + 1, 0);
  id_to_worker_name_.resize(id + 1);
  id_to_worker_name_[id] = worker_name;
  return id;
}

int SharedLinear2Bounds::RegisterNewId(std::string worker_name) {
  absl::MutexLock mutex_lock(mutex_);
  const int id = id_to_worker_name_.size();

  id_to_stats_.resize(id + 1);
  id_to_worker_name_.resize(id + 1);
  id_to_worker_name_[id] = worker_name;
  return id;
}

bool SharedClausesManager::ShouldReadBatch(int reader_id, int writer_id) {
  return reader_id != writer_id;
}

void SharedClausesManager::AddBinaryClause(int id, int lit1, int lit2) {
  if (lit2 < lit1) std::swap(lit1, lit2);
  const auto p = std::make_pair(lit1, lit2);

  absl::MutexLock mutex_lock(mutex_);
  const auto [unused_it, inserted] = added_binary_clauses_set_.insert(p);
  if (inserted) {
    added_binary_clauses_.push_back(p);
    if (always_synchronize_) ++last_visible_binary_clause_;
    id_to_num_exported_[id]++;

    // Small optim. If the worker is already up to date with clauses to import,
    // we can mark this new clause as already seen.
    if (id_to_last_processed_binary_clause_[id] ==
        added_binary_clauses_.size() - 1) {
      id_to_last_processed_binary_clause_[id]++;
    }
  }
}

void SharedClausesManager::AddBatch(int id, CompactVectorVector<int> batch) {
  absl::MutexLock mutex_lock(mutex_);
  id_to_num_exported_[id] += batch.size();
  pending_batches_.push_back(std::move(batch));
}

const CompactVectorVector<int>& SharedClausesManager::GetUnseenClauses(int id) {
  std::vector<absl::Span<const int>> result;
  {
    absl::MutexLock mutex_lock(mutex_);
    id_to_last_finished_batch_[id] = id_to_last_returned_batch_[id];
    if (id_to_last_returned_batch_[id] + 1 < batches_.size()) {
      id_to_last_returned_batch_[id] += 1;
      return batches_[id_to_last_returned_batch_[id]];
    }
  }
  static CompactVectorVector<int>* const empty_batch =
      new CompactVectorVector<int>();
  return *empty_batch;
}

void SharedClausesManager::GetUnseenBinaryClauses(
    int id, std::vector<std::pair<int, int>>* new_clauses) {
  new_clauses->clear();
  absl::MutexLock mutex_lock(mutex_);
  const int last_binary_clause_seen = id_to_last_processed_binary_clause_[id];
  if (last_binary_clause_seen >= last_visible_binary_clause_) return;

  new_clauses->assign(
      added_binary_clauses_.begin() + last_binary_clause_seen,
      added_binary_clauses_.begin() + last_visible_binary_clause_);
  id_to_last_processed_binary_clause_[id] = last_visible_binary_clause_;
}

void SharedClausesManager::LogStatistics(SolverLogger* logger) {
  absl::MutexLock mutex_lock(mutex_);
  absl::btree_map<std::string, int64_t> name_to_table_line;
  for (int id = 0; id < id_to_num_exported_.size(); ++id) {
    if (id_to_num_exported_[id] == 0) continue;
    name_to_table_line[id_to_worker_name_[id]] = id_to_num_exported_[id];
  }
  if (!name_to_table_line.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back({"Clauses shared", "Num"});
    for (const auto& [name, count] : name_to_table_line) {
      table.push_back({FormatName(name), FormatCounter(count)});
    }
    SOLVER_LOG(logger, FormatTable(table));
  }
}

// TODO(user): Add some library to simplify this "transposition". Ideally we
// could merge small table with few columns. I am thinking list (row_name,
// col_name, count) + function that create table?
void SharedLinear2Bounds::LogStatistics(SolverLogger* logger) {
  absl::MutexLock mutex_lock(mutex_);
  absl::btree_map<std::string, Stats> name_to_table_line;
  for (int id = 0; id < id_to_stats_.size(); ++id) {
    const Stats stats = id_to_stats_[id];
    if (!stats.empty()) {
      name_to_table_line[id_to_worker_name_[id]] = stats;
    }
  }
  for (int import_id = 0; import_id < import_id_to_index_.size(); ++import_id) {
    name_to_table_line[import_id_to_name_[import_id]].num_imported =
        import_id_to_num_imported_[import_id];
  }
  if (!name_to_table_line.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back({"Linear2 shared", "New", "Updated", "Imported"});
    for (const auto& [name, stats] : name_to_table_line) {
      table.push_back({FormatName(name), FormatCounter(stats.num_new),
                       FormatCounter(stats.num_update),
                       FormatCounter(stats.num_imported)});
    }
    SOLVER_LOG(logger, FormatTable(table));
  }
}

void SharedClausesManager::Synchronize() {
  std::vector<CompactVectorVector<int>> batches_to_merge;
  {
    absl::MutexLock mutex_lock(mutex_);
    last_visible_binary_clause_ = added_binary_clauses_.size();
    const int num_workers = id_to_last_processed_binary_clause_.size();
    if (num_workers <= 1) return;

    if (pending_batches_.size() >= num_full_workers_) {
      batches_to_merge = std::move(pending_batches_);
    }

    // Delete batches that have been consumed by all workers.
    // Keep a few batches around for startup (min finished batch doesn't count
    // workers that haven't registered yet).
    if (batches_.size() > kMinBatches) {
      const int min_finished_batch =
          std::min<int>(batches_.size() - kMinBatches,
                        *absl::c_min_element(id_to_last_finished_batch_) + 1);
      for (int i = 0; i < min_finished_batch; ++i) {
        VLOG(2) << "Erasing batch";
        batches_.pop_front();
      }
      for (int id = 0; id < id_to_last_finished_batch_.size(); ++id) {
        id_to_last_returned_batch_[id] -= min_finished_batch;
        id_to_last_finished_batch_[id] -= min_finished_batch;
      }
    }
    // TODO(user): We could cleanup binary clauses that have been consumed.
  }
  if (batches_to_merge.empty()) return;
  UniqueClauseStream next_batch;
  for (const auto& batch : batches_to_merge) {
    for (int i = 0; i < batch.size(); ++i) {
      next_batch.Add(batch[i]);
    }
  }
  if (next_batch.NumBufferedLiterals() > 0) {
    absl::MutexLock mutex_lock(mutex_);
    VLOG(2) << "Merging batch";
    batches_.push_back(next_batch.NextBatch());
  }
}

void SharedLinear2Bounds::Add(int id, Key expr, IntegerValue lb,
                              IntegerValue ub) {
  DCHECK(expr.IsCanonicalized()) << expr;

  absl::MutexLock mutex_lock(mutex_);
  auto [it, inserted] = shared_bounds_.insert({expr, {lb, ub}});
  if (inserted) {
    // It is new.
    id_to_stats_[id].num_new++;
    newly_updated_keys_.push_back(expr);
  } else {
    // Update the individual bounds if the new ones are better.
    auto& bounds = it->second;
    const bool update_lb = lb > bounds.first;
    if (update_lb) bounds.first = lb;
    const bool update_ub = ub < bounds.second;
    if (update_ub) bounds.second = ub;
    if (update_lb || update_ub) {
      id_to_stats_[id].num_update++;
      newly_updated_keys_.push_back(expr);
    }
  }
}

int SharedLinear2Bounds::RegisterNewImportId(std::string name) {
  absl::MutexLock mutex_lock(mutex_);
  const int import_id = import_id_to_index_.size();
  import_id_to_name_.push_back(name);
  import_id_to_index_.push_back(0);
  import_id_to_num_imported_.push_back(0);
  return import_id;
}

std::vector<
    std::pair<SharedLinear2Bounds::Key, std::pair<IntegerValue, IntegerValue>>>
SharedLinear2Bounds::NewlyUpdatedBounds(int import_id) {
  std::vector<std::pair<Key, std::pair<IntegerValue, IntegerValue>>> result;

  absl::MutexLock mutex_lock(mutex_);
  MaybeCompressNewlyUpdateKeys();
  const int size = newly_updated_keys_.size();
  for (int i = import_id_to_index_[import_id]; i < size; ++i) {
    const auto& key = newly_updated_keys_[i];
    result.push_back({key, shared_bounds_[key]});
  }
  import_id_to_index_[import_id] = size;
  return result;
}

void SharedLinear2Bounds::MaybeCompressNewlyUpdateKeys() {
  int min_index = 0;
  for (const int index : import_id_to_index_) {
    min_index = std::min(index, min_index);
  }
  if (min_index == 0) return;

  newly_updated_keys_.erase(newly_updated_keys_.begin(),
                            newly_updated_keys_.begin() + min_index);
  for (int& index_ref : import_id_to_index_) {
    index_ref -= min_index;
  }
}

void SharedStatistics::AddStats(
    absl::Span<const std::pair<std::string, int64_t>> stats) {
  absl::MutexLock mutex_lock(mutex_);
  for (const auto& [key, count] : stats) {
    stats_[key] += count;
  }
}

void SharedStatistics::Log(SolverLogger* logger) {
  absl::MutexLock mutex_lock(mutex_);
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
