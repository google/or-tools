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

#include "ortools/base/numbers.h"

#include <cctype>
#include <cstdlib>

namespace operations_research {

#ifdef _MSC_VER
#define strtof strtod
#define strtoll _strtoi64
#endif  // _MSC_VER

bool safe_strtof(const char* str, float* value) {
  char* endptr;
  *value = strtof(str, &endptr);
  if (endptr != str) {
    while (isspace(*endptr)) ++endptr;
  }
  // Ignore range errors from strtod/strtof.
  // The values it returns on underflow and
  // overflow are the right fallback in a
  // robust setting.
  return *str != '\0' && *endptr == '\0';
}

bool safe_strtod(const char* str, double* value) {
  char* endptr;
  *value = strtod(str, &endptr);
  if (endptr != str) {
    while (isspace(*endptr)) ++endptr;
  }
  // Ignore range errors from strtod.  The values it
  // returns on underflow and overflow are the right
  // fallback in a robust setting.
  return *str != '\0' && *endptr == '\0';
}

bool safe_strtof(const std::string& str, float* value) {
  return safe_strtof(str.c_str(), value);
}

bool safe_strtod(const std::string& str, double* value) {
  return safe_strtod(str.c_str(), value);
}

bool safe_strto64(const std::string& str, int64* value) {
  if (str.empty()) return false;
  char* endptr;
  *value = strtoll(str.c_str(), &endptr, /*base=*/10);  // NOLINT
  return *endptr == '\0' && str[0] != '\0';
}

#undef strtof
#undef strtoll

}  // namespace operations_research
