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
#ifndef ORTOOLS_BASE_MACROS_BUILDENV_H_
#define ORTOOLS_BASE_MACROS_BUILDENV_H_

namespace operations_research {

// If OR-Tools is built with Google's internal toolchain, `__GOOGLE_PLATFORM__`
// will be defined. In this case, we define `ORTOOLS_BUILDENV_GOOGLE_INTERNAL`
// to delimit Google-internal code that should be stripped out when preparing
// the open-source release.
#if defined(__GOOGLE_PLATFORM__)
#define ORTOOLS_BUILDENV_GOOGLE_INTERNAL
inline constexpr bool kBuildEnvGoogleInternal = true;
#else
inline constexpr bool kBuildEnvGoogleInternal = false;
#endif

}  // namespace operations_research

#endif  // ORTOOLS_BASE_MACROS_BUILDENV_H_
