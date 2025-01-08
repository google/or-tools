// Copyright 2010-2024 Google LLC
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

#include "ortools/base/constant_divisor.h"

#include <cstdint>
#include <limits>

#include "absl/log/check.h"
#include "absl/numeric/int128.h"

namespace util {
namespace math {

// Fast div/mod implementation based on
// "Faster Remainder by Direct Computation: Applications to Compilers and
// Software Libraries" Daniel Lemire, Owen Kaser, Nathan Kurz arXiv:1902.01961
ConstantDivisor<uint64_t>::ConstantDivisor(value_type d)
    : ConstantDivisorBase((absl::Uint128Max() / d) + 1, d) {
  CHECK_GT(d, 1) << "ConstantDivisor<uint64_t> only supports denominators > 1.";
}

// If we hardcode shift_amount to 32, the 32-bit formula is:
// magic_number = 2 ^ 64 / d
// value / d = value * magic_number >> 64
//
// One caveat is that for d == 1, magic_number takes 65 bits overflowing a
// uint64_t.  So, we again disallow inputs with d == 1.
ConstantDivisor<uint32_t>::ConstantDivisor(value_type d)
    : ConstantDivisorBase((std::numeric_limits<MagicValueType>::max() / d) + 1,
                          d) {
  CHECK_GT(d, 1) << "ConstantDivisor<uint32_t> only supports denominators > 1.";
}

ConstantDivisor<uint16_t>::ConstantDivisor(value_type d)
    : ConstantDivisorBase((MagicValueType{1} << kShift) / d + 1, d) {
  CHECK_GT(d, 0);
}

ConstantDivisor<uint8_t>::ConstantDivisor(value_type d)
    : ConstantDivisorBase((MagicValueType{1} << kShift) / d + 1, d) {
  CHECK_GT(d, 0);
}

}  // namespace math
}  // namespace util
