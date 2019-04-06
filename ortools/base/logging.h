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

#ifndef OR_TOOLS_BASE_LOGGING_H_
#define OR_TOOLS_BASE_LOGGING_H_

#include <cassert>

#if defined(_MSC_VER)
#define GLOG_NO_ABBREVIATED_SEVERITIES
#endif

#include "glog/logging.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"

#define QCHECK CHECK
#define ABSL_DIE_IF_NULL CHECK_NOTNULL

// used by or-tools non C++ ports to bridge with the C++ layer.
void FixFlagsAndEnvironmentForSwig();

#endif  // OR_TOOLS_BASE_LOGGING_H_
