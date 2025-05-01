
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
#include <iterator>
#include <random>

#include "ortools/base/stl_util.h"
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

void BestFit(const ElementCostVector& weights, Cost bin_capacity,
             const std::vector<ElementIndex>& items, PartialBins& bins_data) {
  for (ElementIndex item : items) {
    Cost item_weight = weights[item];
    BaseInt selected_bin = bins_data.bins.size();
    for (BaseInt bin = 0; bin < bins_data.bins.size(); ++bin) {
      Cost max_load = bin_capacity - item_weight;
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
      VLOG_EVERY_N_SEC(1, 1)
          << "[RGEN] Generating bins: " << unique_bin_num << " / " << num_bins
          << " (" << 100.0 * unique_bin_num / num_bins << "%)";
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
      BestFit(model.weights(), model.bin_capacity(), items, bins_data);
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
  absl::c_sort(items, [&](auto i1, auto i2) {
    return model.weights()[i1] > model.weights()[i2];
  });
  BestFit(model.weights(), model.bin_capacity(), items, bins_data);
  InsertBinsIntoModel(bins_data, scp_model);
  VLOG(1) << "[BFIT] Largest first best-fit solution: " << bins_data.bins.size()
          << " bins";

  scp_model.CompleteModel();
  return scp_model;
}

void ExpKnap::SaveBin() {
  collected_bins_.back().clear();
  for (ElementIndex i : exceptions_) {
    break_selection_[i] = !break_selection_[i];
  }
  for (ElementIndex i : break_solution_) {
    if (break_selection_[i]) {
      collected_bins_.back().push_back(i);
    }
  }
  for (ElementIndex i : exceptions_) {
    if (break_selection_[i]) {
      collected_bins_.back().push_back(i);
    }
    break_selection_[i] = !break_selection_[i];
  }
  absl::c_sort(collected_bins_.back());
  DCHECK(absl::c_adjacent_find(collected_bins_.back()) ==
         collected_bins_.back().end());
}
void ExpKnap::InitSolver(const ElementCostVector& profits,
                         const ElementCostVector& weights, Cost capacity,
                         BaseInt bnb_nodes_limit) {
  capacity_ = capacity;
  items_.resize(profits.size());
  collected_bins_.clear();
  for (ElementIndex i; i < ElementIndex(items_.size()); ++i) {
    items_[i] = {profits[i], weights[i], i};
  }
  absl::c_sort(items_, [](Item i1, Item i2) {
    return i1.profit / i1.weight < i2.profit / i2.weight;
  });

  bnb_node_countdown_ = bnb_nodes_limit;
  best_delta_ = .0;
  exceptions_.clear();
  break_selection_.assign(profits.size(), false);
  break_solution_.clear();
}

void ExpKnap::FindGoodColumns(const ElementCostVector& profits,
                              const ElementCostVector& weights, Cost capacity,
                              BaseInt bnb_nodes_limit) {
  InitSolver(profits, weights, capacity, bnb_nodes_limit);
  Cost curr_best_cost = .0;
  PartialBins more_bins;
  std::vector<ElementIndex> remaining_items;

  bnb_node_countdown_ = bnb_nodes_limit;
  inserted_items_.assign(profits.size(), false);
  do {
    collected_bins_.emplace_back();
    Heuristic();
    VLOG(5) << "[KPCG] Heuristic solution: cost "
            << break_profit_sum_ + best_delta_;
    EleBranch();

    for (ElementIndex i : collected_bins_.back()) {
      inserted_items_[i] = true;
    }
    gtl::STLEraseAllFromSequenceIf(
        &items_, [&](Item item) { return inserted_items_[item.index]; });
  } while (!items_.empty() && break_profit_sum_ + best_delta_ > 1.0);
}

bool ExpKnap::EleBranch() {
  exceptions_.clear();
  return EleBranch(.0, break_weight_sum_ - capacity_, break_it_ - 1, break_it_);
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
  VLOG(6) << "[KPCG] EleBranch: profit_delta " << profit_delta << " overweigth "
          << overweigth;
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
    while (bnb_node_countdown_ > 0 && in_item < items_.rend() &&
           BoundCheck(best_delta_, profit_delta, overweigth, *in_item) >= 0) {
      exceptions_.push_back(in_item->index);
      Cost next_delta = profit_delta + in_item->profit;
      Cost next_oweight = overweigth + in_item->weight;
      maximal &= !EleBranch(next_delta, next_oweight, out_item, ++in_item);
      exceptions_.pop_back();
    }

    if (improved && maximal) {
      SaveBin();
    }
    improved |= !maximal;
  } else {
    while (bnb_node_countdown_ > 0 && out_item >= items_.rbegin() &&
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

void ExpKnap::Heuristic() {
  best_delta_ = break_profit_sum_ = break_weight_sum_ = .0;
  break_it_ = items_.rbegin();
  break_selection_.assign(items_.size(), false);
  break_solution_.clear();
  exceptions_.clear();
  while (break_it_ < items_.rend() &&
         break_it_->weight <= capacity_ - break_weight_sum_) {
    break_profit_sum_ += break_it_->profit;
    break_weight_sum_ += break_it_->weight;
    break_selection_[break_it_->index] = true;
    break_solution_.push_back(break_it_->index);
    ++break_it_;
  }
  Cost residual = capacity_ - break_weight_sum_;

  VLOG(5) << "[KPCG] Break solution: cost " << break_profit_sum_
          << ", residual " << residual;

  Cost profit_delta_ub = residual * break_it_->profit / break_it_->weight;
  if (profit_delta_ub == .0) {
    SaveBin();
    return;
  }

  // Try filling the residual space with less efficient (maybe smaller) items
  best_delta_ = .0;
  for (auto it = break_it_; it < items_.rend(); it++) {
    if (it->weight <= residual && it->profit > best_delta_) {
      exceptions_ = {it->index};
      best_delta_ = it->profit;
      if (best_delta_ >= profit_delta_ub) {
        SaveBin();
        return;
      }
    }
  }

  // Try removing an item and adding the break item
  Cost min_weight = break_it_->weight - residual;
  for (auto it = break_it_ - 1; it >= items_.rbegin(); it--) {
    Cost profit_delta = break_it_->profit - it->profit;
    if (it->weight >= min_weight && profit_delta > best_delta_) {
      exceptions_ = {break_it_->index, it->index};
      best_delta_ = profit_delta;
      if (best_delta_ >= profit_delta_ub) {
        SaveBin();
        return;
      }
    }
  }
  SaveBin();
}

bool BinPackingSetCoverModel::UpdateCore(
    Cost best_lower_bound, const ElementCostVector& best_multipliers,
    const scp::Solution& best_solution, bool force) {
  if (!base::IsTimeToUpdate(best_lower_bound, force)) {
    return false;
  }

  Cost full_lower_bound = base::UpdateMultipliers(best_multipliers);
  if (scp::DivideIfGE0(std::abs(full_lower_bound - best_lower_bound),
                       best_lower_bound) < 0.01 &&
      best_lower_bound != prev_lower_bound_) {
    prev_lower_bound_ = best_lower_bound;
    knapsack_solver_.FindGoodColumns(best_multipliers, bpp_model_->weights(),
                                     bpp_model_->bin_capacity(),
                                     /*bnb_nodes_limit=*/1000);
    BaseInt num_added_bins = 0;
    for (const SparseColumn& bin : knapsack_solver_.collected_bins()) {
      num_added_bins += AddBin(bin) ? 1 : 0;
    }
    if (num_added_bins > 0) {
      VLOG(4) << "[KPCG] Added " << num_added_bins << " / "
              << globals_.full_model.num_subsets() << " bins";
    }
    base::SizeUpdate();
    // TODO(user): add incremental update only for the new columns just added
    base::UpdateMultipliers(best_multipliers);
  }

  if (base::num_focus_subsets() < FixingFullModelView().num_focus_subsets()) {
    ComputeAndSetFocus(best_lower_bound, best_solution);
    return true;
  }
  return false;
}
}  // namespace operations_research
