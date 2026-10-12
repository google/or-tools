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

#include "ortools/constraint_solver/pack.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {

class PackTest : public ::testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("TableTest"); }
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(PackTest, OneUpperBoundDimension, solver, true) {
  // Test one dimension with a capacity limit.
  const int kNumberOfVariables = 9;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> weights = {1, 2, 3, 1, 2, 3, 1, 2, 6};
  std::vector<int64_t> upper_bounds = {2, 4, 5};
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumLessOrEqualConstantDimension(weights, upper_bounds);
  solver->AddConstraint(pack);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 1, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 1, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[1]->SetValue(1);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[0]->SetValue(2);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[2]->RemoveValue(3);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_BOUND_TO(vars[2], 2);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_DOMAIN(vars[4], 0, 1, 3);
  EXPECT_BOUND_TO(vars[5], 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_DOMAIN(vars[7], 0, 1, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, OneUpperBoundCallback1Dimension, solver, true) {
  // Test one dimension with a capacity limit.
  const int kNumberOfVariables = 9;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> vector_weights = {1, 2, 3, 1, 2, 3, 1, 2, 6};
  Solver::IndexEvaluator1 weights = [&vector_weights](int64_t index) {
    return vector_weights[index];
  };
  std::vector<int64_t> upper_bounds = {2, 4, 5};
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumLessOrEqualConstantDimension(weights, upper_bounds);
  solver->AddConstraint(pack);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 1, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 1, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[1]->SetValue(1);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[0]->SetValue(2);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[2]->RemoveValue(3);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_BOUND_TO(vars[2], 2);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_DOMAIN(vars[4], 0, 1, 3);
  EXPECT_BOUND_TO(vars[5], 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_DOMAIN(vars[7], 0, 1, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, OneUpperBoundCallback2Dimension, solver, true) {
  // Test one dimension with a capacity limit.
  const int kNumberOfVariables = 9;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> vector_weights = {1, 2, 4, 1, 2, 4, 1, 2, 4};
  std::vector<int> vector_preferred = {0, 0, 0, 2, 2, 2, 1, 1, 1};
  Solver::IndexEvaluator2 weights = [&vector_weights, &vector_preferred](
                                        int64_t item_index, int64_t bin_index) {
    return vector_weights[item_index] +
           (bin_index != vector_preferred[item_index]);
  };
  std::vector<int64_t> upper_bounds = {2, 4, 5};
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumLessOrEqualConstantDimension(weights, upper_bounds);
  solver->AddConstraint(pack);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_DOMAIN(vars[4], 1, 2, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 1, 3);
  EXPECT_EQ_RANGE(vars[8], 1, 3);
  vars[1]->SetValue(1);
  EXPECT_EQ_DOMAIN(vars[0], 0, 2, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_DOMAIN(vars[3], 0, 2, 3);
  EXPECT_EQ_RANGE(vars[4], 2, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 2, 3);
  EXPECT_EQ_RANGE(vars[8], 2, 3);
  vars[3]->SetValue(2);
  EXPECT_EQ_DOMAIN(vars[0], 0, 2, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_BOUND_TO(vars[2], 3);
  EXPECT_BOUND_TO(vars[3], 2);
  EXPECT_EQ_RANGE(vars[4], 2, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 2, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, OneUpperBoundCallback2DimensionVarLoad, solver,
                     true) {
  // Test one dimension with a capacity limit.
  const int kNumberOfVariables = 9;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> vector_weights = {1, 2, 4, 1, 2, 4, 1, 2, 4};
  std::vector<int> vector_preferred = {0, 0, 0, 2, 2, 2, 1, 1, 1};
  Solver::IndexEvaluator2 weights = [&vector_weights, &vector_preferred](
                                        int64_t item_index, int64_t bin_index) {
    return vector_weights[item_index] +
           (bin_index != vector_preferred[item_index]);
  };
  std::vector<IntVar*> load_vars;
  load_vars.push_back(solver->MakeIntVar(0, 2));
  load_vars.push_back(solver->MakeIntVar(0, 4));
  load_vars.push_back(solver->MakeIntVar(0, 5));
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumEqualVarDimension(weights, load_vars);
  solver->AddConstraint(pack);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_DOMAIN(vars[4], 1, 2, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 1, 3);
  EXPECT_EQ_RANGE(vars[8], 1, 3);
  vars[1]->SetValue(1);
  EXPECT_EQ_RANGE(load_vars[1], 3, 4);
  EXPECT_EQ_DOMAIN(vars[0], 0, 2, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_DOMAIN(vars[3], 0, 2, 3);
  EXPECT_EQ_RANGE(vars[4], 2, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 2, 3);
  EXPECT_EQ_RANGE(vars[8], 2, 3);
  vars[3]->SetValue(2);
  EXPECT_EQ_RANGE(load_vars[2], 1, 5);
  EXPECT_EQ_DOMAIN(vars[0], 0, 2, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_BOUND_TO(vars[2], 3);
  EXPECT_BOUND_TO(vars[3], 2);
  EXPECT_EQ_RANGE(vars[4], 2, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 2, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, InitialPropagateFails, solver, false) {
  // Test failing initial propagation.
  const int kNumberOfVariables = 1;
  const int kNumberOfBins = 1;
  Pack* pack = nullptr;
  {  // Block so that the vectors are destroyed before the constraint is posted
    std::vector<IntVar*> vars;
    solver->MakeIntVarArray(kNumberOfVariables, 0, 0, "var_", &vars);
    std::vector<int64_t> weights = {2};
    std::vector<int64_t> upper_bounds = {1};
    pack = solver->MakePack(vars, kNumberOfBins);
    pack->AddWeightedSumLessOrEqualConstantDimension(weights, upper_bounds);
  }
  solver->AddConstraint(pack);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, MoreBinsThanVariables, solver, true) {
  // Test early failure.
  const int kNumberOfVariables = 1;
  const int kNumberOfBins = 3;
  Pack* pack = nullptr;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  pack = solver->MakePack(vars, kNumberOfBins);
  solver->AddConstraint(pack);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, OneSumEqVarDimensionAsUpperBound, solver, true) {
  // Test one dimension, capacity = variable.
  const int kNumberOfVariables = 9;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> weights = {1, 2, 3, 1, 2, 3, 1, 2, 6};
  std::vector<IntVar*> load_vars;
  load_vars.push_back(solver->MakeIntVar(0, 2));
  load_vars.push_back(solver->MakeIntVar(0, 4));
  load_vars.push_back(solver->MakeIntVar(0, 5));
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumEqualVarDimension(weights, load_vars);
  solver->AddConstraint(pack);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 1, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 1, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[1]->SetValue(1);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[0]->SetValue(2);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[2]->RemoveValue(3);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_BOUND_TO(vars[2], 2);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_DOMAIN(vars[4], 0, 1, 3);
  EXPECT_BOUND_TO(vars[5], 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_DOMAIN(vars[7], 0, 1, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, OneSumEqVarDimensionTouchLoad, solver, true) {
  // Test one dimension, load == var, touch the var.
  const int kNumberOfVariables = 5;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> weights = {1, 2, 3, 4, 5};
  std::vector<IntVar*> load_vars;
  load_vars.push_back(solver->MakeIntVar(0, 2, "load_0"));
  load_vars.push_back(solver->MakeIntVar(0, 4, "load_1"));
  load_vars.push_back(solver->MakeIntVar(0, 5, "load_2"));
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumEqualVarDimension(weights, load_vars);
  solver->AddConstraint(pack);
  vars[0]->SetValue(2);
  EXPECT_EQ(1, load_vars[2]->Min());
  load_vars[0]->SetMin(1);
  EXPECT_EQ(2, load_vars[0]->Min());
  EXPECT_EQ(1, vars[1]->Size());
  EXPECT_EQ(0, vars[1]->Min());
  load_vars[2]->SetMax(4);
  EXPECT_EQ(2, vars[3]->Size());
  EXPECT_FALSE(vars[3]->Contains(2));
  vars[2]->RemoveValue(kNumberOfBins);  // Must be assigned.
  vars[3]->RemoveValue(kNumberOfBins);
  EXPECT_TRUE(vars[2]->Bound());
  EXPECT_EQ(2, vars[2]->Min());
  EXPECT_TRUE(vars[3]->Bound());
  EXPECT_EQ(1, vars[3]->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, WeightedAssignedDimensionReductionDown, solver,
                     true) {
  // Test dimension that maintains the weighted sum of assigned items,
  // and move the resulting cost down.
  const int kNumberOfVariables = 4;
  const int kNumberOfBins = 1;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> weights = {100, 10, 2, 1};
  IntVar* const cost_var = solver->MakeIntVar(0, 113, "cost var");
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumOfAssignedDimension(weights, cost_var);
  solver->AddConstraint(pack);
  cost_var->SetMax(101);
  EXPECT_FALSE(vars[0]->Bound());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  cost_var->SetMax(99);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_EQ(int64_t{1}, vars[0]->Min());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  cost_var->SetMax(9);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_TRUE(vars[1]->Bound());
  EXPECT_EQ(int64_t{1}, vars[1]->Min());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, WeightedAssignedDimensionReductionUp, solver,
                     true) {
  // Test dimension that maintains the weighted sum of assigned items,
  // and move the resulting cost up.
  const int kNumberOfVariables = 4;
  const int kNumberOfBins = 1;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> weights = {100, 10, 2, 1};
  IntVar* const cost_var = solver->MakeIntVar(0, 113, "cost var");
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddWeightedSumOfAssignedDimension(weights, cost_var);
  solver->AddConstraint(pack);
  cost_var->SetMin(9);
  EXPECT_FALSE(vars[0]->Bound());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  cost_var->SetMin(15);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_EQ(int64_t{0}, vars[0]->Min());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  cost_var->SetMin(111);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_TRUE(vars[1]->Bound());
  EXPECT_EQ(int64_t{0}, vars[1]->Min());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, CountBinReductionUp, solver, true) {
  // Test dimension that count the number of used bins, and move the
  // resulting cost up.
  const int kNumberOfVariables = 4;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  IntVar* const count = solver->MakeIntVar(0, kNumberOfBins);
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddCountUsedBinDimension(count);
  solver->AddConstraint(pack);
  EXPECT_EQ(0, count->Min());
  EXPECT_EQ(3, count->Max());
  vars[0]->SetValue(0);
  vars[1]->RemoveValue(2);
  vars[2]->RemoveValue(2);
  EXPECT_EQ(1, count->Min());
  EXPECT_EQ(3, count->Max());
  count->SetMin(3);
  EXPECT_TRUE(vars[3]->Bound());
  EXPECT_EQ(2, vars[3]->Min());
  EXPECT_EQ(3, vars[1]->Size());
  EXPECT_EQ(3, vars[2]->Size());
  vars[1]->SetValue(3);
  EXPECT_TRUE(vars[2]->Bound());
  EXPECT_EQ(1, vars[2]->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, CountBinReductionDown, solver, true) {
  // Test dimension that count the number of used bins, and move the
  // resulting cost down.
  const int kNumberOfVariables = 4;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  IntVar* const count = solver->MakeIntVar(0, kNumberOfBins);
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddCountUsedBinDimension(count);
  solver->AddConstraint(pack);
  EXPECT_EQ(0, count->Min());
  EXPECT_EQ(3, count->Max());
  vars[0]->SetValue(0);
  vars[1]->SetRange(0, 1);
  vars[2]->SetRange(0, 2);
  vars[3]->SetRange(0, 3);
  EXPECT_EQ(1, count->Min());
  EXPECT_EQ(3, count->Max());
  count->SetMax(2);
  EXPECT_EQ(2, vars[1]->Size());
  EXPECT_EQ(3, vars[2]->Size());
  EXPECT_EQ(4, vars[3]->Size());
  vars[1]->SetValue(0);
  EXPECT_EQ(3, vars[2]->Size());
  EXPECT_EQ(4, vars[3]->Size());
  vars[2]->SetValue(1);
  EXPECT_FALSE(vars[3]->Contains(2));
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, CountAssignedReductionDown, solver, true) {
  // Test dimension that count the number of assigned items, and move
  // the resulting cost down.
  const int kNumberOfVariables = 4;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  IntVar* const count = solver->MakeIntVar(0, kNumberOfVariables);
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddCountAssignedItemsDimension(count);
  solver->AddConstraint(pack);
  EXPECT_EQ(0, count->Min());
  EXPECT_EQ(4, count->Max());
  vars[0]->SetValue(0);
  vars[1]->SetRange(0, 1);
  vars[2]->SetRange(0, 2);
  vars[3]->SetRange(0, 3);
  EXPECT_EQ(3, count->Min());
  EXPECT_EQ(4, count->Max());
  count->SetMax(3);
  EXPECT_EQ(3, vars[3]->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, OneUpperConstBoundDimension, solver, true) {
  // Same test as weighted sums of items below capacity, but using
  // IntConsts instead of simple integer.
  const int kNumberOfVariables = 9;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  const int kWeights[] = {1, 2, 3, 1, 2, 3, 1, 2, 6};
  std::vector<IntVar*> weights;
  for (int i = 0; i < kNumberOfVariables; ++i) {
    weights.push_back(solver->MakeIntConst(kWeights[i]));
  }
  std::vector<int64_t> upper_bounds = {2, 4, 5};
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddSumVariableWeightsLessOrEqualConstantDimension(weights,
                                                          upper_bounds);
  solver->AddConstraint(pack);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 1, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 1, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[1]->SetValue(1);
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[0]->SetValue(2);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_EQ_RANGE(vars[2], 2, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_RANGE(vars[4], 0, 3);
  EXPECT_EQ_RANGE(vars[5], 2, 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_RANGE(vars[7], 0, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  vars[2]->RemoveValue(3);
  EXPECT_BOUND_TO(vars[0], 2);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_BOUND_TO(vars[2], 2);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  EXPECT_EQ_DOMAIN(vars[4], 0, 1, 3);
  EXPECT_BOUND_TO(vars[5], 3);
  EXPECT_EQ_RANGE(vars[6], 0, 3);
  EXPECT_EQ_DOMAIN(vars[7], 0, 1, 3);
  EXPECT_BOUND_TO(vars[8], 3);
  return nullptr;
}

TEST_IN_PROPAGATION0(PackTest, VariableUsage, solver, true) {
  // Test that take care of variable weights of items. The first is 3
  // times bigger than the second, and sum of the weights of the 3rd
  // and 4th items is 4.
  const int kNumberOfVariables = 4;
  const int kNumberOfBins = 3;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kNumberOfVariables, 0, kNumberOfBins, "var_", &vars);
  std::vector<int64_t> upper_bounds = {1, 3, 4};
  std::vector<IntVar*> weights;
  solver->MakeIntVarArray(kNumberOfVariables, 1, 4, "usage_", &weights);
  Pack* const pack = solver->MakePack(vars, kNumberOfBins);
  pack->AddSumVariableWeightsLessOrEqualConstantDimension(weights,
                                                          upper_bounds);
  solver->AddConstraint(pack);
  solver->AddConstraint(
      solver->MakeEquality(solver->MakeProd(weights[0], 3)->Var(), weights[1]));
  solver->AddConstraint(
      solver->MakeEquality(solver->MakeSum(weights[2], weights[3]), 4));
  // Item 0 uses 1, item 1 uses 3.
  EXPECT_BOUND_TO(weights[0], 1);
  EXPECT_BOUND_TO(weights[1], 3);
  EXPECT_EQ_RANGE(vars[1], 1, 3);
  EXPECT_EQ_RANGE(vars[2], 0, 3);
  EXPECT_EQ_RANGE(vars[3], 0, 3);
  weights[3]->SetMin(2);
  // Item 3 cannot fit in bin 0.
  EXPECT_EQ_RANGE(vars[3], 1, 3);
  vars[2]->SetValue(0);
  // weights[2] pushed down to 1 , weights[3]-> pushed up to 3.
  EXPECT_BOUND_TO(weights[2], 1);
  EXPECT_BOUND_TO(weights[3], 3);
  vars[1]->SetValue(2);
  // Item 1 occupies bin 2 (nearly). Item 3 can fit into 1 or be unscheduled.
  EXPECT_EQ_DOMAIN(vars[3], 1, 3);
  return nullptr;
}

class PostPackDecision : public Decision {
 public:
  PostPackDecision(Pack* pack, IntVar* item1, IntVar* item2)
      : pack_(pack), item1_(item1), item2_(item2) {}
  void Apply(Solver* const s) override {
    // Put item 1 in the bin
    item1_->SetValue(0);
    s->AddConstraint(pack_);
  }

  // Refute will be called after a backtrack.
  void Refute(Solver* const s) override {
    // Put item 2 in the bin
    item2_->SetValue(0);
    s->AddConstraint(pack_);
  }

  std::string DebugString() const override { return "PostPackDecisionBuilder"; }

 private:
  Pack* pack_;
  IntVar* item1_;
  IntVar* item2_;
};

class SingleDecisionDecisionBuilder : public DecisionBuilder {
 public:
  explicit SingleDecisionDecisionBuilder(Decision* decision)
      : decision_(decision) {}
  Decision* Next(Solver* const s) override {
    Decision* ans = decision_;
    decision_ = nullptr;
    return ans;
  }

 private:
  Decision* decision_;
};

TEST_F(PackTest, InitialPropagateTwice) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(2, 0, 1, &vars);
  Pack* pack = solver_->MakePack(vars, 1);
  std::vector<int64_t> weights = {5, 2};
  std::vector<int64_t> capa = {3};
  pack->AddWeightedSumLessOrEqualConstantDimension(weights, capa);
  PostPackDecision decision(pack, vars[0], vars[1]);
  SingleDecisionDecisionBuilder db(&decision);
  bool solution_found = solver_->Solve(&db);
  EXPECT_TRUE(solution_found);  // The right branch should find a solution
}

}  // namespace
}  // namespace operations_research
