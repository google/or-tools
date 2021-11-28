// Copyright 2010-2021 Google LLC
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

// This file contains string processing functions related to
// uppercase, lowercase, etc.
#include "ortools/base/case.h"

#include <functional>
#include <string>

#include "absl/hash/hash.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

namespace strings {

std::ostream& operator<<(std::ostream& os,
                         const AsciiCapitalizationType& type) {
  switch (type) {
    case AsciiCapitalizationType::kLower:
      return os << "kLower";
    case AsciiCapitalizationType::kUpper:
      return os << "kUpper";
    case AsciiCapitalizationType::kFirst:
      return os << "kFirst";
    case AsciiCapitalizationType::kMixed:
      return os << "kMixed";
    case AsciiCapitalizationType::kNoAlpha:
      return os << "kNoAlpha";
    default:
      return os << "INVALID";
  }
}

AsciiCapitalizationType GetAsciiCapitalization(const absl::string_view input) {
  const char* s = input.data();
  const char* const end = s + input.size();
  // find the caps type of the first alpha char
  for (; s != end && !(absl::ascii_isupper(*s) || absl::ascii_islower(*s));
       ++s) {
  }
  if (s == end) return AsciiCapitalizationType::kNoAlpha;
  const AsciiCapitalizationType firstcapstype =
      (absl::ascii_islower(*s)) ? AsciiCapitalizationType::kLower
                                : AsciiCapitalizationType::kUpper;

  // skip ahead to the next alpha char
  for (++s; s != end && !(absl::ascii_isupper(*s) || absl::ascii_islower(*s));
       ++s) {
  }
  if (s == end) return firstcapstype;
  const AsciiCapitalizationType capstype =
      (absl::ascii_islower(*s)) ? AsciiCapitalizationType::kLower
                                : AsciiCapitalizationType::kUpper;

  if (firstcapstype == AsciiCapitalizationType::kLower &&
      capstype == AsciiCapitalizationType::kUpper)
    return AsciiCapitalizationType::kMixed;

  for (; s != end; ++s)
    if ((absl::ascii_isupper(*s) &&
         capstype != AsciiCapitalizationType::kUpper) ||
        (absl::ascii_islower(*s) &&
         capstype != AsciiCapitalizationType::kLower))
      return AsciiCapitalizationType::kMixed;

  if (firstcapstype == AsciiCapitalizationType::kUpper &&
      capstype == AsciiCapitalizationType::kLower)
    return AsciiCapitalizationType::kFirst;
  return capstype;
}

int AsciiCaseInsensitiveCompare(absl::string_view s1, absl::string_view s2) {
  if (s1.size() == s2.size()) {
    return strncasecmp(s1.data(), s2.data(), s1.size());
  } else if (s1.size() < s2.size()) {
    int res = strncasecmp(s1.data(), s2.data(), s1.size());
    return (res == 0) ? -1 : res;
  } else {
    int res = strncasecmp(s1.data(), s2.data(), s2.size());
    return (res == 0) ? 1 : res;
  }
}

size_t AsciiCaseInsensitiveHash::operator()(absl::string_view s) const {
  // return absl::HashOf(absl::AsciiStrToLower(s));
  return std::hash<std::string>{}(absl::AsciiStrToLower(s));
}

bool AsciiCaseInsensitiveEq::operator()(absl::string_view s1,
                                        absl::string_view s2) const {
  return s1.size() == s2.size() &&
         strncasecmp(s1.data(), s2.data(), s1.size()) == 0;
}

void MakeAsciiTitlecase(std::string* s, absl::string_view delimiters) {
  bool upper = true;
  for (auto& ch : *s) {
    if (upper) {
      ch = absl::ascii_toupper(ch);
    }
    upper = (absl::StrContains(delimiters, ch));
  }
}

std::string MakeAsciiTitlecase(absl::string_view s,
                               absl::string_view delimiters) {
  std::string result(s);
  MakeAsciiTitlecase(&result, delimiters);
  return result;
}
}  // namespace strings
