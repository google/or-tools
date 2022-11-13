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

#include "ortools/util/fp_roundtrip_conv.h"

#include <array>
#include <charconv>
#include <limits>
#include <ostream>
#include <string>
#include <system_error>  // NOLINT(build/c++11)

#include "absl/status/statusor.h"
#include "absl/strings/charconv.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_builder.h"

#define OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
#if defined(__wasm__) || defined(ANDROID) || defined(_MSC_VER)
#undef OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
#endif
#if defined(__APPLE__) || defined(__FreeBSD__)
#undef OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
#endif
#if defined(__GNUC__) && !defined(__llvm__) && __GNUC__ < 11
#undef OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
#endif

namespace operations_research {
namespace {

// When using std::to_chars(), the maximum number of digits for the mantissa is
// std::numeric_limits<double>::max_digits10, which is 17. On top of that the
// max_exponent10 is 308, which takes at most 3 digits. We also have to take
// into account the "e+"/"e-", the "-" sign and the ".". Thus the buffer must be
// at least 17 + 3 + 2 + 1 + 1 = 24 bytes long.
//
// When using absl::SNPrintF() with "%.*g" with `max_digits10` for the
// precision, it prints at most `precision + 4` digits. The +4 occurs for
// numbers d.ddddd...eX where -1 <= X <= -4 since in that case the number is
// printed with some leading zeros (i.e. 0.000dddd...). Thus we must have a
// string at least 28 bytes long.
//
// To be safe we had some margin and use a buffer of 4 * 8 bytes.
using RoundTripDoubleBuffer = std::array<char, 32>;

// Writes the double to the provided buffer and returns a view on the written
// range.
absl::string_view RoundTripDoubleToBuffer(const double value,
                                          RoundTripDoubleBuffer& buffer) {
#ifdef OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  CHECK(result.ec == std::errc()) << std::make_error_code(result.ec).message();
  return absl::string_view(buffer.data(), result.ptr - buffer.data());
#else   // OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
  // Here we use a version that use enough digits to have roundtrip. We lose the
  // specification that we use the shortest string though.
  //
  // We use absl::SNPrintF() since it does not depend on the locale (contrary to
  // std::snprintf()).
  const int written =
      absl::SNPrintF(buffer.data(), buffer.size(), "%.*g",
                     std::numeric_limits<double>::max_digits10, value);
  CHECK_GT(written, 0);
  CHECK_LT(written, buffer.size());
  return absl::string_view(buffer.data(), written);
#endif  // OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
}

}  // namespace

#ifdef OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED
ABSL_CONST_INIT const bool kStdToCharsDoubleIsSupported = true;
#else
ABSL_CONST_INIT const bool kStdToCharsDoubleIsSupported = false;
#endif

#undef OPERATION_RESEARCH_STD_TO_CHARS_DOUBLE_SUPPORTED

std::ostream& operator<<(std::ostream& out,
                         const RoundTripDoubleFormat& format) {
  RoundTripDoubleBuffer buffer;
  out << RoundTripDoubleToBuffer(format.value_, buffer);
  return out;
}

std::string RoundTripDoubleFormat::ToString(const double value) {
  RoundTripDoubleBuffer buffer;
  return std::string(RoundTripDoubleToBuffer(value, buffer));
}

absl::StatusOr<double> RoundTripDoubleFormat::Parse(
    const absl::string_view str_value) {
  const char* const begin = str_value.data();
  const char* const end = begin + str_value.size();
  double ret = 0.0;
  const auto result = absl::from_chars(begin, end, ret);
  if (result.ec == std::errc()) {
    if (result.ptr != end) {
      return util::InvalidArgumentErrorBuilder()
             << '"' << absl::CEscape(str_value)
             << "\" has unexpected suffix starting at index "
             << (result.ptr - begin);
    }
    return ret;
  }
  switch (result.ec) {
    case std::errc::invalid_argument:
      return util::InvalidArgumentErrorBuilder()
             << '"' << absl::CEscape(str_value) << "\" is not a valid double";
    case std::errc::result_out_of_range:
      return util::InvalidArgumentErrorBuilder()
             << '"' << absl::CEscape(str_value)
             << "\" does not fit in a double precision float";
    default:
      return util::InternalErrorBuilder()
             << "parsing of \"" << absl::CEscape(str_value)
             << "\" failed with an unexpected error: "
             << std::make_error_code(result.ec).message();
  }
}

}  // namespace operations_research
