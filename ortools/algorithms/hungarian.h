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

// An O(n^4) implementation of the Kuhn-Munkres algorithm (a.k.a. the
// Hungarian algorithm) for solving the assignment problem.
// The assignment problem takes a set of agents, a set of tasks and a
// cost associated with assigning each agent to each task and produces
// an optimal (i.e., least cost) assignment of agents to tasks.
// The code also enables computing a maximum assignment by changing the
// input matrix.

#ifndef ORTOOLS_ALGORITHMS_HUNGARIAN_H_
#define ORTOOLS_ALGORITHMS_HUNGARIAN_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"

namespace operations_research {

// See IMPORTANT NOTE at the top of the file.
void MinimizeLinearAssignment(
    absl::Span<const std::vector<double>> cost,
    absl::flat_hash_map<int, int>* direct_assignment,
    absl::flat_hash_map<int, int>* reverse_assignment);

// See IMPORTANT NOTE at the top of the file.
void MaximizeLinearAssignment(
    absl::Span<const std::vector<double>> cost,
    absl::flat_hash_map<int, int>* direct_assignment,
    absl::flat_hash_map<int, int>* reverse_assignment);

}  // namespace operations_research

#endif  // ORTOOLS_ALGORITHMS_HUNGARIAN_H_
