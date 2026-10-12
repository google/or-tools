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

#include <stdint.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "benchmark/benchmark.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research {
namespace {

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

// Benchmark of tested_fn() with numbers returned by data_fn().
template <std::vector<double> (*data_fn)(), std::string (*tested_fn)(double)>
void BM_ToString(benchmark::State& state) {
  const std::vector<double> test_values = data_fn();
  const std::size_t num_test_values = test_values.size();
  int64_t num_items_processed = 0;
  for (const auto s : state) {
    benchmark::DoNotOptimize(
        tested_fn(test_values[num_items_processed % num_test_values]));
    ++num_items_processed;
  }
  state.SetItemsProcessed(num_items_processed);
}

static std::string CallHighPrecision(double value) {
  return absl::StrCat(absl::HighPrecision(value));
}

BENCHMARK_TEMPLATE(BM_ToString, SmallIntegers, RoundTripDoubleFormat::ToString);
BENCHMARK_TEMPLATE(BM_ToString, SmallIntegers, CallHighPrecision);
BENCHMARK_TEMPLATE(BM_ToString, SmallIntegers, CallSNPrintF);
BENCHMARK_TEMPLATE(BM_ToString, BigIntegers, RoundTripDoubleFormat::ToString);
BENCHMARK_TEMPLATE(BM_ToString, BigIntegers, CallHighPrecision);
BENCHMARK_TEMPLATE(BM_ToString, BigIntegers, CallSNPrintF);
BENCHMARK_TEMPLATE(BM_ToString, RandomDoublesAroundZero,
                   RoundTripDoubleFormat::ToString);
BENCHMARK_TEMPLATE(BM_ToString, RandomDoublesAroundZero, CallHighPrecision);
BENCHMARK_TEMPLATE(BM_ToString, RandomDoublesAroundZero, CallSNPrintF);
BENCHMARK_TEMPLATE(BM_ToString, RandomFiniteDoubles,
                   RoundTripDoubleFormat::ToString);
BENCHMARK_TEMPLATE(BM_ToString, RandomFiniteDoubles, CallHighPrecision);
BENCHMARK_TEMPLATE(BM_ToString, RandomFiniteDoubles, CallSNPrintF);

}  // namespace
}  // namespace operations_research
