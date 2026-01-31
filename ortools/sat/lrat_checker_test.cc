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

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {
namespace {

class ClauseFactory {
 public:
  ClausePtr NewClause(absl::Span<const int> literals) {
    const ClausePtr clause = ClausePtr(Literals(literals));
    sat_clauses_.push_back(absl::WrapUnique(clause.GetSatClause()));
    return clause;
  }

 private:
  std::vector<std::unique_ptr<SatClause>> sat_clauses_;
};

TEST(LratCheckerTest, LiteralEquivalentToItsNegationIsUnsat) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({-1, +2});  // +1 => +2
  const ClausePtr c2 = factory.NewClause({-2, -1});  // +2 => -1
  const ClausePtr c3 = factory.NewClause({+1, +2});  // -1 => +2
  const ClausePtr c4 = factory.NewClause({-2, +1});  // +2 => +1
  const ClausePtr c5 = factory.NewClause({-1});
  const ClausePtr c6 = factory.NewClause({});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);

  EXPECT_TRUE(checker.AddInferredClause(c5, {c1, c2}));
  EXPECT_TRUE(checker.AddInferredClause(c6, {c5, c3, c4}));
  EXPECT_TRUE(checker.Check());
}

TEST(LratCheckerTest, RewriteClauseOk) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, +3});
  const ClausePtr c2 = factory.NewClause({-2, +4});  // +2 => +4
  const ClausePtr c3 = factory.NewClause({-3, +5});  // +3 => +5

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);

  EXPECT_TRUE(checker.RewriteClause(c1, Literals({+1, +4, +5}), {c2, c3, c1}));
  EXPECT_EQ(checker.error_message(), "");
}

TEST(LratCheckerTest, ValidationStopsAtFirstEmptyClause) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, -2});  // -1 => -2
  const ClausePtr c2 = factory.NewClause({+2, +3});  // -2 => +3
  const ClausePtr c3 = factory.NewClause({-3, +2});  // -3 => +2
  const ClausePtr c4 = factory.NewClause({-2, +1});  // +2 => +1
  const ClausePtr c5 = factory.NewClause({-1, +4});  // +1 => +4
  const ClausePtr c6 = factory.NewClause({+1});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);
  checker.AddProblemClause(c5);

  // not(a) => ... => a proves that a is true. If the content in the middle is
  // of the same form, not(b) => ... => b, then not(a) => not(b) => ... => b if
  // sufficient to prove a.
  EXPECT_TRUE(checker.AddInferredClause(c6, {c1, c2, c3}));
  // Using unneeded clauses after that is still valid (here not(a) => not(b) =>
  // c => b => a), even if these clauses would not become unit or empty if they
  // were processed.
  EXPECT_TRUE(checker.AddInferredClause(c6, {c1, c2, c3, c4}));
}

TEST(LratCheckerTest, CheckSuccessWithoutRatClauses) {
  // Example from Fig. 1 of 'Efficient Certified RAT Verification'.
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, -3});
  const ClausePtr c2 = factory.NewClause({-1, -2, +3});
  const ClausePtr c3 = factory.NewClause({+2, +3, -4});
  const ClausePtr c4 = factory.NewClause({-2, -3, +4});
  const ClausePtr c5 = factory.NewClause({-1, -3, -4});
  const ClausePtr c6 = factory.NewClause({+1, +3, +4});
  const ClausePtr c7 = factory.NewClause({-1, +2, +4});
  const ClausePtr c8 = factory.NewClause({+1, -2, -4});
  const ClausePtr c9 = factory.NewClause({+1, +2});
  const ClausePtr c10 = factory.NewClause({+1, +3});
  const ClausePtr c11 = factory.NewClause({+1});
  const ClausePtr c12 = factory.NewClause({+2});
  const ClausePtr c13 = factory.NewClause({});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);
  checker.AddProblemClause(c5);
  checker.AddProblemClause(c6);
  checker.AddProblemClause(c7);
  checker.AddProblemClause(c8);

  EXPECT_TRUE(checker.AddInferredClause(c9, {c1, c6, c3}));
  checker.DeleteClauses({c1});
  EXPECT_TRUE(checker.AddInferredClause(c10, {c9, c8, c6}));
  checker.DeleteClauses({c6});
  EXPECT_TRUE(checker.AddInferredClause(c11, {c10, c9, c4, c8}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({c10, c9, c8});
  EXPECT_TRUE(checker.AddInferredClause(c12, {c11, c7, c5, c3}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({c7, c3});
  EXPECT_TRUE(checker.AddInferredClause(c13, {c11, c12, c2, c4, c5}));
  EXPECT_TRUE(checker.Check());
}

TEST(LratCheckerTest, CheckSuccessWithRatClause) {
  // Example from Fig. 2 of 'Efficient Certified RAT Verification'.
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, -3});
  const ClausePtr c2 = factory.NewClause({-1, -2, +3});
  const ClausePtr c3 = factory.NewClause({+2, +3, -4});
  const ClausePtr c4 = factory.NewClause({-2, -3, +4});
  const ClausePtr c5 = factory.NewClause({-1, -3, -4});
  const ClausePtr c6 = factory.NewClause({+1, +3, +4});
  const ClausePtr c7 = factory.NewClause({-1, +2, +4});
  const ClausePtr c8 = factory.NewClause({+1, -2, -4});
  const ClausePtr c9 = factory.NewClause({+1});
  const ClausePtr c10 = factory.NewClause({+2});
  const ClausePtr c11 = factory.NewClause({});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);
  checker.AddProblemClause(c5);
  checker.AddProblemClause(c6);
  checker.AddProblemClause(c7);
  checker.AddProblemClause(c8);

  EXPECT_TRUE(
      checker.AddInferredClause(c9, /*rup_clauses=*/{},
                                {LratChecker::RatClauses{c2, {c6, c8}},
                                 LratChecker::RatClauses{c5, {c1, c8}},
                                 LratChecker::RatClauses{c7, {c6, c1}}}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({c8, c6, c1});
  EXPECT_TRUE(checker.AddInferredClause(c10, {c9, c7, c5, c3}));
  EXPECT_FALSE(checker.Check());
  checker.DeleteClauses({c7, c3});
  EXPECT_TRUE(checker.AddInferredClause(c11, {c9, c10, c2, c4, c5}));
  EXPECT_TRUE(checker.Check());
}

TEST(LratCheckerTest, CheckSuccessWithRatClausesExtensions) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, -3});
  const ClausePtr c2 = factory.NewClause({-1, -2, +3});
  const ClausePtr c3 = factory.NewClause({+2, +3, -4});
  const ClausePtr c4 = factory.NewClause({-2, -3, +4});
  const ClausePtr c5 = factory.NewClause({-1, -3, -4});
  const ClausePtr c6 = factory.NewClause({+1, +3, +4});
  const ClausePtr c7 = factory.NewClause({-1, +2, +4});
  const ClausePtr c8 = factory.NewClause({+1, -2, -4});
  const ClausePtr c9 = factory.NewClause({+1, +2});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);
  checker.AddProblemClause(c5);
  checker.AddProblemClause(c6);
  checker.AddProblemClause(c7);
  checker.AddProblemClause(c8);

  EXPECT_TRUE(checker.AddInferredClause(
      // Clause 1 becomes unit ({-3}).
      c9, {c1},
      // clause 9 ∪ {+3} = {+1, +2, +3} has two complementary literals with
      // clause 2 (+1 and +2). Hence the unit propagation proof is not needed.
      {LratChecker::RatClauses{c2, {}},
       // Likewise with clause 5 (+1 and +3).
       LratChecker::RatClauses{c5, {}},
       // If we assume that all the literals of clause 9 ∪ {+3} are false, as
       // well as all those of clause 7 except the negation of the pivot (i.e.,
       // {+2, +4}), then clause 6 is in conflict. But this would not be the
       // case if {+3}, propagated from clause 1 above, was ignored.
       LratChecker::RatClauses{c7, {c6}}}));
}

TEST(LratCheckerTest, ErrorStateIsSticky) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2});
  const ClausePtr c2 = factory.NewClause({-1, -2});
  const ClausePtr c3 = factory.NewClause({+5, +6});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  EXPECT_TRUE(checker.AddProblemClause(c1));
  EXPECT_FALSE(checker.AddInferredClause(c2, {}));
  // This clause is valid, but there was an error at a previous step.
  EXPECT_FALSE(checker.AddProblemClause(c3));
  EXPECT_EQ(checker.error_message(),
            absl::StrCat("In clause ", c2,
                         ": wrong number of resolvant clauses in RAT proof"));
}

TEST(LratCheckerTest, CompleteStateIsSticky) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1});
  const ClausePtr c2 = factory.NewClause({-1});
  const ClausePtr c3 = factory.NewClause({});
  const ClausePtr c4 = factory.NewClause({-1, -2});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  EXPECT_TRUE(checker.AddProblemClause(c1));
  EXPECT_TRUE(checker.AddProblemClause(c2));
  EXPECT_TRUE(checker.AddInferredClause(c3, {c1, c2}));
  EXPECT_TRUE(checker.Check());
  // This clause is invalid (wrong number of resolvant clauses), but it is
  // ignored since the proof is already complete.
  EXPECT_TRUE(checker.AddInferredClause(c4, {}));
  EXPECT_EQ(checker.error_message(), "");
}

TEST(LratCheckerTest, UnitClauseIdIsNotUnit) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, +3});
  const ClausePtr c2 = factory.NewClause({+1});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.AddProblemClause(c1);

  EXPECT_FALSE(checker.AddInferredClause(c2, {c1}));
  EXPECT_THAT(checker.error_message(),
              ::testing::HasSubstr(absl::StrCat(
                  "In clause ", c2, ": rup_clause ", c1, " is not unit")));
}

TEST(LratCheckerTest, MissingPivotForRatProof) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, +3});
  const ClausePtr c2 = factory.NewClause({});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);

  EXPECT_FALSE(
      checker.AddInferredClause(c2, {}, {LratChecker::RatClauses{c1, {}}}));
  EXPECT_EQ(checker.error_message(),
            absl::StrCat("In clause ", c2, ": missing pivot for RAT proof"));
}

TEST(LratCheckerTest, MissingClauseInRatIds) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, -3});
  const ClausePtr c2 = factory.NewClause({-1, -2, +3});
  const ClausePtr c3 = factory.NewClause({+1, +2});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);

  // If the last clause in `rup_clauses` is not a conflict, the `rat_clauses`
  // proof must be valid (here it is not, since clause 2 contains the negation
  // of the pivot but is not listed).
  EXPECT_FALSE(checker.AddInferredClause(c3, {c1}));
  EXPECT_EQ(checker.error_message(),
            absl::StrCat("In clause ", c3,
                         ": wrong number of resolvant clauses in RAT proof"));
}

TEST(LratCheckerTest, DuplicateRatResolvantId) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, -3});
  const ClausePtr c2 = factory.NewClause({-1, -2, +3});
  const ClausePtr c3 = factory.NewClause({+2, +3, -4});
  const ClausePtr c4 = factory.NewClause({-2, -3, +4});
  const ClausePtr c5 = factory.NewClause({-1, -3, -4});
  const ClausePtr c6 = factory.NewClause({+1, +3, +4});
  const ClausePtr c7 = factory.NewClause({-1, +2, +4});
  const ClausePtr c8 = factory.NewClause({+1, -2, -4});
  const ClausePtr c9 = factory.NewClause({+1});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);
  checker.AddProblemClause(c5);
  checker.AddProblemClause(c6);
  checker.AddProblemClause(c7);
  checker.AddProblemClause(c8);

  EXPECT_FALSE(
      checker.AddInferredClause(c9, /*rup_clauses=*/{},
                                {LratChecker::RatClauses{c2, {c6, c8}},
                                 LratChecker::RatClauses{c5, {c1, c8}},
                                 LratChecker::RatClauses{c5, {c1, c8}}}));
  EXPECT_EQ(checker.error_message(),
            absl::StrCat("In clause ", c9, ": duplicate resolvant ", c5));
}

TEST(LratCheckerTest, MissingNegatedPivotInRatResolvant) {
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2});
  const ClausePtr c2 = factory.NewClause({-3, +4});
  const ClausePtr c3 = factory.NewClause({+3, +4});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);

  EXPECT_FALSE(
      checker.AddInferredClause(c3, {}, {LratChecker::RatClauses{c1, {}}}));
  EXPECT_EQ(checker.error_message(),
            absl::StrCat("In clause ", c3,
                         ": missing negated pivot in resolvant ", c1));
}

TEST(LratCheckerTest, RatUnitClauseIdIsNotUnit) {
  // Example from Fig. 2 of 'Efficient Certified RAT Verification'.
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, -3});
  const ClausePtr c2 = factory.NewClause({-1, -2, +3});
  const ClausePtr c3 = factory.NewClause({+2, +3, -4});
  const ClausePtr c4 = factory.NewClause({-2, -3, +4});
  const ClausePtr c5 = factory.NewClause({-1, -3, -4});
  const ClausePtr c6 = factory.NewClause({+1, +3, +4});
  const ClausePtr c7 = factory.NewClause({-1, +2, +4});
  const ClausePtr c8 = factory.NewClause({+1, -2, -4});
  const ClausePtr c9 = factory.NewClause({+1});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);
  checker.AddProblemClause(c5);
  checker.AddProblemClause(c6);
  checker.AddProblemClause(c7);
  checker.AddProblemClause(c8);

  EXPECT_FALSE(
      checker.AddInferredClause(c9, /*rup_clauses=*/{},
                                {LratChecker::RatClauses{c2, {c7, c8}},
                                 LratChecker::RatClauses{c5, {c1, c8}},
                                 LratChecker::RatClauses{c7, {c6, c1}}}));
  EXPECT_EQ(checker.error_message(),
            absl::StrCat("In clause ", c9, ": rat_clauses.rup_clause ", c7,
                         " is not unit"));
}

TEST(LratCheckerTest, LastRatUnitClauseIdIsNotAConflict) {
  // Example from Fig. 2 of 'Efficient Certified RAT Verification'.
  Model model;
  ClauseFactory factory;
  const ClausePtr c1 = factory.NewClause({+1, +2, -3});
  const ClausePtr c2 = factory.NewClause({-1, -2, +3});
  const ClausePtr c3 = factory.NewClause({+2, +3, -4});
  const ClausePtr c4 = factory.NewClause({-2, -3, +4});
  const ClausePtr c5 = factory.NewClause({-1, -3, -4});
  const ClausePtr c6 = factory.NewClause({+1, +3, +4});
  const ClausePtr c7 = factory.NewClause({-1, +2, +4});
  const ClausePtr c8 = factory.NewClause({+1, -2, -4});
  const ClausePtr c9 = factory.NewClause({+1});

  LratChecker& checker = *model.GetOrCreate<LratChecker>();
  checker.EnableRatProofs();
  checker.AddProblemClause(c1);
  checker.AddProblemClause(c2);
  checker.AddProblemClause(c3);
  checker.AddProblemClause(c4);
  checker.AddProblemClause(c5);
  checker.AddProblemClause(c6);
  checker.AddProblemClause(c7);
  checker.AddProblemClause(c8);

  EXPECT_FALSE(checker.AddInferredClause(
      c9, /*rup_clauses=*/{},
      // Clause 6 does not become empty after propagation (clause 8 missing).
      {LratChecker::RatClauses{c2, {c6}}, LratChecker::RatClauses{c5, {c1, c8}},
       LratChecker::RatClauses{c7, {c6, c1}}}));
  EXPECT_EQ(checker.error_message(),
            absl::StrCat("In clause ", c9,
                         ": last rup_clause for rat_clauses.resolvant ", c2,
                         " is not a conflict"));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
