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

#include "ortools/sat/pb_constraint.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ContainerEq;

template <typename... Args>
auto LiteralsAre(Args... literals) {
  return ::testing::ElementsAre(Literal(literals)...);
}

std::vector<LiteralWithCoeff> MakePb(
    absl::Span<const std::pair<int, Coefficient>> input) {
  std::vector<LiteralWithCoeff> result;
  result.reserve(input.size());
  for (const auto p : input) {
    result.push_back({Literal(p.first), p.second});
  }
  return result;
}

TEST(ComputeBooleanLinearExpressionCanonicalForm, RemoveZeroCoefficient) {
  Coefficient bound_shift, max_value;
  auto cst = MakePb({{+1, 4}, {+2, 0}, {+3, 4}, {+5, 0}});
  const auto result = MakePb({{+1, 4}, {+3, 4}});
  EXPECT_TRUE(ComputeBooleanLinearExpressionCanonicalForm(&cst, &bound_shift,
                                                          &max_value));
  EXPECT_THAT(cst, ContainerEq(result));
  EXPECT_EQ(bound_shift, 0);
  EXPECT_EQ(max_value, 8);
}

TEST(ComputeBooleanLinearExpressionCanonicalForm, MakeAllCoefficientPositive) {
  Coefficient bound_shift, max_value;
  auto cst = MakePb({{+1, 4}, {+2, -3}, {+3, 4}, {+5, -1}});
  const auto result = MakePb({{-5, 1}, {-2, 3}, {+1, 4}, {+3, 4}});
  EXPECT_TRUE(ComputeBooleanLinearExpressionCanonicalForm(&cst, &bound_shift,
                                                          &max_value));
  EXPECT_THAT(cst, ContainerEq(result));
  EXPECT_EQ(bound_shift, 4);
  EXPECT_EQ(max_value, 12);
}

TEST(ComputeBooleanLinearExpressionCanonicalForm, MergeSameVariableCase1) {
  Coefficient bound_shift, max_value;
  // 4x -3(1-x) +4(1-x) -x is the same as to 2x + 1
  auto cst = MakePb({{+1, 4}, {-1, -3}, {-1, 4}, {+1, -1}});
  const auto result = MakePb({{+1, 2}});
  EXPECT_TRUE(ComputeBooleanLinearExpressionCanonicalForm(&cst, &bound_shift,
                                                          &max_value));
  EXPECT_THAT(cst, ContainerEq(result));
  EXPECT_EQ(bound_shift, -1);
  EXPECT_EQ(max_value, 2);
}

TEST(ComputeBooleanLinearExpressionCanonicalForm, MergeSameVariableCase2) {
  Coefficient bound_shift, max_value;
  // 4x -3(1-x) +4(1-x) -5x is the same as to -2x + 1
  // which is expressed as 2(1-x) -2 +1
  auto cst = MakePb({{+1, 4}, {-1, -3}, {-1, 4}, {+1, -5}});
  const auto result = MakePb({{-1, 2}});
  EXPECT_TRUE(ComputeBooleanLinearExpressionCanonicalForm(&cst, &bound_shift,
                                                          &max_value));
  EXPECT_THAT(cst, ContainerEq(result));
  EXPECT_EQ(bound_shift, 1);
  EXPECT_EQ(max_value, 2);
}

TEST(ComputeBooleanLinearExpressionCanonicalForm, MergeSameVariableCase3) {
  Coefficient bound_shift, max_value;
  // Here the last variable will disappear completely
  auto cst = MakePb({{+1, 4}, {+2, -3}, {+2, 4}, {+2, -1}});
  const auto result = MakePb({{+1, 4}});
  EXPECT_TRUE(ComputeBooleanLinearExpressionCanonicalForm(&cst, &bound_shift,
                                                          &max_value));
  EXPECT_THAT(cst, ContainerEq(result));
  EXPECT_EQ(bound_shift, 0);
  EXPECT_EQ(max_value, 4);
}

TEST(ComputeBooleanLinearExpressionCanonicalForm, Overflow) {
  Coefficient bound_shift, max_value;
  auto cst = MakePb({{+1, -kCoefficientMax}, {+2, -kCoefficientMax}});
  EXPECT_FALSE(ComputeBooleanLinearExpressionCanonicalForm(&cst, &bound_shift,
                                                           &max_value));
}

TEST(ComputeBooleanLinearExpressionCanonicalForm, BigIntCase) {
  Coefficient bound_shift, max_value;
  auto cst = MakePb({{+1, -kCoefficientMax}, {-1, -kCoefficientMax}});
  const auto result = MakePb({});
  EXPECT_TRUE(ComputeBooleanLinearExpressionCanonicalForm(&cst, &bound_shift,
                                                          &max_value));
  EXPECT_THAT(cst, ContainerEq(result));
  EXPECT_EQ(bound_shift, kCoefficientMax);
  EXPECT_EQ(max_value, 0);
}

TEST(ApplyLiteralMappingTest, BasicTest) {
  Coefficient bound_shift, max_value;

  // This is needed to initizalize the ITIVector below.
  std::vector<LiteralIndex> temp{
      kTrueLiteralIndex,   kFalseLiteralIndex,    // var1 fixed to true.
      Literal(-1).Index(), Literal(+1).Index(),   // var2 mapped to not(var1)
      Literal(+2).Index(), Literal(-2).Index(),   // var3 mapped to var2
      kFalseLiteralIndex,  kTrueLiteralIndex,     // var4 fixed to false
      Literal(+2).Index(), Literal(-2).Index()};  // var5 mapped to var2
  util_intops::StrongVector<LiteralIndex, LiteralIndex> mapping(temp.begin(),
                                                                temp.end());

  auto cst = MakePb({{+1, 4}, {+3, -3}, {+2, 4}, {+4, 7}, {+5, 5}});
  EXPECT_TRUE(ApplyLiteralMapping(mapping, &cst, &bound_shift, &max_value));
  const auto result = MakePb({{+2, 2}, {-1, 4}});
  EXPECT_THAT(cst, ContainerEq(result));
  EXPECT_EQ(bound_shift, -4);
  EXPECT_EQ(max_value, 6);
}

TEST(SimplifyCanonicalBooleanLinearConstraint, CoefficientsLargerThanRhs) {
  auto cst = MakePb({{+1, 4}, {+2, 5}, {+3, 6}, {-4, 7}});
  Coefficient rhs(10);
  SimplifyCanonicalBooleanLinearConstraint(&cst, &rhs);
  EXPECT_THAT(cst, ContainerEq(cst));
  rhs = Coefficient(5);
  SimplifyCanonicalBooleanLinearConstraint(&cst, &rhs);
  const auto result = MakePb({{+1, 4}, {+2, 5}, {+3, 6}, {-4, 6}});
  EXPECT_THAT(cst, ContainerEq(result));
}

TEST(CanonicalBooleanLinearProblem, BasicTest) {
  auto cst = MakePb({{+1, 4}, {+2, -5}, {+3, 6}, {-4, 7}});
  CanonicalBooleanLinearProblem problem;
  problem.AddLinearConstraint(true, Coefficient(-5), true, Coefficient(5),
                              &cst);

  // We have just one constraint because the >= -5 is always true.
  EXPECT_EQ(1, problem.NumConstraints());
  const auto result0 = MakePb({{+1, 4}, {-2, 5}, {+3, 6}, {-4, 7}});
  EXPECT_EQ(problem.Rhs(0), 10);
  EXPECT_THAT(problem.Constraint(0), ContainerEq(result0));

  // So lets restrict it and only use the lower bound
  // Note that the API destroy the input so we have to reconstruct it.
  cst = MakePb({{+1, 4}, {+2, -5}, {+3, 6}, {-4, 7}});
  problem.AddLinearConstraint(true, Coefficient(-4), false,
                              /*unused*/ Coefficient(-10), &cst);

  // Now we have another constraint corresponding to the >= -4 constraint.
  EXPECT_EQ(2, problem.NumConstraints());
  const auto result1 = MakePb({{-1, 4}, {+2, 5}, {-3, 6}, {+4, 7}});
  EXPECT_EQ(problem.Rhs(1), 21);
  EXPECT_THAT(problem.Constraint(1), ContainerEq(result1));
}

TEST(CanonicalBooleanLinearProblem, BasicTest2) {
  auto cst = MakePb({{+1, 1}, {+2, 2}});
  CanonicalBooleanLinearProblem problem;
  problem.AddLinearConstraint(true, Coefficient(2), false,
                              /*unused*/ Coefficient(0), &cst);

  EXPECT_EQ(1, problem.NumConstraints());
  const auto result = MakePb({{-1, 1}, {-2, 2}});
  EXPECT_EQ(problem.Rhs(0), 1);
  EXPECT_THAT(problem.Constraint(0), ContainerEq(result));
}

TEST(CanonicalBooleanLinearProblem, OverflowCases) {
  auto cst = MakePb({});
  CanonicalBooleanLinearProblem problem;
  for (int i = 0; i < 2; ++i) {
    std::vector<LiteralWithCoeff> reference;
    if (i == 0) {
      // This is a constraint with a "bound shift" of 10.
      reference = MakePb({{+1, -10}, {+2, 10}});
    } else {
      // This is a constraint with a "bound shift" of -10 since its domain value
      // is actually [10, 10].
      reference = MakePb({{+1, 10}, {-1, 10}});
    }

    // All These constraint are trivially satisfiables, so no new constraints
    // should be added.
    cst = reference;
    EXPECT_TRUE(problem.AddLinearConstraint(true, -kCoefficientMax, true,
                                            kCoefficientMax, &cst));
    cst = reference;
    EXPECT_TRUE(problem.AddLinearConstraint(true, -kCoefficientMax - 1, true,
                                            kCoefficientMax, &cst));
    cst = reference;
    EXPECT_TRUE(problem.AddLinearConstraint(true, Coefficient(-10), true,
                                            Coefficient(10), &cst));

    // These are trivially unsat, and all AddLinearConstraint() should return
    // false.
    cst = reference;
    EXPECT_FALSE(problem.AddLinearConstraint(true, kCoefficientMax, true,
                                             kCoefficientMax, &cst));
    cst = reference;
    EXPECT_FALSE(problem.AddLinearConstraint(true, -kCoefficientMax, true,
                                             -kCoefficientMax, &cst));
    cst = reference;
    EXPECT_FALSE(problem.AddLinearConstraint(
        true, -kCoefficientMax, true, -kCoefficientMax - Coefficient(1), &cst));
  }

  // No constraints were actually added.
  EXPECT_EQ(problem.NumConstraints(), 0);
}

// Constructs a vector from the current trail, so we can use LiteralsAre().
std::vector<Literal> TrailToVector(const Trail& trail) {
  std::vector<Literal> output;
  for (int i = 0; i < trail.Index(); ++i) output.push_back(trail[i]);
  return output;
}

TEST(UpperBoundedLinearConstraintTest, ConstructionAndBasicPropagation) {
  Coefficient threshold;
  PbConstraintsEnqueueHelper helper;
  helper.reasons.resize(10);
  Trail trail;
  trail.Resize(10);

  UpperBoundedLinearConstraint cst(
      MakePb({{+1, 4}, {+2, 4}, {-3, 5}, {+4, 10}}));
  cst.InitializeRhs(Coefficient(7), 0, &threshold, &trail, &helper);
  EXPECT_EQ(threshold, 2);
  EXPECT_THAT(TrailToVector(trail), LiteralsAre(-4));

  trail.Enqueue(Literal(-3), AssignmentType::kSearchDecision);
  threshold -= 5;  // The coeff of -3 in cst.
  EXPECT_TRUE(cst.Propagate(trail.Info(Literal(-3).Variable()).trail_index,
                            &threshold, &trail, &helper));
  EXPECT_EQ(threshold, 2);
  EXPECT_THAT(TrailToVector(trail), LiteralsAre(-4, -3, -1, -2));

  // Untrail.
  trail.Untrail(0);
  threshold += 5;
  cst.Untrail(&threshold, 0);
  EXPECT_EQ(threshold, 2);
}

TEST(UpperBoundedLinearConstraintTest, Conflict) {
  Coefficient threshold;
  Trail trail;
  trail.Resize(10);
  PbConstraintsEnqueueHelper helper;
  helper.reasons.resize(10);

  // At most one constraint.
  UpperBoundedLinearConstraint cst(
      MakePb({{+1, 1}, {+2, 1}, {+3, 1}, {+4, 1}}));
  cst.InitializeRhs(Coefficient(1), 0, &threshold, &trail, &helper);
  EXPECT_EQ(threshold, 0);

  // Two assignment from other part of the solver.
  trail.SetDecisionLevel(1);
  trail.Enqueue(Literal(+1), AssignmentType::kSearchDecision);
  trail.SetDecisionLevel(2);
  trail.Enqueue(Literal(+2), AssignmentType::kSearchDecision);

  // We propagate only +1.
  threshold -= 1;
  EXPECT_FALSE(cst.Propagate(trail.Info(Literal(+1).Variable()).trail_index,
                             &threshold, &trail, &helper));
  EXPECT_THAT(helper.conflict, LiteralsAre(-1, -2));
}

TEST(UpperBoundedLinearConstraintTest, CompactReason) {
  Coefficient threshold;
  Trail trail;
  trail.Resize(10);
  PbConstraintsEnqueueHelper helper;
  helper.reasons.resize(10);

  // At most one constraint.
  UpperBoundedLinearConstraint cst(
      MakePb({{+1, 1}, {+2, 2}, {+3, 3}, {+4, 4}}));
  cst.InitializeRhs(Coefficient(7), 0, &threshold, &trail, &helper);
  EXPECT_EQ(threshold, 3);

  // Two assignment from other part of the solver.
  trail.SetDecisionLevel(1);
  trail.Enqueue(Literal(+1), AssignmentType::kSearchDecision);
  trail.SetDecisionLevel(2);
  trail.Enqueue(Literal(+2), AssignmentType::kSearchDecision);
  trail.SetDecisionLevel(3);
  trail.Enqueue(Literal(+3), AssignmentType::kSearchDecision);

  // We propagate when +3 is processed.
  threshold = -3;
  const int source_trail_index = trail.Info(Literal(+3).Variable()).trail_index;
  EXPECT_TRUE(cst.Propagate(source_trail_index, &threshold, &trail, &helper));
  EXPECT_EQ(trail.Index(), 4);
  EXPECT_EQ(trail[3], Literal(-4));

  // -1 do not need to be in the reason since {-3, -2} propagates exactly
  // the same way.
  cst.FillReason(trail, source_trail_index, Literal(-4).Variable(),
                 &helper.conflict);
  EXPECT_THAT(helper.conflict, LiteralsAre(-3, -2));
}

TEST(PbConstraintsTest, Duplicates) {
  Model model;
  PbConstraints& csts = *(model.GetOrCreate<PbConstraints>());
  Trail& trail = *(model.GetOrCreate<Trail>());

  trail.Resize(10);
  csts.Resize(10);

  CHECK_EQ(csts.NumberOfConstraints(), 0);
  csts.AddConstraint(MakePb({{-1, 7}, {-2, 7}, {+3, 7}}), Coefficient(20),
                     &trail);
  csts.AddConstraint(MakePb({{-1, 1}, {-2, 3}, {+3, 7}}), Coefficient(20),
                     &trail);
  CHECK_EQ(csts.NumberOfConstraints(), 2);

  // Adding the same constraints will do nothing.
  csts.AddConstraint(MakePb({{-1, 7}, {-2, 7}, {+3, 7}}), Coefficient(20),
                     &trail);
  CHECK_EQ(csts.NumberOfConstraints(), 2);
  CHECK_EQ(trail.Index(), 0);

  // Over constraining it will fix the 3 literals.
  csts.AddConstraint(MakePb({{-1, 7}, {-2, 7}, {+3, 7}}), Coefficient(6),
                     &trail);
  CHECK_EQ(csts.NumberOfConstraints(), 2);
  EXPECT_THAT(TrailToVector(trail), LiteralsAre(+1, +2, -3));
}

TEST(PbConstraintsTest, BasicPropagation) {
  Model model;
  PbConstraints& csts = *(model.GetOrCreate<PbConstraints>());
  Trail& trail = *(model.GetOrCreate<Trail>());

  trail.Resize(10);
  trail.SetDecisionLevel(1);
  trail.Enqueue(Literal(-1), AssignmentType::kSearchDecision);

  csts.Resize(10);
  csts.AddConstraint(MakePb({{-1, 1}, {+2, 1}}), Coefficient(1), &trail);
  csts.AddConstraint(MakePb({{-1, 7}, {-2, 7}, {+3, 7}}), Coefficient(20),
                     &trail);
  csts.AddConstraint(MakePb({{-1, 1}, {-2, 1}, {-3, 1}, {+4, 1}}),
                     Coefficient(3), &trail);

  EXPECT_THAT(TrailToVector(trail), LiteralsAre(-1, -2));
  while (!csts.PropagationIsDone(trail)) EXPECT_TRUE(csts.Propagate(&trail));
  EXPECT_THAT(TrailToVector(trail), LiteralsAre(-1, -2, -3, -4));

  // Test the reason for each assignment.
  EXPECT_THAT(trail.Reason(Literal(-2).Variable()), LiteralsAre(+1));
  EXPECT_THAT(trail.Reason(Literal(-3).Variable()), LiteralsAre(+2, +1));
  EXPECT_THAT(trail.Reason(Literal(-4).Variable()), LiteralsAre(+3, +2, +1));

  // Untrail, and repropagate everything.
  csts.Untrail(trail, 0);
  trail.Untrail(0);
  trail.Enqueue(Literal(-1), AssignmentType::kSearchDecision);
  while (!csts.PropagationIsDone(trail)) EXPECT_TRUE(csts.Propagate(&trail));
  EXPECT_THAT(TrailToVector(trail), LiteralsAre(-1, -2, -3, -4));
}

TEST(PbConstraintsTest, BasicDeletion) {
  Model model;
  PbConstraints& csts = *(model.GetOrCreate<PbConstraints>());
  Trail& trail = *(model.GetOrCreate<Trail>());

  PbConstraintsEnqueueHelper helper;
  helper.reasons.resize(10);
  trail.Resize(10);
  trail.SetDecisionLevel(0);
  csts.Resize(10);
  csts.AddConstraint(MakePb({{-1, 1}, {+2, 1}}), Coefficient(1), &trail);
  csts.AddConstraint(MakePb({{-1, 7}, {-2, 7}, {+3, 7}}), Coefficient(20),
                     &trail);
  csts.AddConstraint(MakePb({{-1, 1}, {-2, 1}, {-3, 1}, {+4, 1}}),
                     Coefficient(3), &trail);

  // Delete the first constraint.
  EXPECT_EQ(3, csts.NumberOfConstraints());
  csts.DeleteConstraint(0);
  EXPECT_EQ(2, csts.NumberOfConstraints());

  // The constraint 1 is deleted, so enqueuing -1 shouldn't propagate.
  trail.Enqueue(Literal(-1), AssignmentType::kSearchDecision);
  while (!csts.PropagationIsDone(trail)) EXPECT_TRUE(csts.Propagate(&trail));
  EXPECT_EQ("-1", trail.DebugString());

  // But also enqueing -2 should.
  trail.Enqueue(Literal(-2), AssignmentType::kSearchDecision);
  while (!csts.PropagationIsDone(trail)) EXPECT_TRUE(csts.Propagate(&trail));
  EXPECT_EQ("-1 -2 -3 -4", trail.DebugString());

  // Let's bactrack.
  trail.Untrail(1);
  csts.Untrail(trail, 1);

  // Let's delete one more constraint.
  csts.DeleteConstraint(0);
  EXPECT_EQ(1, csts.NumberOfConstraints());

  // Now, if we enqueue -2 again, nothing is propagated.
  trail.Enqueue(Literal(-2), AssignmentType::kSearchDecision);
  while (!csts.PropagationIsDone(trail)) EXPECT_TRUE(csts.Propagate(&trail));
  EXPECT_EQ("-1 -2", trail.DebugString());

  // We need to also enqueue -3 for -4 to be propagated.
  trail.Enqueue(Literal(-3), AssignmentType::kSearchDecision);
  while (!csts.PropagationIsDone(trail)) EXPECT_TRUE(csts.Propagate(&trail));
  EXPECT_EQ("-1 -2 -3 -4", trail.DebugString());

  // Deleting everything doesn't crash.
  csts.DeleteConstraint(0);
  EXPECT_EQ(0, csts.NumberOfConstraints());
}

TEST(PbConstraintsTest, UnsatAtConstruction) {
  Model model;
  PbConstraints& csts = *(model.GetOrCreate<PbConstraints>());
  Trail& trail = *(model.GetOrCreate<Trail>());

  trail.Resize(10);
  trail.SetDecisionLevel(1);
  trail.Enqueue(Literal(+1), AssignmentType::kUnitReason);
  trail.Enqueue(Literal(+2), AssignmentType::kUnitReason);
  trail.Enqueue(Literal(+3), AssignmentType::kUnitReason);

  csts.Resize(10);

  EXPECT_TRUE(
      csts.AddConstraint(MakePb({{+1, 1}, {+2, 1}}), Coefficient(2), &trail));
  while (!csts.PropagationIsDone(trail)) EXPECT_TRUE(csts.Propagate(&trail));

  // We need to propagate before adding this constraint for the AddConstraint()
  // to notice that it is unsat. Otherwise, it will be noticed at propagation
  // time.
  EXPECT_FALSE(csts.AddConstraint(MakePb({{+1, 1}, {+2, 1}, {+3, 1}}),
                                  Coefficient(2), &trail));
  EXPECT_TRUE(csts.AddConstraint(MakePb({{+1, 1}, {+2, 1}, {+4, 1}}),
                                 Coefficient(2), &trail));
}

TEST(PbConstraintsTest, AddConstraintWithLevel0Propagation) {
  Model model;
  PbConstraints& csts = *(model.GetOrCreate<PbConstraints>());
  Trail& trail = *(model.GetOrCreate<Trail>());

  trail.Resize(10);
  trail.SetDecisionLevel(0);
  csts.Resize(10);

  EXPECT_TRUE(csts.AddConstraint(MakePb({{+1, 1}, {+2, 3}, {+3, 7}}),
                                 Coefficient(2), &trail));
  EXPECT_EQ(trail.Index(), 2);
  EXPECT_EQ(trail[0], Literal(-2));
  EXPECT_EQ(trail[1], Literal(-3));
}

TEST(PbConstraintsTest, AddConstraintUMR) {
  const auto cst = MakePb({{+3, 7}});
  UpperBoundedLinearConstraint c(cst);
  // Calling hashing on c generates an UMR that is triggered during the hash_map
  // lookup below.
  const uint64_t ct_hash = c.hash();
  absl::flat_hash_map<uint64_t, std::vector<int>> store;
  std::vector<int>& vec = store[ct_hash];
  EXPECT_EQ(vec.size(), 0);
}

TEST(PbConstraintsDeathTest, AddConstraintWithLevel0PropagationInSearch) {
  Model model;
  PbConstraints& csts = *(model.GetOrCreate<PbConstraints>());
  Trail& trail = *(model.GetOrCreate<Trail>());

  trail.Resize(10);
  trail.SetDecisionLevel(10);
  csts.Resize(10);

  // If the decision level is not 0, this will fail.
  ASSERT_DEATH(csts.AddConstraint(MakePb({{+1, 1}, {+2, 3}, {+3, 7}}),
                                  Coefficient(2), &trail),
               "var should have been propagated at an earlier level.");
}

TEST(PbConstraintsDeathTest, AddConstraintPrecondition) {
  Model model;
  PbConstraints& csts = *(model.GetOrCreate<PbConstraints>());
  Trail& trail = *(model.GetOrCreate<Trail>());

  trail.Resize(10);
  trail.SetDecisionLevel(1);
  trail.Enqueue(Literal(+1), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(+2), AssignmentType::kUnitReason);
  trail.SetDecisionLevel(2);
  trail.Enqueue(Literal(+3), AssignmentType::kSearchDecision);
  csts.Resize(10);

  // We can't add this constraint since it is conflicting under the current
  // assignment.
  EXPECT_FALSE(csts.AddConstraint(MakePb({{+1, 1}, {+2, 1}, {+3, 1}}),
                                  Coefficient(2), &trail));

  trail.Untrail(trail.Index() - 1);  //  Remove the +3.
  EXPECT_EQ(trail.Index(), 2);
  csts.Untrail(trail, 2);

  // Adding this one at a decision level of 2 will also fail because it will
  // propagate 3 from decision level 1.
  ASSERT_DEATH(csts.AddConstraint(MakePb({{+1, 1}, {+2, 1}, {+3, 2}}),
                                  Coefficient(3), &trail),
               "var should have been propagated at an earlier level.");

  // However, adding the same constraint while the decision level is 1 is ok.
  // It will propagate -3 at the correct decision level.
  trail.SetDecisionLevel(1);
  EXPECT_TRUE(csts.AddConstraint(MakePb({{+1, 1}, {+2, 1}, {+3, 2}}),
                                 Coefficient(3), &trail));
  EXPECT_EQ(trail.Index(), 3);
  EXPECT_EQ(trail[2], Literal(-3));
}

TEST(MutableUpperBoundedLinearConstraintTest, LinearAddition) {
  MutableUpperBoundedLinearConstraint cst_a;
  cst_a.ClearAndResize(5);
  cst_a.AddTerm(Literal(+1), Coefficient(3));
  cst_a.AddTerm(Literal(+2), Coefficient(4));
  cst_a.AddTerm(Literal(+3), Coefficient(5));
  cst_a.AddTerm(Literal(+4), Coefficient(1));
  cst_a.AddTerm(Literal(+5), Coefficient(1));
  cst_a.AddToRhs(Coefficient(10));

  // The result of cst_a + cst_b is describes in the comments.
  MutableUpperBoundedLinearConstraint cst_b;
  cst_b.ClearAndResize(5);
  cst_b.AddTerm(Literal(+1), Coefficient(3));  // 3x + 3x = 6x
  cst_b.AddTerm(Literal(-2), Coefficient(3));  // 4x + 3(1-x) = x + 3
  cst_b.AddTerm(Literal(+3), Coefficient(3));  // 5x + 3x = 8x
  cst_b.AddTerm(Literal(-4), Coefficient(6));  // x + 6(1-x) = 5(1-x) + 1
  cst_b.AddTerm(Literal(+5), Coefficient(5));  // x + 5x = 6x
  cst_b.AddToRhs(Coefficient(10));

  for (BooleanVariable var : cst_b.PossibleNonZeros()) {
    cst_a.AddTerm(cst_b.GetLiteral(var), cst_b.GetCoefficient(var));
  }
  cst_a.AddToRhs(cst_b.Rhs());

  EXPECT_EQ(cst_a.DebugString(), "6[+1] + 1[+2] + 8[+3] + 5[-4] + 6[+5] <= 16");
}

TEST(MutableUpperBoundedLinearConstraintTest, ReduceCoefficients) {
  MutableUpperBoundedLinearConstraint cst;
  cst.ClearAndResize(100);
  Coefficient max_value(0);
  for (int i = 1; i <= 10; ++i) {
    max_value += Coefficient(i);
    cst.AddTerm(Literal(BooleanVariable(i), true), Coefficient(i));
  }
  cst.AddToRhs(max_value - 3);

  // The constraint is equivalent to sum i * Literal(i, false) >= 3,
  // So we can reduce any coeff > 3 to 3 and change the rhs accordingly.
  cst.ReduceCoefficients();
  for (BooleanVariable var : cst.PossibleNonZeros()) {
    EXPECT_LE(cst.GetCoefficient(var), 3);
  }
  EXPECT_EQ(cst.Rhs(), 1 + 2 + 3 * 8 - 3);
}

TEST(MutableUpperBoundedLinearConstraintTest, ComputeSlackForTrailPrefix) {
  MutableUpperBoundedLinearConstraint cst;
  cst.ClearAndResize(100);
  cst.AddTerm(Literal(+1), Coefficient(3));
  cst.AddTerm(Literal(+2), Coefficient(4));
  cst.AddTerm(Literal(+3), Coefficient(5));
  cst.AddTerm(Literal(+4), Coefficient(6));
  cst.AddTerm(Literal(+5), Coefficient(7));
  cst.AddToRhs(Coefficient(10));

  Trail trail;
  trail.Resize(10);
  trail.Enqueue(Literal(+1), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(-2), AssignmentType::kUnitReason);
  trail.Enqueue(Literal(+3), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(-5), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(+4), AssignmentType::kSearchDecision);

  EXPECT_EQ(Coefficient(10), cst.ComputeSlackForTrailPrefix(trail, 0));
  EXPECT_EQ(Coefficient(10 - 3), cst.ComputeSlackForTrailPrefix(trail, 1));
  EXPECT_EQ(Coefficient(10 - 3), cst.ComputeSlackForTrailPrefix(trail, 2));
  EXPECT_EQ(Coefficient(10 - 3 - 5), cst.ComputeSlackForTrailPrefix(trail, 3));
  EXPECT_EQ(Coefficient(10 - 3 - 5), cst.ComputeSlackForTrailPrefix(trail, 4));
  EXPECT_EQ(Coefficient(10 - 14), cst.ComputeSlackForTrailPrefix(trail, 5));
  EXPECT_EQ(Coefficient(10 - 14), cst.ComputeSlackForTrailPrefix(trail, 50));
}

TEST(MutableUpperBoundedLinearConstraintTest, ReduceSlackToZero) {
  MutableUpperBoundedLinearConstraint cst;
  cst.ClearAndResize(100);
  cst.AddTerm(Literal(+1), Coefficient(3));
  cst.AddTerm(Literal(+2), Coefficient(1));
  cst.AddTerm(Literal(+3), Coefficient(5));
  cst.AddTerm(Literal(+4), Coefficient(6));
  cst.AddTerm(Literal(+5), Coefficient(7));
  cst.AddToRhs(Coefficient(10));

  Trail trail;
  trail.Resize(10);
  trail.Enqueue(Literal(+1), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(-2), AssignmentType::kUnitReason);
  trail.Enqueue(Literal(+3), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(+5), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(+4), AssignmentType::kSearchDecision);

  // +1, -2 and +3 gives a slack of 2.
  EXPECT_EQ(Coefficient(2), cst.ComputeSlackForTrailPrefix(trail, 3));

  // It also propagate -4 and -5, to have the same propagation but with a slack
  // of zero, we can call ReduceSlackToZero().
  cst.ReduceSlackTo(trail, 3, Coefficient(2), Coefficient(0));

  // +1 and +3 have the same coeff.
  EXPECT_EQ(cst.GetCoefficient(BooleanVariable(0)), Coefficient(3));
  EXPECT_EQ(cst.GetCoefficient(BooleanVariable(2)), Coefficient(5));

  // the variable 1 disappeared.
  EXPECT_EQ(cst.GetCoefficient(BooleanVariable(1)), Coefficient(0));

  // The propagated variable coeff has been reduced by the slack.
  EXPECT_EQ(cst.GetCoefficient(BooleanVariable(3)), Coefficient(6 - 2));
  EXPECT_EQ(cst.GetCoefficient(BooleanVariable(4)), Coefficient(7 - 2));

  // The rhs has been reduced by slack, and the slack is now 0.
  EXPECT_EQ(cst.Rhs(), Coefficient(10 - 2));
  EXPECT_EQ(Coefficient(0), cst.ComputeSlackForTrailPrefix(trail, 3));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
