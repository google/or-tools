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

#ifndef OR_TOOLS_BASE_JOIN_H_
#define OR_TOOLS_BASE_JOIN_H_

#include <string>
#include <sstream>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/stringpiece.h"

// A buffer size large enough for all FastToBuffer functions.
const int kFastToBufferSize = 32;

// Writes output to the beginning of the given buffer. Returns a pointer to the
// end of the std::string (i.e. to the NUL char). Buffer must be at least 12 bytes.
// Not actually fast, but maybe someday!
template <class T>
char* NumToBuffer(T i, char* buffer) {
  std::stringstream ss;
  ss << i;
  const std::string s = ss.str();
  strcpy(buffer, s.c_str());  // NOLINT
  return buffer + s.size();
}

struct AlphaNum {
  StringPiece piece;
  char digits[kFastToBufferSize];

  // No bool ctor -- bools convert to an integral type.
  // A bool ctor would also convert incoming pointers (bletch).

  AlphaNum(int32 i32)  // NOLINT(runtime/explicit)
      : piece(digits, NumToBuffer(i32, digits) - &digits[0]) {}
  AlphaNum(uint32 u32)  // NOLINT(runtime/explicit)
      : piece(digits, NumToBuffer(u32, digits) - &digits[0]) {}
  AlphaNum(long l)  // NOLINT
      : piece(digits, NumToBuffer(l, digits) - &digits[0]) {}
  AlphaNum(unsigned long ul)  // NOLINT
      : piece(digits, NumToBuffer(ul, digits) - &digits[0]) {}
  AlphaNum(int64 i64)  // NOLINT(runtime/explicit)
      : piece(digits, NumToBuffer(i64, digits) - &digits[0]) {}
  AlphaNum(uint64 u64)  // NOLINT(runtime/explicit)
      : piece(digits, NumToBuffer(u64, digits) - &digits[0]) {}
  AlphaNum(float f)  // NOLINT(runtime/explicit)
      : piece(digits, strlen(NumToBuffer(f, digits))) {}
  AlphaNum(double f)  // NOLINT(runtime/explicit)
      : piece(digits, strlen(NumToBuffer(f, digits))) {}
  AlphaNum(const char* c_str) : piece(c_str) {}   // NOLINT(runtime/explicit)
  AlphaNum(const StringPiece& pc) : piece(pc) {}  // NOLINT(runtime/explicit)
  AlphaNum(const std::string& s) : piece(s) {}         // NOLINT(runtime/explicit)

  StringPiece::size_type size() const { return piece.size(); }
  const char* data() const { return piece.data(); }

 private:
  // Use ":" not ':'
  AlphaNum(char c);  // NOLINT(runtime/explicit)
};

extern AlphaNum gEmptyAlphaNum;

std::string StrCat(const AlphaNum& a);
std::string StrCat(const AlphaNum& a, const AlphaNum& b);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h, const AlphaNum& i);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h, const AlphaNum& i,
              const AlphaNum& j);
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h, const AlphaNum& i,
              const AlphaNum& j, const AlphaNum& k);

void StrAppend(std::string* s, const AlphaNum& a);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h,
               const AlphaNum& i);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h,
               const AlphaNum& i, const AlphaNum& j);
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h,
               const AlphaNum& i, const AlphaNum& j, const AlphaNum& k);

namespace strings {
template <class Iterable>
std::string Join(const Iterable& elements, const std::string& separator) {
  std::string out;
  for (const auto& e : elements) {
    if (!out.empty()) out += separator;
    StrAppend(&out, e);
  }
  return out;
}
}  // namespace strings

#endif  // OR_TOOLS_BASE_JOIN_H_
