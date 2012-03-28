// Copyright 2010-2012 Google
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

#include "base/basictypes.h"
#include "base/stringpiece.h"
#include "base/stringprintf.h"

namespace operations_research {
// ----- StrCat -----

std::string StrCat(const StringPiece& p1, const StringPiece& p2) {
  std::string result = p1.ToString();
  result += p2.ToString();
  return result;
}

std::string StrCat(const StringPiece& p1,
                   const StringPiece& p2,
                   const StringPiece& p3) {
  std::string result = p1.ToString();
  result += p2.ToString();
  result += p3.ToString();
  return result;
}

std::string StrCat(const StringPiece& p1,
                   const StringPiece& p2,
                   const StringPiece& p3,
                   const StringPiece& p4) {
  std::string result = p1.ToString();
  result += p2.ToString();
  result += p3.ToString();
  result += p4.ToString();
  return result;
}

std::string StrCat(const StringPiece& p1,
                   const StringPiece& p2,
                   const StringPiece& p3,
                   const StringPiece& p4,
                   const StringPiece& p5) {
  std::string result = p1.ToString();
  result += p2.ToString();
  result += p3.ToString();
  result += p4.ToString();
  result += p5.ToString();
  return result;
}

std::string StrCat(int64 a1, const StringPiece& p2) {
  return StringPrintf("%lld%s", a1, p2.ToString().c_str());
}

std::string StrCat(const StringPiece& p1, int64 a2) {
  return StringPrintf("%s%lld", p1.ToString().c_str(), a2);
}

}  // namespace operations_research
