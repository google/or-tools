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

// Benchmark Results (`-c opt`, AMD Rome 64-core) comparing:
// - `Std`: `BisectMonotonic` with `std::midpoint`
// - `Ord`: `BisectMonotonic` with `OrdinalMidpoint`
// - `Direct`: `OrdinalBisectMonotonic` (transforms bounds once and applies only
//   `OrdinalToFloat` per step)
//
// Problem (Bracket)   | Type   | Std (ns) | Ord (ns) | Direct | vs Ord / Std
// --------------------+--------+----------+----------+--------+--------------
// exp(x)-1e38 [0,max] | float  |    1,971 |      583 |    210 | 2.78x / 9.39x
// exp(x)-1e38 [0,max] | double |   15,265 |    1,674 |    670 | 2.50x / 22.8x
// exp(x)-1    [0,max] | float  |    2,830 |      342 |    123 | 2.78x / 23.0x
// exp(x)-1    [0,max] | double |   33,372 |      878 |    351 | 2.50x / 95.1x
// x * x - 2   [1, 2]  | float  |      111 |      212 |   35.7 | 5.94x / 3.11x
// x * x - 2   [1, 2]  | double |      237 |      422 |   65.2 | 6.47x / 3.63x
//
// Why `OrdinalBisectMonotonic` is faster even within a single binade
// (`[1,2]`):
// 1. No per-step forward transforms or XMM <-> GPR round-trips: in
//    `BisectMonotonic` with `OrdinalMidpoint`, `(low, high)` are stored as
//    floats, forcing each step to move `low` and `high` from floating-point
//    registers to integer registers (`FloatToOrdinal`), compute the midpoint,
//    and move the result back (`OrdinalToFloat`). `OrdinalBisectMonotonic`
//    keeps `(low_ordinal, high_ordinal)` in integer registers for the entire
//    loop and performs only a single `OrdinalToFloat(mid_ordinal)` per step.
// 2. Faster than `std::midpoint` on `[1, 2]` (`3.1x`-`3.6x`): even when both
//    methods take the exact same `M` steps (`23` for `float`, `52` for
//    `double`), `std::midpoint(float, float)` executes magnitude range checks
//    and branches at every step to guard against overflow and underflow,
//    whereas `std::midpoint(low_ordinal, high_ordinal)` compiles to a
//    branchless integer subtraction, right-shift, and addition.

#include <cmath>
#include <limits>
#include <numeric>

#include "benchmark/benchmark.h"
#include "ortools/algorithms/float_ordinal_bijection.h"

namespace operations_research {
namespace {

// Holds the final bracketing interval `[low, high]` and the number of steps
// taken by `BisectMonotonic`.
template <typename Float>
struct BisectionResult {
  Float low;
  Float high;
  int num_steps;
};

// Bisects a monotonically increasing function `fn` on `[low, high]` using
// `midpoint_fn` until `mid == low` or `mid == high`, counting each step.
template <typename Float, typename Function, typename MidpointFn>
BisectionResult<Float> BisectMonotonic(Float low, Float high, Function fn,
                                       MidpointFn midpoint_fn) {
  int num_steps = 0;
  while (low < high) {
    const Float mid = midpoint_fn(low, high);
    if (mid == low || mid == high) {
      break;
    }
    ++num_steps;
    if (fn(mid) <= Float{0}) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return {.low = low, .high = high, .num_steps = num_steps};
}

// Bisects a monotonically increasing function `fn` on `[low, high]` by
// transforming `low` and `high` to ordinals once and applying only the inverse
// transform `OrdinalToFloat` to evaluate `fn` at each step.
template <typename Float, typename Function>
BisectionResult<Float> OrdinalBisectMonotonic(const Float low, const Float high,
                                              Function fn) {
  using Ordinal = Ordinal<Float>;
  Ordinal low_ordinal = FloatToOrdinal(low);
  Ordinal high_ordinal = FloatToOrdinal(high);
  int num_steps = 0;
  while (low_ordinal < high_ordinal) {
    const Ordinal mid_ordinal = std::midpoint(low_ordinal, high_ordinal);
    if (mid_ordinal == low_ordinal || mid_ordinal == high_ordinal) {
      break;
    }
    ++num_steps;
    if (fn(OrdinalToFloat(mid_ordinal)) <= Float{0}) {
      low_ordinal = mid_ordinal;
    } else {
      high_ordinal = mid_ordinal;
    }
  }
  return {.low = OrdinalToFloat(low_ordinal),
          .high = OrdinalToFloat(high_ordinal),
          .num_steps = num_steps};
}

// Evaluates `exp(x) - kOffset`, using `std::expm1(x)` when `kOffset == 1.0` to
// avoid catastrophic cancellation when `|x| <= 2^-digits` and preserve sign
// accuracy down to `kSubnormalMin`.
template <auto kOffset, typename Float = decltype(kOffset)>
Float ExpM(const Float x) {
  if constexpr (kOffset == Float{1.0}) {
    return std::expm1(x);
  } else {
    return std::exp(x) - kOffset;
  }
}

// Evaluates `x * x - 2` to bisect `sqrt(2)`.
template <typename Float>
Float Sqrt2Residual(const Float x) {
  return x * x - Float{2};
}

// Benchmarks `BisectMonotonic` with `std::midpoint` on `exp(x) - 1e38 = 0` over
// `[0, max]` using Google Benchmark `state`.
template <typename Float>
void BM_BisectExpMBig_StdMidpoint(benchmark::State& state) {
  constexpr Float kBigValue = Float{1e38};
  const auto float_midpoint = [](const Float a, const Float b) -> Float {
    return std::midpoint(a, b);
  };
  for (const auto _ : state) {
    Float low = Float{0};
    Float high = std::numeric_limits<Float>::max();
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, ExpM<kBigValue>, float_midpoint);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectExpMBig_StdMidpoint<float>);
BENCHMARK(BM_BisectExpMBig_StdMidpoint<double>);

// Benchmarks `BisectMonotonic` with `OrdinalMidpoint` on `exp(x) - 1e38 = 0`
// over `[0, max]` using Google Benchmark `state`.
template <typename Float>
void BM_BisectExpMBig_OrdinalMidpoint(benchmark::State& state) {
  constexpr Float kBigValue = Float{1e38};
  const auto ordinal_midpoint = [](const Float a, const Float b) -> Float {
    return OrdinalMidpoint(a, b);
  };
  for (const auto _ : state) {
    Float low = Float{0};
    Float high = std::numeric_limits<Float>::max();
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, ExpM<kBigValue>, ordinal_midpoint);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectExpMBig_OrdinalMidpoint<float>);
BENCHMARK(BM_BisectExpMBig_OrdinalMidpoint<double>);

// Benchmarks `OrdinalBisectMonotonic` on `exp(x) - 1e38 = 0` over `[0, max]`
// using Google Benchmark `state`.
template <typename Float>
void BM_BisectExpMBig_OrdinalBisect(benchmark::State& state) {
  constexpr Float kBigValue = Float{1e38};
  for (const auto _ : state) {
    Float low = Float{0};
    Float high = std::numeric_limits<Float>::max();
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        OrdinalBisectMonotonic(low, high, ExpM<kBigValue>);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectExpMBig_OrdinalBisect<float>);
BENCHMARK(BM_BisectExpMBig_OrdinalBisect<double>);

// Benchmarks `BisectMonotonic` with `std::midpoint` on `exp(x) - 1 = 0` over
// `[0, max]` using Google Benchmark `state`.
template <typename Float>
void BM_BisectExpMOne_StdMidpoint(benchmark::State& state) {
  const auto float_midpoint = [](const Float a, const Float b) -> Float {
    return std::midpoint(a, b);
  };
  for (const auto _ : state) {
    Float low = Float{0};
    Float high = std::numeric_limits<Float>::max();
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, ExpM<Float{1.0}>, float_midpoint);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectExpMOne_StdMidpoint<float>);
BENCHMARK(BM_BisectExpMOne_StdMidpoint<double>);

// Benchmarks `BisectMonotonic` with `OrdinalMidpoint` on `exp(x) - 1 = 0` over
// `[0, max]` using Google Benchmark `state`.
template <typename Float>
void BM_BisectExpMOne_OrdinalMidpoint(benchmark::State& state) {
  const auto ordinal_midpoint = [](const Float a, const Float b) -> Float {
    return OrdinalMidpoint(a, b);
  };
  for (const auto _ : state) {
    Float low = Float{0};
    Float high = std::numeric_limits<Float>::max();
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, ExpM<Float{1.0}>, ordinal_midpoint);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectExpMOne_OrdinalMidpoint<float>);
BENCHMARK(BM_BisectExpMOne_OrdinalMidpoint<double>);

// Benchmarks `OrdinalBisectMonotonic` on `exp(x) - 1 = 0` over `[0, max]` using
// Google Benchmark `state`.
template <typename Float>
void BM_BisectExpMOne_OrdinalBisect(benchmark::State& state) {
  for (const auto _ : state) {
    Float low = Float{0};
    Float high = std::numeric_limits<Float>::max();
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        OrdinalBisectMonotonic(low, high, ExpM<Float{1.0}>);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectExpMOne_OrdinalBisect<float>);
BENCHMARK(BM_BisectExpMOne_OrdinalBisect<double>);

// Benchmarks `BisectMonotonic` with `std::midpoint` on `x * x - 2 = 0` over
// `[1, 2]` using Google Benchmark `state`.
template <typename Float>
void BM_BisectSqrt2_StdMidpoint(benchmark::State& state) {
  const auto float_midpoint = [](const Float a, const Float b) -> Float {
    return std::midpoint(a, b);
  };
  for (const auto _ : state) {
    Float low = Float{1};
    Float high = Float{2};
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, Sqrt2Residual<Float>, float_midpoint);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectSqrt2_StdMidpoint<float>);
BENCHMARK(BM_BisectSqrt2_StdMidpoint<double>);

// Benchmarks `BisectMonotonic` with `OrdinalMidpoint` on `x * x - 2 = 0` over
// `[1, 2]` using Google Benchmark `state`.
template <typename Float>
void BM_BisectSqrt2_OrdinalMidpoint(benchmark::State& state) {
  const auto ordinal_midpoint = [](const Float a, const Float b) -> Float {
    return OrdinalMidpoint(a, b);
  };
  for (const auto _ : state) {
    Float low = Float{1};
    Float high = Float{2};
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, Sqrt2Residual<Float>, ordinal_midpoint);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectSqrt2_OrdinalMidpoint<float>);
BENCHMARK(BM_BisectSqrt2_OrdinalMidpoint<double>);

// Benchmarks `OrdinalBisectMonotonic` on `x * x - 2 = 0` over `[1, 2]` using
// Google Benchmark `state`.
template <typename Float>
void BM_BisectSqrt2_OrdinalBisect(benchmark::State& state) {
  for (const auto _ : state) {
    Float low = Float{1};
    Float high = Float{2};
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        OrdinalBisectMonotonic(low, high, Sqrt2Residual<Float>);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(BM_BisectSqrt2_OrdinalBisect<float>);
BENCHMARK(BM_BisectSqrt2_OrdinalBisect<double>);

}  // namespace
}  // namespace operations_research
