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
#include "sat/simplification.h"
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

DEFINE_bool(presolve, false,
            "Only work on pure SAT problem. If true, presolve the problem.");

DEFINE_bool(probing, false, "If true, presolve the problem using probing.");


DEFINE_bool(refine_core, false,
            "If true, turn on the unsat_proof parameters and if the problem is "
            "UNSAT, refine as much as possible its UNSAT core in order to get "
            "a small one.");

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
  if (HasSuffixString(filename, ".opb") ||
      HasSuffixString(filename, ".opb.bz2")) {
    OpbReader reader;
    if (!reader.Load(filename, problem)) {
      LOG(FATAL) << "Cannot load file '" << filename << "'.";
    }
  } else if (HasSuffixString(filename, ".cnf") ||
             HasSuffixString(filename, ".cnf.gz") ||
             HasSuffixString(filename, ".wcnf") ||
             HasSuffixString(filename, ".wcnf.gz")) {
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
  if (!LoadBooleanProblem(problem, solver.get())) {
    LOG(INFO) << "UNSAT when loading the problem.";
  }
  if (!AddObjectiveConstraint(
          problem, !FLAGS_lower_bound.empty(),
          Coefficient(atoi64(FLAGS_lower_bound)), !FLAGS_upper_bound.empty(),
          Coefficient(atoi64(FLAGS_upper_bound)), solver.get())) {
    LOG(INFO) << "UNSAT when setting the objective constraint.";
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
  if (FLAGS_fu_malik || FLAGS_linear_scan || FLAGS_wpm1 || FLAGS_qmaxsat ||
      FLAGS_core_enc) {
    if (FLAGS_randomize > 0 && (FLAGS_linear_scan || FLAGS_qmaxsat)) {
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

    // Presolve.
    if (FLAGS_presolve) {
      SatPostsolver postsolver(problem.num_variables());

      // We use a new block so the memory used by the presolver can be
      // reclaimed as soon as it is no longer needed.
      //
      // TODO(user): Automatically adapt the number of iterations.
      result = SatSolver::MODEL_SAT;
      for (int i = 0; i < 4; ++i) {
        const int saved_num_variables = solver->NumVariables();

        // Probe + find equivalent literals.
        ITIVector<LiteralIndex, LiteralIndex> equiv_map;
        ProbeAndFindEquivalentLiteral(solver.get(), &postsolver, &equiv_map);

        // Register the fixed variables with the presolver.
        // TODO(user): Find a better place for this?
        solver->Backtrack(0);
        for (int i = 0; i < solver->LiteralTrail().Index(); ++i) {
          postsolver.FixVariable(solver->LiteralTrail()[i]);
        }

        SatPresolver presolver(&postsolver);
        presolver.SetParameters(parameters);
        presolver.SetEquivalentLiteralMapping(equiv_map);
        solver->ExtractClauses(&presolver);
        solver.release();
        if (!presolver.Presolve()) {
          printf("c unsat during presolve!\n");

          // This is just here for the satistics display below to work.
          solver.reset(new SatSolver());
          result = SatSolver::MODEL_UNSAT;
          break;
        }

        // Load the presolved problem in a new solver.
        solver.reset(new SatSolver());
        solver->SetParameters(parameters);
        presolver.LoadProblemIntoSatSolver(solver.get());
        postsolver.ApplyMapping(presolver.VariableMapping());

        // Stop if a fixed point has been reached.
        if (solver->NumVariables() == saved_num_variables) break;
      }

      // Solve.
      if (result != SatSolver::MODEL_UNSAT) {
        result = solver->Solve();
      }

      // Recover the solution.
      if (result == SatSolver::MODEL_SAT) {
        solution = postsolver.ExtractAndPostsolveSolution(*solver.get());
        CHECK(IsAssignmentValid(problem, solution));
      }

      // Statistics of the solver on the presolved problem.
      printf("c status: %s\n", SatStatusString(result).c_str());
      printf("c conflicts: %lld\n", solver->num_failures());
      printf("c branches: %lld\n", solver->num_branches());
      printf("c propagations: %lld\n", solver->num_propagations());

      // Overall time.
      printf("c walltime: %f\n", wall_timer.Get());
      printf("c usertime: %f\n", user_timer.Get());
      return EXIT_SUCCESS;
    }

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
    if (problem.objective().coefficients_size() > 0) {
      const Coefficient objective = ComputeObjectiveValue(problem, solution);
      printf("c objective: %.16g\n",
             AddOffsetAndScaleObjectiveValue(problem, objective));
      printf("c best bound: %.16g\n", scaled_best_bound);
    }
  } else {
    printf("c objective: na\n");
  }

  // Print final statistics.
  printf("c status: %s\n", SatStatusString(result).c_str());
  printf("c conflicts: %lld\n", solver->num_failures());
  printf("c branches: %lld\n", solver->num_branches());
  printf("c propagations: %lld\n", solver->num_propagations());
  printf("c walltime: %f\n", wall_timer.Get());
  printf("c usertime: %f\n", user_timer.Get());
  printf("c deterministic time: %f\n", solver->deterministic_time());
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
