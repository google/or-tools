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

#include "ortools/sat/stat_tables.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"

namespace operations_research::sat {

SharedStatTables::SharedStatTables() {
  absl::MutexLock mutex_lock(&mutex_);

  timing_table_.push_back(
      {"Task timing", "n [     min,      max]      avg      dev     time",
       "n [     min,      max]      avg      dev    dtime"});

  search_table_.push_back({"Search stats", "Bools", "Conflicts", "Branches",
                           "Restarts", "BoolPropag", "IntegerPropag"});

  clauses_table_.push_back({"SAT stats", "ClassicMinim", "LitRemoved",
                            "LitLearned", "LitForgotten", "Subsumed",
                            "MClauses", "MDecisions", "MLitTrue", "MSubsumed",
                            "MLitRemoved", "MReused"});

  lp_table_.push_back({"Lp stats", "Component", "Iterations", "AddedCuts",
                       "OPTIMAL", "DUAL_F.", "DUAL_U."});

  lp_dim_table_.push_back(
      {"Lp dimension", "Final dimension of first component"});

  lp_debug_table_.push_back({"Lp debug", "CutPropag", "CutEqPropag", "Adjust",
                             "Overflow", "Bad", "BadScaling"});

  lp_manager_table_.push_back({"Lp pool", "Constraints", "Updates", "Simplif",
                               "Merged", "Shortened", "Split", "Strengthened",
                               "Cuts/Call"});

  lns_table_.push_back(
      {"LNS stats", "Improv/Calls", "Closed", "Difficulty", "TimeLimit"});

  ls_table_.push_back({"LS stats", "Batches", "Restarts/Perturbs", "LinMoves",
                       "GenMoves", "CompoundMoves", "Bactracks",
                       "WeightUpdates", "ScoreComputed"});
}

void SharedStatTables::AddTimingStat(const SubSolver& subsolver) {
  absl::MutexLock mutex_lock(&mutex_);
  timing_table_.push_back({FormatName(subsolver.name()), subsolver.TimingInfo(),
                           subsolver.DeterministicTimingInfo()});
}

void SharedStatTables::AddSearchStat(absl::string_view name, Model* model) {
  absl::MutexLock mutex_lock(&mutex_);
  CpSolverResponse r;
  model->GetOrCreate<SharedResponseManager>()->FillSolveStatsInResponse(model,
                                                                        &r);
  search_table_.push_back({FormatName(name), FormatCounter(r.num_booleans()),
                           FormatCounter(r.num_conflicts()),
                           FormatCounter(r.num_branches()),
                           FormatCounter(r.num_restarts()),
                           FormatCounter(r.num_binary_propagations()),
                           FormatCounter(r.num_integer_propagations())});
}

void SharedStatTables::AddClausesStat(absl::string_view name, Model* model) {
  absl::MutexLock mutex_lock(&mutex_);
  SatSolver::Counters counters = model->GetOrCreate<SatSolver>()->counters();
  clauses_table_.push_back(
      {FormatName(name), FormatCounter(counters.num_minimizations),
       FormatCounter(counters.num_literals_removed),
       FormatCounter(counters.num_literals_learned),
       FormatCounter(counters.num_literals_forgotten),
       FormatCounter(counters.num_subsumed_clauses),
       FormatCounter(counters.minimization_num_clauses),
       FormatCounter(counters.minimization_num_decisions),
       FormatCounter(counters.minimization_num_true),
       FormatCounter(counters.minimization_num_subsumed),
       FormatCounter(counters.minimization_num_removed_literals),
       FormatCounter(counters.minimization_num_reused)});
}

void SharedStatTables::AddLpStat(absl::string_view name, Model* model) {
  absl::MutexLock mutex_lock(&mutex_);

  // Sum per component for the lp_table.
  int64_t num_compo = 0;
  int64_t num_iter = 0;
  int64_t num_cut = 0;
  int64_t num_optimal = 0;
  int64_t num_dual_feasible = 0;
  int64_t num_dual_unbounded = 0;

  // Last dimension of the first component for the lp_dim_table_.
  std::string dimension;

  // Sum per component for the lp_manager_table_.
  int64_t num_constraints = 0;
  int64_t num_constraint_updates = 0;
  int64_t num_simplifications = 0;
  int64_t num_merged_constraints = 0;
  int64_t num_shortened_constraints = 0;
  int64_t num_split_constraints = 0;
  int64_t num_coeff_strengthening = 0;
  int64_t num_cuts = 0;
  int64_t num_add_cut_calls = 0;

  // For the cut table.
  absl::btree_map<std::string, int> type_to_num_cuts;

  // For the debug table.
  int64_t total_num_cut_propagations = 0;
  int64_t total_num_eq_propagations = 0;
  int64_t num_adjusts = 0;
  int64_t num_cut_overflows = 0;
  int64_t num_bad_cuts = 0;
  int64_t num_scaling_issues = 0;

  auto* lps = model->GetOrCreate<LinearProgrammingConstraintCollection>();
  for (const auto* lp : *lps) {
    const auto& manager = lp->constraint_manager();
    ++num_compo;
    num_iter += lp->total_num_simplex_iterations();
    num_cut += manager.num_cuts();
    const auto& num_solve_by_status = lp->num_solves_by_status();

    const int optimal_as_int = static_cast<int>(glop::ProblemStatus::OPTIMAL);
    if (optimal_as_int < num_solve_by_status.size()) {
      num_optimal += num_solve_by_status[optimal_as_int];
    }

    const int dual_feasible_as_int =
        static_cast<int>(glop::ProblemStatus::DUAL_FEASIBLE);
    if (dual_feasible_as_int < num_solve_by_status.size()) {
      num_dual_feasible += num_solve_by_status[dual_feasible_as_int];
    }

    const int dual_unbounded_as_int =
        static_cast<int>(glop::ProblemStatus::DUAL_UNBOUNDED);
    if (dual_unbounded_as_int < num_solve_by_status.size()) {
      num_dual_unbounded += num_solve_by_status[dual_unbounded_as_int];
    }

    // In case of more than one component, we take the first one.
    if (dimension.empty()) dimension = lp->DimensionString();

    // Sum for the lp debug table.
    total_num_cut_propagations += lp->total_num_cut_propagations();
    total_num_eq_propagations += lp->total_num_eq_propagations();
    num_adjusts += lp->num_adjusts();
    num_cut_overflows += lp->num_cut_overflows();
    num_bad_cuts += lp->num_bad_cuts();
    num_scaling_issues += lp->num_scaling_issues();

    // Sum for the lp manager table.
    num_constraints += manager.num_constraints();
    num_constraint_updates += manager.num_constraint_updates();
    num_simplifications += manager.num_simplifications();
    num_merged_constraints += manager.num_merged_constraints();
    num_shortened_constraints += manager.num_shortened_constraints();
    num_split_constraints += manager.num_split_constraints();
    num_coeff_strengthening += manager.num_coeff_strenghtening();
    num_cuts += manager.num_cuts();
    num_add_cut_calls += manager.num_add_cut_calls();

    for (const auto& [cut_name, num] : manager.type_to_num_cuts()) {
      type_to_num_cuts[cut_name] += num;
    }
  }
  if (num_compo == 0) return;

  lp_table_.push_back(
      {FormatName(name), FormatCounter(num_compo), FormatCounter(num_iter),
       FormatCounter(num_cut), FormatCounter(num_optimal),
       FormatCounter(num_dual_feasible), FormatCounter(num_dual_unbounded)});

  if (!dimension.empty()) {
    lp_dim_table_.push_back({FormatName(name), dimension});
  }

  if (!type_to_num_cuts.empty()) {
    lp_cut_table_.push_back({std::string(name), std::move(type_to_num_cuts)});
  }

  lp_debug_table_.push_back(
      {FormatName(name), FormatCounter(total_num_cut_propagations),
       FormatCounter(total_num_eq_propagations), FormatCounter(num_adjusts),
       FormatCounter(num_cut_overflows), FormatCounter(num_bad_cuts),
       FormatCounter(num_scaling_issues)});

  lp_manager_table_.push_back({FormatName(name), FormatCounter(num_constraints),
                               FormatCounter(num_constraint_updates),
                               FormatCounter(num_simplifications),
                               FormatCounter(num_merged_constraints),
                               FormatCounter(num_shortened_constraints),
                               FormatCounter(num_split_constraints),
                               FormatCounter(num_coeff_strengthening),
                               absl::StrCat(FormatCounter(num_cuts), "/",
                                            FormatCounter(num_add_cut_calls))});
}

void SharedStatTables::AddLnsStat(absl::string_view name,
                                  int64_t num_fully_solved_calls,
                                  int64_t num_calls,
                                  int64_t num_improving_calls,
                                  double difficulty,
                                  double deterministic_limit) {
  absl::MutexLock mutex_lock(&mutex_);
  const double fully_solved_proportion =
      static_cast<double>(num_fully_solved_calls) /
      static_cast<double>(std::max(int64_t{1}, num_calls));
  lns_table_.push_back(
      {FormatName(name), absl::StrCat(num_improving_calls, "/", num_calls),
       absl::StrFormat("%2.0f%%", 100 * fully_solved_proportion),
       absl::StrFormat("%0.2e", difficulty),
       absl::StrFormat("%0.2f", deterministic_limit)});
}

void SharedStatTables::AddLsStat(absl::string_view name, int64_t num_batches,
                                 int64_t num_restarts, int64_t num_linear_moves,
                                 int64_t num_general_moves,
                                 int64_t num_compound_moves,
                                 int64_t num_backtracks,
                                 int64_t num_weight_updates,
                                 int64_t num_scores_computed) {
  absl::MutexLock mutex_lock(&mutex_);
  ls_table_.push_back(
      {FormatName(name), FormatCounter(num_batches),
       FormatCounter(num_restarts), FormatCounter(num_linear_moves),
       FormatCounter(num_general_moves), FormatCounter(num_compound_moves),
       FormatCounter(num_backtracks), FormatCounter(num_weight_updates),
       FormatCounter(num_scores_computed)});
}

void SharedStatTables::Display(SolverLogger* logger) {
  if (!logger->LoggingIsEnabled()) return;

  absl::MutexLock mutex_lock(&mutex_);
  if (timing_table_.size() > 1) SOLVER_LOG(logger, FormatTable(timing_table_));
  if (search_table_.size() > 1) SOLVER_LOG(logger, FormatTable(search_table_));
  if (clauses_table_.size() > 1) {
    SOLVER_LOG(logger, FormatTable(clauses_table_));
  }

  if (lp_table_.size() > 1) SOLVER_LOG(logger, FormatTable(lp_table_));
  if (lp_dim_table_.size() > 1) SOLVER_LOG(logger, FormatTable(lp_dim_table_));
  if (lp_debug_table_.size() > 1) {
    SOLVER_LOG(logger, FormatTable(lp_debug_table_));
  }
  if (lp_manager_table_.size() > 1) {
    SOLVER_LOG(logger, FormatTable(lp_manager_table_));
  }

  // Construct and generate lp cut table.
  // Note that this one is transposed compared to the normal one.
  if (!lp_cut_table_.empty()) {
    // Collect all line names.
    absl::btree_map<std::string, int> lines;
    for (const auto& [_, map] : lp_cut_table_) {
      for (const auto& [type_name, _] : map) {
        lines[type_name] = 0;
      }
    }

    // Create header and compute index.
    std::vector<std::vector<std::string>> table;
    int line_index = 1;
    const int num_cols = lp_cut_table_.size() + 1;
    table.push_back({"Lp Cut"});
    table[0].resize(num_cols, "");
    for (const auto& [type_name, _] : lines) {
      lines[type_name] = line_index++;
      table.push_back({absl::StrCat(type_name, ":")});
      table.back().resize(num_cols, "-");
    }

    // Fill lines.
    int col_index = 1;
    for (const auto& [name, map] : lp_cut_table_) {
      table[0][col_index] =
          num_cols > 10 && name.size() > 6 ? name.substr(0, 6) : name;
      for (const auto& [type_name, count] : map) {
        table[lines[type_name]][col_index] = FormatCounter(count);
      }
      ++col_index;
    }

    if (table.size() > 1) SOLVER_LOG(logger, FormatTable(table));
  }

  if (lns_table_.size() > 1) SOLVER_LOG(logger, FormatTable(lns_table_));
  if (ls_table_.size() > 1) SOLVER_LOG(logger, FormatTable(ls_table_));
}

}  // namespace operations_research::sat
