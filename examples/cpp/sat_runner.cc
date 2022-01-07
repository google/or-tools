// Copyright 2010-2021 Google LLC
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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "examples/cpp/opb_reader.h"
#include "examples/cpp/sat_cnf_reader.h"
#include "google/protobuf/text_format.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/drat_proof_handler.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/symmetry.h"
#include "ortools/util/file_util.h"
#include "ortools/util/time_limit.h"

ABSL_FLAG(
    std::string, input, "",
    "Required: input file of the problem to solve. Many format are supported:"
    ".cnf (sat, max-sat, weighted max-sat), .opb (pseudo-boolean sat/optim) "
    "and by default the LinearBooleanProblem proto (binary or text).");

ABSL_FLAG(
    std::string, output, "",
    "If non-empty, write the input problem as a LinearBooleanProblem proto to "
    "this file. By default it uses the binary format except if the file "
    "extension is '.txt'. If the problem is SAT, a satisfiable assignment is "
    "also written to the file.");

ABSL_FLAG(bool, output_cnf_solution, false,
          "If true and the problem was solved to optimality, this output "
          "the solution to stdout in cnf form.\n");

ABSL_FLAG(std::string, params, "",
          "Parameters for the sat solver in a text format of the "
          "SatParameters proto, example: --params=use_conflicts:true.");

ABSL_FLAG(bool, strict_validity, false,
          "If true, stop if the given input is invalid (duplicate literals, "
          "out of range, zero cofficients, etc.)");

ABSL_FLAG(
    std::string, lower_bound, "",
    "If not empty, look for a solution with an objective value >= this bound.");

ABSL_FLAG(
    std::string, upper_bound, "",
    "If not empty, look for a solution with an objective value <= this bound.");

ABSL_FLAG(bool, fu_malik, false,
          "If true, search the optimal solution with the Fu & Malik algo.");

ABSL_FLAG(bool, wpm1, false,
          "If true, search the optimal solution with the WPM1 algo.");

ABSL_FLAG(bool, qmaxsat, false,
          "If true, search the optimal solution with a linear scan and "
          " the cardinality encoding used in qmaxsat.");

ABSL_FLAG(bool, core_enc, false,
          "If true, search the optimal solution with the core-based "
          "cardinality encoding algo.");

ABSL_FLAG(bool, linear_scan, false,
          "If true, search the optimal solution with the linear scan algo.");

ABSL_FLAG(int, randomize, 500,
          "If positive, solve that many times the problem with a random "
          "decision heuristic before trying to optimize it.");

ABSL_FLAG(bool, use_symmetry, false,
          "If true, find and exploit the eventual symmetries "
          "of the problem.");

ABSL_FLAG(bool, presolve, true,
          "Only work on pure SAT problem. If true, presolve the problem.");

ABSL_FLAG(bool, probing, false, "If true, presolve the problem using probing.");

ABSL_FLAG(bool, use_cp_model, true,
          "Whether to interpret everything as a CpModelProto or "
          "to read by default a CpModelProto.");

ABSL_FLAG(bool, reduce_memory_usage, false,
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
  for (const int64_t value : objective.coefficients()) {
    if (value < 0) best_bound += Coefficient(value);
  }
  return AddOffsetAndScaleObjectiveValue(problem, best_bound);
}

bool LoadBooleanProblem(const std::string& filename,
                        LinearBooleanProblem* problem, CpModelProto* cp_model) {
  if (absl::EndsWith(filename, ".opb") ||
      absl::EndsWith(filename, ".opb.bz2")) {
    OpbReader reader;
    if (!reader.Load(filename, problem)) {
      LOG(FATAL) << "Cannot load file '" << filename << "'.";
    }
  } else if (absl::EndsWith(filename, ".cnf") ||
             absl::EndsWith(filename, ".cnf.gz") ||
             absl::EndsWith(filename, ".wcnf") ||
             absl::EndsWith(filename, ".wcnf.gz")) {
    SatCnfReader reader;
    if (absl::GetFlag(FLAGS_fu_malik) || absl::GetFlag(FLAGS_linear_scan) ||
        absl::GetFlag(FLAGS_wpm1) || absl::GetFlag(FLAGS_qmaxsat) ||
        absl::GetFlag(FLAGS_core_enc)) {
      reader.InterpretCnfAsMaxSat(true);
    }
    if (absl::GetFlag(FLAGS_use_cp_model)) {
      if (!reader.Load(filename, cp_model)) {
        LOG(FATAL) << "Cannot load file '" << filename << "'.";
      }
    } else {
      if (!reader.Load(filename, problem)) {
        LOG(FATAL) << "Cannot load file '" << filename << "'.";
      }
    }
  } else if (absl::GetFlag(FLAGS_use_cp_model)) {
    LOG(INFO) << "Reading a CpModelProto.";
    *cp_model = ReadFileToProtoOrDie<CpModelProto>(filename);
  } else {
    LOG(INFO) << "Reading a LinearBooleanProblem.";
    *problem = ReadFileToProtoOrDie<LinearBooleanProblem>(filename);
  }
  return true;
}

std::string SolutionString(const LinearBooleanProblem& problem,
                           const std::vector<bool>& assignment) {
  std::string output;
  BooleanVariable limit(problem.original_num_variables());
  for (BooleanVariable index(0); index < limit; ++index) {
    if (index > 0) output += " ";
    absl::StrAppend(&output,
                    Literal(index, assignment[index.value()]).SignedValue());
  }
  return output;
}

// To benefit from the operations_research namespace, we put all the main() code
// here.
int Run() {
  SatParameters parameters;
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  // Parse the --params flag.
  parameters.set_log_search_progress(true);
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_params), &parameters))
        << absl::GetFlag(FLAGS_params);
  }

  // Initialize the solver.
  std::unique_ptr<SatSolver> solver(new SatSolver());
  solver->SetParameters(parameters);

  // Read the problem.
  LinearBooleanProblem problem;
  CpModelProto cp_model;
  if (!LoadBooleanProblem(absl::GetFlag(FLAGS_input), &problem, &cp_model)) {
    CpSolverResponse response;
    response.set_status(CpSolverStatus::MODEL_INVALID);
    return EXIT_SUCCESS;
  }
  if (!absl::GetFlag(FLAGS_use_cp_model)) {
    LOG(INFO) << "Converting to CpModelProto ...";
    cp_model = BooleanProblemToCpModelproto(problem);
  }

  // TODO(user): clean this hack. Ideally LinearBooleanProblem should be
  // completely replaced by the more general CpModelProto.
  if (absl::GetFlag(FLAGS_use_cp_model)) {
    problem.Clear();  // We no longer need it, release memory.
    Model model;
    model.Add(NewSatParameters(parameters));
    const CpSolverResponse response = SolveCpModel(cp_model, &model);

    if (!absl::GetFlag(FLAGS_output).empty()) {
      if (absl::EndsWith(absl::GetFlag(FLAGS_output), "txt")) {
        CHECK_OK(file::SetTextProto(absl::GetFlag(FLAGS_output), response,
                                    file::Defaults()));
      } else {
        CHECK_OK(file::SetBinaryProto(absl::GetFlag(FLAGS_output), response,
                                      file::Defaults()));
      }
    }

    // The SAT competition requires a particular exit code and since we don't
    // really use it for any other purpose, we comply.
    if (response.status() == CpSolverStatus::OPTIMAL) return 10;
    if (response.status() == CpSolverStatus::FEASIBLE) return 10;
    if (response.status() == CpSolverStatus::INFEASIBLE) return 20;
    return EXIT_SUCCESS;
  }

  if (absl::GetFlag(FLAGS_strict_validity)) {
    const absl::Status status = ValidateBooleanProblem(problem);
    if (!status.ok()) {
      LOG(ERROR) << "Invalid Boolean problem: " << status.message();
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
  if (absl::GetFlag(FLAGS_probing)) {
    // TODO(user): This is nice for testing, but consumes memory.
    original_problem = problem;
    ProbeAndSimplifyProblem(&probing_postsolver, &problem);
  }

  // Load the problem into the solver.
  if (absl::GetFlag(FLAGS_reduce_memory_usage)) {
    if (!LoadAndConsumeBooleanProblem(&problem, solver.get())) {
      LOG(INFO) << "UNSAT when loading the problem.";
    }
  } else {
    if (!LoadBooleanProblem(problem, solver.get())) {
      LOG(INFO) << "UNSAT when loading the problem.";
    }
  }
  auto strtoint64 = [](const std::string& word) {
    int64_t value = 0;
    if (!word.empty()) CHECK(absl::SimpleAtoi(word, &value));
    return value;
  };
  if (!AddObjectiveConstraint(
          problem, !absl::GetFlag(FLAGS_lower_bound).empty(),
          Coefficient(strtoint64(absl::GetFlag(FLAGS_lower_bound))),
          !absl::GetFlag(FLAGS_upper_bound).empty(),
          Coefficient(strtoint64(absl::GetFlag(FLAGS_upper_bound))),
          solver.get())) {
    LOG(INFO) << "UNSAT when setting the objective constraint.";
  }

  // Symmetries!
  //
  // TODO(user): To make this compatible with presolve, we just need to run
  // it after the presolve step.
  if (absl::GetFlag(FLAGS_use_symmetry)) {
    CHECK(!absl::GetFlag(FLAGS_reduce_memory_usage)) << "incompatible";
    CHECK(!absl::GetFlag(FLAGS_presolve)) << "incompatible";
    LOG(INFO) << "Finding symmetries of the problem.";
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    FindLinearBooleanProblemSymmetries(problem, &generators);
    std::unique_ptr<SymmetryPropagator> propagator(new SymmetryPropagator);
    for (int i = 0; i < generators.size(); ++i) {
      propagator->AddSymmetry(std::move(generators[i]));
    }
    solver->AddPropagator(propagator.get());
    solver->TakePropagatorOwnership(std::move(propagator));
  }

  // Optimize?
  std::vector<bool> solution;
  SatSolver::Status result = SatSolver::LIMIT_REACHED;
  if (absl::GetFlag(FLAGS_fu_malik) || absl::GetFlag(FLAGS_linear_scan) ||
      absl::GetFlag(FLAGS_wpm1) || absl::GetFlag(FLAGS_qmaxsat) ||
      absl::GetFlag(FLAGS_core_enc)) {
    if (absl::GetFlag(FLAGS_randomize) > 0 &&
        (absl::GetFlag(FLAGS_linear_scan) || absl::GetFlag(FLAGS_qmaxsat))) {
      CHECK(!absl::GetFlag(FLAGS_reduce_memory_usage)) << "incompatible";
      absl::BitGen bitgen;
      result = SolveWithRandomParameters(STDOUT_LOG, problem,
                                         absl::GetFlag(FLAGS_randomize), bitgen,
                                         solver.get(), &solution);
    }
    if (result == SatSolver::LIMIT_REACHED) {
      if (absl::GetFlag(FLAGS_qmaxsat)) {
        solver = absl::make_unique<SatSolver>();
        solver->SetParameters(parameters);
        CHECK(LoadBooleanProblem(problem, solver.get()));
        result = SolveWithCardinalityEncoding(STDOUT_LOG, problem, solver.get(),
                                              &solution);
      } else if (absl::GetFlag(FLAGS_core_enc)) {
        result = SolveWithCardinalityEncodingAndCore(STDOUT_LOG, problem,
                                                     solver.get(), &solution);
      } else if (absl::GetFlag(FLAGS_fu_malik)) {
        result = SolveWithFuMalik(STDOUT_LOG, problem, solver.get(), &solution);
      } else if (absl::GetFlag(FLAGS_wpm1)) {
        result = SolveWithWPM1(STDOUT_LOG, problem, solver.get(), &solution);
      } else if (absl::GetFlag(FLAGS_linear_scan)) {
        result =
            SolveWithLinearScan(STDOUT_LOG, problem, solver.get(), &solution);
      }
    }
  } else {
    // Only solve the decision version.
    parameters.set_log_search_progress(true);
    solver->SetParameters(parameters);
    if (absl::GetFlag(FLAGS_presolve)) {
      std::unique_ptr<TimeLimit> time_limit =
          TimeLimit::FromParameters(parameters);
      SolverLogger logger;
      result = SolveWithPresolve(&solver, time_limit.get(), &solution,
                                 /*drat_proof_handler=*/nullptr, &logger);
      if (result == SatSolver::FEASIBLE) {
        CHECK(IsAssignmentValid(problem, solution));
      }
    } else {
      result = solver->Solve();
      if (result == SatSolver::FEASIBLE) {
        ExtractAssignment(problem, *solver, &solution);
        CHECK(IsAssignmentValid(problem, solution));
      }
    }
  }

  // Print the solution status.
  if (result == SatSolver::FEASIBLE) {
    if (absl::GetFlag(FLAGS_fu_malik) || absl::GetFlag(FLAGS_linear_scan) ||
        absl::GetFlag(FLAGS_wpm1) || absl::GetFlag(FLAGS_core_enc)) {
      absl::PrintF("s OPTIMUM FOUND\n");
      CHECK(!solution.empty());
      const Coefficient objective = ComputeObjectiveValue(problem, solution);
      scaled_best_bound = AddOffsetAndScaleObjectiveValue(problem, objective);

      // Postsolve.
      if (absl::GetFlag(FLAGS_probing)) {
        solution = probing_postsolver.PostsolveSolution(solution);
        problem = original_problem;
      }
    } else {
      absl::PrintF("s SATISFIABLE\n");
    }

    // Check and output the solution.
    CHECK(IsAssignmentValid(problem, solution));
    if (absl::GetFlag(FLAGS_output_cnf_solution)) {
      absl::PrintF("v %s\n", SolutionString(problem, solution));
    }
    if (!absl::GetFlag(FLAGS_output).empty()) {
      CHECK(!absl::GetFlag(FLAGS_reduce_memory_usage)) << "incompatible";
      if (result == SatSolver::FEASIBLE) {
        StoreAssignment(solver->Assignment(), problem.mutable_assignment());
      }
      if (absl::EndsWith(absl::GetFlag(FLAGS_output), ".txt")) {
        CHECK_OK(file::SetTextProto(absl::GetFlag(FLAGS_output), problem,
                                    file::Defaults()));
      } else {
        CHECK_OK(file::SetBinaryProto(absl::GetFlag(FLAGS_output), problem,
                                      file::Defaults()));
      }
    }
  }
  if (result == SatSolver::INFEASIBLE) {
    absl::PrintF("s UNSATISFIABLE\n");
  }

  // Print status.
  absl::PrintF("c status: %s\n", SatStatusString(result));

  // Print objective value.
  if (solution.empty()) {
    absl::PrintF("c objective: na\n");
    absl::PrintF("c best bound: na\n");
  } else {
    const Coefficient objective = ComputeObjectiveValue(problem, solution);
    absl::PrintF("c objective: %.16g\n",
                 AddOffsetAndScaleObjectiveValue(problem, objective));
    absl::PrintF("c best bound: %.16g\n", scaled_best_bound);
  }

  // Print final statistics.
  absl::PrintF("c booleans: %d\n", solver->NumVariables());
  absl::PrintF("c conflicts: %d\n", solver->num_failures());
  absl::PrintF("c branches: %d\n", solver->num_branches());
  absl::PrintF("c propagations: %d\n", solver->num_propagations());
  absl::PrintF("c walltime: %f\n", wall_timer.Get());
  absl::PrintF("c usertime: %f\n", user_timer.Get());
  absl::PrintF("c deterministic_time: %f\n", solver->deterministic_time());

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\n"
    "This program solves a given Boolean linear problem.";

int main(int argc, char** argv) {
  // By default, we want to show how the solver progress. Note that this needs
  // to be set before InitGoogle() which has the nice side-effect of allowing
  // the user to override it.
  google::InitGoogleLogging(kUsage);
  absl::ParseCommandLine(argc, argv);
  absl::SetFlag(&FLAGS_alsologtostderr, true);
  return operations_research::sat::Run();
}
