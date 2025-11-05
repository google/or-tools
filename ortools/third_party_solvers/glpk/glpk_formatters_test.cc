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

#include "ortools/third_party_solvers/glpk/glpk_formatters.h"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

// All ASCII control characters (except '\0').
std::vector<char> AllControlCharacters() {
  std::vector<char> characters;
  for (int i = 1; i < 0x100; ++i) {
    const char c = static_cast<char>(i);
    if (absl::ascii_iscntrl(c)) {
      characters.push_back(c);
    }
  }
  return characters;
}

// All characters that are not ASCII control characters (i.e. does not include
// 0).
std::vector<char> AllNoncontrolCharacters() {
  std::vector<char> characters;
  for (int i = 1; i < 0x100; ++i) {
    const char c = static_cast<char>(i);
    if (!absl::ascii_iscntrl(c)) {
      characters.push_back(c);
    }
  }
  return characters;
}

TEST(TruncateAndQuoteGLPKNameTest, ShortNameWithValidCharacters) {
  std::ostringstream oss;
  for (const char c : AllNoncontrolCharacters()) {
    // The quoting characters is quoted itself.
    if (c != '\\') {
      oss << c;
    }
  }
  ASSERT_LE(oss.str().size(), kMaxGLPKNameLen);
  EXPECT_EQ(TruncateAndQuoteGLPKName(oss.str()), oss.str());
}

TEST(TruncateAndQuoteGLPKNameTest, QuoteCharacter) {
  {
    constexpr std::string_view kPrefix = "prefix";
    constexpr std::string_view kSuffix = "suffix";
    EXPECT_EQ(TruncateAndQuoteGLPKName(absl::StrCat(kPrefix, "\\", kSuffix)),
              absl::StrCat(kPrefix, "\\\\", kSuffix));
  }

  {
    const std::string kLongSuffix(2 * kMaxGLPKNameLen, '-');
    const std::string kExpectedTruncatedLongSuffix(
        kMaxGLPKNameLen - 2 /* "\\\\" */, '-');
    EXPECT_EQ(TruncateAndQuoteGLPKName(absl::StrCat("\\", kLongSuffix)),
              absl::StrCat("\\\\", kExpectedTruncatedLongSuffix));
  }

  {
    const std::string kLongPrefix(kMaxGLPKNameLen - 1 /* size("\\\\") - 1 */,
                                  '-');
    EXPECT_EQ(TruncateAndQuoteGLPKName(absl::StrCat(kLongPrefix, "\\")),
              kLongPrefix);
  }
}

TEST(TruncateAndQuoteGLPKNameTest, ControlCharactersAreQuoted) {
  constexpr std::string_view kPrefix = "prefix";
  constexpr std::string_view kSuffix = "suffix";

  // Suffix and expected suffix for the test of truncation.
  const std::string kLongSuffix(2 * kMaxGLPKNameLen, '-');
  const std::string kExpectedTruncatedLongSuffix(
      kMaxGLPKNameLen - 4 /* "\xHH" */, '-');

  // Prefix for the test that partial escape sequences are not included.
  const std::string kLongPrefix(kMaxGLPKNameLen - 3 /* size("\xHH") - 1 */,
                                '-');

  for (const char c : AllControlCharacters()) {
    {
      // StrCat does not allow `char` parameters.
      const std::string name =
          absl::StrCat(kPrefix, std::string(1, c), kSuffix);
      const std::string expected =
          absl::StrCat(kPrefix, "\\x", absl::Hex(c, absl::kZeroPad2), kSuffix);
      EXPECT_EQ(TruncateAndQuoteGLPKName(name), expected) << name;
    }

    // Test that we still truncate the string taking into account the control
    // characters escaping.
    {
      const std::string name = absl::StrCat(std::string(1, c), kLongSuffix);
      const std::string expected = absl::StrCat(
          "\\x", absl::Hex(c, absl::kZeroPad2), kExpectedTruncatedLongSuffix);
      EXPECT_EQ(TruncateAndQuoteGLPKName(name), expected) << name;
    }

    // Test that if there is not enough place in the string we don't include an
    // escape character.
    {
      const std::string name = absl::StrCat(kLongPrefix, std::string(1, c));
      EXPECT_EQ(TruncateAndQuoteGLPKName(name), kLongPrefix) << name;
    }
  }
}

}  // namespace
}  // namespace operations_research
