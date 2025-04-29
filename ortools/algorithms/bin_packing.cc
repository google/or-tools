
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
#include <absl/log/log.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

#include <algorithm>
#include <random>

#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_cft.h"
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
      DCHECK_GT(value, .0);
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
  for (ElementIndex item : items) {
    Cost item_weight = model.weights()[item];
    BaseInt selected_bin = bins_data.bins.size();
    for (BaseInt bin = 0; bin < bins_data.bins.size(); ++bin) {
      Cost max_load = model.bin_capacity() - item_weight;
      if (bins_data.loads[bin] <= max_load &&
          (selected_bin == bins_data.bins.size() ||
           bins_data.loads[bin] > bins_data.loads[selected_bin])) {
        selected_bin = bin;
      }
    }
    if (selected_bin == bins_data.bins.size()) {
      bins_data.bins.emplace_back();
      bins_data.loads.emplace_back();
    }
    bins_data.bins[selected_bin].push_back(item);
    bins_data.loads[selected_bin] += item_weight;
  }
}

const SparseColumn& BinPackingSetCoverModel::BinPackingModelGlobals::GetBin(
    SubsetIndex j) const {
  if (j < SubsetIndex(full_model.num_subsets())) {
    return full_model.columns()[j];
  }
  DCHECK(candidate_bin != nullptr);
  return *candidate_bin;
}

uint64_t BinPackingSetCoverModel::BinHash::operator()(SubsetIndex j) const {
  DCHECK(globals != nullptr);
  return absl::HashOf(globals->GetBin(j));
}

uint64_t BinPackingSetCoverModel::BinEq::operator()(SubsetIndex j1,
                                                    SubsetIndex j2) const {
  DCHECK(globals != nullptr);
  return globals->GetBin(j1) == globals->GetBin(j2);
}

bool BinPackingSetCoverModel::AddBin(const SparseColumn& bin) {
  if (TryInsertBin(bin)) {
    globals_.full_model.AddEmptySubset(1.0);
    for (ElementIndex i : bin) {
      globals_.full_model.AddElementToLastSubset(i);
    }
    return true;
  }
  return false;
}

bool BinPackingSetCoverModel::TryInsertBin(const SparseColumn& bin) {
  DCHECK(absl::c_is_sorted(bin));
  DCHECK(absl::c_adjacent_find(bin) == bin.end());
  DCHECK(globals_.candidate_bin == nullptr);
  globals_.candidate_bin = &bin;

  SubsetIndex candidate_j(globals_.full_model.num_subsets());
  bool inserted = bin_set_.insert(candidate_j).second;

  globals_.candidate_bin = nullptr;
  return inserted;
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

void AddRandomizedBins(const BinPackingModel& model, BaseInt num_bins,
                       BinPackingSetCoverModel& scp_model, std::mt19937& rnd) {
  PartialBins bins_data;
  std::vector<ElementIndex> items(model.num_items());
  absl::c_iota(items, ElementIndex(0));

  while (scp_model.full_model().num_subsets() < num_bins) {
    // Generate bins all containing a specific item
    for (ElementIndex n : model.ItemRange()) {
      BaseInt unique_bin_num = scp_model.full_model().num_subsets();
      VLOG_EVERY_N_SEC(1, 5)
          << "Generating bins: " << unique_bin_num << " / " << num_bins << " ("
          << 100.0 * unique_bin_num / num_bins << "%)";
      if (scp_model.full_model().num_subsets() >= num_bins) {
        break;
      }

      absl::c_shuffle(items, rnd);

      auto n_it = absl::c_find(items, n);
      std::iter_swap(n_it, items.end() - 1);
      items.pop_back();

      bins_data.bins.clear();
      bins_data.loads.clear();
      for (BaseInt j = 0; j < 10; ++j) {
        bins_data.bins.push_back({n});
        bins_data.loads.push_back(model.weights()[n]);
      }
      BestFit(model, items, bins_data);
      InsertBinsIntoModel(bins_data, scp_model);

      items.push_back(n);

      if (unique_bin_num == scp_model.full_model().num_subsets()) {
        VLOG(1) << "No new bins generated.";
        break;
      }
    }
  }

  scp_model.CompleteModel();
}

BinPackingSetCoverModel GenerateInitialBins(const BinPackingModel& model) {
  BinPackingSetCoverModel scp_model(&model);
  PartialBins bins_data;
  std::vector<ElementIndex> items(model.num_items());

  absl::c_iota(items, ElementIndex(0));
  BestFit(model, items, bins_data);
  InsertBinsIntoModel(bins_data, scp_model);
  BaseInt solution_bin_num = bins_data.bins.size();
  VLOG(1) << "Best-fit solution: " << solution_bin_num << " bins";

  // Largest first
  if (!absl::c_is_sorted(model.weights(), std::greater<>())) {
    bins_data.bins.clear();
    bins_data.loads.clear();
    absl::c_sort(items, [&](auto i1, auto i2) {
      return model.weights()[i1] > model.weights()[i2];
    });
    BestFit(model, items, bins_data);
    InsertBinsIntoModel(bins_data, scp_model);
  }

  scp_model.CompleteModel();
  return scp_model;
}

void ExpKnap::Solve(const ElementCostVector& profits,
                    const ElementCostVector& weights, Cost capacity,
                    BaseInt bnb_nodes_limit) {
  capacity_ = capacity;
  best_delta_ = .0;
  exceptions_.clear();
  maximal_exceptions_.clear();
  break_solution_.assign(profits.size(), false);
  items_.resize(profits.size());
  bnb_node_countdown_ = bnb_nodes_limit;

  for (ElementIndex i; i < ElementIndex(items_.size()); ++i) {
    items_[i] = {std::max(1e-6, profits[i]), weights[i], i};
  }

  absl::c_sort(items_, [](Item i1, Item i2) {
    return i1.profit / i1.weight > i2.profit / i2.weight;
  });
  Heuristic(items_);
  maximal_exceptions_.push_back(exceptions_);
  exceptions_.clear();
  VLOG(5) << "[KPCG] Heuristic solution: cost "
          << break_profit_sum_ + best_delta_;

  EleBranch(.0, break_weight_sum_ - capacity, break_it_ - 1, break_it_ + 1);
}

namespace {
static Cost BoundCheck(Cost best_delta, Cost profit_delta, Cost overweight,
                       ExpKnap::Item item) {
  Cost bound = best_delta + 0.0;  // + 1.0 for integral profits
  return (profit_delta - bound) * item.weight - overweight * item.profit;
}
}  // namespace

// Adapted version from David Pisinger elebranch function:
// https://hjemmesider.diku.dk/~pisinger/expknap.c
bool ExpKnap::EleBranch(Cost profit_delta, Cost overweigth, ItemIt out_item,
                        ItemIt in_item) {
  if (bnb_node_countdown_-- <= 0) {
    return false;
  }
  bool improved = false;

  if (overweigth <= .0) {
    if (profit_delta > best_delta_) {
      best_delta_ = profit_delta;
      improved = true;
      VLOG(5) << "[KPCG] Improved best cost "
              << break_profit_sum_ + best_delta_;
    }

    bool maximal = true;
    while (bnb_node_countdown_ > 0 && in_item < items_.end() &&
           BoundCheck(best_delta_, profit_delta, overweigth, *in_item) >= 0) {
      exceptions_.push_back(in_item->index);
      Cost next_delta = profit_delta + in_item->profit;
      Cost next_oweight = overweigth + in_item->weight;
      maximal &= !EleBranch(next_delta, next_oweight, out_item, ++in_item);
      exceptions_.pop_back();
    }

    if (improved && maximal) {
      maximal_exceptions_.push_back(exceptions_);
    }
    improved |= !maximal;
  } else {
    while (bnb_node_countdown_ > 0 && out_item >= items_.begin() &&
           BoundCheck(best_delta_, profit_delta, overweigth, *out_item) >= 0) {
      exceptions_.push_back(out_item->index);
      Cost next_delta = profit_delta - out_item->profit;
      Cost next_oweight = overweigth - out_item->weight;
      improved |= EleBranch(next_delta, next_oweight, --out_item, in_item);
      exceptions_.pop_back();
    }
  }
  return improved;
}

void ExpKnap::Heuristic(
    const util_intops::StrongVector<ElementIndex, Item>& items) {
  exceptions_.clear();

  break_profit_sum_ = break_weight_sum_ = .0;
  break_it_ = items.begin();
  while (break_it_ < items.end() &&
         break_it_->weight <= capacity_ - break_weight_sum_) {
    break_profit_sum_ += break_it_->profit;
    break_weight_sum_ += break_it_->weight;
    break_solution_[break_it_->index] = true;
    ++break_it_;
  }
  Cost residual = capacity_ - break_weight_sum_;

  VLOG(5) << "[KPCG] Break solution: cost " << break_profit_sum_
          << ", residual " << residual;

  Cost profit_delta_ub = residual * break_it_->profit / break_it_->weight;
  if (profit_delta_ub == .0) {
    return;
  }

  // Try filling the residual space with less efficient (maybe smaller) items
  best_delta_ = .0;
  for (auto it = break_it_; it < items.end(); it++) {
    if (it->weight <= residual && it->profit > best_delta_) {
      exceptions_ = {it->index};
      best_delta_ = it->profit;
      if (best_delta_ >= profit_delta_ub) {
        return;
      }
    }
  }

  // Try removing an item and adding the break item
  Cost min_weight = break_it_->weight - residual;
  for (auto it = break_it_ - 1; it >= items.begin(); it--) {
    Cost profit_delta = break_it_->profit - it->profit;
    if (it->weight >= min_weight && profit_delta > best_delta_) {
      exceptions_ = {break_it_->index, it->index};
      best_delta_ = profit_delta;
      if (best_delta_ >= profit_delta_ub) {
        return;
      }
    }
  }
}

bool BinPackingSetCoverModel::UpdateCore(
    Cost best_lower_bound, const ElementCostVector& best_multipliers,
    const scp::Solution& best_solution, bool force) {
  if (--column_gen_countdown_ <= 0 && best_lower_bound != prev_lower_bound_) {
    column_gen_countdown_ = column_gen_period_;
    prev_lower_bound_ = best_lower_bound;
    knapsack_solver_.Solve(best_multipliers, bpp_model_->weights(),
                           bpp_model_->bin_capacity(),
                           /*bnb_nodes_limit=*/10000);
    const auto& exception_list = knapsack_solver_.maximal_exceptions();
    ElementBoolVector solution = knapsack_solver_.break_solution();
    BaseInt num_added_bins = 0;
    SparseColumn bin;
    for (const std::vector<ElementIndex>& exception : exception_list) {
      for (ElementIndex i : exception) {
        solution[i] = !solution[i];
      }

      bin.clear();
      for (ElementIndex i : globals_.full_model.ElementRange()) {
        if (solution[i]) {
          bin.push_back(i);
        }
      }
      num_added_bins += AddBin(bin) ? 1 : 0;

      for (ElementIndex i : exception) {
        solution[i] = !solution[i];
      }
    }
    if (num_added_bins > 0) {
      VLOG(4) << "[KPCG] Added " << num_added_bins << " / "
              << globals_.full_model.num_subsets() << " bins";
    }
  }

  scp::FullToCoreModel::SizeUpdate();
  scp::FullToCoreModel::FullToSubModelInvariantCheck();
  return scp::FullToCoreModel::UpdateCore(best_lower_bound, best_multipliers,
                                          best_solution, force);
}
}  // namespace operations_research
