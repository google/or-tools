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

#ifndef ORTOOLS_SET_COVER_REPORTING_H_
#define ORTOOLS_SET_COVER_REPORTING_H_

#include <cmath>
#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/base/protoutil.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover.pb.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

// Returns the separator string for CSV or LaTeX format or space otherwise.
std::string Separator(bool csv, bool latex);

// Returns the end of line string for CSV or LaTeX format or newline otherwise.
std::string Eol(bool csv, bool latex);

// This class is used to store the results of a run of a solution generator,
// including the algorithm name, the cost, the time, and the solution itself.
class RunResult {
 public:
  // Default constructor. Note that problem_name_ and algorithm_name_ are empty,
  // which is fine as this state is used to indicate that the run did not
  // happen, thus there is no result to report.
  RunResult() = default;

  explicit RunResult(const SetCoverSolutionGenerator& gen)
      : response_(gen.inv()->ExportSolutionAsProto()),
        solution_(gen.inv()->is_selected()) {
    response_.set_problem_name(gen.inv()->model()->name());
    response_.set_algorithm_name(gen.name());
    *response_.mutable_total_duration() =
        util_time::EncodeGoogleApiProto(gen.run_time()).value();
    response_.set_is_unicost(gen.inv()->model()->is_unicost());
    response_.set_solution_cardinality(gen.inv()->ComputeCardinality());
    // TODO(user): add time_to_best to the generator.
    // *response_.mutable_duration_to_best() =
    //     util_time::EncodeGoogleApiProto(gen.time_to_best()).value();
  }

  RunResult(absl::string_view problem_name, absl::string_view algorithm_name,
            Cost cost, BaseInt solution_cardinality, absl::Duration time,
            const SubsetBoolVector& solution, bool is_optimal = false) {
    CHECK(!problem_name.empty());
    CHECK(!algorithm_name.empty());
    response_.set_problem_name(std::string(problem_name));
    response_.set_algorithm_name(std::string(algorithm_name));
    response_.set_cost(cost);
    *response_.mutable_total_duration() =
        util_time::EncodeGoogleApiProto(time).value();
    response_.set_is_optimal(is_optimal);
    response_.set_solution_cardinality(solution_cardinality);
    solution_ = solution;
    response_.set_num_subsets(solution.size());
    for (BaseInt i = 0; i < solution.size(); ++i) {
      if (solution[SubsetIndex(i)]) {
        response_.add_subset(i);
      }
    }
  }

  absl::string_view problem_name() const { return response_.problem_name(); }
  absl::string_view algorithm_name() const {
    return response_.algorithm_name();
  }
  Cost cost() const { return response_.cost(); }
  BaseInt solution_cardinality() const { return response_.subset_size(); }
  absl::Duration total_duration() const {
    return util_time::DecodeGoogleApiProto(response_.total_duration()).value();
  }
  absl::Duration duration_to_best() const {
    return util_time::DecodeGoogleApiProto(response_.duration_to_best())
        .value();
  }
  const SetCoverSolutionResponse& response() const { return response_; }
  const SubsetBoolVector& solution() const { return solution_; }
  bool is_optimal() const { return response_.is_optimal(); }
  bool is_unicost() const { return response_.is_unicost(); }

 private:
  SetCoverSolutionResponse response_;
  SubsetBoolVector solution_;
};

// This class is used to store the results of multiple runs of solution
// generators, and to compute the geometric mean of the speedups and cost ratios
// when comparing a new solution generator to a reference solution generator.
class RunStats {
 public:
  RunStats() = default;
  RunStats(absl::string_view new_batch_name, absl::string_view ref_batch_name)
      : new_batch_name_(new_batch_name), ref_batch_name_(ref_batch_name) {}
  void Add(const RunResult& new_batch, const RunResult& ref_batch) {
    cost_ratio_sum_of_logs_ += std::log(new_batch.cost() / ref_batch.cost());
    speedup_sum_of_logs_ += std::log(absl::FDivDuration(
        ref_batch.total_duration(), new_batch.total_duration()));
    ++count_;
  }

  // Returns the geometric mean of the cost ratios.
  double GetGeoMeanCostRatio() const {
    return std::exp(cost_ratio_sum_of_logs_ / count_);
  }

  // Returns the geometric mean of the speedups.
  double GetGeoMeanSpeedup() const {
    return std::exp(speedup_sum_of_logs_ / count_);
  }

  absl::string_view new_batch_name() const { return new_batch_name_; }
  absl::string_view ref_batch_name() const { return ref_batch_name_; }

 private:
  // Names of the new and reference solution generators, used to print the
  // results in a readable format.
  std::string new_batch_name_;
  std::string ref_batch_name_;

  // Sum of the logarithms of the speedups and cost ratios, to compute the
  // geometric mean by taking the exponential in GetGeoMeanSpeedup/CostRatio.
  double speedup_sum_of_logs_ = 0.0;
  double cost_ratio_sum_of_logs_ = 0.0;

  // Number of runs added to the stats, used to compute the geometric mean.
  int count_ = 0;
};

// Formatting and reporting functions with LaTeX and CSV support.
class Report {
 public:
  Report(bool csv, bool latex) : csv_(csv), latex_(latex) {
    QCHECK(!(csv && latex)) << "csv and latex are mutually exclusive";
    sep_ = ::operations_research::Separator(csv_, latex_);
    eol_ = ::operations_research::Eol(csv_, latex_);
  }

  // Returns the separator string.
  std::string Separator() const { return sep_; }

  // Returns the end of line string.
  std::string Eol() const { return eol_; }

  // Reports the name and sizes of the model in a string.
  void ReportModelNameAndSizes(const SetCoverModel& model);

  // Reports the gap between two solutions in a string. Returns the result as a
  // RunResult for further use.
  RunResult ReportGap(const RunResult& new_run, const RunResult& ref_run);

  // Reports the result of a run.
  void ReportRunResult(const RunResult& result);

  // Reports the stats of multiple runs collected in a RunStats object.
  void ReportRunStats(const RunStats& stats);

  // Formats the model and stats in LaTeX or CSV format.
  void ReportModelStats(const SetCoverModel& model);

  // Logs stats about the model to LOG(INFO).
  void LogStats(const SetCoverModel& model) const;

  // Sets the name of the model to the filename, with a * suffix if the model is
  // unicost. Removes the .txt suffix from the filename if any.
  void SetModelName(absl::string_view filename, bool unicost,
                    SetCoverModel* model) const;

  template <typename... Args>
  void StrAppend(const Args&... args) {
    absl::StrAppend(&output_, args...);
  }

  const std::string& output() const { return output_; }

  void Clear() { output_.clear(); }

 private:
  // In the case of LaTeX, the stats are printed in the format:
  //   & 123.456 (123) ± 12.34 (12) & [123, 456]  corresponding to
  //   & mean (median) ± stddev (iqr) & [min, max]
  // In the case of CSV, the stats are printed as a VerboseString.
  std::string FormatStats(const SetCoverModel::Stats& stats) const;

  // Formats time in microseconds for LaTeX. For CSV, it is formatted as
  // seconds by adding the suffix "e-6, s".
  std::string FormatTime(int64_t time_us) const;

  // Formats the cost and time, with cost in bold if the condition is true.
  std::string FormatCostAndTimeIf(bool condition, double cost,
                                  int64_t time_us) const;

  // Formats the speedup with one decimal place.
  // TODO(user): Bolden the speedup if it is better than 1.0.
  std::string FormattedSpeedup(const RunResult& new_result,
                               const RunResult& ref_result) const;

  // Computes the ratio of the cost of the new solution generator to the cost of
  // the reference solution generator.
  double CostRatio(const RunResult& new_result,
                   const RunResult& ref_result) const;

  // Computes the speedup of the new solution generator compared to the
  // reference solution generator, where the speedup is defined as the ratio of
  // the cumulated time of the reference solution generator to the cumulated
  // time of the new solution generator.
  double Speedup(const RunResult& new_result,
                 const RunResult& ref_result) const;

  // Returns the string str in LaTeX bold if condition is true and --latex is
  // true.
  std::string BoldIf(bool condition, absl::string_view str) const;

  // The output string to be printed.
  std::string output_;

  // Whether the output is in CSV format.
  bool csv_ = false;

  // Whether the output is in LaTeX format.
  bool latex_ = false;

  // The separator string for CSV or LaTeX format or space otherwise.
  std::string sep_;

  // The end of line string for CSV or LaTeX format or newline otherwise.
  std::string eol_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_REPORTING_H_
