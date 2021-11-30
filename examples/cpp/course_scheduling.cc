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

#include "examples/cpp/course_scheduling.h"

#include <cmath>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "examples/cpp/course_scheduling.pb.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {

absl::Status CourseSchedulingSolver::ValidateModelAndLoadClasses(
    const CourseSchedulingModel& model) {
  time_slot_count_ = model.days_count() * model.daily_time_slot_count();
  room_count_ = model.rooms_size();
  solve_for_rooms_ = room_count_ > 0;
  // If there are no rooms given, we have to give room_count at least one in
  // order for the loops creating the solver variables and constraints to work.
  if (!solve_for_rooms_) {
    room_count_ = 1;
  }

  // Validate the information given for each course.
  for (const Course& course : model.courses()) {
    if (course.consecutive_slots_count() != 1 &&
        course.consecutive_slots_count() != 2) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "The course titled %s has %d consecutive time slots specified when "
          "it can only have 1 or 2.",
          course.display_name(), course.consecutive_slots_count()));
    }

    if (course.teacher_section_counts_size() != course.teacher_indices_size()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("The course titled %s should have the same number of "
                          "teacher indices and section numbers.",
                          course.display_name()));
    }

    for (const int teacher_index : course.teacher_indices()) {
      if (teacher_index >= model.teachers_size()) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "The course titled %s has teacher %d assigned to it but there are "
            "only %d teachers.",
            course.display_name(), teacher_index, model.teachers_size()));
      }
    }

    for (const int room_index : course.room_indices()) {
      if (room_index >= model.rooms_size()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("The course titled %s is slotted for room index %d "
                            "but there are only %d rooms.",
                            course.display_name(), room_index, room_count_));
      }
    }
  }

  // Validate the information given for each teacher and create hash sets of the
  // restricted indices for each teacher.
  teacher_to_restricted_slots_ =
      std::vector<absl::flat_hash_set<int>>(model.teachers_size());
  for (int teacher_index = 0; teacher_index < model.teachers_size();
       ++teacher_index) {
    for (const int restricted_slot :
         model.teachers(teacher_index).restricted_time_slots()) {
      if (restricted_slot >= time_slot_count_) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Teacher with name %s has restricted time slot %d "
                            "but there are only %d time slots.",
                            model.teachers(teacher_index).display_name(),
                            restricted_slot, time_slot_count_));
      }
      teacher_to_restricted_slots_[teacher_index].insert(restricted_slot);
    }
  }

  // Since each course can have multiple sections (classes), we need to
  // "flatten" out each course so that each of its sections gets a unique index.
  // This vector stores the information for calculating each unique index. The
  // first value is the unique index the course's sections begin at. The second
  // value is the total number of sections of that course.
  course_to_classes_ = std::vector<std::vector<int>>(model.courses_size());
  // For each teacher, store the class unique indices that they teach.
  teacher_to_classes_ =
      std::vector<absl::flat_hash_set<int>>(model.teachers_size());
  // Store course indices of courses that have a single section.
  absl::flat_hash_set<int> singleton_courses;
  int flattened_course_index = 0;
  for (int course_index = 0; course_index < model.courses_size();
       ++course_index) {
    const Course& course = model.courses(course_index);
    int total_section_count = 0;
    for (int teacher = 0; teacher < course.teacher_indices_size(); ++teacher) {
      for (int section = 0; section < course.teacher_section_counts(teacher);
           ++section) {
        teacher_to_classes_[course.teacher_indices(teacher)].insert(
            flattened_course_index);
        course_to_classes_[course_index].push_back(flattened_course_index);
        ++flattened_course_index;
      }
      total_section_count += course.teacher_section_counts(teacher);
    }
    if (total_section_count == 1) {
      singleton_courses.insert(course_index);
    }
  }
  class_count_ = flattened_course_index;

  // Validate the information given for each student. Store each student's
  // course pairs.
  for (const Student& student : model.students()) {
    for (const int course_index : student.course_indices()) {
      if (course_index >= model.courses_size()) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Student with name %s has course index %d but there are only %d "
            "courses.",
            student.display_name(), course_index, model.courses_size()));
      }
    }
    InsertSortedPairs(std::vector<int>(student.course_indices().begin(),
                                       student.course_indices().end()),
                      &course_conflicts_);
  }

  LOG(INFO) << "Number of days: " << model.days_count();
  LOG(INFO) << "Number of daily time slots: " << model.daily_time_slot_count();
  LOG(INFO) << "Total number of time slots: " << time_slot_count_;
  LOG(INFO) << "Number of courses: " << model.courses_size();
  LOG(INFO) << "Total number of classes: " << class_count_;
  LOG(INFO) << "Number of teachers: " << model.teachers_size();
  LOG(INFO) << "Number of students: " << model.students_size();
  if (solve_for_rooms_) {
    LOG(INFO) << "Number of rooms: " << model.rooms_size();
  }

  return absl::OkStatus();
}

CourseSchedulingResult CourseSchedulingSolver::Solve(
    const CourseSchedulingModel& model) {
  CourseSchedulingResult result;

  const auto validation_status = ValidateModelAndLoadClasses(model);
  if (!validation_status.ok()) {
    result.set_solver_status(
        CourseSchedulingResultStatus::SOLVER_MODEL_INVALID);
    result.set_message(std::string(validation_status.message()));
    return result;
  }

  ConflictPairs class_conflicts;
  result = SolveModel(model, class_conflicts);

  if (result.solver_status() != SOLVER_FEASIBLE &&
      result.solver_status() != SOLVER_OPTIMAL) {
    return result;
  }

  const auto verifier_status = VerifyCourseSchedulingResult(model, result);
  if (!verifier_status.ok()) {
    result.set_solver_status(CourseSchedulingResultStatus::ABNORMAL);
    result.set_message(std::string(verifier_status.message()));
  }

  return result;
}

CourseSchedulingResult CourseSchedulingSolver::SolveModel(
    const CourseSchedulingModel& model, const ConflictPairs& class_conflicts) {
  CourseSchedulingResult result;

  result = ScheduleCourses(class_conflicts, model);
  if (result.solver_status() != SOLVER_FEASIBLE &&
      result.solver_status() != SOLVER_OPTIMAL) {
    if (result.solver_status() == SOLVER_INFEASIBLE) {
      result.set_message("The problem is infeasible with the given courses.");
    }
    return result;
  }

  ConflictPairs class_conflicts_to_try = AssignStudents(model, &result);

  if (class_conflicts_to_try.empty()) return result;

  std::vector<std::pair<int, int>> conflicts(class_conflicts_to_try.begin(),
                                             class_conflicts_to_try.end());

  const int conflicts_count = conflicts.size();
  const int conflicts_log = conflicts_count == 1 ? 1 : log2(conflicts_count);
  for (int i = 0; i < conflicts_log; ++i) {
    const int divisions = MathUtil::IPow<double>(2, i);
    for (int j = 0; j < divisions; ++j) {
      const int start = std::floor(conflicts_count * j / divisions);
      const int end = std::floor(conflicts_count * (j + 1) / divisions);

      ConflictPairs new_class_conflicts = class_conflicts;
      if (end >= conflicts_count) {
        new_class_conflicts.insert(conflicts.begin() + start, conflicts.end());
      } else {
        new_class_conflicts.insert(conflicts.begin() + start,
                                   conflicts.begin() + end);
      }

      result = SolveModel(model, new_class_conflicts);
      if (result.solver_status() == SOLVER_FEASIBLE ||
          result.solver_status() == SOLVER_OPTIMAL) {
        return result;
      }
    }
  }

  return result;
}

std::vector<int> CourseSchedulingSolver::GetRoomIndices(const Course& course) {
  if (solve_for_rooms_) {
    return std::vector<int>(course.room_indices().begin(),
                            course.room_indices().end());
  }

  return {0};
}

void CourseSchedulingSolver::InsertSortedPairs(const std::vector<int>& list,
                                               ConflictPairs* pairs) {
  for (int first = 1; first < list.size(); ++first) {
    for (int second = first; second < list.size(); ++second) {
      pairs->insert(std::minmax(list[first - 1], list[second]));
    }
  }
}

std::vector<absl::flat_hash_set<int>>
CourseSchedulingSolver::GetClassesByTimeSlot(
    const CourseSchedulingResult* result) {
  std::vector<absl::flat_hash_set<int>> time_slot_to_classes(time_slot_count_);

  for (const ClassAssignment& class_assignment : result->class_assignments()) {
    const int course_index = class_assignment.course_index();
    const int section_number = class_assignment.section_number();
    for (const int time_slot : class_assignment.time_slots()) {
      time_slot_to_classes[time_slot].insert(
          course_to_classes_[course_index][section_number]);
    }
  }

  return time_slot_to_classes;
}

void CourseSchedulingSolver::AddVariableIfNonNull(double coeff,
                                                  const MPVariable* var,
                                                  MPConstraint* ct) {
  if (var == nullptr) return;

  ct->SetCoefficient(var, coeff);
}

CourseSchedulingResult CourseSchedulingSolver::ScheduleCourses(
    const ConflictPairs& class_conflicts, const CourseSchedulingModel& model) {
  LOG(INFO) << "Starting schedule courses solver with "
            << class_conflicts.size() << " class conflicts.";
  MPSolver mip_solver("CourseSchedulingMIP",
                      MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  const double kInfinity = std::numeric_limits<double>::infinity();

  // Create binary variables x(n,t,r) where x(n,t,r) = 1 indicates that course n
  // is scheduled for time slot t in course r. Variables are only created if
  // attendees of class n are available for time slot t and if the course can be
  // placed into room r.
  std::vector<std::vector<std::vector<MPVariable*>>> variables(
      class_count_,
      std::vector<std::vector<MPVariable*>>(
          time_slot_count_, std::vector<MPVariable*>(room_count_, nullptr)));
  for (int proto_index = 0; proto_index < model.courses_size(); ++proto_index) {
    const Course& course = model.courses(proto_index);
    const int course_index = course_to_classes_[proto_index][0];
    int total_section_index = 0;
    for (int i = 0; i < course.teacher_section_counts_size(); ++i) {
      const int teacher = course.teacher_indices(i);
      const int section_count = course.teacher_section_counts(i);
      for (int section = 0; section < section_count; ++section) {
        for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
          if (teacher_to_restricted_slots_[teacher].contains(time_slot)) {
            continue;
          }

          for (const int room : GetRoomIndices(course)) {
            variables[course_index + total_section_index][time_slot][room] =
                mip_solver.MakeBoolVar(absl::StrFormat(
                    "x_%d_%d_%d", course_index + total_section_index, time_slot,
                    room));
          }
        }
        ++total_section_index;
      }
    }
  }

  std::vector<std::vector<MPVariable*>> intermediate_vars(
      class_count_, std::vector<MPVariable*>(time_slot_count_));
  for (int class_index = 0; class_index < class_count_; ++class_index) {
    for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
      MPConstraint* const ct = mip_solver.MakeRowConstraint(0, 0);
      for (int room = 0; room < room_count_; ++room) {
        if (variables[class_index][time_slot][room] == nullptr) continue;

        ct->SetCoefficient(variables[class_index][time_slot][room], 1);
      }
      if (!ct->terms().empty()) {
        intermediate_vars[class_index][time_slot] = mip_solver.MakeBoolVar(
            absl::StrFormat("intermediate_%d_%d", class_index, time_slot));
        ct->SetCoefficient(intermediate_vars[class_index][time_slot], -1);
      }
    }
  }

  // 1. Each course meets no more than its number of consecutive time slots a
  // day.
  for (int day = 0; day < model.days_count(); ++day) {
    for (int course = 0; course < model.courses_size(); ++course) {
      const int consecutive_slot_count =
          model.courses(course).consecutive_slots_count();
      for (const int class_index : course_to_classes_[course]) {
        MPConstraint* const ct =
            mip_solver.MakeRowConstraint(0, consecutive_slot_count);
        for (int daily_time_slot = 0;
             daily_time_slot < model.daily_time_slot_count();
             ++daily_time_slot) {
          AddVariableIfNonNull(
              1,
              intermediate_vars[class_index]
                               [(day * model.daily_time_slot_count()) +
                                daily_time_slot],
              ct);
        }
      }
    }
  }

  // 2. Each course must meet the given number of times * its number of
  // consecutive time slots.
  for (int course = 0; course < model.courses_size(); ++course) {
    const int meeting_count = model.courses(course).meetings_count();
    const int consecutive_slot_count =
        model.courses(course).consecutive_slots_count();
    for (const int class_index : course_to_classes_[course]) {
      MPConstraint* const ct =
          mip_solver.MakeRowConstraint(meeting_count * consecutive_slot_count,
                                       meeting_count * consecutive_slot_count);
      for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
        AddVariableIfNonNull(1, intermediate_vars[class_index][time_slot], ct);
      }
    }
  }

  // 3. Teachers are scheduled for no more than one course per time slot.
  for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
    for (int teacher = 0; teacher < model.teachers_size(); ++teacher) {
      MPConstraint* const ct = mip_solver.MakeRowConstraint(0, 1);
      for (const int class_index : teacher_to_classes_[teacher]) {
        AddVariableIfNonNull(1, intermediate_vars[class_index][time_slot], ct);
      }
    }
  }

  // 4. Each room can only be occupied by one course for each time slot.
  if (solve_for_rooms_) {
    for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
      for (int room = 0; room < room_count_; ++room) {
        MPConstraint* const ct = mip_solver.MakeRowConstraint(0, 1);
        for (int class_index = 0; class_index < class_count_; ++class_index) {
          AddVariableIfNonNull(1, variables[class_index][time_slot][room], ct);
        }
      }
    }
  }

  // 5. Ensure each class is scheduled for the correct number of consecutive
  // time slots.
  for (int course = 0; course < model.courses_size(); ++course) {
    const int consecutive_slot_count =
        model.courses(course).consecutive_slots_count();
    if (consecutive_slot_count == 1) continue;
    for (const int class_index : course_to_classes_[course]) {
      for (int day = 0; day < model.days_count(); ++day) {
        for (int room = 0; room < room_count_; ++room) {
          // If only the previous time slot is chosen, force the current time
          // slot to be chosen.
          for (int daily_time_slot = 0;
               daily_time_slot < model.daily_time_slot_count();
               ++daily_time_slot) {
            MPConstraint* const ct = mip_solver.MakeRowConstraint(0, kInfinity);
            const int time_slot_offset =
                day * model.daily_time_slot_count() + daily_time_slot;

            if (daily_time_slot > 0) {
              AddVariableIfNonNull(
                  1, variables[class_index][time_slot_offset - 1][room], ct);
            }
            AddVariableIfNonNull(
                -0.5, variables[class_index][time_slot_offset][room], ct);
            if (daily_time_slot < model.daily_time_slot_count() - 1) {
              AddVariableIfNonNull(
                  0.5, variables[class_index][time_slot_offset + 1][room], ct);
            }
          }
        }
      }
    }
  }

  // 6. Ensure that there are at least two sections of each course_conflicts_
  // pair that are scheduled for different time slots.
  for (const auto& conflict_pair : course_conflicts_) {
    const int course_1 = conflict_pair.first;
    const int course_2 = conflict_pair.second;
    for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
      const int bound = course_to_classes_[course_1].size() +
                        course_to_classes_[course_2].size() - 1;
      MPConstraint* const ct = mip_solver.MakeRowConstraint(0, bound);
      for (const int class_1 : course_to_classes_[course_1]) {
        AddVariableIfNonNull(1, intermediate_vars[class_1][time_slot], ct);
      }
      for (const int class_2 : course_to_classes_[course_2]) {
        AddVariableIfNonNull(1, intermediate_vars[class_2][time_slot], ct);
      }
    }
  }

  // 7. Ensure that conflicting class pairs are not scheduled for the same time
  // slot.
  for (const auto& conflict_pair : class_conflicts) {
    for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
      MPConstraint* const ct = mip_solver.MakeRowConstraint(0, 1);
      AddVariableIfNonNull(1, intermediate_vars[conflict_pair.first][time_slot],
                           ct);
      AddVariableIfNonNull(
          1, intermediate_vars[conflict_pair.second][time_slot], ct);
    }
  }

  MPSolver::ResultStatus status = mip_solver.Solve();

  CourseSchedulingResult result;
  result.set_solver_status(MipStatusToCourseSchedulingResultStatus(status));
  if (status != MPSolver::OPTIMAL && status != MPSolver::FEASIBLE) {
    if (status == MPSolver::UNBOUNDED) {
      result.set_message(
          "MIP solver returned UNBOUNDED: the model is solved but the solution "
          "is infinity");
    } else if (status == MPSolver::ABNORMAL) {
      result.set_message(
          "MIP solver returned ABNORMAL: some error occurred while solving");
    }
    return result;
  }

  for (int course = 0; course < model.courses_size(); ++course) {
    for (int section = 0; section < course_to_classes_[course].size();
         ++section) {
      ClassAssignment class_assignment;
      class_assignment.set_course_index(course);
      class_assignment.set_section_number(section);
      const int class_index = course_to_classes_[course][section];

      for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
        for (int room = 0; room < room_count_; ++room) {
          MPVariable* x_i = variables[class_index][time_slot][room];
          if (x_i != nullptr && x_i->solution_value() == 1) {
            if (solve_for_rooms_) {
              class_assignment.add_room_indices(room);
            }
            class_assignment.add_time_slots(time_slot);
          }
        }
      }
      *result.add_class_assignments() = class_assignment;
    }
  }
  return result;
}

CourseSchedulingSolver::ConflictPairs CourseSchedulingSolver::AssignStudents(
    const CourseSchedulingModel& model, CourseSchedulingResult* result) {
  LOG(INFO) << "Starting assign students solver.";
  MPSolver mip_solver("AssignStudentsMIP",
                      MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);

  // Create binary variables y(s,n) where y(s,n) = 1 indicates that student s is
  // enrolled in class n. Variables are created for a student and each section
  // of a course that they are signed up to take.
  std::vector<std::vector<MPVariable*>> variables(
      model.students_size(), std::vector<MPVariable*>(class_count_, nullptr));
  for (int student_index = 0; student_index < model.students_size();
       ++student_index) {
    const Student& student = model.students(student_index);
    for (const int course_index : student.course_indices()) {
      for (const int class_index : course_to_classes_[course_index]) {
        variables[student_index][class_index] = mip_solver.MakeBoolVar(
            absl::StrFormat("y_%d_%d", student_index, class_index));
      }
    }
  }

  // 1. Each student must be assigned to exactly one section for each course
  // they are signed up to take.
  for (int student_index = 0; student_index < model.students_size();
       ++student_index) {
    const Student& student = model.students(student_index);
    for (const int course_index : student.course_indices()) {
      MPConstraint* const ct = mip_solver.MakeRowConstraint(1, 1);
      for (const int class_index : course_to_classes_[course_index]) {
        AddVariableIfNonNull(1, variables[student_index][class_index], ct);
      }
    }
  }

  // 2. Ensure that the minimum and maximum capacities for each class are met.
  for (int course_index = 0; course_index < model.courses_size();
       ++course_index) {
    const Course& course = model.courses(course_index);
    const int min_cap = course.min_capacity();
    const int max_cap = course.max_capacity();
    for (const int class_index : course_to_classes_[course_index]) {
      MPConstraint* const ct = mip_solver.MakeRowConstraint(min_cap, max_cap);
      for (int student = 0; student < model.students_size(); ++student) {
        AddVariableIfNonNull(1, variables[student][class_index], ct);
      }
    }
  }

  // 3. Each student should be assigned to one class per time slot. This is a
  // soft constraint -- if violated, then the variable infeasibility_var(s,t)
  // will be greater than 0 for that student s and time slot t.
  std::vector<std::vector<MPVariable*>> infeasibility_vars(
      model.students_size(),
      std::vector<MPVariable*>(time_slot_count_, nullptr));
  std::vector<absl::flat_hash_set<int>> time_slot_to_classes =
      GetClassesByTimeSlot(result);
  for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
    for (int student_index = 0; student_index < model.students_size();
         ++student_index) {
      infeasibility_vars[student_index][time_slot] = mip_solver.MakeIntVar(
          0, class_count_,
          absl::StrFormat("f_%d_%d", student_index, time_slot));
      const Student& student = model.students(student_index);

      MPConstraint* const ct = mip_solver.MakeRowConstraint(0, 1);
      ct->SetCoefficient(infeasibility_vars[student_index][time_slot], -1);
      for (const int course_index : student.course_indices()) {
        for (const int class_index : course_to_classes_[course_index]) {
          if (!time_slot_to_classes[time_slot].contains(class_index)) continue;

          AddVariableIfNonNull(1, variables[student_index][class_index], ct);
        }
      }
    }
  }

  // Minimize the infeasibility vars. If the objective is 0, then we have found
  // a feasible solution for the course schedule. If the objective is greater
  // than 0, then some students were assigned to multiple courses for the same
  // time slot and we need to find a new schedule for the courses.
  MPObjective* const objective = mip_solver.MutableObjective();
  for (int student_index = 0; student_index < model.students_size();
       ++student_index) {
    for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
      objective->SetCoefficient(infeasibility_vars[student_index][time_slot],
                                1);
    }
  }

  mip_solver.SetSolverSpecificParametersAsString("limits/gap=0.01");
  MPSolver::ResultStatus status = mip_solver.Solve();
  ConflictPairs class_conflict_pairs;

  // This model will only be infeasible if the minimum or maximum capacities
  // are violated.
  if (status != MPSolver::OPTIMAL && status != MPSolver::FEASIBLE) {
    result->set_solver_status(MipStatusToCourseSchedulingResultStatus(status));
    result->clear_class_assignments();
    if (status == MPSolver::INFEASIBLE) {
      result->set_message(
          "Check the minimum or maximum capacity constraints for your "
          "classes.");
    }

    return class_conflict_pairs;
  }

  LOG(INFO) << "Finished assign students solver with " << objective->Value()
            << " schedule violations.";
  if (objective->Value() > 0) {
    for (int time_slot = 0; time_slot < time_slot_count_; ++time_slot) {
      for (int student_index = 0; student_index < model.students_size();
           ++student_index) {
        std::vector<int> conflicting_classes;
        MPVariable* const f_i = infeasibility_vars[student_index][time_slot];
        if (f_i != nullptr && f_i->solution_value() == 0) continue;

        for (const int class_index : time_slot_to_classes[time_slot]) {
          MPVariable* const y_i = variables[student_index][class_index];
          if (y_i != nullptr && y_i->solution_value() == 1) {
            conflicting_classes.push_back(class_index);
          }
        }
        InsertSortedPairs(conflicting_classes, &class_conflict_pairs);
      }
    }
    return class_conflict_pairs;
  }

  for (int student_index = 0; student_index < model.students_size();
       ++student_index) {
    StudentAssignment student_assignment;
    student_assignment.set_student_index(student_index);

    const Student& student = model.students(student_index);
    for (const int course_index : student.course_indices()) {
      for (int section_index = 0;
           section_index < course_to_classes_[course_index].size();
           ++section_index) {
        int class_index = course_to_classes_[course_index][section_index];
        MPVariable* const y_i = variables[student_index][class_index];

        if (y_i->solution_value() == 1) {
          student_assignment.add_course_indices(course_index);
          student_assignment.add_section_indices(section_index);
        }
      }
    }
    *result->add_student_assignments() = student_assignment;
  }

  return class_conflict_pairs;
}

CourseSchedulingResultStatus
CourseSchedulingSolver::MipStatusToCourseSchedulingResultStatus(
    MPSolver::ResultStatus mip_status) {
  switch (mip_status) {
    case MPSolver::ResultStatus::OPTIMAL:
      return SOLVER_OPTIMAL;
    case MPSolver::ResultStatus::FEASIBLE:
      return SOLVER_FEASIBLE;
    case MPSolver::ResultStatus::INFEASIBLE:
      return SOLVER_INFEASIBLE;
    case MPSolver::ResultStatus::UNBOUNDED:
    case MPSolver::ResultStatus::MODEL_INVALID:
      return SOLVER_MODEL_INVALID;
    case MPSolver::ResultStatus::NOT_SOLVED:
      return SOLVER_NOT_SOLVED;
    case MPSolver::ResultStatus::ABNORMAL:
      return ABNORMAL;
    default:
      return COURSE_SCHEDULING_RESULT_STATUS_UNSPECIFIED;
  }
}

absl::Status CourseSchedulingSolver::VerifyCourseSchedulingResult(
    const CourseSchedulingModel& model, const CourseSchedulingResult& result) {
  std::vector<absl::flat_hash_set<int>> class_to_time_slots(class_count_);
  std::vector<absl::flat_hash_set<int>> room_to_time_slots(model.rooms_size());
  for (const ClassAssignment& class_assignment : result.class_assignments()) {
    const int course_index = class_assignment.course_index();
    const int meetings_count = model.courses(course_index).meetings_count();
    const int consecutive_time_slots =
        model.courses(course_index).consecutive_slots_count();

    // Verify that each class meets the correct number of times.
    if (class_assignment.time_slots_size() !=
        meetings_count * consecutive_time_slots) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Verification failed: The course titled %s and section number %d "
          "meets %d times when it should meet %d times.",
          model.courses(course_index).display_name(),
          class_assignment.section_number(), class_assignment.time_slots_size(),
          meetings_count * consecutive_time_slots));
    }

    const int class_index =
        course_to_classes_[course_index][class_assignment.section_number()];
    std::vector<std::vector<int>> day_to_time_slots(model.days_count());
    for (const int time_slot : class_assignment.time_slots()) {
      class_to_time_slots[class_index].insert(time_slot);
      day_to_time_slots[std::floor(time_slot / model.daily_time_slot_count())]
          .push_back(time_slot);
    }

    // Verify that a class meets no more than its consecutive time slot count
    // per day. If a class needs 2 consecutive time slots, verify that it is
    // scheduled accordingly.
    for (int day = 0; day < model.days_count(); ++day) {
      const int day_count = day_to_time_slots[day].size();
      if (day_count != 0 && day_count != consecutive_time_slots) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Verification failed: The course titled %s does "
                            "not meet the correct number of times in "
                            "day %d.",
                            model.courses(course_index).display_name(), day));
      }
      if (day_count != 2) continue;

      const int first_slot = day_to_time_slots[day][0];
      const int second_slot = day_to_time_slots[day][1];
      if (std::abs(first_slot - second_slot) != 1) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Verification failed: The course titled %s is not "
                            "scheduled for consecutive time slots "
                            "in day %d.",
                            model.courses(course_index).display_name(), day));
      }
    }

    // Verify that their is no more than 1 class per room for each time slot.
    if (solve_for_rooms_) {
      for (int i = 0; i < class_assignment.room_indices_size(); ++i) {
        const int room = class_assignment.room_indices(i);
        const int time_slot = class_assignment.time_slots(i);
        if (room_to_time_slots[room].contains(time_slot)) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Verification failed: Multiple classes have "
                              "been assigned to room %s during time slot %d.",
                              model.rooms(room).display_name(), time_slot));
        }
        room_to_time_slots[room].insert(time_slot);
      }
    }
  }

  // Verify that each teacher is assigned to no more than one class per time
  // slot and that each teacher is not assigned to their restricted time slots.
  for (int teacher = 0; teacher < model.teachers_size(); ++teacher) {
    const auto& class_list = teacher_to_classes_[teacher];
    absl::flat_hash_set<int> teacher_time_slots;
    for (const int class_index : class_list) {
      for (const int time_slot : class_to_time_slots[class_index]) {
        if (teacher_to_restricted_slots_[teacher].contains(time_slot)) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Verification failed: Teacher with name %s has been assigned to "
              "restricted time slot %d.",
              model.teachers(teacher).display_name(), time_slot));
        }
        if (teacher_time_slots.contains(time_slot)) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Verification failed: Teacher with name %s has been assigned to "
              "multiple classes at time slot %d.",
              model.teachers(teacher).display_name(), time_slot));
        }
        teacher_time_slots.insert(time_slot);
      }
    }
  }

  std::vector<int> class_student_count(class_count_);
  for (const StudentAssignment& student_assignment :
       result.student_assignments()) {
    const int student_index = student_assignment.student_index();

    // Verify that each student is assigned to the correct courses.
    std::vector<int> enrolled_courses =
        std::vector<int>(model.students(student_index).course_indices().begin(),
                         model.students(student_index).course_indices().end());
    std::vector<int> assigned_courses =
        std::vector<int>(student_assignment.course_indices().begin(),
                         student_assignment.course_indices().end());
    std::sort(enrolled_courses.begin(), enrolled_courses.end());
    std::sort(assigned_courses.begin(), assigned_courses.end());
    if (enrolled_courses != assigned_courses) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Verification failed: Student with name %s has not "
                          "been assigned the correct courses.",
                          model.students(student_index).display_name()));
    }

    // Verify that each student is assigned to no more than one class per time
    // slot.
    absl::flat_hash_set<int> student_time_slots;
    for (int i = 0; i < student_assignment.course_indices_size(); ++i) {
      const int course_index = student_assignment.course_indices(i);
      const int section = student_assignment.section_indices(i);
      const int class_index = course_to_classes_[course_index][section];
      ++class_student_count[class_index];

      for (const int time_slot : class_to_time_slots[class_index]) {
        if (student_time_slots.contains(time_slot)) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Verification failed: Student with name %s has been assigned to "
              "multiple classes at time slot %d.",
              model.students(student_index).display_name(), time_slot));
        }
        student_time_slots.insert(time_slot);
      }
    }
  }

  // Verify size of each class is within the minimum and maximum capacities.
  for (int course = 0; course < model.courses_size(); ++course) {
    const int min_cap = model.courses(course).min_capacity();
    const int max_cap = model.courses(course).max_capacity();
    for (const int class_index : course_to_classes_[course]) {
      const int class_size = class_student_count[class_index];
      if (class_size < min_cap) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Verification failed: The course titled %s has %d students when it "
            "should have at least %d students.",
            model.courses(course).display_name(), class_size, min_cap));
      }
      if (class_size > max_cap) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Verification failed: The course titled %s has %d students when it "
            "should have no more than %d students.",
            model.courses(course).display_name(), class_size, max_cap));
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace operations_research
