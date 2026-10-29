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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_RESOURCE_H_
#define ORTOOLS_CONSTRAINT_SOLVER_RESOURCE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

DisjunctiveConstraint* BuildDisjunctiveConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::string& name);

DisjunctiveConstraint* BuildStrictDisjunctiveConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::string& name);

Constraint* BuildCumulativeConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::vector<int64_t>& demands, int64_t capacity,
    const std::string& name);

Constraint* BuildCumulativeConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::vector<int>& demands, int64_t capacity, const std::string& name);

Constraint* BuildCumulativeConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::vector<int64_t>& demands, IntVar* capacity,
    absl::string_view name);

Constraint* BuildCumulativeConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::vector<int>& demands, IntVar* capacity, const std::string& name);

Constraint* BuildCumulativeConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::vector<IntVar*>& demands, int64_t capacity,
    const std::string& name);

Constraint* BuildCumulativeConstraint(
    Solver* solver, const std::vector<IntervalVar*>& intervals,
    const std::vector<IntVar*>& demands, IntVar* capacity,
    const std::string& name);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_RESOURCE_H_
