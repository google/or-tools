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
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/path.h"
#include "ortools/base/timer.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/reporting.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_lagrangian.h"
#include "ortools/set_cover/set_cover_mip.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_reader.h"

// Example usages:
//
// Solve all the problems in the benchmarks directory and produce LaTeX output.
// Run the classic algorithms (Chvatal's greedy algorithm + greedy descent) on
// problems with up to 100,000 elements. Display summaries (with geomean ratios)
// for each group of problems.
/* Copy-paste to a terminal:
    set_cover_solve --benchmarks --benchmarks_dir ~/set_covering_benchmarks \
    --max_elements_for_chvatal 100000 --solve --latex --summarize
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

// Output format flags.
ABSL_FLAG(bool, csv, false,
          "Output in CSV format, if false and latex is false, output in human "
          "readable format.");
ABSL_FLAG(bool, latex, false,
          "Output in LaTeX format, if false and csv is false, output in human "
          "readable format.");

ABSL_FLAG(bool, solve, false, "Solve the model.");
ABSL_FLAG(bool, stats, false, "Log stats about the model.");
ABSL_FLAG(bool, summarize, false,
          "Display the comparison of the solution generators.");

ABSL_FLAG(bool, thrifty_lns, false, "Run thrifty LNS.");
ABSL_FLAG(
    int, max_elements_for_chvatal, 5000,
    "Do not use Chvatal's (classic greedy) algorithm on larger problems.");

ABSL_FLAG(bool, benchmarks, false, "Run benchmarks.");
ABSL_FLAG(std::string, benchmarks_dir, "", "Benchmarks directory.");
ABSL_FLAG(bool, load_solution, false,
          "Load solutions from the solutions directory.");
ABSL_FLAG(std::string, solutions_dir, "", "Solutions directory.");

ABSL_FLAG(bool, render, false, "Render the problem as a PNG image.");
ABSL_FLAG(std::string, render_dir, "", "Directory to write the PNG images.");

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

ABSL_FLAG(bool, run_element_degree, false,
          "Run ElementDegreeSolutionGenerator.");
ABSL_FLAG(bool, run_lazy_element_degree, false,
          "Run LazyElementDegreeSolutionGenerator.");
ABSL_FLAG(bool, run_randomized_lazy_element_degree, true,
          "Run randomized LazyElementDegreeSolutionGenerator.");
ABSL_FLAG(bool, run_lazy_steepest, false, "Run LazySteepestSearch.");
ABSL_FLAG(bool, run_lazy_steepest_from_random, false,
          "Run LazySteepestSearch from random solution.");
ABSL_FLAG(bool, run_clique_guided, false, "Run CliqueGuidedLNS.");
ABSL_FLAG(bool, run_lower_bounds, false, "Run lower bounds.");
ABSL_FLAG(bool, run_element_based_tree_search, true,
          "Run ElementBasedTreeSearch.");
ABSL_FLAG(bool, run_tree_search, true, "Run TreeSearch.");
ABSL_FLAG(bool, run_lp, true, "Run LP (computes a lower bound).");
ABSL_FLAG(bool, run_mip, false, "Run MIP.");
// TODO(user): Add flags to:
// - Choose problems by name or by size: filter_name, max_elements, max_subsets.
// - Exclude problems by name: exclude_name.
// - Choose which solution generators to run.
// - Parameterize the number of threads. num_threads.

namespace operations_research {
using CL = SetCoverInvariant::ConsistencyLevel;

namespace {

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

std::vector<std::string> BuildVector(const char* const files[], int size) {
  return std::vector<std::string>(files, files + size);
}

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

// Runs a solution generator and reports the results in a string. Returns the
// result as a RunResult for further use.
RunResult RunAndReport(SetCoverSolutionGenerator& gen, Report& report) {
  gen.inv()->Clear();
  CHECK(gen.NextSolution());
  const RunResult result(gen);
  DCHECK(gen.inv()->CheckConsistency(CL::kCostAndCoverage));
  report.ReportRunResult(result);
  return result;
}

// Runs a MIP or LP solver and reports the results in a string. Returns the
// result as a RunResult for further use.
RunResult RunAndReport(SetCoverMip& gen, bool use_integers, Report& report) {
  SetCoverInvariant* inv = gen.inv();
  inv->Clear();
  gen.UseIntegers(use_integers).UseMipSolver(SetCoverMipSolver::SCIP);
  const double time_limit_seconds =
      use_integers ? absl::GetFlag(FLAGS_mip_time_limit_seconds)
                   : absl::GetFlag(FLAGS_lp_time_limit_seconds);
  gen.SetTimeLimit(absl::Seconds(time_limit_seconds));
  if (gen.NextSolution()) {
    const RunResult result(
        inv->model()->name(),
        use_integers ? "SetCoverMIP" : "SetCoverLPLowerBound",
        use_integers ? inv->cost() : inv->LowerBound(),
        inv->ComputeCardinality(), gen.run_time(), inv->is_selected());
    report.ReportRunResult(result);
    return result;
  }
  LOG(INFO) << "SetCoverMip::NextSolution() failed with status: "
            << gen.solve_status();
  return RunResult(gen);
}

// Runs a solution generator starting from the solution in the base_result, and
// reports the results in a string. Returns the result as a RunResult for
// further use.
RunResult RunFromResultAndReport(const RunResult& base_result,
                                 SetCoverSolutionGenerator& gen,
                                 Report& report) {
  SetCoverInvariant* inv = gen.inv();
  inv->LoadSolution(base_result.solution());
  inv->Recompute(CL::kCostAndCoverage);
  CHECK(gen.NextSolution());
  DCHECK(inv->CheckConsistency(CL::kCostAndCoverage));
  const RunResult result(
      base_result.problem_name(),
      absl::StrCat(base_result.algorithm_name(), "+", gen.name()), inv->cost(),
      inv->ComputeCardinality(), base_result.total_duration() + gen.run_time(),
      inv->is_selected());
  report.ReportRunResult(result);
  return result;
}

RunResult RunSolutionLoadAndReport(bool run_load, absl::string_view filename,
                                   SetCoverInvariant& inv, Report& report) {
  if (!run_load) return RunResult();
  const std::string problem_name = inv.model()->name();
  const std::string solution_filename = file::JoinPath(
      absl::GetFlag(FLAGS_solutions_dir), absl::StrCat(problem_name, ".sol"));
  WallTimer timer;
  timer.Start();
  const SubsetBoolVector solution =
      ReadSetCoverSolutionDat(solution_filename, inv.model()->num_subsets());
  inv.LoadSolution(solution);
  inv.Recompute(CL::kFreeAndUncovered);
  const RunResult result(problem_name, "LoadSolution", inv.cost(),
                         inv.ComputeCardinality(), timer.GetDuration(),
                         solution);
  report.ReportRunResult(result);
  return result;
}

// Helper function to run the lagrangian lower bound, that has a similar API to
// the other lower bound functions.
// TODO(user): Make the API uniform, and remove this function.
Cost ComputeLagrangianLowerBound(SetCoverInvariant& inv, int _) {
  const SetCoverModel* model = inv.model();
  SetCoverLagrangian lagrangian(&inv, "LagrangianLowerBound");
  lagrangian.UseNumThreads(absl::GetFlag(FLAGS_num_lagrangian_threads));
  const auto [lower_bound, reduced_costs, multipliers] =
      lagrangian.ComputeLowerBound(model->subset_costs(), inv.cost());
  return lower_bound;
}

// Computes a lower bound using `lb_function`, and reports the results in a
// string. Returns the result as a RunResult for further use.
template <typename Func>
RunResult RunLowerBoundAndReport(bool run_lower_bound, Func lb_function,
                                 absl::string_view algorithm_name,
                                 SetCoverInvariant& inv, Report& report) {
  if (!run_lower_bound) return RunResult();
  WallTimer timer;
  timer.Start();
  const Cost lb =
      lb_function(inv, absl::GetFlag(FLAGS_num_random_dual_ascent_passes));
  timer.Stop();
  const RunResult result(inv.model()->name(), algorithm_name, lb, 0,
                         timer.GetDuration(), SubsetBoolVector());
  report.ReportRunResult(result);
  return result;
}

RunResult RunChvatalAndReport(bool run_chvatal, SetCoverInvariant& inv,
                              Report& report) {
  if (!run_chvatal) return RunResult();
  GreedySolutionGenerator chvatal(&inv);  // Classic greedy (Chvatal)
  return RunAndReport(chvatal, report);
}

RunResult RunElementDegreeAndReport(bool run_element_degree,
                                    SetCoverInvariant& inv, Report& report) {
  if (!run_element_degree) return RunResult();
  ElementDegreeSolutionGenerator element_degree(&inv);
  return RunAndReport(element_degree, report);
}

RunResult RunLazyElementDegreeAndReport(bool run_lazy_element_degree,
                                        SetCoverInvariant& inv,
                                        Report& report) {
  if (!run_lazy_element_degree) return RunResult();
  LazyElementDegreeSolutionGenerator lazy_element_degree(&inv);
  return RunAndReport(lazy_element_degree, report);
}

RunResult RunRandomizedLazyElementDegreeAndReport(
    bool run_randomized_lazy_element_degree, SetCoverInvariant& inv,
    Report& report) {
  if (!run_randomized_lazy_element_degree) return RunResult();
  LazyElementDegreeSolutionGenerator lazy_random(&inv);
  lazy_random.SetName(
      absl::StrCat("LazyElementDegreeGeneratorRandom",
                   absl::GetFlag(FLAGS_num_random_lazy_element_degree_runs)));
  lazy_random.SetNumRandomPasses(
      absl::GetFlag(FLAGS_num_random_lazy_element_degree_runs));
  RunResult lazy_random_result = RunAndReport(lazy_random, report);
  DCHECK(inv.CheckConsistency(CL::kCostAndCoverage));
  return lazy_random_result;
}

RunResult RunLazySteepestAndReport(bool run_lazy_steepest,
                                   const RunResult& lazy_element_degree_result,
                                   SetCoverInvariant& inv, Report& report) {
  if (!run_lazy_steepest) return RunResult();
  LazySteepestSearch lazy_steepest(&inv);
  return RunFromResultAndReport(lazy_element_degree_result, lazy_steepest,
                                report);
}

RunResult RunCliqueGuidedAndReport(bool run_clique_guided,
                                   const RunResult& lazy_random_steepest_result,
                                   SetCoverInvariant& inv, Report& report) {
  if (!run_clique_guided) return RunResult();
  CliqueGuidedLNS clique_guided_lns(&inv);
  clique_guided_lns.SetMaxCliqueSize(1000).SetMaxNumCliques(400).SetTimeLimit(
      absl::Milliseconds(500));
  clique_guided_lns.NextSolution();
  return RunFromResultAndReport(lazy_random_steepest_result, clique_guided_lns,
                                report);
}

RunResult RunElementBasedTreeSearchAndReport(bool run_element_based_tree_search,
                                             SetCoverInvariant& inv,
                                             Report& report) {
  if (!run_element_based_tree_search) return RunResult();
#if 0  // Placeholder for now.
  ElementBasedTreeSearch element_based_tree_search(&inv);
  element_based_tree_search.SetMaxDiscrepancy(4).SetTimeLimit(
      absl::Milliseconds(1000));
  return RunAndReport(element_based_tree_search, report);
#endif
  return RunResult();
}

RunResult RunTreeSearchAndReport(bool run_tree_search, SetCoverInvariant& inv,
                                 Report& report) {
  if (!run_tree_search) return RunResult();
#if 0  // Placeholder for now.
  TreeSearch tree_search(&inv);
  tree_search.SetMaxDiscrepancy(4).SetTimeLimit(absl::Milliseconds(1000));
  return RunAndReport(tree_search, report);
#endif
  return RunResult();
}

RunResult RunMipAndReport(bool run_mip, bool use_integers,
                          SetCoverInvariant& inv, Report& report) {
  if (!run_mip) return RunResult();
  SetCoverMip mip(&inv);
  return RunAndReport(mip, use_integers, report);
}

RunResult RunThriftyLNSAndReport(bool run_thrifty_lns, const RunResult& start,
                                 SetCoverSolutionGenerator& first_sol_gen,
                                 SetCoverSolutionGenerator& improvement_gen,
                                 Report& report) {
  if (!run_thrifty_lns) return RunResult();
  SetCoverInvariant& inv = *first_sol_gen.inv();
  CHECK_EQ(&inv, improvement_gen.inv());

  Cost best_cost = start.cost();
  SubsetBoolVector best_solution = start.solution();

  WallTimer timer;
  timer.Start();
  const double kFractionOfElementsToClear = 0.1;
  const int kMaxNumIterations = 500;
  inv.LoadSolution(start.solution());
  DCHECK(inv.CheckConsistency(CL::kCostAndCoverage));
  for (int i = 0; i < kMaxNumIterations; ++i) {
    // Note: ClearRandomSubsets modifies inv.
    const std::vector<SubsetIndex> range = ClearRandomSubsets(
        kFractionOfElementsToClear * inv.trace().size(), &inv);
    CHECK(first_sol_gen.NextSolution());
    if (inv.cost() < best_cost) {
      best_cost = inv.cost();
      best_solution = inv.is_selected();
    }
  }
  timer.Stop();
  const RunResult result(inv.model()->name(),
                         absl::StrCat("ThriftyLNS(", first_sol_gen.name(), ")"),
                         best_cost, inv.ComputeCardinality(),
                         timer.GetDuration(), best_solution);
  report.ReportRunResult(result);
  return result;
}
}  // namespace

double RunSolvers() {
  const auto& input = absl::GetFlag(FLAGS_input);
  const auto& input_format = ParseFileFormat(absl::GetFlag(FLAGS_input_fmt));
  QCHECK(!input.empty()) << "No input file specified.";
  QCHECK(input.empty() || input_format != SetCoverFormat::EMPTY)
      << "Input format cannot be empty.";

  SetCoverModel model = ReadModel(input, absl::GetFlag(FLAGS_input_fmt));
  Report report(absl::GetFlag(FLAGS_csv), absl::GetFlag(FLAGS_latex));
  report.LogStats(model);
  SetCoverInvariant inv(&model);

  GreedySolutionGenerator greedy(&inv);
  const RunResult greedy_result = RunAndReport(greedy, report);
  SteepestSearch steepest(&inv);
  RunFromResultAndReport(greedy_result, steepest, report);
  LazySteepestSearch lazy_steepest(&inv);
  RunFromResultAndReport(greedy_result, lazy_steepest, report);

  ElementDegreeSolutionGenerator element_degree(&inv);
  const RunResult element_degree_result = RunAndReport(element_degree, report);
  RunFromResultAndReport(element_degree_result, steepest, report);
  RunFromResultAndReport(element_degree_result, lazy_steepest, report);

  LazyElementDegreeSolutionGenerator lazy_element_degree(&inv);
  const RunResult lazy_element_degree_result =
      RunAndReport(lazy_element_degree, report);
  RunFromResultAndReport(lazy_element_degree_result, steepest, report);
  RunFromResultAndReport(lazy_element_degree_result, lazy_steepest, report);
  RunLowerBoundAndReport(true, ComputeDualAscentLB, "DualAscentLB", inv,
                         report);
  RunLowerBoundAndReport(true, ComputeDualAscentLBFullRandom,
                         "DualAscentLBFullRandom", inv, report);

  RunLowerBoundAndReport(true, ComputeLagrangianLowerBound, "LagrangianLB", inv,
                         report);

  RunMipAndReport(absl::GetFlag(FLAGS_run_lp), false, inv, report);
  RunMipAndReport(absl::GetFlag(FLAGS_run_mip), true, inv, report);
  std::cout << report.output();

  // IterateClearAndMip(name, inv);
  // ElementDegreeGeneratorRandomClearSteepestIterate(name, &inv);
  // TODO(user): add a RunAndReport for GLS.
  // GuidedLocalSearch gls(&inv);
  // RunFromResultAndReport(lazy_element_degree_result, gls, &output);
  return inv.cost();
}

void Benchmarks() {
  QCHECK(!absl::GetFlag(FLAGS_benchmarks_dir).empty())
      << "Benchmarks directory must be specified.";
  const std::vector<BenchmarksTableRow> kBenchmarks = BenchmarksTable();

  const bool run_all = true;  // TODO(user): streamline the flags.
  const bool run_element_degree =
      run_all || absl::GetFlag(FLAGS_run_element_degree);
  const bool run_lazy_element_degree =
      run_all || absl::GetFlag(FLAGS_run_lazy_element_degree);
  const bool run_randomized_lazy_element_degree =
      run_all || absl::GetFlag(FLAGS_run_randomized_lazy_element_degree);
  const bool run_lazy_steepest =
      run_all || absl::GetFlag(FLAGS_run_lazy_steepest);
  const bool run_lazy_steepest_from_random =
      run_all || absl::GetFlag(FLAGS_run_lazy_steepest_from_random);
  const bool run_clique_guided =
      run_all || absl::GetFlag(FLAGS_run_clique_guided);
  const bool run_lower_bounds =
      run_all || absl::GetFlag(FLAGS_run_lower_bounds);
  const bool run_element_based_tree_search =
      run_all || absl::GetFlag(FLAGS_run_element_based_tree_search);
  const bool run_tree_search = run_all || absl::GetFlag(FLAGS_run_tree_search);
  const bool run_lp = run_all || absl::GetFlag(FLAGS_run_lp);
  const bool run_mip = run_all || absl::GetFlag(FLAGS_run_mip);

  Report report(absl::GetFlag(FLAGS_csv), absl::GetFlag(FLAGS_latex));
  if (absl::GetFlag(FLAGS_stats)) {
    report.StrAppend(absl::StrJoin({"Problem", "|S|", "|U|", "nnz", "Fill",
                                    "Col size", "Row size"},
                                   report.Separator()),
                     report.Eol());
  }
  for (const auto& [dir, files, input_format] : kBenchmarks) {
    RunStats element_degree_vs_chvatal("ElementDegreeGenerator",
                                       "GreedyGenerator");
    RunStats lazy_element_degree_vs_chvatal(
        absl::StrCat("LazyElementDegreeGenerator",
                     absl::GetFlag(FLAGS_num_random_lazy_element_degree_runs)),
        "GreedyGenerator");
    RunStats lazy_steepest_vs_steepest("LazySteepestSearch", "SteepestSearch");
    RunStats lazy_greedy_vs_chvatal("LazyGreedyGenerator", "GreedyGenerator");
    RunStats clique_guided_vs_steepest("CliqueGuidedLNS", "SteepestSearch");
    RunStats thrifty_lns_vs_chvatal("ThriftyLNS", "GreedyGenerator");
    std::vector<RunStats*> run_stats_list = {
        &element_degree_vs_chvatal, &lazy_element_degree_vs_chvatal,
        &lazy_steepest_vs_steepest, &lazy_greedy_vs_chvatal,
        &thrifty_lns_vs_chvatal};

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
      report.SetModelName(filename, absl::GetFlag(FLAGS_unicost), &model);
      if (absl::GetFlag(FLAGS_stats)) {
        report.ReportModelStats(model);
      }
      if (!absl::GetFlag(FLAGS_solve)) continue;

      model.CreateSparseRowView();

      LOG(INFO) << "Solving " << model.name();
      report.ReportModelNameAndSizes(model);

      SetCoverInvariant inv_with_loaded_solution(&model);
      const RunResult loaded_solution_result =
          RunSolutionLoadAndReport(absl::GetFlag(FLAGS_load_solution), filename,
                                   inv_with_loaded_solution, report);

      SetCoverInvariant inv(&model);

      // Do not run Chvatal's classic algorithm on large problems, this is too
      // slow.
      const bool run_chvatal =
          model.num_elements() <= absl::GetFlag(FLAGS_max_elements_for_chvatal);
      const RunResult chvatal_result =
          RunChvatalAndReport(run_chvatal, inv, report);
      report.ReportGap(chvatal_result, loaded_solution_result);

      RunResult chvatal_steepest_result;
      if (run_chvatal) {
        // SteepestSearch starts from the Greedy solution.
        SteepestSearch steepest(&inv);
        chvatal_steepest_result =
            RunFromResultAndReport(chvatal_result, steepest, report);
      }
      report.ReportGap(chvatal_steepest_result, loaded_solution_result);

      const RunResult element_degree_result =
          RunElementDegreeAndReport(run_element_degree, inv, report);
      report.ReportGap(element_degree_result, loaded_solution_result);

      const RunResult lazy_element_degree_result =
          RunLazyElementDegreeAndReport(run_lazy_element_degree, inv, report);

      const RunResult lazy_random_result =
          RunRandomizedLazyElementDegreeAndReport(
              run_randomized_lazy_element_degree, inv, report);
      report.ReportGap(lazy_random_result, loaded_solution_result);
      CHECK_EQ(lazy_element_degree_result.cost(), element_degree_result.cost());

      const RunResult lazy_steepest_result = RunLazySteepestAndReport(
          run_lazy_steepest, lazy_element_degree_result, inv, report);

      const RunResult lazy_random_steepest_result = RunLazySteepestAndReport(
          run_lazy_steepest_from_random, lazy_random_result, inv, report);
      report.ReportGap(lazy_random_steepest_result, loaded_solution_result);

      const RunResult clique_guided_result = RunCliqueGuidedAndReport(
          run_clique_guided, lazy_random_steepest_result, inv, report);
      report.ReportGap(clique_guided_result, loaded_solution_result);
      report.ReportGap(clique_guided_result, lazy_random_steepest_result);

      const RunResult dual_ascent_result = RunLowerBoundAndReport(
          run_lower_bounds, ComputeDualAscentLB, "DualAscentLB", inv, report);
      report.ReportGap(loaded_solution_result, dual_ascent_result);

      const RunResult dual_ascent_full_random_result = RunLowerBoundAndReport(
          run_lower_bounds, ComputeDualAscentLBFullRandom,
          "DualAscentLBFullRandom", inv, report);
      report.ReportGap(loaded_solution_result, dual_ascent_full_random_result);

      const RunResult lagrangian_relaxation_result =
          RunLowerBoundAndReport(run_lower_bounds, ComputeLagrangianLowerBound,
                                 "LagrangianRelaxationLB", inv, report);
      report.ReportGap(loaded_solution_result, lagrangian_relaxation_result);

      const RunResult lp_result = RunMipAndReport(run_lp, false, inv, report);
      report.ReportGap(lp_result, loaded_solution_result);

      const RunResult element_based_tree_search_result =
          RunElementBasedTreeSearchAndReport(run_element_based_tree_search, inv,
                                             report);
      report.ReportGap(element_based_tree_search_result,
                       loaded_solution_result);

      const RunResult tree_search_result =
          RunTreeSearchAndReport(run_tree_search, inv, report);
      report.ReportGap(tree_search_result, loaded_solution_result);
      const RunResult mip_result = RunMipAndReport(run_mip, true, inv, report);
      report.ReportGap(mip_result, loaded_solution_result);
      if (run_chvatal) {
        element_degree_vs_chvatal.Add(element_degree_result, chvatal_result);
        lazy_element_degree_vs_chvatal.Add(lazy_element_degree_result,
                                           chvatal_result);
        lazy_steepest_vs_steepest.Add(lazy_steepest_result,
                                      chvatal_steepest_result);
        lazy_greedy_vs_chvatal.Add(lazy_steepest_result, chvatal_result);
        clique_guided_vs_steepest.Add(clique_guided_result,
                                      chvatal_steepest_result);
      }
      if (absl::GetFlag(FLAGS_thrifty_lns)) {
        LazyElementDegreeSolutionGenerator lazy_element_degree_for_thrifty_lns(
            &inv);
        LazySteepestSearch lazy_steepest_for_thrifty_lns(&inv);
        const RunResult thrifty_lns_result = RunThriftyLNSAndReport(
            absl::GetFlag(FLAGS_thrifty_lns), lazy_steepest_result,
            lazy_element_degree_for_thrifty_lns, lazy_steepest_for_thrifty_lns,
            report);
        if (run_chvatal) {
          thrifty_lns_vs_chvatal.Add(thrifty_lns_result, chvatal_result);
        }
      }
      report.StrAppend(report.Eol());
      std::cout << report.output();
      report.Clear();
    }
    if (absl::GetFlag(FLAGS_summarize)) {
      for (const RunStats* run_stats : run_stats_list) {
        if (run_stats == &thrifty_lns_vs_chvatal &&
            !absl::GetFlag(FLAGS_thrifty_lns)) {
          continue;
        }
        report.ReportRunStats(*run_stats);
        report.StrAppend(report.Eol());
        std::cout << report.output();
        report.Clear();
      }
    }
  }
}

void SolveFromCommandLine() {
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
  const auto problem = output.empty() ? input : output;
  Report report(absl::GetFlag(FLAGS_csv), absl::GetFlag(FLAGS_latex));
  if (absl::GetFlag(FLAGS_stats)) {
    report.LogStats(model);
  }
  if (absl::GetFlag(FLAGS_solve)) {
    LOG(INFO) << "Solving " << problem;
    model.CreateSparseRowView();
    SetCoverInvariant inv(&model);
    LazyElementDegreeSolutionGenerator lazy_element_degree(&inv);
    const RunResult lazy_element_degree_result =
        RunAndReport(lazy_element_degree, report);
    LazyElementDegreeSolutionGenerator randomized_lazy_element_degree(&inv);
    randomized_lazy_element_degree.SetNumRandomPasses(100);
    LazySteepestSearch lazy_steepest(&inv);
    RunThriftyLNSAndReport(true, lazy_element_degree_result,
                           randomized_lazy_element_degree, lazy_steepest,
                           report);
    std::cout << report.output() << std::endl;
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
  } else if (absl::GetFlag(FLAGS_solve)) {
    operations_research::RunSolvers();
  } else {
    operations_research::SolveFromCommandLine();
  }
  return 0;
}
