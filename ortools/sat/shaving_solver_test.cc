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

#include "ortools/sat/shaving_solver.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

class VariablesShavingSolverTestPeer {
 public:
  using ShavingGraph = VariablesShavingSolver::ShavingGraph;
};

namespace {

using ShavingGraph = VariablesShavingSolverTestPeer::ShavingGraph;

int AddVariable(int64_t lb, int64_t ub, CpModelProto* model) {
  IntegerVariableProto* var = model->add_variables();
  var->add_domain(lb);
  var->add_domain(ub);
  return model->variables_size() - 1;
}

void AddLinearConstraint(int var1, int var2, CpModelProto* model) {
  LinearConstraintProto* linear = model->add_constraints()->mutable_linear();
  linear->add_vars(var1);
  linear->add_coeffs(1);
  linear->add_vars(var2);
  linear->add_coeffs(1);
  linear->add_domain(0);
  linear->add_domain(100);
}

int AddInterval(int start_var, CpModelProto* model) {
  const int index = model->constraints_size();
  IntervalConstraintProto* interval =
      model->add_constraints()->mutable_interval();
  interval->mutable_start()->add_vars(start_var);
  interval->mutable_start()->add_coeffs(1);
  interval->mutable_size()->set_offset(1);
  *interval->mutable_end() = interval->start();
  interval->mutable_end()->set_offset(1);
  return index;
}

std::vector<Domain> GetDomains(const CpModelProto& model) {
  std::vector<Domain> domains;
  domains.reserve(model.variables_size());
  for (const IntegerVariableProto& var : model.variables()) {
    domains.push_back(ReadDomainFromProto(var));
  }
  return domains;
}

TEST(ShavingGraphTest, BoundedByVariableBudget) {
  CpModelProto model;
  const int root = AddVariable(0, 10, &model);
  const int first = AddVariable(0, 10, &model);
  const int second = AddVariable(0, 10, &model);
  AddLinearConstraint(root, first, &model);
  AddLinearConstraint(root, second, &model);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(root, GetDomains(model), /*max_variables=*/2,
                    /*max_constraints=*/0);

  ASSERT_EQ(model.constraints_size(), 2);
  EXPECT_TRUE(graph.IsConstraintActive(0, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(1, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 2);
  EXPECT_EQ(neighborhood.num_active_constraints, 1);
}

TEST(ShavingGraphTest, BoundedByConstraintBudget) {
  CpModelProto model;
  const int root = AddVariable(0, 10, &model);
  const int first = AddVariable(0, 10, &model);
  const int second = AddVariable(0, 10, &model);
  AddLinearConstraint(root, first, &model);
  AddLinearConstraint(root, second, &model);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(root, GetDomains(model), /*max_variables=*/0,
                    /*max_constraints=*/1);

  ASSERT_EQ(model.constraints_size(), 2);
  EXPECT_TRUE(graph.IsConstraintActive(0, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(1, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 2);
  EXPECT_EQ(neighborhood.num_active_constraints, 1);
}

TEST(ShavingGraphTest, UnboundedExtractionFindsConnectedComponent) {
  CpModelProto model;
  const int root = AddVariable(0, 10, &model);
  const int middle = AddVariable(0, 10, &model);
  const int last = AddVariable(0, 10, &model);
  const int isolated = AddVariable(0, 10, &model);
  const int isolated_other = AddVariable(0, 10, &model);
  AddLinearConstraint(root, middle, &model);
  AddLinearConstraint(middle, last, &model);
  AddLinearConstraint(isolated, isolated_other, &model);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(root, GetDomains(model), /*max_variables=*/0,
                    /*max_constraints=*/0);

  ASSERT_EQ(model.constraints_size(), 3);
  EXPECT_TRUE(graph.IsConstraintActive(0, neighborhood));
  EXPECT_TRUE(graph.IsConstraintActive(1, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(2, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 3);
  EXPECT_EQ(neighborhood.num_active_constraints, 2);
}

TEST(ShavingGraphTest, ReusesVisitedStampsAcrossNeighborhoods) {
  CpModelProto model;
  const int first_root = AddVariable(0, 10, &model);
  const int first_other = AddVariable(0, 10, &model);
  const int second_root = AddVariable(0, 10, &model);
  const int second_other = AddVariable(0, 10, &model);
  AddLinearConstraint(first_root, first_other, &model);
  AddLinearConstraint(second_root, second_other, &model);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood first =
      graph.Extract(first_root, GetDomains(model), /*max_variables=*/0,
                    /*max_constraints=*/0);
  EXPECT_TRUE(graph.IsConstraintActive(0, first));

  const ShavingGraph::Neighborhood second =
      graph.Extract(second_root, GetDomains(model), /*max_variables=*/0,
                    /*max_constraints=*/0);
  EXPECT_FALSE(graph.IsConstraintActive(0, second));
  EXPECT_TRUE(graph.IsConstraintActive(1, second));
  EXPECT_EQ(second.num_active_variables, 2);
  EXPECT_EQ(second.num_active_constraints, 1);
}

TEST(ShavingGraphTest, SkipsConstraintsWithFalseEnforcementLiteral) {
  CpModelProto model;
  const int root = AddVariable(0, 10, &model);
  const int other = AddVariable(0, 10, &model);
  const int enforcement = AddVariable(0, 0, &model);
  ConstraintProto* ct = model.add_constraints();
  ct->add_enforcement_literal(enforcement);
  LinearConstraintProto* linear = ct->mutable_linear();
  linear->add_vars(root);
  linear->add_coeffs(1);
  linear->add_vars(other);
  linear->add_coeffs(1);
  linear->add_domain(0);
  linear->add_domain(100);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(root, GetDomains(model), /*max_variables=*/0,
                    /*max_constraints=*/0);

  ASSERT_EQ(model.constraints_size(), 1);
  EXPECT_FALSE(graph.IsConstraintActive(0, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 1);
  EXPECT_EQ(neighborhood.num_active_constraints, 0);
}

TEST(ShavingGraphTest, TraversesSchedulingConstraintIntervals) {
  CpModelProto model;
  const int x_start = AddVariable(0, 10, &model);
  const int y_start = AddVariable(0, 10, &model);
  const int x_interval = AddInterval(x_start, &model);
  const int y_interval = AddInterval(y_start, &model);

  ConstraintProto* no_overlap = model.add_constraints();
  no_overlap->mutable_no_overlap()->add_intervals(x_interval);
  no_overlap->mutable_no_overlap()->add_intervals(y_interval);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(x_start, GetDomains(model), /*max_variables=*/0,
                    /*max_constraints=*/0);

  ASSERT_EQ(model.constraints_size(), 3);
  EXPECT_TRUE(graph.IsConstraintActive(x_interval, neighborhood));
  EXPECT_TRUE(graph.IsConstraintActive(y_interval, neighborhood));
  EXPECT_TRUE(graph.IsConstraintActive(2, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 2);
  EXPECT_EQ(neighborhood.num_active_constraints, 3);
}

TEST(ShavingGraphTest, NoOverlap2DPairRespectsVariableBudget) {
  CpModelProto model;
  const int x_start = AddVariable(0, 10, &model);
  const int y_start = AddVariable(0, 10, &model);
  const int presence = AddVariable(0, 1, &model);
  const int x_interval = AddInterval(x_start, &model);
  const int y_interval = AddInterval(y_start, &model);

  const int no_overlap_index = model.constraints_size();
  ConstraintProto* no_overlap = model.add_constraints();
  no_overlap->add_enforcement_literal(presence);
  no_overlap->mutable_no_overlap_2d()->add_x_intervals(x_interval);
  no_overlap->mutable_no_overlap_2d()->add_y_intervals(y_interval);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(x_start, GetDomains(model), /*max_variables=*/1,
                    /*max_constraints=*/0);

  EXPECT_TRUE(graph.IsConstraintActive(x_interval, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(y_interval, neighborhood));
  EXPECT_FALSE(
      graph.IsNoOverlap2DConstraintActive(no_overlap_index, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 1);
  EXPECT_EQ(neighborhood.num_active_constraints, 1);
}

TEST(ShavingGraphTest, NoOverlap2DNeighborhoodSelectsOnlyReachedPairs) {
  CpModelProto model;
  const int x_start = AddVariable(0, 10, &model);
  const int y_start = AddVariable(0, 10, &model);
  const int other_x_start = AddVariable(0, 10, &model);
  const int other_y_start = AddVariable(0, 10, &model);
  const int presence = AddVariable(0, 1, &model);
  const int x_interval = AddInterval(x_start, &model);
  const int y_interval = AddInterval(y_start, &model);
  const int other_x_interval = AddInterval(other_x_start, &model);
  const int other_y_interval = AddInterval(other_y_start, &model);

  const int no_overlap_index = model.constraints_size();
  ConstraintProto* no_overlap = model.add_constraints();
  no_overlap->add_enforcement_literal(presence);
  NoOverlap2DConstraintProto* no_overlap_2d =
      no_overlap->mutable_no_overlap_2d();
  no_overlap_2d->add_x_intervals(x_interval);
  no_overlap_2d->add_y_intervals(y_interval);
  no_overlap_2d->add_x_intervals(other_x_interval);
  no_overlap_2d->add_y_intervals(other_y_interval);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(x_start, GetDomains(model), /*max_variables=*/3,
                    /*max_constraints=*/0);

  EXPECT_TRUE(graph.IsConstraintActive(x_interval, neighborhood));
  EXPECT_TRUE(graph.IsConstraintActive(y_interval, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(other_x_interval, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(other_y_interval, neighborhood));
  EXPECT_TRUE(graph.IsNoOverlap2DPairActive(0, neighborhood));
  EXPECT_FALSE(graph.IsNoOverlap2DPairActive(1, neighborhood));
  EXPECT_TRUE(
      graph.IsNoOverlap2DConstraintActive(no_overlap_index, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 3);
  EXPECT_EQ(neighborhood.num_active_constraints, 3);
}

TEST(ShavingGraphTest, ExpandsNoOverlap2DAsCompletePairs) {
  CpModelProto model;
  const int x_start = AddVariable(0, 10, &model);
  const int y_start = AddVariable(0, 10, &model);
  const int presence = AddVariable(0, 1, &model);
  const int x_interval = AddInterval(x_start, &model);
  const int y_interval = AddInterval(y_start, &model);

  const int no_overlap_index = model.constraints_size();
  ConstraintProto* no_overlap = model.add_constraints();
  no_overlap->add_enforcement_literal(presence);
  no_overlap->mutable_no_overlap_2d()->add_x_intervals(x_interval);
  no_overlap->mutable_no_overlap_2d()->add_y_intervals(y_interval);

  const int other = AddVariable(0, 10, &model);
  AddLinearConstraint(presence, other, &model);

  ShavingGraph graph(model);
  const ShavingGraph::Neighborhood neighborhood =
      graph.Extract(x_start, GetDomains(model), /*max_variables=*/0,
                    /*max_constraints=*/0);

  ASSERT_EQ(model.constraints_size(), 4);
  EXPECT_TRUE(graph.IsConstraintActive(x_interval, neighborhood));
  EXPECT_TRUE(graph.IsConstraintActive(y_interval, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(no_overlap_index, neighborhood));
  EXPECT_FALSE(graph.IsConstraintActive(3, neighborhood));
  ASSERT_EQ(graph.no_overlap_2d_pairs().size(), 1);
  EXPECT_TRUE(graph.IsNoOverlap2DPairActive(0, neighborhood));
  EXPECT_TRUE(
      graph.IsNoOverlap2DConstraintActive(no_overlap_index, neighborhood));
  EXPECT_EQ(neighborhood.num_active_variables, 3);
  EXPECT_EQ(neighborhood.num_active_constraints, 3);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
