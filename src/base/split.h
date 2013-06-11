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

#ifndef OR_TOOLS_BASE_SPLIT_H_
#define OR_TOOLS_BASE_SPLIT_H_

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringpiece.h"

namespace operations_research {
// ----------------------------------------------------------------------
// SplitStringUsing()
//    Split a string using a character delimiter. Append the components
//    to 'result'.  If there are consecutive delimiters, this function skips
//    over all of them.
// ----------------------------------------------------------------------
void SplitStringUsing(const std::string& full, const char* delim,
                      std::vector<std::string>* res);

// We define here a very truncated version of the powerful strings::Split()
// function. As of 2013-04, it can only be used like this:
// const char* separators = ...;
// std::vector<string> x = strings::Split(
//     full, strings::delimiter::AnyOf(separators), strings::SkipEmpty());
namespace strings {
// Slightly different API that directly returns the std::vector<string>.
std::vector<std::string> Split(const std::string& full, const char* delim,
                               int flags);
namespace delimiter {
inline const char* AnyOf(const char* x) { return x; }
}  // namespace delimiter

inline int SkipEmpty() { return 0xDEADBEEF; }
}  // namespace strings

}  // namespace operations_research
#endif  // OR_TOOLS_BASE_SPLIT_H_
