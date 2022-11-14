// Copyright 2010-2022 Google LLC
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

#include <cmath>

#include "ortools/base/accurate_sum.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
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
template <class DenseRowOrColumn1, class DenseRowOrColumn2>
Fractional ScalarProduct(const DenseRowOrColumn1& u,
                         const DenseRowOrColumn2& v) {
  DCHECK_EQ(u.size().value(), v.size().value());
  Fractional sum(0.0);
  typename DenseRowOrColumn1::IndexType i(0);
  typename DenseRowOrColumn2::IndexType j(0);
  const size_t num_blocks = u.size().value() / 4;
  for (size_t block = 0; block < num_blocks; ++block) {
    // Computing the sum of 4 elements at once may allow the compiler to
    // generate more efficient code, e.g. using SIMD and checking the loop
    // condition much less frequently.
    //
    // This produces different results from the case where each multiplication
    // is added to sum separately. An extreme example of this can be derived
    // using the fact that 1e11 + 2e-6 == 1e11, but 1e11 + 8e-6 > 1e11.
    //
    // While the results are different, they aren't necessarily better or worse.
    // Typically, sum will be of larger magnitude than any individual
    // multiplication, so one might expect, in practice, this method to yield
    // more accurate results. However, if accuracy is vital, use the precise
    // version.
    sum += (u[i++] * v[j++]) + (u[i++] * v[j++]) + (u[i++] * v[j++]) +
           (u[i++] * v[j++]);
  }
  while (i < u.size()) {
    sum += u[i++] * v[j++];
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

template <class DenseRowOrColumn>
Fractional ScalarProduct(const DenseRowOrColumn& u, const ScatteredColumn& v) {
  DCHECK_EQ(u.size().value(), v.values.size().value());
  if (v.ShouldUseDenseIteration()) {
    return ScalarProduct(u, v.values);
  }
  Fractional sum = 0.0;
  for (const auto e : v) {
    sum += (u[typename DenseRowOrColumn::IndexType(e.row().value())] *
            e.coefficient());
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

// Computes a scalar product for entries with index not greater than max_index.
template <class DenseRowOrColumn>
Fractional PartialScalarProduct(const DenseRowOrColumn& u,
                                const SparseColumn& v, int max_index) {
  Fractional sum(0.0);
  for (const SparseColumn::Entry e : v) {
    if (e.row().value() >= max_index) {
      return sum;
    }
    sum += u[typename DenseRowOrColumn::IndexType(e.row().value())] *
           e.coefficient();
  }
  return sum;
}

// Returns the norm^2 (sum of the square of the entries) of the given column.
// The precise version uses KahanSum and are about two times slower.
Fractional SquaredNorm(const SparseColumn& v);
Fractional SquaredNorm(const DenseColumn& column);
Fractional SquaredNorm(const ColumnView& v);
Fractional SquaredNorm(const ScatteredColumn& v);
Fractional PreciseSquaredNorm(const SparseColumn& v);
Fractional PreciseSquaredNorm(const DenseColumn& column);
Fractional PreciseSquaredNorm(const ScatteredColumn& v);

// Returns the maximum of the |coefficients| of 'v'.
Fractional InfinityNorm(const DenseColumn& v);
Fractional InfinityNorm(const SparseColumn& v);
Fractional InfinityNorm(const ColumnView& v);

// Returns the fraction of non-zero entries of the given row.
//
// TODO(user): Take a Scattered row/col instead. This is only used to report
// stats, but we should still have a sparse version to do it faster.
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
Fractional RestrictedInfinityNorm(const ColumnView& column,
                                  const DenseBooleanColumn& rows_to_consider,
                                  RowIndex* row_index);

// Sets to false the entry b[row] if column[row] is non null.
// Note that if 'b' was true only on the non-zero position of column, this can
// be used as a fast way to clear 'b'.
void SetSupportToFalse(const ColumnView& column, DenseBooleanColumn* b);

// Returns true iff for all 'row' we have '|column[row]| <= radius[row]'.
bool IsDominated(const ColumnView& column, const DenseColumn& radius);

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

// Returns true if the given vector of bool is all false.
template <typename BoolVector>
bool IsAllFalse(const BoolVector& v) {
  return std::all_of(v.begin(), v.end(), [](bool value) { return !value; });
}

// Permutes the given dense vector. It uses for this an all zero scratchpad.
template <typename IndexType, typename PermutationIndexType>
inline void PermuteWithScratchpad(
    const Permutation<PermutationIndexType>& permutation,
    StrictITIVector<IndexType, Fractional>* zero_scratchpad,
    StrictITIVector<IndexType, Fractional>* input_output) {
  DCHECK(IsAllZero(*zero_scratchpad));
  const IndexType size = input_output->size();
  zero_scratchpad->swap(*input_output);
  input_output->resize(size, 0.0);
  for (IndexType index(0); index < size; ++index) {
    const Fractional value = (*zero_scratchpad)[index];
    if (value != 0.0) {
      const IndexType permuted_index(
          permutation[PermutationIndexType(index.value())].value());
      (*input_output)[permuted_index] = value;
    }
  }
  zero_scratchpad->assign(size, 0.0);
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
    (*zero_scratchpad)[index_ref] = 0.0;
    const IndexType permuted_index(permutation[index_ref]);
    (*output)[permuted_index] = value;
    index_ref = permuted_index;
  }
}

// Sets a dense vector for which the non zeros are known to be non_zeros.
template <typename IndexType, typename ScatteredRowOrCol>
inline void ClearAndResizeVectorWithNonZeros(IndexType size,
                                             ScatteredRowOrCol* v) {
  // Only use the sparse version if there is less than 5% non-zeros positions
  // compared to the wanted size. Note that in most cases the vector will
  // already be of the correct size.
  const double kSparseThreshold = 0.05;
  if (!v->non_zeros.empty() &&
      v->non_zeros.size() < kSparseThreshold * size.value()) {
    for (const IndexType index : v->non_zeros) {
      DCHECK_LT(index, v->values.size());
      (*v)[index] = 0.0;
    }
    v->values.resize(size, 0.0);
    DCHECK(IsAllZero(v->values));
  } else {
    v->values.AssignToZero(size);
  }
  v->non_zeros.clear();
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
// to be wrong.
template <bool supported_infinity_is_positive>
class SumWithOneMissing {
 public:
  SumWithOneMissing() : num_infinities_(0), sum_() {}

  void Add(Fractional x) {
    DCHECK(!std::isnan(x));

    if (!IsFinite(x)) {
      DCHECK_EQ(x, Infinity());
      ++num_infinities_;
      return;
    }

    // If we overflow, then there is not much we can do. This is needed
    // because KahanSum seems to give nan if we try to add stuff to an
    // infinite sum.
    if (!IsFinite(sum_.Value())) return;

    sum_.Add(x);
  }

  void RemoveOneInfinity() {
    DCHECK_GE(num_infinities_, 1);
    --num_infinities_;
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

  // When the term we substract has a big magnitude, the SumWithout() can be
  // quite imprecise. On can use these version to have more defensive bounds.
  Fractional SumWithoutLb(Fractional c) const {
    if (!IsFinite(c)) return SumWithout(c);
    return SumWithout(c) - std::abs(c) * 1e-12;
  }

  Fractional SumWithoutUb(Fractional c) const {
    if (!IsFinite(c)) return SumWithout(c);
    return SumWithout(c) + std::abs(c) * 1e-12;
  }

 private:
  Fractional Infinity() const {
    return supported_infinity_is_positive ? kInfinity : -kInfinity;
  }

  // Count how many times Add() was called with an infinite value.
  int num_infinities_;
  KahanSum sum_;  // stripped of all the infinite values.
};
typedef SumWithOneMissing<true> SumWithPositiveInfiniteAndOneMissing;
typedef SumWithOneMissing<false> SumWithNegativeInfiniteAndOneMissing;

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_LP_UTILS_H_
