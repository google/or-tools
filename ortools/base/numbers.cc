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

// This file contains std::string processing functions related to
// numeric values.

#include "ortools/base/numbers.h"

#include <errno.h>  // for errno

#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>

// #include "ortools/base/logging.h"
#include "absl/strings/ascii.h"
#include "ortools/base/strtoint.h"

namespace strings {

inline int64_t strtoint64(const char* nptr, char** endptr, int base) {
  return std::strtoll(nptr, endptr, base);  // NOLINT
}

inline uint64_t strtouint64(const char* nptr, char** endptr, int base) {
  return std::strtoull(nptr, endptr, base);  // NOLINT
}

inline int32_t strtoint32(const char* nptr, char** endptr, int base) {
  return std::strtol(nptr, endptr, base);  // NOLINT
}

inline uint32_t strtouint32(const char* nptr, char** endptr, int base) {
  return std::strtoul(nptr, endptr, base);  // NOLINT
}

// ----------------------------------------------------------------------
// ParseLeadingInt32Value()
// ParseLeadingUInt32Value()
//    A simple parser for [u]int32_t values. Returns the parsed value
//    if a valid value is found; else returns deflt
//    This cannot handle decimal numbers with leading 0s.
// --------------------------------------------------------------------

int32_t ParseLeadingInt32Value(const char* str, int32_t deflt) {
  using std::numeric_limits;

  char* error = nullptr;
  long value = strtol(str, &error, 0);  // NOLINT
  // Limit long values to int32_t min/max.  Needed for lp64; no-op on 32 bits.
  if (value > numeric_limits<int32_t>::max()) {
    value = numeric_limits<int32_t>::max();
  } else if (value < numeric_limits<int32_t>::min()) {
    value = numeric_limits<int32_t>::min();
  }
  return (error == str) ? deflt : value;
}

uint32_t ParseLeadingUInt32Value(const char* str, uint32_t deflt) {
  using std::numeric_limits;

  if (numeric_limits<unsigned long>::max() ==  // NOLINT
      numeric_limits<uint32_t>::max()) {
    // When long is 32 bits, we can use strtoul.
    char* error = nullptr;
    const uint32_t value = strtoul(str, &error, 0);  // NOLINT
    return (error == str) ? deflt : value;
  } else {
    // When long is 64 bits, we must use strtoint64 and handle limits
    // by hand.  The reason we cannot use a 64-bit strtoul is that
    // it would be impossible to differentiate "-2" (that should wrap
    // around to the value UINT_MAX-1) from a std::string with ULONG_MAX-1
    // (that should be pegged to UINT_MAX due to overflow).
    char* error = nullptr;
    int64_t value = strtoint64(str, &error, 0);
    if (value > numeric_limits<uint32_t>::max() ||
        value < -static_cast<int64_t>(numeric_limits<uint32_t>::max())) {
      value = numeric_limits<uint32_t>::max();
    }
    // Within these limits, truncation to 32 bits handles negatives correctly.
    return (error == str) ? deflt : value;
  }
}

// ----------------------------------------------------------------------
// ParseLeadingDec32Value
// ParseLeadingUDec32Value
//    A simple parser for [u]int32_t values. Returns the parsed value
//    if a valid value is found; else returns deflt
//    The std::string passed in is treated as *10 based*.
//    This can handle strings with leading 0s.
// --------------------------------------------------------------------

int32_t ParseLeadingDec32Value(const char* str, int32_t deflt) {
  using std::numeric_limits;

  char* error = nullptr;
  long value = strtol(str, &error, 10);  // NOLINT
  // Limit long values to int32_t min/max.  Needed for lp64; no-op on 32 bits.
  if (value > numeric_limits<int32_t>::max()) {
    value = numeric_limits<int32_t>::max();
  } else if (value < numeric_limits<int32_t>::min()) {
    value = numeric_limits<int32_t>::min();
  }
  return (error == str) ? deflt : value;
}

uint32_t ParseLeadingUDec32Value(const char* str, uint32_t deflt) {
  using std::numeric_limits;

  if (numeric_limits<unsigned long>::max() ==  // NOLINT
      numeric_limits<uint32_t>::max()) {
    // When long is 32 bits, we can use strtoul.
    char* error = nullptr;
    const uint32_t value = strtoul(str, &error, 10);  // NOLINT
    return (error == str) ? deflt : value;
  } else {
    // When long is 64 bits, we must use strtoint64 and handle limits
    // by hand.  The reason we cannot use a 64-bit strtoul is that
    // it would be impossible to differentiate "-2" (that should wrap
    // around to the value UINT_MAX-1) from a std::string with ULONG_MAX-1
    // (that should be pegged to UINT_MAX due to overflow).
    char* error = nullptr;
    int64_t value = strtoint64(str, &error, 10);
    if (value > numeric_limits<uint32_t>::max() ||
        value < -static_cast<int64_t>(numeric_limits<uint32_t>::max())) {
      value = numeric_limits<uint32_t>::max();
    }
    // Within these limits, truncation to 32 bits handles negatives correctly.
    return (error == str) ? deflt : value;
  }
}

// ----------------------------------------------------------------------
// ParseLeadingUInt64Value
// ParseLeadingInt64Value
// ParseLeadingHex64Value
//    A simple parser for 64-bit values. Returns the parsed value if a
//    valid integer is found; else returns deflt
//    Uint64_t and int64_t cannot handle decimal numbers with leading 0s.
// --------------------------------------------------------------------
uint64_t ParseLeadingUInt64Value(const char* str, uint64_t deflt) {
  char* error = nullptr;
  const uint64_t value = strtouint64(str, &error, 0);
  return (error == str) ? deflt : value;
}

int64_t ParseLeadingInt64Value(const char* str, int64_t deflt) {
  char* error = nullptr;
  const int64_t value = strtoint64(str, &error, 0);
  return (error == str) ? deflt : value;
}

uint64_t ParseLeadingHex64Value(const char* str, uint64_t deflt) {
  char* error = nullptr;
  const uint64_t value = strtouint64(str, &error, 16);
  return (error == str) ? deflt : value;
}

// ----------------------------------------------------------------------
// ParseLeadingDec64Value
// ParseLeadingUDec64Value
//    A simple parser for [u]int64_t values. Returns the parsed value
//    if a valid value is found; else returns deflt
//    The std::string passed in is treated as *10 based*.
//    This can handle strings with leading 0s.
// --------------------------------------------------------------------

int64_t ParseLeadingDec64Value(const char* str, int64_t deflt) {
  char* error = nullptr;
  const int64_t value = strtoint64(str, &error, 10);
  return (error == str) ? deflt : value;
}

uint64_t ParseLeadingUDec64Value(const char* str, uint64_t deflt) {
  char* error = nullptr;
  const uint64_t value = strtouint64(str, &error, 10);
  return (error == str) ? deflt : value;
}

// ----------------------------------------------------------------------
// ParseLeadingDoubleValue()
//    A simple parser for double values. Returns the parsed value
//    if a valid value is found; else returns deflt
// --------------------------------------------------------------------

double ParseLeadingDoubleValue(const char* str, double deflt) {
  char* error = nullptr;
  errno = 0;
  const double value = strtod(str, &error);
  if (errno != 0 ||    // overflow/underflow happened
      error == str) {  // no valid parse
    return deflt;
  } else {
    return value;
  }
}

// ----------------------------------------------------------------------
// ParseLeadingBoolValue()
//    A recognizer of boolean std::string values. Returns the parsed value
//    if a valid value is found; else returns deflt.  This skips leading
//    whitespace, is case insensitive, and recognizes these forms:
//    0/1, false/true, no/yes, n/y
// --------------------------------------------------------------------
bool ParseLeadingBoolValue(const char* str, bool deflt) {
  static const int kMaxLen = 5;
  char value[kMaxLen + 1];
  // Skip whitespace
  while (absl::ascii_isspace(*str)) {
    ++str;
  }
  int len = 0;
  for (; len <= kMaxLen && absl::ascii_isalnum(*str); ++str) {
    value[len++] = absl::ascii_tolower(*str);
  }
  if (len == 0 || len > kMaxLen) return deflt;
  value[len] = '\0';
  switch (len) {
    case 1:
      if (value[0] == '0' || value[0] == 'n') return false;
      if (value[0] == '1' || value[0] == 'y') return true;
      break;
    case 2:
      if (!strcmp(value, "no")) return false;
      break;
    case 3:
      if (!strcmp(value, "yes")) return true;
      break;
    case 4:
      if (!strcmp(value, "true")) return true;
      break;
    case 5:
      if (!strcmp(value, "false")) return false;
      break;
  }
  return deflt;
}
}  // namespace strings
