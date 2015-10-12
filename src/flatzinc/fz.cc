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

// This is the skeleton for the official flatzinc interpreter.  Much
// of the funcionnalities are fixed (name of parameters, format of the
// input): see http://www.minizinc.org/downloads/doc-1.6/flatzinc-spec.pdf

#if defined(__GNUC__)  // Linux or Mac OS X.
#include <signal.h>
#endif  // __GNUC__

#include <iostream>  // NOLINT
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/stringprintf.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/threadpool.h"
#include "base/timer.h"
#include "flatzinc/model.h"
#include "flatzinc/parser.h"
#include "flatzinc/presolve.h"
#include "flatzinc/search.h"
#include "flatzinc/solver.h"

DEFINE_int32(log_period, 10000000, "Search log period");
DEFINE_bool(all, false, "Search for all solutions");
DEFINE_bool(free, false, "Ignore search annotations");
DEFINE_bool(last_conflict, false, "Use last conflict search hints");
DEFINE_int32(num_solutions, 0, "Number of solution to search for");
DEFINE_int32(time_limit, 0, "time limit in ms");
DEFINE_int32(workers, 0, "Number of workers");
DEFINE_bool(use_impact, false, "Use impact based search");
DEFINE_double(restart_log_size, -1, "Restart log size for impact search");
DEFINE_int32(luby_restart, -1, "Luby restart factor, <= 0 = no luby");
DEFINE_int32(heuristic_period, 100, "Period to call heuristics in free search");
DEFINE_bool(verbose_impact, false, "Verbose impact");
DEFINE_bool(verbose_mt, false, "Verbose Multi-Thread");
DEFINE_bool(presolve, true, "Use presolve.");
DEFINE_bool(read_from_stdin, false, "Read the FlatZinc from stdin, not from a file");

DECLARE_bool(fz_logging);
DECLARE_bool(log_prefix);
DECLARE_bool(use_sat);

using operations_research::ThreadPool;
extern void interrupt_handler(int s);

namespace operations_research {
void Solve(const FzModel* const model, const FzSolverParameters& parameters,
           FzParallelSupportInterface* parallel_support) {
  FzSolver solver(*model);
  CHECK(solver.Extract());
  solver.Solve(parameters, parallel_support);
}

void SequentialRun(const FzModel* model) {
  FzSolverParameters parameters;
  parameters.all_solutions = FLAGS_all;
  parameters.free_search = FLAGS_free;
  parameters.last_conflict = FLAGS_last_conflict;
  parameters.heuristic_period = FLAGS_heuristic_period;
  parameters.ignore_unknown = false;
  parameters.log_period = FLAGS_log_period;
  parameters.luby_restart = FLAGS_luby_restart;
  parameters.num_solutions = FLAGS_num_solutions;
  parameters.restart_log_size = FLAGS_restart_log_size;
  parameters.threads = FLAGS_workers;
  parameters.time_limit_in_ms = FLAGS_time_limit;
  parameters.use_log = FLAGS_fz_logging;
  parameters.verbose_impact = FLAGS_verbose_impact;
  parameters.worker_id = -1;
  parameters.search_type =
      FLAGS_use_impact ? FzSolverParameters::IBS : FzSolverParameters::DEFAULT;

  std::unique_ptr<FzParallelSupportInterface> parallel_support(
      MakeSequentialSupport(FLAGS_all, FLAGS_num_solutions));
  Solve(model, parameters, parallel_support.get());
}

void ParallelRun(const FzModel* const model, int worker_id,
                 FzParallelSupportInterface* parallel_support) {
  FzSolverParameters parameters;
  parameters.all_solutions = FLAGS_all;
  parameters.heuristic_period = FLAGS_heuristic_period;
  parameters.ignore_unknown = false;
  parameters.log_period = 0;
  parameters.luby_restart = -1;
  parameters.num_solutions = FLAGS_num_solutions;
  parameters.random_seed = worker_id * 10;
  parameters.threads = FLAGS_workers;
  parameters.time_limit_in_ms = FLAGS_time_limit;
  parameters.use_log = false;
  parameters.verbose_impact = false;
  parameters.worker_id = worker_id;
  switch (worker_id) {
    case 0: {
      parameters.free_search = false;
      parameters.last_conflict = false;
      parameters.search_type = operations_research::FzSolverParameters::DEFAULT;
      parameters.restart_log_size = -1.0;
      break;
    }
    case 1: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type =
          operations_research::FzSolverParameters::MIN_SIZE;
      parameters.restart_log_size = -1.0;
      break;
    }
    case 2: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type = operations_research::FzSolverParameters::IBS;
      parameters.restart_log_size = FLAGS_restart_log_size;
      break;
    }
    case 3: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type =
          operations_research::FzSolverParameters::FIRST_UNBOUND;
      parameters.restart_log_size = -1.0;
      parameters.heuristic_period = 10000000;
      break;
    }
    case 4: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type = operations_research::FzSolverParameters::DEFAULT;
      parameters.restart_log_size = -1.0;
      parameters.heuristic_period = 30;
      parameters.run_all_heuristics = true;
      break;
    }
    default: {
      parameters.free_search = true;
      parameters.last_conflict = false;
      parameters.search_type =
          worker_id % 2 == 0
              ? operations_research::FzSolverParameters::RANDOM_MIN
              : operations_research::FzSolverParameters::RANDOM_MAX;
      parameters.restart_log_size = -1.0;
      parameters.luby_restart = 250;
    }
  }
  Solve(model, parameters, parallel_support);
}

void FixAndParseParameters(int* argc, char*** argv) {
  FLAGS_log_prefix = false;
  char all_param[] = "--all";
  char free_param[] = "--free";
  char workers_param[] = "--workers";
  char solutions_param[] = "--num_solutions";
  char logging_param[] = "--fz_logging";
  char verbose_param[] = "--fz_verbose";
  char debug_param[] = "--fz_debug";
  for (int i = 1; i < *argc; ++i) {
    if (strcmp((*argv)[i], "-a") == 0) {
      (*argv)[i] = all_param;
    }
    if (strcmp((*argv)[i], "-f") == 0) {
      (*argv)[i] = free_param;
    }
    if (strcmp((*argv)[i], "-p") == 0) {
      (*argv)[i] = workers_param;
    }
    if (strcmp((*argv)[i], "-n") == 0) {
      (*argv)[i] = solutions_param;
    }
    if (strcmp((*argv)[i], "-l") == 0) {
      (*argv)[i] = logging_param;
    }
    if (strcmp((*argv)[i], "-v") == 0) {
      (*argv)[i] = verbose_param;
    }
    if (strcmp((*argv)[i], "-d") == 0) {
      (*argv)[i] = debug_param;
    }
  }
  const char kUsage[] =
      "Usage: see flags.\nThis program parses and solve a flatzinc problem.";

  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(argc, argv, true);

  // Fix the number of solutions.
  if (FLAGS_num_solutions == 0) {  // not specified
    FLAGS_num_solutions = FLAGS_all ? kint32max : 1;
  }
}

void ParseAndRun(const std::string& input, int num_workers, bool input_is_filename) {
  WallTimer timer;
  timer.Start();
  std::string problem_name(input_is_filename ? input : "stdin");
  if (input_is_filename) {
    problem_name.resize(problem_name.size() - 4);
    size_t found = problem_name.find_last_of("/\\");
    if (found != std::string::npos) {
      problem_name = problem_name.substr(found + 1);
    }
  }
  FzModel model(problem_name);
  if (input_is_filename) {
    CHECK(ParseFlatzincFile(input, &model));
  } else {
    CHECK(ParseFlatzincString(input, &model));
  }

  FZLOG << "File " << (input_is_filename ? input : "stdin")
        << " parsed in " << timer.GetInMs() << " ms"
        << FZENDL;
  FzPresolver presolve;
  presolve.CleanUpModelForTheCpSolver(&model, FLAGS_use_sat);
  if (FLAGS_presolve) {
    FZLOG << "Presolve model" << FZENDL;
    timer.Reset();
    timer.Start();
    presolve.Run(&model);
    FZLOG << "  - done in " << timer.GetInMs() << " ms" << FZENDL;
  }
  FzModelStatistics stats(model);
  stats.PrintStatistics();

#if defined(__GNUC__)
  signal(SIGINT, &interrupt_handler);
#endif

  if (num_workers == 0) {
    operations_research::SequentialRun(&model);
  } else {
    std::unique_ptr<operations_research::FzParallelSupportInterface>
        parallel_support(operations_research::MakeMtSupport(
            FLAGS_all, FLAGS_num_solutions, FLAGS_verbose_mt));
    {
      ThreadPool pool("Parallel FlatZinc", num_workers);
      for (int w = 0; w < num_workers; ++w) {
        pool.Add(NewCallback(ParallelRun, &model, w, parallel_support.get()));
      }
      pool.StartWorkers();
    }
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::FixAndParseParameters(&argc, &argv);
  if (FLAGS_read_from_stdin) { // allow users to pipe in the FlatZinc via stdin
    std::string inputText = "";
    std::string currentLine;
    while (std::getline(std::cin, currentLine)) {
      inputText.append(currentLine);
    }

    operations_research::ParseAndRun(inputText, FLAGS_workers, false);
  } else {
    if (argc <= 1) {
      LOG(ERROR) << "Usage: " << argv[0] << " <file>";
      exit(EXIT_FAILURE);
    }
    operations_research::ParseAndRun(argv[1], FLAGS_workers, true);
  }
  return 0;
}
