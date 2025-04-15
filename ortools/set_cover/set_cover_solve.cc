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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "absl/base/macros.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
#include "ortools/base/timer.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_reader.h"

// Example usages:
//
// Solve all the problems in the benchmarks directory and produce LaTeX output.
// Run the classic algorithms on problem with up to 100,000 elements. Display
// summaries (with geomean ratios) for each group of problems.
/* Copy-paste to a terminal:
    set_cover_solve --benchmarks --benchmarks_dir ~/benchmarks \
    --max_elements_for_classic 100000 --solve --latex --summarize
*/
// Generate a new model from the rail4284 problem, with 100,000 elements and
// 1,000,000,000 subsets, with row_scale = 1.1, column_scale = 1.1, and
// cost_scale = 10.0:
/* Copy-paste to a terminal:
    set_cover_solve --input ~/scp-orlib/rail4284.txt --input_fmt rail \
    --output ~/rail4284_1B.txt  --output_fmt orlibrail \
    --num_elements_wanted 100000 --num_subsets_wanted 100000000 \
    --cost_scale 10.0 --row_scale 1.1  --column_scale 1.1 --generate
*/
// Display statistics about trail4284_1B.txt:
/* Copy-paste to a terminal:
    set_cover_solve --input ~/rail4284_1B.txt --input_fmt orlib --stats
*/
//

ABSL_FLAG(std::string, input, "", "REQUIRED: Input file name.");
ABSL_FLAG(std::string, input_fmt, "",
          "REQUIRED: Input file format. Either proto, proto_bin, rail, "
          "orlib or fimi.");

ABSL_FLAG(std::string, output, "",
          "If non-empty, write the returned solution to the given file.");
ABSL_FLAG(std::string, output_fmt, "",
          "If out is non-empty, use the given format for the output.");

ABSL_FLAG(bool, generate, false, "Generate a new model from the input model.");
ABSL_FLAG(int, num_elements_wanted, 0,
          "Number of elements wanted in the new generated model.");
ABSL_FLAG(int, num_subsets_wanted, 0,
          "Number of subsets wanted in the new generated model.");
ABSL_FLAG(float, row_scale, 1.0, "Row scale for the new generated model.");
ABSL_FLAG(float, column_scale, 1.0,
          "Column scale for the new generated model.");
ABSL_FLAG(float, cost_scale, 1.0, "Cost scale for the new generated model.");

ABSL_FLAG(bool, unicost, false, "Set all costs to 1.0.");

ABSL_FLAG(bool, latex, false, "Output in LaTeX format. CSV otherwise.");

ABSL_FLAG(bool, solve, false, "Solve the model.");
ABSL_FLAG(bool, stats, false, "Log stats about the model.");
ABSL_FLAG(bool, summarize, false,
          "Display the comparison of the solution generators.");
ABSL_FLAG(bool, tlns, false, "Run thrifty LNS.");
ABSL_FLAG(int, max_elements_for_classic, 5000,
          "Do not use classic on larger problems.");

ABSL_FLAG(bool, benchmarks, false, "Run benchmarks.");
ABSL_FLAG(std::string, benchmarks_dir, "", "Benchmarks directory.");
ABSL_FLAG(bool, collate_scp, false, "Collate the SCP benchmarks.");

// TODO(user): Add flags to:
// - Choose problems by name or by size: filter_name, max_elements, max_subsets.
// - Exclude problems by name: exclude_name.
// - Choose which solution generators to run.
// - Parameterize the number of threads. num_threads.

namespace operations_research {
using CL = SetCoverInvariant::ConsistencyLevel;

int64_t RunTimeInMicroseconds(const SetCoverSolutionGenerator& gen) {
  return absl::ToInt64Microseconds(gen.run_time());
}

int64_t RunTimeInNanoseconds(const SetCoverSolutionGenerator& gen) {
  return absl::ToInt64Nanoseconds(gen.run_time());
}

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
                      absl::string_view alg_name, const SetCoverInvariant& inv,
                      int64_t run_time) {
  LOG(INFO) << ", " << problem_name << ", " << alg_name << ", cost, "
            << inv.CostOrLowerBound() << ", solution_cardinality, "
            << inv.ComputeCardinality() << ", " << run_time << "e-6, s";
}

void LogCostAndTiming(const SetCoverSolutionGenerator& generator) {
  const SetCoverInvariant& inv = *generator.inv();
  const SetCoverModel& model = *inv.model();
  LogCostAndTiming(model.name(), generator.name(), inv,
                   RunTimeInMicroseconds(generator));
}

// TODO(user): Move this set_cover_reader
enum class FileFormat {
  EMPTY,
  ORLIB,
  RAIL,
  FIMI,
  PROTO,
  PROTO_BIN,
  TXT,
};

// TODO(user): Move this set_cover_reader
FileFormat ParseFileFormat(const std::string& format_name) {
  if (format_name.empty()) {
    return FileFormat::EMPTY;
  } else if (absl::EqualsIgnoreCase(format_name, "orlib")) {
    return FileFormat::ORLIB;
  } else if (absl::EqualsIgnoreCase(format_name, "rail")) {
    return FileFormat::RAIL;
  } else if (absl::EqualsIgnoreCase(format_name, "fimi")) {
    return FileFormat::FIMI;
  } else if (absl::EqualsIgnoreCase(format_name, "proto")) {
    return FileFormat::PROTO;
  } else if (absl::EqualsIgnoreCase(format_name, "proto_bin")) {
    return FileFormat::PROTO_BIN;
  } else if (absl::EqualsIgnoreCase(format_name, "txt")) {
    return FileFormat::TXT;
  } else {
    LOG(FATAL) << "Unsupported input format: " << format_name;
  }
}

// TODO(user): Move this set_cover_reader
SetCoverModel ReadModel(absl::string_view filename, FileFormat format) {
  switch (format) {
    case FileFormat::ORLIB:
      return ReadOrlibScp(filename);
    case FileFormat::RAIL:
      return ReadOrlibRail(filename);
    case FileFormat::FIMI:
      return ReadFimiDat(filename);
    case FileFormat::PROTO:
      return ReadSetCoverProto(filename, /*binary=*/false);
    case FileFormat::PROTO_BIN:
      return ReadSetCoverProto(filename, /*binary=*/true);
    default:
      LOG(FATAL) << "Unsupported input format: " << static_cast<int>(format);
  }
}

// TODO(user): Move this set_cover_reader
SubsetBoolVector ReadSolution(absl::string_view filename, FileFormat format) {
  switch (format) {
    case FileFormat::TXT:
      return ReadSetCoverSolutionText(filename);
    case FileFormat::PROTO:
      return ReadSetCoverSolutionProto(filename, /*binary=*/false);
    case FileFormat::PROTO_BIN:
      return ReadSetCoverSolutionProto(filename, /*binary=*/true);
    default:
      LOG(FATAL) << "Unsupported input format: " << static_cast<int>(format);
  }
}

// TODO(user): Move this set_cover_reader
void WriteModel(const SetCoverModel& model, const std::string& filename,
                FileFormat format) {
  LOG(INFO) << "Writing model to " << filename;
  switch (format) {
    case FileFormat::ORLIB:
      WriteOrlibScp(model, filename);
      break;
    case FileFormat::RAIL:
      WriteOrlibRail(model, filename);
      break;
    case FileFormat::PROTO:
      WriteSetCoverProto(model, filename, /*binary=*/false);
      break;
    case FileFormat::PROTO_BIN:
      WriteSetCoverProto(model, filename, /*binary=*/true);
      break;
    default:
      LOG(FATAL) << "Unsupported output format: " << static_cast<int>(format);
  }
}

// TODO(user): Move this set_cover_reader
void WriteSolution(const SetCoverModel& model, const SubsetBoolVector& solution,
                   absl::string_view filename, FileFormat format) {
  switch (format) {
    case FileFormat::TXT:
      WriteSetCoverSolutionText(model, solution, filename);
      break;
    case FileFormat::PROTO:
      WriteSetCoverSolutionProto(model, solution, filename,
                                 /*binary=*/false);
      break;
    case FileFormat::PROTO_BIN:
      WriteSetCoverSolutionProto(model, solution, filename,
                                 /*binary=*/true);
      break;
    default:
      LOG(FATAL) << "Unsupported output format: " << static_cast<int>(format);
  }
}

SetCoverInvariant RunLazyElementDegree(SetCoverModel* model) {
  SetCoverInvariant inv(model);
  LazyElementDegreeSolutionGenerator element_degree(&inv);
  CHECK(element_degree.NextSolution());
  DCHECK(inv.CheckConsistency(CL::kCostAndCoverage));
  LogCostAndTiming(element_degree);
  return inv;
}

SetCoverInvariant RunGreedy(SetCoverModel* model) {
  SetCoverInvariant inv(model);
  GreedySolutionGenerator classic(&inv);
  CHECK(classic.NextSolution());
  DCHECK(inv.CheckConsistency(CL::kCostAndCoverage));
  LogCostAndTiming(classic);
  return inv;
}

// Formatting and reporting functions with LaTeX and CSV support.
namespace {
std::string Separator() { return absl::GetFlag(FLAGS_latex) ? " & " : ", "; }

std::string Eol() { return absl::GetFlag(FLAGS_latex) ? " \\\\\n" : "\n"; }

// Computes the ratio of the cost of the new solution generator to the cost of
// the reference solution generator.
double CostRatio(SetCoverSolutionGenerator& ref_gen,
                 SetCoverSolutionGenerator& new_gen) {
  return new_gen.cost() / ref_gen.cost();
}

// Computes the speedup of the new solution generator compared to the reference
// solution generator, where the speedup is defined as the ratio of the
// cumulated time of the reference solution generator to the cumulated time of
// the new solution generator.
double Speedup(SetCoverSolutionGenerator& ref_gen,
               SetCoverSolutionGenerator& new_gen) {
  // Avoid division by zero by considering the case where the new generator took
  // less than 1 nanosecond (!) to run.
  return 1.0 * RunTimeInNanoseconds(ref_gen) /
         std::max<int64_t>(1, RunTimeInNanoseconds(new_gen));
}

// Same as above, but the cumulated time is the sum of the initialization and
// search times for two pairs of solution generators.
double SpeedupOnCumulatedTimes(SetCoverSolutionGenerator& ref_init,
                               SetCoverSolutionGenerator& ref_search,
                               SetCoverSolutionGenerator& new_init,
                               SetCoverSolutionGenerator& new_search) {
  return 1.0 *
         (RunTimeInNanoseconds(ref_init) + RunTimeInNanoseconds(ref_search)) /
         std::max<int64_t>(1, RunTimeInNanoseconds(new_init) +
                                  RunTimeInNanoseconds(new_search));
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

// Formats the speedup with one decimal place.
// TODO(user): Bolden the speedup if it is better than 1.0.
std::string FormattedSpeedup(SetCoverSolutionGenerator& ref_gen,
                             SetCoverSolutionGenerator& new_gen) {
  return absl::StrFormat("%.1f", Speedup(ref_gen, new_gen));
}

// Reports the second of the comparison of two solution generators, with only
// the cost and time of the new solution generator.
std::string ReportSecondPart(SetCoverSolutionGenerator& ref_gen,
                             SetCoverSolutionGenerator& new_gen) {
  const double ref_cost = ref_gen.cost();
  const double new_cost = new_gen.cost();
  return absl::StrCat(Separator(),
                      FormatCostAndTimeIf(new_cost <= ref_cost, new_cost,
                                          RunTimeInMicroseconds(new_gen)),
                      Separator(), FormattedSpeedup(ref_gen, new_gen));
}

// Reports the cost and time of both solution generators.
std::string ReportBothParts(SetCoverSolutionGenerator& ref_gen,
                            SetCoverSolutionGenerator& new_gen) {
  const double ref_cost = ref_gen.cost();
  const double new_cost = new_gen.cost();
  return absl::StrCat(Separator(),
                      FormatCostAndTimeIf(ref_cost <= new_cost, ref_cost,
                                          RunTimeInMicroseconds(ref_gen)),
                      ReportSecondPart(ref_gen, new_gen));
}

// Formats the model and stats in LaTeX or CSV format.
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

// Sets the name of the model to the filename, with a * suffix if the model is
// unicost. Removes the .txt suffix from the filename if any.
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

// Accumulates data to compute the geometric mean of a sequence of values.
class GeometricMean {
 public:
  GeometricMean() : sum_(0.0), count_(0) {}
  void Add(double value) {
    sum_ += std::log(value);
    ++count_;
  }
  double Get() const { return std::exp(sum_ / count_); }

 private:
  double sum_;
  int count_;
};

std::vector<std::string> BuildVector(const char* const files[], int size) {
  return std::vector<std::string>(files, files + size);
}
}  // namespace

// List all the files from the literature.
static const char* const kRailFiles[] = {
    "rail507.txt",  "rail516.txt",  "rail582.txt",  "rail2536.txt",
    "rail2586.txt", "rail4284.txt", "rail4872.txt",
};

static const char* const kScp4To6Files[] = {
    "scp41.txt", "scp42.txt", "scp43.txt", "scp44.txt", "scp45.txt",
    "scp46.txt", "scp47.txt", "scp48.txt", "scp49.txt", "scp410.txt",
    "scp51.txt", "scp52.txt", "scp53.txt", "scp54.txt", "scp55.txt",
    "scp56.txt", "scp57.txt", "scp58.txt", "scp59.txt", "scp510.txt",
    "scp61.txt", "scp62.txt", "scp63.txt", "scp64.txt", "scp65.txt",
};

static const char* const kScpAToEFiles[] = {
    "scpa1.txt", "scpa2.txt", "scpa3.txt", "scpa4.txt", "scpa5.txt",
    "scpb1.txt", "scpb2.txt", "scpb3.txt", "scpb4.txt", "scpb5.txt",
    "scpc1.txt", "scpc2.txt", "scpc3.txt", "scpc4.txt", "scpc5.txt",
    "scpd1.txt", "scpd2.txt", "scpd3.txt", "scpd4.txt", "scpd5.txt",
    "scpe1.txt", "scpe2.txt", "scpe3.txt", "scpe4.txt", "scpe5.txt",
};

static const char* const kScpNrFiles[] = {
    "scpnre1.txt", "scpnre2.txt", "scpnre3.txt", "scpnre4.txt", "scpnre5.txt",
    "scpnrf1.txt", "scpnrf2.txt", "scpnrf3.txt", "scpnrf4.txt", "scpnrf5.txt",
    "scpnrg1.txt", "scpnrg2.txt", "scpnrg3.txt", "scpnrg4.txt", "scpnrg5.txt",
    "scpnrh1.txt", "scpnrh2.txt", "scpnrh3.txt", "scpnrh4.txt", "scpnrh5.txt",
};

static const char* const kScpClrFiles[] = {
    "scpclr10.txt",
    "scpclr11.txt",
    "scpclr12.txt",
    "scpclr13.txt",
};

static const char* const kScpCycFiles[] = {
    "scpcyc06.txt", "scpcyc07.txt", "scpcyc08.txt",
    "scpcyc09.txt", "scpcyc10.txt", "scpcyc11.txt",
};

static const char* const kWedelinFiles[] = {
    "a320_coc.txt", "a320.txt",      "alitalia.txt",
    "b727.txt",     "sasd9imp2.txt", "sasjump.txt",
};

static const char* const kBalasFiles[] = {
    "aa03.txt", "aa04.txt", "aa05.txt", "aa06.txt", "aa11.txt", "aa12.txt",
    "aa13.txt", "aa14.txt", "aa15.txt", "aa16.txt", "aa17.txt", "aa18.txt",
    "aa19.txt", "aa20.txt", "bus1.txt", "bus2.txt",
};

static const char* const kFimiFiles[] = {
    "accidents.dat", "chess.dat",   "connect.dat", "kosarak.dat",
    "mushroom.dat",  // "pumsb.dat", "pumsb_star.dat",
    "retail.dat",    "webdocs.dat",
};

using BenchmarksTableRow =
    std::tuple<std::string, std::vector<std::string>, FileFormat>;

std::vector<BenchmarksTableRow> BenchmarksTable() {
// This creates a vector of tuples, where each tuple contains the directory
// name, the vector of files and the file format.
// It is assumed that the scp* files are in BENCHMARKS_DIR/scp-orlib, the rail
// files are in BENCHMARKS_DIR/scp-rail, etc., with BENCHMARKS_DIR being the
// directory specified by the --benchmarks_dir flag.
// Use a macro to be able to compute the size of the array at compile time.
#define BUILD_VECTOR(files) BuildVector(files, ABSL_ARRAYSIZE(files))
  std::vector<BenchmarksTableRow> result;
  std::vector<std::string> all_scp_files;
  if (absl::GetFlag(FLAGS_collate_scp)) {
    all_scp_files = BUILD_VECTOR(kScp4To6Files);
    all_scp_files.insert(all_scp_files.end(), kScpAToEFiles,
                         kScpAToEFiles + ABSL_ARRAYSIZE(kScpAToEFiles));
    all_scp_files.insert(all_scp_files.end(), kScpNrFiles,
                         kScpNrFiles + ABSL_ARRAYSIZE(kScpNrFiles));
    all_scp_files.insert(all_scp_files.end(), kScpCycFiles,
                         kScpCycFiles + ABSL_ARRAYSIZE(kScpCycFiles));
    result = {{"scp-orlib", all_scp_files, FileFormat::ORLIB}};
  } else {
    result = {
        {"scp-orlib", BUILD_VECTOR(kScp4To6Files), FileFormat::ORLIB},
        {"scp-orlib", BUILD_VECTOR(kScpAToEFiles), FileFormat::ORLIB},
        {"scp-orlib", BUILD_VECTOR(kScpNrFiles), FileFormat::ORLIB},
        {"scp-orlib", BUILD_VECTOR(kScpClrFiles), FileFormat::ORLIB},
        {"scp-orlib", BUILD_VECTOR(kScpCycFiles), FileFormat::ORLIB},
    };
  }
  result.insert(
      result.end(),
      {
          {"scp-rail", BUILD_VECTOR(kRailFiles), FileFormat::RAIL},
          {"scp-wedelin", BUILD_VECTOR(kWedelinFiles), FileFormat::ORLIB},
          {"scp-balas", BUILD_VECTOR(kBalasFiles), FileFormat::ORLIB},
          {"scp-fimi", BUILD_VECTOR(kFimiFiles), FileFormat::FIMI},
      });
  return result;

#undef BUILD_VECTOR
}

void Benchmarks() {
  QCHECK(!absl::GetFlag(FLAGS_benchmarks_dir).empty())
      << "Benchmarks directory must be specified.";
  const std::vector<BenchmarksTableRow> kBenchmarks = BenchmarksTable();
  const std::string kSep = Separator();
  const std::string kEol = Eol();
  if (absl::GetFlag(FLAGS_stats)) {
    std::cout << absl::StrJoin(std::make_tuple("Problem", "|S|", "|U|", "nnz",
                                               "Fill", "Col size", "Row size"),
                               kSep)
              << kEol;
  }
  for (const auto& [dir, files, input_format] : kBenchmarks) {
    GeometricMean element_vs_classic_cost_ratio_geomean;
    GeometricMean element_vs_classic_speedup_geomean;
    GeometricMean lazy_vs_classic_cost_ratio_geomean;
    GeometricMean lazy_vs_classic_speedup_geomean;
    GeometricMean lazy_steepest_vs_steepest_cost_ratio_geomean;
    GeometricMean lazy_steepest_vs_steepest_speedup_geomean;
    GeometricMean combined_cost_ratio_geomean;
    GeometricMean combined_speedup_geomean;
    GeometricMean tlns_cost_ratio_geomean;
    for (const std::string& filename : files) {
      const std::string filespec = absl::StrCat(
          absl::GetFlag(FLAGS_benchmarks_dir), "/", dir, "/", filename);

      LOG(INFO) << "Reading " << filespec;
      SetCoverModel model = ReadModel(filespec, input_format);
      if (absl::GetFlag(FLAGS_unicost)) {
        for (const SubsetIndex subset : model.SubsetRange()) {
          model.SetSubsetCost(subset, 1.0);
        }
      }
      SetModelName(filename, &model);
      if (absl::GetFlag(FLAGS_stats)) {
        std::cout << FormatModelAndStats(model) << kEol;
      }
      if (!absl::GetFlag(FLAGS_solve)) continue;
      LOG(INFO) << "Solving " << model.name();
      std::string output =
          absl::StrJoin(std::make_tuple(model.name(), model.num_subsets(),
                                        model.num_elements()),
                        kSep);
      model.CreateSparseRowView();

      SetCoverInvariant classic_inv(&model);
      GreedySolutionGenerator classic(&classic_inv);
      SteepestSearch steepest(&classic_inv);
      const bool run_classic_solvers =
          model.num_elements() <= absl::GetFlag(FLAGS_max_elements_for_classic);
      if (run_classic_solvers) {
        CHECK(classic.NextSolution());
        CHECK(steepest.NextSolution());
        DCHECK(classic_inv.CheckConsistency(CL::kCostAndCoverage));
      }

      SetCoverInvariant element_degree_inv(&model);
      ElementDegreeSolutionGenerator element_degree(&element_degree_inv);
      CHECK(element_degree.NextSolution());
      DCHECK(element_degree_inv.CheckConsistency(CL::kCostAndCoverage));

      absl::StrAppend(&output, ReportBothParts(classic, element_degree));

      SetCoverInvariant lazy_inv(&model);
      LazyElementDegreeSolutionGenerator lazy_element_degree(&lazy_inv);
      CHECK(lazy_element_degree.NextSolution());
      DCHECK(lazy_inv.CheckConsistency(CL::kCostAndCoverage));

      absl::StrAppend(&output, ReportBothParts(classic, lazy_element_degree));

      LazySteepestSearch lazy_steepest(&lazy_inv);
      lazy_steepest.NextSolution();
      absl::StrAppend(&output, ReportBothParts(steepest, lazy_steepest));

      if (run_classic_solvers) {
        element_vs_classic_cost_ratio_geomean.Add(
            CostRatio(classic, element_degree));
        element_vs_classic_speedup_geomean.Add(
            Speedup(classic, element_degree));
        lazy_vs_classic_cost_ratio_geomean.Add(
            CostRatio(classic, lazy_element_degree));
        lazy_vs_classic_speedup_geomean.Add(
            Speedup(classic, lazy_element_degree));
        lazy_steepest_vs_steepest_cost_ratio_geomean.Add(
            CostRatio(steepest, lazy_steepest));
        lazy_steepest_vs_steepest_speedup_geomean.Add(
            Speedup(steepest, lazy_steepest));
        combined_cost_ratio_geomean.Add(CostRatio(classic, lazy_steepest));
        combined_speedup_geomean.Add(SpeedupOnCumulatedTimes(
            classic, steepest, lazy_element_degree, lazy_steepest));
      }

      Cost best_cost = lazy_inv.cost();
      SubsetBoolVector best_assignment = lazy_inv.is_selected();
      if (absl::GetFlag(FLAGS_tlns)) {
        WallTimer timer;
        LazySteepestSearch steepest(&lazy_inv);
        timer.Start();
        for (int i = 0; i < 500; ++i) {
          std::vector<SubsetIndex> range =
              ClearRandomSubsets(0.1 * lazy_inv.trace().size(), &lazy_inv);
          CHECK(lazy_element_degree.NextSolution());
          CHECK(steepest.NextSolution());
          if (lazy_inv.cost() < best_cost) {
            best_cost = lazy_inv.cost();
            best_assignment = lazy_inv.is_selected();
          }
        }
        timer.Stop();
        absl::StrAppend(
            &output, kSep,
            FormatCostAndTimeIf(
                best_cost <= lazy_element_degree.cost() &&
                    best_cost <= classic.cost(),
                best_cost, absl::ToInt64Microseconds(timer.GetDuration())));
        tlns_cost_ratio_geomean.Add(best_cost / classic.cost());
      }
      std::cout << output << kEol;
    }

    if (absl::GetFlag(FLAGS_summarize)) {
      std::cout
          << "Element degree / classic speedup geometric mean: "
          << element_vs_classic_speedup_geomean.Get() << kEol
          << "Element degree / classic cost ratio geometric mean: "
          << element_vs_classic_cost_ratio_geomean.Get() << kEol
          << "Lazy element degree / classic speedup geometric mean: "
          << lazy_vs_classic_speedup_geomean.Get() << kEol
          << "Lazy element degree / classic cost ratio geometric mean: "
          << lazy_vs_classic_cost_ratio_geomean.Get() << kEol
          << "Improvement element degree / classic speedup geometric mean: "
          << lazy_steepest_vs_steepest_speedup_geomean.Get() << kEol
          << "Improvement cost ratio geometric mean: "
          << lazy_steepest_vs_steepest_cost_ratio_geomean.Get() << kEol
          << "Combined speedup geometric mean: "
          << combined_speedup_geomean.Get() << kEol;
      if (absl::GetFlag(FLAGS_tlns)) {
        std::cout << "TLNS cost ratio geometric mean: "
                  << tlns_cost_ratio_geomean.Get() << kEol;
      }
    }
  }
}

void Run() {
  const auto& input = absl::GetFlag(FLAGS_input);
  const auto& input_format = ParseFileFormat(absl::GetFlag(FLAGS_input_fmt));
  const auto& output = absl::GetFlag(FLAGS_output);
  const auto& output_format = ParseFileFormat(absl::GetFlag(FLAGS_output_fmt));
  QCHECK(!input.empty()) << "No input file specified.";
  QCHECK(input.empty() || input_format != FileFormat::EMPTY)
      << "Input format cannot be empty.";
  QCHECK(output.empty() || output_format != FileFormat::EMPTY)
      << "Output format cannot be empty.";
  SetCoverModel model = ReadModel(input, input_format);
  if (absl::GetFlag(FLAGS_generate)) {
    model.CreateSparseRowView();
    model = SetCoverModel::GenerateRandomModelFrom(
        model, absl::GetFlag(FLAGS_num_elements_wanted),
        absl::GetFlag(FLAGS_num_subsets_wanted), absl::GetFlag(FLAGS_row_scale),
        absl::GetFlag(FLAGS_column_scale), absl::GetFlag(FLAGS_cost_scale));
  }
  if (!output.empty()) {
    if (output_format == FileFormat::ORLIB) {
      model.CreateSparseRowView();
    }
    WriteModel(model, output, output_format);
  }
  auto problem = output.empty() ? input : output;
  if (absl::GetFlag(FLAGS_stats)) {
    LogStats(model);
  }
  if (absl::GetFlag(FLAGS_solve)) {
    LOG(INFO) << "Solving " << problem;
    model.CreateSparseRowView();
    SetCoverInvariant inv = RunLazyElementDegree(&model);
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  if (absl::GetFlag(FLAGS_benchmarks)) {
    operations_research::Benchmarks();
  } else {
    operations_research::Run();
  }
  return 0;
}
