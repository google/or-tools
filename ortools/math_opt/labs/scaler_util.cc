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

#include "ortools/math_opt/labs/scaler_util.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/types.h"
#include "ortools/util/fp_utils.h"

// This file provides an implementation of the ideas exposed in
// go/mpsolver-scaling. The rationale of why scaling is important in the
// context of mathematical programming, the limits imposed by the common
// solver implementations, and the algorithmic ideas are explored there.

namespace operations_research {

std::string AutoShiftingBitmask65::DebugString() const {
  return absl::StrFormat("msb: %3d mantissa: 0x%016lx", msb_, mask_);
}

// The computation of bit_diff relies on the hardware using two's complement
// representation for negative numbers. Test that the corner case holds.
// Note that according to the C++ standard, static_cast behaves as if two's
// complement
static_assert(static_cast<uint32_t>(
                  static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) -
                  static_cast<uint32_t>(std::numeric_limits<int32_t>::min())) ==
              static_cast<int64_t>(std::numeric_limits<int32_t>::max()) -
                  static_cast<int64_t>(std::numeric_limits<int32_t>::min()));
void AutoShiftingBitmask65::SetBit(int bit) {
  // Update mask_ if the distance between msb_ and bit is up to 65 bit
  // positions.
  if (bit < msb_) {
    // The correctness of this operation relies on the machine using two's
    // complement representation for negative numbers. We test that in the
    // preceding static_assert.
    const uint32_t bit_diff =
        static_cast<uint32_t>(msb_) - static_cast<uint32_t>(bit);
    if (bit_diff <= 64) mask_ |= uint64_t{1} << (64 - bit_diff);
    return;
  }
  // Nothing to do, already set.
  if (bit == msb_) return;
  // Set new msb_.
  const uint32_t bit_diff =
      static_cast<uint32_t>(bit) - static_cast<uint32_t>(msb_);
  msb_ = bit;
  // If the bits are too far apart, it is equivalent to set the mask to zero
  // and return. Also catch extreme case where resulting mask_ is 1.
  if (bit_diff >= 64) {
    mask_ = bit_diff == 64 ? 1 : 0;
    return;
  }
  // Regular case: shift mask to adjust to new maximum.
  mask_ = mask_ >> bit_diff;
  mask_ |= uint64_t{1} << (64 - bit_diff);  // New position of former msb_.
}

RowScalingRange::RowScalingRange(const int min_log2_value,
                                 const int max_log2_value)
    : min_log2_value_(min_log2_value), max_log2_value_(max_log2_value) {
  // 2^0=1 should always be a valid coefficient.
  CHECK_LE(min_log2_value, 0);
  CHECK_GE(max_log2_value, 0);
}

std::string RowScalingRange::DebugString() const {
  return absl::StrFormat("coeff: {%s} bounds: {%s}",
                         coefficients_.DebugString(), bounds_.DebugString());
}

void RowScalingRange::UpdateWithBounds(const double value,
                                       const double lower_bound,
                                       const double upper_bound) {
  DCHECK_LT(std::abs(value), kScalerInfinity);
  coefficients_.SetBit(fast_ilogb(value));
  if (std::abs(lower_bound) <= kScalerInfinity) {
    // Our choice of kScalerInfinity ensures this quantity does not overflow.
    bounds_.SetBit(fast_ilogb(value * lower_bound));
  }
  if (std::abs(upper_bound) <= kScalerInfinity) {
    bounds_.SetBit(fast_ilogb(value * upper_bound));
  }
}

void RowScalingRange::Update(const double value) {
  if (std::abs(value) >= kScalerInfinity) return;
  const int value_ilogb = fast_ilogb(value);
  coefficients_.SetBit(value_ilogb);
  bounds_.SetBit(value_ilogb);
}

namespace internal {

// Note that max_exponent is stored separately in the AutoShiftingBitmask65
// structure, so we are keeping 65 bits of information. To retain only the most
// significant kRelZeroLog2 bits, we need to set to zero all initial bits in the
// exponent mask. We do this by masking these bits. We also ensure that we don't
// consider bits under the absolute zero tolerance.
//
// We ignore all bits that are either below -kScalerInfinityLog2 (absolute zero)
// or below range.GetMsb() + kRelZeroLog2. So we precompute the number of bits
// that this will discard in our 65-bit mantissa.
int ComputeMinimumNonIgnoredBit(const AutoShiftingBitmask65& range) {
  // We need some fractional bits to be considered as 'reliable'.
  static_assert(kRelZeroLog2 < 0);
  const int num_discarded_bits =
      65 + std::max(kRelZeroLog2, -(range.GetMsb() + kScalerInfinityLog2));
  if (num_discarded_bits >= 64) {
    // Two special cases (regrouped to speed up the common case):
    // 1) If we need to discard the entire mantissa + the MSB, we're below the
    //    absolute zero: return 0, i.e. there are no bits at all.
    if (num_discarded_bits >= 65) return 0;
    // 2) We need to discard the 64-bit mantissa but not the MSB.
    return range.GetMsb();
  }
  // We remove the ignored bits from the mantissa and compute the LSB.
  const int least_significant_bit =
      ffsll(range.GetMask() & (kuint64max << num_discarded_bits));
  // If no other bit was set, then the only exponent seen was the
  // max_exponent.
  return range.GetMsb() +
         (least_significant_bit == 0 ? 0 : least_significant_bit - 65);
}

}  // namespace internal

// In this function we want to see if a sequence of numbers and products (whose
// information is already summarized in the range.coefficients and
// range.bounds) needs to be re-scaled. We do this with some caveats:
//
// First, if the maximum magnitude is under our absolute zero tolerance,
// we do not perform any scaling, as the recommended way to deal with these
// coefficients is to disregard them (i.e. treat them as true zero values).
//
// Second, given that we use the concept of relative zero magnitudes (which
// should be treated as zero), we truncate the information in the ranges
// to consider up to kRelZeroLog2 bits.
//
// With these modifications, we compute the largest and smallest exponents
// seen.
// - If both ranges are within [min_log2_value_, max_log2_value_], don't scale.
// - Otherwise, first try to maintain or shift coefficients such that
//   range.coefficients is within [min_log2_value_, max_log2_value_], If the
//   coefficient range is larger, we snap its upper bound to max_log2_value_
//   (and its lower bound will be below min_log2_value_).
// - If after the previous shift, there is still room to improve on the
//   range.bounds (i.e. if range.coefficients is within [min_log2_value_,
//   max_log2_value_], first try to maintain or shift coefficients such that
//   range.bounds is within [min_log2_value_, max_log2_value_], If the
//   bounds range is larger, we snap its upper bound to max_log2_value_ (and
//   its lower bound will be below min_log2_value_), while at the same time
//   ensuring that the resulting range.coefficients will still be within
//   [min_log2_value_, max_log2_value_].
int RowScalingRange::GetLog2Scale(
    const OverflowHandlingMode overflow_handling_mode) const {
  const int max_coefficient_bit = coefficients_.GetMsb();
  const int max_overall_bit = std::max(bounds_.GetMsb(), max_coefficient_bit);
  // If max_overall_bit is under our absolute zero tolerance, do not scale.
  if (max_overall_bit <= -kScalerInfinityLog2) return 0;

  const int min_coefficient_bit =
      internal::ComputeMinimumNonIgnoredBit(coefficients_);
  const int min_overall_bit =
      std::max(-std::numeric_limits<int32_t>::max(),
               std::min(internal::ComputeMinimumNonIgnoredBit(bounds_),
                        min_coefficient_bit));
  if (max_overall_bit <= max_log2_value_ &&
      min_overall_bit >= min_log2_value_) {
    return 0;
  }
  return CorrectLog2Scale(
      GetUncorrectedLog2Scale(
          /*bit_range=*/{.min = min_coefficient_bit,
                         .max = max_coefficient_bit},
          overflow_handling_mode),
      /*coefficient_bit_range=*/
      {.min = min_coefficient_bit, .max = max_coefficient_bit},
      /*overall_bit_range=*/{.min = min_overall_bit, .max = max_overall_bit});
}

int RowScalingRange::GetUncorrectedLog2Scale(
    const Log2BitRange bit_range,
    const OverflowHandlingMode overflow_handling_mode) const {
  if (bit_range.max - bit_range.min <= max_log2_value_ - min_log2_value_) {
    // If the coefficient range fits within the range of desired value, there is
    // no overflow, and then there is not difference between the two modes of
    // overflow handling.
    const int log2_scale = [&]() {
      if (bit_range.max > max_log2_value_) {
        return max_log2_value_ - bit_range.max;
      } else if (bit_range.min < min_log2_value_) {
        return min_log2_value_ - bit_range.min;
      } else {
        return 0;
      }
    }();
    DCHECK_GE(bit_range.min + log2_scale, min_log2_value_);
    DCHECK_LE(bit_range.max + log2_scale, max_log2_value_);
    return log2_scale;
  }
  // Otherwise, the scaling depend on the scaling_mode.
  switch (overflow_handling_mode) {
    case OverflowHandlingMode::kClampToMin:
      return min_log2_value_ - bit_range.min;
    case OverflowHandlingMode::kClampToMax:
      return max_log2_value_ - bit_range.max;
    case OverflowHandlingMode::kEvenOverflow:
      // Although this formula can be simplified, in this form is easier to
      // understand.
      const int overflow =
          (bit_range.max - bit_range.min) - (max_log2_value_ - min_log2_value_);
      // We need to move the smallest coefficient to min_log2_value_ minus
      // half the overflow.
      return (min_log2_value_ - bit_range.min) - overflow / 2;
  }
}

int RowScalingRange::CorrectLog2Scale(
    int log2_scale, const Log2BitRange coefficient_bit_range,
    const Log2BitRange overall_bit_range) const {
  // Compute the interval [min_delta..max_delta] of the delta (positive or
  // negative) that we can add to log2_scale while keeping the coefficient
  // range within bounds.
  const int max_delta =
      std::max(0, max_log2_value_ - coefficient_bit_range.max - log2_scale);
  const int min_delta =
      std::min(0, min_log2_value_ - coefficient_bit_range.min - log2_scale);
  // Move to improve quality of bounds range.
  if (overall_bit_range.max + log2_scale > max_log2_value_) {
    log2_scale += std::max(
        min_delta, max_log2_value_ - overall_bit_range.max - log2_scale);
  } else if (overall_bit_range.min + log2_scale < min_log2_value_) {
    log2_scale += std::min(
        max_delta, min_log2_value_ - overall_bit_range.min - log2_scale);
  }
  VLOG(4) << absl::StrFormat(
      "coeff {%d,%d} bound {%d,%d} scale %d delta {%d,%d}",
      coefficient_bit_range.min, coefficient_bit_range.max,
      overall_bit_range.min, overall_bit_range.max, log2_scale, min_delta,
      max_delta);
  return log2_scale;
}

void ColumnScalingRange::UpdateWithCoefficient(const double coefficient) {
  if (std::abs(coefficient) < kScalerInfinity) {
    coefficients_.SetBit(fast_ilogb(coefficient));
  }
}

namespace {

struct IntMinMax {
  int min = kScalerInfinityLog2;
  int max = -kScalerInfinityLog2;
};

std::optional<IntMinMax> BitRangeFromBitmask(
    const AutoShiftingBitmask65& range) {
  const int max_bit = range.GetMsb();
  if (max_bit <= kRelZeroLog2) {
    return std::nullopt;
  }
  return IntMinMax{.min = internal::ComputeMinimumNonIgnoredBit(range),
                   .max = max_bit};
}

std::optional<IntMinMax> BitRangeFromBounds(const double lower_bound,
                                            const double upper_bound) {
  std::optional<IntMinMax> result;
  if (std::abs(lower_bound) > kRelZero && lower_bound > -kScalerInfinity) {
    const int lower_bound_ilogb = fast_ilogb(lower_bound);
    result = IntMinMax{.min = lower_bound_ilogb, .max = lower_bound_ilogb};
  }
  if (std::abs(upper_bound) > kRelZero && upper_bound < kScalerInfinity) {
    if (!result.has_value()) {
      result.emplace();
    }
    const int upper_bound_ilogb = fast_ilogb(upper_bound);
    result = IntMinMax{.min = std::min(result->min, upper_bound_ilogb),
                       .max = std::max(result->max, upper_bound_ilogb)};
  }
  return result;
}

// Returns how far the `range` is from the extrema of our acceptable interval
// [`min_log2_value`, `max_log2_value`]. A negative value in `.min`
// (respectively, `.max`) means that the `range` exceeds the lower (resp.,
// upper) bound of the acceptable interval; a nonnegative value indicates how
// far `range` can be shifted before it hits the lower (resp., upper) bound.
IntMinMax BitRangeToBitRangeDiff(const std::optional<IntMinMax> range,
                                 const int min_log2_value,
                                 const int max_log2_value) {
  if (!range.has_value()) {
    return IntMinMax{.min = kScalerInfinityLog2, .max = kScalerInfinityLog2};
  }
  return IntMinMax{.min = range->min - min_log2_value,
                   .max = max_log2_value - range->max};
}

}  // namespace

int ColumnScalingRange::GetLog2Scale(const int min_log2_value,
                                     const int max_log2_value) const {
  // Negative values mean that we would like to do some scaling to repair.
  // Nonnegative values mean that we have this amount of slack to scale, for
  // some other reason, without hitting the extrema in this direction.
  const IntMinMax coefficient_diff = BitRangeToBitRangeDiff(
      BitRangeFromBitmask(coefficients_), min_log2_value, max_log2_value);
  const IntMinMax bound_diff =
      BitRangeToBitRangeDiff(BitRangeFromBounds(lower_bound_, upper_bound_),
                             min_log2_value, max_log2_value);

  // There are 5 possible cases to consider, each with potentially conflicting
  // remedies. So, we order them: prefer coefficient scaling over bound
  // scaling, and prefer scaling down large values over scaling up small values.
  if (coefficient_diff.max < 0) {
    // The coefficients are too large. We implicitly ClampToMax, and we care
    // more about matrix coefficients than about bounds, so we look here first.
    return -coefficient_diff.max;
  } else if (coefficient_diff.min < 0) {
    // The coefficients are too small. Again, we care more about matrix
    // coefficients than about bounds, so we handle this second, making sure not
    // to scale so much that the coefficients become too large.
    return std::max(coefficient_diff.min, -coefficient_diff.max);
  } else if (bound_diff.max < 0) {
    // The coefficients are fine, but the upper bound is large. We scale the
    // variables, but mind to make sure that we don't make the coefficients too
    // large as the bounds and coefficients are scaled in different directions.
    return std::max(bound_diff.max, -coefficient_diff.max);
  } else if (bound_diff.min < 0) {
    // Everything is OK except for the lower bound. We must watch for both the
    // coefficients becoming too small and for the bound becoming too large.
    return std::min(-bound_diff.min,
                    std::min(coefficient_diff.min, bound_diff.max));
  } else {
    // Everything is within range, so don't do any scaling.
    return 0;
  }
}

}  // namespace operations_research
