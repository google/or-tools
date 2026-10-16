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

#include "ortools/base/accurate_sum.h"

#include <cmath>
#include <cstdint>

#include "gtest/gtest.h"

namespace operations_research {
namespace {

// A 6-significant-digit decimal floating-point type matching the worked example
// in https://en.wikipedia.org/wiki/Kahan_summation_algorithm#Worked_example.
class Decimal6Float {
 public:
  Decimal6Float() : value_(0.0) {}
  explicit Decimal6Float(double value) : value_(RoundTo6SigFigs(value)) {}

  double value() const { return value_; }

  Decimal6Float operator+(const Decimal6Float& other) const {
    return Decimal6Float(value_ + other.value_);
  }

  Decimal6Float operator-(const Decimal6Float& other) const {
    return Decimal6Float(value_ - other.value_);
  }

  Decimal6Float& operator+=(const Decimal6Float& other) {
    *this = *this + other;
    return *this;
  }

 private:
  static double RoundTo6SigFigs(double x) {
    if (x == 0.0) return 0.0;
    const double scale =
        std::pow(10.0, 5 - std::floor(std::log10(std::abs(x))));
    return std::round(x * scale) / scale;
  }

  double value_;
};

TEST(AccurateSumTest, DefaultValueIsZero) {
  AccurateSum<double> sum;
  EXPECT_EQ(sum.Value(), 0.0);
}

TEST(AccurateSumTest, WikipediaWorkedExample) {
  const Decimal6Float a(10000.0);
  const Decimal6Float b(3.14159);
  const Decimal6Float c(2.71828);

  // Naive 6-digit summation rounds 10000.0 + 3.14159 to 10003.1 and then
  // 10003.1 + 2.71828 to 10005.8 instead of the true rounded sum 10005.9.
  const Decimal6Float naive_sum = (a + b) + c;
  EXPECT_NEAR(naive_sum.value(), 10005.8, 1e-9);

  AccurateSum<Decimal6Float> kahan_sum;
  kahan_sum.Add(a);
  kahan_sum.Add(b);
  kahan_sum.Add(c);
  EXPECT_NEAR(kahan_sum.Value().value(), 10005.9, 1e-9);
}

TEST(AccurateSumTest, SubUlpAdditionsCarryIntoSum) {
  // In IEEE-754 double precision, 1.0 + 1e-16 == 1.0 (since ulp(1.0) = 2^-52
  // ~= 2.22e-16). Adding 1e-16 ten times in naive arithmetic leaves the sum at
  // 1.0, whereas Kahan summation accumulates the sub-ulp increments in the
  // compensation term and carries them into the high-order sum.
  constexpr double kSubUlp = 1e-16;
  ASSERT_EQ(1.0 + kSubUlp, 1.0);

  double naive_sum = 1.0;
  AccurateSum<double> kahan_sum;
  kahan_sum.Add(1.0);
  for (int i = 0; i < 10; ++i) {
    naive_sum += kSubUlp;
    kahan_sum.Add(kSubUlp);
  }
  EXPECT_EQ(naive_sum, 1.0);
  EXPECT_GT(kahan_sum.Value(), 1.0);
  EXPECT_EQ(kahan_sum.Value(), 1.0 + 1e-15);
}

TEST(AccurateSumTest, SmallIncrementsAreAccumulated) {
  constexpr double kEpsilon = 1e-16;
  ASSERT_EQ(1.0 + kEpsilon, 1.0);

  constexpr int kNumAdds = 10'000'000;
  double naive_sum = 1.0;
  AccurateSum<double> kahan_sum;
  kahan_sum.Add(1.0);
  for (int i = 0; i < kNumAdds; ++i) {
    naive_sum += kEpsilon;
    kahan_sum.Add(kEpsilon);
  }
  EXPECT_EQ(naive_sum, 1.0);
  EXPECT_EQ(kahan_sum.Value(), 1.0 + 1e-9);
}

TEST(AccurateSumTest, SumOfSquares) {
  for (const bool increasing : {true, false}) {
    AccurateSum<double> kahan_sum;
    double naive_sum = 0.0;
    constexpr int64_t kN = 1'000'000;
    for (int64_t i = 0; i <= kN; ++i) {
      const int64_t v = increasing ? i : kN - i;
      const double sq = static_cast<double>(v * v);
      kahan_sum.Add(sq);
      naive_sum += sq;
    }
    const double expected_sum =
        static_cast<double>((kN * (kN + 1) * (2 * kN + 1)) / 6);
    EXPECT_EQ(kahan_sum.Value(), expected_sum);
    EXPECT_NE(naive_sum, expected_sum);
  }
}

TEST(AccurateSumTest, CopyPreservesCompensation) {
  constexpr double kSubHalfUlp = 1e-16;
  ASSERT_EQ(1.0 + kSubHalfUlp, 1.0);

  AccurateSum<double> sum;
  sum.Add(1.0);
  sum.Add(kSubHalfUlp);
  // After one sub-half-ulp addition, Value() is still 1.0 and kSubHalfUlp is
  // held only in error_sum_.
  EXPECT_EQ(sum.Value(), 1.0);

  AccurateSum<double> copy = sum;
  copy.Add(kSubHalfUlp);
  // Because error_sum_ was preserved in `copy`, the second kSubHalfUlp pushes
  // the total compensation to 2e-16 > 0.5 * ulp(1.0), rounding Value() up.
  EXPECT_GT(copy.Value(), 1.0);
}

}  // namespace
}  // namespace operations_research
