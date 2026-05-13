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

#ifndef OR_TOOLS_LINEAR_SOLVER_SAMPLES_NETWORK_DESIGN_ILPH_H_
#define OR_TOOLS_LINEAR_SOLVER_SAMPLES_NETWORK_DESIGN_ILPH_H_

#include <memory>
#include <vector>

#include "ortools/graph/graph.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/samples/network_design_ilph.h"
#include "ortools/routing/parsers/capacity_planning.pb.h"

// An implementation of the algorithm described in "An Efficient Matheuristic
// for the Multicommodity Fixed-Charge Network Design Problem", Gendron et al.
// IFAC 49-12, 2016.
// https://www.sciencedirect.com/science/article/pii/S2405896316308175

namespace operations_research {

// Representation of a Capacity Planning problem using a graph data structure.
struct CapacityPlanningProblem {
  std::vector<double> variable_costs;  // Variable cost per arc.
  std::vector<double> capacities;      // Capacity per arc.
  std::vector<double> fixed_costs;     // Fixed cost per arc.
  // The below representation of the problem demands is a bit redundant but
  // enables smaller and cleaner code when setting up the problem as a MIP.
  // TODO(user): switch to a hash_map in the unlikely case this becomes a memory
  // consumption problem.
  std::vector<std::vector<double>>
      demands_at_node_per_commodity;  // Demand (if < 0, or supply if > 0) at
                                      // node per commodity.
  std::vector<double> demands_per_commodity;  // Demand per commodity, without
                                              // the indexing per node. In this
                                              // case, the value is always > 0.
  util::ReverseArcListGraph<> graph;  // The network on which the optimization
                                      // has to be performed.
  int num_commodities;
};

// Converts a CapacityPlanningInstance to a CapacityPlanningProblem, which is
// easier to use for modeling using MIPs.
::absl::Status Convert(const CapacityPlanningInstance& request,
                       CapacityPlanningProblem* problem);

struct CapacityPlanningParameters {
  absl::Duration time_limit;
};

class CapacityPlanningMipModel {
 public:
  CapacityPlanningMipModel() = default;
  void Build(const CapacityPlanningProblem& problem, bool relax_integrality);
  MPSolver::ResultStatus Solve(CapacityPlanningParameters parameters);
  void ExportModelToFile(absl::string_view filename) const;
  // Flow variables per arc per commodity.
  std::vector<std::vector<MPVariable*>> GetFlowVariables() const {
    return flow_;
  }
  // Decision (binary) variables to open an arc or not. The binary aspect can be
  // relaxed if relax_integrality is passed as true in Build().
  std::vector<MPVariable*> GetOpeningVariables() const { return open_; }
  int num_arcs() const { return num_arcs_; }
  int num_nodes() const { return num_nodes_; }
  int num_commodities() const { return num_commodities_; }
  MPSolver* solver() const { return solver_.get(); }

 private:
  std::unique_ptr<MPSolver> solver_;
  // flow_[arc][commodity] represents the flow of `commodity` on `arc`.
  std::vector<std::vector<MPVariable*>> flow_;
  // open_[arc] represents the decision to open `arc` or not. It is a Boolean
  // variable, which can be relaxed if relax_integrity is passed as true to
  // StateProblem().
  std::vector<MPVariable*> open_;

  int num_arcs_;
  int num_nodes_;
  int num_commodities_;
};

enum CapacityPlanningStatus {
  INVALID_INPUT = 0,
  PROCESSING = 1,
  SOLUTION_COMPUTED = 2,
  OPTIMAL_SOLUTION_COMPUTED = 3
};

class CapacityPlanningILPH {
 public:
  CapacityPlanningILPH() : best_cost_(MPSolver::infinity()) {}
  void Build(const CapacityPlanningProblem& problem);
  MPSolver::ResultStatus Solve();
  std::vector<std::vector<MPVariable*>> GetFlowVariables() const {
    return mip_restricted_model_.GetFlowVariables();
  }
  std::vector<MPVariable*> GetOpeningVariables() const {
    return mip_restricted_model_.GetOpeningVariables();
  }
  int num_arcs() const { return mip_restricted_model_.num_arcs(); }
  int num_nodes() const { return mip_restricted_model_.num_nodes(); }
  int num_commodities() const {
    return mip_restricted_model_.num_commodities();
  }

  double best_cost() const { return best_cost_; }

 private:
  // We maintain two different models for the same problem:
  // 1) An LP relaxation (Problem Q in the paper referenced above), to which
  // pseudo-cuts will be added. This problem only becomes more constrained at
  // each iteration.
  CapacityPlanningMipModel lp_relaxation_model_;
  CapacityPlanningParameters lp_relaxation_params_;

  // 2) A MIP model (Problem P in the paper), for which the variables that
  // are 0 or 1 in the solution of lp_relaxation_model are fixed, thus reducing
  // the complexity of solving the MIP problem. Note that this problem changes
  // at each iteration.
  CapacityPlanningMipModel mip_restricted_model_;
  CapacityPlanningParameters mip_restricted_params_;

  double best_cost_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_SAMPLES_NETWORK_DESIGN_ILPH_H_
