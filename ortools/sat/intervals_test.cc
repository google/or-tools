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

#include "ortools/sat/intervals.h"

#include <stdint.h>

#include "gtest/gtest.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {
namespace {

TEST(IntervalsRepositoryTest, Precedences) {
  Model model;
  const AffineExpression start1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size1(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression start2(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size2(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end2(model.Add(NewIntegerVariable(0, 10)));

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable a = repo->CreateInterval(start1, end1, size1);
  const IntervalVariable b = repo->CreateInterval(start2, end2, size2);

  // Ok to call many times.
  repo->CreateDisjunctivePrecedenceLiteral(a, b);
  repo->CreateDisjunctivePrecedenceLiteral(a, b);

  EXPECT_NE(kNoLiteralIndex,
            repo->GetPrecedenceLiteral(repo->End(a), repo->Start(b)));
  EXPECT_EQ(Literal(repo->GetPrecedenceLiteral(repo->End(a), repo->Start(b))),
            Literal(repo->GetPrecedenceLiteral(repo->End(b), repo->Start(a)))
                .Negated());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
