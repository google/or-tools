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

#include "ortools/sat/cp_model_solver.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_test_utils.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::UnorderedElementsAre;

int AddVariable(int64_t lb, int64_t ub, CpModelProto* model) {
  const int index = model->variables_size();
  sat::IntegerVariableProto* var = model->add_variables();
  var->add_domain(lb);
  var->add_domain(ub);
  return index;
}

int AddInterval(int64_t start, int64_t size, int64_t end, CpModelProto* model) {
  const int index = model->constraints_size();
  IntervalConstraintProto* interval =
      model->add_constraints()->mutable_interval();
  interval->mutable_start()->add_vars(AddVariable(start, end - size, model));
  interval->mutable_start()->add_coeffs(1);
  interval->mutable_size()->set_offset(size);
  *interval->mutable_end() = interval->start();
  interval->mutable_end()->set_offset(size);
  return index;
}

int AddOptionalInterval(int64_t start, int64_t size, int64_t end,
                        int existing_enforcement_variable,
                        CpModelProto* model) {
  const int index = model->constraints_size();
  ConstraintProto* constraint = model->add_constraints();
  constraint->add_enforcement_literal(existing_enforcement_variable);
  IntervalConstraintProto* interval = constraint->mutable_interval();
  interval->mutable_start()->add_vars(AddVariable(start, end - size, model));
  interval->mutable_start()->add_coeffs(1);
  interval->mutable_size()->set_offset(size);
  *interval->mutable_end() = interval->start();
  interval->mutable_end()->set_offset(size);
  return index;
}

TEST(LoadCpModelTest, PureSatProblem) {
  const CpModelProto model_proto = Random3SatProblem(100, 3);
  LOG(INFO) << CpModelStats(model_proto);
  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  LOG(INFO) << CpSolverResponseStats(response);
}

TEST(LoadCpModelTest, PureSatProblemWithLimit) {
  const CpModelProto model_proto = Random3SatProblem(500);
  LOG(INFO) << CpModelStats(model_proto);
  Model model;
  model.Add(NewSatParameters("max_deterministic_time:0.00001"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::UNKNOWN);
  LOG(INFO) << CpSolverResponseStats(response);
}

TEST(LoadCpModelTest, BooleanLinearOptimizationProblem) {
  const CpModelProto model_proto = RandomLinearProblem(20, 5);
  LOG(INFO) << CpModelStats(model_proto);
  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  LOG(INFO) << CpSolverResponseStats(response);
}

TEST(StopAfterFirstSolutionTest, BooleanLinearOptimizationProblem) {
  const CpModelProto model_proto = RandomLinearProblem(100, 100);
  LOG(INFO) << CpModelStats(model_proto);

  Model model;
  SatParameters params;
  params.set_num_search_workers(8);
  params.set_stop_after_first_solution(true);

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&num_solutions](const CpSolverResponse& /*r*/) { num_solutions++; }));
  model.Add(NewSatParameters(params));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::FEASIBLE);
  EXPECT_GE(num_solutions, 1);

  // Because we have 8 threads and we currently report all solution as we found
  // them, we might report more than one the time every subsolver is
  // terminated. This happens 8% of the time as of March 2020.
  EXPECT_LE(num_solutions, 2);
  LOG(INFO) << CpSolverResponseStats(response);
}

TEST(RelativeGapLimitTest, BooleanLinearOptimizationProblem) {
  const CpModelProto model_proto = RandomLinearProblem(100, 100);
  LOG(INFO) << CpModelStats(model_proto);

  Model model;
  SatParameters params;
  params.set_num_workers(1);
  params.set_relative_gap_limit(1e10);  // Should stop at the first solution!

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&num_solutions](const CpSolverResponse& /*r*/) { num_solutions++; }));
  model.Add(NewSatParameters(params));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);

  // We reported OPTIMAL, but there is indeed a gap.
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_LT(response.best_objective_bound() + 1e-6, response.objective_value());
  EXPECT_EQ(1, num_solutions);
  LOG(INFO) << CpSolverResponseStats(response);
}

TEST(LoadCpModelTest, InvalidProblem) {
  CpModelProto model_proto;
  model_proto.add_variables();  // No domain.
  Model model;
  EXPECT_EQ(SolveCpModel(model_proto, &model).status(),
            CpSolverStatus::MODEL_INVALID);
}

TEST(LoadCpModelTest, UnsatProblem) {
  CpModelProto model_proto;
  for (int i = 0; i < 2; ++i) {
    AddVariable(i, i, &model_proto);
  }
  auto* ct = model_proto.add_constraints()->mutable_linear();
  ct->add_domain(0);
  ct->add_domain(0);
  ct->add_vars(0);
  ct->add_coeffs(1);
  ct->add_vars(1);
  ct->add_coeffs(1);
  Model model;
  EXPECT_EQ(SolveCpModel(model_proto, &model).status(),
            CpSolverStatus::INFEASIBLE);
}

TEST(LoadCpModelTest, SimpleCumulative) {
  CpModelProto model_proto;
  AddInterval(0, 2, 4, &model_proto);
  AddInterval(1, 2, 4, &model_proto);
  ConstraintProto* ct = model_proto.add_constraints();
  ct->mutable_cumulative()->add_intervals(0);
  ct->mutable_cumulative()->add_demands()->set_offset(3);
  ct->mutable_cumulative()->add_intervals(1);
  ct->mutable_cumulative()->add_demands()->set_offset(4);
  ct->mutable_cumulative()->mutable_capacity()->set_offset(6);

  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.solution(0), 0);  // start_1
  EXPECT_EQ(response.solution(1), 2);  // start_2
}

TEST(SolverCpModelTest, EmptyModel) {
  const CpModelProto cp_model = ParseTestProto("solution_hint {}");

  SatParameters params;
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, SimpleInterval) {
  CpModelProto model_proto;
  const int deadline = 6;
  const int i1 = AddInterval(0, 3, deadline, &model_proto);
  const int i3 = AddInterval(3, 3, deadline, &model_proto);
  NoOverlapConstraintProto* no_overlap =
      model_proto.add_constraints()->mutable_no_overlap();
  no_overlap->add_intervals(i1);
  no_overlap->add_intervals(i3);
  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, SimpleOptionalIntervalFeasible) {
  CpModelProto model_proto;
  const int deadline = 6;
  const int i1_enforcement = AddVariable(0, 1, &model_proto);
  const int i1 =
      AddOptionalInterval(0, 3, deadline, i1_enforcement, &model_proto);

  const int i2_enforcement = AddVariable(0, 1, &model_proto);
  const int i2 =
      AddOptionalInterval(2, 2, deadline, i2_enforcement, &model_proto);

  const int i3 = AddInterval(3, 3, deadline, &model_proto);

  NoOverlapConstraintProto* no_overlap =
      model_proto.add_constraints()->mutable_no_overlap();
  no_overlap->add_intervals(i1);
  no_overlap->add_intervals(i2);
  no_overlap->add_intervals(i3);

  BoolArgumentProto* one_of_intervals_on =
      model_proto.add_constraints()->mutable_bool_xor();
  one_of_intervals_on->add_literals(i1_enforcement);
  one_of_intervals_on->add_literals(i2_enforcement);

  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, SimpleOptionalIntervalInfeasible) {
  CpModelProto model_proto;
  const int deadline = 6;
  const int i1_enforcement = AddVariable(0, 1, &model_proto);
  const int i1 =
      AddOptionalInterval(0, 3, deadline, i1_enforcement, &model_proto);

  const int i2_enforcement = AddVariable(0, 1, &model_proto);
  const int i2 =
      AddOptionalInterval(2, 2, deadline, i2_enforcement, &model_proto);

  const int i3 = AddInterval(3, 3, deadline, &model_proto);

  NoOverlapConstraintProto* no_overlap =
      model_proto.add_constraints()->mutable_no_overlap();
  no_overlap->add_intervals(i1);
  no_overlap->add_intervals(i2);
  no_overlap->add_intervals(i3);

  BoolArgumentProto* one_of_intervals_on =
      model_proto.add_constraints()->mutable_bool_and();
  one_of_intervals_on->add_literals(i1_enforcement);
  one_of_intervals_on->add_literals(i2_enforcement);

  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(SolveCpModelTest, NonInstantiatedVariables) {
  CpModelProto model_proto;
  const int a = AddVariable(0, 10, &model_proto);
  const int b = AddVariable(0, 10, &model_proto);
  auto* linear_constraint = model_proto.add_constraints()->mutable_linear();
  linear_constraint->add_vars(a);
  linear_constraint->add_vars(b);
  linear_constraint->add_coeffs(1);
  linear_constraint->add_coeffs(1);
  linear_constraint->add_domain(4);
  linear_constraint->add_domain(5);

  // We need to fix the first one, otherwise the lower bound will not be
  // enough for the second.
  model_proto.add_search_strategy()->add_variables(0);

  Model model;
  SatParameters params;
  params.set_instantiate_all_variables(false);
  params.set_search_branching(SatParameters::FIXED_SEARCH);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));

  const CpSolverResponse response = SolveCpModel(model_proto, &model);

  // Because we didn't try to instantiate the variables, we just did one round
  // of propagation. Note that this allows to use the solve as a simple
  // propagation engine with no search decision (modulo the binary variable that
  // will be instantiated anyway)!
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  ASSERT_EQ(2, response.solution_size());
  EXPECT_EQ(response.solution(0), 0);

  // Note that this one was not instantiated, but we used its lower bound.
  EXPECT_EQ(response.solution(1), 4);
}

// When there is nothing to do, we had a bug that didn't copy the solution
// with the core based solver, this simply test this corner case.
TEST(SolveCpModelTest, TrivialModelWithCore) {
  CpModelProto model_proto;
  const int a = AddVariable(1, 1, &model_proto);
  model_proto.mutable_objective()->add_vars(a);
  model_proto.mutable_objective()->add_coeffs(1);
  Model model;
  SatParameters params;
  params.set_optimize_with_core(true);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_TRUE(SolutionIsFeasible(
      model_proto, std::vector<int64_t>(response.solution().begin(),
                                        response.solution().end())));
}

TEST(SolveCpModelTest, TrivialLinearTranslatedModel) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: -10 domain: 10 }
    variables { domain: -10 domain: 10 }
    variables { domain: -461168601842738790 domain: 461168601842738790 }
    constraints {
      linear {
        vars: 0
        vars: 1
        coeffs: 1
        coeffs: 1
        domain: -4611686018427387903
        domain: 4611686018427387903
      }
    }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 2
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
    objective { vars: 2 coeffs: -1 scaling_factor: -1 }
  )pb");
  Model model;
  model.Add(NewSatParameters("cp_model_presolve:false"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_TRUE(SolutionIsFeasible(
      model_proto, std::vector<int64_t>(response.solution().begin(),
                                        response.solution().end())));
}

TEST(ConvertMPModelProtoToCpModelProtoTest, SimpleLinearExampleWithMaximize) {
  const MPModelProto mp_model = ParseTestProto(R"pb(
    maximize: true
    objective_offset: 0
    variable {
      lower_bound: -10
      upper_bound: 10
      objective_coefficient: 1
      is_integer: true
    }
    variable {
      lower_bound: -10
      upper_bound: 10
      objective_coefficient: 2
      is_integer: true
    }
    constraint {
      lower_bound: -100
      upper_bound: 100
      var_index: 0
      var_index: 1
      coefficient: 1
      coefficient: 1
    }
  )pb");
  CpModelProto cp_model;
  SolverLogger logger;
  ConvertMPModelProtoToCpModelProto(SatParameters(), mp_model, &cp_model,
                                    &logger);
  Model model;
  model.Add(NewSatParameters("cp_model_presolve:false"));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_TRUE(SolutionIsFeasible(
      cp_model, std::vector<int64_t>(response.solution().begin(),
                                     response.solution().end())));
}

TEST(SolveCpModelTest, SmallDualConnectedComponentsModel) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 1 domain: 10 }
    variables { domain: 1 domain: 10 }
    variables { domain: 1 domain: 10 }
    variables { domain: 1 domain: 10 }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 2 domain: 0 domain: 8 }
    }
    constraints {
      linear { vars: 2 vars: 3 coeffs: 1 coeffs: 2 domain: 0 domain: 6 }
    }
    objective {
      vars: 0
      vars: 1
      vars: 2
      vars: 3
      coeffs: -1
      coeffs: -2
      coeffs: -3
      coeffs: -4
      scaling_factor: -1
    }
  )pb");
  Model model;
  model.Add(NewSatParameters("cp_model_presolve:false"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_TRUE(SolutionIsFeasible(
      model_proto, std::vector<int64_t>(response.solution().begin(),
                                        response.solution().end())));
}

TEST(SolveCpModelTest, DualConnectedComponentsModel) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 1 domain: 10 }
    variables { domain: 1 domain: 10 }
    variables { domain: 1 domain: 10 }
    variables { domain: 1 domain: 10 }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 2 domain: 0 domain: 8 }
    }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 1 domain: 2 domain: 20 }
    }
    constraints {
      linear { vars: 2 vars: 3 coeffs: 1 coeffs: 2 domain: 0 domain: 6 }
    }
    constraints {
      linear { vars: 2 vars: 3 coeffs: 1 coeffs: 1 domain: 2 domain: 20 }
    }
    objective {
      vars: 0
      vars: 1
      vars: 2
      vars: 3
      coeffs: -1
      coeffs: -2
      coeffs: -3
      coeffs: -4
      scaling_factor: -1
    }
  )pb");
  Model model;
  model.Add(NewSatParameters("cp_model_presolve:false"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_TRUE(SolutionIsFeasible(
      model_proto, std::vector<int64_t>(response.solution().begin(),
                                        response.solution().end())));
}

TEST(SolveCpModelTest, EnumerateAllSolutions) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 1 domain: 4 }
    variables { domain: 1 domain: 4 }
    variables { domain: 1 domain: 4 }
    variables { domain: 1 domain: 4 }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 24);
}

TEST(SolveCpModelTest, EnumerateAllSolutionsBis) {
  const std::string model_str = R"pb(
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 5 }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 1 domain: 6 domain: 6 }
    }
  )pb";
  const CpModelProto model_proto = ParseTestProto(model_str);

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
        // Test the response was correctly filled.
        EXPECT_NE(0, response.num_branches());
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 5);
}

TEST(SolveCpModelTest, EnumerateAllSolutionsDomainsWithHoleInVar) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 1 domain: 2 domain: 4 domain: 5 }
    constraints {
      enforcement_literal: 0
      enforcement_literal: 1
      linear { vars: 2 coeffs: 1 domain: 2 domain: 2 }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 3 * 4 + 1);
}

TEST(SolveCpModelTest, EnumerateAllSolutionsDomainsWithHoleInEnforcedLinear1) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 1 domain: 5 }
    constraints {
      enforcement_literal: 0
      enforcement_literal: 1
      linear {
        vars: 2
        coeffs: 1
        domain: [ 1, 2, 4, 4 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 5 * 3 + 3);
}

TEST(SolveCpModelTest, EnumerateAllSolutionsDomainsWithHoleInEnforcedLinear2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 2 }
    variables { domain: 0 domain: 2 }
    constraints {
      enforcement_literal: 0
      enforcement_literal: 1
      linear {
        vars: 2
        coeffs: 1
        vars: 3
        coeffs: 1
        domain: [ 0, 1, 3, 4 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 9 * 3 + (9 - 3));
}

TEST(SolveCpModelTest, EnumerateAllSolutionsDomainsWithHoleInLinear2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 2 }
    variables { domain: 0 domain: 2 }
    constraints {
      linear {
        vars: 0
        coeffs: 1
        vars: 1
        coeffs: 1
        domain: [ 0, 1, 3, 4 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 9 - 3);
}

TEST(SolveCpModelTest, EnumerateAllSolutionsAndCopyToResponse) {
  const std::string model_str = R"pb(
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 5 }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 1 domain: 6 domain: 6 }
    }
  )pb";
  const CpModelProto model_proto = ParseTestProto(model_str);

  SatParameters params;
  params.set_enumerate_all_solutions(true);
  params.set_fill_additional_solutions_in_response(true);
  params.set_solution_pool_size(1000);  // A big enough value.

  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  std::vector<std::vector<int64_t>> additional_solutions;
  for (const auto& solution : response.additional_solutions()) {
    additional_solutions.push_back(std::vector<int64_t>(
        solution.values().begin(), solution.values().end()));
  }
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_THAT(additional_solutions,
              UnorderedElementsAre(
                  UnorderedElementsAre(1, 5), UnorderedElementsAre(2, 4),
                  UnorderedElementsAre(3, 3), UnorderedElementsAre(4, 2),
                  UnorderedElementsAre(5, 1)));

  // Not setting the solution_pool_size high enough gives partial result.
  // Because we randomize variable order, we don't know which solution will be
  // in the pool deterministically.
  params.set_solution_pool_size(3);
  const CpSolverResponse response2 = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response2.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response2.additional_solutions().size(), 3);
}

TEST(SolveCpModelTest, EnumerateAllSolutionsOfEmptyModel) {
  const std::string model_str = R"pb(
    variables { domain: 0 domain: 2 }
    variables { domain: 0 domain: 2 }
    variables { domain: 0 domain: 2 }
  )pb";
  const CpModelProto model_proto = ParseTestProto(model_str);

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 27);
}

TEST(SolveCpModelTest, SolutionsAreCorrectlyPostsolvedInTheObserver) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 1 domain: 4 }
    variables { domain: 1 domain: 1 }
    variables { domain: 3 domain: 3 }
    variables { domain: 1 domain: 4 }
  )pb");
  Model model;
  model.Add(NewFeasibleSolutionObserver([](const CpSolverResponse& response) {
    EXPECT_EQ(response.solution_size(), 4);
    LOG(INFO) << absl::StrJoin(response.solution(), " ");
  }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, ObjectiveDomainLowerBound) {
  // y = 10 - 2x.
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 1 domain: 10 }
    variables { domain: 1 domain: 10 }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 2 coeffs: 1 domain: 10 domain: 10 }
    }
    objective { vars: 1 coeffs: 1 domain: 1 domain: 10 }
  )pb");
  for (int lb = 1; lb <= 8; ++lb) {
    model_proto.mutable_objective()->set_domain(0, lb);
    Model model;
    model.Add(NewSatParameters("cp_model_presolve:false"));
    const CpSolverResponse response = SolveCpModel(model_proto, &model);
    EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
    EXPECT_EQ(response.objective_value(), lb % 2 ? lb + 1 : lb);
  }
}

TEST(SolveCpModelTest, LinMaxObjectiveDomainLowerBoundInfeasible) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 1 ]
      }
    }
    constraints {
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 2, 9223372036854775807 ]
      }
    }
    constraints {
      lin_max {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    objective { vars: 2 coeffs: 1 }
  )pb");

  Model model;
  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(SolveCpModelTest, LinMaxUniqueTargetLowerBoundInfeasible) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 1 ]
      }
    }
    constraints {
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 2, 9223372036854775807 ]
      }
    }
    constraints {
      lin_max {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(SolveCpModelTest, LinMaxUniqueTarget) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 4 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 1 ]
      }
    }
    constraints {
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 4 ]
      }
    }
    constraints {
      lin_max {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, HintWithCore) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 2, 8 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
      scaling_factor: 1
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 2, 3 ]
    }
  )pb");
  Model model;
  model.Add(NewSatParameters("optimize_with_core:true, linearization_level:0"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(2.0, response.objective_value());
}

TEST(SolveCpModelTest, BadHintWithCore) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 5 }
    variables { domain: 2 domain: 8 }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 1
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
    objective { vars: 2 scaling_factor: 1 coeffs: 1 }
    solution_hint { vars: 0 vars: 1 values: 4 values: 5 }
  )pb");
  Model model;
  model.Add(NewSatParameters("optimize_with_core:true, linearization_level:0"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(2.0, response.objective_value());
}

TEST(SolveCpModelTest, ForcedBadHint) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 5 }
    variables { domain: 2 domain: 8 }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 1
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
    objective { vars: 2 scaling_factor: 1 coeffs: 1 }
    solution_hint { vars: 0 vars: 1 values: 4 values: 5 }
  )pb");
  Model model;
  model.Add(NewSatParameters(
      "fix_variables_to_their_hinted_value:true, linearization_level:0"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(SolveCpModelTest, UnforcedBadHint) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 5 }
    variables { domain: 2 domain: 8 }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 1
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
    objective { vars: 2 scaling_factor: 1 coeffs: 1 }
    solution_hint { vars: 0 vars: 1 values: 4 values: 5 }
  )pb");
  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, HintWithNegativeRef) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    solution_hint { vars: -1 values: 1 }
  )pb");
  Model model;
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(SolveCpModelTest, SolutionHintBasicTest) {
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_num_workers(1);
  for (int loop = 0; loop < 50; ++loop) {
    CpModelProto model_proto;

    // Because the random problem might be UNSAT, we loop a few times until we
    // have a SAT one.
    for (;;) {
      model_proto = Random3SatProblem(200, 3);

      // Find a solution.
      Model model;
      model.Add(NewSatParameters(params));
      const CpSolverResponse response = SolveCpModel(model_proto, &model);
      if (response.status() != CpSolverStatus::OPTIMAL) continue;
      if (response.num_conflicts() == 0) continue;

      // Copy the solution to the hint.
      for (int i = 0; i < response.solution_size(); ++i) {
        model_proto.mutable_solution_hint()->add_vars(i);
        model_proto.mutable_solution_hint()->add_values(response.solution(i));
      }
      break;
    }

    // Now solve again, we should have no conflict!
    {
      Model model;
      int num_solution = 0;
      model.Add(NewSatParameters(params));
      model.Add(NewFeasibleSolutionObserver(
          [&num_solution](const CpSolverResponse& /*r*/) { num_solution++; }));
      const CpSolverResponse response = SolveCpModel(model_proto, &model);
      EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
      EXPECT_EQ(response.num_conflicts(), 0);
      EXPECT_EQ(num_solution, 1);
    }
  }
}

TEST(SolveCpModelTest, SolutionHintRepairTest) {
  SatParameters params;
  params.set_num_workers(1);
  params.set_cp_model_presolve(false);

  // NOTE(user): This test doesn't ensure that the hint is repaired. It only
  // makes sure that the solver doesn't crash if the hint is perturbed.
  CpModelProto model_proto;

  // Because the random problem might be UNSAT, we loop a few times until we
  // have a SAT one.
  for (;;) {
    model_proto = Random3SatProblem(200, 3);

    // Find a solution.
    Model model;
    model.Add(NewSatParameters(params));

    const CpSolverResponse response = SolveCpModel(model_proto, &model);
    if (response.status() != CpSolverStatus::OPTIMAL) continue;
    if (response.num_conflicts() == 0) continue;

    // Copy the solution to the hint with small perturbation.
    model_proto.mutable_solution_hint()->add_vars(0);
    model_proto.mutable_solution_hint()->add_values(response.solution(0) ^ 1);
    for (int i = 1; i < response.solution_size(); ++i) {
      model_proto.mutable_solution_hint()->add_vars(i);
      model_proto.mutable_solution_hint()->add_values(response.solution(i));
    }
    break;
  }

  // Now solve again.
  {
    Model model;
    params.set_repair_hint(true);
    model.Add(NewSatParameters(params));
    int num_solution = 0;
    model.Add(NewFeasibleSolutionObserver(
        [&num_solution](const CpSolverResponse& /*r*/) { num_solution++; }));
    const CpSolverResponse response = SolveCpModel(model_proto, &model);
    EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
    EXPECT_EQ(num_solution, 1);
  }
}

TEST(SolveCpModelTest, SolutionHintMinimizeL1DistanceTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 1, 2, 4, 8 ]
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 0, 1, 0, 1 ]
    }
  )pb");

  // TODO(user): Instead, we might change the presolve to always try to keep the
  // given hint feasible.
  Model model;
  model.Add(
      NewSatParameters("repair_hint:true, stop_after_first_solution:true, "
                       "keep_all_feasible_solutions_in_presolve:true"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_THAT(response.status(),
              AnyOf(Eq(CpSolverStatus::OPTIMAL), Eq(CpSolverStatus::FEASIBLE)));
  EXPECT_THAT(response.objective_value(), AnyOf(Eq(8), Eq(2)));
}

TEST(SolveCpModelTest, SolutionHintObjectiveTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 1, 2, 3, 4 ]
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 1, 0, 0, 1 ]
    }
  )pb");
  Model model;
  std::vector<double> solutions;
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_cp_model_presolve(false);
  parameters->set_log_search_progress(true);
  model.Add(
      NewFeasibleSolutionObserver([&solutions](const CpSolverResponse& r) {
        solutions.push_back(r.objective_value());
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  ASSERT_GE(solutions.size(), 2);
  EXPECT_EQ(solutions[0], 5.0);
  EXPECT_EQ(solutions.back(), 0.0);
}

TEST(SolveCpModelTest, SolutionHintOptimalObjectiveTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ -1, 2, 3, -4 ]
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 1, 0, 0, 1 ]
    }
  )pb");
  Model model;
  std::vector<double> solutions;
  model.Add(
      NewFeasibleSolutionObserver([&solutions](const CpSolverResponse& r) {
        solutions.push_back(r.objective_value());
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  ASSERT_EQ(solutions.size(), 1);
  EXPECT_EQ(solutions[0], -5.0);
}

TEST(SolveCpModelTest, SolutionHintEnumerateTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { name: "x" domain: 0 domain: 10 }
    variables { name: "y" domain: 0 domain: 10 }
    constraints {
      linear { vars: 1 vars: 0 coeffs: 1 coeffs: 1 domain: 10 domain: 10 }
    }
    solution_hint { vars: 0 values: -1 }
  )pb");
  Model model;
  SatParameters parameters;
  parameters.set_cp_model_presolve(false);
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&num_solutions](const CpSolverResponse& /*r*/) { num_solutions++; }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(num_solutions, 11);
}

TEST(SolveCpModelTest, SolutionHintAndAffineRelation) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 4, 4, 8, 8, 12, 12 ] }
    variables { domain: [ 2, 2, 4, 4, 6, 6 ] }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 8, 4 ]
    }
  )pb");
  SatParameters params;
  params.set_enumerate_all_solutions(true);
  params.set_stop_after_first_solution(true);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::FEASIBLE);
  EXPECT_EQ(response.solution(0), 8);
  EXPECT_EQ(response.solution(1), 4);
  EXPECT_EQ(response.num_conflicts(), 0);
}

TEST(SolveCpModelTest, MultipleEnforcementLiteral) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 25 + 25 + 25 + /*when enforced*/ 5);
}

TEST(SolveCpModelTest, TightenedDomains) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 1 domain: 10 }
    variables { domain: 0 domain: 10 }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 2
        coeffs: 3
        domain: 0
        domain: 7
      }
    }
  )pb");
  SatParameters params;
  params.set_fill_tightened_domains_in_response(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  CpSolverResponse response_with_domains_only;
  *response_with_domains_only.mutable_tightened_variables() =
      response.tightened_variables();

  const CpSolverResponse expected_domains = ParseTestProto(R"pb(
    tightened_variables { domain: 0 domain: 5 }
    tightened_variables { domain: 1 domain: 3 }
    tightened_variables { domain: 0 domain: 1 }
  )pb");
  EXPECT_THAT(expected_domains,
              testing::EqualsProto(response_with_domains_only));
}

TEST(SolveCpModelTest, TightenedDomainsAfterPresolve) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 1 domain: 10 }
    variables { domain: 0 domain: 10 }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 2
        coeffs: 3
        domain: 0
        domain: 7
      }
    }
  )pb");
  SatParameters params;
  params.set_fill_tightened_domains_in_response(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_stop_after_presolve(true);

  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  CpSolverResponse response_with_domains_only;
  *response_with_domains_only.mutable_tightened_variables() =
      response.tightened_variables();

  const CpSolverResponse expected_domains = ParseTestProto(R"pb(
    tightened_variables { domain: 0 domain: 5 }
    tightened_variables { domain: 1 domain: 3 }
    tightened_variables { domain: 0 domain: 1 }
  )pb");
  EXPECT_THAT(expected_domains,
              testing::EqualsProto(response_with_domains_only));
}

TEST(SolveCpModelTest, TightenedDomains2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 100 }
    constraints {
      enforcement_literal: 0
      linear { vars: 1 coeffs: 1 domain: 90 domain: 100 }
    }
    constraints {
      enforcement_literal: -1
      linear { vars: 1 coeffs: 1 domain: 0 domain: 10 }
    }
  )pb");
  SatParameters params;
  params.set_fill_tightened_domains_in_response(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  CpSolverResponse response_with_domains_only;
  *response_with_domains_only.mutable_tightened_variables() =
      response.tightened_variables();

  const CpSolverResponse expected_domains = ParseTestProto(R"pb(
    tightened_variables { domain: 0 domain: 1 }
    tightened_variables { domain: 0 domain: 10 domain: 90 domain: 100 }
  )pb");
  EXPECT_THAT(expected_domains,
              testing::EqualsProto(response_with_domains_only));
}

TEST(SolveCpModelTest, TightenedDomainsIfInfeasible) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 1 domain: 10 }
    variables { domain: 0 domain: 10 }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 2
        coeffs: 3
        domain: 80
        domain: 87
      }
    }
  )pb");
  SatParameters params;
  params.set_fill_tightened_domains_in_response(true);

  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(CpSolverStatus::INFEASIBLE, response.status());
  EXPECT_TRUE(response.tightened_variables().empty());
}

TEST(SolveCpModelTest, PermutedObjectiveNoPresolve) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 7 domain: 10 }
    variables { domain: 4 domain: 10 }
    variables { domain: 5 domain: 10 }
    objective {
      vars: [ 2, 1, 0 ]
      coeffs: [ 1, 2, 3 ]
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(CpSolverStatus::OPTIMAL, response.status());
}

TEST(SolveCpModelTest, TriviallyInfeasibleAssumptions) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 0 }
    assumptions: [ 0, 1 ]
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  EXPECT_THAT(response.sufficient_assumptions_for_infeasibility(),
              testing::ElementsAre(1));
}

TEST(SolveCpModelTest, TriviallyInfeasibleNegatedAssumptions) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 1 domain: 1 }
    assumptions: [ 0, -2 ]
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  EXPECT_THAT(response.sufficient_assumptions_for_infeasibility(),
              testing::ElementsAre(-2));
}

TEST(SolveCpModelTest, AssumptionsAndInfeasibility) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 3 }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 4, 4 ]
      }
    }
    assumptions: [ 0 ]
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  EXPECT_THAT(response.sufficient_assumptions_for_infeasibility(),
              testing::ElementsAre(0));
}

TEST(SolveCpModelTest, AssumptionsAndInfeasibility2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 3 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 4, 4 ]
      }
    }
    assumptions: [ 3, 0, 2 ]
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  EXPECT_THAT(response.sufficient_assumptions_for_infeasibility(),
              testing::ElementsAre(0));
}

TEST(SolveCpModelTest, AssumptionsAndInfeasibility3) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "a"
      domain: [ 0, 1 ]
    }
    variables {
      name: "b"
      domain: [ 0, 1 ]
    }
    variables {
      name: "i1"
      domain: [ 0, 1 ]
    }
    variables {
      name: "i2"
      domain: [ 0, 1 ]
    }
    variables {
      name: "i3"
      domain: [ 0, 1 ]
    }
    variables {
      name: "i4"
      domain: [ 0, 1 ]
    }
    constraints {
      enforcement_literal: 2
      bool_or { literals: [ -1, 1 ] }
    }
    constraints {
      enforcement_literal: 3
      bool_or { literals: [ 0, 1 ] }
    }
    constraints {
      enforcement_literal: 4
      bool_or { literals: [ -2, -1 ] }
    }
    constraints {
      enforcement_literal: 5
      bool_or { literals: [ -2 ] }
    }
    assumptions: [ 2, 3, 4, 5 ]
  )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  EXPECT_THAT(response.sufficient_assumptions_for_infeasibility(),
              testing::ElementsAre(2, 3, 5));
}

TEST(SolveCpModelTest, RegressionTest) {
  // This used to wrongly return UNSAT.
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 1 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {
      enforcement_literal: -2
      bool_or { literals: -1 }
    }
  )pb");
  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

// This used to crash because of how nodes with no arc were handled.
TEST(SolveCpModelTest, RouteConstraintRegressionTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      routes {
        tails: [ 0, 1, 3 ]
        heads: [ 1, 3, 0 ]
        literals: [ 0, 2, 1 ]
      }
    }
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(SolveCpModelTest, ObjectiveInnerObjectiveBasic) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 2, 10 ] }
    variables { domain: [ 2, 10 ] }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 2 ]
      scaling_factor: 10
      offset: 5
    }
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.objective_value(), 10 * (6 + 5));
  EXPECT_EQ(response.best_objective_bound(), 10 * (6 + 5));
  EXPECT_EQ(response.inner_objective_lower_bound(), 6);
}

TEST(SolveCpModelTest, ObjectiveDomainWithCore) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 6, 100 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
      scaling_factor: 1
      domain: [ 3, 4, 8, 10 ]
    }
  )pb");
  Model model;
  SatParameters params;
  params.set_optimize_with_core(true);
  params.set_linearization_level(0);
  params.set_log_search_progress(true);
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(8.0, response.objective_value());
}

TEST(SolveCpModelTest, ObjectiveDomainWithCore2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 6, 8 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
      scaling_factor: 1
      domain: [ 3, 4, 9, 10 ]
    }
  )pb");
  Model model;
  SatParameters params;
  params.set_optimize_with_core(true);
  params.set_linearization_level(0);
  params.set_log_search_progress(true);
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(SolveCpModelTest, EnumerateAllSolutionsReservoir) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: 1 domain: 4 }
    variables { domain: 1 domain: 4 }
    variables { domain: 1 domain: 4 }
    variables { domain: 1 domain: 4 }
    constraints {
      reservoir {
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        time_exprs { vars: 3 coeffs: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: -1 }
        level_changes: { offset: 3 }
        level_changes: { offset: -3 }
        min_level: 0
        max_level: 3
      }
    }
  )pb");

  // We can have (var0 <= var1) <= (var2 <= var3) or the other way.
  for (const bool encode : {true, false}) {
    SatParameters params;
    params.set_enumerate_all_solutions(true);
    params.set_expand_reservoir_constraints(encode);
    Model model;
    model.Add(NewSatParameters(params));
    int count = 0;
    model.Add(
        NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
          LOG(INFO) << absl::StrJoin(response.solution(), " ");
          ++count;
        }));
    const CpSolverResponse response = SolveCpModel(model_proto, &model);
    EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
    EXPECT_EQ(count, 89);
  }
}

TEST(SolveCpModelTest, EmptyModel) {
  const CpModelProto model_proto = ParseTestProto(R"pb()pb");
  SatParameters params;
  params.set_log_search_progress(true);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, EmptyOptimizationModel) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective { offset: 0 }
  )pb");
  SatParameters params;
  params.set_log_search_progress(true);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, EmptyOptimizationModelBuggy) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective { offset: 0 }
  )pb");
  SatParameters params;
  params.set_num_workers(1);
  params.set_log_search_progress(true);

  // This causes the inner solver to abort before finding the empty solution!
  params.set_max_number_of_conflicts(0);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::UNKNOWN);
}

TEST(SolveCpModelTest, EmptyOptimizationModelMultiThread) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective { offset: 0 }
  )pb");
  SatParameters params;
  params.set_log_search_progress(true);

  // This causes the inner solver to abort before finding the empty solution!
  // In non-interleave mode, everyone aborts and we finish with UNKNOWN.
  params.set_max_number_of_conflicts(0);
  params.set_num_workers(8);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(SolveCpModelTest, EmptyOptimizationModelBuggyInterleave) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective { offset: 0 }
  )pb");
  SatParameters params;
  params.set_log_search_progress(true);

  // This cause each chunk to abort right away with UNKNOWN. But because we are
  // in chunked mode, we always reschedule full solver and we never finish if
  // there is no time limit.
  //
  // TODO(user): Fix this behavior by not rescheduling in this case?
  params.set_max_number_of_conflicts(0);
  params.set_num_workers(8);
  params.set_interleave_search(true);
  params.set_use_feasibility_jump(false);
  params.set_interleave_batch_size(10);
  params.set_max_time_in_seconds(1);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);

  // The feasibility jump solver does not care about max_number_of_conflicts(),
  // so it finds the empty solution. But it is disabled in interleaved search.
  EXPECT_THAT(response.status(), testing::Eq(CpSolverStatus::UNKNOWN));
}

TEST(PresolveCpModelTest, Issue4068) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 1, 2 ] }
    constraints {
      no_overlap_2d {
        x_intervals: [ 1, 2 ]
        y_intervals: [ 3, 4 ]
      }
    }
    constraints {
      interval {
        start {}
        end {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        size {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      interval {
        start {}
        end { offset: 1 }
        size { offset: 1 }
      }
    }
    constraints {
      interval {
        start {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
        end {
          vars: [ 2 ]
          coeffs: [ 1 ]
          offset: 2
        }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { offset: 2 }
        end {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 2
        }
        size {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 2);
}

TEST(PresolveCpModelTest, EmptyExactlyOne) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one {} }
  )pb");
  Model model;
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, EmptyConstantProduct) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    constraints { int_prod { target { offset: 2 } } })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, EmptyElement) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    constraints { element {} })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, EmptyCumulativeNegativeCapacity) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    constraints { cumulative { capacity { offset: -1 } } })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, BadAutomaton) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    constraints {
      automaton {
        transition_tail: -2
        transition_head: -1
        transition_label: 1
        exprs { coeffs: 1 }
      }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, ConstantEnforcementLiteral) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 0 }
    constraints {
      enforcement_literal: -1
      bool_xor {}
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, EmptySearchStrategyExpr) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    constraints {}
    search_strategy {
      domain_reduction_strategy: SELECT_UPPER_HALF
      exprs {}
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, ConstantSearchStrategyExpr) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    constraints {}
    search_strategy {
      domain_reduction_strategy: SELECT_UPPER_HALF
      exprs { offset: 1 }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, NegativeElement) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    constraints { element { target: -1 vars: -1 } }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, NegativeAutomaton) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    constraints {
      automaton {
        final_states: 3
        transition_tail: 0
        transition_head: 0
        transition_label: 0
        vars: [ -1 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, ImpossibleInterval) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: 1 domain: 10 }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end {}
        size {}
      }
    })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, BadCumulative) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: 1 domain: 10 }
    constraints { cumulative { capacity { vars: 0 coeffs: -1 } } })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, NegatedStrategy) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: 1 domain: 4617263143898057573 }
    variables { domain: 1 domain: 1 }
    search_strategy { variables: -1 }
    assumptions: 1)pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
  params.set_cp_model_presolve(true);
  params.set_log_search_progress(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, CumulativeWithOverflow) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 6 }
    variables { domain: 3 domain: 3 }
    variables { domain: 0 domain: 6 }
    constraints {
      enforcement_literal: 0
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 3 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      cumulative {
        intervals: 0
        demands { offset: 4402971607593202523 }
      }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  params.set_log_search_progress(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, CumulativeWithOverflow2) {
  const CpModelProto cp_model =
      ParseTestProto(
          R"pb(
            variables { domain: 1 domain: 10 }
            variables { domain: 1 domain: 10 }
            constraints {
              cumulative { capacity { vars: 0 coeffs: 0 offset: -1 } }
            })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, NoOverlap2dCornerCase) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 6 }
        constraints {
          enforcement_literal: 2
          interval {
            start { vars: 0 coeffs: 1 }
            end { vars: 1 coeffs: 1 }
            size { offset: 3 }
          }
        }
        constraints {
          enforcement_literal: 2
          interval {
            start { vars: 3 coeffs: 1 }
            end { vars: 2 coeffs: 1 }
            size { offset: 2 }
          }
        }
        constraints { no_overlap_2d { x_intervals: 0 y_intervals: 1 } }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, BadDivision) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 1 domain: 4 }
        variables { domain: 1 domain: 4 }
        constraints {
          int_div {
            target { vars: 1 coeffs: 0 }
            exprs { offset: 1 }
            exprs { vars: 1 coeffs: 0 offset: 1 }
          }
        }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, CumulativeWithNegativeCapacity) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 6 }
        variables { domain: 2 domain: 2 }
        variables { domain: 2 domain: 6 }
        constraints {
          enforcement_literal: 1
          interval {
            start { vars: 2 coeffs: 1 }
            end { vars: 4 coeffs: 1 }
            size { vars: 3 coeffs: 1 }
          }
        }
        constraints {
          cumulative {
            capacity { offset: -1 }
            intervals: 0
            demands { vars: 0 coeffs: -1 offset: 1 }
          }
        }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, TrivialTableNegated) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 1 ] }
        constraints {
          table {
            values: [ 0, 1 ]
            negated: true
            exprs { offset: 1 }
            exprs { vars: 0 coeffs: 1 }
          }
        })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, TrivialTable) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 1 ] }
        constraints {
          table {
            values: [ 0, 1 ]
            exprs { offset: 1 }
            exprs { vars: 0 coeffs: 1 }
          }
        })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(NoOverlap2dCpModelTest, RequiresLns) {
  const CpModelProto cp_model = ParseTestProto(R"pb(
    variables: {
      name: "x_0"
      domain: [ 0, 80 ]
    }
    variables: {
      name: "y_0"
      domain: [ 0, 40 ]
    }
    variables: {
      name: "x_1"
      domain: [ 0, 80 ]
    }
    variables: {
      name: "y_1"
      domain: [ 0, 60 ]
    }
    variables: {
      name: "x_2"
      domain: [ 0, 90 ]
    }
    variables: {
      name: "y_2"
      domain: [ 0, 50 ]
    }
    variables: { domain: [ 1, 1 ] }
    variables: { domain: [ 0, 200 ] }
    variables: { domain: [ 0, 200 ] }
    variables: { domain: [ 0, 200 ] }
    variables: { domain: [ 0, 200 ] }
    variables: { domain: [ 0, 200 ] }
    variables: { domain: [ 0, 200 ] }
    constraints: {
      no_overlap_2d: {
        x_intervals: [ 1, 3, 5 ]
        y_intervals: [ 2, 4, 6 ]
      }
    }
    constraints: {
      name: "x_interval_0"
      enforcement_literal: 6
      interval: {
        start: { vars: 0 coeffs: 1 }
        end: { vars: 0 coeffs: 1 offset: 20 }
        size: { offset: 20 }
      }
    }
    constraints: {
      name: "y_interval_0"
      enforcement_literal: 6
      interval: {
        start: { vars: 1 coeffs: 1 }
        end: { vars: 1 coeffs: 1 offset: 60 }
        size: { offset: 60 }
      }
    }
    constraints: {
      name: "x_interval_1"
      enforcement_literal: 6
      interval: {
        start: { vars: 2 coeffs: 1 }
        end: { vars: 2 coeffs: 1 offset: 20 }
        size: { offset: 20 }
      }
    }
    constraints: {
      name: "y_interval_1"
      enforcement_literal: 6
      interval: {
        start: { vars: 3 coeffs: 1 }
        end: { vars: 3 coeffs: 1 offset: 40 }
        size: { offset: 40 }
      }
    }
    constraints: {
      name: "x_interval_2"
      enforcement_literal: 6
      interval: {
        start: { vars: 4 coeffs: 1 }
        end: { vars: 4 coeffs: 1 offset: 10 }
        size: { offset: 10 }
      }
    }
    constraints: {
      name: "y_interval_2"
      enforcement_literal: 6
      interval: {
        start: { vars: 5 coeffs: 1 }
        end: { vars: 5 coeffs: 1 offset: 50 }
        size: { offset: 50 }
      }
    }
    constraints: {
      linear: {
        vars: [ 7, 0, 2 ]
        coeffs: [ 1, -2, 2 ]
        domain: [ 0, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 7, 0, 2 ]
        coeffs: [ 1, 2, -2 ]
        domain: [ 0, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 8, 0, 4 ]
        coeffs: [ 1, -2, 2 ]
        domain: [ 10, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 8, 0, 4 ]
        coeffs: [ 1, 2, -2 ]
        domain: [ -10, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 9, 2, 4 ]
        coeffs: [ 1, -2, 2 ]
        domain: [ 10, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 9, 2, 4 ]
        coeffs: [ 1, 2, -2 ]
        domain: [ -10, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 10, 1, 3 ]
        coeffs: [ 1, -2, 2 ]
        domain: [ 20, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 10, 1, 3 ]
        coeffs: [ 1, 2, -2 ]
        domain: [ -20, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 11, 1, 5 ]
        coeffs: [ 1, -2, 2 ]
        domain: [ 10, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 11, 1, 5 ]
        coeffs: [ 1, 2, -2 ]
        domain: [ -10, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 12, 3, 5 ]
        coeffs: [ 1, -2, 2 ]
        domain: [ -10, 9223372036854775807 ]
      }
    }
    constraints: {
      linear: {
        vars: [ 12, 3, 5 ]
        coeffs: [ 1, 2, -2 ]
        domain: [ 10, 9223372036854775807 ]
      }
    }
    objective: {
      vars: [ 7, 8, 9, 10, 11, 12 ]
      coeffs: [ 1, 1, 1, 1, 1, 1 ]
    }
  )pb");
  SatParameters params;
  params.set_num_workers(16);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.objective_value(), 120);
}
TEST(PresolveCpModelTest, TableWithOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: -6055696632510658248 domain: 10 }
        variables { domain: 0 domain: 10 }
        constraints {
          table { vars: 1 vars: 0 values: 2 values: 0 negated: true }
        })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, ProdOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 5 }
        variables { domain: 0 domain: 5 }
        constraints {
          int_prod {
            target { offset: -3652538342751591977 }
            exprs { offset: -3 }
            exprs { vars: 0 coeffs: 0 offset: -3243792610144686519 }
            exprs {}
            exprs { offset: -1 }
          }
        }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, ModuloNotCanonical) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 10 }
        variables { domain: -4299172082820395165 domain: 10 }
        variables { domain: 0 domain: 10 }
        constraints {
          int_mod {
            target { vars: 1 coeffs: 1 offset: -4 }
            exprs { vars: 0 coeffs: 0 }
            exprs { offset: 3 }
          }
        }
        search_strategy { variables: 0 variables: 1 })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, CumulativeWithOverflow3) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 4 }
        variables { domain: 2 domain: 2 }
        variables { domain: 0 domain: 4 }
        variables { domain: 1 domain: 4 }
        variables { domain: 2 domain: 33554434 }
        variables { domain: 0 domain: 4 }
        variables { domain: 3 domain: 3 }
        variables { domain: 4 domain: 4 }
        variables { domain: 6 domain: 18014398509481990 }
        constraints {
          interval {
            start {}
            end { vars: 2 coeffs: 1 }
            size { vars: 4 coeffs: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 3 coeffs: 1 }
            end { vars: 5 coeffs: 1 }
            size { vars: 4 coeffs: 1 }
          }
        }
        constraints {
          cumulative {
            capacity { vars: 8 coeffs: 129 }
            intervals: 0
            intervals: 1
            demands { vars: 2 coeffs: 1 offset: 1 }
            demands { vars: 7 coeffs: 1 }
          }
        }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, CumulativeWithOverflow4) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 4 }
        variables { domain: 2 domain: 2 }
        variables { domain: 0 domain: 4 }
        variables { domain: 1 domain: 4 }
        variables { domain: 2 domain: 33554434 }
        variables { domain: 0 domain: 4 }
        variables { domain: 3 domain: 3 }
        variables { domain: 4 domain: 32772 }
        variables { domain: 6 domain: 3848116990577877790 }
        constraints {
          interval {
            start {}
            end { vars: 2 coeffs: 1 }
            size { vars: 4 coeffs: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 3 coeffs: 1 }
            end { vars: 5 coeffs: 1 }
            size { vars: 4 coeffs: 1 }
          }
        }
        constraints {
          cumulative {
            capacity { vars: 8 coeffs: 1 }
            intervals: 0
            intervals: 1
            demands { vars: 6 coeffs: 1 offset: 1 }
            demands { vars: 7 coeffs: 1 offset: 1 }
          }
        }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, FoundByFuzzing) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 1024 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 1, 4 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 1, 4 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 1, 4 ] }
        variables { domain: [ 0, 4 ] }
        variables { domain: [ 0, 4 ] }
        variables { domain: [ 0, 4 ] }
        variables { domain: [ 0, 4 ] }
        variables { domain: [ 0, 4 ] }
        variables { domain: [ 0, 4 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 512 ] }
        variables { domain: [ 0, 512 ] }
        variables { domain: [ 0, 2048 ] }
        variables { domain: [ 0, 512 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 1 ] }
        constraints {
          linear {
            vars: [ 2, 1 ]
            coeffs: [ 1, -1 ]
            domain: [ -9223372036854775808, 0 ]
          }
        }
        constraints {
          linear {
            vars: [ 3, 1 ]
            coeffs: [ 1, -1 ]
            domain: [ 1, 9223372036854775807 ]
          }
        }
        constraints {
          linear {
            vars: [ 5, 4 ]
            coeffs: [ 1, -1 ]
            domain: [ -9223372036854775808, 0 ]
          }
        }
        constraints {
          linear {
            vars: [ 6, 4 ]
            coeffs: [ 1, -1 ]
            domain: [ 1, 9223372036854775807 ]
          }
        }
        constraints {}
        constraints {
          linear {
            vars: [ 9, 7 ]
            coeffs: [ 1, -1 ]
            domain: [ 1, 9223372036854775807 ]
          }
        }
        constraints {
          linear {
            vars: [ 3, 1, 10 ]
            coeffs: [ -1, 1, 1 ]
            domain: [ -1, -1 ]
          }
        }
        constraints {
          interval {
            start {
              vars: [ 1 ]
              coeffs: [ 1 ]
              offset: 1
            }
            end {
              vars: [ 3 ]
              coeffs: [ 1 ]
            }
            size {
              vars: [ 10 ]
              coeffs: [ 1 ]
            }
          }
        }
        constraints {
          linear {
            vars: [ 1, 2, 11 ]
            coeffs: [ -1, 1, 1 ]
            domain: [ 0, 0 ]
          }
        }
        constraints {
          interval {
            start {
              vars: [ 2 ]
              coeffs: [ 1 ]
            }
            end {
              vars: [ 1 ]
              coeffs: [ 1 ]
            }
            size {
              vars: [ 11 ]
              coeffs: [ 1 ]
            }
          }
        }
        constraints {
          linear {
            vars: [ 6, 4, 12 ]
            coeffs: [ -1, 1, 1 ]
            domain: [ -1, -1 ]
          }
        }
        constraints {
          interval {
            start {
              vars: [ 4 ]
              coeffs: [ 1 ]
              offset: 1
            }
            end {
              vars: [ 6 ]
              coeffs: [ 1 ]
              offset: 1
            }
            size {
              vars: [ 12 ]
              coeffs: [ 1 ]
            }
          }
        }
        constraints {
          linear {
            vars: [ 4, 5, 13 ]
            coeffs: [ -1, 1, 1 ]
            domain: [ 0, 0 ]
          }
        }
        constraints {
          interval {
            start {
              vars: [ 5 ]
              coeffs: [ 1 ]
            }
            end {
              vars: [ 4 ]
              coeffs: [ 1 ]
            }
            size {
              vars: [ 13 ]
              coeffs: [ 1 ]
            }
          }
        }
        constraints {
          linear {
            vars: [ 9, 7, 14 ]
            coeffs: [ -1, 1, 1 ]
            domain: [ -1, -1 ]
          }
        }
        constraints {
          interval {
            start {
              vars: [ 7 ]
              coeffs: [ 1 ]
              offset: 1
            }
            end {
              vars: [ 9 ]
              coeffs: [ 1 ]
            }
            size {
              vars: [ 14 ]
              coeffs: [ 1 ]
            }
          }
        }
        constraints {
          linear {
            vars: [ 7, 8, 15 ]
            coeffs: [ -1, 1, 1 ]
            domain: [ 0, 0 ]
          }
        }
        constraints {
          interval {
            start {
              vars: [ 8 ]
              coeffs: [ 1 ]
            }
            end {
              vars: [ 7 ]
              coeffs: [ 1 ]
            }
            size {
              vars: [ 15 ]
              coeffs: [ 1 ]
            }
          }
        }
        constraints {}
        constraints {
          linear {
            vars: [ 6, 1 ]
            coeffs: [ 1, -1 ]
            domain: [ 1, 9223372036854775807 ]
          }
        }
        constraints {
          enforcement_literal: [ -17 ]
          linear {
            vars: [ 4, 1 ]
            coeffs: [ 1, -1 ]
            domain: [ 0, 0 ]
          }
        }
        constraints {
          enforcement_literal: [ 16 ]
          interval {
            start {
              vars: [ 1 ]
              coeffs: [ 1 ]
            }
            end {
              vars: [ 1 ]
              coeffs: [ 1 ]
              offset: 1
            }
            size { offset: 1 }
          }
        }
        constraints {
          interval {
            start {
              vars: [ 4 ]
              coeffs: [ 1 ]
            }
            end {
              vars: [ 4 ]
              coeffs: [ 1 ]
              offset: 1
            }
            size { offset: 1 }
          }
        }
        constraints {
          linear {
            vars: [ 8, 4 ]
            coeffs: [ 1, -1 ]
            domain: [ -9223372036854775808, 0 ]
          }
        }
        constraints {
          linear {
            vars: [ 9, 4 ]
            coeffs: [ 1, -1 ]
            domain: [ 1, 9223372036854775807 ]
          }
        }
        constraints {
          enforcement_literal: [ -18 ]
          linear {
            vars: [ 7, 4 ]
            coeffs: [ 1, -1 ]
            domain: [ 0, 0 ]
          }
        }
        constraints {
          enforcement_literal: [ 17 ]
          interval {
            start {
              vars: [ 4 ]
              coeffs: [ 1 ]
            }
            end {
              vars: [ 4 ]
              coeffs: [ 1 ]
              offset: 1
            }
            size { offset: 1 }
          }
        }
        constraints {
          linear {
            vars: [ 7 ]
            coeffs: [ 1 ]
            domain: [ 0, 0 ]
          }
        }
        constraints {
          linear {
            vars: [ 1 ]
            coeffs: [ 1 ]
            domain: [ 1, 1 ]
          }
        }
        constraints {
          linear {
            vars: [ 18, 0 ]
            coeffs: [ 2, -1 ]
            domain: [ -9223372036854775808, 0 ]
          }
        }
        constraints {
          cumulative {
            capacity {
              vars: [ 18 ]
              coeffs: [ 1 ]
            }
            intervals: [ 7, 11, 15 ]
            demands { offset: 1 }
            demands { offset: 1 }
            demands { offset: 2 }
          }
        }
        constraints {
          linear {
            vars: [ 19, 0 ]
            coeffs: [ 2, -1 ]
            domain: [ -9223372036854775808, 0 ]
          }
        }
        constraints {
          cumulative {
            capacity {
              vars: [ 19 ]
              coeffs: [ 1 ]
            }
            intervals: [ 9, 13, 17 ]
            demands { offset: 1 }
            demands { offset: 1 }
            demands { offset: 2 }
          }
        }
        constraints {
          cumulative {
            capacity {
              vars: [ 20 ]
              coeffs: [ 1 ]
            }
            intervals: [ 21, 26 ]
            demands { offset: 1 }
            demands { offset: 2 }
          }
        }
        constraints {
          linear {
            vars: [ 21, 0 ]
            coeffs: [ 2, -1 ]
            domain: [ -9223372036854775808, 0 ]
          }
        }
        constraints {
          cumulative {
            capacity {
              vars: [ 21 ]
              coeffs: [ 1 ]
            }
            intervals: [ 22 ]
            demands { offset: 1 }
          }
        }
        constraints {
          enforcement_literal: [ 22 ]
          linear {
            vars: [ 2, 3 ]
            coeffs: [ 1, -1 ]
            domain: [ -1, -1 ]
          }
        }
        constraints {
          enforcement_literal: [ 23 ]
          linear {
            vars: [ 5, 6 ]
            coeffs: [ 1, -1 ]
            domain: [ -1, -1 ]
          }
        }
        constraints {
          enforcement_literal: [ 24 ]
          linear {
            vars: [ 8, 9 ]
            coeffs: [ 1, -1 ]
            domain: [ -1, -1 ]
          }
        }
        objective {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, AllDifferentNotCanonical) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 1, 4294967306 ] }
        variables { domain: [ 1, 6 ] }
        variables { domain: [ 0, 10 ] }
        variables { domain: [ 1, 10000000 ] }
        constraints {
          all_diff {
            exprs { vars: 1 coeffs: 0 }
            exprs {}
            exprs { vars: 1 coeffs: 2 }
            exprs { vars: 0 coeffs: -1 }
          }
        })pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, HintGetBrokenByPresolve) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 1 ] }
        constraints {
          enforcement_literal: [ -1 ]
          table { vars: 1 }
        }
        constraints {
          enforcement_literal: [ -1 ]
          table {
            values: [ 9223372036854775807, 1 ]
            exprs {
              vars: [ 0 ]
              coeffs: [ 1 ]
              offset: 3562345932446661909
            }
            exprs {
              vars: [ 1 ]
              coeffs: [ 1 ]
            }
          }
        }
        objective {
          vars: [ 0, 1 ]
          coeffs: [ 1, 2 ]
        }
        solution_hint {
          vars: [ 0, 1 ]
          values: [ 1, 0 ]
        })pb");
  SatParameters params;
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, DisjunctiveFromFuzzing) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 6 ] }
        variables { domain: [ 3, 140737488355331 ] }
        variables { domain: [ 0, 6 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 2, 6 ] }
        variables { domain: [ 2, 2 ] }
        variables { domain: [ 2, 6 ] }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 offset: -1 }
            size { vars: 2 coeffs: -1 offset: 2199023255554 }
          }
        }
        constraints {
          interval {
            start { vars: 5 coeffs: 1 }
            end { vars: 7 coeffs: 1 }
            size { vars: 6 coeffs: 1 }
          }
        }
        constraints { no_overlap { intervals: [ 0, 0, 1, 1 ] } }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, PresolveChangesFeasibility) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 0 ] }
        variables { domain: [ 1, 1 ] }
        constraints { cumulative { capacity { vars: 1 coeffs: -1 } } }
        solution_hint { vars: 1 values: 6277701416517650879 }
      )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, PresolveChangesFeasibility2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 1 ] }
        constraints {
          enforcement_literal: -1
          bool_and { literals: 0 }
        }
        objective {
          vars: [ 0, 1, 2, 3 ]
          coeffs: [ -1, 2, 3, -4 ]
        }
        solution_hint {
          vars: [ 0, 1, 2, 3 ]
          values: [ 1, 0, 0, 1 ]
        }
        assumptions: -1
      )pb");
  SatParameters params;
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, HintContradictsAssumptions) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { name: "x" domain: 0 domain: 1 }
        variables { name: "y" domain: 0 domain: 1 }
        constraints { bool_or { literals: 0 } }
        constraints { bool_or { literals: -1 literals: -2 } }
        solution_hint { vars: 1 values: 1 }
        assumptions: 1
      )pb");
  CpSolverResponse response = SolveWithParameters(cp_model, SatParameters());
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, CumulativeOutOfBoundsRead) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 10 }
        variables { domain: 0 domain: 10 }
        constraints { cumulative { capacity { vars: 0 coeffs: -1 } } }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, InverseCrash) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 0 }
        variables { domain: 1 domain: 1 }
        constraints { inverse { f_direct: 1 f_inverse: 1 } }
        solution_hint { vars: 1 values: -1 })pb");
  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, CumulativeOutOfBoundsReadFixedDemand) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 4 }
        variables { domain: 2 domain: 2 }
        variables { domain: 0 domain: 4 }
        variables { domain: 1 domain: 4 }
        variables { domain: 2 domain: 2 }
        variables { domain: 0 domain: 4 }
        variables { domain: 3 domain: 3 }
        variables { domain: 4 domain: 4 }
        variables { domain: 6 domain: 6 }
        constraints {
          interval {
            start {}
            end { vars: 2 coeffs: 1 offset: 1 }
            size { vars: 2 coeffs: 1 offset: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 3 coeffs: 1 }
            end { vars: 5 coeffs: 1 }
            size { vars: 4 coeffs: 1 }
          }
        }
        constraints {
          cumulative {
            capacity { vars: 8 coeffs: 36028797018963969 }
            intervals: 0
            intervals: 1
            demands { vars: 6 coeffs: 1 offset: -3 }
            demands { vars: 8 coeffs: 1 }
          }
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, PresolveChangesFeasibilityMultiplication) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 2327064070896255483, 2327067369431138070 ] }
        variables { domain: [ 257, 1099511627786 ] }
        constraints {
          int_prod {
            target { vars: 0 coeffs: 1 offset: -6 }
            exprs { vars: 1 coeffs: 3 offset: 2327064070896254706 }
          }
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, BadNoOverlap2d) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 2 }
        variables { domain: 2 domain: 6 }
        constraints {
          enforcement_literal: 0
          bool_or {}
        }
        constraints {
          enforcement_literal: 1
          interval {
            start { vars: 0 coeffs: 1 }
            end { vars: 3 coeffs: 1 offset: -4607772983994847345 }
            size { vars: 2 coeffs: 1 }
          }
        }
        constraints { no_overlap_2d { x_intervals: 1 y_intervals: 1 } }
      )pb");
  SatParameters params;

  params.set_use_timetabling_in_no_overlap_2d(true);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, NoOverlapLinearizationOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 6 }
        variables { domain: -3659321530269907407 domain: 3496689482055784131 }
        variables { domain: 2 domain: 7 }
        constraints {
          enforcement_literal: 0
          linear {
            vars: [ 1, 2, 3 ]
            coeffs: [ 1, 1, -1 ]
            domain: [ 0, 0 ]
          }
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, TableHintBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 576460752303423489 }
        variables { domain: 0 domain: 268435457 }
        variables { domain: 0 domain: 576460752303423489 }
        constraints {
          table { vars: 1 values: 17179869184 values: 1 negated: true }
        }
        solution_hint {
          vars: 0
          vars: 1
          vars: 2
          vars: 3
          values: 1
          values: 0
          values: 0
          values: 1
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, LinearizationBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: -4040617518406929344 domain: 10 }
        variables { domain: 6 domain: 10 }
        variables { domain: 0 domain: 10 }
        variables { domain: 1 domain: 10000000 }
        constraints {
          all_diff {
            exprs {
              vars: 1
              coeffs: 18014398509481986
              offset: -1252623043085079047
            }
            exprs { vars: 0 coeffs: -1 }
            exprs { vars: 0 coeffs: -1 offset: -7 }
          }
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, DetectDuplicateColumnsOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: -4611686018427387903 domain: 0 }
        variables { domain: 0 domain: 5 }
        variables { domain: 0 domain: 5 }
        objective {
          vars: 0
          vars: 1
          coeffs: 1
          coeffs: 1
          domain: 1
          domain: 7666432986417144262
        })pb");
  SatParameters params;

  params.set_log_search_progress(true);
  params.set_cp_model_presolve(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, TableExpandPreservesSolutionHint) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 18014398509481985 }
        variables { domain: 0 domain: 1 }
        constraints {
          enforcement_literal: -1
          table {
            values: 9223372036854775807
            values: 1
            values: 0
            exprs { vars: 1 coeffs: 1 }
          }
        }
        solution_hint {
          vars: [ 0, 1, 2, 3 ]
          values: [ 1, 0, 0, 1 ]
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(true);
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, TableExpandPreservesSolutionHint2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: [ 0, 1, 18014398509481985, 18014398509481985 ] }
        variables { domain: [ 0, 18014398509481985 ] }
        variables { domain: [ 0, 18014398509481985 ] }
        constraints {
          enforcement_literal: -1
          table {
            values: [ 9223372036854775807, 1, 0 ]
            exprs { vars: 1 coeffs: 1 }
          }
        }
        constraints {
          enforcement_literal: 0
          table {
            values: [ 9223372036854775807, 1, 0 ]
            exprs { vars: 1 coeffs: 1 }
          }
        }
        solution_hint {
          vars: [ 0, 1, 2, 3 ]
          values: [ 1, 0, 0, 1 ]
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(true);
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, PresolveOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 4611686018427387903 }
        variables { domain: 0 domain: 1 }
        variables {
          domain: 8
          domain: 12
          domain: 2986687222969572620
          domain: 2986687222969572620
        }
        variables { domain: 2 domain: 2 }
        constraints {
          int_prod {
            target { vars: 2 coeffs: 1 }
            exprs { vars: 1 coeffs: 1 offset: 1 }
            exprs { vars: 0 coeffs: 1 }
          }
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(true);
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, InverseBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: -3744721377111001386 domain: 0 }
        variables { domain: 0 domain: 3 }
        variables {
          domain: -1
          domain: 0
          domain: 4611686018427387903
          domain: 4611686018427387903
        }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        constraints {
          inverse {
            f_direct: 0
            f_direct: 2
            f_direct: 4
            f_direct: 6
            f_inverse: 1
            f_inverse: 3
            f_inverse: 5
            f_inverse: 7
          }
        }
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(true);
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, FuzzerCrash3) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1024 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 1 domain: 4 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 1 domain: 4 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 1 domain: 4 }
        variables { domain: 0 domain: 4 }
        variables { domain: 0 domain: 4 }
        variables { domain: 0 domain: 4 }
        variables { domain: 0 domain: 4 }
        variables { domain: 0 domain: 4 }
        variables { domain: 0 domain: 4 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 512 }
        variables { domain: 0 domain: 512 }
        variables { domain: 0 domain: 2048 }
        variables { domain: 0 domain: 512 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        constraints {
          linear {
            vars: 2
            vars: 1
            coeffs: 1
            coeffs: -1
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          linear {
            vars: 3
            vars: 1
            coeffs: 1
            coeffs: -1
            domain: 1
            domain: 9223372036854775807
          }
        }
        constraints {}
        constraints {}
        constraints {
          linear {
            vars: 8
            vars: 7
            coeffs: 1
            coeffs: -1
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          linear {
            vars: 9
            vars: 7
            coeffs: 1
            coeffs: -1
            domain: 1
            domain: 9223372036854775807
          }
        }
        constraints {}
        constraints {
          interval {
            start { vars: 1 coeffs: 1 offset: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 10 coeffs: 1 }
          }
        }
        constraints {
          linear {
            vars: 1
            vars: 2
            vars: 11
            coeffs: -1
            coeffs: 1
            coeffs: 1
            domain: 0
            domain: 0
          }
        }
        constraints {
          interval {
            start { vars: 2 coeffs: 1 }
            end { vars: 1 coeffs: 1 }
            size { vars: 11 coeffs: 1 }
          }
        }
        constraints {
          linear {
            vars: 6
            vars: 4
            vars: 12
            coeffs: -1
            coeffs: 1
            coeffs: 1
            domain: -1
            domain: -1
          }
        }
        constraints {
          interval {
            start { vars: 4 coeffs: 1 offset: 1 }
            end { vars: 6 coeffs: 1 }
            size { vars: 12 coeffs: 1 }
          }
        }
        constraints {
          linear {
            vars: 4
            vars: 5
            vars: 13
            coeffs: -1
            coeffs: 1
            coeffs: 1
            domain: 0
            domain: 0
          }
        }
        constraints {
          interval {
            start { vars: 5 coeffs: 1 }
            end { vars: 4 coeffs: 1 }
            size { vars: 13 coeffs: 1 }
          }
        }
        constraints {}
        constraints {
          interval {
            start { vars: 7 coeffs: 1 offset: 1 }
            end { vars: 9 coeffs: 1 }
            size { vars: 14 coeffs: 1 }
          }
        }
        constraints {
          linear {
            vars: 7
            vars: 8
            vars: 15
            coeffs: -1
            coeffs: 1
            coeffs: 1
            domain: 0
            domain: 0
          }
        }
        constraints {
          interval {
            start { vars: 8 coeffs: 1 }
            end { vars: 7 coeffs: 1 }
            size { vars: 15 coeffs: 1 }
          }
        }
        constraints {
          linear {
            vars: 5
            vars: 1
            coeffs: 1
            coeffs: -1
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          linear {
            vars: 6
            vars: 1
            coeffs: 1
            coeffs: -1
            domain: 1
            domain: 9223372036854775807
          }
        }
        constraints {
          enforcement_literal: -17
          linear { vars: 4 vars: 1 coeffs: 1 coeffs: -1 domain: 0 domain: 0 }
        }
        constraints {
          enforcement_literal: 16
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 1 coeffs: 1 offset: 1 }
            size { offset: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 4 coeffs: 1 }
            end { vars: 4 coeffs: 1 offset: 1 }
            size { offset: 1 }
          }
        }
        constraints {
          linear {
            vars: 8
            vars: 4
            coeffs: 1
            coeffs: -1
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          linear {
            vars: 9
            vars: 4
            coeffs: 1
            coeffs: -1
            domain: 1
            domain: 9223372036854775807
          }
        }
        constraints {
          enforcement_literal: -18
          linear { vars: 7 vars: 4 coeffs: 1 coeffs: -1 domain: 0 domain: 0 }
        }
        constraints {
          enforcement_literal: 17
          interval {
            start { vars: 4 coeffs: 1 }
            end { vars: 4 coeffs: 1 offset: 4175356038966811637 }
            size { offset: 1 }
          }
        }
        constraints { linear { vars: 7 coeffs: 1 domain: 0 domain: 0 } }
        constraints {}
        constraints {
          linear {
            vars: 18
            vars: 0
            coeffs: 2
            coeffs: -1
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          cumulative {
            capacity { vars: 18 coeffs: 1 }
            intervals: 7
            intervals: 11
            intervals: 15
            demands { offset: 1 }
            demands { offset: 1 }
            demands { offset: 2 }
          }
        }
        constraints {
          linear {
            vars: 19
            vars: 0
            coeffs: 2
            coeffs: -1
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          cumulative {
            capacity { vars: 19 coeffs: 1 }
            intervals: 9
            intervals: 13
            intervals: 17
            demands { offset: 1 }
            demands { offset: 1 }
            demands { offset: 2 }
          }
        }
        constraints {
          linear {
            vars: 20
            vars: 0
            coeffs: 1
            coeffs: -2
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          cumulative {
            capacity { vars: 20 coeffs: 1 }
            intervals: 21
            intervals: 26
            demands { offset: 1 }
            demands { offset: 2 }
          }
        }
        constraints {
          linear {
            vars: 21
            vars: 0
            coeffs: 2
            coeffs: -1
            domain: -9223372036854775808
            domain: 0
          }
        }
        constraints {
          cumulative {
            capacity { vars: 21 coeffs: 1 }
            intervals: 22
            demands { offset: 1 }
          }
        }
        constraints {
          enforcement_literal: 22
          linear { vars: 2 vars: 3 coeffs: 1 coeffs: -1 domain: -1 domain: -1 }
        }
        constraints {
          enforcement_literal: 23
          linear { vars: 5 vars: 6 coeffs: 1 coeffs: -1 domain: -1 domain: -1 }
        }
        constraints {
          enforcement_literal: 24
          linear { vars: 8 vars: 9 coeffs: 1 coeffs: -1 domain: -1 domain: -1 }
        }
        objective { vars: 0 coeffs: 1 }
      )pb");
  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, PotentialOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables {
          domain: 2
          domain: 2447234766972268842
          domain: 3535826881723299506
          domain: 4050838349900690071
        }
        variables {
          domain: -2798048574462918627
          domain: 2251799813685248
          domain: 357364299240879354
          domain: 1499017952464168848
        }
        variables { domain: 1 domain: 1 }
        variables { domain: 0 domain: 1 }
        constraints {
          reservoir {
            max_level: 2
            time_exprs { vars: 0 coeffs: 1 }
            time_exprs { vars: 1 coeffs: 1 }
            active_literals: 2
            active_literals: 3
            level_changes { offset: -1 }
            level_changes { offset: 1 }
          }
        }
      )pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, LinearizationOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables {
          domain: 0
          domain: 6
          domain: 1495197974356070066
          domain: 1495197974356070067
        }
        variables { domain: 0 domain: 50 }
        constraints {
          linear {
            vars: 0
            vars: 2
            vars: 3
            coeffs: 2
            coeffs: 4
            coeffs: -1
            domain: -896813501530156794
            domain: 6343756879353628413
            domain: 9223372036854775807
            domain: 9223372036854775807
          }
        }
      )pb");
  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, LinearizationOverflow2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: -1 domain: 0 }
        variables { domain: 0 domain: 5 }
        variables { domain: 0 domain: 5 }
        variables { domain: 2 domain: 8 }
        constraints {
          linear {
            vars: 1
            vars: 2
            coeffs: 1
            coeffs: -1
            domain: 3
            domain: 1387315275818938588
            domain: 9223372036854775807
            domain: 9223372036854775807
          }
        }
      )pb");
  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, IntervalThatCanOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables {
          domain: -2700435943562583052
          domain: 122393683034791
          domain: 1153604922529384902
          domain: 1153604922529384903
        }
        variables { domain: 2 domain: 2 }
        variables {
          domain: 5
          domain: 8198
          domain: 502515202656425278
          domain: 3082664781292582538
        }
        variables { domain: 3 domain: 6 }
        variables { domain: 3 domain: 3 }
        variables { domain: 3 domain: 6 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: 1 }
          }
        }
        assumptions: -1
      )pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::MODEL_INVALID);
}

TEST(PresolveCpModelTest, ProdPotentialOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: -4611686018427387903 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 2 }
        variables { domain: 0 domain: 100 }
        constraints {
          int_prod {
            target { vars: 2 coeffs: 1 }
            exprs { vars: 1 coeffs: 1 }
            exprs { vars: 0 coeffs: 1 }
          }
        }
        assumptions: 0
      )pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, CumulativeCornerCase) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        constraints { cumulative { capacity { vars: 1 coeffs: -1 } } }
        objective { vars: 1 offset: 1 coeffs: 1 }
        solution_hint {}
        assumptions: 1
        assumptions: -1)pb");
  SatParameters params;

  params.set_linearization_level(2);

  params.set_cp_model_presolve(false);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, ProdPotentialOverflow2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: -2547768298502951547 domain: 0 }
        variables { domain: 2 domain: 2 }
        variables { domain: -4611686018427387903 domain: 1 }
        constraints {
          int_prod {
            target { vars: 2 coeffs: 1 }
            exprs { vars: 1 coeffs: 1 }
            exprs { vars: 0 coeffs: 1 }
          }
        }
        solution_hint {})pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, NoOverlap2dBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 6 }
        variables { domain: -2 domain: 19 domain: 2185 domain: 2185 }
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 808 }
        variables { domain: 3 domain: 6 }
        variables { domain: 3 domain: 3 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 4 coeffs: 1 }
            end { vars: 6 coeffs: 1 }
            size { vars: 5 coeffs: 1 }
          }
        }
        constraints { no_overlap_2d { x_intervals: 0 y_intervals: 1 } }
        assumptions: 0
      )pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, NoOverlap2dBug2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 6 }
        variables { domain: -58 domain: 11 domain: 3523 domain: 3524 }
        variables { domain: 0 domain: 6 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: 1 }
          }
        }
        constraints { no_overlap_2d { x_intervals: 0 y_intervals: 0 } }
        assumptions: 0)pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, LinMaxBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 4096 }
        variables { domain: -3013 domain: 516 domain: 680 domain: 681 }
        variables { domain: 1 domain: 4 }
        variables { domain: 1 domain: 4 }
        variables { domain: 1 domain: 4 }
        constraints {
          lin_max {
            exprs { vars: 0 vars: 0 coeffs: 1 coeffs: -1 offset: -4032 }
          }
        })pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, InverseBug2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        variables { domain: 0 domain: 3 }
        constraints {
          lin_max {
            target { vars: 0 coeffs: -1 }
            exprs { vars: 1 coeffs: 1 offset: -1 }
          }
        }
        constraints {
          inverse {
            f_direct: 0
            f_direct: 2
            f_direct: 4
            f_direct: 6
            f_inverse: 1
            f_inverse: 3
            f_inverse: 5
            f_inverse: 7
          }
        })pb");

  SatParameters params;

  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, ElementBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 5 }
        variables { domain: -1298 domain: -1 domain: 4095 domain: 4095 }
        constraints {
          element {
            linear_index { vars: 0 coeffs: 1 }
            linear_target { vars: 1 coeffs: -3 }
            exprs { offset: 1 }
            exprs { offset: 2 }
            exprs { offset: 3 }
            exprs { offset: 4 }
            exprs { offset: 5 }
            exprs { offset: 6 }
          }
        })pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, AutomatonBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: -1 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        constraints {
          automaton {
            final_states: 3
            transition_tail: [ 0, 0, 1, 2, 1, 2 ]
            transition_head: [ 1, 2, 1, 2, 3, 3 ]
            transition_label: [ 0, 1, 0, 1, 1, 0 ]
            exprs { vars: 0 coeffs: 1 }
            exprs { vars: 1 coeffs: 1 }
            exprs { vars: 1 coeffs: -1 }
            exprs { vars: 2 coeffs: 1 }
            exprs { vars: 3 coeffs: 1 }
          }
        })pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, NoOverlapBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 0 }
        variables { domain: 0 domain: 0 }
        variables { domain: 0 domain: 0 }
        variables { domain: 0 domain: 0 }
        variables { domain: 0 domain: 0 }
        variables { domain: 0 domain: 0 domain: 2 domain: 2 }
        constraints {
          enforcement_literal: 0
          interval {
            start {}
            end { vars: 0 coeffs: 1 }
            size { offset: 1 }
          }
        }
        constraints {
          enforcement_literal: 0
          interval {
            start {}
            end {}
            size { vars: 6 coeffs: 1 }
          }
        }
        constraints {
          no_overlap { intervals: 1 intervals: 0 intervals: 1 intervals: 0 }
        })pb");

  SatParameters params;

  params.set_debug_crash_if_presolve_breaks_hint(true);

  // Enable all fancy heuristics.
  params.set_linearization_level(2);
  params.set_use_try_edge_reasoning_in_no_overlap_2d(true);
  params.set_exploit_all_precedences(true);
  params.set_use_hard_precedences_in_cumulative(true);
  params.set_max_num_intervals_for_timetable_edge_finding(1000);
  params.set_use_overload_checker_in_cumulative(true);
  params.set_use_strong_propagation_in_disjunctive(true);
  params.set_use_timetable_edge_finding_in_cumulative(true);
  params.set_max_pairs_pairwise_reasoning_in_no_overlap_2d(50000);
  params.set_use_timetabling_in_no_overlap_2d(true);
  params.set_use_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_area_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_conservative_scale_overload_checker(true);
  params.set_use_dual_scheduling_heuristics(true);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, NoOverlapBug2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: -1558 domain: 2 domain: 2476 domain: 3080 }
        variables { domain: -3998 domain: 5 domain: 3175 domain: 3527 }
        variables { domain: 3 domain: 38 domain: 2329 domain: 2922 }
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 6 }
        variables { domain: 5 domain: 18 domain: 402 domain: 1493 }
        variables { domain: 258 domain: 1534 domain: 2025 domain: 2026 }
        variables { domain: -4096 domain: 1962 domain: 2394 domain: 3458 }
        variables { domain: 3 domain: 3 }
        variables { domain: 3 domain: 6 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: 1 offset: 2 }
          }
        }
        constraints {
          enforcement_literal: 4
          interval {
            start { vars: 5 coeffs: 1 }
            end { vars: 7 coeffs: 1 }
            size { vars: 6 coeffs: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 8 coeffs: 1 }
            end { vars: 10 coeffs: 1 }
            size { vars: 9 coeffs: 1 }
          }
        }
        constraints {
          no_overlap { intervals: 1 intervals: 0 intervals: 1 intervals: 2 }
        }
        constraints { bool_xor { literals: 0 literals: 4 } }
        floating_point_objective { vars: 1 coeffs: 1 offset: 2 }
      )pb");

  SatParameters params;

  params.set_debug_crash_if_presolve_breaks_hint(true);

  // Enable all fancy heuristics.
  params.set_linearization_level(2);
  params.set_use_try_edge_reasoning_in_no_overlap_2d(true);
  params.set_exploit_all_precedences(true);
  params.set_use_hard_precedences_in_cumulative(true);
  params.set_max_num_intervals_for_timetable_edge_finding(1000);
  params.set_use_overload_checker_in_cumulative(true);
  params.set_use_strong_propagation_in_disjunctive(true);
  params.set_use_timetable_edge_finding_in_cumulative(true);
  params.set_max_pairs_pairwise_reasoning_in_no_overlap_2d(50000);
  params.set_use_timetabling_in_no_overlap_2d(true);
  params.set_use_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_area_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_conservative_scale_overload_checker(true);
  params.set_use_dual_scheduling_heuristics(true);

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, LinMaxBug2) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 2 }
        constraints {
          lin_max {
            target { vars: 0 coeffs: -1 }
            exprs { vars: 0 coeffs: -1 offset: -1 }
          }
        })pb");

  SatParameters params;
  params.set_cp_model_presolve(false);

  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, NoOverlap2dBug3) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 6 }
        variables { domain: 3 domain: 3 }
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 6 }
        variables { domain: -3 domain: 3 domain: 3033 domain: 3033 }
        variables { domain: 2 domain: 6 }
        variables { domain: 3 domain: 6 }
        variables { domain: 0 domain: 0 }
        variables { domain: 3 domain: 3 }
        variables { domain: 3 domain: 6 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: 1 offset: 2 }
          }
        }
        constraints {
          enforcement_literal: 4
          interval {
            start { vars: 5 coeffs: 1 }
            end { vars: 7 coeffs: 1 }
            size { vars: 6 coeffs: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 8 coeffs: 1 }
            end { vars: 10 coeffs: 1 }
            size { vars: 9 coeffs: 1 }
          }
        }
        constraints {
          no_overlap_2d {
            x_intervals: 1
            x_intervals: 1
            y_intervals: 1
            y_intervals: 1
          }
        }
        constraints { bool_xor { literals: 0 literals: 4 } })pb");

  SatParameters params;
  params.set_max_time_in_seconds(4.0);
  params.set_debug_crash_if_presolve_breaks_hint(true);

  // Enable all fancy heuristics.
  params.set_linearization_level(2);
  params.set_use_try_edge_reasoning_in_no_overlap_2d(true);
  params.set_exploit_all_precedences(true);
  params.set_use_hard_precedences_in_cumulative(true);
  params.set_max_num_intervals_for_timetable_edge_finding(1000);
  params.set_use_overload_checker_in_cumulative(true);
  params.set_use_strong_propagation_in_disjunctive(true);
  params.set_use_timetable_edge_finding_in_cumulative(true);
  params.set_max_pairs_pairwise_reasoning_in_no_overlap_2d(50000);
  params.set_use_timetabling_in_no_overlap_2d(true);
  params.set_use_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_area_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_conservative_scale_overload_checker(true);
  params.set_use_dual_scheduling_heuristics(true);
  params.set_maximum_regions_to_split_in_disconnected_no_overlap_2d(100);

  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, ObjectiveOverflow) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 1 }
        constraints {
          exactly_one {
            literals: 0
            literals: 1
            literals: 2
            literals: 3
            literals: 4
          }
        }
        objective {
          vars: 1
          vars: 0
          coeffs: 4611686018427387903
          coeffs: -1771410674732262910
        })pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_log_search_progress(true);
  params.set_linearization_level(2);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(StopSolveTest, StopBeforeStart) {
  CpModelProto model_proto;
  AddInterval(0, 2, 4, &model_proto);
  AddInterval(1, 2, 4, &model_proto);
  ConstraintProto* ct = model_proto.add_constraints();
  ct->mutable_cumulative()->add_intervals(0);
  ct->mutable_cumulative()->add_demands()->set_offset(3);
  ct->mutable_cumulative()->add_intervals(1);
  ct->mutable_cumulative()->add_demands()->set_offset(4);
  ct->mutable_cumulative()->mutable_capacity()->set_offset(6);

  Model model;
  StopSearch(&model);
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::UNKNOWN);
}

TEST(PresolveCpModelTest, NoOverlap2DCumulativeRelaxationBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: -1353 domain: 1143 domain: 3041 domain: 3042 }
        variables { domain: -2 domain: 5 domain: 1207 domain: 1207 }
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 1 }
        variables { domain: 2 domain: 6 }
        variables { domain: 2 domain: 2 }
        variables { domain: 1 domain: 4096 }
        variables { domain: 1 domain: 4096 }
        variables { domain: 2 domain: 6 }
        variables { domain: 1 domain: 4096 }
        variables { domain: 3 domain: 3 }
        variables { domain: 3 domain: 6 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: 1 offset: 2 }
          }
        }
        constraints {
          enforcement_literal: 4
          interval {
            start { vars: 5 coeffs: 1 }
            end { vars: 7 coeffs: 1 }
            size { vars: 6 coeffs: 1 }
          }
        }
        constraints {
          interval {
            start { vars: 8 coeffs: 1 }
            end { vars: 10 coeffs: 1 }
            size { vars: 9 coeffs: 1 }
          }
        }
        constraints { no_overlap { intervals: 0 intervals: 1 intervals: 2 } }
        constraints { no_overlap_2d {} }
        constraints { no_overlap_2d { x_intervals: 0 y_intervals: 0 } }
        objective { vars: 0 vars: 1 coeffs: -1 coeffs: -3237 }
        search_strategy {
          variables: 1
          domain_reduction_strategy: SELECT_UPPER_HALF
        })pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  params.set_use_timetabling_in_no_overlap_2d(true);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.inner_objective_lower_bound(), -9846954);
}

TEST(PresolveCpModelTest, ReservoirBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 3 domain: 2805 domain: 2923 domain: 2923 }
        variables { domain: 0 domain: 0 }
        variables { domain: 1 domain: 1 }
        variables { domain: 0 domain: 1 }
        constraints {
          reservoir {
            max_level: 2
            time_exprs { vars: 0 coeffs: 1 }
            time_exprs { vars: 1 coeffs: 1 }
            active_literals: 2
            active_literals: 3
            level_changes { offset: -1 }
            level_changes { offset: 1 }
          }
        }
        search_strategy { variables: 0 }
        search_strategy {
          variable_selection_strategy: CHOOSE_MIN_DOMAIN_SIZE
          domain_reduction_strategy: SELECT_MAX_VALUE
          exprs { offset: -1 }
        }
        solution_hint {})pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, IntModBug) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: -1264 domain: -1 domain: 4095 domain: 4096 }
        constraints {
          int_mod {
            target { offset: 1 }
            exprs { vars: 0 coeffs: 1 offset: -2607 }
            exprs { offset: 2780 }
          }
        })pb");

  SatParameters params;

  params.set_cp_model_presolve(false);
  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, CumulativeBugWithEmptyInterval) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: [ 0, 37 ] }
        variables { domain: [ 1, 1 ] }
        variables { domain: [ 2, 2 ] }
        variables { domain: [ 0, 1 ] }
        variables { domain: [ 0, 0 ] }
        variables { domain: [ 1, 4 ] }
        variables { domain: [ 1, 4 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 0, 0 ] }
        variables { domain: [ 0, 3 ] }
        variables { domain: [ 1, 1 ] }
        constraints {
          interval {
            start { offset: 2 }
            end { offset: 2 }
            size {}
          }
        }
        constraints {
          interval {
            start {
              vars: [ 3 ]
              coeffs: [ 1 ]
              offset: 1
            }
            end {
              vars: [ 5 ]
              coeffs: [ 1 ]
            }
            size {
              vars: [ 7 ]
              coeffs: [ 1 ]
            }
          }
        }
        constraints {
          interval {
            start { offset: 1 }
            end {
              vars: [ 6 ]
              coeffs: [ 1 ]
            }
            size {
              vars: [ 6 ]
              coeffs: [ 1 ]
              offset: -1
            }
          }
        }
        constraints {
          linear {
            vars: [ 3, 5 ]
            coeffs: [ -1, 1 ]
            domain: [ 1, 4 ]
          }
        }
        constraints {
          linear {
            vars: [ 3, 5, 7 ]
            coeffs: [ 1, -1, 1 ]
            domain: [ -1, -1 ]
          }
        }
        constraints {
          linear {
            vars: [ 10, 5 ]
            coeffs: [ -1, 1 ]
            domain: [ 1, 4 ]
          }
        }
        constraints {
          linear {
            vars: [ 3, 6 ]
            coeffs: [ -1, 1 ]
            domain: [ 1, 4 ]
          }
        }
        constraints {
          linear {
            vars: [ 0, 9 ]
            coeffs: [ -1, 2 ]
            domain: [ -37, 0 ]
          }
        }
        constraints {
          cumulative {
            capacity {
              vars: [ 9 ]
              coeffs: [ 1 ]
            }
            intervals: [ 0, 1, 2 ]
            demands { offset: 1 }
            demands { offset: 1 }
            demands { offset: 2 }
          }
        }
        objective {
          vars: [ 0, 6 ]
          scaling_factor: 1
          coeffs: [ 1, -1 ]
          domain: [ -4, 37 ]
        }
      )pb");

  SatParameters params;
  params.set_max_time_in_seconds(4.0);
  params.set_debug_crash_if_presolve_breaks_hint(true);

  params.set_log_search_progress(true);
  params.set_linearization_level(2);

  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.inner_objective_lower_bound(), 0);

  params.set_cp_model_presolve(false);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.inner_objective_lower_bound(), 0);
}

TEST(PresolveCpModelTest, CumulativeBug3) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 1 domain: 1 }
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 6 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: -1 offset: 2 }
          }
        }
        constraints {
          cumulative {
            capacity { offset: 1 }
            intervals: 0
            demands { vars: 1 coeffs: 1 offset: 1 }
          }
        }
        objective { vars: 1 coeffs: -1 }
      )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);

  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.inner_objective_lower_bound(), -6);

  params.set_cp_model_presolve(false);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.inner_objective_lower_bound(), -6);
}

TEST(PresolveCpModelTest, CumulativeBug4) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        variables { domain: 0 domain: 1 }
        variables { domain: 0 domain: 6 }
        variables { domain: -920 domain: -15 domain: 1540 domain: 1692 }
        variables { domain: 0 domain: 6 }
        variables { domain: 0 domain: 1 }
        variables { domain: -1 domain: 0 domain: 4096 domain: 4096 }
        variables { domain: 0 domain: 17 }
        constraints {
          enforcement_literal: 0
          interval {
            start { vars: 1 coeffs: 1 }
            end { vars: 3 coeffs: 1 }
            size { vars: 2 coeffs: 1 offset: 2 }
          }
        }
        constraints {
          enforcement_literal: 4
          interval {
            start { offset: 2 }
            end { vars: 6 coeffs: 1 }
            size { vars: 5 coeffs: 1 }
          }
        }
        constraints {
          cumulative {
            capacity { offset: 1 }
            intervals: 1
            demands { vars: 6 coeffs: 1 }
          }
        }
        constraints { bool_xor { literals: 0 literals: 4 } }
      )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  params.set_cp_model_presolve(false);
  params.set_cp_model_probing_level(0);
  params.set_symmetry_level(0);

  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  params.set_cp_model_presolve(true);
  response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
