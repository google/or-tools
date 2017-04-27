// Copyright 2010-2014 Google
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

#include <csignal>
#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/timer.h"
#include "ortools/base/file.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/base/strutil.h"
#include "ortools/base/threadpool.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/drat.h"
#include "examples/cpp/opb_reader.h"
#include "ortools/sat/optimization.h"
#include "examples/cpp/sat_cnf_reader.h"
#include "ortools/base/join.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/symmetry.h"
#include "ortools/util/time_limit.h"
#include "ortools/base/random.h"
#include "ortools/base/status.h"

DEFINE_string(
    input, "",
    "Required: input file of the problem to solve. Many format are supported:"
    ".cnf (sat, max-sat, weighted max-sat), .opb (pseudo-boolean sat/optim) "
    "and by default the LinearBooleanProblem proto (binary or text).");

DEFINE_string(
    output, "",
    "If non-empty, write the input problem as a LinearBooleanProblem proto to "
    "this file. By default it uses the binary format except if the file "
    "extension is '.txt'. If the problem is SAT, a satisfiable assignment is "
    "also written to the file.");

DEFINE_bool(output_cnf_solution, false,
            "If true and the problem was solved to optimality, this output "
            "the solution to stdout in cnf form.\n");

DEFINE_string(params, "",
              "Parameters for the sat solver in a text format of the "
              "SatParameters proto, example: --params=use_conflicts:true.");

DEFINE_bool(strict_validity, false,
            "If true, stop if the given input is invalid (duplicate literals, "
            "out of range, zero cofficients, etc.)");

DEFINE_string(
    lower_bound, "",
    "If not empty, look for a solution with an objective value >= this bound.");

DEFINE_string(
    upper_bound, "",
    "If not empty, look for a solution with an objective value <= this bound.");

DEFINE_bool(fu_malik, false,
            "If true, search the optimal solution with the Fu & Malik algo.");

DEFINE_bool(wpm1, false,
            "If true, search the optimal solution with the WPM1 algo.");

DEFINE_bool(qmaxsat, false,
            "If true, search the optimal solution with a linear scan and "
            " the cardinality encoding used in qmaxsat.");

DEFINE_bool(core_enc, false,
            "If true, search the optimal solution with the core-based "
            "cardinality encoding algo.");

DEFINE_bool(linear_scan, false,
            "If true, search the optimal solution with the linear scan algo.");

DEFINE_int32(randomize, 500,
             "If positive, solve that many times the problem with a random "
             "decision heuristic before trying to optimize it.");

DEFINE_bool(use_symmetry, false,
            "If true, find and exploit the eventual symmetries "
            "of the problem.");

DEFINE_bool(presolve, true,
            "Only work on pure SAT problem. If true, presolve the problem.");

DEFINE_bool(probing, false, "If true, presolve the problem using probing.");


DEFINE_bool(reduce_memory_usage, false,
            "If true, do not keep a copy of the original problem in memory."
            "This reduce the memory usage, but disable the solution cheking at "
            "the end.");

namespace operations_research {
namespace sat {
namespace {

// Returns a trivial best bound. The best bound corresponds to the lower bound
// (resp. upper bound) in case of a minimization (resp. maximization) problem.
double GetScaledTrivialBestBound(const LinearBooleanProblem& problem) {
  Coefficient best_bound(0);
  const LinearObjective& objective = problem.objective();
  for (const int64 value : objective.coefficients()) {
    if (value < 0) best_bound += Coefficient(value);
  }
  return AddOffsetAndScaleObjectiveValue(problem, best_bound);
}

void LoadBooleanProblem(std::string filename, LinearBooleanProblem* problem) {
  if (strings::EndsWith(filename, ".opb") ||
      strings::EndsWith(filename, ".opb.bz2")) {
    OpbReader reader;
    if (!reader.Load(filename, problem)) {
      LOG(FATAL) << "Cannot load file '" << filename << "'.";
    }
  } else if (strings::EndsWith(filename, ".cnf") ||
             strings::EndsWith(filename, ".cnf.gz") ||
             strings::EndsWith(filename, ".wcnf") ||
             strings::EndsWith(filename, ".wcnf.gz")) {
    SatCnfReader reader;
    if (FLAGS_fu_malik || FLAGS_linear_scan || FLAGS_wpm1 || FLAGS_qmaxsat ||
        FLAGS_core_enc) {
      reader.InterpretCnfAsMaxSat(true);
    }
    if (!reader.Load(filename, problem)) {
      LOG(FATAL) << "Cannot load file '" << filename << "'.";
    }
  } else {
    file::ReadFileToProtoOrDie(filename, problem);
  }
}

std::string SolutionString(const LinearBooleanProblem& problem,
                      const std::vector<bool>& assignment) {
  std::string output;
  BooleanVariable limit(problem.original_num_variables());
  for (BooleanVariable index(0); index < limit; ++index) {
    if (index > 0) output += " ";
    StrAppend(&output, Literal(index, assignment[index.value()]).SignedValue());
  }
  return output;
}

// To benefit from the operations_research namespace, we put all the main() code
// here.
int Run() {
  SatParameters parameters;
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  // In the algorithms below, this seems like a good parameter.
  parameters.set_count_assumption_levels_in_lbd(false);

  // Parse the --params flag.
  if (!FLAGS_params.empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(FLAGS_params, &parameters))
        << FLAGS_params;
  }

  Model model;
  DratWriter* drat_writer = model.GetOrCreate<DratWriter>();

  // Initialize the solver.
  std::unique_ptr<SatSolver> solver(new SatSolver());
  solver->SetDratWriter(drat_writer);
  solver->SetParameters(parameters);

  // The global time limit.
  std::unique_ptr<TimeLimit> time_limit(TimeLimit::FromParameters(parameters));

  // Catch ^C.
  bool interrupt_solve = false;
  time_limit->RegisterExternalBooleanAsLimit(&interrupt_solve);

  // Read the problem.
  LinearBooleanProblem problem;
  LoadBooleanProblem(FLAGS_input, &problem);
  if (FLAGS_strict_validity) {
    const util::Status status = ValidateBooleanProblem(problem);
    if (!status.ok()) {
      LOG(ERROR) << "Invalid Boolean problem: " << status.error_message();
      return EXIT_FAILURE;
    }
  }

  // Count the time from there.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();
  double scaled_best_bound = GetScaledTrivialBestBound(problem);

  // Probing.
  SatPostsolver probing_postsolver(problem.num_variables());
  LinearBooleanProblem original_problem;
  if (FLAGS_probing) {
    // TODO(user): This is nice for testing, but consumes memory.
    original_problem = problem;
    ProbeAndSimplifyProblem(&probing_postsolver, &problem);
  }

  // Load the problem into the solver.
  if (FLAGS_reduce_memory_usage) {
    if (!LoadAndConsumeBooleanProblem(&problem, solver.get())) {
      LOG(INFO) << "UNSAT when loading the problem.";
    }
  } else {
    if (!LoadBooleanProblem(problem, solver.get())) {
      LOG(INFO) << "UNSAT when loading the problem.";
    }
  }
  if (!AddObjectiveConstraint(
          problem, !FLAGS_lower_bound.empty(),
          Coefficient(atoi64(FLAGS_lower_bound)), !FLAGS_upper_bound.empty(),
          Coefficient(atoi64(FLAGS_upper_bound)), solver.get())) {
    LOG(INFO) << "UNSAT when setting the objective constraint.";
  }

  if (drat_writer != nullptr) {
    drat_writer->SetNumVariables(solver->NumVariables());
  }

  // Symmetries!
  //
  // TODO(user): To make this compatible with presolve, we just need to run
  // it after the presolve step.
  if (FLAGS_use_symmetry) {
    CHECK(!FLAGS_reduce_memory_usage) << "incompatible";
    CHECK(!FLAGS_presolve) << "incompatible";
    LOG(INFO) << "Finding symmetries of the problem.";
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    FindLinearBooleanProblemSymmetries(problem, &generators);
    std::unique_ptr<SymmetryPropagator> propagator(new SymmetryPropagator);
    for (int i = 0; i < generators.size(); ++i) {
      propagator->AddSymmetry(std::move(generators[i]));
    }
    solver->AddPropagator(std::move(propagator));
  }

  // Optimize?
  std::vector<bool> solution;
  SatSolver::Status result = SatSolver::LIMIT_REACHED;
  if (FLAGS_fu_malik || FLAGS_linear_scan || FLAGS_wpm1 || FLAGS_qmaxsat ||
      FLAGS_core_enc) {
    if (FLAGS_randomize > 0 && (FLAGS_linear_scan || FLAGS_qmaxsat)) {
      CHECK(!FLAGS_reduce_memory_usage) << "incompatible";
      result = SolveWithRandomParameters(STDOUT_LOG, problem, FLAGS_randomize,
                                         solver.get(), &solution);
    }
    if (result == SatSolver::LIMIT_REACHED) {
      if (FLAGS_qmaxsat) {
        solver.reset(new SatSolver());
        solver->SetParameters(parameters);
        CHECK(LoadBooleanProblem(problem, solver.get()));
        result = SolveWithCardinalityEncoding(STDOUT_LOG, problem, solver.get(),
                                              &solution);
      } else if (FLAGS_core_enc) {
        result = SolveWithCardinalityEncodingAndCore(STDOUT_LOG, problem,
                                                     solver.get(), &solution);
      } else if (FLAGS_fu_malik) {
        result = SolveWithFuMalik(STDOUT_LOG, problem, solver.get(), &solution);
      } else if (FLAGS_wpm1) {
        result = SolveWithWPM1(STDOUT_LOG, problem, solver.get(), &solution);
      } else if (FLAGS_linear_scan) {
        result =
            SolveWithLinearScan(STDOUT_LOG, problem, solver.get(), &solution);
      }
    }
  } else {
    // Only solve the decision version.
    parameters.set_log_search_progress(true);
    solver->SetParameters(parameters);
    if (FLAGS_presolve) {
      result = SolveWithPresolve(&solver, &solution, drat_writer);
      if (result == SatSolver::MODEL_SAT) {
        CHECK(IsAssignmentValid(problem, solution));
      }
    } else {
      result = solver->Solve();
      if (result == SatSolver::MODEL_SAT) {
        ExtractAssignment(problem, *solver, &solution);
        CHECK(IsAssignmentValid(problem, solution));
      }
    }
  }

  // Print the solution status.
  if (result == SatSolver::MODEL_SAT) {
    if (FLAGS_fu_malik || FLAGS_linear_scan || FLAGS_wpm1 || FLAGS_core_enc) {
      printf("s OPTIMUM FOUND\n");
      CHECK(!solution.empty());
      const Coefficient objective = ComputeObjectiveValue(problem, solution);
      scaled_best_bound = AddOffsetAndScaleObjectiveValue(problem, objective);

      // Postsolve.
      if (FLAGS_probing) {
        solution = probing_postsolver.PostsolveSolution(solution);
        problem = original_problem;
      }
    } else {
      printf("s SATISFIABLE\n");
    }

    // Check and output the solution.
    CHECK(IsAssignmentValid(problem, solution));
    if (FLAGS_output_cnf_solution) {
      printf("v %s\n", SolutionString(problem, solution).c_str());
    }
    if (!FLAGS_output.empty()) {
      CHECK(!FLAGS_reduce_memory_usage) << "incompatible";
      if (result == SatSolver::MODEL_SAT) {
        StoreAssignment(solver->Assignment(), problem.mutable_assignment());
      }
      if (strings::EndsWith(FLAGS_output, ".txt")) {
        CHECK_OK(file::SetTextProto(FLAGS_output, problem, file::Defaults()));
      } else {
        CHECK_OK(file::SetBinaryProto(FLAGS_output, problem, file::Defaults()));
      }
    }
  }
  if (result == SatSolver::MODEL_UNSAT) {
    printf("s UNSATISFIABLE\n");
  }

  // Print status.
  printf("c status: %s\n", SatStatusString(result).c_str());

  // Print objective value.
  if (solution.empty()) {
    printf("c objective: na\n");
    printf("c best bound: na\n");
  } else {
    const Coefficient objective = ComputeObjectiveValue(problem, solution);
    printf("c objective: %.16g\n",
           AddOffsetAndScaleObjectiveValue(problem, objective));
    printf("c best bound: %.16g\n", scaled_best_bound);
  }

  // Print final statistics.
  printf("c booleans: %d\n", solver->NumVariables());
  printf("c conflicts: %lld\n", solver->num_failures());
  printf("c branches: %lld\n", solver->num_branches());
  printf("c propagations: %lld\n", solver->num_propagations());
  printf("c walltime: %f\n", wall_timer.Get());
  printf("c usertime: %f\n", user_timer.Get());
  printf("c deterministic_time: %f\n", solver->deterministic_time());

  // The SAT competition requires a particular exit code and since we don't
  // really use it for any other purpose, we comply.
  if (result == SatSolver::MODEL_SAT) return 10;
  if (result == SatSolver::MODEL_UNSAT) return 20;
  return 0;
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\n"
    "This program solves a given Boolean linear problem.";

int main(int argc, char** argv) {
  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  return operations_research::sat::Run();
}
