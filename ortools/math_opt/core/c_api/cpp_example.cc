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

// Demonstrates how to call the MathOpt C API defined in solver.h from C++.
//
// At a high level, the example:
//  * builds a ModelProto in C++,
//  * serializes the model to binary,
//  * calls MathOptSolve() from the C-API on the model binary, which outputs a
//    SolveResultProto in binary,
//  * parses a C++ SolveResultProto from the binary,
//  * prints some key parts of the SolveResultProto.
//
// Actual C++ users should use MathOpt's various C++ APIs. This is just a
// demonstration of how the C API is intended to be used (from any language that
// an interoperate with C).

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>

#include "absl/status/status.h"
#include "ortools/base/init_google.h"
#include "ortools/math_opt/core/c_api/solver.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

// This example solves the optimization problem:
//   max x
//   x in [0, 1]
// and then prints out the termination reason and objective value.
int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  // Create a serialized ModelProto for the problem.
  operations_research::math_opt::ModelProto model;
  model.mutable_variables()->add_ids(0);
  model.mutable_variables()->add_lower_bounds(0.0);
  model.mutable_variables()->add_upper_bounds(1.0);
  model.mutable_variables()->add_names("x");
  model.mutable_variables()->add_integers(false);
  model.mutable_objective()->set_maximize(true);
  model.mutable_objective()->mutable_linear_coefficients()->add_ids(0);
  model.mutable_objective()->mutable_linear_coefficients()->add_values(1.0);
  const std::string model_str = model.SerializeAsString();
  const void* model_bin = model_str.data();
  const size_t model_bin_size = model_str.size();

  // Pick a solver.
  const int solver_type =
      static_cast<int>(operations_research::math_opt::SOLVER_TYPE_GLOP);

  // Set up the output arguments for MathOptSolve()
  void* result_bin = nullptr;
  size_t result_bin_size = 0;
  char* status_msg = nullptr;

  // Call the C API to do solve the model and populate the output arguments.
  const int status_code = MathOptSolve(model_bin, model_bin_size, solver_type,
                                       /*interrupter=*/nullptr, &result_bin,
                                       &result_bin_size, &status_msg);

  // If MathOptSolve() failed, print the error and abort.
  if (status_code != 0) {
    std::cerr << absl::Status(static_cast<absl::StatusCode>(status_code),
                              status_msg)
              << std::endl;
    // If you handle the error instead of crashing, be sure to free status_msg.
    std::abort();
  }

  // Recover the SolveResultProto from the output arguments (stored as a
  // serialized proto).
  operations_research::math_opt::SolveResultProto result;
  if (!result.ParseFromArray(result_bin, static_cast<int>(result_bin_size))) {
    std::cout << "failed to parse SolveResultProto" << std::endl;
    std::abort();
  }

  // Print out the desired output.
  std::cout << "Termination is optimal: "
            << (result.termination().reason() ==
                operations_research::math_opt::TERMINATION_REASON_OPTIMAL)
            << std::endl;
  std::cout << "Objective value: "
            << result.termination().objective_bounds().primal_bound()
            << std::endl;

  // Clean up any memory allocated by MathOptSolve(). Note that invoking these
  // functions on nullptr is safe.
  MathOptFree(result_bin);
  MathOptFree(status_msg);
  return 0;
}
