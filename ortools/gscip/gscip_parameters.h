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

#ifndef OR_TOOLS_GSCIP_GSCIP_PARAMETERS_H_
#define OR_TOOLS_GSCIP_GSCIP_PARAMETERS_H_

#include "absl/time/time.h"
#include "ortools/gscip/gscip.pb.h"

namespace operations_research {

void SetTimeLimit(absl::Duration time_limit, GScipParameters* parameters);
absl::Duration TimeLimit(const GScipParameters& parameters);
bool TimeLimitSet(const GScipParameters& parameters);

// CHECK fails if num_threads < 1.
void SetMaxNumThreads(int num_threads, GScipParameters* parameters);

// Returns 1 if the number of threads it not specified.
int MaxNumThreads(const GScipParameters& parameters);
bool MaxNumThreadsSet(const GScipParameters& parameters);

// log_level must be in [0, 5], where 0 is none, 5 is most verbose, and the
// default is 4. CHECK fails on bad log_level. Default level displays standard
// search logs.
void SetLogLevel(GScipParameters* parameters, int log_level);
int LogLevel(const GScipParameters& parameters);
bool LogLevelSet(const GScipParameters& parameters);

// Sets the log level to 4 if enabled, 0 if disabled (see above).
void SetOutputEnabled(GScipParameters* parameters, bool output_enabled);
// Checks if the log level is equal to zero.
bool OutputEnabled(const GScipParameters& parameters);
bool OutputEnabledSet(const GScipParameters& parameters);

// Sets an initial seed (shift) for all pseudo-random number generators used
// within SCIP. Valid values are [0:INT_MAX] i.e. [0:2^31-1]. If an invalid
// value is passed, 0 would be stored instead.
void SetRandomSeed(GScipParameters* parameters, int random_seed);
// Returns -1 if unset.
int RandomSeed(const GScipParameters& parameters);
bool RandomSeedSet(const GScipParameters& parameters);

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_PARAMETERS_H_
