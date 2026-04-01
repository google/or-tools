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

#include "ortools/port/sysinfo.h"

#include <cstdint>
#include <fstream>
#include <optional>

#include "ortools/base/macros/os.h"

#if defined(ORTOOLS_TARGET_OS_IS_LINUX)
#include <unistd.h>
#endif

#if defined(ORTOOLS_TARGET_OS_IS_MACOS)
#include <mach/mach_init.h>
#include <mach/task.h>
#endif

#if defined(ORTOOLS_TARGET_OS_IS_ANY_BSD)
#include <sys/resource.h>
#include <sys/time.h>
#endif

#if defined(ORTOOLS_TARGET_OS_IS_WINDOWS)
#include <windows.h>
// windows.h must be included first.
#include <psapi.h>
#endif

namespace operations_research {
namespace sysinfo {

std::optional<uint64_t> MemoryUsageProcess() {
#if defined(ORTOOLS_TARGET_OS_IS_LINUX)
  // /proc/self/statm contains the memory usage of the current process.
  // https://man7.org/linux/man-pages/man5/proc_pid_statm.5.html
  std::ifstream stream("/proc/self/statm");
  if (!stream.is_open()) {
    return std::nullopt;
  }
  uint64_t total_program_size, resident_set_size;
  stream >> total_program_size >> resident_set_size;
  if (stream.fail()) {
    return std::nullopt;
  }
  return resident_set_size * getpagesize();
#elif defined(ORTOOLS_TARGET_OS_IS_MACOS)
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO,
                                (task_info_t)&t_info, &t_info_count)) {
    return std::nullopt;
  }
  return t_info.resident_size;
#elif defined(ORTOOLS_TARGET_OS_IS_WINDOWS)
  // https://docs.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-PROCESS_MEMORY_COUNTERS
  PROCESS_MEMORY_COUNTERS counters;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
    return std::nullopt;
  }
  return counters.WorkingSetSize;
#elif defined(ORTOOLS_TARGET_OS_IS_ANY_BSD)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return std::nullopt;
  }
  constexpr uint64_t kKiloByte = 1024;
  return static_cast<uint64_t>(usage.ru_maxrss) * kKiloByte;
#else
  return std::nullopt;
#endif
}

}  // namespace sysinfo
}  // namespace operations_research
