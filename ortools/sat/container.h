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

#ifndef ORTOOLS_SAT_CONTAINER_H_
#define ORTOOLS_SAT_CONTAINER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

  LiteralsOrOffsets(const LiteralsOrOffsets&) = delete;
  LiteralsOrOffsets& operator=(const LiteralsOrOffsets&) = delete;
  LiteralsOrOffsets(LiteralsOrOffsets&&) = default;
  LiteralsOrOffsets& operator=(LiteralsOrOffsets&&) = default;

  using Offset = int;

  // Adds a literal to the end of the list of literals.
  void PushBackLiteral(Literal literal);

  // Adds an offset to the set of offsets.
  void InsertOffset(Offset offset) {
    values_.push_back({.offset = offset});
    ++num_offsets_;
  }

  int num_literals() const { return num_literals_; }
  int num_offsets() const { return num_offsets_; }

  absl::Span<const Literal> literals() const;
  absl::Span<Literal> literals();

  // Note that order is arbitrary.
  absl::Span<const Offset> offsets() const;
  absl::Span<Offset> offsets();

  // Clears the literals and offsets. If `shrink_to_fit` is true, releases the
  // backing memory.
  void Clear(bool shrink_to_fit);

  // Clears the literals. If `shrink_to_fit` is true, compacts the backing
  // memory (i.e., also removes the potential hole in between literals and
  // offsets).
  void ClearLiterals(bool shrink_to_fit);

  // Clears offsets.
  void ClearOffsets() { num_offsets_ = 0; }

  // Resizes the list of literals. If `new_size` is smaller than the current
  // size, the literals are truncated (memory is not released). Otherwise,
  // the list is extended with default-constructed literals.
  void ResizeLiterals(int new_size);

  // Removes all literals for which `predicate` returns true, and truncates the
  // list of literals to the number of remaining literals (memory is not
  // released).
  template <typename Predicate>
  void RemoveLiteralsIf(Predicate predicate);

  // Sorts the list of literals, removes duplicate literals, and truncates the
  // list of literals to the number of remaining literals (memory is not
  // released).
  void SortLiteralsAndRemoveDuplicates();

  // Returns the backing capacity for literals and offsets.
  size_t capacity() const { return values_.capacity(); }

  static constexpr size_t kInlineElements = 4;

 private:
  // Elements are either literals or offsets.
  union LiteralOrOffset {
    Literal literal;
    Offset offset;
  };
  static_assert(std::is_trivially_destructible_v<Literal>);

  uint32_t num_literals_ = 0;
  uint32_t num_offsets_ = 0;
  // This array contains `num_literals_` literals, then `values_.size() -
  // num_literals_ - num_offsets_` holes, then `num_offsets_` offsets.
  // TODO(user): Try a custom implementation of a inlined vector with just a
  // "capacity" we can always put the literal in [0, num_literals) and the
  // offsets in [capa-num_offset, capa). This should have the same number of
  // inlined element while taking 64bit less per LiteralsOrOffsets.
  absl::InlinedVector<LiteralOrOffset, kInlineElements> values_;
};

inline absl::Span<const Literal> LiteralsOrOffsets::literals() const {
  DCHECK_LE(num_literals_, values_.size());
  // Casting is OK because "pointer to LiteralOrOffset" is
  // pointer-interconvertible with "pointer to Literal": `LiteralOrOffset` is
  // an union object and the other is a non-static data member of that object.
  return absl::MakeConstSpan(reinterpret_cast<const Literal*>(values_.data()),
                             num_literals_);
}

inline absl::Span<Literal> LiteralsOrOffsets::literals() {
  DCHECK_LE(num_literals_, values_.size());
  return absl::MakeSpan(reinterpret_cast<Literal*>(values_.data()),
                        num_literals_);
}

inline absl::Span<const LiteralsOrOffsets::Offset> LiteralsOrOffsets::offsets()
    const {
  DCHECK_LE(num_offsets_, values_.size());
  return absl::MakeConstSpan(
      reinterpret_cast<const Offset*>(values_.data() +
                                      (values_.size() - num_offsets_)),
      num_offsets_);
}

inline absl::Span<LiteralsOrOffsets::Offset> LiteralsOrOffsets::offsets() {
  DCHECK_LE(num_offsets_, values_.size());
  return absl::MakeSpan(reinterpret_cast<Offset*>(
                            values_.data() + (values_.size() - num_offsets_)),
                        num_offsets_);
}

inline void LiteralsOrOffsets::PushBackLiteral(Literal literal) {
  // When adding a new literal N, there are three cases:
  //   -  There is a hole, we can put the literal here:
  //      `LLLL..OOO` -> `LLLLN.OOO`, or
  //      `LLLL..` -> `LLLLN.`
  //   -  There are only literals. We can just append the literal:
  //      `LLLL` -> `LLLLN`
  //   -  There is no space before the offsets. The last offset must be moved
  //      to the back to make room:
  //      `LLLLOOO` -> `LLLL.OOO` -> `LLLLNOOO`
  if (values_.size() > (num_literals_ + num_offsets_)) {
    // There is a hole, use it.
    values_[num_literals_].literal = literal;
  } else if (num_offsets_ == 0) {
    values_.push_back({.literal = literal});
  } else {
    // Move the first offset to the back to make room for the new literal.
    values_.push_back(values_[num_literals_]);
    values_[num_literals_].literal = literal;
  }
  ++num_literals_;
}

inline void LiteralsOrOffsets::ResizeLiterals(int new_size) {
  if (new_size <= num_literals_) {
    num_literals_ = new_size;
    return;
  }
  const int grow_by = new_size - num_literals_;
  // We have to create a hole of size at least `grow_by` by moving offsets to
  // the end.
  const int hole_size = values_.size() - (num_literals_ + num_offsets_);
  DCHECK_GE(hole_size, 0);
  if (hole_size < grow_by) {
    const int hole_growth = grow_by - hole_size;
    values_.resize(values_.size() + hole_growth);
    memcpy(values_.data() + num_literals_ + hole_size + num_offsets_,
           values_.data() + num_literals_ + hole_size,
           hole_growth * sizeof(LiteralOrOffset));
  }
  // Initialize with default-constructed literals.
  LiteralOrOffset* const hole_data = values_.data() + num_literals_;
  for (int i = 0; i < grow_by; ++i) {
    hole_data[i].literal = {};
  }
  num_literals_ = new_size;
}

template <typename Predicate>
void LiteralsOrOffsets::RemoveLiteralsIf(Predicate predicate) {
  const auto new_end = std::remove_if(literals().begin(), literals().end(),
                                      std::move(predicate));
  num_literals_ = new_end - literals().begin();
}

inline void LiteralsOrOffsets::SortLiteralsAndRemoveDuplicates() {
  const auto range = literals();
  std::sort(range.begin(), range.end());
  const auto new_end = std::unique(range.begin(), range.end());
  num_literals_ = new_end - literals().begin();
}

inline void LiteralsOrOffsets::Clear(bool shrink_to_fit) {
  num_literals_ = 0;
  num_offsets_ = 0;
  if (shrink_to_fit) {
    gtl::STLClearObject(&values_);
  }
}

inline void LiteralsOrOffsets::ClearLiterals(bool shrink_to_fit) {
  num_literals_ = 0;
  if (shrink_to_fit) {
    absl::InlinedVector<LiteralOrOffset, kInlineElements> tmp;
    if (num_offsets_ > 0) {
      tmp.insert(tmp.end(), values_.data() + (values_.size() - num_offsets_),
                 values_.data() + values_.size());
    }
    values_ = std::move(tmp);
  }
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_CONTAINER_H_
