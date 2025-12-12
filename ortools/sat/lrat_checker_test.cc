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

#include "ortools/sat/lrat_checker.h"

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {
namespace {

TEST(LratCheckerTest, LiteralEquivalentToItsNegationIsUnsat) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({-1, +2}));  // +1 => +2
  checker.AddProblemClause(ClauseId(2), Literals({-2, -1}));  // +2 => -1
  checker.AddProblemClause(ClauseId(3), Literals({+1, +2}));  // -1 => +2
  checker.AddProblemClause(ClauseId(4), Literals({-2, +1}));  // +2 => +1

  EXPECT_TRUE(checker.AddInferredClause(ClauseId(5), {Literals({-1})},
                                        {ClauseId(1), ClauseId(2)}));
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(6), {}, {ClauseId(5), ClauseId(3), ClauseId(4)}));
  EXPECT_TRUE(checker.Check());
}

TEST(LratCheckerTest, ValidationStopsAtFirstEmptyClause) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, -2}));  // -1 => -2
  checker.AddProblemClause(ClauseId(2), Literals({+2, +3}));  // -2 => +3
  checker.AddProblemClause(ClauseId(3), Literals({-3, +2}));  // -3 => +2
  checker.AddProblemClause(ClauseId(4), Literals({-2, +1}));  // +2 => +1
  checker.AddProblemClause(ClauseId(5), Literals({-1, +4}));  // +1 => +4

  // not(a) => ... => a proves that a is true. If the content in the middle is
  // of the same form, not(b) => ... => b, then not(a) => not(b) => ... => b if
  // sufficient to prove a.
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(6), {Literals({+1})}, {ClauseId(1), ClauseId(2), ClauseId(3)}));
  // Using unneeded clauses after that is still valid (here not(a) => not(b) =>
  // c => b => a), even if these clauses would not become unit or empty if they
  // were processed.
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(7), {Literals({+1})},
      {ClauseId(1), ClauseId(2), ClauseId(3), ClauseId(4)}));
}

TEST(LratCheckerTest, CheckSuccessWithoutRatClauses) {
  // Example from Fig. 1 of 'Efficient Certified RAT Verification'.
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -3}));
  checker.AddProblemClause(ClauseId(2), Literals({-1, -2, +3}));
  checker.AddProblemClause(ClauseId(3), Literals({+2, +3, -4}));
  checker.AddProblemClause(ClauseId(4), Literals({-2, -3, +4}));
  checker.AddProblemClause(ClauseId(5), Literals({-1, -3, -4}));
  checker.AddProblemClause(ClauseId(6), Literals({+1, +3, +4}));
  checker.AddProblemClause(ClauseId(7), Literals({-1, +2, +4}));
  checker.AddProblemClause(ClauseId(8), Literals({+1, -2, -4}));

  EXPECT_TRUE(
      checker.AddInferredClause(ClauseId(9), Literals({+1, +2}),
                                {ClauseId(1), ClauseId(6), ClauseId(3)}));
  checker.DeleteClauses({ClauseId(1)});
  EXPECT_TRUE(
      checker.AddInferredClause(ClauseId(10), Literals({+1, +3}),
                                {ClauseId(9), ClauseId(8), ClauseId(6)}));
  checker.DeleteClauses({ClauseId(6)});
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(11), Literals({+1}),
      {ClauseId(10), ClauseId(9), ClauseId(4), ClauseId(8)}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({ClauseId(10), ClauseId(9), ClauseId(8)});
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(12), Literals({+2}),
      {ClauseId(11), ClauseId(7), ClauseId(5), ClauseId(3)}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({ClauseId(7), ClauseId(3)});
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(13), {},
      {ClauseId(11), ClauseId(12), ClauseId(2), ClauseId(4), ClauseId(5)}));
  EXPECT_TRUE(checker.Check());
}

TEST(LratCheckerTest, CheckSuccessWithRatClause) {
  // Example from Fig. 2 of 'Efficient Certified RAT Verification'.
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -3}));
  checker.AddProblemClause(ClauseId(2), Literals({-1, -2, +3}));
  checker.AddProblemClause(ClauseId(3), Literals({+2, +3, -4}));
  checker.AddProblemClause(ClauseId(4), Literals({-2, -3, +4}));
  checker.AddProblemClause(ClauseId(5), Literals({-1, -3, -4}));
  checker.AddProblemClause(ClauseId(6), Literals({+1, +3, +4}));
  checker.AddProblemClause(ClauseId(7), Literals({-1, +2, +4}));
  checker.AddProblemClause(ClauseId(8), Literals({+1, -2, -4}));

  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(9), Literals({+1}), /*unit_ids=*/{},
      {LratChecker::RatIds{ClauseId(2), {ClauseId(6), ClauseId(8)}},
       LratChecker::RatIds{ClauseId(5), {ClauseId(1), ClauseId(8)}},
       LratChecker::RatIds{ClauseId(7), {ClauseId(6), ClauseId(1)}}}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({ClauseId(8), ClauseId(6), ClauseId(1)});
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(10), Literals({+2}),
      {ClauseId(9), ClauseId(7), ClauseId(5), ClauseId(3)}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({ClauseId(7), ClauseId(3)});
  EXPECT_TRUE(checker.AddInferredClause(
      ClauseId(11), {},
      {ClauseId(9), ClauseId(10), ClauseId(2), ClauseId(4), ClauseId(5)}));
  EXPECT_TRUE(checker.Check());
}

TEST(LratCheckerTest, CheckSuccessWithRatClausesExtensions) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -3}));
  checker.AddProblemClause(ClauseId(2), Literals({-1, -2, +3}));
  checker.AddProblemClause(ClauseId(3), Literals({+2, +3, -4}));
  checker.AddProblemClause(ClauseId(4), Literals({-2, -3, +4}));
  checker.AddProblemClause(ClauseId(5), Literals({-1, -3, -4}));
  checker.AddProblemClause(ClauseId(6), Literals({+1, +3, +4}));
  checker.AddProblemClause(ClauseId(7), Literals({-1, +2, +4}));
  checker.AddProblemClause(ClauseId(8), Literals({+1, -2, -4}));

  EXPECT_TRUE(checker.AddInferredClause(
      // Clause 1 becomes unit ({-3}).
      ClauseId(9), Literals({+1, +2}), {ClauseId(1)},
      // clause 9 ∪ {+3} = {+1, +2, +3} has two complementary literals with
      // clause 2 (+1 and +2). Hence the unit propagation proof is not needed.
      {LratChecker::RatIds{ClauseId(2), {}},
       // Likewise with clause 5 (+1 and +3).
       LratChecker::RatIds{ClauseId(5), {}},
       // If we assume that all the literals of clause 9 ∪ {+3} are false, as
       // well as all those of clause 7 except the negation of the pivot (i.e.,
       // {+2, +4}), then clause 6 is in conflict. But this would not be the
       // case if {+3}, propagated from clause 1 above, was ignored.
       LratChecker::RatIds{ClauseId(7), {ClauseId(6)}}}));
}

TEST(LratCheckerTest, ClauseIdAlreadyUsed) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  EXPECT_TRUE(checker.AddProblemClause(ClauseId(1), Literals({+1, +2})));
  EXPECT_FALSE(checker.AddProblemClause(ClauseId(1), Literals({+3, +4})));
  EXPECT_EQ(checker.error_message(), "In clause 1: clause ID 1 already used");
}

TEST(LratCheckerTest, ErrorStateIsSticky) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  EXPECT_TRUE(checker.AddProblemClause(ClauseId(1), Literals({+1, +2})));
  EXPECT_FALSE(checker.AddProblemClause(ClauseId(1), Literals({+3, +4})));
  // This clause is valid, but there was an error at a previous step.
  EXPECT_FALSE(checker.AddProblemClause(ClauseId(2), Literals({+5, +6})));
  EXPECT_EQ(checker.error_message(), "In clause 1: clause ID 1 already used");
}

TEST(LratCheckerTest, CompleteStateIsSticky) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  EXPECT_TRUE(checker.AddProblemClause(ClauseId(1), Literals({})));
  EXPECT_TRUE(checker.Check());
  // This clause is invalid (ID already used), but it is ignored since the proof
  // is already complete.
  EXPECT_TRUE(checker.AddProblemClause(ClauseId(1), Literals({+1, +2})));
  EXPECT_EQ(checker.error_message(), "");
}

TEST(LratCheckerTest, ClauseWithTwoComplementaryLiteralsIsIgnored) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  EXPECT_TRUE(checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -1})));
  // The clause ID is the same as the previous one, but this should be OK as the
  // previous clause should not be ignored.
  EXPECT_TRUE(checker.AddProblemClause(ClauseId(1), Literals({+3, +4})));
}

TEST(LratCheckerTest, UnknownUnitClauseId) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2}));

  EXPECT_FALSE(checker.AddInferredClause(ClauseId(2), Literals({+3, +4}),
                                         {ClauseId(2)}));
  EXPECT_EQ(checker.error_message(), "In clause 2: unit_id 2 not found");
}

TEST(LratCheckerTest, UnitClauseIdIsNotUnit) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, +3}));

  EXPECT_FALSE(
      checker.AddInferredClause(ClauseId(2), Literals({+1}), {ClauseId(1)}));
  EXPECT_THAT(checker.error_message(),
              ::testing::HasSubstr("In clause 2: unit_id 1 is not unit"));
}

TEST(LratCheckerTest, MissingPivotForRatProof) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, +3}));

  EXPECT_FALSE(checker.AddInferredClause(
      ClauseId(2), Literals({}), {}, {LratChecker::RatIds{ClauseId(1), {}}}));
  EXPECT_EQ(checker.error_message(),
            "In clause 2: missing pivot for RAT proof");
}

TEST(LratCheckerTest, MissingClauseInRatIds) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -3}));
  checker.AddProblemClause(ClauseId(2), Literals({-1, -2, +3}));

  // If the last clause in `unit_ids` is not a conflict, the `rat` proof must be
  // valid (here it is not, since clause 2 contains the negation of the pivot
  // but is not listed).
  EXPECT_FALSE(checker.AddInferredClause(ClauseId(3), Literals({+1, +2}),
                                         {ClauseId(1)}));
  EXPECT_EQ(checker.error_message(),
            "In clause 3: wrong number of resolvant IDs in RAT proof");
}

TEST(LratCheckerTest, DuplicateRatResolvantId) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -3}));
  checker.AddProblemClause(ClauseId(2), Literals({-1, -2, +3}));
  checker.AddProblemClause(ClauseId(3), Literals({+2, +3, -4}));
  checker.AddProblemClause(ClauseId(4), Literals({-2, -3, +4}));
  checker.AddProblemClause(ClauseId(5), Literals({-1, -3, -4}));
  checker.AddProblemClause(ClauseId(6), Literals({+1, +3, +4}));
  checker.AddProblemClause(ClauseId(7), Literals({-1, +2, +4}));
  checker.AddProblemClause(ClauseId(8), Literals({+1, -2, -4}));

  EXPECT_FALSE(checker.AddInferredClause(
      ClauseId(9), Literals({+1}), /*unit_ids=*/{},
      {LratChecker::RatIds{ClauseId(2), {ClauseId(6), ClauseId(8)}},
       LratChecker::RatIds{ClauseId(5), {ClauseId(1), ClauseId(8)}},
       LratChecker::RatIds{ClauseId(5), {ClauseId(1), ClauseId(8)}}}));
  EXPECT_EQ(checker.error_message(), "In clause 9: duplicate resolvant_id 5");
}

TEST(LratCheckerTest, UnknownRatResolvantId) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2}));
  checker.AddProblemClause(ClauseId(2), Literals({-3, +4}));

  EXPECT_FALSE(
      checker.AddInferredClause(ClauseId(3), Literals({+3, +4}), {},
                                {LratChecker::RatIds{ClauseId(3), {}}}));
  EXPECT_EQ(checker.error_message(), "In clause 3: resolvant_id 3 not found");
}

TEST(LratCheckerTest, MissingNegatedPivotInRatResolvant) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2}));
  checker.AddProblemClause(ClauseId(2), Literals({-3, +4}));

  EXPECT_FALSE(
      checker.AddInferredClause(ClauseId(3), Literals({+3, +4}), {},
                                {LratChecker::RatIds{ClauseId(1), {}}}));
  EXPECT_EQ(checker.error_message(),
            "In clause 3: missing negated pivot in resolvant_id 1");
}

TEST(LratCheckerTest, UnknownRatUnitId) {
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2}));
  checker.AddProblemClause(ClauseId(2), Literals({-3, +4}));

  EXPECT_FALSE(checker.AddInferredClause(
      ClauseId(3), Literals({+3, +4}), {},
      {LratChecker::RatIds{ClauseId(2), {ClauseId(3)}}}));
  EXPECT_EQ(checker.error_message(), "In clause 3: rat.unit_id 3 not found");
}

TEST(LratCheckerTest, RatUnitClauseIdIsNotUnit) {
  // Example from Fig. 2 of 'Efficient Certified RAT Verification'.
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -3}));
  checker.AddProblemClause(ClauseId(2), Literals({-1, -2, +3}));
  checker.AddProblemClause(ClauseId(3), Literals({+2, +3, -4}));
  checker.AddProblemClause(ClauseId(4), Literals({-2, -3, +4}));
  checker.AddProblemClause(ClauseId(5), Literals({-1, -3, -4}));
  checker.AddProblemClause(ClauseId(6), Literals({+1, +3, +4}));
  checker.AddProblemClause(ClauseId(7), Literals({-1, +2, +4}));
  checker.AddProblemClause(ClauseId(8), Literals({+1, -2, -4}));

  EXPECT_FALSE(checker.AddInferredClause(
      ClauseId(9), Literals({+1}), /*unit_ids=*/{},
      {LratChecker::RatIds{ClauseId(2), {ClauseId(7), ClauseId(8)}},
       LratChecker::RatIds{ClauseId(5), {ClauseId(1), ClauseId(8)}},
       LratChecker::RatIds{ClauseId(7), {ClauseId(6), ClauseId(1)}}}));
  EXPECT_EQ(checker.error_message(), "In clause 9: rat.unit_id 7 is not unit");
}

TEST(LratCheckerTest, LastRatUnitClauseIdIsNotAConflict) {
  // Example from Fig. 2 of 'Efficient Certified RAT Verification'.
  Model model;
  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(ClauseId(1), Literals({+1, +2, -3}));
  checker.AddProblemClause(ClauseId(2), Literals({-1, -2, +3}));
  checker.AddProblemClause(ClauseId(3), Literals({+2, +3, -4}));
  checker.AddProblemClause(ClauseId(4), Literals({-2, -3, +4}));
  checker.AddProblemClause(ClauseId(5), Literals({-1, -3, -4}));
  checker.AddProblemClause(ClauseId(6), Literals({+1, +3, +4}));
  checker.AddProblemClause(ClauseId(7), Literals({-1, +2, +4}));
  checker.AddProblemClause(ClauseId(8), Literals({+1, -2, -4}));

  EXPECT_FALSE(checker.AddInferredClause(
      ClauseId(9), Literals({+1}), /*unit_ids=*/{},
      // Clause 6 does not become empty after propagation (clause 8 missing).
      {LratChecker::RatIds{ClauseId(2), {ClauseId(6)}},
       LratChecker::RatIds{ClauseId(5), {ClauseId(1), ClauseId(8)}},
       LratChecker::RatIds{ClauseId(7), {ClauseId(6), ClauseId(1)}}}));
  EXPECT_EQ(
      checker.error_message(),
      "In clause 9: last unit_id for rat.resolvant_id 2 is not a conflict");
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
