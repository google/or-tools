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

DEFINE_bool(search_optimal, false,
            "If true, search for the optimal solution. "
            "The algorithm is currently really basic.");

DEFINE_int32(randomize, 100,
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

void RandomizeDecisionHeuristic(MTRandom* random, SatParameters* parameters) {
  // Random preferred variable order.
  const google::protobuf::EnumDescriptor* order_d =
      SatParameters::VariableOrder_descriptor();
  parameters->set_preferred_variable_order(
      static_cast<SatParameters::VariableOrder>(
          order_d->value(random->Uniform(order_d->value_count()))->number()));

  // Random polarity initial value.
  const google::protobuf::EnumDescriptor* polarity_d =
      SatParameters::Polarity_descriptor();
  parameters->set_initial_polarity(static_cast<SatParameters::Polarity>(
      polarity_d->value(random->Uniform(polarity_d->value_count()))->number()));

  // Other random parameters.
  parameters->set_use_phase_saving(random->OneIn(2));
  const std::vector<double> ratios = {0.0, 0.0, 0.0, 0.01, 1.0};
  parameters->set_random_polarity_ratio(ratios[random->Uniform(ratios.size())]);

  // IMPORTANT: SetParameters() will reinitialize the seed, so we must change
  // it.
  parameters->set_random_seed(parameters->random_seed() + 1);
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
    if (!reader.Load(filename, problem)) {
      LOG(FATAL) << "Cannot load file '" << filename << "'.";
    }
  } else {
    file::ReadFileToProtoOrDie(filename, problem);
  }
}

std::string SolutionString(const LinearBooleanProblem& problem,
                      const VariablesAssignment& assignment) {
  std::string output;
  VariableIndex limit(problem.original_num_variables());
  for (VariableIndex index(0); index < limit; ++index) {
    if (index > 0) output += " ";
    output += StringPrintf("%d", assignment.GetTrueLiteralForAssignedVariable(
                                                index).SignedValue());
  }
  return output;
}

void PrintObjective(double objective) {
  printf("o %lld\n", static_cast<int64>(objective));
}

int64 SolveWithDifferentParameters(const LinearBooleanProblem& problem,
                                   SatSolver* solver, TimeLimit* time_limit,
                                   int num_solve) {
  MTRandom random("A random seed.");
  SatParameters parameters = solver->parameters();

  // We start with a low limit (increased on each LIMIT_REACHED).
  parameters.set_log_search_progress(false);
  parameters.set_max_number_of_conflicts(10);

  int64 min_seen = std::numeric_limits<int64>::max();
  int64 max_seen = std::numeric_limits<int64>::min();
  int64 best = min_seen;
  for (int i = 0; i < num_solve; ++i) {
    solver->Backtrack(0);
    RandomizeDecisionHeuristic(&random, &parameters);
    parameters.set_max_time_in_seconds(time_limit->GetTimeLeft());
    solver->SetParameters(parameters);
    solver->ResetDecisionHeuristic();

    const bool use_obj = random.OneIn(4);
    if (use_obj) UseObjectiveForSatAssignmentPreference(problem, solver);

    const SatSolver::Status result = solver->Solve();
    if (result == SatSolver::LIMIT_REACHED) {
      printf("limit reached\n");
      parameters.set_max_number_of_conflicts(
          static_cast<int64>(1.1 * parameters.max_number_of_conflicts()));
      if (time_limit->LimitReached()) return best;
      continue;
    }

    CHECK_EQ(result, SatSolver::MODEL_SAT);
    CHECK(IsAssignmentValid(problem, solver->Assignment()));

    const Coefficient objective =
        ComputeObjectiveValue(problem, solver->Assignment());
    best = std::min(best, objective.value());
    const int64 scaled_objective =
        static_cast<int64>(GetScaledObjective(problem, objective));
    min_seen = std::min(min_seen, scaled_objective);
    max_seen = std::max(max_seen, scaled_objective);

    printf("objective preference: %s\n", use_obj ? "true" : "false");
    printf("%s", parameters.DebugString().c_str());
    printf("  %lld   [%lld, %lld]\n", scaled_objective, min_seen, max_seen);
  }
  return best;
}

// Same as Run() with --solve_optimal, no logging, and an output in the cnf
// format.
int RunWithCnfOutputFormat(std::string filename) {
  SatSolver solver;

  // Read the problem.
  LinearBooleanProblem problem;
  LoadBooleanProblem(filename, &problem);

  // Load the problem into the solver.
  if (!LoadBooleanProblem(problem, &solver)) {
    LOG(FATAL) << "Couldn't load problem '" << filename << "'.";
  }

  // This has a big positive impact on most problems.
  UseObjectiveForSatAssignmentPreference(problem, &solver);

  Coefficient objective = kCoefficientMax;
  while (true) {
    const SatSolver::Status result = solver.Solve();
    if (result == SatSolver::MODEL_UNSAT) {
      if (objective == kCoefficientMax) {
        printf("s UNSAT\n");
      } else {
        printf("s OPTIMUM FOUND\n");
        printf("v %s\n", SolutionString(problem, solver.Assignment()).c_str());
      }
      break;
    }
    if (result != SatSolver::MODEL_SAT) {
      printf("c LIMIT REACHED\n");
      break;
    }
    CHECK(IsAssignmentValid(problem, solver.Assignment()));
    const Coefficient old_objective = objective;
    objective = ComputeObjectiveValue(problem, solver.Assignment());
    PrintObjective(GetScaledObjective(problem, objective));
    CHECK_LT(objective, old_objective);
    solver.Backtrack(0);
    if (!AddObjectiveConstraint(problem, false, Coefficient(0), true,
                                objective - 1, &solver)) {
      printf("s OPTIMUM FOUND\n");
      printf("v %s\n", SolutionString(problem, solver.Assignment()).c_str());
      break;
    }
  }
  return EXIT_SUCCESS;
}

// To benefit from the operations_research namespace, we put all the main() code
// here.
int Run() {
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  SatParameters parameters;
  if (!FLAGS_params.empty()) {
    CHECK(google::protobuf::TextFormat::ParseFromString(FLAGS_params, &parameters))
        << FLAGS_params;
  }
  parameters.set_log_search_progress(true);

  // Enforce some parameters if we are looking for UNSAT core.
  if (FLAGS_refine_core) {
    parameters.set_unsat_proof(true);
    parameters.set_treat_binary_clauses_separately(false);
  }

  // Initialize the solver.
  SatSolver solver;
  solver.SetParameters(parameters);

  // Read the problem.
  LinearBooleanProblem problem;
  LoadBooleanProblem(FLAGS_input, &problem);


  // Load the problem into the solver.
  if (!LoadBooleanProblem(problem, &solver)) {
    LOG(FATAL) << "Couldn't load problem '" << FLAGS_input << "'.";
  }
  if (!AddObjectiveConstraint(
          problem, !FLAGS_lower_bound.empty(),
          Coefficient(atoi64(FLAGS_lower_bound)), !FLAGS_upper_bound.empty(),
          Coefficient(atoi64(FLAGS_upper_bound)), &solver)) {
    LOG(FATAL) << "Issue when setting the objective bounds.";
  }

  // Symmetries!
  if (FLAGS_use_symmetry) {
    LOG(INFO) << "Finding symmetries of the problem.";
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    FindLinearBooleanProblemSymmetries(problem, &generators);
    solver.AddSymmetries(&generators);
  }

  // Heuristics to drive the SAT search.
  UseObjectiveForSatAssignmentPreference(problem, &solver);

  // Basic search for the optimal value by calling multiple times the solver.
  if (FLAGS_search_optimal &&
      problem.type() == LinearBooleanProblem::MINIMIZATION) {
    TimeLimit time_limit(parameters.max_time_in_seconds());
    Coefficient objective = kCoefficientMax;
    int old_num_fixed_variables = 0;
    bool first_time = true;
    while (true) {
      if (first_time && FLAGS_randomize > 0) {
        first_time = false;
        solver.SetParameters(parameters);
        objective = SolveWithDifferentParameters(problem, &solver, &time_limit,
                                                 FLAGS_randomize);
        solver.SetParameters(parameters);
        solver.Backtrack(0);
        solver.ResetDecisionHeuristic();
        UseObjectiveForSatAssignmentPreference(problem, &solver);
      } else {
        const SatSolver::Status result = solver.Solve();
        if (result == SatSolver::MODEL_UNSAT) {
          if (objective == kCoefficientMax) {
            LOG(INFO) << "The problem is UNSAT";
            break;
          }
          LOG(INFO) << "Optimal found!";
          LOG(INFO) << "Objective = " << GetScaledObjective(problem, objective);
          LOG(INFO) << "Time = " << time_limit.GetElapsedTime();
          break;
        }
        if (result != SatSolver::MODEL_SAT) {
          LOG(INFO) << "Search aborted.";
          if (objective == kCoefficientMax) {
            LOG(INFO) << "No solution found!";
            LOG(INFO) << "Objective = " << kCoefficientMax;
          } else {
            LOG(INFO) << "Objective = " << GetScaledObjective(problem,
                                                              objective);
          }
          LOG(INFO) << "Time = " << time_limit.GetElapsedTime();
          break;
        }
        CHECK(IsAssignmentValid(problem, solver.Assignment()));
        const Coefficient old_objective = objective;
        objective = ComputeObjectiveValue(problem, solver.Assignment());
        PrintObjective(GetScaledObjective(problem, objective));
        CHECK_LT(objective, old_objective);
      }
      solver.Backtrack(0);
      if (objective != kCoefficientMax) {
        if (!AddObjectiveConstraint(problem, false, Coefficient(0), true,
                                    objective - 1, &solver)) {
          LOG(INFO) << "UNSAT (when tightenning the objective constraint).";
          LOG(INFO) << "Optimal found!";
          LOG(INFO) << "Objective = " << GetScaledObjective(problem, objective);
          LOG(INFO) << "Time = " << time_limit.GetElapsedTime();
          break;
        }
      }
      parameters.set_max_time_in_seconds(time_limit.GetTimeLeft());
      solver.SetParameters(parameters);

    }
    return EXIT_SUCCESS;
  }

  // Solve.
  const SatSolver::Status result = solver.Solve();
  if (result == SatSolver::MODEL_SAT) {
    CHECK(IsAssignmentValid(problem, solver.Assignment()));
  }

  // Unsat with verification.
  // Note(user): For now we just compute an UNSAT core and check it.
  if (result == SatSolver::MODEL_UNSAT && parameters.unsat_proof()) {
    std::vector<int> core;
    solver.ComputeUnsatCore(&core);
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
        LOG(INFO) << "Core #" << i << " checked, next size is " << core.size();
        ++i;
      } while (core.size() != old_core_size);
    }
  }

  if (!FLAGS_output.empty()) {
    if (result == SatSolver::MODEL_SAT) {
      StoreAssignment(solver.Assignment(), problem.mutable_assignment());
    }
    if (HasSuffixString(FLAGS_output, ".txt")) {
      file::WriteProtoToASCIIFileOrDie(problem, FLAGS_output);
    } else {
      file::WriteProtoToFileOrDie(problem, FLAGS_output);
    }
  }

  CHECK(FLAGS_expected_result == "undefined" ||
        (FLAGS_expected_result == "sat" && result == SatSolver::MODEL_SAT) ||
        (FLAGS_expected_result == "unsat" && result == SatSolver::MODEL_UNSAT));
  return EXIT_SUCCESS;
}
}  // namespace
}  // namespace sat
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\n"
    "This program solves a given sat problem.";

int main(int argc, char** argv) {
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (argc == 2) {
    printf("c %s\n", argv[1]);
    return operations_research::sat::RunWithCnfOutputFormat(argv[1]);
  } else {
    return operations_research::sat::Run();
  }
}
