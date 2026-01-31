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

#include "ortools/sat/sat_base.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace sat {
namespace {

TEST(BooleanVariableTest, Api) {
  BooleanVariable var1(1);
  BooleanVariable var2(2);
  BooleanVariable var3(2);
  EXPECT_NE(var1, var2);
  EXPECT_EQ(var2, var3);
}

TEST(LiteralTest, Api) {
  BooleanVariable var1(1);
  BooleanVariable var2(2);
  Literal l1(var1, true);
  Literal l2(var2, false);
  Literal l3 = l2.Negated();
  EXPECT_EQ(l1.Variable(), var1);
  EXPECT_EQ(l2.Variable(), var2);
  EXPECT_EQ(l3.Variable(), var2);
  EXPECT_TRUE(l1.IsPositive());
  EXPECT_TRUE(l2.IsNegative());
  EXPECT_TRUE(l3.IsPositive());
}

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

TEST(SatClauseTest, BasicAllocation) {
  std::unique_ptr<SatClause> clause(SatClause::Create(Literals({+1, -2, +4})));
  EXPECT_EQ(3, clause->size());
  EXPECT_EQ(Literal(+1), clause->FirstLiteral());
  EXPECT_EQ(Literal(-2), clause->SecondLiteral());
}

TEST(SatClauseTest, ClassSize) { EXPECT_EQ(4, sizeof(SatClause)); }

TEST(ClausePtrTest, Accessors) {
  const Literal a(LiteralIndex(0x7654321A));
  const Literal b(LiteralIndex(0x7654321B));
  const Literal c(LiteralIndex(0x7654321C));
  SatClause* clause = SatClause::Create({a, b, c});
  const ClausePtr empty = ClausePtr::EmptyClausePtr();
  const ClausePtr unit = ClausePtr(a);
  const ClausePtr binary = ClausePtr(a, b);
  const ClausePtr sat_clause = ClausePtr(clause);
  const ClausePtr sat_empty = ClausePtr(std::vector<Literal>());
  const ClausePtr sat_unit = ClausePtr(std::vector<Literal>({a}));

  EXPECT_EQ(empty.GetType(), ClausePtr::kEmptyClause);
  EXPECT_EQ(unit.GetType(), ClausePtr::kUnitClause);
  EXPECT_EQ(binary.GetType(), ClausePtr::kBinaryClause);
  EXPECT_EQ(sat_clause.GetType(), ClausePtr::kSatClause);
  EXPECT_EQ(sat_empty.GetType(), ClausePtr::kSatClause);
  EXPECT_EQ(sat_unit.GetType(), ClausePtr::kSatClause);

  EXPECT_EQ(unit.GetFirstLiteral(), a);
  EXPECT_THAT(std::vector<Literal>(
                  {binary.GetFirstLiteral(), binary.GetSecondLiteral()}),
              testing::UnorderedElementsAre(a, b));

  EXPECT_THAT(empty.GetLiterals(), testing::IsEmpty());
  EXPECT_THAT(unit.GetLiterals(), testing::ElementsAre(a));
  EXPECT_THAT(binary.GetLiterals(), testing::UnorderedElementsAre(a, b));
  EXPECT_THAT(sat_clause.GetLiterals(), testing::ElementsAre(a, b, c));
  EXPECT_THAT(sat_empty.GetLiterals(), testing::IsEmpty());
  EXPECT_THAT(sat_unit.GetLiterals(), testing::ElementsAre(a));

  EXPECT_EQ(binary, ClausePtr(b, a));

  EXPECT_FALSE(empty.IsSatClausePtr());
  EXPECT_FALSE(unit.IsSatClausePtr());
  EXPECT_FALSE(binary.IsSatClausePtr());
  EXPECT_TRUE(sat_clause.IsSatClausePtr());
  EXPECT_TRUE(sat_empty.IsSatClausePtr());
  EXPECT_TRUE(sat_unit.IsSatClausePtr());

  EXPECT_EQ(sat_clause.GetSatClause(), clause);

  delete sat_clause.GetSatClause();
  delete sat_empty.GetSatClause();
  delete sat_unit.GetSatClause();
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
