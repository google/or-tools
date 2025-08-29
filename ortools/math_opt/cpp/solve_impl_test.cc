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

#include "ortools/math_opt/cpp/solve_impl.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/base_solver.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/sparse_collection_matchers.h"
#include "ortools/math_opt/cpp/callback.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_arguments.h"
#include "ortools/math_opt/cpp/compute_infeasible_subsystem_result.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/update_result.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt::internal {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::EquivToProto;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

class BaseSolverMock : public BaseSolver {
 public:
  MOCK_METHOD(absl::StatusOr<SolveResultProto>, Solve,
              (const SolveArgs& arguments), (override));

  MOCK_METHOD(absl::StatusOr<ComputeInfeasibleSubsystemResultProto>,
              ComputeInfeasibleSubsystem,
              (const ComputeInfeasibleSubsystemArgs& arguments), (override));

  MOCK_METHOD(absl::StatusOr<bool>, Update, (ModelUpdateProto model_update),
              (override));
};

// Mock for BaseSolverFactory.
using BaseSolverFactoryMock =
    testing::MockFunction<absl::StatusOr<std::unique_ptr<BaseSolver>>(
        SolverTypeProto solver_type, const ModelProto& model,
        SolveInterrupter* local_canceller)>;

// Delegate all calls to another instance of BaseSolver.
//
// This is used as a return value in BaseSolverFactoryMock as:
// * this function needs to return a unique_ptr<BaseSolver>
// * but we want to be able to use a BaseSolverMock from the stack.
//
// Thus we can simply mock BaseSolverFactoryMock with
//   BaseSolverMock solver;
//
//   EXPECT_CALL(factory_mock,
//               Call(EquivToProto(basic_lp.model.ExportModel()), _, _))
//       .WillOnce(Return(ByMove(
//         std::make_unique<DelegatingBaseSolver>(&solver))));
//
// and add other EXPECT_CALL(solver, ...) on the instance that exists on stack.
class DelegatingBaseSolver : public BaseSolver {
 public:
  // Wraps the input solver interface, delegating calls to it. The optional
  // destructor_cb callback will be called in ~DelegatingBaseSolver().
  explicit DelegatingBaseSolver(
      BaseSolver* const solver,
      absl::AnyInvocable<void()> destructor_cb = nullptr)
      : solver_(ABSL_DIE_IF_NULL(solver)),
        destructor_cb_(std::move(destructor_cb)) {}

  ~DelegatingBaseSolver() override {
    if (destructor_cb_ != nullptr) {
      destructor_cb_();
    }
  }

  absl::StatusOr<SolveResultProto> Solve(const SolveArgs& arguments) override {
    return solver_->Solve(arguments);
  }

  absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(
      const ComputeInfeasibleSubsystemArgs& arguments) override {
    return solver_->ComputeInfeasibleSubsystem(arguments);
  }

  absl::StatusOr<bool> Update(ModelUpdateProto model_update) override {
    return solver_->Update(std::move(model_update));
  }

 private:
  BaseSolver* const solver_;
  absl::AnyInvocable<void()> destructor_cb_;
};

// Returns a matcher that matches the fields of SolveArgs according to the
// provided matchers.
//
// Note that we have to use template parameters for function/data pointers
// fields as we can't use testing::Matcher<FuncType> or
// testing::Matcher<DataType*>.
template <typename MessageCallbackMatcher, typename CallbackMatcher,
          typename SolveInterrupterPtrMatcher>
testing::Matcher<const BaseSolver::SolveArgs&> SolveArgsAre(
    testing::Matcher<SolveParametersProto> parameters,
    testing::Matcher<ModelSolveParametersProto> model_parameters,
    MessageCallbackMatcher message_callback,
    testing::Matcher<CallbackRegistrationProto> callback_registration,
    CallbackMatcher user_cb, SolveInterrupterPtrMatcher interrupter) {
  return AllOf(
      Field("parameters", &BaseSolver::SolveArgs::parameters, parameters),
      Field("model_parameters", &BaseSolver::SolveArgs::model_parameters,
            model_parameters),
      Field("message_callback", &BaseSolver::SolveArgs::message_callback,
            message_callback),
      Field("callback_registration",
            &BaseSolver::SolveArgs::callback_registration,
            callback_registration),
      Field("user_cb", &BaseSolver::SolveArgs::user_cb, user_cb),
      Field("interrupter", &BaseSolver::SolveArgs::interrupter, interrupter));
}

// Returns a matcher that matches the fields of ComputeInfeasibleSubsystemArgs
// according to the provided matchers.
//
// Note that we have to use template parameters for function/data pointers
// fields as we can't use testing::Matcher<FuncType> or
// testing::Matcher<DataType*>.
template <typename MessageCallbackMatcher, typename SolveInterrupterPtrMatcher>
testing::Matcher<const BaseSolver::ComputeInfeasibleSubsystemArgs&>
ComputeInfeasibleSubsystemArgsAre(
    testing::Matcher<SolveParametersProto> parameters,
    MessageCallbackMatcher message_callback,
    SolveInterrupterPtrMatcher interrupter) {
  return AllOf(
      Field("parameters",
            &BaseSolver::ComputeInfeasibleSubsystemArgs::parameters,
            parameters),
      Field("message_callback",
            &BaseSolver::ComputeInfeasibleSubsystemArgs::message_callback,
            message_callback),
      Field("interrupter",
            &BaseSolver::ComputeInfeasibleSubsystemArgs::interrupter,
            interrupter));
}

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

  // Sets the upper bound of constraint c to -2.0 and returns the corresponding
  // update.
  std::optional<ModelUpdateProto> UpdateUpperBoundOfC() {
    const std::unique_ptr<UpdateTracker> tracker = model.NewUpdateTracker();
    model.set_upper_bound(c, -2.0);
    return tracker->ExportModelUpdate().value();
  }

  Model model;
  const Variable x;
  const LinearConstraint c;
};

// Test calling Solve() without any callback.
TEST(SolveImplTest, SuccessfulSolveNoCallback) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolveInterrupter interrupter;
  args.interrupter = &interrupter;

  args.message_callback = [](absl::Span<const std::string>) {};

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(SOLVER_TYPE_GLOP,
                     EquivToProto(basic_lp.model.ExportModel()), Ne(nullptr)))
        .WillOnce(
            Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(SolveArgsAre(
                            EquivToProto(args.parameters.Proto()),
                            EquivToProto(model_parameters), Ne(nullptr),
                            EquivToProto(args.callback_registration.Proto()),
                            Eq(nullptr), Eq(&interrupter))))
        .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a})));
  }

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      SolveImpl(factory_mock.AsStdFunction(), basic_lp.model, SolverType::kGlop,
                args, /*user_canceller=*/nullptr,
                /*remove_names=*/false));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));
}

// Test calling Solve() with a callback.
TEST(SolveImplTest, SuccessfulSolveWithCallback) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve = [&](const BaseSolver::SolveArgs& args)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    args.user_cb(cb_data);
    return basic_lp.OptimalResult({basic_lp.a});
  };

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(
        factory_mock,
        Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(
            Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(SolveArgsAre(
                            EquivToProto(args.parameters.Proto()),
                            EquivToProto(model_parameters), Eq(nullptr),
                            EquivToProto(args.callback_registration.Proto()),
                            Ne(nullptr), Eq(nullptr))))
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
      SolveImpl(factory_mock.AsStdFunction(), basic_lp.model, SolverType::kGlop,
                args, /*user_canceller=*/nullptr, /*remove_names=*/false));

  EXPECT_EQ(callback_called_count, 1);
  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(result.variable_values(),
              UnorderedElementsAre(Pair(basic_lp.a, 0.0)));
}

TEST(SolveImplTest, RemoveNamesSendsNoNames) {
  Model model;
  model.AddBinaryVariable("x");

  ModelProto expected_model;
  expected_model.mutable_variables()->add_ids(0);
  expected_model.mutable_variables()->add_lower_bounds(0.0);
  expected_model.mutable_variables()->add_upper_bounds(1.0);
  expected_model.mutable_variables()->add_integers(true);

  SolveResultProto fake_result;
  *fake_result.mutable_termination() =
      NoSolutionFoundTerminationProto(/*is_maximize=*/false, LIMIT_TIME);

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(SOLVER_TYPE_GLOP, EquivToProto(expected_model), _))
        .WillOnce(
            Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

    EXPECT_CALL(solver, Solve(SolveArgsAre(_, _, _, _, _, _)))
        .WillOnce(Return(fake_result));
  }

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      SolveImpl(factory_mock.AsStdFunction(), model, SolverType::kGlop, {},
                /*user_canceller=*/nullptr, /*remove_names=*/true));
}

// Test calling Solve() with a solver that fails to returns the SolverInterface
// for a given model.
TEST(SolveImplTest, FailingSolveInstantiation) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(absl::InternalError("instantiation failed"))));

  ASSERT_THAT(SolveImpl(factory_mock.AsStdFunction(), basic_lp.model,
                        SolverType::kGlop, args,
                        /*user_canceller=*/nullptr, /*remove_names=*/false),
              StatusIs(absl::StatusCode::kInternal, "instantiation failed"));
}

// Test calling Solve() with a solver that returns an error on
// SolverInterface::Solve().
TEST(SolveImplTest, FailingSolve) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(
        factory_mock,
        Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(
            Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(SolveArgsAre(
                            EquivToProto(args.parameters.Proto()),
                            EquivToProto(model_parameters), Eq(nullptr),
                            EquivToProto(args.callback_registration.Proto()),
                            Eq(nullptr), Eq(nullptr))))
        .WillOnce(Return(absl::InternalError("solve failed")));
  }

  ASSERT_THAT(
      SolveImpl(factory_mock.AsStdFunction(), basic_lp.model, SolverType::kGlop,
                args, /*user_canceller=*/nullptr, /*remove_names=*/false),
      StatusIs(absl::StatusCode::kInternal, "solve failed"));
}

TEST(SolveImplTest, NullCallback) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(
          Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

  EXPECT_THAT(
      SolveImpl(factory_mock.AsStdFunction(), basic_lp.model, SolverType::kGlop,
                args, /*user_canceller=*/nullptr, /*remove_names=*/false),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("no callback was provided")));
}

TEST(SolveImplTest, WrongModelInModelParameters) {
  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({other_basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(
          Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

  EXPECT_THAT(
      SolveImpl(factory_mock.AsStdFunction(), basic_lp.model, SolverType::kGlop,
                args, /*user_canceller=*/nullptr, /*remove_names=*/false),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(SolveImplTest, WrongModelInCallbackRegistration) {
  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.callback_registration.mip_solution_filter =
      MakeKeepKeysFilter({other_basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(
          Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

  EXPECT_THAT(
      SolveImpl(factory_mock.AsStdFunction(), basic_lp.model, SolverType::kGlop,
                args, /*user_canceller=*/nullptr, /*remove_names=*/false),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(SolveImplTest, WrongModelInCallbackResult) {
  // We repeat the same test but either return a valid result or an error in
  // fake_solve.
  for (const bool return_an_error : {false, true}) {
    SCOPED_TRACE(return_an_error ? "with fake_solve returning an error"
                                 : "with fake_solve returning a result");
    BasicLp basic_lp;
    BasicLp other_basic_lp;

    SolveArguments args;
    args.parameters.enable_output = true;

    args.callback_registration.add_lazy_constraints = true;
    args.callback_registration.events.insert(CallbackEvent::kMipSolution);

    // Will be set to the provided local_canceller in the factory.
    SolveInterrupter* provided_local_canceller = nullptr;

    const auto fake_solve = [&](const BaseSolver::SolveArgs& args)
        -> absl::StatusOr<SolveResultProto> {
      CallbackDataProto cb_data;
      cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
      *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
          {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
      CallbackResultProto result = args.user_cb(cb_data);
      // Errors in callback should result in early termination.
      EXPECT_TRUE(result.terminate());
      // Errors in callback should trigger the cancellation.
      EXPECT_TRUE(provided_local_canceller->IsInterrupted());
      // The returned value should be ignored.
      if (return_an_error) {
        return absl::CancelledError("solver has been cancelled");
      }
      return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
    };

    BaseSolverFactoryMock factory_mock;
    BaseSolverMock solver;
    {
      InSequence s;

      EXPECT_CALL(
          factory_mock,
          Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
          .WillOnce([&](const SolverTypeProto solver_type,
                        const ModelProto& model,
                        SolveInterrupter* const local_canceller) {
            provided_local_canceller = local_canceller;
            return std::make_unique<DelegatingBaseSolver>(&solver);
          });

      ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                           args.model_parameters.Proto());
      EXPECT_CALL(solver, Solve(SolveArgsAre(
                              EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Eq(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Ne(nullptr), Eq(nullptr))))
          .WillOnce(fake_solve);
    }

    args.callback = [&](const CallbackData& callback_data) {
      CallbackResult result;
      // We use the wrong model here.
      result.AddLazyConstraint(other_basic_lp.a + other_basic_lp.b <= 3);
      return result;
    };

    EXPECT_THAT(SolveImpl(factory_mock.AsStdFunction(), basic_lp.model,
                          SolverType::kGlop, args, /*user_canceller=*/nullptr,
                          /*remove_names=*/false),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(internal::kInputFromInvalidModelStorage)));
  }
}

TEST(SolveImplTest, UserCancellation) {
  BasicLp basic_lp;

  // Will be set to the provided local_canceller in the factory.
  SolveInterrupter* provided_local_canceller = nullptr;

  const auto fake_solve = [&](const BaseSolver::SolveArgs& args)
      -> absl::StatusOr<SolveResultProto> {
    // The solver should have been cancelled before its Solve() is called.
    EXPECT_TRUE(provided_local_canceller->IsInterrupted());
    return absl::CancelledError("solver has been cancelled");
  };

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP, _, Ne(nullptr)))
        .WillOnce([&](const SolverTypeProto solver_type,
                      const ModelProto& model,
                      SolveInterrupter* const local_canceller) {
          provided_local_canceller = local_canceller;
          return std::make_unique<DelegatingBaseSolver>(&solver);
        });

    EXPECT_CALL(solver, Solve(_)).WillOnce(fake_solve);
  }

  SolveInterrupter user_canceller;
  user_canceller.Interrupt();

  ASSERT_THAT(
      SolveImpl(factory_mock.AsStdFunction(), basic_lp.model, SolverType::kGlop,
                {}, /*user_canceller=*/&user_canceller,
                /*remove_names=*/false),
      StatusIs(absl::StatusCode::kCancelled));
}

TEST(ComputeInfeasibleSubsystemImplTest, SuccessfulCall) {
  BasicInfeasibleLp lp;

  ComputeInfeasibleSubsystemArguments args;
  args.parameters.enable_output = true;

  SolveInterrupter interrupter;
  args.interrupter = &interrupter;

  args.message_callback = [](absl::Span<const std::string>) {};

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(SOLVER_TYPE_GLOP, EquivToProto(lp.model.ExportModel()), _))
        .WillOnce(
            Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

    EXPECT_CALL(solver,
                ComputeInfeasibleSubsystem(ComputeInfeasibleSubsystemArgsAre(
                    EquivToProto(args.parameters.Proto()), Ne(nullptr),
                    Eq(&interrupter))))
        .WillOnce(Return(lp.InfeasibleResult()));
  }

  ASSERT_OK_AND_ASSIGN(
      const ComputeInfeasibleSubsystemResult result,
      ComputeInfeasibleSubsystemImpl(
          factory_mock.AsStdFunction(), lp.model, SolverType::kGlop, args,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  EXPECT_EQ(result.feasibility, FeasibilityStatus::kInfeasible);
}

TEST(ComputeInfeasibleSubsystemImplTest, FailingSolve) {
  BasicInfeasibleLp lp;

  ComputeInfeasibleSubsystemArguments args;
  args.parameters.enable_output = true;

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(SOLVER_TYPE_GLOP, EquivToProto(lp.model.ExportModel()), _))
        .WillOnce(
            Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

    EXPECT_CALL(
        solver,
        ComputeInfeasibleSubsystem(ComputeInfeasibleSubsystemArgsAre(
            EquivToProto(args.parameters.Proto()), Eq(nullptr), Eq(nullptr))))
        .WillOnce(Return(absl::InternalError("infeasible subsystem failed")));
  }

  ASSERT_THAT(
      ComputeInfeasibleSubsystemImpl(
          factory_mock.AsStdFunction(), lp.model, SolverType::kGlop, args,
          /*user_canceller=*/nullptr, /*remove_names=*/false),
      StatusIs(absl::StatusCode::kInternal, "infeasible subsystem failed"));
}

TEST(ComputeInfeasibleSubsystemImplTest, UserCancellation) {
  BasicLp basic_lp;

  // Will be set to the provided local_canceller in the factory.
  SolveInterrupter* provided_local_canceller = nullptr;

  const auto fake_solve =
      [&](const BaseSolver::ComputeInfeasibleSubsystemArgs& args)
      -> absl::StatusOr<ComputeInfeasibleSubsystemResultProto> {
    // The solver should have been cancelled before its
    // ComputeInfeasibleSubsystem() is called.
    EXPECT_TRUE(provided_local_canceller->IsInterrupted());
    return absl::CancelledError("solver has been cancelled");
  };

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(SOLVER_TYPE_GLOP,
                     EquivToProto(basic_lp.model.ExportModel()), Ne(nullptr)))
        .WillOnce([&](const SolverTypeProto solver_type,
                      const ModelProto& model,
                      SolveInterrupter* const local_canceller) {
          provided_local_canceller = local_canceller;
          return std::make_unique<DelegatingBaseSolver>(&solver);
        });

    EXPECT_CALL(solver, ComputeInfeasibleSubsystem(_)).WillOnce(fake_solve);
  }

  SolveInterrupter user_canceller;
  user_canceller.Interrupt();

  ASSERT_THAT(ComputeInfeasibleSubsystemImpl(
                  factory_mock.AsStdFunction(), basic_lp.model,
                  SolverType::kGlop, {}, /*user_canceller=*/&user_canceller,
                  /*remove_names=*/false),
              StatusIs(absl::StatusCode::kCancelled));
}

TEST(IncrementalSolverImplTest, NullModel) {
  BaseSolverFactoryMock factory_mock;

  EXPECT_THAT(IncrementalSolverImpl::New(
                  factory_mock.AsStdFunction(), nullptr, SolverType::kGlop,
                  /*user_canceller=*/nullptr, /*remove_names=*/false),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("model")));
}

TEST(IncrementalSolverImplTest, SolverType) {
  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;
  BasicLp basic_lp;

  EXPECT_CALL(factory_mock,
              Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()),
                   Ne(nullptr)))
      .WillOnce(
          Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> incremental_solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));
  EXPECT_EQ(incremental_solver->solver_type(), SolverType::kGlop);
}

// Test calling IncrementalSolver without any callback with a succeeding
// non-empty update.
TEST(IncrementalSolverImplTest, IncrementalSolveNoCallback) {
  BasicLp basic_lp;

  BaseSolverMock solver_interface;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  SolveInterrupter interrupter;
  args_1.interrupter = &interrupter;

  BaseSolverFactoryMock factory_mock;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                         args_1.model_parameters.Proto());
    EXPECT_CALL(
        solver_interface,
        Solve(SolveArgsAre(EquivToProto(args_1.parameters.Proto()),
                           EquivToProto(model_parameters_1), Eq(nullptr),
                           EquivToProto(args_1.callback_registration.Proto()),
                           Eq(nullptr), Eq(&interrupter))))
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
    EXPECT_CALL(
        solver_interface,
        Solve(SolveArgsAre(EquivToProto(args_2.parameters.Proto()),
                           EquivToProto(model_parameters_2), Eq(nullptr),
                           EquivToProto(args_2.callback_registration.Proto()),
                           Eq(nullptr), Eq(nullptr))))
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

TEST(IncrementalSolverImplTest, RemoveNamesSendsNoNamesOnModel) {
  Model model;
  model.AddBinaryVariable("x");

  ModelProto expected_model;
  expected_model.mutable_variables()->add_ids(0);
  expected_model.mutable_variables()->add_lower_bounds(0.0);
  expected_model.mutable_variables()->add_upper_bounds(1.0);
  expected_model.mutable_variables()->add_integers(true);

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;
  EXPECT_CALL(factory_mock,
              Call(SOLVER_TYPE_GLOP, EquivToProto(expected_model), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  EXPECT_OK(IncrementalSolverImpl::New(
      factory_mock.AsStdFunction(), &model, SolverType::kGlop,
      /*user_canceller=*/nullptr, /*remove_names=*/true));
}

TEST(IncrementalSolverImplTest, RemoveNamesSendsNoNamesOnModelUpdate) {
  Model model;

  SolveArguments args;

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP, _, _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IncrementalSolver> solver,
                       IncrementalSolverImpl::New(factory_mock.AsStdFunction(),
                                                  &model, SolverType::kGlop,
                                                  /*user_canceller=*/nullptr,
                                                  /*remove_names=*/true));

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

TEST(IncrementalSolverImplTest, RemoveNamesOnFullModelAfterUpdateFails) {
  Model model;

  SolveArguments args;

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP, _, _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IncrementalSolver> solver,
                       IncrementalSolverImpl::New(factory_mock.AsStdFunction(),
                                                  &model, SolverType::kGlop,
                                                  /*user_canceller=*/nullptr,
                                                  /*remove_names=*/true));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  model.AddBinaryVariable("x");

  ModelProto expected_model;
  expected_model.mutable_variables()->add_ids(0);
  expected_model.mutable_variables()->add_lower_bounds(0.0);
  expected_model.mutable_variables()->add_upper_bounds(1.0);
  expected_model.mutable_variables()->add_integers(true);

  EXPECT_CALL(solver_interface, Update(_)).WillOnce(Return(false));
  BaseSolverMock solver_interface2;
  EXPECT_CALL(factory_mock,
              Call(SOLVER_TYPE_GLOP, EquivToProto(expected_model), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface2))));

  ASSERT_OK_AND_ASSIGN(const UpdateResult update_result, solver->Update());
  EXPECT_FALSE(update_result.did_update);
}

// Test calling IncrementalSolver without any callback with an empty update.
TEST(IncrementalSolverImplTest, IncrementalSolveWithEmptyUpdate) {
  BasicLp basic_lp;

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  {
    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                         args_1.model_parameters.Proto());
    EXPECT_CALL(
        solver_interface,
        Solve(SolveArgsAre(EquivToProto(args_1.parameters.Proto()),
                           EquivToProto(model_parameters_1), Eq(nullptr),
                           EquivToProto(args_1.callback_registration.Proto()),
                           Eq(nullptr), Eq(nullptr))))
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
    EXPECT_CALL(
        solver_interface,
        Solve(SolveArgsAre(EquivToProto(args_2.parameters.Proto()),
                           EquivToProto(model_parameters_2), Eq(nullptr),
                           EquivToProto(args_2.callback_registration.Proto()),
                           Eq(nullptr), Eq(nullptr))))
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
TEST(IncrementalSolverImplTest, IncrementalSolveWithFailedUpdate) {
  BasicLp basic_lp;

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_1;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(
          Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver_1))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                       args_1.model_parameters.Proto());
  EXPECT_CALL(solver_1, Solve(SolveArgsAre(
                            EquivToProto(args_1.parameters.Proto()),
                            EquivToProto(model_parameters_1), Eq(nullptr),
                            EquivToProto(args_1.callback_registration.Proto()),
                            Eq(nullptr), Eq(nullptr))))
      .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a})));

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

  BaseSolverMock solver_2;

  {
    InSequence s;

    EXPECT_CALL(solver_1, Update(EquivToProto(*update)))
        .WillOnce(Return(false));

    EXPECT_CALL(
        factory_mock,
        Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(
            Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver_2))));
  }

  ASSERT_OK_AND_ASSIGN(const UpdateResult update_result, solver->Update());
  EXPECT_FALSE(update_result.did_update);

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);
  Mock::VerifyAndClearExpectations(&solver_2);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_2,
                       args_2.model_parameters.Proto());
  EXPECT_CALL(solver_2, Solve(SolveArgsAre(
                            EquivToProto(args_2.parameters.Proto()),
                            EquivToProto(model_parameters_2), Eq(nullptr),
                            EquivToProto(args_2.callback_registration.Proto()),
                            Eq(nullptr), Eq(nullptr))))
      .WillOnce(Return(basic_lp.OptimalResult({basic_lp.a, basic_lp.b},
                                              /*after_update=*/true)));

  ASSERT_OK_AND_ASSIGN(const SolveResult result_2,
                       solver->SolveWithoutUpdate(args_2));

  EXPECT_EQ(result_2.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(
      result_2.variable_values(),
      UnorderedElementsAre(Pair(basic_lp.a, 0.0), Pair(basic_lp.b, 2.0)));
}

// Test calling IncrementalSolver without any callback and with an impossible
// update, i.e. an update that contains an unsupported feature.
TEST(IncrementalSolverImplTest, IncrementalSolveWithImpossibleUpdate) {
  BasicLp basic_lp;

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_1;

  // The first solve.
  SolveArguments args_1;
  args_1.parameters.enable_output = true;
  args_1.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(
          Return(ByMove(std::make_unique<DelegatingBaseSolver>(&solver_1))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters_1,
                       args_1.model_parameters.Proto());
  EXPECT_CALL(solver_1, Solve(SolveArgsAre(
                            EquivToProto(args_1.parameters.Proto()),
                            EquivToProto(model_parameters_1), Eq(nullptr),
                            EquivToProto(args_1.callback_registration.Proto()),
                            Eq(nullptr), Eq(nullptr))))
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
    EXPECT_CALL(
        factory_mock,
        Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(absl::InternalError("*unsupported model*"))));
  }

  ASSERT_THAT(solver->Update(),
              StatusIs(absl::StatusCode::kInternal,
                       AllOf(HasSubstr("*unsupported model*"),
                             HasSubstr("solver re-creation failed"))));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_1);

  // Next calls should fail and not crash. Note that since we failed recreating
  // a new solver we still will use solver_1; and this solver will return an
  // error.

  EXPECT_CALL(solver_1, Update(_))
      .WillOnce(
          Return(ByMove(absl::InvalidArgumentError("previous call failed"))));

  basic_lp.model.set_lower_bound(basic_lp.a, -3.0);
  EXPECT_THAT(solver->Update(), StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("update failed")));
}

// Test calling IncrementalSolver with a callback. We don't test calling
// Update() here since only the Solve() function takes a callback.
TEST(IncrementalSolverImplTest, SuccessfulSolveWithCallback) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve = [&](const BaseSolver::SolveArgs& args)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    args.user_cb(cb_data);
    return basic_lp.OptimalResult({basic_lp.a});
  };

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(
      factory_mock,

      Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(
      solver_interface,
      Solve(SolveArgsAre(EquivToProto(args.parameters.Proto()),
                         EquivToProto(model_parameters), Eq(nullptr),
                         EquivToProto(args.callback_registration.Proto()),
                         Ne(nullptr), Eq(nullptr))))
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
TEST(IncrementalSolverImplTest, FailingSolverInstantiation) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;
  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(ByMove(absl::InternalError("instantiation failed"))));

  ASSERT_THAT(IncrementalSolverImpl::New(factory_mock.AsStdFunction(),
                                         &basic_lp.model, SolverType::kGlop,
                                         /*user_canceller=*/nullptr,
                                         /*remove_names=*/false),
              StatusIs(absl::StatusCode::kInternal, "instantiation failed"));
}

// Test calling IncrementalSolver with a solver that returns an error on
// SolverInterface::Solve().
TEST(IncrementalSolverImplTest, FailingSolver) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(
      solver_interface,
      Solve(SolveArgsAre(EquivToProto(args.parameters.Proto()),
                         EquivToProto(model_parameters), Eq(nullptr),
                         EquivToProto(args.callback_registration.Proto()),
                         Eq(nullptr), Eq(nullptr))))
      .WillOnce(Return(absl::InternalError("solve failed")));

  ASSERT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInternal, "solve failed"));
}

// Test calling IncrementalSolver with a solver that returns an error on
// SolverInterface::Update().
TEST(IncrementalSolverImplTest, FailingSolverUpdate) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

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
TEST(IncrementalSolverImplTest, UpdateAndSolve) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve = [&](const BaseSolver::SolveArgs& args)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    args.user_cb(cb_data);
    return basic_lp.OptimalResult({basic_lp.a});
  };

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(
      factory_mock,

      Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

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
    EXPECT_CALL(
        solver_interface,
        Solve(SolveArgsAre(EquivToProto(args.parameters.Proto()),
                           EquivToProto(model_parameters), Eq(nullptr),
                           EquivToProto(args.callback_registration.Proto()),
                           Ne(nullptr), Eq(nullptr))))
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
TEST(IncrementalSolverImplTest, UpdateAndSolveWithFailingSolver) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(
      solver_interface,
      Solve(SolveArgsAre(EquivToProto(args.parameters.Proto()),
                         EquivToProto(model_parameters), Eq(nullptr),
                         EquivToProto(args.callback_registration.Proto()),
                         Eq(nullptr), Eq(nullptr))))
      .WillOnce(Return(absl::InternalError("solve failed")));

  ASSERT_THAT(solver->Solve(args),
              StatusIs(absl::StatusCode::kInternal, "solve failed"));
}

// Test calling IncrementalSolver::Solve() with a solver that returns an error
// on SolverInterface::Update().
TEST(IncrementalSolverImplTest, UpdateAndSolveWithFailingSolverUpdate) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  const std::optional<ModelUpdateProto> update = basic_lp.UpdateUpperBoundOfB();
  ASSERT_TRUE(update);

  EXPECT_CALL(solver_interface, Update(EquivToProto(*update)))
      .WillOnce(Return(absl::InternalError("*update failure*")));

  ASSERT_THAT(solver->Solve({}), StatusIs(absl::StatusCode::kInternal,
                                          AllOf(HasSubstr("*update failure*"),
                                                HasSubstr("update failed"))));
}

TEST(IncrementalSolverImplTest, NullCallback) {
  BasicLp basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({basic_lp.a});

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  EXPECT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no callback was provided")));
}

TEST(IncrementalSolverImplTest, WrongModelInModelParameters) {
  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.model_parameters =
      ModelSolveParameters::OnlySomePrimalVariables({other_basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  EXPECT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(IncrementalSolverImplTest, WrongModelInCallbackRegistration) {
  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  // Here we use the wrong variable.
  args.callback_registration.mip_solution_filter =
      MakeKeepKeysFilter({other_basic_lp.a});

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  EXPECT_CALL(
      factory_mock,

      Call(SOLVER_TYPE_GLOP, EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  EXPECT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(IncrementalSolverImplTest, WrongModelInCallbackResult) {
  BasicLp basic_lp;
  BasicLp other_basic_lp;

  SolveArguments args;
  args.parameters.enable_output = true;

  args.callback_registration.add_lazy_constraints = true;
  args.callback_registration.events.insert(CallbackEvent::kMipSolution);

  const auto fake_solve = [&](const BaseSolver::SolveArgs& args)
      -> absl::StatusOr<SolveResultProto> {
    CallbackDataProto cb_data;
    cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    *cb_data.mutable_primal_solution_vector() = MakeSparseDoubleVector(
        {{basic_lp.a.id(), 1.0}, {basic_lp.b.id(), 0.0}});
    args.user_cb(cb_data);
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver_interface;

  args.callback = [&](const CallbackData& callback_data) {
    CallbackResult result;
    // We use the wrong model here.
    result.AddLazyConstraint(other_basic_lp.a + other_basic_lp.b <= 3);
    return result;
  };

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP,
                                 EquivToProto(basic_lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<IncrementalSolver> solver,
      IncrementalSolverImpl::New(
          factory_mock.AsStdFunction(), &basic_lp.model, SolverType::kGlop,
          /*user_canceller=*/nullptr, /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                       args.model_parameters.Proto());
  EXPECT_CALL(
      solver_interface,
      Solve(SolveArgsAre(EquivToProto(args.parameters.Proto()),
                         EquivToProto(model_parameters), Eq(nullptr),
                         EquivToProto(args.callback_registration.Proto()),
                         Ne(nullptr), Eq(nullptr))))
      .WillOnce(fake_solve);

  EXPECT_THAT(solver->SolveWithoutUpdate(args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(IncrementalSolverImplTest, ComputeInfeasibleSubsystem) {
  BasicInfeasibleLp lp;

  BaseSolverMock solver_interface;

  // The first computation.
  ComputeInfeasibleSubsystemArguments args_1;
  args_1.parameters.enable_output = true;

  SolveInterrupter interrupter;
  args_1.interrupter = &interrupter;

  BaseSolverFactoryMock factory_mock;
  EXPECT_CALL(factory_mock,
              Call(SOLVER_TYPE_GLOP, EquivToProto(lp.model.ExportModel()), _))
      .WillOnce(Return(
          ByMove(std::make_unique<DelegatingBaseSolver>(&solver_interface))));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IncrementalSolver> solver,
                       IncrementalSolverImpl::New(factory_mock.AsStdFunction(),
                                                  &lp.model, SolverType::kGlop,
                                                  /*user_canceller=*/nullptr,
                                                  /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  EXPECT_CALL(solver_interface,
              ComputeInfeasibleSubsystem(ComputeInfeasibleSubsystemArgsAre(
                  EquivToProto(args_1.parameters.Proto()), Eq(nullptr),
                  Eq(&interrupter))))
      .WillOnce(Return(lp.InfeasibleResult()));

  {
    ASSERT_OK_AND_ASSIGN(
        const ComputeInfeasibleSubsystemResult result,
        solver->ComputeInfeasibleSubsystemWithoutUpdate(args_1));
    EXPECT_EQ(result.feasibility, FeasibilityStatus::kInfeasible);
  }

  // Second computation with update.
  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver_interface);

  const std::optional<ModelUpdateProto> update = lp.UpdateUpperBoundOfC();
  ASSERT_TRUE(update);

  ComputeInfeasibleSubsystemArguments args_2;
  args_2.parameters.enable_output = true;

  {
    InSequence s;

    EXPECT_CALL(solver_interface, Update(EquivToProto(*update)))
        .WillOnce(Return(true));

    EXPECT_CALL(
        solver_interface,
        ComputeInfeasibleSubsystem(ComputeInfeasibleSubsystemArgsAre(
            EquivToProto(args_2.parameters.Proto()), Eq(nullptr), Eq(nullptr))))
        .WillOnce(Return(lp.InfeasibleResult()));
  }

  ASSERT_OK_AND_ASSIGN(const ComputeInfeasibleSubsystemResult result,
                       solver->ComputeInfeasibleSubsystem(args_2));
  EXPECT_EQ(result.feasibility, FeasibilityStatus::kInfeasible);
}

TEST(IncrementalSolverImplTest, UserCancellation) {
  BasicLp basic_lp;

  // Will be set to the provided local_canceller in the factory.
  SolveInterrupter* provided_local_canceller = nullptr;

  BaseSolverFactoryMock factory_mock;
  BaseSolverMock solver;

  EXPECT_CALL(factory_mock, Call(SOLVER_TYPE_GLOP, _, Ne(nullptr)))
      .WillOnce([&](const SolverTypeProto solver_type, const ModelProto& model,
                    SolveInterrupter* const local_canceller) {
        provided_local_canceller = local_canceller;
        return std::make_unique<DelegatingBaseSolver>(&solver);
      });

  SolveInterrupter user_canceller;

  ASSERT_OK_AND_ASSIGN(
      const std::unique_ptr<IncrementalSolver> incremental_solver,
      IncrementalSolverImpl::New(factory_mock.AsStdFunction(), &basic_lp.model,
                                 SolverType::kGlop,
                                 /*user_canceller=*/&user_canceller,
                                 /*remove_names=*/false));

  Mock::VerifyAndClearExpectations(&factory_mock);
  Mock::VerifyAndClearExpectations(&solver);

  ASSERT_NE(provided_local_canceller, nullptr);

  // Since user_canceller has not been cancelled yet the local canceller should
  // still be untriggered.
  EXPECT_FALSE(provided_local_canceller->IsInterrupted());

  // Triggering the user canceller should trigger the local canceller.
  user_canceller.Interrupt();
  EXPECT_TRUE(provided_local_canceller->IsInterrupted());
}

}  // namespace
}  // namespace operations_research::math_opt::internal
