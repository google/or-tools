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

#ifndef OR_TOOLS_SAT_SCHEDULING_CONSTRAINTS_H_
#define OR_TOOLS_SAT_SCHEDULING_CONSTRAINTS_H_

#include <cstddef>
#include <vector>

#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// This propagator enforces that the target variable is equal to the min of the
// selected variables. This equation only holds if the enforcement literal is
// true.
//
// This constraint expects that enforcement_literal <==> bool_or(selectors).
std::function<void(Model*)> EqualMinOfSelectedVariables(
    Literal enforcement_literal, AffineExpression target,
    const std::vector<AffineExpression>& vars,
    const std::vector<Literal>& selectors);

// This propagator enforces that the target variable is equal to the max of the
// selected variables. This equation only holds if the enforcement literal is
// true.
//
// This constraint expects that enforcement_literal <==> bool_or(selectors).u
std::function<void(Model*)> EqualMaxOfSelectedVariables(
    Literal enforcement_literal, AffineExpression target,
    const std::vector<AffineExpression>& vars,
    const std::vector<Literal>& selectors);

// This constraint enforces that the target interval is an exact cover of the
// underlying intervals.
//
// It means start(span) is the min of the start of all performed intervals. Also
// end(span) is the max of the end of all performed intervals.
//
// Furthermore, the following conditions also hold:
//   - If the target interval is present, then at least one interval variables
//     is present.
//   - if the target interval is absent, all intervals are absent.
//   - If one interval is present, the target interval is present too.
std::function<void(Model*)> SpanOfIntervals(
    IntervalVariable span, const std::vector<IntervalVariable>& intervals);
}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SCHEDULING_CONSTRAINTS_H_
