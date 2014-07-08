// Copyright 2010-2013 Google
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
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/strtoint.h"
#include "base/timer.h"
#include "base/file.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "base/strutil.h"
// TODO(user): Move sat_cnf_reader.h and sat_runner.cc to examples?
#include "cpp/opb_reader.h"
#include "cpp/sat_cnf_reader.h"
#include "base/random.h"
#include "sat/boolean_problem.h"
#include "sat/optimization.h"
#include "sat/sat_solver.h"
#include "util/time_limit.h"
#include "algorithms/sparse_permutation.h"

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
    "also writen to the file.");

DEFINE_bool(output_cnf_solution, false,
            "If true and the problem was solved to optimality, this output "
            "the solution to stdout in cnf form.\n");

DEFINE_string(
    expected_result, "undefined",
    "Checks the result against expected. Possible values are undefined, "
    "sat, unsat");

DEFINE_string(params, "",
              "Parameters for the sat solver in a text format of the "
              "SatParameters proto, example: --params=use_conflicts:true.");

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

DEFINE_bool(use_cardinality_encoding, true,
            "If true, use an encoding of the at most k constraint instead "
            "of the native PB format.");

DEFINE_bool(linear_scan, false,
            "If true, search the optimal solution with the linear scan algo.");

DEFINE_int32(randomize, 500,
             "If positive, solve that many times the problem with a random "
             "decision heuristic before trying to optimize it.");

DEFINE_bool(use_symmetry, false,
            "If true, find and exploit the eventual symmetries "
            "of the problem.");


DEFINE_bool(refine_core, false,
            "If true, turn on the unsat_proof parameters and if the problem is "
            "UNSAT, refine as much as possible its UNSAT core in order to get "
            "a small one.");

namespace operations_research {
namespace sat {
namespace {

// Returns the scaled objective.
double GetScaledObjective(const LinearBooleanProblem& problem,
                          Coefficient objective) {
  return objective.value() * problem.objective().scaling_factor() +
         problem.objective().offset();
}

void LoadBooleanProblem(std::string filename, LinearBooleanProblem* problem) {
  if (HasSuffixString(filename, ".opb") ||
      HasSuffixString(filename, ".opb.bz2")) {
    OpbReader reader;
    if (!reader.Load(filename, problem)) {
      LOG(FATAL) << "Cannot load file '" << filename << "'.";
    }
  } else if (HasSuffixString(filename, ".cnf") ||
             HasSuffixString(filename, ".wcnf")) {
    SatCnfReader reader;
    if (FLAGS_fu_malik || FLAGS_linear_scan || FLAGS_wpm1) {
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
  VariableIndex limit(problem.original_num_variables());
  for (VariableIndex index(0); index < limit; ++index) {
    if (index > 0) output += " ";
    output += StringPrintf(
        "%d", Literal(index, assignment[index.value()]).SignedValue());
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

  // Enforce some parameters if we are looking for UNSAT core.
  if (FLAGS_refine_core) {
    parameters.set_unsat_proof(true);
    parameters.set_treat_binary_clauses_separately(false);
  }

  // Initialize the solver.
  std::unique_ptr<SatSolver> solver(new SatSolver());
  solver->SetParameters(parameters);

  // Read the problem.
  LinearBooleanProblem problem;
  LoadBooleanProblem(FLAGS_input, &problem);

  // Count the time from there.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  // Load the problem into the solver.
  if (!LoadBooleanProblem(problem, solver.get())) {
    LOG(FATAL) << "Couldn't load problem '" << FLAGS_input << "'.";
  }
  if (!AddObjectiveConstraint(
          problem, !FLAGS_lower_bound.empty(),
          Coefficient(atoi64(FLAGS_lower_bound)), !FLAGS_upper_bound.empty(),
          Coefficient(atoi64(FLAGS_upper_bound)), solver.get())) {
    LOG(FATAL) << "Issue when setting the objective bounds.";
  }

  // Symmetries!
  if (FLAGS_use_symmetry) {
    LOG(INFO) << "Finding symmetries of the problem.";
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    FindLinearBooleanProblemSymmetries(problem, &generators);
    solver->AddSymmetries(&generators);
  }

  // Optimize?
  std::vector<bool> solution;
  SatSolver::Status result = SatSolver::LIMIT_REACHED;
  if (FLAGS_fu_malik || FLAGS_linear_scan || FLAGS_wpm1) {
    if (FLAGS_randomize > 0 && FLAGS_linear_scan) {
      result = SolveWithRandomParameters(STDOUT_LOG, problem, FLAGS_randomize,
                                         solver.get(), &solution);
    }
    if (result == SatSolver::LIMIT_REACHED) {
      if (FLAGS_use_cardinality_encoding) {
        // We use a new solver to not have any PB constraints.
        if (FLAGS_linear_scan) {
          solver.reset(new SatSolver());
          solver->SetParameters(parameters);
          CHECK(LoadBooleanProblem(problem, solver.get()));
        }
        if (FLAGS_linear_scan) {
          result = SolveWithCardinalityEncoding(STDOUT_LOG, problem,
                                                solver.get(), &solution);
        } else {
          result = SolveWithCardinalityEncodingAndCore(STDOUT_LOG, problem,
                                                       solver.get(), &solution);
        }
      } else {
        result =
            FLAGS_fu_malik
                ? SolveWithFuMalik(STDOUT_LOG, problem, solver.get(), &solution)
                : FLAGS_wpm1 ? SolveWithWPM1(STDOUT_LOG, problem, solver.get(),
                                             &solution)
                             : SolveWithLinearScan(STDOUT_LOG, problem,
                                                   solver.get(), &solution);
      }
    }
  } else {
    // Only solve the decision version.
    parameters.set_log_search_progress(true);
    solver->SetParameters(parameters);
    result = solver->Solve();
    if (result == SatSolver::MODEL_SAT) {
      ExtractAssignment(problem, *solver, &solution);
      CHECK(IsAssignmentValid(problem, solution));
    }

    // Unsat with verification.
    // Note(user): For now we just compute an UNSAT core and check it.
    if (result == SatSolver::MODEL_UNSAT && parameters.unsat_proof()) {
      std::vector<int> core;
      solver->ComputeUnsatCore(&core);
      LOG(INFO) << "UNSAT. Identified a core of " << core.size()
                << " constraints.";

      // The following block is mainly for testing the UNSAT core feature.
      if (FLAGS_refine_core) {
        int old_core_size = core.size();
        LinearBooleanProblem old_problem;
        LinearBooleanProblem core_unsat_problem;
        old_problem.CopyFrom(problem);
        int i = 1;
        do {
          ExtractSubproblem(old_problem, core, &core_unsat_problem);
          core_unsat_problem.set_name(StringPrintf("Subproblem #%d", i));
          old_core_size = core.size();
          old_problem.CopyFrom(core_unsat_problem);
          SatSolver new_solver;
          new_solver.SetParameters(parameters);
          CHECK(LoadBooleanProblem(core_unsat_problem, &new_solver));
          CHECK_EQ(new_solver.Solve(), SatSolver::MODEL_UNSAT) << "Wrong core!";
          new_solver.ComputeUnsatCore(&core);
          LOG(INFO) << "Core #" << i << " checked, next size is "
                    << core.size();
          ++i;
        } while (core.size() != old_core_size);
      }
    }

    if (!FLAGS_output.empty()) {
      if (result == SatSolver::MODEL_SAT) {
        StoreAssignment(solver->Assignment(), problem.mutable_assignment());
      }
      if (HasSuffixString(FLAGS_output, ".txt")) {
        file::WriteProtoToASCIIFileOrDie(problem, FLAGS_output);
      } else {
        file::WriteProtoToFileOrDie(problem, FLAGS_output);
      }
    }

    CHECK(
        FLAGS_expected_result == "undefined" ||
        (FLAGS_expected_result == "sat" && result == SatSolver::MODEL_SAT) ||
        (FLAGS_expected_result == "unsat" && result == SatSolver::MODEL_UNSAT));
  }

  // Print the solution status.
  if (result == SatSolver::MODEL_SAT) {
    if (FLAGS_fu_malik || FLAGS_linear_scan || FLAGS_wpm1) {
      printf("s OPTIMUM FOUND\n");
    } else {
      printf("s SAT\n");
    }
  }
  if (result == SatSolver::MODEL_UNSAT) {
    printf("s UNSAT\n");
  }

  // Check the solution if it is non-empty.
  if (!solution.empty()) {
    CHECK(IsAssignmentValid(problem, solution));
    if (FLAGS_output_cnf_solution) {
      printf("v %s\n", SolutionString(problem, solution).c_str());
    }
    if (problem.type() != LinearBooleanProblem::SATISFIABILITY) {
      const Coefficient objective = ComputeObjectiveValue(problem, solution);
      printf("c objective: %.16g\n", GetScaledObjective(problem, objective));
    }
  } else {
    // No solutionof an optimization problem? we output kint64max by convention.
    if (problem.type() != LinearBooleanProblem::SATISFIABILITY) {
      printf("c objective: %lld\n", kint64max);
    }
  }

  // Print final statistics.
  printf("c status: %s\n", SatStatusString(result).c_str());
  printf("c conflicts: %lld\n", solver->num_failures());
  printf("c branches: %lld\n", solver->num_branches());
  printf("c propagations: %lld\n", solver->num_propagations());
  printf("c walltime: %f\n", wall_timer.Get());
  printf("c usertime: %f\n", user_timer.Get());
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\n"
    "This program solves a given Boolean linear problem.";

int main(int argc, char** argv) {
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  return operations_research::sat::Run();
}
