// Copyright 2010 Google
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

#include "base/stringpiece.h"
#include "base/util.h"

namespace operations_research {
string StrCat(const StringPiece& p1, const StringPiece& p2);
string StrCat(const StringPiece& p1,
              const StringPiece& p2,
              const StringPiece& p3);
string StrCat(const StringPiece& p1,
              const StringPiece& p2,
              const StringPiece& p3,
              const StringPiece& p4);
string StrCat(int64 a1, const StringPiece& p2);
string StrCat(const StringPiece& p1, int64 a2);
}  // namespace operations_research
#endif  // OR_TOOLS_BASE_JOIN_H_
