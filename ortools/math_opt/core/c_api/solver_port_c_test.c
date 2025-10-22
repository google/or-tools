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

// A minimal use of MathOptSolve() from C to ensure the code compiles in C.
// NOTE: this file is .c, not .cc, so the blaze will test this compiles as C.

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "ortools/math_opt/core/c_api/solver.h"

void TestSolveEmptyModel() {
  // Empty model
  const size_t model_size = 0;
  const void* model = NULL;
  // Solve with glop
  const int solver_type = 3;
  void* solve_result = NULL;
  size_t solve_result_size = 0;
  char* status_msg = NULL;
  const int error =
      MathOptSolve(model, model_size, solver_type, /*interrupter=*/NULL,
                   &solve_result, &solve_result_size, &status_msg);
  if (error) {
    printf("error on MathOptSolve, status code: %d, status message: %s", error,
           status_msg);
    // If you handle the error instead of crashing, be sure to free status_msg.
    abort();
  }
  MathOptFree(status_msg);
  MathOptFree(solve_result);
}

void TestInterruptSolveEmptyModel() {
  // Empty model
  const size_t model_size = 0;
  const void* model = NULL;
  // Solve with glop
  const int solver_type = 3;
  void* solve_result = NULL;
  size_t solve_result_size = 0;
  char* status_msg = NULL;
  struct MathOptInterrupter* interrupter = MathOptNewInterrupter();
  MathOptInterrupt(interrupter);
  const int error =
      MathOptSolve(model, model_size, solver_type, /*interrupter=*/NULL,
                   &solve_result, &solve_result_size, &status_msg);
  if (error) {
    printf("error on MathOptSolve, status code: %d, status message: %s", error,
           status_msg);
    // If you handle the error instead of crashing, be sure to free status_msg.
    abort();
  }
  if (MathOptIsInterrupted(interrupter) == 0) {
    printf("interrupter should be interrupted");
    abort();
  }
  MathOptFreeInterrupter(interrupter);
  MathOptFree(status_msg);
  MathOptFree(solve_result);
}

int main(int argc, char** argv) {
  TestSolveEmptyModel();
  TestInterruptSolveEmptyModel();
  return EXIT_SUCCESS;
}
