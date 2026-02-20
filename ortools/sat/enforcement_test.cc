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

#include "ortools/sat/enforcement.h"

#include <stdint.h>

#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

TEST(EnforcementPropagatorTest, BasicTest) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  const EnforcementId id1 = propag->Register(Literals({+1}));
  const EnforcementId id2 = propag->Register(Literals({+1, +2}));
  const EnforcementId id3 = propag->Register(Literals({-2}));

  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CANNOT_PROPAGATE);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+1));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+2));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::IS_FALSE);

  CHECK(sat_solver->ResetToLevelZero());
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CANNOT_PROPAGATE);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
}

TEST(EnforcementPropagatorTest, UntrailWork) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  const EnforcementId id1 = propag->Register(Literals({+1}));
  const EnforcementId id2 = propag->Register(Literals({+2}));
  const EnforcementId id3 = propag->Register(Literals({+3}));

  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+1));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+2));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
  const int level = sat_solver->CurrentDecisionLevel();

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+3));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::IS_ENFORCED);

  sat_solver->Backtrack(level);
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
}

TEST(EnforcementPropagatorTest, AddingAtPositiveLevelTrue) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  EXPECT_TRUE(propag->Propagate(trail));
  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+1));
  EXPECT_TRUE(propag->Propagate(trail));

  const EnforcementId id = propag->Register(std::vector<Literal>{+1});
  EXPECT_EQ(propag->Status(id), EnforcementStatus::IS_ENFORCED);

  sat_solver->Backtrack(0);
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
}

TEST(EnforcementPropagatorTest, AddingAtPositiveLevelFalse) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  EXPECT_TRUE(propag->Propagate(trail));
  sat_solver->EnqueueDecisionIfNotConflicting(Literal(-1));
  EXPECT_TRUE(propag->Propagate(trail));

  const EnforcementId id = propag->Register(std::vector<Literal>{+1});
  EXPECT_EQ(propag->Status(id), EnforcementStatus::IS_FALSE);

  sat_solver->Backtrack(0);
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id), EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
