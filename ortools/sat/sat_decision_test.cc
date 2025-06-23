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

#include "ortools/sat/sat_decision.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

TEST(SatDecisionPolicyTest, ExternalPreferences) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  SatDecisionPolicy* decision = model.GetOrCreate<SatDecisionPolicy>();

  const int num_variables = 1000;
  trail->Resize(num_variables);
  decision->IncreaseNumVariables(num_variables);

  // Generate some arbitrary priorities (all different).
  std::vector<std::pair<BooleanVariable, double>> var_with_preference(
      num_variables);
  for (int i = 0; i < num_variables; ++i) {
    var_with_preference[i] = {BooleanVariable(i),
                              static_cast<double>(i) / num_variables};
  }

  absl::BitGen random;
  // Add them in random order.
  std::shuffle(var_with_preference.begin(), var_with_preference.end(), random);
  for (const auto p : var_with_preference) {
    const Literal literal(p.first, true);
    decision->SetAssignmentPreference(literal, p.second);
  }

  // Expect them in decreasing order.
  std::sort(var_with_preference.begin(), var_with_preference.end());
  std::reverse(var_with_preference.begin(), var_with_preference.end());
  for (int i = 0; i < num_variables; ++i) {
    const Literal literal(var_with_preference[i].first, true);
    EXPECT_EQ(literal, decision->NextBranch());
    trail->EnqueueSearchDecision(literal);
  }
}

TEST(SatDecisionPolicyTest, ErwaHeuristic) {
  Model model;
  auto* params = model.GetOrCreate<SatParameters>();
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* decision = model.GetOrCreate<SatDecisionPolicy>();
  sat_solver->SetNumVariables(10);
  params->set_use_erwa_heuristic(true);
  decision->ResetDecisionHeuristic();

  // Default.
  EXPECT_EQ(Literal(BooleanVariable(0), false), decision->NextBranch());

  // Do not return assigned decision.
  // Note that the priority queue tie-breaking is not in order.
  sat_solver->EnqueueDecisionIfNotConflicting(
      Literal(BooleanVariable(0), true));
  EXPECT_EQ(Literal(BooleanVariable(9), false), decision->NextBranch());

  // Lets enqueue some more.
  trail->EnqueueWithUnitReason(Literal(BooleanVariable(1), false));
  trail->EnqueueWithUnitReason(Literal(BooleanVariable(2), true));

  // We can Bump the reason and simulate a conflict.
  decision->BumpVariableActivities({Literal(BooleanVariable(2), true)});
  decision->BeforeConflict(trail->Index());
  sat_solver->Backtrack(0);

  // Now this bumped variable is first, with polarity as assigned.
  EXPECT_EQ(Literal(BooleanVariable(2), true), decision->NextBranch());
}

TEST(SatDecisionPolicyTest, SetTargetPolarityInStablePhase) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  SatDecisionPolicy* decision = model.GetOrCreate<SatDecisionPolicy>();
  const int num_variables = 100;
  trail->Resize(num_variables);
  decision->IncreaseNumVariables(num_variables);

  for (int i = 0; i < num_variables; ++i) {
    decision->SetTargetPolarityIfUnassigned(Literal(BooleanVariable(i), i % 2));
  }

  decision->SetStablePhase(true);
  for (int i = 0; i < num_variables; ++i) {
    const Literal literal = decision->NextBranch();
    EXPECT_EQ(literal, Literal(BooleanVariable(literal.Variable()),
                               literal.Variable().value() % 2));
    trail->EnqueueSearchDecision(literal);
  }
}

TEST(SatDecisionPolicyTest, SetTargetPolarity) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  SatDecisionPolicy* decision = model.GetOrCreate<SatDecisionPolicy>();
  const int num_variables = 100;
  trail->Resize(num_variables);
  decision->IncreaseNumVariables(num_variables);

  for (int i = 0; i < num_variables; ++i) {
    decision->SetTargetPolarityIfUnassigned(Literal(BooleanVariable(i), i % 2));
  }

  decision->SetStablePhase(false);
  for (int i = 0; i < num_variables; ++i) {
    const Literal literal = decision->NextBranch();
    EXPECT_EQ(literal, Literal(BooleanVariable(literal.Variable()),
                               literal.Variable().value() % 2));
    trail->EnqueueSearchDecision(literal);
  }
}
}  // namespace
}  // namespace sat
}  // namespace operations_research
