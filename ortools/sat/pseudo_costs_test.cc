// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/pseudo_costs.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

TEST(GetBoundChangeTest, LowerBoundChange) {
  Model model;
  auto* encoder = model.GetOrCreate<IntegerEncoder>();

  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const Literal decision = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(x, IntegerValue(3)));

  PseudoCosts pseudo_costs(&model);
  pseudo_costs.SaveBoundChanges(decision, {});
  auto& bound_changes = pseudo_costs.BoundChanges();
  EXPECT_EQ(1, bound_changes.size());
  PseudoCosts::VariableBoundChange bound_change = bound_changes[0];
  EXPECT_EQ(bound_change.var, x);
  EXPECT_EQ(bound_change.lower_bound_change, IntegerValue(3));
}

TEST(GetBoundChangeTest, UpperBoundChange) {
  Model model;
  auto* encoder = model.GetOrCreate<IntegerEncoder>();

  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const Literal decision = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(x, IntegerValue(7)));

  PseudoCosts pseudo_costs(&model);
  pseudo_costs.SaveBoundChanges(decision, {});
  auto& bound_changes = pseudo_costs.BoundChanges();
  EXPECT_EQ(1, bound_changes.size());
  PseudoCosts::VariableBoundChange bound_change = bound_changes[0];
  EXPECT_EQ(bound_change.var, NegationOf(x));
  EXPECT_EQ(bound_change.lower_bound_change, IntegerValue(3));
}

TEST(GetBoundChangeTest, EqualityDecision) {
  Model model;
  auto* encoder = model.GetOrCreate<IntegerEncoder>();

  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  Literal decision(model.GetOrCreate<SatSolver>()->NewBooleanVariable(), true);
  encoder->AssociateToIntegerEqualValue(decision, x, IntegerValue(6));

  PseudoCosts pseudo_costs(&model);
  pseudo_costs.SaveBoundChanges(decision, {});
  auto& bound_changes = pseudo_costs.BoundChanges();
  EXPECT_EQ(2, bound_changes.size());
  PseudoCosts::VariableBoundChange lower_bound_change = bound_changes[0];
  EXPECT_EQ(lower_bound_change.var, x);
  EXPECT_EQ(lower_bound_change.lower_bound_change, IntegerValue(6));
  PseudoCosts::VariableBoundChange upper_bound_change = bound_changes[1];
  EXPECT_EQ(upper_bound_change.var, NegationOf(x));
  EXPECT_EQ(upper_bound_change.lower_bound_change, IntegerValue(4));
}

TEST(PseudoCosts, Initialize) {
  Model model;
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_pseudo_cost_reliability_threshold(1);

  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));

  PseudoCosts pseudo_costs(&model);

  EXPECT_EQ(0.0, pseudo_costs.GetCost(x));
  EXPECT_EQ(0.0, pseudo_costs.GetCost(NegationOf(x)));
  EXPECT_EQ(0.0, pseudo_costs.GetCost(y));
  EXPECT_EQ(0.0, pseudo_costs.GetCost(NegationOf(y)));
  EXPECT_EQ(0, pseudo_costs.GetNumRecords(x));
  EXPECT_EQ(0, pseudo_costs.GetNumRecords(NegationOf(x)));
  EXPECT_EQ(0, pseudo_costs.GetNumRecords(y));
  EXPECT_EQ(0, pseudo_costs.GetNumRecords(NegationOf(y)));
}

namespace {
void SimulateDecision(Literal decision, IntegerValue obj_delta, Model* model) {
  const IntegerVariable objective_var =
      model->GetOrCreate<ObjectiveDefinition>()->objective_var;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* pseudo_costs = model->GetOrCreate<PseudoCosts>();

  pseudo_costs->BeforeTakingDecision(decision);
  const IntegerValue lb = integer_trail->LowerBound(objective_var);
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(objective_var, lb + obj_delta), {}, {}));
  pseudo_costs->AfterTakingDecision();
}
}  // namespace

TEST(PseudoCosts, UpdateCostOfNewVar) {
  Model model;
  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_pseudo_cost_reliability_threshold(1);

  const IntegerVariable objective_var = model.Add(NewIntegerVariable(0, 100));
  model.GetOrCreate<ObjectiveDefinition>()->objective_var = objective_var;

  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  auto* pseudo_costs = model.GetOrCreate<PseudoCosts>();

  SimulateDecision(encoder->GetOrCreateAssociatedLiteral(
                       IntegerLiteral::GreaterOrEqual(x, IntegerValue(3))),
                   IntegerValue(6), &model);

  EXPECT_EQ(2.0, pseudo_costs->GetCost(x));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(x)));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(x));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(x)));

  SimulateDecision(encoder->GetOrCreateAssociatedLiteral(
                       IntegerLiteral::LowerOrEqual(y, IntegerValue(8))),
                   IntegerValue(6), &model);

  EXPECT_EQ(2.0, pseudo_costs->GetCost(x));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(x)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(y));
  EXPECT_EQ(3.0, pseudo_costs->GetCost(NegationOf(y)));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(x));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(x)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(y));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(NegationOf(y)));
}

TEST(PseudoCosts, BasicCostUpdate) {
  Model model;
  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_pseudo_cost_reliability_threshold(1);

  const IntegerVariable objective_var = model.Add(NewIntegerVariable(0, 100));
  model.GetOrCreate<ObjectiveDefinition>()->objective_var = objective_var;

  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  auto* pseudo_costs = model.GetOrCreate<PseudoCosts>();

  SimulateDecision(encoder->GetOrCreateAssociatedLiteral(
                       IntegerLiteral::GreaterOrEqual(x, IntegerValue(3))),
                   IntegerValue(6), &model);

  EXPECT_EQ(2.0, pseudo_costs->GetCost(x));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(x)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(y));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(y)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(z));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(z)));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(x));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(x)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(y));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(y)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(z));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(z)));

  SimulateDecision(encoder->GetOrCreateAssociatedLiteral(
                       IntegerLiteral::LowerOrEqual(y, IntegerValue(8))),
                   IntegerValue(6), &model);

  EXPECT_EQ(2.0, pseudo_costs->GetCost(x));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(x)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(y));
  EXPECT_EQ(3.0, pseudo_costs->GetCost(NegationOf(y)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(z));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(z)));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(x));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(x)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(y));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(NegationOf(y)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(z));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(z)));
}

TEST(PseudoCosts, PseudoCostReliabilityTest) {
  Model model;
  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_pseudo_cost_reliability_threshold(2);

  const IntegerVariable objective_var = model.Add(NewIntegerVariable(0, 100));
  model.GetOrCreate<ObjectiveDefinition>()->objective_var = objective_var;

  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  auto* pseudo_costs = model.GetOrCreate<PseudoCosts>();

  SimulateDecision(encoder->GetOrCreateAssociatedLiteral(
                       IntegerLiteral::GreaterOrEqual(x, IntegerValue(3))),
                   IntegerValue(6), &model);

  EXPECT_EQ(2.0, pseudo_costs->GetCost(x));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(x)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(y));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(y)));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(x));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(x)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(y));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(y)));
  EXPECT_EQ(kNoIntegerVariable, pseudo_costs->GetBestDecisionVar());

  SimulateDecision(encoder->GetOrCreateAssociatedLiteral(
                       IntegerLiteral::LowerOrEqual(y, IntegerValue(8))),
                   IntegerValue(14), &model);

  EXPECT_EQ(2.0, pseudo_costs->GetCost(x));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(NegationOf(x)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(y));
  EXPECT_EQ(7.0, pseudo_costs->GetCost(NegationOf(y)));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(x));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(NegationOf(x)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(y));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(NegationOf(y)));
  EXPECT_EQ(kNoIntegerVariable, pseudo_costs->GetBestDecisionVar());

  SimulateDecision(encoder->GetOrCreateAssociatedLiteral(
                       IntegerLiteral::LowerOrEqual(x, IntegerValue(8))),
                   IntegerValue(6), &model);

  EXPECT_EQ(2.0, pseudo_costs->GetCost(x));
  EXPECT_EQ(3.0, pseudo_costs->GetCost(NegationOf(x)));
  EXPECT_EQ(0.0, pseudo_costs->GetCost(y));
  EXPECT_EQ(7.0, pseudo_costs->GetCost(NegationOf(y)));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(x));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(NegationOf(x)));
  EXPECT_EQ(0, pseudo_costs->GetNumRecords(y));
  EXPECT_EQ(1, pseudo_costs->GetNumRecords(NegationOf(y)));
  EXPECT_EQ(NegationOf(x), pseudo_costs->GetBestDecisionVar());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
