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

#ifndef OR_TOOLS_GRAPH_FP_MIN_COST_FLOW_H_
#define OR_TOOLS_GRAPH_FP_MIN_COST_FLOW_H_

#include <cmath>
#include <limits>
#include <optional>
#include <ostream>
#include <vector>

#include "absl/log/check.h"
#include "ortools/graph/min_cost_flow.h"

namespace operations_research {

// An implementation of an approximate min-cost-max-flow algorithm supporting
// floating point numbers for flow capacities.
//
// It uses the integer algorithm of SimpleMinCostFlow by scaling and rounding
// floating point supply quantities and capacities to make them fit on
// integers. This can be seen as using fixed-point arithmetic.
//
// Note that this only supports min-cost-max-flow and not min-cost-flow. The
// reason is that with floating point numbers it is harder to define that all
// demand and supply are met. This would require some tolerances and testing
// those tolerances would require solving the max-flow anyway.
class SimpleFloatingPointMinCostFlow {
 public:
  using NodeIndex = SimpleMinCostFlow::NodeIndex;
  using ArcIndex = SimpleMinCostFlow::ArcIndex;
  using CostValue = SimpleMinCostFlow::CostValue;
  using FPFlowQuantity = double;
  using Status = SimpleMinCostFlow::Status;

  // Statistics associated with a call to SolveMaxFlowWithMinCost().
  //
  // Returned by LastSolveStats().
  struct SolveStats {
    // The scaling factor used to convert the FPFlowQuantity to a
    // SimpleMinCostFlow::FlowQuantity.
    double scale = 1.0;

    // The number of values tested for the scaling factor.
    //
    // Internally SolveMaxFlowWithMinCost() will compute a first scaling factor
    // with floating point arithmetic. Due to the approximate nature of this
    // computation, it may still be too high. If the resulting integer numbers
    // lead to integer overflows, a new lower scaling factor is computing.
    int num_tested_scales = 0;

    // Prints all the stats values on a single line.
    friend std::ostream& operator<<(std::ostream& out, const SolveStats& stats);
  };

  explicit SimpleFloatingPointMinCostFlow(NodeIndex reserve_num_nodes = 0,
                                          ArcIndex reserve_num_arcs = 0);

  // This type is neither copyable nor movable.
  SimpleFloatingPointMinCostFlow(const SimpleFloatingPointMinCostFlow&) =
      delete;
  SimpleFloatingPointMinCostFlow& operator=(
      const SimpleFloatingPointMinCostFlow&) = delete;

  // Adds a directed arc from tail to head to the underlying graph with
  // a given capacity and cost per unit of flow.
  // * Node indices must be non-negative (>= 0).
  // * The capacity must be finite. When not, SolveMaxFlowWithMinCost() returns
  //   BAD_CAPACITY_RANGE. Negative values are OK and will be considered zero
  //   (which is useful when computed values are close to zero but negative).
  // * The unit cost can take any integer value (even negative).
  // * Self-looping and duplicate arcs are supported.
  // * After the method finishes, NumArcs() == the returned ArcIndex + 1.
  ArcIndex AddArcWithCapacityAndUnitCost(NodeIndex tail, NodeIndex head,
                                         FPFlowQuantity capacity,
                                         CostValue unit_cost);

  // Sets the supply of the given node. The node index must be non-negative (>=
  // 0). Nodes implicitly created will have a default supply set to 0. A demand
  // is modeled as a negative supply.
  //
  // The supply quantity must be finite. When not, SolveMaxFlowWithMinCost()
  // returns BAD_CAPACITY_RANGE.
  void SetNodeSupply(NodeIndex node, FPFlowQuantity supply);

  // Computes a maximum-flow with minimum cost.
  //
  // This function returns the Status of the underlying
  // SimpleMinCostFlow::SolveMaxFlowWithMinCost().
  //
  // It also returns BAD_CAPACITY_RANGE:
  // * either when the arc capacities or node supply quantities are NaN or
  //   infinite,
  // * or when the computed in-flow or out-flow of a node results in an infinite
  //   value,
  // * or if it failed to find a scale factor to make capacities and supply
  //   quantities fit in integers.
  // In case of failure, a LOG(ERROR) should contain the explanation.
  Status SolveMaxFlowWithMinCost();

  // Returns the flow on arc, this only make sense for a successful
  // SolveMaxFlowWithMinCost().
  //
  // If called before SolveMaxFlowWithMinCost(), it will return 0.0.
  //
  // Note: It is possible that there is more than one optimal solution. The
  // algorithm is deterministic so it will always return the same solution for
  // a given problem. However, there is no guarantee of this from one code
  // version to the next (but the code does not change often).
  FPFlowQuantity Flow(ArcIndex arc) const { return arc_flow_[arc]; }

  // Returns the statistics of the last call to SolveMaxFlowWithMinCost().
  SolveStats LastSolveStats() const;

  // Accessors for the user given data. The implementation will crash if "arc"
  // is not in [0, NumArcs()) or "node" is not in [0, NumNodes()).
  NodeIndex NumNodes() const { return integer_flow_.NumNodes(); }
  ArcIndex NumArcs() const { return integer_flow_.NumArcs(); }
  NodeIndex Tail(ArcIndex arc) const { return integer_flow_.Tail(arc); }
  NodeIndex Head(ArcIndex arc) const { return integer_flow_.Head(arc); }
  FPFlowQuantity Capacity(ArcIndex arc) const { return arc_capacity_[arc]; }
  FPFlowQuantity Supply(NodeIndex node) const { return node_supply_[node]; }
  CostValue UnitCost(ArcIndex arc) const { return integer_flow_.UnitCost(arc); }

 private:
  // Returns the max value all in-flows or out-flows of each nodes. If some
  // nodes or arcs have non-finite supply or capacities, returns std::nullopt
  // after the rationale has been printed to LOG(ERROR).
  //
  // Precisely, returns max(max(in_flow(n) ∀ n), max(out_flow(n) ∀ n)). Returns
  // 0.0 when there are no nodes.
  std::optional<FPFlowQuantity> ComputeMaxInOrOutFlow();

  // Sets the integer supply and capacity values on integer_flow_ from
  // node_supply_ and arc_capacity_ floating point values after computing the
  // log2_scale_. It will also updates num_tested_scales_.
  //
  // The integer quantities are computed by:
  //
  //   static_cast<FlowQuantity>(
  //     std::round(Scale(log2_scale_) * fp_flow_quantity));
  //
  // Returns true in case of success, false otherwise (after the rational was
  // printed to LOG(ERROR)).
  bool ScaleSupplyAndCapacity();

  // Updates arc_flow_ using the values of integer_flow_ and the status of the
  // solve.
  void UpdateFlowFromIntegerFlow(Status solve_status);

  // The underlying integer min-cost-max-flow solver.
  SimpleMinCostFlow integer_flow_;

  // The log2 of the scale applied to FPFlowQuantity values to get the integer
  // FlowQuantity ones in integer_flow_. When ScaleSupplyAndCapacity() succeeds
  // this will contain a value for which both Scale() and InvScale() are finite
  // and non-zero.
  //
  // See Scale() and InvScale().
  int log2_scale_ = 0;

  // The number of values of scale_ tested during the clal to
  // ScaleSupplyAndCapacity().
  int num_tested_scales_ = 0;

  // Invariant on the following vectors: their size matches
  // integer_flow_.NumNodes() or integer_flow_.NumArcs().
  std::vector<FPFlowQuantity> arc_capacity_;
  std::vector<FPFlowQuantity> node_supply_;
  std::vector<FPFlowQuantity> arc_flow_;
};

// This namespace contains internal functions only there for tests.
namespace internal {

// Scale the input floating point flow to the integer flow, clamping the
// result to [-kMaxFlowQuantity, kMaxFlowQuantity].
//
// The scale and fp_flow must be finite (CHECKed).
//
// This function is internal. It is only exposed since by construction the input
// fp_flow and scale of ScaleFlow() should never trigger the code paths that
// lead to overflows. But we still want to make sure that if they are ever
// triggered they work as expected.
inline SimpleMinCostFlow::FlowQuantity ScaleFlow(
    const SimpleFloatingPointMinCostFlow::FPFlowQuantity fp_flow,
    const double scale) {
  CHECK(std::isfinite(scale)) << scale;
  CHECK(std::isfinite(fp_flow)) << fp_flow;
  const double rounded_scaled_flow = std::round(scale * fp_flow);
  // Note that we compare to kMaxFlowQuantity with >= and not > as:
  // * the comparison will convert kMaxFlowQuantity to `double` first (see
  //   https://en.cppreference.com/w/cpp/language/usual_arithmetic_conversions),
  // * and kMaxFlowQuantity (2^63 - 1) is not exactly representable in a
  //   `double` (that only has 53 bits of mantissa),
  // * thus it will be rounded to the nearest `double`, i.e. 2^64,
  // * so if we were to compare with > only, we would not reject the `double`
  //   2^64 that can't fit in an int64_t; comparing with >= will reject this
  //   number and only accept `double` that are less or equal to the predecessor
  //   of 2^64, which all fit in an int64_t.
  constexpr SimpleMinCostFlow::FlowQuantity kMaxFlowQuantity =
      std::numeric_limits<SimpleMinCostFlow::FlowQuantity>::max();
  if (rounded_scaled_flow >= kMaxFlowQuantity) {
    return kMaxFlowQuantity;
  }
  if (rounded_scaled_flow <= -kMaxFlowQuantity) {
    return -kMaxFlowQuantity;
  }
  return static_cast<SimpleMinCostFlow::FlowQuantity>(rounded_scaled_flow);
}

}  // namespace internal
}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_FP_MIN_COST_FLOW_H_
