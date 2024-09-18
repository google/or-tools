// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/symmetry.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

TEST(SymmetryPropagatorTest, Permute) {
  const int num_variables = 6;
  const int num_literals = 2 * num_variables;
  std::unique_ptr<SparsePermutation> perm(new SparsePermutation(num_literals));
  perm->AddToCurrentCycle(Literal(+3).Index().value());
  perm->AddToCurrentCycle(Literal(+2).Index().value());
  perm->AddToCurrentCycle(Literal(-4).Index().value());
  perm->CloseCurrentCycle();
  // Note that the permutation 'p' must be compatible with the negation.
  // That is negation(p(l)) = p(negation(l)). This is actually not required
  // for this test though.
  perm->AddToCurrentCycle(Literal(-3).Index().value());
  perm->AddToCurrentCycle(Literal(-2).Index().value());
  perm->AddToCurrentCycle(Literal(+4).Index().value());
  perm->CloseCurrentCycle();

  Trail trail;
  SymmetryPropagator propagator;
  propagator.AddSymmetry(std::move(perm));
  trail.RegisterPropagator(&propagator);

  std::vector<Literal> literals = Literals({+1, +2, -2, +3});
  std::vector<Literal> output;
  propagator.Permute(0, literals, &output);
  EXPECT_THAT(output,
              ElementsAre(Literal(+1), Literal(-4), Literal(+4), Literal(+2)));
}

TEST(SymmetryPropagatorTest, BasicTest) {
  const int num_variables = 6;
  const int num_literals = 2 * num_variables;
  std::unique_ptr<SparsePermutation> perm(new SparsePermutation(num_literals));
  perm->AddToCurrentCycle(Literal(+3).Index().value());
  perm->AddToCurrentCycle(Literal(+2).Index().value());
  perm->AddToCurrentCycle(Literal(-4).Index().value());
  perm->CloseCurrentCycle();
  // Note that the permutation 'p' must be compatible with the negation.
  // That is negation(p(l)) = p(negation(l)).
  perm->AddToCurrentCycle(Literal(-3).Index().value());
  perm->AddToCurrentCycle(Literal(-2).Index().value());
  perm->AddToCurrentCycle(Literal(+4).Index().value());
  perm->CloseCurrentCycle();
  perm->AddToCurrentCycle(Literal(-5).Index().value());
  perm->AddToCurrentCycle(Literal(+5).Index().value());
  perm->CloseCurrentCycle();

  Trail trail;
  trail.Resize(num_variables);
  SymmetryPropagator propagator;
  propagator.AddSymmetry(std::move(perm));
  trail.RegisterPropagator(&propagator);

  // We need a mock propagator to inject a reason.
  struct MockPropagator : SatPropagator {
    MockPropagator() : SatPropagator("MockPropagator") {}
    bool Propagate(Trail* trail) final { return true; }
    absl::Span<const Literal> Reason(const Trail& /*trail*/,
                                     int /*trail_index*/,
                                     int64_t /*conflict_id*/) const final {
      return reason;
    }
    std::vector<Literal> reason;
  };
  MockPropagator mock_propagator;
  trail.RegisterPropagator(&mock_propagator);

  // With such a trail, nothing should propagate because the first non-symmetric
  // literal +3 is a decision.
  trail.Enqueue(Literal(+3), AssignmentType::kSearchDecision);
  trail.Enqueue(Literal(-5), mock_propagator.PropagatorId());
  while (!propagator.PropagationIsDone(trail)) {
    EXPECT_TRUE(propagator.Propagate(&trail));
  }
  EXPECT_EQ(trail.Index(), 2);

  // Now we take the decision +2 (which is the image of +3).
  trail.Enqueue(Literal(+2), AssignmentType::kUnitReason);

  // We need to initialize the reason for -5, because it will be needed during
  // the conflict creation that the Propagate() below will trigger.
  mock_propagator.reason = Literals({-3});

  // Because -5 is now the first non-symmetric literal, a conflict is detected
  // since +5 can then be propagated.
  EXPECT_FALSE(propagator.PropagationIsDone(trail));
  EXPECT_FALSE(propagator.Propagate(&trail));

  // Let assume that the reason for -5 is the assignment +3 (which make sense
  // since it was propagated). The expected conflict is as stated below because
  // if -5 and +2 are true, by summetry since we had +3 => -5 we know that +2 =>
  // 5.
  //
  // Note: by convention all the literals of a reason or a conflict are false.
  EXPECT_THAT(trail.FailingClause(), ElementsAre(Literal(-2), Literal(+5)));

  // Let backtrack to the trail to +3.
  trail.Untrail(trail.Index() - 2);
  propagator.Untrail(trail, trail.Index());

  // Let now assume that +3 => +2, by symmetry we can also propagate -4!
  while (!propagator.PropagationIsDone(trail)) {
    EXPECT_TRUE(propagator.Propagate(&trail));
  }
  EXPECT_EQ(trail.Index(), 1);
  trail.Enqueue(Literal(+2), mock_propagator.PropagatorId());
  EXPECT_FALSE(propagator.PropagationIsDone(trail));
  EXPECT_TRUE(propagator.Propagate(&trail));
  EXPECT_EQ(trail.Index(), 3);
  EXPECT_EQ(trail[2], Literal(-4));

  // Once again, if the reason for +2 was the assignment +3, we can compute
  // the reason for the assignment -4 (it is just the symmetric of the other).
  EXPECT_THAT(trail.Reason(Literal(-4).Variable()), ElementsAre(Literal(-2)));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
