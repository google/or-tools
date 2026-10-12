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

#include "ortools/util/fp_roundtrip_conv.h"

#include <stdint.h>

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "absl/base/casts.h"
#include "absl/base/no_destructor.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/types.h"
#include "ortools/util/random_subset.h"

namespace operations_research {
namespace {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::MatchesRegex;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST(RoundTripDoubleFormatTest, Stream) {
  // We test only one case here; other cases are tested by tests of
  // RoundTripDoubleFormat::ToString() since they both use the same algorithm.
  std::ostringstream oss;
  oss << RoundTripDoubleFormat(10.0);
  EXPECT_EQ(oss.str(), "10");
}

TEST(RoundTripDoubleFormatTest, AbslStringify) {
  // We test only one case here; other cases are tested by tests of
  // RoundTripDoubleFormat::ToString() since they both use the same algorithm.
  EXPECT_EQ(absl::StrCat(RoundTripDoubleFormat(10.0)), "10");
  EXPECT_EQ(absl::StrFormat("x = %v", RoundTripDoubleFormat(0.5)), "x = 0.5");
}

TEST(RoundTripDoubleFormatToStringTest, SomeValues) {
  using limits = std::numeric_limits<double>;

  EXPECT_EQ(RoundTripDoubleFormat::ToString(0.0), "0");
  EXPECT_EQ(RoundTripDoubleFormat::ToString(-0.0), "-0");
  // Some compilers do not implement std::to_chars() for non integer types; thus
  // we have round-trip but not shorted possible representation.
  if (kStdToCharsDoubleIsSupported) {
    EXPECT_EQ(RoundTripDoubleFormat::ToString(0.1), "0.1");
  } else {
    EXPECT_EQ(RoundTripDoubleFormat::ToString(0.1), "0.10000000000000001");
  }
  // std::to_chars() follows conventions from std::printf() which either prints
  // infinities as "inf" or "infinity". For NaN it uses "nan" or
  // "nan(something)".
  //
  // Refs:
  //  * https://en.cppreference.com/w/cpp/utility/to_chars
  //  * https://en.cppreference.com/w/cpp/io/c/fprintf
  EXPECT_THAT(RoundTripDoubleFormat::ToString(limits::infinity()),
              AnyOf("inf", "infinity"));
  EXPECT_THAT(RoundTripDoubleFormat::ToString(-limits::infinity()),
              AnyOf("-inf", "-infinity"));
  constexpr absl::string_view kDetailedNaNRegex = "nan\\(.*\\)";
  EXPECT_THAT(RoundTripDoubleFormat::ToString(limits::quiet_NaN()),
              AnyOf("nan", MatchesRegex(kDetailedNaNRegex)));
  EXPECT_THAT(RoundTripDoubleFormat::ToString(limits::signaling_NaN()),
              AnyOf("nan", MatchesRegex(kDetailedNaNRegex)));
}

absl::StatusOr<double> RoundTrip(const double value) {
  return RoundTripDoubleFormat::Parse(RoundTripDoubleFormat::ToString(value));
}

TEST(RoundTripDoubleFormatToStringTest, RoundTrip) {
  using limits = std::numeric_limits<double>;

  // Here we must print the value using special formatter since EXPECT_THAT will
  // round the printed value (printing "0.1" for std::nextafter(0.1, 1.0));
#define OR_TEST_ROUND_TRIP(val)                                        \
  do {                                                                 \
    const double input = (val);                                        \
    EXPECT_THAT(RoundTrip(input), IsOkAndHolds(input))                 \
        << std::setw(limits::max_digits10) << "val: " << #val << " = " \
        << input;                                                      \
  } while (false)

  OR_TEST_ROUND_TRIP(0.0);
  OR_TEST_ROUND_TRIP(-0.0);
  // For zeros we want to test the sign too.
  {
    ASSERT_OK_AND_ASSIGN(const double pos_zero, RoundTrip(0.0));
    EXPECT_FALSE(std::signbit(pos_zero));
  }
  {
    ASSERT_OK_AND_ASSIGN(const double neg_zero, RoundTrip(-0.0));
    EXPECT_TRUE(std::signbit(neg_zero));
  }
  OR_TEST_ROUND_TRIP(0.1);
  OR_TEST_ROUND_TRIP(std::nextafter(0.1, 1.0));
  OR_TEST_ROUND_TRIP(std::nextafter(0.1, 0.0));
  OR_TEST_ROUND_TRIP(std::nextafter(-0.1, 0.0));
  OR_TEST_ROUND_TRIP(std::nextafter(-0.1, -1.0));
  OR_TEST_ROUND_TRIP(limits::max());
  OR_TEST_ROUND_TRIP(-limits::max());
  OR_TEST_ROUND_TRIP(std::nextafter(limits::max(), 0.0));
  OR_TEST_ROUND_TRIP(std::nextafter(-limits::max(), 0.0));
  OR_TEST_ROUND_TRIP(limits::min());
  OR_TEST_ROUND_TRIP(-limits::min());
  OR_TEST_ROUND_TRIP(limits::denorm_min());
  OR_TEST_ROUND_TRIP(-limits::denorm_min());
  OR_TEST_ROUND_TRIP(std::nextafter(limits::denorm_min(), 1.0));
  OR_TEST_ROUND_TRIP(-std::nextafter(limits::denorm_min(), 1.0));
  OR_TEST_ROUND_TRIP(limits::infinity());
  OR_TEST_ROUND_TRIP(-limits::infinity());

#undef OR_TEST_ROUND_TRIP

  // We can't test NaN with OR_TEST_ROUND_TRIP since operator== returns false.
  // On top of that, we don't guarantee full round-trip for NaN values.
#define OR_TEST_NAN_ROUND_TRIP(val)                              \
  do {                                                           \
    const double input = (val);                                  \
    ASSERT_OK_AND_ASSIGN(const double result, RoundTrip(input)); \
    EXPECT_TRUE(std::isnan(result)) << result;                   \
  } while (false)

  OR_TEST_NAN_ROUND_TRIP(limits::quiet_NaN());
  OR_TEST_NAN_ROUND_TRIP(limits::signaling_NaN());
  OR_TEST_NAN_ROUND_TRIP(std::nan("1"));
  OR_TEST_NAN_ROUND_TRIP(std::nan("2"));

#undef OR_TEST_NAN_ROUND_TRIP
}

TEST(RoundTripDoubleFormatToStringTest, RandomRoundTrip) {
  using limits = std::numeric_limits<double>;
  const std::vector<double> special_values = {
      limits::infinity(),
      0,
      1,
      0.1,
      118274912198123.124132411,
      limits::denorm_min(),
      limits::min(),
      limits::max(),
      limits::epsilon(),
      limits::quiet_NaN(),
      limits::signaling_NaN(),
  };

  // We need this for RandomSubsetOf() below.
  std::vector<int> zero_to_63(64);
  std::iota(zero_to_63.begin(), zero_to_63.end(), 0);

  absl::BitGen random;

  // Generates a random bits to be used as the tested double value.
  const auto random_bits = [&]() {
    if (absl::Bernoulli(random, 0.5)) {
      // Fully random bits.
      return absl::Uniform<uint64_t>(absl::IntervalClosed, random, 0,
                                     kuint64max);
    }

    // Pick a special double value, choose a random sign and fuzz it.
    const double base = special_values.at(
        absl::Uniform<std::size_t>(random, 0, special_values.size()));
    uint64_t bits =
        absl::bit_cast<uint64_t>(absl::Bernoulli(random, 0.5) ? base : -base);
    const int num_fuzz_bits = absl::Uniform(random, 0, 10);
    for (const int bit : RandomSubsetOf(num_fuzz_bits, &zero_to_63, random)) {
      bits ^= uint64_t{1} << bit;
    }
    return bits;
  };

  const absl::Time end = absl::Now() + absl::Seconds(10);
  int rounds = 0;
  while (absl::Now() < end) {
    ++rounds;
    const double value = absl::bit_cast<double>(random_bits());
    SCOPED_TRACE(absl::StrFormat("value: %.*g", limits::max_digits10, value));
    ASSERT_OK_AND_ASSIGN(const double round_trip, RoundTrip(value));
    if (std::isnan(value)) {
      ASSERT_TRUE(std::isnan(round_trip))
          << std::setw(limits::max_digits10) << "round_trip: " << round_trip;
      continue;
    }
    // Here we use absl::bit_cast instead of == to differentiate -0 from +0.
    ASSERT_EQ(absl::bit_cast<uint64_t>(round_trip),
              absl::bit_cast<uint64_t>(value))
        << std::setw(limits::max_digits10) << " round_trip: " << round_trip;
  }
  LOG(INFO) << "tested " << rounds << " numbers";
}

TEST(RoundTripDoubleFormatParseTest, BadInputs) {
  EXPECT_THAT(
      RoundTripDoubleFormat::Parse(""),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("\"\""), HasSubstr("not a valid double"))));
  EXPECT_THAT(RoundTripDoubleFormat::Parse(R"("3.0")"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr(R"("\"3.0\"")"),
                             HasSubstr("not a valid double"))));
  EXPECT_THAT(
      RoundTripDoubleFormat::Parse(" 3.0"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr(R"(" 3.0")"), HasSubstr("not a valid double"))));
  EXPECT_THAT(RoundTripDoubleFormat::Parse("3.0\n"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("\"3.0\\n\""),
                             HasSubstr("suffix starting at index 3"))));
  EXPECT_THAT(RoundTripDoubleFormat::Parse("3.0 "),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr(R"("3.0 ")"),
                             HasSubstr("suffix starting at index 3"))));
  EXPECT_THAT(RoundTripDoubleFormat::Parse("13.1.2"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr(R"("13.1.2")"),
                             HasSubstr("suffix starting at index 4"))));
  EXPECT_THAT(
      RoundTripDoubleFormat::Parse("13e504"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr(R"("13e504")"), HasSubstr("does not fit"))));
}

// Functions with a signature compatible with RoundTripDoubleFormat::ToString()
// used to benchmark absl::SNPrintF() with BM_ToString().
std::string CallSNPrintF(const double value) {
  std::string buffer(/*count=*/32, /*ch=*/'\0');
  const int written =
      absl::SNPrintF(buffer.data(), buffer.size(), "%.*g",
                     std::numeric_limits<double>::max_digits10, value);
  CHECK_GT(written, 0);
  CHECK_LT(written, buffer.size());
  buffer.erase(written);
  return buffer;
}

// Returns integers around 0.
std::vector<double> SmallIntegers() {
  constexpr int kNumNumbers = 1000;
  std::vector<double> test_values;
  test_values.reserve(kNumNumbers);
  for (int i = 0; i < kNumNumbers; ++i) {
    test_values.push_back(i - kNumNumbers / 2);
  }
  return test_values;
}

// Returns integers around ±1e308.
std::vector<double> BigIntegers() {
  constexpr int kNumNumbers = 1000;
  std::vector<double> test_values;
  test_values.reserve(kNumNumbers);
  for (int i = 0; i < kNumNumbers; ++i) {
    const int signed_i = i - kNumNumbers / 2;
    const double magnitude =
        std::numeric_limits<double>::max() / std::pow(10.0, std::abs(signed_i));
    test_values.push_back(signed_i < 0 ? -magnitude : magnitude);
  }
  return test_values;
}

// Returns random numbers in [-1., 1.].
//
// The same numbers are returned each time this function is called.
std::vector<double> RandomDoublesAroundZero() {
  static const absl::NoDestructor<std::vector<double>> test_values([]() {
    constexpr int kNumNumbers = 1000;
    std::vector<double> test_values;
    test_values.reserve(kNumNumbers);
    absl::BitGen bitgen;
    for (int i = 0; i < kNumNumbers; ++i) {
      test_values.push_back(
          absl::Uniform(absl::IntervalClosedClosed, bitgen, -1.0, 1.0));
    }
    return test_values;
  }());
  return *test_values;
}

// Returns random numbers in all finite numbers.
//
// The same numbers are returned each time this function is called.
std::vector<double> RandomFiniteDoubles() {
  static const absl::NoDestructor<std::vector<double>> test_values([]() {
    constexpr int kNumNumbers = 1000;
    std::vector<double> test_values;
    test_values.reserve(kNumNumbers);
    absl::BitGen bitgen;
    for (int i = 0; i < kNumNumbers; ++i) {
      test_values.push_back(absl::Uniform(absl::IntervalClosedClosed, bitgen,
                                          std::numeric_limits<double>::lowest(),
                                          std::numeric_limits<double>::max()));
    }
    return test_values;
  }());
  return *test_values;
}

class ToStringTest : public testing::TestWithParam<std::string (*)(double)> {};

TEST_P(ToStringTest, SmallIntegers) {
  for (const double value : SmallIntegers()) {
    const std::string value_str = GetParam()(value);
    EXPECT_THAT(RoundTripDoubleFormat::Parse(value_str), IsOkAndHolds(value));
  }
}

TEST_P(ToStringTest, BigIntegers) {
  for (const double value : BigIntegers()) {
    const std::string value_str = GetParam()(value);
    EXPECT_THAT(RoundTripDoubleFormat::Parse(value_str), IsOkAndHolds(value));
  }
}

TEST_P(ToStringTest, RandomDoublesAroundZero) {
  for (const double value : RandomDoublesAroundZero()) {
    const std::string value_str = GetParam()(value);
    EXPECT_THAT(RoundTripDoubleFormat::Parse(value_str), IsOkAndHolds(value));
  }
}

TEST_P(ToStringTest, RandomFiniteDoubles) {
  for (const double value : RandomFiniteDoubles()) {
    const std::string value_str = GetParam()(value);
    EXPECT_THAT(RoundTripDoubleFormat::Parse(value_str), IsOkAndHolds(value));
  }
}

static std::string CallHighPrecision(double value) {
  return absl::StrCat(absl::HighPrecision(value));
}

INSTANTIATE_TEST_SUITE_P(
    All, ToStringTest,
    testing::Values(&RoundTripDoubleFormat::ToString,
                    static_cast<std::string (*)(double)>(&CallHighPrecision),
                    &CallSNPrintF));

}  // namespace
}  // namespace operations_research
