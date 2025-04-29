
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

#include <vector>

#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_cft.h"

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

using SubsetHashVector = util_intops::StrongVector<SubsetIndex, uint64_t>;


class ExpKnap {
 public:
  struct Item {
    Cost profit;  // profit
    Cost weight;  // weight
    ElementIndex index;
  };
  using ItemIt = std::vector<Item>::const_iterator;

  void Solve(const ElementCostVector& profits, const ElementCostVector& weights,
             Cost capacity, BaseInt bnb_nodes_limit);

  bool EleBranch(Cost profit_sum, Cost overweight, ItemIt out_item,
                 ItemIt in_item);

  void Heuristic(const util_intops::StrongVector<ElementIndex, Item>& items);

  ElementBoolVector break_solution() const { return break_solution_; }

  std::vector<std::vector<ElementIndex>> maximal_exceptions() const {
    return maximal_exceptions_;
  }

 private:
  Cost capacity_;                                        // capacity
  util_intops::StrongVector<ElementIndex, Item> items_;  // items
  ItemIt break_it_;
  Cost break_profit_sum_;
  Cost break_weight_sum_;
  Cost best_delta_;
  std::vector<ElementIndex> exceptions_;
  std::vector<std::vector<ElementIndex>> maximal_exceptions_;
  ElementBoolVector break_solution_;
  BaseInt bnb_node_countdown_;
};

class BinPackingSetCoverModel : public scp::FullToCoreModel {
  struct BinPackingModelGlobals {
    // Dirty hack to avoid invalidation of pointers/references
    // A pointer to this data structure is used to compute the hash of bins
    // starting from their indices.
    scp::Model full_model;

    // External bins do not have a valid index in the model, a temporary pointer
    // is used insteda (even dirtier hack).
    const SparseColumn* candidate_bin;

    const SparseColumn& GetBin(SubsetIndex j) const;
  };

  struct BinHash {
    const BinPackingModelGlobals* globals;
    uint64_t operator()(SubsetIndex j) const;
  };

  struct BinEq {
    const BinPackingModelGlobals* globals;
    uint64_t operator()(SubsetIndex j1, SubsetIndex j2) const;
  };

 public:
  BinPackingSetCoverModel(const BinPackingModel* bpp_model)
      : globals_{scp::Model(), nullptr},
        bpp_model_(bpp_model),
        knapsack_solver_(),
        bin_set_({}, 0, BinHash{&globals_}, BinEq{&globals_}),
        column_gen_countdown_(10),
        column_gen_period_(10) {}
  const scp::Model& full_model() const { return globals_.full_model; }

  bool AddBin(const SparseColumn& bin);
  void CompleteModel() {
    globals_.full_model.CreateSparseRowView();
    static_cast<scp::FullToCoreModel&>(*this) =
        scp::FullToCoreModel(&globals_.full_model);
  }

  bool UpdateCore(Cost best_lower_bound,
                  const ElementCostVector& best_multipliers,
                  const scp::Solution& best_solution, bool force) override;

 private:
  bool TryInsertBin(const SparseColumn& bin);

  BinPackingModelGlobals globals_;
  const BinPackingModel* bpp_model_;
  ExpKnap knapsack_solver_;

  // Contains bin indices, but it really should contains "bins" (aka,
  // SparseColumn). However to avoid redundant allocations (in scp::Model and
  // in the set) we cannot store them also here. We cannot also use
  // iterators/pointers/references because they can be invalidated. So we store
  // bin indices and do ungodly hacky shenanigans to get the bins from them.
  absl::flat_hash_set<SubsetIndex, BinHash, BinEq> bin_set_;

  Cost prev_lower_bound_;
  BaseInt column_gen_countdown_;
  BaseInt column_gen_period_;
};

BinPackingModel ReadBpp(absl::string_view filename);
BinPackingModel ReadCsp(absl::string_view filename);

void BestFit(const BinPackingModel& model,
             const std::vector<ElementIndex>& items, PartialBins& bins_data);

BinPackingSetCoverModel GenerateBins(const BinPackingModel& model,
                                     BaseInt num_bins = 0);

}  // namespace operations_research
#endif /* OR_TOOLS_ORTOOLS_ALGORITHMS_BIN_PACKING_H */
