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
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/options.h"
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

struct ProblemData {
  const char* const filename;
  const double best_known_value;
  const double best_known_lb;
};

struct BenchmarkGroup {
  std::vector<absl::Span<const ProblemData>> problem_data_arrays;
  std::string file_extension;
  std::string type_string;
  SetCoverFormat format;
};

// List all the files from the literature.
static const ProblemData kRailFiles[] = {
    {"rail507", 174, 0},   {"rail516", 182, 0},  {"rail582", 211, 0},
    {"rail2536", 691, 0},  {"rail2586", 952, 0}, {"rail4284", 1065, 0},
    {"rail4872", 1527, 0},
};

static const ProblemData kScp4To6Files[] = {
    {"scp41", 429, 0},  {"scp42", 512, 0},  {"scp43", 516, 0},
    {"scp44", 494, 0},  {"scp45", 512, 0},  {"scp46", 560, 0},
    {"scp47", 430, 0},  {"scp48", 492, 0},  {"scp49", 641, 0},
    {"scp410", 514, 0}, {"scp51", 253, 0},  {"scp52", 302, 0},
    {"scp53", 226, 0},  {"scp54", 242, 0},  {"scp55", 211, 0},
    {"scp56", 213, 0},  {"scp57", 293, 0},  {"scp58", 288, 0},
    {"scp59", 279, 0},  {"scp510", 265, 0}, {"scp61", 138, 0},
    {"scp62", 146, 0},  {"scp63", 145, 0},  {"scp64", 131, 0},
    {"scp65", 161, 0},
};

static const ProblemData kScpAToEFiles[] = {
    {"scpa1", 253, 0}, {"scpa2", 252, 0}, {"scpa3", 232, 0}, {"scpa4", 234, 0},
    {"scpa5", 236, 0}, {"scpb1", 69, 0},  {"scpb2", 76, 0},  {"scpb3", 80, 0},
    {"scpb4", 79, 0},  {"scpb5", 72, 0},  {"scpc1", 227, 0}, {"scpc2", 219, 0},
    {"scpc3", 243, 0}, {"scpc4", 219, 0}, {"scpc5", 214, 0}, {"scpd1", 60, 0},
    {"scpd2", 66, 0},  {"scpd3", 72, 0},  {"scpd4", 62, 0},  {"scpd5", 61, 0},
    {"scpe1", 5, 0},   {"scpe2", 5, 0},   {"scpe3", 5, 0},   {"scpe4", 5, 0},
    {"scpe5", 5, 0},
};

static const ProblemData kScpNrFiles[] = {
    {"scpnre1", 29, 0},  {"scpnre2", 30, 0},  {"scpnre3", 27, 0},
    {"scpnre4", 28, 0},  {"scpnre5", 28, 0},  {"scpnrf1", 14, 0},
    {"scpnrf2", 15, 0},  {"scpnrf3", 14, 0},  {"scpnrf4", 14, 0},
    {"scpnrf5", 13, 0},  {"scpnrg1", 176, 0}, {"scpnrg2", 154, 0},
    {"scpnrg3", 166, 0}, {"scpnrg4", 168, 0}, {"scpnrg5", 168, 0},
    {"scpnrh1", 63, 0},  {"scpnrh2", 63, 0},  {"scpnrh3", 59, 0},
    {"scpnrh4", 58, 0},  {"scpnrh5", 55, 0},
};

static const ProblemData kScpClrFiles[] = {
    {"scpclr10", 0, 0},
    {"scpclr11", 0, 0},
    {"scpclr12", 0, 0},
    {"scpclr13", 0, 0},
};

static const ProblemData kScpCycFiles[] = {
    {"scpcyc06", 0, 0}, {"scpcyc07", 0, 0}, {"scpcyc08", 0, 0},
    {"scpcyc09", 0, 0}, {"scpcyc10", 0, 0}, {"scpcyc11", 0, 0},
};

static const ProblemData kWedelinFiles[] = {
    {"a320_coc", 0, 0}, {"a320", 0, 0},      {"alitalia", 0, 0},
    {"b727", 0, 0},     {"sasd9imp2", 0, 0}, {"sasjump", 0, 0},
};

static const ProblemData kBalasFiles[] = {
    {"aa03", 0, 0}, {"aa04", 0, 0}, {"aa05", 0, 0}, {"aa06", 0, 0},
    {"aa11", 0, 0}, {"aa12", 0, 0}, {"aa13", 0, 0}, {"aa14", 0, 0},
    {"aa15", 0, 0}, {"aa16", 0, 0}, {"aa17", 0, 0}, {"aa18", 0, 0},
    {"aa19", 0, 0}, {"aa20", 0, 0}, {"bus1", 0, 0}, {"bus2", 0, 0},
};

static const ProblemData kFimiFiles[] = {
    {"accidents", 0, 0},
    {"chess", 0, 0},
    {"connect", 0, 0},
    {"kosarak", 0, 0},
    {"mushroom", 0, 0},
    // "pumsb", "pumsb_star",
    {"retail", 0, 0},
    {"webdocs", 0, 0},
};

std::vector<BenchmarkGroup> CreateBenchmarkList() {
  return {
      {
          .problem_data_arrays = {absl::MakeConstSpan(kScp4To6Files)},
          .file_extension = ".txt",
          .type_string = "orlib",
          .format = SetCoverFormat::ORLIB,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kScpAToEFiles)},
          .file_extension = ".txt",
          .type_string = "orlib",
          .format = SetCoverFormat::ORLIB,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kScpNrFiles)},
          .file_extension = ".txt",
          .type_string = "orlib",
          .format = SetCoverFormat::ORLIB,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kScpClrFiles)},
          .file_extension = ".txt",
          .type_string = "orlib",
          .format = SetCoverFormat::ORLIB,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kScpCycFiles)},
          .file_extension = ".txt",
          .type_string = "orlib",
          .format = SetCoverFormat::ORLIB,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kRailFiles)},
          .file_extension = ".txt",
          .type_string = "rail",
          .format = SetCoverFormat::RAIL,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kWedelinFiles)},
          .file_extension = ".txt",
          .type_string = "wedelin",
          .format = SetCoverFormat::ORLIB,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kBalasFiles)},
          .file_extension = ".txt",
          .type_string = "balas",
          .format = SetCoverFormat::ORLIB,
      },
      {
          .problem_data_arrays = {absl::MakeConstSpan(kFimiFiles)},
          .file_extension = ".dat",
          .type_string = "fimi",
          .format = SetCoverFormat::FIMI,
      },
  };
}

template <typename ParamsType>
RunResult RunAndReport(SetCoverOptimizer<ParamsType>& gen, Report& report) {
  gen.inv()->Clear();
  CHECK(gen.Optimize());
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
  if (gen.Optimize()) {
    const RunResult result(
        inv->model()->name(),
        use_integers ? "SetCoverMIP" : "SetCoverLPLowerBound",
        use_integers ? inv->cost() : inv->LowerBound(),
        inv->ComputeCardinality(), gen.run_time(), inv->is_selected());
    report.ReportRunResult(result);
    return result;
  }
  LOG(INFO) << "SetCoverMip::Optimize() failed with status: "
            << gen.solve_status();
  return RunResult(gen);
}

// Runs a solution generator starting from the solution in the base_result, and
// reports the results in a string. Returns the result as a RunResult for
// further use.
template <typename ParamsType>
RunResult RunFromResultAndReport(const RunResult& base_result,
                                 SetCoverOptimizer<ParamsType>& gen,
                                 Report& report) {
  SetCoverInvariant* inv = gen.inv();
  inv->LoadSolution(base_result.solution());
  inv->Recompute(CL::kCostAndCoverage);
  CHECK(gen.Optimize());
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

// Computes a lower bound using `gen` optimizer, and reports the results.
template <typename ParamsType>
RunResult RunLowerBoundAndReport(bool run_lower_bound,
                                 SetCoverOptimizer<ParamsType>& gen,
                                 Report& report) {
  if (!run_lower_bound) return RunResult();
  CHECK(gen.Optimize());
  const RunResult result(gen.inv()->model()->name(), gen.name(),
                         gen.inv()->LowerBound(), 0, gen.run_time(),
                         SubsetBoolVector());
  report.ReportRunResult(result);
  return result;
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
  GreedySolutionOptimizer chvatal(&inv);  // Classic greedy (Chvatal)
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
  clique_guided_lns.Optimize();
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

template <typename ParamsT1, typename ParamsT2>
RunResult RunThriftyLNSAndReport(bool run_thrifty_lns, const RunResult& start,
                                 SetCoverOptimizer<ParamsT1>& initial_sol_gen,
                                 SetCoverOptimizer<ParamsT2>& improvement_gen,
                                 Report& report) {
  if (!run_thrifty_lns) return RunResult();
  SetCoverInvariant& inv = *initial_sol_gen.inv();
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
    CHECK(initial_sol_gen.Optimize());
    if (inv.cost() < best_cost) {
      best_cost = inv.cost();
      best_solution = inv.is_selected();
    }
  }
  timer.Stop();
  const RunResult result(
      inv.model()->name(),
      absl::StrCat("ThriftyLNS(", initial_sol_gen.name(), ")"), best_cost,
      inv.ComputeCardinality(), timer.GetDuration(), best_solution);
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

  GreedySolutionOptimizer greedy(&inv);
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
  DualAscentOptimizer dual_ascent(&inv);
  dual_ascent.SetNumRandomPasses(
      absl::GetFlag(FLAGS_num_random_dual_ascent_passes));
  RunLowerBoundAndReport(true, dual_ascent, report);

  DualAscentOptimizer dual_ascent_full_random(&inv);
  dual_ascent_full_random.UseFullRandomization(true).SetNumRandomPasses(
      absl::GetFlag(FLAGS_num_random_dual_ascent_passes));
  RunLowerBoundAndReport(true, dual_ascent_full_random, report);

  VolumeOptimizer volume(&inv);
  RunLowerBoundAndReport(true, volume, report);

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
  const std::vector<BenchmarkGroup> kBenchmarks = CreateBenchmarkList();

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
  for (const BenchmarkGroup& group : kBenchmarks) {
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

    for (const auto& problem_data_array : group.problem_data_arrays) {
      for (const ProblemData& problem_data : problem_data_array) {
        const std::string filename =
            absl::StrCat(problem_data.filename, group.file_extension);
        const std::string filespec =
            absl::StrCat(absl::GetFlag(FLAGS_benchmarks_dir), "/",
                         group.type_string, "/", filename);

        LOG(INFO) << "Reading " << filespec;
        SetCoverModel model = ReadModel(filespec, group.format);
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
        const RunResult loaded_solution_result = RunSolutionLoadAndReport(
            absl::GetFlag(FLAGS_load_solution), filename,
            inv_with_loaded_solution, report);

        SetCoverInvariant inv(&model);

        // Do not run Chvatal's classic algorithm on large problems, this is too
        // slow.
        const bool run_chvatal = model.num_elements() <=
                                 absl::GetFlag(FLAGS_max_elements_for_chvatal);
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
        CHECK_EQ(lazy_element_degree_result.cost(),
                 element_degree_result.cost());

        const RunResult lazy_steepest_result = RunLazySteepestAndReport(
            run_lazy_steepest, lazy_element_degree_result, inv, report);

        const RunResult lazy_random_steepest_result = RunLazySteepestAndReport(
            run_lazy_steepest_from_random, lazy_random_result, inv, report);
        report.ReportGap(lazy_random_steepest_result, loaded_solution_result);

        const RunResult clique_guided_result = RunCliqueGuidedAndReport(
            run_clique_guided, lazy_random_steepest_result, inv, report);
        report.ReportGap(clique_guided_result, loaded_solution_result);
        report.ReportGap(clique_guided_result, lazy_random_steepest_result);

        DualAscentOptimizer dual_ascent(&inv);
        dual_ascent.SetNumRandomPasses(
            absl::GetFlag(FLAGS_num_random_dual_ascent_passes));
        const RunResult dual_ascent_result =
            RunLowerBoundAndReport(run_lower_bounds, dual_ascent, report);
        report.ReportGap(loaded_solution_result, dual_ascent_result);

        DualAscentOptimizer dual_ascent_full_random(&inv);
        dual_ascent_full_random.UseFullRandomization(true).SetNumRandomPasses(
            absl::GetFlag(FLAGS_num_random_dual_ascent_passes));
        const RunResult dual_ascent_full_random_result = RunLowerBoundAndReport(
            run_lower_bounds, dual_ascent_full_random, report);
        report.ReportGap(loaded_solution_result,
                         dual_ascent_full_random_result);

        VolumeOptimizer volume(&inv);
        const RunResult volume_result =
            RunLowerBoundAndReport(run_lower_bounds, volume, report);
        report.ReportGap(loaded_solution_result, volume_result);

        const RunResult lagrangian_relaxation_result = RunLowerBoundAndReport(
            run_lower_bounds, ComputeLagrangianLowerBound,
            "LagrangianRelaxationLB", inv, report);
        report.ReportGap(loaded_solution_result, lagrangian_relaxation_result);

        const RunResult lp_result = RunMipAndReport(run_lp, false, inv, report);
        report.ReportGap(lp_result, loaded_solution_result);

        const RunResult element_based_tree_search_result =
            RunElementBasedTreeSearchAndReport(run_element_based_tree_search,
                                               inv, report);
        report.ReportGap(element_based_tree_search_result,
                         loaded_solution_result);

        const RunResult tree_search_result =
            RunTreeSearchAndReport(run_tree_search, inv, report);
        report.ReportGap(tree_search_result, loaded_solution_result);
        const RunResult mip_result =
            RunMipAndReport(run_mip, true, inv, report);
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
          LazyElementDegreeSolutionGenerator
              lazy_element_degree_for_thrifty_lns(&inv);
          LazySteepestSearch lazy_steepest_for_thrifty_lns(&inv);
          const RunResult thrifty_lns_result = RunThriftyLNSAndReport(
              absl::GetFlag(FLAGS_thrifty_lns), lazy_steepest_result,
              lazy_element_degree_for_thrifty_lns,
              lazy_steepest_for_thrifty_lns, report);
          if (run_chvatal) {
            thrifty_lns_vs_chvatal.Add(thrifty_lns_result, chvatal_result);
          }
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
