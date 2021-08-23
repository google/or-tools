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

#include "ortools/gscip/gscip_parameters.h"

#include "ortools/base/logging.h"

namespace operations_research {

// NOTE(user): the open source build for proto is less accepting of
// absl::string_view than internally, so we do more conversions than would
// appear necessary.
namespace {
constexpr absl::string_view kLimitsTime = "limits/time";
constexpr absl::string_view kParallelMaxNThreads = "parallel/maxnthreads";
constexpr absl::string_view kDisplayVerbLevel = "display/verblevel";
constexpr absl::string_view kRandomSeedParam = "randomization/randomseedshift";
constexpr absl::string_view kCatchCtrlCParam = "misc/catchctrlc";
}  // namespace

void GScipSetTimeLimit(absl::Duration time_limit, GScipParameters* parameters) {
  if (time_limit < absl::Seconds(1e20) && time_limit > absl::Duration()) {
    (*parameters->mutable_real_params())[std::string(kLimitsTime)] =
        absl::ToDoubleSeconds(time_limit);
  } else {
    parameters->mutable_real_params()->erase(std::string(kLimitsTime));
  }
}

absl::Duration GScipTimeLimit(const GScipParameters& parameters) {
  if (parameters.real_params().contains(std::string(kLimitsTime))) {
    const double scip_limit =
        parameters.real_params().at(std::string(kLimitsTime));
    if (scip_limit >= 1e20) {
      return absl::InfiniteDuration();
    } else if (scip_limit <= 0.0) {
      return absl::Duration();
    } else {
      return absl::Seconds(scip_limit);
    }
  }
  return absl::InfiniteDuration();
}

bool GScipTimeLimitSet(const GScipParameters& parameters) {
  return parameters.real_params().contains(std::string(kLimitsTime));
}

void GScipSetMaxNumThreads(int num_threads, GScipParameters* parameters) {
  CHECK_GE(num_threads, 1);
  (*parameters->mutable_int_params())[std::string(kParallelMaxNThreads)] =
      num_threads;
}

int GScipMaxNumThreads(const GScipParameters& parameters) {
  if (parameters.int_params().contains(std::string(kParallelMaxNThreads))) {
    return parameters.int_params().at(std::string(kParallelMaxNThreads));
  }
  return 1;
}

bool GScipMaxNumThreadsSet(const GScipParameters& parameters) {
  return parameters.int_params().contains(std::string(kParallelMaxNThreads));
}

void GScipSetLogLevel(GScipParameters* parameters, int log_level) {
  CHECK_GE(log_level, 0);
  CHECK_LE(log_level, 5);
  (*parameters->mutable_int_params())[std::string(kDisplayVerbLevel)] =
      log_level;
}

int GScipLogLevel(const GScipParameters& parameters) {
  return parameters.int_params().contains(std::string(kDisplayVerbLevel))
             ? parameters.int_params().at(std::string(kDisplayVerbLevel))
             : 4;
}
bool GScipLogLevelSet(const GScipParameters& parameters) {
  return parameters.int_params().contains(std::string(kDisplayVerbLevel));
}

void GScipSetOutputEnabled(GScipParameters* parameters, bool output_enabled) {
  if (output_enabled) {
    parameters->mutable_int_params()->erase(std::string(kDisplayVerbLevel));
  } else {
    (*parameters->mutable_int_params())[std::string(kDisplayVerbLevel)] = 0;
  }
}
bool GScipOutputEnabled(const GScipParameters& parameters) {
  return !parameters.int_params().contains(std::string(kDisplayVerbLevel)) ||
         (parameters.int_params().at(std::string(kDisplayVerbLevel)) > 0);
}

bool GScipOutputEnabledSet(const GScipParameters& parameters) {
  return GScipLogLevelSet(parameters);
}

void GScipSetRandomSeed(GScipParameters* parameters, int random_seed) {
  random_seed = std::max(0, random_seed);
  (*parameters->mutable_int_params())[std::string(kRandomSeedParam)] =
      random_seed;
}

int GScipRandomSeed(const GScipParameters& parameters) {
  if (GScipRandomSeedSet(parameters)) {
    return parameters.int_params().at(std::string(kRandomSeedParam));
  }
  return -1;  // Unset value.
}

bool GScipRandomSeedSet(const GScipParameters& parameters) {
  return parameters.int_params().contains(std::string(kRandomSeedParam));
}

void GScipSetCatchCtrlC(const bool catch_ctrl_c,
                        GScipParameters* const parameters) {
  (*parameters->mutable_bool_params())[std::string(kCatchCtrlCParam)] =
      catch_ctrl_c;
}

bool GScipCatchCtrlC(const GScipParameters& parameters) {
  if (GScipCatchCtrlCSet(parameters)) {
    return parameters.bool_params().at(std::string(kCatchCtrlCParam));
  }
  return true;
}

bool GScipCatchCtrlCSet(const GScipParameters& parameters) {
  return parameters.bool_params().contains(std::string(kCatchCtrlCParam));
}

}  // namespace operations_research
