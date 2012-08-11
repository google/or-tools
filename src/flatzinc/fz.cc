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

DEFINE_int32(log_frequency, 10000000, "Search log frequency");
DEFINE_bool(log, false, "Show search log");
DEFINE_bool(all, false, "Search for all solutions");
DEFINE_bool(free, false, "Ignore search annotations");
DEFINE_int32(num_solutions, 0, "Number of solution to search for");
DEFINE_int32(time_limit, 0, "time limit in ms");
DEFINE_int32(threads, 0, "threads");
DEFINE_int32(simplex_frequency, 0, "Simplex frequency, 0 = no simplex");
DEFINE_bool(use_impact, false, "Use impact based search");
DEFINE_double(restart_log_size, -1, "Restart log size for impact search");
DEFINE_int32(luby_factor, -1, "Luby restart factor, <= 0 = no luby");
DEFINE_bool(verbose_impact, false, "Verbose impact");
DECLARE_bool(log_prefix);

namespace operations_research {
void Run(const std::string& file) {
  FlatZincModel fz_model;
  if (file == "-") {
    fz_model.Parse(std::cin);
  } else {
    std::cout << "%%  model:                " << file << std::endl;
    fz_model.Parse(file);
  }

  fz_model.Solve(FLAGS_log_frequency,
                 FLAGS_log,
                 FLAGS_all,
                 FLAGS_free,
                 FLAGS_num_solutions,
                 FLAGS_time_limit,
                 FLAGS_simplex_frequency,
                 FLAGS_use_impact,
                 FLAGS_restart_log_size,
                 FLAGS_luby_factor,
                 FLAGS_verbose_impact);
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
  operations_research::Run(argv[1]);
  return 0;
}
