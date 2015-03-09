// Copyright 2011-2014 Google
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
//
//
//  Several helper functions to generate random numbers.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_RANDOM_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_RANDOM_H

#include "base/random.h"

#include "common/constants.h"

#include "common/common_flags.h"

namespace operations_research {
  
// Random seed generator.
int32 GetSeed() {
  if (FLAGS_deterministic_random_seed) {
    return ACMRandom::DeterministicSeed();
  } else {
    return ACMRandom::HostnamePidTimeSeed();
  }
}

} // namespace opertional_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_RANDOM_H