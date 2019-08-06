// Copyright 2010-2018 Google LLC
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
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
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
  //
  // TODO(user): The constant should depend on what algorithm is used. Clearing
  // a dense vector is a lot more efficient than doing more complex stuff. Clean
  // this up by extracting all the currently used constants in one place with
  // meaningful names.
  bool ShouldUseDenseIteration() const {
    if (non_zeros.empty()) return true;
    const double kThresholdForUsingDenseRepresentation = 0.8;
    return static_cast<double>(non_zeros.size()) >
           kThresholdForUsingDenseRepresentation *
               static_cast<double>(values.size().value());
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
