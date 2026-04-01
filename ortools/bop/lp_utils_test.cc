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

#include "ortools/bop/lp_utils.h"

#include <stdint.h>

#include <random>
#include <string>

#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/bop/boolean_problem.pb.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace bop {
namespace {

using ::testing::EqualsProto;

TEST(ConversionCycleTest, MPModelProtoToBooleanLinearProgramAndBack) {
  std::mt19937 random(12345);

  // We start by generating a random binary optimization problem in the
  // MPModelProto format. Note that we use a minimization problem, its works
  // with maximization too, but the conversion will transform the model to a
  // minimization one.
  const int kNumVariables = 100;
  const int kNumConstraints = 100;
  MPModelProto mp_model;
  mp_model.set_maximize(false);
  mp_model.set_objective_offset(123.456);
  mp_model.set_name("test problem");
  for (int i = 0; i < kNumVariables; ++i) {
    MPVariableProto* variable = mp_model.add_variable();
    variable->set_name(absl::StrFormat("variable %d.", i));
    variable->set_is_integer(true);
    variable->set_lower_bound(0.0 - absl::Uniform<double>(random, 0.0, 0.5));
    variable->set_upper_bound(1.0 + absl::Uniform<double>(random, 0.0, 0.5));
    variable->set_objective_coefficient(
        absl::Uniform<double>(random, 1.0, 2.0));
  }
  for (int j = 0; j < kNumConstraints; ++j) {
    MPConstraintProto* constraint = mp_model.add_constraint();
    constraint->set_name(absl::StrFormat("constraint %d.", j));
    constraint->set_lower_bound(absl::Uniform<double>(random, -10.0, 0.0));
    constraint->set_upper_bound(absl::Uniform<double>(random, 0.0, 10.0));
    for (int i = 0; i < kNumVariables; ++i) {
      if (absl::Bernoulli(random, 1.0 / 5)) {
        constraint->add_var_index(i);
        constraint->add_coefficient(absl::Uniform<double>(random, 1.0, 2.0));
      }
    }
  }

  // Convert it back and forth.
  LinearBooleanProblem problem;
  glop::LinearProgram lp;  // Intermediate representation.
  MPModelProto mp_model2;
  EXPECT_TRUE(ConvertBinaryMPModelProtoToBooleanProblem(mp_model, &problem));
  ConvertBooleanProblemToLinearProgram(problem, &lp);
  LinearProgramToMPModelProto(lp, &mp_model2);

  // Check the rescaling.
  for (int i = 0; i < kNumVariables; ++i) {
    EXPECT_COMPARABLE(mp_model.variable(i).objective_coefficient(),
                      mp_model2.variable(i).objective_coefficient(), 1e-10);
  }
  for (int j = 0; j < kNumConstraints; ++j) {
    const int size = mp_model.constraint(j).var_index_size();
    EXPECT_EQ(size, mp_model2.constraint(j).var_index_size());
    if (size == 0) continue;
    const double scaling = mp_model.constraint(j).coefficient(0) /
                           mp_model2.constraint(j).coefficient(0);
    for (int i = 0; i < size; ++i) {
      EXPECT_EQ(mp_model.constraint(j).var_index(i),
                mp_model2.constraint(j).var_index(i));
      EXPECT_COMPARABLE(mp_model.constraint(j).coefficient(i),
                        mp_model2.constraint(j).coefficient(i) * scaling,
                        1e-10);
    }
  }

  // Note that redoing a round of conversion lead to exactly the same result
  // now!! This is because we use a scaling factor of the form 2^n (neat for
  // this test).
  MPModelProto mp_model3;
  EXPECT_TRUE(ConvertBinaryMPModelProtoToBooleanProblem(mp_model2, &problem));
  ConvertBooleanProblemToLinearProgram(problem, &lp);
  LinearProgramToMPModelProto(lp, &mp_model3);
  EXPECT_THAT(mp_model2, EqualsProto(mp_model3));
}

TEST(ConvertBinaryMPModelProtoToBooleanProblemTest, FixedVariableAt0) {
  MPModelProto mp_model;
  MPVariableProto* variable = mp_model.add_variable();
  variable->set_is_integer(true);
  variable->set_lower_bound(-0.3);
  variable->set_upper_bound(0.9);

  LinearBooleanProblem problem;
  EXPECT_TRUE(ConvertBinaryMPModelProtoToBooleanProblem(mp_model, &problem));
  EXPECT_EQ(1, problem.constraints_size());
  EXPECT_EQ(1, problem.constraints(0).literals(0));
  EXPECT_EQ(1, problem.constraints(0).literals_size());
  EXPECT_EQ(0, problem.constraints(0).lower_bound());
  EXPECT_EQ(0, problem.constraints(0).upper_bound());
}

TEST(ConvertBinaryMPModelProtoToBooleanProblemTest, FixedVariableAt1) {
  MPModelProto mp_model;
  MPVariableProto* variable = mp_model.add_variable();
  variable->set_is_integer(true);
  variable->set_lower_bound(0.1);
  variable->set_upper_bound(1.9);

  LinearBooleanProblem problem;
  EXPECT_TRUE(ConvertBinaryMPModelProtoToBooleanProblem(mp_model, &problem));
  EXPECT_EQ(1, problem.constraints_size());
  EXPECT_EQ(1, problem.constraints(0).literals(0));
  EXPECT_EQ(1, problem.constraints(0).literals_size());
  EXPECT_EQ(1, problem.constraints(0).lower_bound());
  EXPECT_EQ(1, problem.constraints(0).upper_bound());
}

TEST(ConvertBinaryMPModelProtoToBooleanProblemTest, NonBinaryVariable) {
  MPModelProto mp_model;
  MPVariableProto* variable = mp_model.add_variable();
  variable->set_is_integer(true);
  variable->set_lower_bound(0.1);
  variable->set_upper_bound(0.9);

  LinearBooleanProblem problem;
  EXPECT_FALSE(ConvertBinaryMPModelProtoToBooleanProblem(mp_model, &problem));
}

}  // namespace
}  // namespace bop
}  // namespace operations_research
