// Copyright 2010-2017 Google
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


#ifndef OR_TOOLS_LP_DATA_PERMUTATION_H_
#define OR_TOOLS_LP_DATA_PERMUTATION_H_

#include "ortools/lp_data/lp_types.h"
#include "ortools/util/return_macros.h"

namespace operations_research {
namespace glop {

// Permutation<IndexType> is a template class for storing and using
// row- and column- permutations, when instantiated with RowIndex and ColIndex
// respectively.
//
// By a row permutation we mean a permutation that maps the row 'i' of a matrix
// (or column vector) to the row 'permutation[i]' and in a similar fashion by a
// column permutation we mean a permutation that maps the column 'j' of a matrix
// (or row vector) to the column 'permutation[j]'.
//
// A permutation can be represented as a matrix P, but it gets a bit tricky
// here: P.x permutes the rows of x according to the permutation P but x^T.P
// permutes the columns of x^T (a row vector) using the INVERSE permutation.
// That is, to permute the columns of x^T using P, one has to compute
// x^T.P^{-1} but P^{-1} = P^T so the notation is consistent: If P.x permutes x,
// then (P.x)^T = x^T.P^T permutes x^T with the same permutation.
//
// So to be clear, if P and Q are permutation matrices, the matrix P.A.Q^{-1}
// is the image of A through the row permutation P and column permutation Q.
template <typename IndexType>
class Permutation {
 public:
  Permutation() : perm_() {}

  explicit Permutation(IndexType size) : perm_(size.value(), IndexType(0)) {}

  IndexType size() const { return IndexType(perm_.size()); }
  bool empty() const { return perm_.empty(); }

  void clear() { perm_.clear(); }

  void resize(IndexType size, IndexType value) {
    perm_.resize(size.value(), value);
  }

  void assign(IndexType size, IndexType value) {
    perm_.assign(size.value(), value);
  }

  IndexType& operator[](IndexType i) { return perm_[i]; }

  const IndexType operator[](IndexType i) const { return perm_[i]; }

  // Populates the calling object with the inverse permutation of the parameter
  // inverse.
  void PopulateFromInverse(const Permutation& inverse);

  // Populates the calling object with the identity permutation.
  void PopulateFromIdentity();

  // Populates the calling object with a random permutation.
  void PopulateRandomly();

  // Returns true if the calling object contains a permutation, false otherwise.
  bool Check() const;

  // Returns the signature of a permutation in O(n), where n is the permutation
  // size.
  // The signature of a permutation is the product of the signature of
  // the cycles defining the permutation.
  // The signature of an odd cycle is 1, while the signature of an even cycle
  // is -1. (Remembering hint: the signature of a swap (a 2-cycle) is -1.)
  int ComputeSignature() const;

 private:
  ITIVector<IndexType, IndexType> perm_;

  DISALLOW_COPY_AND_ASSIGN(Permutation);
};

typedef Permutation<RowIndex> RowPermutation;
typedef Permutation<ColIndex> ColumnPermutation;

// Applies the permutation perm to the vector b. Overwrites result to store
// the result.
// TODO(user): Try to restrict this method to using the same integer type in
// the permutation and for the vector indices, i.e.
// IndexType == ITIVectorType::IndexType. Some client code will need to be
// refactored.
template <typename IndexType, typename ITIVectorType>
void ApplyPermutation(const Permutation<IndexType>& perm,
                      const ITIVectorType& b, ITIVectorType* result);

// Applies the inverse of perm to the vector b. Overwrites result to store
// the result.
template <typename IndexType, typename ITIVectorType>
void ApplyInversePermutation(const Permutation<IndexType>& perm,
                             const ITIVectorType& b, ITIVectorType* result);

// Specialization of ApplyPermutation(): apply a column permutation to a
// row-indexed vector v.
template <typename RowIndexedVector>
void ApplyColumnPermutationToRowIndexedVector(
    const Permutation<ColIndex>& col_perm, RowIndexedVector* v) {
  RowIndexedVector temp_v = *v;
  ApplyPermutation(col_perm, temp_v, v);
}

// --------------------------------------------------------
// Implementation
// --------------------------------------------------------

template <typename IndexType>
void Permutation<IndexType>::PopulateFromInverse(const Permutation& inverse) {
  const size_t size = inverse.perm_.size();
  perm_.resize(size);
  for (IndexType i(0); i < size; ++i) {
    perm_[inverse[i]] = i;
  }
}

template <typename IndexType>
void Permutation<IndexType>::PopulateFromIdentity() {
  const size_t size = perm_.size();
  perm_.resize(size, IndexType(0));
  for (IndexType i(0); i < size; ++i) {
    perm_[i] = i;
  }
}

template <typename IndexType>
void Permutation<IndexType>::PopulateRandomly() {
  PopulateFromIdentity();
  std::random_shuffle(perm_.begin(), perm_.end());
}

template <typename IndexType>
bool Permutation<IndexType>::Check() const {
  const size_t size = perm_.size();
  ITIVector<IndexType, bool> visited(size, false);
  for (IndexType i(0); i < size; ++i) {
    if (perm_[i] < 0 || perm_[i] >= size) {
      return false;
    }
    visited[perm_[i]] = true;
  }
  for (IndexType i(0); i < size; ++i) {
    if (!visited[i]) {
      return false;
    }
  }
  return true;
}

template <typename IndexType>
int Permutation<IndexType>::ComputeSignature() const {
  const size_t size = perm_.size();
  ITIVector<IndexType, bool> visited(size);
  DCHECK(Check());
  int signature = 1;
  for (IndexType i(0); i < size; ++i) {
    if (!visited[i]) {
      int cycle_size = 0;
      IndexType j = i;
      do {
        j = perm_[j];
        visited[j] = true;
        ++cycle_size;
      } while (j != i);
      if ((cycle_size & 1) == 0) {
        signature = -signature;
      }
    }
  }
  return signature;
}

template <typename IndexType, typename ITIVectorType>
void ApplyPermutation(const Permutation<IndexType>& perm,
                      const ITIVectorType& b, ITIVectorType* result) {
  RETURN_IF_NULL(result);
  const IndexType size(perm.size());
  if (size == 0) return;
  DCHECK_EQ(size.value(), b.size().value());
  result->resize(b.size(), /*whatever junk value*/ b.back());
  for (IndexType i(0); i < size; ++i) {
    const typename ITIVectorType::IndexType ith_index(i.value());
    const typename ITIVectorType::IndexType permuted(perm[i].value());
    (*result)[permuted] = b[ith_index];
  }
}

template <typename IndexType, typename ITIVectorType>
void ApplyInversePermutation(const Permutation<IndexType>& perm,
                             const ITIVectorType& b, ITIVectorType* result) {
  RETURN_IF_NULL(result);
  const IndexType size(perm.size().value());
  if (size == 0) return;
  DCHECK_EQ(size.value(), b.size().value());
  result->resize(b.size(), /*whatever junk value*/ b.back());
  for (IndexType i(0); i < size; ++i) {
    const typename ITIVectorType::IndexType ith_index(i.value());
    const typename ITIVectorType::IndexType permuted(perm[i].value());
    (*result)[ith_index] = b[permuted];
  }
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_PERMUTATION_H_
