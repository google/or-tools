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

#ifndef ORTOOLS_BASE_MACROS_OS_SUPPORT_H_
#define ORTOOLS_BASE_MACROS_OS_SUPPORT_H_

#include "ortools/base/macros/os.h"

namespace operations_research {

// Proto descriptor support.
#if defined(ORTOOLS_TARGET_OS_IS_ANDROID) || \
    defined(ORTOOLS_TARGET_OS_IS_IPHONE) ||  \
    defined(ORTOOLS_TARGET_OS_IS_EMSCRIPTEN)
static_assert(kTargetOs == TargetOs::kAndroid ||
              kTargetOs == TargetOs::kEmscripten ||
              kTargetOs == TargetOs::kIPhone);
inline constexpr bool kTargetOsSupportsProtoDescriptor = false;
#else
static_assert(kTargetOs != TargetOs::kAndroid &&
              kTargetOs != TargetOs::kEmscripten &&
              kTargetOs != TargetOs::kIPhone);
#define ORTOOLS_TARGET_OS_SUPPORTS_PROTO_DESCRIPTOR
inline constexpr bool kTargetOsSupportsProtoDescriptor = true;
#endif

// File support.
#if defined(ORTOOLS_TARGET_OS_IS_ANDROID) || \
    defined(ORTOOLS_TARGET_OS_IS_IPHONE) ||  \
    defined(ORTOOLS_TARGET_OS_IS_EMSCRIPTEN)
static_assert(kTargetOs == TargetOs::kAndroid ||
              kTargetOs == TargetOs::kEmscripten ||
              kTargetOs == TargetOs::kIPhone);
inline constexpr bool kTargetOsSupportsFile = false;
#else
static_assert(kTargetOs != TargetOs::kAndroid &&
              kTargetOs != TargetOs::kEmscripten &&
              kTargetOs != TargetOs::kIPhone);
#define ORTOOLS_TARGET_OS_SUPPORTS_FILE
inline constexpr bool kTargetOsSupportsFile = true;
#endif

// Thread support.
#if defined(ORTOOLS_TARGET_OS_IS_ANDROID) || \
    defined(ORTOOLS_TARGET_OS_IS_IPHONE) ||  \
    defined(ORTOOLS_TARGET_OS_IS_EMSCRIPTEN)
static_assert(kTargetOs == TargetOs::kAndroid ||
              kTargetOs == TargetOs::kEmscripten ||
              kTargetOs == TargetOs::kIPhone);
inline constexpr bool kTargetOsSupportsThreads = false;
#else
static_assert(kTargetOs != TargetOs::kAndroid &&
              kTargetOs != TargetOs::kEmscripten &&
              kTargetOs != TargetOs::kIPhone);
#define ORTOOLS_TARGET_OS_SUPPORTS_THREADS
inline constexpr bool kTargetOsSupportsThreads = true;
#endif

}  // namespace operations_research

#endif  // ORTOOLS_BASE_MACROS_OS_SUPPORT_H_
