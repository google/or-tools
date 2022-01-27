// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/symmetry_util.h"

#include <cstdint>

#include "ortools/algorithms/dynamic_partition.h"

namespace operations_research {
namespace sat {

std::vector<std::vector<int>> BasicOrbitopeExtraction(
    const std::vector<std::unique_ptr<SparsePermutation>>& generators) {
  // Count the number of permutations that are compositions of 2-cycle and
  // regroup them according to the number of cycles.
  std::vector<std::vector<int>> num_cycles_to_2cyclers;
  for (int g = 0; g < generators.size(); ++g) {
    const std::unique_ptr<SparsePermutation>& perm = generators[g];
    bool contain_only_2cycles = true;
    const int num_cycles = perm->NumCycles();
    for (int i = 0; i < num_cycles; ++i) {
      if (perm->Cycle(i).size() != 2) {
        contain_only_2cycles = false;
        break;
      }
    }
    if (!contain_only_2cycles) continue;
    if (num_cycles >= num_cycles_to_2cyclers.size()) {
      num_cycles_to_2cyclers.resize(num_cycles + 1);
    }
    num_cycles_to_2cyclers[num_cycles].push_back(g);
  }

  // Heuristic: we try to grow the orbitope that has the most potential for
  // fixing variables.
  //
  // TODO(user): We could grow each and keep the real maximum.
  int best = -1;
  int best_score = 0;
  for (int i = 0; i < num_cycles_to_2cyclers.size(); ++i) {
    if (num_cycles_to_2cyclers[i].size() > 1) {
      const int num_perms = num_cycles_to_2cyclers[i].size() + 1;
      VLOG(1) << "Potential orbitope: " << i << " x " << num_perms;
      const int64_t score = std::min(i, num_perms);
      if (score > best_score) {
        best = i;
        best_score = score;
      }
    }
  }

  std::vector<std::vector<int>> orbitope;
  if (best == -1) return orbitope;

  // We will track the element already added so we never have duplicates.
  std::vector<bool> in_matrix;

  // Greedily grow the orbitope.
  orbitope.resize(best);
  for (const int g : num_cycles_to_2cyclers[best]) {
    // Start using the first permutation.
    if (orbitope[0].empty()) {
      const std::unique_ptr<SparsePermutation>& perm = generators[g];
      const int num_cycles = perm->NumCycles();
      for (int i = 0; i < num_cycles; ++i) {
        for (const int x : perm->Cycle(i)) {
          orbitope[i].push_back(x);
          if (x >= in_matrix.size()) in_matrix.resize(x + 1, false);
          in_matrix[x] = true;
        }
      }
      continue;
    }

    // We want to find a column such that g sends it to variables not already
    // in the orbitope matrix.
    //
    // Note(user): This relies on the cycle in each permutation to be ordered by
    // smaller element first. This way we don't have to account any row
    // permutation of the orbitope matrix. The code that detect the symmetries
    // of the problem should already return permutation in this canonical
    // format.
    std::vector<int> grow;
    int matching_column_index = -1;
    const std::unique_ptr<SparsePermutation>& perm = generators[g];
    const int num_cycles = perm->NumCycles();
    for (int i = 0; i < num_cycles; ++i) {
      // Extract the two elements of this transposition.
      std::vector<int> tmp;
      for (const int x : perm->Cycle(i)) tmp.push_back(x);
      const int a = tmp[0];
      const int b = tmp[1];

      // We want one element to appear in matching_column_index and the other to
      // not appear at all.
      int num_matches_a = 0;
      int num_matches_b = 0;
      int last_match_index = -1;
      for (int j = 0; j < orbitope[i].size(); ++j) {
        if (orbitope[i][j] == a) {
          ++num_matches_a;
          last_match_index = j;
        } else if (orbitope[i][j] == b) {
          ++num_matches_b;
          last_match_index = j;
        }
      }
      if (last_match_index == -1) break;
      if (matching_column_index == -1) {
        matching_column_index = last_match_index;
      }
      if (matching_column_index != last_match_index) break;
      if (num_matches_a == 0 && num_matches_b == 1) {
        if (a >= in_matrix.size() || !in_matrix[a]) grow.push_back(a);
      } else if (num_matches_a == 1 && num_matches_b == 0) {
        if (b >= in_matrix.size() || !in_matrix[b]) grow.push_back(b);
      } else {
        break;
      }
    }

    // If grow is of full size, we can extend the orbitope.
    if (grow.size() == num_cycles) {
      for (int i = 0; i < orbitope.size(); ++i) {
        orbitope[i].push_back(grow[i]);
        if (grow[i] >= in_matrix.size()) in_matrix.resize(grow[i] + 1, false);
        in_matrix[grow[i]] = true;
      }
    }
  }

  return orbitope;
}

std::vector<int> GetOrbits(
    int n, const std::vector<std::unique_ptr<SparsePermutation>>& generators) {
  MergingPartition union_find;
  union_find.Reset(n);
  for (const std::unique_ptr<SparsePermutation>& perm : generators) {
    const int num_cycles = perm->NumCycles();
    for (int i = 0; i < num_cycles; ++i) {
      // Note that there is currently no random access api like cycle[j].
      int first;
      bool is_first = true;
      for (const int x : perm->Cycle(i)) {
        if (is_first) {
          first = x;
          is_first = false;
        } else {
          union_find.MergePartsOf(first, x);
        }
      }
    }
  }

  int num_parts = 0;
  std::vector<int> orbits(n, -1);
  for (int i = 0; i < n; ++i) {
    if (union_find.NumNodesInSamePartAs(i) == 1) continue;
    const int root = union_find.GetRootAndCompressPath(i);
    if (orbits[root] == -1) orbits[root] = num_parts++;
    orbits[i] = orbits[root];
  }
  return orbits;
}

std::vector<int> GetOrbitopeOrbits(
    int n, const std::vector<std::vector<int>>& orbitope) {
  std::vector<int> orbits(n, -1);
  for (int i = 0; i < orbitope.size(); ++i) {
    for (int j = 0; j < orbitope[i].size(); ++j) {
      CHECK_EQ(orbits[orbitope[i][j]], -1);
      orbits[orbitope[i][j]] = i;
    }
  }
  return orbits;
}

}  // namespace sat
}  // namespace operations_research
