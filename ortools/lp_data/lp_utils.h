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


// Basic utility functions on Fractional or row/column of Fractional.

#ifndef OR_TOOLS_LP_DATA_LP_UTILS_H_
#define OR_TOOLS_LP_DATA_LP_UTILS_H_

#include "ortools/base/accurate_sum.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse_column.h"

namespace operations_research {
namespace glop {

// TODO(user): For some Fractional types, it may not gain much (or even nothing
// if we are in infinite precision) to use this sum. A solution is to templatize
// this class and specialize it to a normal sum for the Fractional type we want
// so in this case the PreciseXXX() functions below will become equivalent to
// their normal version.
typedef AccurateSum<Fractional> KahanSum;

// Returns the square of a Fractional.
// Useful to shorten the code when f is an expression or a long name.
inline Fractional Square(Fractional f) { return f * f; }

// Returns distance from a given fractional number to the closest integer. It
// means that the result is always contained in range of [0.0, 0.5].
static inline Fractional Fractionality(Fractional f) {
  return std::abs(f - std::round(f));
}

// Returns the scalar product between u and v.
// The precise versions use KahanSum and are about two times slower.
template <class DenseRowOrColumn, class DenseRowOrColumn2>
Fractional ScalarProduct(const DenseRowOrColumn& u,
                         const DenseRowOrColumn2& v) {
  DCHECK_EQ(u.size().value(), v.size().value());
  Fractional sum(0.0);
  for (typename DenseRowOrColumn::IndexType i(0); i < u.size(); ++i) {
    sum += u[i] * v[typename DenseRowOrColumn2::IndexType(i.value())];
  }
  return sum;
}

// Note: This version is heavily used in the pricing.
// TODO(user): Optimize this more (SSE or unroll with two sums). Another
// option is to skip the u[col] that are 0.0 rather than fetching the coeff
// and doing a Fractional multiplication.
template <class DenseRowOrColumn>
Fractional ScalarProduct(const DenseRowOrColumn& u, const SparseColumn& v) {
  Fractional sum(0.0);
  for (const SparseColumn::Entry e : v) {
    sum += u[typename DenseRowOrColumn::IndexType(e.row().value())] *
           e.coefficient();
  }
  return sum;
}

template <class DenseRowOrColumn, class DenseRowOrColumn2>
Fractional PreciseScalarProduct(const DenseRowOrColumn& u,
                                const DenseRowOrColumn2& v) {
  DCHECK_EQ(u.size().value(), v.size().value());
  KahanSum sum;
  for (typename DenseRowOrColumn::IndexType i(0); i < u.size(); ++i) {
    sum.Add(u[i] * v[typename DenseRowOrColumn2::IndexType(i.value())]);
  }
  return sum.Value();
}

template <class DenseRowOrColumn>
Fractional PreciseScalarProduct(const DenseRowOrColumn& u,
                                const SparseColumn& v) {
  KahanSum sum;
  for (const SparseColumn::Entry e : v) {
    sum.Add(u[typename DenseRowOrColumn::IndexType(e.row().value())] *
            e.coefficient());
  }
  return sum.Value();
}

template <class DenseRowOrColumn>
Fractional PreciseScalarProduct(const DenseRowOrColumn& u,
                                ScatteredColumnReference v) {
  DCHECK_EQ(u.size().value(), v.dense_column.size().value());
  if (v.non_zero_rows.size() >
      ScatteredColumnReference::kDenseThresholdForPreciseSum *
          v.dense_column.size().value()) {
    return PreciseScalarProduct(u, v.dense_column);
  }
  KahanSum sum;
  for (const RowIndex row : v.non_zero_rows) {
    sum.Add(u[typename DenseRowOrColumn::IndexType(row.value())] * v[row]);
  }
  return sum.Value();
}

// Returns the norm^2 (sum of the square of the entries) of the given column.
// The precise version uses KahanSum and are about two times slower.
Fractional SquaredNorm(const SparseColumn& v);
Fractional SquaredNorm(const DenseColumn& v);
Fractional PreciseSquaredNorm(const SparseColumn& v);
Fractional PreciseSquaredNorm(const DenseColumn& v);
Fractional PreciseSquaredNorm(ScatteredColumnReference v);

// Returns the maximum of the |coefficients| of 'v'.
Fractional InfinityNorm(const DenseColumn& v);
Fractional InfinityNorm(const SparseColumn& v);

// Returns the fraction of non-zero entries of the given row.
double Density(const DenseRow& row);

// Sets to 0.0 all entries of the given row whose fabs() is lower than the given
// threshold.
void RemoveNearZeroEntries(Fractional threshold, DenseRow* row);
void RemoveNearZeroEntries(Fractional threshold, DenseColumn* column);

// Transposition functions implemented below with a cast so it should actually
// have no complexity cost.
const DenseRow& Transpose(const DenseColumn& col);
const DenseColumn& Transpose(const DenseRow& row);

// Returns the maximum of the |coefficients| of the given column restricted
// to the rows_to_consider. Also returns the first RowIndex 'row' that attains
// this maximum. If the maximum is 0.0, then row_index is left untouched.
Fractional RestrictedInfinityNorm(const SparseColumn& column,
                                  const DenseBooleanColumn& rows_to_consider,
                                  RowIndex* row_index);

// Sets to false the entry b[row] if column[row] is non null.
// Note that if 'b' was true only on the non-zero position of column, this can
// be used as a fast way to clear 'b'.
void SetSupportToFalse(const SparseColumn& column, DenseBooleanColumn* b);

// Returns true iff for all 'row' we have '|column[row]| <= radius[row]'.
bool IsDominated(const SparseColumn& column, const DenseColumn& radius);

// This cast based implementation should be safe, as long as DenseRow and
// DenseColumn are implemented by the same underlying type.
// We still do some DCHECK to be sure it works as expected in addition to the
// unit tests.
inline const DenseRow& Transpose(const DenseColumn& col) {
  const DenseRow& row = reinterpret_cast<const DenseRow&>(col);
  DCHECK_EQ(col.size(), ColToRowIndex(row.size()));
  DCHECK(col.empty() || (&(col[RowIndex(0)]) == &(row[ColIndex(0)])));
  return row;
}

// Similar comment as the other Transpose() implementation above.
inline const DenseColumn& Transpose(const DenseRow& row) {
  const DenseColumn& col = reinterpret_cast<const DenseColumn&>(row);
  DCHECK_EQ(col.size(), ColToRowIndex(row.size()));
  DCHECK(col.empty() || (&(col[RowIndex(0)]) == &(row[ColIndex(0)])));
  return col;
}

// Computes the positions of the non-zeros of a dense vector.
template <typename IndexType>
inline void ComputeNonZeros(const StrictITIVector<IndexType, Fractional>& input,
                            std::vector<IndexType>* non_zeros) {
  non_zeros->clear();
  const IndexType end = input.size();
  for (IndexType index(0); index < end; ++index) {
    if (input[index] != 0.0) {
      non_zeros->push_back(index);
    }
  }
}

// Returns true if the given Fractional container is all zeros.
template <typename Container>
inline bool IsAllZero(const Container& input) {
  for (Fractional value : input) {
    if (value != 0.0) return false;
  }
  return true;
}

// Permutes the given dense vector and computes the positions of its non-zeros.
// It uses for this an all zero dense vector (zero_scratchpad).
// This operation is efficient for a sparse vector because:
// - It combines two iterations in one.
// - It avoids cache pollution by not looking at unecessary permuted locations.
// Note that the produced non_zeros is not ordered which may be a drawback.
// TODO(user): Investigate alternatives with an ordered non_zeros.
template <typename IndexType, typename PermutationIndexType,
          typename NonZeroIndexType>
inline void PermuteAndComputeNonZeros(
    const Permutation<PermutationIndexType>& permutation,
    StrictITIVector<IndexType, Fractional>* zero_scratchpad,
    StrictITIVector<IndexType, Fractional>* output,
    std::vector<NonZeroIndexType>* non_zeros) {
  non_zeros->clear();
  DCHECK(IsAllZero(*zero_scratchpad));
  zero_scratchpad->swap(*output);
  output->resize(zero_scratchpad->size(), 0.0);
  const IndexType end = zero_scratchpad->size();
  for (IndexType index(0); index < end; ++index) {
    const Fractional value = (*zero_scratchpad)[index];
    if (value != 0.0) {
      (*zero_scratchpad)[index] = 0.0;
      const IndexType permuted_index(
          permutation[PermutationIndexType(index.value())].value());
      (*output)[permuted_index] = value;
      non_zeros->push_back(NonZeroIndexType(permuted_index.value()));
    }
  }
}

// Same as PermuteAndComputeNonZeros() except that we assume that the given
// non-zeros are the initial non-zeros positions of output.
template <typename IndexType>
inline void PermuteWithKnownNonZeros(
    const Permutation<IndexType>& permutation,
    StrictITIVector<IndexType, Fractional>* zero_scratchpad,
    StrictITIVector<IndexType, Fractional>* output,
    std::vector<IndexType>* non_zeros) {
  DCHECK(IsAllZero(*zero_scratchpad));
  zero_scratchpad->swap(*output);
  output->resize(zero_scratchpad->size(), 0.0);
  for (IndexType& index_ref : *non_zeros) {
    const Fractional value = (*zero_scratchpad)[index_ref];
    DCHECK_NE(value, 0.0);
    (*zero_scratchpad)[index_ref] = 0.0;
    const IndexType permuted_index(permutation[index_ref]);
    (*output)[permuted_index] = value;
    index_ref = permuted_index;
  }
}

// Same algorithm as PermuteAndComputeNonZeros() above when the non-zeros are
// not needed. This should be faster than a simple ApplyPermutation() if the
// input vector is relatively sparse. The input is the initial value of output.
inline void ApplyPermutationWhenInputIsProbablySparse(
    const Permutation<RowIndex>& permutation, DenseColumn* zero_scratchpad,
    DenseColumn* output) {
  const RowIndex num_rows(permutation.size());
  DCHECK(IsAllZero(*zero_scratchpad));
  zero_scratchpad->swap(*output);
  output->resize(num_rows, 0.0);
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional value = (*zero_scratchpad)[row];
    if (value != 0.0) {
      (*zero_scratchpad)[row] = 0.0;
      (*output)[permutation[row]] = value;
    }
  }
}

// Sets a dense vector for which the non zeros are known to be non_zeros.
template <typename Vector, typename IndexType>
inline void ClearAndResizeVectorWithNonZeros(
    IndexType size, Vector* v, std::vector<IndexType>* non_zeros) {
  // Only use the sparse version if there is less than 5% non-zeros positions
  // compared to the wanted size. Note that in most cases the vector will
  // already be of the correct size.
  const double kSparseThreshold = 0.05;
  if (non_zeros->size() < kSparseThreshold * size.value()) {
    for (const IndexType index : *non_zeros) {
      DCHECK_LT(index, v->size());
      (*v)[index] = 0.0;
    }
    v->resize(size, 0.0);
    DCHECK(IsAllZero(*v));
  } else {
    v->AssignToZero(size);
  }
  non_zeros->clear();
}

// Changes the sign of all the entries in the given vector.
template <typename IndexType>
inline void ChangeSign(StrictITIVector<IndexType, Fractional>* data) {
  const IndexType end = data->size();
  for (IndexType i(0); i < end; ++i) {
    (*data)[i] = -(*data)[i];
  }
}

// Given N Fractional elements, this class maintains their sum and can
// provide, for each element X, the sum of all elements except X.
// The subtelty is that it works well with infinities: for example, if there is
// exactly one infinite element X, then SumWithout(X) will be finite.
//
// Two flavors of this class are provided: SumWithPositiveInfiniteAndOneMissing
// supports calling Add() with normal numbers and positive infinities (and will
// DCHECK() that), and SumWithNegativeInfiniteAndOneMissing does the same with
// negative infinities.
//
// The numerical accuracy suffers however. If X is 1e100 and SumWithout(X)
// should be 1e-100, then the value actually returned by SumWithout(X) is likely
// to be wrong (by up to std::numeric_limits<Fractional>::epsilon() ^ 2).
template <bool supported_infinity_is_positive>
class SumWithOneMissing {
 public:
  SumWithOneMissing() : num_infinities_(0), sum_() {}

  void Add(Fractional x) {
    if (num_infinities_ > 1) return;
    if (IsFinite(x)) {
      sum_.Add(x);
      return;
    }
    DCHECK_EQ(Infinity(), x);
    ++num_infinities_;
  }

  Fractional Sum() const {
    if (num_infinities_ > 0) return Infinity();
    return sum_.Value();
  }

  Fractional SumWithout(Fractional x) const {
    if (IsFinite(x)) {
      if (num_infinities_ > 0) return Infinity();
      return sum_.Value() - x;
    }
    DCHECK_EQ(Infinity(), x);
    if (num_infinities_ > 1) return Infinity();
    return sum_.Value();
  }

 private:
  Fractional Infinity() const {
    return supported_infinity_is_positive ? kInfinity : -kInfinity;
  }

  // Count how many times Add() was called with an infinite value. The count is
  // stopped at 2 to be a bit faster.
  int num_infinities_;
  KahanSum sum_;  // stripped of all the infinite values.
};
typedef SumWithOneMissing<true> SumWithPositiveInfiniteAndOneMissing;
typedef SumWithOneMissing<false> SumWithNegativeInfiniteAndOneMissing;

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_LP_UTILS_H_
