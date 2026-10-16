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
// exp(x)-1e38 [0,max] | float  |    1,979 |      588 |    206 | 2.85x / 9.61x
// exp(x)-1e38 [0,max] | double |   14,363 |    1,610 |    664 | 2.42x / 21.6x
// exp(x)-1    [0,max] | float  |    2,838 |      344 |    105 | 3.28x / 27.0x
// exp(x)-1    [0,max] | double |   32,017 |      927 |    298 | 3.11x / 107x
// x * x - 2   [1, 2]  | float  |      101 |      206 |   36.9 | 5.58x / 2.74x
// x * x - 2   [1, 2]  | double |      233 |      436 |   65.6 | 6.65x / 3.55x
// y^3-2*y-5   [0,max] | float  |      914 |      330 |   47.9 | 6.89x / 19.1x
// y^3-2*y-5   [0,max] | double |    6,569 |      627 |   84.0 | 7.46x / 78.2x
// y^3-2*y-5   [2, 3]  | float  |      132 |      233 |   35.7 | 6.53x / 3.70x
// y^3-2*y-5   [2, 3]  | double |      311 |      500 |   69.9 | 7.15x / 4.45x
//
// Key observations:
// 1. No per-step forward transforms or XMM <-> GPR round-trips: in
//    `BisectMonotonic` with `OrdinalMidpoint`, `(low, high)` are stored as
//    floats, forcing each step to move `low` and `high` from floating-point
//    registers to integer registers (`FloatToOrdinal`), compute the midpoint,
//    and move the result back (`OrdinalToFloat`). `OrdinalBisectMonotonic`
//    keeps `(low_ordinal, high_ordinal)` in integer registers for the entire
//    loop and performs only a single `OrdinalToFloat(mid_ordinal)` per step.
// 2. Faster than `std::midpoint` on `[1, 2]` (`2.7x`-`3.6x`): even when both
//    methods take the exact same `M` steps (`23` for `float`, `52` for
//    `double`), `std::midpoint(float, float)` executes magnitude range checks
//    and branches at every step to guard against overflow and underflow,
//    whereas `std::midpoint(low_ordinal, high_ordinal)` compiles to a
//    branchless integer subtraction, right-shift, and addition.
// 3. Low penalty for wide initial brackets (`[0, max]` or `[0, +inf]` vs
//    `[2, 3]` on `y^3 - 2*y - 5`): omitting domain-specific bracket narrowing
//    and starting directly from `[0, max]` (or `[0, +inf]`) adds only `E`
//    exponent-halving steps (`8` for `float`, `11` for `double`) on top of the
//    `M` significand steps (`23` for `float`, `52` for `double`). With the
//    ordinal transform, `[0, max]` costs only `1.20x`-`1.34x` (`Direct`) or
//    `1.25x`-`1.42x` (`Ord`) as much as `[2, 3]`, whereas `std::midpoint` pays
//    a `6.9x` (`float`) to `21.1x` (`double`) penalty to step through every
//    binade linearly.

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

// Evaluates `std::midpoint(a, b)`. Taking the address of a non-member standard
// library function such as `std::midpoint<Float>` has unspecified behavior in
// C++20 (`[namespace.std]` §16.4.5.2.1), and `<numeric>` also overloads
// `std::midpoint` for both arithmetic types `T(T, T)` and pointers `T*(T*,
// T*)`, making `std::midpoint<Float>` an ambiguous overload set when passed to
// a deduced template parameter `MidpointFn`.
template <typename Float>
Float StdMidpoint(const Float a, const Float b) {
  return std::midpoint(a, b);
}

// Evaluates `x * x - 2` to bisect `sqrt(2)`.
template <typename Float>
Float Sqrt2Residual(const Float x) {
  return x * x - Float{2};
}

// Evaluates Isaac Newton's historical cubic `y^3 - 2 * y - 5` from *De analysi
// per aequationes numero terminorum infinitas* (1669, published 1711) and
// *Methodus fluxionum et serierum infinitarum* (1671, published 1736), which
// has a unique real root at `y* ~= 2.09455148154232659148`.
template <typename Float>
Float NewtonCubicResidual(const Float y) {
  return y * y * y - Float{2} * y - Float{5};
}

// Benchmarks `BisectMonotonic` with `std::midpoint` on `kFn` over
// `[kLow, kHigh]` using Google Benchmark `state`.
template <typename Float, Float (*kFn)(Float), Float kLow = Float{0},
          Float kHigh = std::numeric_limits<Float>::max()>
void BM_Bisect_StdMidpoint(benchmark::State& state) {
  for (const auto _ : state) {
    Float low = kLow;
    Float high = kHigh;
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, kFn, StdMidpoint<Float>);
    benchmark::DoNotOptimize(res);
  }
}

// Benchmarks `BisectMonotonic` with `OrdinalMidpoint` on `kFn` over
// `[kLow, kHigh]` using Google Benchmark `state`.
template <typename Float, Float (*kFn)(Float), Float kLow = Float{0},
          Float kHigh = std::numeric_limits<Float>::max()>
void BM_Bisect_OrdinalMidpoint(benchmark::State& state) {
  for (const auto _ : state) {
    Float low = kLow;
    Float high = kHigh;
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res =
        BisectMonotonic(low, high, kFn, OrdinalMidpoint<Float>);
    benchmark::DoNotOptimize(res);
  }
}

// Benchmarks `OrdinalBisectMonotonic` on `kFn` over `[kLow, kHigh]` using
// Google Benchmark `state`.
template <typename Float, Float (*kFn)(Float), Float kLow = Float{0},
          Float kHigh = std::numeric_limits<Float>::max()>
void BM_Bisect_OrdinalBisect(benchmark::State& state) {
  for (const auto _ : state) {
    Float low = kLow;
    Float high = kHigh;
    benchmark::DoNotOptimize(low);
    benchmark::DoNotOptimize(high);
    BisectionResult<Float> res = OrdinalBisectMonotonic(low, high, kFn);
    benchmark::DoNotOptimize(res);
  }
}

// 1. `exp(x) - 1e38 = 0` on `[0, max]`.
BENCHMARK(BM_Bisect_StdMidpoint<float, ExpM<float{1e38}>>);
BENCHMARK(BM_Bisect_StdMidpoint<double, ExpM<double{1e38}>>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<float, ExpM<float{1e38}>>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<double, ExpM<double{1e38}>>);
BENCHMARK(BM_Bisect_OrdinalBisect<float, ExpM<float{1e38}>>);
BENCHMARK(BM_Bisect_OrdinalBisect<double, ExpM<double{1e38}>>);

// 2. `exp(x) - 1 = 0` on `[0, max]`.
BENCHMARK(BM_Bisect_StdMidpoint<float, ExpM<float{1.0}>>);
BENCHMARK(BM_Bisect_StdMidpoint<double, ExpM<double{1.0}>>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<float, ExpM<float{1.0}>>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<double, ExpM<double{1.0}>>);
BENCHMARK(BM_Bisect_OrdinalBisect<float, ExpM<float{1.0}>>);
BENCHMARK(BM_Bisect_OrdinalBisect<double, ExpM<double{1.0}>>);

// 3. `x * x - 2 = 0` on `[1, 2]`.
BENCHMARK(BM_Bisect_StdMidpoint<float, Sqrt2Residual, 1.0f, 2.0f>);
BENCHMARK(BM_Bisect_StdMidpoint<double, Sqrt2Residual, 1.0, 2.0>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<float, Sqrt2Residual, 1.0f, 2.0f>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<double, Sqrt2Residual, 1.0, 2.0>);
BENCHMARK(BM_Bisect_OrdinalBisect<float, Sqrt2Residual, 1.0f, 2.0f>);
BENCHMARK(BM_Bisect_OrdinalBisect<double, Sqrt2Residual, 1.0, 2.0>);

// 4. `y^3 - 2 * y - 5 = 0` on `[0, max]`.
BENCHMARK(BM_Bisect_StdMidpoint<float, NewtonCubicResidual>);
BENCHMARK(BM_Bisect_StdMidpoint<double, NewtonCubicResidual>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<float, NewtonCubicResidual>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<double, NewtonCubicResidual>);
BENCHMARK(BM_Bisect_OrdinalBisect<float, NewtonCubicResidual>);
BENCHMARK(BM_Bisect_OrdinalBisect<double, NewtonCubicResidual>);

// 5. `y^3 - 2 * y - 5 = 0` on `[2, 3]`.
BENCHMARK(BM_Bisect_StdMidpoint<float, NewtonCubicResidual, 2.0f, 3.0f>);
BENCHMARK(BM_Bisect_StdMidpoint<double, NewtonCubicResidual, 2.0, 3.0>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<float, NewtonCubicResidual, 2.0f, 3.0f>);
BENCHMARK(BM_Bisect_OrdinalMidpoint<double, NewtonCubicResidual, 2.0, 3.0>);
BENCHMARK(BM_Bisect_OrdinalBisect<float, NewtonCubicResidual, 2.0f, 3.0f>);
BENCHMARK(BM_Bisect_OrdinalBisect<double, NewtonCubicResidual, 2.0, 3.0>);

}  // namespace
}  // namespace operations_research
