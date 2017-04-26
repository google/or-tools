// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_BASE_STRTOINT_H_
#define OR_TOOLS_BASE_STRTOINT_H_

#include <stdlib.h>  // For strtol* functions.
#include <string>
#include "ortools/base/basictypes.h"

namespace operations_research {
// Conversions to a 32-bit integer can pass the call to strto[u]l on 32-bit
// platforms, but need a little extra work on 64-bit platforms.
inline int32 strto32(const char* nptr, char** endptr, int base) {
  return strtol(nptr, endptr, base);  // NOLINT
}

inline uint32 strtou32(const char* nptr, char** endptr, int base) {
  return strtoul(nptr, endptr, base);  // NOLINT
}

// For now, long long is 64-bit on all the platforms we care about, so these
// functions can simply pass the call to strto[u]ll.
inline int64 strto64(const char* nptr, char** endptr, int base) {
#if defined(_MSC_VER)
  return _strtoi64(nptr, endptr, base);  // NOLINT
#else
  return strtoll(nptr, endptr, base);   // NOLINT
#endif  // _MSC_VER
}

inline uint64 strtou64(const char* nptr, char** endptr, int base) {
#if defined(_MSC_VER)
  return _strtoui64(nptr, endptr, base);  // NOLINT
#else
  return strtoull(nptr, endptr, base);  // NOLINT
#endif  // _MSC_VER
}

// Although it returns an int, atoi() is implemented in terms of strtol, and
// so has differing overflow and underflow behavior.  atol is the same.
inline int32 atoi32(const char* nptr) { return strto32(nptr, NULL, 10); }

inline int64 atoi64(const char* nptr) { return strto64(nptr, NULL, 10); }

// Convenience versions of the above that take a std::string argument.
inline int32 atoi32(const std::string& s) { return atoi32(s.c_str()); }

inline int64 atoi64(const std::string& s) { return atoi64(s.c_str()); }
}  // namespace operations_research
#endif  // OR_TOOLS_BASE_STRTOINT_H_
