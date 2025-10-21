// Copyright 2010-2025 Google LLC
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

#ifndef OR_TOOLS_SET_COVER_FAST_VARINT_H_
#define OR_TOOLS_SET_COVER_FAST_VARINT_H_

#include <cstdint>
#include <limits>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"

namespace operations_research {
// This is namespace contains utilities to encode and decode variable-length
// integers.
// The code assumes that the machine is little-endian :
static_assert(absl::endian::native == absl::endian::little);

// TODO(user): implement a version for big-endian machines.

// The encoding of a varint takes a uint64_t and returns a uint64_t which
// encodes its length in the lower bits.
// For a uint64_t value n, we define its width in bits BitWidth(n) as the number
// of bits needed to represent the value in binary. Similarly, we define its
// width in bytes ByteWidth(n) as the number of bytes needed to represent the
// value in binary.
// The length of the encoding is defined as the index of the first occurrence
// of a 0 bit starting from the least significant bit of the encoding.
// Thus, if the lower bit of the encoding is 0b0, the encoding size is 1.
// If the lower bits of the encoding ar 0b01, the encoding size is 2, etc.
// It is easy to remark that the encoding of the length consumes 1 bit per byte
// in the encoding. This means that each byte of the encoding can contain a
// payload of 7 bits.
// A full uint64_t value can contain 8*7 = 56 bits of payload, with a prefix
// 0b01111111 with seven 1s.
// For values above 2^56, we use the prefix encoding kLargeEncodingPrefix (0xff,
// which corresponds to an encoding length of 9), followed by the full 64-bit
// value.
//
// The advantages of this encoding are:
// - There is a minimum number of memory accesses to read and write the
//   encoding. An encoded number less than 1 << 56 can be read and written in a
//   single 64-bit memory access. Note that the write clobbers the bytes located
//   after the encoded value, which is most often not an issue when doing
//   push_back on a vector. There are two memory accesses only when reading an
//   encoded value of 1 << 56 and above.
// - Encoding is very fast and can be branchless on clang 20+. Division by 7 is
//   simply avoided using a classic approximation for a limited range.
// - Decoding is also very fast and can also be branchless. Both decoding and
//   encoding cam benefit from the fast "trans-dichotomous" instructions of
//   modern chips to count the number of leading or trailing zeros in a word.
// - The encoding size is as small as for an LEB128 encoding, and smaller for
//   values 1 << 63 and above (which happends to be half of the 64-bit values).
// - Zero is encoded as 0, similarly to LEB128, which makes it easy to write
//   code testing against 0.
//
// The main drawbacks of this encoding are:
// - It is not resilient to errors, as there is no way to check for the start of
//   the next encoded value when reading from a byte stream. In our case, we are
//   using this to delta-encode a vector of sorted integers in memory, and we
//   are not impacted by network errors.
// - It only supports little-endian machines, although it would not be hard to
//   convert to a big-endian encoding by using the upper bits instead of the
//   lower bits. Again, this means that the encoding is not intended to be used
//   in network protocols.
//
// In conclusion, this encoding is intended to be used as a delta-encoding in
// cases where the size of the delta-encoded integers is less than 56 bits.
// This encoding is amply enough in 2025 as workstations sport 43 bits of
// physical memory, and server chips with 57 bits of virtual memory.
// This encoding is not intended to be used in network protocols, it is
// intended to be used in-memory for example as the representation of the
// indices of a sparse Boolean matrix.
// For network protocols, it's better to use the Varint class from
// ortools/util/varint.h or
// util/coding/varint.h which is used in many Google-internal systems including
// Protocol Buffers.
// Compared to the other encodings as described in
// https://dcreager.net/2021/03/a-better-varint/ (Google Varint is named Metric
// Varint) this encoding is better suited to low-endian machines than "Imperial
// Varint" and simpler to implement than vu128.
//
// The naming of the namespace is due to John Von Neumann saying that all
// computers should be little-endian. Since this encoding is only intended to be
// used on little-endian machines, we thought it would be a good idea to make
// this reference to the great man in the name.

namespace VonNeumannVarint {

static constexpr uint64_t kLargeEncodingPrefix = 0xff;
static constexpr uint64_t kLargeEncodingSize = 9;
static constexpr uint64_t kFirstLargeEncodingValue = 1ull << 56;
static constexpr uint64_t kWordBits = std::numeric_limits<uint64_t>::digits;

// Classical division by 7 for small values. When compiled on x86_64, the
// generated code is two LEAs and one shift.
static constexpr uint32_t DivBy7(uint32_t n) {
  DCHECK_LE(n, 89);  // Works up to 89. We only need up to 64 + 6 = 70 anyway.
  return n * 37ULL >> 8;
}

// Returns 1 for n == 0, Ceil(n / 7) otherwise.
// i.e. max(1, ceil(n / 7)) or max(1, (n + 6) / 7). Needs only 1 LEA.
// Also a classic that can be derived using a static constexpr computation.
static constexpr uint32_t CeilDivBy7(uint32_t n) {
  // DCHECK_LE(n, 448);  // Works for n up to 448.
  return 1 + (9 * n / 64);
}

// Returns an upper bound on the number of bytes needed to store the encoding of
// the value. In the case more that 8 bytes are needed, it is encoded using the
// large encoding. Note that an encoding with all bits set would return 64. The
// only reasonable tests for output values of this function are order
// comparisons against the size of uint64_t.
static constexpr uint32_t EncodingLength(uint64_t encoding) {
  return 1 + absl::countr_one(encoding);
}

// Returns true if the value needs to be encoded using the large encoding.
static constexpr bool NeedsLargeEncoding(uint64_t value) {
  return value >= kFirstLargeEncodingValue;
}

// Returns true if the encoding uses starts with the prefix
// kLargeEncodingPrefix.
static constexpr bool UsesLargeEncoding(uint64_t encoding) {
  return (encoding & kLargeEncodingPrefix) == kLargeEncodingPrefix;
}

// Returns true if the encoding uses one byte, i.e. the lower bit is 0.
static constexpr bool UsesOneByte(uint64_t encoding) {
  return (encoding & 1) == 0;
}

// Decodes a one-byte encoding. The encoding is assumed to be valid.
static constexpr uint64_t DecodeOneByte(uint8_t encoding) {
  return encoding >> 1;
}

// Returns the number of bits needed to represent the value in binary.
static constexpr uint32_t BitWidth(uint64_t value) {
  // value | 1 takes as many bits as value alone, except for value = 0, where it
  // takes 1 bit, which is what we actually want.
  return kWordBits - absl::countl_zero(value | 1);
}

// Returns the number of 7-bit chunks ("septets") necessary to encode the
// varint. It's the ceiling of the bit width of the value divided by 7.
// Note that this can return 10 when for values 1 << 63 and above, but we avoid
// creating an extra case by just testing that the septet width is greater than
// 8 before using the kLargeEncodingPrefix prefix.
static constexpr uint32_t SeptetWidth(uint64_t value) {
  return CeilDivBy7(BitWidth(value));
}

// Returns a mask where the low n bits are set to 1 and the rest are set to 0.
static constexpr uint64_t LowerBitsMask(uint64_t n) {
  // We have to treat the special case when n is equal to the word size.
  // With NDEBUG the generated code has no branch instructions.
  return n == kWordBits ? ~0ull : (1ull << n) - 1;
}

// Same as LowerBitsMask, but does work is n is equal to the word size.
static constexpr uint64_t UnsafeLowerBitsMask(uint64_t n) {
  // The case when n is equal to the word size is not handled here.
  DCHECK_LT(n, kWordBits);
  return (1ull << n) - 1;
}

// Encodes a uint64_t into a varint and returns the value along with the
// header mask used in the lower 8 bits of the encoding. Note that the code
// has no multiplication, division, nor branch as even Num7BitChunks uses
// conditional moves.
// Before clang 20, there is a branch in the code. In clang 20 the code is
// branchless, and the generated code has 17 instructions.
// With gcc, there is only one branch.
static constexpr uint64_t EncodeSmallVarintDefault(uint64_t value) {
  const uint32_t encoding_size = SeptetWidth(value);
  DCHECK_LE(encoding_size, sizeof(value));  // 8 bytes = 56 usable bits.
  const uint64_t header = UnsafeLowerBitsMask(encoding_size - 1);
  return header | (value << encoding_size);
}

// Decodes a small varint and returns the value. It is the duty of the caller
// to ensure that the encoding is valid, and for example the prefix is not
// kLargeEncodingPrefix.
// Before clang 20, there are twos branches in the code. In clang 20 the code is
// branchless, and the generated code has 12 instructions. With gcc there is
// only one branch.
static constexpr uint64_t DecodeSmallVarintDefault(uint64_t encoding) {
  DCHECK(!UsesLargeEncoding(encoding));
  const uint32_t encoding_size = EncodingLength(encoding);
  DCHECK_LE(encoding_size, sizeof(encoding));
  const uint64_t mask = UnsafeLowerBitsMask(7 * encoding_size);
  const uint64_t value = (encoding >> encoding_size) & mask;
  return value;
}

// Special code generation for machines that support advanced bit manipulation
// (ABM) instructions of the groups BMI, BMI2, and LZCNT.
// These instructions are available on the so-called x86_64-v3 architecture
// (Haswell and later).
// The following macro is used to generate code for machines with ABM. We do not
// use multiversioning. In order to maximise performance, we prefer to detect
// the machine type at run time in higher-level code, and to have two code paths
// depending on the architecture.
#ifdef VN_VARINT_ABM_GEN
#error "VN_VARINT_ABM_GEN is already defined."
#endif
#define VN_VARINT_ABM_GEN __attribute__((target("bmi,bmi2,lzcnt")))

// Same as EncodeSmallVarint, but for x86_64-v3 machines. The code is exactly
// the same, but clang generates only 14 instructions, and it does not have a
// branch even with older versions of clang.
// gcc does generate a conditional branch at the beginning of the function.
VN_VARINT_ABM_GEN static constexpr uint64_t EncodeSmallVarintABM(
    uint64_t value) {
  const uint32_t encoding_size = SeptetWidth(value);
  DCHECK_LE(encoding_size, sizeof(value));  // 8 bytes = 56 usable bits.
  const uint64_t header = UnsafeLowerBitsMask(encoding_size - 1);
  return header | (value << encoding_size);
}

// Same as DecodeSmallVarint, but for x86_64-v3 machines. The code is exactly
// the same, but even older versions of clang or gcc generate only 8
// instructions without a branch.
VN_VARINT_ABM_GEN static constexpr uint64_t DecodeSmallVarintABM(
    uint64_t encoding) {
  DCHECK(!UsesLargeEncoding(encoding));
  const uint32_t encoding_size = EncodingLength(encoding);
  DCHECK_LE(encoding_size, sizeof(encoding));
  const uint64_t mask = UnsafeLowerBitsMask(7 * encoding_size);
  const uint64_t value = (encoding >> encoding_size) & mask;
  return value;
}

#undef VN_VARINT_ABM_GEN

// Encodes a uint64_t into a varint and returns the value.
// If use_abm is true, the code is optimized for x86_64-v3 machines.
template <bool use_abm = false>
static constexpr uint64_t EncodeSmallVarint(uint64_t value) {
  return use_abm ? EncodeSmallVarintABM(value)
                 : EncodeSmallVarintDefault(value);
}

// Decodes a small varint and returns the value.
// If use_abm is true, the code is optimized for x86_64-v3 machines.
template <bool use_abm = false>
static constexpr uint64_t DecodeSmallVarint(uint64_t encoding) {
  return use_abm ? DecodeSmallVarintABM(encoding)
                 : DecodeSmallVarintDefault(encoding);
}

}  // namespace VonNeumannVarint
}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_FAST_VARINT_H_
