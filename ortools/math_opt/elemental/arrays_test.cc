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

#include "ortools/math_opt/elemental/arrays.h"

#include <array>
#include <iterator>
#include <string>
#include <tuple>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/array.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;

// Sums the elements of an array-like object `a`.
template <auto a>
constexpr int Sum() {
  return ApplyOnIndexRange<std::size(a)>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      []<int... i>() { return (a[i] + ... + 0); });
}

// Same as `Sum`, but starts at 1.
template <auto a>
constexpr int SumPlusOne() {
  return ApplyOnIndexRange<std::size(a)>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      []<int... i>() { return (a[i] + ... + 1); });
}

#if __cplusplus >= 202002L
// NOLINTBEGIN(clang-diagnostic-pre-c++20-compat)
TEST(ApplyOnIndexRangeTest, Sum) {
  EXPECT_EQ(Sum<gtl::to_array({5, 3, 1})>(), 9);
  EXPECT_EQ(SumPlusOne<gtl::to_array({5, 3, 1})>(), 10);
}
// NOLINTEND(clang-diagnostic-pre-c++20-compat)
#endif

// Returns the weighted sum of the elements of an array-like object `a`, where
// weights are indices.
template <auto a>
constexpr double ScaledSum() {
  return ApplyOnIndexRange<std::size(a)>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      []<int... i>() { return ((i * a[i]) + ... + 0.0); });
}

#if __cplusplus >= 202002L
// NOLINTBEGIN(clang-diagnostic-pre-c++20-compat)
TEST(ApplyOnIndexRangeTest, ScaledSum) {
  EXPECT_EQ(ScaledSum<gtl::to_array({5, 3, 1})>(), 5.0);
}
// NOLINTEND(clang-diagnostic-pre-c++20-compat)
#endif

// Returns the number of even elements in an array-like object `a`.
template <auto a>
constexpr int CountEven() {
  return ApplyOnIndexRange<std::size(a)>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      []<int... i>() { return ((a[i] % 2 == 0 ? 1 : 0) + ... + 0); });
}

#if __cplusplus >= 202002L
// NOLINTBEGIN(clang-diagnostic-pre-c++20-compat)
TEST(ApplyOnIndexRangeTest, CountEven) {
  EXPECT_EQ(CountEven<gtl::to_array({5, 4, 8, 1, 10})>(), 3);
}
// NOLINTEND(clang-diagnostic-pre-c++20-compat)
#endif

// Returns array of doubles of the same size as `a`, where each element has been
// halved.
template <auto a>
constexpr std::array<double, std::size(a)> Half() {
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  return ApplyOnIndexRange<std::size(a)>([]<int... i>() {
    return std::array<double, std::size(a)>(
        {(static_cast<double>(a[i]) / 2.0)...});
  });
}

#if __cplusplus >= 202002L
// NOLINTBEGIN(clang-diagnostic-pre-c++20-compat)
TEST(ApplyOnIndexRangeTest, Half) {
  EXPECT_THAT(Half<gtl::to_array({5, 4, 8, 1, 10})>(),
              ElementsAre(2.5, 2.0, 4.0, 0.5, 5.0));
}
// NOLINTEND(clang-diagnostic-pre-c++20-compat)
#endif

// Returns true of all elements of `a` are even.
template <auto a>
constexpr int AllEven() {
  return ApplyOnIndexRange<std::size(a)>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      []<int... i>() { return (((a[i] % 2) == 0) && ...); });
}

// Returns true of any element of `a` is even.
template <auto a>
constexpr int AnyEven() {
  return ApplyOnIndexRange<std::size(a)>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      []<int... i>() { return (((a[i] % 2) == 0) || ...); });
}

#if __cplusplus >= 202002L
// NOLINTBEGIN(clang-diagnostic-pre-c++20-compat)
TEST(ApplyOnIndexRangeTest, Even) {
  EXPECT_FALSE(AllEven<gtl::to_array({5, 4, 8, 1, 10})>());
  EXPECT_TRUE(AnyEven<gtl::to_array({5, 4, 8, 1, 10})>());

  EXPECT_TRUE(AllEven<gtl::to_array({8, 2, 6})>());
  EXPECT_TRUE(AnyEven<gtl::to_array({8, 2, 6})>());

  EXPECT_FALSE(AllEven<gtl::to_array({3, 7, 1})>());
  EXPECT_FALSE(AnyEven<gtl::to_array({3, 7, 1})>());
}
// NOLINTEND(clang-diagnostic-pre-c++20-compat)
#endif

// A example of a more complex reduce operation using `ForEachIndex`. Here, we
// want to collect a list of integers for which an operation (`may_fail`)
// failed.
TEST(ForEachIndexTest, CollectTest) {
  constexpr auto may_fail = [](int i) {
    if (i == 3 || i == 7 || i == 42) {
      return absl::InvalidArgumentError("bad number");
    }
    return absl::OkStatus();
  };

  EXPECT_THAT(
      ForEachIndex<21>(
          // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
          [&may_fail, failed_indices = std::vector<int>()]<int i>() mutable
              -> const std::vector<int>& {
            if (!may_fail(i).ok()) {
              failed_indices.push_back(i);
            }
            return failed_indices;
          }),
      ElementsAre(3, 7));
}

TEST(ForEachTest, StrCatHeterogeneousTypes) {
  EXPECT_EQ(
      ForEach(
          [r = std::string()](const auto& v) mutable -> absl::string_view {
            absl::StrAppend(&r, " ", v);
            return r;
          },
          std::make_tuple("a", 1, 0.5)),
      " a 1 0.5");
}

}  // namespace
}  // namespace operations_research::math_opt
