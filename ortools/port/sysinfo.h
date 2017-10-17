// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_PORT_SYSINFO_H_
#define OR_TOOLS_PORT_SYSINFO_H_

#include "ortools/base/integral_types.h"

namespace operations_research {
namespace sysinfo {

// Return the memory usage in bytes of the process.
// Will return -1 if MemoryUsage is not supported on platform (e.g. Android).
//
// Note: fixing this on Android isn't really feasible, memory usage is only
// available from Java.  So any code depending on this needs to deal with
// the case where memory info is not available.
//
// See base/sysinfo.h MemoryUsage
int64 MemoryUsageProcess();

}  // namespace sysinfo
}  // namespace operations_research

#endif  // OR_TOOLS_PORT_SYSINFO_H_
