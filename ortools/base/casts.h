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

// This file is provided for compatibility purposes.
//
#ifndef OR_TOOLS_BASE_CASTS_H_
#define OR_TOOLS_BASE_CASTS_H_

#include <cstring>

template <class Dest, class Source>
inline Dest bit_cast(const Source& source) {
  COMPILE_ASSERT(sizeof(Dest) == sizeof(Source),
                 bit_cast_on_object_with_different_sizes);

  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

#endif  // OR_TOOLS_BASE_CASTS_H_
