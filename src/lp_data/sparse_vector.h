// Copyright 2010-2014 Google
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

// Classes to represent sparse vectors.
//
// The following are very good references for terminology, data structures,
// and algorithms:
//
// I.S. Duff, A.M. Erisman and J.K. Reid, "Direct Methods for Sparse Matrices",
// Clarendon, Oxford, UK, 1987, ISBN 0-19-853421-3,
// http://www.amazon.com/dp/0198534213
//
// T.A. Davis, "Direct methods for Sparse Linear Systems", SIAM, Philadelphia,
// 2006, ISBN-13: 978-0-898716-13, http://www.amazon.com/dp/0898716136
//
// Both books also contain a wealth of references.

#ifndef OR_TOOLS_LP_DATA_SPARSE_VECTOR_H_
#define OR_TOOLS_LP_DATA_SPARSE_VECTOR_H_

#include <algorithm>
#include <string>

#include "base/logging.h"  // for CHECK*
#include "base/integral_types.h"
#include "lp_data/lp_types.h"
#include "lp_data/permutation.h"
#include "util/iterators.h"
#include "util/return_macros.h"

namespace operations_research {
namespace glop {

// --------------------------------------------------------
// SparseVector
// --------------------------------------------------------
// This class allows to store a vector taking advantage of its sparsity.
// Space complexity is in O(num_entries).
// In the current implementation, entries are stored in a first-in order (order
// of SetCoefficient() calls) when they are added; then the "cleaning" process
// sorts them by index (and duplicates are removed: the last entry takes
// precedence).
// Many methods assume that the entries are sorted by index and without
// duplicates, and DCHECK() that.
//
// Default copy construction is fully supported.
//
// This class uses strong integer types (i.e. no implicit cast to/from other
// integer types) for both:
// - the index of entries (eg. SparseVector<RowIndex> is a SparseColumn,
//   see ./sparse_column.h).
// - the *internal* indices of entries in the internal storage, which is an
//   entirely different type: EntryType.
// TODO(user): un-expose this type to client; by getting rid of the
// index-based APIs and leveraging iterator-based APIs; if possible.
template <typename IndexType>
class SparseVector {
 public:
  typedef IndexType Index;

  typedef StrictITIVector<Index, Fractional> DenseVector;
  typedef Permutation<Index> IndexPermutation;

  SparseVector();

  // Read-only API for a given SparseVector entry. The typical way for a
  // client to use this is to use the natural range iteration defined by the
  // Iterator class below:
  //   SparseVector<int> v;
  //   ...
  //   for (const SparseVector<int>::Entry e : v) {
  //     LOG(INFO) << "Index: " << e.index() << ", Coeff: " << e.coefficient();
  //   }
  //
  // Note that this can only be used when the vector has no duplicates.
  //
  // Note(user): using either "const SparseVector<int>::Entry&" or
  // "const SparseVector<int>::Entry" yields the exact same performance on the
  // netlib, thus we recommend to use the latter version, for consistency.
  class Entry;
  class Iterator;
  Iterator begin() const;
  Iterator end() const;

  // Clears the vector, i.e. removes all entries.
  void Clear();

  // Clears the vector and releases the memory it uses.
  void ClearAndRelease();

  // Reserve the underlying storage for the given number of entries.
  void Reserve(EntryIndex size);

  // Returns true if the vector is empty.
  bool IsEmpty() const;

  // Cleans the vector, i.e. removes zero-values entries, removes duplicates
  // entries and sorts remaining entries in increasing index order.
  // Runs in O(num_entries * log(num_entries)).
  void CleanUp();

  // Returns true if the entries of this SparseVector are in strictly increasing
  // index order and if the vector contains no duplicates nor zero coefficients.
  // Runs in O(num_entries). It is not const because it modifies
  // possibly_contains_duplicates_.
  bool IsCleanedUp() const;

  // Swaps the content of this sparse vector with the one passed as argument.
  // Works in O(1).
  void Swap(SparseVector* other);

  // Populates the current vector from sparse_vector.
  // Runs in O(num_entries).
  void PopulateFromSparseVector(const SparseVector& sparse_vector);

  // Populates the current vector from dense_vector.
  // Runs in O(num_indices_in_dense_vector).
  void PopulateFromDenseVector(const DenseVector& dense_vector);

  // Returns true when the vector contains no duplicates. Runs in
  // O(max_index + num_entries), max_index being the largest index in entry.
  // This method allocates (and deletes) a Boolean array of size max_index.
  // Note that we use a mutable Boolean to make subsequent call runs in O(1).
  bool CheckNoDuplicates() const;

  // Same as CheckNoDuplicates() except it uses a reusable boolean vector
  // to make the code more efficient. Runs in O(num_entries).
  // Note that boolean_vector should be initialized to false before calling this
  // method; It will remain equal to false after calls to CheckNoDuplicates().
  // Note that we use a mutable Boolean to make subsequent call runs in O(1).
  bool CheckNoDuplicates(StrictITIVector<Index, bool>* boolean_vector) const;

  // Defines the coefficient at index, i.e. vector[index] = value;
  void SetCoefficient(Index index, Fractional value);

  // Removes an entry from the vector if present. The order of the other entries
  // is preserved. Runs in O(num_entries).
  void DeleteEntry(Index index);

  // Sets to 0.0 (i.e. remove) all entries whose fabs() is lower or equal to
  // the given threshold.
  void RemoveNearZeroEntries(Fractional threshold);

  // Same as RemoveNearZeroEntries, but the entry magnitude of each row is
  // multiplied by weights[row] before being compared with threshold.
  void RemoveNearZeroEntriesWithWeights(Fractional threshold,
                                        const DenseVector& weights);

  // Moves the entry with given Index to the first position in the vector. If
  // the entry is not present, nothing happens.
  void MoveEntryToFirstPosition(Index index);

  // Moves the entry with given Index to the last position in the vector. If
  // the entry is not present, nothing happens.
  void MoveEntryToLastPosition(Index index);

  // Multiplies all entries by factor.
  // i.e. entry.coefficient *= factor.
  void MultiplyByConstant(Fractional factor);

  // Multiplies all entries by its corresponding factor,
  // i.e. entry.coefficient *= factors[entry.index].
  void ComponentWiseMultiply(const StrictITIVector<Index, Fractional>& factors);

  // Divides all entries by factor.
  // i.e. entry.coefficient /= factor.
  void DivideByConstant(Fractional factor);

  // Divides all entries by its corresponding factor,
  // i.e. entry.coefficient /= factors[entry.index].
  void ComponentWiseDivide(const DenseVector& factors);

  // Populates a dense vector from the sparse vector.
  // Runs in O(num_indices) as the dense vector values have to be reset to 0.0.
  void CopyToDenseVector(Index num_indices, DenseVector* dense_vector) const;

  // Populates a dense vector from the permuted sparse vector.
  // Runs in O(num_indices) as the dense vector values have to be reset to 0.0.
  void PermutedCopyToDenseVector(const IndexPermutation& index_perm,
                                 Index num_indices,
                                 DenseVector* dense_vector) const;

  // Performs the operation dense_vector += multiplier * this.
  // This is known as multiply-accumulate or (fused) multiply-add.
  void AddMultipleToDenseVector(Fractional multiplier,
                                DenseVector* dense_vector) const;

  // WARNING: BOTH vectors (the current and the destination) MUST be "clean",
  // i.e. sorted and without duplicates.
  // Performs the operation accumulator_vector += multiplier * this, removing
  // a given index which must be in both vectors, and pruning new entries that
  // are zero with a relative precision of 2 * std::numeric_limits::epsilon().
  void AddMultipleToSparseVectorAndDeleteCommonIndex(
      Fractional multiplier, Index removed_common_index,
      SparseVector* accumulator_vector) const;

  // Same as AddMultipleToSparseVectorAndDeleteCommonIndex() but instead of
  // deleting the common index, leave it unchanged.
  void AddMultipleToSparseVectorAndIgnoreCommonIndex(
      Fractional multiplier, Index ignored_common_index,
      SparseVector* accumulator_vector) const;

  // Applies the index permutation to all entries: index = index_perm[index];
  void ApplyIndexPermutation(const IndexPermutation& index_perm);

  // Same as ApplyIndexPermutation but deletes the index if index_perm[index]
  // is negative.
  void ApplyPartialIndexPermutation(const IndexPermutation& index_perm);

  // Removes the entries for which index_perm[index] is non-negative and appends
  // them to output. Note that the index of the entries are NOT permuted.
  void MoveTaggedEntriesTo(const IndexPermutation& index_perm,
                           SparseVector* output);

  // Returns the coefficient at position index.
  // Call with care: runs in O(number-of-entries) as entries may not be sorted.
  Fractional LookUpCoefficient(Index index) const;

  // Note this method can only be used when the vector has no duplicates.
  EntryIndex num_entries() const {
    DCHECK(CheckNoDuplicates());
    return EntryIndex(entry_.size());
  }

  // Returns the first entry's index and coefficient; note that 'first' doesn't
  // mean 'entry with the smallest index'.
  // Runs in O(1).
  // Note this method can only be used when the vector has no duplicates.
  Index GetFirstIndex() const {
    DCHECK(CheckNoDuplicates());
    return entry_.front().index;
  }
  Fractional GetFirstCoefficient() const {
    DCHECK(CheckNoDuplicates());
    return entry_.front().coefficient;
  }

  // Like GetFirst*, but for the last entry.
  Index GetLastIndex() const {
    DCHECK(CheckNoDuplicates());
    return entry_.back().index;
  }
  Fractional GetLastCoefficient() const {
    DCHECK(CheckNoDuplicates());
    return entry_.back().coefficient;
  }

  // Allows to loop over the entry indices like this:
  //   for (const EntryIndex i : sparse_vector.AllEntryIndices()) { ... }
  // TODO(user): consider removing this, in favor of the natural range
  // iteration.
  IntegerRange<EntryIndex> AllEntryIndices() const {
    return IntegerRange<EntryIndex>(EntryIndex(0), entry_.size());
  }

  // Returns true if this vector is exactly equal to the given one, i.e. all its
  // index indices and coefficients appear in the same order and are equal.
  bool IsEqualTo(const SparseVector& other) const;

  // An exhaustive, pretty-printed listing of the entries, in their
  // internal order. a.DebugString() == b.DebugString() iff a.IsEqualTo(b).
  std::string DebugString() const;

 protected:
  // The internal storage of entries.
  struct InternalEntry {
    Index index;
    Fractional coefficient;
    // TODO(user): Evaluate the impact of having padding (12 bytes struct when
    //              Fractional is a double).
    // Note(user): The two constructors below are not needed when we switch
    // to initializer lists.
    InternalEntry() : index(0), coefficient(0.0) {}
    InternalEntry(Index i, Fractional c) : index(i), coefficient(c) {}
    bool operator<(const InternalEntry& other) const {
      return index < other.index;
    }
  };

  // TODO(user): Consider making this public and using it instead of EntryRow()
  // EntryCoefficient() in SparseColumn.
  const InternalEntry& entry(EntryIndex i) const { return entry_[i]; }

  // Vector of entries. Not necessarily sorted.
  // TODO(user): try splitting the std::vector<InternalEntry> into two vectors
  // of index and coefficient, like it is done in SparseMatrix.
  StrictITIVector<EntryIndex, InternalEntry> entry_;

  // This is here to speed up the CheckNoDuplicates() methods and is mutable
  // so we can perform checks on const argument.
  mutable bool may_contain_duplicates_;

 private:
  // Actual implementation of AddMultipleToSparseVectorAndDeleteCommonIndex()
  // and AddMultipleToSparseVectorAndIgnoreCommonIndex() which is shared.
  void AddMultipleToSparseVectorInternal(
      bool delete_common_index, Fractional multiplier, Index common_index,
      SparseVector* accumulator_vector) const;
};

template <class Index>
class SparseVector<Index>::Entry {
 public:
  Index index() const { return sparse_vector_.entry(i_).index; }
  Fractional coefficient() const {
    return sparse_vector_.entry(i_).coefficient;
  }

 protected:
  Entry(const SparseVector& sparse_vector, EntryIndex i)
      : sparse_vector_(sparse_vector), i_(i) {}
  const SparseVector& sparse_vector_;
  EntryIndex i_;
};

template <class Index>
class SparseVector<Index>::Iterator : private SparseVector<Index>::Entry {
 public:
  void operator++() { ++Entry::i_; }
  bool operator!=(const Iterator& other) const {
    // This operator is intended for use in natural range iteration ONLY.
    // Therefore, we prefer to use '<' so that a buggy range iteration which
    // start point is *after* its end point stops immediately, instead of
    // iterating 2^(number of bits of EntryIndex) times.
    return internal_entry_index() < other.internal_entry_index();
  }
  const Entry& operator*() const { return *static_cast<const Entry*>(this); }

 private:
  explicit Iterator(const SparseVector& sparse_vector, EntryIndex i)
      : Entry(sparse_vector, i) {
    DCHECK(sparse_vector.CheckNoDuplicates());
  }
  inline EntryIndex internal_entry_index() const { return Entry::i_; }
  friend class SparseVector;
};

template <class Index>
typename SparseVector<Index>::Iterator SparseVector<Index>::begin() const {
  return Iterator(*this, EntryIndex(0));
}

template <class Index>
typename SparseVector<Index>::Iterator SparseVector<Index>::end() const {
  return Iterator(*this, EntryIndex(entry_.size()));
}

// --------------------------------------------------------
// SparseVector implementation
// --------------------------------------------------------
template <typename IndexType>
SparseVector<IndexType>::SparseVector()
    : entry_(), may_contain_duplicates_(false) {}

template <typename IndexType>
void SparseVector<IndexType>::Clear() {
  entry_.clear();
  may_contain_duplicates_ = false;
}

template <typename IndexType>
void SparseVector<IndexType>::ClearAndRelease() {
  StrictITIVector<EntryIndex, InternalEntry> empty;
  empty.swap(entry_);
  may_contain_duplicates_ = false;
}

template <typename IndexType>
void SparseVector<IndexType>::Reserve(EntryIndex size) {
  entry_.reserve(size.value());
}

template <typename IndexType>
bool SparseVector<IndexType>::IsEmpty() const {
  return entry_.empty();
}

template <typename IndexType>
void SparseVector<IndexType>::Swap(SparseVector* other) {
  entry_.swap(other->entry_);
  std::swap(may_contain_duplicates_, other->may_contain_duplicates_);
}

template <typename IndexType>
void SparseVector<IndexType>::CleanUp() {
  std::stable_sort(entry_.begin(), entry_.end());
  EntryIndex new_index(0);
  const EntryIndex num_entries = entry_.size();
  for (EntryIndex i(0); i < num_entries; ++i) {
    if (i + 1 < num_entries && entry_[i + 1].index == entry_[i].index) continue;
    if (entry_[i].coefficient != 0.0) {
      entry_[new_index] = entry_[i];
      ++new_index;
    }
  }
  entry_.resize_down(new_index);
  may_contain_duplicates_ = false;
}

template <typename IndexType>
bool SparseVector<IndexType>::IsCleanedUp() const {
  Index previous_index(-1);
  for (const EntryIndex i : AllEntryIndices()) {
    const Index index = entry(i).index;
    if (index <= previous_index || entry(i).coefficient == 0.0) return false;
    previous_index = index;
  }
  may_contain_duplicates_ = false;
  return true;
}

template <typename IndexType>
void SparseVector<IndexType>::PopulateFromSparseVector(
    const SparseVector& sparse_vector) {
  entry_ = sparse_vector.entry_;
  may_contain_duplicates_ = sparse_vector.may_contain_duplicates_;
}

template <typename IndexType>
void SparseVector<IndexType>::PopulateFromDenseVector(
    const DenseVector& dense_vector) {
  Clear();
  const Index num_indices(dense_vector.size());
  for (Index index(0); index < num_indices; ++index) {
    if (dense_vector[index] != 0.0) {
      SetCoefficient(index, dense_vector[index]);
    }
  }
  may_contain_duplicates_ = false;
}

template <typename IndexType>
bool SparseVector<IndexType>::CheckNoDuplicates(
    StrictITIVector<IndexType, bool>* boolean_vector) const {
  RETURN_VALUE_IF_NULL(boolean_vector, false);
  // Note(user): Using num_entries() or any function that call
  // CheckNoDuplicates() again will cause an infinite loop!
  if (!may_contain_duplicates_ || entry_.size() <= 1) return true;

  // Update size if needed.
  const Index max_index = std::max_element(entry_.begin(), entry_.end())->index;
  if (boolean_vector->size() <= max_index) {
    boolean_vector->resize(max_index + 1, false);
  }

  may_contain_duplicates_ = false;
  for (const EntryIndex i : AllEntryIndices()) {
    const Index index = entry(i).index;
    if ((*boolean_vector)[index]) {
      may_contain_duplicates_ = true;
      break;
    }
    (*boolean_vector)[index] = true;
  }

  // Reset boolean_vector to false.
  for (const EntryIndex i : AllEntryIndices()) {
    (*boolean_vector)[entry(i).index] = false;
  }
  return !may_contain_duplicates_;
}

template <typename IndexType>
bool SparseVector<IndexType>::CheckNoDuplicates() const {
  // Using num_entries() or any function in that will call CheckNoDuplicates()
  // again will cause an infinite loop!
  if (!may_contain_duplicates_ || entry_.size() <= 1) return true;
  StrictITIVector<Index, bool> boolean_vector;
  return CheckNoDuplicates(&boolean_vector);
}

// Do not filter out zero values, as a zero value can be added to reset a
// previous value. Zero values and duplicates will be removed by CleanUp.
template <typename IndexType>
void SparseVector<IndexType>::SetCoefficient(Index index, Fractional value) {
  DCHECK_GE(index, 0);
  // TODO(user): when we drop MSVC2012 in the open-source project,
  // use initializer list-based idiom:  entry_.push_back({index, value});
  entry_.push_back(InternalEntry(index, value));
  may_contain_duplicates_ = true;
}

template <typename IndexType>
void SparseVector<IndexType>::DeleteEntry(Index index) {
  DCHECK(CheckNoDuplicates());
  EntryIndex i(0);
  const EntryIndex end(num_entries());
  while (i < end && entry(i).index != index) {
    ++i;
  }
  if (i == end) return;
  entry_.erase(entry_.begin() + i.value());
}

template <typename IndexType>
void SparseVector<IndexType>::RemoveNearZeroEntries(Fractional threshold) {
  DCHECK(CheckNoDuplicates());
  EntryIndex new_index(0);
  for (const EntryIndex i : AllEntryIndices()) {
    const Fractional magnitude = fabs(entry(i).coefficient);
    if (magnitude > threshold) {
      entry_[new_index] = entry_[i];
      ++new_index;
    }
  }
  entry_.resize_down(new_index);
}

template <typename IndexType>
void SparseVector<IndexType>::RemoveNearZeroEntriesWithWeights(
    Fractional threshold, const DenseVector& weights) {
  DCHECK(CheckNoDuplicates());
  EntryIndex new_index(0);
  for (const EntryIndex i : AllEntryIndices()) {
    if (fabs(entry_[i].coefficient) * weights[entry_[i].index] > threshold) {
      entry_[new_index] = entry_[i];
      ++new_index;
    }
  }
  entry_.resize_down(new_index);
}

template <typename IndexType>
void SparseVector<IndexType>::MoveEntryToFirstPosition(Index index) {
  DCHECK(CheckNoDuplicates());
  for (const EntryIndex i : AllEntryIndices()) {
    if (entry(i).index == index) {
      std::swap(entry_[EntryIndex(0)], entry_[i]);
      return;
    }
  }
}

template <typename IndexType>
void SparseVector<IndexType>::MoveEntryToLastPosition(Index index) {
  DCHECK(CheckNoDuplicates());
  for (const EntryIndex i : AllEntryIndices()) {
    if (entry(i).index == index) {
      std::swap(entry_[num_entries() - 1], entry_[i]);
      return;
    }
  }
}

template <typename IndexType>
void SparseVector<IndexType>::MultiplyByConstant(Fractional factor) {
  for (const EntryIndex i : AllEntryIndices()) {
    entry_[i].coefficient *= factor;
  }
}

template <typename IndexType>
void SparseVector<IndexType>::ComponentWiseMultiply(
    const DenseVector& factors) {
  for (const EntryIndex i : AllEntryIndices()) {
    entry_[i].coefficient *= factors[entry(i).index];
  }
}

template <typename IndexType>
void SparseVector<IndexType>::DivideByConstant(Fractional factor) {
  for (const EntryIndex i : AllEntryIndices()) {
    entry_[i].coefficient /= factor;
  }
}

template <typename IndexType>
void SparseVector<IndexType>::ComponentWiseDivide(const DenseVector& factors) {
  for (const EntryIndex i : AllEntryIndices()) {
    entry_[i].coefficient /= factors[entry(i).index];
  }
}

template <typename IndexType>
void SparseVector<IndexType>::CopyToDenseVector(
    Index num_indices, DenseVector* dense_vector) const {
  RETURN_IF_NULL(dense_vector);
  dense_vector->AssignToZero(num_indices);
  for (const EntryIndex i : AllEntryIndices()) {
    (*dense_vector)[entry(i).index] = entry(i).coefficient;
  }
}

template <typename IndexType>
void SparseVector<IndexType>::PermutedCopyToDenseVector(
    const IndexPermutation& index_perm, Index num_indices,
    DenseVector* dense_vector) const {
  RETURN_IF_NULL(dense_vector);
  dense_vector->AssignToZero(num_indices);
  for (const EntryIndex i : AllEntryIndices()) {
    (*dense_vector)[index_perm[entry(i).index]] = entry(i).coefficient;
  }
}

template <typename IndexType>
void SparseVector<IndexType>::AddMultipleToDenseVector(
    Fractional multiplier, DenseVector* dense_vector) const {
  RETURN_IF_NULL(dense_vector);
  if (multiplier == 0.0) return;
  for (const EntryIndex i : AllEntryIndices()) {
    (*dense_vector)[entry(i).index] += multiplier * entry(i).coefficient;
  }
}

template <typename IndexType>
void SparseVector<IndexType>::AddMultipleToSparseVectorAndDeleteCommonIndex(
    Fractional multiplier, Index removed_common_index,
    SparseVector* accumulator_vector) const {
  AddMultipleToSparseVectorInternal(true, multiplier, removed_common_index,
                                    accumulator_vector);
}

template <typename IndexType>
void SparseVector<IndexType>::AddMultipleToSparseVectorAndIgnoreCommonIndex(
    Fractional multiplier, Index removed_common_index,
    SparseVector* accumulator_vector) const {
  AddMultipleToSparseVectorInternal(false, multiplier, removed_common_index,
                                    accumulator_vector);
}

template <typename IndexType>
void SparseVector<IndexType>::AddMultipleToSparseVectorInternal(
    bool delete_common_index, Fractional multiplier, Index common_index,
    SparseVector* accumulator_vector) const {
  // DCHECK that the input is correct.
  DCHECK(IsCleanedUp());
  DCHECK(accumulator_vector->IsCleanedUp());
  DCHECK(CheckNoDuplicates());
  DCHECK(accumulator_vector->CheckNoDuplicates());
  DCHECK_NE(0.0, LookUpCoefficient(common_index));
  DCHECK_NE(0.0, accumulator_vector->LookUpCoefficient(common_index));

  // Implementation notes: we create a temporary SparseVector "c" to hold the
  // result. We call "a" the first vector (i.e. the current object, which will
  // be multiplied by "multiplier"), and "b" the second vector (which will be
  // swapped with "c" at the end to hold the result).
  // We incrementally build c as: a * multiplier + b.
  const SparseVector& a = *this;
  const SparseVector& b = *accumulator_vector;
  SparseVector c;
  EntryIndex ia(0);  // Index in the vector "a"
  EntryIndex ib(0);  // ... and "b"
  EntryIndex ic(0);  // ... and "c"
  const EntryIndex size_a = a.num_entries();
  const EntryIndex size_b = b.num_entries();
  const int size_adjustment = delete_common_index ? -2 : 0;
  c.entry_.resize(size_a + size_b + size_adjustment, InternalEntry());
  while ((ia < size_a) && (ib < size_b)) {
    const Index index_a = a.entry(ia).index;
    const Index index_b = b.entry(ib).index;
    // Benchmarks done by fdid@ in 2012 showed that it was faster to put the
    // "if" clauses in that specific order.
    if (index_a == index_b) {
      if (index_a != common_index) {
        const Fractional a_coeff_mul = multiplier * a.entry(ia).coefficient;
        const Fractional b_coeff = b.entry(ib).coefficient;
        const Fractional sum = a_coeff_mul + b_coeff;
        // We use the factor 2.0 because the error can be slightly greater than
        // 1ulp, and we don't want to leave such near zero entries.
        if (fabs(sum) > 2.0 * std::numeric_limits<Fractional>::epsilon() *
                            std::max(fabs(a_coeff_mul), fabs(b_coeff))) {
          c.entry_[ic] = InternalEntry(index_a, sum);
          ++ic;
        }
      } else if (!delete_common_index) {
        c.entry_[ic] = b.entry(ib);
        ++ic;
      }
      ++ia;
      ++ib;
    } else if (index_a < index_b) {
      c.entry_[ic] =
          InternalEntry(index_a, multiplier * a.entry(ia).coefficient);
      ++ia;
      ++ic;
    } else {  // index_b < index_a
      c.entry_[ic] = b.entry_[ib];
      ++ib;
      ++ic;
    }
  }
  while (ia < size_a) {
    c.entry_[ic] =
        InternalEntry(a.entry(ia).index, multiplier * a.entry(ia).coefficient);
    ++ia;
    ++ic;
  }
  while (ib < size_b) {
    c.entry_[ic] = b.entry_[ib];
    ++ib;
    ++ic;
  }
  c.entry_.resize_down(ic);
  c.may_contain_duplicates_ = false;
  c.Swap(accumulator_vector);
}

template <typename IndexType>
void SparseVector<IndexType>::ApplyIndexPermutation(
    const IndexPermutation& index_perm) {
  for (const EntryIndex i : AllEntryIndices()) {
    entry_[i].index = index_perm[entry_[i].index];
  }
}

template <typename IndexType>
void SparseVector<IndexType>::ApplyPartialIndexPermutation(
    const IndexPermutation& index_perm) {
  EntryIndex new_index(0);
  for (const EntryIndex i : AllEntryIndices()) {
    const Index index = entry_[i].index;
    if (index_perm[index] >= 0) {
      entry_[new_index].index = index_perm[index];
      entry_[new_index].coefficient = entry_[i].coefficient;
      ++new_index;
    }
  }
  entry_.resize_down(new_index);
}

template <typename IndexType>
void SparseVector<IndexType>::MoveTaggedEntriesTo(
    const IndexPermutation& index_perm, SparseVector* output) {
  // Note that this function is called many times, so performance does matter
  // and it is why we optimized the "nothing to do" case.
  const EntryIndex end(entry_.size());
  EntryIndex i(0);
  while (true) {
    if (i >= end) return;  // "nothing to do" case.
    if (index_perm[entry_[i].index] >= 0) break;
    ++i;
  }
  output->entry_.push_back(entry_[i]);
  for (EntryIndex j(i + 1); j < end; ++j) {
    if (index_perm[entry_[j].index] < 0) {
      entry_[i] = entry_[j];
      ++i;
    } else {
      output->entry_.push_back(entry_[j]);
    }
  }
  entry_.resize_down(i);

  // TODO(user): In the way we use this function, we know that will not
  // happen, but it is better to be careful so we can check that properly in
  // debug mode.
  output->may_contain_duplicates_ = true;
}

template <typename IndexType>
Fractional SparseVector<IndexType>::LookUpCoefficient(Index index) const {
  Fractional value(0.0);
  for (const EntryIndex i : AllEntryIndices()) {
    if (entry(i).index == index) {
      // Keep in mind the vector may contains several entries with the same
      // index. In such a case the last one is returned.
      // TODO(user): investigate whether an optimized version of
      // LookUpCoefficient for "clean" columns yields speed-ups.
      value = entry(i).coefficient;
    }
  }
  return value;
}

template <typename IndexType>
bool SparseVector<IndexType>::IsEqualTo(const SparseVector& other) const {
  // We do not take into account the mutable value may_contain_duplicates_.
  if (num_entries() != other.num_entries()) return false;
  for (const EntryIndex i : AllEntryIndices()) {
    if (entry(i).index != other.entry(i).index) return false;
    if (entry(i).coefficient != other.entry(i).coefficient) return false;
  }
  return true;
}

template <typename IndexType>
std::string SparseVector<IndexType>::DebugString() const {
  std::string s;
  for (const EntryIndex i : AllEntryIndices()) {
    if (i != 0) s += ", ";
    StringAppendF(&s, "[%d]=%g", entry(i).index.value(), entry(i).coefficient);
  }
  return s;
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_SPARSE_VECTOR_H_
