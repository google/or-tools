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

#ifndef OR_TOOLS_MATH_OPT_LABS_SCALER_UTIL_H_
#define OR_TOOLS_MATH_OPT_LABS_SCALER_UTIL_H_

#include <cstdint>
#include <limits>
#include <string>

namespace operations_research {

// Limit on finite quantities.
// Note that most MIP solvers adhere to the rule of using absolute tolerances
// when solving problems. This has many implications (see go/mpsolver-scaling
// for details), but one of them is that you can not meaningfully
// operate/optimize on problems with ranges far away from 10^-6 (the usual
// primal and dual tolerances for most solvers). In fact, most solvers treat
// modest numbers as infinity:
// - Gurobi 1e100 (but bounds over 1e20 are considered infinite)
// - Cplex 1e20
// - SCIP 1e20
// - XPRESS 1e20
// This has to do with the fact that when you compare floating point numbers
// that differ beyond 2^51, the smaller quantity is treated just as zero. We
// allow for far larger values to be considered as `valid` before scaling, but
// these values should be mapped to ranges that the solvers can effectively deal
// with. However, we still consider very large values just as an infinite
// quantity, this protects from overflow in double computations, and also
// signals possible user errors. We also use 2^-kScalerInfinityLog2 as absolute
// zero threshold, i.e. anything less than or equal to 2^-kScalerInfinityLog2 is
// considered as zero.
constexpr int kScalerInfinityLog2 = 332;
// This is hexadecimal notation for floating point, we expect
// ilogb(kScalerInfinity) == kScalerInfinityLog2.
// TODO(user): Change this into pow(2, kScalerInfinityLog2) when it
// supports constexpr.
// NOTE(user): I had to change 0x1p332 to the decimal expression as
// SWIG does not understand (as of 2020-11-24) hexadecimal floating point
// notation.
constexpr double kScalerInfinity =
    8.7490028991320476975e+99;  // 2^332 \approx 8.749e99.

// We use a relative tolerance of about 2e-10 to distinguish zero right hand
// sides from non-zero right-hand-side, see
// go/mpsolver-scaling#the-canned-recommendation.
// Our choice of 2^-32 can be understood as trusting the first 32 bits of
// mantissa results on computation, and treating the last 20 bits as
// `unreliable` due to possible accumulated rounding errors.
constexpr int kRelZeroLog2 = -32;
constexpr double kRelZero = 2.3283064365386962890625e-10;

// A bitmask that remembers the 65 most significant bits set.
class AutoShiftingBitmask65 {
 public:
  // Get the most significant bit set.
  inline int GetMsb() const { return msb_; }
  // Get a mask of the following 64 bits sets, where bit 0 is the least
  // significant bit.
  inline uint64_t GetMask() const { return mask_; }
  void SetBit(int bit);
  std::string DebugString() const;

 private:
  // Most significant bit.
  int msb_ = std::numeric_limits<int32_t>::min();
  // Bits sets under 'msb',
  // note that for k [0:63] if ((mask>>k)&1) == 1,
  // then we have set bit (msb - 64 + k)
  uint64_t mask_ = 0;
};

enum class OverflowHandlingMode { kClampToMin, kClampToMax, kEvenOverflow };

// Stores data associated with a single row -- coefficients and the bounds of
// variables associated with those coefficients -- and suggests how to scale
// them to bring them to a more desirable range (from the perspective of a
// mixed-integer programming solver).
class RowScalingRange {
 public:
  RowScalingRange() = default;
  // Will CHECK-fail if `min_log2_value` is positive or if `max_log2_value` is
  // negative.
  RowScalingRange(int min_log2_value, int max_log2_value);

  // Computes the power-of-two scaling factor that will bring the data in this
  // object to a desirable numerical range. There are a lot of nitty-gritty
  // details to do this in a reasonable and general way; see the .cc file for
  // details.
  int GetLog2Scale(OverflowHandlingMode overflow_handling_mode) const;

  void Update(double value);
  // This function keeps track of the range of double values (or values *
  // bound if the bound is a finite quantity) seen in a sequence of values (for
  // example, coefficients and bounds of variables in a linear constraint). To
  // keep things simple, it relies on looking at the exponent of the double
  // representation, but remembers only the 65 most significant such exponents.
  // Note that there is no point in storing more than 52 significant bits as
  // that is the precision limit of double numbers.
  void UpdateWithBounds(double value, double lower_bound, double upper_bound);

  std::string DebugString() const;

  // The rest is exposed for testing purposes only.
  struct Log2BitRange {
    int min = 0;
    int max = 0;
  };
  int GetUncorrectedLog2Scale(
      Log2BitRange bit_range,
      OverflowHandlingMode overflow_handling_mode) const;
  int CorrectLog2Scale(int log2_scale, Log2BitRange coefficient_bit_range,
                       Log2BitRange overall_bit_range) const;

 private:
  // Order of magnitudes for actual coefficients.
  AutoShiftingBitmask65 coefficients_;
  // Order of magnitudes for products of bounds and coefficients.
  AutoShiftingBitmask65 bounds_;
  // Range of exponent considered as acceptable.
  const int min_log2_value_ = 0;   // 2^0 = 1.
  const int max_log2_value_ = 12;  // 2^12 = 4096.
};

// Stores values associated with a single variable -- its bounds, and
// coefficients from constraints and objectives, and suggests how to scale them
// to bring them to a more desirable range (from the perspective of a
// mixed-integer programming solver).
class ColumnScalingRange {
 public:
  ColumnScalingRange(const double lower_bound, const double upper_bound)
      : lower_bound_(lower_bound), upper_bound_(upper_bound) {}

  // Computes the power-of-two scaling factor that will bring the data in this
  // object to a desirable numerical range of
  // [2^`min_log2_value`, 2^`max_log2_value`]. If this is not attainable, it
  // will prefer to scale coefficients over bounds, and prefer to scale down
  // large values over scaling up small values (i.e., it is implicitly providing
  // ClampToMax behavior).
  int GetLog2Scale(int min_log2_value, int max_log2_value) const;

  void UpdateWithCoefficient(double coefficient);

 private:
  double lower_bound_;
  double upper_bound_;

  AutoShiftingBitmask65 coefficients_;
};

// Exposed publicly for testing purposes only.
namespace internal {

// Compute the smallest bit in `range`, ignoring those that are either very
// small (below -kScalerInfinityLog2) or will be discarded to fit within a
// 65-bit mantissa (relative to the most significant bit in `range`).
int ComputeMinimumNonIgnoredBit(const AutoShiftingBitmask65& range);

}  // namespace internal

}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_LABS_SCALER_UTIL_H_
