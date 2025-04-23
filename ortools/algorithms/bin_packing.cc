
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
#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

#include <algorithm>
#include <random>

#include "ortools/algorithms/radix_sort.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_cft.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_submodel.h"
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

namespace {
template <typename int_type>
bool SimpleAtoValue(absl::string_view str, int_type* out) {
  return absl::SimpleAtoi(str, out);
}
bool SimpleAtoValue(absl::string_view str, bool* out) {
  return absl::SimpleAtob(str, out);
}
bool SimpleAtoValue(absl::string_view str, double* out) {
  return absl::SimpleAtod(str, out);
}
bool SimpleAtoValue(absl::string_view str, float* out) {
  return absl::SimpleAtof(str, out);
}
}  // namespace

BinPackingModel ReadBpp(absl::string_view filename) {
  BinPackingModel model;
  BaseInt num_items = 0;
  for (const std::string& line :
       FileLines(filename, FileLineIterator::REMOVE_INLINE_CR |
                               FileLineIterator::REMOVE_BLANK_LINES)) {
    if (num_items == 0) {
      if (!SimpleAtoValue(line, &num_items)) {
        LOG(WARNING) << "Invalid number of elements in file: " << line;
      }
      continue;
    }

    Cost value = .0;
    if (!SimpleAtoValue(line, &value)) {
      LOG(WARNING) << "Invalid value in file: " << line;
      continue;
    }
    if (model.bin_capacity() <= .0) {
      model.set_bin_capacity(value);
    } else {
      model.AddItem(value);
    }
  }
  DCHECK_GT(model.bin_capacity(), .0);
  DCHECK_GT(model.weights().size(), .0);
  DCHECK_EQ(num_items, model.weights().size());
  model.SortWeights();
  return model;
}

BinPackingModel ReadCsp(absl::string_view filename) {
  BinPackingModel model;
  BaseInt num_item_types = 0;
  for (const std::string& line :
       FileLines(filename, FileLineIterator::REMOVE_INLINE_CR |
                               FileLineIterator::REMOVE_BLANK_LINES)) {
    if (num_item_types == 0) {
      if (!SimpleAtoValue(line, &num_item_types)) {
        LOG(WARNING) << "Invalid number of elements in file: " << line;
      }
      continue;
    }
    if (model.bin_capacity() <= .0) {
      Cost capacity = .0;
      if (!SimpleAtoValue(line, &capacity)) {
        LOG(WARNING) << "Invalid value in file: " << line;
      }
      model.set_bin_capacity(capacity);
      continue;
    }

    std::pair<absl::string_view, absl::string_view> weight_and_demand =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());

    Cost weight = .0;
    if (!SimpleAtoValue(weight_and_demand.first, &weight)) {
      LOG(WARNING) << "Invalid weight in file: " << line;
      continue;
    }

    BaseInt demand = 0;
    if (!SimpleAtoValue(weight_and_demand.second, &demand)) {
      LOG(WARNING) << "Invalid demand in file: " << line;
      continue;
    }
    for (BaseInt i = 0; i < demand; ++i) {
      model.AddItem(weight);
    }
  }
  DCHECK_GT(model.bin_capacity(), .0);
  DCHECK_GT(model.weights().size(), .0);
  DCHECK_GE(num_item_types, model.num_items());
  model.SortWeights();
  return model;
}

void BestFit(const BinPackingModel& model,
             const std::vector<ElementIndex>& items, PartialBins& bins_data) {
  BaseInt new_bin = bins_data.bins.size();
  for (ElementIndex item : items) {
    Cost item_weight = model.weights()[item];
    BaseInt selected_bin = new_bin;
    for (BaseInt bin = 0; bin < bins_data.bins.size(); ++bin) {
      Cost max_load = model.bin_capacity() - item_weight;
      if (bins_data.loads[bin] <= max_load &&
          (selected_bin == new_bin ||
           bins_data.loads[bin] > bins_data.loads[selected_bin])) {
        selected_bin = bin;
      }
    }
    bins_data.bins[selected_bin].push_back(item);
    bins_data.loads[selected_bin] += item_weight;
    if (selected_bin == new_bin) {
      ++new_bin;
    }
  }
}

void InsertBinsIntoModel(PartialBins& bins_data,
                         BinPackingSetCoverModel& model) {
  for (BaseInt i = 0; i < bins_data.bins.size(); ++i) {
    if (bins_data.bins[i].size() > 0) {
      absl::c_sort(bins_data.bins[i]);
      model.AddBin(bins_data.bins[i]);
    }
  }
}

BinPackingSetCoverModel GenerateBins(const BinPackingModel& model,
                                     PartialBins& best_solution,
                                     BaseInt num_bins) {
  BinPackingSetCoverModel scp_model;
  PartialBins bins_data;
  std::vector<ElementIndex> items(model.num_items());

  absl::c_iota(items, ElementIndex(0));
  BestFit(model, items, bins_data);
  if (best_solution.bins.empty() ||
      bins_data.bins.size() < best_solution.bins.size()) {
    best_solution = bins_data;
  }
  InsertBinsIntoModel(bins_data, scp_model);

  // Largest first
  if (!absl::c_is_sorted(model.weights(), std::greater<>())) {
    bins_data.bins.clear();
    bins_data.loads.clear();
    absl::c_sort(items, [&](auto i1, auto i2) {
      return model.weights()[i1] > model.weights()[i2];
    });
    BestFit(model, items, bins_data);
    if (bins_data.bins.size() < best_solution.bins.size()) {
      best_solution = bins_data;
    }
    InsertBinsIntoModel(bins_data, scp_model);
  }

  std::mt19937 rnd(0);
  while (scp_model.full_model().num_subsets() < num_bins) {
    // Generate bins all containing a specific item
    for (ElementIndex n : model.ItemRange()) {
      absl::c_shuffle(items, rnd);

      auto n_it = absl::c_find(items, n);
      std::iter_swap(n_it, items.end() - 1);
      items.pop_back();

      bins_data.bins.clear();
      bins_data.loads.clear();
      for (BaseInt j = 0; j < best_solution.bins.size(); ++j) {
        bins_data.bins.push_back({n});
        bins_data.loads.push_back(model.weights()[n]);
        BestFit(model, items, bins_data);
        InsertBinsIntoModel(bins_data, scp_model);
      }

      items.push_back(n);
    }
  }

  return scp_model;
}

}  // namespace operations_research
