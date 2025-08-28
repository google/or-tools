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

#include "ortools/math_opt/solver_tests/invalid_input_tests.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_join.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status-matchers.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

using ::absl::StatusCode;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

std::ostream& operator<<(std::ostream& out,
                         const InvalidInputTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << " use_integer_variables: " << params.use_integer_variables << " }";
  return out;
}

InvalidParameterTest::InvalidParameterTest()
    : model_(), x_(model_.AddContinuousVariable(0.0, 1.0, "x")) {
  model_.Maximize(2 * x_);
}

InvalidParameterTestParams::InvalidParameterTestParams(
    const SolverType solver_type, SolveParameters solve_parameters,
    std::vector<std::string> expected_error_substrings)
    : solver_type(solver_type),
      solve_parameters(std::move(solve_parameters)),
      expected_error_substrings(std::move(expected_error_substrings)) {}

std::ostream& operator<<(std::ostream& out,
                         const InvalidParameterTestParams& params) {
  out << "{ solver_type: " << params.solver_type << " solve_params: "
      << ProtobufShortDebugString(params.solve_parameters.Proto())
      << " expected_error_substrings: [ "
      << absl::StrJoin(params.expected_error_substrings, "; ") << " ]";
  return out;
}

namespace {

TEST_P(InvalidInputTest, InvalidModel) {
  ModelProto model;
  model.set_name("simple_model");
  model.mutable_variables()->add_ids(3);
  model.mutable_variables()->add_lower_bounds(2.0);
  model.mutable_variables()->add_upper_bounds(3.0);
  model.mutable_variables()->add_upper_bounds(4.0);
  model.mutable_variables()->add_integers(GetParam().use_integer_variables);
  model.mutable_variables()->add_names("x3");

  EXPECT_THAT(Solver::New(EnumToProto(TestedSolver()), model, /*arguments=*/{}),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST_P(InvalidInputTest, InvalidCommonParameters) {
  ASSERT_OK_AND_ASSIGN(
      const std::unique_ptr<Solver> solver,
      Solver::New(EnumToProto(TestedSolver()), ModelProto{}, /*arguments=*/{}));
  Solver::SolveArgs solve_args;
  solve_args.parameters.set_threads(-1);
  EXPECT_THAT(solver->Solve(solve_args),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST_P(InvalidInputTest, InvalidUpdate) {
  ModelProto model;
  model.set_name("simple_model");
  model.mutable_variables()->add_ids(3);
  model.mutable_variables()->add_lower_bounds(2.0);
  model.mutable_variables()->add_upper_bounds(3.0);
  model.mutable_variables()->add_integers(GetParam().use_integer_variables);
  model.mutable_variables()->add_names("x3");

  ASSERT_OK_AND_ASSIGN(auto solver,
                       Solver::New(EnumToProto(TestedSolver()), model,
                                   /*arguments=*/{}));

  ModelUpdateProto update;
  update.add_deleted_variable_ids(2);

  EXPECT_THAT(solver->Update(update), StatusIs(StatusCode::kInvalidArgument));
}

TEST_P(InvalidParameterTest, InvalidParameterNameAsError) {
  SolveArguments args = {.parameters = GetParam().solve_parameters};
  const auto result = SimpleSolve();
  ASSERT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
  for (const std::string& error : GetParam().expected_error_substrings) {
    SCOPED_TRACE(error);
    EXPECT_THAT(result.status().message(), HasSubstr(error));
  }
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
