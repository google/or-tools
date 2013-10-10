// Copyright 2010-2013 Google
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
#ifndef OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_
#define OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_

#include "base/integral_types.h"

namespace operations_research {
// ---------- Overflow utility functions ----------

// A note on overflow treatment.
// kint64min and kint64max are treated as infinity.
// Thus if the computation overflows, the result is always kint64m(ax/in).
//
// Note(user): this is actually wrong: when computing A-B, if A is kint64max
// and B is finite, then A-B won't be kint64max: overflows aren't sticky.
// TODO(user): consider making some operations overflow-sticky, some others
// not, but make an explicit choice throughout.

inline bool AddOverflows(int64 left, int64 right) {
  return right > 0 && left > kint64max - right;
}

inline bool AddUnderflows(int64 left, int64 right) {
  return right < 0 && left < kint64min - right;
}

inline int64 CapAdd(int64 left, int64 right) {
  return AddOverflows(left, right)
             ? kint64max
             : (AddUnderflows(left, right) ? kint64min : left + right);
}

uint64 UnsignedCapAdd(uint64 left, uint64 right);

uint64 UnsignedCapSub(uint64 left, uint64 right);

inline bool SubOverflows(int64 left, int64 right) {
  return right < 0 && left > kint64max + right;
}

inline bool SubUnderflows(int64 left, int64 right) {
  return right > 0 && left < kint64min + right;
}

inline int64 CapSub(int64 left, int64 right) {
  return SubOverflows(left, right)
             ? kint64max
             : (SubUnderflows(left, right) ? kint64min : left - right);
}

inline int64 CapOpp(int64 v) {
  // Note: -kint64min != kint64max.
  return v == kint64min ? kint64max : -v;
}

int64 CapProd(int64 left, int64 right);

uint64 UnsignedCapProd(uint64 left, uint64 right);
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_SATURATED_ARITHMETIC_H_
