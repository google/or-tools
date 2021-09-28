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

#include "ortools/packing/arc_flow_builder.h"

#include <algorithm>
#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/topologicalsorter.h"

namespace operations_research {
namespace packing {
namespace {

class ArcFlowBuilder {
 public:
  // Same arguments as BuildArcFlowGraph(): see the .h.
  ArcFlowBuilder(const std::vector<int>& bin_dimensions,
                 const std::vector<std::vector<int>>& item_dimensions_by_type,
                 const std::vector<int>& demand_by_type);

  // Builds the arc-flow graph.
  ArcFlowGraph BuildVectorBinPackingGraph();

  // For debugging purposes.tring(
  // Returns the number of states explored in the dynamic programming phase.
  int64_t NumDpStates() const;

 private:
  // All items data, regrouped for sorting purposes.
  struct Item {
    std::vector<int> dimensions;
    int demand;
    int original_index;

    // Used to sort items by relative size.
    double NormalizedSize(const std::vector<int>& bin_dimensions) const;
  };

  // State of the dynamic programming algorithm.
  struct DpState {
    int cur_item_index;
    int cur_item_quantity;
    std::vector<int> used_dimensions;
    // DP State indices of the states that can be obtained by moving
    // either "right" to (cur_item_index, cur_item_quantity++) or "up"
    // to (cur_item_index++, cur_item_quantity=0). -1 if impossible.
    int right_child;
    int up_child;
  };

  // Add item iteratively to create all possible nodes in a forward pass.
  void ForwardCreationPass(DpState* dp_state);
  // Scan DP-nodes backward to relabels each nodes by increasing them as much
  // as possible.
  void BackwardCompressionPass(int state_index);
  // Relabel nodes by decreasing them as much as possible.
  void ForwardCompressionPass(const std::vector<int>& source_node);

  // Can we fit one more item in the bin?
  bool CanFitNewItem(const std::vector<int>& used_dimensions, int item) const;
  // Create a new used_dimensions that is used_dimensions + item dimensions.
  std::vector<int> AddItem(const std::vector<int>& used_dimensions,
                           int item) const;

  // DpState helpers.
  int LookupOrCreateDpState(int item, int quantity,
                            const std::vector<int>& used_dimensions);

  const std::vector<int> bin_dimensions_;
  std::vector<Item> items_;

  typedef absl::flat_hash_map<std::vector<int>, int> VectorIntIntMap;
  int GetOrCreateNode(const std::vector<int>& used_dimensions);

  // We store all DP states in a dense vector, and remember their index
  // in the dp_state_index_ map (we use a tri-dimensional indexing because
  // it's faster for the hash part).
  std::vector<DpState*> dp_states_;
  std::vector<std::vector<VectorIntIntMap>> dp_state_index_;

  // The ArcFlowGraph will have nodes which will correspond to "some"
  // of the vector<int> representing the partial bin usages encountered during
  // the algo. These two data structures map one to the other (note that nodes
  // are dense integers).
  absl::flat_hash_map<std::vector<int>, int> node_indices_;
  std::vector<std::vector<int>> nodes_;

  std::set<ArcFlowGraph::Arc> arcs_;
};

double ArcFlowBuilder::Item::NormalizedSize(
    const std::vector<int>& bin_dimensions) const {
  double size = 0.0;
  for (int i = 0; i < bin_dimensions.size(); ++i) {
    size += static_cast<double>(dimensions[i]) / bin_dimensions[i];
  }
  return size;
}

int64_t ArcFlowBuilder::NumDpStates() const {
  int64_t res = 1;  // We do not store the initial state.
  for (const auto& it1 : dp_state_index_) {
    for (const auto& it2 : it1) {
      res += it2.size();
    }
  }
  return res;
}

ArcFlowBuilder::ArcFlowBuilder(
    const std::vector<int>& bin_dimensions,
    const std::vector<std::vector<int>>& item_dimensions_by_type,
    const std::vector<int>& demand_by_type)
    : bin_dimensions_(bin_dimensions) {
  // Checks dimensions.
  for (int i = 0; i < bin_dimensions.size(); ++i) {
    CHECK_GT(bin_dimensions[i], 0);
  }

  const int num_items = item_dimensions_by_type.size();
  items_.resize(num_items);
  for (int i = 0; i < num_items; ++i) {
    items_[i].dimensions = item_dimensions_by_type[i];
    items_[i].demand = demand_by_type[i];
    items_[i].original_index = i;
  }
  std::sort(items_.begin(), items_.end(), [&](const Item& a, const Item& b) {
    return a.NormalizedSize(bin_dimensions_) >
           b.NormalizedSize(bin_dimensions_);
  });
}

bool ArcFlowBuilder::CanFitNewItem(const std::vector<int>& used_dimensions,
                                   int item) const {
  for (int d = 0; d < bin_dimensions_.size(); ++d) {
    if (used_dimensions[d] + items_[item].dimensions[d] > bin_dimensions_[d]) {
      return false;
    }
  }
  return true;
}

std::vector<int> ArcFlowBuilder::AddItem(
    const std::vector<int>& used_dimensions, int item) const {
  DCHECK(CanFitNewItem(used_dimensions, item));
  std::vector<int> result = used_dimensions;
  for (int d = 0; d < bin_dimensions_.size(); ++d) {
    result[d] += items_[item].dimensions[d];
  }
  return result;
}

int ArcFlowBuilder::GetOrCreateNode(const std::vector<int>& used_dimensions) {
  const auto& it = node_indices_.find(used_dimensions);
  if (it != node_indices_.end()) {
    return it->second;
  }
  const int index = node_indices_.size();
  node_indices_[used_dimensions] = index;
  nodes_.push_back(used_dimensions);
  return index;
}

ArcFlowGraph ArcFlowBuilder::BuildVectorBinPackingGraph() {
  // Initialize the DP states map.
  dp_state_index_.resize(items_.size());
  for (int i = 0; i < items_.size(); ++i) {
    dp_state_index_[i].resize(items_[i].demand + 1);
  }

  // Explore all possible DP states (starting from the initial 'empty' state),
  // and remember their ancestry.
  std::vector<int> zero(bin_dimensions_.size(), 0);
  dp_states_.push_back(new DpState({0, 0, zero, -1, -1}));
  for (int i = 0; i < dp_states_.size(); ++i) {
    ForwardCreationPass(dp_states_[i]);
  }

  // We can clear the dp_state_index map as it will not be used anymore.
  // From now on, we will use the dp_states.used_dimensions to store the new
  // labels in the backward pass.
  const int64_t num_dp_states = NumDpStates();
  dp_state_index_.clear();

  // Backwards pass: "push" the bin dimensions as far as possible.
  const int num_states = dp_states_.size();
  std::vector<std::pair<int, int>> flat_deps;
  for (int i = 0; i < dp_states_.size(); ++i) {
    if (dp_states_[i]->up_child != -1) {
      flat_deps.push_back(std::make_pair(dp_states_[i]->up_child, i));
    }
    if (dp_states_[i]->right_child != -1) {
      flat_deps.push_back(std::make_pair(dp_states_[i]->right_child, i));
    }
  }
  const std::vector<int> sorted_work =
      util::graph::DenseIntStableTopologicalSortOrDie(num_states, flat_deps);
  for (const int w : sorted_work) {
    BackwardCompressionPass(w);
  }

  // ForwardCreationPass again, push the bin dimensions as low as possible.
  const std::vector<int> source_node = dp_states_[0]->used_dimensions;
  // We can now delete the states stored in dp_states_.
  gtl::STLDeleteElements(&dp_states_);
  ForwardCompressionPass(source_node);

  // We need to connect all nodes that corresponds to at least one item selected
  // to the sink node.
  const int sink_node_index = nodes_.size() - 1;
  for (int node = 1; node < sink_node_index; ++node) {
    arcs_.insert({node, sink_node_index, -1});
  }

  ArcFlowGraph result;
  result.arcs.assign(arcs_.begin(), arcs_.end());
  result.nodes.assign(nodes_.begin(), nodes_.end());
  result.num_dp_states = num_dp_states;
  return result;
}

int ArcFlowBuilder::LookupOrCreateDpState(
    int item, int quantity, const std::vector<int>& used_dimensions) {
  VectorIntIntMap& map = dp_state_index_[item][quantity];
  const int index =
      map.insert({used_dimensions, dp_states_.size()}).first->second;
  if (index == dp_states_.size()) {
    dp_states_.push_back(
        new DpState({item, quantity, used_dimensions, -1, -1}));
  }
  return index;
}

void ArcFlowBuilder::ForwardCreationPass(DpState* dp_state) {
  const int item = dp_state->cur_item_index;
  const int quantity = dp_state->cur_item_quantity;
  const std::vector<int>& used_dimensions = dp_state->used_dimensions;

  // Explore path up.
  if (item < items_.size() - 1) {
    dp_state->up_child = LookupOrCreateDpState(item + 1, 0, used_dimensions);
  } else {
    dp_state->up_child = -1;
  }

  // Explore path right.
  if (quantity < items_[item].demand && CanFitNewItem(used_dimensions, item)) {
    const std::vector<int> added = AddItem(used_dimensions, item);
    dp_state->right_child = LookupOrCreateDpState(item, quantity + 1, added);
  } else {
    dp_state->right_child = -1;
  }
}

void ArcFlowBuilder::BackwardCompressionPass(int state_index) {
  // The goal of this function is to fill this.
  std::vector<int>& result = dp_states_[state_index]->used_dimensions;

  // Inherit our result from the result one step up.
  const int up_index = dp_states_[state_index]->up_child;
  const std::vector<int>& result_up =
      up_index == -1 ? bin_dimensions_ : dp_states_[up_index]->used_dimensions;
  result = result_up;

  // Adjust our result from the result one step right.
  const int right_index = dp_states_[state_index]->right_child;
  if (right_index == -1) return;  // We're done.
  const std::vector<int>& result_right =
      dp_states_[right_index]->used_dimensions;
  const Item& item = items_[dp_states_[state_index]->cur_item_index];
  for (int d = 0; d < bin_dimensions_.size(); ++d) {
    result[d] = std::min(result[d], result_right[d] - item.dimensions[d]);
  }

  // Insert the arc from the node to the "right" node.
  const int node = GetOrCreateNode(result);
  const int right_node = GetOrCreateNode(result_right);
  DCHECK_NE(node, right_node);
  arcs_.insert({node, right_node, item.original_index});
  // Also insert the 'dotted' arc from the node to the "up" node (if different).
  if (result != result_up) {
    const int up_node = GetOrCreateNode(result_up);
    arcs_.insert({node, up_node, -1});
  }
}

// Reverse version of the backward pass.
// Revisit states forward, and relabel nodes with the longest path in each
// dimensions from the source. The only meaningfull difference is that we use
// arcs and nodes, instead of dp_states.
void ArcFlowBuilder::ForwardCompressionPass(
    const std::vector<int>& source_node) {
  const int num_nodes = node_indices_.size();
  const int num_dims = bin_dimensions_.size();
  std::set<ArcFlowGraph::Arc> new_arcs;
  std::vector<std::vector<int>> new_nodes;
  VectorIntIntMap new_node_indices;
  std::vector<int> node_remap(num_nodes, -1);
  // We need to revert the sorting of items as arcs store the original index.
  std::vector<int> reverse_item_index_map(items_.size(), -1);
  for (int i = 0; i < items_.size(); ++i) {
    reverse_item_index_map[items_[i].original_index] = i;
  }

  std::vector<std::pair<int, int>> forward_deps;
  std::vector<std::vector<ArcFlowGraph::Arc>> incoming_arcs(num_nodes);
  for (const ArcFlowGraph::Arc& arc : arcs_) {
    forward_deps.push_back(std::make_pair(arc.source, arc.destination));
    incoming_arcs[arc.destination].push_back(arc);
  }

  const std::vector<int> sorted_work =
      util::graph::DenseIntStableTopologicalSortOrDie(num_nodes, forward_deps);

  const int old_source_node = GetOrCreateNode(source_node);
  const int old_sink_node = GetOrCreateNode(bin_dimensions_);
  CHECK_EQ(sorted_work.front(), old_source_node);
  CHECK_EQ(sorted_work.back(), old_sink_node);

  // Process nodes in order and remap state to max(previous_state + item
  // dimensions).
  for (const int w : sorted_work) {
    std::vector<int> new_used(num_dims, 0);
    if (w == sorted_work.back()) {  // Do not compress the sink node.
      new_used = bin_dimensions_;
    } else {
      for (const ArcFlowGraph::Arc& arc : incoming_arcs[w]) {
        const int item =
            arc.item_index == -1 ? -1 : reverse_item_index_map[arc.item_index];
        const int prev_node = node_remap[arc.source];
        const std::vector<int>& prev = new_nodes[prev_node];
        DCHECK_NE(prev_node, -1);
        for (int d = 0; d < num_dims; ++d) {
          if (item != -1) {
            new_used[d] =
                std::max(new_used[d], prev[d] + items_[item].dimensions[d]);
          } else {
            new_used[d] = std::max(new_used[d], prev[d]);
          }
        }
      }
    }
    const auto& it = new_node_indices.find(new_used);
    if (it != new_node_indices.end()) {
      node_remap[w] = it->second;
    } else {
      const int new_index = new_nodes.size();
      new_nodes.push_back(new_used);
      new_node_indices[new_used] = new_index;
      node_remap[w] = new_index;
    }
  }
  // Remap arcs.
  for (const ArcFlowGraph::Arc& arc : arcs_) {
    CHECK_NE(node_remap[arc.source], -1);
    CHECK_NE(node_remap[arc.destination], -1);
    // Remove loss arcs between merged nodes.
    if (arc.item_index == -1 &&
        node_remap[arc.source] == node_remap[arc.destination])
      continue;
    new_arcs.insert(
        {node_remap[arc.source], node_remap[arc.destination], arc.item_index});
  }
  VLOG(1) << "Reduced nodes from " << num_nodes << " to " << new_nodes.size();
  VLOG(1) << "Reduced arcs from " << arcs_.size() << " to " << new_arcs.size();
  nodes_ = new_nodes;
  arcs_ = new_arcs;
  CHECK_NE(node_remap[old_source_node], -1);
  CHECK_EQ(0, node_remap[old_source_node]);
  CHECK_NE(node_remap[old_sink_node], -1);
  CHECK_EQ(nodes_.size() - 1, node_remap[old_sink_node]);
}

}  // namespace

bool ArcFlowGraph::Arc::operator<(const ArcFlowGraph::Arc& other) const {
  if (source != other.source) return source < other.source;
  if (destination != other.destination) return destination < other.destination;
  return item_index < other.item_index;
}

ArcFlowGraph BuildArcFlowGraph(
    const std::vector<int>& bin_dimensions,
    const std::vector<std::vector<int>>& item_dimensions_by_type,
    const std::vector<int>& demand_by_type) {
  ArcFlowBuilder afb(bin_dimensions, item_dimensions_by_type, demand_by_type);
  return afb.BuildVectorBinPackingGraph();
}

}  // namespace packing
}  // namespace operations_research
