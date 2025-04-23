
// Copyright 2025 Francesco Cavaliere
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

#include "ortools/algorithms/bin_packing.h"

#include <absl/algorithm/container.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

#include "ortools/util/filelineiter.h"

namespace operations_research {

void BinPackingModel::set_bin_capacity(Cost capacity) {
  if (capacity <= 0) {
    LOG(WARNING) << "Bin capacity must be positive.";
    return;
  }
  bin_capcaity_ = capacity;
}
void BinPackingModel::AddItem(Cost weight) {
  if (weight > bin_capcaity_) {
    LOG(WARNING) << "Element weight exceeds bin capacity.";
    return;
  }
  weigths_.push_back(weight);
  is_sorted_ = false;
}
void BinPackingModel::SortWeights() {
  if (!is_sorted_) {
    absl::c_sort(weigths_);
    is_sorted_ = true;
  }
}

}  // namespace operations_research
