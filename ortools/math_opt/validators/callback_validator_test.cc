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

#include "ortools/math_opt/validators/callback_validator.h"

#include <cstdint>
#include <initializer_list>
#include <string>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::absl::StatusCode;
using ::google::protobuf::contrib::parse_proto::ParseTextProto;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::ValuesIn;
using ::testing::status::StatusIs;

ModelSummary InitModelSummary(
    const std::initializer_list<int64_t>& variable_ids) {
  ModelSummary result;
  for (const int64_t id : variable_ids) {
    CHECK_OK(result.variables.Insert(id, ""));
  }
  return result;
}

absl::StatusOr<CallbackRegistrationProto> ParseCBRegistration(
    absl::string_view input) {
  return ParseTextProto<CallbackRegistrationProto>(input);
}

absl::StatusOr<CallbackResultProto> ParseCBResultProto(
    absl::string_view input) {
  return ParseTextProto<CallbackResultProto>(input);
}

TEST(ValidateCallbackResultProto, EmptyOk) {
  const CallbackResultProto callback_result;
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                       )pb"));
  EXPECT_OK(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({})));
}

TEST(ValidateCallbackResultProto, CutsReturnedOutsideMipNodeOrSolution) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ -1 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: 0.0
                           upper_bound: 0.0
                           is_lazy: true
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_UNSPECIFIED
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_UNSPECIFIED,
                                  callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("can't return cuts for callback_event")));
}

// Check IdsAndValues is called for cuts
TEST(ValidateCallbackResultProto, CutsInvalidIdsAndValues) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ -1 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: 0.0
                           upper_bound: 0.0
                           is_lazy: true
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Expected ids to be nonnegative")));
}

TEST(ValidateCallbackResultProto, CutsIdNotInModel) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ 1 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: 0.0
                           upper_bound: 0.0
                           is_lazy: true
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({0})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("is missing from model IDs")));
}

// Check CheckScalar is called for cut lower_bound
TEST(ValidateCallbackResultProto, CutsInfiniteLowerBound) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ 0 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: infinity
                           upper_bound: 0.0
                           is_lazy: true
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({0})),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid positive infinite value"),
                     HasSubstr("GeneratedLinearConstraint.lower_bound"))));
}

// Check CheckScalar is called for cut upper_bound
TEST(ValidateCallbackResultProto, CutsNegativeInfiniteUpperBound) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ 0 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: 0.0
                           upper_bound: -infinity
                           is_lazy: true
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({0})),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid negative infinite value"),
                     HasSubstr("GeneratedLinearConstraint.upper_bound"))));
}

TEST(ValidateCallbackResultProto, CutsInfiniteBounds) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ 0 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: -infinity
                           upper_bound: infinity
                           is_lazy: true
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({0})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("bounds [-inf,inf]")));
}

TEST(ValidateCallbackResultProto, CutsInvalidLazy) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ 0 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: 0.0
                           upper_bound: 0.0
                           is_lazy: true
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                         add_lazy_constraints: false
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({0})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("invalid GeneratedLinearConstraint "
                         "with lazy attribute set to true")));
}

TEST(ValidateCallbackResultProto, CutsInvalidNonLazy) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ 0 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: 0.0
                           upper_bound: 0.0
                           is_lazy: false
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                         add_cuts: false
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({0})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("invalid GeneratedLinearConstraint with lazy "
                         "attribute set to false")));
}

TEST(ValidateCallbackResultProto, CutsOk) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts:
                         [ {
                           linear_expression: {
                             ids: [ 0 ]
                             values: [ 0.0 ]
                           }
                           lower_bound: 0.0
                           upper_bound: 0.0
                           is_lazy: false
                         }]
                         suggested_solutions: []
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                         add_cuts: true
                       )pb"));
  EXPECT_OK(ValidateCallbackResultProto(
      callback_result, CALLBACK_EVENT_MIP_NODE, callback_registration,
      InitModelSummary({0})));
}

TEST(ValidateCallbackResultProto, SolutionReturnedOutsideMipNodeOrSolution) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts: []
                         suggested_solutions:
                         [ {
                           ids: [ -1 ]
                           values: [ 0.0 ]
                         }]
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_MIP_NODE
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_MIP_NODE,
                                  callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Expected ids to be nonnegative")));
}

TEST(ValidateCallbackResultProto, SolutionInvalid) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts: []
                         suggested_solutions:
                         [ {
                           ids: [ -1 ]
                           values: [ 0.0 ]
                         }]
                       )pb"));
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_UNSPECIFIED
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackResultProto(callback_result, CALLBACK_EVENT_UNSPECIFIED,
                                  callback_registration, InitModelSummary({})),
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("can't return suggested solutions for callback_event")));
}

TEST(ValidateCallbackResultProto, SolutionOk) {
  ASSERT_OK_AND_ASSIGN(const CallbackResultProto callback_result,
                       ParseCBResultProto(R"pb(
                         terminate: false
                         cuts: []
                         suggested_solutions:
                         [ {
                           ids: [ 0 ]
                           values: [ 0.0 ]
                         }]
                       )pb"));
  ASSERT_OK_AND_ASSIGN(
      const CallbackRegistrationProto callback_registration_mip_node,
      ParseCBRegistration(R"pb(
        request_registration: CALLBACK_EVENT_MIP_NODE
      )pb"));
  ASSERT_OK_AND_ASSIGN(
      const CallbackRegistrationProto callback_registration_mip_solution,
      ParseCBRegistration(R"pb(
        request_registration: CALLBACK_EVENT_MIP_SOLUTION
      )pb"));
  EXPECT_OK(ValidateCallbackResultProto(
      callback_result, CALLBACK_EVENT_MIP_NODE, callback_registration_mip_node,
      InitModelSummary({0})));
  EXPECT_OK(ValidateCallbackResultProto(
      callback_result, CALLBACK_EVENT_MIP_SOLUTION,
      callback_registration_mip_solution, InitModelSummary({0, 1})));
}

TEST(CallbackRegistrationProtoTest, InvalidEventUnspecified) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: CALLBACK_EVENT_UNSPECIFIED
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackRegistration(callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("invalid event")));
}

TEST(CallbackRegistrationProtoTest, InvalidEventValue) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: -1
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackRegistration(callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("invalid event")));
}

TEST(CallbackRegistrationProtoTest, InvalidSolutionFilterId) {
  ASSERT_OK_AND_ASSIGN(
      const CallbackRegistrationProto callback_registration,
      ParseCBRegistration(R"pb(
        mip_solution_filter: { filter_by_ids: true filtered_ids: 0 }
      )pb"));
  EXPECT_THAT(
      ValidateCallbackRegistration(callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("filtered_ids"),
                     HasSubstr("missing from model IDs"))));
}

TEST(CallbackRegistrationProtoTest, InvalidNodeFilterId) {
  ASSERT_OK_AND_ASSIGN(
      const CallbackRegistrationProto callback_registration,
      ParseCBRegistration(R"pb(
        mip_node_filter: { filter_by_ids: true filtered_ids: 0 }
      )pb"));
  EXPECT_THAT(
      ValidateCallbackRegistration(callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("filtered_ids"),
                     HasSubstr("missing from model IDs"))));
}

TEST(CallbackRegistrationProtoTest, InvalidNodeFilterOrder) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         mip_node_filter: {
                           filter_by_ids: true
                           filtered_ids: [ 1, 0 ]
                         }
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackRegistration(callback_registration,
                                   InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("strictly increasing")));
}

TEST(CallbackRegistrationProtoTest, EmptyRegistrationOk) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb()pb"));
  EXPECT_OK(ValidateCallbackRegistration(callback_registration,
                                         InitModelSummary({})));
}

TEST(CallbackRegistrationProtoTest, CutsWithoutEventNode) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: [ CALLBACK_EVENT_MIP_SOLUTION ]
                         add_cuts: true
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackRegistration(callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("only add cuts at event CALLBACK_EVENT_MIP_NODE")));
}

TEST(CallbackRegistrationProtoTest, LazyWithoutEventNodeOrSolution) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: [ CALLBACK_EVENT_SIMPLEX ]
                         add_lazy_constraints: true
                       )pb"));
  EXPECT_THAT(
      ValidateCallbackRegistration(callback_registration, InitModelSummary({})),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr(
                   "only add lazy constraints at events "
                   "CALLBACK_EVENT_MIP_NODE and CALLBACK_EVENT_MIP_SOLUTION")));
}

TEST(CallbackRegistrationProtoTest, CutsLazyAreOk) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: [ CALLBACK_EVENT_MIP_NODE ]
                         add_lazy_constraints: true
                         add_cuts: true
                       )pb"));
  EXPECT_OK(ValidateCallbackRegistration(callback_registration,
                                         InitModelSummary({})));
}

TEST(CallbackRegistrationProtoTest, MipNodeNoCutsOrLazyOk) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: [ CALLBACK_EVENT_MIP_NODE ]
                       )pb"));
  EXPECT_OK(ValidateCallbackRegistration(callback_registration,
                                         InitModelSummary({})));
}

TEST(CallbackRegistrationProtoTest, MipSolutionLazyOk) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: [ CALLBACK_EVENT_MIP_SOLUTION ]
                         add_lazy_constraints: true
                       )pb"));
  EXPECT_OK(ValidateCallbackRegistration(callback_registration,
                                         InitModelSummary({})));
}

TEST(CallbackRegistrationProtoTest, MipSolutionNoLazyOk) {
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: [ CALLBACK_EVENT_MIP_SOLUTION ]
                       )pb"));
  EXPECT_OK(ValidateCallbackRegistration(callback_registration,
                                         InitModelSummary({})));
}

CallbackRegistrationProto FullRegistration() {
  CallbackRegistrationProto full_registration;
  full_registration.set_add_cuts(true);
  full_registration.set_add_lazy_constraints(true);
  full_registration.mutable_mip_node_filter()->set_skip_zero_values(true);
  full_registration.mutable_mip_node_filter()->set_filter_by_ids(true);
  full_registration.mutable_mip_node_filter()->add_filtered_ids(0);
  full_registration.mutable_mip_node_filter()->add_filtered_ids(1);
  full_registration.mutable_mip_solution_filter()->set_skip_zero_values(true);
  full_registration.mutable_mip_solution_filter()->set_filter_by_ids(true);
  full_registration.mutable_mip_solution_filter()->add_filtered_ids(0);
  full_registration.mutable_mip_solution_filter()->add_filtered_ids(1);
  full_registration.add_request_registration(CALLBACK_EVENT_PRESOLVE);
  full_registration.add_request_registration(CALLBACK_EVENT_SIMPLEX);
  full_registration.add_request_registration(CALLBACK_EVENT_MIP);
  full_registration.add_request_registration(CALLBACK_EVENT_MIP_SOLUTION);
  full_registration.add_request_registration(CALLBACK_EVENT_MIP_NODE);
  full_registration.add_request_registration(CALLBACK_EVENT_BARRIER);
  return full_registration;
}

CallbackDataProto ParseCBData(absl::string_view input) {
  CallbackDataProto result;
  CHECK(google::protobuf::TextFormat::ParseFromString(input, &result));
  return result;
}

TEST(CallbackDataProtoTest, UnregisteredEvent) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    simplex_stats: { primal_infeasibility: -1 })pb");
  ASSERT_OK_AND_ASSIGN(const CallbackRegistrationProto callback_registration,
                       ParseCBRegistration(R"pb(
                         request_registration: [
                           CALLBACK_EVENT_SIMPLEX,
                           CALLBACK_EVENT_MIP,
                           CALLBACK_EVENT_MIP_SOLUTION,
                           CALLBACK_EVENT_MIP_NODE,
                           CALLBACK_EVENT_BARRIER
                         ])pb"));
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, callback_registration,
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("not part of the registered_events")));
}

TEST(CallbackDataProtoTest, EmptyDataOk) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE)pb");
  EXPECT_OK(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                      InitModelSummary({0, 1})));
}

TEST(CallbackDataProtoTest, InvalidBoundChanges) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    presolve_stats: { bound_changes: -1 }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("bound_changes")));
}

TEST(CallbackDataProtoTest, InvalidCoefficientChanges) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    presolve_stats: { coefficient_changes: -1 }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("coefficient_changes")));
}

TEST(CallbackDataProtoTest, InvalidRuntimeSeconds) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    runtime: { nanos: -1 }
  )pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("nanos")));
}

TEST(CallbackDataProtoTest, InvalidRuntimeNanos) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    runtime: { seconds: -1 }
  )pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("seconds")));
}

TEST(CallbackDataProtoTest, InvalidSimplexIterationCount) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    simplex_stats: { iteration_count: -1 }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("iteration_count")));
}

TEST(CallbackDataProtoTest, InvalidSimplexObjectiveValueNan) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    simplex_stats: { objective_value: nan }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("objective_value")));
}

TEST(CallbackDataProtoTest, InvalidSimplexObjectiveValueInfinity) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    simplex_stats: { objective_value: infinity }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("objective_value")));
}

TEST(CallbackDataProtoTest, InvalidSimplexPrimalInfeasibility) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    simplex_stats: { primal_infeasibility: -1 }
  )pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("primal_infeasibility")));
}

TEST(CallbackDataProtoTest, InvalidSimplexDualInfeasibility) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    simplex_stats: { dual_infeasibility: -1 }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("dual_infeasibility")));
}

TEST(CallbackDataProtoTest, InvalidBarrierIterationCount) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    barrier_stats: { iteration_count: -1 }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("iteration_count")));
}

TEST(CallbackDataProtoTest, InvalidBarrierPrimalObjective) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    barrier_stats: { primal_objective: infinity }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("primal_objective")));
}

TEST(CallbackDataProtoTest, InvalidBarrierDualObjective) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    barrier_stats: { dual_objective: infinity }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("dual_objective")));
}

TEST(CallbackDataProtoTest, InvalidBarrierComplementarity) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    barrier_stats: { complementarity: infinity }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("complementarity")));
}

TEST(CallbackDataProtoTest, InvalidMipPrimalBound) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { primal_bound: nan }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("primal_bound")));
}

TEST(CallbackDataProtoTest, ValidMipPrimalBoundInfinity) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { primal_bound: infinity }
  )pb");
  EXPECT_OK(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                      InitModelSummary({0, 1})));
}

TEST(CallbackDataProtoTest, ValidMipPrimalBoundNegativeInfinity) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { primal_bound: -infinity }
  )pb");
  EXPECT_OK(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                      InitModelSummary({0, 1})));
}

TEST(CallbackDataProtoTest, InvalidMipDualBound) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { dual_bound: nan }
  )pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("dual_bound")));
}

TEST(CallbackDataProtoTest, ValidMipDualBoundInfinity) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { dual_bound: infinity }
  )pb");
  EXPECT_OK(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                      InitModelSummary({0, 1})));
}

TEST(CallbackDataProtoTest, ValidMipDualBoundNegativeInfinity) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { dual_bound: -infinity }
  )pb");
  EXPECT_OK(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                      InitModelSummary({0, 1})));
}

TEST(CallbackDataProtoTest, InvalidMipExploredNodes) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { explored_nodes: -1 }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("explored_nodes")));
}

TEST(CallbackDataProtoTest, InvalidMipOpenNodes) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { open_nodes: -1 }
  )pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("open_nodes")));
}

TEST(CallbackDataProtoTest, InvalidMipSimplexIterations) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { simplex_iterations: -1 }
  )pb");
  EXPECT_THAT(
      ValidateCallbackDataProto(callback_data, FullRegistration(),
                                InitModelSummary({0, 1})),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("simplex_iterations")));
}

TEST(CallbackDataProtoTest, InvalidMipNumberOfSolutions) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { number_of_solutions_found: -1 }
  )pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("number_of_solutions_found")));
}

TEST(CallbackDataProtoTest, InvalidMipCuttingPlanes) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_PRESOLVE
    mip_stats: { cutting_planes_in_lp: -1 }
  )pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("cutting_planes_in_lp")));
}

std::string PrintCallbackEventProto(
    const testing::TestParamInfo<CallbackEventProto>& info) {
  // Test names should only have ASCII alpha numeric characters.
  return absl::StrReplaceAll(CallbackEventProto_Name(info.param), {{"_", ""}});
}

class NoSolutionEvent : public ::testing::TestWithParam<CallbackEventProto> {};

TEST_P(NoSolutionEvent, InvalidSolutionForEvent) {
  const CallbackEventProto event = GetParam();
  CallbackDataProto callback_data = ParseCBData(R"pb(
    primal_solution_vector: { ids: 0 values: 1 })pb");
  callback_data.set_event(event);
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("can't provide primal_solution_vector")));
}

INSTANTIATE_TEST_SUITE_P(NoSolutionEvents, NoSolutionEvent,
                         ValuesIn({CALLBACK_EVENT_PRESOLVE,
                                   CALLBACK_EVENT_SIMPLEX, CALLBACK_EVENT_MIP,
                                   CALLBACK_EVENT_BARRIER}),
                         PrintCallbackEventProto);

class PrimalSolutionEvent
    : public ::testing::TestWithParam<CallbackEventProto> {
 public:
  PrimalSolutionEvent()
      : callback_data_(ParseCBData(R"pb(
          primal_solution_vector: { ids: 0 values: 1 })pb")),
        variable_values_(callback_data_.mutable_primal_solution_vector()) {}
  CallbackDataProto callback_data_;
  SparseDoubleVectorProto* variable_values_;
};

TEST_P(PrimalSolutionEvent, ValidSolution) {
  const CallbackEventProto event = GetParam();
  callback_data_.set_event(event);
  EXPECT_OK(ValidateCallbackDataProto(callback_data_, FullRegistration(),
                                      InitModelSummary({0, 1})));
}

TEST_P(PrimalSolutionEvent, InvalidZeroValue) {
  const CallbackEventProto event = GetParam();
  callback_data_.set_event(event);
  variable_values_->add_ids(1);
  variable_values_->add_values(0.0);
  EXPECT_THAT(ValidateCallbackDataProto(callback_data_, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("should not contain the pair")));
}

TEST_P(PrimalSolutionEvent, InvalidFilteredId) {
  const CallbackEventProto event = GetParam();
  callback_data_.set_event(event);
  variable_values_->add_ids(2);
  variable_values_->add_values(1);
  EXPECT_THAT(ValidateCallbackDataProto(callback_data_, FullRegistration(),
                                        InitModelSummary({0, 1, 2})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("should not contain the pair")));
}

TEST_P(PrimalSolutionEvent, InvalidIdInSolution) {
  CallbackRegistrationProto callback_registration = FullRegistration();
  callback_registration.mutable_mip_node_filter()->set_filter_by_ids(false);
  callback_registration.mutable_mip_solution_filter()->set_filter_by_ids(false);
  const CallbackEventProto event = GetParam();
  callback_data_.set_event(event);
  variable_values_->add_ids(2);
  variable_values_->add_values(1);
  EXPECT_THAT(ValidateCallbackDataProto(callback_data_, callback_registration,
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("missing from model IDs")));
}

INSTANTIATE_TEST_SUITE_P(PrimalSolutionsEvents, PrimalSolutionEvent,
                         ValuesIn({CALLBACK_EVENT_MIP_SOLUTION,
                                   CALLBACK_EVENT_MIP_NODE}),
                         PrintCallbackEventProto);

TEST(CallbackDataProtoTest, NoSolutionInMipNodeEventIsOk) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_MIP_NODE)pb");
  EXPECT_OK(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                      InitModelSummary({0, 1})));
}

TEST(CallbackDataProtoTest, NoSolutionInMipSolutionEventIsNotOk) {
  const CallbackDataProto callback_data = ParseCBData(R"pb(
    event: CALLBACK_EVENT_MIP_SOLUTION)pb");
  EXPECT_THAT(ValidateCallbackDataProto(callback_data, FullRegistration(),
                                        InitModelSummary({0, 1})),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("must provide primal_solution_vector")));
}

TEST(CheckRegisteredCallbackEventsTest, Empty) {
  EXPECT_OK(CheckRegisteredCallbackEvents(/*registration=*/{},
                                          /*supported_events=*/{}));
  EXPECT_OK(CheckRegisteredCallbackEvents(/*registration=*/{},
                                          /*supported_events=*/{
                                              CALLBACK_EVENT_MIP,
                                          }));
}

TEST(CheckRegisteredCallbackEventsTest, AllValid) {
  CallbackRegistrationProto registration;
  registration.mutable_request_registration()->Add(CALLBACK_EVENT_MIP);
  registration.mutable_request_registration()->Add(CALLBACK_EVENT_MIP_SOLUTION);
  EXPECT_OK(CheckRegisteredCallbackEvents(registration,
                                          /*supported_events=*/{
                                              CALLBACK_EVENT_PRESOLVE,
                                              CALLBACK_EVENT_MIP,
                                              CALLBACK_EVENT_MIP_SOLUTION,
                                          }));
}

TEST(CheckRegisteredCallbackEventsTest, OneInvalid) {
  CallbackRegistrationProto registration;
  registration.mutable_request_registration()->Add(CALLBACK_EVENT_MIP);
  registration.mutable_request_registration()->Add(CALLBACK_EVENT_MIP_SOLUTION);
  EXPECT_THAT(CheckRegisteredCallbackEvents(registration,
                                            /*supported_events=*/
                                            {
                                                CALLBACK_EVENT_PRESOLVE,
                                                CALLBACK_EVENT_MIP_SOLUTION,
                                            }),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "event CALLBACK_EVENT_MIP is not supported"));
}

TEST(CheckRegisteredCallbackEventsTest, MultipleInvalid) {
  CallbackRegistrationProto registration;
  registration.mutable_request_registration()->Add(CALLBACK_EVENT_MIP);
  registration.mutable_request_registration()->Add(CALLBACK_EVENT_SIMPLEX);
  registration.mutable_request_registration()->Add(CALLBACK_EVENT_MIP_SOLUTION);
  EXPECT_THAT(CheckRegisteredCallbackEvents(registration,
                                            /*supported_events=*/
                                            {
                                                CALLBACK_EVENT_PRESOLVE,
                                                CALLBACK_EVENT_MIP_SOLUTION,
                                            }),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "events { CALLBACK_EVENT_SIMPLEX, CALLBACK_EVENT_MIP } "
                       "are not supported"));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
