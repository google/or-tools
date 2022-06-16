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

#ifndef OR_TOOLS_SAT_SYMMETRY_UTIL_H_
#define OR_TOOLS_SAT_SYMMETRY_UTIL_H_

#include <memory>
#include <vector>

#include "ortools/algorithms/sparse_permutation.h"

namespace operations_research {
namespace sat {

// Given the generator for a permutation group of [0, n-1], tries to identify
// a grouping of the variables in an p x q matrix such that any permutations
// of the columns of this matrix is in the given group.
//
// The name comes from: "Packing and Partitioning Orbitopes", Volker Kaibel,
// Marc E. Pfetsch, https://arxiv.org/abs/math/0603678 . Here we just detect it,
// independently of the constraints on the variables in this matrix. We can also
// detect non-Boolean orbitope.
//
// In order to detect orbitope, this basic algorithm requires that the
// generators of the orbitope must only contain one or more 2-cyle (i.e
// transpositions). Thus they must be involutions. The list of transpositions in
// the SparsePermutation must also be listed in a canonical order.
//
// TODO(user): Detect more than one orbitope? Note that once detected, the
// structure can be exploited efficiently, but for now, a more "generic"
// algorithm based on stabilizator should achieve the same preprocessing power,
// so I don't know how hard we need to invest in orbitope detection.
//
// TODO(user): The heuristic is quite limited for now, but this works on
// graph20-20-1rand.mps.gz. I suspect the generators provided by the detection
// code follow our preconditions.
std::vector<std::vector<int>> BasicOrbitopeExtraction(
    const std::vector<std::unique_ptr<SparsePermutation>>& generators);

// Returns a vector of size n such that
// - orbits[i] == -1 iff i is never touched by the generators (singleton orbit).
// - orbits[i] = orbit_index, where orbits are numbered from 0 to num_orbits - 1
//
// TODO(user): We could reuse the internal memory if needed.
std::vector<int> GetOrbits(
    int n, const std::vector<std::unique_ptr<SparsePermutation>>& generators);

// Returns the orbits under the given orbitope action.
// Same results format as in GetOrbits(). Note that here, the orbit index
// is simply the row index of an element in the orbitope matrix.
std::vector<int> GetOrbitopeOrbits(
    int n, const std::vector<std::vector<int>>& orbitope);

// Given the generators for a permutation group of [0, n-1], update it to
// a set of generators of the group stabilizing the given element.
//
// Note that one can add symmetry breaking constraints by repeatedly doing:
// 1/ Call GetOrbits() using the current set of generators.
// 2/ Choose an element x0 in a large orbit (x0, .. xi ..) , and add x0 >= xi
//    for all i.
// 3/ Update the set of generators to the one stabilizing x0.
//
// This is more or less what is described in "Symmetry Breaking Inequalities
// from the Schreier-Sims Table", Domenico Salvagnin,
// https://link.springer.com/chapter/10.1007/978-3-319-93031-2_37
//
// TODO(user): Implement!
inline void TransformToGeneratorOfStabilizer(
    int to_stabilize,
    std::vector<std::unique_ptr<SparsePermutation>>* generators) {}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SYMMETRY_UTIL_H_
