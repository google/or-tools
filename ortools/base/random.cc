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


#include <cstring>
#include <ctime>
#include <random>

#include "ortools/base/random.h"

namespace operations_research {

uint32_t ACMRandom::Next() {
  return absl::uniform_int_distribution<uint32_t>(0, kuint32max)(generator_);
}

uint32_t ACMRandom::Uniform(uint32_t n) { return n == 0 ? 0 : Next() % n; }

uint64_t ACMRandom::Next64() {
  return absl::uniform_int_distribution<uint64_t>(0, kuint64max)(generator_);
}

uint64_t ACMRandom::operator()(uint64_t val_max) {
  return val_max == 0 ? 0 : Next64() % val_max;
}

int32_t ACMRandom::HostnamePidTimeSeed() {
  return std::random_device{}();
}

int32_t ACMRandom::DeterministicSeed() { return 0; }

}  // namespace operations_research
