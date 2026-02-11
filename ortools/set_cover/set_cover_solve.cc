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

#include <cmath>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "absl/base/macros.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/base/init_google.h"
#include "ortools/base/timer.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/formatting_helper.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_lagrangian.h"
#include "ortools/set_cover/set_cover_mip.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_reader.h"

// Example usages:
//
// Solve all the problems in the benchmarks directory and produce LaTeX output.
// Run the classic algorithms on problem with up to 100,000 elements. Display
// summaries (with geomean ratios) for each group of problems.
/* Copy-paste to a terminal:
    set_cover_solve --benchmarks --benchmarks_dir ~/set_covering_benchmarks \
    --max_elements_for_classic 100000 --solve --latex --summarize
*/
// Generate a new model from the rail4284 problem, with 100,000 elements and
// 1,000,000,000 subsets, with row_scale = 1.1, column_scale = 1.1, and
// cost_scale = 10.0:
/* Copy-paste to a terminal:
    set_cover_solve --input ~/set_covering_benchmarks/orlib/rail4284.txt
    --input_fmt rail  --output ~/rail4284_1B.txt  --output_fmt orlibrail \
    --num_elements_wanted 100000 --num_subsets_wanted 100000000 \
    --cost_scale 10.0 --row_scale 1.1  --column_scale 1.1 --generate
*/
// Display statistics about rail4284_1B.txt:
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
ABSL_FLAG(bool, full_run, false, "Run all the solvers sequentially.");
ABSL_FLAG(double, lp_time_limit_seconds, 3.0, "Time limit for LP solvers.");
ABSL_FLAG(double, mip_time_limit_seconds, 3.0, "Time limit for MIP solvers.");
ABSL_FLAG(int, num_lagrangian_threads, 8,
          "Number of threads to use for lagrangian computations.");
ABSL_FLAG(int, num_random_lazy_element_degree_runs, 50,
          "Number of runs of the randomized lazy element degree generator.");
ABSL_FLAG(int, num_random_dual_ascent_passes, 50,
          "Number of passes for random dual ascent lower bound.");
// TODO(user): Add flags to:
// - Choose problems by name or by size: filter_name, max_elements, max_subsets.
// - Exclude problems by name: exclude_name.
// - Choose which solution generators to run.
// - Parameterize the number of threads. num_threads.

namespace operations_research {
using CL = SetCoverInvariant::ConsistencyLevel;

namespace {
void ComputeAndLogDualAscentLB(const SetCoverInvariant& inv) {
  WallTimer timer;
  timer.Start();
  const Cost random_based_lb = ComputeDualAscentLB(
      inv, absl::GetFlag(FLAGS_num_random_dual_ascent_passes));
  timer.Stop();
  std::cout << "DualAscentLB ("
            << absl::GetFlag(FLAGS_num_random_dual_ascent_passes)
            << " passes): " << random_based_lb << " computed in "
            << timer.GetDuration() << "\n";
}

void ComputeAndLogDegreeBasedDualAscentLB(const SetCoverInvariant& inv) {
  WallTimer timer;
  timer.Start();
  const Cost lower_bound = ComputeDegreeBasedDualAscentLB(
      inv, absl::GetFlag(FLAGS_num_random_dual_ascent_passes));
  timer.Stop();
  std::cout << "DegreeBasedDualAscentLB ("
            << absl::GetFlag(FLAGS_num_random_dual_ascent_passes)
            << " passes): " << lower_bound << " computed in "
            << timer.GetDuration() << "\n";
}
}  // namespace

// In the case of a MIP, do not assume that gen.NextSolution() returns true,
// because it can time out.
SetCoverInvariant& Run(SetCoverMip& gen) {
  if (!gen.NextSolution()) {
    LOG(INFO) << "Failed to generate a solution. Status: "
              << gen.solve_status();
  }
  LogCostAndTiming(gen);
  return *gen.inv();
}

SetCoverInvariant& Run(SetCoverSolutionGenerator& gen) {
  CHECK(gen.NextSolution());
  LogCostAndTiming(gen);
  return *gen.inv();
}

// Runs two solution generators in parallel, and returns the invariant of the
// first one.
// Do not use this function if one of the solution generators is an LP/MIP
// solver.
// TODO(user): Look at whether it is interesting to combine MIP and other
// heuristics.
SetCoverInvariant& RunPair(SetCoverSolutionGenerator& gen1,
                           SetCoverSolutionGenerator& gen2) {
  CHECK_EQ(gen1.inv(), gen2.inv());
  CHECK_NE(gen1.class_name(), "Mip");
  CHECK_NE(gen2.class_name(), "Mip");
  SetCoverInvariant& inv = *gen1.inv();
  inv.Clear();
  CHECK(gen1.NextSolution());
  // CHECK the invariant consistency for the gen1 consistency level.
  DCHECK(gen1.CheckInvariantConsistency());
  LogCostAndTiming(gen1);
  CHECK(gen2.NextSolution());
  LogCostAndTiming(gen2);
  // CHECK the invariant consistency for the gen2 consistency level, which may
  // be different from the gen1 consistency level.
  DCHECK(gen2.CheckInvariantConsistency());
  return inv;
}

void ComputeLagrangianLowerBound(SetCoverInvariant* inv) {
  const SetCoverModel& model = *inv->model();
  SetCoverLagrangian lagrangian(inv, "LagrangianLowerBound");
  lagrangian.UseNumThreads(absl::GetFlag(FLAGS_num_lagrangian_threads));
  const auto [lower_bound, reduced_costs, multipliers] =
      lagrangian.ComputeLowerBound(model.subset_costs(), inv->cost());
  LogCostAndTiming(lagrangian);
}

double RunSolvers() {
  const auto& input = absl::GetFlag(FLAGS_input);
  const auto& input_format = ParseFileFormat(absl::GetFlag(FLAGS_input_fmt));
  QCHECK(!input.empty()) << "No input file specified.";
  QCHECK(input.empty() || input_format != SetCoverFormat::EMPTY)
      << "Input format cannot be empty.";

  SetCoverModel model = ReadModel(input, absl::GetFlag(FLAGS_input_fmt));
  LogStats(model);
  SetCoverInvariant inv(&model);

  GreedySolutionGenerator greedy(&inv);
  ElementDegreeSolutionGenerator element_degree(&inv);
  LazyElementDegreeSolutionGenerator lazy_element_degree(&inv);

  SteepestSearch steepest(&inv);
  LazySteepestSearch lazy_steepest(&inv);

  GuidedLocalSearch gls(&inv);

  SetCoverMip lower_bound_lp(&inv, "LPLowerBound");
  lower_bound_lp.UseMipSolver(SetCoverMipSolver::SCIP).UseIntegers(false);
  lower_bound_lp.SetTimeLimitInSeconds(
      absl::GetFlag(FLAGS_lp_time_limit_seconds));

  SetCoverMip mip(&inv);
  // Use SCIP by default, prefer Gurobi for large pbs.
  mip.UseMipSolver(SetCoverMipSolver::SCIP).UseIntegers(true);
  mip.SetTimeLimitInSeconds(absl::GetFlag(
      FLAGS_mip_time_limit_seconds));  // Use >300s large problems.

  RunPair(greedy, steepest);
  RunPair(greedy, lazy_steepest);
  RunPair(element_degree, steepest);
  RunPair(element_degree, lazy_steepest);
  RunPair(lazy_element_degree, steepest);
  RunPair(lazy_element_degree, lazy_steepest);

  ComputeAndLogDualAscentLB(inv);
  ComputeAndLogDegreeBasedDualAscentLB(inv);

  ComputeLagrangianLowerBound(&inv);
  Run(lower_bound_lp);
  Run(mip);

  // IterateClearAndMip(name, inv);
  // ElementDegreeGeneratorRandomClearSteepestIterate(name, &inv);
  return inv.cost();
}

namespace {
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
    std::tuple<std::string, std::vector<std::string>, SetCoverFormat>;

std::vector<BenchmarksTableRow> BenchmarksTable() {
// This creates a vector of tuples, where each tuple contains the directory
// name, the vector of files and the file format.
// It is assumed that the scp* files are in BENCHMARKS_DIR/orlib, the rail
// files are in BENCHMARKS_DIR/rail, etc., with BENCHMARKS_DIR being the
// directory specified by the --benchmarks_dir flag.
// Use a macro to be able to compute the size of the array at compile time.
#define BUILD_VECTOR(files) BuildVector(files, ABSL_ARRAYSIZE(files))
#define APPEND(v, array) v.insert(v.end(), array, array + ABSL_ARRAYSIZE(array))
  std::vector<BenchmarksTableRow> result;
  std::vector<std::string> all_scp_files;
  if (absl::GetFlag(FLAGS_collate_scp)) {
    all_scp_files = BUILD_VECTOR(kScp4To6Files);
    APPEND(all_scp_files, kScpAToEFiles);
    APPEND(all_scp_files, kScpNrFiles);
    APPEND(all_scp_files, kScpClrFiles);
    result = {{"orlib", all_scp_files, SetCoverFormat::ORLIB}};
  } else {
    result = {
        {"orlib", BUILD_VECTOR(kScp4To6Files), SetCoverFormat::ORLIB},
        {"orlib", BUILD_VECTOR(kScpAToEFiles), SetCoverFormat::ORLIB},
        {"orlib", BUILD_VECTOR(kScpNrFiles), SetCoverFormat::ORLIB},
        {"orlib", BUILD_VECTOR(kScpClrFiles), SetCoverFormat::ORLIB},
        {"orlib", BUILD_VECTOR(kScpCycFiles), SetCoverFormat::ORLIB},
    };
  }
  result.insert(
      result.end(),
      {
          {"rail", BUILD_VECTOR(kRailFiles), SetCoverFormat::RAIL},
          {"wedelin", BUILD_VECTOR(kWedelinFiles), SetCoverFormat::ORLIB},
          {"balas", BUILD_VECTOR(kBalasFiles), SetCoverFormat::ORLIB},
          {"fimi", BUILD_VECTOR(kFimiFiles), SetCoverFormat::FIMI},
      });
  return result;

#undef BUILD_VECTOR
#undef APPEND
}

void Benchmarks() {
  QCHECK(!absl::GetFlag(FLAGS_benchmarks_dir).empty())
      << "Benchmarks directory must be specified.";
  const std::vector<BenchmarksTableRow> kBenchmarks = BenchmarksTable();
  const std::string kSep = Separator();
  const std::string kEol = Eol();
  if (absl::GetFlag(FLAGS_stats)) {
    std::cout << absl::StrJoin({"Problem", "|S|", "|U|", "nnz", "Fill",
                                "Col size", "Row size"},
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
    GeometricMean lazy_greedy_vs_classic_speedup_geomean;
    GeometricMean lazy_greedy_vs_classic_cost_ratio_geomean;
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
      ComputeAndLogDualAscentLB(classic_inv);
      ComputeAndLogDegreeBasedDualAscentLB(classic_inv);
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

      SetCoverInvariant lazy_random_inv(&model);
      LazyElementDegreeSolutionGenerator lazy_random(&lazy_random_inv);
      lazy_random.SetNumRandomPasses(
          absl::GetFlag(FLAGS_num_random_lazy_element_degree_runs));
      CHECK(lazy_random.NextSolution());
      DCHECK(lazy_random_inv.CheckConsistency(CL::kCostAndCoverage));
      Cost lazy_random_cost = lazy_random_inv.cost();
      Cost lazy_element_degree_cost = lazy_inv.cost();
      if (lazy_random_cost < lazy_element_degree_cost) {
        std::cout
            << "Lazy random cost is less than lazy element degree cost by: "
            << (1 - lazy_random_cost / lazy_element_degree_cost) * 100
            << "% : " << lazy_random_cost << " < " << lazy_element_degree_cost
            << kEol;
      }
      absl::StrAppend(&output, ReportBothParts(classic, lazy_random));

      LazySteepestSearch lazy_steepest(&lazy_inv);
      lazy_steepest.NextSolution();
      absl::StrAppend(&output, ReportBothParts(steepest, lazy_steepest));

      SetCoverInvariant lazy_greedy_inv(&model);
      LazyGreedySolutionGenerator lazy_greedy(&lazy_greedy_inv);
      lazy_greedy.NextSolution();
      absl::StrAppend(&output, ReportBothParts(classic, lazy_greedy));

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
        lazy_greedy_vs_classic_speedup_geomean.Add(
            Speedup(classic, lazy_greedy));
        lazy_greedy_vs_classic_cost_ratio_geomean.Add(
            CostRatio(classic, lazy_greedy));
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
          << "Lazy Greedy / Classic speedup geometric mean: "
          << lazy_greedy_vs_classic_speedup_geomean.Get() << kEol
          << "Lazy Greedy / Classic cost ratio geometric mean: "
          << lazy_greedy_vs_classic_cost_ratio_geomean.Get() << kEol
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
          << combined_speedup_geomean.Get() << kEol
          << "Improvement lazy greedy / classic speedup geometric mean: "
          << lazy_greedy_vs_classic_speedup_geomean.Get() << kEol;
      if (absl::GetFlag(FLAGS_tlns)) {
        std::cout << "TLNS cost ratio geometric mean: "
                  << tlns_cost_ratio_geomean.Get() << kEol;
      }
    }
  }
}

void Run() {
  const std::string input = absl::GetFlag(FLAGS_input);
  const SetCoverFormat input_format =
      ParseFileFormat(absl::GetFlag(FLAGS_input_fmt));
  const std::string output = absl::GetFlag(FLAGS_output);
  const SetCoverFormat output_format =
      ParseFileFormat(absl::GetFlag(FLAGS_output_fmt));
  QCHECK(!input.empty()) << "No input file specified.";
  QCHECK(input.empty() || input_format != SetCoverFormat::EMPTY)
      << "Input format cannot be empty.";
  QCHECK(output.empty() || output_format != SetCoverFormat::EMPTY)
      << "Output format cannot be empty.";
  SetCoverModel model = ReadModel(input, absl::GetFlag(FLAGS_input_fmt));
  if (absl::GetFlag(FLAGS_generate)) {
    model.CreateSparseRowView();
    model = SetCoverModel::GenerateRandomModelFrom(
        model, absl::GetFlag(FLAGS_num_elements_wanted),
        absl::GetFlag(FLAGS_num_subsets_wanted), absl::GetFlag(FLAGS_row_scale),
        absl::GetFlag(FLAGS_column_scale), absl::GetFlag(FLAGS_cost_scale));
  }
  if (!output.empty()) {
    if (output_format == SetCoverFormat::ORLIB) {
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
    SetCoverInvariant inv(&model);
    LazyElementDegreeSolutionGenerator lazy_element_degree(&inv);
    Run(lazy_element_degree);
    for (int i = 0;
         i < absl::GetFlag(FLAGS_num_random_lazy_element_degree_runs); ++i) {
      SetCoverInvariant inv(&model);
      LazyElementDegreeSolutionGenerator randomized_lazy_element_degree(&inv);
      randomized_lazy_element_degree.SetNumRandomPasses(100);
      LazySteepestSearch lazy_steepest(&inv);
      RunPair(randomized_lazy_element_degree, lazy_steepest);
    }
  }
  // TODO(user): Add flags to select which solvers to run.
  // TODO(user): Add flag to output the solution, either as csv or as text
  // proto.
}
}  // namespace operations_research

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  if (absl::GetFlag(FLAGS_benchmarks)) {
    operations_research::Benchmarks();
  } else if (absl::GetFlag(FLAGS_full_run)) {
    operations_research::RunSolvers();
  } else {
    operations_research::Run();
  }
  return 0;
}
