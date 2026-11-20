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

#include "examples/cpp/cgc.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "examples/cpp/cgc_data.h"
#include "gtest/gtest.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/temp_file.h"

namespace operations_research {
namespace {

std::string CreateTempFile(absl::string_view content) {
  auto temp_file = file::MakeTempFilename(::testing::TempDir(), "cgc_test");
  CHECK_OK(temp_file.status());
  CHECK_OK(file::SetContents(*temp_file, content, file::Defaults()));
  return *temp_file;
}

class ConstrainedGuillotineCuttingTest : public ::testing::Test {
 protected:
  static const char kCorrectInput[];
  static const char kIncorrectInput[];
  static const char kCorrectSmallInput[];
};

const char ConstrainedGuillotineCuttingTest::kCorrectInput[] =
    "4\n"
    "214 25\n"
    "1 2 3 4\n"
    "5 6 7 8\n"
    "9 10 11 12\n"
    "13 14 15 16\n";
const char ConstrainedGuillotineCuttingTest::kIncorrectInput[] =
    "2\n"
    "214 25\n"
    "1 2 3 4\n";
const char ConstrainedGuillotineCuttingTest::kCorrectSmallInput[] =
    "2\n"
    "2 2\n"
    "1 1 3 4\n"
    "1 2 4 1\n";

TEST_F(ConstrainedGuillotineCuttingTest, LoadInputFiles) {
  const std::string correct_file = CreateTempFile(kCorrectInput);
  const std::string incorrect_file = CreateTempFile(kIncorrectInput);
  ConstrainedGuillotineCuttingData data;

  ASSERT_TRUE(data.LoadFromFile(correct_file));
  const std::vector<ConstrainedGuillotineCuttingData::Piece>& pieces =
      data.pieces();

  EXPECT_EQ(4, pieces.size());
  EXPECT_EQ(214, data.root_length());
  EXPECT_EQ(25, data.root_width());

  int piece_number = 0;
  for (const auto& piece : pieces) {
    EXPECT_EQ(4 * piece_number + 1, piece.length);
    EXPECT_EQ(4 * piece_number + 2, piece.width);
    EXPECT_EQ(4 * piece_number + 3, piece.max_appearances);
    EXPECT_EQ(4 * piece_number + 4, piece.value);
    piece_number++;
  }

  EXPECT_FALSE(data.LoadFromFile(incorrect_file));
}

TEST_F(ConstrainedGuillotineCuttingTest, Solved) {
  const std::string correct_small_file = CreateTempFile(kCorrectSmallInput);
  std::unique_ptr<ConstrainedGuillotineCuttingData> data(
      new ConstrainedGuillotineCuttingData());

  ASSERT_TRUE(data->LoadFromFile(correct_small_file));
  ConstrainedGuillotineCutting cgc(std::move(data));

  EXPECT_FALSE(cgc.Solved());
  cgc.Solve(absl::InfiniteDuration());

  EXPECT_TRUE(cgc.Solved());
  EXPECT_EQ(12, cgc.MaximumValue());
}

}  // namespace
}  // namespace operations_research
