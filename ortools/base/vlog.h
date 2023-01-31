// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_BASE_VLOG_H_
#define OR_TOOLS_BASE_VLOG_H_

#include <errno.h>
#include <string.h>
#include <time.h>

#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <ostream>
#include <sstream>
#include <string>
#if defined(__GNUC__) && defined(__linux__)
#include <unistd.h>
#endif
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/vlog_is_on.h"

// Log only in verbose mode.

#define VLOG(verboselevel) LOG_IF(INFO, VLOG_IS_ON(verboselevel))

#define VLOG_EVERY_N(verboselevel, n) \
  LOG_IF_EVERY_N(INFO, VLOG_IS_ON(verboselevel), n)

#if defined(DEBUG_MODE)
#define DVLOG(verboselevel) VLOG(verboselevel)
#else  // !defined(DEBUG_MODE)
#define DVLOG(verboselevel) LOG_IF(INFO, false)
#endif  // !defined(DEBUG_MODE)

#endif  // OR_TOOLS_BASE_VLOG_H_
