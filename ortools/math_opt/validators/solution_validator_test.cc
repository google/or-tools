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

#include "ortools/math_opt/validators/solution_validator.h"

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextProto;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

SparseDoubleVectorProto MakeSparseDoubleVector(
    std::initializer_list<std::pair<int64_t, double>> pairs) {
  SparseDoubleVectorProto ret;
  for (const auto [id, value] : pairs) {
    ret.add_ids(id);
    ret.add_values(value);
  }
  return ret;
}

ModelSummary MakeModelSummary(
    std::initializer_list<int64_t> variables,
    std::initializer_list<int64_t> linear_constraints) {
  ModelSummary ret;
  for (const auto v : variables) {
    CHECK_OK(ret.variables.Insert(v, ""));
  }
  for (const auto c : linear_constraints) {
    CHECK_OK(ret.linear_constraints.Insert(c, ""));
  }
  return ret;
}

PrimalSolutionProto MakePrimalSolutionProto(
    std::initializer_list<std::pair<int64_t, double>> variable_values,
    const double objective_value) {
  PrimalSolutionProto solution;
  *solution.mutable_variable_values() = MakeSparseDoubleVector(variable_values);
  solution.set_objective_value(objective_value);
  solution.set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
  return solution;
}

PrimalRayProto MakePrimalRayProto(
    std::initializer_list<std::pair<int64_t, double>> variable_values) {
  PrimalRayProto ray;
  *ray.mutable_variable_values() = MakeSparseDoubleVector(variable_values);
  return ray;
}

DualSolutionProto MakeDualSolutionProto(
    std::initializer_list<std::pair<int64_t, double>> dual_values,
    std::initializer_list<std::pair<int64_t, double>> reduced_costs,
    double objective_value) {
  DualSolutionProto solution;
  *solution.mutable_dual_values() = MakeSparseDoubleVector(dual_values);
  *solution.mutable_reduced_costs() = MakeSparseDoubleVector(reduced_costs);
  solution.set_objective_value(objective_value);
  solution.set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
  return solution;
}

DualRayProto MakeDualRayProto(
    std::initializer_list<std::pair<int64_t, double>> dual_values,
    std::initializer_list<std::pair<int64_t, double>> reduced_costs) {
  DualRayProto ray;
  *ray.mutable_dual_values() = MakeSparseDoubleVector(dual_values);
  *ray.mutable_reduced_costs() = MakeSparseDoubleVector(reduced_costs);
  return ray;
}

TEST(ValidatePrimalSolutionVectorTest, EmptyPrimalSolution) {
  SparseDoubleVectorProto primal_solution_vector;
  EXPECT_OK(
      ValidatePrimalSolutionVector(primal_solution_vector, {}, ModelSummary()));
}

TEST(ValidatePrimalSolutionVectorTest, NonEmptyPrimalSolution) {
  EXPECT_OK(
      ValidatePrimalSolutionVector(MakeSparseDoubleVector({{0, 1.0}, {1, 2.0}}),
                                   {}, MakeModelSummary({0, 1}, {})));
}

TEST(ValidatePrimalSolutionVectorTest, NaNValuePrimalSolution) {
  EXPECT_THAT(ValidatePrimalSolutionVector(
                  MakeSparseDoubleVector({{0, std::nan("")}, {1, 2.0}}), {},
                  MakeModelSummary({0, 1}, {})),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("NaN")));
}

TEST(ValidatePrimalSolutionVectorTest, InfValuePrimalSolution) {
  EXPECT_THAT(ValidatePrimalSolutionVector(
                  MakeSparseDoubleVector({{0, kInf}, {1, 2.0}}), {},
                  MakeModelSummary({0, 1}, {})),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("inf")));
}

TEST(ValidatePrimalSolutionVectorTest, FilteredValueByIdPrimalSolution) {
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.add_filtered_ids(1);
  EXPECT_OK(ValidatePrimalSolutionVector(MakeSparseDoubleVector({{1, 2.0}}),
                                         filter, MakeModelSummary({0, 1}, {})));
}

TEST(ValidatePrimalSolutionVectorTest, FilteredZerosPrimalSolution) {
  SparseVectorFilterProto filter;
  filter.set_skip_zero_values(true);
  EXPECT_OK(ValidatePrimalSolutionVector(MakeSparseDoubleVector({{1, 2.0}}),
                                         filter, MakeModelSummary({0, 1}, {})));
}

TEST(ValidatePrimalSolutionVectorTest, UnfilteredValuePrimalSolution) {
  // We only test one possible filter, assuming the check is implemented using
  // the SparseVectorFilterPredicate. Hence we don't repeat all cases here.
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.add_filtered_ids(1);
  EXPECT_THAT(
      ValidatePrimalSolutionVector(MakeSparseDoubleVector({{0, 1.0}, {1, 2.0}}),
                                   filter, MakeModelSummary({0, 1}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("filtered")));
}

TEST(ValidateSuggestedSolutionTest, EmptyPrimalSolution) {
  SparseDoubleVectorProto primal_solution_vector;
  EXPECT_OK(ValidateSuggestedSolution(primal_solution_vector, ModelSummary()));
}

TEST(ValidateSuggestedSolutionTest, FullPrimalSolution) {
  EXPECT_OK(
      ValidateSuggestedSolution(MakeSparseDoubleVector({{0, 1.0}, {1, 2.0}}),
                                MakeModelSummary({0, 1}, {})));
}

TEST(ValidateSuggestedSolutionTest, PartialPrimalSolution) {
  EXPECT_OK(ValidateSuggestedSolution(MakeSparseDoubleVector({{1, 2.0}}),
                                      MakeModelSummary({0, 1}, {})));
}

TEST(ValidateSuggestedSolutionTest, NaNValuePrimalSolution) {
  EXPECT_THAT(ValidateSuggestedSolution(
                  MakeSparseDoubleVector({{0, std::nan("")}, {1, 2.0}}),
                  MakeModelSummary({0, 1}, {})),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("NaN")));
}

TEST(ValidateSuggestedSolutionTest, InfValuePrimalSolution) {
  EXPECT_THAT(
      ValidateSuggestedSolution(MakeSparseDoubleVector({{0, kInf}, {1, 2.0}}),
                                MakeModelSummary({0, 1}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("inf")));
}

TEST(ValidatePrimalSolutionTest, ValidatePrimalSolutionVectorIsCalled) {
  EXPECT_THAT(
      ValidatePrimalSolution(
          MakePrimalSolutionProto({{0, kInf}, {1, 2.0}}, 3.0), {},
          MakeModelSummary({0, 1}, {})),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("inf"),
                HasSubstr("invalid PrimalSolutionProto.variable_values"))));
}

TEST(ValidatePrimalSolutionTest, InfObjectivePrimalSolution) {
  EXPECT_THAT(
      ValidatePrimalSolution(
          MakePrimalSolutionProto({{0, 1.0}, {1, 2.0}}, kInf), {},
          MakeModelSummary({0, 1}, {})),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("inf"),
                HasSubstr("invalid PrimalSolutionProto.objective_value"))));
}

TEST(ValidatePrimalSolutionTest, NaNObjectivePrimalSolution) {
  EXPECT_THAT(
      ValidatePrimalSolution(
          MakePrimalSolutionProto({{0, 1.0}, {1, 2.0}}, std::nan("")), {},
          MakeModelSummary({0, 1}, {})),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("NaN"),
                HasSubstr("invalid PrimalSolutionProto.objective_value"))));
}

TEST(ValidatePrimalSolutionTest, MissingObjectivePrimalSolution) {
  ASSERT_OK_AND_ASSIGN(const PrimalSolutionProto primal_solution,
                       ParseTextProto<PrimalSolutionProto>(R"pb(
                         variable_values: {
                           ids: [ 0 ]
                           values: [ 1 ]
                         }
                         feasibility_status: SOLUTION_STATUS_FEASIBLE
                       )pb"));
  EXPECT_OK(
      ValidatePrimalSolution(primal_solution, {}, MakeModelSummary({0}, {})));
}

TEST(ValidatePrimalSolutionTest, AuxiliaryObjectiveInvalidId) {
  ModelSummary summary;
  PrimalSolutionProto primal_solution = MakePrimalSolutionProto({}, 0.0);
  (*primal_solution.mutable_auxiliary_objective_values())[2] = 3.0;
  EXPECT_THAT(
      ValidatePrimalSolution(primal_solution, {}, summary),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(
              HasSubstr("ID: 2"),
              HasSubstr(
                  "invalid PrimalSolutionProto.auxiliary_objective_values"))));
}

TEST(ValidatePrimalSolutionTest, InfAuxiliaryObjectivePrimalSolution) {
  ModelSummary summary;
  ASSERT_OK(summary.auxiliary_objectives.Insert(2, ""));
  PrimalSolutionProto primal_solution = MakePrimalSolutionProto({}, 0.0);
  (*primal_solution.mutable_auxiliary_objective_values())[2] = kInf;
  EXPECT_THAT(
      ValidatePrimalSolution(primal_solution, {}, summary),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(
              HasSubstr("inf"),
              HasSubstr(
                  "invalid PrimalSolutionProto.auxiliary_objective_values"))));
}

TEST(ValidatePrimalSolutionTest, NaNAuxiliaryObjectivePrimalSolution) {
  ModelSummary summary;
  ASSERT_OK(summary.auxiliary_objectives.Insert(2, ""));
  PrimalSolutionProto primal_solution = MakePrimalSolutionProto({}, 0.0);
  (*primal_solution.mutable_auxiliary_objective_values())[2] = std::nan("");
  EXPECT_THAT(
      ValidatePrimalSolution(primal_solution, {}, summary),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(
              HasSubstr("NaN"),
              HasSubstr(
                  "invalid PrimalSolutionProto.auxiliary_objective_values"))));
}

TEST(ValidatePrimalSolutionTest, InvalidSolutionStatus) {
  ASSERT_OK_AND_ASSIGN(const PrimalSolutionProto primal_solution,
                       ParseTextProto<PrimalSolutionProto>(R"pb(
                         variable_values: {
                           ids: [ 0 ]
                           values: [ 1 ]
                         }
                         objective_value: 0,
                         feasibility_status: -1
                       )pb"));
  EXPECT_THAT(
      ValidatePrimalSolution(primal_solution, {}, MakeModelSummary({0}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("status = -1"),
                     HasSubstr("PrimalSolutionProto.feasibility_status"))));
}

TEST(ValidatePrimalSolutionTest, SolutionStatusUnspecified) {
  ASSERT_OK_AND_ASSIGN(const PrimalSolutionProto primal_solution,
                       ParseTextProto<PrimalSolutionProto>(R"pb(
                         variable_values: {
                           ids: [ 0 ]
                           values: [ 1 ]
                         }
                         objective_value: 0,
                         feasibility_status: SOLUTION_STATUS_UNSPECIFIED
                       )pb"));
  EXPECT_THAT(
      ValidatePrimalSolution(primal_solution, {}, MakeModelSummary({0}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("status = SOLUTION_STATUS_UNSPECIFIED"),
                     HasSubstr("PrimalSolutionProto.feasibility_status"))));
}

TEST(ValidatePrimalRayTest, EmptyPrimalRayProto) {
  EXPECT_OK(ValidatePrimalRay(PrimalRayProto(), {}, ModelSummary()));
}

TEST(ValidatePrimalRayTest, NonEmptyPrimalRayProto) {
  EXPECT_OK(ValidatePrimalRay(MakePrimalRayProto({{0, 1.0}, {1, 2.0}}), {},
                              MakeModelSummary({0, 1}, {})));
}

TEST(ValidatePrimalRayTest, NaNValuePrimalRayProto) {
  EXPECT_THAT(
      ValidatePrimalRay(MakePrimalRayProto({{0, std::nan("")}, {1, 2.0}}), {},
                        MakeModelSummary({0, 1}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("NaN"),
                     HasSubstr("invalid PrimalRayProto.variable_values"))));
}

TEST(ValidatePrimalRayTest, InfValuePrimalRayProto) {
  EXPECT_THAT(
      ValidatePrimalRay(MakePrimalRayProto({{0, kInf}, {1, 2.0}}), {},
                        MakeModelSummary({0, 1}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("inf"),
                     HasSubstr("invalid PrimalRayProto.variable_values"))));
}

TEST(ValidatePrimalRayTest, FilteredValueByIdPrimalRayProto) {
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.add_filtered_ids(1);
  EXPECT_OK(ValidatePrimalRay(MakePrimalRayProto({{1, 2.0}}), filter,
                              MakeModelSummary({0, 1}, {})));
}

TEST(ValidatePrimalRayTest, FilteredZerosPrimalRayProto) {
  SparseVectorFilterProto filter;
  filter.set_skip_zero_values(true);
  EXPECT_OK(ValidatePrimalRay(MakePrimalRayProto({{1, 2.0}}), filter,
                              MakeModelSummary({0, 1}, {})));
}

TEST(ValidatePrimalRayTest, UnfilteredValuePrimalRayProto) {
  // We only test one possible filter, assuming the check is implemented using
  // the SparseVectorFilterPredicate. Hence we don't repeat all cases here.
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.add_filtered_ids(1);
  EXPECT_THAT(
      ValidatePrimalRay(MakePrimalRayProto({{0, 1.0}, {1, 2.0}}), filter,
                        MakeModelSummary({0, 1}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("filtered"),
                     HasSubstr("invalid PrimalRayProto.variable_values"))));
}

TEST(ValidateDualSolutionTest, EmptyDualSolutionProto) {
  DualSolutionProto dual_solution;
  dual_solution.set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_OK(ValidateDualSolution(dual_solution, {}, ModelSummary()));
}

TEST(ValidateDualSolutionTest, NonEmptyDualSolutionProto) {
  EXPECT_OK(ValidateDualSolution(
      MakeDualSolutionProto({{0, 1.0}, {1, 2.0}}, {{10, 1.0}, {11, 2.0}}, 3.0),
      {}, MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualSolutionTest, NaNConstraintDualDualSolutionProto) {
  EXPECT_THAT(
      ValidateDualSolution(MakeDualSolutionProto({{0, std::nan("")}, {1, 2.0}},
                                                 {{10, 1.0}, {11, 2.0}}, 3.0),
                           {}, MakeModelSummary({0, 1}, {10, 11})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("NaN"),
                     HasSubstr("invalid DualSolutionProto.dual_values"))));
}

TEST(ValidateDualSolutionTest, InfConstraintDualDualSolutionProto) {
  EXPECT_THAT(
      ValidateDualSolution(MakeDualSolutionProto({{0, kInf}, {1, 2.0}},
                                                 {{10, 1.0}, {11, 2.0}}, 3.0),
                           {}, MakeModelSummary({10, 11}, {0, 1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("inf"),
                     HasSubstr("invalid DualSolutionProto.dual_values"))));
}

TEST(ValidateDualSolutionTest,
     FilteredValueByIdConstraintDualDualSolutionProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_dual_values_filter()->set_filter_by_ids(true);
  parameters.mutable_dual_values_filter()->add_filtered_ids(1);
  EXPECT_OK(ValidateDualSolution(
      MakeDualSolutionProto({{1, 2.0}}, {{10, 1.0}, {11, 2.0}}, 3.0),
      parameters, MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualSolutionTest, FilteredZerosConstraintDualDualSolutionProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_dual_values_filter()->set_skip_zero_values(true);
  EXPECT_OK(ValidateDualSolution(
      MakeDualSolutionProto({{1, 2.0}}, {{10, 1.0}, {11, 2.0}}, 3.0),
      parameters, MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualSolutionTest, UnfilteredConstraintDualDualSolutionProto) {
  // We only test one possible filter, assuming the check is implemented using
  // the SparseVectorFilterPredicate. Hence we don't repeat all cases here.
  ModelSolveParametersProto parameters;
  parameters.mutable_dual_values_filter()->set_filter_by_ids(true);
  parameters.mutable_dual_values_filter()->add_filtered_ids(1);
  EXPECT_THAT(
      ValidateDualSolution(MakeDualSolutionProto({{0, 1.0}, {1, 2.0}},
                                                 {{10, 1.0}, {11, 2.0}}, 3.0),
                           parameters, MakeModelSummary({10, 11}, {0, 1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("filtered"),
                     HasSubstr("invalid DualSolutionProto.dual_values"))));
}

TEST(ValidateDualSolutionTest, NaNVarReducedCostDualSolutionProto) {
  EXPECT_THAT(
      ValidateDualSolution(
          MakeDualSolutionProto({{0, 1.0}, {1, 2.0}},
                                {{10, std::nan("")}, {11, 2.0}}, 3.0),
          {}, MakeModelSummary({10, 11}, {0, 1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("NaN"),
                     HasSubstr("invalid DualSolutionProto.reduced_costs"))));
}

TEST(ValidateDualSolutionTest, InfVarReducedCostDualSolutionProto) {
  EXPECT_THAT(
      ValidateDualSolution(MakeDualSolutionProto({{0, 1.0}, {1, 2.0}},
                                                 {{10, kInf}, {11, 2.0}}, 3.0),
                           {}, MakeModelSummary({10, 11}, {0, 1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("inf"),
                     HasSubstr("invalid DualSolutionProto.reduced_costs"))));
}

TEST(ValidateDualSolutionTest,
     FilteredValueByIdVarReducedCostDualSolutionProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  parameters.mutable_reduced_costs_filter()->add_filtered_ids(11);
  EXPECT_OK(ValidateDualSolution(
      MakeDualSolutionProto({{0, 1.0}, {1, 2.0}}, {{11, 2.0}}, 3.0), parameters,
      MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualSolutionTest, FilteredZerosVarReducedCostDualSolutionProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_reduced_costs_filter()->set_skip_zero_values(true);
  EXPECT_OK(ValidateDualSolution(
      MakeDualSolutionProto({{0, 1.0}, {1, 2.0}}, {{11, 2.0}}, 3.0), parameters,
      MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualSolutionTest, UnfilteredVarReducedCostDualSolutionProto) {
  // We only test one possible filter, assuming the check is implemented using
  // the SparseVectorFilterPredicate. Hence we don't repeat all cases here.
  ModelSolveParametersProto parameters;
  parameters.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  parameters.mutable_reduced_costs_filter()->add_filtered_ids(11);
  EXPECT_THAT(
      ValidateDualSolution(MakeDualSolutionProto({{0, 1.0}, {1, 2.0}},
                                                 {{10, 1.0}, {11, 2.0}}, 3.0),
                           parameters, MakeModelSummary({10, 11}, {0, 1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("filtered"),
                     HasSubstr("invalid DualSolutionProto.reduced_costs"))));
}

TEST(ValidateDualSolutionTest, InfObjectiveDualSolutionProto) {
  EXPECT_THAT(
      ValidateDualSolution(MakeDualSolutionProto({{0, 1.0}, {1, 2.0}},
                                                 {{10, 1.0}, {11, 2.0}}, kInf),
                           {}, MakeModelSummary({10, 11}, {0, 1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("inf"),
                     HasSubstr("invalid DualSolutionProto.objective_value"))));
}

TEST(ValidateDualSolutionTest, NaNObjectiveDualSolutionProto) {
  EXPECT_THAT(
      ValidateDualSolution(
          MakeDualSolutionProto({{0, 1.0}, {1, 2.0}}, {{10, 1.0}, {11, 2.0}},
                                std::nan("")),
          {}, MakeModelSummary({10, 11}, {0, 1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("NaN"),
                     HasSubstr("invalid DualSolutionProto.objective_value"))));
}

TEST(ValidateDualSolutionTest, MissingObjectiveDualSolution) {
  ASSERT_OK_AND_ASSIGN(const DualSolutionProto dual_solution,
                       ParseTextProto<DualSolutionProto>(R"pb(
                         reduced_costs: {
                           ids: [ 0 ]
                           values: [ 1 ]
                         }
                         feasibility_status: SOLUTION_STATUS_FEASIBLE
                       )pb"));
  EXPECT_OK(ValidateDualSolution(dual_solution, {}, MakeModelSummary({0}, {})));
}

TEST(ValidateDualSolutionTest, InvalidSolutionStatus) {
  ASSERT_OK_AND_ASSIGN(const DualSolutionProto dual_solution,
                       ParseTextProto<DualSolutionProto>(R"pb(
                         reduced_costs: {
                           ids: [ 0 ]
                           values: [ 1 ]
                         }
                         objective_value: 0,
                         feasibility_status: -1
                       )pb"));
  EXPECT_THAT(
      ValidateDualSolution(dual_solution, {}, MakeModelSummary({0}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("status = -1"),
                     HasSubstr("DualSolutionProto.feasibility_status"))));
}

TEST(ValidateDualSolutionTest, SolutionStatusUnspecified) {
  ASSERT_OK_AND_ASSIGN(const DualSolutionProto dual_solution,
                       ParseTextProto<DualSolutionProto>(R"pb(
                         reduced_costs: {
                           ids: [ 0 ]
                           values: [ 1 ]
                         }
                         objective_value: 0,
                         feasibility_status: SOLUTION_STATUS_UNSPECIFIED
                       )pb"));
  EXPECT_THAT(
      ValidateDualSolution(dual_solution, {}, MakeModelSummary({0}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("status = SOLUTION_STATUS_UNSPECIFIED"),
                     HasSubstr("DualSolutionProto.feasibility_status"))));
}

TEST(ValidateDualRayTest, EmptyDualRayProto) {
  EXPECT_OK(ValidateDualRay(DualRayProto(), {}, ModelSummary()));
}

TEST(ValidateDualRayTest, NonEmptyDualRayProto) {
  EXPECT_OK(ValidateDualRay(
      MakeDualRayProto({{0, 1.0}, {1, 2.0}}, {{10, 1.0}, {11, 2.0}}), {},
      MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualRayTest, NaNConstraintDualDualRayProto) {
  EXPECT_THAT(ValidateDualRay(MakeDualRayProto({{0, std::nan("")}, {1, 2.0}},
                                               {{10, 1.0}, {11, 2.0}}),
                              {}, MakeModelSummary({0, 1}, {10, 11})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"),
                             HasSubstr("invalid DualRayProto.dual_values"))));
}

TEST(ValidateDualRayTest, InfConstraintDualDualRayProto) {
  EXPECT_THAT(ValidateDualRay(MakeDualRayProto({{0, kInf}, {1, 2.0}},
                                               {{10, 1.0}, {11, 2.0}}),
                              {}, MakeModelSummary({0, 1}, {10, 11})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("inf"),
                             HasSubstr("invalid DualRayProto.dual_values"))));
}

TEST(ValidateDualRayTest, FilteredValueByIdConstraintDualDualRayProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_dual_values_filter()->set_filter_by_ids(true);
  parameters.mutable_dual_values_filter()->add_filtered_ids(1);
  EXPECT_OK(
      ValidateDualRay(MakeDualRayProto({{1, 2.0}}, {{10, 1.0}, {11, 2.0}}),
                      parameters, MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualRayTest, FilteredZerosConstraintDualDualRayProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_dual_values_filter()->set_skip_zero_values(true);
  EXPECT_OK(
      ValidateDualRay(MakeDualRayProto({{1, 2.0}}, {{10, 1.0}, {11, 2.0}}),
                      parameters, MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualRayTest, UnfilteredConstraintDualDualRayProto) {
  // We only test one possible filter, assuming the check is implemented using
  // the SparseVectorFilterPredicate. Hence we don't repeat all cases here.
  ModelSolveParametersProto parameters;
  parameters.mutable_dual_values_filter()->set_filter_by_ids(true);
  parameters.mutable_dual_values_filter()->add_filtered_ids(1);
  EXPECT_THAT(ValidateDualRay(MakeDualRayProto({{0, 1.0}, {1, 2.0}},
                                               {{10, 1.0}, {11, 2.0}}),
                              parameters, MakeModelSummary({10, 11}, {0, 1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("filtered"),
                             HasSubstr("invalid DualRayProto.dual_values"))));
}

TEST(ValidateDualRayTest, NaNVarReducedCostDualRayProto) {
  EXPECT_THAT(ValidateDualRay(MakeDualRayProto({{0, 1.0}, {1, 2.0}},
                                               {{10, std::nan("")}, {11, 2.0}}),
                              {}, MakeModelSummary({10, 11}, {0, 1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"),
                             HasSubstr("invalid DualRayProto.reduced_costs"))));
}

TEST(ValidateDualRayTest, InfVarReducedCostDualRayProto) {
  EXPECT_THAT(ValidateDualRay(MakeDualRayProto({{0, 1.0}, {1, 2.0}},
                                               {{10, kInf}, {11, 2.0}}),
                              {}, MakeModelSummary({10, 11}, {0, 1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("inf"),
                             HasSubstr("invalid DualRayProto.reduced_costs"))));
}

TEST(ValidateDualRayTest, FilteredValueByIdVarReducedCostDualRayProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  parameters.mutable_reduced_costs_filter()->add_filtered_ids(11);
  EXPECT_OK(ValidateDualRay(MakeDualRayProto({{0, 1.0}, {1, 2.0}}, {{11, 2.0}}),
                            parameters, MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualRayTest, FilteredZerosVarReducedCostDualRayProto) {
  ModelSolveParametersProto parameters;
  parameters.mutable_reduced_costs_filter()->set_skip_zero_values(true);
  EXPECT_OK(ValidateDualRay(MakeDualRayProto({{0, 1.0}, {1, 2.0}}, {{11, 2.0}}),
                            parameters, MakeModelSummary({10, 11}, {0, 1})));
}

TEST(ValidateDualRayTest, UnfilteredVarReducedCostDualRayProto) {
  // We only test one possible filter, assuming the check is implemented using
  // the SparseVectorFilterPredicate. Hence we don't repeat all cases here.
  ModelSolveParametersProto parameters;
  parameters.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  parameters.mutable_reduced_costs_filter()->add_filtered_ids(11);
  EXPECT_THAT(ValidateDualRay(MakeDualRayProto({{0, 1.0}, {1, 2.0}},
                                               {{10, 1.0}, {11, 2.0}}),
                              parameters, MakeModelSummary({10, 11}, {0, 1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("filtered"),
                             HasSubstr("invalid DualRayProto.reduced_costs"))));
}

absl::StatusOr<BasisProto> ParseBasisProto(absl::string_view basis) {
  return ParseTextProto<BasisProto>(basis);
}

// Basis corresponds to feasible solution x0 = 1, x1 = 0, x2 = 0
// for
//
// x0 + x1 + x2 == 1
//      0 <= x0 <= 2
//      0 <= x1 <= 1
//   -inf <= x2 <= inf

absl::StatusOr<BasisProto> SimpleBasisProto() {
  return ParseBasisProto(R"pb(
    constraint_status: {
      ids: [ 0 ],
      values: [ BASIS_STATUS_FIXED_VALUE ]
    }
    variable_status: {
      ids: [ 0, 1, 2 ],
      values: [
        BASIS_STATUS_BASIC,
        BASIS_STATUS_AT_UPPER_BOUND,
        BASIS_STATUS_FREE
      ]
    }
    basic_dual_feasibility: SOLUTION_STATUS_UNDETERMINED
  )pb");
}

ModelSummary SimpleModelSummary() {
  ModelSummary summary;
  summary.variables = {{0, "x0"}, {1, "x1"}, {2, "x2"}};
  summary.linear_constraints = {{0, "equality"}};
  return summary;
}

TEST(ValidateBasisTest, SimpleBasisProtoValid) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  EXPECT_OK(ValidateBasis(basis, SimpleModelSummary()));
}

TEST(ValidateBasisTest, InvalidBasicDualFeasibility) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNSPECIFIED);
  EXPECT_THAT(ValidateBasis(basis, SimpleModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("BasisProto.basic_dual_feasibility")));
}

TEST(ValidateBasisTest, InvalidBasicDualFeasibilityOkWhenDeactivated) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNSPECIFIED);
  EXPECT_OK(ValidateBasis(basis, SimpleModelSummary(),
                          /*check_dual_feasibility=*/false));
}

TEST(ValidateBasisTest, ConstraintStatusInvalidExtraId) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_constraint_status()->add_ids(4);
  EXPECT_THAT(ValidateBasis(basis, SimpleModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("BasisProto.constraint_status invalid")));
}

TEST(ValidateBasisTest, ConstraintStatusInvalidNegativeId) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_constraint_status()->set_ids(0, -1);
  EXPECT_THAT(
      ValidateBasis(basis, SimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("nonnegative"),
                     HasSubstr("BasisProto.constraint_status invalid"))));
}

TEST(ValidateBasisTest, ConstraintStatusInvalidStatus) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_constraint_status()->set_values(0, BASIS_STATUS_UNSPECIFIED);

  EXPECT_THAT(
      ValidateBasis(basis, SimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ound BASIS_STATUS_UNSPECIFIED"),
                     HasSubstr("BasisProto.constraint_status invalid"))));
}

TEST(ValidateBasisTest, ConstraintStatusNotValid) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_constraint_status()->set_values(
      0, static_cast<BasisStatusProto>(BasisStatusProto_MAX + 5));

  EXPECT_THAT(
      ValidateBasis(basis, SimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("invalid status"),
                     HasSubstr("BasisProto.constraint_status invalid"))));
}

TEST(ValidateBasisTest, VariableStatusInvalidExtraId) {
  // Just check that SparseBasisStatusVectorIsValid is called for variables.
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_variable_status()->add_ids(4);
  EXPECT_THAT(ValidateBasis(basis, SimpleModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("BasisProto.variable_status invalid")));
}

TEST(ValidateBasisTest, MissingConstraintStatusOk) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};

  ModelSolveParametersProto parameters;
  BasisProto basis;
  basis.mutable_variable_status()->add_ids(2);
  basis.mutable_variable_status()->add_values(BASIS_STATUS_FIXED_VALUE);
  basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_OK(ValidateBasis(basis, summary));
}

TEST(ValidateBasisTest, MissingConstraintStatusNotOk) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  BasisProto basis;
  basis.mutable_variable_status()->add_ids(2);
  basis.mutable_variable_status()->add_values(BASIS_STATUS_FIXED_VALUE);
  basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_THAT(
      ValidateBasis(basis, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("BasisProto.constraint_status.ids has size"),
                     HasSubstr("model_summary.linear_constraints"))));
}

TEST(ValidateBasisTest, MissingVariableStatusNotOk) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  BasisProto basis;
  basis.mutable_constraint_status()->add_ids(0);
  basis.mutable_constraint_status()->add_values(BASIS_STATUS_FIXED_VALUE);
  basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_THAT(
      ValidateBasis(basis, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("BasisProto.variable_status.ids has size"),
                     HasSubstr("model_summary.variables"))));
}

TEST(ValidateBasisTest, ConstraintsExtraId) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_constraint_status()->add_ids(4);
  basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
  EXPECT_THAT(
      ValidateBasis(basis, SimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("BasisProto.constraint_status.ids has size"),
                     HasSubstr("model_summary.linear_constraints"))));
}

TEST(ValidateBasisTest, ConstraintsMissingId) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_constraint_status()->set_ids(0, 1);
  EXPECT_THAT(
      ValidateBasis(basis, SimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Id:"),
                     HasSubstr("BasisProto.constraint_status.ids is missing "
                               "from model_summary.linear_constraints"))));
}

TEST(ValidateBasisTest, VariableExtraId) {
  // Just make sure IsFiltered is called for variables.
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_variable_status()->add_ids(4);
  basis.mutable_variable_status()->add_values(BASIS_STATUS_BASIC);
  EXPECT_THAT(
      ValidateBasis(basis, SimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("BasisProto.variable_status.ids has size"),
                     HasSubstr("model_summary.variables"))));
}

TEST(ValidateBasisTest, InconsistentNonBasics) {
  ASSERT_OK_AND_ASSIGN(BasisProto basis, SimpleBasisProto());
  basis.mutable_constraint_status()->set_values(0, BASIS_STATUS_BASIC);

  EXPECT_THAT(
      ValidateBasis(basis, SimpleModelSummary()),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("inconsistent number of non-basic variable+constraints")));
}

PrimalSolutionProto VerySimplePrimalSolutionProto(SolutionStatusProto status) {
  PrimalSolutionProto solution;
  solution.mutable_variable_values()->add_ids(0);
  solution.mutable_variable_values()->add_values(1.0);
  solution.set_feasibility_status(status);
  solution.set_objective_value(0.0);
  return solution;
}

DualSolutionProto VerySimpleDualSolutionProto(SolutionStatusProto status) {
  DualSolutionProto solution;
  solution.mutable_reduced_costs()->add_ids(0);
  solution.mutable_reduced_costs()->add_values(1.0);
  solution.set_feasibility_status(status);
  solution.set_objective_value(0.0);
  return solution;
}

BasisProto VerySimpleBasisProto(SolutionStatusProto status) {
  BasisProto basis;
  basis.mutable_variable_status()->add_ids(0);
  basis.mutable_variable_status()->add_values(BASIS_STATUS_AT_UPPER_BOUND);
  basis.set_basic_dual_feasibility(status);
  return basis;
}

ModelSummary VerySimpleModelSummary() { return MakeModelSummary({0}, {}); }

TEST(ValidateSolutionTest, EmptySolution) {
  const SolutionProto solution;
  EXPECT_THAT(ValidateSolution(solution, {}, VerySimpleModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("empty")));
}

TEST(ValidateSolutionTest, PrimalSolutionsAreValidated) {
  SolutionProto solution;
  *solution.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_UNSPECIFIED);
  EXPECT_THAT(
      ValidateSolution(solution, {}, VerySimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("status = SOLUTION_STATUS_UNSPECIFIED"),
                     HasSubstr("PrimalSolutionProto.feasibility_status"))));
}

TEST(ValidateSolutionTest, DualSolutionsAreValidated) {
  SolutionProto solution;
  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_UNSPECIFIED);
  EXPECT_THAT(
      ValidateSolution(solution, {}, VerySimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("status = SOLUTION_STATUS_UNSPECIFIED"),
                     HasSubstr("DualSolutionProto.feasibility_status"))));
}

TEST(ValidateSolutionTest, BasesAreValidated) {
  SolutionProto solution;
  *solution.mutable_basis() = VerySimpleBasisProto(SOLUTION_STATUS_UNSPECIFIED);
  EXPECT_THAT(
      ValidateSolution(solution, {}, VerySimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("status = SOLUTION_STATUS_UNSPECIFIED"),
                     HasSubstr("invalid BasisProto.basic_dual_feasibility"))));
}

TEST(ValidateSolutionTest, BasisAndDualStatusCompatibilityUndeterminedBasis) {
  SolutionProto solution;
  *solution.mutable_basis() =
      VerySimpleBasisProto(SOLUTION_STATUS_UNDETERMINED);

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_OK(ValidateSolution(solution, {}, VerySimpleModelSummary()));

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  EXPECT_OK(ValidateSolution(solution, {}, VerySimpleModelSummary()));

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  EXPECT_THAT(
      ValidateSolution(solution, {}, VerySimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("incompatible basis and dual solution"))));
}

TEST(ValidateSolutionTest, BasisAndDualStatusCompatibilityFeasibleBasis) {
  SolutionProto solution;
  *solution.mutable_basis() = VerySimpleBasisProto(SOLUTION_STATUS_FEASIBLE);

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_THAT(
      ValidateSolution(solution, {}, VerySimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("incompatible basis and dual solution"))));

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  EXPECT_OK(ValidateSolution(solution, {}, VerySimpleModelSummary()));

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  EXPECT_THAT(
      ValidateSolution(solution, {}, VerySimpleModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("incompatible basis and dual solution"))));
}

TEST(ValidateSolutionTest, BasisAndDualStatusCompatibilityInfeasibleBasis) {
  SolutionProto solution;
  *solution.mutable_basis() = VerySimpleBasisProto(SOLUTION_STATUS_INFEASIBLE);

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_OK(ValidateSolution(solution, {}, VerySimpleModelSummary()));

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  EXPECT_OK(ValidateSolution(solution, {}, VerySimpleModelSummary()));

  *solution.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  EXPECT_OK(ValidateSolution(solution, {}, VerySimpleModelSummary()));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
