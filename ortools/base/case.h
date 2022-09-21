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

// This package contains character classification functions for evaluating
// the case state of strings, and converting strings to uppercase, lowercase,
// etc.
//
// Unlike <ctype.h> (or absl/strings/ascii.h), the functions in this file
// are designed to operate on strings, not single characters.
//
// Except for those marked as "using the C/POSIX locale", these functions are
// for ASCII strings only.

#ifndef OR_TOOLS_BASE_CASE_H_
#define OR_TOOLS_BASE_CASE_H_

#ifndef _MSC_VER
#include <strings.h>  // for strcasecmp, but msvc does not have this header
#endif

#include <cstddef>
#include <cstring>
#include <functional>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/port.h"  // disable some warnings on Windows
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"

namespace strings {
// Enum values returned by GetAsciiCapitalization().
enum class AsciiCapitalizationType {
  kLower,   // Entirely lowercase
  kUpper,   // Entirely uppercase
  kFirst,   // First letter uppercase
  kMixed,   // Mixed case
  kNoAlpha  // Not an alphabetic string
};

// Prints the name of an enum value.
std::ostream& operator<<(std::ostream& os, const AsciiCapitalizationType& type);

// GetAsciiCapitalization()
//
// Returns a value indicating whether an ASCII string is entirely lowercase,
// entirely uppercase, first letter uppercase, or mixed case, as returned by
// `absl::ascii_islower()` and `absl::ascii_isupper()`.
AsciiCapitalizationType GetAsciiCapitalization(absl::string_view input);

// AsciiCaseInsensitiveCompare()
//
// Performs a case-insensitive absl::string_view comparison.
// Returns:
//    less than 0:    if s1 < s2
//    equal to 0:     if s1 == s2
//    greater than 0: if s1 > s2
int AsciiCaseInsensitiveCompare(absl::string_view s1, absl::string_view s2);

// AsciiCaseInsensitiveLess()
//
// Performs a case-insensitive less-than absl::string_view comparison. This
// function object is useful as a template parameter for set/map of
// absl::string_view-compatible types, if uniqueness of keys is
// case-insensitive.
// Can be used for heterogeneous lookups in associative containers. Example:
//
// absl::btree_map<std::string, std::string, AsciiCaseInsensitiveLess> map;
// absl::string_view key = ...;
// auto it = map.find(key);
struct AsciiCaseInsensitiveLess {
  // Enable heterogeneous lookup.
  using is_transparent = void;
  bool operator()(absl::string_view s1, absl::string_view s2) const {
    return AsciiCaseInsensitiveCompare(s1, s2) < 0;
  }
};

// AsciiCaseInsensitiveHash and AsciiCaseInsensitiveEq
//
// Performs a case-insensitive hash/eq absl::string_view operations. This
// function objects are useful as a template parameter for hash set/map of
// absl::string_view-compatible types, if uniqueness of keys is
// case-insensitive.
// Can be used for heterogeneous lookups in absl associative containers.
// Example:
//
// absl::flat_hash_map<std::string, std::string,
//                     AsciiCaseInsensitiveHash,
//                     AsciiCaseInsensitiveEq>
//     map;
// absl::string_view key = ...;
// auto it = map.find(key);
struct AsciiCaseInsensitiveHash {
  using is_transparent = void;
  size_t operator()(absl::string_view s) const;
};
struct AsciiCaseInsensitiveEq {
  using is_transparent = void;
  bool operator()(absl::string_view s1, absl::string_view s2) const;
};

// MakeAsciiTitlecase()
//
// Capitalizes the first character of each word in a string, using the set of
// characters in `delimiters` to use as word boundaries. This function can be
// implemented using a regular expression, but this version should be more
// efficient.
void MakeAsciiTitlecase(std::string* s, absl::string_view delimiters);

// As above but with string_view as input
std::string MakeAsciiTitlecase(absl::string_view s,
                               absl::string_view delimiters);

}  // namespace strings
#endif  // OR_TOOLS_BASE_CASE_H_
