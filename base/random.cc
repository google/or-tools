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

namespace {
inline uint32 Word32At(const void *p) {
  uint32 t;
  memcpy(&t, p, sizeof(t));
  return t;
}
}  // namespace

#if defined(__GNUC__) && defined(__linux__)
#include <linux/limits.h>
#endif

int32 ACMRandom::HostnamePidTimeSeed() {
#if defined(__GNUC__)
  char name[PATH_MAX + 20];      // need 12 bytes for 3 'empty' uint32's
  assert(sizeof(name) - PATH_MAX > sizeof(uint32) * 3);

  if (gethostname(name, PATH_MAX) != 0) {
    return 0;
  }
  const int namelen = strlen(name);
  for (int i = 0; i < sizeof(uint32) * 3; ++i) {
    name[namelen + i] = '\0';   // so we mix 0's once we get to end-of-string
  }

  uint32 a = getpid();
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint32 b = ((tv.tv_sec + tv.tv_usec) & GG_ULONGLONG(0xffffffff));
  uint32 c = 0;
  for (int i = 0; i < namelen; i += sizeof(uint32) * 3) {
    a += Word32At(name + i);
    b += Word32At(name + i + sizeof(uint32));
    c += Word32At(name + i + sizeof(uint32) + sizeof(uint32));
    mix(a,b,c);
  }
  c += namelen;                      // one final mix
  mix(a,b,c);
  return static_cast<int32>(c);      // I guess the seed can be negative
#elif defined(_MSC_VER)
  uint64 a = GetTickCount();
  uint64 b = GG_ULONGLONG(0xe08c1d668b756f82);
  uint64 c = GetCurrentProcessId();
  mix(a, b, c);
  return c;
#else
  return 0;
#endif
}

int32 ACMRandom::DeterministicSeed() { return 0; }

}  // namespace operations_research
