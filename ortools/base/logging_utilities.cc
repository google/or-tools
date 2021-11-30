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

#include "ortools/base/logging_utilities.h"

#include <signal.h>
#include <stdio.h>
#if !defined(_MSC_VER)
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>  // For geteuid.
#endif

#if defined(_MSC_VER)  // windows compatibility layer.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN /* We always want minimal includes */
#endif

#include <direct.h>  /* for _getcwd() */
#include <io.h>      /* because we so often use open/close/etc */
#include <process.h> /* for _getpid() */
#include <stdarg.h>  /* template_dictionary.cc uses va_copy */
#include <string.h>  /* for _strnicmp(), strerror_s() */
#include <time.h>    /* for localtime_s() */
#include <windows.h>
#include <winsock.h> /* for gethostname */
/* Note: the C++ #includes are all together at the bottom.  This file is
 * used by both C and C++ code, so we put all the C++ together.
 */

/* 4244: otherwise we get problems when subtracting two size_t's to an int
 * 4251: it's complaining about a private struct I've chosen not to dllexport
 * 4355: we use this in a constructor, but we do it safely
 * 4715: for some reason VC++ stopped realizing you can't return after abort()
 * 4800: we know we're casting ints/char*'s to bools, and we're ok with that
 * 4996: Yes, we're ok using "unsafe" functions like fopen() and strerror()
 * 4312: Converting uint32_t to a pointer when testing %p
 * 4267: also subtracting two size_t to int
 * 4722: Destructor never returns due to abort()
 */
#pragma warning(disable : 4244 4251 4355 4715 4800 4996 4267 4312 4722)

/* file I/O */
#define PATH_MAX 1024
#define access _access
#define getcwd _getcwd
#define open _open
#define read _read
#define write _write
#define lseek _lseek
#define close _close
#define popen _popen
#define pclose _pclose
#define R_OK 04 /* read-only (for access()) */
#define S_ISDIR(m) (((m)&_S_IFMT) == _S_IFDIR)

#define O_WRONLY _O_WRONLY
#define O_CREAT _O_CREAT
#define O_EXCL _O_EXCL

#ifndef __MINGW32__
enum { STDIN_FILENO = 0, STDOUT_FILENO = 1, STDERR_FILENO = 2 };
#endif
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE

/* Not quite as lightweight as a hard-link, but more than good enough for us. */
#define link(oldpath, newpath) CopyFileA(oldpath, newpath, false)

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

/* In windows-land, hash<> is called hash_compare<> (from xhash.h) */
/* VC11 provides std::hash */
#if defined(_MSC_VER) && (_MSC_VER < 1700)
#define hash hash_compare
#endif

/* We can't just use _vsnprintf and _snprintf as drop-in-replacements,
 * because they don't always NUL-terminate. :-(  We also can't use the
 * name vsnprintf, since windows defines that (but not snprintf (!)).
 */

// These call the windows _vsnprintf, but always NUL-terminate.
int safe_vsnprintf(char* str, size_t size, const char* format, va_list ap) {
  if (size == 0)  // not even room for a \0?
    return -1;    // not what C99 says to do, but what windows does
  str[size - 1] = '\0';
  return _vsnprintf(str, size - 1, format, ap);
}

#define vsnprintf(str, size, format, ap) safe_vsnprintf(str, size, format, ap)
#ifndef va_copy
#define va_copy(dst, src) (dst) = (src)
#endif

/* Windows doesn't support specifying the number of buckets as a
 * hash_map constructor arg, so we leave this blank.
 */
#define CTEMPLATE_SMALL_HASHTABLE
#define DEFAULT_TEMPLATE_ROOTDIR ".."

// ----------------------------------- SYSTEM/PROCESS
typedef int pid_t;
#define getpid _getpid

// ----------------------------------- THREADS
typedef DWORD pthread_t;
#define pthread_self GetCurrentThreadId

inline struct tm* localtime_r(const time_t* timep, struct tm* result) {
  localtime_s(result, timep);
  return result;
}

inline char* strerror_r(int errnum, char* buf, size_t buflen) {
  strerror_s(buf, buflen, errnum);
  return buf;
}

#endif  // _MSC_VER

namespace google {

static const char* g_program_invocation_short_name = NULL;
static pthread_t g_main_thread_id;

}  // namespace google

// The following APIs are all internal.
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/time/time.h"
#include "ortools/base/commandlineflags.h"

ABSL_FLAG(bool, symbolize_stacktrace, true,
          "Symbolize the stack trace in the tombstone");

namespace google {

typedef void DebugWriter(const char*, void*);

// The %p field width for printf() functions is two characters per byte.
// For some environments, add two extra bytes for the leading "0x".
static const int kPrintfPointerFieldWidth = 2 + 2 * sizeof(void*);

static void DebugWriteToStderr(const char* data, void*) {
  // This one is signal-safe.
  if (write(STDERR_FILENO, data, strlen(data)) < 0) {
    // Ignore errors.
  }
}

static void DebugWriteToString(const char* data, void* arg) {
  reinterpret_cast<std::string*>(arg)->append(data);
}

// Print a program counter and its symbol name.
static void DumpPCAndSymbol(DebugWriter* writerfn, void* arg, void* pc,
                            const char* const prefix) {
  char tmp[1024];
  const char* symbol = "(unknown)";
  // Symbolizes the previous address of pc because pc may be in the
  // next function.  The overrun happens when the function ends with
  // a call to a function annotated noreturn (e.g. CHECK).
  if (absl::Symbolize(reinterpret_cast<char*>(pc) - 1, tmp, sizeof(tmp))) {
    symbol = tmp;
  }
  char buf[1024];
  snprintf(buf, sizeof(buf), "%s@ %*p  %s\n", prefix, kPrintfPointerFieldWidth,
           pc, symbol);
  writerfn(buf, arg);
}

static void DumpPC(DebugWriter* writerfn, void* arg, void* pc,
                   const char* const prefix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "%s@ %*p\n", prefix, kPrintfPointerFieldWidth, pc);
  writerfn(buf, arg);
}

// Dump current stack trace as directed by writerfn
static void DumpStackTrace(int skip_count, DebugWriter* writerfn, void* arg) {
  // Print stack trace
  void* stack[32];
  int depth = absl::GetStackTrace(stack, ABSL_ARRAYSIZE(stack), skip_count + 1);
  for (int i = 0; i < depth; i++) {
    if (absl::GetFlag(FLAGS_symbolize_stacktrace)) {
      DumpPCAndSymbol(writerfn, arg, stack[i], "    ");
    } else {
      DumpPC(writerfn, arg, stack[i], "    ");
    }
  }
}

static void DumpStackTraceAndExit() {
  DumpStackTrace(1, DebugWriteToStderr, NULL);
  abort();
}

namespace logging_internal {

const char* ProgramInvocationShortName() {
  if (g_program_invocation_short_name != NULL) {
    return g_program_invocation_short_name;
  } else {
    // TODO(user): Use /proc/self/cmdline and so?
    return "UNKNOWN";
  }
}

bool IsGoogleLoggingInitialized() {
  return g_program_invocation_short_name != NULL;
}

unsigned int GetTID() {
  return static_cast<unsigned int>(absl::base_internal::GetTID());
}

int64_t CycleClock_Now() { return absl::ToUnixMicros(absl::Now()); }

int64_t UsecToCycles(int64_t usec) { return usec; }

static int32_t g_main_thread_pid = getpid();
int32_t GetMainThreadPid() { return g_main_thread_pid; }

const char* const_basename(const char* filepath) {
  const char* base = strrchr(filepath, '/');
#ifdef _MSC_VER  // Look for either path separator in Windows
  if (!base) base = strrchr(filepath, '\\');
#endif
  return base ? (base + 1) : filepath;
}

static std::string g_my_user_name;  // NOLINT
const std::string& MyUserName() { return g_my_user_name; }
static void MyUserNameInitializer() {
#if defined(_MSC_VER)
  const char* user = getenv("USERNAME");
#else
  const char* user = getenv("USER");
#endif
  if (user != NULL) {
    g_my_user_name = user;
  } else {
#if !defined(_MSC_VER)  // Not windows.
    struct passwd pwd;
    struct passwd* result = NULL;
    char buffer[1024] = {'\0'};
    uid_t uid = geteuid();
    int pwuid_res = getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &result);
    if (pwuid_res == 0) {
      g_my_user_name = pwd.pw_name;
    } else {
      snprintf(buffer, sizeof(buffer), "uid%d", uid);
      g_my_user_name = buffer;
    }
#endif  // _defined(_MSC_VER)
    if (g_my_user_name.empty()) {
      g_my_user_name = "invalid-user";
    }
  }
}

class GoogleInitializer {
 public:
  typedef void (*void_function)(void);
  GoogleInitializer(const char*, void_function f) { f(); }
};

#define REGISTER_MODULE_INITIALIZER(name, body)       \
  namespace {                                         \
  static void google_init_module_##name() { body; }   \
  GoogleInitializer google_initializer_module_##name( \
      #name, google_init_module_##name);              \
  }

REGISTER_MODULE_INITIALIZER(logging_utilities, MyUserNameInitializer());

void DumpStackTraceToString(std::string* stacktrace) {
  DumpStackTrace(1, DebugWriteToString, stacktrace);
}

// We use an atomic operation to prevent problems with calling CrashReason
// from inside the Mutex implementation (potentially through RAW_CHECK).
static const CrashReason* g_reason = 0;

void SetCrashReason(const CrashReason* r) {
  sync_val_compare_and_swap(&g_reason, reinterpret_cast<const CrashReason*>(0),
                            r);
}

void InitGoogleLoggingUtilities(const char* argv0) {
  CHECK(!IsGoogleLoggingInitialized())
      << "You called InitGoogleLogging() twice!";
  const char* slash = strrchr(argv0, '/');
#ifdef _MSC_VER
  if (!slash) slash = strrchr(argv0, '\\');
#endif
  g_program_invocation_short_name = slash ? slash + 1 : argv0;
  g_main_thread_id = pthread_self();

  InstallFailureFunction(&DumpStackTraceAndExit);
}

void ShutdownGoogleLoggingUtilities() {
  CHECK(IsGoogleLoggingInitialized())
      << "You called ShutdownGoogleLogging() without calling "
         "InitGoogleLogging() first!";
  g_program_invocation_short_name = NULL;
#if !defined(_MSC_VER)
  closelog();
#endif  // !defined(_MSC_VER)
}

}  // namespace logging_internal

}  // namespace google
