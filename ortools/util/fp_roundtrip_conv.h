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

// Classes and function to convert floating point numbers to string so that no
// information is lost (i.e. that we can make a round trip from double to string
// and back to double without losing data).
#ifndef OR_TOOLS_UTIL_FP_ROUNDTRIP_CONV_H_
#define OR_TOOLS_UTIL_FP_ROUNDTRIP_CONV_H_

#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace operations_research {

// True if the plateform supports `double` to std::to_chars().
//
// std::to_chars() for double is not yet supported on Emscripten, Android and
// iOS; they only implement std::to_chars() for integers.
ABSL_CONST_INIT extern const bool kStdToCharsDoubleIsSupported;

// Formatter using std::to_chars() to print a `double` so that a round trip
// conversion back to double will result in the same number (using
// absl::from_chars()). One exception are NaNs that may not round trip
// (i.e. multiple NaNs could end up being printed the same).
//
// Usage:
//
//  const double x = ...;
//  LOG(INFO) << "x: " << RoundTripDoubleFormat(x);
//
//  const std::string x_str =
//    absl::StrCat("x: ", RoundTripDoubleFormat::ToString(x));
//
//  ASSIGN_OR_RETURN(const double y,
//                   RoundTripDoubleFormat::Parse(x_str));
//
// Note that some operating systems do not support std::to_chars() for double
// yet but only implement it for integers (see kStdToCharsDoubleIsSupported). On
// those operating systems, a formatting equivalent to using "%.*g" with
// max_digits10 precision is used.
class RoundTripDoubleFormat {
 public:
  explicit RoundTripDoubleFormat(const double value) : value_(value) {}

  // Prints the formatted double to the provided stream.
  friend std::ostream& operator<<(std::ostream& out,
                                  const RoundTripDoubleFormat& format);

  // Returns a string with the provided double formatted.
  //
  // Prefer using operator<< when possible (with LOG(), StatusBuilder,
  // std::cout,...) since it avoids allocating a temporary string.
  //
  // PERFORMANCE: operator<< may be noticeably faster in some extreme cases,
  // especially in non-64bit platforms or when value is in (-∞, -1e+100] or in
  // [-1e-100, 0). This is because the string won't fit in
  // small-string-optimization buffer, and will thus need a heap memory
  // allocation which is slow.
  static std::string ToString(double value);

  // Parses the input string with absl::from_chars(), returning an error if the
  // input string does not start with a number or has extra characters after
  // it. It also fails if the number can't fit in a `double`.
  //
  // This function offers a round-trip from string printed/built by this
  // formatter.
  static absl::StatusOr<double> Parse(const absl::string_view str_value);

 private:
  const double value_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_FP_ROUNDTRIP_CONV_H_
