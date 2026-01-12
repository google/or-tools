// Copyright 2010-2025 Google LLC
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

// This file implements the main function for the Two-Dimensional
// Constrained Guillotine Cutting solver. It reads the problem
// specification from an input file specified via command-line flags,
// and prints the solution to standard output.
//
// Example usage:
// ./cgc_main --input_file=testdata/cgc/my_input_file.in
// Other examples of input files in testdata/cgc/.

#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "examples/cpp/cgc.h"
#include "examples/cpp/cgc_data.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"

ABSL_FLAG(std::string, input_file, "", "Input data file");
ABSL_FLAG(int, time_limit_in_ms, 0,
          "Time limit in milliseconds. 0 means no time limit. "
          "If different, the solver will provide the best solution "
          "that was found in that amount of time.");
ABSL_FLAG(bool, print_maximum_value, false,
          "If true, it prints the maximum value found.");
ABSL_FLAG(bool, print_solution, false,
          "If true, it prints the maximum value and the cutting pattern.");

using operations_research::ConstrainedGuillotineCutting;
using operations_research::ConstrainedGuillotineCuttingData;

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  if (absl::GetFlag(FLAGS_input_file).empty()) {
    LOG(QFATAL) << "Please supply an input file with --input_file=";
  }
  LOG(INFO) << "Processing file " << absl::GetFlag(FLAGS_input_file);

  auto data = std::make_unique<ConstrainedGuillotineCuttingData>();

  if (!data->LoadFromFile(absl::GetFlag(FLAGS_input_file))) {
    LOG(QFATAL) << "Input file " << absl::GetFlag(FLAGS_input_file)
                << " was not loaded.";
  }

  ConstrainedGuillotineCutting cgc(std::move(data));
  const absl::Duration time_limit =
      absl::GetFlag(FLAGS_time_limit_in_ms) == 0
          ? absl::InfiniteDuration()
          : absl::Milliseconds(absl::GetFlag(FLAGS_time_limit_in_ms));
  cgc.Solve(time_limit);

  if (cgc.Solved()) {
    if (absl::GetFlag(FLAGS_print_solution)) {
      cgc.PrintSolution();
    } else if (absl::GetFlag(FLAGS_print_maximum_value)) {
      absl::PrintF("%d", cgc.MaximumValue());
    } else {
      LOG(INFO) << "The maximum value found is: " << cgc.MaximumValue();
    }
  } else {
    absl::PrintF("There was no solution found in %v ms.\n", time_limit);
  }
}
