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

#include "ortools/math_opt/cpp/solve.h"

#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/solver_interface_mock.h"
#include "ortools/math_opt/core/sparse_collection_matchers.h"
#include "ortools/math_opt/cpp/callback.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"
#include "ortools/math_opt/cpp/enums.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

// Basic LP model:
//
//   a and b are continuous variable
//
//   minimize a - b
//       s.t. 0 <= a
//            0 <= b <= 3
struct BasicLp {
  BasicLp();

  // Sets the upper bound of variable b to 2.0 and returns the corresponding
  // update.
  std::optional<ModelUpdateProto> UpdateUpperBoundOfB();

  // Returns the expected optimal result for this model. Only put the given set
  // of variables in the result (to test filters). When `after_update` is true,
  // returns the optimal result after UpdateUpperBoundOfB() has been called.
  SolveResultProto OptimalResult(const absl::flat_hash_set<Variable>& vars,
                                 bool after_update = false) const;

  Model model;
  const Variable a;
  const Variable b;
};

BasicLp::BasicLp()
    : a(model.AddVariable(0.0, kInf, false, "a")),
      b(model.AddVariable(0.0, 3.0, false, "b")) {}

std::optional<ModelUpdateProto> BasicLp::UpdateUpperBoundOfB() {
  const std::unique_ptr<UpdateTracker> tracker = model.NewUpdateTracker();
  model.set_upper_bound(b, 2.0);
  return tracker->ExportModelUpdate().value();
}

SolveResultProto BasicLp::OptimalResult(
    const absl::flat_hash_set<Variable>& vars, bool after_update) const {
  SolveResultProto result;
  result.mutable_termination()->set_reason(TERMINATION_REASON_OPTIMAL);
  result.mutable_solve_stats()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  result.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);
  PrimalSolutionProto* const solution =
      result.add_solutions()->mutable_primal_solution();
  solution->set_objective_value(0.0);
  solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  if (vars.contains(a)) {
    solution->mutable_variable_values()->add_ids(a.id());
    solution->mutable_variable_values()->add_values(0.0);
  }
  if (vars.contains(b)) {
    solution->mutable_variable_values()->add_ids(b.id());
    solution->mutable_variable_values()->add_values(after_update ? 2.0 : 3.0);
  }
  return result;
}

// Test calling Solve() without any callback.
TEST(SolveTest, SuccessfulSolveNoCallback) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolveInterrupter interrupter;
  args.interrupter = &interrupter;

  args.message_callback = [](absl::Span<const std::string>) {};

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Ne(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(&interrupter)))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a})));
  }

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));
}

// Test calling Solve() with a callback.
TEST(SolveTest, SuccessfulSolveWithCallback) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback cb,
          const SolveInterrupter* absl_nullable const interrupter)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    ASSIGN_OR_RETURN(const CallbackResultProto result, cb(cb_data));
    return basic_lp.OptimalResult({basic_lp.a});
  };

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,

                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Eq(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Ne(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
  }

  int callback_called_count = 0;
  args.callback = [&](const CallbackData& callback_data) {
    ++callback_called_count;
    CallbackResult result;
    result.AddLazyConstraint(basic_lp.a + basic_lp.b <= 3);
    return result;
  };
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(callback_called_count, 1);
  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));
}

TEST(SolveTest, RemoveNamesSendsNoNames) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  Model model;
  model.AddBinaryVariable("x");

  SolveArguments args;
  SolverInitArguments init_args;
  init_args.remove_names = true;

  ModelProto expected_model;
  expected_model.mutable_variables()->add_ids(0);
  expected_model.mutable_variables()->add_lower_bounds(0.0);
  expected_model.mutable_variables()->add_upper_bounds(1.0);
  expected_model.mutable_variables()->add_integers(true);

  SolveResultProto fake_result;
  *fake_result.mutable_termination() =
      NoSolutionFoundTerminationProto(/*is_maximize=*/false, LIMIT_TIME);

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock, Call(EquivToProto(expected_model), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    EXPECT_CALL(solver, Solve(_, _, _, _, _, _)).WillOnce(Return(fake_result));
  }

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, EnumFromProto(registration.solver_type()).value(), args,
            init_args));
}

// Test calling Solve() with a solver that fails to returns the SolverInterface
// for a given model.
TEST(SolveTest, FailingSolveInstantiation) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolverInterfaceMock solver;
  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(absl::InternalError("instantiation failed"))));

  ASSERT_THAT(Solve(basic_lp.model,
                    EnumFromProto(registration.solver_type()).value(), args),
              StatusIs(absl::StatusCode::kInternal, "instantiation failed"));
}

// Test calling Solve() with a solver that returns an error on
// SolverInterface::Solve().
TEST(SolveTest, FailingSolve) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Eq(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(nullptr)))
        .WillOnce(Return(absl::InternalError("solve failed")));
  }

  ASSERT_THAT(Solve(basic_lp.model,
                    EnumFromProto(registration.solver_type()).value(), args),
              StatusIs(absl::StatusCode::kInternal, "solve failed"));
}

TEST(SolveTest, NullCallback) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  SolverInterfaceMock solver;
  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

  EXPECT_THAT(
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("no callback")));
}

TEST(SolveTest, WrongModelInModelParameters) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({other_basic_lp.a});

  SolverInterfaceMock solver;
  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

  EXPECT_THAT(Solve(basic_lp.model,
                    EnumFromProto(registration.solver_type()).value(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(SolveTest, WrongModelInCallbackRegistration) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.callback_registration.mip_solution_filter =
      MakeKeepKeysFilter({other_basic_lp.a});

  SolverInterfaceMock solver;
  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

  EXPECT_THAT(Solve(basic_lp.model,
                    EnumFromProto(registration.solver_type()).value(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(SolveTest, WrongModelInCallbackResult) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback cb,
          const SolveInterrupter* absl_nullable const interrupter)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    ASSIGN_OR_RETURN(const CallbackResultProto result, cb(cb_data));
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,

                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Eq(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Ne(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
  }

  args.callback = [&](const CallbackData& callback_data) {
    CallbackResult result;
    // We use the wrong model here.
    result.AddLazyConstraint(other_basic_lp.a + other_basic_lp.b <= 3);
    return result;
  };

  EXPECT_THAT(Solve(basic_lp.model,
                    EnumFromProto(registration.solver_type()).value(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(IncrementalSolverTest, NullModel) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  EXPECT_THAT(NewIncrementalSolver(
                  nullptr, EnumFromProto(registration.solver_type()).value()),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("model")));
}

TEST(IncrementalSolverTest, SolverType) {
  BasicLp basic_lp;
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model, SolverType::kGlop));
  EXPECT_EQ(solver->solver_type(), SolverType::kGlop);
}

// Test calling IncrementalSolver without any callback with a succeeding
// non-empty update.
TEST(IncrementalSolverTest, IncrementalSolveNoCallback) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolverInterfaceMock solver_interface;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolveInterrupter interrupter;
  args_1.interrupter = &interrupter;

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                         args_1.model_parameters.Proto());
    EXPECT_CALL(solver_interface,
                Solve(EquivToProto(args_1.parameters.Proto()),
                      EquivToProto(model_parameters_1), Eq(nullptr),
                      EquivToProto(args_1.callback_registration.Proto()),
                      Eq(nullptr), Eq(&interrupter)))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a})));
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result_1,
                       solver->SolveWithoutUpdate(args_1));

  EXPECT_EQ(result_1.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result_1.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));

  // Second solve with update.
  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  const std::optional<ModelUpdateProto> update = basic_lp.UpdateUpperBoundOfB();
  ASSERT_TRUE(update);

  SolveArguments args_2;
  args_2.parameters.enable_output = true;

  EXPECT_CALL(solver_interface, Update(EquivToProto(*update)))
      .WillOnce(Return(true));

  ASSERT_OK_AND_ASSIGN(const UpdateResult update_result, solver->Update());
  EXPECT_TRUE(update_result.did_update);

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_2,
                         args_2.model_parameters.Proto());
    EXPECT_CALL(solver_interface,
                Solve(EquivToProto(args_2.parameters.Proto()),
                      EquivToProto(model_parameters_2), Eq(nullptr),
                      EquivToProto(args_2.callback_registration.Proto()),
                      Eq(nullptr), Eq(nullptr)))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a, basic_lp.b},
                                                /*after_update=*/true)));
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result_2,
                       solver->SolveWithoutUpdate(args_2));

  EXPECT_EQ(result_2.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(
      result_2.variable_values(),
      UnorderedElementsAre(Pair(basic_lp.a, 0.0), Pair(basic_lp.b, 2.0)));
}

TEST(IncrementalSolverTest, RemoveNamesSendsNoNamesOnModel) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  Model model;
  model.AddBinaryVariable("x");

  SolverInitArguments init_args;
  init_args.remove_names = true;

  ModelProto expected_model;
  expected_model.mutable_variables()->add_ids(0);
  expected_model.mutable_variables()->add_lower_bounds(0.0);
  expected_model.mutable_variables()->add_upper_bounds(1.0);
  expected_model.mutable_variables()->add_integers(true);

  SolverInterfaceMock solver_interface;
  EXPECT_CALL(factory_mock, Call(EquivToProto(expected_model), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  EXPECT_OK(NewIncrementalSolver(
      &model, EnumFromProto(registration.solver_type()).value(), init_args));
}

TEST(IncrementalSolverTest, RemoveNamesSendsNoNamesOnModelUpdate) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  Model model;

  SolveArguments args;
  SolverInitArguments init_args;
  init_args.remove_names = true;

  SolverInterfaceMock solver_interface;
  EXPECT_CALL(factory_mock, Call(_, _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&model,
                           EnumFromProto(registration.solver_type()).value(),
                           init_args));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  model.AddBinaryVariable("x");

  ModelUpdateProto expected_update;
  expected_update.mutable_new_variables()->add_ids(0);
  expected_update.mutable_new_variables()->add_lower_bounds(0.0);
  expected_update.mutable_new_variables()->add_upper_bounds(1.0);
  expected_update.mutable_new_variables()->add_integers(true);

  EXPECT_CALL(solver_interface, Update(EquivToProto(expected_update)))
      .WillOnce(Return(true));

  ASSERT_OK_AND_ASSIGN(const UpdateResult update_result, solver->Update());
  EXPECT_TRUE(update_result.did_update);
}

TEST(IncrementalSolverTest, RemoveNamesOnFullModelAfterUpdateFails) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  Model model;

  SolveArguments args;
  SolverInitArguments init_args;
  init_args.remove_names = true;

  SolverInterfaceMock solver_interface;
  EXPECT_CALL(factory_mock, Call(_, _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&model,
                           EnumFromProto(registration.solver_type()).value(),
                           init_args));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  model.AddBinaryVariable("x");

  ModelProto expected_model;
  expected_model.mutable_variables()->add_ids(0);
  expected_model.mutable_variables()->add_lower_bounds(0.0);
  expected_model.mutable_variables()->add_upper_bounds(1.0);
  expected_model.mutable_variables()->add_integers(true);

  EXPECT_CALL(solver_interface, Update(_)).WillOnce(Return(false));
  SolverInterfaceMock solver_interface2;
  EXPECT_CALL(factory_mock, Call(EquivToProto(expected_model), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface2))));

  ASSERT_OK_AND_ASSIGN(const UpdateResult update_result, solver->Update());
  EXPECT_FALSE(update_result.did_update);
}

// Test calling IncrementalSolver without any callback with an empty update.
TEST(IncrementalSolverTest, IncrementalSolveWithEmptyUpdate) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolverInterfaceMock solver_interface;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                         args_1.model_parameters.Proto());
    EXPECT_CALL(solver_interface,
                Solve(EquivToProto(args_1.parameters.Proto()),
                      EquivToProto(model_parameters_1), Eq(nullptr),
                      EquivToProto(args_1.callback_registration.Proto()),
                      Eq(nullptr), Eq(nullptr)))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a})));
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result_1,
                       solver->SolveWithoutUpdate(args_1));

  EXPECT_EQ(result_1.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result_1.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));

  // Second solve with update.
  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  SolveArguments args_2;
  args_2.parameters.enable_output = true;

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_2,
                         args_2.model_parameters.Proto());
    EXPECT_CALL(solver_interface,
                Solve(EquivToProto(args_2.parameters.Proto()),
                      EquivToProto(model_parameters_2), Eq(nullptr),
                      EquivToProto(args_2.callback_registration.Proto()),
                      Eq(nullptr), Eq(nullptr)))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a, basic_lp.b})));
  }

  ASSERT_OK_AND_ASSIGN(const UpdateResult update_result, solver->Update());
  EXPECT_TRUE(update_result.did_update);
  ASSERT_OK_AND_ASSIGN(const SolveResult result_2,
                       solver->SolveWithoutUpdate(args_2));

  EXPECT_EQ(result_2.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(
      result_2.variable_values(),
      UnorderedElementsAre(Pair(basic_lp.a, 0.0), Pair(basic_lp.b, 3.0)));
}

// Test calling IncrementalSolver without any callback and with a failing
// update; thus resulting in the re-creation of the solver instead.
//
// This also tests that at any given time only one instance of Solver
// exists. This is important for Gubori as only one instance can exist at any
// given time when using a single-use license.
TEST(IncrementalSolverTest, IncrementalSolveWithFailedUpdate) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolverInterfaceMock solver_1;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  // The current number of instances of solver.
  int num_instances = 0;
  const auto constructor_cb = [&]() {
    ++num_instances;
    // We only want one instance at most at any given time.
    EXPECT_LE(num_instances, 1);
  };
  const auto destructor_cb = [&]() {
    ASSERT_GE(num_instances, 1);
    --num_instances;
  };
  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce([&](const ModelProto&, const SolverInterface::InitArgs&) {
        constructor_cb();
        return std::make_unique<DelegatingSolver>(&solver_1, destructor_cb);
      });

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                         args_1.model_parameters.Proto());
    EXPECT_CALL(solver_1,
                Solve(EquivToProto(args_1.parameters.Proto()),
                      EquivToProto(model_parameters_1), Eq(nullptr),
                      EquivToProto(args_1.callback_registration.Proto()),
                      Eq(nullptr), Eq(nullptr)))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a})));
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result_1,
                       solver->SolveWithoutUpdate(args_1));

  EXPECT_EQ(result_1.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result_1.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));

  // Second solve with update.
  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);

  const std::optional<ModelUpdateProto> update = basic_lp.UpdateUpperBoundOfB();
  ASSERT_TRUE(update);

  SolveArguments args_2;
  args_2.parameters.enable_output = true;

  SolverInterfaceMock solver_2;

  {
    InSequence s;

    EXPECT_CALL(solver_1, Update(EquivToProto(*update)))
        .WillOnce(Return(false));

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce([&](const ModelProto&, const SolverInterface::InitArgs&) {
          constructor_cb();
          return std::make_unique<DelegatingSolver>(&solver_2, destructor_cb);
        });
  }

  ASSERT_OK_AND_ASSIGN(const UpdateResult update_result, solver->Update());
  EXPECT_FALSE(update_result.did_update);

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);
  Mock::VerifyAndClearExpectations(&solver_2);

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_2,
                         args_2.model_parameters.Proto());
    EXPECT_CALL(solver_2,
                Solve(EquivToProto(args_2.parameters.Proto()),
                      EquivToProto(model_parameters_2), Eq(nullptr),
                      EquivToProto(args_2.callback_registration.Proto()),
                      Eq(nullptr), Eq(nullptr)))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a, basic_lp.b},
                                                /*after_update=*/true)));
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result_2,
                       solver->SolveWithoutUpdate(args_2));

  EXPECT_EQ(result_2.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(
      result_2.variable_values(),
      UnorderedElementsAre(Pair(basic_lp.a, 0.0), Pair(basic_lp.b, 2.0)));
}

// Test calling IncrementalSolver without any callback and with an impossible
// update, i.e. an update that contains an unsupported feature.
TEST(IncrementalSolverTest, IncrementalSolveWithImpossibleUpdate) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolverInterfaceMock solver_1;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver_1))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                       args_1.model_parameters.Proto());
  EXPECT_CALL(solver_1,
              Solve(EquivToProto(args_1.parameters.Proto()),
                    EquivToProto(model_parameters_1), Eq(nullptr),
                    EquivToProto(args_1.callback_registration.Proto()),
                    Eq(nullptr), Eq(nullptr)))
      .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a})));

  ASSERT_OK_AND_ASSIGN(const SolveResult result_1,
                       solver->SolveWithoutUpdate(args_1));

  EXPECT_EQ(result_1.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result_1.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));

  // Update.
  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);

  const std::optional<ModelUpdateProto> update = basic_lp.UpdateUpperBoundOfB();
  ASSERT_TRUE(update);

  SolveArguments args_2;
  args_2.parameters.enable_output = true;

  {
    InSequence s;

    // The solver will refuse the update with the unsupported feature.
    EXPECT_CALL(solver_1, Update(EquivToProto(*update)))
        .WillOnce(Return(false));

    // The solver factory will fail for the same reason.
    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(absl::InternalError("*unsupported model*"))));
  }

  ASSERT_THAT(solver->Update(),
              StatusIs(absl::StatusCode::kInternal,
                       AllOf(HasSubstr("*unsupported model*"),
                             HasSubstr("solver re-creation failed"))));

  // Next calls should fail and not crash.
  basic_lp.model.set_lower_bound(basic_lp.a, -3.0);
  EXPECT_THAT(solver->Update(), StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("Update() failed")));
  EXPECT_THAT(solver->SolveWithoutUpdate(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Update() failed")));
}

// Test calling IncrementalSolver with a callback. We don't test calling
// Update() here since only the Solve() function takes a callback.
TEST(IncrementalSolverTest, SuccessfulSolveWithCallback) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback cb,
          const SolveInterrupter* absl_nullable const interrupter)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    ASSIGN_OR_RETURN(const CallbackResultProto result, cb(cb_data));
    return basic_lp.OptimalResult({basic_lp.a});
  };

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock,

              Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(solver_interface,
              Solve(EquivToProto(args.parameters.Proto()),
                    EquivToProto(model_parameters), Eq(nullptr),
                    EquivToProto(args.callback_registration.Proto()),
                    Ne(nullptr), Eq(nullptr)))
      .WillOnce(fake_solve);

  int callback_called_count = 0;
  args.callback = [&](const CallbackData& callback_data) {
    ++callback_called_count;
    CallbackResult result;
    result.AddLazyConstraint(basic_lp.a + basic_lp.b <= 3);
    return result;
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       solver->SolveWithoutUpdate(args));

  EXPECT_EQ(callback_called_count, 1);
  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));
}

// Test calling IncrementalSolver with a solver that fails to returns the
// SolverInterface for a given model.
TEST(IncrementalSolverTest, FailingSolverInstantiation) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolverInterfaceMock solver_interface;
  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(absl::InternalError("instantiation failed"))));

  ASSERT_THAT(
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()),
      StatusIs(absl::StatusCode::kInternal, "instantiation failed"));
}

// Test calling IncrementalSolver with a solver that returns an error on
// SolverInterface::Solve().
TEST(IncrementalSolverTest, FailingSolver) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(solver_interface,
              Solve(EquivToProto(args.parameters.Proto()),
                    EquivToProto(model_parameters), Eq(nullptr),
                    EquivToProto(args.callback_registration.Proto()),
                    Eq(nullptr), Eq(nullptr)))
      .WillOnce(Return(absl::InternalError("solve failed")));

  ASSERT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInternal, "solve failed"));
}

// Test calling IncrementalSolver with a solver that returns an error on
// SolverInterface::Update().
TEST(IncrementalSolverTest, FailingSolverUpdate) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  const std::optional<ModelUpdateProto> update = basic_lp.UpdateUpperBoundOfB();
  ASSERT_TRUE(update);

  EXPECT_CALL(solver_interface, Update(EquivToProto(*update)))
      .WillOnce(Return(absl::InternalError("*update failure*")));

  ASSERT_THAT(solver->Update(), StatusIs(absl::StatusCode::kInternal,
                                         AllOf(HasSubstr("*update failure*"),
                                               HasSubstr("update failed"))));
}

// Test calling IncrementalSolver::Solve() with a callback and a non trivial
// update.
TEST(IncrementalSolverTest, UpdateAndSolve) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback cb,
          const SolveInterrupter* absl_nullable const interrupter)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    ASSIGN_OR_RETURN(const CallbackResultProto result, cb(cb_data));
    return basic_lp.OptimalResult({basic_lp.a});
  };

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock,

              Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  // Update the model before calling Solve().
  const std::optional<ModelUpdateProto> update = basic_lp.UpdateUpperBoundOfB();
  ASSERT_TRUE(update);

  {
    InSequence s;

    EXPECT_CALL(solver_interface, Update(EquivToProto(*update)))
        .WillOnce(Return(true));
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver_interface,
                Solve(EquivToProto(args.parameters.Proto()),
                      EquivToProto(model_parameters), Eq(nullptr),
                      EquivToProto(args.callback_registration.Proto()),
                      Ne(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
  }

  int callback_called_count = 0;
  args.callback = [&](const CallbackData& callback_data) {
    ++callback_called_count;
    CallbackResult result;
    result.AddLazyConstraint(basic_lp.a + basic_lp.b <= 3);
    return result;
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver->Solve(args));

  EXPECT_EQ(callback_called_count, 1);
  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));
}

// Test calling IncrementalSolver::Solve() with a solver that returns an error
// on SolverInterface::Solve().
TEST(IncrementalSolverTest, UpdateAndSolveWithFailingSolver) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(solver_interface,
              Solve(EquivToProto(args.parameters.Proto()),
                    EquivToProto(model_parameters), Eq(nullptr),
                    EquivToProto(args.callback_registration.Proto()),
                    Eq(nullptr), Eq(nullptr)))
      .WillOnce(Return(absl::InternalError("solve failed")));

  ASSERT_THAT(solver->Solve(args),
              StatusIs(absl::StatusCode::kInternal, "solve failed"));
}

// Test calling IncrementalSolver::Solve() with a solver that returns an error
// on SolverInterface::Update().
TEST(IncrementalSolverTest, UpdateAndSolveWithFailingSolverUpdate) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  const std::optional<ModelUpdateProto> update = basic_lp.UpdateUpperBoundOfB();
  ASSERT_TRUE(update);

  EXPECT_CALL(solver_interface, Update(EquivToProto(*update)))
      .WillOnce(Return(absl::InternalError("*update failure*")));

  ASSERT_THAT(solver->Solve(), StatusIs(absl::StatusCode::kInternal,
                                        AllOf(HasSubstr("*update failure*"),
                                              HasSubstr("update failed"))));
}

TEST(IncrementalSolverTest, NullCallback) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  EXPECT_THAT(
      solver->SolveWithoutUpdate(args),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("no callback")));
}

TEST(IncrementalSolverTest, WrongModelInModelParameters) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({other_basic_lp.a});
  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  EXPECT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(IncrementalSolverTest, WrongModelInCallbackRegistration) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.callback_registration.mip_solution_filter =
      MakeKeepKeysFilter({other_basic_lp.a});

  SolverInterfaceMock solver_interface;

  EXPECT_CALL(factory_mock,

              Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  EXPECT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(IncrementalSolverTest, WrongModelInCallbackResult) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback cb,
          const SolveInterrupter* absl_nullable const interrupter)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    ASSIGN_OR_RETURN(const CallbackResultProto result, cb(cb_data));
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  SolverInterfaceMock solver_interface;

  args.callback = [&](const CallbackData& callback_data) {
    CallbackResult result;
    // We use the wrong model here.
    result.AddLazyConstraint(other_basic_lp.a + other_basic_lp.b <= 3);
    return result;
  };

  EXPECT_CALL(factory_mock, Call(EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      NewIncrementalSolver(&basic_lp.model,
                           EnumFromProto(registration.solver_type()).value()));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(solver_interface,
              Solve(EquivToProto(args.parameters.Proto()),
                    EquivToProto(model_parameters), Eq(nullptr),
                    EquivToProto(args.callback_registration.Proto()),
                    Ne(nullptr), Eq(nullptr)))
      .WillOnce(fake_solve);

  EXPECT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

// Basic infeasible LP model:
//
//   minimize 0
//       s.t. x <= -1 (linear constraint)
//            0 <= x <= 1 (bounds)
struct BasicInfeasibleLp {
  BasicInfeasibleLp()
      : x(model.AddContinuousVariable(0.0, 1.0, "x")),
        c(model.AddLinearConstraint(x <= -1.0, "c")) {}

  ComputeInfeasibleSubsystemResultProto InfeasibleResult() const {
    ComputeInfeasibleSubsystemResultProto result;
    result.set_feasibility(FEASIBILITY_STATUS_INFEASIBLE);
    (*result.mutable_infeasible_subsystem()->mutable_variable_bounds())[0]
        .set_lower(true);
    (*result.mutable_infeasible_subsystem()->mutable_variable_bounds())[0]
        .set_upper(false);
    (*result.mutable_infeasible_subsystem()->mutable_linear_constraints())[0]
        .set_lower(false);
    (*result.mutable_infeasible_subsystem()->mutable_linear_constraints())[0]
        .set_upper(true);
    result.set_is_minimal(true);
    return result;
  }

  Model model;
  const Variable x;
  const LinearConstraint c;
};

TEST(ComputeInfeasibleSubsystemTest, SuccessfulCall) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicInfeasibleLp lp;

  ComputeInfeasibleSubsystemArguments args;
  args.parameters.enable_output = true;

  SolveInterrupter interrupter;
  args.interrupter = &interrupter;

  args.message_callback = [](absl::Span<const std::string>) {};

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock, Call(EquivToProto(lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    EXPECT_CALL(solver, ComputeInfeasibleSubsystem(
                            EquivToProto(args.parameters.Proto()), Ne(nullptr),
                            Eq(&interrupter)))
        .WillOnce(Return(lp.InfeasibleResult()));
  }

  ASSERT_OK_AND_ASSIGN(
      const ComputeInfeasibleSubsystemResult result,
      ComputeInfeasibleSubsystem(
          lp.model, EnumFromProto(registration.solver_type()).value(), args));

  EXPECT_EQ(result.feasibility, FeasibilityStatus::kInfeasible);
}

TEST(ComputeInfeasibleSubsystemTest, FailingSolve) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicInfeasibleLp lp;

  ComputeInfeasibleSubsystemArguments args;
  args.parameters.enable_output = true;

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock, Call(EquivToProto(lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    EXPECT_CALL(solver, ComputeInfeasibleSubsystem(
                            EquivToProto(args.parameters.Proto()), Eq(nullptr),
                            Eq(nullptr)))
        .WillOnce(Return(absl::InternalError("infeasible subsystem failed")));
  }

  ASSERT_THAT(
      ComputeInfeasibleSubsystem(
          lp.model, EnumFromProto(registration.solver_type()).value(), args,
          {}),
      StatusIs(absl::StatusCode::kInternal, "infeasible subsystem failed"));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
