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

#ifndef OR_TOOLS_BASE_INTEGRAL_TYPES_H_
#define OR_TOOLS_BASE_INTEGRAL_TYPES_H_

#include <cinttypes>
#include <cstdint>
#include <iostream>  // NOLINT

// Detect 64 bit.
#undef ARCH_K8
#if defined(_MSC_VER) && defined(_WIN64)
#define ARCH_K8
#elif defined(__APPLE__) && defined(__GNUC__)
#define ARCH_K8  // We only support 64 bit on Mac OS X.
#elif defined(__GNUC__) && defined(__LP64__) && !defined(__aarch64__)
#define ARCH_K8  // Linux x86_64
#endif

typedef signed char int8;
typedef short int16;  // NOLINT
typedef int int32;
typedef int64_t int64;

typedef unsigned char uint8;
typedef unsigned short uint16;  // NOLINT
typedef unsigned int uint32;
typedef uint64_t uint64;

static const uint8 kuint8max = UINT8_MAX;
static const uint16 kuint16max = UINT16_MAX;
static const uint32 kuint32max = UINT32_MAX;
static const uint64 kuint64max = UINT64_MAX;

static const int8 kint8min = INT8_MIN;
static const int8 kint8max = INT8_MAX;
static const int16 kint16min = INT16_MIN;
static const int16 kint16max = INT16_MAX;
static const int32 kint32min = INT32_MIN;
static const int32 kint32max = INT32_MAX;
static const int64 kint64min = INT64_MIN;
static const int64 kint64max = INT64_MAX;

#ifdef STLPORT
#include <cstdio>
// int64 output not present in STL port.
inline std::ostream& operator<<(std::ostream& os, int64 i) {
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%" PRId64, i);
  os << buffer;
  return os;
}

inline std::ostream& operator<<(std::ostream& os, uint64 i) {
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%llu", i);
  os << buffer;
  return os;
}
#endif  // STLPORT

#endif  // OR_TOOLS_BASE_INTEGRAL_TYPES_H_
