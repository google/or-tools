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

// [START solve_callback_stream_solutions_cc_include]
#include <iostream>
#include <ostream>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"

// [END solve_callback_stream_solutions_cc_include]

namespace {

absl::Status Main() {
  // [START solve_callback_stream_solutions]
  namespace math_opt = operations_research::math_opt;
  math_opt::Model model;
  const math_opt::Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);
  absl::Mutex cb_mutex;
  auto callback = [x, &cb_mutex](const math_opt::CallbackData& cb_data) {
    CHECK_EQ(cb_data.event, math_opt::CallbackEvent::kMipSolution);
    // Warning: either the callback should be threadsafe, or you must rely on
    // solver specific guarantees that the callback isn't invoked concurrently.
    const absl::MutexLock lock(cb_mutex);
    std::cout << "Found solution x = " << cb_data.solution->at(x) << std::endl;
    return math_opt::CallbackResult();
  };
  const math_opt::CallbackRegistration cb_reg = {
      .events = {math_opt::CallbackEvent::kMipSolution}};
  // Gurobi, CpSat, and SCIP all support this callback.
  const math_opt::SolverType solver = math_opt::SolverType::kGscip;
  OR_ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                      math_opt::Solve(model, solver,
                                      {.callback_registration = cb_reg,
                                       .callback = std::move(callback)}));
  OR_RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  std::cout << "Optimal x = " << result.variable_values().at(x) << std::endl;
  return absl::OkStatus();
  // [END solve_callback_stream_solutions]
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
