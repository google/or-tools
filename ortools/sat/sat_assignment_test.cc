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

#include "ortools/sat/sat_assignment.h"

#include "gtest/gtest.h"
#include "ortools/sat/sat_literal.h"

namespace operations_research {
namespace sat {
namespace {

TEST(VariablesAssignmentTest, Api) {
  BooleanVariable var0(0);
  BooleanVariable var1(1);
  BooleanVariable var2(2);

  VariablesAssignment assignment;
  assignment.Resize(3);
  assignment.AssignFromTrueLiteral(Literal(var0, true));
  assignment.AssignFromTrueLiteral(Literal(var1, false));

  EXPECT_TRUE(assignment.LiteralIsTrue(Literal(var0, true)));
  EXPECT_TRUE(assignment.LiteralIsFalse(Literal(var0, false)));
  EXPECT_TRUE(assignment.LiteralIsTrue(Literal(var1, false)));
  EXPECT_FALSE(assignment.VariableIsAssigned(var2));

  assignment.UnassignLiteral(Literal(var0, true));
  EXPECT_FALSE(assignment.VariableIsAssigned(var0));

  assignment.AssignFromTrueLiteral(Literal(var2, false));
  EXPECT_TRUE(assignment.LiteralIsTrue(Literal(var2, false)));
  EXPECT_FALSE(assignment.LiteralIsTrue(Literal(var2, true)));
  EXPECT_TRUE(assignment.LiteralIsFalse(Literal(var2, true)));
  EXPECT_FALSE(assignment.LiteralIsFalse(Literal(var2, false)));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
