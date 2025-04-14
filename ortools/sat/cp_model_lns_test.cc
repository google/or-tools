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

#include "ortools/sat/cp_model_lns.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_solver_helpers.h"
#include "ortools/sat/cp_model_test_utils.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::EqualsProto;

TEST(NeighborhoodGeneratorHelperTest, ActiveVariables) {
  // Generate a random problem and find a solution.
  const int num_variables = 1000;
  const CpModelProto proto =
      Random3SatProblem(num_variables, /*proportion_of_constraints=*/2.0);
  Model model;
  const CpSolverResponse solution = SolveCpModel(proto, &model);
  EXPECT_EQ(solution.status(), CpSolverStatus::OPTIMAL);

  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  SharedBoundsManager shared_bounds_manager(proto);
  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit, &shared_bounds_manager);
  CHECK_EQ(helper.NumActiveVariables(), num_variables);

  // Fix two variables at zero.
  shared_bounds_manager.ReportPotentialNewBounds("name", {1, 2}, {0, 0},
                                                 {0, 0});
  shared_bounds_manager.Synchronize();
  CHECK_EQ(helper.NumActiveVariables(), num_variables);

  // Synchronize().
  helper.Synchronize();
  CHECK_EQ(helper.NumActiveVariables(), num_variables - 2);
}

template <class T>
class GeneratorTest : public ::testing::Test {};

// Wrapper for LocalBranchingLpBasedNeighborhoodGenerator with fixed callback.
class LocalBranchingLpBasedNeighborhoodGeneratorWrapper
    : public LocalBranchingLpBasedNeighborhoodGenerator {
 public:
  explicit LocalBranchingLpBasedNeighborhoodGeneratorWrapper(
      NeighborhoodGeneratorHelper const* helper, absl::string_view name)
      : LocalBranchingLpBasedNeighborhoodGenerator(helper, name, nullptr,
                                                   &shared_) {}

 private:
  Model m_;
  CpModelProto model_proto_;
  SharedClasses shared_ = SharedClasses{&model_proto_, &m_};
};

using GeneratorTypes =
    ::testing::Types<RelaxRandomVariablesGenerator,
                     VariableGraphNeighborhoodGenerator,
                     ConstraintGraphNeighborhoodGenerator,
                     LocalBranchingLpBasedNeighborhoodGeneratorWrapper>;
TYPED_TEST_SUITE(GeneratorTest, GeneratorTypes);

TYPED_TEST(GeneratorTest, BasicContract) {
  // Generate a random problem and find a solution.
  const int num_variables = 1000;
  const CpModelProto proto =
      Random3SatProblem(num_variables, /*proportion_of_constraints=*/3.0);
  Model model;
  const CpSolverResponse solution = SolveCpModel(proto, &model);
  EXPECT_EQ(solution.status(), CpSolverStatus::OPTIMAL);

  SatParameters params;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  shared_response_manager->InitializeObjective(proto);
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  TypeParam generator(&helper, "test");
  random_engine_t random;
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 0.2;
  const Neighborhood neighborhood = generator.Generate(solution, data, random);
  EXPECT_TRUE(neighborhood.is_generated);
  const CpModelProto& local_variables = neighborhood.delta;
  EXPECT_TRUE(neighborhood.is_reduced);

  // Expect that around 0.8 * num_variables are fixed and that they are fixed to
  // their value in solution.
  int num_fixed = 0;
  for (int i = 0; i < num_variables; ++i) {
    if (local_variables.variables(i).domain(0) ==
        local_variables.variables(i).domain(1)) {
      ++num_fixed;
      EXPECT_EQ(local_variables.variables(i).domain(0), solution.solution(i));
    }
  }
  EXPECT_EQ(num_fixed, 0.8 * num_variables);
}

TYPED_TEST(GeneratorTest, NoReduction) {
  // Generate a random problem and find a solution.
  const int num_variables = 100;
  const CpModelProto proto = Random3SatProblem(num_variables, 2);
  Model model;
  const CpSolverResponse solution = SolveCpModel(proto, &model);
  EXPECT_EQ(solution.status(), CpSolverStatus::OPTIMAL);

  SatParameters params;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  TypeParam generator(&helper, "test");
  random_engine_t random;
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 1.0;
  const Neighborhood neighborhood = generator.Generate(solution, data, random);

  EXPECT_TRUE(neighborhood.is_generated);

  // In these cases we might stay inside a connected components.
  if constexpr (std::is_same_v<TypeParam, VariableGraphNeighborhoodGenerator> ||
                std::is_same_v<TypeParam,
                               ConstraintGraphNeighborhoodGenerator>) {
    return;
  }

  EXPECT_FALSE(neighborhood.is_reduced);
  EXPECT_EQ(neighborhood.delta.variables_size(), proto.variables_size());
  for (int v = 0; v < proto.variables_size(); ++v) {
    EXPECT_THAT(proto.variables(v),
                EqualsProto(neighborhood.delta.variables(v)));
  }
}

TYPED_TEST(GeneratorTest, ReadyToGenerate) {
  // Generate a random problem and find a solution.
  const int num_variables = 10;
  const CpModelProto proto = RandomLinearProblem(num_variables, 1);
  SatParameters params;
  params.set_stop_after_first_solution(true);
  const CpSolverResponse solution = SolveWithParameters(proto, params);

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  shared_response_manager->InitializeObjective(proto);
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, model.GetOrCreate<SatParameters>(),
                                     shared_response_manager, &time_limit);
  TypeParam generator(&helper, "test");
  EXPECT_FALSE(generator.ReadyToGenerate());
  shared_response_manager->NewSolution(solution.solution(),
                                       solution.solution_info(), &model);
  shared_response_manager->MutableSolutionsRepository()->Synchronize();
  EXPECT_EQ(1, shared_response_manager->SolutionsRepository().NumSolutions());
  EXPECT_TRUE(generator.ReadyToGenerate());
}

TYPED_TEST(GeneratorTest, ModelWithoutConstraintDoNotCrash) {
  CpModelProto proto;
  for (int i = 0; i < 10; ++i) {
    IntegerVariableProto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }

  Model model;
  const CpSolverResponse solution = SolveCpModel(proto, &model);
  EXPECT_EQ(solution.status(), CpSolverStatus::OPTIMAL);

  SatParameters params;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  TypeParam generator(&helper, "test");
  random_engine_t random;
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 0.2;
  const Neighborhood neighborhood = generator.Generate(solution, data, random);
  if (neighborhood.is_generated) {
    const CpModelProto& local_variables = neighborhood.delta;
    EXPECT_EQ(local_variables.variables_size(), 10);
  }
}

TEST(GeneratorTest, UCBScore) {
  CpModelProto proto;
  IntegerVariableProto* var = proto.add_variables();
  var->add_domain(0);
  var->add_domain(1);

  Model model;
  const CpSolverResponse solution = SolveCpModel(proto, &model);
  EXPECT_EQ(solution.status(), CpSolverStatus::OPTIMAL);

  SatParameters params;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  RelaxRandomVariablesGenerator generator(&helper, "test");
  EXPECT_EQ(generator.GetUCBScore(/*total_num_calls=*/0),
            std::numeric_limits<double>::infinity());

  // Add some data.
  for (int i = 0; i < 100; ++i) {
    NeighborhoodGenerator::SolveData data;
    data.initial_best_objective = IntegerValue(55);
    data.new_objective = IntegerValue(0);
    data.deterministic_time = 9.0;
    generator.AddSolveData(data);
  }
  generator.Synchronize();

  EXPECT_NEAR(5.8255, generator.GetUCBScore(/*total_num_calls=*/200), 1e-4);
}

TEST(RelaxationInducedNeighborhoodGeneratorTest, NoNeighborhoodGeneratedRINS) {
  CpModelProto proto;
  IntegerVariableProto* var = proto.add_variables();
  var->add_domain(0);
  var->add_domain(1);
  proto.mutable_objective()->add_vars(0);
  proto.mutable_objective()->add_coeffs(1);

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  shared_response_manager->InitializeObjective(proto);
  SharedLPSolutionRepository lp_solutions(/*num_solutions_to_keep=*/1);
  SharedIncompleteSolutionManager incomplete_solutions;

  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  RelaxationInducedNeighborhoodGenerator generator(
      &helper, shared_response_manager, &lp_solutions, &incomplete_solutions,
      "rins");

  // No solution is recorded.
  EXPECT_FALSE(generator.ReadyToGenerate());

  CpSolverResponse solution;
  solution.add_solution(0);
  shared_response_manager->NewSolution(solution.solution(),
                                       solution.solution_info(), &model);
  shared_response_manager->MutableSolutionsRepository()->Synchronize();
  lp_solutions.NewLPSolution({0.0});
  lp_solutions.Synchronize();

  EXPECT_TRUE(generator.ReadyToGenerate());
  random_engine_t random;
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 0.2;
  const Neighborhood neighborhood2 = generator.Generate(solution, data, random);
  EXPECT_TRUE(neighborhood2.is_generated);
}

TEST(RelaxationInducedNeighborhoodGeneratorTest, NoNeighborhoodGeneratedRENS) {
  CpModelProto proto;
  IntegerVariableProto* var = proto.add_variables();
  var->add_domain(0);
  var->add_domain(1);
  proto.mutable_objective()->add_vars(0);
  proto.mutable_objective()->add_coeffs(1);

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  SharedLPSolutionRepository lp_solutions(/*num_solutions_to_keep=*/1);
  SharedIncompleteSolutionManager incomplete_solutions;

  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  RelaxationInducedNeighborhoodGenerator generator(
      &helper, /*response_manager=*/nullptr, &lp_solutions,
      &incomplete_solutions, "rens");

  // No solution is recorded.
  EXPECT_FALSE(generator.ReadyToGenerate());

  lp_solutions.NewLPSolution({0.0});
  lp_solutions.Synchronize();

  EXPECT_TRUE(generator.ReadyToGenerate());
  random_engine_t random;
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 0.2;
  const Neighborhood neighborhood2 = generator.Generate({}, data, random);
  EXPECT_TRUE(neighborhood2.is_generated);
}

TEST(RelaxationInducedNeighborhoodGeneratorTest, ValueOutOfDomain) {
  // NOTE(user): This can only happen in RENS.
  CpModelProto proto;
  IntegerVariableProto* var = proto.add_variables();
  var->add_domain(0);
  var->add_domain(2);
  var->add_domain(5);
  var->add_domain(7);
  proto.mutable_objective()->add_vars(0);
  proto.mutable_objective()->add_coeffs(1);

  Model model;
  SharedLPSolutionRepository lp_solutions(/*num_solutions_to_keep=*/1);
  SharedIncompleteSolutionManager incomplete_solutions;
  lp_solutions.NewLPSolution({3.0});
  lp_solutions.Synchronize();

  SatParameters params;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  RelaxationInducedNeighborhoodGenerator generator(
      &helper, /*response_manager=*/nullptr, &lp_solutions,
      &incomplete_solutions, "rens");

  EXPECT_TRUE(generator.ReadyToGenerate());

  // A value outside the domain cause the neighborhood to not be generated.
  // Note that none of the arguments are currently used by this generator.
  random_engine_t random;
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 0.0;
  EXPECT_FALSE(generator.Generate({}, data, random).is_generated);

  // A value in the domain is ok.
  lp_solutions.NewLPSolution({1.0});
  lp_solutions.Synchronize();

  EXPECT_TRUE(generator.ReadyToGenerate());
  EXPECT_TRUE(generator.Generate({}, data, random).is_generated);
}

TEST(LocalBranchingNeighborhoodGeneratorTest,
     SimpleLocalBranchingNeighborhood) {
  // max 2x + 2y + 3z
  //     x = y
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 1 }
    variables { name: 'y' domain: 0 domain: 1 }
    variables { name: 'z' domain: 0 domain: 1 }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ -2, -2, -3 ]
    }
  )pb");
  const CpSolverResponse initial_solution = ParseTestProto(R"pb(
    solution: [ 0, 0, 0 ])pb");

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  SharedBoundsManager shared_bounds_manager(proto);
  shared_response_manager->InitializeObjective(proto);

  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  LocalBranchingLpBasedNeighborhoodGeneratorWrapper generator(&helper, "test");
  random_engine_t random;

  // 1-variable neighborhood (difficulty in (0, 1/3])
  // Expected: Local branching LP solution: (0, 0, 1) -> fix x, y; relax z
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 0.3;
  const Neighborhood one_var_neighborhood =
      generator.Generate(initial_solution, data, random);
  EXPECT_TRUE(one_var_neighborhood.is_generated);
  EXPECT_THAT(one_var_neighborhood.delta.variables(0).domain(),
              testing::ElementsAre(0, 0));
  EXPECT_THAT(one_var_neighborhood.delta.variables(1).domain(),
              testing::ElementsAre(0, 0));
  EXPECT_THAT(one_var_neighborhood.delta.variables(2).domain(),
              testing::ElementsAre(0, 1));

  // 2-variable neighborhood (difficulty in (1/3, 2/3])
  // Expected: Local branching LP solution: (1, 1, 0) -> fix z; relax x, y
  data.difficulty = 0.6;
  const Neighborhood two_var_neighborhood =
      generator.Generate(initial_solution, data, random);
  EXPECT_TRUE(two_var_neighborhood.is_generated);
  EXPECT_THAT(two_var_neighborhood.delta.variables(0).domain(),
              testing::ElementsAre(0, 1));
  EXPECT_THAT(two_var_neighborhood.delta.variables(1).domain(),
              testing::ElementsAre(0, 1));
  EXPECT_THAT(two_var_neighborhood.delta.variables(2).domain(),
              testing::ElementsAre(0, 0));

  // 3-variable neighborhood (difficulty in (2/3, 1])
  // Expected: Local branching LP solution: (1, 1, 1) -> relax x, y, z
  data.difficulty = 1.0;
  const Neighborhood three_var_neighborhood =
      generator.Generate(initial_solution, data, random);
  EXPECT_TRUE(three_var_neighborhood.is_generated);
  EXPECT_THAT(three_var_neighborhood.delta.variables(0).domain(),
              testing::ElementsAre(0, 1));
  EXPECT_THAT(three_var_neighborhood.delta.variables(1).domain(),
              testing::ElementsAre(0, 1));
  EXPECT_THAT(three_var_neighborhood.delta.variables(2).domain(),
              testing::ElementsAre(0, 1));
}

TEST(LocalBranchingNeighborhoodGeneratorTest,
     GeneralIntegerVariablesAreAlwaysRelaxed) {
  // min x
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 1 }
    variables { name: 'y' domain: 0 domain: 1 }
    variables { name: 'z' domain: 0 domain: 2 }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
    }
  )pb");
  const CpSolverResponse initial_solution = ParseTestProto(R"pb(
    solution: [ 1, 1, 2 ])pb");

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  SharedBoundsManager shared_bounds_manager(proto);
  shared_response_manager->InitializeObjective(proto);

  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit);
  LocalBranchingLpBasedNeighborhoodGeneratorWrapper generator(&helper, "test");
  absl::BitGen random;

  // For a difficulty of 1.0, all variables should be relaxed, binary or not.
  NeighborhoodGenerator::SolveData data;
  data.difficulty = 1.0;
  const Neighborhood neighborhood =
      generator.Generate(initial_solution, data, random);
  EXPECT_TRUE(neighborhood.is_generated);
  EXPECT_THAT(neighborhood.delta.variables(0).domain(),
              testing::ElementsAre(0, 1));
  EXPECT_THAT(neighborhood.delta.variables(1).domain(),
              testing::ElementsAre(0, 1));
  EXPECT_THAT(neighborhood.delta.variables(2).domain(),
              testing::ElementsAre(0, 2));

  // A difficulty of 0.5 should mean that half of the binary variables are
  // relaxed, and any other variables should always be relaxed. Variable x
  // should be selected to relax since the objective is to minimize x.
  data.difficulty = 0.5;
  const Neighborhood neighborhood_fix_one =
      generator.Generate(initial_solution, data, random);
  EXPECT_TRUE(neighborhood_fix_one.is_generated);
  // x should be relaxed.
  EXPECT_THAT(neighborhood_fix_one.delta.variables(0).domain(),
              testing::ElementsAre(0, 1));
  // One of either y or z should be relaxed.
  EXPECT_THAT(
      absl::MakeConstSpan({neighborhood_fix_one.delta.variables(1).domain(),
                           neighborhood_fix_one.delta.variables(2).domain()}),
      testing::AnyOf(testing::ElementsAre(testing::ElementsAre(1, 1),
                                          testing::ElementsAre(0, 2)),
                     testing::ElementsAre(testing::ElementsAre(0, 1),
                                          testing::ElementsAre(2, 2))));
}

TEST(NeighborhoodGeneratorHelperTest, BoundAreUpdatedOnSynchronize) {
  CpModelProto proto;
  IntegerVariableProto* var = proto.add_variables();
  var->add_domain(0);
  var->add_domain(10);

  Model model;
  const CpSolverResponse solution = SolveCpModel(proto, &model);
  EXPECT_EQ(solution.status(), CpSolverStatus::OPTIMAL);

  SharedBoundsManager shared_bounds_manager(proto);

  SatParameters params;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit, &shared_bounds_manager);

  // Initial bound.
  EXPECT_EQ(ReadDomainFromProto(helper.FullNeighborhood().delta.variables(0)),
            Domain(0, 10));

  // Report new bounds.
  // Note that the id has to be different than the one from the helper.
  shared_bounds_manager.ReportPotentialNewBounds("auto", {0}, {4}, {4});
  shared_bounds_manager.Synchronize();

  // No change since not synchronized.
  {
    absl::ReaderMutexLock lock(&helper.graph_mutex_);
    EXPECT_TRUE(helper.IsActive(0));
  }
  EXPECT_EQ(ReadDomainFromProto(helper.FullNeighborhood().delta.variables(0)),
            Domain(0, 10));
  helper.Synchronize();

  // New bound are properly there.
  {
    absl::ReaderMutexLock lock(&helper.graph_mutex_);
    EXPECT_FALSE(helper.IsActive(0));
  }
  EXPECT_EQ(ReadDomainFromProto(helper.FullNeighborhood().delta.variables(0)),
            Domain(4, 4));
}

TEST(NeighborhoodGeneratorHelperTest, FixGivenVariables) {
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 10 }
    variables { name: 'z' domain: 0 domain: 10 }
    constraints {
      linear {
        vars: [ 0, 0, 1, 2 ]
        coeffs: [ 1, -1, 1, 1 ]
        domain: [ 5, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, 2 ]
        domain: [ 3, 3 ]
      }
    }
  )pb");
  const CpSolverResponse response = ParseTestProto(R"pb(
    solution: [ 2, 3, 4 ])pb");

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  SharedBoundsManager shared_bounds_manager(proto);

  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit, &shared_bounds_manager);

  Bitset64<int> variables_to_fix(helper.NumVariables());
  for (const int var : {2, 0}) variables_to_fix.Set(var);
  const Neighborhood n = helper.FixGivenVariables(response, variables_to_fix);
  const CpModelProto expected_output =
      DEBUG_MODE ? ParseTestProto(R"pb(
        variables { name: "x" domain: 2 domain: 2 }
        variables { name: "y" domain: 0 domain: 10 }
        variables { name: "z" domain: 4 domain: 4 }
        solution_hint { vars: 1 values: 3 }
      )pb")
                 : ParseTestProto(R"pb(
                     variables { domain: 2 domain: 2 }
                     variables { domain: 0 domain: 10 }
                     variables { domain: 4 domain: 4 }
                     solution_hint { vars: 1 values: 3 }
                   )pb");
  EXPECT_THAT(n.delta, testing::EqualsProto(expected_output));
}

TEST(NeighborhoodGeneratorHelperTest, InfeasibleCopyAndFixVariables) {
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    constraints {
      linear {
        vars: [ 0, 0, 1, 2 ]
        coeffs: [ 1, -1, 1, 1 ]
        domain: [ 5, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, 2 ]
        domain: [ 3, 3 ]
      }
    }
  )pb");
  const CpSolverResponse response = ParseTestProto(R"pb(
    solution: [ 2, 3, -2 ])pb");

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  SharedBoundsManager shared_bounds_manager(proto);

  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit, &shared_bounds_manager);

  Bitset64<int> variables_to_fix(helper.NumVariables());
  for (const int var : {2, 0}) variables_to_fix.Set(var);
  const Neighborhood n = helper.FixGivenVariables(response, variables_to_fix);
  const CpModelProto expected_output =
      ParseTestProto(R"pb(variables { domain: [ 2, 2 ] }
                          variables { domain: [ 0, 10 ] }
                          variables { domain: [ 0, 0 ] }
                          solution_hint {
                            vars: [ 1 ]
                            values: [ 3 ]
                          }
      )pb");
  EXPECT_THAT(n.delta, testing::EqualsProto(expected_output));
}

TEST(NeighborhoodGeneratorHelperTest, GetSchedulingPrecedences) {
  const CpModelProto proto = ParseTestProto(R"pb(
    variables { name: 'a' domain: 0 domain: 10 }
    variables { name: 'b' domain: 0 domain: 10 }
    variables { name: 'c' domain: 0 domain: 10 }
    variables { name: 'd' domain: 0 domain: 10 }
    variables { name: 'e' domain: 0 domain: 10 }
    variables { name: 'f' domain: 0 domain: 10 }
    variables { name: 'g' domain: 0 domain: 10 }
    variables { name: 'h' domain: 0 domain: 10 }
    variables { name: 'i' domain: 0 domain: 10 }
    variables { name: 'j' domain: 0 domain: 10 }
    variables { name: 'k' domain: 0 domain: 10 }
    variables { name: 'l' domain: 0 domain: 10 }
    variables { name: 'm' domain: 0 domain: 10 }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size: { offset: 3 }
        end: { vars: 0 coeffs: 1 offset: 3 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size: { offset: 2 }
        end: { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size: { offset: 1 }
        end: { vars: 2 coeffs: 1 offset: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size: { offset: 1 }
        end: { vars: 3 coeffs: 1 offset: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        size: { offset: 2 }
        end: { vars: 4 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        size: { offset: 2 }
        end: { vars: 5 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        size: { offset: 4 }
        end: { vars: 6 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 7 coeffs: 1 }
        size: { offset: 1 }
        end: { vars: 7 coeffs: 1 offset: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 8 coeffs: 1 }
        size: { offset: 2 }
        end: { vars: 8 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 9 coeffs: 1 }
        size: { offset: 2 }
        end: { vars: 9 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 10 coeffs: 1 }
        size: { offset: 1 }
        end: { vars: 10 coeffs: 1 offset: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 11 coeffs: 1 }
        size: { offset: 2 }
        end: { vars: 11 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 12 coeffs: 1 }
        size: { offset: 2 }
        end: { vars: 12 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ],
        demands { offset: 3 }
        demands { offset: 1 }
        demands { offset: 2 }
        demands { offset: 3 }
        demands { offset: 2 }
        demands { offset: 2 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 3 }
        demands { offset: 3 }
        demands { offset: 1 }
        demands { offset: 4 }
        capacity: { offset: 5 }
      }
    }
  )pb");
  const CpSolverResponse initial_solution = ParseTestProto(R"pb(
    solution: [ 0, 0, 2, 3, 3, 4, 5, 5, 5, 6, 8, 8, 9 ])pb");

  Model model;
  auto* shared_response_manager = model.GetOrCreate<SharedResponseManager>();
  SharedBoundsManager shared_bounds_manager(proto);

  SatParameters params;
  Model main_model;
  ModelSharedTimeLimit time_limit(&main_model);
  NeighborhoodGeneratorHelper helper(&proto, &params, shared_response_manager,
                                     &time_limit, &shared_bounds_manager);
  random_engine_t random;
  const std::vector<std::pair<int, int>> precedences =
      helper.GetSchedulingPrecedences({}, initial_solution, random);
  EXPECT_EQ(13, precedences.size());
  const std::vector<std::pair<int, int>> expected_precedences = {
      {0, 3}, {1, 2},  {2, 4}, {3, 5},  {3, 7},  {4, 6},  {4, 8},
      {5, 9}, {6, 12}, {7, 9}, {8, 11}, {9, 10}, {10, 12}};
  EXPECT_EQ(precedences, expected_precedences);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
