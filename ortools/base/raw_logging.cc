// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Author: Maxim Lifantsev
//
// logging_unittest.cc covers the functionality herein
#include "ortools/base/raw_logging.h"

#include <errno.h>
#include <fcntl.h>  // for open()
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "absl/debugging/stacktrace.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"  // To pick up flag settings etc.
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
static CrashReason crash_reason;
static char crash_buf[kLogBufSize + 1] = {0};  // Will end in '\0'

void RawLog__(LogSeverity severity, const char* file, int line,
              const char* format, ...) {
  if (!(absl::GetFlag(FLAGS_logtostderr) ||
        severity >= absl::GetFlag(FLAGS_stderrthreshold) ||
        absl::GetFlag(FLAGS_alsologtostderr) ||
        !IsGoogleLoggingInitialized())) {
    return;  // this stderr log message is suppressed
  }
  // can't call localtime_r here: it can allocate
  char buffer[kLogBufSize];
  char* buf = buffer;
  int size = sizeof(buffer);

  // NOTE: this format should match the specification in base/logging.h
  DoRawLog(&buf, &size, "%c0000 00:00:00.000000 %5u %s:%d] RAW: ",
           LogSeverityNames[severity][0], GetTID(),
           const_basename(const_cast<char*>(file)), line);

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
    if (!sync_val_compare_and_swap(&crashed, false, true)) {
      crash_reason.filename = file;
      crash_reason.line_number = line;
      memcpy(crash_buf, msg_start, msg_size);  // Don't include prefix
      crash_reason.message = crash_buf;
      crash_reason.depth = absl::GetStackTrace(
          crash_reason.stack, ARRAYSIZE(crash_reason.stack), 1);
      SetCrashReason(&crash_reason);
    }
    LogMessage::Fail();  // abort()
  }
}

}  // namespace google
