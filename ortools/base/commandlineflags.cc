// Copyright 2010-2021 Google LLC
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

#include "ortools/base/commandlineflags.h"

// We add this method to tell the linker to include these symbols.
void CommandLineFlagsUnusedMethod() {
  const char kUsage[] = "Unused";
  absl::SetProgramUsageMessage(kUsage);
  int argc = 0;
  char** argv = nullptr;
  absl::ParseCommandLine(argc, argv);
}
