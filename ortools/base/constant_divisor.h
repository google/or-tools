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

#ifndef OR_TOOLS_BASE_CONSTANT_DIVISOR_H_
#define OR_TOOLS_BASE_CONSTANT_DIVISOR_H_

// Provides faster division in situations where the same divisor is used
// repeatedly but is not known at compile time. For example, a hash table might
// not be sized until the model is loaded, but once loaded it is not resized for
// the life of the model. As this is not a compile time constant, the compiler
// can not optimize away the division.
//
// However the cost of precomputing coefficients when the hash table is sized
// can be dwarfed by the cycles saved avoiding hardware division on every
// lookup. See benchmark section below to estimate the breakeven point. This
// reduces the CPU penalty of non-power of two sized hash tables, bloom filters,
// etc.
//
//
// The following template and specializations are defined in this file:
//  - ConstantDivisor<T>
//  - ConstantDivisor<uint8_t>
//  - ConstantDivisor<uint16_t>
//  - ConstantDivisor<uint32_t> (Only supports denominators > 1)
//  - ConstantDivisor<uint64_t> (Only supports denominators > 1)
//
// Fast div/mod implementation based on
// "Faster Remainder by Direct Computation: Applications to Compilers and
// Software Libraries" Daniel Lemire, Owen Kaser, Nathan Kurz arXiv:1902.01961
//
// Usage:
//   uint64_t n;  // Not known at compile time!
//   ConstantDivisor<uint64_t> divisor(n);
//   uint64_t m = ...;
//   EXPECT_EQ(m / n, divisor.div(m));
//   EXPECT_EQ(m % n, divisor.mod(m));
//

#include <cstdint>

#include "absl/numeric/int128.h"

namespace util {
namespace math {

template <typename T>
class ConstantDivisor {
 public:
  typedef T value_type;

  explicit ConstantDivisor(value_type denominator)
      : denominator_(denominator) {}

  value_type div(value_type n) const { return n / denominator_; }

  value_type mod(value_type n) const { return n % denominator_; }

  friend value_type operator/(value_type a, const ConstantDivisor& b) {
    return b.div(a);
  }

  friend value_type operator%(value_type a, const ConstantDivisor& b) {
    return b.mod(a);
  }

 private:
  value_type denominator_;
};

namespace internal {

// Common code for all specializations.
template <typename T, typename MagicT, typename Impl>
class ConstantDivisorBase {
 public:
  using value_type = T;

  explicit ConstantDivisorBase(MagicT magic, value_type denominator)
      : magic_(magic), denominator_(denominator) {}

  value_type mod(value_type numerator) const {
    return numerator -
           static_cast<const Impl*>(this)->div(numerator) * denominator_;
  }

  friend value_type operator/(value_type a, const Impl& b) { return b.div(a); }

  friend value_type operator%(value_type a, const Impl& b) { return b.mod(a); }

  value_type denominator() const { return denominator_; }

 protected:
  using MagicValueType = MagicT;
  static_assert(sizeof(MagicT) >= 2 * sizeof(value_type));
  MagicT magic_;

 private:
  value_type denominator_;
};
}  // namespace internal

// Division and modulus using uint64_t numerators and denominators.
template <>
class ConstantDivisor<uint64_t>
    : public internal::ConstantDivisorBase<uint64_t, absl::uint128,
                                           ConstantDivisor<uint64_t>> {
 public:
  // REQUIRES: denominator > 1
  explicit ConstantDivisor(value_type denominator);

  value_type div(value_type numerator) const {
    return MultiplyHi(magic_, numerator);
  }

 private:
  static uint64_t MultiplyHi(absl::uint128 a, uint64_t b) {
    absl::uint128 lo(absl::Uint128Low64(a));
    absl::uint128 hi(absl::Uint128High64(a));
    absl::uint128 bottom = (lo * b) >> 64;
    absl::uint128 top = (hi * b);
    return absl::Uint128High64(bottom + top);
  }
};

// Division and modulus using uint32_t numerators and denominators.
template <>
class ConstantDivisor<uint32_t>
    : public internal::ConstantDivisorBase<uint32_t, uint64_t,
                                           ConstantDivisor<uint32_t>> {
 public:
  using value_type = uint32_t;

  // REQUIRES: denominator > 1
  explicit ConstantDivisor(value_type denominator);

  value_type div(value_type numerator) const {
    return absl::Uint128High64(absl::uint128(numerator) * magic_);
  }
};

// Division and modulus using uint16_t numerators and denominators.
template <>
class ConstantDivisor<uint16_t>
    : public internal::ConstantDivisorBase<uint16_t, uint64_t,
                                           ConstantDivisor<uint16_t>> {
 public:
  using value_type = uint16_t;

  explicit ConstantDivisor(value_type denominator);

  value_type div(value_type numerator) const {
    return static_cast<uint16_t>(
        (magic_ * static_cast<MagicValueType>(numerator)) >> kShift);
  }

 private:
  // Any value in [32;48] works here.
  static constexpr MagicValueType kShift = 32;
};

// Division and modulus using uint8_t numerators and denominators.
template <>
class ConstantDivisor<uint8_t>
    : public internal::ConstantDivisorBase<uint8_t, uint32_t,
                                           ConstantDivisor<uint8_t>> {
 public:
  using value_type = uint8_t;

  explicit ConstantDivisor(value_type denominator);

  value_type div(value_type numerator) const {
    return static_cast<uint8_t>(
        (magic_ * static_cast<MagicValueType>(numerator)) >> kShift);
  }

 private:
  // Any value in [16;24] works here.
  static constexpr MagicValueType kShift = 16;
};

}  // namespace math
}  // namespace util

#endif  // OR_TOOLS_BASE_CONSTANT_DIVISOR_H_
