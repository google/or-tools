// Copyright 2010-2013 Google
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

#ifndef OR_TOOLS_BASE_STRUTIL_H_
#define OR_TOOLS_BASE_STRUTIL_H_

#include <string>
#include "base/stringpiece.h"

using std::string;

namespace operations_research {
// ----------------------------------------------------------------------
// HasSuffixString()
//    Return true if str ends in suffix.
// ----------------------------------------------------------------------

inline bool HasSuffixString(const StringPiece& str, const StringPiece& suffix) {
  return str.ends_with(suffix);
}

inline std::string StrCat(const std::string& str1, const std::string& str2) {
  return str1 + str2;
}

inline std::string StrCat(const std::string& str1, const std::string& str2,
                     const std::string& str3) {
  return str1 + str2 + str3;
}

inline std::string StrCat(const std::string& str1, const std::string& str2, const std::string& str3,
                     const std::string& str4) {
  return str1 + str2 + str3 + str4;
}

inline std::string StrCat(const std::string& str1, const std::string& str2, const std::string& str3,
                     const std::string& str4, const std::string& str5) {
  return str1 + str2 + str3 + str4 + str5;
}

inline std::string StrCat(const std::string& str1, const std::string& str2, const std::string& str3,
                     const std::string& str4, const std::string& str5,
                     const std::string& str6) {
  return str1 + str2 + str3 + str4 + str5 + str6;
}

inline std::string StrCat(const std::string& str1, const std::string& str2, const std::string& str3,
                     const std::string& str4, const std::string& str5, const std::string& str6,
                     const std::string& str7) {
  return str1 + str2 + str3 + str4 + str5 + str6 + str7;
}

inline std::string StrCat(const std::string& str1, const std::string& str2, const std::string& str3,
                     const std::string& str4, const std::string& str5, const std::string& str6,
                     const std::string& str7, const std::string& str8) {
  return str1 + str2 + str3 + str4 + str5 + str6 + str7 + str8;
}

inline std::string StrCat(const std::string& str1, const std::string& str2, const std::string& str3,
                     const std::string& str4, const std::string& str5, const std::string& str6,
                     const std::string& str7, const std::string& str8,
                     const std::string& str9) {
  return str1 + str2 + str3 + str4 + str5 + str6 + str7 + str8 + str9;
}

inline void StrAppend(std::string* str1, const std::string& str2) { *str1 += str2; }

inline void StrAppend(std::string* str1, const std::string& str2, const std::string& str3) {
  *str1 += str2 + str3;
}

inline void StrAppend(std::string* str1, const std::string& str2, const std::string& str3,
                      const std::string& str4) {
  *str1 += str2 + str3 + str4;
}

inline void StrAppend(std::string* str1, const std::string& str2, const std::string& str3,
                      const std::string& str4, const std::string& str5) {
  *str1 += str2 + str3 + str4 + str5;
}
}  // namespace operations_research
#endif  // OR_TOOLS_BASE_STRUTIL_H_
