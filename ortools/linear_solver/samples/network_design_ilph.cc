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

#include "ortools/linear_solver/samples/network_design_ilph.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/graph/graph.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/file_util.h"

namespace operations_research {

::absl::Status Convert(const routing::CapacityPlanningInstance& request,
                       CapacityPlanningProblem* problem) {
  const routing::NetworkTopology& topology = request.topology();
  const int num_arcs = topology.from_node_size();
  CHECK_EQ(num_arcs, topology.to_node_size());
  CHECK_EQ(num_arcs, topology.variable_cost_size());
  CHECK_EQ(num_arcs, topology.fixed_cost_size());
  CHECK_EQ(num_arcs, topology.capacity_size());
  problem->variable_costs.resize(num_arcs, 0);
  problem->capacities.resize(num_arcs, 0);
  problem->fixed_costs.resize(num_arcs, 0);
  for (int arc = 0; arc < num_arcs; ++arc) {
    problem->graph.AddArc(topology.from_node(arc), topology.to_node(arc));
    problem->variable_costs[arc] = topology.variable_cost(arc);
    problem->capacities[arc] = topology.capacity(arc);
    problem->fixed_costs[arc] = topology.fixed_cost(arc);
  }
  const int num_nodes = problem->graph.num_nodes();
  problem->demands_at_node_per_commodity.resize(num_nodes);
  const routing::Commodities& commodities = request.commodities();
  const int num_commodities = commodities.from_node_size();
  problem->num_commodities = num_commodities;
  CHECK_EQ(commodities.to_node_size(), num_commodities);
  CHECK_EQ(commodities.demand_size(), num_commodities);
  for (int node = 0; node < num_nodes; ++node) {
    problem->demands_at_node_per_commodity[node].resize(num_commodities, 0);
  }
  problem->demands_per_commodity.resize(num_commodities, 0);
  for (int commodity = 0; commodity < num_commodities; ++commodity) {
    const double demand = commodities.demand(commodity);
    CHECK_GT(demand, 0);
    const int from_node = commodities.from_node(commodity);
    CHECK_LT(from_node, num_nodes);
    const int to_node = commodities.to_node(commodity);
    CHECK_LT(to_node, num_nodes);
    problem->demands_at_node_per_commodity[from_node][commodity] = demand;
    problem->demands_at_node_per_commodity[to_node][commodity] = -demand;
    problem->demands_per_commodity[commodity] = demand;
  }
  return absl::OkStatus();
}

MPSolver::ResultStatus CapacityPlanningMipModel::Solve(
    CapacityPlanningParameters parameters) {
  solver_->SetTimeLimit(parameters.time_limit);
  return solver_->Solve();
}

void CapacityPlanningMipModel::Build(const CapacityPlanningProblem& problem,
                                     bool relax_integrality) {
  solver_ = std::make_unique<MPSolver>(
      "Capacity planning solver", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  // flow_[arc][commodity] represents the flow of `commodity` on `arc`.
  num_arcs_ = problem.graph.num_arcs();
  num_nodes_ = problem.graph.num_nodes();
  num_commodities_ = problem.num_commodities;
  flow_.resize(num_arcs_);
  for (int arc = 0; arc < num_arcs_; ++arc) {
    flow_[arc].resize(num_commodities_);
    for (int commodity = 0; commodity < num_commodities_; ++commodity) {
      flow_[arc][commodity] = solver_->MakeNumVar(
          0, MPSolver::infinity(), absl::StrCat("flow_", arc, "_", commodity));
    }
  }

  // open_[arc] represents the decision to open `arc` or not. It is a
  // Boolean variable.
  open_.resize(num_arcs_);
  for (int arc = 0; arc < num_arcs_; ++arc) {
    if (relax_integrality) {
      open_[arc] = solver_->MakeNumVar(0, 1, absl::StrCat("open_", arc));
    } else {
      open_[arc] = solver_->MakeIntVar(0, 1, absl::StrCat("open_", arc));
    }
  }

  // flow_[arc][commodity] <= min(demand[commodity], capacities[arc]) *
  // open_[arc]
  for (int arc = 0; arc < num_arcs_; ++arc) {
    for (int commodity = 0; commodity < num_commodities_; ++commodity) {
      MPConstraint* const bounding_constraint_on_arc =
          solver_->MakeRowConstraint(
              -MPSolver::infinity(), 0.0,
              absl::StrCat("bounding_on_arc_", arc, "_commodity_", commodity));
      bounding_constraint_on_arc->SetCoefficient(flow_[arc][commodity], 1.0);
      bounding_constraint_on_arc->SetCoefficient(
          open_[arc], -std::min(problem.capacities[arc],
                                problem.demands_per_commodity[commodity]));
    }
  }

  // Flow conservation constraints:
  // For each commodity, the sum of the flows over outgoing arcs, minus the
  // sum of the flows over incoming arcs is equal to
  // demands_at_node_per_commodity[node][commodity].
  for (int commodity = 0; commodity < num_commodities_; ++commodity) {
    for (int node = 0; node < problem.graph.num_nodes(); ++node) {
      const int64_t in_flow =
          problem.demands_at_node_per_commodity[node][commodity];
      VLOG(1) << "Supply for commodity " << commodity << " at node " << node
              << " is " << in_flow;
      MPConstraint* const flow_conservation_constraint =
          solver_->MakeRowConstraint(
              in_flow, in_flow,
              absl::StrCat("flow_conservation_", node, "_", commodity));
      for (const int arc : problem.graph.OutgoingArcs(node)) {
        flow_conservation_constraint->SetCoefficient(flow_[arc][commodity],
                                                     1.0);
      }
      for (const int arc : problem.graph.IncomingArcs(node)) {
        flow_conservation_constraint->SetCoefficient(flow_[arc][commodity],
                                                     -1.0);
      }
    }
  }
  // For all arcs: sum(flow_[arc][commodity]) <=
  //                   problem.capacities[arc] * open_[arc].
  for (int arc = 0; arc < num_arcs_; ++arc) {
    MPConstraint* const capacity_constraint = solver_->MakeRowConstraint(
        -MPSolver::infinity(), 0.0, absl::StrCat("capacity_", arc));
    capacity_constraint->SetCoefficient(open_[arc], -problem.capacities[arc]);
    for (int commodity = 0; commodity < num_commodities_; ++commodity) {
      capacity_constraint->SetCoefficient(flow_[arc][commodity], 1.0);
    }
  }
  MPObjective* const objective = solver_->MutableObjective();
  objective->SetMinimization();
  for (int arc = 0; arc < num_arcs_; ++arc) {
    for (int commodity = 0; commodity < num_commodities_; ++commodity) {
      objective->SetCoefficient(flow_[arc][commodity],
                                problem.variable_costs[arc]);
    }
    objective->SetCoefficient(open_[arc], problem.fixed_costs[arc]);
  }
}

void CapacityPlanningMipModel::ExportModelToFile(
    absl::string_view filename) const {
  MPModelProto exported_model;
  solver_->ExportModelToProto(&exported_model);
  const absl::Status status =
      WriteProtoToFile(filename, exported_model, ProtoWriteFormat::kProtoText,
                       /*gzipped=*/false);
  LOG_IF(ERROR, !status.ok()) << status;
}

void CapacityPlanningILPH::Build(const CapacityPlanningProblem& problem) {
  lp_relaxation_model_.Build(problem, /*relax_integrality=*/true);
  mip_restricted_model_.Build(problem, /*relax_integrality=*/false);
}

MPSolver::ResultStatus CapacityPlanningILPH::CapacityPlanningILPH::Solve() {
  best_cost_ = MPSolver::infinity();
  // TODO(user): manage time more finely, by giving a total time to run, and do
  // not fix a number of iterations.
  mip_restricted_params_.time_limit = absl::Minutes(5);
  lp_relaxation_params_.time_limit = absl::Minutes(5);
  lp_relaxation_model_.solver()->SetTimeLimit(lp_relaxation_params_.time_limit);
  mip_restricted_model_.solver()->SetTimeLimit(
      mip_restricted_params_.time_limit);
  const int kNumIterations = 10;
  for (int iter = 0; iter < kNumIterations; ++iter) {
    LOG(INFO) << "Iteration # " << iter;
    MPSolver::ResultStatus status =
        lp_relaxation_model_.Solve(lp_relaxation_params_);
    if (status != MPSolver::OPTIMAL) return status;
    std::vector<double> open_values(lp_relaxation_model_.num_arcs(), 0.0);
    const double linear_relaxation_cost =
        lp_relaxation_model_.solver()->Objective().Value();
    // Get the values for the opening variables before we modify the LP
    // relaxation.
    for (int arc = 0; arc < lp_relaxation_model_.num_arcs(); ++arc) {
      open_values[arc] =
          lp_relaxation_model_.GetOpeningVariables()[arc]->solution_value();
    }
    // Create the pseudo-cut for the LP-relaxation model. First define J
    // as the set where the opening variables have value y* = 0 or 1 in
    // the linear relaxation. We then want a solution that is different
    // from the current solution: Sum over J |y - y*| >= 1, where y
    // denotes the opening variables.
    MPConstraint* pseudo_cut = lp_relaxation_model_.solver()->MakeRowConstraint(
        -MPSolver::infinity(), MPSolver::infinity(), "pseudo_cut");
    double pseudo_cut_lb = 1.0;
    int num_fixed_vars = 0;
    for (int arc = 0; arc < lp_relaxation_model_.num_arcs(); ++arc) {
      const double y = open_values[arc];
      const double rounded_y = std::round(y);
      // Is arc a member of J? If so, round it and fix it in the MIP relaxation
      // model.
      if (std::abs(y - rounded_y) < 1e-5) {
        ++num_fixed_vars;
        mip_restricted_model_.GetOpeningVariables()[arc]->SetBounds(rounded_y,
                                                                    rounded_y);
        // Compute the coefficient of the pseudo-cut.
        // Add |y - y*| to the pseudo cut.
        // When y* == 0, |y - y*| == y, so just add y with coefficient 1.0.
        // When y* == 1, |y - y*| == 1 - y, so add y with coefficient -1.0,
        // and decrease the lower bound by 1.
        const double coefficient = rounded_y == 0.0 ? 1.0 : -1.0;
        pseudo_cut_lb -= rounded_y == 0.0 ? 0.0 : 1.0;
        pseudo_cut->SetCoefficient(
            lp_relaxation_model_.GetOpeningVariables()[arc], coefficient);
      }
    }
    LOG(INFO) << "LP relaxation cost = " << linear_relaxation_cost;
    pseudo_cut->SetLB(pseudo_cut_lb);
    LOG(INFO) << "Pseudo cut added. " << num_fixed_vars << " out of "
              << lp_relaxation_model_.num_arcs() << " variables fixed.";
    LOG(INFO) << "Solving MIP.";
    // Solve the reduced problem P(y, J)
    if (mip_restricted_model_.solver()->Solve() == MPSolver::INFEASIBLE) break;

    for (int arc = 0; arc < mip_restricted_model_.num_arcs(); ++arc) {
      VLOG(1)
          << "y[" << arc << "] = "
          << mip_restricted_model_.GetOpeningVariables()[arc]->solution_value();
    }
    const double mip_relaxation_cost =
        mip_restricted_model_.solver()->Objective().Value();
    best_cost_ = std::min(best_cost_, mip_relaxation_cost);
    LOG(INFO) << "MIP relaxation objective = " << mip_relaxation_cost
              << ", best cost = " << best_cost_;
    // Relax the bounds on the MIP relaxation P.
    for (int arc = 0; arc < mip_restricted_model_.num_arcs(); ++arc) {
      mip_restricted_model_.GetOpeningVariables()[arc]->SetBounds(0, 1);
    }
  }
  LOG(INFO) << "Best cost = " << best_cost_;
  return MPSolver::FEASIBLE;
}

}  // namespace operations_research
