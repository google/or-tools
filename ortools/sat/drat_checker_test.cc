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

#include "ortools/sat/drat_checker.h"

#include <limits>
#include <string>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {
namespace {

template <typename... Args>
auto LiteralsAre(Args... literals) {
  return ::testing::ElementsAre(Literal(literals)...);
}

const double kMaxTimeInSeconds = std::numeric_limits<double>::infinity();

DratChecker::Status CheckOptimizedProof(const DratChecker& drat_checker) {
  DratChecker optimized_proof_checker;
  for (const auto& clause : drat_checker.GetUnsatSubProblem()) {
    optimized_proof_checker.AddProblemClause(clause);
  }
  for (const auto& clause : drat_checker.GetOptimizedProof()) {
    optimized_proof_checker.AddInferredClause(clause);
  }
  return optimized_proof_checker.Check(kMaxTimeInSeconds);
}

TEST(DratCheckerTest, CheckBasicSuccess) {
  // Example from Fig. 3 of 'Trimming while Checking Clausal Proofs'.
  DratChecker checker;
  checker.AddProblemClause(Literals({-2, +3}));
  checker.AddProblemClause(Literals({+1, +3}));
  checker.AddProblemClause(Literals({-1, +2}));
  checker.AddProblemClause(Literals({-1, -2}));
  checker.AddProblemClause(Literals({+1, -2}));
  checker.AddProblemClause(Literals({+2, -3}));

  checker.AddInferredClause(Literals({-2}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
  EXPECT_EQ(DratChecker::Status::VALID, CheckOptimizedProof(checker));
}

TEST(DratCheckerTest, CheckBasicSuccessWithClauseAddedSeveralTimes) {
  DratChecker checker;
  checker.AddProblemClause(Literals({-2, +3}));
  checker.AddProblemClause(Literals({+1, +3}));
  checker.AddProblemClause(Literals({-1, +2}));
  checker.AddProblemClause(Literals({-1, -2}));
  checker.AddProblemClause(Literals({+1, -2}));
  checker.AddProblemClause(Literals({+2, -3}));

  // Add a clause two times and deletes it on)e time, there should still be one
  // copy left, which is needed for the rest )of the proof.
  checker.AddInferredClause(Literals({-2}));
  checker.AddInferredClause(Literals({-2}));
  checker.DeleteClause(Literals({-2}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
  EXPECT_EQ(DratChecker::Status::VALID, CheckOptimizedProof(checker));
}

TEST(DratCheckerTest, CheckSimpleSuccess) {
  // Example from Fig. 7 of 'Trimming while C)hecking Clausal Proofs'.
  DratChecker checker;
  checker.AddProblemClause(Literals({+1, +2, -3}));
  checker.AddProblemClause(Literals({-1, -2, +3}));
  checker.AddProblemClause(Literals({+2, +3, -4}));
  checker.AddProblemClause(Literals({-2, -3, +4, -3}));  // Duplicate literals
  checker.AddProblemClause(Literals({+1, +3, +4}));
  checker.AddProblemClause(Literals({-1, -3, -4}));
  checker.AddProblemClause(Literals({-1, +2, +4}));
  checker.AddProblemClause(Literals({+1, -2, -4}));

  checker.AddInferredClause(Literals({+1, +2}));
  checker.DeleteClause(Literals({+1, +2, -3, +2}));  // Duplicate literals.
  checker.AddInferredClause(Literals({+1, +1}));     // Duplicate literals.
  checker.DeleteClause(Literals({+1, +3, +4}));
  checker.DeleteClause(
      Literals({-4, -2, +1}));  // Different order from clause #8.
  checker.AddInferredClause(Literals({+2}));
  checker.DeleteClause(Literals({+2, +3, -4}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
  EXPECT_EQ(DratChecker::Status::VALID, CheckOptimizedProof(checker));
}

TEST(DratCheckerTest, CheckComplexSuccessRupProof) {
  // Example from Fig. 3 of 'Verifying Refutations with Extended Resolution'.
  DratChecker checker;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        for (int l = 0; l < 2; ++l) {
          checker.AddProblemClause(
              Literals({i == 0 ? 1 : -1, j == 0 ? 2 : -2, k == 0 ? 3 : -3,
                        l == 0 ? 4 : -4}));
        }
      }
    }
  }

  checker.AddInferredClause(Literals({1, 2, 3}));
  checker.AddInferredClause(Literals({1, 2}));
  checker.AddInferredClause(Literals({1, 3}));
  checker.AddInferredClause(Literals({1}));
  checker.AddInferredClause(Literals({2, 3}));
  checker.AddInferredClause(Literals({2}));
  checker.AddInferredClause(Literals({3}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
  EXPECT_EQ(DratChecker::Status::VALID, CheckOptimizedProof(checker));
}

TEST(DratCheckerTest, CheckComplexSuccessRapProof) {
  // Example from Fig. 3 of 'Verifying Refutations with Extended Resolution'.
  DratChecker checker;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        for (int l = 0; l < 2; ++l) {
          checker.AddProblemClause(
              Literals({i == 0 ? 1 : -1, j == 0 ? 2 : -2, k == 0 ? 3 : -3,
                        l == 0 ? 4 : -4}));
        }
      }
    }
  }

  checker.AddInferredClause(Literals({1}));
  checker.AddInferredClause(Literals({2}));
  checker.AddInferredClause(Literals({3}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
  EXPECT_EQ(DratChecker::Status::VALID, CheckOptimizedProof(checker));
}

TEST(DratCheckerTest, CheckComplexSuccessRapProofWithExtendedResolution) {
  // Example from Fig. 3 of 'Verifying Refutations with Extended Resolution'.
  DratChecker checker;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      for (int k = 0; k < 2; ++k) {
        for (int l = 0; l < 2; ++l) {
          checker.AddProblemClause(
              Literals({i == 0 ? 1 : -1, j == 0 ? 2 : -2, k == 0 ? 3 : -3,
                        l == 0 ? 4 : -4}));
        }
      }
    }
  }

  // Proof using additional variables not used in the problem clauses.
  checker.AddInferredClause(Literals({5, 1, 2}));
  checker.AddInferredClause(Literals({5, 1, -2}));
  checker.AddInferredClause(Literals({5, -1, 2}));
  checker.AddInferredClause(Literals({5, -1, -2}));
  checker.AddInferredClause(Literals({-5, 3, 4}));
  checker.AddInferredClause(Literals({-5, 3, -4}));
  checker.AddInferredClause(Literals({-5, -3, 4}));
  checker.AddInferredClause(Literals({-5, -3, -4}));
  checker.AddInferredClause(Literals({5, 1}));
  checker.AddInferredClause(Literals({5}));
  checker.AddInferredClause(Literals({3}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
  EXPECT_EQ(DratChecker::Status::VALID, CheckOptimizedProof(checker));
}

TEST(DratCheckerTest, CheckBasicSuccessWithoutDeletedClauses) {
  // Example from Fig. 7 of 'Trimming while Checking Clausal Proofs' without
  // the deleted clauses (which are not required to check the proof, but can
  // reduce its verification time).
  DratChecker checker;
  checker.AddProblemClause(Literals({+1, +2, -3}));
  checker.AddProblemClause(Literals({-1, -2, +3}));
  checker.AddProblemClause(Literals({+2, +3, -4}));
  checker.AddProblemClause(Literals({-2, -3, +4}));
  checker.AddProblemClause(Literals({+1, +3, +4}));
  checker.AddProblemClause(Literals({-1, -3, -4}));
  checker.AddProblemClause(Literals({-1, +2, +4}));
  checker.AddProblemClause(Literals({+1, -2, -4}));

  checker.AddInferredClause(Literals({+1, +2}));
  checker.AddInferredClause(Literals({+1}));
  checker.AddInferredClause(Literals({+2}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
  EXPECT_EQ(DratChecker::Status::VALID, CheckOptimizedProof(checker));
}

TEST(DratCheckerTest, CheckBasicFailure) {
  // Example from Fig. 7 of 'Trimming while Checking Clausal Proofs', with some
  // necessary lemmas of the proof removed (enough to also disable RAT checks).
  DratChecker checker;
  checker.AddProblemClause(Literals({+1, +2, -3}));
  checker.AddProblemClause(Literals({-1, -2, +3}));
  checker.AddProblemClause(Literals({+2, +3, -4}));
  checker.AddProblemClause(Literals({-2, -3, +4}));
  checker.AddProblemClause(Literals({+1, +3, +4}));
  checker.AddProblemClause(Literals({-1, -3, -4}));
  checker.AddProblemClause(Literals({-1, +2, +4}));
  checker.AddProblemClause(Literals({+1, -2, -4}));

  checker.AddInferredClause(Literals({+2}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::INVALID, checker.Check(kMaxTimeInSeconds));
}

TEST(DratCheckerTest, CheckFailureClauseNeededForProofDeleted) {
  // Example from Fig. 7 of 'Trimming while Checking Clausal Proofs', with
  // a clause deleted before it is no longer needed for the proof.
  DratChecker checker;
  checker.AddProblemClause(Literals({+1, +2, -3}));
  checker.AddProblemClause(Literals({-1, -2, +3}));
  checker.AddProblemClause(Literals({+2, +3, -4}));
  checker.AddProblemClause(Literals({-2, -3, +4}));
  checker.AddProblemClause(Literals({+1, +3, +4}));
  checker.AddProblemClause(Literals({-1, -3, -4}));
  checker.AddProblemClause(Literals({-1, +2, +4}));
  checker.AddProblemClause(Literals({+1, -2, -4}));

  checker.AddInferredClause(Literals({+1, +2}));
  checker.DeleteClause(Literals({+1, +2, -3}));
  checker.AddInferredClause(Literals({+1}));
  checker.DeleteClause(Literals({+1, +3, +4}));
  checker.DeleteClause(Literals({+1, -2, -4}));
  checker.DeleteClause(Literals({+2, +3, -4}));
  checker.AddInferredClause(Literals({+2}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::INVALID, checker.Check(kMaxTimeInSeconds));
}

TEST(DratCheckerTest,
     CheckBasicFailureClauseNeededForProofDeletedSeveralTimesInSequence) {
  DratChecker checker;
  checker.AddProblemClause(Literals({-2, +3}));
  checker.AddProblemClause(Literals({+1, +3}));
  checker.AddProblemClause(Literals({-1, +2}));
  checker.AddProblemClause(Literals({-1, -2}));
  checker.AddProblemClause(Literals({+1, -2}));
  checker.AddProblemClause(Literals({+2, -3}));

  // Add and delete a clause two times, there should still be no copy left,
  // yielding an invalid proof because this clause is needed for the rest of the
  // proof.
  checker.AddInferredClause(Literals({-2}));
  checker.DeleteClause(Literals({-2}));
  checker.AddInferredClause(Literals({-2}));
  checker.DeleteClause(Literals({-2}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::INVALID, checker.Check(kMaxTimeInSeconds));
}

TEST(DratCheckerTest,
     CheckBasicFailureClauseNeededForProofDeletedSeveralTimesInParallel) {
  DratChecker checker;
  checker.AddProblemClause(Literals({-2, +3}));
  checker.AddProblemClause(Literals({+1, +3}));
  checker.AddProblemClause(Literals({-1, +2}));
  checker.AddProblemClause(Literals({-1, -2}));
  checker.AddProblemClause(Literals({+1, -2}));
  checker.AddProblemClause(Literals({+2, -3}));

  // Add and delete a clause two times, there should still be no copy left,
  // yielding an invalid proof because this clause is needed for the rest of the
  // proof.
  checker.AddInferredClause(Literals({-2}));
  checker.AddInferredClause(Literals({-2}));
  checker.DeleteClause(Literals({-2}));
  checker.DeleteClause(Literals({-2}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::INVALID, checker.Check(kMaxTimeInSeconds));
}

TEST(DratCheckerTest, CheckBasicFailureTimeOut) {
  DratChecker checker;
  checker.AddProblemClause(Literals({+1}));
  checker.AddProblemClause(Literals({-1}));
  checker.AddInferredClause(Literals({}));

  EXPECT_EQ(DratChecker::Status::UNKNOWN, checker.Check(-1.0));
}

TEST(ContainsLiteralTest, Basic) {
  EXPECT_FALSE(ContainsLiteral(Literals({1, 2, 3}), Literal(4)));
  EXPECT_FALSE(ContainsLiteral(Literals({1, 2, 3}), Literal(-2)));
  EXPECT_TRUE(ContainsLiteral(Literals({1, 2, 3}), Literal(1)));
  EXPECT_TRUE(ContainsLiteral(Literals({1, 2, 3}), Literal(2)));
  EXPECT_TRUE(ContainsLiteral(Literals({1, 2, 3}), Literal(3)));
}

TEST(ResolveTest, Basic) {
  std::vector<Literal> resolvent;
  VariablesAssignment assignment(10);
  EXPECT_FALSE(Resolve(Literals({+1, +2, +3}), Literals({-3, -2, +1}),
                       Literal(2), &assignment, &resolvent));

  EXPECT_TRUE(Resolve(Literals({+1, +2, +3}), Literals({-1, +3, +2}),
                      Literal(1), &assignment, &resolvent));
  EXPECT_THAT(resolvent, LiteralsAre(+2, +3));

  EXPECT_TRUE(Resolve(Literals({+1, +2, +3}), Literals({-1, +5, +3, +4}),
                      Literal(1), &assignment, &resolvent));
  EXPECT_THAT(resolvent, LiteralsAre(+2, +3, +5, +4));

  EXPECT_TRUE(Resolve(Literals({+1, +3, +2}), Literals({+5, -3, +4}),
                      Literal(3), &assignment, &resolvent));
  EXPECT_THAT(resolvent, LiteralsAre(+1, +2, +5, +4));
}

TEST(DratCheckerTest, ReadFromFiles) {
  DratChecker checker;
  std::string cnf_file_path = file::JoinPath(testing::TempDir(), "drup.cnf");
  std::string drat_file_path = file::JoinPath(testing::TempDir(), "drup.drat");
  ASSERT_OK(file::SetContents(
      cnf_file_path,
      R"(c Example from Fig. 7 of "Trimming while Checking Clausal Proofs"
p cnf 4 8
 1  2 -3  0
-1 -2  3  0
 2  3 -4  0
-2 -3  4  0
 1  3  4  0
-1 -3 -4  0
-1  2  4  0
 1 -2 -4  0
  )",
      file::Defaults()));

  ASSERT_OK(file::SetContents(drat_file_path,
                              R"(  1  2  0
d 1  2 -3  0
  1  0
d 1  3  4  0
  2  0
d 2  3 -4  0
  0
)",
                              file::Defaults()));

  EXPECT_TRUE(AddProblemClauses(cnf_file_path, &checker));
  EXPECT_TRUE(AddInferredAndDeletedClauses(drat_file_path, &checker));
  EXPECT_EQ(DratChecker::Status::VALID, checker.Check(kMaxTimeInSeconds));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
