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

#ifndef ORTOOLS_SET_COVER_FORMATTING_HELPER_H_
#define ORTOOLS_SET_COVER_FORMATTING_HELPER_H_

#include <cstdint>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

ABSL_DECLARE_FLAG(bool, latex);
ABSL_DECLARE_FLAG(bool, unicost);

// Formatting and reporting functions with LaTeX and CSV support.
namespace operations_research {
void LogStats(const SetCoverModel& model);

void LogCostAndTiming(absl::string_view problem_name,
                      absl::string_view alg_name, SetCoverInvariant& inv,
                      int64_t run_time);

void LogCostAndTiming(const SetCoverSolutionGenerator& generator);

inline std::string Separator() {
  return absl::GetFlag(FLAGS_latex) ? " & " : ", ";
}

inline std::string Eol() {
  return absl::GetFlag(FLAGS_latex) ? " \\\\\n" : "\n";
}

// Computes the ratio of the cost of the new solution generator to the cost of
// the reference solution generator.
double CostRatio(SetCoverSolutionGenerator& ref_gen,
                 SetCoverSolutionGenerator& new_gen);

// Computes the speedup of the new solution generator compared to the reference
// solution generator, where the speedup is defined as the ratio of the
// cumulated time of the reference solution generator to the cumulated time of
// the new solution generator.
double Speedup(SetCoverSolutionGenerator& ref_gen,
               SetCoverSolutionGenerator& new_gen);

// Same as above, but the cumulated time is the sum of the initialization and
// search times for two pairs of solution generators.
double SpeedupOnCumulatedTimes(SetCoverSolutionGenerator& ref_init,
                               SetCoverSolutionGenerator& ref_search,
                               SetCoverSolutionGenerator& new_init,
                               SetCoverSolutionGenerator& new_search);

// In the case of LaTeX, the stats are printed in the format:
//   & 123.456 (123) ± 12.34 (12) & [123, 456]  corresponding to
//   & mean (median) ± stddev (iqr) & [min, max]
// In the case of CSV, the stats are printed as a VerboseString.
std::string FormatStats(const SetCoverModel::Stats& stats);

// Returns the string str in LaTeX bold if condition is true and --latex is
// true.
std::string BoldIf(bool condition, absl::string_view str);

// Formats time in microseconds for LaTeX. For CSV, it is formatted as
// seconds by adding the suffix "e-6, s".
std::string FormatTime(int64_t time_us);

// Formats the cost and time, with cost in bold if the condition is true.
std::string FormatCostAndTimeIf(bool condition, double cost, int64_t time_us);

// Formats the speedup with one decimal place.
// TODO(user): Bolden the speedup if it is better than 1.0.
std::string FormattedSpeedup(SetCoverSolutionGenerator& ref_gen,
                             SetCoverSolutionGenerator& new_gen);

// Reports the second of the comparison of two solution generators, with only
// the cost and time of the new solution generator.
std::string ReportSecondPart(SetCoverSolutionGenerator& ref_gen,
                             SetCoverSolutionGenerator& new_gen);

// Reports the cost and time of both solution generators.
std::string ReportBothParts(SetCoverSolutionGenerator& ref_gen,
                            SetCoverSolutionGenerator& new_gen);

// Formats the model and stats in LaTeX or CSV format.
std::string FormatModelAndStats(const SetCoverModel& model);

// Sets the name of the model to the filename, with a * suffix if the model is
// unicost. Removes the .txt suffix from the filename if any.
void SetModelName(absl::string_view filename, SetCoverModel* model);

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_FORMATTING_HELPER_H_
