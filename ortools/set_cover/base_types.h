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

#ifndef OR_TOOLS_SET_COVER_BASE_TYPES_H_
#define OR_TOOLS_SET_COVER_BASE_TYPES_H_

#include <cstdint>

#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {

// Basic non-strict type for cost. The speed penalty for using double is ~2%.
using Cost = double;

// Base non-strict integer type for counting elements and subsets.
// Using ints makes it possible to represent problems with more than 2 billion
// (2e9) elements and subsets. If need arises one day, BaseInt can be split
// into SubsetBaseInt and ElementBaseInt.
// Quick testing has shown a slowdown of about 20-25% when using int64_t.
using BaseInt = int32_t;

// We make heavy use of strong typing to avoid obvious mistakes.
// Subset index.
DEFINE_STRONG_INT_TYPE(SubsetIndex, BaseInt);

// Element index.
DEFINE_STRONG_INT_TYPE(ElementIndex, BaseInt);

// Position in a vector. The vector may either represent a column, i.e. a
// subset with all its elements, or a row, i,e. the list of subsets which
// contain a given element.
DEFINE_STRONG_INT_TYPE(ColumnEntryIndex, BaseInt);
DEFINE_STRONG_INT_TYPE(RowEntryIndex, BaseInt);

using SubsetRange = util_intops::StrongIntRange<SubsetIndex>;
using ElementRange = util_intops::StrongIntRange<ElementIndex>;
using ColumnEntryRange = util_intops::StrongIntRange<ColumnEntryIndex>;

using SubsetCostVector = util_intops::StrongVector<SubsetIndex, Cost>;
using ElementCostVector = util_intops::StrongVector<ElementIndex, Cost>;

using SparseColumn = util_intops::StrongVector<ColumnEntryIndex, ElementIndex>;
using SparseRow = util_intops::StrongVector<RowEntryIndex, SubsetIndex>;

using ElementToIntVector = util_intops::StrongVector<ElementIndex, BaseInt>;
using SubsetToIntVector = util_intops::StrongVector<SubsetIndex, BaseInt>;

// Views of the sparse vectors. These need not be aligned as it's their contents
// that need to be aligned.
using SparseColumnView = util_intops::StrongVector<SubsetIndex, SparseColumn>;
using SparseRowView = util_intops::StrongVector<ElementIndex, SparseRow>;

using SubsetBoolVector = util_intops::StrongVector<SubsetIndex, bool>;
using ElementBoolVector = util_intops::StrongVector<ElementIndex, bool>;

}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_BASE_TYPES_H_
