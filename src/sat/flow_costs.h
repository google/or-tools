// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_SAT_FLOW_COSTS_H_
#define OR_TOOLS_SAT_FLOW_COSTS_H_

#include <vector>

#include "linear_solver/linear_solver.h"
#include "sat/integer.h"
#include "sat/model.h"
#include "sat/sat_base.h"

namespace operations_research {
namespace sat {

// Propagator for the flow-cost constraint, see the doc of FlowCostsConstraint()
// for a description. This use the LP relaxation to propagates bounds on the
// demands and possible flows.
// Inspired by Downing, Feydy & Stuckey "Explaining Flow-Based Propagation".
// https://doi.org/10.1007/978-3-642-29828-8_10
// The Reduced Cost Strengthening technique is a classic in mixed integer
// programming, for instance see the thesis of Tobias Achterberg,
// "Constraint Integer Programming", sections 7.7 and 8.8, algorithm 7.11.
// http://nbn-resolving.de/urn:nbn:de:0297-zib-11129
class FlowCosts : public PropagatorInterface {
 public:
  FlowCosts(const std::vector<IntegerVariable>& demands,
            const std::vector<IntegerVariable>& flow,
            const std::vector<int>& tails, const std::vector<int>& heads,
            const std::vector<std::vector<int>>& arc_costs_per_cost_type,
            const std::vector<IntegerVariable>& costs,
            IntegerTrail* integer_trail);

  bool Propagate() override;

  bool IncrementalPropagate(const std::vector<int>& watch_indices) override;

  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // Fills the deductions vector with reduced cost deductions
  // that can be made from the current state of the LP solver.
  // This should be called after Solve():
  // if the optimization was a minimization, the direction variable should be
  // 1.0 and objective_delta the objective's upper bound minus the optimal;
  // if the optimization was a maximization, direction should be -1.0
  // and objective_delta the optimal minus the objective's lower bound.
  void ReducedCostStrengtheningDeductions(double direction,
                                          double objective_delta);

  // Fills the integer_reason_ vector with literals that explain the current
  // state of the LP solver, i.e. why it failed or why the optimal value cannot
  // be better. Parameter direction should be 1.0 or -1.0,
  // see ReducedCostStrengtheningDeductions().
  void FillIntegerReason(double direction);

  std::vector<IntegerLiteral> integer_reason_;
  std::vector<IntegerLiteral> deductions_;

  const int num_nodes_;
  const int num_arcs_;
  const int num_costs_;
  const int num_vars_;
  static const double kEpsilon;

  // CP variables representing the flow cost.
  std::vector<IntegerVariable> demands_cp_;
  std::vector<IntegerVariable> flow_cp_;
  std::vector<IntegerVariable> total_costs_per_cost_type_cp_;

  IntegerTrail* integer_trail_;

  // Underlying LP solver and variables that mirror the CP variables.
  MPSolver lp_solver_;
  MPObjective* objective_lp_;
  std::vector<MPVariable*> demands_lp_;
  std::vector<MPVariable*> flow_lp_;
  std::vector<MPVariable*> total_costs_per_cost_type_lp_;
  MPVariable* violation_;

  // The propagator treats all variables the same way, thus we aggregate
  // CP variables and LP variables to copy state from CP to LP easily,
  // generate explanations, etc...
  std::vector<IntegerVariable> all_cp_variables_;
  std::vector<MPVariable*> all_lp_variables_;

  // Current solution of the LP problem,
  // that supports the existence of a CP solution.
  // This is used to prevent fruitless LP solve():
  // as long as a CP bound change leaves the LP solution within bounds,
  // the propagator will not find failure or deduce anything new.
  std::vector<double> lp_solution_;
};

// Given a graph whose arcs are given by tails/heads,
// enforces that flow variables model a flow respecting demands,
// and several costs reflecting flow usage.
//
// The input must be as follows:
// - demands must be a vector of num_nodes_ IntegerVariable,
// - flow must be a vector of num_arcs_ IntegerVariable,
// - tails and heads must be vectors of num_arcs_ ints within [0, num_nodes_),
// - arc_costs_per_cost_type must be a num_costs_ * num_arcs_ ints matrix.
// - total_costs_per_cost_type must be a vector of num_costs_ IntegerVariables,
// Note that all IntegerVariables can have arbitrary domain.
//
// Formally, the constraint enforces flow conservation:
// forall node, demands[node] - sum_{arc = node->_} flow[arc]
//                            + sum_{arc = _->node} flow[arc] = 0
//
// It also enforces the costs for every cost type: forall c,
// costs[c] = sum_arc arc_cost[c][arc] * flow[arc].
std::function<void(Model*)> FlowCostsConstraint(
    const std::vector<IntegerVariable>& demands,
    const std::vector<IntegerVariable>& flow, const std::vector<int>& tails,
    const std::vector<int>& heads,
    const std::vector<std::vector<int>>& arc_costs_per_cost_type,
    const std::vector<IntegerVariable>& total_costs_per_cost_type);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_FLOW_COSTS_H_
