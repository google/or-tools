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

#ifndef OR_TOOLS_BASE_SYSINFO_H_
#define OR_TOOLS_BASE_SYSINFO_H_

#include "ortools/base/basictypes.h"

namespace operations_research {
// Returns the memory usage of the process.
int64 GetProcessMemoryUsage();
}  // namespace operations_research

inline int64 MemoryUsage(int unused) {
  return operations_research::GetProcessMemoryUsage();
}

#endif  // OR_TOOLS_BASE_SYSINFO_H_
