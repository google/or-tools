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

// Testing matchers built around fp_utils.h functions.
#ifndef ORTOOLS_UTIL_FP_UTILS_TESTING_H_
#define ORTOOLS_UTIL_FP_UTILS_TESTING_H_

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {

// Prints the absolute and relative distances between a and b in the
// result_listener.
//
// This is used to implement WithinAbsoluteOrRelativeTolerances() matchers
// below).
template <typename Float>
void PrintWithinAbsoluteOrRelativeTolerancesDistances(
    testing::MatchResultListener& result_listener, const Float a,
    const Float b) {
  const auto abs_dist = std::abs(a - b);
  // We use std::fmax() as parameter may be NaN (in which case the other
  // parameter is used).
  const auto rel_dist = abs_dist / std::fmax(std::abs(a), std::abs(b));
  result_listener << "absolute distance: " << RoundTripDoubleFormat(abs_dist)
                  << " relative distance: " << RoundTripDoubleFormat(rel_dist);
}

// Tests that a value is close to an `expected` one with provided relative and
// absolute tolerances.
//
// See:
// * WithinSameAbsoluteOrRelativeTolerance() when both tolerance are the same,
// * the overload without `expected` when using `Pointwise()`.
MATCHER_P3(WithinAbsoluteOrRelativeTolerances, expected, relative_tolerance,
           absolute_tolerance,
           absl::StrFormat("%s close to %v with rel_tol=%v abs_tol=%v",
                           (negation ? "isn't" : "is"),
                           RoundTripDoubleFormat(expected),
                           RoundTripDoubleFormat(relative_tolerance),
                           RoundTripDoubleFormat(absolute_tolerance))) {
  if (AreWithinAbsoluteOrRelativeTolerances(
          arg, expected,
          /*relative_tolerance=*/relative_tolerance,
          /*absolute_tolerance=*/absolute_tolerance)) {
    return true;
  }

  PrintWithinAbsoluteOrRelativeTolerancesDistances(*result_listener, arg,
                                                   expected);
  return false;
}

// Tests that a value is close to an `expected` one with provided tolerance used
// as both a relative and an absolute tolerance.
//
// See:
// * WithinAbsoluteOrRelativeTolerances() when both tolerance are not the same,
// * the overload without `expected` when using `Pointwise()`.
MATCHER_P2(WithinSameAbsoluteOrRelativeTolerance, expected, tolerance,
           absl::StrFormat("%s close to %v with rel_tol=abs_tol=%v",
                           (negation ? "isn't" : "is"),
                           RoundTripDoubleFormat(expected),
                           RoundTripDoubleFormat(tolerance))) {
  if (AreWithinAbsoluteOrRelativeTolerances(arg, expected,
                                            /*relative_tolerance=*/tolerance,
                                            /*absolute_tolerance=*/tolerance)) {
    return true;
  }

  PrintWithinAbsoluteOrRelativeTolerancesDistances(*result_listener, arg,
                                                   expected);
  return false;
}

// Overload of WithinAbsoluteOrRelativeTolerances() that compares pairs of
// numbers (represented as std::tuple<>), typically used with
// testing::Pointwise().
MATCHER_P2(WithinAbsoluteOrRelativeTolerances, relative_tolerance,
           absolute_tolerance,
           absl::StrFormat("%s close with rel_tol=%v abs_tol=%v",
                           (negation ? "aren't" : "are"),
                           RoundTripDoubleFormat(relative_tolerance),
                           RoundTripDoubleFormat(absolute_tolerance))) {
  const auto [a, b] = arg;
  if (AreWithinAbsoluteOrRelativeTolerances(
          a, b,
          /*relative_tolerance=*/relative_tolerance,
          /*absolute_tolerance=*/absolute_tolerance)) {
    return true;
  }

  PrintWithinAbsoluteOrRelativeTolerancesDistances(*result_listener, a, b);
  return false;
}

// Overload of WithinSameAbsoluteOrRelativeTolerance() that compares pairs of
// numbers (represented as std::tuple<>), typically used with
// testing::Pointwise().
MATCHER_P(WithinSameAbsoluteOrRelativeTolerance, tolerance,
          absl::StrFormat("%s close with rel_tol=abs_tol=%v",
                          (negation ? "aren't" : "are"),
                          RoundTripDoubleFormat(tolerance))) {
  const auto [a, b] = arg;
  if (AreWithinAbsoluteOrRelativeTolerances(a, b,
                                            /*relative_tolerance=*/tolerance,
                                            /*absolute_tolerance=*/tolerance)) {
    return true;
  }

  PrintWithinAbsoluteOrRelativeTolerancesDistances(*result_listener, a, b);
  return false;
}

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FP_UTILS_TESTING_H_
