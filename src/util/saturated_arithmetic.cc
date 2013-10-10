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
#include "util/saturated_arithmetic.h"

namespace operations_research {
// ---------- Overflow utility functions ----------

uint64 UnsignedCapAdd(uint64 left, uint64 right) {
  const uint64 sum = left + right;
  return (sum < left) || (sum < right) ? kint64max : sum;
}

uint64 UnsignedCapSub(uint64 left, uint64 right) {
  return left >= right ? left - right : 0;
}

int64 CapProd(int64 left, int64 right) {
  if (left == 0 || right == 0) {
    return 0;
  }
  if (left > 0) {   /* left is positive */
    if (right > 0) {/* left and right are positive */
      if (left > (kint64max / right)) {
        return kint64max;
      }
    } else {/* left positive, right non-positive */
      if (right < (kint64min / left)) {
        return kint64min;
      }
    }               /* left positive, right non-positive */
  } else {          /* left is non-positive */
    if (right > 0) {/* left is non-positive, right is positive */
      if (left < (kint64min / right)) {
        return kint64min;
      }
    } else {/* left and right are non-positive */
      if (right < (kint64max / left)) {
        return kint64max;
      }
    } /* end if left and right are non-positive */
  }   /* end if left is non-positive */
  return left * right;
}

uint64 UnsignedCapProd(uint64 left, uint64 right) {
  if (left == 0 || right == 0) {
    return 0;
  }
  if (left > (kuint64max / right)) {
    return kuint64max;
  }
  return left * right;
}
}  // namespace operations_research
