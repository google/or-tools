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

#include "ortools/sat/sat_status.h"

#include "gtest/gtest.h"

namespace operations_research {
namespace sat {
namespace {

TEST(SatStatusTest, UnsatAndFeasibilityStatus) {
  constexpr FeasibilityStatus from_unsat = kUnsat;
  EXPECT_TRUE(from_unsat.unsat());
  EXPECT_FALSE(from_unsat.ok());
  EXPECT_EQ(from_unsat, FeasibilityStatus(false));
  EXPECT_NE(from_unsat, kOk);

  EXPECT_FALSE(kOk.unsat());
  EXPECT_TRUE(kOk.ok());
  EXPECT_EQ(kOk, FeasibilityStatus(true));
  EXPECT_EQ(kUnsat, UnsatStatus{});
}

TEST(SatStatusTest, ModificationStatus) {
  EXPECT_FALSE(kUnchanged.modified());
  EXPECT_TRUE(kModified.modified());
  EXPECT_EQ(kUnchanged, ModificationStatus(false));
  EXPECT_EQ(kModified, ModificationStatus(true));
  EXPECT_NE(kUnchanged, kModified);

  ModificationStatus acc = kUnchanged;
  acc |= kUnchanged;
  EXPECT_EQ(acc, kUnchanged);
  acc |= kModified;
  EXPECT_EQ(acc, kModified);
  acc |= kUnchanged;
  EXPECT_EQ(acc, kModified);
}

TEST(SatStatusTest, ChangeStatus) {
  constexpr ChangeStatus from_unsat = kUnsat;
  EXPECT_TRUE(from_unsat.unsat());
  EXPECT_FALSE(from_unsat.modified());
  EXPECT_EQ(from_unsat, ChangeStatus(FeasibilityStatus(kUnsat)));
  EXPECT_EQ(FeasibilityStatus(from_unsat), kUnsat);

  constexpr ChangeStatus unchanged = kUnchanged;
  EXPECT_FALSE(unchanged.unsat());
  EXPECT_FALSE(unchanged.modified());
  EXPECT_EQ(unchanged, ChangeStatus(false));
  EXPECT_EQ(unchanged, ChangeStatus(kOk));
  EXPECT_EQ(FeasibilityStatus(unchanged), kOk);

  constexpr ChangeStatus modified = kModified;
  EXPECT_FALSE(modified.unsat());
  EXPECT_TRUE(modified.modified());
  EXPECT_EQ(modified, ChangeStatus(true));
  EXPECT_EQ(FeasibilityStatus(modified), kOk);
  EXPECT_NE(unchanged, modified);
  EXPECT_NE(from_unsat, unchanged);
  EXPECT_NE(from_unsat, modified);

  ChangeStatus acc = kUnchanged;
  acc |= kUnchanged;
  EXPECT_EQ(acc, kUnchanged);
  acc |= kModified;
  EXPECT_EQ(acc, kModified);

  ChangeStatus unsat_acc = kUnsat;
  unsat_acc |= kModified;
  EXPECT_EQ(unsat_acc, kUnsat);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
