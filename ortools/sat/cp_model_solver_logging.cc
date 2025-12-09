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

#include "ortools/sat/cp_model_solver_logging.h"

#include <cctype>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

namespace {
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

void SolverProgressLogger::UpdateProgress(const SolverStatusChangeInfo& info) {
  if (info.solved) {
    SOLVER_LOG(logger_,
               SatProgressMessage("Done", wall_timer_.Get(), info.change_info));
    return;
  }
  if (info.new_best_solution) {
    RegisterSolutionFound(info.change_info, num_solutions_);

    num_solutions_++;
    if (is_optimization_) {
      RegisterSolutionFound(info.change_info, num_solutions_);
      SOLVER_LOG(logger_,
                 ProgressMessage(
                     absl::StrCat(num_solutions_), wall_timer_.Get(),
                     info.best_objective_value, info.cur_objective_value_lb,
                     info.cur_objective_value_ub, info.change_info));
      return;
    } else {
      SOLVER_LOG(logger_,
                 SatProgressMessage(absl::StrCat(num_solutions_),
                                    wall_timer_.Get(), info.change_info));
    }
  }
  if (info.new_lower_bound || info.new_upper_bound) {
    logger_->ThrottledLog(
        bounds_logging_id_,
        ProgressMessage("Bound", wall_timer_.Get(), info.best_objective_value,
                        info.cur_objective_value_lb,
                        info.cur_objective_value_ub, info.change_info));
    RegisterObjectiveBoundImprovement(info.change_info);
  }
}

void SolverProgressLogger::RegisterSolutionFound(
    const std::string& improvement_info, int solution_number) {
  if (improvement_info.empty()) return;
  const std::string subsolver_name = ExtractSubSolverName(improvement_info);
  primal_improvements_count_[subsolver_name]++;
  primal_improvements_min_rank_.insert({subsolver_name, solution_number});
  primal_improvements_max_rank_[subsolver_name] = solution_number;
}

void SolverProgressLogger::RegisterObjectiveBoundImprovement(
    const std::string& improvement_info) {
  if (improvement_info.empty() || improvement_info == "initial domain") return;
  dual_improvements_count_[ExtractSubSolverName(improvement_info)]++;
}

void SolverProgressLogger::DisplayImprovementStatistics(
    SolverLogger* logger) const {
  if (!primal_improvements_count_.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back(
        {absl::StrCat("Solutions (", num_solutions_, ")"), "Num", "Rank"});
    for (const auto& entry : primal_improvements_count_) {
      auto it = primal_improvements_min_rank_.find(entry.first);
      if (it == primal_improvements_min_rank_.end()) continue;
      const int min_rank = it->second;
      it = primal_improvements_max_rank_.find(entry.first);
      if (it == primal_improvements_max_rank_.end()) continue;
      const int max_rank = it->second;
      table.push_back({FormatName(entry.first), FormatCounter(entry.second),
                       absl::StrCat("[", min_rank, ",", max_rank, "]")});
    }
    SOLVER_LOG(logger, FormatTable(table));
  }
  if (!dual_improvements_count_.empty()) {
    std::vector<std::vector<std::string>> table;
    table.push_back({"Objective bounds", "Num"});
    for (const auto& entry : dual_improvements_count_) {
      table.push_back({FormatName(entry.first), FormatCounter(entry.second)});
    }
    SOLVER_LOG(logger, FormatTable(table));
  }
}

}  // namespace sat
}  // namespace operations_research
