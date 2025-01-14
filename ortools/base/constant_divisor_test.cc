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

#include "ortools/base/constant_divisor.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "absl/flags/flag.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"

ABSL_FLAG(int32_t, random_iterations, 100000,
          "Number of iterations for ConstantDivisorTest::RandomCases.");

namespace util {
namespace math {
namespace {

template <typename T>
class NativeDivisor {
 public:
  typedef T value_type;

  explicit NativeDivisor(T denominator) : denominator_(denominator) {}

  T div(T value) const { return value / denominator_; }

  T mod(T value) const { return value % denominator_; }

  friend value_type operator/(value_type a, const NativeDivisor& b) {
    return b.div(a);
  }

  friend value_type operator%(value_type a, const NativeDivisor& b) {
    return b.mod(a);
  }

 private:
  const T denominator_;
};

template <typename Divisor>
class ConstantDivisorTest : public ::testing::Test {};

// Currently there is no specialization for int, so it should default to the
// builtin version.
TEST(ConstantDivisorTemplateTest, Simple) {
  ConstantDivisor<int> divisor(3);
  EXPECT_EQ(4, divisor.div(12));
  EXPECT_EQ(1, divisor.mod(13));
  EXPECT_EQ(4, 12 / divisor);
  EXPECT_EQ(1, 13 % divisor);
}

TEST(ConstantDivisorUint64Test, Bugs) {
  // If forumula (27) from p231 is ever implemented, these divisors will break
  // if a >= is accidentally used instead of >.
  EXPECT_EQ(uint64_t{828560257293048160},
            ConstantDivisor<uint64_t>(21).div(uint64_t{17399765403154011380u}));
  EXPECT_EQ(uint64_t{185733693349184273},
            ConstantDivisor<uint64_t>(99).div(uint64_t{18387635641569243125u}));
}

TEST(ConstantDivisorUint16Test, Supports1) {
  ConstantDivisor<uint16_t> divisor(1);
  ASSERT_EQ(42, 42 / divisor);
  ASSERT_EQ(0, 42 % divisor);
}

TEST(ConstantDivisorUint8Test, Exhaustive) {
  // This is cheap, so test all values.
  for (int denominator = 1; denominator < 256; ++denominator) {
    ConstantDivisor<uint8_t> divisor(denominator);
    for (int value = 0; value < 256; ++value) {
      ASSERT_EQ(value / denominator, divisor.div(value))
          << "denominator: " << denominator << " value: " << value;
      ASSERT_EQ(value % denominator, divisor.mod(value))
          << "denominator: " << denominator << " value: " << value;
    }
  }
}

typedef ::testing::Types<ConstantDivisor<uint16_t>, ConstantDivisor<uint32_t>,
                         ConstantDivisor<uint64_t>, NativeDivisor<uint16_t>,
                         NativeDivisor<uint32_t>, NativeDivisor<uint64_t> >
    Divisors;
TYPED_TEST_SUITE(ConstantDivisorTest, Divisors);

TYPED_TEST(ConstantDivisorTest, Simple) {
  TypeParam divisor(3);
  EXPECT_EQ(4, divisor.div(12));
  EXPECT_EQ(1, divisor.mod(13));
  EXPECT_EQ(4, 12 / divisor);
  EXPECT_EQ(1, 13 % divisor);
}

TYPED_TEST(ConstantDivisorTest, CornerCases) {
  EXPECT_EQ(1, TypeParam(5).div(5));
  EXPECT_EQ(2, TypeParam(2).div(4));
  if constexpr (sizeof(typename TypeParam::value_type) >= sizeof(uint16_t)) {
    EXPECT_EQ(100, TypeParam(5).div(500));
  }
  const auto kTypeMax =
      std::numeric_limits<typename TypeParam::value_type>::max();
  if constexpr (sizeof(typename TypeParam::value_type) >= sizeof(uint16_t)) {
    EXPECT_EQ(kTypeMax / 345, TypeParam(345).div(kTypeMax));
  }
  EXPECT_EQ(1, TypeParam(kTypeMax).div(kTypeMax));
  EXPECT_EQ(1, TypeParam(kTypeMax - 1).div(kTypeMax));
  EXPECT_EQ(0, TypeParam(kTypeMax).div((kTypeMax - 1)));
}

TYPED_TEST(ConstantDivisorTest, Bugs) {
  if constexpr (sizeof(typename TypeParam::value_type) < sizeof(uint32_t)) {
    GTEST_SKIP() << "This test is only for 32-bit and above.";
  } else {
    // Cases that triggered bugs found during initial implementation.
    EXPECT_EQ(0, TypeParam(2969932030).div(265448460));
    EXPECT_EQ(2, TypeParam(978790915).div(2489284541));
    EXPECT_EQ(1, TypeParam(4113163180).div(4220126436));
    EXPECT_EQ(2072455839, TypeParam(2).div(4144911678));
  }
}

// Choose a random value of type T, biased towards smaller values.
template <typename T>
T ChooseValue(absl::BitGenRef gen) {
  return absl::Uniform<T>(gen, 0, std::numeric_limits<T>::max()) >>
         absl::Uniform<unsigned>(gen, 0, 8 * sizeof(T));
}

TYPED_TEST(ConstantDivisorTest, RandomCases) {
  typedef typename TypeParam::value_type T;
  absl::BitGen gen;
  for (int i = 0; i < absl::GetFlag(FLAGS_random_iterations); ++i) {
    T denominator = std::max<T>(2, ChooseValue<T>(gen));
    T value = ChooseValue<T>(gen);
    TypeParam divisor(denominator);
    ASSERT_EQ(value / denominator, divisor.div(value))
        << value << " / " << denominator;
    ASSERT_EQ(value % denominator, divisor.mod(value));
  }
}

// Gives a sense of benchmark overhead.
class NoopDivisor {
 public:
  typedef uint32_t value_type;

  explicit NoopDivisor(uint32_t) {}

  uint32_t div(uint32_t value) const { return value; }

  uint32_t mod(uint32_t value) const { return value; }
};

// Choose a random denominator which is supported by all our implementations,
// biased towards smaller denominators for uint64_t/uint32_t/uint16_t.
template <typename T>
T ChooseDenominator(absl::BitGenRef random) {
  return std::max(uint8_t{2}, ChooseValue<uint8_t>(random));
}

template <typename Divisor>
void BM_Divide(benchmark::State& state) {
  typedef typename Divisor::value_type T;
  absl::BitGen gen;
  std::vector<T> values;
  for (int i = 0; i < 100000; ++i) {
    values.push_back(ChooseValue<T>(gen));
  }

  for (auto _ : state) {
    state.PauseTiming();
    Divisor divisor(ChooseDenominator<T>(gen));
    state.ResumeTiming();
    for (T value : values) {
      benchmark::DoNotOptimize(divisor.div(value));
    }
  }
}
BENCHMARK_TEMPLATE(BM_Divide, NoopDivisor);
BENCHMARK_TEMPLATE(BM_Divide, NativeDivisor<uint8_t>);
BENCHMARK_TEMPLATE(BM_Divide, ConstantDivisor<uint8_t>);
BENCHMARK_TEMPLATE(BM_Divide, NativeDivisor<uint16_t>);
BENCHMARK_TEMPLATE(BM_Divide, ConstantDivisor<uint16_t>);
BENCHMARK_TEMPLATE(BM_Divide, NativeDivisor<uint32_t>);
BENCHMARK_TEMPLATE(BM_Divide, ConstantDivisor<uint32_t>);
BENCHMARK_TEMPLATE(BM_Divide, NativeDivisor<uint64_t>);
BENCHMARK_TEMPLATE(BM_Divide, ConstantDivisor<uint64_t>);

template <typename Divisor>
void BM_Modulo(benchmark::State& state) {
  typedef typename Divisor::value_type T;
  absl::BitGen gen;
  std::vector<T> values;
  for (int i = 0; i < 100000; ++i) {
    values.push_back(ChooseValue<T>(gen));
  }

  for (auto _ : state) {
    state.PauseTiming();
    Divisor divisor(ChooseDenominator<T>(gen));
    state.ResumeTiming();
    for (T value : values) {
      benchmark::DoNotOptimize(divisor.mod(value));
    }
  }
}
BENCHMARK_TEMPLATE(BM_Modulo, NoopDivisor);
BENCHMARK_TEMPLATE(BM_Modulo, NativeDivisor<uint8_t>);
BENCHMARK_TEMPLATE(BM_Modulo, ConstantDivisor<uint8_t>);
BENCHMARK_TEMPLATE(BM_Modulo, NativeDivisor<uint16_t>);
BENCHMARK_TEMPLATE(BM_Modulo, ConstantDivisor<uint16_t>);
BENCHMARK_TEMPLATE(BM_Modulo, NativeDivisor<uint32_t>);
BENCHMARK_TEMPLATE(BM_Modulo, ConstantDivisor<uint32_t>);
BENCHMARK_TEMPLATE(BM_Modulo, NativeDivisor<uint64_t>);
BENCHMARK_TEMPLATE(BM_Modulo, ConstantDivisor<uint64_t>);

template <typename Divisor>
void BM_ConstructDivisor(benchmark::State& state) {
  typedef typename Divisor::value_type T;
  absl::BitGen gen;
  std::vector<T> values;
  for (int i = 0; i < 2048; ++i) {
    values.push_back(ChooseDenominator<T>(gen));
  }

  int mask = values.size() - 1;
  int i = 0;
  for (auto _ : state) {
    Divisor divisor(values[i & mask]);
    benchmark::DoNotOptimize(divisor.div(values[(i + 1) & mask]));
    i++;
  }
}
BENCHMARK_TEMPLATE(BM_ConstructDivisor, NoopDivisor);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, NativeDivisor<uint8_t>);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, ConstantDivisor<uint8_t>);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, NativeDivisor<uint16_t>);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, ConstantDivisor<uint16_t>);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, NativeDivisor<uint32_t>);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, ConstantDivisor<uint32_t>);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, NativeDivisor<uint64_t>);
BENCHMARK_TEMPLATE(BM_ConstructDivisor, ConstantDivisor<uint64_t>);

}  // namespace
}  // namespace math
}  // namespace util
