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

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_TESTING_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_TESTING_H_

#include <cstdint>
#include <random>
#include <vector>

#include "ortools/math_opt/elemental/attr_key.h"
namespace operations_research::math_opt {

// Creates `num_keys` random `AttrKey<n>`s. Each element is in the range
// `[0, id_bound)`.
template <int n, typename Symmetry>
std::vector<AttrKey<n, Symmetry>> MakeRandomAttrKeys(const int num_keys,
                                                     const int64_t id_bound) {
  std::minstd_rand rng;
  rng.seed(1234);  // For reproducibility.
  std::uniform_int_distribution<int64_t> rand(0, id_bound - 1);
  std::vector<AttrKey<n, Symmetry>> keys;
  keys.reserve(num_keys);
  for (int i = 0; i < num_keys; ++i) {
    if constexpr (n == 0) {
      keys.emplace_back();
    } else if constexpr (n == 1) {
      keys.emplace_back(rand(rng));
    } else if constexpr (n == 2) {
      keys.emplace_back(rand(rng), rand(rng));
    }
  }
  return keys;
}

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_TESTING_H_
