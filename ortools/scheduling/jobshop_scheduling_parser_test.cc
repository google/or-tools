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

#include "ortools/scheduling/jobshop_scheduling_parser.h"

#include <string>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/path.h"

namespace operations_research {
namespace scheduling {
namespace jssp {
namespace {

std::string GetPath(absl::string_view filename) {
  constexpr absl::string_view kTestDataDir =
      "_main/ortools/scheduling/testdata/";
  return file::JoinPath(::testing::SrcDir(), kTestDataDir, filename);
}

TEST(RcpspParserTest, Jssp) {
  JsspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("ft06")));
  const JsspInputProblem problem = parser.problem();
  EXPECT_EQ(6, problem.jobs_size());
  EXPECT_EQ(6, problem.machines_size());
}

TEST(RcpspParserTest, Taillard) {
  JsspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("50_10_01_ta041.txt")));
  const JsspInputProblem problem = parser.problem();
  EXPECT_EQ(50, problem.jobs_size());
  EXPECT_EQ(10, problem.machines_size());
}

TEST(RcpspParserTest, Flexible) {
  JsspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("02a.fjs")));
  const JsspInputProblem problem = parser.problem();
  EXPECT_EQ(10, problem.jobs_size());
  EXPECT_EQ(5, problem.machines_size());
}

TEST(RcpspParserTest, Sdst) {
  JsspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("SDST10_ta001.txt")));
  const JsspInputProblem problem = parser.problem();
  EXPECT_EQ(20, problem.jobs_size());
  EXPECT_EQ(5, problem.machines_size());
  for (const Machine& m : problem.machines()) {
    ASSERT_TRUE(m.has_transition_time_matrix());
    EXPECT_EQ(m.transition_time_matrix().transition_time_size(),
              problem.jobs_size() * problem.jobs_size());
  }
}

TEST(RcpspParserTest, Tardiness) {
  JsspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("jb1.txt")));
  const JsspInputProblem problem = parser.problem();
  EXPECT_EQ(10, problem.jobs_size());
  EXPECT_EQ(5, problem.machines_size());
}

TEST(RcpspParserTest, Pss) {
  JsspParser parser;
  ASSERT_TRUE(
      parser.ParseFile(GetPath("taillard-jobshop-15_15-1_225_100_150-1")));
  const JsspInputProblem problem = parser.problem();
  EXPECT_EQ(15, problem.jobs_size());
  EXPECT_EQ(15, problem.machines_size());
}

TEST(RcpspParserTest, EarlyTardy) {
  JsspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("1010_1_3")));
  const JsspInputProblem problem = parser.problem();
  EXPECT_EQ(10, problem.jobs_size());
  EXPECT_EQ(10, problem.machines_size());
  EXPECT_EQ(1033, problem.jobs(0).early_due_date());
  EXPECT_EQ(1033, problem.jobs(0).late_due_date());
  EXPECT_EQ(3, problem.jobs(0).earliness_cost_per_time_unit());
  EXPECT_EQ(10, problem.jobs(0).lateness_cost_per_time_unit());
}

}  // namespace
}  // namespace jssp
}  // namespace scheduling
}  // namespace operations_research
