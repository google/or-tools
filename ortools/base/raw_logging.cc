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

#include "ortools/base/raw_logging.h"

#include <errno.h>
#include <fcntl.h>  // for open()
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "absl/base/macros.h"
#include "absl/debugging/stacktrace.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/logging_utilities.h"

#if !defined(_MSC_VER)
#include <sys/syscall.h>  // for syscall()
#define safe_write(fd, s, len) syscall(SYS_write, fd, s, len)
#else
#include <io.h>  // _write()
// Not so safe, but what can you do?
#define safe_write(fd, s, len) _write(fd, s, len)
#endif

#if defined(_MSC_VER) && !defined(__MINGW32__)
enum { STDIN_FILENO = 0, STDOUT_FILENO = 1, STDERR_FILENO = 2 };
#endif

namespace google {

// CAVEAT: vsnprintf called from *DoRawLog below has some (exotic) code paths
// that invoke malloc() and getenv() that might acquire some locks.
// If this becomes a problem we should reimplement a subset of vsnprintf
// that does not need locks and malloc.

// Helper for RawLog__ below.
// *DoRawLog writes to *buf of *size and move them past the written portion.
// It returns true iff there was no overflow or error.
static bool DoRawLog(char** buf, int* size, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(*buf, *size, format, ap);
  va_end(ap);
  if (n < 0 || n > *size) return false;
  *size -= n;
  *buf += n;
  return true;
}

// Helper for RawLog__ below.
inline static bool VADoRawLog(char** buf, int* size, const char* format,
                              va_list ap) {
  int n = vsnprintf(*buf, *size, format, ap);
  if (n < 0 || n > *size) return false;
  *size -= n;
  *buf += n;
  return true;
}

static const int kLogBufSize = 3000;
static bool crashed = false;
static logging_internal::CrashReason crash_reason;
static char crash_buf[kLogBufSize + 1] = {0};  // Will end in '\0'

void RawLog__(LogSeverity severity, const char* file, int line,
              const char* format, ...) {
  if (!(absl::GetFlag(FLAGS_logtostderr) ||
        severity >= absl::GetFlag(FLAGS_stderrthreshold) ||
        absl::GetFlag(FLAGS_alsologtostderr) ||
        !logging_internal::IsGoogleLoggingInitialized())) {
    return;  // this stderr log message is suppressed
  }
  // can't call localtime_r here: it can allocate
  char buffer[kLogBufSize];
  char* buf = buffer;
  int size = sizeof(buffer);

  // NOTE: this format should match the specification in base/logging.h
  DoRawLog(&buf, &size, "%c0000 00:00:00.000000 %5u %s:%d] RAW: ",
           LogSeverityNames[severity][0], logging_internal::GetTID(),
           logging_internal::const_basename(const_cast<char*>(file)), line);

  // Record the position and size of the buffer after the prefix
  const char* msg_start = buf;
  const int msg_size = size;

  va_list ap;
  va_start(ap, format);
  bool no_chop = VADoRawLog(&buf, &size, format, ap);
  va_end(ap);
  if (no_chop) {
    DoRawLog(&buf, &size, "\n");
  } else {
    DoRawLog(&buf, &size, "RAW_LOG ERROR: The Message was too long!\n");
  }
  // We make a raw syscall to write directly to the stderr file descriptor,
  // avoiding FILE buffering (to avoid invoking malloc()), and bypassing
  // libc (to side-step any libc interception).
  // We write just once to avoid races with other invocations of RawLog__.
  safe_write(STDERR_FILENO, buffer, strlen(buffer));
  if (severity == GLOG_FATAL) {
    if (!logging_internal::sync_val_compare_and_swap(&crashed, false, true)) {
      crash_reason.filename = file;
      crash_reason.line_number = line;
      memcpy(crash_buf, msg_start, msg_size);  // Don't include prefix
      crash_reason.message = crash_buf;
      crash_reason.depth = absl::GetStackTrace(
          crash_reason.stack, ABSL_ARRAYSIZE(crash_reason.stack), 1);
      SetCrashReason(&crash_reason);
    }
    LogMessage::Fail();  // abort()
  }
}

}  // namespace google
