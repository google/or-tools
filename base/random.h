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

#ifndef BASE_RANDOM_H_
#define BASE_RANDOM_H_

#include "base/util.h"

namespace operations_research {

// ACM minimal standard random number generator.  (re-entrant.)
class ACMRandom {
 public:
  explicit ACMRandom(int32 seed) : seed_(seed) {}
  int32 Next();
  int32 Uniform(int32 max_value);
  int64 Next64();
  float RndFloat() {
    return Next() * 0.000000000465661273646;  // x: x * (M-1) = 1 - eps
  }

  void Reset(int32 seed) { seed_ = seed; }
  static int32 HostnamePidTimeSeed();
  static int32 DeterministicSeed();

 private:
  int32 seed_;
};

}  // namespace operations_research

#endif  // BASE_RANDOM_H_
