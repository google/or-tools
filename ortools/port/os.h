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

// Defines macros and constexpr about target OS features.
#ifndef ORTOOLS_PORT_OS_H_
#define ORTOOLS_PORT_OS_H_

namespace operations_research {

#if defined(__ANDROID__)
#define ORTOOLS_TARGET_OS_IS_ANDROID
#endif

#if (defined(__apple__) || defined(__APPLE__) || defined(__MACH__))
// Temporarily close the `namespace operations_research` to include a file.
}  // namespace operations_research
// From https://stackoverflow.com/a/49560690
#include "TargetConditionals.h"
// Reopen the namespace.
namespace operations_research {
#if TARGET_OS_IPHONE == 1
#define ORTOOLS_TARGET_OS_IS_IOS
#endif
#endif

#if defined(__EMSCRIPTEN__)
#define ORTOOLS_TARGET_OS_IS_EMSCRIPTEN
inline constexpr bool kTargetOsIsEmscripten = true;
#else   // __EMSCRIPTEN__
inline constexpr bool kTargetOsIsEmscripten = false;
#endif  // __EMSCRIPTEN__

#if defined(ORTOOLS_TARGET_OS_IS_ANDROID) || \
    defined(ORTOOLS_TARGET_OS_IS_IOS) ||     \
    defined(ORTOOLS_TARGET_OS_IS_EMSCRIPTEN)
#define ORTOOLS_TARGET_OS_SUPPORTS_THREADS 0
#else
#define ORTOOLS_TARGET_OS_SUPPORTS_THREADS 1
#endif

#ifdef __PORTABLE_PLATFORM__
#define ORTOOLS_TARGET_OS_IS_PORTABLE_PLATFORM
inline constexpr bool kTargetOsIsPortablePlatform = true;
#else   // __PORTABLE_PLATFORM__
inline constexpr bool kTargetOsIsPortablePlatform = false;
#endif  // __PORTABLE_PLATFORM__

}  // namespace operations_research

#endif  // ORTOOLS_PORT_OS_H_
