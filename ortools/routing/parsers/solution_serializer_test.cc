// Copyright 2010-2024 Google LLC
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

#include "ortools/routing/parsers/solution_serializer.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/mutable_memfile.h"
#include "ortools/base/options.h"
#include "ortools/routing/parsers/simple_graph.h"

namespace operations_research::routing {
namespace {

using testing::MatchesRegex;

TEST(RoutingSolutionSerializerTest, RoutingSolutionEventComparison) {
  RoutingSolution::Event t1 = {RoutingSolution::Event::Type::kStart, 0,
                               Arc{0, 0}};
  RoutingSolution::Event t2 = {RoutingSolution::Event::Type::kStart, 0,
                               Arc{0, 0}};  // Same as t1.
  RoutingSolution::Event t3 = {RoutingSolution::Event::Type::kEnd, 0,
                               Arc{0, 0}};
  RoutingSolution::Event t4 = {RoutingSolution::Event::Type::kStart, 1,
                               Arc{0, 0}};
  RoutingSolution::Event t5 = {RoutingSolution::Event::Type::kStart, 0,
                               Arc{1, 0}};
  RoutingSolution::Event t6 = {RoutingSolution::Event::Type::kStart, 0,
                               Arc{0, 1}};
  EXPECT_EQ(t1, t1);
  EXPECT_EQ(t1, t2);
  EXPECT_NE(t1, t3);
  EXPECT_NE(t1, t4);
  EXPECT_NE(t1, t5);
  EXPECT_NE(t1, t6);
}

TEST(RoutingSolutionSerializerTest, ParseEmptyString) {
  EXPECT_EQ(RoutingOutputFormatFromString(""), RoutingOutputFormat::kNone);
}

TEST(RoutingSolutionSerializerTest, ParseUnrecognizedString) {
  EXPECT_EQ(RoutingOutputFormatFromString("ThisIsPureGarbage"),
            RoutingOutputFormat::kNone);
}

TEST(RoutingSolutionSerializerTest, ParseNoneString) {
  EXPECT_EQ(RoutingOutputFormatFromString("NONE"), RoutingOutputFormat::kNone);
}

TEST(RoutingSolutionSerializerTest, ParseTsplibString) {
  EXPECT_EQ(RoutingOutputFormatFromString("tsplib"),
            RoutingOutputFormat::kTSPLIB);
  EXPECT_EQ(RoutingOutputFormatFromString("TSPLIB"),
            RoutingOutputFormat::kTSPLIB);
}

TEST(RoutingSolutionSerializerTest, ParseCvrplibString) {
  EXPECT_EQ(RoutingOutputFormatFromString("cvrplib"),
            RoutingOutputFormat::kCVRPLIB);
  EXPECT_EQ(RoutingOutputFormatFromString("CVRPLIB"),
            RoutingOutputFormat::kCVRPLIB);
}

TEST(RoutingSolutionSerializerTest, ParseCarplibString) {
  EXPECT_EQ(RoutingOutputFormatFromString("carplib"),
            RoutingOutputFormat::kCARPLIB);
  EXPECT_EQ(RoutingOutputFormatFromString("CARPLIB"),
            RoutingOutputFormat::kCARPLIB);
}

TEST(RoutingSolutionSerializerTest, ParseNearplibString) {
  EXPECT_EQ(RoutingOutputFormatFromString("nearplib"),
            RoutingOutputFormat::kNEARPLIB);
  EXPECT_EQ(RoutingOutputFormatFromString("NEARPLIB"),
            RoutingOutputFormat::kNEARPLIB);
}

TEST(RoutingSolutionSerializerTest, FromSplitRoutesWithOneRoute) {
  // Specifically test RouteFromVector in the implementation.
  const std::vector<std::vector<int64_t>> routes{{0, 1, 3, 0}};
  const RoutingSolution result = RoutingSolution::FromSplitRoutes(routes);

  const RoutingSolution expected_output = RoutingSolution{
      std::vector<RoutingSolution::Route>{RoutingSolution::Route{
          RoutingSolution::Event{RoutingSolution::Event::Type::kStart, -1,
                                 Arc{0, 0}},
          RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                 Arc{0, 1}},
          RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                 Arc{1, 3}},
          RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                 Arc{3, 0}},
          RoutingSolution::Event{RoutingSolution::Event::Type::kEnd, -1,
                                 Arc{0, 0}},
      }},
      std::vector<int64_t>{-1},
      std::vector<int64_t>{-1},
  };
  EXPECT_EQ(result, expected_output);
}

TEST(RoutingSolutionSerializerTest, FromSplitRoutesWithTwoRoutes) {
  // Specifically test RoutesFromVector in the implementation.
  const std::vector<std::vector<int64_t>> routes{
      {0, 1, 3, 0},
      {0, 2, 0},
  };
  const RoutingSolution result = RoutingSolution::FromSplitRoutes(routes);

  const RoutingSolution expected_output = {RoutingSolution{
      std::vector<RoutingSolution::Route>{
          RoutingSolution::Route{
              RoutingSolution::Event{RoutingSolution::Event::Type::kStart, -1,
                                     Arc{0, 0}},
              RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                     Arc{0, 1}},
              RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                     Arc{1, 3}},
              RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                     Arc{3, 0}},
              RoutingSolution::Event{RoutingSolution::Event::Type::kEnd, -1,
                                     Arc{0, 0}},
          },
          RoutingSolution::Route{
              RoutingSolution::Event{RoutingSolution::Event::Type::kStart, -1,
                                     Arc{0, 0}},
              RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                     Arc{0, 2}},
              RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                     Arc{2, 0}},
              RoutingSolution::Event{RoutingSolution::Event::Type::kEnd, -1,
                                     Arc{0, 0}},
          },
      },
      std::vector<int64_t>{-1, -1},
      std::vector<int64_t>{-1, -1},
  }};

  EXPECT_EQ(result, expected_output);
}

TEST(RoutingSolutionSerializerTest, SolutionToTsplib) {
  const std::vector<int64_t> solution{0, 1, 2, 3, 0, -1, 0, 4, 5, 6, 0, -1};
  const std::string expected_output =
      "0[\r\n]+1[\r\n]+2[\r\n]+3[\r\n]+0[\r\n]+-1[\r\n]+"
      "0[\r\n]+4[\r\n]+5[\r\n]+6[\r\n]+0[\r\n]+-1[\r\n]+";
  EXPECT_THAT(RoutingSolution::FromSplitRoutes(
                  RoutingSolution::SplitRoutes(solution, -1), 0)
                  .SerializeToString(RoutingOutputFormat::kTSPLIB),
              MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, SolutionToTsplibFile) {
  const std::string file_name{std::tmpnam(nullptr)};
  RegisteredMutableMemFile registered(file_name);

  const std::vector<std::vector<int64_t>> solution_vector{{0, 1, 2, 3, 0},
                                                          {0, 4, 5, 6, 0}};
  const std::string expected_output =
      "NAME : Test name[\r\n]+"
      "COMMENT : Length = -1; Total time = -1.000000 s[\r\n]+"
      "TYPE : TOUR[\r\n]+"
      "DIMENSION : 7[\r\n]+"
      "TOUR_SECTION[\r\n]+"
      "0[\r\n]+1[\r\n]+2[\r\n]+3[\r\n]+0[\r\n]+-1[\r\n]+"
      "0[\r\n]+4[\r\n]+5[\r\n]+6[\r\n]+0[\r\n]+-1[\r\n]+"
      "EOF";

  RoutingSolution solution =
      RoutingSolution::FromSplitRoutes(solution_vector, 0);
  solution.SetName("Test name");
  solution.WriteToSolutionFile(RoutingOutputFormat::kTSPLIB, file_name);
  std::string written_solution;
  CHECK_OK(file::GetContents(file_name, &written_solution, file::Defaults()));
  EXPECT_THAT(written_solution, MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, SolutionToCvrplib) {
  // Depot: 1.
  const std::vector<int64_t> solution{1, 2, 3, 1, -1, 1, 4, 5, 6, 1, -1};
  const std::string expected_output =
      "Route #1: 1 2[\r\n]+"
      "Route #2: 3 4 5[\r\n]+";

  EXPECT_THAT(RoutingSolution::FromSplitRoutes(
                  RoutingSolution::SplitRoutes(solution, -1), 1)
                  .SerializeToString(RoutingOutputFormat::kCVRPLIB),
              MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, SolutionToCvrplibInvalidNoStart) {
  const std::vector<RoutingSolution::Route> routes = {
      RoutingSolution::Route{
          RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                 Arc{0, 1}},
          RoutingSolution::Event{RoutingSolution::Event::Type::kEnd, -1,
                                 Arc{0, 0}},
      },
  };
  const RoutingSolution solution{routes, {4}, {4}};
  std::string solution_str;

  EXPECT_DEATH(
      solution_str = solution.SerializeToString(RoutingOutputFormat::kCVRPLIB),
      "");
}

TEST(RoutingSolutionSerializerTest, SolutionToCvrplibInvalidNoEnd) {
  const std::vector<RoutingSolution::Route> routes = {
      RoutingSolution::Route{
          RoutingSolution::Event{RoutingSolution::Event::Type::kStart, -1,
                                 Arc{0, 0}},
          RoutingSolution::Event{RoutingSolution::Event::Type::kTransit, -1,
                                 Arc{0, 1}},
      },
  };
  const RoutingSolution solution{routes, {4}, {4}};
  std::string solution_str;

  EXPECT_DEATH(
      solution_str = solution.SerializeToString(RoutingOutputFormat::kCVRPLIB),
      "");
}

TEST(RoutingSolutionSerializerTest, SolutionToCvrplibDepot0Dimacs) {
  // Section 7 from
  // http://dimacs.rutgers.edu/files/6916/3848/0327/CVRP_Competition_Rules.pdf
  const std::vector<int64_t> solution{0, 1, 4, 0, -1, 0, 3, 2, 5, 0, -1};
  const std::string expected_output =
      "Route #1: 1 4[\r\n]+"
      "Route #2: 3 2 5[\r\n]+";

  EXPECT_THAT(RoutingSolution::FromSplitRoutes(
                  RoutingSolution::SplitRoutes(solution, -1), 0)
                  .SerializeToString(RoutingOutputFormat::kCVRPLIB),
              MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, SolutionToCvrplibDepot1Dimacs) {
  // Section 7 from
  // http://dimacs.rutgers.edu/files/6916/3848/0327/CVRP_Competition_Rules.pdf
  const std::vector<int64_t> solution{1, 2, 5, 1, -1, 1, 4, 3, 6, 1, -1};
  const std::string expected_output =
      "Route #1: 1 4[\r\n]+"
      "Route #2: 3 2 5[\r\n]+";

  EXPECT_THAT(RoutingSolution::FromSplitRoutes(
                  RoutingSolution::SplitRoutes(solution, -1), 1)
                  .SerializeToString(RoutingOutputFormat::kCVRPLIB),
              MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, SolutionToCvrplibFile) {
  const std::string file_name{std::tmpnam(nullptr)};
  RegisteredMutableMemFile registered(file_name);

  const std::vector<std::vector<int64_t>> solution_vector{{0, 1, 2, 3, 0},
                                                          {0, 4, 5, 6, 0}};
  const std::string expected_output =
      "Route #1: 1 2 3[\r\n]+"
      "Route #2: 4 5 6[\r\n]+"
      "Cost 4857";

  RoutingSolution solution =
      RoutingSolution::FromSplitRoutes(solution_vector, 0);
  solution.SetTotalCost(4857);
  solution.WriteToSolutionFile(RoutingOutputFormat::kCVRPLIB, file_name);
  std::string written_solution;
  CHECK_OK(file::GetContents(file_name, &written_solution, file::Defaults()));
  EXPECT_THAT(written_solution, MatchesRegex(expected_output));
}

RoutingSolution MakeTestArcRoutingInstance() {
  using Event = RoutingSolution::Event;
  using Type = Event::Type;
  return {std::vector<RoutingSolution::Route>{
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kServeArc, 12, Arc{4, 10}, "A1"},
                  Event{Type::kServeArc, 21, Arc{10, 8}, "A2"},
                  Event{Type::kServeArc, 8, Arc{8, 1}, "A3"},
                  Event{Type::kServeArc, 7, Arc{1, 3}, "A4"},
                  Event{Type::kServeArc, 2, Arc{3, 0}, "A5"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kServeArc, 5, Arc{0, 11}, "A6"},
                  Event{Type::kServeArc, 14, Arc{5, 6}, "A7"},
                  Event{Type::kServeArc, 19, Arc{7, 10}, "A8"},
                  Event{Type::kServeArc, 22, Arc{10, 9}, "A9"},
                  Event{Type::kServeArc, 4, Arc{9, 0}, "A10"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kServeArc, 13, Arc{11, 4}, "A11"},
                  Event{Type::kServeArc, 9, Arc{2, 3}, "A12"},
                  Event{Type::kServeArc, 6, Arc{1, 2}, "A13"},
                  Event{Type::kServeArc, 10, Arc{2, 4}, "A14"},
                  Event{Type::kServeArc, 11, Arc{4, 5}, "A15"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kServeArc, 15, Arc{11, 5}, "A16"},
                  Event{Type::kServeArc, 16, Arc{6, 7}, "A17"},
                  Event{Type::kServeArc, 18, Arc{7, 9}, "A18"},
                  Event{Type::kServeArc, 20, Arc{9, 8}, "A19"},
                  Event{Type::kServeArc, 1, Arc{1, 0}, "A20"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kServeArc, 17, Arc{11, 6}, "A21"},
                  Event{Type::kServeArc, 3, Arc{6, 0}, "A22"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
          },
          std::vector<int64_t>{5, 5, 5, 5, 2},
          std::vector<int64_t>{76, 60, 86, 53, 41},
          7,
          6,
          30.84};
}

RoutingSolution MakeTestEdgeNodeArcRoutingInstance() {
  using Event = RoutingSolution::Event;
  using Type = Event::Type;
  return {std::vector<RoutingSolution::Route>{
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kTransit, -1, Arc{0, 4}},
                  Event{Type::kServeEdge, 12, Arc{4, 10}, "E1"},
                  Event{Type::kServeArc, 21, Arc{10, 8}, "A2"},
                  Event{Type::kServeNode, 8, Arc{8, 8}},
                  Event{Type::kTransit, -1, Arc{8, 1}},
                  Event{Type::kServeEdge, 7, Arc{1, 3}, "E3"},
                  Event{Type::kServeArc, 2, Arc{3, 0}, "A4"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kServeEdge, 5, Arc{0, 11}, "E5"},
                  Event{Type::kTransit, -1, Arc{11, 5}},
                  Event{Type::kServeEdge, 14, Arc{5, 6}, "E6"},
                  Event{Type::kTransit, -1, Arc{6, 7}},
                  Event{Type::kServeEdge, 19, Arc{7, 10}, "E7"},
                  Event{Type::kServeEdge, 22, Arc{10, 9}, "E8"},
                  Event{Type::kServeEdge, 4, Arc{9, 0}, "E9"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kTransit, -1, Arc{0, 11}},
                  Event{Type::kServeArc, 13, Arc{11, 4}, "A10"},
                  Event{Type::kTransit, -1, Arc{4, 2}},
                  Event{Type::kServeEdge, 9, Arc{2, 3}, "E11"},
                  Event{Type::kTransit, -1, Arc{3, 1}},
                  Event{Type::kServeArc, 6, Arc{1, 2}, "A12"},
                  Event{Type::kServeNode, 10, Arc{2, 2}},
                  Event{Type::kTransit, -1, Arc{2, 4}},
                  Event{Type::kServeEdge, 11, Arc{4, 5}, "E13"},
                  Event{Type::kTransit, -1, Arc{5, 0}},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kTransit, -1, Arc{0, 11}},
                  Event{Type::kServeNode, 15, Arc{11, 11}},
                  Event{Type::kServeEdge, 16, Arc{11, 7}, "E14"},
                  Event{Type::kServeEdge, 18, Arc{7, 9}, "E15"},
                  Event{Type::kServeEdge, 20, Arc{9, 8}, "E16"},
                  Event{Type::kTransit, -1, Arc{8, 1}},
                  Event{Type::kServeEdge, 1, Arc{1, 0}, "E17"},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
              RoutingSolution::Route{
                  Event{Type::kStart, 0, Arc{0, 0}},
                  Event{Type::kTransit, -1, Arc{0, 11}},
                  Event{Type::kServeNode, 17, Arc{11, 11}},
                  Event{Type::kTransit, -1, Arc{11, 6}},
                  Event{Type::kServeNode, 3, Arc{6, 6}},
                  Event{Type::kTransit, -1, Arc{6, 0}},
                  Event{Type::kEnd, 0, Arc{0, 0}},
              },
          },
          std::vector<int64_t>{5, 5, 5, 5, 2},
          std::vector<int64_t>{76, 60, 86, 53, 41},
          7,
          6,
          30.84};
}

TEST(RoutingSolutionSerializerTest, CarpSolutionToCarplib) {
  // http://dimacs.rutgers.edu/programs/challenge/vrp/carp/
  const std::string expected_solution_output =
      "0 1 1 5 76 7 \\(D 0,1,1\\) \\(S 12,5,11\\) \\(S 21,11,9\\) "
      "\\(S 8,9,2\\) \\(S 7,2,4\\) \\(S 2,4,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 2 5 60 7 \\(D 0,1,1\\) \\(S 5,1,12\\) \\(S 14,6,7\\) "
      "\\(S 19,8,11\\) \\(S 22,11,10\\) \\(S 4,10,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 3 5 86 7 \\(D 0,1,1\\) \\(S 13,12,5\\) \\(S 9,3,4\\) "
      "\\(S 6,2,3\\) \\(S 10,3,5\\) \\(S 11,5,6\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 4 5 53 7 \\(D 0,1,1\\) \\(S 15,12,6\\) \\(S 16,7,8\\) "
      "\\(S 18,8,10\\) \\(S 20,10,9\\) \\(S 1,2,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 5 2 41 4 \\(D 0,1,1\\) \\(S 17,12,7\\) \\(S 3,7,1\\) \\(D 0,1,1\\)";

  const RoutingSolution solution = MakeTestArcRoutingInstance();
  EXPECT_THAT(solution.SerializeToString(RoutingOutputFormat::kCARPLIB),
              MatchesRegex(expected_solution_output));
}

TEST(RoutingSolutionSerializerTest, CarpSolutionToCarplibFile) {
  const std::string file_name{std::tmpnam(nullptr)};
  RegisteredMutableMemFile registered(file_name);

  RoutingSolution solution = MakeTestArcRoutingInstance();
  const std::string expected_output =
      "7[\r\n]+"
      "5[\r\n]+"
      "30.840000[\r\n]+"
      "0 1 1 5 76 7 \\(D 0,1,1\\) \\(S 12,5,11\\) \\(S 21,11,9\\) "
      "\\(S 8,9,2\\) \\(S 7,2,4\\) \\(S 2,4,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 2 5 60 7 \\(D 0,1,1\\) \\(S 5,1,12\\) \\(S 14,6,7\\) "
      "\\(S 19,8,11\\) \\(S 22,11,10\\) \\(S 4,10,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 3 5 86 7 \\(D 0,1,1\\) \\(S 13,12,5\\) \\(S 9,3,4\\) \\(S 6,2,3\\) "
      "\\(S 10,3,5\\) \\(S 11,5,6\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 4 5 53 7 \\(D 0,1,1\\) \\(S 15,12,6\\) \\(S 16,7,8\\) "
      "\\(S 18,8,10\\) \\(S 20,10,9\\) \\(S 1,2,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 5 2 41 4 \\(D 0,1,1\\) \\(S 17,12,7\\) \\(S 3,7,1\\) \\(D 0,1,1\\)";
  solution.SetName("Test name");
  solution.WriteToSolutionFile(RoutingOutputFormat::kCARPLIB, file_name);

  std::string written_solution;
  CHECK_OK(file::GetContents(file_name, &written_solution, file::Defaults()));
  EXPECT_THAT(written_solution, MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, NearpSolutionToCarplib) {
  const std::string expected_solution_output =
      "0 1 1 5 76 7 \\(D 0,1,1\\) \\(S 12,5,11\\) \\(S 21,11,9\\) "
      "\\(S 8,9,9\\) \\(S 7,2,4\\) \\(S 2,4,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 2 5 60 7 \\(D 0,1,1\\) \\(S 5,1,12\\) \\(S 14,6,7\\) "
      "\\(S 19,8,11\\) \\(S 22,11,10\\) \\(S 4,10,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 3 5 86 7 \\(D 0,1,1\\) \\(S 13,12,5\\) \\(S 9,3,4\\) \\(S 6,2,3\\) "
      "\\(S 10,3,3\\) \\(S 11,5,6\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 4 5 53 7 \\(D 0,1,1\\) \\(S 15,12,12\\) \\(S 16,12,8\\) "
      "\\(S 18,8,10\\) \\(S 20,10,9\\) \\(S 1,2,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 5 2 41 4 \\(D 0,1,1\\) \\(S 17,12,12\\) \\(S 3,7,7\\) \\(D 0,1,1\\)";

  const RoutingSolution solution = MakeTestEdgeNodeArcRoutingInstance();
  EXPECT_THAT(solution.SerializeToString(RoutingOutputFormat::kCARPLIB),
              MatchesRegex(expected_solution_output));
}

TEST(RoutingSolutionSerializerTest, NearpSolutionToCarplibFile) {
  const std::string file_name{std::tmpnam(nullptr)};
  RegisteredMutableMemFile registered(file_name);

  RoutingSolution solution = MakeTestEdgeNodeArcRoutingInstance();
  const std::string expected_output =
      "7[\r\n]+"
      "5[\r\n]+"
      "30.840000[\r\n]+"
      "0 1 1 5 76 7 \\(D 0,1,1\\) \\(S 12,5,11\\) \\(S 21,11,9\\) "
      "\\(S 8,9,9\\) \\(S 7,2,4\\) \\(S 2,4,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 2 5 60 7 \\(D 0,1,1\\) \\(S 5,1,12\\) \\(S 14,6,7\\) "
      "\\(S 19,8,11\\) \\(S 22,11,10\\) \\(S 4,10,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 3 5 86 7 \\(D 0,1,1\\) \\(S 13,12,5\\) \\(S 9,3,4\\) \\(S 6,2,3\\) "
      "\\(S 10,3,3\\) \\(S 11,5,6\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 4 5 53 7 \\(D 0,1,1\\) \\(S 15,12,12\\) \\(S 16,12,8\\) "
      "\\(S 18,8,10\\) \\(S 20,10,9\\) \\(S 1,2,1\\) \\(D 0,1,1\\)[\r\n]+"
      "0 1 5 2 41 4 \\(D 0,1,1\\) \\(S 17,12,12\\) \\(S 3,7,7\\) \\(D 0,1,1\\)";
  solution.SetName("Test name");
  solution.WriteToSolutionFile(RoutingOutputFormat::kCARPLIB, file_name);

  std::string written_solution;
  CHECK_OK(file::GetContents(file_name, &written_solution, file::Defaults()));
  EXPECT_THAT(written_solution, MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, CarpSolutionToNearplib) {
  const std::string expected_solution_output =
      "Route #1 : 1 5-A1-11-A2-9-A3-2-A4-4-A5-1[\r\n]+"
      "Route #2 : 1-A6-12 6-A7-7 8-A8-11-A9-10-A10-1[\r\n]+"
      "Route #3 : 1 12-A11-5 3-A12-4 2-A13-3-A14-5-A15-6 1[\r\n]+"
      "Route #4 : 1 12-A16-6 7-A17-8-A18-10-A19-9 2-A20-1[\r\n]+"
      "Route #5 : 1 12-A21-7-A22-1";

  const RoutingSolution solution = MakeTestArcRoutingInstance();
  EXPECT_THAT(solution.SerializeToString(RoutingOutputFormat::kNEARPLIB),
              MatchesRegex(expected_solution_output));
}

TEST(RoutingSolutionSerializerTest, CarpSolutionToNearplibFile) {
  const std::string file_name{std::tmpnam(nullptr)};
  RegisteredMutableMemFile registered(file_name);

  RoutingSolution solution = MakeTestArcRoutingInstance();
  const std::string date =
      absl::FormatTime("%B %d, %E4Y", absl::Now(), absl::LocalTimeZone());
  const std::string expected_output =
      "Instance name:   Test name[\r\n]+"
      "Authors:         DIMACS CARP[\r\n]+"
      "Date:            " +
      date +
      "[\r\n]+"
      "Reference:       OR-Tools[\r\n]+"
      "Solution[\r\n]+"
      "Route #1 : 1 5-A1-11-A2-9-A3-2-A4-4-A5-1[\r\n]+"
      "Route #2 : 1-A6-12 6-A7-7 8-A8-11-A9-10-A10-1[\r\n]+"
      "Route #3 : 1 12-A11-5 3-A12-4 2-A13-3-A14-5-A15-6 1[\r\n]+"
      "Route #4 : 1 12-A16-6 7-A17-8-A18-10-A19-9 2-A20-1[\r\n]+"
      "Route #5 : 1 12-A21-7-A22-1[\r\n]+"
      "Total cost:       7";
  solution.SetName("Test name");
  solution.SetAuthors("DIMACS CARP");
  solution.WriteToSolutionFile(RoutingOutputFormat::kNEARPLIB, file_name);

  std::string written_solution;
  CHECK_OK(file::GetContents(file_name, &written_solution, file::Defaults()));
  EXPECT_THAT(written_solution, MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, NearpSolutionToNearplib) {
  const std::string expected_solution_output =
      "Route #1 : 1 5-E1-11-A2-9 N9 2-E3-4-A4-1[\r\n]+"
      "Route #2 : 1-E5-12 6-E6-7 8-E7-11-E8-10-E9-1[\r\n]+"
      "Route #3 : 1 12-A10-5 3-E11-4 2-A12-3 N3 5-E13-6 1[\r\n]+"
      "Route #4 : 1 N12-E14-8-E15-10-E16-9 2-E17-1[\r\n]+"
      "Route #5 : 1 N12 N7 1";
  // TODO(user): the following output would be ideal (because shorter). It
  // would be achieved by implementing the relevant TODO in
  // SerializeToNEARPLIBString.
  // Route #1 : 1 5-E1-11-A2-N9 2-E3-4-A4-1
  // Route #3 : 1 12-A10-5 3-E11-4 2-A12-N3 5-E13-6 1

  const RoutingSolution solution = MakeTestEdgeNodeArcRoutingInstance();
  EXPECT_THAT(solution.SerializeToString(RoutingOutputFormat::kNEARPLIB),
              MatchesRegex(expected_solution_output));
}

TEST(RoutingSolutionSerializerTest, NearpSolutionToNearplibFile) {
  const std::string file_name{std::tmpnam(nullptr)};
  RegisteredMutableMemFile registered(file_name);

  RoutingSolution solution = MakeTestEdgeNodeArcRoutingInstance();
  const std::string date =
      absl::FormatTime("%B %d, %E4Y", absl::Now(), absl::LocalTimeZone());
  const std::string expected_output =
      "Instance name:   Test name[\r\n]+"
      "Authors:         Based on DIMACS CARP[\r\n]+"
      "Date:            " +
      date +
      "[\r\n]+"
      "Reference:       OR-Tools[\r\n]+"
      "Solution[\r\n]+"
      "Route #1 : 1 5-E1-11-A2-9 N9 2-E3-4-A4-1[\r\n]+"
      "Route #2 : 1-E5-12 6-E6-7 8-E7-11-E8-10-E9-1[\r\n]+"
      "Route #3 : 1 12-A10-5 3-E11-4 2-A12-3 N3 5-E13-6 1[\r\n]+"
      "Route #4 : 1 N12-E14-8-E15-10-E16-9 2-E17-1[\r\n]+"
      "Route #5 : 1 N12 N7 1[\r\n]+"
      "Total cost:       7";
  solution.SetName("Test name");
  solution.SetAuthors("Based on DIMACS CARP");
  solution.WriteToSolutionFile(RoutingOutputFormat::kNEARPLIB, file_name);

  std::string written_solution;
  CHECK_OK(file::GetContents(file_name, &written_solution, file::Defaults()));
  EXPECT_THAT(written_solution, MatchesRegex(expected_output));
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsTsplib) {
  EXPECT_EQ(FormatStatistic("STAT", 4, RoutingOutputFormat::kTSPLIB),
            "STAT = 4");
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsCvrplib) {
  EXPECT_EQ(FormatStatistic("STAT", 4, RoutingOutputFormat::kCVRPLIB),
            "STAT 4");
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsCarplib) {
  EXPECT_EQ(FormatStatistic("STAT", 4, RoutingOutputFormat::kCARPLIB), "4");
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsNearplib) {
  EXPECT_EQ(FormatStatistic("STAT", 4, RoutingOutputFormat::kNEARPLIB),
            "STAT : 4");
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsTsplibLongPrecision) {
  EXPECT_EQ(FormatStatistic("STAT", 591.556557, RoutingOutputFormat::kTSPLIB),
            "STAT = 591.556557");
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsCvrplibLongPrecision) {
  EXPECT_EQ(FormatStatistic("STAT", 591.556557, RoutingOutputFormat::kCVRPLIB),
            "STAT 591.556557");
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsCarplibLongPrecision) {
  EXPECT_EQ(FormatStatistic("STAT", 591.556557, RoutingOutputFormat::kCARPLIB),
            "591.556557");
}

TEST(RoutingSolutionSerializerTest, FormatStatisticAsNearplibLongPrecision) {
  EXPECT_EQ(FormatStatistic("STAT", 591.556557, RoutingOutputFormat::kNEARPLIB),
            "STAT : 591.556557");
}
}  // namespace
}  // namespace operations_research::routing
