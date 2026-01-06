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

#include "ortools/scheduling/course_scheduling.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/scheduling/course_scheduling.pb.h"

namespace operations_research {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

TEST(CourseSchedulingTest, CheckMultipleSectionsCorrectlyScheduled) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 2
    daily_time_slot_count: 2
    courses {
      meetings_count: 2
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_indices: 1
      teacher_section_counts: 1
      teacher_section_counts: 2
    }
    teachers {}
    teachers { restricted_time_slots: 1 }
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckDailyMaximumForCoursesIsNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      meetings_count: 4
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckTeacherIsNotDoubleBooked) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckRoomAssignmentsForTimeSlotNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
      room_indices: 0
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 1
      teacher_section_counts: 1
      room_indices: 0
    }
    teachers {}
    teachers {}
    rooms {}
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest,
     CheckConsecutiveTimeslotsValuesStartOfDayNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 2
    daily_time_slot_count: 4
    courses {
      meetings_count: 1
      consecutive_slots_count: 2
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {
      restricted_time_slots: 1
      restricted_time_slots: 2
      restricted_time_slots: 3
      restricted_time_slots: 5
      restricted_time_slots: 6
      restricted_time_slots: 7
    }
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckConsecutiveTimeslotsValuesEndOfDayNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 2
    daily_time_slot_count: 4
    courses {
      meetings_count: 1
      consecutive_slots_count: 2
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {
      restricted_time_slots: 0
      restricted_time_slots: 1
      restricted_time_slots: 2
      restricted_time_slots: 4
      restricted_time_slots: 5
      restricted_time_slots: 6
    }
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckSingletonCoursesNotScheduledForSameTime) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 0
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 1
      teacher_section_counts: 1
    }
    teachers {}
    teachers {}
    students { course_indices: 0 course_indices: 1 }
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckMinimumCapacityForCourseNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 6
      max_capacity: 10
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
    students { course_indices: 0 }
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "Check the minimum or maximum capacity constraints for your "
             "classes."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckMaximumCapacityForCourseNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 2
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
    students { course_indices: 0 }
    students { course_indices: 0 }
    students { course_indices: 0 }
  )pb");
  const CourseSchedulingResult infeasible_result_ = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "Check the minimum or maximum capacity constraints for your "
             "classes."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result_));
}

TEST(CourseSchedulingTest, CheckStudentsAreNotDoubleBooked) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 0
      teacher_indices: 1
      teacher_section_counts: 1
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 2
      teacher_section_counts: 1
    }
    teachers { restricted_time_slots: 1 }
    teachers { restricted_time_slots: 1 }
    teachers { restricted_time_slots: 1 }
    students { course_indices: 0 course_indices: 1 }
  )pb");
  const CourseSchedulingResult infeasible_result = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result));
}

TEST(CourseSchedulingTest, CheckStudentsAreNotDoubleBooked_TooManyCourses) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 0
      teacher_section_counts: 2
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 1
      teacher_indices: 2
      teacher_section_counts: 1
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 2
      teacher_section_counts: 1
    }
    teachers {}
    teachers {}
    teachers {}
    students { course_indices: 0 course_indices: 1 course_indices: 2 }
  )pb");
  const CourseSchedulingResult infeasible_result = ParseTestProto(R"pb(
    solver_status: SOLVER_INFEASIBLE
    message: "The problem is infeasible with the given courses."
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(infeasible_result));
}

TEST(CourseSchedulingTest, CheckTeacherNotScheduledForRestrictedSlot) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_indices: 1
      teacher_section_counts: 1
      teacher_section_counts: 1
    }
    teachers { restricted_time_slots: 1 }
    teachers { restricted_time_slots: 0 }
  )pb");
  const CourseSchedulingResult expected_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    class_assignments { course_index: 0 section_number: 1 time_slots: 1 }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(expected_result));
}

TEST(CourseSchedulingTest, CheckConsecutiveTimeSlotsValuesNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 2
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 2
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers { restricted_time_slots: 1 }
  )pb");
  const CourseSchedulingResult expected_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments {
      course_index: 0
      section_number: 0
      time_slots: 2
      time_slots: 3
    }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(expected_result));
}

TEST(CourseSchedulingTest, CheckStudentsCorrectlyAssignedToSingletonCourses) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 0
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 1
      teacher_section_counts: 1
    }
    teachers { restricted_time_slots: 0 }
    teachers {}
    students { course_indices: 0 course_indices: 1 }
    students { course_indices: 1 course_indices: 0 }
  )pb");
  const CourseSchedulingResult expected_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 1 }
    class_assignments { course_index: 1 section_number: 0 time_slots: 0 }
    student_assignments {
      student_index: 0
      course_indices: 0
      course_indices: 1
      section_indices: 0
      section_indices: 0
    }
    student_assignments {
      student_index: 1
      course_indices: 1
      course_indices: 0
      section_indices: 0
      section_indices: 0
    }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(expected_result));
}

TEST(CourseSchedulingTest, CheckStudentsNotDoubleBookedForTimeSlot) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 0
      teacher_indices: 1
      teacher_indices: 2
      teacher_section_counts: 1
      teacher_section_counts: 1
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      min_capacity: 0
      max_capacity: 5
      teacher_indices: 3
      teacher_section_counts: 1
    }
    teachers { restricted_time_slots: 1 }
    teachers { restricted_time_slots: 1 }
    teachers { restricted_time_slots: 0 }
    teachers { restricted_time_slots: 1 }
    students { course_indices: 0 course_indices: 1 }
  )pb");
  const CourseSchedulingResult expected_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    class_assignments { course_index: 0 section_number: 1 time_slots: 0 }
    class_assignments { course_index: 0 section_number: 2 time_slots: 1 }
    class_assignments { course_index: 1 section_number: 0 time_slots: 0 }
    student_assignments {
      student_index: 0
      course_indices: 0
      course_indices: 1
      section_indices: 2
      section_indices: 0
    }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(expected_result));
}

TEST(CourseSchedulingTest,
     AssertErrorWhenTeacherIndexNumberDoesNotMatchNumSectionsNumber) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      display_name: "English"
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_indices: 1
      teacher_section_counts: 2
    }
    teachers {}
    teachers {}
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), SOLVER_MODEL_INVALID);
  ASSERT_EQ(result.message(),
            "The course titled English should have the same number of teacher "
            "indices and section numbers.");
}

TEST(CourseSchedulingTest,
     AssertErrorWhenCourseIsAllottedRoomIndexThatDoesNotExist) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      display_name: "English"
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
      room_indices: 1
    }
    teachers {}
    rooms {}
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), SOLVER_MODEL_INVALID);
  ASSERT_EQ(result.message(),
            "The course titled English is slotted for room index 1 but there "
            "are only 1 rooms.");
}

TEST(CourseSchedulingTest,
     AssertErrorWhenCourseIsGivenTeacherIndexThatDoesNotExist) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      display_name: "English"
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 1
      teacher_section_counts: 1
    }
    teachers {}
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), SOLVER_MODEL_INVALID);
  ASSERT_EQ(result.message(),
            "The course titled English has teacher 1 assigned to it but there "
            "are only 1 teachers.");
}

TEST(CourseSchedulingTest,
     AssertErrorWhenConsecutiveTimeSlotNumberIsMoreThanTwo) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      display_name: "English"
      meetings_count: 1
      consecutive_slots_count: 3
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), SOLVER_MODEL_INVALID);
  ASSERT_EQ(result.message(),
            "The course titled English has 3 consecutive time slots specified "
            "when it can only have 1 or 2.");
}

TEST(CourseSchedulingTest,
     AssertErrorWhenTeacherHasRestrictedTimeSlotThatDoesNotExist) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      display_name: "English"
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers { display_name: "A" restricted_time_slots: 1 }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), SOLVER_MODEL_INVALID);
  ASSERT_EQ(result.message(),
            "Teacher with name A has restricted time slot 1 but there are only "
            "1 time slots.");
}

TEST(CourseSchedulingTest,
     AssertErrorWhenStudentHasCourseIndexThatDoesNotExist) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      display_name: "English"
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
    students { display_name: "Marvin" course_indices: 1 }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), SOLVER_MODEL_INVALID);
  ASSERT_EQ(result.message(),
            "Student with name Marvin has course index 1 but there are only 1 "
            "courses.");
}

class CourseSchedulingVerifierTestSolver : public CourseSchedulingSolver {
 public:
  void SetResultToReturn(CourseSchedulingResult result_to_return) {
    result_to_return_ = result_to_return;
  }

 protected:
  CourseSchedulingResult SolveModel(
      const CourseSchedulingModel& model,
      const ConflictPairs& class_conflicts) override {
    return result_to_return_;
  }

 private:
  CourseSchedulingResult result_to_return_;
};

TEST(CourseSchedulingTest, CheckVerifierErrorWhenTwoClassesAssignedToSameRoom) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
      room_indices: 0
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 1
      teacher_section_counts: 1
      room_indices: 0
    }
    teachers {}
    teachers {}
    rooms { display_name: "Zaphod" }
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments {
      course_index: 0
      section_number: 0
      time_slots: 0
      room_indices: 0
    }
    class_assignments {
      course_index: 1
      section_number: 0
      time_slots: 0
      room_indices: 0
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: Multiple classes have been assigned to room "
            "Zaphod during time slot 0.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenClassDoesNotMeetCorrectNumberOfTimes) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      display_name: "English"
      meetings_count: 3
      consecutive_slots_count: 2
      teacher_indices: 0
      teacher_section_counts: 2
    }
    teachers {}
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments {
      course_index: 0
      section_number: 0
      time_slots: 0
      time_slots: 1
      time_slots: 2
      time_slots: 3
      time_slots: 4
      time_slots: 5
    }
    class_assignments {
      course_index: 0
      section_number: 1
      time_slots: 0
      time_slots: 1
      time_slots: 2
      time_slots: 3
      time_slots: 5
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: The course titled English and section number "
            "1 meets 5 times when it should meet 6 times.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenClassMeetsMoreThanConsecutiveSlotCountPerDay) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 2
    daily_time_slot_count: 3
    courses {
      display_name: "English"
      meetings_count: 2
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments {
      course_index: 0
      section_number: 0
      time_slots: 3
      time_slots: 4
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: The course titled English does not meet the "
            "correct number of "
            "times in day 1.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenClassIsNotScheduledForConsecutiveSlots) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 2
    daily_time_slot_count: 3
    courses {
      display_name: "English"
      meetings_count: 2
      consecutive_slots_count: 2
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments {
      course_index: 0
      section_number: 0
      time_slots: 1
      time_slots: 2
      time_slots: 3
      time_slots: 5
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: The course titled English is not scheduled "
            "for consecutive time "
            "slots in day 1.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenTeacherIsDoubleBookedForATimeSlot) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers { display_name: "Marvin" }
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    class_assignments { course_index: 1 section_number: 0 time_slots: 0 }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: Teacher with name Marvin has been assigned "
            "to multiple classes at time slot 0.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenTeacherIsAssignedToRestrictedTimeSlot) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers { display_name: "Marvin" restricted_time_slots: 1 }
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    class_assignments { course_index: 1 section_number: 0 time_slots: 1 }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: Teacher with name Marvin has been assigned "
            "to restricted time slot 1.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenStudentNotAssignedToCorrectCourses) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 3
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 1
      teacher_section_counts: 1
    }
    teachers {}
    teachers {}
    students { display_name: "Marvin" course_indices: 0 }
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    class_assignments { course_index: 1 section_number: 0 time_slots: 0 }
    student_assignments {
      student_index: 0
      course_indices: 1
      section_indices: 0
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: Student with name Marvin has not been "
            "assigned the correct courses.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenStudentIsDoubleBookedForATimeSlot) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 1
      teacher_section_counts: 1
    }
    teachers {}
    teachers {}
    students { display_name: "Marvin" course_indices: 0 course_indices: 1 }
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    class_assignments { course_index: 1 section_number: 0 time_slots: 0 }
    student_assignments {
      student_index: 0
      course_indices: 0
      course_indices: 1
      section_indices: 0
      section_indices: 0
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: Student with name Marvin has been assigned "
            "to multiple classes at time slot 0.");
}

TEST(CourseSchedulingTest,
     CheckVerifierErrorWhenClassSizeDoesNotReachMinCapacity) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      display_name: "English"
      meetings_count: 1
      min_capacity: 3
      max_capacity: 10
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
    students { course_indices: 0 }
    students { course_indices: 0 }
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    student_assignments {
      student_index: 0
      course_indices: 0
      section_indices: 0
    }
    student_assignments {
      student_index: 1
      course_indices: 0
      section_indices: 0
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: The course titled English has 2 students "
            "when it should have at least 3 students.");
}

TEST(CourseSchedulingTest, CheckVerifierErrorWhenClassSizeExceedsMaxCapacity) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      display_name: "English"
      meetings_count: 1
      min_capacity: 0
      max_capacity: 2
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
    }
    teachers {}
    students { course_indices: 0 }
    students { course_indices: 0 }
    students { course_indices: 0 }
  )pb");
  const CourseSchedulingResult error_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments { course_index: 0 section_number: 0 time_slots: 0 }
    student_assignments {
      student_index: 0
      course_indices: 0
      section_indices: 0
    }
    student_assignments {
      student_index: 1
      course_indices: 0
      section_indices: 0
    }
    student_assignments {
      student_index: 2
      course_indices: 0
      section_indices: 0
    }
  )pb");

  CourseSchedulingVerifierTestSolver solver;
  solver.SetResultToReturn(error_result);
  const CourseSchedulingResult result = solver.Solve(model);

  ASSERT_EQ(result.solver_status(), ABNORMAL);
  ASSERT_EQ(result.message(),
            "Verification failed: The course titled English has 3 students "
            "when it should have no more than 2 students.");
}

TEST(CourseSchedulingTest, CheckRoomAssignmentsForCourseNotViolated) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 1
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
      room_indices: 1
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 1
      teacher_section_counts: 1
      room_indices: 0
    }
    teachers {}
    teachers {}
    rooms {}
    rooms {}
  )pb");
  const CourseSchedulingResult expected_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments {
      course_index: 0
      section_number: 0
      time_slots: 0
      room_indices: 1
    }
    class_assignments {
      course_index: 1
      section_number: 0
      time_slots: 0
      room_indices: 0
    }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(expected_result));
}

TEST(CourseSchedulingTest, CheckConsecutiveTimeSlotsScheduledForSameRoom) {
  const CourseSchedulingModel model = ParseTestProto(R"pb(
    days_count: 1
    daily_time_slot_count: 2
    courses {
      meetings_count: 1
      consecutive_slots_count: 1
      teacher_indices: 0
      teacher_section_counts: 1
      room_indices: 0
    }
    courses {
      meetings_count: 1
      consecutive_slots_count: 2
      teacher_indices: 1
      teacher_section_counts: 1
      room_indices: 0
      room_indices: 1
    }
    teachers { restricted_time_slots: 0 }
    teachers {}
    rooms {}
    rooms {}
  )pb");
  const CourseSchedulingResult expected_result = ParseTestProto(R"pb(
    solver_status: SOLVER_OPTIMAL
    class_assignments {
      course_index: 0
      section_number: 0
      time_slots: 1
      room_indices: 0
    }
    class_assignments {
      course_index: 1
      section_number: 0
      time_slots: 0
      room_indices: 1
      time_slots: 1
      room_indices: 1
    }
  )pb");

  CourseSchedulingSolver solver;
  const CourseSchedulingResult result = solver.Solve(model);

  EXPECT_THAT(result, testing::EqualsProto(expected_result));
}

}  // namespace
}  // namespace operations_research
