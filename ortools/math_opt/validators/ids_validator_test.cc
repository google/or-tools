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

#include "ortools/math_opt/validators/ids_validator.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/types.h"
#include "ortools/math_opt/core/model_summary.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::absl::StatusCode;
using ::testing::AllOf;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

TEST(CheckIdsRangeAndStrictlyIncreasingTests,
     IdsNonnegativeAndStrictlyIncreasing) {
  const std::vector<int64_t> ids = {0, 3, 5};
  EXPECT_OK(CheckIdsRangeAndStrictlyIncreasing(ids));
}

TEST(CheckIdsRangeAndStrictlyIncreasingTests, NegativeId) {
  const std::vector<int64_t> ids = {-3, 0, 5};
  EXPECT_THAT(CheckIdsRangeAndStrictlyIncreasing(ids),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("nonnegative"),
                             HasSubstr("at index 0 found id: -3"))));
}

TEST(CheckIdsRangeAndStrictlyIncreasingTests, MaxInt64Id) {
  const std::vector<int64_t> ids = {0, 5, kint64max};
  EXPECT_THAT(
      CheckIdsRangeAndStrictlyIncreasing(ids),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr("max(int64_t)"),
                HasSubstr(absl::StrCat("at index 2 found id: ", kint64max)))));
}

TEST(CheckIdsRangeAndStrictlyIncreasingTests, NonincreasingIds) {
  const std::vector<int64_t> ids = {0, 5, 3};
  EXPECT_THAT(
      CheckIdsRangeAndStrictlyIncreasing(ids),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Expected ids to be strictly increasing"),
                     HasSubstr("at index 1"), HasSubstr("at index 2"))));
}

TEST(CheckIdsSubsetTests, IdsAreSubset) {
  const std::vector<int64_t> ids = {0, 4, 5};
  const IdNameBiMap universe = {{0, ""}, {4, ""}, {5, ""}, {6, ""}};
  EXPECT_OK(CheckIdsSubset(ids, universe));
}

TEST(CheckIdsSubsetTests, IdsAreSubsetWithUpperBound) {
  const std::vector<int64_t> ids = {0, 4, 5};
  const IdNameBiMap universe = {{0, ""}, {4, ""}, {5, ""}, {8, ""}};
  EXPECT_OK(CheckIdsSubset(ids, universe, 6));
}

TEST(CheckIdsSubsetTests, ExceedsUpperBound) {
  const std::vector<int64_t> ids = {0, 4, 8};
  const IdNameBiMap universe = {{0, ""}, {4, ""}, {5, ""}, {8, ""}};
  EXPECT_THAT(CheckIdsSubset(ids, universe, 6),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper")));
}

TEST(CheckIdsSubsetTests, NotInUniverse) {
  const std::vector<int64_t> ids = {0, 4, 7};
  const IdNameBiMap universe = {{0, ""}, {4, ""}, {5, ""}, {8, ""}};
  EXPECT_THAT(
      CheckIdsSubset(ids, universe),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("not found")));
}

TEST(CheckIdsSubsetTests, IdsAreSubsetWithLabels) {
  const std::vector<int64_t> ids = {0, 4, 5};
  const IdNameBiMap universe = {{0, ""}, {4, ""}, {5, ""}, {6, ""}};
  EXPECT_OK(
      CheckIdsSubset(ids, universe, "ids_description", "universe_description"));
}

TEST(CheckIdsSubsetTests, MissingId) {
  const std::vector<int64_t> ids = {0, 4, 3};
  const IdNameBiMap universe = {{0, ""}, {4, ""}, {5, ""}, {6, ""}};
  EXPECT_THAT(
      CheckIdsSubset(ids, universe, "ids_description", "universe_description"),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr("Id:"),
                HasSubstr(
                    "ids_description is missing from universe_description"))));
}

TEST(CheckIdsIdenticalTests, IdsAreIdentical) {
  const std::vector<int64_t> first_ids = {0, 4, 5};
  const IdNameBiMap second_ids = {{0, ""}, {4, ""}, {5, ""}};
  EXPECT_OK(CheckIdsIdentical(first_ids, second_ids, "first_description",
                              "second_description"));
}

TEST(CheckIdsIdenticalTests, IdsHaveDifferentLength) {
  const std::vector<int64_t> first_ids = {0, 4, 5, 6};
  const IdNameBiMap second_ids = {{0, ""}, {4, ""}, {5, ""}};
  EXPECT_THAT(CheckIdsIdentical(first_ids, second_ids, "first_description",
                                "second_description"),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("first_description has size"),
                             HasSubstr("but second_description has size"))));
}

TEST(CheckIdsIdenticalTests, MissingId) {
  const std::vector<int64_t> first_ids = {0, 4, 3};
  const IdNameBiMap second_ids = {{0, ""}, {4, ""}, {5, ""}};
  EXPECT_THAT(
      CheckIdsIdentical(first_ids, second_ids, "first_description",
                        "second_description"),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr("Id:"),
                HasSubstr(
                    "first_description is missing from second_description"))));
}

TEST(CheckIdsSubsetHashSetTests, IdsAreSubset) {
  const std::vector<int64_t> ids = {5, 0, 4, 5};
  const absl::flat_hash_set<int64_t> universe = {0, 4, 5, 6};
  EXPECT_OK(CheckIdsSubset(ids, universe, "ids", "universe"));
}

TEST(CheckIdsSubsetHashSetTests, NotInUniverse) {
  const std::vector<int64_t> ids = {0, 4, 7};
  const absl::flat_hash_set<int64_t> universe = {0, 4, 5, 8};
  EXPECT_THAT(
      CheckIdsSubset(ids, universe, "my_ids", "my_universe"),
      StatusIs(StatusCode::kInvalidArgument, ContainsRegex("my_ids.*missing")));
}

TEST(CheckSortedIdsSubsetTests, IdsAreSubset) {
  const std::vector<int64_t> ids = {0, 4, 4, 6};
  const std::vector<int64_t> universe = {0, 4, 5, 6};
  EXPECT_OK(CheckSortedIdsSubset(ids, universe, "ids", "universe"));
}

TEST(CheckSortedIdsSubsetTests, EmptyIds) {
  EXPECT_OK(CheckSortedIdsSubset({}, {}, "ids", "universe"));
  EXPECT_OK(CheckSortedIdsSubset({}, {1, 2}, "ids", "universe"));
}

TEST(CheckSortedIdsSubsetTests, NotInUniverse) {
  const std::vector<int64_t> ids = {0, 4, 7};
  const std::vector<int64_t> universe = {0, 4, 5, 8};
  EXPECT_THAT(
      CheckSortedIdsSubset(ids, universe, "my_ids", "my_universe"),
      StatusIs(StatusCode::kInvalidArgument, ContainsRegex("my_ids.*missing")));
}

TEST(CheckSortedIdsSubsetTests, PastEndOfUniverse) {
  const std::vector<int64_t> ids = {0, 9};
  const std::vector<int64_t> universe = {0, 4, 5, 8};
  EXPECT_THAT(
      CheckSortedIdsSubset(ids, universe, "my_ids", "my_universe"),
      StatusIs(StatusCode::kInvalidArgument, ContainsRegex("9.*missing")));
}

TEST(CheckSortedIdsSubsetTests, EmptyUniverse) {
  const std::vector<int64_t> ids = {0};
  EXPECT_THAT(CheckSortedIdsSubset(ids, {}, "my_ids", "my_universe"),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("missing")));
}

TEST(CheckSortedIdsSubsetTests, IdsNotSorted) {
  const std::vector<int64_t> ids = {4, 0};
  const std::vector<int64_t> universe = {0, 4, 5, 8};
  EXPECT_THAT(
      CheckSortedIdsSubset(ids, universe, "my_ids", "my_universe"),
      StatusIs(StatusCode::kInvalidArgument, ContainsRegex("my_ids.*sorted")));
}

TEST(CheckSortedIdsSubsetTests, UniverseNotSorted) {
  const std::vector<int64_t> ids = {0, 4};
  const std::vector<int64_t> universe = {0, 5, 4, 8};
  EXPECT_THAT(CheckSortedIdsSubset(ids, universe, "my_ids", "my_universe"),
              StatusIs(StatusCode::kInvalidArgument,
                       ContainsRegex("my_universe.*strictly increasing")));
}

TEST(CheckSortedIdsSubsetTests, UniverseNotSortedAfterLastId) {
  const std::vector<int64_t> ids = {0};
  const std::vector<int64_t> universe = {0, 5, 4};
  EXPECT_THAT(CheckSortedIdsSubset(ids, universe, "my_ids", "my_universe"),
              StatusIs(StatusCode::kInvalidArgument,
                       ContainsRegex("my_universe.*strictly increasing")));
}

TEST(CheckSortedIdsSubsetTests, UniverseHasDuplicates) {
  const std::vector<int64_t> ids = {4};
  const std::vector<int64_t> universe = {0, 4, 4};
  EXPECT_THAT(CheckSortedIdsSubset(ids, universe, "my_ids", "my_universe"),
              StatusIs(StatusCode::kInvalidArgument,
                       ContainsRegex("my_universe.*strictly increasing")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
