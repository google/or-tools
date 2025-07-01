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

#include "ortools/routing/parsers/solomon_parser.h"

#include <string>

#include "absl/flags/flag.h"
#include "gtest/gtest.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/gmock.h"
#include "ortools/base/path.h"

#define ROOT_DIR "_main/"

ABSL_FLAG(std::string, solomon_test_archive,
          "ortools/routing/benchmarks/solomon/"
          "testdata/solomon.zip",
          "Solomon: testing archive");
ABSL_FLAG(std::string, solomon_test_instance, "google2.txt",
          "Solomon: testing instance");

namespace operations_research::routing {
namespace {
TEST(SolomonParserTest, LoadEmptyFileName) {
  std::string empty_file_name;
  SolomonParser parser;
  EXPECT_FALSE(parser.LoadFile(empty_file_name));
}

TEST(SolomonParserTest, LoadNonExistingFile) {
  SolomonParser parser;
  EXPECT_FALSE(parser.LoadFile(""));
}

TEST(SolomonParserTest, LoadNonEmptyArchive) {
  std::string empty_archive_name;
  SolomonParser parser;
  EXPECT_FALSE(parser.LoadFile(absl::GetFlag(FLAGS_solomon_test_instance),
                               empty_archive_name));
}

TEST(SolomonParserTest, LoadNonExistingArchive) {
  SolomonParser parser;
  EXPECT_FALSE(parser.LoadFile(absl::GetFlag(FLAGS_solomon_test_instance), ""));
}

TEST(SolomonParserTest, LoadNonExistingInstance) {
  SolomonParser parser;
  EXPECT_FALSE(parser.LoadFile(
      "doesnotexist.txt",
      file::JoinPath(::testing::SrcDir(),
                     absl::GetFlag(FLAGS_solomon_test_archive))));
}

TEST(SolomonSolutionParserTest, LoadEmptyFileName) {
  std::string empty_file_name;
  SolomonSolutionParser parser;
  EXPECT_FALSE(parser.LoadFile(empty_file_name));
}

TEST(SolomonSolutionParserTest, LoadNonExistingFile) {
  SolomonSolutionParser parser;
  EXPECT_FALSE(parser.LoadFile(""));
}

TEST(SolomonSolutionParserTest, LoadFile) {
  SolomonSolutionParser parser;
  EXPECT_TRUE(parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                             "ortools/routing/parsers/testdata/"
                                             "c1_10_2-90-42222.96.txt")));
  EXPECT_EQ(parser.NumberOfRoutes(), 90);
  EXPECT_EQ(parser.GetValueFromKey("Instance Name"), "c1_10_2");
  EXPECT_EQ(
      parser.GetValueFromKey("Authors"),
      "Zhu He, Longfei Wang, Weibo Lin, Yujie Chen, Haoyuan Hu "
      "(haoyuan.huhy@cainiao.com), Yinghui Xu & VRP Team (Ying Zhang, Guotao "
      "Wu, Kunpeng Han et al.), unpublished result of CAINIAO AI.");
  EXPECT_EQ(parser.GetValueFromKey("Date"), "05-10-2018");
  EXPECT_EQ(parser.GetValueFromKey("Reference"),
            "\"New Algorithm for VRPTW\", unpublished result of CAINIAO AI.");
  EXPECT_EQ(parser.GetValueFromKey("NonExistingKey"), "");
  EXPECT_THAT(parser.route(0), ::testing::ElementsAre(1, 987, 466, 279, 31, 276,
                                                      263, 207, 646, 193, 3));
}

}  // namespace
}  // namespace operations_research::routing
