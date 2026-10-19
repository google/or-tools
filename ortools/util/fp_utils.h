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

// Utility functions on IEEE floating-point numbers.
// Implemented on float, double, and long double.
//
// Also a placeholder for tools controlling and checking FPU rounding modes.
//
// IMPORTANT NOTICE: you need to compile your binary with -frounding-math if
// you want to use rounding modes.

#ifndef ORTOOLS_UTIL_FP_UTILS_H_
#define ORTOOLS_UTIL_FP_UTILS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/util/full_precision_inequalities.h"

namespace operations_research {

// WARNING: A DEBUGGING TOOL, AND NOTHING ELSE. Do not instantiate this class
// in production code, in a library meant for production code, or in a solver
// path that a user can reach. Unmasking an exception makes the process abort
// on an operation that IEEE 754 defines as valid and that the rest of the code
// is entitled to rely on: a library that computes with a NaN deliberately, a
// probe that overflows on purpose, a vectorized loop that evaluates both
// branches, and a good deal of third-party numerical code all become crashes.
// The failure lands wherever the object happens to be alive, including in
// unrelated code called from there, so shipping one turns a correct program
// into a fragile one.
//
// What it does: the constructor unmasks the requested floating-point
// exceptions, so that raising one of them traps instead of merely setting a
// status flag, and the destructor restores the previously enabled exceptions.
// The point is to find, in a test or under a debugger, the operation that
// manufactures a NaN or an infinity, by turning it into a crash at the
// instruction that causes it.
//
// Examples, each trapping the operation on the right:
//
//   ScopedFloatingPointEnv trap(FE_DIVBYZERO);  // 1.0 / 0.0
//   ScopedFloatingPointEnv trap(FE_OVERFLOW);   // 1e308 * 10.0
//   ScopedFloatingPointEnv trap(FE_INVALID);    // 0.0 / 0.0, and other NaNs
//   ScopedFloatingPointEnv trap(FE_DIVBYZERO | FE_OVERFLOW);  // either one
//
// FE_INVALID catches the operation that manufactures a NaN out of operands
// that are not NaNs, not the later ones that merely propagate it.
//
// Platform dependencies: trapping is supported on 64-bit x86 Linux with glibc
// via `feenableexcept()` and `fedisableexcept()`. On all other platforms, the
// class is a no-op and `exceptions_enabled()` returns false. An object that
// silently does nothing on unsupported platforms is one more reason not to
// build anything on top of this class.
//
// Note to open-source users:
//  - Platforms that provide `feenableexcept()` and `fedisableexcept()` may
//    work as-is on `x86_64` or other architectures once added to the
//    `OR_TOOLS_FP_TRAPS_GNU` preprocessor guard in `fp_utils.cc`. See the
//    comments in `fp_utils.cc` for details.
//  - Windows/MSVC requires platform-specific code (`_controlfp_s()` and
//    `#pragma fenv_access(on)`).
//  - Apple macOS and iOS do not provide `feenableexcept()` in the Apple SDK,
//    and Apple Silicon (`arm64`) CPUs do not support trapped floating-point
//    exceptions in hardware (the Arm architecture makes the `FPCR` trap-enable
//    bits optional, and Apple cores hardwire them to zero). On `arm64` Darwin
//    targets where hardware traps are present, the kernel delivers them as
//    `SIGILL` (`ILL_ILLTRP`) rather than `SIGFPE`, so platform-specific code
//    would also need to install a `SIGILL` signal handler to dispatch
//    floating-point exceptions.
//  - Bug reports, test feedback, ports to new platforms, and contributions are
//    welcome.
class ScopedFloatingPointEnv {
 public:
  // `excepts` is an or-combination of FE_XXX constants, possibly empty.
  explicit ScopedFloatingPointEnv(int excepts);
  ~ScopedFloatingPointEnv();

  // The floating-point environment is per-thread state; copying an active
  // guard would restore the saved mask twice or out of LIFO order.
  ScopedFloatingPointEnv(const ScopedFloatingPointEnv&) = delete;
  ScopedFloatingPointEnv& operator=(const ScopedFloatingPointEnv&) = delete;

  // Whether the exceptions requested at construction are actually trapping.
  // An empty request counts as enabled on supported platforms.
  bool exceptions_enabled() const { return exceptions_enabled_; }

 private:
  int saved_excepts_ = 0;
  bool exceptions_enabled_ = false;
};

// Tests whether x and y are close to one another using absolute and relative
// tolerances.
// Returns true if |x - y| <= a (with a being the absolute_tolerance).
// The above case is useful for values that are close to zero.
// Returns true if |x - y| <= max(|x|, |y|) * r. (with r being the relative
//                                                tolerance.)
// The cases for infinities are treated separately to avoid generating NaNs.
template <typename FloatType>
bool AreWithinAbsoluteOrRelativeTolerances(FloatType x, FloatType y,
                                           FloatType relative_tolerance,
                                           FloatType absolute_tolerance) {
  DCHECK_LE(0.0, relative_tolerance);
  DCHECK_LE(0.0, absolute_tolerance);
  DCHECK_GT(1.0, relative_tolerance);
  if (std::isinf(x) || std::isinf(y)) {
    return x == y;
  }
  const FloatType difference = std::fabs(x - y);
  if (difference <= absolute_tolerance) {
    return true;
  }
  const FloatType largest_magnitude = std::max(std::fabs(x), std::fabs(y));
  return difference <= largest_magnitude * relative_tolerance;
}

// Tests whether x and y are close to one another using an absolute tolerance.
// Returns true if |x - y| <= a (with a being the absolute_tolerance).
// The cases for infinities are treated separately to avoid generating NaNs.
//
// Some matchers are available in fp_utils_testing.h for gUnit tests.
template <typename FloatType>
bool AreWithinAbsoluteTolerance(FloatType x, FloatType y,
                                FloatType absolute_tolerance) {
  DCHECK_LE(0.0, absolute_tolerance);
  if (std::isinf(x) || std::isinf(y)) {
    return x == y;
  }
  return std::fabs(x - y) <= absolute_tolerance;
}

// Returns true if x is less than y or slightly greater than y with the given
// absolute or relative tolerance.
template <typename FloatType>
bool IsSmallerWithinTolerance(FloatType x, FloatType y, FloatType tolerance) {
  if (std::isinf(y)) return x <= y;
  return x <= y + tolerance * std::max(FloatType(1.0),
                                       std::min(std::abs(x), std::abs(y)));
}

// Returns true if x is within tolerance of any integer.  Always returns
// false for x equal to +/- infinity.
template <typename FloatType>
inline bool IsIntegerWithinTolerance(FloatType x, FloatType tolerance) {
  DCHECK_LE(0.0, tolerance);
  if (std::isinf(x)) return false;
  return std::abs(x - std::round(x)) <= tolerance;
}

// Given an array of doubles, this computes a positive scaling factor such that
// the scaled doubles can then be rounded to integers with little or no loss of
// precision, and so that the L1 norm of these integers is <= max_sum. More
// precisely, the following formulas will hold (c[i] is input[i], for brevity):
// a. For all i, |round(factor * c[i]) / factor  - c[i]| <= error * |c[i]|
// b. The sum over i of |round(factor * c[i])| <= max_sum.
//
// The algorithm tries to minimize "error" (which is the relative error for one
// coefficient). Note however than in really broken cases, the error might be
// infinity and the factor zero.
//
// Note on the algorithm:
// - It only uses factors of the form 2^n (i.e. ldexp(1.0, n)) for simplicity.
// - The error will be zero in many practical instances. For example, if x
//   contains only integers with low magnitude; or if x contains doubles whose
//   exponents cover a small range.
// - It chooses the factor as high as possible under the given constraints, as
//   a result the numbers produced may be large. To balance this, we recommend
//   to divide the scaled integers by their gcd() which will result in no loss
//   of precision and will help in many practical cases.
//
// TODO(user): incorporate the gcd computation here? The issue is that I am
// not sure if I just do factor /= gcd that round(x * factor) will be the same.
void GetBestScalingOfDoublesToInt64(absl::Span<const double> input,
                                    int64_t max_absolute_sum,
                                    double* scaling_factor,
                                    double* max_relative_coeff_error);

// Returns the scaling factor like above replacing condition b with:
//  b1.  The sum over i of min(0, round(factor * c[i] * x[i])) >= -max_sum
//  b2.  The sum over i of max(0, round(factor * c[i] * x[i])) <= max_sum,
// for any possible values of the x[i] such that x[i] is in [lb[i], ub[i]]. Note
// that b above is equivalent to take lb[i] = -1 and ub[i] = 1 for each i.
double GetBestScalingOfDoublesToInt64(absl::Span<const double> input,
                                      absl::Span<const double> lb,
                                      absl::Span<const double> ub,
                                      int64_t max_absolute_sum);
// This computes:
//
// The max_relative_coeff_error, which is the maximum over all coeff of
// |round(factor * x[i]) / (factor * x[i])  - 1|.
//
// The max_scaled_sum_error which is a bound on the maximum difference between
// the exact scaled sum and the rounded one. One needs to divide this by
// scaling_factor to have the maximum absolute error on the original sum.
void ComputeScalingErrors(absl::Span<const double> input,
                          absl::Span<const double> lb,
                          absl::Span<const double> ub, double scaling_factor,
                          double* max_relative_coeff_error,
                          double* max_scaled_sum_error);

// Given a constraint constraint_lb <= sum input_coeffs[i] Xi <= constraints_ub.
// Where each Xi is an integer in [input_lbs[i], input_ubs[i]]. This assumes we
// scale it via std::round(input_coeffs[i] * scaling_factor) and it computes
// "tight" errors. See the Get*Error() functions for more details.
class TightScalingErrorHelper {
 public:
  // Loads an unscaled constraint. We use a class and this API so the memory
  // can be reused from one constraint to the next.
  //
  // Checked preconditions: finite non-zero coefficients and
  // kint64min < input_lbs[i] <= input_ubs[i] < kint64max.
  void LoadUnscaledConstraint(absl::Span<const double> input_coeffs,
                              absl::Span<const int64_t> input_lbs,
                              absl::Span<const int64_t> input_ubs);

  // This and LoadUnscaledConstraint() must be called before any of the
  // GetLowerBoundRoundingError() functions. The API is done like this so we can
  // compute the error for multiple scaling factor on the same constraint.
  // Checked preconditions: finite scaling_factor >= 0.
  void LoadScalingFactor(double scaling_factor);

  // Alternative to LoadUnscaledConstraint()/LoadScalingFactor(), this loads
  // directly the scaled/rounded pair. Note that we keep the same name, but the
  // two set of coeff can really be anything as long as they have the same size.
  // kint64min < input_lbs[i] <= input_ubs[i] < kint64max.
  void LoadBothConstraints(absl::Span<const double> scaled_coeffs,
                           absl::Span<const double> rounded_coeffs,
                           absl::Span<const int64_t> input_lbs,
                           absl::Span<const int64_t> input_ubs);

  // We can compute 4 different errors. To avoid division, they are all
  // expressed in the scaled domain. One can divide the errors by scaling_factor
  // to be in the original constraint domain.
  //
  // This works as follow (same for the upper bound side).
  // Given
  //   SCALED_TERMS = sum xi * (input_coeffs[i] * scaling_factor)
  //   ROUNDED_TERMS = sum xi * std::round(input_coeffs[i] * scaling_factor)
  // For any feasible solution of:
  //   (SCALED_TERMS >= constraint_lb)
  // we should have:
  //   (ROUNDED_TERMS >= constraint_lb - rounding_error)
  // Moreover, if x' is a solution of the above constraint, we have:
  //   (SCALED_TERMS' >= constraint_lb - rounding_error - unrounding_error)
  //
  // Note: all computations are done with double, so these bounds are not exact,
  // but should be relatively precise for reasonable constraints.
  //
  // Note: For the upper bound, the sign of the errors are inverted, so
  // we will have ROUNDED_TERMS <= constraints_ub + rounding_error.
  //
  // Note: The bounds are "monotonous" in the sense that the lower the
  // constraint lb (resp. the higher the constraint ub), the lower the error
  // will be.
  double GetLowerBoundRoundingError(double constraint_lb) {
    bound_.Clear();
    bound_.Add(constraint_lb);
    return -Optimize<true>(scaled_coeffs_, rounded_coeffs_, bound_);
  }
  double GetLowerBoundUnroundingError(double constraint_lb) {
    bound_.Clear();
    bound_.Add(constraint_lb);
    return -Optimize<true>(rounded_coeffs_, scaled_coeffs_, bound_);
  }
  double GetUpperBoundRoundingError(double constraint_ub) {
    bound_.Clear();
    bound_.Add(-constraint_ub);
    return Optimize<false>(scaled_coeffs_, rounded_coeffs_, bound_);
  }
  double GetUpperBoundUnroundingError(double constraint_ub) {
    bound_.Clear();
    bound_.Add(-constraint_ub);
    return Optimize<false>(rounded_coeffs_, scaled_coeffs_, bound_);
  }

  // Assuming ROUNDED_TERMS >= constraint_lb, returns a guaranteed lower bound
  // on SCALED_TERMS + scaled_offset.
  double GetScaledLowerBoundFromRoundedLowerBound(TightSum constraint_lb,
                                                  double scaled_offset = 0.0) {
    return Optimize<true, /*compute_error=*/false>(
        rounded_coeffs_, scaled_coeffs_, constraint_lb, scaled_offset);
  }

 private:
  // Returns the minimum of (sum (new_coeffs[i] - coeffs[i]) * Xi) where:
  // - Xi in [lbs_[i], ubs_[i]],
  // - sum coeffs[i] * Xi >= slack (or <= -slack if is_ge_constraint is false).
  //
  // If `compute_error` is false, returns a guaranteed lower bound on
  // offset + (sum new_coeffs[i] * Xi) directly instead of subtracting
  // `coeffs[i] * Xi`.
  //
  // If we declare that we have `ubs_[i] - lbs_[i]` items of cost
  // `new_coeffs[i]`, the problem we are solving can be seen as a knapsack
  // problem. We relax the number of items we pick of each type to a continuous
  // variable, so have a relaxed problem we can solve exactly with a greedy
  // assignment to provide a correct bound.
  //
  // If `is_ge_constraint` is false, returns the maximum subject to <= -slack.
  // This is the same problem if we reformulate the constraint as distance
  // to the maximum activity.
  //
  // Warning: modifies the passed slack.
  //
  // Note that since this shouldn't be used on an infeasible problem, we don't
  // explicitly detect infeasibility and still return something in this case.
  template <bool is_ge_constraint, bool compute_error = true>
  double Optimize(absl::Span<const double> coeffs,
                  absl::Span<const double> new_coeffs, TightSum& slack,
                  double offset = 0.0);
  // Temporary data for Optimize().
  struct KnapsackItem {
    double coeff;           // coeffs[i]
    double delta;           // new_coeffs[i] - coeffs[i] (usually small)
    int64_t range;          // upper_bounds[i] - lower_bounds[i]
    int64_t initial_bound;  // lower_bounds[i] for >= constraint
    int64_t other_bound;    // upper_bounds[i] for >= constraint
  };
  std::vector<KnapsackItem> items_;
  TightSum total_sum_;
  TightSum bound_;

  // Input constraint data. only change on LoadConstraint().
  std::vector<double> coeffs_;  // non-negative.
  std::vector<int64_t> lbs_;
  std::vector<int64_t> ubs_;

  // Just to make the code shorter.
  // We shouldn't care too much about speed here.
  std::vector<double> scaled_coeffs_;
  std::vector<double> rounded_coeffs_;
  bool has_overflow_ = false;
};

// Returns the Greatest Common Divisor of the numbers
// round(fabs(x[i] * scaling_factor)). The numbers 0 are ignored and if they are
// all zero then the result is 1. Note that round(fabs()) is the same as
// fabs(round()) since the numbers are rounded away from zero.
int64_t ComputeGcdOfRoundedDoubles(absl::Span<const double> x,
                                   double scaling_factor);

// Returns alpha * x + (1 - alpha) * y.
template <typename FloatType>
inline FloatType Interpolate(FloatType x, FloatType y, FloatType alpha) {
  return alpha * x + (1.0 - alpha) * y;
}

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FP_UTILS_H_
