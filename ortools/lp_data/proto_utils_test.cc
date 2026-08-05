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

#include "ortools/lp_data/proto_utils.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {
namespace {
using ::testing::EqualsProto;

TEST(ConversionTest, DoubleConversion) {
  MPModelProto test_proto;
  test_proto.set_maximize(true);
  test_proto.set_objective_offset(10);
  test_proto.set_name("Test");

  MPVariableProto* variable1 = test_proto.add_variable();
  variable1->set_lower_bound(-80);
  variable1->set_upper_bound(60);
  variable1->set_name("variable1");
  variable1->set_is_integer(false);
  variable1->set_objective_coefficient(10);

  MPConstraintProto* constraint = test_proto.add_constraint();
  constraint->set_lower_bound(-kInfinity);
  constraint->set_upper_bound(80);
  constraint->set_name("constraint1");
  constraint->add_var_index(0);
  constraint->add_coefficient(10.0);

  LinearProgram program;
  MPModelProtoToLinearProgram(test_proto, &program);
  MPModelProto output_proto;
  LinearProgramToMPModelProto(program, &output_proto);
  EXPECT_THAT(test_proto, EqualsProto(output_proto));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
