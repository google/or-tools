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

#ifndef OR_TOOLS_GSCIP_GSCIP_PARAMETERS_H_
#define OR_TOOLS_GSCIP_GSCIP_PARAMETERS_H_

#include "absl/time/time.h"
#include "ortools/gscip/gscip.pb.h"

namespace operations_research {

void GScipSetTimeLimit(absl::Duration time_limit, GScipParameters* parameters);
absl::Duration GScipTimeLimit(const GScipParameters& parameters);
bool GScipTimeLimitSet(const GScipParameters& parameters);

// CHECK fails if num_threads < 1.
void GScipSetMaxNumThreads(int num_threads, GScipParameters* parameters);

// Returns 1 if the number of threads it not specified.
int GScipMaxNumThreads(const GScipParameters& parameters);
bool GScipMaxNumThreadsSet(const GScipParameters& parameters);

// log_level must be in [0, 5], where 0 is none, 5 is most verbose, and the
// default is 4. CHECK fails on bad log_level. Default level displays standard
// search logs.
void GScipSetLogLevel(GScipParameters* parameters, int log_level);
int GScipLogLevel(const GScipParameters& parameters);
bool GScipLogLevelSet(const GScipParameters& parameters);

// Sets the log level to 4 if enabled, 0 if disabled (see above).
void GScipSetOutputEnabled(GScipParameters* parameters, bool output_enabled);
// Checks if the log level is equal to zero.
bool GScipOutputEnabled(const GScipParameters& parameters);
bool GScipOutputEnabledSet(const GScipParameters& parameters);

// Sets an initial seed (shift) for all pseudo-random number generators used
// within SCIP. Valid values are [0:INT_MAX] i.e. [0:2^31-1]. If an invalid
// value is passed, 0 would be stored instead.
void GScipSetRandomSeed(GScipParameters* parameters, int random_seed);
// Returns -1 if unset.
int GScipRandomSeed(const GScipParameters& parameters);
bool GScipRandomSeedSet(const GScipParameters& parameters);

// Sets the misc/catchctrlc property.
void GScipSetCatchCtrlC(bool catch_ctrl_c, GScipParameters* parameters);
// Returns the misc/catchctrlc property; true if not set (the default SCIP
// behavior).
bool GScipCatchCtrlC(const GScipParameters& parameters);
// Returns true when the misc/catchctrlc property is set.
bool GScipCatchCtrlCSet(const GScipParameters& parameters);

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_PARAMETERS_H_
