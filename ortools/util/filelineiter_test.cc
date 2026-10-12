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

#include "ortools/util/filelineiter.h"

#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/temp_file.h"

namespace operations_research {
namespace {

std::string CreateTempFile(absl::string_view content) {
  auto temp_file = file::MakeTempFilename(::testing::TempDir(), "filelineiter");
  CHECK_OK(temp_file.status());
  CHECK_OK(file::SetContents(*temp_file, content, file::Defaults()));
  return *temp_file;
}

TEST(FileLinesTest, DefaultOptions) {
  const std::string file_content = "line1\nline2\nline3\nline4\n";
  const std::string temp_file = CreateTempFile(file_content);

  int num_lines = 0;
  for (const std::string& line : FileLines(temp_file)) {
    ++num_lines;
    EXPECT_EQ(line, absl::StrFormat("line%d", num_lines));
  }
  EXPECT_EQ(num_lines, 4);
}

void TestWithOptions(absl::string_view file_content, int options,
                     const absl::FormatSpec<int>& expected_output_template) {
  const std::string temp_file = CreateTempFile(file_content);

  int num_lines = 0;
  for (const std::string& line : FileLines(temp_file, options)) {
    ++num_lines;
    EXPECT_EQ(line, absl::StrFormat(expected_output_template, num_lines));
  }
  EXPECT_EQ(num_lines, 4);
}

TEST(FileLinesTest, KeepLinefeed) {
  TestWithOptions("line1\nline2\nline3\nline4\n",
                  FileLineIterator::KEEP_LINEFEED, "line%d\n");
}

TEST(FileLinesTest, RemoveLinefeed) {
  TestWithOptions("line1\nline2\nline3\nline4\n", FileLineIterator::DEFAULT,
                  "line%d");
}

TEST(FileLinesTest, RemoveInlineCRRemoveLineFeed) {
  TestWithOptions("lin\re1\nline2\nline3\r\nline4\r\n",
                  FileLineIterator::REMOVE_INLINE_CR, "line%d");
}

TEST(FileLinesTest, RemoveInlineCRKeepLinefeed) {
  TestWithOptions(
      "lin\re1\nline2\nline3\r\nline4\r\n",
      FileLineIterator::REMOVE_INLINE_CR | FileLineIterator::KEEP_LINEFEED,
      "line%d\n");
}

TEST(FileLinesTest, RemoveBlankLines) {
  TestWithOptions("line1\n\nline2\nline3\nline4\n\n",
                  FileLineIterator::REMOVE_BLANK_LINES, "line%d");
}

TEST(FileLinesTest, RemoveInlineCRRemoveBlankLines) {
  TestWithOptions(
      "line1\n\r\nline2\nline3\nline4\n\r\n",
      FileLineIterator::REMOVE_INLINE_CR | FileLineIterator::REMOVE_BLANK_LINES,
      "line%d");
}

TEST(FileLinesTest, KeepBlankLines) {
  const std::string file_content = "line1\n\nline2\nline3\nline4\n\n";
  const char* expected_output[] = {"line1", "", "line2", "line3", "line4", ""};
  const std::string temp_file = CreateTempFile(file_content);

  int num_lines = 0;
  for (const std::string& line : FileLines(temp_file)) {
    EXPECT_EQ(line, expected_output[num_lines]);
    ++num_lines;
  }
  EXPECT_EQ(num_lines, 6);
}

TEST(FileLinesTest, LargeFile) {
  std::string file_content;
  for (int i = 0; i < 100000; ++i) {
    file_content.append(absl::StrFormat("This is line number %d.\n", i));
  }
  const std::string temp_file = CreateTempFile(file_content);
  int num_lines = 0;
  for (const std::string& line : FileLines(temp_file)) {
    EXPECT_EQ(line, absl::StrFormat("This is line number %d.", num_lines));
    ++num_lines;
  }
  EXPECT_EQ(num_lines, 100000);
}

TEST(FileLinesTest, LargeAndUniqueLineWithoughEol) {
  const std::string file_content(1000000, 'a');
  const std::string temp_file = CreateTempFile(file_content);
  int num_lines = 0;
  for (const std::string& line : FileLines(temp_file)) {
    EXPECT_EQ(line, file_content);
    ++num_lines;
  }
  EXPECT_EQ(num_lines, 1);
}

TEST(FileLinesTest, LargeLinesAndEmptyLines) {
  std::string file_content(1000000, 'a');
  file_content[5 * 1024 - 1] = '\n';
  file_content[5 * 1024] = '\n';
  file_content[(5 + 15) * 1024] = '\n';
  file_content[(5 + 15) * 1024 + 123456] = '\n';
  const std::string temp_file = CreateTempFile(file_content);
  std::vector<int> sizes;
  for (const std::string& line : FileLines(temp_file)) {
    sizes.push_back(line.size());
  }
  EXPECT_EQ(sizes.size(), 5);
  EXPECT_EQ(sizes[0], 5 * 1024 - 1);
  EXPECT_EQ(sizes[1], 0);
  EXPECT_EQ(sizes[2], 15 * 1024 - 1);
  EXPECT_EQ(sizes[3], 123456 - 1);
  EXPECT_EQ(sizes[4], 1000000 - (5 + 15) * 1024 - 123456 - 1);
}

TEST(FileLinesTest, NonExistentFile) {
  int num_lines = 0;
  for (const __attribute__((unused)) std::string& line :
       FileLines("inexistent_filename")) {
    ++num_lines;
  }
  EXPECT_EQ(num_lines, 0);
}

}  // namespace
}  // namespace operations_research
