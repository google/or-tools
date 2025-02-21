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

#include "ortools/sat/routing_cuts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/connected_components.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/max_flow.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

namespace {

// If we have many sets of (var >= values relations) and we know at least one
// set is true, then we can derive valid global lower bounds on the variable
// that appear in all such sets.
//
// This represents "one step" of computing such bounds by adding the sets one at
// the time. When we process a set 'new_lbs':
// - For the first set, then 'new_lbs' is globally valid.
// - For a new set, we need to erase variables not appearing in new_lbs and
//   take the min for the ones appearing in both.
//
// TODO(user): The operation is symmetric, so we could swap both hash_map if
// new_lbs is smaller than current_lbs as a small optimization.
void ComputeMinLowerBoundOfSharedVariables(
    const absl::flat_hash_map<IntegerVariable, IntegerValue>& new_lbs,
    absl::flat_hash_map<IntegerVariable, IntegerValue>* current_lbs) {
  if (new_lbs.empty()) current_lbs->clear();
  for (auto it = current_lbs->begin(); it != current_lbs->end();) {
    const IntegerVariable var = it->first;
    auto other_it = new_lbs.find(var);
    if (other_it == new_lbs.end()) {
      // We have no bounds about var in the new_fact, we need to erase it.
      //
      // Note: it = current_lbs->erase(it) does not work for flat_hash_map, this
      // is the recommended way.
      current_lbs->erase(it++);
    } else {
      // Update.
      it->second = std::min(it->second, other_it->second);
      ++it;
    }
  }
}

}  // namespace

MinOutgoingFlowHelper::MinOutgoingFlowHelper(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals, Model* model)
    : tails_(tails),
      heads_(heads),
      literals_(literals),
      binary_relation_repository_(
          *model->GetOrCreate<BinaryRelationRepository>()),
      trail_(*model->GetOrCreate<Trail>()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      in_subset_(num_nodes, false),
      index_in_subset_(num_nodes, -1),
      incoming_arc_indices_(num_nodes),
      outgoing_arc_indices_(num_nodes),
      reachable_(num_nodes, false),
      next_reachable_(num_nodes, false),
      node_var_lower_bounds_(num_nodes),
      next_node_var_lower_bounds_(num_nodes) {}

MinOutgoingFlowHelper::~MinOutgoingFlowHelper() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"RoutingDp/num_full_dp_calls", num_full_dp_calls_});
  stats.push_back({"RoutingDp/num_full_dp_skips", num_full_dp_skips_});
  stats.push_back(
      {"RoutingDp/num_full_dp_early_abort", num_full_dp_early_abort_});
  stats.push_back(
      {"RoutingDp/num_full_dp_work_abort", num_full_dp_work_abort_});
  for (const auto& [name, count] : num_by_type_) {
    stats.push_back({absl::StrCat("RoutingDp/num_bounds_", name), count});
  }
  shared_stats_->AddStats(stats);
}

int MinOutgoingFlowHelper::ComputeDemandBasedMinOutgoingFlow(
    absl::Span<const int> subset, const RouteRelationsHelper& helper) {
  DCHECK_EQ(helper.num_nodes(), in_subset_.size());
  DCHECK_EQ(helper.num_arcs(), tails_.size());

  // TODO(user): When we try multiple algorithm from this class on the same
  // subset, we should initialize the graph just once as this is costly on large
  // problem.
  InitializeGraph(subset);

  int min_outgoing_flow = 1;
  std::string best_name;
  for (int d = 0; d < helper.num_dimensions(); ++d) {
    for (const bool negate_variables : {false, true}) {
      for (const bool use_incoming : {true, false}) {
        bool gcd_was_used = false;
        const int bound = ComputeMinNumberOfBins(
            RelaxIntoSpecialBinPackingProblem(subset, d, negate_variables,
                                              use_incoming, helper),
            &gcd_was_used);
        if (bound > min_outgoing_flow) {
          min_outgoing_flow = bound;
          best_name = absl::StrCat((use_incoming ? "in_" : "out_"),
                                   (gcd_was_used ? "gcd_" : ""),
                                   (negate_variables ? "neg_" : ""), d);
        }
      }
    }
  }

  if (min_outgoing_flow > 1) num_by_type_[best_name]++;
  return min_outgoing_flow;
}

absl::Span<ItemOrBin> MinOutgoingFlowHelper::RelaxIntoSpecialBinPackingProblem(
    absl::Span<const int> subset, int dimension, bool negate_variables,
    bool use_incoming, const RouteRelationsHelper& helper) {
  tmp_bin_packing_problem_.clear();

  // Computes UB/LB. The derivation in the .h used a simple version, but we can
  // be a bit tighter. We explain for the use_incoming case only:
  //
  // In this setting, recall that bins correspond to the first node of each
  // route covering the subset. The max_upper_bound is a maximum over the last
  // node of each route, and as such it does not need to consider nodes with no
  // arc leaving the subset: such nodes cannot be last.
  //
  // Moreover, if the bin is not empty, then the bound for that bin does not
  // need to consider the variable of the "bin" node itself. If the new bound is
  // non-negative, then it is also valid for the empty bin. If it is negative we
  // have an issue as using it will force the bin to not be empty. But we can
  // always use std::max(0, new_bound) which is always valid and still tighter
  // than the bound explained in the .h which is always non-negative.
  //
  // This way we can have a different bound for each bin, which is slightly
  // stronger than using the same bound for all of them.
  const int num_nodes = in_subset_.size();
  std::vector<IntegerValue> min_lower_bound_of_others(num_nodes,
                                                      kMaxIntegerValue);
  std::vector<IntegerValue> max_upper_bound_of_others(num_nodes,
                                                      kMinIntegerValue);

  // We do a forward and a backward pass in order to compute the min/max
  // of all the other nodes for each node.
  for (const bool forward_pass : {true, false}) {
    IntegerValue local_min = kMaxIntegerValue;
    IntegerValue local_max = kMinIntegerValue;
    const int size = subset.size();
    for (int i = 0; i < size; ++i) {
      const int n = forward_pass ? subset[i] : subset[size - 1 - i];
      IntegerVariable var = helper.GetNodeVariable(n, dimension);
      if (var == kNoIntegerVariable) return {};
      if (negate_variables) var = NegationOf(var);

      // The local_min/max contains the min/max of all nodes strictly before 'n'
      // in the forward pass (resp. striclty after). So after two passes,
      // min/max_..._of_others[n] will contains the min/max of all nodes
      // different from n.
      min_lower_bound_of_others[n] =
          std::min(min_lower_bound_of_others[n], local_min);
      max_upper_bound_of_others[n] =
          std::max(max_upper_bound_of_others[n], local_max);

      // Update local_min/max.
      if (has_incoming_arcs_from_outside_[n]) {
        local_min =
            std::min(local_min, integer_trail_.LevelZeroLowerBound(var));
      }
      if (has_outgoing_arcs_to_outside_[n]) {
        local_max =
            std::max(local_max, integer_trail_.LevelZeroUpperBound(var));
      }
    }
  }

  // TODO(user): This should probably be moved to RouteRelationsHelper.
  auto get_relation_lhs = [&](int arc_index) {
    const auto& r = helper.GetArcRelation(arc_index, dimension);
    if (r.empty() || r.head_coeff != 1 || r.tail_coeff != -1) {
      // Use X_head-X_tail \in [lb(X_head)-ub(X_tail), ub(X_head)-lb(X_tail)].
      const IntegerVariable tail_var =
          helper.GetNodeVariable(tails_[arc_index], dimension);
      const IntegerVariable head_var =
          helper.GetNodeVariable(heads_[arc_index], dimension);
      if (negate_variables) {
        // Opposite of the rhs.
        return integer_trail_.LevelZeroLowerBound(tail_var) -
               integer_trail_.LevelZeroUpperBound(head_var);
      }
      return integer_trail_.LevelZeroLowerBound(head_var) -
             integer_trail_.LevelZeroUpperBound(tail_var);
    }
    return negate_variables ? -r.rhs : r.lhs;
  };

  for (const int n : subset) {
    IntegerVariable var = helper.GetNodeVariable(n, dimension);
    if (negate_variables) var = NegationOf(var);

    const absl::Span<const int> arcs =
        use_incoming ? incoming_arc_indices_[n] : outgoing_arc_indices_[n];
    IntegerValue demand = kMaxIntegerValue;
    for (const int a : arcs) {
      demand = std::min(demand, get_relation_lhs(a));
    }

    ItemOrBin obj;
    obj.demand = demand;

    // We compute the capacity like this to avoid overflow and always have
    // a non-negative capacity (see above).
    obj.capacity = 0;
    if (use_incoming) {
      if (max_upper_bound_of_others[n] >
          integer_trail_.LevelZeroLowerBound(var)) {
        obj.capacity = max_upper_bound_of_others[n] -
                       integer_trail_.LevelZeroLowerBound(var);
      }
    } else {
      if (integer_trail_.LevelZeroUpperBound(var) >
          min_lower_bound_of_others[n]) {
        obj.capacity = integer_trail_.LevelZeroUpperBound(var) -
                       min_lower_bound_of_others[n];
      }
    }

    // Note that we don't explicitly deal with the corner case of a subset node
    // with no arcs. This corresponds to INFEASIBLE problem and should be dealt
    // with elsewhere. Being "less restrictive" will still result in a valid
    // bound and that is enough here.
    if ((use_incoming && !has_incoming_arcs_from_outside_[n]) ||
        (!use_incoming && !has_outgoing_arcs_to_outside_[n])) {
      obj.type = ItemOrBin::MUST_BE_ITEM;
      obj.capacity = 0;
    } else if (arcs.empty()) {
      obj.type = ItemOrBin::MUST_BE_BIN;
      obj.demand = 0;  // We don't want kMaxIntegerValue !
    } else {
      obj.type = ItemOrBin::ITEM_OR_BIN;
    }

    tmp_bin_packing_problem_.push_back(obj);
  }

  return absl::MakeSpan(tmp_bin_packing_problem_);
}

int ComputeMinNumberOfBins(absl::Span<ItemOrBin> objects, bool* gcd_was_used) {
  if (objects.empty()) return 0;

  IntegerValue sum_of_demands(0);
  int64_t gcd = 0;
  for (const ItemOrBin& obj : objects) {
    sum_of_demands = CapAddI(sum_of_demands, obj.demand);
    gcd = std::gcd(gcd, std::abs(obj.demand.value()));
  }

  // TODO(user): we can probably handle a couple of extra case rather than just
  // bailing out here and below.
  if (AtMinOrMaxInt64I(sum_of_demands)) return 0;

  // If the gcd of all the demands term is positive, we can divide everything.
  *gcd_was_used = (gcd > 1);
  if (gcd > 1) {
    for (ItemOrBin& obj : objects) {
      obj.demand /= gcd;
      obj.capacity = FloorRatio(obj.capacity, gcd);
    }
    sum_of_demands /= gcd;
  }

  // For a given choice of bins (set B), a feasible problem must satisfy.
  //     sum_{i \notin B} demands_i <= sum_{i \in B} capacity_i.
  //
  // Using this we can compute a lower bound on the number of bins needed
  // using a greedy algorithm that chooses B in order to make the above
  // inequality as "loose" as possible.
  //
  // This puts 'a' before 'b' if we get more unused capacity by using 'a' as a
  // bin and 'b' as an item rather than the other way around. If we call the
  // other items demands D and capacities C, the two options are:
  // - option1:   a.demand  + D <= b.capacity + C
  // - option2:   b.demand  + D <= a.capacity + C
  //
  // The option2 above leads to more unused capacity:
  //    (a.capacity - b.demand) > (b.capacity - a.demand).
  std::stable_sort(objects.begin(), objects.end(),
                   [](const ItemOrBin& a, const ItemOrBin& b) {
                     if (a.type != b.type) {
                       // We want in order:
                       // MUST_BE_BIN, ITEM_OR_BIN, MUST_BE_ITEM.
                       return a.type > b.type;
                     }
                     return a.capacity + a.demand > b.capacity + b.demand;
                   });

  // We start with no bins (sum_of_demands=everything, sum_of_capacity=0) and
  // add the best bins one by one until we have sum_of_demands <=
  // sum_of_capacity.
  int num_bins = 0;
  IntegerValue sum_of_capacity(0);
  for (; num_bins < objects.size(); ++num_bins) {
    // Use obj as a bin instead of as an item, if possible (i.e., unless
    // obj.type is MUST_BE_ITEM).
    const ItemOrBin& obj = objects[num_bins];
    if (obj.type != ItemOrBin::MUST_BE_BIN &&
        sum_of_demands <= sum_of_capacity) {
      return num_bins;
    }
    if (obj.type == ItemOrBin::MUST_BE_ITEM) {
      // Because of our order, we only have objects of type MUST_BE_ITEM left.
      // Hence we can no longer change items to bins, and since the demands
      // exceed the capacity, the problem is infeasible. We just return the
      // (number of objects + 1) in this case (any bound is valid).
      DCHECK_GT(sum_of_demands, sum_of_capacity);
      return objects.size() + 1;
    }
    sum_of_capacity = CapAddI(sum_of_capacity, obj.capacity);
    if (AtMinOrMaxInt64I(sum_of_capacity)) return num_bins;
    sum_of_demands -= obj.demand;
  }
  return num_bins;
}

int MinOutgoingFlowHelper::ComputeMinOutgoingFlow(
    absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK(absl::c_all_of(next_reachable_, [](bool b) { return !b; }));
  DCHECK(absl::c_all_of(node_var_lower_bounds_,
                        [](const auto& m) { return m.empty(); }));
  DCHECK(absl::c_all_of(next_node_var_lower_bounds_,
                        [](const auto& m) { return m.empty(); }));

  InitializeGraph(subset);

  // Conservatively assume that each subset node is reachable from outside.
  // TODO(user): use has_incoming_arcs_from_outside_[] to be more precise.
  reachable_ = in_subset_;

  // Maximum number of nodes of a feasible path inside the subset.
  int longest_path_length = 1;
  absl::flat_hash_map<IntegerVariable, IntegerValue> tmp_lbs;
  while (longest_path_length < subset.size()) {
    bool at_least_one_next_node_reachable = false;
    for (const int head : subset) {
      bool is_reachable = false;
      for (const int incoming_arc_index : incoming_arc_indices_[head]) {
        const int tail = tails_[incoming_arc_index];
        const Literal lit = literals_[incoming_arc_index];
        if (!reachable_[tail]) continue;

        // If this arc cannot be taken skip.
        tmp_lbs.clear();
        if (!binary_relation_repository_.PropagateLocalBounds(
                integer_trail_, lit, node_var_lower_bounds_[tail], &tmp_lbs)) {
          continue;
        }

        if (!is_reachable) {
          // This is the first arc that reach this node.
          is_reachable = true;
          next_node_var_lower_bounds_[head] = tmp_lbs;
        } else {
          // Combine information with previous one.
          ComputeMinLowerBoundOfSharedVariables(
              tmp_lbs, &next_node_var_lower_bounds_[head]);
        }
      }

      next_reachable_[head] = is_reachable;
      if (next_reachable_[head]) at_least_one_next_node_reachable = true;
    }
    if (!at_least_one_next_node_reachable) {
      break;
    }
    std::swap(reachable_, next_reachable_);
    std::swap(node_var_lower_bounds_, next_node_var_lower_bounds_);
    for (const int n : subset) {
      next_node_var_lower_bounds_[n].clear();
    }
    ++longest_path_length;
  }

  // The maximum number of distinct paths of length `longest_path_length`.
  int max_longest_paths = 0;
  for (const int n : subset) {
    if (reachable_[n]) ++max_longest_paths;

    // Reset the temporary data structures for the next call.
    next_reachable_[n] = false;
    node_var_lower_bounds_[n].clear();
    next_node_var_lower_bounds_[n].clear();
  }
  return GetMinOutgoingFlow(subset.size(), longest_path_length,
                            max_longest_paths);
}

int MinOutgoingFlowHelper::GetMinOutgoingFlow(int subset_size,
                                              int longest_path_length,
                                              int max_longest_paths) {
  if (max_longest_paths * longest_path_length < subset_size) {
    // If `longest_path_length` is 1, there should be one such path per node,
    // and thus `max_longest_paths` should be the subset size. This branch can
    // thus not be taken in this case.
    DCHECK_GT(longest_path_length, 1);
    // TODO(user): Consider using information about the number of shorter
    // paths to derive an even better bound.
    return max_longest_paths +
           MathUtil::CeilOfRatio(
               subset_size - max_longest_paths * longest_path_length,
               longest_path_length - 1);
  }
  return MathUtil::CeilOfRatio(subset_size, longest_path_length);
}

namespace {
struct Path {
  uint32_t node_set;  // Bit i is set iif node subset[i] is in the path.
  int last_node;      // The last node in the path.

  bool operator==(const Path& p) const {
    return node_set == p.node_set && last_node == p.last_node;
  }

  template <typename H>
  friend H AbslHashValue(H h, const Path& p) {
    return H::combine(std::move(h), p.node_set, p.last_node);
  }
};

}  // namespace

void MinOutgoingFlowHelper::InitializeGraph(absl::Span<const int> subset) {
  const int num_nodes = in_subset_.size();
  in_subset_.assign(num_nodes, false);
  index_in_subset_.assign(num_nodes, -1);
  for (int i = 0; i < subset.size(); ++i) {
    const int n = subset[i];
    in_subset_[n] = true;
    index_in_subset_[n] = i;
  }

  has_incoming_arcs_from_outside_.assign(num_nodes, false);
  has_outgoing_arcs_to_outside_.assign(num_nodes, false);

  for (auto& v : incoming_arc_indices_) v.clear();
  for (auto& v : outgoing_arc_indices_) v.clear();
  for (int i = 0; i < tails_.size(); ++i) {
    const int tail = tails_[i];
    const int head = heads_[i];

    // we always ignore self-arcs here.
    if (tail == head) continue;

    if (in_subset_[tail] && in_subset_[head]) {
      outgoing_arc_indices_[tail].push_back(i);
      incoming_arc_indices_[head].push_back(i);
    } else if (in_subset_[tail] && !in_subset_[head]) {
      has_outgoing_arcs_to_outside_[tail] = true;
    } else if (!in_subset_[tail] && in_subset_[head]) {
      has_incoming_arcs_from_outside_[head] = true;
    }
  }
}

int MinOutgoingFlowHelper::ComputeTightMinOutgoingFlow(
    absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LE(subset.size(), 32);

  std::vector<int> longest_path_length_by_end_node(subset.size(), 1);
  InitializeGraph(subset);

  absl::flat_hash_map<IntegerVariable, IntegerValue> tmp_lbs;
  absl::flat_hash_map<Path, absl::flat_hash_map<IntegerVariable, IntegerValue>>
      path_var_bounds;
  std::vector<Path> paths;
  std::vector<Path> next_paths;
  for (int i = 0; i < subset.size(); ++i) {
    // TODO(user): use has_incoming_arcs_from_outside_[] to skip some nodes.
    paths.push_back(
        {.node_set = static_cast<uint32_t>(1 << i), .last_node = subset[i]});
    path_var_bounds[paths.back()] = {};  // LevelZero bounds.
  }
  int longest_path_length = 1;
  for (int path_length = 1; path_length <= subset.size(); ++path_length) {
    for (const Path& path : paths) {
      // We remove it from the hash_map since this entry should no longer be
      // used as we create path of increasing length.
      DCHECK(path_var_bounds.contains(path));
      const absl::flat_hash_map<IntegerVariable, IntegerValue> path_bounds =
          std::move(path_var_bounds.extract(path).mapped());

      // We have found a feasible path, update the path length statistics...
      longest_path_length = path_length;
      longest_path_length_by_end_node[index_in_subset_[path.last_node]] =
          path_length;

      // ... and try to extend the path with each arc going out of its last
      // node.
      for (const int outgoing_arc_index :
           outgoing_arc_indices_[path.last_node]) {
        const int head = heads_[outgoing_arc_index];
        const int head_index_in_subset = index_in_subset_[head];
        DCHECK_NE(head_index_in_subset, -1);
        if (path.node_set & (1 << head_index_in_subset)) continue;
        const Path new_path = {
            .node_set = path.node_set | (1 << head_index_in_subset),
            .last_node = head};

        // If this arc cannot be taken skip.
        tmp_lbs.clear();
        if (!binary_relation_repository_.PropagateLocalBounds(
                integer_trail_, literals_[outgoing_arc_index], path_bounds,
                &tmp_lbs)) {
          continue;
        }

        const auto [it, inserted] = path_var_bounds.insert({new_path, tmp_lbs});
        if (inserted) {
          // We have a feasible path to a new state.
          next_paths.push_back(new_path);
        } else {
          // We found another way to reach this state, only keep common best
          // bounds.
          ComputeMinLowerBoundOfSharedVariables(tmp_lbs, &it->second);
        }
      }
    }
    std::swap(paths, next_paths);
    next_paths.clear();
  }

  int max_longest_paths = 0;
  for (int i = 0; i < subset.size(); ++i) {
    if (longest_path_length_by_end_node[i] == longest_path_length) {
      ++max_longest_paths;
    }
  }

  return GetMinOutgoingFlow(subset.size(), longest_path_length,
                            max_longest_paths);
}

bool MinOutgoingFlowHelper::SubsetMightBeServedWithKRoutes(
    int k, absl::Span<const int> subset) {
  if (k >= subset.size()) return true;
  if (subset.size() > 31) return true;

  ++num_full_dp_calls_;
  InitializeGraph(subset);

  struct State {
    // Bit i is set iif node subset[i] is in one of the current routes.
    uint32_t node_set;

    // The last nodes of each of the k routes. If the hamming weight is less
    // that k, then at least one route is still empty.
    uint32_t last_nodes_set;

    // Valid lower bounds for this state.
    //
    // Note that unlike the other algorithm here, we keep the collective bounds
    // of all the nodes so far, so this is likely in
    // O(longest_route * num_dimensions) which can take quite a lot of space.
    //
    // By "dimensions", we mean the number of variables appearing in binary
    // relation controlled by an arc literal. See for instance
    // RouteRelationsHelper that also uses a similar definition.
    //
    // Hopefully the DFS order limit the number of entry to O(n^2 * k), so still
    // somewhat reasonable for small values.
    absl::flat_hash_map<IntegerVariable, IntegerValue> lbs;
  };

  const int size = subset.size();
  const uint32_t final_mask = (1 << size) - 1;

  // This is also correlated to the work done, and we abort if we starts to
  // do too much work on one instance.
  int64_t allocated_memory_estimate = 0;

  // We just do a DFS from the initial state.
  std::vector<State> states;
  states.push_back(State());
  while (!states.empty()) {
    if (allocated_memory_estimate > 1e7) {
      ++num_full_dp_work_abort_;
      return true;  // Abort.
    }
    const State from_state = std::move(states.back());
    states.pop_back();

    // The number of routes is the hamming weight of from_state.last_nodes_set.
    const int num_routes = absl::popcount(from_state.last_nodes_set);

    // We start by choosing the first k starts (in increasing order).
    // For that we only add after the maximum position already chosen.
    if (num_routes < k) {
      const int num_extra = k - num_routes - 1;
      for (int i = 0; i + num_extra < size; ++i) {
        if (from_state.node_set >> i) continue;
        if (!has_incoming_arcs_from_outside_[subset[i]]) continue;

        // All "initial-state" start with empty hash-map that correspond to
        // the level zero bounds.
        State to_state;
        const uint32_t head_mask = (1 << i);
        to_state.node_set = from_state.node_set | head_mask;
        to_state.last_nodes_set = from_state.last_nodes_set | head_mask;
        if (to_state.node_set == final_mask) {
          ++num_full_dp_early_abort_;
          return true;  // All served!
        }
        states.push_back(std::move(to_state));
      }
      continue;
    }

    // We have k routes, extend one of the last nodes.
    //
    // TODO(user): we cannot have any last node with
    // has_outgoing_arcs_to_outside_[node] at false! Exploit this.
    for (int i = 0; i < size; ++i) {
      const uint32_t tail_mask = 1 << i;
      if ((from_state.last_nodes_set & tail_mask) == 0) continue;

      for (const int outgoing_arc_index : outgoing_arc_indices_[subset[i]]) {
        const int head = heads_[outgoing_arc_index];
        const uint32_t head_mask = (1 << index_in_subset_[head]);
        if (from_state.node_set & head_mask) continue;

        State to_state;
        to_state.lbs = from_state.lbs;  // keep old bounds
        if (!binary_relation_repository_.PropagateLocalBounds(
                integer_trail_, literals_[outgoing_arc_index], from_state.lbs,
                &to_state.lbs)) {
          continue;
        }

        to_state.node_set = from_state.node_set | head_mask;
        to_state.last_nodes_set = from_state.last_nodes_set | head_mask;
        to_state.last_nodes_set ^= tail_mask;
        allocated_memory_estimate += to_state.lbs.size();
        if (to_state.node_set == final_mask) {
          ++num_full_dp_early_abort_;
          return true;  // All served!
        }
        states.push_back(std::move(to_state));
      }
    }
  }

  // We explored everything, no way to serve this with only k routes!
  return false;
}

namespace {
IntegerVariable UniqueSharedVariable(const Relation& r1, const Relation& r2) {
  DCHECK_NE(r1.a.var, r1.b.var);
  DCHECK_NE(r2.a.var, r2.b.var);
  if (r1.a.var == r2.a.var && r1.b.var != r2.b.var) return r1.a.var;
  if (r1.a.var == r2.b.var && r1.b.var != r2.a.var) return r1.a.var;
  if (r1.b.var == r2.a.var && r1.a.var != r2.b.var) return r1.b.var;
  if (r1.b.var == r2.b.var && r1.a.var != r2.a.var) return r1.b.var;
  return kNoIntegerVariable;
}

class RouteRelationsBuilder {
 public:
  using Relation = RouteRelationsHelper::Relation;

  RouteRelationsBuilder(
      int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
      absl::Span<const Literal> literals,
      absl::Span<const IntegerVariable> flat_node_dim_variables,
      const BinaryRelationRepository& binary_relation_repository)
      : num_nodes_(num_nodes),
        num_arcs_(tails.size()),
        tails_(tails),
        heads_(heads),
        literals_(literals),
        binary_relation_repository_(binary_relation_repository) {
    if (!flat_node_dim_variables.empty()) {
      DCHECK_EQ(flat_node_dim_variables.size() % num_nodes, 0);
      num_dimensions_ = flat_node_dim_variables.size() / num_nodes;
      flat_node_dim_variables_.assign(flat_node_dim_variables.begin(),
                                      flat_node_dim_variables.end());
    }
  }

  int num_dimensions() const { return num_dimensions_; }

  const std::vector<IntegerVariable>& flat_node_dim_variables() const {
    return flat_node_dim_variables_;
  }

  const std::vector<Relation>& flat_arc_dim_relations() const {
    return flat_arc_dim_relations_;
  }

  bool Build() {
    if (flat_node_dim_variables_.empty()) {
      // Step 1: find the number of dimensions (as the number of connected
      // components in the graph of binary relations), and find to which
      // dimension each variable belongs.
      ComputeDimensionOfEachVariable();
      if (num_dimensions_ == 0) return false;

      // Step 2: find the variables which can be unambiguously associated with a
      // node and dimension.
      // - compute the indices of the binary relations which can be
      // unambiguously associated with the incoming and outgoing arcs of each
      // node, per dimension.
      ComputeAdjacentRelationsPerNodeAndDimension();
      // - find variable associations by using variables which are uniquely
      // shared by two adjacent relations of a node.
      std::queue<std::pair<int, int>> node_dim_pairs =
          ComputeVarAssociationsFromSharedVariableOfAdjacentRelations();
      if (node_dim_pairs.empty()) return false;
      // - find more variable associations by using arcs from nodes with
      // an associated variable, whose other end has no associated variable, and
      // where only one variable can be associated with it.
      ComputeVarAssociationsFromRelationsWithSingleFreeVar(node_dim_pairs);
    }

    // Step 3: compute the relation for each arc and dimension, now that the
    // variables associated with each node and dimension are known.
    ComputeArcRelations();
    return true;
  }

 private:
  IntegerVariable& node_variable(int node, int dimension) {
    return flat_node_dim_variables_[node * num_dimensions_ + dimension];
  };

  const Relation& arc_relation(int arc_index, int dimension) const {
    return flat_arc_dim_relations_[arc_index * num_dimensions_ + dimension];
  }

  void SetArcRelation(int arc_index, int dimension, IntegerVariable tail_var,
                      const sat::Relation& r) {
    Relation& relation =
        flat_arc_dim_relations_[arc_index * num_dimensions_ + dimension];
    // If several relations are associated with the same arc, keep the first one
    // found, unless the new one has opposite +1/-1 coefficients (these
    // relations are preferred because they can be used to compute "demand
    // based" min outgoing flows).
    if (!relation.empty()) {
      if (IntTypeAbs(r.a.coeff) != 1 || r.b.coeff != -r.a.coeff) return;
    }
    if (tail_var == r.a.var) {
      relation.tail_coeff = r.a.coeff;
      relation.head_coeff = r.b.coeff;
    } else {
      relation.tail_coeff = r.b.coeff;
      relation.head_coeff = r.a.coeff;
    }
    relation.lhs = r.lhs;
    relation.rhs = r.rhs;
    if (relation.head_coeff < 0) {
      relation.tail_coeff = -relation.tail_coeff;
      relation.head_coeff = -relation.head_coeff;
      relation.lhs = -relation.lhs;
      relation.rhs = -relation.rhs;
      std::swap(relation.lhs, relation.rhs);
    }
  }

  void ComputeDimensionOfEachVariable() {
    // Step 1: find the number of dimensions (as the number of connected
    // components in the graph of binary relations).
    // TODO(user): see if we can use a shared
    // DenseConnectedComponentsFinder with one node per variable of the whole
    // model instead.
    ConnectedComponentsFinder<IntegerVariable> cc_finder;
    for (int i = 0; i < num_arcs_; ++i) {
      if (tails_[i] == heads_[i]) continue;
      num_arcs_per_literal_[literals_[i]]++;
      for (const int relation_index :
           binary_relation_repository_.IndicesOfRelationsEnforcedBy(
               literals_[i])) {
        const auto& r = binary_relation_repository_.relation(relation_index);
        if (r.a.var == kNoIntegerVariable || r.b.var == kNoIntegerVariable) {
          continue;
        }
        cc_finder.AddEdge(r.a.var, r.b.var);
      }
    }
    const std::vector<std::vector<IntegerVariable>> connected_components =
        cc_finder.FindConnectedComponents();
    for (int i = 0; i < connected_components.size(); ++i) {
      for (const IntegerVariable var : connected_components[i]) {
        dimension_by_var_[var] = i;
      }
    }
    num_dimensions_ = connected_components.size();
  }

  void ComputeAdjacentRelationsPerNodeAndDimension() {
    adjacent_relation_indices_ = std::vector<std::vector<std::vector<int>>>(
        num_dimensions_, std::vector<std::vector<int>>(num_nodes_));
    for (int i = 0; i < num_arcs_; ++i) {
      if (tails_[i] == heads_[i]) continue;
      // If a literal is associated with more than one arc, a relation
      // associated with this literal cannot be unambiguously associated with an
      // arc.
      if (num_arcs_per_literal_[literals_[i]] > 1) continue;
      for (const int relation_index :
           binary_relation_repository_.IndicesOfRelationsEnforcedBy(
               literals_[i])) {
        const auto& r = binary_relation_repository_.relation(relation_index);
        if (r.a.var == kNoIntegerVariable || r.b.var == kNoIntegerVariable) {
          continue;
        }
        const int dimension = dimension_by_var_[r.a.var];
        adjacent_relation_indices_[dimension][tails_[i]].push_back(
            relation_index);
        adjacent_relation_indices_[dimension][heads_[i]].push_back(
            relation_index);
      }
    }
  }

  // Returns the (node, dimension) pairs for which a variable association has
  // been found.
  std::queue<std::pair<int, int>>
  ComputeVarAssociationsFromSharedVariableOfAdjacentRelations() {
    flat_node_dim_variables_ = std::vector<IntegerVariable>(
        num_nodes_ * num_dimensions_, kNoIntegerVariable);
    std::queue<std::pair<int, int>> result;
    for (int n = 0; n < num_nodes_; ++n) {
      for (int d = 0; d < num_dimensions_; ++d) {
        // If two relations on incoming or outgoing arcs of n have a unique
        // shared variable, such as in the case of l <-X,Y-> n <-Y,Z-> m (i.e. a
        // relation between X and Y on the (l,n) arc, and a relation between Y
        // and Z on the (n,m) arc), then this variable is necessarily associated
        // with n.
        for (const int r1_index : adjacent_relation_indices_[d][n]) {
          const auto& r1 = binary_relation_repository_.relation(r1_index);
          for (const int r2_index : adjacent_relation_indices_[d][n]) {
            if (r1_index == r2_index) continue;
            const auto& r2 = binary_relation_repository_.relation(r2_index);
            const IntegerVariable shared_var = UniqueSharedVariable(r1, r2);
            if (shared_var == kNoIntegerVariable) continue;
            DCHECK_EQ(dimension_by_var_[shared_var], d);
            IntegerVariable& node_var = node_variable(n, d);
            if (node_var == kNoIntegerVariable) {
              result.push({n, d});
            } else if (node_var != shared_var) {
              VLOG(2) << "Several vars per node and dimension in route with "
                      << num_nodes_ << " nodes and " << num_arcs_ << " arcs";
              return {};
            }
            node_var = shared_var;
          }
        }
      }
    }
    return result;
  }

  void ComputeVarAssociationsFromRelationsWithSingleFreeVar(
      std::queue<std::pair<int, int>>& node_dim_pairs_to_process) {
    std::vector<std::vector<int>> adjacent_arcs_per_node(num_nodes_);
    for (int i = 0; i < num_arcs_; ++i) {
      if (tails_[i] == heads_[i]) continue;
      adjacent_arcs_per_node[tails_[i]].push_back(i);
      adjacent_arcs_per_node[heads_[i]].push_back(i);
    }
    while (!node_dim_pairs_to_process.empty()) {
      const auto [node, dimension] = node_dim_pairs_to_process.front();
      const IntegerVariable node_var = node_variable(node, dimension);
      DCHECK_NE(node_var, kNoIntegerVariable);
      node_dim_pairs_to_process.pop();
      for (const int arc_index : adjacent_arcs_per_node[node]) {
        const int tail = tails_[arc_index];
        const int head = heads_[arc_index];
        DCHECK(node == tail || node == head);
        int other_node = node == tail ? head : tail;
        if (node_variable(other_node, dimension) != kNoIntegerVariable) {
          continue;
        }
        IntegerVariable candidate_var = kNoIntegerVariable;
        bool candidate_var_is_unique = true;
        for (const int relation_index :
             binary_relation_repository_.IndicesOfRelationsEnforcedBy(
                 literals_[arc_index])) {
          const auto& r = binary_relation_repository_.relation(relation_index);
          if (r.a.var == kNoIntegerVariable || r.b.var == kNoIntegerVariable) {
            continue;
          }
          if (r.a.var == node_var) {
            if (candidate_var != kNoIntegerVariable &&
                candidate_var != r.b.var) {
              candidate_var_is_unique = false;
              break;
            }
            candidate_var = r.b.var;
          }
          if (r.b.var == node_var) {
            if (candidate_var != kNoIntegerVariable &&
                candidate_var != r.a.var) {
              candidate_var_is_unique = false;
              break;
            }
            candidate_var = r.a.var;
          }
        }
        if (candidate_var != kNoIntegerVariable && candidate_var_is_unique) {
          node_variable(other_node, dimension) = candidate_var;
          node_dim_pairs_to_process.push({other_node, dimension});
        }
      }
    }
  }

  void ComputeArcRelations() {
    flat_arc_dim_relations_ =
        std::vector<Relation>(num_arcs_ * num_dimensions_, Relation());
    int num_inconsistent_relations = 0;
    for (int i = 0; i < num_arcs_; ++i) {
      const int tail = tails_[i];
      const int head = heads_[i];
      if (tail == head) continue;
      for (const int relation_index :
           binary_relation_repository_.IndicesOfRelationsEnforcedBy(
               literals_[i])) {
        const auto& r = binary_relation_repository_.relation(relation_index);
        if (r.a.var == kNoIntegerVariable || r.b.var == kNoIntegerVariable) {
          continue;
        }
        for (int dimension = 0; dimension < num_dimensions_; ++dimension) {
          IntegerVariable& tail_var = node_variable(tail, dimension);
          IntegerVariable& head_var = node_variable(head, dimension);
          if (tail_var == kNoIntegerVariable ||
              head_var == kNoIntegerVariable) {
            continue;
          }
          if (!((tail_var == r.a.var && head_var == r.b.var) ||
                (tail_var == r.b.var && head_var == r.a.var))) {
            ++num_inconsistent_relations;
            continue;
          }
          SetArcRelation(i, dimension, tail_var, r);
        }
      }
      // If some relations are missing for this arc, check if we can use
      // enforced relations to fill them.
      for (int d = 0; d < num_dimensions_; ++d) {
        if (!arc_relation(i, d).empty()) continue;
        const IntegerVariable tail_var = node_variable(tail, d);
        const IntegerVariable head_var = node_variable(head, d);
        if (tail_var == kNoIntegerVariable || head_var == kNoIntegerVariable) {
          continue;
        }
        for (const int relation_index :
             binary_relation_repository_.IndicesOfRelationsBetween(tail_var,
                                                                   head_var)) {
          SetArcRelation(i, d, tail_var,
                         binary_relation_repository_.relation(relation_index));
        }
      }
    }
    if (num_inconsistent_relations > 0) {
      VLOG(2) << num_inconsistent_relations
              << " inconsistent relations in route with " << num_nodes_
              << " nodes and " << num_arcs_ << " arcs";
    }
  }

  const int num_nodes_;
  const int num_arcs_;
  absl::Span<const int> tails_;
  absl::Span<const int> heads_;
  absl::Span<const Literal> literals_;
  const BinaryRelationRepository& binary_relation_repository_;

  int num_dimensions_;
  absl::flat_hash_map<IntegerVariable, int> dimension_by_var_;
  absl::flat_hash_map<Literal, int> num_arcs_per_literal_;
  // The indices of the binary relations associated with the incoming and
  // outgoing arcs of each node, per dimension.
  std::vector<std::vector<std::vector<int>>> adjacent_relation_indices_;
  // The variable associated with node n and dimension d (or kNoIntegerVariable
  // if there is none) is at index n * num_dimensions_ + d.
  std::vector<IntegerVariable> flat_node_dim_variables_;
  // The relation associated with arc a and dimension d (or kNoRelation if
  // there is none) is at index a * num_dimensions_ + d.
  std::vector<Relation> flat_arc_dim_relations_;
};
}  // namespace

std::unique_ptr<RouteRelationsHelper> RouteRelationsHelper::Create(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals,
    absl::Span<const IntegerVariable> flat_node_dim_variables,
    const BinaryRelationRepository& binary_relation_repository) {
  RouteRelationsBuilder builder(num_nodes, tails, heads, literals,
                                flat_node_dim_variables,
                                binary_relation_repository);
  if (!builder.Build()) return nullptr;
  std::unique_ptr<RouteRelationsHelper> helper(new RouteRelationsHelper(
      builder.num_dimensions(), builder.flat_node_dim_variables(),
      builder.flat_arc_dim_relations()));
  if (VLOG_IS_ON(2)) helper->LogStats();
  return helper;
}

RouteRelationsHelper::RouteRelationsHelper(
    int num_dimensions, std::vector<IntegerVariable> flat_node_dim_variables,
    std::vector<Relation> flat_arc_dim_relations)
    : num_dimensions_(num_dimensions),
      flat_node_dim_variables_(std::move(flat_node_dim_variables)),
      flat_arc_dim_relations_(std::move(flat_arc_dim_relations)) {
  DCHECK_GE(num_dimensions_, 1);
}

void RouteRelationsHelper::RemoveArcs(
    absl::Span<const int> sorted_arc_indices) {
  int new_size = 0;
  const int num_arcs = this->num_arcs();
  for (int i = 0; i < num_arcs; ++i) {
    if (!sorted_arc_indices.empty() && sorted_arc_indices.front() == i) {
      sorted_arc_indices.remove_prefix(1);
      continue;
    }
    for (int d = 0; d < num_dimensions_; ++d) {
      flat_arc_dim_relations_[new_size++] =
          flat_arc_dim_relations_[i * num_dimensions_ + d];
    }
  }
  flat_arc_dim_relations_.resize(new_size);
}

void RouteRelationsHelper::LogStats() const {
  const int num_nodes = this->num_nodes();
  const int num_arcs = this->num_arcs();
  LOG(INFO) << "Route with " << num_nodes << " nodes and " << num_arcs
            << " arcs";
  for (int d = 0; d < num_dimensions_; ++d) {
    int num_vars = 0;
    int num_relations = 0;
    for (int i = 0; i < num_nodes; ++i) {
      if (GetNodeVariable(i, d) != kNoIntegerVariable) ++num_vars;
    }
    for (int i = 0; i < num_arcs; ++i) {
      if (!GetArcRelation(i, d).empty()) ++num_relations;
    }
    LOG(INFO) << "dimension " << d << ": " << num_vars << " vars and "
              << num_relations << " relations";
  }
}

namespace {
// Converts a literal index in a model proto to a Literal.
Literal ToLiteral(int lit) {
  return Literal(BooleanVariable(PositiveRef(lit)), RefIsPositive(lit));
}

// Converts a variable index in a NodeVariables proto to an IntegerVariable.
IntegerVariable ToPositiveIntegerVariable(int i) {
  return IntegerVariable(PositiveRef(i) << 1);
}

// Converts an IntegerVariable to variable indices in a NodeVariables proto.
int ToNodeVariableIndex(IntegerVariable var) {
  if (var == kNoIntegerVariable) return -1;
  return var.value() >> 1;
}

// Returns a repository containing a partial view (i.e. without coefficients or
// domains) of the enforced linear constraints (of size 2 only) in `model`. This
// is the only information needed to infer the mapping from variables to nodes
// in routes constraints.
BinaryRelationRepository ComputePartialBinaryRelationRepository(
    const CpModelProto& model) {
  BinaryRelationRepository repository;
  for (const ConstraintProto& ct : model.constraints()) {
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    const absl::Span<const int> vars = ct.linear().vars();
    if (ct.enforcement_literal().size() != 1 || vars.size() != 2) continue;
    if (vars[0] == vars[1]) continue;
    repository.AddPartialRelation(ToLiteral(ct.enforcement_literal(0)),
                                  ToPositiveIntegerVariable(vars[0]),
                                  ToPositiveIntegerVariable(vars[1]));
  }
  repository.Build();
  return repository;
}

// Returns the number of dimensions added to the constraint.
int MaybeFillRoutesConstraintNodeVariables(
    RoutesConstraintProto& routes, const BinaryRelationRepository& repository) {
  int max_node = 0;
  for (const int node : routes.tails()) {
    max_node = std::max(max_node, node);
  }
  for (const int node : routes.heads()) {
    max_node = std::max(max_node, node);
  }
  const int num_nodes = max_node + 1;
  std::vector<Literal> literals;
  literals.reserve(routes.literals_size());
  for (int lit : routes.literals()) {
    literals.push_back(ToLiteral(lit));
  }
  const std::unique_ptr<RouteRelationsHelper> helper =
      RouteRelationsHelper::Create(num_nodes, routes.tails(), routes.heads(),
                                   literals,
                                   /*flat_node_dim_variables=*/{}, repository);
  if (helper == nullptr) return 0;

  for (int d = 0; d < helper->num_dimensions(); ++d) {
    RoutesConstraintProto::NodeVariables& dimension = *routes.add_dimensions();
    for (int n = 0; n < num_nodes; ++n) {
      const IntegerVariable var = helper->GetNodeVariable(n, d);
      dimension.add_vars(ToNodeVariableIndex(var));
    }
  }
  return helper->num_dimensions();
}

}  // namespace

std::pair<int, int> MaybeFillMissingRoutesConstraintNodeVariables(
    const CpModelProto& input_model, CpModelProto& output_model) {
  std::vector<RoutesConstraintProto*> routes_to_fill;
  for (ConstraintProto& ct : *output_model.mutable_constraints()) {
    if (ct.constraint_case() != ConstraintProto::kRoutes) continue;
    if (!ct.routes().dimensions().empty()) continue;
    routes_to_fill.push_back(ct.mutable_routes());
  }
  if (routes_to_fill.empty()) return {0, 0};

  int total_num_dimensions = 0;
  const BinaryRelationRepository partial_repository =
      ComputePartialBinaryRelationRepository(input_model);
  for (RoutesConstraintProto* routes : routes_to_fill) {
    total_num_dimensions +=
        MaybeFillRoutesConstraintNodeVariables(*routes, partial_repository);
  }
  return {static_cast<int>(routes_to_fill.size()), total_num_dimensions};
}

namespace {

class OutgoingCutHelper {
 public:
  using NodeVariables = RoutesConstraintProto::NodeVariables;

  OutgoingCutHelper(int num_nodes, bool is_route_constraint, int64_t capacity,
                    absl::Span<const int64_t> demands,
                    absl::Span<const IntegerVariable> flat_node_dim_variables,
                    absl::Span<const int> tails, absl::Span<const int> heads,
                    absl::Span<const Literal> literals, Model* model)
      : num_nodes_(num_nodes),
        is_route_constraint_(is_route_constraint),
        capacity_(capacity),
        demands_(demands.begin(), demands.end()),
        tails_(tails.begin(), tails.end()),
        heads_(heads.begin(), heads.end()),
        literals_(literals.begin(), literals.end()),
        params_(*model->GetOrCreate<SatParameters>()),
        trail_(*model->GetOrCreate<Trail>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        encoder_(model->GetOrCreate<IntegerEncoder>()),
        in_subset_(num_nodes, false),
        self_arc_literal_(num_nodes_),
        self_arc_lp_value_(num_nodes_),
        nodes_incoming_weight_(num_nodes_),
        nodes_outgoing_weight_(num_nodes_),
        min_outgoing_flow_helper_(num_nodes, tails_, heads_, literals_, model),
        route_relations_helper_(RouteRelationsHelper::Create(
            num_nodes, tails, heads, literals, flat_node_dim_variables,
            *model->GetOrCreate<BinaryRelationRepository>())) {
    // Compute the total demands in order to know the minimum incoming/outgoing
    // flow.
    for (const int64_t demand : demands) total_demand_ += demand;
    complement_of_subset_.reserve(num_nodes_);
  }

  int num_nodes() const { return num_nodes_; }

  bool is_route_constraint() const { return is_route_constraint_; }

  // Returns the arcs computed by InitializeForNewLpSolution().
  absl::Span<const ArcWithLpValue> relevant_arcs() const {
    return relevant_arcs_;
  }

  // Returns the arcs computed in InitializeForNewLpSolution(), sorted by
  // decreasing lp_value.
  absl::Span<const std::pair<int, int>> ordered_arcs() const {
    return ordered_arcs_;
  }

  // This should be called each time a new LP solution is available, before
  // using the other methods.
  void InitializeForNewLpSolution(LinearConstraintManager* manager);

  // Merge the relevant arcs (tail, head) and (head, tail) into a single (tail,
  // head) arc, with the sum of their lp_values.
  absl::Span<const ArcWithLpValue> SymmetrizedRelevantArcs() {
    symmetrized_relevant_arcs_ = relevant_arcs_;
    SymmetrizeArcs(&symmetrized_relevant_arcs_);
    return symmetrized_relevant_arcs_;
  }

  // Try to add an outgoing cut from the given subset.
  bool TrySubsetCut(std::string name, absl::Span<const int> subset,
                    LinearConstraintManager* manager);

  // If we look at the symmetrized version (tail <-> head = tail->head +
  // head->tail) and we split all the edges between a subset of nodes S and the
  // outside into a set A and the other d(S)\A, and |A| is odd, we have a
  // constraint of the form:
  //   "all edge of A at 1" => sum other edges >= 1.
  // This is because a cycle or multiple-cycle must go in/out an even number
  // of time. This enforced constraint simply linearize to:
  //    sum_d(S)\A x_e + sum_A (1 - x_e) >= 1.
  //
  // Given a subset of nodes, it is easy to identify the best subset A of edge
  // to consider.
  bool TryBlossomSubsetCut(std::string name,
                           absl::Span<const ArcWithLpValue> symmetrized_edges,
                           absl::Span<const int> subset,
                           LinearConstraintManager* manager);

 private:
  // Removes the arcs with a literal fixed at false at level zero. This is
  // especially useful to remove fixed self loop.
  void FilterFalseArcsAtLevelZero();

  // Add a cut of the form Sum_{outgoing arcs from S} lp >= rhs_lower_bound.
  //
  // Note that we used to also add the same cut for the incoming arcs, but
  // because of flow conservation on these problems, the outgoing flow is always
  // the same as the incoming flow, so adding this extra cut doesn't seem
  // relevant.
  bool AddOutgoingCut(LinearConstraintManager* manager, std::string name,
                      int subset_size, const std::vector<bool>& in_subset,
                      int64_t rhs_lower_bound, int outside_node_to_ignore);

  const int num_nodes_;
  const bool is_route_constraint_;
  const int64_t capacity_;
  std::vector<int64_t> demands_;
  std::vector<int> tails_;
  std::vector<int> heads_;
  std::vector<Literal> literals_;
  std::vector<ArcWithLpValue> relevant_arcs_;
  std::vector<ArcWithLpValue> symmetrized_relevant_arcs_;
  std::vector<std::pair<int, int>> ordered_arcs_;

  const SatParameters& params_;
  const Trail& trail_;
  ModelRandomGenerator* random_;
  IntegerEncoder* encoder_;

  int64_t total_demand_ = 0;
  std::vector<bool> in_subset_;
  std::vector<int> complement_of_subset_;

  // Self-arc information, indexed in [0, num_nodes_)
  std::vector<int> nodes_with_self_arc_;
  std::vector<Literal> self_arc_literal_;
  std::vector<double> self_arc_lp_value_;

  // Temporary memory used by TrySubsetCut().
  std::vector<double> nodes_incoming_weight_;
  std::vector<double> nodes_outgoing_weight_;

  MaxBoundedSubsetSum max_bounded_subset_sum_;
  MaxBoundedSubsetSumExact max_bounded_subset_sum_exact_;
  MinOutgoingFlowHelper min_outgoing_flow_helper_;
  std::unique_ptr<RouteRelationsHelper> route_relations_helper_;
};

void OutgoingCutHelper::FilterFalseArcsAtLevelZero() {
  if (trail_.CurrentDecisionLevel() != 0) return;

  int new_size = 0;
  const int size = static_cast<int>(tails_.size());
  const VariablesAssignment& assignment = trail_.Assignment();
  std::vector<int> removed_arcs;
  for (int i = 0; i < size; ++i) {
    if (assignment.LiteralIsFalse(literals_[i])) {
      removed_arcs.push_back(i);
      continue;
    }
    tails_[new_size] = tails_[i];
    heads_[new_size] = heads_[i];
    literals_[new_size] = literals_[i];
    ++new_size;
  }
  if (new_size < size) {
    tails_.resize(new_size);
    heads_.resize(new_size);
    literals_.resize(new_size);
    if (route_relations_helper_ != nullptr) {
      route_relations_helper_->RemoveArcs(removed_arcs);
    }
  }
}

void OutgoingCutHelper::InitializeForNewLpSolution(
    LinearConstraintManager* manager) {
  FilterFalseArcsAtLevelZero();

  // We will collect only the arcs with a positive lp_values to speed up some
  // computation below.
  relevant_arcs_.clear();
  nodes_with_self_arc_.clear();

  // Sort the arcs by non-increasing lp_values.
  const auto& lp_values = manager->LpValues();
  std::vector<std::pair<double, int>> relevant_arc_by_decreasing_lp_values;
  for (int i = 0; i < literals_.size(); ++i) {
    const IntegerVariable direct_view = encoder_->GetLiteralView(literals_[i]);
    const double lp_value =
        direct_view != kNoIntegerVariable
            ? lp_values[direct_view]
            : 1.0 - lp_values[encoder_->GetLiteralView(literals_[i].Negated())];

    // We treat self-edge separately.
    // Note also that we do not need to include them in relevant_arcs_.
    //
    // TODO(user): If there are multiple self-arc, the code should still
    // work, but is not ideal.
    if (tails_[i] == heads_[i]) {
      const int node = tails_[i];
      nodes_with_self_arc_.push_back(node);
      self_arc_lp_value_[node] = lp_value;
      self_arc_literal_[node] = literals_[i];
      continue;
    }

    if (lp_value < 1e-6) continue;
    relevant_arcs_.push_back({tails_[i], heads_[i], lp_value});
    relevant_arc_by_decreasing_lp_values.push_back({lp_value, i});
  }
  std::sort(relevant_arc_by_decreasing_lp_values.begin(),
            relevant_arc_by_decreasing_lp_values.end(),
            std::greater<std::pair<double, int>>());

  gtl::STLSortAndRemoveDuplicates(&nodes_with_self_arc_);

  ordered_arcs_.clear();
  for (const auto& [score, arc] : relevant_arc_by_decreasing_lp_values) {
    ordered_arcs_.push_back({tails_[arc], heads_[arc]});
  }
}

namespace {

// Compute the current outgoing/incoming flow out of the subset.
// In many cases this will be the same, but not with outside_node_to_ignore
// or in case our LP does not contain all the constraints.
//
// Looping over all arcs can take a significant portion of the running time,
// it is why it is faster to do it only on arcs with non-zero lp values which
// should be in linear number rather than the total number of arc which can be
// quadratic.
//
// TODO(user): For the symmetric case there is an even faster algo. See if
// it can be generalized to the asymmetric one if become needed.
// Reference is algo 6.4 of the "The Traveling Salesman Problem" book
// mentionned above.
std::pair<double, double> GetIncomingAndOutgoingLpFlow(
    absl::Span<const ArcWithLpValue> relevant_arcs,
    const std::vector<bool>& in_subset, int outside_node_to_ignore = -1) {
  double outgoing_flow = 0.0;
  double incoming_flow = 0.0;
  for (const auto arc : relevant_arcs) {
    const bool tail_in = in_subset[arc.tail];
    const bool head_in = in_subset[arc.head];
    if (tail_in && !head_in) {
      if (arc.head == outside_node_to_ignore) continue;
      outgoing_flow += arc.lp_value;
    } else if (!tail_in && head_in) {
      if (arc.tail == outside_node_to_ignore) continue;
      incoming_flow += arc.lp_value;
    }
  }
  return {incoming_flow, outgoing_flow};
}

}  // namespace

bool OutgoingCutHelper::AddOutgoingCut(LinearConstraintManager* manager,
                                       std::string name, int subset_size,
                                       const std::vector<bool>& in_subset,
                                       int64_t rhs_lower_bound,
                                       int outside_node_to_ignore) {
  // Skip cut if it is not violated.
  const auto [in_flow, out_flow] = GetIncomingAndOutgoingLpFlow(
      relevant_arcs_, in_subset, outside_node_to_ignore);
  const double out_violation = static_cast<double>(rhs_lower_bound) - out_flow;
  const double in_violation = static_cast<double>(rhs_lower_bound) - in_flow;
  if (out_violation <= 1e-3 && in_violation <= 1e-3) return false;

  // We create the cut and rely on AddCut() for computing its efficacy and
  // rejecting it if it is bad.
  LinearConstraintBuilder outgoing(encoder_, IntegerValue(rhs_lower_bound),
                                   kMaxIntegerValue);
  LinearConstraintBuilder incoming(encoder_, IntegerValue(rhs_lower_bound),
                                   kMaxIntegerValue);

  // Rather than doing two loops, we initialize the cuts right away, even if
  // only one of them will be used.
  for (int i = 0; i < tails_.size(); ++i) {
    const bool tail_in = in_subset[tails_[i]];
    const bool head_in = in_subset[heads_[i]];
    if (tail_in && !head_in) {
      if (heads_[i] == outside_node_to_ignore) continue;
      CHECK(outgoing.AddLiteralTerm(literals_[i], IntegerValue(1)));
    } else if (!tail_in && head_in) {
      if (tails_[i] == outside_node_to_ignore) continue;
      CHECK(incoming.AddLiteralTerm(literals_[i], IntegerValue(1)));
    }
  }

  // As arcs get fixed (this happens a lot in LNS subproblems), even if the
  // incoming flow is the same as the outgoing flow, the number of incoming arcs
  // might be widely different from the one of outgoing arcs. We prefer to pick
  // the sparser cut.
  const double out_efficacy = out_violation / std::sqrt(outgoing.NumTerms());
  const double in_efficacy = in_violation / std::sqrt(incoming.NumTerms());

  // Select the best option between outgoing and incoming.
  LinearConstraintBuilder& cut_builder =
      (out_efficacy >= in_efficacy) ? outgoing : incoming;

  // A node is said to be optional if it can be excluded from the subcircuit,
  // in which case there is a self-loop on that node.
  // If there are optional nodes, use extended formula:
  // sum(cut) >= 1 - optional_loop_in - optional_loop_out
  // where optional_loop_in's node is in subset, optional_loop_out's is out.
  // TODO(user): Favor optional loops fixed to zero at root.
  int num_optional_nodes_in = 0;
  int num_optional_nodes_out = 0;
  int optional_loop_in = -1;
  int optional_loop_out = -1;
  for (const int n : nodes_with_self_arc_) {
    if (in_subset[n]) {
      num_optional_nodes_in++;
      if (optional_loop_in == -1 ||
          self_arc_lp_value_[n] < self_arc_lp_value_[optional_loop_in]) {
        optional_loop_in = n;
      }
    } else {
      num_optional_nodes_out++;
      if (optional_loop_out == -1 ||
          self_arc_lp_value_[n] < self_arc_lp_value_[optional_loop_out]) {
        optional_loop_out = n;
      }
    }
  }

  // This just makes sure we don't call this with a bound > 1 if there is
  // optional node inside the subset.
  CHECK(rhs_lower_bound == 1 || num_optional_nodes_in == 0);

  // Support optional nodes if any.
  if (num_optional_nodes_in + num_optional_nodes_out > 0) {
    // When all optionals of one side are excluded in lp solution, no cut.
    if (num_optional_nodes_in == subset_size &&
        (optional_loop_in == -1 ||
         self_arc_lp_value_[optional_loop_in] > 1.0 - 1e-6)) {
      return false;
    }
    if (num_optional_nodes_out == num_nodes_ - subset_size &&
        (optional_loop_out == -1 ||
         self_arc_lp_value_[optional_loop_out] > 1.0 - 1e-6)) {
      return false;
    }

    // There is no mandatory node in subset, add optional_loop_in.
    if (num_optional_nodes_in == subset_size) {
      CHECK_EQ(rhs_lower_bound, 1);
      CHECK(cut_builder.AddLiteralTerm(self_arc_literal_[optional_loop_in],
                                       IntegerValue(1)));
    }

    // There is no mandatory node out of subset, add optional_loop_out.
    if (num_optional_nodes_out == num_nodes_ - subset_size) {
      CHECK_EQ(rhs_lower_bound, 1);
      CHECK(cut_builder.AddLiteralTerm(self_arc_literal_[optional_loop_out],
                                       IntegerValue(1)));
    }
  }

  return manager->AddCut(cut_builder.Build(), name);
}

bool OutgoingCutHelper::TrySubsetCut(std::string name,
                                     absl::Span<const int> subset,
                                     LinearConstraintManager* manager) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LT(subset.size(), num_nodes_);

  // Do some initialization.
  bool contain_depot = false;
  in_subset_.assign(num_nodes_, false);
  for (const int n : subset) {
    in_subset_[n] = true;
    if (n == 0 && is_route_constraint_) {
      contain_depot = true;
    }
  }

  // For the route-constraint, we will always consider the subset without the
  // depot. We complement it if needed.
  if (contain_depot) {
    complement_of_subset_.clear();
    for (int i = 0; i < num_nodes_; ++i) {
      if (!in_subset_[i]) {
        complement_of_subset_.push_back(i);
      }
      in_subset_[i] = !in_subset_[i];
    }

    // Change the span to point in the new subset!
    subset = complement_of_subset_;
  }

  // For now we can only apply fancy route cuts if all nodes in subset are
  // mandatory.
  bool all_subset_nodes_are_mandatory = true;
  if (is_route_constraint_) {
    for (const int n : nodes_with_self_arc_) {
      if (in_subset_[n]) {
        all_subset_nodes_are_mandatory = false;
        break;
      }
    }
  }

  // The TSP case is "easy".
  //
  // TODO(user): Turn on some of the automatic detection for circuit constraint.
  // Even if we are looking for a full circuit of the mandatory nodes, some
  // side-constraint might require to go in and out of a subset more than once.
  //
  // TODO(user): deal with non-mandatory node in the route constraint?
  if (!is_route_constraint_ || !all_subset_nodes_are_mandatory) {
    return AddOutgoingCut(manager, name, subset.size(), in_subset_,
                          /*rhs_lower_bound=*/1,
                          /*outside_node_to_ignore=*/-1);
  }

  // Compute a lower bound on the outgoing flow assuming all node in the subset
  // must be served.
  int64_t min_outgoing_flow = 1;

  // Bounds inferred automatically from the enforced binary relation of the
  // model.
  //
  // TODO(user): This is still not as good as the "capacity" bounds below in
  // some cases. Fix! we should be able to use the same relation to infer the
  // capacity bounds somehow.
  if (route_relations_helper_ != nullptr) {
    const int bound =
        min_outgoing_flow_helper_.ComputeDemandBasedMinOutgoingFlow(
            subset, *route_relations_helper_);
    if (bound > min_outgoing_flow) {
      absl::StrAppend(&name, "AutomaticDimension");
      min_outgoing_flow = bound;
    }
  }
  if (subset.size() <
      params_.routing_cut_subset_size_for_tight_binary_relation_bound()) {
    const int bound =
        min_outgoing_flow_helper_.ComputeTightMinOutgoingFlow(subset);
    if (bound > min_outgoing_flow) {
      absl::StrAppend(&name, "AutomaticTight");
      min_outgoing_flow = bound;
    }
  } else if (subset.size() <
             params_.routing_cut_subset_size_for_binary_relation_bound()) {
    const int bound = min_outgoing_flow_helper_.ComputeMinOutgoingFlow(subset);
    if (bound > min_outgoing_flow) {
      absl::StrAppend(&name, "Automatic");
      min_outgoing_flow = bound;
    }
  }

  // Bounds coming from the demands_/capacity_ fields (if set).
  // If we cannot reach the capacity given the demands in the subset, we can
  // derive tighter bounds.
  std::vector<int> to_ignore_candidates;
  if (!demands_.empty()) {
    int64_t has_excessive_demands = false;
    int64_t has_negative_demands = false;
    int64_t sum_of_elements = 0;
    std::vector<int64_t> elements;
    for (const int n : subset) {
      const int64_t d = demands_[n];
      if (d < 0) has_negative_demands = true;
      if (d > capacity_) has_excessive_demands = true;
      sum_of_elements += d;
      elements.push_back(d);
    }

    // Lets wait for these to disappear before adding cuts.
    if (has_excessive_demands) return false;

    // Try to tighten the capacity using DP. Note that there is no point doing
    // anything if one route can serve all demands since then the bound is
    // already tight.
    //
    // TODO(user): Compute a bound in the presence of negative demands?
    int64_t tightened_capacity = capacity_;
    int tightening_level = 0;
    if (!has_negative_demands && sum_of_elements > capacity_) {
      max_bounded_subset_sum_.Reset(capacity_);
      for (const int64_t e : elements) {
        max_bounded_subset_sum_.Add(e);
      }
      tightened_capacity = max_bounded_subset_sum_.CurrentMax();
      if (tightened_capacity < capacity_) {
        tightening_level = 1;
      }

      // If the complexity looks ok, try a more expensive DP than the quick one
      // above.
      if (max_bounded_subset_sum_exact_.ComplexityEstimate(
              elements.size(), capacity_) < params_.routing_cut_dp_effort()) {
        const int64_t exact =
            max_bounded_subset_sum_exact_.MaxSubsetSum(elements, capacity_);
        CHECK_LE(exact, tightened_capacity);
        if (exact < tightened_capacity) {
          tightening_level = 2;
          tightened_capacity = exact;
        }
      }
    }

    const int64_t flow_lower_bound =
        MathUtil::CeilOfRatio(sum_of_elements, tightened_capacity);
    if (flow_lower_bound > min_outgoing_flow) {
      min_outgoing_flow = flow_lower_bound;
      absl::StrAppend(&name, "Demand", tightening_level);
    }

    if (flow_lower_bound >= min_outgoing_flow) {
      // We compute the biggest extra item that could fit in 'flow_lower_bound'
      // bins. If the first (flow_lower_bound - 1) bins are tight, i.e. all
      // their tightened_capacity is filled, then the last bin will have
      // 'last_bin_fillin' stuff, which will leave 'space_left' to fit an extra
      // item.
      const int64_t last_bin_fillin =
          sum_of_elements - (flow_lower_bound - 1) * tightened_capacity;
      const int64_t space_left = capacity_ - last_bin_fillin;
      DCHECK_GE(space_left, 0);
      DCHECK_LT(space_left, capacity_);

      // It is possible to make this cut stronger, using similar reasoning to
      // the Multistar CVRP cuts: if there is a node n (other than the depot)
      // outside of `subset`, with a demand that is greater than space_left,
      // then the outgoing flow of (subset + n) is >= flow_lower_bound + 1.
      // Using this, we can show that we still need `flow_lower_bound` on the
      // outgoing arcs of `subset` other than those towards n (noted A in the
      // diagram below):
      //
      //         ^       ^
      //         |       |
      //    ----------------
      // <--|  subset  | n |-->
      //    ----------------
      //         |       |
      //         v       v
      //
      // \------A------/\--B--/
      //
      // By hypothesis, outgoing_flow(A) + outgoing_flow(B) > flow_lower_bound
      // and, since n is not the depot, outgoing_flow(B) <= 1. Hence
      // outgoing_flow(A) >= flow_lower_bound.
      //
      // Note that this reasoning also applies to the incoming_flow, we have the
      // same lower bound from the incoming flow not arriving from such node.
      //
      // Also of note, is that even if this node is optional, the bound is still
      // valid since if any flow leave or come to this node, it must be in the
      // tour.
      for (int n = 1; n < num_nodes_; ++n) {
        if (in_subset_[n]) continue;
        if (demands_[n] > space_left) {
          to_ignore_candidates.push_back(n);
        }
      }
    }
  }

  if (subset.size() <=
      params_.routing_cut_subset_size_for_exact_binary_relation_bound()) {
    // Before doing something expensive, we can check if this might generate
    // a violated cut in the first place.
    const auto [in_flow, out_flow] =
        GetIncomingAndOutgoingLpFlow(relevant_arcs_, in_subset_);
    const double max_flow = std::max(in_flow, out_flow);
    if (max_flow + 1e-2 >= min_outgoing_flow + 1.0) {
      min_outgoing_flow_helper_.ReportDpSkip();
    } else if (!min_outgoing_flow_helper_.SubsetMightBeServedWithKRoutes(
                   min_outgoing_flow, subset)) {
      // TODO(user): Shall we call SubsetMightBeServedWithKRoutes() again
      // with min_outgoing_flow + 1 here?
      absl::StrAppend(&name, "DP");
      min_outgoing_flow += 1;
      to_ignore_candidates.clear();  // no longer valid.
    }
  }

  // Out of to_ignore_candidates, use an heuristic to pick one.
  int outside_node_to_ignore = -1;
  if (!to_ignore_candidates.empty()) {
    absl::StrAppend(&name, "Lifted");

    // Compute the lp weight going from subset to the candidates or from the
    // candidates to the subset.
    //
    // Note that we only reset the position that we care about below.
    for (const int n : to_ignore_candidates) {
      nodes_incoming_weight_[n] = 0;
      nodes_outgoing_weight_[n] = 0;
    }
    for (const auto arc : relevant_arcs_) {
      if (in_subset_[arc.tail] && !in_subset_[arc.head]) {
        nodes_incoming_weight_[arc.head] += arc.lp_value;
      } else if (!in_subset_[arc.tail] && in_subset_[arc.head]) {
        nodes_outgoing_weight_[arc.tail] += arc.lp_value;
      }
    }

    // Pick the set of candidates with the highest lp weight.
    std::vector<int> bests;
    double best_weight = 0.0;
    for (const int n : to_ignore_candidates) {
      const double weight =
          std::max(nodes_outgoing_weight_[n], nodes_incoming_weight_[n]);
      if (bests.empty() || weight > best_weight) {
        bests.clear();
        bests.push_back(n);
        best_weight = weight;
      } else if (weight == best_weight) {
        bests.push_back(n);
      }
    }

    // Randomly pick if we have many "bests".
    outside_node_to_ignore =
        bests.size() == 1
            ? bests[0]
            : bests[absl::Uniform<int>(*random_, 0, bests.size())];
  }

  return AddOutgoingCut(manager, name, subset.size(), in_subset_,
                        /*rhs_lower_bound=*/min_outgoing_flow,
                        outside_node_to_ignore);
}

bool OutgoingCutHelper::TryBlossomSubsetCut(
    std::string name, absl::Span<const ArcWithLpValue> symmetrized_edges,
    absl::Span<const int> subset, LinearConstraintManager* manager) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LT(subset.size(), num_nodes_);

  // Initialize "in_subset".
  for (const int n : subset) in_subset_[n] = true;
  auto cleanup = ::absl::MakeCleanup([subset, this]() {
    for (const int n : subset) in_subset_[n] = false;
  });

  // The heuristic assumes non-duplicates arcs, otherwise they are all bundled
  // together in the same symmetric edge, and the result is probably wrong.
  absl::flat_hash_set<std::pair<int, int>> special_edges;
  int num_inverted = 0;
  double sum_inverted = 0.0;
  double sum = 0.0;
  double best_change = 1.0;
  ArcWithLpValue const* best_swap = nullptr;
  for (const ArcWithLpValue& arc : symmetrized_edges) {
    if (in_subset_[arc.tail] == in_subset_[arc.head]) continue;

    if (arc.lp_value > 0.5) {
      ++num_inverted;
      special_edges.insert({arc.tail, arc.head});
      sum_inverted += 1.0 - arc.lp_value;
    } else {
      sum += arc.lp_value;
    }

    const double change = std::abs(2 * arc.lp_value - 1.0);
    if (change < best_change) {
      best_change = change;
      best_swap = &arc;
    }
  }

  // If the we don't have an odd number, we move the best edge from one set to
  // the other.
  if (num_inverted % 2 == 0) {
    if (best_swap == nullptr) return false;
    if (special_edges.contains({best_swap->tail, best_swap->head})) {
      --num_inverted;
      special_edges.erase({best_swap->tail, best_swap->head});
      sum_inverted -= (1.0 - best_swap->lp_value);
      sum += best_swap->lp_value;
    } else {
      ++num_inverted;
      special_edges.insert({best_swap->tail, best_swap->head});
      sum_inverted += (1.0 - best_swap->lp_value);
      sum -= best_swap->lp_value;
    }
  }
  if (sum + sum_inverted > 0.99) return false;

  // For the route constraint, it is actually allowed to have circuit of size
  // 2, so the reasoning is wrong if one of the edges touches the depot.
  if (!demands_.empty()) {
    for (const auto [tail, head] : special_edges) {
      if (tail == 0) return false;
    }
  }

  // If there is just one special edge, and all other node can be ignored, then
  // the reasonning is wrong too since we can have a 2-cycle. In that case
  // we enforce the constraint when an extra self-loop literal is at zero.
  int best_optional_index = -1;
  if (special_edges.size() == 1) {
    int num_other_optional = 0;
    const auto [special_tail, special_head] = *special_edges.begin();
    for (const int n : nodes_with_self_arc_) {
      if (n != special_head && n != special_tail) {
        ++num_other_optional;
        if (best_optional_index == -1 ||
            self_arc_lp_value_[n] < self_arc_lp_value_[best_optional_index]) {
          best_optional_index = n;
        }
      }
    }
    if (num_other_optional + 2 < num_nodes_) best_optional_index = -1;
  }

  // Try to generate the cut.
  //
  // We deal with the corner case with duplicate arcs, or just one side of a
  // "symmetric" edge present.
  int num_actual_inverted = 0;
  absl::flat_hash_set<std::pair<int, int>> processed_arcs;
  LinearConstraintBuilder builder(encoder_, IntegerValue(1), kMaxIntegerValue);

  // Add extra self-loop at zero enforcement if needed.
  // TODO(user): Can we deal with the 2-cycle case in a better way?
  if (best_optional_index != -1) {
    absl::StrAppend(&name, "_opt");

    // This is tricky: The normal cut assume x_e <= 1, but in case of a single
    // 2 cycle, x_e can be equal to 2. So we need a coeff of 2 to disable that
    // cut.
    CHECK(builder.AddLiteralTerm(self_arc_literal_[best_optional_index],
                                 IntegerValue(2)));
  }

  for (int i = 0; i < tails_.size(); ++i) {
    if (tails_[i] == heads_[i]) continue;
    if (in_subset_[tails_[i]] == in_subset_[heads_[i]]) continue;

    const std::pair<int, int> key = {tails_[i], heads_[i]};
    const std::pair<int, int> r_key = {heads_[i], tails_[i]};
    const std::pair<int, int> s_key = std::min(key, r_key);
    if (special_edges.contains(s_key) && !processed_arcs.contains(key)) {
      processed_arcs.insert(key);
      CHECK(builder.AddLiteralTerm(literals_[i], IntegerValue(-1)));
      if (!processed_arcs.contains(r_key)) {
        ++num_actual_inverted;
      }
      continue;
    }

    // Normal edge.
    CHECK(builder.AddLiteralTerm(literals_[i], IntegerValue(1)));
  }
  builder.AddConstant(IntegerValue(num_actual_inverted));
  if (num_actual_inverted % 2 == 0) return false;

  return manager->AddCut(builder.Build(), name);
}

}  // namespace

void GenerateInterestingSubsets(int num_nodes,
                                absl::Span<const std::pair<int, int>> arcs,
                                int stop_at_num_components,
                                std::vector<int>* subset_data,
                                std::vector<absl::Span<const int>>* subsets) {
  // We will do a union-find by adding one by one the arc of the lp solution
  // in the order above. Every intermediate set during this construction will
  // be a candidate for a cut.
  //
  // In parallel to the union-find, to efficiently reconstruct these sets (at
  // most num_nodes), we construct a "decomposition forest" of the different
  // connected components. Note that we don't exploit any asymmetric nature of
  // the graph here. This is exactly the algo 6.3 in the book above.
  int num_components = num_nodes;
  std::vector<int> parent(num_nodes);
  std::vector<int> root(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    parent[i] = i;
    root[i] = i;
  }
  auto get_root_and_compress_path = [&root](int node) {
    int r = node;
    while (root[r] != r) r = root[r];
    while (root[node] != r) {
      const int next = root[node];
      root[node] = r;
      node = next;
    }
    return r;
  };
  for (const auto& [initial_tail, initial_head] : arcs) {
    if (num_components <= stop_at_num_components) break;
    const int tail = get_root_and_compress_path(initial_tail);
    const int head = get_root_and_compress_path(initial_head);
    if (tail != head) {
      // Update the decomposition forest, note that the number of nodes is
      // growing.
      const int new_node = parent.size();
      parent.push_back(new_node);
      parent[head] = new_node;
      parent[tail] = new_node;
      --num_components;

      // It is important that the union-find representative is the same node.
      root.push_back(new_node);
      root[head] = new_node;
      root[tail] = new_node;
    }
  }

  // For each node in the decomposition forest, try to add a cut for the set
  // formed by the nodes and its children. To do that efficiently, we first
  // order the nodes so that for each node in a tree, the set of children forms
  // a consecutive span in the subset_data vector. This vector just lists the
  // nodes in the "pre-order" graph traversal order. The Spans will point inside
  // the subset_data vector, it is why we initialize it once and for all.
  ExtractAllSubsetsFromForest(parent, subset_data, subsets,
                              /*node_limit=*/num_nodes);
}

void ExtractAllSubsetsFromForest(absl::Span<const int> parent,
                                 std::vector<int>* subset_data,
                                 std::vector<absl::Span<const int>>* subsets,
                                 int node_limit) {
  // To not reallocate memory since we need the span to point inside this
  // vector, we resize subset_data right away.
  int out_index = 0;
  const int num_nodes = parent.size();
  subset_data->resize(std::min(num_nodes, node_limit));
  subsets->clear();

  // Starts by creating the corresponding graph and find the root.
  util::StaticGraph<int> graph(num_nodes, num_nodes - 1);
  for (int i = 0; i < num_nodes; ++i) {
    if (parent[i] != i) {
      graph.AddArc(parent[i], i);
    }
  }
  graph.Build();

  // Perform a dfs on the rooted tree.
  // The subset_data will just be the node in post-order.
  std::vector<int> subtree_starts(num_nodes, -1);
  std::vector<int> stack;
  stack.reserve(num_nodes);
  for (int i = 0; i < parent.size(); ++i) {
    if (parent[i] != i) continue;

    stack.push_back(i);  // root.
    while (!stack.empty()) {
      const int node = stack.back();

      // The node was already explored, output its subtree and pop it.
      if (subtree_starts[node] >= 0) {
        stack.pop_back();
        if (node < node_limit) {
          (*subset_data)[out_index++] = node;
        }
        const int start = subtree_starts[node];
        const int size = out_index - start;
        subsets->push_back(absl::MakeSpan(&(*subset_data)[start], size));
        continue;
      }

      // Explore.
      subtree_starts[node] = out_index;
      for (const int child : graph[node]) {
        stack.push_back(child);
      }
    }
  }
}

std::vector<int> ComputeGomoryHuTree(
    int num_nodes, absl::Span<const ArcWithLpValue> relevant_arcs) {
  // Initialize the graph. Note that we use only arcs with a relevant lp
  // value, so this should be small in practice.
  SimpleMaxFlow max_flow;
  for (const auto& [tail, head, lp_value] : relevant_arcs) {
    max_flow.AddArcWithCapacity(tail, head, std::round(1.0e6 * lp_value));
    max_flow.AddArcWithCapacity(head, tail, std::round(1.0e6 * lp_value));
  }

  // Compute an equivalent max-flow tree, according to the paper.
  // This version should actually produce a Gomory-Hu cut tree.
  std::vector<int> min_cut_subset;
  std::vector<int> parent(num_nodes, 0);
  for (int s = 1; s < num_nodes; ++s) {
    const int t = parent[s];
    if (max_flow.Solve(s, t) != SimpleMaxFlow::OPTIMAL) break;
    max_flow.GetSourceSideMinCut(&min_cut_subset);
    bool parent_of_t_in_subset = false;
    for (const int i : min_cut_subset) {
      if (i == parent[t]) parent_of_t_in_subset = true;
      if (i != s && parent[i] == t) parent[i] = s;
    }
    if (parent_of_t_in_subset) {
      parent[s] = parent[t];
      parent[t] = s;
    }
  }

  return parent;
}

void SymmetrizeArcs(std::vector<ArcWithLpValue>* arcs) {
  for (ArcWithLpValue& arc : *arcs) {
    if (arc.tail <= arc.head) continue;
    std::swap(arc.tail, arc.head);
  }
  std::sort(arcs->begin(), arcs->end(),
            [](const ArcWithLpValue& a, const ArcWithLpValue& b) {
              return std::tie(a.tail, a.head) < std::tie(b.tail, b.head);
            });

  int new_size = 0;
  int last_tail = -1;
  int last_head = -1;
  for (const ArcWithLpValue& arc : *arcs) {
    if (arc.tail == last_tail && arc.head == last_head) {
      (*arcs)[new_size - 1].lp_value += arc.lp_value;
      continue;
    }
    (*arcs)[new_size++] = arc;
    last_tail = arc.tail;
    last_head = arc.head;
  }
  arcs->resize(new_size);
}

// We roughly follow the algorithm described in section 6 of "The Traveling
// Salesman Problem, A computational Study", David L. Applegate, Robert E.
// Bixby, Vasek Chvatal, William J. Cook.
//
// Note that this is mainly a "symmetric" case algo, but it does still work for
// the asymmetric case.
void SeparateSubtourInequalities(OutgoingCutHelper& helper,
                                 LinearConstraintManager* manager) {
  const int num_nodes = helper.num_nodes();
  if (num_nodes <= 2) return;

  helper.InitializeForNewLpSolution(manager);

  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, helper.ordered_arcs(),
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);

  const int depot = 0;
  if (helper.is_route_constraint()) {
    // Add the depot so that we have a trivial bound on the number of
    // vehicle.
    subsets.push_back(absl::MakeSpan(&depot, 1));
  }

  // Hack/optim: we exploit the tree structure of the subsets to not add a cut
  // for a larger subset if we added a cut from one included in it.
  //
  // TODO(user): Currently if we add too many not so relevant cuts, our generic
  // MIP cut heuritic are way too slow on TSP/VRP problems.
  int last_added_start = -1;

  // Process each subsets and add any violated cut.
  int num_added = 0;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    if (start <= last_added_start) continue;
    if (helper.TrySubsetCut("Circuit", subset, manager)) {
      ++num_added;
      last_added_start = start;
    }
  }

  // If there were no cut added by the heuristic above, we try exact separation.
  //
  // With n-1 max_flow from a source to all destination, we can get the global
  // min-cut. Here, we use a slightly more advanced algorithm that will find a
  // min-cut for all possible pair of nodes. This is achieved by computing a
  // Gomory-Hu tree, still with n-1 max flow call.
  //
  // Note(user): Compared to any min-cut, these cut have some nice properties
  // since they are "included" in each other. This might help with combining
  // them within our generic IP cuts framework.
  //
  // TODO(user): I had an older version that tried the n-cuts generated during
  // the course of the algorithm. This could also be interesting. But it is
  // hard to tell with our current benchmark setup.
  if (num_added != 0) return;
  absl::Span<const ArcWithLpValue> symmetrized_relevant_arcs =
      helper.SymmetrizedRelevantArcs();
  std::vector<int> parent =
      ComputeGomoryHuTree(num_nodes, symmetrized_relevant_arcs);

  // Try all interesting subset from the Gomory-Hu tree.
  ExtractAllSubsetsFromForest(parent, &subset_data, &subsets);
  last_added_start = -1;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    if (subset.size() == num_nodes) continue;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    if (start <= last_added_start) continue;
    if (helper.TrySubsetCut("CircuitExact", subset, manager)) {
      ++num_added;
      last_added_start = start;
    }
  }

  // Exact separation of symmetric Blossom cut. We use the algorithm in the
  // paper: "A Faster Exact Separation Algorithm for Blossom Inequalities", Adam
  // N. Letchford, Gerhard Reinelt, Dirk Oliver Theis, 2004.
  if (num_added != 0) return;
  if (num_nodes <= 2) return;
  std::vector<ArcWithLpValue> for_blossom;
  for_blossom.reserve(symmetrized_relevant_arcs.size());
  for (ArcWithLpValue arc : symmetrized_relevant_arcs) {
    if (arc.lp_value > 0.5) arc.lp_value = 1.0 - arc.lp_value;
    if (arc.lp_value < 1e-6) continue;
    for_blossom.push_back(arc);
  }
  parent = ComputeGomoryHuTree(num_nodes, for_blossom);
  ExtractAllSubsetsFromForest(parent, &subset_data, &subsets);
  last_added_start = -1;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    if (subset.size() == num_nodes) continue;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    if (start <= last_added_start) continue;
    if (helper.TryBlossomSubsetCut("CircuitBlossom", symmetrized_relevant_arcs,
                                   subset, manager)) {
      ++num_added;
      last_added_start = start;
    }
  }
}

namespace {

// Returns for each literal its integer view, or the view of its negation.
std::vector<IntegerVariable> GetAssociatedVariables(
    absl::Span<const Literal> literals, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  std::vector<IntegerVariable> result;
  for (const Literal l : literals) {
    const IntegerVariable direct_view = encoder->GetLiteralView(l);
    if (direct_view != kNoIntegerVariable) {
      result.push_back(direct_view);
    } else {
      result.push_back(encoder->GetLiteralView(l.Negated()));
      DCHECK_NE(result.back(), kNoIntegerVariable);
    }
  }
  return result;
}

}  // namespace

// We use a basic algorithm to detect components that are not connected to the
// rest of the graph in the LP solution, and add cuts to force some arcs to
// enter and leave this component from outside.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals, Model* model) {
  auto helper = std::make_unique<OutgoingCutHelper>(
      num_nodes, /*is_route_constraint=*/false, /*capacity=*/0,
      /*demands=*/absl::Span<const int64_t>(),
      /*flat_node_dim_variables=*/
      absl::Span<const IntegerVariable>{}, tails, heads, literals, model);
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [helper = std::move(helper)](LinearConstraintManager* manager) {
        SeparateSubtourInequalities(*helper, manager);
        return true;
      };
  return result;
}

CutGenerator CreateCVRPCutGenerator(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals, absl::Span<const int64_t> demands,
    absl::Span<const IntegerVariable> flat_node_dim_variables, int64_t capacity,
    Model* model) {
  auto helper = std::make_unique<OutgoingCutHelper>(
      num_nodes, /*is_route_constraint=*/true, capacity, demands,
      flat_node_dim_variables, tails, heads, literals, model);
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [helper = std::move(helper)](LinearConstraintManager* manager) {
        SeparateSubtourInequalities(*helper, manager);
        return true;
      };
  return result;
}

// This is really similar to SeparateSubtourInequalities, see the reference
// there.
void SeparateFlowInequalities(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const AffineExpression> arc_capacities,
    std::function<void(const std::vector<bool>& in_subset,
                       IntegerValue* min_incoming_flow,
                       IntegerValue* min_outgoing_flow)>
        get_flows,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    LinearConstraintManager* manager, Model* model) {
  // We will collect only the arcs with a positive lp capacity value to speed up
  // some computation below.
  struct Arc {
    int tail;
    int head;
    double lp_value;
    IntegerValue offset;
  };
  std::vector<Arc> relevant_arcs;

  // Often capacities have a coeff > 1.
  // We currently exploit this if all coeff have a gcd > 1.
  int64_t gcd = 0;

  // Sort the arcs by non-increasing lp_values.
  std::vector<std::pair<double, int>> arc_by_decreasing_lp_values;
  for (int i = 0; i < arc_capacities.size(); ++i) {
    const double lp_value = arc_capacities[i].LpValue(lp_values);
    if (!arc_capacities[i].IsConstant()) {
      gcd = std::gcd(gcd, std::abs(arc_capacities[i].coeff.value()));
    }
    if (lp_value < 1e-6 && arc_capacities[i].constant == 0) continue;
    relevant_arcs.push_back(
        {tails[i], heads[i], lp_value, arc_capacities[i].constant});
    arc_by_decreasing_lp_values.push_back({lp_value, i});
  }
  if (gcd == 0) return;
  std::sort(arc_by_decreasing_lp_values.begin(),
            arc_by_decreasing_lp_values.end(),
            std::greater<std::pair<double, int>>());

  std::vector<std::pair<int, int>> ordered_arcs;
  for (const auto& [score, arc] : arc_by_decreasing_lp_values) {
    if (tails[arc] == -1) continue;
    if (heads[arc] == -1) continue;
    ordered_arcs.push_back({tails[arc], heads[arc]});
  }
  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, ordered_arcs,
                             /*stop_at_num_components=*/1, &subset_data,
                             &subsets);

  // Process each subsets and add any violated cut.
  std::vector<bool> in_subset(num_nodes, false);
  for (const absl::Span<const int> subset : subsets) {
    DCHECK(!subset.empty());
    DCHECK_LE(subset.size(), num_nodes);

    // Initialize "in_subset" and the subset demands.
    for (const int n : subset) in_subset[n] = true;

    IntegerValue min_incoming_flow;
    IntegerValue min_outgoing_flow;
    get_flows(in_subset, &min_incoming_flow, &min_outgoing_flow);

    // We will sum the offset of all incoming/outgoing arc capacities.
    // Note that all arcs with a non-zero offset are part of relevant_arcs.
    IntegerValue incoming_offset(0);
    IntegerValue outgoing_offset(0);

    // Compute the current flow in and out of the subset.
    //
    // This can take a significant portion of the running time, it is why it is
    // faster to do it only on arcs with non-zero lp values which should be in
    // linear number rather than the total number of arc which can be quadratic.
    double lp_outgoing_flow = 0.0;
    double lp_incoming_flow = 0.0;
    for (const auto arc : relevant_arcs) {
      if (arc.tail != -1 && in_subset[arc.tail]) {
        if (arc.head == -1 || !in_subset[arc.head]) {
          incoming_offset += arc.offset;
          lp_outgoing_flow += arc.lp_value;
        }
      } else {
        if (arc.head != -1 && in_subset[arc.head]) {
          outgoing_offset += arc.offset;
          lp_incoming_flow += arc.lp_value;
        }
      }
    }

    // If the gcd is greater than one, because all variables are integer we
    // can round the flow lower bound to the next multiple of the gcd.
    //
    // TODO(user): Alternatively, try MIR heuristics if the coefficients in
    // the capacities are not all the same.
    if (gcd > 1) {
      const IntegerValue test_incoming = min_incoming_flow - incoming_offset;
      const IntegerValue new_incoming =
          CeilRatio(test_incoming, IntegerValue(gcd)) * IntegerValue(gcd);
      const IntegerValue incoming_delta = new_incoming - test_incoming;
      if (incoming_delta > 0) min_incoming_flow += incoming_delta;
    }
    if (gcd > 1) {
      const IntegerValue test_outgoing = min_outgoing_flow - outgoing_offset;
      const IntegerValue new_outgoing =
          CeilRatio(test_outgoing, IntegerValue(gcd)) * IntegerValue(gcd);
      const IntegerValue outgoing_delta = new_outgoing - test_outgoing;
      if (outgoing_delta > 0) min_outgoing_flow += outgoing_delta;
    }

    if (lp_incoming_flow < ToDouble(min_incoming_flow) - 1e-6) {
      VLOG(2) << "INCOMING CUT " << lp_incoming_flow
              << " >= " << min_incoming_flow << " size " << subset.size()
              << " offset " << incoming_offset << " gcd " << gcd;
      LinearConstraintBuilder cut(model, min_incoming_flow, kMaxIntegerValue);
      for (int i = 0; i < tails.size(); ++i) {
        if ((tails[i] == -1 || !in_subset[tails[i]]) &&
            (heads[i] != -1 && in_subset[heads[i]])) {
          cut.AddTerm(arc_capacities[i], 1.0);
        }
      }
      manager->AddCut(cut.Build(), "IncomingFlow");
    }

    if (lp_outgoing_flow < ToDouble(min_outgoing_flow) - 1e-6) {
      VLOG(2) << "OUGOING CUT " << lp_outgoing_flow
              << " >= " << min_outgoing_flow << " size " << subset.size()
              << " offset " << outgoing_offset << " gcd " << gcd;
      LinearConstraintBuilder cut(model, min_outgoing_flow, kMaxIntegerValue);
      for (int i = 0; i < tails.size(); ++i) {
        if ((tails[i] != -1 && in_subset[tails[i]]) &&
            (heads[i] == -1 || !in_subset[heads[i]])) {
          cut.AddTerm(arc_capacities[i], 1.0);
        }
      }
      manager->AddCut(cut.Build(), "OutgoingFlow");
    }

    // Sparse clean up.
    for (const int n : subset) in_subset[n] = false;
  }
}

CutGenerator CreateFlowCutGenerator(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<AffineExpression>& arc_capacities,
    std::function<void(const std::vector<bool>& in_subset,
                       IntegerValue* min_incoming_flow,
                       IntegerValue* min_outgoing_flow)>
        get_flows,
    Model* model) {
  CutGenerator result;
  for (const AffineExpression expr : arc_capacities) {
    if (!expr.IsConstant()) result.vars.push_back(expr.var);
  }
  result.generate_cuts = [=](LinearConstraintManager* manager) {
    SeparateFlowInequalities(num_nodes, tails, heads, arc_capacities, get_flows,
                             manager->LpValues(), manager, model);
    return true;
  };
  return result;
}

}  // namespace sat
}  // namespace operations_research
