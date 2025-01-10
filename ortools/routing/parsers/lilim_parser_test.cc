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

#include "ortools/routing/parsers/lilim_parser.h"

#include <string>

#include "absl/flags/flag.h"
#include "gtest/gtest.h"
#include "ortools/base/path.h"

#define ROOT_DIR "_main/"

namespace operations_research::routing {
namespace {

void CheckData(const LiLimParser& parser) {
  EXPECT_EQ(parser.NumberOfNodes(), 1009);
  EXPECT_FALSE(parser.GetDelivery(0).has_value());
  EXPECT_FALSE(parser.GetPickup(0).has_value());
  EXPECT_TRUE(parser.GetDelivery(2).has_value());
  EXPECT_EQ(parser.GetDelivery(2).value(), 752);
  EXPECT_TRUE(parser.GetPickup(1).has_value());
  EXPECT_EQ(parser.GetPickup(1).value(), 582);
  EXPECT_EQ(parser.demands()[1], -10);
  EXPECT_EQ(parser.demands()[2], 10);
}

TEST(LiLimParserTest, LoadEmptyFileName) {
  std::string empty_file_name;
  LiLimParser parser;
  EXPECT_FALSE(parser.LoadFile(empty_file_name));
}

TEST(LiLimParserTest, LoadNonExistingFile) {
  LiLimParser parser;
  EXPECT_FALSE(parser.LoadFile("doesnotexist.txt"));
}

TEST(LiLimParserTest, LoadExistingFile) {
  LiLimParser parser;
  EXPECT_TRUE(parser.LoadFile(file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                             "ortools/routing/parsers"
                                             "/testdata/pdptw_LRC2_10_6.txt")));
  CheckData(parser);
  // Load a non-existing file to check the parser was cleaned.
  EXPECT_FALSE(parser.LoadFile("doesnotexist.txt"));
  EXPECT_EQ(parser.NumberOfNodes(), 0);
}

TEST(LiLimParserTest, LoadNonEmptyArchive) {
  std::string empty_archive_name;
  LiLimParser parser;
  EXPECT_FALSE(parser.LoadFile("pdptw_LRC2_10_6.txt", empty_archive_name));
}

TEST(LiLimParserTest, LoadNonExistingArchive) {
  LiLimParser parser;
  EXPECT_FALSE(parser.LoadFile("pdptw_LRC2_10_6.txt", ""));
}

TEST(LiLimParserTest, LoadNonExistingInstance) {
  LiLimParser parser;
  EXPECT_FALSE(parser.LoadFile("doesnotexist.txt",
                               file::JoinPath(::testing::SrcDir(), ROOT_DIR
                                              "ortools/routing/"
                                              "/parsers/testdata/lilim.zip")));
}

}  // namespace
}  // namespace operations_research::routing
