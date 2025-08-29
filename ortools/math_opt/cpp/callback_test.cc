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

#include "ortools/math_opt/cpp/callback.h"

#include <limits>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/protoutil.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/cpp/enums_testing.h"
#include "ortools/math_opt/cpp/map_filter.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::EqualsProto;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

INSTANTIATE_TYPED_TEST_SUITE_P(CallbackEvent, EnumTest, CallbackEvent);

TEST(CallbackDataTest, Creation) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  CallbackDataProto proto;
  proto.set_event(CALLBACK_EVENT_MIP_NODE);
  auto var_values = proto.mutable_primal_solution_vector();
  var_values->add_ids(0);
  var_values->add_ids(1);
  var_values->add_values(3.0);
  var_values->add_values(5.0);
  ASSERT_OK_AND_ASSIGN(*proto.mutable_runtime(),
                       util_time::EncodeGoogleApiProto(absl::Seconds(11)));
  proto.mutable_presolve_stats()->set_removed_variables(3);
  proto.mutable_simplex_stats()->set_iteration_count(12);
  proto.mutable_barrier_stats()->set_primal_objective(10.0);
  proto.mutable_mip_stats()->set_explored_nodes(4);
  CallbackData cb_data(&storage, proto);
  EXPECT_EQ(cb_data.event, CallbackEvent::kMipNode);
  ASSERT_TRUE(cb_data.solution.has_value());
  EXPECT_THAT(*cb_data.solution,
              UnorderedElementsAre(Pair(x, 3.0), Pair(y, 5.0)));
  EXPECT_EQ(cb_data.runtime, absl::Seconds(11));
  EXPECT_THAT(cb_data.presolve_stats, EqualsProto("removed_variables: 3"));
  EXPECT_THAT(cb_data.simplex_stats, EqualsProto("iteration_count: 12"));
  EXPECT_THAT(cb_data.barrier_stats, EqualsProto("primal_objective: 10.0"));
  EXPECT_THAT(cb_data.mip_stats, EqualsProto("explored_nodes: 4"));

  EXPECT_OK(cb_data.CheckModelStorage(&storage));
  const ModelStorage other_storage;
  EXPECT_THAT(cb_data.CheckModelStorage(&other_storage),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid variable")));

  EXPECT_THAT(cb_data.Proto(), IsOkAndHolds(EquivToProto(proto)));
}

TEST(CallbackDataTest, CreationWithEventConstructor) {
  CallbackData cb_data(CallbackEvent::kMipNode, absl::Seconds(10));
  EXPECT_EQ(cb_data.event, CallbackEvent::kMipNode);
  EXPECT_EQ(cb_data.runtime, absl::Seconds(10));
}

TEST(CallbackDataTest, NoSolution) {
  ModelStorage storage;
  CallbackDataProto proto;
  proto.set_event(CALLBACK_EVENT_MIP_NODE);
  proto.mutable_mip_stats()->set_explored_nodes(4);
  CallbackData cb_data(&storage, proto);
  EXPECT_EQ(cb_data.event, CallbackEvent::kMipNode);
  EXPECT_FALSE(cb_data.solution.has_value());
  EXPECT_THAT(cb_data.mip_stats, EqualsProto("explored_nodes: 4"));

  EXPECT_OK(cb_data.CheckModelStorage(&storage));
  const ModelStorage other_storage;
  EXPECT_OK(cb_data.CheckModelStorage(&other_storage));

  EXPECT_THAT(cb_data.Proto(), IsOkAndHolds(EquivToProto(proto)));
}

TEST(CallbackDataTest, EmptySolution) {
  ModelStorage storage;
  CallbackDataProto proto;
  proto.set_event(CALLBACK_EVENT_MIP_NODE);
  proto.mutable_primal_solution_vector();
  proto.mutable_mip_stats()->set_explored_nodes(4);
  CallbackData cb_data(&storage, proto);
  EXPECT_EQ(cb_data.event, CallbackEvent::kMipNode);
  ASSERT_TRUE(cb_data.solution.has_value());
  EXPECT_THAT(*cb_data.solution, IsEmpty());
  EXPECT_THAT(cb_data.mip_stats, EqualsProto("explored_nodes: 4"));

  EXPECT_OK(cb_data.CheckModelStorage(&storage));
  const ModelStorage other_storage;
  EXPECT_OK(cb_data.CheckModelStorage(&other_storage));

  EXPECT_THAT(cb_data.Proto(), IsOkAndHolds(EquivToProto(proto)));
}

TEST(CallbackDataTest, InvalidRuntime) {
  EXPECT_THAT(
      CallbackData(
          /*event=*/CallbackEvent::kPresolve,
          /*runtime=*/absl::InfiniteDuration())
          .Proto(),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("runtime")));
}

TEST(CallbackRegistrationTest, ProtoRoundtrip) {
  Model model;
  model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  // Test with all fields set, and boolean fields to true (non default
  // value). Here we assume we use the same function for the mip_node_filter and
  // the mip_solution_filter, and thus test the skip_zero_values with one and
  // the filter_by_ids for the other.
  {
    CallbackRegistrationProto registration_proto;
    registration_proto.add_request_registration(CALLBACK_EVENT_MIP_SOLUTION);
    registration_proto.add_request_registration(CALLBACK_EVENT_MIP_NODE);
    registration_proto.mutable_mip_node_filter()->set_skip_zero_values(true);
    registration_proto.mutable_mip_solution_filter()->set_filter_by_ids(true);
    registration_proto.mutable_mip_solution_filter()->add_filtered_ids(y.id());
    registration_proto.set_add_cuts(true);
    registration_proto.set_add_lazy_constraints(true);

    ASSERT_OK_AND_ASSIGN(
        const CallbackRegistration registration,
        CallbackRegistration::FromProto(model, registration_proto));

    EXPECT_OK(registration.CheckModelStorage(model.storage()));
    EXPECT_THAT(registration.Proto(), EquivToProto(registration_proto));
  }

  // Test with boolean field to false (default value).
  {
    CallbackRegistrationProto registration_proto;
    registration_proto.set_add_cuts(false);
    registration_proto.set_add_lazy_constraints(false);

    ASSERT_OK_AND_ASSIGN(
        const CallbackRegistration registration,
        CallbackRegistration::FromProto(model, registration_proto));

    EXPECT_OK(registration.CheckModelStorage(model.storage()));
    EXPECT_THAT(registration.Proto(), EquivToProto(registration_proto));
  }
}

TEST(CallbackRegistrationTest, CheckModelStorageMixedModels) {
  ModelStorage storage_1;
  const Variable x(&storage_1, storage_1.AddVariable("x"));
  ModelStorage storage_2;
  const Variable y(&storage_2, storage_2.AddVariable("y"));
  CallbackRegistration registration;
  registration.events = {CallbackEvent::kMipNode, CallbackEvent::kMipSolution};
  registration.mip_node_filter.filtered_keys = {x};
  registration.mip_solution_filter.filtered_keys = {y};
  EXPECT_THAT(registration.CheckModelStorage(&storage_1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(CallbackRegistrationTest, FromProtoBadEvent) {
  CallbackRegistrationProto registration_proto;
  registration_proto.add_request_registration(CALLBACK_EVENT_UNSPECIFIED);
  ASSERT_THAT(
      CallbackRegistration::FromProto(Model{}, registration_proto),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("CallbackRegistrationProto.request_registration[0]"),
                HasSubstr("CALLBACK_EVENT_UNSPECIFIED"))));
}

TEST(CallbackRegistrationTest, FromProtoRepeatedEvent) {
  CallbackRegistrationProto registration_proto;
  registration_proto.add_request_registration(CALLBACK_EVENT_MIP_NODE);
  registration_proto.add_request_registration(CALLBACK_EVENT_MIP_NODE);
  ASSERT_THAT(
      CallbackRegistration::FromProto(Model{}, registration_proto),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("CallbackRegistrationProto.request_registration[1]"),
                HasSubstr("mip_node"))));
}

TEST(CallbackRegistrationTest, FromProtoInvalidMipSolutionFilterId) {
  CallbackRegistrationProto registration_proto;
  registration_proto.mutable_mip_solution_filter()->set_filter_by_ids(true);
  registration_proto.mutable_mip_solution_filter()->add_filtered_ids(1);
  ASSERT_THAT(CallbackRegistration::FromProto(Model{}, registration_proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("mip_solution_filter"),
                             HasSubstr("id: 1 not in model"))));
}

TEST(CallbackRegistrationTest, FromProtoInvalidMipNodeFilterId) {
  CallbackRegistrationProto registration_proto;
  registration_proto.mutable_mip_node_filter()->set_filter_by_ids(true);
  registration_proto.mutable_mip_node_filter()->add_filtered_ids(1);
  ASSERT_THAT(CallbackRegistration::FromProto(Model{}, registration_proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("mip_node_filter"),
                             HasSubstr("id: 1 not in model"))));
}

TEST(CallbackResultTest, ProtoRoundtrip) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  // Here we don't start with the proto, as below we also test
  // AddLazyConstraint() and AddUserCut() behavior. We instead start with a
  // CallbackResult and build an expected proto.
  CallbackResult result;
  result.terminate = true;
  result.suggested_solutions.push_back({{x, 2.0}, {y, 3.0}});
  result.suggested_solutions.push_back({{y, 5.5}});
  result.AddLazyConstraint(x + y <= 2.0);
  result.AddUserCut(y <= 1.0);
  result.AddUserCut(-1.0 <= 2 * x + y);

  CallbackResultProto expected_result_proto;
  expected_result_proto.set_terminate(true);
  {
    CallbackResultProto::GeneratedLinearConstraint& cut =
        *expected_result_proto.add_cuts();
    cut.set_lower_bound(-kInf);
    cut.set_upper_bound(2.0);
    cut.set_is_lazy(true);
    cut.mutable_linear_expression()->add_ids(0);
    cut.mutable_linear_expression()->add_values(1.0);
    cut.mutable_linear_expression()->add_ids(1);
    cut.mutable_linear_expression()->add_values(1.0);
  }
  {
    CallbackResultProto::GeneratedLinearConstraint& cut =
        *expected_result_proto.add_cuts();
    cut.set_lower_bound(-kInf);
    cut.set_upper_bound(1.0);
    cut.set_is_lazy(false);
    cut.mutable_linear_expression()->add_ids(1);
    cut.mutable_linear_expression()->add_values(1.0);
  }
  {
    CallbackResultProto::GeneratedLinearConstraint& cut =
        *expected_result_proto.add_cuts();
    cut.set_lower_bound(-1.0);
    cut.set_upper_bound(kInf);
    cut.set_is_lazy(false);
    cut.mutable_linear_expression()->add_ids(0);
    cut.mutable_linear_expression()->add_values(2.0);
    cut.mutable_linear_expression()->add_ids(1);
    cut.mutable_linear_expression()->add_values(1.0);
  }
  {
    SparseDoubleVectorProto& suggested_solution =
        *expected_result_proto.add_suggested_solutions();
    suggested_solution.add_ids(0);
    suggested_solution.add_values(2.0);
    suggested_solution.add_ids(1);
    suggested_solution.add_values(3.0);
  }
  {
    SparseDoubleVectorProto& suggested_solution =
        *expected_result_proto.add_suggested_solutions();
    suggested_solution.add_ids(1);
    suggested_solution.add_values(5.5);
  }

  EXPECT_OK(result.CheckModelStorage(model.storage()));
  EXPECT_THAT(result.Proto(), EqualsProto(expected_result_proto));

  // Now test the round trip using the expected proto.
  {
    ASSERT_OK_AND_ASSIGN(
        const CallbackResult result_from_proto,
        CallbackResult::FromProto(model, expected_result_proto));
    EXPECT_THAT(result_from_proto.Proto(), EqualsProto(expected_result_proto));
  }
}

TEST(CallbackResultTest, MixedModels) {
  ModelStorage storage_1;
  const Variable x(&storage_1, storage_1.AddVariable("x"));
  ModelStorage storage_2;
  const Variable y(&storage_2, storage_2.AddVariable("y"));
  CallbackResult result;
  result.AddLazyConstraint(x <= 1.0);
  result.suggested_solutions.push_back({{y, 1.0}});
  EXPECT_THAT(result.CheckModelStorage(&storage_1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(internal::kInputFromInvalidModelStorage)));
}

TEST(CallbackResultTest, FromProtoBadCutVariable) {
  CallbackResultProto result_proto;
  {
    CallbackResultProto::GeneratedLinearConstraint& cut =
        *result_proto.add_cuts();
    cut.mutable_linear_expression()->add_ids(1);
    cut.mutable_linear_expression()->add_values(1.0);
  }
  EXPECT_THAT(
      CallbackResult::FromProto(Model{}, result_proto),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("CallbackResultProto.cuts[0].linear_expression")));
}

TEST(CallbackResultTest, FromProtoBadSolutionVariable) {
  CallbackResultProto result_proto;
  {
    SparseDoubleVectorProto& suggested_solution =
        *result_proto.add_suggested_solutions();
    suggested_solution.add_ids(1);
    suggested_solution.add_values(5.5);
  }
  EXPECT_THAT(
      CallbackResult::FromProto(Model{}, result_proto),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("CallbackResultProto.suggested_solutions[0]")));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
