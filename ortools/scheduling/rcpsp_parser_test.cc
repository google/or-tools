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

#include "ortools/scheduling/rcpsp_parser.h"

#include <string>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/path.h"

namespace operations_research {
namespace scheduling {
namespace rcpsp {

namespace {

std::string GetPath(absl::string_view filename) {
  constexpr absl::string_view kTestDataDir =
      "_main/ortools/scheduling/testdata/";
  return file::JoinPath(::testing::SrcDir(), kTestDataDir, filename);
}

TEST(RcpspParserTest, SingleMode) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("j301_1.sm")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(32, problem.tasks_size());
  EXPECT_EQ(4, problem.resources_size());
}

TEST(RcpspParserTest, MultiMode) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("c1510_1.mm.txt")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(18, problem.tasks_size());
  EXPECT_EQ(4, problem.resources_size());
}

TEST(RcpspParserTest, MultiModeMax) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("psp1.sch")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(12, problem.tasks_size());
  EXPECT_EQ(7, problem.resources_size());
  EXPECT_TRUE(problem.is_rcpsp_max());
}

TEST(RcpspParserTest, SingleModeMax) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("ubo_10_psp2.sch")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(12, problem.tasks_size());
  EXPECT_EQ(5, problem.resources_size());
  EXPECT_TRUE(problem.is_rcpsp_max());
  EXPECT_FALSE(problem.is_consumer_producer());
}

TEST(RcpspParserTest, SingleModeMaxReservoir) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("psp10_1.sch")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(12, problem.tasks_size());
  EXPECT_EQ(5, problem.resources_size());
  EXPECT_TRUE(problem.is_rcpsp_max());
  EXPECT_TRUE(problem.is_consumer_producer());
}

TEST(RcpspParserTest, SingleModeInvestment) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("rip1.sch")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(12, problem.tasks_size());
  EXPECT_EQ(1, problem.resources_size());
  EXPECT_TRUE(problem.is_resource_investment());
  EXPECT_EQ(19, problem.deadline());
}

TEST(RcpspParserTest, SingleModePatterson) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("rg30_set1_pat1.rcp")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(32, problem.tasks_size());
  EXPECT_EQ(4, problem.resources_size());
}

TEST(RcpspParserTest, SingleModeLargePatterson) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("rg300_1.rcp")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(302, problem.tasks_size());
  EXPECT_EQ(4, problem.resources_size());
}

TEST(RcpspParserTest, MultiModeMmLib) {
  RcpspParser parser;
  ASSERT_TRUE(parser.ParseFile(GetPath("mmlib100_j100100_1.mm.txt")));
  const RcpspProblem problem = parser.problem();
  EXPECT_EQ(102, problem.tasks_size());
  EXPECT_EQ(4, problem.resources_size());
}
}  // namespace
}  // namespace rcpsp
}  // namespace scheduling
}  // namespace operations_research
