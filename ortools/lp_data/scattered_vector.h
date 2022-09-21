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

#ifndef OR_TOOLS_LP_DATA_SCATTERED_VECTOR_H_
#define OR_TOOLS_LP_DATA_SCATTERED_VECTOR_H_

#include <cmath>
#include <limits>

#include "ortools/base/basictypes.h"
#include "ortools/base/logging.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace glop {

// A class representing an entry of a scattered vector.  The i-th nonzero
// element of the vector is assumed to be located at indices[i] and its value is
// coefficients[indices[i]], i.e., coefficients is a dense array.
template <typename IndexType>
class ScatteredVectorEntry {
 public:
  using Index = IndexType;

  Index index() const { return index_[i_.value()]; }
  Fractional coefficient() const {
    return coefficient_[index_[i_.value()].value()];
  }

 protected:
  ScatteredVectorEntry(const Index* indices, const Fractional* coefficients,
                       EntryIndex i)
      : i_(i), index_(indices), coefficient_(coefficients) {}

  EntryIndex i_;
  const Index* index_;
  const Fractional* coefficient_;
};

// A simple struct that contains a DenseVector and its non-zero indices.
// TODO(user): This should be changed from struct to class.
template <typename Index,
          typename Iterator = VectorIterator<ScatteredVectorEntry<Index>>>
struct ScatteredVector {
  StrictITIVector<Index, Fractional> values;

  // This can be left empty in which case we just have the dense representation
  // above. Otherwise, it should always be a superset of the actual non-zeros.
  bool non_zeros_are_sorted = false;
  std::vector<Index> non_zeros;

  // Temporary vector used in some sparse computation on the ScatteredVector.
  // True indicates a possible non-zero value. Note that its state is not always
  // consistent.
  StrictITIVector<Index, bool> is_non_zero;

  // In many cases there is a choice between treating the ScatteredVector as
  // dense or as sparse.  By default, dense algorithms are used when the
  // proportion of non-zero entries is greater than
  // kDefaultRatioForUsingDenseIteration.
  //
  // TODO(user): The constant should depend on what algorithm is used. Clearing
  // a dense vector is a lot more efficient than doing more complex stuff. Clean
  // this up by extracting all the currently used constants in one place with
  // meaningful names.
  constexpr static const double kDefaultRatioForUsingDenseIteration = 0.8;

  Fractional operator[](Index index) const { return values[index]; }
  Fractional& operator[](Index index) { return values[index]; }

  // The iterator syntax for (auto entry : v) where v is a ScatteredVector only
  // works when non_zeros is populated (i.e., when the vector is treated as
  // sparse).
  Iterator begin() const {
    DCHECK(!non_zeros.empty() || IsAllZero(values));
    return Iterator(this->non_zeros.data(), this->values.data(), EntryIndex(0));
  }
  Iterator end() const {
    return Iterator(this->non_zeros.data(), this->values.data(),
                    EntryIndex(non_zeros.size()));
  }

  // Add the given value to the vector at position index.  This interface
  // encapsulates usage of the "is_non_zero" array, which should not be
  // explicitly referenced outside of this struct.
  void Add(Index index, Fractional value) {
    values[index] += value;
    if (!is_non_zero[index] && value != 0.0) {
      is_non_zero[index] = true;
      non_zeros.push_back(index);
      non_zeros_are_sorted = false;
    }
  }

  // Sorting the non-zeros is not always needed, but it allows us to have
  // exactly the same behavior while using a sparse iteration or a dense one. So
  // we always do it after a Solve().
  void SortNonZerosIfNeeded() {
    if (!non_zeros_are_sorted) {
      std::sort(non_zeros.begin(), non_zeros.end());
      non_zeros_are_sorted = true;
    }
  }

  // Returns true if it is more advantageous to use a dense iteration rather
  // than using the non-zeros positions.
  bool ShouldUseDenseIteration(
      double ratio_for_using_dense_representation) const {
    if (non_zeros.empty()) return true;
    return static_cast<double>(non_zeros.size()) >
           ratio_for_using_dense_representation *
               static_cast<double>(values.size().value());
  }

  bool ShouldUseDenseIteration() const {
    return ShouldUseDenseIteration(kDefaultRatioForUsingDenseIteration);
  }

  // Efficiently clears the is_non_zero vector.
  void ClearSparseMask() {
    if (ShouldUseDenseIteration()) {
      is_non_zero.assign(values.size(), false);
    } else {
      is_non_zero.resize(values.size(), false);
      for (const Index index : non_zeros) {
        is_non_zero[index] = false;
      }
      DCHECK(IsAllFalse(is_non_zero));
    }
  }

  // Update the is_non_zero vector to be consistent with the non_zeros vector.
  void RepopulateSparseMask() {
    ClearSparseMask();
    for (const Index index : non_zeros) is_non_zero[index] = true;
  }

  // If the proportion of non-zero entries is too large, clears the vector of
  // non-zeros.
  void ClearNonZerosIfTooDense(double ratio_for_using_dense_representation) {
    if (ShouldUseDenseIteration(ratio_for_using_dense_representation)) {
      ClearSparseMask();
      non_zeros.clear();
    }
  }

  void ClearNonZerosIfTooDense() {
    ClearNonZerosIfTooDense(kDefaultRatioForUsingDenseIteration);
  }

  // Returns an over-estimate of the number of non-zeros. This is actually
  // exact for sparse vector, or the full size otherwise.
  size_t NumNonZerosEstimate() const {
    return non_zeros.empty() ? values.size().value() : non_zeros.size();
  }
};

// Specializations used in the code.
class ScatteredColumnEntry : public ScatteredVectorEntry<RowIndex> {
 public:
  // Returns the row of the current entry.
  RowIndex row() const { return index(); }

 protected:
  ScatteredColumnEntry(const RowIndex* indices, const Fractional* coefficients,
                       EntryIndex i)
      : ScatteredVectorEntry<RowIndex>(indices, coefficients, i) {}
};

class ScatteredRowEntry : public ScatteredVectorEntry<ColIndex> {
 public:
  // Returns the column of the current entry.
  ColIndex column() const { return index(); }

 protected:
  ScatteredRowEntry(const ColIndex* indices, const Fractional* coefficients,
                    EntryIndex i)
      : ScatteredVectorEntry<ColIndex>(indices, coefficients, i) {}
};

using ScatteredColumnIterator = VectorIterator<ScatteredColumnEntry>;
using ScatteredRowIterator = VectorIterator<ScatteredRowEntry>;

struct ScatteredColumn
    : public ScatteredVector<RowIndex, ScatteredColumnIterator> {};
struct ScatteredRow : public ScatteredVector<ColIndex, ScatteredRowIterator> {};

inline const ScatteredRow& TransposedView(const ScatteredColumn& c) {
  return reinterpret_cast<const ScatteredRow&>(c);
}
inline const ScatteredColumn& TransposedView(const ScatteredRow& r) {
  return reinterpret_cast<const ScatteredColumn&>(r);
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_SCATTERED_VECTOR_H_
