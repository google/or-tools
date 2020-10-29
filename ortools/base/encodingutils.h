// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_ENCODINGUTILS_H_
#define OR_TOOLS_BASE_ENCODINGUTILS_H_

#include <string>

namespace EncodingUtils {

// Returns the number of characters of a UTF8-encoded string.
inline int UTF8StrLen(const std::string& utf8_str) {
  if (utf8_str.empty()) return 0;
  const char* c = utf8_str.c_str();
  int count = 0;
  while (*c != '\0') {
    ++count;
    // See http://en.wikipedia.org/wiki/UTF-8#Description .
    const unsigned char x = *c;
    c += x < 0xC0 ? 1 : x < 0xE0 ? 2 : x < 0xF0 ? 3 : 4;
  }
  return count;
}

}  // namespace EncodingUtils

#endif  // OR_TOOLS_BASE_ENCODINGUTILS_H_
