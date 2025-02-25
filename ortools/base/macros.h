// Copyright 2010-2025 Google LLC
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

#include "ortools/base/base_export.h"  // IWYU pragma: export

#define COMPILE_ASSERT(x, msg)

#ifdef NDEBUG
const bool DEBUG_MODE = false;
#else   // NDEBUG
const bool DEBUG_MODE = true;
#endif  // NDEBUG

#endif  // OR_TOOLS_BASE_MACROS_H_
