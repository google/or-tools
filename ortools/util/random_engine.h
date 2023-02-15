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

// Defines the random engine type to use within operations_research code.

#ifndef OR_TOOLS_UTIL_RANDOM_ENGINE_H_
#define OR_TOOLS_UTIL_RANDOM_ENGINE_H_

#include <random>

namespace operations_research {

using random_engine_t = std::mt19937_64;

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_RANDOM_ENGINE_H_
