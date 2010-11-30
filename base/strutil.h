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

// A string-like object that points to a sized piece of memory.
//
// Functions or methods may use const StringPiece& parameters to accept either
// a "const char*" or a "string" value that will be implicitly converted to
// a StringPiece.  The implicit conversion means that it is often appropriate
// to include this .h file in other files rather than forward-declaring
// StringPiece as would be appropriate for most other Google classes.
//
// Systematic usage of StringPiece is encouraged as it will reduce unnecessary
// conversions from "const char*" to "string" and back again.

#ifndef BASE_STRUTIL_H
#define BASE_STRUTIL_H

#include "base/stringpiece.h"

namespace operations_research {
// ----------------------------------------------------------------------
// HasSuffixString()
//    Return true if str ends in suffix.
// ----------------------------------------------------------------------

inline bool HasSuffixString(const StringPiece& str,
                            const StringPiece& suffix) {
  return str.ends_with(suffix);
}
}  // namespace operations_research
#endif  // BASE_STRUTIL_H
