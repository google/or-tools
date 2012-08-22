/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *  Modified:
 *     Laurent Perron (lperron@google.com)
 *
 *  Copyright:
 *     Guido Tack, 2007
 *
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "flatzinc/flatzinc.h"

DEFINE_int32(log_period, 10000000, "Search log period");
DEFINE_bool(use_log, false, "Show search log");
DEFINE_bool(all, false, "Search for all solutions");
DEFINE_bool(free, false, "Ignore search annotations");
DEFINE_int32(num_solutions, 0, "Number of solution to search for");
DEFINE_int32(time_limit, 0, "time limit in ms");
DEFINE_int32(threads, 0, "threads");
DEFINE_int32(simplex_frequency, 0, "Simplex frequency, 0 = no simplex");
DEFINE_bool(use_impact, false, "Use impact based search");
DEFINE_double(restart_log_size, -1, "Restart log size for impact search");
DEFINE_int32(luby_restart, -1, "Luby restart factor, <= 0 = no luby");
DEFINE_int32(heuristic_period, 30, "Period to call heuristics in free search");
DEFINE_bool(verbose_impact, false, "Verbose impact");
DECLARE_bool(log_prefix);

namespace operations_research {
int Run(const std::string& file,
        const FlatZincSearchParameters& parameters,
        FzParallelSupport* const parallel_support) {
  FlatZincModel fz_model;
  if (file == "-") {
    if (!fz_model.Parse(std::cin)) {
      return -1;
    }
  } else {
    parallel_support->Init(
        parameters.worker_id,
        StringPrintf("%%%%  model:                %s\n", file.c_str()));
    if (!fz_model.Parse(file)) {
      return -1;
    }
  }


  fz_model.Solve(parameters, parallel_support);
  return 0;
}
}  // namespace operations_research

int main(int argc, char** argv) {
  FLAGS_log_prefix=false;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-a") == 0) {
      argv[i] = "--all";
    }
    if (strcmp(argv[i], "-f") == 0) {
      argv[i] = "--free";
    }
    if (strcmp(argv[i], "-p") == 0) {
      argv[i] = "--threads";
    }
    if (strcmp(argv[i], "-n") == 0) {
      argv[i] = "--num_solutions";
    }
  }
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (argc <= 1) {
    LOG(ERROR) << "Usage: " << argv[0] << " <file>";
    exit(EXIT_FAILURE);
  }

  if (FLAGS_threads == 0) {
    operations_research::FlatZincSearchParameters parameters;
    parameters.all_solutions = FLAGS_all;
    parameters.free_search = FLAGS_free;
    parameters.heuristic_period = FLAGS_heuristic_period;
    parameters.ignore_unknown = false;
    parameters.log_period = FLAGS_log_period;
    parameters.luby_restart = FLAGS_luby_restart;
    parameters.num_solutions = FLAGS_num_solutions;
    parameters.restart_log_size = FLAGS_restart_log_size;
    parameters.simplex_frequency = FLAGS_simplex_frequency;
    parameters.threads = FLAGS_threads;
    parameters.time_limit_in_ms = FLAGS_time_limit;
    parameters.use_log = FLAGS_use_log;
    parameters.verbose_impact = FLAGS_verbose_impact;
    parameters.worker_id = -1;
    parameters.search_type = FLAGS_use_impact ?
        operations_research::FlatZincSearchParameters::IBS :
        operations_research::FlatZincSearchParameters::FIRST_UNBOUND;

    operations_research::FzParallelSupport* const parallel_support =
        operations_research::MakeSequentialSupport(parameters.all_solutions);
    return operations_research::Run(argv[1], parameters, parallel_support);
  } else {
    operations_research::FlatZincSearchParameters parameters;
    parameters.all_solutions = FLAGS_all;
    parameters.free_search = FLAGS_free;
    parameters.heuristic_period = FLAGS_heuristic_period;
    parameters.ignore_unknown = false;
    parameters.log_period = FLAGS_log_period;
    parameters.luby_restart = FLAGS_luby_restart;
    parameters.num_solutions = FLAGS_num_solutions;
    parameters.restart_log_size = FLAGS_restart_log_size;
    parameters.simplex_frequency = FLAGS_simplex_frequency;
    parameters.threads = FLAGS_threads;
    parameters.time_limit_in_ms = FLAGS_time_limit;
    parameters.use_log = FLAGS_use_log;
    parameters.verbose_impact = FLAGS_verbose_impact;
    parameters.worker_id = 0;
    parameters.search_type = FLAGS_use_impact ?
        operations_research::FlatZincSearchParameters::IBS :
        operations_research::FlatZincSearchParameters::FIRST_UNBOUND;
    operations_research::FzParallelSupport* const parallel_support =
        operations_research::MakeMtSupport(parameters.all_solutions);
    return operations_research::Run(argv[1], parameters, parallel_support);
  }
}
