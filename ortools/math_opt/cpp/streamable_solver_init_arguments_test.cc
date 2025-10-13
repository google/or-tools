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

#include "ortools/math_opt/cpp/streamable_solver_init_arguments.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::EqualsProto;

TEST(StreamableInitArgumentsTest, Empty) {
  const SolverInitializerProto args_proto;
  ASSERT_OK_AND_ASSIGN(const StreamableSolverInitArguments args,
                       StreamableSolverInitArguments::FromProto(args_proto));
  EXPECT_THAT(args.Proto(), EqualsProto(args_proto));
}

TEST(StreamableGurobiInitArgumentsTest, ISV) {
  SolverInitializerProto args_proto;
  GurobiInitializerProto::ISVKey& isv_key_proto =
      *args_proto.mutable_gurobi()->mutable_isv_key();
  isv_key_proto.set_name("the name");
  isv_key_proto.set_application_name("the application");
  isv_key_proto.set_expiration(15);
  isv_key_proto.set_key("the key");

  ASSERT_OK_AND_ASSIGN(const StreamableSolverInitArguments args,
                       StreamableSolverInitArguments::FromProto(args_proto));
  EXPECT_THAT(args.Proto(), EqualsProto(args_proto));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
