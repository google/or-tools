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

#ifndef OR_TOOLS_BASE_LOGGING_UTILITIES_H_
#define OR_TOOLS_BASE_LOGGING_UTILITIES_H_

#include <string>

#include "absl/base/internal/sysinfo.h"  // for GetTID().
#include "absl/base/macros.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/logging.h"

namespace google {
namespace logging_internal {

const char* ProgramInvocationShortName();

bool IsGoogleLoggingInitialized();

int64 CycleClock_Now();

int64 UsecToCycles(int64 usec);

int32 GetMainThreadPid();

unsigned int GetTID();

const std::string& MyUserName();

// Get the part of filepath after the last path separator.
// (Doesn't modify filepath, contrary to basename() in libgen.h.)
const char* const_basename(const char* filepath);

// Wrapper of __sync_val_compare_and_swap. If the GCC extension isn't
// defined, we try the CPU specific logics (we only support x86 and
// x86_64 for now) first, then use a naive implementation, which has a
// race condition.
template <typename T>
inline T sync_val_compare_and_swap(T* ptr, T oldval, T newval) {
#if defined(__GNUC__)
  return __sync_val_compare_and_swap(ptr, oldval, newval);
#else
  T ret = *ptr;
  if (ret == oldval) {
    *ptr = newval;
  }
  return ret;
#endif
}

void DumpStackTraceToString(std::string* stacktrace);

struct CrashReason {
  CrashReason() : filename(0), line_number(0), message(0), depth(0) {}

  const char* filename;
  int line_number;
  const char* message;

  // We'll also store a bit of stack trace context at the time of crash as
  // it may not be available later on.
  void* stack[32];
  int depth;
};

void SetCrashReason(const CrashReason* r);

void InitGoogleLoggingUtilities(const char* argv0);
void ShutdownGoogleLoggingUtilities();

}  // namespace logging_internal
}  // namespace google

#endif  // OR_TOOLS_BASE_LOGGING_UTILITIES_H_
