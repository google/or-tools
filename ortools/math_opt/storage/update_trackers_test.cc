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

#include "ortools/math_opt/storage/update_trackers.h"

#include <ostream>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;

// Test class of Data to use with UpdateTrackers.
struct TestTrackerData {
  TestTrackerData(int x, int y) : x(x), y(y) {}
  int x;
  int y;
};

// Returns a matcher that expects TestTrackerData.x to be equal to expected_x.
::testing::Matcher<TestTrackerData> TestTrackerDataXEq(const int expected_x) {
  return ::testing::Field("x", &TestTrackerData::x, ::testing::Eq(expected_x));
}

std::ostream& operator<<(std::ostream& out, const TestTrackerData& data) {
  out << "{ x: " << data.x << " y: " << data.y << " }";
  return out;
}

TEST(UpdateTrackersTest, Empty) {
  UpdateTrackers<TestTrackerData> update_trackers;
  EXPECT_THAT(update_trackers.GetUpdatedTrackers(), IsEmpty());
}

TEST(UpdateTrackersTest, NewNeverUsedDeleted) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);
  update_trackers.DeleteUpdateTracker(tracker);
  EXPECT_THAT(update_trackers.GetUpdatedTrackers(), IsEmpty());
}

TEST(UpdateTrackersTest, NewUsedDeleted) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);
  EXPECT_THAT(update_trackers.GetUpdatedTrackers(),
              ElementsAre(Pair(tracker, Pointee(TestTrackerDataXEq(2)))));

  update_trackers.DeleteUpdateTracker(tracker);
  EXPECT_THAT(update_trackers.GetUpdatedTrackers(), IsEmpty());
}

TEST(UpdateTrackersTest, GetDataNewUnused) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const auto& const_ref = update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);

  EXPECT_THAT(const_ref.GetData(tracker), TestTrackerDataXEq(2));

  TestTrackerData& data = update_trackers.GetData(tracker);
  data.x = -2;
  EXPECT_THAT(const_ref.GetData(tracker), TestTrackerDataXEq(-2));
}

TEST(UpdateTrackersTest, GetDataNewUsed) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const auto& const_ref = update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);
  EXPECT_THAT(update_trackers.GetUpdatedTrackers(),
              ElementsAre(Pair(tracker, Pointee(TestTrackerDataXEq(2)))));

  EXPECT_THAT(const_ref.GetData(tracker), TestTrackerDataXEq(2));

  TestTrackerData& data = update_trackers.GetData(tracker);
  data.x = -2;
  EXPECT_THAT(const_ref.GetData(tracker), TestTrackerDataXEq(-2));
}

TEST(UpdateTrackersDeathTest, NewUsedDeleteTwice) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);
  update_trackers.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(update_trackers.DeleteUpdateTracker(tracker),
                            HasSubstr("does not exist"));
}

TEST(UpdateTrackersDeathTest, NewUsedDeleteAfterActualRemoval) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);
  update_trackers.DeleteUpdateTracker(tracker);

  // Actually remove the tracker.
  EXPECT_THAT(update_trackers.GetUpdatedTrackers(), IsEmpty());

  EXPECT_DEATH_IF_SUPPORTED(update_trackers.DeleteUpdateTracker(tracker),
                            HasSubstr("does not exist"));
}

TEST(UpdateTrackersDeathTest, GetDataNewUsedDelete) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const auto& const_ref = update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);
  update_trackers.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(const_ref.GetData(tracker),
                            HasSubstr("does not exist"));
  EXPECT_DEATH_IF_SUPPORTED(update_trackers.GetData(tracker),
                            HasSubstr("does not exist"));
}

TEST(UpdateTrackersDeathTest, GetDataAfterActualRemoval) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const auto& const_ref = update_trackers;
  const UpdateTrackerId tracker =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/3);
  update_trackers.DeleteUpdateTracker(tracker);

  // Actually remove the tracker.
  EXPECT_THAT(update_trackers.GetUpdatedTrackers(), IsEmpty());

  EXPECT_DEATH_IF_SUPPORTED(const_ref.GetData(tracker),
                            HasSubstr("does not exist"));
  EXPECT_DEATH_IF_SUPPORTED(update_trackers.GetData(tracker),
                            HasSubstr("does not exist"));
}

TEST(UpdateTrackersTest, MultipleTrackers) {
  UpdateTrackers<TestTrackerData> update_trackers;
  const UpdateTrackerId tracker1 =
      update_trackers.NewUpdateTracker(/*x=*/1, /*y=*/1);
  const UpdateTrackerId tracker2 =
      update_trackers.NewUpdateTracker(/*x=*/2, /*y=*/2);
  const UpdateTrackerId tracker3 =
      update_trackers.NewUpdateTracker(/*x=*/3, /*y=*/3);

  update_trackers.DeleteUpdateTracker(tracker2);

  EXPECT_THAT(update_trackers.GetUpdatedTrackers(),
              ElementsAre(Pair(tracker1, Pointee(TestTrackerDataXEq(1))),
                          Pair(tracker3, Pointee(TestTrackerDataXEq(3)))));

  EXPECT_THAT(update_trackers.GetUpdatedTrackers(),
              ElementsAre(Pair(tracker1, Pointee(TestTrackerDataXEq(1))),
                          Pair(tracker3, Pointee(TestTrackerDataXEq(3)))));

  update_trackers.DeleteUpdateTracker(tracker1);
  const UpdateTrackerId tracker4 =
      update_trackers.NewUpdateTracker(/*x=*/4, /*y=*/4);

  EXPECT_THAT(update_trackers.GetUpdatedTrackers(),
              ElementsAre(Pair(tracker3, Pointee(TestTrackerDataXEq(3))),
                          Pair(tracker4, Pointee(TestTrackerDataXEq(4)))));
}

}  // namespace
}  // namespace operations_research::math_opt
