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

#ifndef OR_TOOLS_BASE_RANDOM_H_
#define OR_TOOLS_BASE_RANDOM_H_

#include <random>
#include <string>

#include "absl/random/random.h"
#include "ortools/base/basictypes.h"

namespace operations_research {

// A wrapper around std::mt19937. Called ACMRandom for historical reasons.
class ACMRandom {
 public:
  explicit ACMRandom(int32_t seed) : generator_(seed) {}

  // Unbounded generators.
  uint32_t Next();
  uint64_t Next64();
  uint64_t Rand64() { return Next64(); }
  uint64_t operator()() { return Next64(); }

  // Bounded generators.
  uint32_t Uniform(uint32_t n);
  uint64_t operator()(uint64_t val_max);

  // Seed management.
  void Reset(int32_t seed) { generator_.seed(seed); }
  static int32_t HostnamePidTimeSeed();
  static int32_t DeterministicSeed();

  // C++11 goodies.
  typedef int64_t difference_type;
  typedef uint64_t result_type;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return kuint32max; }

 private:
  std::mt19937 generator_;
};

class MTRandom : public ACMRandom {
 public:
  explicit MTRandom(int32_t seed) : ACMRandom(seed) {}
  // MTRandom also supports a std::string seed.
  explicit MTRandom(const std::string& str_seed)
      : ACMRandom(GenerateInt32SeedFromString(str_seed)) {}

  MTRandom() : ACMRandom(ACMRandom::HostnamePidTimeSeed()) {}

 private:
  int32_t GenerateInt32SeedFromString(const std::string& str) {
    uint32_t seed = 1234567;
    for (size_t i = 0; i < str.size(); ++i) {
      seed *= 1000003;  // prime
      seed += static_cast<uint32_t>(str[i]);
    }
    return seed >> 1;  // Will fit into an int32_t.
  }
};

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_RANDOM_H_
