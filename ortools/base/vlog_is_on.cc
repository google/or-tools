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

#include "ortools/base/vlog_is_on.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <cstdio>
#include <string>

#include "absl/synchronization/mutex.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/logging.h"
#include "ortools/base/logging_utilities.h"
#include "ortools/base/raw_logging.h"

// glog doesn't have annotation
#define ANNOTATE_BENIGN_RACE(address, description)

using std::string;

ABSL_FLAG(int, v, 0,
          "Show all VLOG(m) messages for m <= this."
          " Overridable by --vmodule.");

ABSL_FLAG(std::string, vmodule, "",
          "per-module verbose level."
          " Argument is a comma-separated list of <module name>=<log level>."
          " <module name> is a glob pattern, matched against the filename base"
          " (that is, name ignoring .cc/.h./-inl.h)."
          " <log level> overrides any value given by --v.");

namespace google {

namespace logging_internal {

// Used by logging_unittests.cc so can't make it static here.
GOOGLE_GLOG_DLL_DECL bool SafeFNMatch_(const char* pattern, size_t patt_len,
                                       const char* str, size_t str_len);

// Implementation of fnmatch that does not need 0-termination
// of arguments and does not allocate any memory,
// but we only support "*" and "?" wildcards, not the "[...]" patterns.
// It's not a static function for the unittest.
GOOGLE_GLOG_DLL_DECL bool SafeFNMatch_(const char* pattern, size_t patt_len,
                                       const char* str, size_t str_len) {
  size_t p = 0;
  size_t s = 0;
  while (1) {
    if (p == patt_len && s == str_len) return true;
    if (p == patt_len) return false;
    if (s == str_len) return p + 1 == patt_len && pattern[p] == '*';
    if (pattern[p] == str[s] || pattern[p] == '?') {
      p += 1;
      s += 1;
      continue;
    }
    if (pattern[p] == '*') {
      if (p + 1 == patt_len) return true;
      do {
        if (SafeFNMatch_(pattern + (p + 1), patt_len - (p + 1), str + s,
                         str_len - s)) {
          return true;
        }
        s += 1;
      } while (s != str_len);
      return false;
    }
    return false;
  }
}

}  // namespace logging_internal

using logging_internal::SafeFNMatch_;

int32_t kLogSiteUninitialized = 1000;

// List of per-module log levels from absl::GetFlag(FLAGS_vmodule).
// Once created each element is never deleted/modified
// except for the vlog_level: other threads will read VModuleInfo blobs
// w/o locks and we'll store pointers to vlog_level at VLOG locations
// that will never go away.
// We can't use an STL struct here as we wouldn't know
// when it's safe to delete/update it: other threads need to use it w/o locks.
struct VModuleInfo {
  string module_pattern;
  mutable int32_t vlog_level;  // Conceptually this is an AtomicWord, but it's
                               // too much work to use AtomicWord type here
                               // w/o much actual benefit.
  const VModuleInfo* next;
};

// This protects the following global variables.
static absl::Mutex vmodule_lock;
// Pointer to head of the VModuleInfo list.
// It's a map from module pattern to logging level for those module(s).
static VModuleInfo* vmodule_list = 0;
// Boolean initialization flag.
static bool inited_vmodule = false;

// L >= vmodule_lock.
static void VLOG2Initializer() {
  vmodule_lock.AssertHeld();
  // Can now parse --vmodule flag and initialize mapping of module-specific
  // logging levels.
  inited_vmodule = false;
  const char* vmodule = absl::GetFlag(FLAGS_vmodule).c_str();
  const char* sep;
  VModuleInfo* head = NULL;
  VModuleInfo* tail = NULL;
  while ((sep = strchr(vmodule, '=')) != NULL) {
    string pattern(vmodule, sep - vmodule);
    int module_level;
    if (sscanf(sep, "=%d", &module_level) == 1) {
      VModuleInfo* info = new VModuleInfo;
      info->module_pattern = pattern;
      info->vlog_level = module_level;
      if (head)
        tail->next = info;
      else
        head = info;
      tail = info;
    }
    // Skip past this entry
    vmodule = strchr(sep, ',');
    if (vmodule == NULL) break;
    vmodule++;  // Skip past ","
  }
  if (head) {  // Put them into the list at the head:
    tail->next = vmodule_list;
    vmodule_list = head;
  }
  inited_vmodule = true;
}

// This can be called very early, so we use SpinLock and RAW_VLOG here.
int SetVLOGLevel(const char* module_pattern, int log_level) {
  int result = absl::GetFlag(FLAGS_v);
  int const pattern_len = strlen(module_pattern);
  bool found = false;
  {
    absl::MutexLock l(&vmodule_lock);  // protect whole read-modify-write
    for (const VModuleInfo* info = vmodule_list; info != NULL;
         info = info->next) {
      if (info->module_pattern == module_pattern) {
        if (!found) {
          result = info->vlog_level;
          found = true;
        }
        info->vlog_level = log_level;
      } else if (!found && SafeFNMatch_(info->module_pattern.c_str(),
                                        info->module_pattern.size(),
                                        module_pattern, pattern_len)) {
        result = info->vlog_level;
        found = true;
      }
    }
    if (!found) {
      VModuleInfo* info = new VModuleInfo;
      info->module_pattern = module_pattern;
      info->vlog_level = log_level;
      info->next = vmodule_list;
      vmodule_list = info;
    }
  }
  RAW_VLOG(1, "Set VLOG level for \"%s\" to %d", module_pattern, log_level);
  return result;
}

// NOTE: Individual VLOG statements cache the integer log level pointers.
// NOTE: This function must not allocate memory or require any locks.
bool InitVLOG3__(int32_t** site_flag, int32_t* site_default, const char* fname,
                 int32_t verbose_level) {
  absl::MutexLock l(&vmodule_lock);
  bool read_vmodule_flag = inited_vmodule;
  if (!read_vmodule_flag) {
    VLOG2Initializer();
  }

  // protect the errno global in case someone writes:
  // VLOG(..) << "The last error was " << strerror(errno)
  int old_errno = errno;

  // site_default normally points to absl::GetFlag(FLAGS_v)
  int32_t* site_flag_value = site_default;

  // Get basename for file
  const char* base = strrchr(fname, '/');
  base = base ? (base + 1) : fname;
  const char* base_end = strchr(base, '.');
  size_t base_length = base_end ? size_t(base_end - base) : strlen(base);

  // Trim out trailing "-inl" if any
  if (base_length >= 4 && (memcmp(base + base_length - 4, "-inl", 4) == 0)) {
    base_length -= 4;
  }

  // TODO: Trim out _unittest suffix?  Perhaps it is better to have
  // the extra control and just leave it there.

  // find target in vector of modules, replace site_flag_value with
  // a module-specific verbose level, if any.
  for (const VModuleInfo* info = vmodule_list; info != NULL;
       info = info->next) {
    if (SafeFNMatch_(info->module_pattern.c_str(), info->module_pattern.size(),
                     base, base_length)) {
      site_flag_value = &info->vlog_level;
      // value at info->vlog_level is now what controls
      // the VLOG at the caller site forever
      break;
    }
  }

  // Cache the vlog value pointer if --vmodule flag has been parsed.
  ANNOTATE_BENIGN_RACE(site_flag,
                       "*site_flag may be written by several threads,"
                       " but the value will be the same");
  if (read_vmodule_flag) *site_flag = site_flag_value;

  // restore the errno in case something recoverable went wrong during
  // the initialization of the VLOG mechanism (see above note "protect the..")
  errno = old_errno;
  return *site_flag_value >= verbose_level;
}

}  // namespace google
