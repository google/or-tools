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

#include "ortools/set_cover/reporting.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

std::string Separator(bool csv, bool latex) {
  return csv ? ", " : (latex ? " & " : " ");
}
std::string Eol(bool csv, bool latex) {
  return csv ? "\n" : (latex ? " \\\\\n" : "\n");
}

void Report::ReportModelNameAndSizes(const SetCoverModel& model) {
  if (latex_ || csv_) {
    absl::StrAppend(&output_, model.name(), Separator(), model.num_subsets(),
                    Separator(), model.num_elements(), Separator(),
                    model.num_nonzeros());
  } else {
    absl::StrAppend(&output_, "\n\nName: ", model.name(),
                    "\nNum subsets: ", model.num_subsets(),
                    "\nNum elements: ", model.num_elements(),
                    "\nNum non-zeros: ", model.num_nonzeros(), "\n");
  }
}

void Report::ReportRunResult(const RunResult& result) {
  if (latex_ || csv_) {
    absl::StrAppend(&output_, Separator(),
                    FormatCostAndTimeIf(
                        false, result.cost(),
                        absl::ToInt64Microseconds(result.total_duration())));
  } else {
    absl::StrAppend(
        &output_, "  ", result.algorithm_name(), ", Cost = ", result.cost(),
        ", Solution cardinality = ", result.solution_cardinality(),
        ", Run time = ", absl::FormatDuration(result.total_duration()), "\n");
  }
}

void Report::ReportRunStats(const RunStats& stats) {
  if (latex_ || csv_) {
    absl::StrAppend(&output_, stats.new_batch_name(), Separator(),
                    stats.ref_batch_name(), Separator(),
                    stats.GetGeoMeanSpeedup(), Separator(),
                    stats.GetGeoMeanCostRatio());
  } else {
    absl::StrAppend(&output_, stats.new_batch_name(), " / ",
                    stats.ref_batch_name(),
                    ", Speedup geomean = ", stats.GetGeoMeanSpeedup(),
                    ", Cost ratio geomean, ", stats.GetGeoMeanCostRatio());
  }
}

// Reports the gap between two solutions in a string. Returns the result as a
// RunResult for further use.
RunResult Report::ReportGap(const RunResult& new_run,
                            const RunResult& ref_run) {
  if (new_run.algorithm_name().empty() || ref_run.algorithm_name().empty()) {
    return RunResult();
  }
  QCHECK_EQ(new_run.problem_name(), ref_run.problem_name())
      << "Problem names do not match: " << new_run.problem_name() << " vs "
      << ref_run.problem_name() << "."
      << "new_algorithm_name: " << new_run.algorithm_name()
      << " ref_algorithm_name: " << ref_run.algorithm_name();
  const std::string separator = latex_ || csv_ ? "GapVs" : " / ";
  const RunResult result(new_run.problem_name(),
                         absl::StrCat(new_run.algorithm_name(), separator,
                                      ref_run.algorithm_name()),
                         new_run.cost() / ref_run.cost() - 1.0,  // cost
                         0,                     // solution cardinality
                         absl::ZeroDuration(),  // time
                         SubsetBoolVector()     // solution
  );
  const std::string formatted_gap =
      absl::StrFormat("%.1f", result.cost() * 100.0);
  if (csv_ || latex_) {
    absl::StrAppend(&output_, Separator(), formatted_gap, "%");
  } else {
    absl::StrAppend(&output_, "    Gap:", result.algorithm_name(), " = ",
                    formatted_gap, "%\n");
  }
  return result;
}

void Report::LogStats(const SetCoverModel& model) const {
  std::string header = absl::StrCat(model.name(), sep_);
  // Lines start with a comma to make it easy to copy-paste the output to a
  // spreadsheet as CSV.
  if (!latex_) {
    header = absl::StrCat(sep_, header);
  }
  LOG(INFO) << header << model.ToVerboseString(sep_);
  LOG(INFO) << header << "cost" << sep_
            << model.ComputeCostStats().ToVerboseString(sep_);
  LOG(INFO) << header << "row size stats" << sep_
            << model.ComputeRowStats().ToVerboseString(sep_);
  LOG(INFO) << header << "row size deciles" << sep_
            << absl::StrJoin(model.ComputeRowDeciles(), sep_);
  LOG(INFO) << header << "column size stats" << sep_
            << model.ComputeColumnStats().ToVerboseString(sep_);
  LOG(INFO) << header << "column size deciles" << sep_
            << absl::StrJoin(model.ComputeColumnDeciles(), sep_);
  LOG(INFO) << header << "num singleton rows" << sep_
            << model.ComputeNumSingletonRows() << sep_
            << "num singleton columns" << sep_
            << model.ComputeNumSingletonColumns();
}

double Report::CostRatio(const RunResult& new_result,
                         const RunResult& ref_result) const {
  return new_result.cost() / ref_result.cost();
}

double Report::Speedup(const RunResult& new_result,
                       const RunResult& ref_result) const {
  // Avoid division by zero by considering the case where the new generator took
  // less than 1 nanosecond (!) to run.
  return 1.0 * absl::ToInt64Nanoseconds(ref_result.total_duration()) /
         std::max<int64_t>(
             1, absl::ToInt64Nanoseconds(new_result.total_duration()));
}

// In the case of LaTeX, the stats are printed in the format:
//   & 123.456 (123) +/- 12.34 (12) & [123, 456]  corresponding to
//   & mean (median) +/- stddev (iqr) & [min, max]
// In the case of CSV, the stats are printed as a VerboseString.
std::string Report::FormatStats(const SetCoverModel::Stats& stats) const {
  return latex_ ? absl::StrFormat(
                      " & $%#.2f (%.0f) \\pm %#.2f (%.0f)$ & $[%.0f, %.0f]$",
                      stats.mean, stats.median, stats.stddev, stats.iqr,
                      stats.min, stats.max)
                : stats.ToVerboseString(Separator());
}

// Returns the string str in LaTeX bold if condition is true and --latex is
// true.
std::string Report::BoldIf(bool condition, absl::string_view str) const {
  return condition && latex_ ? absl::StrCat("\\textbf{", str, "}")
                             : std::string(str);
}

// Formats time in microseconds for LaTeX. For CSV, it is formatted as
// seconds by adding the suffix "e-6, s".
std::string Report::FormatTime(int64_t time_us) const {
  return latex_ ? absl::StrFormat("%lld", time_us)
                : absl::StrFormat("%llde-6, s", time_us);
}

// Formats the cost and time, with cost in bold if the condition is true.
std::string Report::FormatCostAndTimeIf(bool condition, double cost,
                                        int64_t time_us) const {
  const std::string cost_display =
      BoldIf(condition, absl::StrFormat("%.9g", cost));
  return absl::StrCat(cost_display, Separator(), FormatTime(time_us));
}

// TODO(user): Bolden the speedup if it is better than 1.0.
std::string Report::FormattedSpeedup(const RunResult& new_result,
                                     const RunResult& ref_result) const {
  return absl::StrFormat("%.1f", Speedup(new_result, ref_result));
}

void Report::ReportModelStats(const SetCoverModel& model) {
  if (latex_) {
    absl::StrAppend(&output_, model.name(), Separator(),
                    model.ToString(Separator()),
                    FormatStats(model.ComputeColumnStats()),
                    FormatStats(model.ComputeRowStats()));
  } else if (csv_) {
    const std::string header =
        absl::StrCat(Separator(), model.name(), Separator());
    absl::StrAppend(&output_, header, model.ToString(Separator()), Eol(),
                    header, "column size stats", Separator(),
                    FormatStats(model.ComputeColumnStats()), Eol(), header,
                    "row size stats", Separator(),
                    FormatStats(model.ComputeRowStats()), Eol());
  } else {
    absl::StrAppend(&output_, model.name(), Separator(),
                    model.ToString(Separator()),
                    FormatStats(model.ComputeColumnStats()),
                    FormatStats(model.ComputeRowStats()), Eol());
  }
}

void Report::SetModelName(absl::string_view filename, bool unicost,
                          SetCoverModel* model) const {
  std::string problem_name = std::string(filename);
  constexpr absl::string_view kTxtSuffix = ".txt";
  constexpr absl::string_view kDatSuffix = ".dat";
  // Remove the .txt suffix from the problem name if any.
  problem_name = absl::StripSuffix(problem_name, kTxtSuffix);
  problem_name = absl::StripSuffix(problem_name, kDatSuffix);
  if (unicost || model->is_unicost()) {
    absl::StrAppend(&problem_name, "*");
  }
  model->SetName(problem_name);
}
}  // namespace operations_research
