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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/base/constant_divisor.h"

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

// Choose a random value of type T, biased towards smaller values.
template <typename T>
T ChooseValue(absl::BitGenRef gen) {
  return absl::Uniform<T>(gen, 0, std::numeric_limits<T>::max()) >>
         absl::Uniform<unsigned>(gen, 0, 8 * sizeof(T));
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
      auto result = divisor.div(value);
      benchmark::DoNotOptimize(result);
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
      auto result = divisor.mod(value);
      benchmark::DoNotOptimize(result);
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
    auto result = divisor.div(values[(i + 1) & mask]);
    benchmark::DoNotOptimize(result);
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
