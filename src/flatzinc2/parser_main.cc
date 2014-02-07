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
// This binary reads an input file in the flatzinc format (see
// http://www.minizinc.org/), parses it, and spits out the model it
// has built.

#include <string>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "flatzinc2/model.h"
#include "flatzinc2/parser.h"

DEFINE_string(file, "", "Input file in the flatzinc format");

int main(int argc, char** argv) {
  FLAGS_log_prefix = false;
  InitGoogle(
      "Parses a flatzinc .fzn file and prints it in human-readable "
      "format",
      &argc, &argv, /*remove_flags=*/true);
  std::string problem_name = FLAGS_file;
  // Remove the .fzn extension.
  QCHECK(problem_name.size() > 4 &&
         problem_name.substr(problem_name.size() - 4) == ".fzn")
      << "Please supply a valid data file name (ending with .fzn) with --file.";
  problem_name.resize(problem_name.size() - 4);
  // Remove the leading path if present.
  size_t basename_offset = problem_name.find_last_of("/\\");
  if (basename_offset != std::string::npos) {
    problem_name = problem_name.substr(basename_offset + 1);
  }
  // Parse the model and print it out.
  operations_research::FzModel model(problem_name);
  operations_research::ParseFlatzincFile(FLAGS_file, &model);
  LOG(INFO) << model.DebugString();
  return 0;
}
