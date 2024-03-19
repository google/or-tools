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

#include "ortools/routing/parsers/solomon_parser.h"

#include <string>

#include "absl/flags/flag.h"
#include "gtest/gtest.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/path.h"

ABSL_FLAG(std::string, test_srcdir, "", "REQUIRED: src dir");

ABSL_FLAG(std::string, solomon_test_archive,
          "ortools/bench/solomon/"
          "testdata/solomon.zip",
          "Solomon: testing archive");
ABSL_FLAG(std::string, solomon_test_instance, "google2.txt",
          "Solomon: testing instance");

namespace operations_research {
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
      file::JoinPath(absl::GetFlag(FLAGS_test_srcdir),
                     absl::GetFlag(FLAGS_solomon_test_archive))));
}
}  // namespace
}  // namespace operations_research
