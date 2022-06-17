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

#ifndef OR_TOOLS_BASE_MACROS_H_
#define OR_TOOLS_BASE_MACROS_H_

#include <cstdlib>  // for size_t.

#define COMPILE_ASSERT(x, msg)

#ifdef NDEBUG
const bool DEBUG_MODE = false;
#else   // NDEBUG
const bool DEBUG_MODE = true;
#endif  // NDEBUG

// DISALLOW_COPY_AND_ASSIGN disallows the copy and operator= functions.
// It goes in the private: declarations in a class.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  TypeName& operator=(const TypeName&) = delete

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#ifndef COMPILER_MSVC
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

#endif  // OR_TOOLS_BASE_MACROS_H_
