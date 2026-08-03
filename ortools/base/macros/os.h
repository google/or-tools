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

// Refer to README.md to understand how to use these macros safely.
#ifndef ORTOOLS_BASE_MACROS_OS_H_
#define ORTOOLS_BASE_MACROS_OS_H_

#if (defined(__freebsd__) || defined(__FreeBSD__))
#define ORTOOLS_TARGET_OS_IS_FREEBSD
#endif

#if defined(__OpenBSD__)
#define ORTOOLS_TARGET_OS_IS_OPENBSD
#endif

#if defined(__NetBSD__)
#define ORTOOLS_TARGET_OS_IS_NETBSD
#endif

#if defined(ORTOOLS_TARGET_OS_IS_FREEBSD) || \
    defined(ORTOOLS_TARGET_OS_IS_OPENBSD) || \
    defined(ORTOOLS_TARGET_OS_IS_NETBSD)
#define ORTOOLS_TARGET_OS_IS_ANY_BSD
#endif

#if defined(__ANDROID__)
#define ORTOOLS_TARGET_OS_IS_ANDROID
#endif

#if defined(__linux__) && !defined(ORTOOLS_TARGET_OS_IS_ANY_BSD) && \
    !defined(ORTOOLS_TARGET_OS_IS_ANDROID)
#define ORTOOLS_TARGET_OS_IS_LINUX
#endif

#if (defined(_WIN64) || defined(_WIN32))
#define ORTOOLS_TARGET_OS_IS_WINDOWS
#endif

#if (defined(__apple__) || defined(__APPLE__) || defined(__MACH__))
// From https://stackoverflow.com/a/49560690
#include "TargetConditionals.h"
#if TARGET_OS_OSX
#define ORTOOLS_TARGET_OS_IS_MACOS
#endif
#if TARGET_OS_IPHONE
// This is set for any non-Mac Apple products (IOS, TV, WATCH)
#define ORTOOLS_TARGET_OS_IS_IPHONE
#endif
#endif

#if defined(__EMSCRIPTEN__)
#define ORTOOLS_TARGET_OS_IS_EMSCRIPTEN
#endif

namespace operations_research {

enum class TargetOs {
  kAndroid,
  kEmscripten,
  kFreeBsd,
  kIPhone,
  kLinux,
  kMacOS,
  kNetBsd,
  kOpenBsd,
  kWindows,
};

#if defined(ORTOOLS_TARGET_OS_IS_FREEBSD)
inline constexpr TargetOs kTargetOs = TargetOs::kFreeBsd;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_OPENBSD)
inline constexpr TargetOs kTargetOs = TargetOs::kOpenBsd;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_NETBSD)
inline constexpr TargetOs kTargetOs = TargetOs::kNetBsd;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_ANDROID)
inline constexpr TargetOs kTargetOs = TargetOs::kAndroid;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_LINUX)
inline constexpr TargetOs kTargetOs = TargetOs::kLinux;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_WINDOWS)
inline constexpr TargetOs kTargetOs = TargetOs::kWindows;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_MACOS)
inline constexpr TargetOs kTargetOs = TargetOs::kMacOS;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_IPHONE)
inline constexpr TargetOs kTargetOs = TargetOs::kIPhone;
#endif
#if defined(ORTOOLS_TARGET_OS_IS_EMSCRIPTEN)
inline constexpr TargetOs kTargetOs = TargetOs::kEmscripten;
#endif

}  // namespace operations_research

#endif  // ORTOOLS_BASE_MACROS_OS_H_
