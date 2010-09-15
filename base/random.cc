// Copyright 2010 Google
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

#include "base/random.h"

namespace operations_research {

int32 ACMRandom::Next() {
  const int32 M = 2147483647L;   // 2^31-1
  const int32 A = 16807;
  // In effect, we are computing seed_ = (seed_ * A) % M, where M = 2^31-1
  uint32 lo = A * static_cast<int32>(seed_ & 0xFFFF);
  uint32 hi = A * static_cast<int32>(static_cast<uint32>(seed_) >> 16);
  lo += (hi & 0x7FFF) << 16;
  if (lo > M) {
    lo &= M;
    ++lo;
  }
  lo += hi >> 15;
  if (lo > M) {
    lo &= M;
    ++lo;
  }
  return (seed_ = static_cast<int32>(lo));
}

int32 ACMRandom::Uniform(int32 n) {
  return Next() % n;
}

int64 ACMRandom::Next64() {
  const int64 next = Next();
  return (next - 1) * 2147483646L + Next();
}

int32 ACMRandom::HostnamePidTimeSeed() {
  return 0;
}

int32 ACMRandom::DeterministicSeed() { return 0; }

}  // namespace operations_research
