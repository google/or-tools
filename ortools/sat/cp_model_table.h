// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_SAT_CP_MODEL_TABLE_H_
#define OR_TOOLS_SAT_CP_MODEL_TABLE_H_

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/presolve_context.h"

namespace operations_research {
namespace sat {

// Canonicalizes the table constraint by removing all unreachable tuples, and
// all columns which have the same variable of a previous column.
//
// This also sort all the tuples.
void CanonicalizeTable(PresolveContext* context, ConstraintProto* ct);

// Removed all fixed columns from the table.
void RemoveFixedColumnsFromTable(PresolveContext* context, ConstraintProto* ct);

// This method tries to compress a list of tuples by merging complementary
// tuples, that is a set of tuples that only differ on one variable, and that
// cover the domain of the variable. In that case, it will keep only one tuple,
// and replace the value for variable by any_value, the equivalent of '*' in
// regexps.
//
// This method is exposed for testing purposes.
constexpr int64_t kTableAnyValue = std::numeric_limits<int64_t>::min();
void CompressTuples(absl::Span<const int64_t> domain_sizes,
                    std::vector<std::vector<int64_t>>* tuples);

// Similar to CompressTuples() but produces a final table where each cell is
// a set of value. This should result in a table that can still be encoded
// efficiently in SAT but with less tuples and thus less extra Booleans. Note
// that if a set of value is empty, it is interpreted at "any" so we can gain
// some space.
//
// The passed tuples vector is used as temporary memory and is detroyed.
// We interpret kTableAnyValue as an "any" tuple.
//
// TODO(user): To reduce memory, we could return some absl::Span in the last
// layer instead of vector.
//
// TODO(user): The final compression is depend on the order of the variables.
// For instance the table (1,1)(1,2)(1,3),(1,4),(2,3) can either be compressed
// as (1,*)(2,3) or (1,{1,2,4})({1,3},3). More experiment are needed to devise
// a better heuristic. It might for example be good to call CompressTuples()
// first.
std::vector<std::vector<absl::InlinedVector<int64_t, 2>>> FullyCompressTuples(
    absl::Span<const int64_t> domain_sizes,
    std::vector<std::vector<int64_t>>* tuples);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_TABLE_H_
