// Copyright 2010 Google
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

#include "base/integral_types.h"
#include "base/logging.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"

namespace operations_research {

// ---------- SmallRevBitSet ----------

SmallRevBitSet::SmallRevBitSet(int64 size) : bits_(0LL), stamp_(0) {
  DCHECK_GT(size, 0);
  DCHECK_LE(size, 64);
}

void SmallRevBitSet::SetToOne(Solver* const s, int64 pos) {
  DCHECK_GE(pos, 0);
  const uint64 current_stamp = s->stamp();
  if (stamp_ < current_stamp) {
    stamp_ = current_stamp;
    s->SaveValue(&bits_);
  }
  bits_ |= OneBit64(pos);
}

void SmallRevBitSet::SetToZero(Solver* const s, int64 pos) {
  DCHECK_GE(pos, 0);
  const uint64 current_stamp = s->stamp();
  if (stamp_ < current_stamp) {
    stamp_ = current_stamp;
    s->SaveValue(&bits_);
  }
  bits_ &= ~OneBit64(pos);
}

int64 SmallRevBitSet::Cardinality() const {
  return BitCount64(bits_);
}

int64 SmallRevBitSet::GetFirstOne() const {
  return LeastSignificantBitPosition64(bits_);
}

// ---------- RevBitSet ----------

RevBitSet::RevBitSet(int64 size)
    : rows_(1), columns_(size), length_(BitLength64(size)),
      bits_(new uint64[length_]), stamps_(new uint64[length_]) {
  DCHECK_GE(size, 1);
  memset(bits_, 0, sizeof(*bits_) * length_);
  memset(stamps_, 0, sizeof(*stamps_) * length_);
}

RevBitSet::RevBitSet(int64 rows, int64 columns)
    : rows_(rows), columns_(columns), length_(BitLength64(rows * columns)),
      bits_(new uint64[length_]), stamps_(new uint64[length_]) {
  DCHECK_GE(rows, 1);
  DCHECK_GE(columns, 1);
  memset(bits_, 0, sizeof(*bits_) * length_);
  memset(stamps_, 0, sizeof(*stamps_) * length_);
}

RevBitSet::~RevBitSet() {
  delete [] bits_;
  delete [] stamps_;
}

void RevBitSet::SetToOne(Solver* const solver, int64 index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, columns_ * rows_);
  const int64 offset = BitOffset64(index);
  const int64 pos = BitPos64(index);
  if (!(bits_[offset] & OneBit64(pos))) {
    const uint64 current_stamp = solver->stamp();
    if (current_stamp > stamps_[offset]) {
      stamps_[offset] = current_stamp;
      solver->SaveValue(&bits_[offset]);
    }
    bits_[offset] |= OneBit64(pos);
  }
}

void RevBitSet::SetToOne(Solver* const solver, int64 row, int64 column) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_GE(column, 0);
  DCHECK_LT(column, columns_);
  SetToOne(solver, row * columns_ + column);
}

void RevBitSet::SetToZero(Solver* const solver, int64 index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, columns_ * rows_);
  const int64 offset = BitOffset64(index);
  const int64 pos = BitPos64(index);
  if (bits_[offset] & OneBit64(pos)) {
    const uint64 current_stamp = solver->stamp();
    if (current_stamp > stamps_[offset]) {
      stamps_[offset] = current_stamp;
      solver->SaveValue(&bits_[offset]);
    }
    bits_[offset] &= ~OneBit64(pos);
  }
}

void RevBitSet::SetToZero(Solver* const solver, int64 row, int64 column) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_GE(column, 0);
  DCHECK_LT(column, columns_);
  SetToZero(solver, row * columns_ + column);
}

bool RevBitSet::IsSet(int64 index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, columns_ * rows_);
  return IsBitSet64(bits_, index);
}

int64 RevBitSet::Cardinality() const {
  int64 card = 0;
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
    const uint64 partial = bits_[i];
    if (partial) {
      if (!(partial & (partial - 1))) {
        if (found_one) {
          return false;
        }
        found_one = true;
      }
    }
  }
  return found_one;
}

int64 RevBitSet::GetFirstBit(int start) const {
  const int end = rows_ * columns_ + columns_ - 1;
  return LeastSignificantBitPosition64(bits_, start, end);
}

int64 RevBitSet::Cardinality(int row) const {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  const int start = row * columns_;
  return BitCountRange64(bits_, start, start + columns_ - 1);
}

bool RevBitSet::IsCardinalityOne(int row) const {
  // TODO(user) : Optimize this one.
  return Cardinality(row) == 1;
}

bool RevBitSet::IsCardinalityZero(int row) const {
  const int start = row * columns_;
  return IsEmptyRange64(bits_, start, start + columns_ - 1);
}

int64 RevBitSet::GetFirstBit(int row, int start) const {
  DCHECK_GE(start, 0);
  DCHECK_GE(row, 0);
  DCHECK_LT(row, rows_);
  DCHECK_LT(start, columns_);
  const int beginning = row * columns_;
  const int end = beginning + columns_ - 1;
  int64 position = LeastSignificantBitPosition64(bits_, beginning + start, end);
  if (position == -1) {
    return -1;
  } else {
    return position - beginning;
  }
}

void RevBitSet::RevClearAll(Solver* const solver) {
  const uint64 current_stamp = solver->stamp();
  for (int offset = 0; offset < length_; ++offset) {
    if (bits_[offset]) {
      if (current_stamp > stamps_[offset]) {
        stamps_[offset] = current_stamp;
        solver->SaveValue(&bits_[offset]);
      }
      bits_[offset] = GG_ULONGLONG(0);
    }
  }
}

}  // namespace operations_research
