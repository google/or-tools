// Copyright 2010-2022 Google LLC
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

//
// Benchmarks for different implementations of interval arithmetic.
//
// The goal of this file is to see how the different possible implementations
// perform against one another, to get a ballpark estimate of the possible gain
// of a full hardware implementation of interval arithmetic as proposed
// in IEEE-1788.
//
// We tried to be as meaningful and simple as possible.
//
// For simpilicity, only addition is implemented, on a single double.
// We could make things faster, and amortize the cost of changing rounding
// modes.
// For example, it is possible, and it is often done, to represent
// intervals as a pair {min, -max} and only round towards -infinty.
// This improves things a bit, but far less than having instructions that
// do not need changing the rounding more.
// We could have computed the lower bounds of the sums, and then their upper
// bounds.
// However, these ideas do not work in the context of a generic bound
// propagator for arbitrarily complex formulas.
// Intervals have to be handled in a simple way, for the programmer to
// be able to use them, and most importantly the rounding mode must be
// reset to its default after each computation.
//
// AVX512 seems to be a promising direction, albeit incomplete, towards
// a good hardware implementation of interval computation.
// Note: compile with -mavx512f to get access to avx512.
//
// Interesting references:
// What every computer scientist should know about floating-point arithmetic
// ACM Computing Surveys Volume 23 Issue 1 March 1991 pp 5–48
// https://dl.acm.org/doi/10.1145/103162.103163
// also at https://docs.oracle.com/cd/E19957-01/800-7895/800-7895.pdf, with the
// addendum https://docs.oracle.com/cd/E37069_01/html/E39019/z400228248508.html.
//
// Accuracy and Stability of Numerical Algorithms by Nicholas J. Higham.
// https://epubs.siam.org/doi/book/10.1137/1.9780898718027
//
// The program outputs a CSV-formatted summary that is easily pastable in
// spreadsheets or table generators. ns/it is the number of nanoseconds per
// iteration.
//
// The correct value for SumOfIntegers is 500000000500000000.000000...
// The correct value for SumOfSquareRoots is 21081851083600.37596259382529338.
//
// Here is a sample run. Notice the width of the intervals. This is the price
// to pay for soundness. There exist algorithms to reduce them. See for example
// https://www.sciencedirect.com/science/article/pii/S0004370298000538
// and the related algorithms.
// The run was performed on a machine supporting AVX512F. AVX512F rounding masks
// per instruction make it possible to effectively use interval arithmetic, even
// though the instructions are not ideal.
//
// Name,ns/it,result
// SumOfIntegers<RoundToNearestEven>,1.1801, \
//     [,500000000067108992.00000000000000,500000000067108992.00000000000000,]
// SumOfIntegers<StdRounding>,19.5461, \
//     [,499999987755261568.00000000000000,500000013244738176.00000000000000,]
// SumOfIntegers<LowLevelRounding>,5.71184, \
//     [,499999987755261568.00000000000000,500000013244738176.00000000000000,]
// SumOfIntegers<Avx512Rounding>,1.17857, \
//     [,499999987755261568.00000000000000,500000013244738176.00000000000000,]
// SumOfSquareRoots<RoundToNearestEven>,2.7984, \
//     [,21081851083600.55859375000000,21081851083600.55859375000000,]
// SumOfSquareRoots<StdRounding>,23.91, \
//     [,21081850394046.03906250000000,21081851773099.68359375000000,]
// SumOfSquareRoots<LowLevelRounding>,6.64908, \
//     [,21081850394046.03906250000000,21081851773099.68359375000000,]
// SumOfSquareRoots<Avx512Rounding>,2.91871, \
//     [,21081850394046.03906250000000,21081851773099.68359375000000,]

// TODO(user): make it work on ARM, RISC-V, and POWER.

#include <cassert>
#include <cfenv>   // NOLINT
#include <chrono>  // NOLINT(build/c++11)
#include <cmath>
#include <cstdint>
#include <cstdio>

#if defined(__x86_64__)
#include <x86intrin.h>
#endif
#if defined(_M_AMD64)  // For MSVC.
#include <intrin.h>
#endif
#if defined(__x86_64__) || defined(_M_AMD64)
#define AMD64
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>
#endif

// Is this really necessary? g++ does not like it.
#if defined(__clang__)
#pragma STDC FENV_ACCESS ON
#endif

struct RoundToNearestEven {
  // Abuse of language to describe an "interval" computed using the standard
  // round-to-even mode.
  static inline double AddUp(double a, double b) { return a + b; }
  static inline double AddDown(double a, double b) { return a + b; }
};

class StdRoundingScope {
 public:
  explicit StdRoundingScope(int new_mode) : saved_mode_(std::fegetround()) {
    std::fesetround(new_mode);
  }
  ~StdRoundingScope() { std::fesetround(saved_mode_); }

 private:
  int saved_mode_;
};

struct StdRounding {
  // Simplest possible implementation of addition with rounding modes on a
  // single double, using the standard fegetround() / fesetround()
  // functions.
  static inline double AddUp(double a, double b) {
    StdRoundingScope up(FE_UPWARD);
    return a + b;
  }

  static inline double AddDown(double a, double b) {
    StdRoundingScope down(FE_DOWNWARD);
    return a + b;
  }
};

// Making it a little bit faster. Supported only on x86-64 for now.
// We use intrinsics to only touch the non-x87 FP rounding modes.
// Contrary to fesetround, it does nothing about x87 rounding mode.
// This is safe because we reset the status to what it was right after
// performing the operation. Also, we store the previous status, and
// therefore minimize the number of reads from the status register.

// Returns the contents of the floating-point control register.
inline unsigned int GetFloatingPointControlRegister() {
#if defined(AMD64)
  // Returns the contents of the MX control status register on x86.
  return _mm_getcsr();
#else
  // TODO(user): implement for other architectures.
  return 0;
#endif  // AMD64
}

// Sets the contents of the floating-point control register.
inline void SetFloatingPointControlRegister(unsigned int status) {
#if defined(AMD64)
  // Sets the contents of the MX control status register on x86.
  _mm_setcsr(status);
#endif
}

// Sets the rounding mode, using the constants as defined in <cfenv>.
inline void SetRoundingMode(int status, int mode) {
#if defined(AMD64)
  // As said above, we're not touching the x87 part of the CPU.
  constexpr int kRoundingModeMask = 0x6000;
  constexpr int kRoundingModeShift = 3;
  SetFloatingPointControlRegister((status & ~kRoundingModeMask) |
                                  (mode << kRoundingModeShift));
#endif  // AMD64
}

class LowLevelRoundingScope {
 public:
  explicit LowLevelRoundingScope(int new_mode)
      : saved_status_(GetFloatingPointControlRegister()) {
    SetRoundingMode(saved_status_, new_mode);
  }
  ~LowLevelRoundingScope() { SetFloatingPointControlRegister(saved_status_); }

 private:
  int saved_status_;
};

struct LowLevelRounding {
  // Faster rounding addition using the above rounding mode functions.
  static inline double AddUp(double a, double b) {
    LowLevelRoundingScope up(FE_UPWARD);
    return a + b;
  }

  static inline double AddDown(double a, double b) {
    LowLevelRoundingScope down(FE_DOWNWARD);
    return a + b;
  }
};

#ifdef __AVX512F__
struct Avx512Rounding {
  // Even better implementation of rounding addition using instructions
  // for which one can specify the rounding mode explicitly.
  // Using AVX512.
  // Note: we did not manage to use multi-versioning with
  // __attribute__((target("arch=skylake-avx512")))
  // and a fallback version with __attribute__((target("default")))
  // which should be done at the caller level and not here for performance
  // reasons.
  static inline double AddUp(double a, double b) {
    const __m128d x = _mm_set_sd(a);
    const __m128d y = _mm_set_sd(b);
    const __m128d result =
        _mm_add_round_sd(x, y, (_MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC));
    return _mm_cvtsd_f64(result);
  }

  static inline double AddDown(double a, double b) {
    const __m128d x = _mm_set_sd(a);
    const __m128d y = _mm_set_sd(b);
    const __m128d result =
        _mm_add_round_sd(x, y, (_MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
    return _mm_cvtsd_f64(result);
  }
};
#else
struct Avx512Rounding {
  static inline double AddUp(double a, double b) { return 0.0; }
  static inline double AddDown(double a, double b) { return 0.0; }
};
#endif

struct Interval {
  Interval(double l, double u) : lb(l), ub(u) {}
  double lb;
  double ub;
};

// The functions that we are considering return an interval, in which
// the correct answer is guaranteed to be.
typedef Interval (*IntervalFunction)(int n);

// This is a small runner that calls an IntervalFunction, counts the
// nanoseconds in a portable way, counts the cycles, and reports various
// stats. We're not concerned with timing the instructions independently.
// Just reporting the number of cycles per iteration is good enough.
void Runner(IntervalFunction f, int n) {
  std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();
  // We don't use __rdtsc() because it's not portable, and it is influenced by
  // throttling. I can't be used to give the true frequency of the CPU.
  Interval result = f(n);
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  double time_in_nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
  printf("%g,[,%20.14f,%20.14f,]\n", time_in_nanos / n, result.lb, result.ub);
  fflush(stdout);
}

// Compute the sum of the first natural numbers up to and including n.
// The result is n * (n + 1) / 2. For n = 1'000'000'000, it is
// 500'000'000'500'000'000 or 5.000000005e+17.
template <typename Impl>
Interval SumOfIntegers(int n) {
  const double max = n;
  double l = 0.0;
  double u = 0.0;
  for (double d = 0.0; d <= max; d += 1.0) {
    l = Impl::AddDown(l, d);
    u = Impl::AddUp(u, d);
  }
  return Interval(l, u);
}

// For 1'000'000'000, the default round-to-nearest rounding mode returns
// 500'000'000'067'108'992 with clang on an x86_64 PC running Linux, far
// from the actual result.
template Interval SumOfIntegers<RoundToNearestEven>(int);
template Interval SumOfIntegers<StdRounding>(int);
template Interval SumOfIntegers<LowLevelRounding>(int);
template Interval SumOfIntegers<Avx512Rounding>(int);

// Compute the sum of the square roots of the first natural numbers up to
// and including n.
// It's a well-known case explained in "Floating-Point Computating: A Comedy
// of Errors?" by Gregory Tarzy and Neil Toda from Sun Microsytems on
// 2004-01-20 on the now-defunct site developers.sun.com.
// The standard rounding mode returns 21081851083600.55859375000000
// while the correct answer computed using Maxima is
// 21081851083600.37596259382529338.
// Interestingly, a Pentium II at 400MHz took more than 6 hours to complete.
// Different pairs of (OS, CPU) produced quite different results. Always
// wrong.
template <typename Impl>
Interval SumOfSquareRoots(int n) {
  Interval result(0.0, 0.0);
  double l = 0.0;
  double u = 0.0;
  for (int i = 0; i <= n; ++i) {
    const double d = std::sqrt(i);
    l = Impl::AddDown(l, d);
    u = Impl::AddUp(u, d);
  }
  return Interval(l, u);
}

template Interval SumOfSquareRoots<RoundToNearestEven>(int);
template Interval SumOfSquareRoots<StdRounding>(int);
template Interval SumOfSquareRoots<LowLevelRounding>(int);
template Interval SumOfSquareRoots<Avx512Rounding>(int);

#define BENCHMARK(fn, n) \
  {                      \
    printf("%s,", #fn);  \
    Runner(fn, n);       \
  }

int main() {
#if !defined(AMD64)
  printf("Warning: x86-64 instrinsics not supported.\n");
#endif
#if !defined(__AVX512F__)
  printf("Warning: AVX512F not supported.\n");
#endif
  const int n = 1'000'000'000;
  printf("Name,ns/it,result\n");
  BENCHMARK(SumOfIntegers<RoundToNearestEven>, n);
  BENCHMARK(SumOfIntegers<StdRounding>, n);
  BENCHMARK(SumOfIntegers<LowLevelRounding>, n);
  BENCHMARK(SumOfIntegers<Avx512Rounding>, n);
  BENCHMARK(SumOfSquareRoots<RoundToNearestEven>, n);
  BENCHMARK(SumOfSquareRoots<StdRounding>, n);
  BENCHMARK(SumOfSquareRoots<LowLevelRounding>, n);
  BENCHMARK(SumOfSquareRoots<Avx512Rounding>, n);
  return 0;
}
