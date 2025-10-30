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

#ifndef OR_TOOLS_SAT_CONTAINER_H_
#define OR_TOOLS_SAT_CONTAINER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// This is a very specific container that is optimized for the specific usage
// patterns of BinaryImplicationGraph.
// It stores an ordered set of literals and an unordered set of offsets.
// internally, both arrays are stored contiguously, literals first, then
// offsets. There might be a hole between the two arrays. In comments, we denote
// literals as `L` and offsets as `O`, and holes as `.`. For example, `LLL..OO`
// has 3 literals and 2 offsets, with a hole of size 2 in between.
class LiteralsOrOffsets {
 public:
  LiteralsOrOffsets() = default;

  ~LiteralsOrOffsets() {
    if (capacity_ > kInlineElements) free(data_.ptr);
  }

  LiteralsOrOffsets(const LiteralsOrOffsets&) = delete;
  LiteralsOrOffsets& operator=(const LiteralsOrOffsets&) = delete;

  LiteralsOrOffsets(LiteralsOrOffsets&& o) {
    num_literals_ = o.num_literals_;
    num_offsets_ = o.num_offsets_;
    capacity_ = o.capacity_;
    data_ = o.data_;
    o.capacity_ = kInlineElements;  // So we don't double-free.
  }

  LiteralsOrOffsets& operator=(LiteralsOrOffsets&& o) {
    num_literals_ = o.num_literals_;
    num_offsets_ = o.num_offsets_;
    capacity_ = o.capacity_;
    data_ = o.data_;
    o.capacity_ = kInlineElements;  // So we don't double-free.
    return *this;
  }

  using Offset = int;

  // Adds a literal to the end of the list of literals.
  void PushBackLiteral(Literal literal) {
    if (num_literals_ + num_offsets_ >= capacity_) {
      GrowCapacity();
    }
    DCHECK_LT(num_literals_ + num_offsets_, capacity_);
    InternalAddress()[num_literals_++].literal = literal;
  }

  // Adds an offset to the set of offsets.
  void InsertOffset(Offset offset) {
    if (num_literals_ + num_offsets_ >= capacity_) {
      GrowCapacity();
    }
    DCHECK_LT(num_literals_ + num_offsets_, capacity_);
    InternalAddress()[capacity_ - (++num_offsets_)].offset = offset;
  }

  int num_literals() const { return num_literals_; }
  int num_offsets() const { return num_offsets_; }

  absl::Span<const Literal> literals() const;
  absl::Span<Literal> literals();

  // Note that order is arbitrary.
  absl::Span<const Offset> offsets() const;
  absl::Span<Offset> offsets();

  // Clearing functions.
  // Call ShrinkToFit() if you want to save memory.
  void ClearLiterals() { num_literals_ = 0; }
  void ClearOffsets() { num_offsets_ = 0; }
  void Clear() {
    num_literals_ = 0;
    num_offsets_ = 0;
  }

  // A bit faster than Clear() + ShrinkToFit().
  void ClearAndReleaseMemory() {
    num_literals_ = 0;
    num_offsets_ = 0;
    if (capacity_ > kInlineElements) free(data_.ptr);
    capacity_ = kInlineElements;
  }

  // Use as little memory as possible.
  void ShrinkToFit() {
    if (capacity_ == kInlineElements) return;  // Smallest possible size.

    const uint32_t needed = num_literals_ + num_offsets_;
    if (needed == capacity_) return;

    DCHECK_GT(capacity_, kInlineElements);
    LiteralOrOffset* old_ptr = data_.ptr;  // This is valid memory.
    if (needed > kInlineElements) {
      LiteralOrOffset* new_ptr = static_cast<LiteralOrOffset*>(
          malloc(static_cast<ptrdiff_t>(needed) * sizeof(LiteralOrOffset)));
      CopyFromOldToNew(old_ptr, capacity_, new_ptr, needed);
      capacity_ = needed;
      data_.ptr = new_ptr;
    } else {
      CopyFromOldToNew(old_ptr, capacity_, data_.inlined, kInlineElements);
      capacity_ = kInlineElements;
    }

    free(old_ptr);
  }

  // Resizes the list of literals to or shorter length.
  void TruncateLiterals(int new_size) {
    CHECK_GE(new_size, 0);
    CHECK_LE(new_size, num_literals_);
    num_literals_ = new_size;
  }

  // Removes all literals for which `predicate` returns true, and truncates the
  // list of literals to the number of remaining literals (memory is not
  // released).
  template <typename Predicate>
  void RemoveLiteralsIf(Predicate predicate);

  // Returns the backing capacity for literals and offsets.
  uint32_t capacity() const { return capacity_; }

  static constexpr uint32_t kInlineElements = 4;

 private:
  // Elements are either literals or offsets.
  union LiteralOrOffset {
    Literal literal;
    Offset offset;
  };
  static_assert(std::is_trivially_destructible_v<LiteralOrOffset>);
  static_assert(std::is_trivially_copyable_v<LiteralOrOffset>);

  LiteralOrOffset* InternalAddress() {
    return capacity_ == kInlineElements ? data_.inlined : data_.ptr;
  }
  const LiteralOrOffset* InternalAddress() const {
    return capacity_ == kInlineElements ? data_.inlined : data_.ptr;
  }

  void CopyFromOldToNew(LiteralOrOffset* old_ptr, uint32_t old_capacity,
                        LiteralOrOffset* new_ptr, uint32_t new_capacity) {
    memcpy(new_ptr, old_ptr, num_literals_ * sizeof(LiteralOrOffset));
    memcpy(new_ptr + (new_capacity - num_offsets_),
           old_ptr + (old_capacity - num_offsets_),
           num_offsets_ * sizeof(LiteralOrOffset));
  }

  void GrowCapacity() {
    // TODO(user): crash later.
    // For now, we do if we use more than 2 GB per LiteralOrOffsets.
    CHECK_LE(capacity_, std::numeric_limits<uint32_t>::max() / 2);
    const uint32_t new_capacity =
        static_cast<uint32_t>(1.3 * static_cast<double>(capacity_));
    CHECK_GT(new_capacity, kInlineElements);

    LiteralOrOffset* new_ptr = static_cast<LiteralOrOffset*>(
        malloc(static_cast<ptrdiff_t>(new_capacity) * sizeof(LiteralOrOffset)));
    CopyFromOldToNew(InternalAddress(), capacity_, new_ptr, new_capacity);

    if (capacity_ > kInlineElements) {
      free(data_.ptr);
    }
    capacity_ = new_capacity;
    data_.ptr = new_ptr;
  }

  // Invariants:
  // num_literals_ + num_offsets_ <= capacity_
  // capacity_ >= kInlineElements.
  uint32_t num_literals_ = 0;
  uint32_t num_offsets_ = 0;
  uint32_t capacity_ = kInlineElements;

  // Inlined memory or a pointer to the content.
  //
  // TODO(user): If we want to be more compact, we can set kInlineElements == 2.
  // We can probably fit a third one, if we don't care about the aligment since
  // we only use 3 * int32_t above. Experiments.
  union {
    LiteralOrOffset inlined[kInlineElements];
    LiteralOrOffset* ptr;
  } data_;
};

inline absl::Span<const Literal> LiteralsOrOffsets::literals() const {
  DCHECK_LE(num_literals_, capacity_);
  // Casting is OK because "pointer to LiteralOrOffset" is
  // pointer-interconvertible with "pointer to Literal": `LiteralOrOffset` is
  // an union object and the other is a non-static data member of that object.
  return absl::MakeConstSpan(
      reinterpret_cast<const Literal*>(InternalAddress()), num_literals_);
}

inline absl::Span<Literal> LiteralsOrOffsets::literals() {
  DCHECK_LE(num_literals_, capacity_);
  return absl::MakeSpan(reinterpret_cast<Literal*>(InternalAddress()),
                        num_literals_);
}

inline absl::Span<const LiteralsOrOffsets::Offset> LiteralsOrOffsets::offsets()
    const {
  DCHECK_LE(num_offsets_, capacity_);
  return absl::MakeConstSpan(
      reinterpret_cast<const Offset*>(InternalAddress() +
                                      (capacity_ - num_offsets_)),
      num_offsets_);
}

inline absl::Span<LiteralsOrOffsets::Offset> LiteralsOrOffsets::offsets() {
  DCHECK_LE(num_offsets_, capacity_);
  return absl::MakeSpan(
      reinterpret_cast<Offset*>(InternalAddress() + (capacity_ - num_offsets_)),
      num_offsets_);
}

template <typename Predicate>
void LiteralsOrOffsets::RemoveLiteralsIf(Predicate predicate) {
  const auto new_end = std::remove_if(literals().begin(), literals().end(),
                                      std::move(predicate));
  num_literals_ = new_end - literals().begin();
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CONTAINER_H_
