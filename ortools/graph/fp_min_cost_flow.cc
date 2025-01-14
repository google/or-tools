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

#include "ortools/graph/fp_min_cost_flow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <type_traits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace {

constexpr SimpleMinCostFlow::FlowQuantity kMaxFlowQuantity =
    std::numeric_limits<SimpleMinCostFlow::FlowQuantity>::max();
constexpr SimpleFloatingPointMinCostFlow::FPFlowQuantity kMaxFPFlow =
    std::numeric_limits<SimpleFloatingPointMinCostFlow::FPFlowQuantity>::max();
using ::operations_research::internal::ScaleFlow;

// Returns the scaling value computed from log2_scale.
double Scale(const int log2_scale) { return std::ldexp(1.0, log2_scale); }

// Returns the inverse of the scaling value computed from log2_scale.
double InvScale(const int log2_scale) { return std::ldexp(1.0, -log2_scale); }

// Returns true if the max in-flow or the max out-flow are >= kMaxFlowQuantity.
bool AreInOrOutFlowsOverflowing(const SimpleMinCostFlow& min_cost_flow) {
  const SimpleMinCostFlow::NodeIndex num_nodes = min_cost_flow.NumNodes();
  const SimpleMinCostFlow::ArcIndex num_arcs = min_cost_flow.NumArcs();
  if (num_nodes == 0) {
    return false;
  }
  std::vector<SimpleMinCostFlow::FlowQuantity> max_node_in_flow(num_nodes);
  std::vector<SimpleMinCostFlow::FlowQuantity> max_node_out_flow(num_nodes);
  for (SimpleMinCostFlow::NodeIndex node = 0; node < num_nodes; ++node) {
    SimpleMinCostFlow::FlowQuantity supply = min_cost_flow.Supply(node);
    if (supply < 0) {
      max_node_in_flow[node] = -supply;
    } else {
      max_node_out_flow[node] = supply;
    }
  }
  for (SimpleMinCostFlow::ArcIndex arc = 0; arc < num_arcs; ++arc) {
    const SimpleMinCostFlow::NodeIndex head = min_cost_flow.Head(arc);
    const SimpleMinCostFlow::NodeIndex tail = min_cost_flow.Tail(arc);
    const SimpleMinCostFlow::FlowQuantity capacity =
        min_cost_flow.Capacity(arc);
    static_assert(std::is_same_v<SimpleMinCostFlow::FlowQuantity, int64_t>,
                  "CapAdd() only exists for int64_t");
    max_node_in_flow[head] = CapAdd(max_node_in_flow[head], capacity);
    max_node_out_flow[tail] = CapAdd(max_node_out_flow[tail], capacity);
  }
  // It is safe to assume absl::c_max_element() returns a valid iterator as we
  // exit this function immediately when `num_nodes == 0`.
  const SimpleMinCostFlow::FlowQuantity max_nodes_in_or_out_flow =
      std::max(*absl::c_max_element(max_node_in_flow),
               *absl::c_max_element(max_node_out_flow));
  return max_nodes_in_or_out_flow == kMaxFlowQuantity;
}

}  // namespace

SimpleFloatingPointMinCostFlow::SimpleFloatingPointMinCostFlow(
    const NodeIndex reserve_num_nodes, const ArcIndex reserve_num_arcs)
    : integer_flow_(/*reserve_num_nodes=*/reserve_num_nodes,
                    /*reserve_num_arcs=*/reserve_num_arcs) {
  if (reserve_num_nodes > 0) {
    node_supply_.reserve(reserve_num_nodes);
  }
  if (reserve_num_arcs > 0) {
    arc_capacity_.reserve(reserve_num_arcs);
    arc_flow_.reserve(reserve_num_arcs);
  }
}

SimpleFloatingPointMinCostFlow::ArcIndex
SimpleFloatingPointMinCostFlow::AddArcWithCapacityAndUnitCost(
    const NodeIndex tail, const NodeIndex head, const FPFlowQuantity capacity,
    const CostValue unit_cost) {
  // Add an arc in the integer flow with a temporary capacity of 0. We will
  // update it when SolveMaxFlowWithMinCost() is called.
  const ArcIndex arc = integer_flow_.AddArcWithCapacityAndUnitCost(
      /*tail=*/tail, /*head=*/head,
      /*capacity=*/0, /*unit_cost=*/unit_cost);
  CHECK_EQ(arc, arc_capacity_.size());
  arc_capacity_.push_back(capacity);
  arc_flow_.push_back(0.0);

  // AddArcWithCapacityAndUnitCost() may have added new nodes based on tail and
  // head; we need to take them into account.
  const NodeIndex new_num_nodes = integer_flow_.NumNodes();
  if (new_num_nodes > node_supply_.size()) {
    node_supply_.resize(new_num_nodes);
  }

  return arc;
}

void SimpleFloatingPointMinCostFlow::SetNodeSupply(
    const NodeIndex node, const FPFlowQuantity supply) {
  // Set a capacity on the integer flow.
  integer_flow_.SetNodeSupply(node, 0);

  if (node >= node_supply_.size()) {
    node_supply_.resize(node + 1);
  }
  node_supply_[node] = supply;
}

SimpleFloatingPointMinCostFlow::Status
SimpleFloatingPointMinCostFlow::SolveMaxFlowWithMinCost() {
  if (!ScaleSupplyAndCapacity()) {
    // Resets the previously computed flow.
    absl::c_fill(arc_flow_, 0.0);
    return Status::BAD_CAPACITY_RANGE;
  }

  const Status solve_status = integer_flow_.SolveMaxFlowWithMinCost();
  UpdateFlowFromIntegerFlow(solve_status);
  return solve_status;
}

std::optional<SimpleFloatingPointMinCostFlow::FPFlowQuantity>
SimpleFloatingPointMinCostFlow::ComputeMaxInOrOutFlow() {
  const NodeIndex num_nodes = integer_flow_.NumNodes();
  const ArcIndex num_arcs = integer_flow_.NumArcs();
  CHECK_EQ(num_nodes, node_supply_.size());
  CHECK_EQ(num_arcs, arc_capacity_.size());

  if (num_nodes == 0) {
    return 0.0;
  }

  // Compute the max in-flow and max out-flow for each node.
  std::vector<FPFlowQuantity> max_node_in_flow(num_nodes, 0.0);
  std::vector<FPFlowQuantity> max_node_out_flow(num_nodes, 0.0);
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    const FPFlowQuantity node_supply = node_supply_[node];
    if (!std::isfinite(node_supply)) {
      LOG(ERROR) << "Node " << node << " supply is not finite: " << node_supply;
      return std::nullopt;
    }
    if (node_supply < 0.0) {
      // Negative supply is demand, thus an input.
      max_node_in_flow[node] = -node_supply;
    } else {
      max_node_out_flow[node] = node_supply;
    }
  }
  for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
    const FPFlowQuantity arc_capacity = arc_capacity_[arc];
    if (!std::isfinite(arc_capacity)) {
      LOG(ERROR) << "Arc " << arc
                 << " capacity is not finite: " << arc_capacity;
      return std::nullopt;
    }
    // Negative capacities are considered zero.
    if (arc_capacity <= 0.0) {
      continue;
    }
    max_node_in_flow[integer_flow_.Head(arc)] += arc_capacity_[arc];
    max_node_out_flow[integer_flow_.Tail(arc)] += arc_capacity_[arc];
  }

  // We already exited when num_nodes == 0 so we know that returned iterators
  // are valid.
  return std::max(*absl::c_max_element(max_node_in_flow),
                  *absl::c_max_element(max_node_out_flow));
}

bool SimpleFloatingPointMinCostFlow::ScaleSupplyAndCapacity() {
  const NodeIndex num_nodes = integer_flow_.NumNodes();
  const ArcIndex num_arcs = integer_flow_.NumArcs();
  CHECK_EQ(num_nodes, node_supply_.size());
  CHECK_EQ(num_arcs, arc_capacity_.size());

  // Compute the scaling factor for flows.
  //
  // We choose to use the largest scaling that would not produce an integer
  // overflow when solving integer_flow_.
  //
  // We could use a smaller scaling though, as long as it would not lose any
  // non-zero bits. This would be a bit more complex though. On top of that
  // always using the largest value may help finding overflow bugs even with
  // simple test data.
  //
  // We want to make sure that the resulting integer flow does not produce a
  // BAD_CAPACITY_RANGE. To do so we must make sure that for each node:
  // * the sum of incoming arcs capacities + the max(0, node_supply),
  // * and the sum of outgoing arcs capacities + the max(0, -node_supply)
  // are less (<) than the max value of FlowQuantity.
  //
  // We thus compute these maximum values using floating point computations and
  // use them to compute a scaling factor. Since floating point computations are
  // rounded the end result may not be correct and the integer sum may still
  // overflow. When that is the case we simply divide the scale by 2 and
  // retry. We could do better by changing the rounding mode of floating point
  // computation to FE_UPWARD instead of FE_TONEAREST to get an upper-bound on
  // the sums but this requires the compiler to properly handle changing
  // rounding modes, which is not reliable.
  num_tested_scales_ = 0;  // Always reset.
  if (num_nodes == 0) {
    // If we don't have nodes, we don't have arcs either. Thus we have nothing
    // to scale.
    log2_scale_ = 0;
    return true;
  }

  const std::optional<FPFlowQuantity> max_nodes_in_or_out_flow =
      ComputeMaxInOrOutFlow();
  if (!max_nodes_in_or_out_flow.has_value()) {
    // A LOG(ERROR) already occurred in ComputeMaxInOrOutFlow().
    return false;
  }
  if (!std::isfinite(max_nodes_in_or_out_flow.value())) {
    // Note that here we could scale down the floating point values to make the
    // sum not overflow. But in practice the caller should avoid this situation.
    LOG(ERROR) << "The computed max node in or out flow is not finite: "
               << max_nodes_in_or_out_flow.value();
    return false;
  }

  // Compute the initial scale based on the max in or out flow over all nodes.
  // We want that:
  //
  //   static_cast<SimpleMinCostFlow::FlowQuantity>(
  //     std::round(Scale(log2_scale_) * max_nodes_in_or_out_flow))
  //     < kMaxFlowQuantity
  //
  // To make scaling and unscaling more precise, we will only use power-of-two
  // values for scale_. Thus we compute p such that:
  //
  //   2^p <= kMaxFlowQuantity / max_nodes_in_or_out_flow
  //
  // `f = std::frexp(x, &p)` returns the floating point number of the form `f *
  // 2^e` such that:
  //
  //   2^(e-1) ≤ x < 2^e
  //
  // Thus we can use it to compute a good starting candidate for the scaling
  // factor. Note though that since the division and the computation of
  // max_nodes_in_or_out_flow have been subject to rounding, this starting value
  // may still lead to overflow of the scaled values. We will thus have a loop
  // to try to lower p until we find a value of the scale that works.
  if (max_nodes_in_or_out_flow == 0.0) {
    // When we have no flow on any node, we can simply scale with 2^0 = 1.
    log2_scale_ = 0;
  } else {
    // Note that if max_nodes_in_or_out_flow is a very small number (< 2^-960)
    // then the following division can overflow. If this is the case we simply
    // replace the scale by the max possible value.
    const double scale_upper_bound =
        std::min(std::numeric_limits<double>::max(),
                 static_cast<double>(kMaxFlowQuantity) /
                     max_nodes_in_or_out_flow.value());
    const double f = std::frexp(scale_upper_bound, &log2_scale_);
    // The result of std::frexp() is such that:
    //   2^(p-1) <= scale_upper_bound < 2^p
    // we thus need to decrement the resulting value to get the lower bound.
    //
    // When f == 0.5, this means that actually scale_upper_bound == 2^(p-1). In
    // that case we know that using this value as the scale will lead to an
    // overflow so we simply decrement it by 2 instead.
    log2_scale_ -= f == 0.5 ? 2 : 1;
  }

  // Iterate on values of p until we find one that does not overflow in
  // integers. We use saturated_arithmetic.h and detect the issue when we
  // overflow FlowQuantity.
  //
  // Note that we don't expect to do more than two loops usually. If we reach
  // cases where we need more loops, then maybe we should a binary search
  // approach here. Note that in any case we do a maximum of ~2000 loops (from
  // the highest representable power-of-two to the smallest one).
  const int initial_log2_scale = log2_scale_;
  // We stop when the scale inverse is not representable anymore in a `double`
  // (which occurs when we reach denormal number, a.k.a. very close to zero).
  for (/*empty*/; std::isfinite(InvScale(log2_scale_)); --log2_scale_) {
    const double scale = Scale(log2_scale_);
    ++num_tested_scales_;

    // Sets the integer supply quantities and capacities.
    for (NodeIndex node = 0; node < num_nodes; ++node) {
      integer_flow_.SetNodeSupply(node, ScaleFlow(node_supply_[node], scale));
    }
    for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
      // Note that here it is safe to use std::max() as we have tested above
      // that capacities are not NaN.
      integer_flow_.SetArcCapacity(
          arc, ScaleFlow(std::max(0.0, arc_capacity_[arc]), scale));
    }

    // Test the loop end condition.
    if (!AreInOrOutFlowsOverflowing(integer_flow_)) {
      return true;
    }
    VLOG(1) << "scale = " << RoundTripDoubleFormat(scale) << " (i.e. 2^"
            << log2_scale_
            << ") lead to an integer overflow; decrementing log2_scale_ and "
               "trying again";
  }
  // It may not be possible to reach this code. If we ever do, we still want
  // to treat this as an error.
  LOG(ERROR) << "Failed to compute a positive scale that works; started with "
                "log2_scale_ = "
             << initial_log2_scale
             << " and stopped at log2_scale_ = " << log2_scale_
             << " with scale_ = " << RoundTripDoubleFormat(Scale(log2_scale_))
             << " 1.0/scale_ = "
             << RoundTripDoubleFormat(InvScale(log2_scale_));
  return false;
}

void SimpleFloatingPointMinCostFlow::UpdateFlowFromIntegerFlow(
    const Status solve_status) {
  switch (solve_status) {
    case Status::OPTIMAL:
    case Status::FEASIBLE: {
      CHECK_EQ(integer_flow_.NumArcs(), arc_flow_.size());
      // ScaleSupplyAndCapacity() only selects log2_scale_ values for which
      // InvScale() is finite.
      const double inv_scale = InvScale(log2_scale_);
      for (ArcIndex arc = 0; arc < integer_flow_.NumArcs(); ++arc) {
        arc_flow_[arc] = inv_scale * integer_flow_.Flow(arc);
      }
      break;
    }
    default:
      // SimpleMinCostFlow::arc_flow_ is usually not set in errors cases so we
      // simply reset the flow.
      absl::c_fill(arc_flow_, 0.0);
      break;
  }
}

SimpleFloatingPointMinCostFlow::SolveStats
SimpleFloatingPointMinCostFlow::LastSolveStats() const {
  return {
      .scale = Scale(log2_scale_),
      .num_tested_scales = num_tested_scales_,
  };
}

std::ostream& operator<<(
    std::ostream& out,
    const SimpleFloatingPointMinCostFlow::SolveStats& stats) {
  out << "{ scale: " << RoundTripDoubleFormat(stats.scale)
      << ", num_tested_scales: " << stats.num_tested_scales << " }";
  return out;
}

}  // namespace operations_research
