// Copyright 2010-2021 Google LLC
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

// [START program]
// Example of a simple nurse scheduling problem.
// [START import]
#include <atomic>
#include <map>
#include <numeric>
#include <tuple>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/time_limit.h"
// [END import]

namespace operations_research {
namespace sat {

void NurseSat() {
  // [START data]
  const int num_nurses = 4;
  const int num_shifts = 3;
  const int num_days = 3;

  std::vector<int> all_nurses(num_nurses);
  std::iota(all_nurses.begin(), all_nurses.end(), 0);

  std::vector<int> all_shifts(num_shifts);
  std::iota(all_shifts.begin(), all_shifts.end(), 0);

  std::vector<int> all_days(num_days);
  std::iota(all_days.begin(), all_days.end(), 0);
  // [END data]

  // Creates the model.
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // Creates shift variables.
  // shifts[(n, d, s)]: nurse 'n' works shift 's' on day 'd'.
  // [START variables]
  std::map<std::tuple<int, int, int>, BoolVar> shifts;
  for (int n : all_nurses) {
    for (int d : all_days) {
      for (int s : all_shifts) {
        auto key = std::make_tuple(n, d, s);
        shifts[key] = cp_model.NewBoolVar().WithName(
            absl::StrFormat("shift_n%dd%ds%d", n, d, s));
      }
    }
  }
  // [END variables]

  // Each shift is assigned to exactly one nurse in the schedule period.
  // [START exactly_one_nurse]
  for (int d : all_days) {
    for (int s : all_shifts) {
      LinearExpr sum;
      for (int n : all_nurses) {
        auto key = std::make_tuple(n, d, s);
        sum += shifts[key];
      }
      cp_model.AddEquality(sum, 1);
    }
  }
  // [END exactly_one_nurse]

  // Each nurse works at most one shift per day.
  // [START at_most_one_shift]
  for (int n : all_nurses) {
    for (int d : all_days) {
      LinearExpr sum;
      for (int s : all_shifts) {
        auto key = std::make_tuple(n, d, s);
        sum += shifts[key];
      }
      cp_model.AddLessOrEqual(sum, 1);
    }
  }
  // [END at_most_one_shift]

  // [START assign_nurses_evenly]
  // Try to distribute the shifts evenly, so that each nurse works
  // min_shifts_per_nurse shifts. If this is not possible, because the total
  // number of shifts is not divisible by the number of nurses, some nurses will
  // be assigned one more shift.
  int min_shifts_per_nurse = (num_shifts * num_days) / num_nurses;
  int max_shifts_per_nurse;
  if ((num_shifts * num_days) % num_nurses == 0) {
    max_shifts_per_nurse = min_shifts_per_nurse;
  } else {
    max_shifts_per_nurse = min_shifts_per_nurse + 1;
  }
  for (int n : all_nurses) {
    std::vector<BoolVar> num_shifts_worked;
    // int num_shifts_worked = 0;
    for (int d : all_days) {
      for (int s : all_shifts) {
        auto key = std::make_tuple(n, d, s);
        num_shifts_worked.push_back(shifts[key]);
      }
    }
    cp_model.AddLessOrEqual(min_shifts_per_nurse,
                            LinearExpr::Sum(num_shifts_worked));
    cp_model.AddLessOrEqual(LinearExpr::Sum(num_shifts_worked),
                            max_shifts_per_nurse);
  }
  // [END assign_nurses_evenly]

  // [START parameters]
  Model model;
  SatParameters parameters;
  parameters.set_linearization_level(0);
  // Enumerate all solutions.
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  // [END parameters]

  // Display the first five solutions.
  // [START solution_printer]
  // Create an atomic Boolean that will be periodically checked by the limit.
  std::atomic<bool> stopped(false);
  model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stopped);

  const int kSolutionLimit = 5;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    for (int d : all_days) {
      LOG(INFO) << "Day " << std::to_string(d);
      for (int n : all_nurses) {
        bool is_working = false;
        for (int s : all_shifts) {
          auto key = std::make_tuple(n, d, s);
          if (SolutionIntegerValue(r, shifts[key])) {
            is_working = true;
            LOG(INFO) << "  Nurse " << std::to_string(n) << " works shift "
                      << std::to_string(s);
          }
        }
        if (!is_working) {
          LOG(INFO) << "  Nurse " << std::to_string(n) << " does not work";
        }
      }
    }
    num_solutions++;
    if (num_solutions >= kSolutionLimit) {
      stopped = true;
      LOG(INFO) << "Stop search after " << kSolutionLimit << " solutions.";
    }
  }));
  // [END solution_printer]

  // [START solve]
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  // [END solve]

  // Statistics.
  // [START statistics]
  LOG(INFO) << "Statistics";
  LOG(INFO) << CpSolverResponseStats(response);
  LOG(INFO) << "solutions found : " << std::to_string(num_solutions);
  // [END statistics]
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::NurseSat();
  return EXIT_SUCCESS;
}
// [END program]
