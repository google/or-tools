// Copyright 2010-2014 Google
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

#include "ortools/graph/util.h"

namespace operations_research {

bool IsSubsetOf0N(const std::vector<int>& v, int n) {
  std::vector<bool> mask(n, false);
  for (const int i : v) {
    if (i < 0 || i >= n || mask[i]) return false;
    mask[i] = true;
  }
  return true;
}

}  // namespace operations_research
