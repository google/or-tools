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

#ifndef OR_TOOLS_EXAMPLES_COURSE_SCHEDULING_H_
#define OR_TOOLS_EXAMPLES_COURSE_SCHEDULING_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "examples/cpp/course_scheduling.pb.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
class CourseSchedulingSolver {
 public:
  CourseSchedulingSolver() : solve_for_rooms_(false) {}
  virtual ~CourseSchedulingSolver() {}

  using ConflictPairs = absl::flat_hash_set<std::pair<int, int>>;

  CourseSchedulingResult Solve(const CourseSchedulingModel& model);

 protected:
  virtual absl::Status ValidateModelAndLoadClasses(
      const CourseSchedulingModel& model);

  virtual CourseSchedulingResult SolveModel(
      const CourseSchedulingModel& model, const ConflictPairs& class_conflicts);

  virtual absl::Status VerifyCourseSchedulingResult(
      const CourseSchedulingModel& model, const CourseSchedulingResult& result);

 private:
  CourseSchedulingResult ScheduleCourses(const ConflictPairs& class_conflicts,
                                         const CourseSchedulingModel& model);

  // This method will modify the CourseSchedulingResult returned from
  // ScheduleCoursesMip, which is why the result is passed in as a pointer.
  ConflictPairs AssignStudents(const CourseSchedulingModel& model,
                               CourseSchedulingResult* result);

  int GetTeacherIndex(int course_index, int section);

  void InsertSortedPairs(const std::vector<int>& list, ConflictPairs* pairs);

  bool ShouldCreateVariable(int course_index, int section, int time_slot,
                            int room);

  std::vector<int> GetRoomIndices(const Course& course);

  std::vector<absl::flat_hash_set<int>> GetClassesByTimeSlot(
      const CourseSchedulingResult* result);

  void AddVariableIfNonNull(double coeff, const MPVariable* var,
                            MPConstraint* ct);

  CourseSchedulingResultStatus MipStatusToCourseSchedulingResultStatus(
      MPSolver::ResultStatus mip_status);

  bool solve_for_rooms_;
  int class_count_;
  int time_slot_count_;
  int room_count_;
  ConflictPairs course_conflicts_;
  std::vector<absl::flat_hash_set<int>> teacher_to_classes_;
  std::vector<absl::flat_hash_set<int>> teacher_to_restricted_slots_;
  std::vector<std::vector<int>> course_to_classes_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_COURSE_SCHEDULING_H_
