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

#ifndef OR_TOOLS_BASE_COMMANDLINEFLAGS_H_
#define OR_TOOLS_BASE_COMMANDLINEFLAGS_H_

#include "gflags/gflags.h"

namespace absl {

template <class T>
inline void SetFlag(T* flag, const T& value) {
  *flag = value;
}

template <class T, class V>
inline void SetFlag(T* flag, const V& value) {
  *flag = value;
}

template <class T>
inline const T& GetFlag(T* flag) {
  return *flag;
}

template <class T>
inline const T& GetFlag(const T& flag) {
  return flag;
}

}  // namespace absl

#define ABSL_DECLARE_FLAG(t, n) DECLARE_##t(n)
#define ABSL_FLAG(t, n, d, h) DEFINE_##t(n, d, h)

#endif  // OR_TOOLS_BASE_COMMANDLINEFLAGS_H_
