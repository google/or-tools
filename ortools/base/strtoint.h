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

// Architecture-neutral plug compatible replacements for strtol() friends.
//
// Long's have different lengths on ILP-32 and LP-64 platforms, and so overflow
// behavior across the two varies when strtol() and similar are used to parse
// 32-bit integers.  Similar problems exist with atoi(), because although it
// has an all-integer interface, it uses strtol() internally, and so suffers
// from the same narrowing problems on assignments to int.
//
// Examples:
//   errno = 0;
//   i = strtol("3147483647", nullptr, 10);
//   printf("%d, errno %d\n", i, errno);
//   //   32-bit platform: 2147483647, errno 34
//   //   64-bit platform: -1147483649, errno 0
//
//   printf("%d\n", atoi("3147483647"));
//   //   32-bit platform: 2147483647
//   //   64-bit platform: -1147483649
//
// A way round this is to define local replacements for these, and use them
// instead of the standard libc functions.
//
// In most 32-bit cases the replacements can be inlined away to a call to the
// libc function.  In a couple of 64-bit cases, however, adapters are required,
// to provide the right overflow and errno behavior.
//

#ifndef OR_TOOLS_BASE_STRTOINT_H_
#define OR_TOOLS_BASE_STRTOINT_H_

#include <cstdint>
#include <string>

#include "absl/base/port.h"  // disable some warnings on Windows
#include "absl/strings/string_view.h"
#include "ortools/base/integral_types.h"

namespace operations_research {

int32_t strtoint32(absl::string_view word);
int64_t strtoint64(absl::string_view word);

// Convenience versions of the above that take a string argument.
inline int32_t atoi32(const std::string& word) { return strtoint32(word); }
inline int64_t atoi64(const std::string& word) { return strtoint64(word); }

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_STRTOINT_H_
