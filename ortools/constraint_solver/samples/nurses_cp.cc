// Copyright 2018 Google LLC
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

#include <numeric>  // std::iota

#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
void SolveNursesExample() {
  // Instantiate the solver.
  Solver solver("NursesExample");
  std::array<int, 4> nurses;
  std::iota(std::begin(nurses), std::end(nurses), 0);
  {
    std::ostringstream oss;
    for (auto i : nurses) oss << ' ' << i;
    LOG(INFO) << "Nurses:" << oss.str();
  }

  // Nurse assigned to shift 0 means not working that day.
  std::array<int, 4> shifts;
  std::iota(std::begin(shifts), std::end(shifts), 0);
  {
    std::ostringstream oss;
    for (auto i : shifts) oss << ' ' << i;
    LOG(INFO) << "Shifts:" << oss.str();
  }

  std::array<int, 7> days;
  std::iota(std::begin(days), std::end(days), 0);
  {
    std::ostringstream oss;
    for (auto i : days) oss << ' ' << i;
    LOG(INFO) << "Days:" << oss.str();
  }

  // Create shift variables.
  std::vector<std::vector<IntVar*>> shifts_matrix(nurses.size());
  std::vector<IntVar*> shifts_flat;
  for (const auto nurse : nurses) {
    for (const auto day : days) {
      std::ostringstream oss;
      oss << "shifts(nurse: " << nurse << ", day: " << day << ")";
      IntVar* var = solver.MakeIntVar(shifts.front(), shifts.back(), oss.str());
      shifts_matrix[nurse].push_back(var);
      shifts_flat.push_back(var);
    }
  }

  // Create nurse variables.
  std::vector<std::vector<IntVar*>> nurses_matrix(shifts.size());
  for (const auto shift : shifts) {
    for (const auto day : days) {
      std::ostringstream oss;
      oss << "nurses(shift: " << shift << ", day: " << day << ")";
      IntVar* var = solver.MakeIntVar(nurses.front(), nurses.back(), oss.str());
      nurses_matrix[shift].push_back(var);
    }
  }

  // Set relationships between shifts and nurses.
  for (const auto day : days) {
    std::vector<IntVar*> nurses_for_day(shifts.size());
    for (const auto shift : shifts) {
      nurses_for_day[shift] = nurses_matrix[shift][day];
    }
    for (const auto nurse : nurses) {
      IntVar* s = shifts_matrix[nurse][day];
      solver.AddConstraint(
          solver.MakeEquality(solver.MakeElement(nurses_for_day, s), nurse));
    }
  }

  // Make assignments different on each day i.e.
  for (const auto day : days) {
    // no shift can have two nurses
    std::vector<IntVar*> shifts_for_day(nurses.size());
    for (const auto nurse : nurses) {
      shifts_for_day[nurse] = shifts_matrix[nurse][day];
    }
    solver.AddConstraint(solver.MakeAllDifferent(shifts_for_day));

    // no nurses can have more than one shifts a day
    std::vector<IntVar*> nurses_for_day(shifts.size());
    for (const auto shift : shifts) {
      nurses_for_day[shift] = nurses_matrix[shift][day];
    }
    solver.AddConstraint(solver.MakeAllDifferent(nurses_for_day));
  }

  // Each nurse works 5 or 6 days in a week.
  for (const auto nurse : nurses) {
    std::vector<IntVar*> nurse_is_working;
    for (const auto day : days) {
      nurse_is_working.push_back(
          solver.MakeIsGreaterOrEqualCstVar(shifts_matrix[nurse][day], 1));
    }
    solver.AddConstraint(solver.MakeSumGreaterOrEqual(nurse_is_working, 5));
    solver.AddConstraint(solver.MakeSumLessOrEqual(nurse_is_working, 6));
  }

  // Create works_shift variables.
  // works_shift_matrix[n][s] is True if
  // nurse n works shift s at least once during the week.
  std::vector<std::vector<IntVar*>> works_shift_matrix(nurses.size());
  for (const auto nurse : nurses) {
    for (const auto shift : shifts) {
      std::ostringstream oss;
      oss << "work_shift(nurse: " << nurse << ", shift: " << shift << ")";
      works_shift_matrix[nurse].push_back(solver.MakeBoolVar(oss.str()));
    }
  }

  for (const auto nurse : nurses) {
    for (const auto shift : shifts) {
      std::vector<IntVar*> shift_s_for_nurse;
      for (const auto day : days) {
        shift_s_for_nurse.push_back(
            solver.MakeIsEqualCstVar(shifts_matrix[nurse][day], shift));
      }
      solver.AddConstraint(
          solver.MakeEquality(works_shift_matrix[nurse][shift],
                              solver.MakeMax(shift_s_for_nurse)->Var()));
    }
  }

  // For each shift(other than 0), at most 2 nurses are assigned to that shift
  // during the week.
  for (std::size_t shift = 1; shift < shifts.size(); ++shift) {
    std::vector<IntVar*> nurses_for_shift;
    for (const auto nurse : nurses) {
      nurses_for_shift.push_back(works_shift_matrix[nurse][shift]);
    }
    solver.AddConstraint(solver.MakeSumLessOrEqual(nurses_for_shift, 2));
  }

  // If a nurse works shifts 2 or 3 on,
  // he must also work that shift the previous day or the following day.
  for (const auto shift : {2, 3}) {
    for (const auto day : days) {
      IntVar* v0 = solver.MakeIsEqualVar(nurses_matrix[shift][day],
                                         nurses_matrix[shift][(day + 1) % 7]);
      IntVar* v1 = solver.MakeIsEqualVar(nurses_matrix[shift][(day + 1) % 7],
                                         nurses_matrix[shift][(day + 2) % 7]);
      solver.AddConstraint(solver.MakeEquality(solver.MakeMax(v0, v1), 1));
    }
  }

  // ----- Search monitors and decision builder -----

  // Create the decision builder.
  DecisionBuilder* const main_phase = solver.MakePhase(
      shifts_flat, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);

  // Search log.
  SearchMonitor* const search_log = nullptr;

  SearchLimit* limit = nullptr;

  // Create the solution collector.
  SolutionCollector* const collector = solver.MakeAllSolutionCollector();
  collector->Add(shifts_flat);

  // Solve
  solver.Solve(main_phase, search_log, nullptr, limit, collector);
  LOG(INFO) << "Number of solutions: " << collector->solution_count();
  LOG(INFO) << "";

  // Display a few solutions picked at random.
  std::array<int, 4> a_few_solutions = {859, 2034, 5091, 7003};
  for (const auto solution : a_few_solutions) {
    LOG(INFO) << "Solution " << solution << ":";
    for (const auto day : days) {
      LOG(INFO) << "Day " << day << ":";
      for (const auto nurse : nurses) {
        LOG(INFO) << "Nurse " << nurse << " assigned to "
                  << "Task "
                  << collector->Value(solution,
                                      shifts_flat[nurse * days.size() + day]);
      }
    }
  }
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Time: " << solver.wall_time() << "ms";
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  operations_research::SolveNursesExample();
  return EXIT_SUCCESS;
}
