// Copyright 2010-2013 Google
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
#include "util/time_limit.h"

DEFINE_bool(time_limit_use_usertime, false,
            "If true, rely on the user time in the TimeLimit class. This is "
            "only recommended for benchmarking on a non-isolated environment.");

namespace operations_research {
// static constants.
const double TimeLimit::kSafetyBufferSeconds = 1e-4;
const int TimeLimit::kHistorySize;
}  // namespace operations_research
