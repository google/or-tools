#include <iostream>  // NOLINT
#include <fstream>  // NOLINT
#include <cstring>
#include "base/unique_ptr.h"
#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/threadpool.h"
#include "flatzinc/flatzinc.h"

DEFINE_int32(log_period, 10000000, "Search log period");
DEFINE_bool(all, false, "Search for all solutions");
DEFINE_bool(free, false, "Ignore search annotations");
DEFINE_int32(num_solutions, 0, "Number of solution to search for");
DEFINE_int32(time_limit, 0, "time limit in ms");
DEFINE_int32(workers, 0, "Number of workers");
DEFINE_int32(simplex_frequency, 0, "Simplex frequency, 0 = no simplex");
DEFINE_bool(use_impact, false, "Use impact based search");
DEFINE_double(restart_log_size, -1, "Restart log size for impact search");
DEFINE_int32(luby_restart, -1, "Luby restart factor, <= 0 = no luby");
DEFINE_int32(heuristic_period, 100, "Period to call heuristics in free search");
DEFINE_bool(verbose_impact, false, "Verbose impact");
DEFINE_bool(verbose_mt, false, "Verbose Multi-Thread");

DECLARE_bool(log_prefix);
DECLARE_bool(logging);

using operations_research::ThreadPool;

namespace operations_research {
int Run(const std::string& file, const FlatZincSearchParameters& parameters,
        FzParallelSupport* const parallel_support) {
  FlatZincModel fz_model;
  if (file == "-") {
    if (!fz_model.Parse(std::cin)) {
      return -1;
    }
  } else {
    parallel_support->Init(
        parameters.worker_id,
        StringPrintf("%%%%  model:                %s", file.c_str()));
    if (!fz_model.Parse(file)) {
      return -1;
    }
  }

  fz_model.Solve(parameters, parallel_support);
  return 0;
}

void ParallelRun(char* const file, int worker_id,
                 FzParallelSupport* const parallel_support) {
  operations_research::FlatZincSearchParameters parameters;
  parameters.all_solutions = FLAGS_all;
  parameters.heuristic_period = FLAGS_heuristic_period;
  parameters.ignore_unknown = false;
  parameters.log_period = 0;
  parameters.luby_restart = -1;
  parameters.num_solutions = FLAGS_num_solutions;
  parameters.random_seed = worker_id * 10;
  parameters.simplex_frequency = FLAGS_simplex_frequency;
  parameters.threads = FLAGS_workers;
  parameters.time_limit_in_ms = FLAGS_time_limit;
  parameters.use_log = false;
  parameters.verbose_impact = false;
  parameters.worker_id = worker_id;
  switch (worker_id) {
    case 0: {
      parameters.free_search = false;
      parameters.search_type =
          operations_research::FlatZincSearchParameters::DEFAULT;
      parameters.restart_log_size = -1.0;
      break;
    }
    case 1: {
      parameters.free_search = true;
      parameters.search_type =
          operations_research::FlatZincSearchParameters::MIN_SIZE;
      parameters.restart_log_size = -1.0;
      break;
    }
    case 2: {
      parameters.free_search = true;
      parameters.search_type =
          operations_research::FlatZincSearchParameters::IBS;
      parameters.restart_log_size = FLAGS_restart_log_size;
      break;
    }
    case 3: {
      parameters.free_search = true;
      parameters.search_type =
          operations_research::FlatZincSearchParameters::FIRST_UNBOUND;
      parameters.restart_log_size = -1.0;
      parameters.heuristic_period = 10000000;
      break;
    }
    case 4: {
      parameters.free_search = true;
      parameters.search_type =
          operations_research::FlatZincSearchParameters::DEFAULT;
      parameters.restart_log_size = -1.0;
      parameters.heuristic_period = 30;
      parameters.run_all_heuristics = true;
      break;
    }
    default: {
      parameters.free_search = true;
      parameters.search_type =
          worker_id % 2 == 0
              ? operations_research::FlatZincSearchParameters::RANDOM_MIN
              : operations_research::FlatZincSearchParameters::RANDOM_MAX;
      parameters.restart_log_size = -1.0;
      parameters.luby_restart = 250;
    }
  }
  Run(file, parameters, parallel_support);
}

void SequentialRun(char* const file) {
  FlatZincSearchParameters parameters;
  parameters.all_solutions = FLAGS_all;
  parameters.free_search = FLAGS_free;
  parameters.heuristic_period = FLAGS_heuristic_period;
  parameters.ignore_unknown = false;
  parameters.log_period = FLAGS_log_period;
  parameters.luby_restart = FLAGS_luby_restart;
  parameters.num_solutions = FLAGS_num_solutions;
  parameters.restart_log_size = FLAGS_restart_log_size;
  parameters.simplex_frequency = FLAGS_simplex_frequency;
  parameters.threads = FLAGS_workers;
  parameters.time_limit_in_ms = FLAGS_time_limit;
  parameters.use_log = FLAGS_logging;
  parameters.verbose_impact = FLAGS_verbose_impact;
  parameters.worker_id = -1;
  parameters.search_type =
      FLAGS_use_impact ? FlatZincSearchParameters::IBS
      : FlatZincSearchParameters::DEFAULT;

  std::unique_ptr<FzParallelSupport> parallel_support(
      operations_research::MakeSequentialSupport(parameters.all_solutions,
                                                 parameters.num_solutions,
                                                 FLAGS_verbose_mt));
  Run(file, parameters, parallel_support.get());
}

void FixAndParseParameters(int* argc, char*** argv) {
  FLAGS_log_prefix = false;
  char all_param[] = "--all";
  char free_param[] = "--free";
  char workers_param[] = "--workers";
  char solutions_param[] = "--num_solutions";
  char logging_param[] = "--logging";
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
  }
  google::ParseCommandLineFlags( argc, argv, true);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::FixAndParseParameters(&argc, &argv);
  if (argc <= 1) {
    LOG(ERROR) << "Usage: " << argv[0] << " <file>";
    exit(EXIT_FAILURE);
  }
  // Fix the number of solutions.
  if (FLAGS_num_solutions == 0) {  // not specified
    FLAGS_num_solutions = FLAGS_all ? kint32max : 1;
  }

  if (FLAGS_workers == 0) {
    operations_research::SequentialRun(argv[1]);
  } else {
    std::unique_ptr<operations_research::FzParallelSupport> parallel_support(
        operations_research::MakeMtSupport(
            FLAGS_all, FLAGS_num_solutions, FLAGS_verbose_mt));
    {
      ThreadPool pool("Parallel FlatZinc", FLAGS_workers);
      pool.StartWorkers();
      for (int w = 0; w < FLAGS_workers; ++w) {
        pool.Add(NewCallback(&operations_research::ParallelRun, argv[1], w,
                             parallel_support.get()));
      }
    }
    return 0;
  }
}
