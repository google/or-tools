// Copyright 2010-2014 Google
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

#include <string>
#include "ortools/base/basictypes.h"

namespace operations_research {

// ACM minimal standard random number generator.  (Re-entrant.)
class ACMRandom {
 public:
  explicit ACMRandom(int32 seed) : seed_(seed) {}

  int32 Next();

  // Returns a random value in [0..n-1]. If n == 0, always returns 0.
  int32 Uniform(int32 n);

  int64 Next64();

  float RndFloat() {
    return Next() * 0.000000000465661273646;  // x: x * (M-1) = 1 - eps
  }

  // Returns a double in [0, 1).
  double RndDouble() {
    // Android does not provide ieee754.h and the associated types.
    union {
      double d;
      int64 i;
    } ieee_double;
    ieee_double.i = Next64();
    ieee_double.i &= ~(1LL << 63);  // Clear sign bit.
    // The returned number will be between 0 and 1. Take into account the
    // exponent offset.
    ieee_double.i |= (1023LL << 52);
    return ieee_double.d - static_cast<double>(1.0);
  }

  double RandDouble() { return RndDouble(); }

  double UniformDouble(double x) { return RandDouble() * x; }

  // Returns a double in [a, b). The distribution is uniform.
  double UniformDouble(double a, double b) { return a + (b - a) * RndDouble(); }

  // Returns true with probability 1/n. If n=0, always returns true.
  bool OneIn(int n) { return Uniform(n) == 0; }

  void Reset(int32 seed) { seed_ = seed; }
  static int32 HostnamePidTimeSeed();
  static int32 DeterministicSeed();

// RandomNumberGenerator concept. Example:
//   ACMRandom rand(my_seed);
//   std::random_shuffle(myvec.begin(), myvec.end(), rand);
#if defined(_MSC_VER)
  typedef __int64 difference_type;  // NOLINT
#else
  typedef long long difference_type;  // NOLINT
#endif
  int64 operator()(int64 val_max) { return Next64() % val_max; }

 private:
  int32 seed_;
};

// This is meant to become an implementation of (or a wrapper around)
// http://www.cplusplus.com/reference/random/mt19937, but for now it is just
// using ACMRandom.
class MTRandom : public ACMRandom {
 public:
  explicit MTRandom(int32 seed) : ACMRandom(seed) {}
  // MTRandom also supports a std::string seed.
  explicit MTRandom(const std::string& str_seed)
      : ACMRandom(GenerateInt32SeedFromString(str_seed)) {}

  MTRandom() : ACMRandom(ACMRandom::HostnamePidTimeSeed()) {}
  uint64 Rand64() { return static_cast<uint64>(Next64()); }

 private:
  int32 GenerateInt32SeedFromString(const std::string& str) {
    uint32 seed = 1234567;
    for (size_t i = 0; i < str.size(); ++i) {
      seed *= 1000003;  // prime
      seed += static_cast<uint32>(str[i]);
    }
    return seed >> 1;  // Will fit into an int32.
  }
};

typedef ACMRandom RandomBase;

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_RANDOM_H_
