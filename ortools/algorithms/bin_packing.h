
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

#ifndef OR_TOOLS_ORTOOLS_ALGORITHMS_BIN_PACKING_H
#define OR_TOOLS_ORTOOLS_ALGORITHMS_BIN_PACKING_H

#include <absl/algorithm/container.h>
#include <absl/container/flat_hash_set.h>
#include <absl/hash/hash.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_submodel.h"

namespace operations_research {

class BinPackingModel {
 public:
  BinPackingModel() = default;
  BaseInt num_items() const { return weigths_.size(); }
  Cost bin_capacity() const { return bin_capcaity_; }
  void set_bin_capacity(Cost capacity);
  const ElementCostVector& weights() const { return weigths_; }
  void AddItem(Cost weight);
  void SortWeights();
  ElementRange ItemRange() const {
    return {ElementIndex(), ElementIndex(weigths_.size())};
  }

 private:
  bool is_sorted_ = false;
  Cost bin_capcaity_ = .0;
  ElementCostVector weigths_ = {};
};

struct PartialBins {
  std::vector<SparseColumn> bins;
  std::vector<Cost> loads;
};

BinPackingModel ReadBpp(absl::string_view filename);
BinPackingModel ReadCsp(absl::string_view filename);

void BestFit(const BinPackingModel& model,
             const std::vector<ElementIndex>& items, PartialBins& bins_data);
}  // namespace operations_research
#endif /* OR_TOOLS_ORTOOLS_ALGORITHMS_BIN_PACKING_H */
