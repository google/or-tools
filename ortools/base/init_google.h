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

#ifndef OR_TOOLS_BASE_INIT_GOOGLE_H_
#define OR_TOOLS_BASE_INIT_GOOGLE_H_

#include "absl/flags/declare.h"  // IWYU pragma: keep
#include "absl/flags/flag.h"     // IWYU pragma: keep
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/initialize.h"
#include "absl/strings/string_view.h"

namespace google {

inline void InitGoogleLogging(absl::string_view usage) {
  absl::InitializeLog();
  if (!usage.empty()) {
    absl::SetProgramUsageMessage(usage);
  }
}

inline void ShutdownGoogleLogging() {}  // No op.

}  // namespace google

// Initializes misc google-related things in the binary.
//
// Typically called early on in main() and must be called before other
// threads start using functions from this file.
//
// 'usage' provides a short usage message passed to
//         absl::SetProgramUsageMessage().
//         Most callers provide the name of the app as 'usage' ?!
// 'argc' and 'argv' are the command line flags to parse. There is no
//         requirement for an element (*argv)[*argc] to exist or to have
//         any particular value, unlike the similar array that is passed
//         to the `main` function.
inline void InitGoogle(absl::string_view usage, int* argc, char*** argv,
                       bool deprecated) {
  google::InitGoogleLogging(usage);
  absl::ParseCommandLine(*argc, *argv);
}

inline void InitGoogle(const char* usage, int* argc, char*** argv,
                       bool deprecated) {
  InitGoogle(absl::NullSafeStringView(usage), argc, argv, deprecated);
}

#endif  // OR_TOOLS_BASE_INIT_GOOGLE_H_
