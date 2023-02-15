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

#ifndef OR_TOOLS_SAT_TABLE_H_
#define OR_TOOLS_SAT_TABLE_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/integral_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Enforces that exactly one literal in line_literals is true, and that
// all literals in the corresponding line of the literal_tuples matrix are true.
// This constraint assumes that exactly one literal per column of the
// literal_tuples matrix is true.
std::function<void(Model*)> LiteralTableConstraint(
    const std::vector<std::vector<Literal>>& literal_tuples,
    const std::vector<Literal>& line_literals);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_TABLE_H_
