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

// Convert strings to numbers or numbers to strings.

#ifndef OR_TOOLS_BASE_NUMBERS_H_
#define OR_TOOLS_BASE_NUMBERS_H_

#include <functional>
#include <limits>
#include <string>
#include <string_view>

#include "absl/strings/numbers.h"

namespace strings {
// ----------------------------------------------------------------------
// ParseLeadingInt32Value
//    A simple parser for int32_t values. Returns the parsed value
//    if a valid integer is found; else returns deflt. It does not
//    check if str is entirely consumed.
//    This cannot handle decimal numbers with leading 0s, since they will be
//    treated as octal.  If you know it's decimal, use ParseLeadingDec32Value.
// --------------------------------------------------------------------
int32_t ParseLeadingInt32Value(const char* str, int32_t deflt);
inline int32_t ParseLeadingInt32Value(std::string_view str, int32_t deflt) {
  return ParseLeadingInt32Value(std::string(str).c_str(), deflt);
}

// ParseLeadingUInt32Value
//    A simple parser for uint32_t values. Returns the parsed value
//    if a valid integer is found; else returns deflt. It does not
//    check if str is entirely consumed.
//    This cannot handle decimal numbers with leading 0s, since they will be
//    treated as octal.  If you know it's decimal, use ParseLeadingUDec32Value.
// --------------------------------------------------------------------
uint32_t ParseLeadingUInt32Value(const char* str, uint32_t deflt);
inline uint32_t ParseLeadingUInt32Value(std::string_view str, uint32_t deflt) {
  return ParseLeadingUInt32Value(std::string(str).c_str(), deflt);
}

// ----------------------------------------------------------------------
// ParseLeadingDec32Value
//    A simple parser for decimal int32_t values. Returns the parsed value
//    if a valid integer is found; else returns deflt. It does not
//    check if str is entirely consumed.
//    The std::string passed in is treated as *10 based*.
//    This can handle strings with leading 0s.
//    See also: ParseLeadingDec64Value
// --------------------------------------------------------------------
int32_t ParseLeadingDec32Value(const char* str, int32_t deflt);
inline int32_t ParseLeadingDec32Value(std::string_view str, int32_t deflt) {
  return ParseLeadingDec32Value(std::string(str).c_str(), deflt);
}

// ParseLeadingUDec32Value
//    A simple parser for decimal uint32_t values. Returns the parsed value
//    if a valid integer is found; else returns deflt. It does not
//    check if str is entirely consumed.
//    The std::string passed in is treated as *10 based*.
//    This can handle strings with leading 0s.
//    See also: ParseLeadingUDec64Value
// --------------------------------------------------------------------
uint32_t ParseLeadingUDec32Value(const char* str, uint32_t deflt);
inline uint32_t ParseLeadingUDec32Value(std::string_view str, uint32_t deflt) {
  return ParseLeadingUDec32Value(std::string(str).c_str(), deflt);
}

// ----------------------------------------------------------------------
// ParseLeadingUInt64Value
// ParseLeadingInt64Value
// ParseLeadingHex64Value
// ParseLeadingDec64Value
// ParseLeadingUDec64Value
//    A simple parser for long long values.
//    Returns the parsed value if a
//    valid integer is found; else returns deflt
// --------------------------------------------------------------------
uint64_t ParseLeadingUInt64Value(const char* str, uint64_t deflt);
inline uint64_t ParseLeadingUInt64Value(std::string_view str, uint64_t deflt) {
  return ParseLeadingUInt64Value(std::string(str).c_str(), deflt);
}
int64_t ParseLeadingInt64Value(const char* str, int64_t deflt);
inline int64_t ParseLeadingInt64Value(std::string_view str, int64_t deflt) {
  return ParseLeadingInt64Value(std::string(str).c_str(), deflt);
}
uint64_t ParseLeadingHex64Value(const char* str, uint64_t deflt);
inline uint64_t ParseLeadingHex64Value(std::string_view str, uint64_t deflt) {
  return ParseLeadingHex64Value(std::string(str).c_str(), deflt);
}
int64_t ParseLeadingDec64Value(const char* str, int64_t deflt);
inline int64_t ParseLeadingDec64Value(std::string_view str, int64_t deflt) {
  return ParseLeadingDec64Value(std::string(str).c_str(), deflt);
}
uint64_t ParseLeadingUDec64Value(const char* str, uint64_t deflt);
inline uint64_t ParseLeadingUDec64Value(std::string_view str, uint64_t deflt) {
  return ParseLeadingUDec64Value(std::string(str).c_str(), deflt);
}

// ----------------------------------------------------------------------
// ParseLeadingDoubleValue
//    A simple parser for double values. Returns the parsed value
//    if a valid double is found; else returns deflt. It does not
//    check if str is entirely consumed.
// --------------------------------------------------------------------
double ParseLeadingDoubleValue(const char* str, double deflt);
inline double ParseLeadingDoubleValue(std::string_view str, double deflt) {
  return ParseLeadingDoubleValue(std::string(str).c_str(), deflt);
}

// ----------------------------------------------------------------------
// ParseLeadingBoolValue()
//    A recognizer of boolean std::string values. Returns the parsed value
//    if a valid value is found; else returns deflt.  This skips leading
//    whitespace, is case insensitive, and recognizes these forms:
//    0/1, false/true, no/yes, n/y
// --------------------------------------------------------------------
bool ParseLeadingBoolValue(const char* str, bool deflt);
inline bool ParseLeadingBoolValue(std::string_view str, bool deflt) {
  return ParseLeadingBoolValue(std::string(str).c_str(), deflt);
}
}  // namespace strings

#endif  // OR_TOOLS_BASE_NUMBERS_H_
