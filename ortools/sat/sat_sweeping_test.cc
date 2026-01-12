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

#include "ortools/sat/sat_sweeping.h"

#include <array>
#include <utility>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

TEST(SatSweepingTest, FullSatSweepingOneProblem) {
  static constexpr std::array<int, 3> kClausesArray[] = {
      {9, -3, 15},     {-8, 19, -19},  {1, -6, -20},   {-7, 8, 2},
      {2, 14, -14},    {3, 14, 19},    {10, -9, -6},   {2, -17, 18},
      {10, 4, -12},    {9, 2, -15},    {1, 14, 17},    {-15, 11, 10},
      {17, -19, -8},   {12, 19, 8},    {10, -10, -14}, {3, 17, -9},
      {-7, -17, 4},    {-7, 10, -15},  {5, 1, 19},     {15, -18, -17},
      {3, -13, -20},   {10, 16, -8},   {12, -11, -9},  {-11, 12, 18},
      {11, -19, 13},   {9, -1, -20},   {18, -20, 12},  {12, 1, 10},
      {16, 18, -18},   {18, -20, -10}, {-18, 15, 14},  {-18, 6, -20},
      {0, -14, -17},   {0, -11, -18},  {-18, 7, 12},   {-8, 3, 15},
      {17, 19, 1},     {-20, -1, 3},   {-19, 1, -9},   {-9, -16, -15},
      {-2, 11, 8},     {4, 18, 5},     {-8, -5, -13},  {-18, 19, -6},
      {4, -18, 11},    {1, 4, 18},     {-5, -8, -11},  {-20, -17, 10},
      {-18, -14, -16}, {-3, -18, -7},  {-11, 19, 16},  {-1, -15, -13},
      {8, -5, 10},     {-17, -7, -1},  {-6, -1, -16},  {-3, -15, -19},
      {16, 13, 10},    {-17, 11, 12},  {15, 11, -2},   {13, 9, -16},
      {15, -9, -7},    {-8, -15, 1},   {-19, -10, 0},  {11, -15, -20},
      {12, -10, 8},    {16, 6, 17},    {19, 14, -2},   {-6, -7, -1},
      {13, 10, 14},    {17, 12, -9},   {-4, -12, -2},  {-13, -5, -9},
      {-2, -5, -8},    {12, -4, -11},  {-5, -10, 18},
  };
  constexpr absl::Span<const std::array<int, 3>> kClauses(kClausesArray);

  CompactVectorVector<int, Literal> clauses;
  for (const std::array<int, 3>& clause : kClauses) {
    clauses.Add({});
    for (int literal : clause) {
      if (literal >= 0) {
        clauses.AppendToLastVector(Literal(BooleanVariable(literal), true));
      } else {
        const BooleanVariable var(PositiveRef(literal));
        clauses.AppendToLastVector(Literal(var, false));
      }
    }
  }

  TimeLimit time_limit;
  const SatSweepingResult result = DoFullSatSweeping(
      std::move(clauses), &time_limit,
      [](Model* m) { m->GetOrCreate<Inprocessing>()->InprocessingRound(); });

  // There is only one possible correct sweep result for this model. Check it.
  EXPECT_EQ(result.status, SatSolver::FEASIBLE);
  EXPECT_THAT(result.binary_clauses,
              testing::Contains(testing::Pair(Literal(+12), Literal(+9))));
  EXPECT_THAT(result.binary_clauses,
              testing::Contains(testing::Pair(Literal(+17), Literal(+7))));
  EXPECT_THAT(result.unary_clauses, ElementsAre(Literal(-18)));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
