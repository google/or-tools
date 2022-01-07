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
// Nurse scheduling problem with shift requests.
// [START import]
#include <atomic>
#include <numeric>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
// [END import]

namespace operations_research {
namespace sat {

void ScheduleRequestsSat() {
  // [START data]
  const int num_nurses = 5;
  const int num_days = 7;
  const int num_shifts = 3;

  std::vector<int> all_nurses(num_nurses);
  std::iota(all_nurses.begin(), all_nurses.end(), 0);

  std::vector<int> all_days(num_days);
  std::iota(all_days.begin(), all_days.end(), 0);

  std::vector<int> all_shifts(num_shifts);
  std::iota(all_shifts.begin(), all_shifts.end(), 0);

  std::vector<std::vector<std::vector<int64_t>>> shift_requests = {
      {
          {0, 0, 1},
          {0, 0, 0},
          {0, 0, 0},
          {0, 0, 0},
          {0, 0, 1},
          {0, 1, 0},
          {0, 0, 1},
      },
      {
          {0, 0, 0},
          {0, 0, 0},
          {0, 1, 0},
          {0, 1, 0},
          {1, 0, 0},
          {0, 0, 0},
          {0, 0, 1},
      },
      {
          {0, 1, 0},
          {0, 1, 0},
          {0, 0, 0},
          {1, 0, 0},
          {0, 0, 0},
          {0, 1, 0},
          {0, 0, 0},
      },
      {
          {0, 0, 1},
          {0, 0, 0},
          {1, 0, 0},
          {0, 1, 0},
          {0, 0, 0},
          {1, 0, 0},
          {0, 0, 0},
      },
      {
          {0, 0, 0},
          {0, 0, 1},
          {0, 1, 0},
          {0, 0, 0},
          {1, 0, 0},
          {0, 1, 0},
          {0, 0, 0},
      },
  };
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
    LinearExpr num_worked_shifts;
    for (int d : all_days) {
      for (int s : all_shifts) {
        auto key = std::make_tuple(n, d, s);
        num_worked_shifts += shifts[key];
      }
    }
    cp_model.AddLessOrEqual(min_shifts_per_nurse, num_worked_shifts);
    cp_model.AddLessOrEqual(num_worked_shifts, max_shifts_per_nurse);
  }
  // [END assign_nurses_evenly]

  // [START objective]
  LinearExpr objective_expr;
  for (int n : all_nurses) {
    for (int d : all_days) {
      for (int s : all_shifts) {
        if (shift_requests[n][d][s] == 1) {
          auto key = std::make_tuple(n, d, s);
          objective_expr += shifts[key] * shift_requests[n][d][s];
        }
      }
    }
  }
  cp_model.Maximize(objective_expr);
  // [END objective]

  // [START solve]
  const CpSolverResponse response = Solve(cp_model.Build());
  // [END solve]

  // [START print_solution]
  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "Solution:";
    for (int d : all_days) {
      LOG(INFO) << "Day " << std::to_string(d);
      for (int n : all_nurses) {
        for (int s : all_shifts) {
          auto key = std::make_tuple(n, d, s);
          if (SolutionIntegerValue(response, shifts[key]) == 1) {
            if (shift_requests[n][d][s] == 1) {
              LOG(INFO) << "  Nurse " << std::to_string(n) << " works shift "
                        << std::to_string(s) << " (requested).";
            } else {
              LOG(INFO) << "  Nurse " << std::to_string(n) << " works shift "
                        << std::to_string(s) << " (not requested).";
            }
          }
        }
      }
      LOG(INFO) << "";
    }
    LOG(INFO) << "Number of shift requests met = " << response.objective_value()
              << " (out of " << num_nurses * min_shifts_per_nurse << ")";
  } else {
    LOG(INFO) << "No optimal solution found !";
  }
  // [END print_solution]

  // Statistics.
  // [START statistics]
  LOG(INFO) << "Statistics";
  LOG(INFO) << CpSolverResponseStats(response);
  // [END statistics]
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ScheduleRequestsSat();
  return EXIT_SUCCESS;
}
// [END program]
