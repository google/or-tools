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

#if defined(__GNUC__)
#include <unistd.h>
#if defined(__linux__)
#include <linux/limits.h>
#endif
#endif
#if defined(_MSC_VER)
#include <Winsock2.h>
#include <windows.h>
#define PATH_MAX 4096
#else
#include <sys/time.h>
#endif

#include <cstring>
#include <ctime>

#include "ortools/base/hash.h"
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

namespace {
static inline uint32_t Word32At(const char* ptr) {
  return ((static_cast<uint32_t>(ptr[0])) +
          (static_cast<uint32_t>(ptr[1]) << 8) +
          (static_cast<uint32_t>(ptr[2]) << 16) +
          (static_cast<uint32_t>(ptr[3]) << 24));
}
}  // namespace

int32_t ACMRandom::HostnamePidTimeSeed() {
  char name[PATH_MAX + 20];  // need 12 bytes for 3 'empty' uint32_t's
  assert(sizeof(name) - PATH_MAX > sizeof(uint32_t) * 3);

  if (gethostname(name, PATH_MAX) != 0) {
    strcpy(name, "default-hostname");  // NOLINT
  }
  const int namelen = strlen(name);
  for (size_t i = 0; i < sizeof(uint32_t) * 3; ++i) {
    name[namelen + i] = '\0';  // so we mix 0's once we get to end-of-string
  }
#if defined(__GNUC__)
  uint32_t a = getpid();
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint32_t b = static_cast<uint32_t>((tv.tv_sec + tv.tv_usec) & 0xffffffff);
#elif defined(_MSC_VER)
  uint32_t a = GetCurrentProcessId();
  uint32_t b = GetTickCount();
#else  // Do not know what to do, returning 0.
  return 0;
#endif
  uint32_t c = 0;
  for (int i = 0; i < namelen; i += sizeof(uint32_t) * 3) {
    a += Word32At(name + i);
    b += Word32At(name + i + sizeof(uint32_t));
    c += Word32At(name + i + sizeof(uint32_t) + sizeof(uint32_t));
    mix(a, b, c);
  }
  c += namelen;  // one final mix
  mix(a, b, c);
  return static_cast<int32_t>(c);  // I guess the seed can be negative
}

int32_t ACMRandom::DeterministicSeed() { return 0; }

}  // namespace operations_research
