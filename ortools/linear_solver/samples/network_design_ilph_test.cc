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

#include "absl/base/log_severity.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/path.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/routing/parsers/capacity_planning.pb.h"
#include "ortools/routing/parsers/dow_parser.h"

namespace operations_research {
namespace {

TEST(ProtoToProblemTest, TransformC33) {
  CapacityPlanningInstance request;
  ::absl::Status status =
      ReadFile(file::JoinPathRespectAbsolute(
                   ::testing::SrcDir(), "operations_research_data/",
                   "MULTICOM_FIXED_CHARGE_NETWORK_DESIGN/C/c33.dow"),
               &request);
  EXPECT_OK(status);
  CapacityPlanningProblem problem;
  status = Convert(request, &problem);
  EXPECT_OK(status);
  EXPECT_EQ(problem.graph.num_nodes(), 21);
  EXPECT_EQ(problem.graph.num_arcs(), 228);
  EXPECT_EQ(problem.num_commodities, 39);
  EXPECT_EQ(problem.demands_per_commodity.size(), problem.num_commodities);
  EXPECT_EQ(problem.demands_at_node_per_commodity.size(),
            problem.graph.num_nodes());
  EXPECT_EQ(problem.demands_at_node_per_commodity[0].size(),
            problem.num_commodities);
  int num_arcs = problem.graph.num_arcs();
  const NetworkTopology& topology = request.topology();
  for (int arc = 0; arc < num_arcs; ++arc) {
    problem.graph.AddArc(topology.from_node(arc), topology.to_node(arc));
    EXPECT_EQ(problem.variable_costs[arc], topology.variable_cost(arc));
    EXPECT_EQ(problem.capacities[arc], topology.capacity(arc));
    EXPECT_EQ(problem.fixed_costs[arc], topology.fixed_cost(arc));
  }
  const Commodities& commodities = request.commodities();
  const int num_commodities = commodities.from_node_size();
  EXPECT_EQ(problem.num_commodities, num_commodities);
  EXPECT_EQ(problem.demands_per_commodity.size(), num_commodities);
  EXPECT_EQ(problem.demands_per_commodity[0], 216);
  EXPECT_EQ(problem.demands_at_node_per_commodity.size(),
            problem.graph.num_nodes());
  EXPECT_EQ(problem.demands_at_node_per_commodity[0].size(),
            problem.num_commodities);
  EXPECT_EQ(problem.demands_at_node_per_commodity[18][0], 216);
  EXPECT_EQ(problem.demands_at_node_per_commodity[6][0], -216);
}

TEST(ProtoToProblemTest, TransformEmptyProto) {
  CapacityPlanningInstance request;
  CapacityPlanningProblem problem;
  ::absl::Status status = Convert(request, &problem);
  EXPECT_OK(status);
  EXPECT_EQ(problem.graph.num_nodes(), 0);
  EXPECT_EQ(problem.graph.num_arcs(), 0);
  EXPECT_EQ(problem.num_commodities, 0);
  EXPECT_EQ(problem.demands_per_commodity.size(), problem.num_commodities);
  EXPECT_EQ(problem.demands_at_node_per_commodity.size(),
            problem.graph.num_nodes());
}

constexpr int kMaxNumAttempts = 5;
for (int i = 0; i < kMaxNumAttempts; ++i) {
  CapacityPlanningMipModel mip_model;
  mip_model.Build(problem, /*relax_integrality=*/false);
  const MPSolver::ResultStatus result_status = mip_model.Solve(parameters);
  if (result_status == MPSolver::NOT_SOLVED) continue;
  ASSERT_EQ(result_status, MPSolver::FEASIBLE);
  EXPECT_GT(mip_model.solver()->Objective().Value(), 423848.0);
  return;
}
FAIL() << "Got NOT_SOLVED in all " << kMaxNumAttempts << " attempts.";
}

TEST(IlphTest, SolveEmptyProto) {
  CapacityPlanningInstance request;
  CapacityPlanningProblem problem;
  ::absl::Status status = Convert(request, &problem);
  EXPECT_OK(status);
  CapacityPlanningParameters parameters;
  parameters.time_limit = absl::Seconds(15);
  CapacityPlanningILPH ilph;
  ilph.Build(problem);
  MPSolver::ResultStatus result_status = ilph.Solve();
  EXPECT_EQ(result_status, MPSolver::INFEASIBLE);
  EXPECT_EQ(ilph.best_cost(), 0.0);
}

}  // namespace
}  // namespace operations_research
