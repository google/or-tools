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

#include "ortools/set_cover/formatting_helper.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

ABSL_DECLARE_FLAG(bool, latex);
ABSL_DECLARE_FLAG(bool, unicost);

namespace operations_research {
void LogStats(const SetCoverModel& model) {
  const std::string sep = absl::GetFlag(FLAGS_latex) ? " & " : ", ";
  std::string header = absl::StrCat(model.name(), sep);
  // Lines start with a comma to make it easy to copy-paste the output to a
  // spreadsheet as CSV.
  if (!absl::GetFlag(FLAGS_latex)) {
    header = absl::StrCat(sep, header);
  }
  LOG(INFO) << header << model.ToVerboseString(sep);
  LOG(INFO) << header << "cost" << sep
            << model.ComputeCostStats().ToVerboseString(sep);
  LOG(INFO) << header << "row size stats" << sep
            << model.ComputeRowStats().ToVerboseString(sep);
  LOG(INFO) << header << "row size deciles" << sep
            << absl::StrJoin(model.ComputeRowDeciles(), sep);
  LOG(INFO) << header << "column size stats" << sep
            << model.ComputeColumnStats().ToVerboseString(sep);
  LOG(INFO) << header << "column size deciles" << sep
            << absl::StrJoin(model.ComputeColumnDeciles(), sep);
  LOG(INFO) << header << "num_singleton_rows" << sep
            << model.ComputeNumSingletonRows() << sep << "num_singleton_columns"
            << sep << model.ComputeNumSingletonColumns();
}

void LogCostAndTiming(const absl::string_view problem_name,
                      absl::string_view alg_name, SetCoverInvariant& inv,
                      int64_t run_time) {
  LOG(INFO) << ", " << problem_name << ", " << alg_name << ", cost, "
            << inv.CostOrLowerBound() << ", solution_cardinality, "
            << inv.ComputeCardinality() << ", " << run_time << "e-6, s";
}

void LogCostAndTiming(const SetCoverSolutionGenerator& generator) {
  SetCoverInvariant& inv = *generator.inv();
  const SetCoverModel& model = *inv.model();
  LogCostAndTiming(model.name(), generator.name(), inv,
                   generator.run_time_us());
}

double CostRatio(SetCoverSolutionGenerator& ref_gen,
                 SetCoverSolutionGenerator& new_gen) {
  return new_gen.cost() / ref_gen.cost();
}

double Speedup(SetCoverSolutionGenerator& ref_gen,
               SetCoverSolutionGenerator& new_gen) {
  // Avoid division by zero by considering the case where the new generator took
  // less than 1 nanosecond (!) to run.
  return 1.0 * ref_gen.run_time_ns() /
         std::max<int64_t>(1, new_gen.run_time_ns());
}

double SpeedupOnCumulatedTimes(SetCoverSolutionGenerator& ref_init,
                               SetCoverSolutionGenerator& ref_search,
                               SetCoverSolutionGenerator& new_init,
                               SetCoverSolutionGenerator& new_search) {
  return 1.0 * (ref_init.run_time_ns() + ref_search.run_time_ns()) /
         std::max<int64_t>(1,
                           new_init.run_time_ns() + new_search.run_time_ns());
}

// In the case of LaTeX, the stats are printed in the format:
//   & 123.456 (123) +/- 12.34 (12) & [123, 456]  corresponding to
//   & mean (median) +/- stddev (iqr) & [min, max]
// In the case of CSV, the stats are printed as a VerboseString.
std::string FormatStats(const SetCoverModel::Stats& stats) {
  return absl::GetFlag(FLAGS_latex)
             ? absl::StrFormat(
                   " & $%#.2f (%.0f) \\pm %#.2f (%.0f)$ & $[%.0f, %.0f]$",
                   stats.mean, stats.median, stats.stddev, stats.iqr, stats.min,
                   stats.max)
             : stats.ToVerboseString(", ");
}

// Returns the string str in LaTeX bold if condition is true and --latex is
// true.
std::string BoldIf(bool condition, absl::string_view str) {
  return condition && absl::GetFlag(FLAGS_latex)
             ? absl::StrCat("\\textbf{", str, "}")
             : std::string(str);
}

// Formats time in microseconds for LaTeX. For CSV, it is formatted as
// seconds by adding the suffix "e-6, s".
std::string FormatTime(int64_t time_us) {
  return absl::GetFlag(FLAGS_latex) ? absl::StrFormat("%d", time_us)
                                    : absl::StrFormat("%de-6, s", time_us);
}

// Formats the cost and time, with cost in bold if the condition is true.
std::string FormatCostAndTimeIf(bool condition, double cost, int64_t time_us) {
  const std::string cost_display =
      BoldIf(condition, absl::StrFormat("%.9g", cost));
  return absl::StrCat(cost_display, Separator(), FormatTime(time_us));
}

// TODO(user): Bolden the speedup if it is better than 1.0.
std::string FormattedSpeedup(SetCoverSolutionGenerator& ref_gen,
                             SetCoverSolutionGenerator& new_gen) {
  return absl::StrFormat("%.1f", Speedup(ref_gen, new_gen));
}

std::string ReportSecondPart(SetCoverSolutionGenerator& ref_gen,
                             SetCoverSolutionGenerator& new_gen) {
  const double ref_cost = ref_gen.cost();
  const double new_cost = new_gen.cost();
  return absl::StrCat(Separator(),
                      FormatCostAndTimeIf(new_cost <= ref_cost, new_cost,
                                          new_gen.run_time_us()),
                      Separator(), FormattedSpeedup(ref_gen, new_gen));
}

std::string ReportBothParts(SetCoverSolutionGenerator& ref_gen,
                            SetCoverSolutionGenerator& new_gen) {
  const double ref_cost = ref_gen.cost();
  const double new_cost = new_gen.cost();
  return absl::StrCat(Separator(),
                      FormatCostAndTimeIf(ref_cost <= new_cost, ref_cost,
                                          ref_gen.run_time_us()),
                      ReportSecondPart(ref_gen, new_gen));
}

std::string FormatModelAndStats(const SetCoverModel& model) {
  if (absl::GetFlag(FLAGS_latex)) {
    return absl::StrCat(model.name(), Separator(), model.ToString(Separator()),
                        FormatStats(model.ComputeColumnStats()),
                        FormatStats(model.ComputeRowStats()));
  } else {  // CSV
    const std::string header =
        absl::StrCat(Separator(), model.name(), Separator());
    return absl::StrCat(header, model.ToString(Separator()), Eol(), header,
                        "column size stats", Separator(),
                        FormatStats(model.ComputeColumnStats()), Eol(), header,
                        "row size stats", Separator(),
                        FormatStats(model.ComputeRowStats()), Eol());
  }
}

void SetModelName(absl::string_view filename, SetCoverModel* model) {
  std::string problem_name = std::string(filename);
  constexpr absl::string_view kTxtSuffix = ".txt";
  // Remove the .txt suffix from the problem name if any.
  if (absl::EndsWith(problem_name, kTxtSuffix)) {
    problem_name.resize(problem_name.size() - kTxtSuffix.size());
  }
  if (absl::GetFlag(FLAGS_unicost) || model->is_unicost()) {
    absl::StrAppend(&problem_name, "*");
  }
  model->SetName(problem_name);
}

}  // namespace operations_research
