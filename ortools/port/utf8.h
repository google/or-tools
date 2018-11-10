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

#ifndef OR_TOOLS_PORT_UTF8_H_
#define OR_TOOLS_PORT_UTF8_H_

#include <string>

#ifndef __PORTABLE_PLATFORM__
#include "ortools/base/encodingutils.h"
#endif

namespace operations_research {
namespace utf8 {

// str_type should be std::string/StringPiece/Cord
template <typename StrType>
int UTF8StrLen(StrType str_type) {
#if defined(__PORTABLE_PLATFORM__)
  return str_type.size();
#else
  return ::EncodingUtils::UTF8StrLen(str_type);
#endif
}

}  // namespace utf8
}  // namespace operations_research

#endif  // OR_TOOLS_PORT_UTF8_H_
