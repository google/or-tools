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

#ifndef ORTOOLS_BASE_LOGGING_H_
#define ORTOOLS_BASE_LOGGING_H_

#include "absl/base/log_severity.h"    // IWYU pragma: export
#include "absl/base/macros.h"          // IWYU pragma: export
#include "absl/flags/declare.h"        // IWYU pragma: export
#include "absl/flags/flag.h"           // IWYU pragma: export
#include "absl/log/check.h"            // IWYU pragma: export
#include "absl/log/die_if_null.h"      // IWYU pragma: export
#include "absl/log/globals.h"          // IWYU pragma: export
#include "absl/log/log.h"              // IWYU pragma: export
#include "absl/log/vlog_is_on.h"       // IWYU pragma: export
#include "absl/memory/memory.h"        // IWYU pragma: export
#include "absl/status/status.h"        // IWYU pragma: export
#include "absl/strings/str_cat.h"      // IWYU pragma: export
#include "absl/strings/string_view.h"  // IWYU pragma: export
#include "ortools/base/base_export.h"  // IWYU pragma: export

#ifdef NDEBUG
const bool DEBUG_MODE = false;
#else   // NDEBUG
const bool DEBUG_MODE = true;
#endif  // NDEBUG

namespace operations_research {

void FixFlagsAndEnvironmentForSwig();

}  // namespace operations_research

#endif  // ORTOOLS_BASE_LOGGING_H_
