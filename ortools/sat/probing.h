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

#ifndef OR_TOOLS_SAT_PROBING_H_
#define OR_TOOLS_SAT_PROBING_H_

#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// Fixes Booleans variables to true/false and see what is propagated. This can:
//
// - Fix some Boolean variables (if we reach a conflict while probing).
//
// - Infer new direct implications. We add them directly to the
//   BinaryImplicationGraph and they can later be used to detect equivalent
//   literals, expand at most ones clique, etc...
//
// - Tighten the bounds of integer variables. If we probe the two possible
//   values of a Boolean (b=0 and b=1), we get for each integer variables two
//   propagated domain D_0 and D_1. The level zero domain can then be
//   intersected with D_0 U D_1. This can restrict the lower/upper bounds of a
//   variable, but it can also create holes in the domain! This will detect
//   common cases like an integer variable in [0, 10] that actually only take
//   two values [0] or [10] depending on one Boolean.
//
// Returns false if the problem was proved INFEASIBLE during probing.
//
// TODO(user): For now we process the Boolean in their natural order, this is
// not the most efficient.
//
// TODO(user): This might generate a lot of new direct implications. We might
// not want to add them directly to the BinaryImplicationGraph and could instead
// use them directly to detect equivalent literal like in
// ProbeAndFindEquivalentLiteral(). The situation is not clear.
//
// TODO(user): More generally, we might want to register any literal => bound in
// the IntegerEncoder. This would allow to remember them and use them in other
// part of the solver (cuts, lifting, ...).
bool ProbeBooleanVariables(double deterministic_time_limit, Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PROBING_H_
