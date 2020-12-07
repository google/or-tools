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

#ifndef OR_TOOLS_BASE_VLOG_IS_ON_H_
#define OR_TOOLS_BASE_VLOG_IS_ON_H_

#include "ortools/base/integral_types.h"
#include "ortools/base/log_severity.h"

namespace google {

#if defined(__GNUC__DISABLED)
// We emit an anonymous static int* variable at every VLOG_IS_ON(n) site.
// (Normally) the first time every VLOG_IS_ON(n) site is hit,
// we determine what variable will dynamically control logging at this site:
// it's either absl::GetFlag(FLAGS_v) or an appropriate internal variable
// matching the current source file that represents results of
// parsing of --vmodule flag and/or SetVLOGLevel calls.
#define VLOG_IS_ON(verboselevel)                                            \
  __extension__({                                                           \
    static int32* vlocal__ = &google::kLogSiteUninitialized;                \
    int32 verbose_level__ = (verboselevel);                                 \
    (*vlocal__ >= verbose_level__) &&                                       \
        ((vlocal__ != &google::kLogSiteUninitialized) ||                    \
         (google::InitVLOG3__(&vlocal__, &absl::GetFlag(FLAGS_v), __FILE__, \
                              verbose_level__)));                           \
  })
#else
// GNU extensions not available, so we do not support --vmodule.
// Dynamic value of absl::GetFlag(FLAGS_v) always controls the logging level.
#define VLOG_IS_ON(verboselevel) (absl::GetFlag(FLAGS_v) >= (verboselevel))
#endif

// Set VLOG(_IS_ON) level for module_pattern to log_level.
// This lets us dynamically control what is normally set by the --vmodule flag.
// Returns the level that previously applied to module_pattern.
// NOTE: To change the log level for VLOG(_IS_ON) sites
//    that have already executed after/during InitGoogleLogging,
//.   one needs to supply the exact --vmodule pattern that applied to them.
//       (If no --vmodule pattern applied to them
//       the value of FLAGS_v will continue to control them.)
extern GOOGLE_GLOG_DLL_DECL int SetVLOGLevel(const char* module_pattern,
                                             int log_level);

// Various declarations needed for VLOG_IS_ON above: =========================

// Special value used to indicate that a VLOG_IS_ON site has not been
// initialized.  We make this a large value, so the common-case check
// of "*vlocal__ >= verbose_level__" in VLOG_IS_ON definition
// passes in such cases and InitVLOG3__ is then triggered.
extern int32 kLogSiteUninitialized;

// Helper routine which determines the logging info for a particalur VLOG site.
//   site_flag     is the address of the site-local pointer to the controlling
//                 verbosity level
//   site_default  is the default to use for *site_flag
//   fname         is the current source file name
//   verbose_level is the argument to VLOG_IS_ON
// We will return the return value for VLOG_IS_ON
// and if possible set *site_flag appropriately.
extern GOOGLE_GLOG_DLL_DECL bool InitVLOG3__(int32** site_flag,
                                             int32* site_default,
                                             const char* fname,
                                             int32 verbose_level);

}  // namespace google

#endif  // OR_TOOLS_BASE_VLOG_IS_ON_H_
