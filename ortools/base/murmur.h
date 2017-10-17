// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BASE_MURMUR_H_
#define OR_TOOLS_BASE_MURMUR_H_

#include "ortools/base/thorough_hash.h"

namespace util_hash {
// In the or-tools project, MurmurHash64 is just a redirection towards
// ThoroughHash. Ideally, it is meant to be using the murmurhash
// algorithm described in http://murmurhash.googlepages.com.
inline uint64 MurmurHash64(const char *buf, const size_t len) {
  return ::operations_research::ThoroughHash(buf, len);
}
}

#endif  // OR_TOOLS_BASE_MURMUR_H_
