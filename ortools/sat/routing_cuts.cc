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
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <queue>
#include <stack>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/numeric/bits.h"
#include "absl/numeric/int128.h"
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
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/routes_support_graph.pb.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

ABSL_FLAG(bool, cp_model_dump_routes_support_graphs, false,
          "DEBUG ONLY. When set to true, SolveCpModel() dumps the arcs with "
          "non-zero LP values of the routes constraints, at decision level 0, "
          "which are used to subsequently generate cuts. The values are "
          "written as a SupportGraphProto in text format to "
          "'FLAGS_cp_model_dump_prefix'support_graph_{counter}.pb.txt.");

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
      implied_bounds_(*model->GetOrCreate<ImpliedBounds>()),
      trail_(*model->GetOrCreate<Trail>()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      integer_encoder_(*model->GetOrCreate<IntegerEncoder>()),
      root_level_bounds_(*model->GetOrCreate<RootLevelLinear2Bounds>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      in_subset_(num_nodes, false),
      index_in_subset_(num_nodes, -1),
      incoming_arc_indices_(num_nodes),
      outgoing_arc_indices_(num_nodes),
      reachable_(num_nodes, false),
      next_reachable_(num_nodes, false),
      node_var_lower_bounds_(num_nodes),
      next_node_var_lower_bounds_(num_nodes),
      bin_packing_helper_(
          model->GetOrCreate<SatParameters>()->routing_cut_dp_effort()) {}

MinOutgoingFlowHelper::~MinOutgoingFlowHelper() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"RoutingDp/num_full_dp_calls", num_full_dp_calls_});
  stats.push_back({"RoutingDp/num_full_dp_skips", num_full_dp_skips_});
  stats.push_back(
      {"RoutingDp/num_full_dp_early_abort", num_full_dp_early_abort_});
  stats.push_back(
      {"RoutingDp/num_full_dp_work_abort", num_full_dp_work_abort_});
  stats.push_back({"RoutingDp/num_full_dp_rc_skip", num_full_dp_rc_skip_});
  stats.push_back(
      {"RoutingDp/num_full_dp_unique_arc", num_full_dp_unique_arc_});
  for (const auto& [name, count] : num_by_type_) {
    stats.push_back({absl::StrCat("RoutingDp/num_bounds_", name), count});
  }
  shared_stats_->AddStats(stats);
}

// TODO(user): This is costly and we might do it more than once per subset.
// fix.
void MinOutgoingFlowHelper::PrecomputeDataForSubset(
    absl::Span<const int> subset) {
  cannot_be_first_.clear();
  cannot_be_last_.clear();

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
  incoming_arcs_from_outside_.assign(num_nodes, {});
  outgoing_arcs_to_outside_.assign(num_nodes, {});

  for (auto& v : incoming_arc_indices_) v.clear();
  for (auto& v : outgoing_arc_indices_) v.clear();
  for (int i = 0; i < tails_.size(); ++i) {
    const int tail = tails_[i];
    const int head = heads_[i];

    // we always ignore self-arcs here.
    if (tail == head) continue;
    const bool tail_in = in_subset_[tail];
    const bool head_in = in_subset_[head];

    if (tail_in && head_in) {
      outgoing_arc_indices_[tail].push_back(i);
      incoming_arc_indices_[head].push_back(i);
    } else if (tail_in && !head_in) {
      outgoing_arcs_to_outside_[tail].Add(i);
      has_outgoing_arcs_to_outside_[tail] = true;
    } else if (!tail_in && head_in) {
      incoming_arcs_from_outside_[head].Add(i);
      has_incoming_arcs_from_outside_[head] = true;
    }
  }
}

int MinOutgoingFlowHelper::ComputeDimensionBasedMinOutgoingFlow(
    absl::Span<const int> subset, const RouteRelationsHelper& helper,
    BestBoundHelper* best_bound) {
  PrecomputeDataForSubset(subset);
  DCHECK_EQ(helper.num_nodes(), in_subset_.size());
  DCHECK_EQ(helper.num_arcs(), tails_.size());

  int min_outgoing_flow = 1;
  std::string best_name;
  for (int d = 0; d < helper.num_dimensions(); ++d) {
    for (const bool negate_expressions : {false, true}) {
      for (const bool use_incoming : {true, false}) {
        std::string info;
        const absl::Span<SpecialBinPackingHelper::ItemOrBin> objects =
            RelaxIntoSpecialBinPackingProblem(subset, d, negate_expressions,
                                              use_incoming, helper);
        const int bound = bin_packing_helper_.ComputeMinNumberOfBins(
            objects, objects_that_cannot_be_bin_and_reach_minimum_, info);
        if (bound >= min_outgoing_flow) {
          min_outgoing_flow = bound;
          best_name = absl::StrCat((use_incoming ? "in" : "out"), info,
                                   (negate_expressions ? "_neg" : ""), "_", d);
          cannot_be_first_.clear();
          cannot_be_last_.clear();
          auto& vec = use_incoming ? cannot_be_first_ : cannot_be_last_;
          for (const int i : objects_that_cannot_be_bin_and_reach_minimum_) {
            vec.push_back(objects[i].node);
          }
          best_bound->Update(bound, "AutomaticDimension", cannot_be_first_,
                             cannot_be_last_);
        }
      }
    }
  }

  if (min_outgoing_flow > 1) num_by_type_[best_name]++;
  return min_outgoing_flow;
}

absl::Span<SpecialBinPackingHelper::ItemOrBin>
MinOutgoingFlowHelper::RelaxIntoSpecialBinPackingProblem(
    absl::Span<const int> subset, int dimension, bool negate_expressions,
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

  // Lower bound on the node expression when it is first to serve the subset.
  const auto lb_when_first = [&helper, this](int n, int dimension,
                                             bool negate_expressions) {
    AffineExpression expr = helper.GetNodeExpression(n, dimension);
    if (negate_expressions) expr = expr.Negated();
    IntegerValue lb = integer_trail_.LevelZeroLowerBound(expr);
    if (incoming_arcs_from_outside_[n].IsUnique()) {
      const int a = incoming_arcs_from_outside_[n].Get();
      AffineExpression tail_expr =
          helper.GetNodeExpression(tails_[a], dimension);
      if (negate_expressions) tail_expr = tail_expr.Negated();
      const IntegerValue offset =
          helper.GetArcOffsetLowerBound(a, dimension, negate_expressions);
      lb = std::max(lb, integer_trail_.LevelZeroLowerBound(tail_expr) + offset);
    }
    return lb;
  };

  // Upper bound on the node expression when it is last to serve the subset.
  const auto ub_when_last = [&helper, this](int n, int dimension,
                                            bool negate_expressions) {
    AffineExpression expr = helper.GetNodeExpression(n, dimension);
    if (negate_expressions) expr = expr.Negated();
    IntegerValue ub = integer_trail_.LevelZeroUpperBound(expr);
    if (outgoing_arcs_to_outside_[n].IsUnique()) {
      const int a = outgoing_arcs_to_outside_[n].Get();
      AffineExpression head_expr =
          helper.GetNodeExpression(heads_[a], dimension);
      if (negate_expressions) head_expr = head_expr.Negated();
      const IntegerValue offset =
          helper.GetArcOffsetLowerBound(a, dimension, negate_expressions);
      ub = std::min(ub, integer_trail_.LevelZeroUpperBound(head_expr) - offset);
    }
    return ub;
  };

  // We do a forward and a backward pass in order to compute the min/max
  // of all the other nodes for each node.
  for (const bool forward_pass : {true, false}) {
    IntegerValue local_min = kMaxIntegerValue;
    IntegerValue local_max = kMinIntegerValue;
    const int size = subset.size();
    for (int i = 0; i < size; ++i) {
      const int n = forward_pass ? subset[i] : subset[size - 1 - i];

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
        local_min = std::min(local_min,
                             lb_when_first(n, dimension, negate_expressions));
      }
      if (has_outgoing_arcs_to_outside_[n]) {
        local_max =
            std::max(local_max, ub_when_last(n, dimension, negate_expressions));
      }
    }
  }

  for (const int n : subset) {
    const absl::Span<const int> arcs =
        use_incoming ? incoming_arc_indices_[n] : outgoing_arc_indices_[n];
    IntegerValue demand = kMaxIntegerValue;
    for (const int a : arcs) {
      demand = std::min(demand, helper.GetArcOffsetLowerBound(
                                    a, dimension, negate_expressions));
    }

    SpecialBinPackingHelper::ItemOrBin obj;
    obj.node = n;
    obj.demand = demand;

    // We compute the capacity like this to avoid overflow and always have
    // a non-negative capacity (see above).
    obj.capacity = 0;
    if (use_incoming) {
      const IntegerValue lb = lb_when_first(n, dimension, negate_expressions);
      if (max_upper_bound_of_others[n] > lb) {
        obj.capacity = max_upper_bound_of_others[n] - lb;
      }
    } else {
      const IntegerValue ub = ub_when_last(n, dimension, negate_expressions);
      if (ub > min_lower_bound_of_others[n]) {
        obj.capacity = ub - min_lower_bound_of_others[n];
      }
    }

    // Note that we don't explicitly deal with the corner case of a subset node
    // with no arcs. This corresponds to INFEASIBLE problem and should be dealt
    // with elsewhere. Being "less restrictive" will still result in a valid
    // bound and that is enough here.
    if ((use_incoming && !has_incoming_arcs_from_outside_[n]) ||
        (!use_incoming && !has_outgoing_arcs_to_outside_[n])) {
      obj.type = SpecialBinPackingHelper::MUST_BE_ITEM;
      obj.capacity = 0;
    } else if (arcs.empty()) {
      obj.type = SpecialBinPackingHelper::MUST_BE_BIN;
      obj.demand = 0;  // We don't want kMaxIntegerValue !
    } else {
      obj.type = SpecialBinPackingHelper::ITEM_OR_BIN;
    }

    tmp_bin_packing_problem_.push_back(obj);
  }

  return absl::MakeSpan(tmp_bin_packing_problem_);
}

int SpecialBinPackingHelper::ComputeMinNumberOfBins(
    absl::Span<ItemOrBin> objects,
    std::vector<int>& objects_that_cannot_be_bin_and_reach_minimum,
    std::string& info) {
  objects_that_cannot_be_bin_and_reach_minimum.clear();
  if (objects.empty()) return 0;

  IntegerValue sum_of_demands(0);
  int64_t gcd = 0;
  int64_t max_capacity = 0;
  int max_num_items = 0;
  bool all_demands_are_non_negative = true;
  for (const ItemOrBin& obj : objects) {
    sum_of_demands = CapAddI(sum_of_demands, obj.demand);
    if (obj.type != MUST_BE_BIN) {
      ++max_num_items;
      if (obj.demand < 0) {
        all_demands_are_non_negative = false;
      }
      gcd = std::gcd(gcd, std::abs(obj.demand.value()));
    }
    if (obj.type != MUST_BE_ITEM) {
      max_capacity = std::max(max_capacity, obj.capacity.value());
    }
  }

  // TODO(user): we can probably handle a couple of extra case rather than just
  // bailing out here and below.
  if (AtMinOrMaxInt64I(sum_of_demands)) return 0;

  // If the gcd of all the demands term is positive, we can divide everything.
  if (gcd > 1) {
    absl::StrAppend(&info, "_gcd");
    for (ItemOrBin& obj : objects) {
      obj.demand /= gcd;
      obj.capacity = FloorRatio(obj.capacity, gcd);
    }
    max_capacity = MathUtil::FloorOfRatio(max_capacity, gcd);
  }

  const int result = ComputeMinNumberOfBinsInternal(
      objects, objects_that_cannot_be_bin_and_reach_minimum);

  // We only try DP if it can help.
  //
  // Tricky: For the GreedyPackingWorks() to make sense, we use the fact that
  // ComputeMinNumberOfBinsInternal() moves the best bins first. In any case
  // the code will still be correct if it is not the case as we just use this
  // as an heuristic to do less work.
  if (result > 1 && all_demands_are_non_negative &&
      max_bounded_subset_sum_exact_.ComplexityEstimate(
          max_num_items, max_capacity) < dp_effort_ &&
      !GreedyPackingWorks(result, objects) &&
      UseDpToTightenCapacities(objects)) {
    const int new_result = ComputeMinNumberOfBinsInternal(
        objects, objects_that_cannot_be_bin_and_reach_minimum);
    CHECK_GE(new_result, result);
    if (new_result > result) {
      absl::StrAppend(&info, "_dp");
    }
    return new_result;
  }

  return result;
}

// TODO(user): We could be more tight here and also compute the max_reachable
// under smaller capacity than the max_capacity.
bool SpecialBinPackingHelper::UseDpToTightenCapacities(
    absl::Span<ItemOrBin> objects) {
  tmp_demands_.clear();
  int64_t max_capacity = 0;
  for (const ItemOrBin& obj : objects) {
    if (obj.type != MUST_BE_BIN) {
      tmp_demands_.push_back(obj.demand.value());
    }
    if (obj.type != MUST_BE_ITEM) {
      max_capacity = std::max(max_capacity, obj.capacity.value());
    }
  }
  const int64_t max_reachable =
      max_bounded_subset_sum_exact_.MaxSubsetSum(tmp_demands_, max_capacity);
  bool some_change = false;
  if (max_reachable < max_capacity) {
    for (ItemOrBin& obj : objects) {
      if (obj.capacity > max_reachable) {
        some_change = true;
        obj.capacity = max_reachable;
      }
    }
  }
  return some_change;
}

int SpecialBinPackingHelper::ComputeMinNumberOfBinsInternal(
    absl::Span<ItemOrBin> objects,
    std::vector<int>& objects_that_cannot_be_bin_and_reach_minimum) {
  objects_that_cannot_be_bin_and_reach_minimum.clear();

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

  // Note that we already checked for overflow.
  IntegerValue sum_of_demands(0);
  for (const ItemOrBin& obj : objects) {
    sum_of_demands += obj.demand;
  }

  // We start with no bins (sum_of_demands=everything, sum_of_capacity=0) and
  // add the best bins one by one until we have sum_of_demands <=
  // sum_of_capacity.
  int num_bins = 0;
  IntegerValue sum_of_capacity(0);
  for (; num_bins < objects.size(); ++num_bins) {
    // Use obj as a bin instead of as an item, if possible (i.e., unless
    // obj.type is MUST_BE_ITEM).
    const ItemOrBin& obj = objects[num_bins];
    if (obj.type != MUST_BE_BIN && sum_of_demands <= sum_of_capacity) {
      break;
    }
    if (obj.type == MUST_BE_ITEM) {
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

  // We can identify items that cannot be a bin in a solution reaching the
  // minimum 'num_bins'.
  //
  // TODO(user): Somewhat related are the items that can be removed while still
  // having the same required 'num_bins' to solve the problem.
  if (num_bins > 0 && num_bins != objects.size()) {
    const int worst_used_bin = num_bins - 1;
    if (objects[worst_used_bin].type == MUST_BE_BIN) {
      // All first object must be bins, we don't have a choice if we want to
      // reach the lower bound. The others cannot be bins.
      for (int i = num_bins; i < objects.size(); ++i) {
        objects_that_cannot_be_bin_and_reach_minimum.push_back(i);
      }
    } else {
      // We revert the worst_used_bin and tries to use the other instead.
      CHECK_EQ(objects[worst_used_bin].type, ITEM_OR_BIN);
      sum_of_demands += objects[worst_used_bin].demand;
      sum_of_capacity -= objects[worst_used_bin].capacity;
      const IntegerValue slack = sum_of_demands - sum_of_capacity;
      CHECK_GE(slack, 0) << num_bins << " " << objects.size() << " "
                         << objects[worst_used_bin].type;

      for (int i = num_bins; i < objects.size(); ++i) {
        if (objects[i].type == MUST_BE_ITEM) continue;

        // If we use this as a bin instead of "worst_used_bin" can we still
        // fulfill the demands?
        const IntegerValue new_demand = sum_of_demands - objects[i].demand;
        const IntegerValue new_capacity = sum_of_capacity + objects[i].capacity;
        if (new_demand > new_capacity) {
          objects_that_cannot_be_bin_and_reach_minimum.push_back(i);
        }
      }
    }
  }

  return num_bins;
}

bool SpecialBinPackingHelper::GreedyPackingWorks(
    int num_bins, absl::Span<const ItemOrBin> objects) {
  if (num_bins >= objects.size()) return true;
  CHECK_GE(num_bins, 1);
  tmp_capacities_.resize(num_bins);
  for (int i = 0; i < num_bins; ++i) {
    tmp_capacities_[i] = objects[i].capacity;
  }

  // We place the objects in order in the first bin that fits.
  for (const ItemOrBin object : objects.subspan(num_bins)) {
    bool placed = false;
    for (int b = 0; b < num_bins; ++b) {
      if (object.demand <= tmp_capacities_[b]) {
        placed = true;
        tmp_capacities_[b] -= object.demand;
        break;
      }
    }
    if (!placed) return false;
  }
  return true;
}

int MinOutgoingFlowHelper::ComputeMinOutgoingFlow(
    absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK(absl::c_all_of(next_reachable_, [](bool b) { return !b; }));
  DCHECK(absl::c_all_of(node_var_lower_bounds_,
                        [](const auto& m) { return m.empty(); }));
  DCHECK(absl::c_all_of(next_node_var_lower_bounds_,
                        [](const auto& m) { return m.empty(); }));
  PrecomputeDataForSubset(subset);

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
        if (!PropagateLocalBounds(
                integer_trail_, root_level_bounds_, binary_relation_repository_,
                implied_bounds_, lit, node_var_lower_bounds_[tail], &tmp_lbs)) {
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

int MinOutgoingFlowHelper::ComputeTightMinOutgoingFlow(
    absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LE(subset.size(), 32);
  PrecomputeDataForSubset(subset);

  std::vector<int> longest_path_length_by_end_node(subset.size(), 1);

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
        if (!PropagateLocalBounds(integer_trail_, root_level_bounds_,
                                  binary_relation_repository_, implied_bounds_,
                                  literals_[outgoing_arc_index], path_bounds,
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
    int k, absl::Span<const int> subset, RouteRelationsHelper* helper,
    LinearConstraintManager* manager, int special_node,
    bool use_forward_direction) {
  cannot_be_first_.clear();
  cannot_be_last_.clear();
  if (k >= subset.size()) return true;
  if (subset.size() > 31) return true;

  // If we have a "special node" from which we must start, make sure it is
  // always first. This simplifies the code a bit, and we don't care about the
  // order of nodes in subset.
  if (special_node >= 0) {
    tmp_subset_.assign(subset.begin(), subset.end());
    bool seen = false;
    for (int i = 0; i < tmp_subset_.size(); ++i) {
      if (tmp_subset_[i] == special_node) {
        seen = true;
        std::swap(tmp_subset_[0], tmp_subset_[i]);
        break;
      }
    }
    CHECK(seen);

    // Make the span point to the new order.
    subset = tmp_subset_;
  }

  PrecomputeDataForSubset(subset);
  ++num_full_dp_calls_;

  // We need a special graph here to handle both options.
  // TODO(user): Maybe we should abstract that in a better way.
  CompactVectorVector<int, std::pair<int, Literal>> adjacency;
  adjacency.reserve(subset.size());
  for (int i = 0; i < subset.size(); ++i) {
    adjacency.Add({});
    const int node = subset[i];
    if (helper != nullptr) {
      CHECK(use_forward_direction);
      for (int j = 0; j < subset.size(); ++j) {
        if (i == j) continue;
        if (helper->PathExists(node, subset[j])) {
          adjacency.AppendToLastVector({subset[j], Literal(kNoLiteralIndex)});
        }
      }
    } else {
      if (use_forward_direction) {
        for (const int arc : outgoing_arc_indices_[node]) {
          adjacency.AppendToLastVector({heads_[arc], literals_[arc]});
        }
      } else {
        for (const int arc : incoming_arc_indices_[node]) {
          adjacency.AppendToLastVector({tails_[arc], literals_[arc]});
        }
      }
    }
  }

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

    // The sum of the reduced costs of all the literals selected to form the
    // route(s) encoded by this state. If this becomes too large we know that
    // this state can never be used to get to a better solution than the current
    // best one and is thus "infeasible" for the problem of finding a better
    // solution.
    absl::int128 sum_of_reduced_costs = 0;
  };

  const int size = subset.size();
  const uint32_t final_mask = (1 << size) - 1;

  const auto& can_be_last_set = use_forward_direction
                                    ? has_outgoing_arcs_to_outside_
                                    : has_incoming_arcs_from_outside_;
  const auto& can_be_first_set = use_forward_direction
                                     ? has_incoming_arcs_from_outside_
                                     : has_outgoing_arcs_to_outside_;

  uint32_t can_be_last_mask = 0;
  for (int i = 0; i < subset.size(); ++i) {
    if (can_be_last_set[subset[i]]) {
      can_be_last_mask |= (1 << i);
    }
  }

  // This is also correlated to the work done, and we abort if we starts to
  // do too much work on one instance.
  int64_t allocated_memory_estimate = 0;

  // We just do a DFS from the initial state.
  std::vector<State> states;
  states.push_back(State());

  // If a state has an unique arc to enter/leave it, we can update it further.
  const auto update_using_unique_arc = [manager, this](UniqueArc unique_arc,
                                                       State& state) {
    if (!unique_arc.IsUnique()) return true;
    if (manager == nullptr) return true;

    ++num_full_dp_unique_arc_;
    const Literal unique_lit = literals_[unique_arc.Get()];
    state.sum_of_reduced_costs += manager->GetLiteralReducedCost(unique_lit);
    if (state.sum_of_reduced_costs > manager->ReducedCostsGap()) {
      ++num_full_dp_rc_skip_;
      return false;
    }

    absl::flat_hash_map<IntegerVariable, IntegerValue> copy = state.lbs;
    return PropagateLocalBounds(integer_trail_, root_level_bounds_,
                                binary_relation_repository_, implied_bounds_,
                                unique_lit, copy, &state.lbs);
  };

  // We always start with the first node in this case.
  if (special_node >= 0) {
    states.back().node_set = 1;
    states.back().last_nodes_set = 1;

    // If there is a unique way to enter (resp. leave) that subset, we can start
    // with tighter bounds!
    const UniqueArc unique_arc = use_forward_direction
                                     ? incoming_arcs_from_outside_[subset[0]]
                                     : outgoing_arcs_to_outside_[subset[0]];
    if (!update_using_unique_arc(unique_arc, states.back())) {
      return false;  // We can't even start!
    }
  }

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
        if (!can_be_first_set[subset[i]]) continue;

        // All "initial-state" start with empty hash-map that correspond to
        // the level zero bounds.
        State to_state;
        const uint32_t head_mask = (1 << i);
        to_state.node_set = from_state.node_set | head_mask;
        to_state.last_nodes_set = from_state.last_nodes_set | head_mask;
        to_state.lbs = from_state.lbs;
        to_state.sum_of_reduced_costs = from_state.sum_of_reduced_costs;

        const UniqueArc unique_arc =
            use_forward_direction ? incoming_arcs_from_outside_[subset[i]]
                                  : outgoing_arcs_to_outside_[subset[i]];
        if (!update_using_unique_arc(unique_arc, to_state)) {
          continue;
        }

        if (to_state.node_set == final_mask) {
          ++num_full_dp_early_abort_;
          return true;  // All served!
        }
        states.push_back(std::move(to_state));
      }
      continue;
    }

    // We have k routes, extend one of the last nodes.
    for (int i = 0; i < size; ++i) {
      const uint32_t tail_mask = 1 << i;
      if ((from_state.last_nodes_set & tail_mask) == 0) continue;
      const int tail = subset[i];
      for (const auto [head, literal] : adjacency[i]) {
        const uint32_t head_mask = (1 << index_in_subset_[head]);
        if (from_state.node_set & head_mask) continue;

        State to_state;

        // Use reduced costs to exclude solutions that cannot improve our
        // current best solution.
        if (manager != nullptr) {
          DCHECK_EQ(helper, nullptr);
          to_state.sum_of_reduced_costs =
              from_state.sum_of_reduced_costs +
              manager->GetLiteralReducedCost(literal);
          if (to_state.sum_of_reduced_costs > manager->ReducedCostsGap()) {
            ++num_full_dp_rc_skip_;
            continue;
          }
        }

        // We start from the old bounds and update them.
        to_state.lbs = from_state.lbs;
        if (helper != nullptr) {
          if (!helper->PropagateLocalBoundsUsingShortestPaths(
                  integer_trail_, tail, head, from_state.lbs, &to_state.lbs)) {
            continue;
          }
        } else {
          if (!PropagateLocalBounds(integer_trail_, root_level_bounds_,
                                    binary_relation_repository_,
                                    implied_bounds_, literal, from_state.lbs,
                                    &to_state.lbs)) {
            continue;
          }
        }

        to_state.node_set = from_state.node_set | head_mask;
        to_state.last_nodes_set = from_state.last_nodes_set | head_mask;
        to_state.last_nodes_set ^= tail_mask;
        allocated_memory_estimate += to_state.lbs.size();
        if (to_state.node_set == final_mask) {
          // One of the last node has no arc to outside, this is not possible.
          if (to_state.last_nodes_set & ~can_be_last_mask) continue;

          // Add the constraints implied by each last node that has an unique
          // way to leave the subset.
          bool infeasible = false;
          int last_mask = to_state.last_nodes_set;
          for (int j = 0; last_mask; j++, last_mask /= 2) {
            if ((last_mask & 1) == 0) continue;

            const UniqueArc unique_arc =
                use_forward_direction ? outgoing_arcs_to_outside_[subset[j]]
                                      : incoming_arcs_from_outside_[subset[j]];
            if (!update_using_unique_arc(unique_arc, to_state)) {
              infeasible = true;
              break;
            }
          }
          if (infeasible) {
            continue;
          }

          ++num_full_dp_early_abort_;
          return true;
        }
        states.push_back(std::move(to_state));
      }
    }
  }

  // We explored everything, no way to serve this with only k routes!
  return false;
}

namespace {

// Represents the relation tail_coeff.X + head_coeff.Y \in [lhs, rhs] between
// the X and Y expressions associated with the tail and head of the given arc,
// respectively, and the given dimension (head_coeff is always positive).
//
// An "empty" struct with all fields set to 0 is used to represent no relation.
struct LocalRelation {
  IntegerValue tail_coeff = 0;
  IntegerValue head_coeff = 0;
  IntegerValue lhs;
  IntegerValue rhs;

  bool empty() const { return tail_coeff == 0 && head_coeff == 0; }
  bool operator==(const LocalRelation& r) const {
    return tail_coeff == r.tail_coeff && head_coeff == r.head_coeff &&
           lhs == r.lhs && rhs == r.rhs;
  }
};

IntegerVariable UniqueSharedVariable(const sat::Relation& r1,
                                     const sat::Relation& r2) {
  const IntegerVariable X[2] = {PositiveVariable(r1.expr.vars[0]),
                                PositiveVariable(r1.expr.vars[1])};
  const IntegerVariable Y[2] = {PositiveVariable(r2.expr.vars[0]),
                                PositiveVariable(r2.expr.vars[1])};
  DCHECK_NE(X[0], X[1]);
  DCHECK_NE(Y[0], Y[1]);
  if (X[0] == Y[0] && X[1] != Y[1]) return X[0];
  if (X[0] == Y[1] && X[1] != Y[0]) return X[0];
  if (X[1] == Y[0] && X[0] != Y[1]) return X[1];
  if (X[1] == Y[1] && X[0] != Y[0]) return X[1];
  return kNoIntegerVariable;
}

class RouteRelationsBuilder {
 public:
  using HeadMinusTailBounds = RouteRelationsHelper::HeadMinusTailBounds;

  RouteRelationsBuilder(
      int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
      absl::Span<const Literal> literals,
      absl::Span<const AffineExpression> flat_node_dim_expressions,
      const BinaryRelationRepository& binary_relation_repository)
      : num_nodes_(num_nodes),
        num_arcs_(tails.size()),
        tails_(tails),
        heads_(heads),
        literals_(literals),
        binary_relation_repository_(binary_relation_repository) {
    if (!flat_node_dim_expressions.empty()) {
      DCHECK_EQ(flat_node_dim_expressions.size() % num_nodes, 0);
      num_dimensions_ = flat_node_dim_expressions.size() / num_nodes;
      flat_node_dim_expressions_.assign(flat_node_dim_expressions.begin(),
                                        flat_node_dim_expressions.end());
    }
  }

  int num_dimensions() const { return num_dimensions_; }

  std::vector<AffineExpression>& flat_node_dim_expressions() {
    return flat_node_dim_expressions_;
  }

  std::vector<HeadMinusTailBounds>& flat_arc_dim_relations() {
    return flat_arc_dim_relations_;
  }

  std::vector<IntegerValue>& flat_shortest_path_lbs() {
    return flat_shortest_path_lbs_;
  }

  bool BuildNodeExpressions() {
    if (flat_node_dim_expressions_.empty()) {
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
    return true;
  }

  void BuildArcRelations(Model* model) {
    // Step 3: compute the relation for each arc and dimension, now that the
    // variables associated with each node and dimension are known.
    ComputeArcRelations(model);
  }

  // We only consider lower bound for now.
  //
  // TODO(user): can we be smarter and compute a "scheduling" like lower bound
  // that exploit the [lb, ub] of the node expression even more?
  void BuildShortestPaths(Model* model) {
    if (num_nodes_ >= 100) return;

    auto& integer_trail = *model->GetOrCreate<IntegerTrail>();
    std::vector<IntegerValue> flat_trivial_lbs(num_nodes_ * num_nodes_);
    const auto trivial = [this, &flat_trivial_lbs](int i,
                                                   int j) -> IntegerValue& {
      CHECK_NE(i, j);
      CHECK_NE(i, 0);
      CHECK_NE(j, 0);
      return flat_trivial_lbs[i * num_nodes_ + j];
    };
    flat_shortest_path_lbs_.resize(num_nodes_ * num_nodes_ * num_dimensions_,
                                   std::numeric_limits<IntegerValue>::max());

    for (int dim = 0; dim < num_dimensions_; ++dim) {
      // We can never beat trivial bound relations, starts by filling them.
      for (int i = 1; i < num_nodes_; ++i) {
        const AffineExpression& i_expr = node_expression(i, dim);
        const IntegerValue i_ub = integer_trail.LevelZeroUpperBound(i_expr);
        for (int j = 1; j < num_nodes_; ++j) {
          if (i == j) continue;
          const AffineExpression& j_expr = node_expression(j, dim);
          trivial(i, j) = integer_trail.LevelZeroLowerBound(j_expr) - i_ub;
        }
      }

      // Starts by the known arc relations.
      const auto path = [this](int dim, int i, int j) -> IntegerValue& {
        return flat_shortest_path_lbs_[dim * num_nodes_ * num_nodes_ +
                                       i * num_nodes_ + j];
      };
      for (int arc = 0; arc < tails_.size(); ++arc) {
        const int tail = tails_[arc];
        const int head = heads_[arc];
        if (tail == 0 || head == 0 || tail == head) continue;
        path(dim, tail, head) =
            std::max(trivial(tail, head),
                     flat_arc_dim_relations_[arc * num_dimensions_ + dim].lhs);
      }

      // We do floyd-warshall to complete them into shortest path.
      for (int k = 1; k < num_nodes_; ++k) {
        for (int i = 1; i < num_nodes_; ++i) {
          if (i == k) continue;
          for (int j = 1; j < num_nodes_; ++j) {
            if (j == k || j == i) continue;
            path(dim, i, j) =
                std::min(path(dim, i, j),
                         std::max(trivial(i, j),
                                  CapAddI(path(dim, i, k), path(dim, k, j))));
          }
        }
      }
    }
  }

 private:
  AffineExpression& node_expression(int node, int dimension) {
    return flat_node_dim_expressions_[node * num_dimensions_ + dimension];
  };

  void ProcessNewArcConstantRelation(int arc_index, int dimension,
                                     IntegerValue head_minus_tail) {
    HeadMinusTailBounds& relation =
        flat_arc_dim_relations_[arc_index * num_dimensions_ + dimension];
    relation.lhs = std::max(relation.lhs, head_minus_tail);
    relation.rhs = std::min(relation.rhs, head_minus_tail);
  }

  void ProcessNewArcRelation(int arc_index, int dimension, LocalRelation r) {
    if (r.empty()) return;
    if (r.head_coeff < 0) {
      r.tail_coeff = -r.tail_coeff;
      r.head_coeff = -r.head_coeff;
      r.lhs = -r.lhs;
      r.rhs = -r.rhs;
      std::swap(r.lhs, r.rhs);
    }
    if (r.head_coeff != 1 || r.tail_coeff != -1) return;

    // Combine the relation information.
    HeadMinusTailBounds& relation =
        flat_arc_dim_relations_[arc_index * num_dimensions_ + dimension];
    relation.lhs = std::max(relation.lhs, r.lhs);
    relation.rhs = std::min(relation.rhs, r.rhs);
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
        DCHECK_NE(r.expr.vars[0], kNoIntegerVariable);
        DCHECK_NE(r.expr.vars[1], kNoIntegerVariable);
        cc_finder.AddEdge(PositiveVariable(r.expr.vars[0]),
                          PositiveVariable(r.expr.vars[1]));
      }
    }
    const std::vector<std::vector<IntegerVariable>> connected_components =
        cc_finder.FindConnectedComponents();
    for (int i = 0; i < connected_components.size(); ++i) {
      for (const IntegerVariable var : connected_components[i]) {
        dimension_by_var_[GetPositiveOnlyIndex(var)] = i;
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
        DCHECK_NE(r.expr.vars[0], kNoIntegerVariable);
        DCHECK_NE(r.expr.vars[1], kNoIntegerVariable);
        const int dimension = dimension_by_var_[GetPositiveOnlyIndex(
            PositiveVariable(r.expr.vars[0]))];
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
    flat_node_dim_expressions_ = std::vector<AffineExpression>(
        num_nodes_ * num_dimensions_, AffineExpression());
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
            DCHECK_EQ(dimension_by_var_[GetPositiveOnlyIndex(shared_var)], d);
            AffineExpression& node_expr = node_expression(n, d);
            if (node_expr.IsConstant()) {
              result.push({n, d});
            } else if (node_expr.var != shared_var) {
              VLOG(2) << "Several vars per node and dimension in route with "
                      << num_nodes_ << " nodes and " << num_arcs_ << " arcs";
              return {};
            }
            node_expr = shared_var;
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
      const AffineExpression node_expr = node_expression(node, dimension);
      DCHECK(!node_expr.IsConstant());
      node_dim_pairs_to_process.pop();
      for (const int arc_index : adjacent_arcs_per_node[node]) {
        const int tail = tails_[arc_index];
        const int head = heads_[arc_index];
        DCHECK(node == tail || node == head);
        int other_node = node == tail ? head : tail;
        if (!node_expression(other_node, dimension).IsConstant()) {
          continue;
        }
        IntegerVariable candidate_var = kNoIntegerVariable;
        bool candidate_var_is_unique = true;
        for (const int relation_index :
             binary_relation_repository_.IndicesOfRelationsEnforcedBy(
                 literals_[arc_index])) {
          const auto& r = binary_relation_repository_.relation(relation_index);
          DCHECK_NE(r.expr.vars[0], kNoIntegerVariable);
          DCHECK_NE(r.expr.vars[1], kNoIntegerVariable);
          const IntegerVariable var0 = PositiveVariable(r.expr.vars[0]);
          const IntegerVariable var1 = PositiveVariable(r.expr.vars[1]);
          if (var0 == node_expr.var) {
            if (candidate_var != kNoIntegerVariable && candidate_var != var1) {
              candidate_var_is_unique = false;
              break;
            }
            candidate_var = var1;
          }
          if (var1 == node_expr.var) {
            if (candidate_var != kNoIntegerVariable && candidate_var != var0) {
              candidate_var_is_unique = false;
              break;
            }
            candidate_var = var0;
          }
        }
        if (candidate_var != kNoIntegerVariable && candidate_var_is_unique) {
          node_expression(other_node, dimension) = candidate_var;
          node_dim_pairs_to_process.push({other_node, dimension});
        }
      }
    }
  }

  void ComputeArcRelations(Model* model) {
    auto& binary_implication_graph =
        *model->GetOrCreate<BinaryImplicationGraph>();
    const auto& integer_encoder = *model->GetOrCreate<IntegerEncoder>();
    const auto& trail = *model->GetOrCreate<Trail>();
    const auto& integer_trail = *model->GetOrCreate<IntegerTrail>();
    const auto& root_level_bounds =
        *model->GetOrCreate<RootLevelLinear2Bounds>();
    const auto& implied_bounds = *model->GetOrCreate<ImpliedBounds>();
    DCHECK_EQ(trail.CurrentDecisionLevel(), 0);

    flat_arc_dim_relations_ = std::vector<HeadMinusTailBounds>(
        num_arcs_ * num_dimensions_, HeadMinusTailBounds());
    binary_implication_graph.ResetWorkDone();
    for (int i = 0; i < num_arcs_; ++i) {
      const int tail = tails_[i];
      const int head = heads_[i];
      if (tail == head) continue;
      // The literals directly or indirectly implied by the arc literal,
      // including the arc literal itself.
      const Literal arc_literal_singleton[1] = {literals_[i]};
      absl::Span<const Literal> implied_literals =
          binary_implication_graph.WorkDone() < 1e8
              ? binary_implication_graph.GetAllImpliedLiterals(literals_[i])
              : absl::MakeSpan(arc_literal_singleton);
      // The integer view of the implied literals (resp. of their negation), and
      // the variable lower bounds implied directly or indirectly by the arc
      // literal.
      absl::flat_hash_set<IntegerVariable> implied_views;
      absl::flat_hash_set<IntegerVariable> negated_implied_views;
      absl::flat_hash_map<IntegerVariable, IntegerValue> implied_lower_bounds;
      for (const Literal implied : implied_literals) {
        implied_views.insert(integer_encoder.GetLiteralView(implied));
        negated_implied_views.insert(
            integer_encoder.GetLiteralView(implied.Negated()));
        for (const auto& [var, lb] :
             integer_encoder.GetIntegerLiterals(implied)) {
          auto it = implied_lower_bounds.find(var);
          if (it == implied_lower_bounds.end()) {
            implied_lower_bounds[var] = lb;
          } else {
            it->second = std::max(it->second, lb);
          }
        }
      }
      // Returns the bounds of the given expression, assuming that all the
      // literals implied by the arc literal are 1.
      auto get_bounds = [&](const NodeExpression& expr) {
        if (expr.var != kNoIntegerVariable) {
          if (implied_views.contains(expr.var)) {
            IntegerValue constant_value = expr.coeff + expr.offset;
            return std::make_pair(constant_value, constant_value);
          }
          if (implied_views.contains(NegationOf(expr.var))) {
            IntegerValue constant_value = -expr.coeff + expr.offset;
            return std::make_pair(constant_value, constant_value);
          }
          if (negated_implied_views.contains(expr.var) ||
              negated_implied_views.contains(NegationOf(expr.var))) {
            return std::make_pair(expr.offset, expr.offset);
          }
        }
        const AffineExpression e(expr.var, expr.coeff, expr.offset);
        IntegerValue lb = integer_trail.LevelZeroLowerBound(e);
        auto it = implied_lower_bounds.find(e.var);
        if (it != implied_lower_bounds.end()) {
          lb = std::max(lb, e.ValueAt(it->second));
        }
        IntegerValue ub = integer_trail.LevelZeroUpperBound(e);
        it = implied_lower_bounds.find(NegationOf(e.var));
        if (it != implied_lower_bounds.end()) {
          ub = std::min(ub, e.ValueAt(-it->second));
        }
        return std::make_pair(lb, ub);
      };
      // Changes `expr` to a constant expression if possible, and returns true.
      // Otherwise, returns false.
      auto to_constant = [&](NodeExpression& expr) {
        if (expr.var == kNoIntegerVariable) return true;
        if (integer_trail.IsFixed(expr.var)) {
          expr.offset += integer_trail.FixedValue(expr.var) * expr.coeff;
          expr.var = kNoIntegerVariable;
          expr.coeff = 0;
          return true;
        }
        const auto [min, max] = get_bounds(expr);
        if (min == max) {
          expr = NodeExpression(kNoIntegerVariable, 0, min);
          return true;
        }
        return false;
      };
      for (int dimension = 0; dimension < num_dimensions_; ++dimension) {
        NodeExpression tail_expr(node_expression(tail, dimension));
        NodeExpression head_expr(node_expression(head, dimension));
        NodeExpression tail_const_expr = tail_expr;
        NodeExpression head_const_expr = head_expr;
        const bool tail_const = to_constant(tail_const_expr);
        const bool head_const = to_constant(head_const_expr);
        if (tail_const && head_const) {
          ProcessNewArcConstantRelation(
              i, dimension, head_const_expr.offset - tail_const_expr.offset);
          continue;
        }
        for (const Literal implied_lit : implied_literals) {
          if (tail_const || head_const) {
            const IntegerVariable var =
                head_const ? tail_expr.var : head_expr.var;
            DCHECK(var != kNoIntegerVariable);
            auto [lb, ub] = implied_bounds.GetImpliedBounds(implied_lit, var);
            if (lb != kMinIntegerValue || ub != kMaxIntegerValue) {
              lb = std::max(lb, integer_trail.LevelZeroLowerBound(var));
              ub = std::min(ub, integer_trail.LevelZeroUpperBound(var));
              Relation r = {
                  .enforcement = implied_lit,
                  .expr = LinearExpression2(var, kNoIntegerVariable, 1, 0),
                  .lhs = lb,
                  .rhs = ub};
              DCHECK(VariableIsPositive(var));
              if (var == tail_const_expr.var) {
                std::swap(r.expr.vars[0], r.expr.vars[1]);
                std::swap(r.expr.coeffs[0], r.expr.coeffs[1]);
              }
              ComputeArcRelation(i, dimension, tail_const_expr, head_const_expr,
                                 r, integer_trail);
            }
          }
          if (tail_expr.var != kNoIntegerVariable &&
              head_expr.var != kNoIntegerVariable) {
            for (const int relation_index :
                 binary_relation_repository_.IndicesOfRelationsEnforcedBy(
                     implied_lit)) {
              Relation r = binary_relation_repository_.relation(relation_index);
              r.expr.MakeVariablesPositive();
              // Try to match the relation variables with the node expression
              // variables. First swap the relation terms if needed (this does
              // not change the relation bounds).
              if (r.expr.vars[0] == head_expr.var ||
                  r.expr.vars[1] == tail_expr.var) {
                std::swap(r.expr.vars[0], r.expr.vars[1]);
                std::swap(r.expr.coeffs[0], r.expr.coeffs[1]);
              }
              // If the relation and node expression variables do not match, we
              // cannot use this relation for this arc.
              if (!((tail_expr.var == r.expr.vars[0] &&
                     head_expr.var == r.expr.vars[1]) ||
                    (tail_expr.var == r.expr.vars[1] &&
                     head_expr.var == r.expr.vars[0]))) {
                continue;
              }
              ComputeArcRelation(i, dimension, tail_expr, head_expr, r,
                                 integer_trail);
            }
          }
        }

        // Check if we can use non-enforced relations to improve the relations.
        if (!tail_expr.IsEmpty() && !head_expr.IsEmpty()) {
          for (const auto& [expr, lb, ub] :
               root_level_bounds.GetAllBoundsContainingVariables(
                   tail_expr.var, head_expr.var)) {
            DCHECK_EQ(expr.vars[0], tail_expr.var);
            DCHECK_EQ(expr.vars[1], head_expr.var);
            ComputeArcRelation(i, dimension, tail_expr, head_expr,
                               Relation{Literal(kNoLiteralIndex), expr, lb, ub},
                               integer_trail);
          }
        }

        // Check if we can use the default relation to improve the relations.
        // Compute the bounds of X_head - X_tail as [lb(X_head) - ub(X_tail),
        // ub(X_head) - lb(X_tail)], which is always true. Note that by our
        // overflow precondition, the difference of any two variable bounds
        // should fit on an int64_t.
        std::pair<IntegerValue, IntegerValue> bounds;
        if (head_expr.var == tail_expr.var) {
          const NodeExpression offset_expr(head_expr.var,
                                           head_expr.coeff - tail_expr.coeff,
                                           head_expr.offset - tail_expr.offset);
          bounds = get_bounds(offset_expr);
        } else {
          const auto [tail_min, tail_max] = get_bounds(tail_expr);
          const auto [head_min, head_max] = get_bounds(head_expr);
          bounds = {head_min - tail_max, head_max - tail_min};
        }
        ProcessNewArcRelation(i, dimension,
                              {.tail_coeff = -1,
                               .head_coeff = 1,
                               .lhs = bounds.first,
                               .rhs = bounds.second});
      }
    }
  }

  // Infers a relation between two given expressions which is equivalent to a
  // given relation `r` between the variables used in these expressions.
  void ComputeArcRelation(int arc_index, int dimension,
                          const NodeExpression& tail_expr,
                          const NodeExpression& head_expr, sat::Relation r,
                          const IntegerTrail& integer_trail) {
    DCHECK(
        (r.expr.vars[0] == tail_expr.var && r.expr.vars[1] == head_expr.var) ||
        (r.expr.vars[0] == head_expr.var && r.expr.vars[1] == tail_expr.var));
    if (r.expr.vars[0] != tail_expr.var) {
      std::swap(r.expr.vars[0], r.expr.vars[1]);
      std::swap(r.expr.coeffs[0], r.expr.coeffs[1]);
    }
    if (r.expr.coeffs[0] == 0 || tail_expr.coeff == 0) {
      LocalRelation result = ComputeArcUnaryRelation(
          head_expr, tail_expr, r.expr.coeffs[1], r.lhs, r.rhs);
      std::swap(result.tail_coeff, result.head_coeff);
      ProcessNewArcRelation(arc_index, dimension, result);
      return;
    }
    if (r.expr.coeffs[1] == 0 || head_expr.coeff == 0) {
      ProcessNewArcRelation(
          arc_index, dimension,
          ComputeArcUnaryRelation(tail_expr, head_expr, r.expr.coeffs[0], r.lhs,
                                  r.rhs));
      return;
    }
    const auto [lhs, rhs] =
        GetDifferenceBounds(tail_expr, head_expr, r,
                            {integer_trail.LowerBound(tail_expr.var),
                             integer_trail.UpperBound(tail_expr.var)},
                            {integer_trail.LowerBound(head_expr.var),
                             integer_trail.UpperBound(head_expr.var)});
    ProcessNewArcRelation(
        arc_index, dimension,
        {.tail_coeff = -1, .head_coeff = 1, .lhs = lhs, .rhs = rhs});
  }

  // Returns a relation between two given expressions which is equivalent to a
  // given relation "coeff * X \in [lhs, rhs]", where X is the variable used in
  // `tail_expression` (or an empty relation if this is not possible).
  LocalRelation ComputeArcUnaryRelation(const NodeExpression& tail_expr,
                                        const NodeExpression& head_expr,
                                        IntegerValue coeff, IntegerValue lhs,
                                        IntegerValue rhs) {
    DCHECK_NE(coeff, 0);
    // A relation a * X \in [lhs, rhs] is equivalent to a (k.a/A) * (A.X+α) + k'
    // * (B.Y+β) \in [k.lhs+ɣ, k.rhs+ɣ] relation between the A.X+α and B.Y+β
    // terms if A != 0, if the division is exact, and if k' = 0 or B = 0, where
    // ɣ=(k.a/A)*α+k'*β. The smallest k > 0 such that k.a/A is integer is
    // |A|/gcd(a, A).
    if (tail_expr.coeff == 0) return {};
    const int64_t k = std::abs(tail_expr.coeff.value()) /
                      std::gcd(tail_expr.coeff.value(), coeff.value());
    // If B = 0 we can use any value for k' = head_coeff. We use the opposite of
    // tail_coeff to get a relation with +1/-1 coefficients if possible.
    const IntegerValue tail_coeff = (k * coeff) / tail_expr.coeff;
    const IntegerValue head_coeff = head_expr.coeff != 0 ? 0 : -tail_coeff;
    const IntegerValue domain_offset =
        CapAddI(CapProdI(tail_coeff, tail_expr.offset),
                CapProdI(head_coeff, head_expr.offset));
    if (AtMinOrMaxInt64I(domain_offset)) {
      return {};
    }
    const IntegerValue lhs_offset = CapAddI(CapProdI(k, lhs), domain_offset);
    const IntegerValue rhs_offset = CapAddI(CapProdI(k, rhs), domain_offset);
    if (AtMinOrMaxInt64I(lhs_offset) || AtMinOrMaxInt64I(rhs_offset)) {
      return {};
    }
    return {
        .tail_coeff = tail_coeff,
        .head_coeff = head_coeff,
        .lhs = lhs_offset,
        .rhs = rhs_offset,
    };
  }

  const int num_nodes_;
  const int num_arcs_;
  absl::Span<const int> tails_;
  absl::Span<const int> heads_;
  absl::Span<const Literal> literals_;
  const BinaryRelationRepository& binary_relation_repository_;

  int num_dimensions_;
  absl::flat_hash_map<PositiveOnlyIndex, int> dimension_by_var_;
  absl::flat_hash_map<Literal, int> num_arcs_per_literal_;

  // The indices of the binary relations associated with the incoming and
  // outgoing arcs of each node, per dimension.
  std::vector<std::vector<std::vector<int>>> adjacent_relation_indices_;

  // See comments for the same fields in RouteRelationsHelper() that this
  // builder creates.
  std::vector<AffineExpression> flat_node_dim_expressions_;
  std::vector<HeadMinusTailBounds> flat_arc_dim_relations_;
  std::vector<IntegerValue> flat_shortest_path_lbs_;
};

}  // namespace

RoutingCumulExpressions DetectDimensionsAndCumulExpressions(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals,
    const BinaryRelationRepository& binary_relation_repository) {
  RoutingCumulExpressions result;
  RouteRelationsBuilder builder(num_nodes, tails, heads, literals, {},
                                binary_relation_repository);
  if (!builder.BuildNodeExpressions()) return result;
  result.num_dimensions = builder.num_dimensions();
  result.flat_node_dim_expressions =
      std::move(builder.flat_node_dim_expressions());
  return result;
}

namespace {
// Returns a lower bound of y_expr - x_expr.
IntegerValue GetDifferenceLowerBound(
    const NodeExpression& x_expr, const NodeExpression& y_expr,
    const sat::Relation& r,
    const std::pair<IntegerValue, IntegerValue>& x_var_bounds,
    const std::pair<IntegerValue, IntegerValue>& y_var_bounds) {
  // Let's note x_expr = A.x+α, y_expr = B.x+β, and r = "b.y-a.x in [lhs, rhs]".
  // We have x in [lb(x), ub(x)] and y in [lb(y), ub(y)], and we want to find
  // the lower bound of y_expr - x_expr = lb(B.x - A.y) + β - α. We have:
  //   B.y - A.x = [(B-k.b).y + k.b.y] - [(A-k.a).x + k.a.x]
  //             = (B+k.b).y + (k.a-A).x + k.(b.y - a.x)
  // for any k. A lower bound on the first term is (B-k.b).lb(y) if B-k.b >= 0,
  // and (B-k.b).ub(y) otherwise. This can be written as:
  //   (B-k.b).y >= max(0, B-k.b).lb(y) + min(0, B-k.b).ub(y)
  // Similarly:
  //   (k.a-A).x >= max(0, k.a-A).lb(x) + min(0, k.a-A).ub(x)
  // And:
  //   k.(b.y - a.x) >= max(0, k).lhs + min(0, k).rhs
  // Hence we get:
  //   B.y - A.x >= max(0, B-k.b).lb(y) + min(0, B-k.b).ub(y) +
  //                max(0, k.a-A).lb(x) + min(0, k.a-A).ub(x) +
  //                max(0, k).lhs + min(0, k).rhs
  // The derivative of this expression with respect to k is piecewise constant
  // and can only change value around 0, A/a, and B/b. Hence we can compute an
  // "interesting" lower bound by taking the maximum of this expression around
  // these points, instead of over all k values.
  // TODO(user): overflows could happen if the node expressions are
  // provided by the user in the model proto.
  auto lower_bound = [&](IntegerValue k) {
    const IntegerValue y_coeff = y_expr.coeff - k * r.expr.coeffs[1];
    const IntegerValue x_coeff = k * (-r.expr.coeffs[0]) - x_expr.coeff;
    return y_coeff * (y_coeff >= 0 ? y_var_bounds.first : y_var_bounds.second) +
           x_coeff * (x_coeff >= 0 ? x_var_bounds.first : x_var_bounds.second) +
           k * (k >= 0 ? r.lhs : r.rhs);
  };
  const IntegerValue k_x =
      MathUtil::FloorOfRatio(x_expr.coeff, -r.expr.coeffs[0]);
  const IntegerValue k_y =
      MathUtil::FloorOfRatio(y_expr.coeff, r.expr.coeffs[1]);
  IntegerValue result = lower_bound(0);
  result = std::max(result, lower_bound(k_x));
  result = std::max(result, lower_bound(k_x + 1));
  result = std::max(result, lower_bound(k_y));
  result = std::max(result, lower_bound(k_y + 1));
  return result + y_expr.offset - x_expr.offset;
}
}  // namespace

std::pair<IntegerValue, IntegerValue> GetDifferenceBounds(
    const NodeExpression& x_expr, const NodeExpression& y_expr,
    const sat::Relation& r,
    const std::pair<IntegerValue, IntegerValue>& x_var_bounds,
    const std::pair<IntegerValue, IntegerValue>& y_var_bounds) {
  DCHECK_EQ(x_expr.var, r.expr.vars[0]);
  DCHECK_EQ(y_expr.var, r.expr.vars[1]);
  DCHECK_NE(x_expr.var, kNoIntegerVariable);
  DCHECK_NE(y_expr.var, kNoIntegerVariable);
  DCHECK_NE(x_expr.coeff, 0);
  DCHECK_NE(y_expr.coeff, 0);
  DCHECK_NE(r.expr.coeffs[0], 0);
  DCHECK_NE(r.expr.coeffs[1], 0);
  const IntegerValue lb =
      GetDifferenceLowerBound(x_expr, y_expr, r, x_var_bounds, y_var_bounds);
  const IntegerValue ub = -GetDifferenceLowerBound(
      x_expr.Negated(), y_expr.Negated(), r, x_var_bounds, y_var_bounds);
  return {lb, ub};
}

std::unique_ptr<RouteRelationsHelper> RouteRelationsHelper::Create(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals,
    absl::Span<const AffineExpression> flat_node_dim_expressions,
    const BinaryRelationRepository& binary_relation_repository, Model* model) {
  CHECK(model != nullptr);
  if (flat_node_dim_expressions.empty()) return nullptr;
  RouteRelationsBuilder builder(num_nodes, tails, heads, literals,
                                flat_node_dim_expressions,
                                binary_relation_repository);
  builder.BuildArcRelations(model);
  builder.BuildShortestPaths(model);
  std::unique_ptr<RouteRelationsHelper> helper(new RouteRelationsHelper(
      builder.num_dimensions(), std::move(builder.flat_node_dim_expressions()),
      std::move(builder.flat_arc_dim_relations()),
      std::move(builder.flat_shortest_path_lbs())));
  if (VLOG_IS_ON(2)) helper->LogStats();
  return helper;
}

RouteRelationsHelper::RouteRelationsHelper(
    int num_dimensions, std::vector<AffineExpression> flat_node_dim_expressions,
    std::vector<HeadMinusTailBounds> flat_arc_dim_relations,
    std::vector<IntegerValue> flat_shortest_path_lbs)
    : num_nodes_(flat_node_dim_expressions.size() / num_dimensions),
      num_dimensions_(num_dimensions),
      flat_node_dim_expressions_(std::move(flat_node_dim_expressions)),
      flat_arc_dim_relations_(std::move(flat_arc_dim_relations)),
      flat_shortest_path_lbs_(std::move(flat_shortest_path_lbs)) {
  DCHECK_GE(num_dimensions_, 1);
  DCHECK_EQ(flat_node_dim_expressions_.size(), num_nodes_ * num_dimensions_);
}

IntegerValue RouteRelationsHelper::GetArcOffsetLowerBound(
    int arc, int dimension, bool negate_expressions) const {
  // TODO(user): If level zero bounds got tighter since we precomputed the
  // relation, we might want to recompute it again.
  const auto& r = GetArcRelation(arc, dimension);
  return negate_expressions ? -r.rhs : r.lhs;
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
      if (!GetNodeExpression(i, d).IsConstant()) ++num_vars;
    }
    for (int i = 0; i < num_arcs; ++i) {
      if (GetArcRelation(i, d).lhs != kMinIntegerValue ||
          GetArcRelation(i, d).rhs != kMaxIntegerValue) {
        ++num_relations;
      }
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
  DCHECK(VariableIsPositive(var));
  return var.value() >> 1;
}

// Returns a repository containing a partial view (i.e. without coefficients or
// domains) of the enforced linear constraints (of size 2 only) in `model`. This
// is the only information needed to infer the mapping from variables to nodes
// in routes constraints.
BinaryRelationRepository ComputePartialBinaryRelationRepository(
    const CpModelProto& model) {
  Model empty_model;
  BinaryRelationRepository repository(&empty_model);
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
int MaybeFillRoutesConstraintNodeExpressions(
    RoutesConstraintProto& routes, const BinaryRelationRepository& repository) {
  int max_node = 0;
  for (const int node : routes.tails()) {
    max_node = std::max(max_node, node);
  }
  for (const int node : routes.heads()) {
    max_node = std::max(max_node, node);
  }
  const int num_nodes = max_node + 1;
  std::vector<int> tails(routes.tails().begin(), routes.tails().end());
  std::vector<int> heads(routes.heads().begin(), routes.heads().end());
  std::vector<Literal> literals;
  literals.reserve(routes.literals_size());
  for (int lit : routes.literals()) {
    literals.push_back(ToLiteral(lit));
  }

  const RoutingCumulExpressions cumuls = DetectDimensionsAndCumulExpressions(
      num_nodes, tails, heads, literals, repository);
  if (cumuls.num_dimensions == 0) return 0;

  for (int d = 0; d < cumuls.num_dimensions; ++d) {
    RoutesConstraintProto::NodeExpressions& dimension =
        *routes.add_dimensions();
    for (int n = 0; n < num_nodes; ++n) {
      const AffineExpression expr = cumuls.GetNodeExpression(n, d);
      LinearExpressionProto& node_expr = *dimension.add_exprs();
      if (expr.var != kNoIntegerVariable) {
        node_expr.add_vars(ToNodeVariableIndex(PositiveVariable(expr.var)));
        node_expr.add_coeffs(VariableIsPositive(expr.var)
                                 ? expr.coeff.value()
                                 : -expr.coeff.value());
      }
      node_expr.set_offset(expr.constant.value());
    }
  }
  return cumuls.num_dimensions;
}

}  // namespace

std::pair<int, int> MaybeFillMissingRoutesConstraintNodeExpressions(
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
        MaybeFillRoutesConstraintNodeExpressions(*routes, partial_repository);
  }
  return {static_cast<int>(routes_to_fill.size()), total_num_dimensions};
}

namespace {

class RoutingCutHelper {
 public:
  RoutingCutHelper(int num_nodes, bool is_route_constraint,
                   absl::Span<const AffineExpression> flat_node_dim_expressions,
                   absl::Span<const int> tails, absl::Span<const int> heads,
                   absl::Span<const Literal> literals, Model* model)
      : num_nodes_(num_nodes),
        is_route_constraint_(is_route_constraint),
        tails_(tails.begin(), tails.end()),
        heads_(heads.begin(), heads.end()),
        literals_(literals.begin(), literals.end()),
        params_(*model->GetOrCreate<SatParameters>()),
        trail_(*model->GetOrCreate<Trail>()),
        integer_trail_(*model->GetOrCreate<IntegerTrail>()),
        binary_relation_repository_(
            *model->GetOrCreate<BinaryRelationRepository>()),
        implied_bounds_(*model->GetOrCreate<ImpliedBounds>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        encoder_(model->GetOrCreate<IntegerEncoder>()),
        root_level_bounds_(*model->GetOrCreate<RootLevelLinear2Bounds>()),
        in_subset_(num_nodes, false),
        self_arc_literal_(num_nodes_),
        self_arc_lp_value_(num_nodes_),
        nodes_incoming_weight_(num_nodes_),
        nodes_outgoing_weight_(num_nodes_),
        min_outgoing_flow_helper_(num_nodes, tails_, heads_, literals_, model),
        route_relations_helper_(RouteRelationsHelper::Create(
            num_nodes, tails_, heads_, literals_, flat_node_dim_expressions,
            *model->GetOrCreate<BinaryRelationRepository>(), model)) {}

  int num_nodes() const { return num_nodes_; }

  bool is_route_constraint() const { return is_route_constraint_; }

  // Returns the arcs computed by InitializeForNewLpSolution().
  absl::Span<const ArcWithLpValue> relevant_arcs() const {
    return relevant_arcs_;
  }

  // Returns the arcs computed in InitializeForNewLpSolution(), sorted by
  // decreasing lp_value.
  //
  // Note: If is_route_constraint() is true, we do not include arcs from/to
  // the depot here.
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

    // We exclude arcs from/to the depot when we have a route constraint.
    if (is_route_constraint_) {
      int new_size = 0;
      for (const ArcWithLpValue& arc : symmetrized_relevant_arcs_) {
        if (arc.tail == 0 || arc.head == 0) continue;
        symmetrized_relevant_arcs_[new_size++] = arc;
      }
      symmetrized_relevant_arcs_.resize(new_size);
    }

    return symmetrized_relevant_arcs_;
  }

  // Try to add an outgoing cut from the given subset.
  bool TrySubsetCut(int known_bound, std::string name,
                    absl::Span<const int> subset,
                    LinearConstraintManager* manager);

  // Returns a bound on the number of vehicle that is valid for this subset and
  // all superset. This takes a current valid bound. It might do nothing
  // depening on the parameters and just return the initial bound.
  int ShortestPathBound(int bound, absl::Span<const int> subset);

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

  // Try adding cuts to exclude paths in the residual graph corresponding to the
  // LP solution which are actually infeasible.
  void TryInfeasiblePathCuts(LinearConstraintManager* manager);

 private:
  // If bool is true it is a "incoming" cut, otherwise "outgoing" one.
  struct BestChoice {
    int node;
    bool incoming;
    double weight;
  };
  BestChoice PickBestNode(absl::Span<const int> cannot_be_first,
                          absl::Span<const int> cannot_be_last);

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
                      int64_t rhs_lower_bound);

  bool MaybeAddStrongerFlowCut(LinearConstraintManager* manager,
                               std::string name, int k,
                               absl::Span<const int> cannot_be_first,
                               absl::Span<const int> cannot_be_last,
                               const std::vector<bool>& in_subset);

  void GenerateCutsForInfeasiblePaths(
      int start_node, int max_path_length,
      const CompactVectorVector<int, std::pair<int, double>>&
          outgoing_arcs_with_lp_values,
      LinearConstraintManager* manager, TopNCuts& top_n_cuts);

  const int num_nodes_;
  const bool is_route_constraint_;
  std::vector<int> tails_;
  std::vector<int> heads_;
  std::vector<Literal> literals_;
  std::vector<ArcWithLpValue> relevant_arcs_;
  std::vector<std::pair<int, double>> relevant_arc_indices_;
  std::vector<ArcWithLpValue> symmetrized_relevant_arcs_;
  std::vector<std::pair<int, int>> ordered_arcs_;

  const SatParameters& params_;
  const Trail& trail_;
  const IntegerTrail& integer_trail_;
  const BinaryRelationRepository& binary_relation_repository_;
  const ImpliedBounds& implied_bounds_;
  ModelRandomGenerator* random_;
  IntegerEncoder* encoder_;
  const RootLevelLinear2Bounds& root_level_bounds_;

  std::vector<bool> in_subset_;

  // Self-arc information, indexed in [0, num_nodes_)
  std::vector<int> nodes_with_self_arc_;
  std::vector<Literal> self_arc_literal_;
  std::vector<double> self_arc_lp_value_;

  // Initialized by TrySubsetCut() for each new subset.
  std::vector<double> nodes_incoming_weight_;
  std::vector<double> nodes_outgoing_weight_;

  // Helper to compute bounds on the minimum number of vehicles to serve a
  // subset.
  MaxBoundedSubsetSum max_bounded_subset_sum_;
  MaxBoundedSubsetSumExact max_bounded_subset_sum_exact_;
  MinOutgoingFlowHelper min_outgoing_flow_helper_;
  std::unique_ptr<RouteRelationsHelper> route_relations_helper_;
};

void RoutingCutHelper::FilterFalseArcsAtLevelZero() {
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

void RoutingCutHelper::InitializeForNewLpSolution(
    LinearConstraintManager* manager) {
  FilterFalseArcsAtLevelZero();

  // We will collect only the arcs with a positive lp_values to speed up some
  // computation below.
  relevant_arcs_.clear();
  relevant_arc_indices_.clear();
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
    relevant_arc_indices_.push_back({i, lp_value});
    relevant_arc_by_decreasing_lp_values.push_back({lp_value, i});
  }
  std::sort(relevant_arc_by_decreasing_lp_values.begin(),
            relevant_arc_by_decreasing_lp_values.end(),
            std::greater<std::pair<double, int>>());

  gtl::STLSortAndRemoveDuplicates(&nodes_with_self_arc_);

  ordered_arcs_.clear();
  for (const auto& [score, arc] : relevant_arc_by_decreasing_lp_values) {
    if (is_route_constraint_) {
      if (tails_[arc] == 0 || heads_[arc] == 0) continue;
    }
    ordered_arcs_.push_back({tails_[arc], heads_[arc]});
  }

  if (absl::GetFlag(FLAGS_cp_model_dump_routes_support_graphs) &&
      trail_.CurrentDecisionLevel() == 0) {
    static std::atomic<int> counter = 0;
    const std::string name =
        absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                     "support_graph_", counter++, ".pb.txt");
    LOG(INFO) << "Dumping routes support graph to '" << name << "'.";
    RoutesSupportGraphProto support_graph_proto;
    for (auto& [tail, head, lp_value] : relevant_arcs_) {
      auto* arc = support_graph_proto.add_arc_lp_values();
      arc->set_tail(tail);
      arc->set_head(head);
      arc->set_lp_value(lp_value);
    }
    CHECK(WriteModelProtoToFile(support_graph_proto, name));
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
// mentioned above.
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

RoutingCutHelper::BestChoice RoutingCutHelper::PickBestNode(
    absl::Span<const int> cannot_be_first,
    absl::Span<const int> cannot_be_last) {
  // Pick the set of candidates with the highest lp weight.
  std::vector<BestChoice> bests;
  double best_weight = 0.0;
  for (const int n : cannot_be_first) {
    const double weight = nodes_incoming_weight_[n];
    if (bests.empty() || weight > best_weight) {
      bests.clear();
      bests.push_back({n, true, weight});
      best_weight = weight;
    } else if (weight == best_weight) {
      bests.push_back({n, true, weight});
    }
  }
  for (const int n : cannot_be_last) {
    const double weight = nodes_outgoing_weight_[n];
    if (bests.empty() || weight > best_weight) {
      bests.clear();
      bests.push_back({n, false, weight});
      best_weight = weight;
    } else if (weight == best_weight) {
      bests.push_back({n, false, weight});
    }
  }
  if (bests.empty()) return {-1, true};
  return bests.size() == 1
             ? bests[0]
             : bests[absl::Uniform<int>(*random_, 0, bests.size())];
}

bool RoutingCutHelper::MaybeAddStrongerFlowCut(
    LinearConstraintManager* manager, std::string name, int k,
    absl::Span<const int> cannot_be_first, absl::Span<const int> cannot_be_last,
    const std::vector<bool>& in_subset) {
  if (cannot_be_first.empty() && cannot_be_last.empty()) return false;

  // We pick the best node out of cannot_be_first/cannot_be_last to derive
  // a stronger cut.
  const BestChoice best_choice = PickBestNode(cannot_be_first, cannot_be_last);
  const int best_node = best_choice.node;
  const bool incoming = best_choice.incoming;
  CHECK_GE(best_node, 0);

  const auto consider_arc = [incoming, &in_subset](int t, int h) {
    return incoming ? !in_subset[t] && in_subset[h]
                    : in_subset[t] && !in_subset[h];
  };

  // We consider the incoming (resp. outgoing_flow) not arriving at (resp. not
  // leaving from) best_node.
  //
  // This flow must be >= k.
  double flow = 0.0;
  for (const auto arc : relevant_arcs_) {
    if (!consider_arc(arc.tail, arc.head)) continue;
    if (arc.tail != best_node && arc.head != best_node) {
      flow += arc.lp_value;
    }
  }

  // Add cut flow >= k.
  const double kTolerance = 1e-4;
  if (flow < static_cast<double>(k) - kTolerance) {
    LinearConstraintBuilder cut_builder(encoder_, IntegerValue(k),
                                        kMaxIntegerValue);
    for (int i = 0; i < tails_.size(); ++i) {
      if (!consider_arc(tails_[i], heads_[i])) continue;
      if (tails_[i] != best_node && heads_[i] != best_node) {
        CHECK(cut_builder.AddLiteralTerm(literals_[i], IntegerValue(1)));
      }
    }
    return manager->AddCut(cut_builder.Build(), name);
  }
  return false;
}

bool RoutingCutHelper::AddOutgoingCut(LinearConstraintManager* manager,
                                      std::string name, int subset_size,
                                      const std::vector<bool>& in_subset,
                                      int64_t rhs_lower_bound) {
  // Skip cut if it is not violated.
  const auto [in_flow, out_flow] =
      GetIncomingAndOutgoingLpFlow(relevant_arcs_, in_subset);
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
      CHECK(outgoing.AddLiteralTerm(literals_[i], IntegerValue(1)));
    } else if (!tail_in && head_in) {
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

int RoutingCutHelper::ShortestPathBound(int bound,
                                        absl::Span<const int> subset) {
  if (!is_route_constraint_) return bound;
  if (!nodes_with_self_arc_.empty()) return bound;
  if (route_relations_helper_ == nullptr) return bound;
  if (!route_relations_helper_->HasShortestPathsInformation()) return bound;
  if (subset.size() >
      params_.routing_cut_subset_size_for_shortest_paths_bound()) {
    return bound;
  }

  while (bound < subset.size()) {
    if (min_outgoing_flow_helper_.SubsetMightBeServedWithKRoutes(
            bound, subset, route_relations_helper_.get())) {
      break;
    }
    bound += 1;
  }

  return bound;
}

bool RoutingCutHelper::TrySubsetCut(int known_bound, std::string name,
                                    absl::Span<const int> subset,
                                    LinearConstraintManager* manager) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LT(subset.size(), num_nodes_);

  // Do some initialization.
  in_subset_.assign(num_nodes_, false);
  for (const int n : subset) {
    in_subset_[n] = true;

    // We should aways generate subset without the depot for the route
    // constraint. This is because we remove arcs from/to the depot in the
    // tree based heuristics to generate all the subsets we try.
    if (is_route_constraint_) CHECK_NE(n, 0);
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
                          /*rhs_lower_bound=*/1);
  }

  // Compute the LP weight from/to the subset, and also the LP weight incoming
  // from outside to a given node in a subset or from the subset to a given node
  // outside (similarly for the outgoing case).
  double total_outgoing_weight = 0.0;
  double total_incoming_weight = 0.0;
  nodes_incoming_weight_.assign(num_nodes_, 0);
  nodes_outgoing_weight_.assign(num_nodes_, 0);
  for (const auto arc : relevant_arcs_) {
    if (in_subset_[arc.tail] != in_subset_[arc.head]) {
      if (in_subset_[arc.tail]) {
        total_outgoing_weight += arc.lp_value;
      } else {
        total_incoming_weight += arc.lp_value;
      }
      nodes_outgoing_weight_[arc.tail] += arc.lp_value;
      nodes_incoming_weight_[arc.head] += arc.lp_value;
    }
  }

  BestBoundHelper best_bound(name);
  best_bound.Update(1, "");
  best_bound.Update(known_bound, "Hierarchical");

  // Bounds inferred automatically from the enforced binary relation of the
  // model.
  //
  // TODO(user): This is still not as good as the "capacity" bounds below in
  // some cases. Fix! we should be able to use the same relation to infer the
  // capacity bounds somehow.
  if (route_relations_helper_ != nullptr) {
    min_outgoing_flow_helper_.ComputeDimensionBasedMinOutgoingFlow(
        subset, *route_relations_helper_, &best_bound);
  }
  if (subset.size() <
      params_.routing_cut_subset_size_for_tight_binary_relation_bound()) {
    const int bound =
        min_outgoing_flow_helper_.ComputeTightMinOutgoingFlow(subset);
    best_bound.Update(bound, "AutomaticTight");
  } else if (subset.size() <
             params_.routing_cut_subset_size_for_binary_relation_bound()) {
    const int bound = min_outgoing_flow_helper_.ComputeMinOutgoingFlow(subset);
    best_bound.Update(bound, "Automatic");
  }

  if (subset.size() <=
      params_.routing_cut_subset_size_for_exact_binary_relation_bound()) {
    // Note that we only look for violated cuts, but also in order to not try
    // all nodes from the subset, we only look at large enough incoming/outgoing
    // weight.
    //
    // TODO(user): We could easily look for an arc that cannot be used to enter
    // or leave a subset rather than a node. This should lead to tighter bound,
    // and more cuts (that would just ignore that arc rather than all the arcs
    // entering/leaving from a node).
    const double threshold = static_cast<double>(best_bound.bound()) - 1e-4;
    for (const int n : subset) {
      if (nodes_incoming_weight_[n] > 0.1 &&
          total_incoming_weight - nodes_incoming_weight_[n] < threshold &&
          !min_outgoing_flow_helper_.SubsetMightBeServedWithKRoutes(
              best_bound.bound(), subset, nullptr, manager, /*special_node=*/n,
              /*use_forward_direction=*/true)) {
        best_bound.Update(best_bound.bound(), "", {n}, {});
      }
      if (nodes_outgoing_weight_[n] > 0.1 &&
          total_outgoing_weight - nodes_outgoing_weight_[n] < threshold &&
          !min_outgoing_flow_helper_.SubsetMightBeServedWithKRoutes(
              best_bound.bound(), subset, nullptr, manager, /*special_node=*/n,
              /*use_forward_direction=*/false)) {
        best_bound.Update(best_bound.bound(), "", {}, {n});
      }
    }
  }

  if (MaybeAddStrongerFlowCut(manager,
                              absl::StrCat(best_bound.name(), "Lifted"),
                              best_bound.bound(), best_bound.CannotBeFirst(),
                              best_bound.CannotBeLast(), in_subset_)) {
    return true;
  }

  return AddOutgoingCut(manager, best_bound.name(), subset.size(), in_subset_,
                        /*rhs_lower_bound=*/best_bound.bound());
}

bool RoutingCutHelper::TryBlossomSubsetCut(
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
  if (!is_route_constraint_) {
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

void RoutingCutHelper::TryInfeasiblePathCuts(LinearConstraintManager* manager) {
  const int max_path_length = params_.routing_cut_max_infeasible_path_length();
  if (max_path_length <= 0) return;

  // The outgoing arcs of the residual graph, per tail node, except self arcs
  // and arcs to or from the depot.
  std::vector<int> keys;
  std::vector<std::pair<int, double>> values;
  for (const auto [arc_index, lp_value] : relevant_arc_indices_) {
    const int tail = tails_[arc_index];
    const int head = heads_[arc_index];

    // Self arcs are filtered out in InitializeForNewLpSolution().
    DCHECK_NE(tail, head);
    if (tail == 0 || head == 0) continue;
    keys.push_back(tail);
    values.push_back({arc_index, lp_value});
  }
  CompactVectorVector<int, std::pair<int, double>> outgoing_arcs_with_lp_values;
  outgoing_arcs_with_lp_values.ResetFromFlatMapping(keys, values, num_nodes_);

  TopNCuts top_n_cuts(5);
  for (int i = 1; i < num_nodes_; ++i) {
    GenerateCutsForInfeasiblePaths(
        i, max_path_length, outgoing_arcs_with_lp_values, manager, top_n_cuts);
  }
  top_n_cuts.TransferToManager(manager);
}

// Note that it makes little sense to use reduced cost here as the LP solution
// should likely satisfy the current LP optimality equation.
void RoutingCutHelper::GenerateCutsForInfeasiblePaths(
    int start_node, int max_path_length,
    const CompactVectorVector<int, std::pair<int, double>>&
        outgoing_arcs_with_lp_values,
    LinearConstraintManager* manager, TopNCuts& top_n_cuts) {
  // Performs a DFS on the residual graph, starting from the given node. To save
  // stack space, we use a manual stack on the heap instead of using recursion.
  // Each stack element corresponds to a feasible path from `start_node` to
  // `last_node`, included.
  struct State {
    // The last node of the path.
    int last_node;

    // The sum of the lp values of the path's arcs.
    double sum_of_lp_values = 0.0;

    // Variable lower bounds that can be inferred by assuming that the path's
    // arc literals are true (with the binary relation repository).
    absl::flat_hash_map<IntegerVariable, IntegerValue> bounds;

    // The index of the next outgoing arc of `last_node` to explore (in
    // outgoing_arcs_with_lp_values[last_node]).
    int iteration = 0;
  };
  std::stack<State> states;
  // Whether each node is on the current path. The current path is the one
  // corresponding to the top stack element.
  std::vector<bool> path_nodes(num_nodes_, false);
  // The literals corresponding to the current path's arcs.
  std::vector<Literal> path_literals;

  path_nodes[start_node] = true;
  states.push({start_node});
  while (!states.empty()) {
    State& state = states.top();
    DCHECK_EQ(states.size(), path_literals.size() + 1);
    // The number of arcs in the current path.
    const int path_length = static_cast<int>(path_literals.size());
    const auto& outgoing_arcs = outgoing_arcs_with_lp_values[state.last_node];

    bool new_state_pushed = false;
    while (state.iteration < outgoing_arcs.size()) {
      const auto [arc_index, lp_value] = outgoing_arcs[state.iteration++];
      State next_state;
      next_state.last_node = heads_[arc_index];
      next_state.sum_of_lp_values = state.sum_of_lp_values + lp_value;
      next_state.iteration = 0;
      // We only consider simple paths.
      if (path_nodes[next_state.last_node]) continue;
      // If a path of length l is infeasible, we can exclude it by adding a cut
      // saying the sum of its arcs literals must be strictly less than l. But
      // this is only beneficial if the sum of the corresponding LP values is
      // currently greater than l. If it is not, and since it cannot become
      // greater for some extension of the path (the LP sum increases at most by
      // 1 for each arc, but l increases exactly by 1), we can stop the search
      // here.
      // Note: "<= path_length" is equivalent to "< l" (l = path_length + 1).
      if (next_state.sum_of_lp_values <=
          static_cast<double>(path_length) + 1e-4) {
        continue;
      }

      const Literal next_literal = literals_[arc_index];
      next_state.bounds = state.bounds;
      if (PropagateLocalBounds(integer_trail_, root_level_bounds_,
                               binary_relation_repository_, implied_bounds_,
                               next_literal, state.bounds,
                               &next_state.bounds)) {
        // Do not explore "long" paths to keep the search time bounded.
        if (path_length < max_path_length) {
          path_nodes[next_state.last_node] = true;
          path_literals.push_back(literals_[arc_index]);
          states.push(std::move(next_state));
          new_state_pushed = true;
          break;
        }
      } else if (path_length > 0) {
        LinearConstraintBuilder cut_builder(encoder_, 0, path_length);
        for (const Literal literal : path_literals) {
          CHECK(cut_builder.AddLiteralTerm(literal, IntegerValue(1)));
        }
        CHECK(cut_builder.AddLiteralTerm(next_literal, IntegerValue(1)));
        top_n_cuts.AddCut(cut_builder.Build(), "CircuitInfeasiblePath",
                          manager->LpValues());
      }
    }
    if (!new_state_pushed) {
      if (!path_literals.empty()) {
        path_literals.pop_back();
      }
      path_nodes[state.last_node] = false;
      states.pop();
    }
  }
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

// Processes each subsets and add any violated cut.
// Returns the number of added cuts.
int TryAllSubsets(std::string cut_name, absl::Span<const int> subset_data,
                  std::vector<absl::Span<const int>> subsets,
                  RoutingCutHelper& helper, LinearConstraintManager* manager) {
  const int num_nodes = subset_data.size();

  // This exploit the fact that all subsets point into subset_data of size
  // num_nodes. Moreover, if S1 is included in S2, we will always process S1
  // before S2.
  //
  // TODO(user): There is probably a better way than doing a linear scan per
  // subset.
  std::vector<bool> cut_was_added(num_nodes, false);
  std::vector<int> shortest_path_lb(num_nodes, 0);

  int num_added = 0;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    if (subset.size() == num_nodes) continue;

    // we exploit the tree structure of the subsets to not add a cut
    // for a larger subset if we added a cut from one included in it. Also,
    // since the "shortest path" lower bound is valid for any superset, we use
    // it to have a starting lb for the number of vehicles.
    //
    // TODO(user): Currently if we add too many not so relevant cuts, our
    // generic MIP cut heuritic are way too slow on TSP/VRP problems.
    int lb_for_that_subset = 1;
    bool included_cut_was_added = false;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    for (int i = start; i < subset.size(); ++i) {
      lb_for_that_subset = std::max(lb_for_that_subset, shortest_path_lb[i]);
      if (cut_was_added[i]) {
        included_cut_was_added = true;
      }
    }

    // If the subset is small enough and the parameters ask for it, compute
    // a lower bound on the number of vehicle for that subset. This uses
    // "shortest path bounds" and thus that bounds will also be valid for
    // any superset !
    lb_for_that_subset = helper.ShortestPathBound(lb_for_that_subset, subset);
    shortest_path_lb[start] = lb_for_that_subset;

    // Note that we still try the num_nodes - 1 subset cut as that gives
    // directly a lower bound on the number of vehicles.
    if (included_cut_was_added && subset.size() + 1 < num_nodes) continue;
    if (helper.TrySubsetCut(lb_for_that_subset, cut_name, subset, manager)) {
      ++num_added;
      cut_was_added[start] = true;
    }
  }

  return num_added;
}

// We roughly follow the algorithm described in section 6 of "The Traveling
// Salesman Problem, A computational Study", David L. Applegate, Robert E.
// Bixby, Vasek Chvatal, William J. Cook.
//
// Note that this is mainly a "symmetric" case algo, but it does still work for
// the asymmetric case.
void SeparateSubtourInequalities(RoutingCutHelper& helper,
                                 LinearConstraintManager* manager) {
  const int num_nodes = helper.num_nodes();
  if (num_nodes <= 2) return;

  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, helper.ordered_arcs(),
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);

  // For a routing problem, we always try with all nodes but the root as this
  // gives a global lower bound on the number of vehicles. Note that usually
  // the arcs with non-zero lp values should connect everything, but that only
  // happen after many cuts on large problems.
  if (helper.is_route_constraint()) {
    subsets.push_back(absl::MakeSpan(&subset_data[1], num_nodes - 1));
  }

  int num_added =
      TryAllSubsets("Circuit", subset_data, subsets, helper, manager);

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
  num_added +=
      TryAllSubsets("CircuitExact", subset_data, subsets, helper, manager);

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
  int last_added_start = -1;
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
  auto helper = std::make_unique<RoutingCutHelper>(
      num_nodes, /*is_route_constraint=*/false,
      /*flat_node_dim_expressions=*/
      absl::Span<const AffineExpression>{}, tails, heads, literals, model);
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [helper = std::move(helper)](LinearConstraintManager* manager) {
        helper->InitializeForNewLpSolution(manager);
        SeparateSubtourInequalities(*helper, manager);
        return true;
      };
  return result;
}

CutGenerator CreateCVRPCutGenerator(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals,
    absl::Span<const AffineExpression> flat_node_dim_expressions,
    Model* model) {
  auto helper = std::make_unique<RoutingCutHelper>(
      num_nodes, /*is_route_constraint=*/true, flat_node_dim_expressions, tails,
      heads, literals, model);
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [helper = std::move(helper)](LinearConstraintManager* manager) {
        manager->CacheReducedCostsInfo();
        helper->InitializeForNewLpSolution(manager);
        SeparateSubtourInequalities(*helper, manager);
        helper->TryInfeasiblePathCuts(manager);
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
