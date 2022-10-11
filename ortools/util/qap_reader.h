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

#ifndef OR_TOOLS_UTIL_QAP_READER_H_
#define OR_TOOLS_UTIL_QAP_READER_H_

#include <string>
#include <vector>

namespace operations_research {

// Quadratic assignment problem (QAP) is a classical combinatorial optimization
// problem. See https://en.wikipedia.org/wiki/Quadratic_assignment_problem.
// In short, there are a set of n facilities and a set of n locations.
// For each pair of locations, a `distance` is specified and for each pair of
// facilities a `weight` is specified (e.g., the amount of supplies transported
// between the two facilities). The problem is to assign all facilities to
// different locations with the goal of minimizing the sum of the distances
// multiplied by the corresponding flows.
struct QapProblem {
  // `weights[i][j]` is the amount of flow from facility i to facility j.
  std::vector<std::vector<int64_t>> weights;

  // `distances[i][j]` is the distance from location i to location j.
  std::vector<std::vector<int64_t>> distances;

  // Best known solution (-1 if not defined).
  int64_t best_known_solution = -1;

  // For testing.
  bool operator==(const QapProblem& q) const {
    return weights == q.weights && distances == q.distances;
  }
};

// Reads a QAP problem from file in a format used in QAPLIB. See
// anjos.mgi.polymtl.ca/qaplib/ for more context. The format is "n W D", where
// n is the number of factories/locations, and W and D are weight and
// distance matrices, respectively. Both W and D are square matrices of size
// N x N. Each entry of the matrices must parse as double (we CHECK if it
// does). Multiple spaces, or '\n' are disregarded. For example:
//
// 2
//
// 0 2
// 3 0
//
// 0 2
// 1 0
//
// defines a problem with two factories and two locations. There are 2 units of
// supply flowing from factory 0 to factory 1, and 3 units of supply flowing
// from factory 1 to 0. The distance from location 0 to location 1 is equal
// to 2, and the distance from location 1 to 0 is equal to 1.
QapProblem ReadQapProblemOrDie(const std::string& filepath);

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_QAP_READER_H_
