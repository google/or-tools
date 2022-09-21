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

#ifndef OR_TOOLS_BASE_INIT_GOOGLE_H_
#define OR_TOOLS_BASE_INIT_GOOGLE_H_

#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"

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
void InitGoogle(const char* usage, int* argc, char*** argv, bool deprecated) {
  google::InitGoogleLogging(usage);
  absl::ParseCommandLine(*argc, *argv);
}

#endif  // OR_TOOLS_BASE_INIT_GOOGLE_H_
