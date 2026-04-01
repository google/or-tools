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

// [START program]
// [START imports]
#include <iostream>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "ortools/base/init_google.h"
#include "ortools/base/status_macros.h"
#include "ortools/graph/rooted_tree.h"
// [END imports]

namespace {

absl::Status Main() {
  // Make an rooted tree on 5 nodes with root 2 and the parental args:
  //  0 -> 1
  //  1 -> 2
  //  2 is root
  //  3 -> 2
  //  4 -> 1
  ASSIGN_OR_RETURN(
      const operations_research::RootedTree<int> tree,
      operations_research::RootedTree<int>::Create(2, {1, 2, -1, 2, 1}));

  // Precompute this for LCA computations below.
  const std::vector<int> depths = tree.AllDepths();

  // Find the path between every pair of nodes in the tree.
  for (int s = 0; s < 5; ++s) {
    for (int t = 0; t < 5; ++t) {
      int lca = tree.LowestCommonAncestorByDepth(s, t, depths);
      const std::vector<int> path = tree.Path(s, t, lca);
      std::cout << s << " -> " << t << " [" << absl::StrJoin(path, ", ") << "]"
                << std::endl;
    }
  }
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  QCHECK_OK(Main());
  return 0;
}
// [END program]
