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

// This binary reads an input file in the flatzinc format (see
// http://www.minizinc.org/), parses it, and spits out the model it
// has built.

#include <string>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/parser.h"
#include "ortools/flatzinc/presolve.h"

DEFINE_string(file, "", "Input file in the flatzinc format.");
DEFINE_bool(print, false, "Print model.");
DEFINE_bool(presolve, false, "Presolve loaded file.");
DEFINE_bool(statistics, false, "Print model statistics");

namespace operations_research {
namespace fz {
void ParseFile(const std::string& filename, bool presolve) {
  FZLOG << "Parsing " << filename << FZENDL;

  std::string problem_name = filename;
  // Remove the .fzn extension.
  CHECK(strings::EndsWith(problem_name, ".fzn"));
  problem_name.resize(problem_name.size() - 4);
  // Remove the leading path if present.
  const size_t found = problem_name.find_last_of("/\\");
  if (found != std::string::npos) {
    problem_name = problem_name.substr(found + 1);
  }

  Model model(problem_name);
  CHECK(ParseFlatzincFile(filename, &model));
  if (presolve) {
    Presolver presolve;
    presolve.CleanUpModelForTheCpSolver(&model, /*use_sat=*/true);
    presolve.Run(&model);
  }
  if (FLAGS_statistics) {
    ModelStatistics stats(model);
    stats.BuildStatistics();
    stats.PrintStatistics();
  }
  if (FLAGS_print) {
    FZLOG << model.DebugString() << FZENDL;
  }
}
}  // namespace fz
}  // namespace operations_research

int main(int argc, char** argv) {
  FLAGS_log_prefix = false;
  const char kUsage[] =
      "Parses a flatzinc .fzn file, optionally presolve it, and prints it in "
      "human-readable format";
  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::fz::ParseFile(FLAGS_file, FLAGS_presolve);
  return 0;
}
