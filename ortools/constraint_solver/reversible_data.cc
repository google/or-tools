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

#include "ortools/constraint_solver/reversible_data.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "absl/log/check.h"
#include "ortools/constraint_solver/reversible_engine.h"
#include "ortools/util/bitset.h"

namespace operations_research {

// ---------- SmallRevBitSet ----------

SmallRevBitSet::SmallRevBitSet(int64_t size) : bits_(0LL) {
  DCHECK_GT(size, 0);
  DCHECK_LE(size, 64);
}

void SmallRevBitSet::SetToOne(ReversibleEngine* engine, int64_t pos) {
  DCHECK_GE(pos, 0);
  bits_.SetValue(engine, bits_.Value() | OneBit64(pos));
}

void SmallRevBitSet::SetToZero(ReversibleEngine* engine, int64_t pos) {
  DCHECK_GE(pos, 0);
  bits_.SetValue(engine, bits_.Value() & ~OneBit64(pos));
}

int64_t SmallRevBitSet::Cardinality() const {
  return BitCount64(bits_.Value());
}

int64_t SmallRevBitSet::GetFirstOne() const {
  if (bits_.Value() == 0) {
    return -1;
  }
  return LeastSignificantBitPosition64(bits_.Value());
}

// ---------- RevBitSet ----------

RevBitSet::RevBitSet(int64_t size)
    : size_(size),
      length_(BitLength64(size)),
      bits_(new uint64_t[length_]),
      stamps_(new uint64_t[length_]) {
  DCHECK_GE(size, 1);
  memset(bits_.get(), 0, sizeof(bits_[0]) * length_);
  memset(stamps_.get(), 0, sizeof(stamps_[0]) * length_);
}

void RevBitSet::Save(ReversibleEngine* engine, int offset) {
  const uint64_t current_stamp = engine->stamp();
  if (current_stamp > stamps_[offset]) {
    stamps_[offset] = current_stamp;
    engine->SaveValue(&bits_[offset]);
  }
}

void RevBitSet::SetToOne(ReversibleEngine* engine, int64_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, size_);
  const int64_t offset = BitOffset64(index);
  const int64_t pos = BitPos64(index);
  if (!(bits_[offset] & OneBit64(pos))) {
    Save(engine, offset);
    bits_[offset] |= OneBit64(pos);
  }
}

void RevBitSet::SetToZero(ReversibleEngine* engine, int64_t index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, size_);
  const int64_t offset = BitOffset64(index);
  const int64_t pos = BitPos64(index);
  if (bits_[offset] & OneBit64(pos)) {
    Save(engine, offset);
    bits_[offset] &= ~OneBit64(pos);
  }
}

bool RevBitSet::IsSet(int64_t index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, size_);
  return IsBitSet64(bits_.get(), index);
}

int64_t RevBitSet::Cardinality() const {
  int64_t card = 0;
  for (int i = 0; i < length_; ++i) {
    card += BitCount64(bits_[i]);
  }
  return card;
}

bool RevBitSet::IsCardinalityZero() const {
  for (int i = 0; i < length_; ++i) {
    if (bits_[i]) {
      return false;
    }
  }
  return true;
}

bool RevBitSet::IsCardinalityOne() const {
  bool found_one = false;
  for (int i = 0; i < length_; ++i) {
    const uint64_t partial = bits_[i];
    if (partial) {
      if (!(partial & (partial - 1))) {
        if (found_one) {
          return false;
        }
        found_one = true;
      } else {
        return false;
      }
    }
  }
  return found_one;
}

int64_t RevBitSet::GetFirstBit(int start) const {
  return LeastSignificantBitPosition64(bits_.get(), start, size_ - 1);
}

void RevBitSet::ClearAll(ReversibleEngine* engine) {
  for (int offset = 0; offset < length_; ++offset) {
    if (bits_[offset]) {
      Save(engine, offset);
      bits_[offset] = uint64_t{0};
    }
  }
}

// ----- RevBitMatrix -----

RevBitMatrix::RevBitMatrix(int64_t rows, int64_t columns)
    : RevBitSet(rows * columns), rows_(rows), columns_(columns) {
  DCHECK_GE(rows, 1);
  DCHECK_GE(columns, 1);
}

RevBitMatrix::~RevBitMatrix() {}

void RevBitMatrix::SetToOne(ReversibleEngine* engine, int64_t row,
                            int64_t column) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_GE(column, 0);
  DCHECK_LT(column, columns_);
  RevBitSet::SetToOne(engine, row * columns_ + column);
}

void RevBitMatrix::SetToZero(ReversibleEngine* engine, int64_t row,
                             int64_t column) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_GE(column, 0);
  DCHECK_LT(column, columns_);
  RevBitSet::SetToZero(engine, row * columns_ + column);
}

int64_t RevBitMatrix::Cardinality(int row) const {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  const int start = row * columns_;
  return BitCountRange64(bits_.get(), start, start + columns_ - 1);
}

bool RevBitMatrix::IsCardinalityOne(int row) const {
  // TODO(user) : Optimize this one.
  return Cardinality(row) == 1;
}

bool RevBitMatrix::IsCardinalityZero(int row) const {
  const int start = row * columns_;
  return IsEmptyRange64(bits_.get(), start, start + columns_ - 1);
}

int64_t RevBitMatrix::GetFirstBit(int row, int start) const {
  DCHECK_GE(start, 0);
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_LT(start, columns_);
  const int beginning = row * columns_;
  const int end = beginning + columns_ - 1;
  int64_t position =
      LeastSignificantBitPosition64(bits_.get(), beginning + start, end);
  if (position == -1) {
    return -1;
  } else {
    return position - beginning;
  }
}

void RevBitMatrix::ClearAll(ReversibleEngine* engine) {
  RevBitSet::ClearAll(engine);
}

// ----- UnsortedNullableRevBitset -----

UnsortedNullableRevBitset::UnsortedNullableRevBitset(int bit_size)
    : bit_size_(bit_size),
      word_size_(BitLength64(bit_size)),
      bits_(word_size_, 0),
      active_words_(word_size_) {}

void UnsortedNullableRevBitset::Init(ReversibleEngine* engine,
                                     const std::vector<uint64_t>& mask) {
  CHECK_LE(mask.size(), word_size_);
  for (int i = 0; i < mask.size(); ++i) {
    if (mask[i]) {
      bits_.SetValue(engine, i, mask[i]);
      active_words_.Insert(engine, i);
    }
  }
}

bool UnsortedNullableRevBitset::RevSubtract(ReversibleEngine* engine,
                                            const std::vector<uint64_t>& mask) {
  bool changed = false;
  to_remove_.clear();
  for (int index : active_words_) {
    if (index < mask.size() && (bits_[index] & mask[index]) != 0) {
      changed = true;
      const uint64_t result = bits_[index] & ~mask[index];
      bits_.SetValue(engine, index, result);
      if (result == 0) {
        to_remove_.push_back(index);
      }
    }
  }

  CleanUpActives(engine);
  return changed;
}

void UnsortedNullableRevBitset::CleanUpActives(ReversibleEngine* engine) {
  // We remove indices of null words in reverse order, as this may be a simpler
  // operations on the RevIntSet (no actual swap).
  for (int i = to_remove_.size() - 1; i >= 0; --i) {
    active_words_.Remove(engine, to_remove_[i]);
  }
}

bool UnsortedNullableRevBitset::RevAnd(ReversibleEngine* engine,
                                       const std::vector<uint64_t>& mask) {
  bool changed = false;
  to_remove_.clear();
  for (int index : active_words_) {
    if (index < mask.size()) {
      if ((bits_[index] & ~mask[index]) != 0) {
        changed = true;
        const uint64_t result = bits_[index] & mask[index];
        bits_.SetValue(engine, index, result);
        if (result == 0) {
          to_remove_.push_back(index);
        }
      }
    } else {
      // Zero the word as the mask is implicitly null.
      changed = true;
      bits_.SetValue(engine, index, 0);
      to_remove_.push_back(index);
    }
  }
  CleanUpActives(engine);
  return changed;
}

bool UnsortedNullableRevBitset::Intersects(const std::vector<uint64_t>& mask,
                                           int* support_index) {
  DCHECK_GE(*support_index, 0);
  DCHECK_LT(*support_index, word_size_);
  if (mask[*support_index] & bits_[*support_index]) {
    return true;
  }
  for (int index : active_words_) {
    if (bits_[index] & mask[index]) {
      *support_index = index;
      return true;
    }
  }
  return false;
}

}  // namespace operations_research
