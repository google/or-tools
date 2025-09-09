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

#include "ortools/math_opt/cpp/solution.h"

#include <optional>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/enums_testing.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/objective.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

INSTANTIATE_TYPED_TEST_SUITE_P(SolutionStatus, EnumTest, SolutionStatus);

TEST(PrimalSolutionTest, ProtoRoundTripTest) {
  ModelStorage model;
  const Variable x(&model, model.AddVariable("x"));
  const Variable y(&model, model.AddVariable("y"));
  const Objective o =
      Objective::Auxiliary(&model, model.AddAuxiliaryObjective(2));

  PrimalSolutionProto proto;
  proto.set_objective_value(9.0);
  (*proto.mutable_auxiliary_objective_values())[o.id().value()] = 3.0;
  proto.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  for (int i : {0, 1}) {
    proto.mutable_variable_values()->add_ids(i);
  }
  for (double v : {2.0, 1.0}) {
    proto.mutable_variable_values()->add_values(v);
  }

  PrimalSolution expected = {.variable_values = {{x, 2.0}, {y, 1.0}},
                             .objective_value = 9.0,
                             .auxiliary_objective_values = {{o, 3.0}},
                             .feasibility_status = SolutionStatus::kInfeasible};

  // Test one way
  EXPECT_THAT(PrimalSolution::FromProto(&model, proto),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
  // Test round trip
  EXPECT_THAT(PrimalSolution::FromProto(&model, expected.Proto()),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
}

TEST(PrimalSolutionTest, InvalidVariableValues) {
  ModelStorage model;
  model.AddVariable("x");

  PrimalSolutionProto proto;
  proto.set_objective_value(9.0);
  proto.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  proto.mutable_variable_values()->add_ids(0);

  EXPECT_THAT(PrimalSolution::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid variable_values")));
}

TEST(PrimalSolutionTest, InvalidAuxiliaryObjectiveValues) {
  ModelStorage model;

  PrimalSolutionProto proto;
  (*proto.mutable_auxiliary_objective_values())[0] = 3.0;

  EXPECT_THAT(PrimalSolution::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid auxiliary_objective_values")));
}

TEST(PrimalSolutionTest, FeasibilityStatusUnspecified) {
  ModelStorage model;
  PrimalSolutionProto proto;
  EXPECT_THAT(PrimalSolution::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("feasibility_status")));
}

TEST(PrimalSolutionTest, GetObjectiveValue) {
  ModelStorage model;
  const Objective p = Objective::Primary(&model);
  const Objective o =
      Objective::Auxiliary(&model, model.AddAuxiliaryObjective(1));
  PrimalSolution solution{.objective_value = 1.0,
                          .auxiliary_objective_values = {{o, 2.0}}};
  EXPECT_EQ(solution.get_objective_value(p), 1.0);
  EXPECT_EQ(solution.get_objective_value(o), 2.0);
}

TEST(PrimalSolutionDeathTest, GetObjectiveValueWrongModel) {
  ModelStorage model_a;
  const Variable v_a(&model_a, model_a.AddVariable("v"));
  const Objective o_a =
      Objective::Auxiliary(&model_a, model_a.AddAuxiliaryObjective(1));
  PrimalSolution solution{.objective_value = 1.0,
                          .auxiliary_objective_values = {{o_a, 2.0}}};

  ModelStorage model_b;
  const Objective p_b = Objective::Primary(&model_b);
  const Objective o_b =
      Objective::Auxiliary(&model_b, model_b.AddAuxiliaryObjective(1));
  // This is a documented corner case where we don't CHECK the model.
  EXPECT_EQ(solution.get_objective_value(p_b), 1.0);
  EXPECT_DEATH(solution.get_objective_value(o_b), "");

  solution.variable_values.insert({v_a, 3.0});
  EXPECT_DEATH(solution.get_objective_value(p_b), "");
  EXPECT_DEATH(solution.get_objective_value(o_b), "");
}

TEST(PrimalRayTest, ProtoRoundTripTest) {
  ModelStorage model;
  const Variable x(&model, model.AddVariable("x"));
  const Variable y(&model, model.AddVariable("y"));

  PrimalRayProto proto;
  for (int i : {0, 1}) {
    proto.mutable_variable_values()->add_ids(i);
  }
  for (double v : {2.0, 1.0}) {
    proto.mutable_variable_values()->add_values(v);
  }

  const PrimalRay expected = {.variable_values = {{x, 2.0}, {y, 1.0}}};

  // Test one way
  EXPECT_THAT(PrimalRay::FromProto(&model, proto),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
  // Test round trip
  EXPECT_THAT(PrimalRay::FromProto(&model, expected.Proto()),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
}

TEST(PrimalRayTest, InvalidVariableValues) {
  ModelStorage model;
  model.AddVariable("x");

  PrimalRayProto proto;
  proto.mutable_variable_values()->add_ids(0);

  EXPECT_THAT(PrimalRay::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid variable_values")));
}

TEST(DualSolutionTest, ProtoRoundTripTest) {
  ModelStorage model;
  const Variable x(&model, model.AddVariable("x"));
  const Variable y(&model, model.AddVariable("y"));
  const LinearConstraint c(&model, model.AddLinearConstraint("c"));
  const LinearConstraint d(&model, model.AddLinearConstraint("d"));
  const QuadraticConstraint e(
      &model, model.AddAtomicConstraint(QuadraticConstraintData{.name = "e"}));
  const QuadraticConstraint f(
      &model, model.AddAtomicConstraint(QuadraticConstraintData{.name = "f"}));

  DualSolutionProto proto;
  proto.set_objective_value(9.0);
  proto.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  for (int i : {0, 1}) {
    proto.mutable_reduced_costs()->add_ids(i);
  }
  for (double v : {2.0, 1.0}) {
    proto.mutable_reduced_costs()->add_values(v);
  }
  for (int i : {0, 1}) {
    proto.mutable_dual_values()->add_ids(i);
  }
  for (double v : {3.0, 4.0}) {
    proto.mutable_dual_values()->add_values(v);
  }
  for (int i : {0, 1}) {
    proto.mutable_quadratic_dual_values()->add_ids(i);
  }
  for (double v : {5.0, 6.0}) {
    proto.mutable_quadratic_dual_values()->add_values(v);
  }

  const DualSolution expected = {
      .dual_values = {{c, 3.0}, {d, 4.0}},
      .quadratic_dual_values = {{e, 5.0}, {f, 6.0}},
      .reduced_costs = {{x, 2.0}, {y, 1.0}},
      .objective_value = 9.0,
      .feasibility_status = SolutionStatus::kFeasible};

  // Test one way
  EXPECT_THAT(DualSolution::FromProto(&model, proto),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
  // Test round trip
  EXPECT_THAT(DualSolution::FromProto(&model, expected.Proto()),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
}

TEST(DualSolutionTest, InvalidDualValues) {
  ModelStorage model;
  model.AddLinearConstraint("c");

  DualSolutionProto proto;
  proto.set_objective_value(9.0);
  proto.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  proto.mutable_dual_values()->add_ids(0);

  EXPECT_THAT(DualSolution::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid dual_values")));
}

TEST(DualSolutionTest, InvalidReducedCosts) {
  ModelStorage model;
  model.AddVariable("x");

  DualSolutionProto proto;
  proto.set_objective_value(9.0);
  proto.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  proto.mutable_reduced_costs()->add_ids(0);

  EXPECT_THAT(DualSolution::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid reduced_costs")));
}

TEST(DualSolutionTest, InvalidQuadraticDualValues) {
  ModelStorage model;
  const QuadraticConstraint c(
      &model, model.AddAtomicConstraint(QuadraticConstraintData{.name = "c"}));

  DualSolutionProto proto;
  proto.set_objective_value(9.0);
  proto.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  proto.mutable_quadratic_dual_values()->add_ids(0);

  EXPECT_THAT(DualSolution::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid quadratic_dual_values")));
}

TEST(DualSolutionTest, FeasibilityStatusUnspecified) {
  ModelStorage model;
  DualSolutionProto proto;
  EXPECT_THAT(DualSolution::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("feasibility_status")));
}

TEST(DualRayTest, ProtoRoundTripTest) {
  ModelStorage model;
  const Variable x(&model, model.AddVariable("x"));
  const Variable y(&model, model.AddVariable("y"));
  const LinearConstraint c(&model, model.AddLinearConstraint("c"));
  const LinearConstraint d(&model, model.AddLinearConstraint("d"));

  DualRayProto proto;
  for (int i : {0, 1}) {
    proto.mutable_reduced_costs()->add_ids(i);
  }
  for (double v : {2.0, 1.0}) {
    proto.mutable_reduced_costs()->add_values(v);
  }
  for (int i : {0, 1}) {
    proto.mutable_dual_values()->add_ids(i);
  }
  for (double v : {3.0, 4.0}) {
    proto.mutable_dual_values()->add_values(v);
  }

  const DualRay expected = {.dual_values = {{c, 3.0}, {d, 4.0}},
                            .reduced_costs = {{x, 2.0}, {y, 1.0}}};

  // Test one way
  EXPECT_THAT(DualRay::FromProto(&model, proto),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
  // Test round trip
  EXPECT_THAT(DualRay::FromProto(&model, expected.Proto()),
              IsOkAndHolds(IsNear(expected, /*tolerance=*/0)));
}

TEST(DualRayTest, InvalidDualValues) {
  ModelStorage model;
  model.AddLinearConstraint("c");

  DualRayProto proto;
  proto.mutable_dual_values()->add_ids(0);

  EXPECT_THAT(DualRay::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid dual_values")));
}

TEST(DualRayTest, InvalidReducedCosts) {
  ModelStorage model;
  model.AddVariable("x");

  DualRayProto proto;
  proto.mutable_reduced_costs()->add_ids(0);

  EXPECT_THAT(DualRay::FromProto(&model, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid reduced_costs")));
}

TEST(BasisTest, ProtoRoundTripTest) {
  ModelStorage model;
  const Variable x(&model, model.AddVariable("x"));
  const Variable y(&model, model.AddVariable("y"));
  const LinearConstraint c(&model, model.AddLinearConstraint("c"));
  const LinearConstraint d(&model, model.AddLinearConstraint("d"));

  BasisProto proto;
  for (int i : {0, 1}) {
    proto.mutable_variable_status()->add_ids(i);
  }
  proto.mutable_variable_status()->add_values(BASIS_STATUS_AT_UPPER_BOUND);
  proto.mutable_variable_status()->add_values(BASIS_STATUS_FREE);
  for (int i : {0, 1}) {
    proto.mutable_constraint_status()->add_ids(i);
  }
  proto.mutable_constraint_status()->add_values(BASIS_STATUS_AT_LOWER_BOUND);
  proto.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
  proto.set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);

  const Basis expected = {.constraint_status = {{c, BasisStatus::kAtLowerBound},
                                                {d, BasisStatus::kBasic}},
                          .variable_status = {{x, BasisStatus::kAtUpperBound},
                                              {y, BasisStatus::kFree}},
                          .basic_dual_feasibility = SolutionStatus::kFeasible};

  EXPECT_OK(expected.CheckModelStorage(&model));

  // Test one way
  EXPECT_THAT(Basis::FromProto(&model, proto), IsOkAndHolds(BasisIs(expected)));
  // Test round trip
  EXPECT_THAT(Basis::FromProto(&model, expected.Proto()),
              IsOkAndHolds(BasisIs(expected)));
}

TEST(BasisTest, VariablesAndConstraintsDifferentModels) {
  ModelStorage model_a;
  const Variable a_x(&model_a, model_a.AddVariable("x"));
  ModelStorage model_b;
  const LinearConstraint b_c(&model_b, model_b.AddLinearConstraint("c"));

  const Basis basis = {.constraint_status = {{b_c, BasisStatus::kAtLowerBound}},
                       .variable_status = {{a_x, BasisStatus::kAtUpperBound}}};

  EXPECT_THAT(basis.CheckModelStorage(&model_a),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
  EXPECT_THAT(basis.CheckModelStorage(&model_b),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(BasisTest, FromProtoUnspecifiedBasicDualFeasibility) {
  ModelStorage storage;
  BasisProto proto;
  ASSERT_OK_AND_ASSIGN(const Basis basis, Basis::FromProto(&storage, proto));
  EXPECT_THAT(basis.variable_status, IsEmpty());
  EXPECT_THAT(basis.constraint_status, IsEmpty());
  EXPECT_EQ(basis.basic_dual_feasibility, std::nullopt);
}

TEST(SolutionTest, ProtoRoundTripTest) {
  ModelStorage model;
  const Variable x(&model, model.AddVariable("x"));
  const Variable y(&model, model.AddVariable("y"));
  const LinearConstraint c(&model, model.AddLinearConstraint("c"));
  const LinearConstraint d(&model, model.AddLinearConstraint("d"));
  const Objective o =
      Objective::Auxiliary(&model, model.AddAuxiliaryObjective(2));

  SolutionProto proto;

  PrimalSolutionProto& primal_proto = *proto.mutable_primal_solution();
  primal_proto.set_objective_value(9.0);
  (*primal_proto.mutable_auxiliary_objective_values())[o.id().value()] = 2.0;
  primal_proto.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  for (int i : {0, 1}) {
    primal_proto.mutable_variable_values()->add_ids(i);
  }
  for (double v : {2.0, 1.0}) {
    primal_proto.mutable_variable_values()->add_values(v);
  }

  DualSolutionProto& dual_proto = *proto.mutable_dual_solution();
  dual_proto.set_objective_value(9.0);
  dual_proto.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  for (int i : {0, 1}) {
    dual_proto.mutable_reduced_costs()->add_ids(i);
  }
  for (double v : {2.0, 1.0}) {
    dual_proto.mutable_reduced_costs()->add_values(v);
  }
  for (int i : {0, 1}) {
    dual_proto.mutable_dual_values()->add_ids(i);
  }
  for (double v : {3.0, 4.0}) {
    dual_proto.mutable_dual_values()->add_values(v);
  }

  BasisProto& basis_proto = *proto.mutable_basis();
  for (int i : {0, 1}) {
    basis_proto.mutable_variable_status()->add_ids(i);
  }
  basis_proto.mutable_variable_status()->add_values(
      BASIS_STATUS_AT_UPPER_BOUND);
  basis_proto.mutable_variable_status()->add_values(BASIS_STATUS_FREE);
  for (int i : {0, 1}) {
    basis_proto.mutable_constraint_status()->add_ids(i);
  }
  basis_proto.mutable_constraint_status()->add_values(
      BASIS_STATUS_AT_LOWER_BOUND);
  basis_proto.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
  basis_proto.set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);

  const Solution expected{
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 2.0}, {y, 1.0}},
                         .objective_value = 9.0,
                         .auxiliary_objective_values = {{o, 2.0}},
                         .feasibility_status = SolutionStatus::kInfeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 3.0}, {d, 4.0}},
                       .reduced_costs = {{x, 2.0}, {y, 1.0}},
                       .objective_value = 9.0,
                       .feasibility_status = SolutionStatus::kFeasible},
      .basis = Basis{.constraint_status = {{c, BasisStatus::kAtLowerBound},
                                           {d, BasisStatus::kBasic}},
                     .variable_status = {{x, BasisStatus::kAtUpperBound},
                                         {y, BasisStatus::kFree}},
                     .basic_dual_feasibility = SolutionStatus::kFeasible}};

  // Test one way
  EXPECT_THAT(
      Solution::FromProto(&model, proto),
      IsOkAndHolds(IsNear(expected, SolutionMatcherOptions{.tolerance = 0.0})));
  // Test round trip
  EXPECT_THAT(
      Solution::FromProto(&model, expected.Proto()),
      IsOkAndHolds(IsNear(expected, SolutionMatcherOptions{.tolerance = 0.0})));
}

TEST(SolutionTest, FromProtoInvalidPrimalSolution) {
  ModelStorage storage;
  storage.AddVariable("x");
  SolutionProto proto;
  proto.mutable_primal_solution()->mutable_variable_values()->add_ids(0);
  proto.mutable_primal_solution()->set_feasibility_status(
      SOLUTION_STATUS_FEASIBLE);
  EXPECT_THAT(Solution::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("primal_solution")));
}

TEST(SolutionTest, FromProtoInvalidDualSolution) {
  ModelStorage storage;
  storage.AddLinearConstraint("c");
  SolutionProto proto;
  proto.mutable_dual_solution()->mutable_dual_values()->add_ids(0);
  proto.mutable_dual_solution()->set_feasibility_status(
      SOLUTION_STATUS_FEASIBLE);
  EXPECT_THAT(
      Solution::FromProto(&storage, proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("dual_solution")));
}

TEST(SolutionTest, FromProtoInvalidBasis) {
  ModelStorage storage;
  storage.AddVariable("c");
  SolutionProto proto;
  proto.mutable_basis()->mutable_variable_status()->add_ids(0);
  EXPECT_THAT(Solution::FromProto(&storage, proto),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("basis")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
