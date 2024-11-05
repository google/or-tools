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

#include "ortools/sat/table.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/container_logging.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

CpSolverStatus SolveTextProto(std::string_view text_proto) {
  const CpModelProto model_proto = ParseTestProto(text_proto);
  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  return SolveCpModel(model_proto, &model).status();
}

TEST(TableConstraintTest, EmptyOrTrivialSemantics) {
  EXPECT_EQ(SolveTextProto(R"pb(
              constraints {
                table {
                  values: []
                  exprs: []
                }
              }
            )pb"),
            CpSolverStatus::OPTIMAL);

  EXPECT_EQ(SolveTextProto(R"pb(
              constraints {
                table {
                  values: []
                  exprs: []
                  negated: true
                }
              }
            )pb"),
            CpSolverStatus::OPTIMAL);

  EXPECT_EQ(SolveTextProto(R"pb(
              constraints {
                table {
                  values: [ 0 ]
                  exprs: []
                }
              }
            )pb"),
            CpSolverStatus::MODEL_INVALID);

  // Invalid linear expression: coeffs without vars
  EXPECT_EQ(SolveTextProto(R"pb(
              constraints {
                table {
                  values: []
                  exprs:
                  [ { coeffs: 1 }]
                }
              }
            )pb"),
            CpSolverStatus::MODEL_INVALID);

  EXPECT_EQ(SolveTextProto(R"pb(
              constraints {
                table {
                  values: []
                  exprs:
                  [ { offset: 1 }]
                }
              }
            )pb"),
            CpSolverStatus::INFEASIBLE);

  EXPECT_EQ(SolveTextProto(R"pb(
              constraints {
                table {
                  values: []
                  exprs:
                  [ { offset: 1 }]
                  negated: true
                }
              }
            )pb"),
            CpSolverStatus::OPTIMAL);

  // Invalid: not affine
  EXPECT_EQ(SolveTextProto(R"pb(
              variables { domain: [ 0, 0 ] }
              variables { domain: [ 0, 0 ] }
              constraints {
                table {
                  values: [ 0 ]
                  exprs:
                  [ {
                    vars: [ 0, 1 ]
                    coeffs: [ 1, 1 ]
                  }]
                }
              }
            )pb"),
            CpSolverStatus::MODEL_INVALID);
}

TEST(TableConstraintTest, EnumerationAndEncoding) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X1"
      domain: [ 0, 4 ]
    }
    variables {
      name: "X3"
      domain: [ 0, 4 ]
    }
    variables {
      name: "X2"
      domain: [ 0, 4 ]
    }
    variables {
      name: "X4"
      domain: [ 0, 4 ]
    }
    constraints { table { vars: 0 vars: 2 values: 0 values: 1 } }
    constraints { table { vars: 1 vars: 3 values: 4 values: 0 } }
    constraints { table { vars: 2 vars: 1 values: 1 values: 4 } }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  // There should be just one solution [0, 4, 1, 0], but the solver used to
  // report more because of extra "free" variable used in the encoding.
  EXPECT_EQ(count, 1);
}

TEST(TableConstraintTest, EnumerationAndEncodingTwoVars) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X1"
      domain: [ 0, 4 ]
    }
    variables {
      name: "X3"
      domain: [ 0, 4 ]
    }
    constraints {
      table {
        vars: [ 0, 1 ]
        values: [ 0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 7);
}

TEST(TableConstraintTest, EnumerationAndEncodingFullPrefix) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        vars: [ 0, 1, 2 ]
        values: [
          0, 0, 0, 0, 1, 1, 0, 2, 2, 1, 0, 1, 1, 1,
          2, 1, 2, 0, 2, 0, 2, 2, 1, 0, 2, 2, 1
        ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 9);
}

TEST(TableConstraintTest, EnumerationAndEncodingPartialPrefix) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        vars: [ 0, 1, 2 ]
        values: [
          0, 0, 0, 0, 2, 2, 1, 0, 1, 1, 1, 2, 1, 2, 0, 2, 0, 2, 2, 1, 0
        ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 7);
}

TEST(TableConstraintTest, EnumerationAndEncodingInvalidTuples) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        vars: [ 0, 1, 2 ]
        values: [
          0, 0, 4, 0, 2, 2, 1, 0, 1, 1, 1, 2, 1, 2, 0, 2, 0, 2, 2, 1, 4
        ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  // There should be exactly one solution per valid tuple.
  EXPECT_EQ(count, 5);
}

TEST(TableConstraintTest, EnumerationAndEncodingOneTupleWithAny) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      table {
        vars: [ 0, 1, 2 ]
        values: [ 1, 0, 2, 1, 1, 2, 1, 2, 2 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 3);
}

TEST(TableConstraintTest, EnumerationAndEncodingPrefixWithLargeNegatedPart) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      table {
        vars: [ 0, 1, 2 ]
        values: [ 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 6);
}

TEST(TableConstraintTest, UnsatTable) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 5, 9 ] }
    constraints { table { vars: 0 vars: 1 values: 3 values: 3 } }
  )pb");

  Model model;
  model.Add(NewSatParameters("cp_model_presolve:false"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(NegatedTableConstraintTest, BasicTest) {
  CpModelBuilder cp_model;
  std::vector<IntVar> vars;
  vars.push_back(cp_model.NewIntVar({1, 2}));
  vars.push_back(cp_model.NewIntVar({1, 3}));
  vars.push_back(cp_model.NewIntVar({1, 3}));

  TableConstraint table = cp_model.AddForbiddenAssignments(vars);
  table.AddTuple({1, 2, 1});
  table.AddTuple({1, 2, 3});
  table.AddTuple({2, 2, 1});

  Model model;
  absl::btree_set<std::vector<int64_t>> solutions;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    std::vector<int64_t> solution;
    for (const IntVar var : vars) {
      solution.push_back(SolutionIntegerValue(r, var));
    }
    solutions.insert(solution);
  }));

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  absl::btree_set<std::vector<int64_t>> expected{{1, 1, 1},
                                                 {1, 1, 2},
                                                 {1, 1, 3},
                                                 // {1, 2, 1},
                                                 {1, 2, 2},
                                                 // {1, 2, 3},
                                                 {1, 3, 1},
                                                 {1, 3, 2},
                                                 {1, 3, 3},
                                                 {2, 1, 1},
                                                 {2, 1, 2},
                                                 {2, 1, 3},
                                                 // {2, 2, 1},
                                                 {2, 2, 2},
                                                 {2, 2, 3},
                                                 {2, 3, 1},
                                                 {2, 3, 2},
                                                 {2, 3, 3}};
  EXPECT_EQ(solutions, expected);
}

TEST(AutomatonTest, TestAutomaton) {
  const int kNumVars = 4;
  CpModelBuilder cp_model;
  std::vector<IntVar> variables;
  for (int i = 0; i < kNumVars; ++i) {
    variables.push_back(IntVar(cp_model.NewBoolVar()));
  }

  AutomatonConstraint automaton = cp_model.AddAutomaton(variables, 0, {3});
  automaton.AddTransition(0, 1, 0L);
  automaton.AddTransition(0, 2, 1L);
  automaton.AddTransition(1, 1, 0L);
  automaton.AddTransition(2, 2, 1L);
  automaton.AddTransition(1, 3, 1L);
  automaton.AddTransition(2, 3, 0L);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {
      automaton {
        final_states: 3
        transition_tail: 0
        transition_tail: 0
        transition_tail: 1
        transition_tail: 2
        transition_tail: 1
        transition_tail: 2
        transition_head: 1
        transition_head: 2
        transition_head: 1
        transition_head: 2
        transition_head: 3
        transition_head: 3
        transition_label: 0
        transition_label: 1
        transition_label: 0
        transition_label: 1
        transition_label: 1
        transition_label: 0
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), testing::EqualsProto(expected_model));

  Model model;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    num_solutions++;
    EXPECT_EQ(r.solution(0), r.solution(1));
    EXPECT_EQ(r.solution(0), r.solution(2));
    EXPECT_NE(r.solution(0), r.solution(3));
  }));

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(num_solutions, 2);
}

TEST(AutomatonTest, LoopingAutomatonMultipleFinalStates) {
  CpModelBuilder cp_model;
  std::vector<IntVar> variables;
  for (int i = 0; i < 10; ++i) {
    variables.push_back(cp_model.NewIntVar({0, 10}));
  }

  // These tuples accept "0*(12)+0*".
  AutomatonConstraint automaton = cp_model.AddAutomaton(variables, 1, {3, 4});
  automaton.AddTransition(1, 1, 0);
  automaton.AddTransition(1, 2, 1);
  automaton.AddTransition(2, 3, 2);
  automaton.AddTransition(3, 2, 1);
  automaton.AddTransition(3, 4, 0);
  automaton.AddTransition(4, 4, 0);

  Model model;
  absl::btree_set<std::vector<int64_t>> solutions;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    std::vector<int64_t> solution;
    for (const IntVar var : variables) {
      solution.push_back(SolutionIntegerValue(r, var));
    }
    solutions.insert(solution);
  }));

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  absl::btree_set<std::vector<int64_t>> expected{
      {0, 0, 0, 0, 0, 0, 0, 0, 1, 2}, {0, 0, 0, 0, 0, 0, 0, 1, 2, 0},
      {0, 0, 0, 0, 0, 0, 1, 2, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 2, 1, 2},
      {0, 0, 0, 0, 0, 1, 2, 0, 0, 0}, {0, 0, 0, 0, 0, 1, 2, 1, 2, 0},
      {0, 0, 0, 0, 1, 2, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 2, 1, 2, 0, 0},
      {0, 0, 0, 0, 1, 2, 1, 2, 1, 2}, {0, 0, 0, 1, 2, 0, 0, 0, 0, 0},
      {0, 0, 0, 1, 2, 1, 2, 0, 0, 0}, {0, 0, 0, 1, 2, 1, 2, 1, 2, 0},
      {0, 0, 1, 2, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 2, 1, 2, 0, 0, 0, 0},
      {0, 0, 1, 2, 1, 2, 1, 2, 0, 0}, {0, 0, 1, 2, 1, 2, 1, 2, 1, 2},
      {0, 1, 2, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 2, 1, 2, 0, 0, 0, 0, 0},
      {0, 1, 2, 1, 2, 1, 2, 0, 0, 0}, {0, 1, 2, 1, 2, 1, 2, 1, 2, 0},
      {1, 2, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 1, 2, 0, 0, 0, 0, 0, 0},
      {1, 2, 1, 2, 1, 2, 0, 0, 0, 0}, {1, 2, 1, 2, 1, 2, 1, 2, 0, 0},
      {1, 2, 1, 2, 1, 2, 1, 2, 1, 2}};
  EXPECT_EQ(solutions, expected);
}

TEST(AutomatonTest, NonogramRule) {
  CpModelBuilder cp_model;
  std::vector<IntVar> variables;
  for (int i = 0; i < 10; ++i) {
    variables.push_back(cp_model.NewIntVar({0, 10}));
  }

  // Accept sequences with 3 '1', then 2 '1', then 1 '1', separated by at least
  // one '0'.
  AutomatonConstraint automaton = cp_model.AddAutomaton(variables, 1, {9});
  automaton.AddTransition(1, 1, 0);
  automaton.AddTransition(1, 2, 1);
  automaton.AddTransition(2, 3, 1);
  automaton.AddTransition(3, 4, 1);
  automaton.AddTransition(4, 5, 0);
  automaton.AddTransition(5, 5, 0);
  automaton.AddTransition(5, 6, 1);
  automaton.AddTransition(6, 7, 1);
  automaton.AddTransition(7, 8, 0);
  automaton.AddTransition(8, 8, 0);
  automaton.AddTransition(8, 9, 1);
  automaton.AddTransition(9, 9, 0);

  Model model;
  absl::btree_set<std::vector<int64_t>> solutions;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    std::vector<int64_t> solution;
    for (const IntVar var : variables) {
      solution.push_back(SolutionIntegerValue(r, var));
    }
    solutions.insert(solution);
  }));

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  absl::btree_set<std::vector<int64_t>> expected{
      {0, 0, 1, 1, 1, 0, 1, 1, 0, 1}, {0, 1, 1, 1, 0, 0, 1, 1, 0, 1},
      {0, 1, 1, 1, 0, 1, 1, 0, 0, 1}, {0, 1, 1, 1, 0, 1, 1, 0, 1, 0},
      {1, 1, 1, 0, 0, 0, 1, 1, 0, 1}, {1, 1, 1, 0, 0, 1, 1, 0, 0, 1},
      {1, 1, 1, 0, 0, 1, 1, 0, 1, 0}, {1, 1, 1, 0, 1, 1, 0, 0, 0, 1},
      {1, 1, 1, 0, 1, 1, 0, 0, 1, 0}, {1, 1, 1, 0, 1, 1, 0, 1, 0, 0}};
  EXPECT_EQ(solutions, expected);
}

TEST(AutomatonTest, AnotherAutomaton) {
  CpModelBuilder cp_model;
  std::vector<IntVar> variables;
  for (int i = 0; i < 7; ++i) {
    variables.push_back(cp_model.NewIntVar({0, 10}));
  }

  AutomatonConstraint automaton =
      cp_model.AddAutomaton(variables, 1, {1, 2, 3, 4, 5, 6, 7});
  automaton.AddTransition(1, 2, 1);
  automaton.AddTransition(1, 5, 2);
  automaton.AddTransition(2, 3, 1);
  automaton.AddTransition(2, 5, 2);
  automaton.AddTransition(3, 4, 1);
  automaton.AddTransition(3, 5, 2);
  automaton.AddTransition(4, 0, 1);
  automaton.AddTransition(4, 5, 2);
  automaton.AddTransition(5, 2, 1);
  automaton.AddTransition(5, 6, 2);
  automaton.AddTransition(6, 2, 1);
  automaton.AddTransition(6, 7, 2);
  automaton.AddTransition(7, 2, 1);
  automaton.AddTransition(7, 0, 2);

  Model model;
  absl::btree_set<std::vector<int64_t>> solutions;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    std::vector<int64_t> solution;
    for (const IntVar var : variables) {
      solution.push_back(SolutionIntegerValue(r, var));
    }
    solutions.insert(solution);
  }));

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_log_search_progress(true);
  model.Add(NewSatParameters(parameters));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);

  // Out of the 2**7 tuples, the one that contains 4 consecutive 1 are:
  // - 1111??? (8)
  // - 21111?? (4)
  // - ?21111? (4)
  // - ??21111 (4)
  EXPECT_EQ(solutions.size(), 128 - 2 * 20);
}

TEST(LiteralTableConstraint, PropagationFromLiterals) {
  Model model;
  std::vector<Literal> selected;
  for (int i = 0; i < 4; ++i) {
    selected.push_back(Literal(model.Add(NewBooleanVariable()), true));
  }
  std::vector<std::vector<Literal>> literals(3);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      literals[i].push_back(Literal(model.Add(NewBooleanVariable()), true));
    }
    model.Add(ExactlyOneConstraint(literals[i]));
  }

  // Tuples (0, 0, 0), (1, 1, 1), (2, 2, 2), (0, 1, 2).
  std::vector<std::vector<Literal>> tuples = {
      {literals[0][0], literals[1][0], literals[2][0]},
      {literals[0][1], literals[1][1], literals[2][1]},
      {literals[0][2], literals[1][2], literals[2][2]},
      {literals[0][0], literals[1][1], literals[2][2]}};

  model.Add(LiteralTableConstraint(tuples, selected));
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();

  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(literals[0][0]));
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(sat_solver->Assignment().LiteralIsFalse(selected[1]));
  EXPECT_TRUE(sat_solver->Assignment().LiteralIsFalse(selected[2]));

  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(literals[1][1]));
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(sat_solver->Assignment().LiteralIsFalse(selected[0]));
  EXPECT_TRUE(sat_solver->Assignment().LiteralIsTrue(selected[3]));
}

TEST(LiteralTableConstraint, PropagationFromSelected) {
  Model model;
  std::vector<Literal> selected;
  for (int i = 0; i < 4; ++i) {
    selected.push_back(Literal(model.Add(NewBooleanVariable()), true));
  }
  std::vector<std::vector<Literal>> literals(3);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      literals[i].push_back(Literal(model.Add(NewBooleanVariable()), true));
    }
    model.Add(ExactlyOneConstraint(literals[i]));
  }

  // Tuples (0, 0, 0), (1, 1, 1), (2, 2, 2), (0, 1, 2).
  std::vector<std::vector<Literal>> tuples = {
      {literals[0][0], literals[1][0], literals[2][0]},
      {literals[0][1], literals[1][1], literals[2][1]},
      {literals[0][2], literals[1][2], literals[2][2]},
      {literals[0][0], literals[1][1], literals[2][2]}};

  model.Add(LiteralTableConstraint(tuples, selected));
  Trail* trail = model.GetOrCreate<Trail>();
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();

  trail->EnqueueSearchDecision(selected[1].Negated());
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(sat_solver->Assignment().LiteralIsFalse(literals[0][1]));
  EXPECT_TRUE(sat_solver->Assignment().LiteralIsFalse(literals[2][1]));

  trail->EnqueueSearchDecision(selected[3].Negated());
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(sat_solver->Assignment().LiteralIsFalse(literals[1][1]));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
