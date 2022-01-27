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

#if defined(__GNUC__) && defined(__linux__)
#include <unistd.h>
#endif
#if defined(__APPLE__) && defined(__GNUC__)  // Mac OS X
#include <mach/mach_init.h>
#include <mach/task.h>
#elif defined(__FreeBSD__)  // FreeBSD
#include <sys/resource.h>
#include <sys/time.h>
#elif defined(_MSC_VER)  // WINDOWS
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#endif

#include <cstdio>

#include "ortools/base/sysinfo.h"

namespace operations_research {
// GetProcessMemoryUsage

#if defined(__APPLE__) && defined(__GNUC__)  // Mac OS X
int64_t GetProcessMemoryUsage() {
  task_t task = MACH_PORT_NULL;
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO,
                                (task_info_t)&t_info, &t_info_count)) {
    return -1;
  }
  int64_t resident_memory = t_info.resident_size;
  return resident_memory;
}
#elif defined(__GNUC__) && !defined(__FreeBSD__)  // LINUX
int64_t GetProcessMemoryUsage() {
  unsigned size = 0;
  char buf[30];
  snprintf(buf, sizeof(buf), "/proc/%u/statm", (unsigned)getpid());
  FILE* const pf = fopen(buf, "r");
  if (pf) {
    if (fscanf(pf, "%u", &size) != 1) return 0;
  }
  fclose(pf);
  return size * int64_t{1024};
}
#elif defined(__FreeBSD__)                        // FreeBSD
int64_t GetProcessMemoryUsage() {
  int who = RUSAGE_SELF;
  struct rusage rusage;
  getrusage(who, &rusage);
  return (int64_t)(rusage.ru_maxrss * int64_t{1024});
}
#elif defined(_MSC_VER)                           // WINDOWS
int64_t GetProcessMemoryUsage() {
  HANDLE hProcess;
  PROCESS_MEMORY_COUNTERS pmc;
  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
                         GetCurrentProcessId());
  int64_t memory = 0;
  if (hProcess) {
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
      memory = pmc.WorkingSetSize;
    }
    CloseHandle(hProcess);
  }
  return memory;
}
#else                                             // Unknown, returning 0.
int64_t GetProcessMemoryUsage() { return 0; }
#endif

}  // namespace operations_research
