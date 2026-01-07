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

#include "ortools/sat/cp_model_presolve.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::EqualsProto;

MATCHER_P(ModelEqualsIgnoringConstraintsOrder, expected, "") {
  CpModelProto arg_no_ct = arg;
  arg_no_ct.clear_constraints();
  CpModelProto expected_no_ct = expected;
  expected_no_ct.clear_constraints();
  if (!ExplainMatchResult(EqualsProto(expected_no_ct), arg_no_ct,
                          result_listener)) {
    return false;
  }
  std::vector<testing::Matcher<ConstraintProto>> expected_constraints;
  for (const ConstraintProto& constraint : expected.constraints()) {
    expected_constraints.push_back(EqualsProto(constraint));
  }
  return ExplainMatchResult(
      testing::UnorderedElementsAreArray(expected_constraints),
      arg.constraints(), result_listener);
}

std::vector<int> RandomPermutation(int num_variables, absl::BitGenRef random) {
  std::vector<int> permutation(num_variables);
  std::iota(permutation.begin(), permutation.end(), 0);
  std::shuffle(permutation.begin(), permutation.end(), random);
  return permutation;
}

// Generate a triangular clause system with a known random solution, and fix the
// "singleton" variables so that the full solution can be found by pure
// propagation.
//
// TODO(user): do the same with a linear system.
CpModelProto RandomTrivialSatProblem(int num_variables,
                                     absl::BitGenRef random) {
  CpModelProto result;
  result.set_name("Random trivial SAT");
  std::vector<int> solution_literals;
  for (int i = 0; i < num_variables; ++i) {
    solution_literals.push_back(absl::Bernoulli(random, 1.0 / 2) ? i : -i - 1);
    IntegerVariableProto* var = result.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  const std::vector<int> perm_a = RandomPermutation(num_variables, random);
  const std::vector<int> perm_b = RandomPermutation(num_variables, random);
  for (int i = 0; i < num_variables; ++i) {
    ConstraintProto* ct = result.add_constraints();
    for (int j = 0; j <= perm_a[i]; ++j) {
      ct->mutable_bool_or()->add_literals(solution_literals[perm_b[j]]);
    }
  }
  return result;
}

CpModelProto PresolveForTest(
    CpModelProto initial_model, SatParameters extra_params = SatParameters(),
    CpSolverStatus expected_status = CpSolverStatus::UNKNOWN) {
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  std::vector<int> mapping;
  Model model;
  model.GetOrCreate<SolverLogger>()->EnableLogging(true);
  model.GetOrCreate<SolverLogger>()->SetLogToStdOut(true);
  auto* params = model.GetOrCreate<SatParameters>();
  params->set_permute_variable_randomly(false);
  params->set_cp_model_probing_level(0);
  params->set_convert_intervals(false);
  params->MergeFrom(extra_params);
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpModelPresolver presolver(&context, &mapping);
  EXPECT_EQ(presolver.Presolve(), expected_status);
  return presolved_model;
}

// This expects the presolve to remove everything and return the mapping model.
CpModelProto GetMappingModel(CpModelProto initial_model,
                             SatParameters extra_params = SatParameters()) {
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  std::vector<int> mapping;
  Model model;
  auto* params = model.GetOrCreate<SatParameters>();
  params->set_permute_variable_randomly(false);
  params->set_cp_model_probing_level(0);
  params->set_convert_intervals(false);
  params->MergeFrom(extra_params);
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpModelPresolver presolver(&context, &mapping);
  presolver.Presolve();
  EXPECT_THAT(CpModelProto(), testing::EqualsProto(presolved_model));
  return mapping_model;
}

// Return a proto with reduced domain after presolve.
CpModelProto GetReducedDomains(CpModelProto initial_model) {
  const int num_vars = initial_model.variables().size();
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  std::vector<int> mapping;
  Model model;
  auto* params = model.GetOrCreate<SatParameters>();
  params->set_keep_all_feasible_solutions_in_presolve(true);
  params->set_permute_variable_randomly(false);
  params->set_cp_model_probing_level(0);
  params->set_convert_intervals(false);
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpModelPresolver presolver(&context, &mapping);
  presolver.Presolve();

  // Only keep variable domain, and erase extra ones.
  mapping_model.clear_constraints();
  mapping_model.mutable_variables()->DeleteSubrange(
      num_vars, mapping_model.mutable_variables()->size() - num_vars);
  return mapping_model;
}

void ExpectInfeasibleDuringPresolve(CpModelProto initial_model) {
  PresolveForTest(initial_model, SatParameters(), CpSolverStatus::INFEASIBLE);
}

CpModelProto PresolveOneConstraint(const CpModelProto& initial_model,
                                   const int constraint_index) {
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  std::vector<int> mapping;
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpModelPresolver presolver(&context, &mapping);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();
  presolver.PresolveOneConstraint(constraint_index);
  presolver.RemoveEmptyConstraints();
  for (int i = 0; i < presolved_model.variables_size(); ++i) {
    FillDomainInProto(context.DomainOf(i),
                      presolved_model.mutable_variables(i));
  }
  return presolved_model;
}

TEST(PresolveCpModelTest, BoolAndWithDuplicate) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_and { literals: [ 2, 3, 4 ] }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 2, 1, 0 ]
      bool_and { literals: [ 3, 4 ] }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BoolAndWithNegatedDuplicate) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_and { literals: [ -3, 3, 4 ] }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -3, -2, -1 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EmptyPresolvedProblem) {
  std::mt19937 random(12345);
  const CpModelProto initial_model = RandomTrivialSatProblem(100, random);
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  Model model;
  std::vector<int> mapping;
  PresolveContext context(&model, &presolved_model, &mapping_model);
  PresolveCpModel(&context, &mapping);
  EXPECT_EQ(presolved_model.variables_size(), 0);
  EXPECT_TRUE(mapping.empty());
  {
    Model tmp_model;
    tmp_model.Add(NewSatParameters("cp_model_presolve:false"));
    const CpSolverResponse r = SolveCpModel(presolved_model, &tmp_model);
    EXPECT_EQ(r.status(), CpSolverStatus::OPTIMAL);
  }

  model.Add(NewSatParameters("cp_model_presolve:false"));
  const CpSolverResponse response = SolveCpModel(mapping_model, &model);
  std::vector<int64_t> solution(response.solution().begin(),
                                response.solution().end());
  EXPECT_TRUE(SolutionIsFeasible(initial_model, solution));
}

TEST(PresolveCpModelTest, SimplifyRemovableConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    name: "celar"
    variables { domain: [ 16, 792 ] }
    variables { domain: [ 16, 792 ] }
    variables { domain: [ 16, 792 ] }
    variables { domain: [ -776, 776 ] }
    variables { domain: [ 0, 776 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -776, 776 ] }
    variables { domain: [ 0, 776 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -238, 238 ] }
    variables { domain: [ 238, 238 ] }
    constraints {
      name: "int_lin_eq"
      linear {
        vars: 0
        vars: 1
        vars: 3
        coeffs: 1
        coeffs: -1
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
    constraints {
      name: "int_abs"
      lin_max {
        target: { vars: 4 coeffs: 1 }
        exprs: { vars: 3 coeffs: 1 }
        exprs: { vars: 3 coeffs: -1 }
      }
    }
    constraints {
      name: "int_le_reif"
      enforcement_literal: 5
      linear { vars: 4 coeffs: 1 domain: -9223372036854775808 domain: 59 }
    }
    constraints {
      name: "int_le_reif (negated)"
      enforcement_literal: -6
      linear { vars: 4 coeffs: 1 domain: 60 domain: 9223372036854775807 }
    }
    constraints {
      name: "int_lin_eq"
      linear {
        vars: 0
        vars: 2
        vars: 6
        coeffs: 1
        coeffs: -1
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
    constraints {
      name: "int_abs"
      lin_max {
        target: { vars: 7 coeffs: 1 }
        exprs: { vars: 6 coeffs: 1 }
        exprs: { vars: 6 coeffs: -1 }
      }
    }
    constraints {
      name: "int_le_reif"
      enforcement_literal: 8
      linear { vars: 7 coeffs: 1 domain: -9223372036854775808 domain: 186 }
    }
    constraints {
      name: "int_le_reif (negated)"
      enforcement_literal: -9
      linear { vars: 7 coeffs: 1 domain: 187 domain: 9223372036854775807 }
    }
    constraints {
      name: "int_lin_eq"
      linear {
        vars: 1
        vars: 2
        vars: 9
        coeffs: 1
        coeffs: -1
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
    constraints {
      name: "int_abs"
      lin_max {
        target: { vars: 10 coeffs: 1 }
        exprs: { vars: 9 coeffs: 1 }
        exprs: { vars: 9 coeffs: -1 }
      }
    }
  )pb");
  // This model is FEASIBLE, but before the CL, trying to solve it was crashing.
  // because of the encoding of the variable was created after the var was
  // marked as removable. It was then in both presolved and mapping models, and
  // the postsolve phase was failing.
  Model model;
  const CpSolverResponse response = SolveCpModel(initial_model, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, BasicLinearConstraintPresolve) {
  // Note(user): I tried a random small problem. Note that the conversion to LP
  // put artificial large bounds to x and y which allow to start the round of
  // propagations that reduces the domains of the variables.
  //
  // When removing z, this is: y = 3 + 2x, 0 <= x + y <= 2 and there is actually
  // only one solution (x = -1). The presolve simplify everything.
  const std::string text_lp =
      "y - 2 x + z = 6;"
      "x + y + z <= 5;"
      "x + y >= 0;"
      "z = 3 ;";
  glop::LinearProgram lp;
  CHECK(ParseLp(text_lp, &lp));
  MPModelProto mp_model;
  LinearProgramToMPModelProto(lp, &mp_model);
  CpModelProto initial_model;
  SolverLogger logger;
  ConvertMPModelProtoToCpModelProto(SatParameters(), mp_model, &initial_model,
                                    &logger);

  const CpModelProto mapping_model = GetMappingModel(initial_model);

  // By default we clear the names.
  EXPECT_EQ(mapping_model.variables(1).name(), "");
  EXPECT_EQ(mapping_model.variables(1).domain(0), -1);
  EXPECT_EQ(mapping_model.variables(1).domain(1), -1);
}

// This test used to fail before CL 180337997.
TEST(PresolveCpModelTest, LinearConstraintCornerCasePresolve) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
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

  // This model is UNSAT, but before the CL, trying to solve it was crashing.
  // because of the duplicate singleton var 0 in the first constraint.
  Model model;
  const CpSolverResponse response = SolveCpModel(initial_model, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

// This test show how we extract simple bool => bound encoding from a big-M
// encoding.
TEST(PresolveCpModelTest, LinearConstraintSplitting) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 10, 1 ]
        domain: [ 3, 15 ]
      }
    }
  )pb");

  // The model is equivalent to    var0 => var1 <= 5
  //                          not(var0) => var1 >= 3
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: 1
        coeffs: [ 1 ]
        domain: [ 0, 5 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: 1
        coeffs: [ 1 ]
        domain: [ 3, 10 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest,
     ExtractEnforcementLiteralFromLinearConstraintPositiveMax) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, 7, 1 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 0
      enforcement_literal: 1
      linear {
        vars: 2
        coeffs: 1
        domain: [ 0, 1 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest,
     ExtractEnforcementLiteralFromLinearConstraintNegativeMax) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, -3, 1 ]
        domain: [ -10, 1 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: -2
      linear {
        vars: 2
        coeffs: 1
        domain: [ 0, 1 ]
      }
    }
    constraints {
      enforcement_literal: -2
      bool_and { literals: -1 }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest,
     ExtractEnforcementLiteralFromLinearConstraintPositiveMin) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, 3, 1 ]
        domain: [ 3, 100 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: -2
      linear {
        vars: 2
        coeffs: 1
        domain: [ 1, 2 ]
      }
    }
    constraints {
      enforcement_literal: -2
      bool_and { literals: 0 }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest,
     ExtractEnforcementLiteralFromLinearConstraintNegativeMin) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, -3, 1 ]
        domain: [ 0, 100 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: 1
      linear {
        vars: 2
        coeffs: 1
        domain: [ 1, 2 ]
      }
    }
    constraints {
      enforcement_literal: -1
      bool_and { literals: -2 }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest,
     ExtractEnforcementLiteralFromLinearConstraintMultiple) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      name: "r0"
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, 3, 1 ]
        domain: [ 2, 100 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      name: "r0"
      enforcement_literal: -1
      enforcement_literal: -2
      linear {
        vars: 2
        coeffs: 1
        domain: [ 2, 2 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BasicLinMaxPresolve) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 7, 12 ] }
    variables { domain: [ -2, 4 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2, 3 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 7, 12 ] }
    variables { domain: [ -2, 4 ] }
    variables { domain: [ 7, 12 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, MoreAdvancedPresolve) {
  // We can remove variable zero from the max since it do not change the
  // outcome.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 10, 12 ] }
    variables { domain: [ 10, 13 ] }
    variables { domain: [ 10, 20 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2, 3 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 10, 12 ] }
    variables { domain: [ 10, 13 ] }
    variables { domain: [ 10, 13 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ConvertToEquality) {
  // We can infer that the target is necessarily equal to the second variable.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -4, 0 ] }
    variables { domain: [ 0, 12 ] }
    variables { domain: [ -2, 0 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2, 3 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -4, 0 ] }
    variables { domain: [ 0, 12 ] }
    variables { domain: [ -2, 0 ] }
    variables { domain: [ 0, 12 ] }
    constraints {
      linear {
        vars: [ 3, 1 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ConvertToEqualityDoNotWork) {
  // Compared to ConvertToEquality, here we can't because the min of that
  // variable is too low.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -4, 0 ] }
    variables { domain: [ -3, 12 ] }
    variables { domain: [ -2, 0 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2, 3 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -4, 0 ] }
    variables { domain: [ -3, 12 ] }
    variables { domain: [ -2, 0 ] }
    variables { domain: [ 0, 12 ] }
    constraints {
      lin_max {
        target: { vars: 3 coeffs: 1 }
        exprs: { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinMaxExprEqualTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: -16777224 domain: 1 }
    constraints {
      lin_max {
        target { vars: -1 coeffs: 1 }
        exprs { vars: -1 coeffs: 1 }
        exprs { vars: 0 coeffs: -10 offset: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BasicLinAbsPresolveVarToAbs) {
  // Note that we use duplicate constraints otherwise, the presolve will
  // solve the problem for us because m appear in only one constraint.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -2, 12 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
      }
    }
    constraints { dummy_constraint { vars: [ 1, 2, 3 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BasicLinAbsPresolveAbsToVar) {
  // Note that we use duplicate constraints otherwise, the presolve will
  // solve the problem for us because m appear in only one constraint.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 20 ] }
    variables { domain: [ 0, 12 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
      }
    }
    constraints { dummy_constraint { vars: [ 1, 2, 3 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -12, 12 ] }
    variables { domain: [ 0, 12 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BasicLinAbsPresolveFixedTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 20 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      lin_max {
        target { offset: 5 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
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
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      lin_max {
        target: { vars: 2 coeffs: 1 }
        exprs: { vars: 0 coeffs: 10 offset: -5 }
        exprs: { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemoveAbsFromUnaryLinear) {
  // Make sure we can only remove the varibale 1 here.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 20 ] }
    variables { domain: [ 0, 12 ] }
    variables { domain: [ 0, 1 ] }
    constraints { dummy_constraint { vars: [ 0, 2 ] } }
    constraints {
      lin_max {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: 1
        coeffs: 1
        domain: [ 3, 5 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -12, 12 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 1
      linear { vars: 0 coeffs: 1 domain: -5 domain: -3 domain: 3 domain: 5 }
    }
  )pb");

  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinMaxBasicPresolveSingleVar) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 7, 12 ] }
    variables { domain: [ -2, 4 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 7, 12 ] }
    variables { domain: [ -2, 4 ] }
    variables { domain: [ 7, 12 ] }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinMaxBasicPresolveExprs) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -2, -1 ] }
    variables { domain: [ -3, 0 ] }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 2, 3 ]
          offset: -5
        }
        exprs {
          vars: [ 1, 2 ]
          coeffs: [ 2, -5 ]
          offset: -6
        }
        exprs {
          vars: [ 0, 2 ]
          coeffs: [ -2, 3 ]
        }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -2, -1 ] }
    variables { domain: [ -1, 0 ] }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 2, 3 ]
          offset: -5
        }
        exprs {
          vars: [ 1, 2 ]
          coeffs: [ 2, -5 ]
          offset: -6
        }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlapRemovedRedundantIntervals) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 3, 5 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 1, 12 ] }
    variables { domain: [ 5, 10 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 6, 12 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        size { vars: 7 coeffs: 1 }
        end { vars: 8 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4, 5 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 6, 7, 8 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 3, 5 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 4, 10 ] }
    variables { domain: [ 5, 10 ] }
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 6, 12 ] }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        size { vars: 7 coeffs: 1 }
        end { vars: 8 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4, 5 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 6, 7, 8 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints { no_overlap { intervals: [ 0, 1 ] } }
  )pb");

  SatParameters params;
  params.set_convert_intervals(true);
  params.set_cp_model_probing_level(2);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model,
              ModelEqualsIgnoringConstraintsOrder(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlapMergeFixedIntervals) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 3, 26 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 3, 26 ] }
    constraints {
      interval {
        start { offset: 0 }
        size { offset: 5 }
        end { offset: 5 }
      }
    }
    constraints {
      interval {
        start { offset: 6 }
        size { offset: 3 }
        end { offset: 9 }
      }
    }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2, 3 ] } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 3, 26 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 3, 26 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start {}
        end { offset: 9 }
        size { offset: 9 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");
  SatParameters params;
  params.set_convert_intervals(true);
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlapNoMergingOfFixedIntervals) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 1, 6 ] }
    variables { domain: [ 1, 26 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 3, 26 ] }
    constraints {
      interval {
        start { offset: 0 }
        size { offset: 5 }
        end { offset: 5 }
      }
    }
    constraints {
      interval {
        start { offset: 6 }
        size { offset: 3 }
        end { offset: 9 }
      }
    }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2, 3 ] } }
  )pb");

  SatParameters params;
  params.set_convert_intervals(true);
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(initial_model));
}

TEST(PresolveCpModelTest, RemoveIsolatedFixedIntervalsBefore) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    constraints {
      interval {
        start { offset: 0 }
        size { offset: 5 }
        end { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: 0 intervals: 1 } }
  )pb");
  SatParameters params;
  params.set_convert_intervals(true);
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemoveIsolatedFixedIntervalsAfter) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    constraints {
      interval {
        start { offset: 26 }
        size { offset: 5 }
        end { offset: 31 }
      }
    }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    variables { domain: [ 5, 20 ] }
    variables { domain: [ 3, 6 ] }
    variables { domain: [ 8, 26 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        size { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: 0 intervals: 1 } }
  )pb");
  SatParameters params;
  params.set_convert_intervals(true);
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, SplitNoOverlap) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 5, 13 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 6, 14 ] }
    variables { domain: [ 14, 20 ] }
    variables { domain: [ 19, 25 ] }
    variables { domain: [ 18, 22 ] }
    variables { domain: [ 23, 27 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 5 }
        end { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 5 }
        end { vars: 3 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        size { offset: 5 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        size { offset: 5 }
        end { vars: 7 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2, 3 ] } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 5, 13 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 6, 14 ] }
    variables { domain: [ 14, 20 ] }
    variables { domain: [ 19, 25 ] }
    variables { domain: [ 18, 22 ] }
    variables { domain: [ 23, 27 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 5 }
        end { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 5 }
        end { vars: 3 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        size { offset: 5 }
        end { vars: 5 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        size { offset: 5 }
        end { vars: 7 coeffs: 1 }
      }
    }
    constraints { no_overlap { intervals: 0 intervals: 1 } }
    constraints { no_overlap { intervals: 2 intervals: 3 } }
  )pb");
  SatParameters params;
  params.set_convert_intervals(true);
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model,
              ModelEqualsIgnoringConstraintsOrder(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlapDuplicateNonZeroSizedInterval) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    constraints {
      interval {
        start { offset: 1 }
        end { offset: 1 }
        size { vars: 0 coeffs: 5 offset: 3 }
      }
    }
    constraints { no_overlap { intervals: 0 intervals: 0 } }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, params, CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, NoOverlapDuplicatePossiblyZeroSizedInterval) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    constraints {
      interval {
        start { offset: 1 }
        end { offset: 1 }
        size { vars: 0 coeffs: 5 }
      }
    }
    constraints { no_overlap { intervals: 0 intervals: 0 } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 0 }
  )pb");

  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlapDuplicateOptionalPossiblyZeroSizedInterval) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 3 }
    variables { domain: 0 domain: 1 }
    constraints {
      enforcement_literal: 1
      interval {
        start { offset: 1 }
        end { offset: 1 }
        size { vars: 0 coeffs: 5 }
      }
    }
    constraints { no_overlap { intervals: 0 intervals: 0 } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 3 }
    variables { domain: 0 domain: 1 }
    constraints {
      enforcement_literal: 1
      linear { vars: 0 coeffs: 1 domain: 0 domain: 0 }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CumulativeWithNoInterval) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 5, 5 ] }
    constraints {
      cumulative {
        intervals: []
        demands: []
        capacity { offset: 0 }
      }
    }
  )pb");

  CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_EQ(presolved_model.constraints_size(), 0);
}

TEST(PresolveCpModelTest, CumulativeWithUnperformedIntervals) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 1
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1 ]
        demands { offset: 2 }
        demands { offset: 3 }
        capacity { offset: 4 }
      }
    }
    constraints {
      linear {
        vars: 1
        coeffs: 1
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: 2
        coeffs: 1
        domain: [ 0, 0 ]
      }
    }
  )pb");

  CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_EQ(presolved_model.constraints_size(), 0);
}

TEST(PresolveCpModelTest, SplitCumulative) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 13, 20 ] }
    variables { domain: [ 18, 22 ] }
    variables { domain: [ 16, 30 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2, 3, 4, 5 ],
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        capacity: { offset: 2 }
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 13, 20 ] }
    variables { domain: [ 18, 22 ] }
    variables { domain: [ 16, 30 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2 ],
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        capacity { offset: 2 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 3, 5, 4 ],
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        capacity: { offset: 2 }
      }
    }
  )pb");
  SatParameters params;

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CumulativeZeroDemands) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 2, 4 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2 ],
        demands:
        [ { offset: 0 }
          , { offset: 2 }
          , { vars: 3 coeffs: 1 }],
        capacity: { offset: 4 }
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 2, 4 ] }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1 ],
        demands:
        [ { offset: 2 }
          , { vars: 3 coeffs: 1 }],
        capacity: { offset: 4 }
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CumulativeDemandsDoNotFit) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 2, 8 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      enforcement_literal: 4
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 3 }
        size { offset: 3 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1 ],
        demands:
        [ { vars: 2 coeffs: 1 }
          , { vars: 3 coeffs: 1 }],
        capacity: { offset: 4 }
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 4 ] }
    variables { domain: [ 2, 8 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      enforcement_literal: 4
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 3 }
        size { offset: 3 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1 ],
        demands:
        [ { vars: 2 coeffs: 1 }
          , { vars: 3 coeffs: 1 }],
        capacity: { offset: 4 }
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CumulativeDemandsDoNotFitSizeMinZero) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 3, 6, 0 ]
        coeffs: [ -1, 1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      interval {
        start {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
        end {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
        size {
          vars: [ 6 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      linear {
        vars: [ 4, 7, 1 ]
        coeffs: [ -1, 1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      interval {
        start {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        end {
          vars: [ 4 ]
          coeffs: [ 1 ]
        }
        size {
          vars: [ 7 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      linear {
        vars: [ 5, 8, 2 ]
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
          vars: [ 5 ]
          coeffs: [ 1 ]
        }
        size {
          vars: [ 8 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 1 }
        intervals: [ 1, 3, 5 ]
        demands { offset: 1 }
        demands { offset: 5 }
        demands { offset: 1 }
      }
    }
  )pb");

  EXPECT_EQ(Solve(initial_model).status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, CumulativeRemoveIncompativeDemands) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 4 ] }
    variables { domain: [ 5, 8 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      enforcement_literal: 5
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 3 }
        size { offset: 3 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2 ],
        demands: { vars: 3 coeffs: 1 }
        demands: { vars: 3 coeffs: 1 }
        demands: { vars: 4 coeffs: 1 }
        capacity: { offset: 4 }
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 4 ] }
    variables { domain: [ 5, 8 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1 ],
        demands: { vars: 3 coeffs: 1 }
        demands: { vars: 3 coeffs: 1 }
        capacity: { offset: 4 }
      }
    }
  )pb");
  SatParameters params;
  // This will force all variables to be kept, even if unused.
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CumulativeGcdDemands) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 7 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 3 }
        size { offset: 3 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2 ],
        demands: { offset: 2 }
        demands: { offset: 2 }
        demands: { offset: 2 }
        capacity: { offset: 4 }
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 1, 9 ] }
    variables { domain: [ 2, 7 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 3 }
        size { offset: 3 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2 ],
        demands: { offset: 1 }
        demands: { offset: 1 }
        demands: { offset: 1 }
        capacity: { offset: 2 }
      }
    }
  )pb");
  SatParameters params;

  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlap2DOneBox) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 5, 5 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 5, 5 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 4 coeffs: 1 }
        size { vars: 5 coeffs: 1 }
      }
    }
    constraints { no_overlap_2d { x_intervals: 0 y_intervals: 1 } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 5, 5 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 5, 5 ] }
  )pb");

  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlap2DRemoveInactiveBoxes) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 0 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 3 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      enforcement_literal: 6
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2 ]
        y_intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 3 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1 ]
        y_intervals: [ 0, 1 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlap2DNoRemoveNullAreaBoxes) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 3 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        end { vars: 7 coeffs: 1 }
        size {}
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2 ]
        y_intervals: [ 0, 1, 3 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(initial_model));
}

TEST(PresolveCpModelTest, NoOverlap2DSplitBoxes) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }   # 0: start 0
    variables { domain: [ 2, 4 ] }   # 3: start 1
    variables { domain: [ 8, 12 ] }  # 6: start 2
    variables { domain: [ 9, 13 ] }  # 9: start 3
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 3 ]
        y_intervals: [ 0, 1, 2, 3 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 2, 4 ] }
    variables { domain: [ 8, 12 ] }
    variables { domain: [ 9, 13 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: 0
        x_intervals: 1
        y_intervals: 0
        y_intervals: 1
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: 2
        x_intervals: 3
        y_intervals: 2
        y_intervals: 3
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlap2DSplitSingletonBoxes) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 2, 4 ] }
    variables { domain: [ 8, 12 ] }  # Disjoint from the other two
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2 ]
        y_intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 2, 4 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1 ]
        y_intervals: [ 0, 1 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlap2DMerge) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 3 ]
        y_intervals: [ 0, 1, 2, 3 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 4, 5 ]
        y_intervals: [ 0, 1, 4, 5 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 2, 3, 4, 5 ]
        y_intervals: [ 2, 3, 4, 5 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 3, 5, 4 ]
        y_intervals: [ 0, 1, 2, 3, 5, 4 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlap2DMergePartial) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 3, 4 ]
        y_intervals: [ 0, 1, 2, 3, 4 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 3, 4, 5 ]
        y_intervals: [ 0, 1, 3, 4, 5 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 1, 3, 4, 5 ]
        y_intervals: [ 1, 3, 4, 5 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 3, 4 ]
        y_intervals: [ 0, 1, 2, 3, 4 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 3, 4, 5 ]
        y_intervals: [ 0, 1, 3, 4, 5 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoOverlap2DMergeWithOverlaps) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 3, 4 ]
        y_intervals: [ 0, 1, 2, 3, 4 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 4, 5 ]
        y_intervals: [ 0, 1, 2, 4, 5 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 1, 3, 4, 5 ]
        y_intervals: [ 1, 3, 4, 5 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 5 }
        size { offset: 5 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1, 2, 3, 4, 5 ]
        y_intervals: [ 0, 1, 2, 3, 4, 5 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithLeftConstant) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 12 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 0, 100 ]
    }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 12 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 20, 24 ]
    }
    constraints {
      linear {
        vars: 2
        vars: 0
        coeffs: 1
        coeffs: -2
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EnforcedIntProdWithLeftConstant) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 12 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 12 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: 2
        vars: 0
        coeffs: 1
        coeffs: -2
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithRightConstant) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 14 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 0, 100 ]
    }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables {
      name: 'x'
      domain: [ 10, 14 ]
    }
    variables {
      name: 'y'
      domain: [ 2, 2 ]
    }
    variables {
      name: 'p'
      domain: [ 20, 28 ]
    }
    constraints {
      linear {
        vars: 2
        vars: 0
        coeffs: 1
        coeffs: -2
        domain: [ 0, 0 ]
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithXEqualTwoX) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    variables { domain: [ 2, 2 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");

  const CpModelProto mapping_model = GetMappingModel(initial_model);
  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 2, 2 ] }
  )pb");
  EXPECT_THAT(expected_mapping_model, testing::EqualsProto(mapping_model));
}

TEST(PresolveCpModelTest, IntProdWithConstantProduct) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2000 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 5, 5 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");

  const CpModelProto mapping_model = GetMappingModel(initial_model);
  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: [ 10, 10 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 5, 5 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { offset: 2 }
        exprs { offset: 5 }
      }
    }
  )pb");
  EXPECT_THAT(expected_mapping_model, testing::EqualsProto(mapping_model));
}

TEST(PresolveCpModelTest, AlwaysFalseIntProd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 20, 30 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 5, 5 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  PresolveForTest(initial_model, SatParameters(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, EnforcedAlwaysFalseIntProd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 20, 30 ] }
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 5, 5 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model;
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithOverflow) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -100000000000, 100000000000 ] }
    variables { domain: [ 0, 0, 100000000000, 100000000000 ] }
    variables { domain: [ 0, 0, 100000000000, 100000000000 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 0, 100000000000, 100000000000 ] }
    variables { domain: [ 0, 0, 100000000000, 100000000000 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      bool_and { literals: -5 }
    }
    constraints {
      linear {
        vars: 1
        vars: 3
        coeffs: 1
        coeffs: -100000000000
        domain: 0
        domain: 0
      }
    }
    constraints {
      linear {
        vars: 2
        vars: 4
        coeffs: 1
        coeffs: -100000000000
        domain: 0
        domain: 0
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithOverflowLargeConstantFactor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 266 }
    constraints {
      int_prod {
        target { offset: 1862270976 }
        exprs { offset: 1862270975 }
        exprs { vars: 0 coeffs: 250970374144 offset: 1 }
      }
    }
  )pb");

  EXPECT_EQ(Solve(initial_model).status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, IntProdWithOverflowLargeNegativeConstantFactor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 266 }
    constraints {
      int_prod {
        target { offset: -1862270976 }
        exprs { offset: -1862270975 }
        exprs { vars: 0 coeffs: 250970374144 offset: 1 }
      }
    }
  )pb");

  EXPECT_EQ(Solve(initial_model).status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, IntProdWithIdentity) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    variables { domain: [ 1, 1 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto mapping_model = GetMappingModel(initial_model);
  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 0 }
    variables { domain: 1 domain: 1 }
  )pb");
  EXPECT_THAT(mapping_model, testing::EqualsProto(expected_mapping_model));
}

TEST(PresolveCpModelTest, IntProdWithXEqualX2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntSquareDomainReduction) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -3, 5 ] }
    variables { domain: [ -30, 30 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -3, 5 ] }
    variables { domain: [ 0, 1, 4, 4, 9, 9, 16, 16, 25, 25 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntSquareLargeDomainReduction) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 110 ] }
    variables { domain: [ -200000, 200000 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 110 ] }
    variables { domain: [ 0, 12100 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntSquareExprDomainReduction) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 110 ] }
    variables { domain: [ -9000, 9000 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -20, 94 ] }
    variables { domain: [ 0, 9000 ] }
    constraints {
      int_prod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdWithAffineRelation) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 0, 3, 3, 6, 6, 9, 9 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    # Add this just to avoid triggering the rule of unused target variable.
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
    }
  )pb");

  // The variable 2 is detected to be of the form 3 * new_var1. Subsequently,
  // the product target is detected to be a multiple of 3, so its target is
  // replaced by new_var2. The domain are computed accordingly.
  CpModelProto presolved_model = PresolveForTest(initial_model);
  presolved_model.clear_objective();
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 6 ] }  # This is old_var_0 / 3.
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 3 ] }  # This is old_var_2 / 3.
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdCoeffDividesTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 9 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 0, 1000 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 10 offset: 20 }
        exprs { vars: 0 coeffs: 1 offset: 3 }
        exprs { vars: 1 coeffs: 5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 9 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 1, 58 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 2 offset: 4 }
        exprs { vars: 0 coeffs: 1 offset: 3 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveOneConstraint(initial_model, /*constraint_index=*/0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntProdGlobalGcd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 9 ] }
    variables { domain: [ 1, 10 ] }
    variables { domain: [ 0, 200 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 9 offset: 18 }
        exprs { vars: 0 coeffs: 2 offset: 4 }
        exprs { vars: 1 coeffs: 6 offset: -6 }
      }
    }
  )pb");

  // The gcd is 12 !
  // So we have 9 * target + 18 is a multiple of 12, so target can be for
  // instance written 4 * new_target + 2.
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 9 ] }
    variables { domain: [ 1, 10 ] }
    variables {
      domain: [ 0, 10, 12, 14, 16, 16, 18, 20, 22, 22, 25, 25, 28, 28, 31, 31 ]
    }  # We divide by 4
    constraints {
      int_prod {
        target { vars: 2 coeffs: 3 offset: 6 }
        exprs { vars: 0 coeffs: 1 offset: 2 }
        exprs { vars: 1 coeffs: 1 offset: -1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NullProduct) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 0 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2 ] } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 5 ] }  # Many possible values here.
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EnforcedNullProduct) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2 ] } }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 20 ] }
    variables { domain: [ 0, 5 ] }  # Many possible values here.
    variables { domain: [ 0, 0 ] }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BooleanProduct) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 5
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: -1 offset: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: -1 offset: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 5
      enforcement_literal: 0
      bool_and { literals: [ 1, -3, 3, -5 ] }
    }
    constraints { bool_or { literals: [ -6, -4, -2, 0, 2, 4 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_permute_variable_randomly(false);
  params.set_cp_model_probing_level(0);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, AffineBooleanProduct) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 30 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 2 offset: 3 }
        exprs { vars: 2 coeffs: 3 offset: 2 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 6, 6, 10, 10, 15, 15, 24, 25 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      enforcement_literal: -2
      linear { vars: 0 vars: 2 coeffs: 1 coeffs: -9 domain: 6 domain: 6 }
    }
    constraints {
      enforcement_literal: 1
      linear { vars: 0 vars: 2 coeffs: 1 coeffs: -15 domain: 10 domain: 10 }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_permute_variable_randomly(false);
  params.set_cp_model_probing_level(0);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EnforcedAffineBooleanProduct) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 30 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 2 offset: 3 }
        exprs { vars: 2 coeffs: 3 offset: 2 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 30 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 3, -2 ]
      linear { vars: 0 vars: 2 coeffs: 1 coeffs: -9 domain: 6 domain: 6 }
    }
    constraints {
      enforcement_literal: [ 3, 1 ]
      linear { vars: 0 vars: 2 coeffs: 1 coeffs: -15 domain: 10 domain: 10 }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_permute_variable_randomly(false);
  params.set_cp_model_probing_level(0);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntDivSimplification) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 20 ] }
    variables { domain: [ -5, 5 ] }
    constraints {
      int_div {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 20 ] }
    variables { domain: [ 1, 1 ] }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntDivSingleVariable) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: -6 offset: 12 }
        exprs { offset: 12 }
        exprs { vars: 0 coeffs: 1 offset: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntDivSimplificationOpp) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 20 ] }
    variables { domain: [ -5, 5 ] }
    constraints {
      int_div {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 20 ] }
    variables { domain: [ -1, -1 ] }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, PositiveFixedTargetAndPositiveDivisor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { offset: 3 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: 5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 15, 19 ] }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ZeroFixedTargetAndPositiveDivisor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { offset: 0 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: 5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -4, 4 ] }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NegativeFixedTargetAndPositiveDivisor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { offset: -3 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: 5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -19, -15 ] }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, PositiveFixedTargetAndNegativeDivisor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { offset: 3 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: -5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -19, -15 ] }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ZeroFixedTargetAndNegativeDivisor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { offset: 0 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: -5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -4, 4 ] }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NegativeFixedTargetAndNegativeDivisor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { offset: -3 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: -5 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 15, 19 ] }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, TargetFixedToPositiveValue) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 210, 288 ] }
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: 100 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 210, 288 ] }
    variables { domain: [ 2, 2 ] }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, TargetFixedToZeroValue) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -55, 75 ] }
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: 100 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ -55, 75 ] }
    variables { domain: [ 0, 0 ] }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, TargetFixedToNegativeValue) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 210, 288 ] }
    variables { domain: [ -30, 30 ] }
    constraints {
      int_div {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: -100 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 210, 288 ] }
    variables { domain: [ -2, -2 ] }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, TargetFixedThenExprPropagated) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 110, 288 ] }
    variables { domain: [ 2, 30 ] }
    constraints {
      int_div {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: 100 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 200, 288 ] }
    variables { domain: [ 2, 2 ] }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntModFixesTargetToZero) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 20 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ -3, 0 ] }
    constraints {
      int_mod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 20 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      int_div {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntModReduceTargetDomain) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 0, 8 ] }
    constraints {
      int_mod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 2, 7 ] }
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      int_div {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      int_prod {
        target { vars: 4 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: 0
        vars: 2
        vars: 4
        coeffs: 1
        coeffs: -1
        coeffs: -1
        domain: 0
        domain: 0
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntModFixedTargetAndMod) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 20 ] }
    variables { domain: [ -5, 11 ] }
    variables { domain: [ -17, 8 ] }
    constraints {
      int_mod {
        target { offset: 2 }
        exprs { vars: 0 coeffs: 1 }
        exprs { offset: 5 }
      }
    }
    constraints {
      int_mod {
        target { offset: 2 }
        exprs { vars: 1 coeffs: 1 }
        exprs { offset: 5 }
      }
    }
    constraints {
      int_mod {
        target { offset: -2 }
        exprs { vars: 2 coeffs: 1 }
        exprs { offset: 5 }
      }
    })pb");

  // We get a representative for each int_mod.
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 3 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 3 }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinearConstraintWithGcd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: 0
        coeffs: 100
        vars: 1
        coeffs: 200
        domain: [ 320, 999 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  EXPECT_EQ(presolved_model.variables_size(), 2);
  ASSERT_EQ(1, presolved_model.constraints_size());
  const LinearConstraintProto& lin = presolved_model.constraints(0).linear();
  EXPECT_EQ(0, lin.vars(0));
  EXPECT_EQ(1, lin.vars(1));
  EXPECT_EQ(1, lin.coeffs(0));
  EXPECT_EQ(2, lin.coeffs(1));
  EXPECT_EQ(4, lin.domain(0));
  EXPECT_EQ(9, lin.domain(1));
}

TEST(PresolveCpModelTest, RemoveNonUsefulTerms) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 10, 10, 4, 3 ]
        domain: [ 0, 29 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemoveNonUsefulTerms2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 9, 9, 4, 3 ]
        domain: [ 0, 26 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemoveNonUsefulTerms3) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 10, 7 ]
        domain: [ 0, 17 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DetectApproximateGCD) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1001, 999 ]
        domain: [ 0, 28500 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 28 ] }
    variables { domain: [ 0, 28 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 28 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinearConstraintWithGcdInfeasible) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 4, 4 ]
        domain: [ 9, 9 ]
      }
    }
  )pb");

  EXPECT_EQ(Solve(initial_model).status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest,
     LinearConstraintWithGcdFalseConstraintWithEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 4, 4 ]
        domain: [ 9, 9 ]
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 0 ] }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IntervalPresolveNegativeSize) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -7, -7, 0, 0 ] }
    constraints {
      interval {
        start { offset: 0 }
        end { vars: 0 coeffs: 1 }
        size { vars: 0 coeffs: 1 }
      }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

// TODO(user): really stop testing the full presolve, we always have to add
// irrelevant constraint so that stuff are not presolved away.
TEST(PresolveCpModelTest, BasicIntervalPresolve) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 3, 10 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 3, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }

    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 4 coeffs: 1 }
        size { vars: 5 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, -1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4, 5 ]
        coeffs: [ 1, -1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1 ]
        y_intervals: [ 0, 1 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 12 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 3, 10 ] }
    variables { domain: [ 0, 12 ] }
    variables { domain: [ 5, 15 ] }
    variables { domain: [ 3, 10 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 4 coeffs: 1 }
        size { vars: 5 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, -1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4, 5 ]
        coeffs: [ 1, -1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: [ 0, 1 ]
        y_intervals: [ 0, 1 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ExpandMinimizeObjective) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -4611686018427387903, 4611686018427387903 ] }
    constraints { dummy_constraint { vars: [ 0, 1 ] } }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 2 domain: -10 domain: 10 }
    }
    constraints {
      linear {
        vars: 0
        vars: 1
        vars: 2
        coeffs: 1
        coeffs: 2
        coeffs: -1
        domain: 3
        domain: 3
      }
    }
    objective { vars: 2 coeffs: 2 offset: 1 }
  )pb");

  // We both expand the objective and merge it with other parallel constraint.
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 2 ]
      offset: -2.5
      scaling_factor: 2
      integer_before_offset: -3
      integer_scaling_factor: 2
      domain: [ -10, 10 ]
    }
  )pb");

  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, ExpandMinimizeObjectiveWithOppositeCoeff) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -4611686018427387903, 4611686018427387903 ] }
    constraints { dummy_constraint { vars: [ 0, 1 ] } }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 2 domain: -10 domain: 10 }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 1 ]
        domain: [ 3, 3 ]
      }
    }
    objective { vars: 2 coeffs: 2 offset: 1 }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 2 ]
        domain: [ -10, 10 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ -1, -2 ]
      offset: 3.5
      scaling_factor: 2
      integer_before_offset: 3
      integer_scaling_factor: 2
      domain: [ -30, 30 ]
    }
  )pb");

  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, ExpandMaximizeObjective) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -4611686018427387903, 4611686018427387903 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      linear { vars: 0 vars: 1 coeffs: 1 coeffs: 1 domain: -10 domain: 10 }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, -1 ]
        domain: [ 3, 3 ]
      }
    }
    objective { vars: -3 coeffs: 2 scaling_factor: -1 offset: 1 }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(2, presolved_model.objective().vars_size());
  EXPECT_EQ(0, presolved_model.objective().vars(0));
  EXPECT_EQ(-1, presolved_model.objective().coeffs(0));
  EXPECT_EQ(1, presolved_model.objective().vars(1));
  EXPECT_EQ(-2, presolved_model.objective().coeffs(1));
  EXPECT_EQ(3.5, presolved_model.objective().offset());
  EXPECT_EQ(-2, presolved_model.objective().scaling_factor());
}

TEST(PresolveCpModelTest, ExpandMaximizeObjectiveWithOppositeCoeff) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -4611686018427387903, 4611686018427387903 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -10, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 1 ]
        domain: [ 3, 3 ]
      }
    }
    objective { vars: -3 coeffs: 2 scaling_factor: -1 offset: 1 }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(2, presolved_model.objective().vars_size());
  EXPECT_EQ(0, presolved_model.objective().vars(0));
  EXPECT_EQ(1, presolved_model.objective().coeffs(0));
  EXPECT_EQ(1, presolved_model.objective().vars(1));
  EXPECT_EQ(2, presolved_model.objective().coeffs(1));
  EXPECT_EQ(-2.5, presolved_model.objective().offset());
  EXPECT_EQ(-2, presolved_model.objective().scaling_factor());
}

TEST(PresolveCpModelTest, ExpandMinimizeObjectiveWithLimitingLinearEquation) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -8, 7 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -10, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, -1 ]
        domain: [ 3, 3 ]
      }
    }
    objective { vars: 2 coeffs: 2 offset: 1 }
  )pb");

  // The objective domain without offset above (and after moving the coeff to
  // the scaling) is [-8, 7], and when doing the transformation new_expression =
  // old_obj + 3, the domain of the new expression is [-5, 10].
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 9 ] }
    variables { domain: [ -7, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -10, 10 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 2 ]
      offset: -2.5
      scaling_factor: 2
      integer_before_offset: -3
      integer_scaling_factor: 2
      domain: [ -5, 10 ]
    }
  )pb");

  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, ExpandMinimizeObjectiveWithLimitingLinearEquation2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -8, 7 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -10, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 1 ]
        domain: [ 3, 3 ]
      }
    }
    objective { vars: 2 coeffs: 2 offset: 1 }
  )pb");

  // This time, we have new_obj = old_obj - 3.
  // Note that the variable #2 is removed, but this do not remove any feasible
  // solution since its value will be uniquely determined via the removed
  // constraint x0 + 2x1 + x2 = 3. The objective domain constrains x0 + 2x1
  // to take feasible value for x3.
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -7, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -10, 10 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ -1, -2 ]
      offset: 3.5
      scaling_factor: 2
      integer_before_offset: 3
      integer_scaling_factor: 2
      domain: [ -11, 4 ]
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, ExpandObjectiveInfeasible) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -10, 10 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
      domain: [ 30, 40 ]
    }
  )pb");

  Model tmp_model;
  EXPECT_EQ(SolveCpModel(initial_model, &tmp_model).status(),
            CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, ExpandObjectiveWithLimitedPresolve) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear { vars: 0 vars: 1 coeffs: -1 coeffs: 1 domain: -1 domain: -1 }
    }
    objective { vars: 1 coeffs: 1 })pb");

  SatParameters params;
  params.set_max_presolve_iterations(0);
  params.set_log_search_progress(true);
  EXPECT_EQ(SolveWithParameters(initial_model, params).status(),
            CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, CircuitConstraint) {
  // A rho shape.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        tails: 0
        heads: 0
        literals: 0  # needed not to be unsat.
        tails: 0
        heads: 1
        literals: 1
        tails: 1
        heads: 2
        literals: 2
        tails: 2
        heads: 3
        literals: 3
        tails: 3
        heads: 1
        literals: 4
      }
    }
  )pb");

  // There is just one possible solution, detected by the presolve.
  const CpModelProto mapping_model = GetMappingModel(initial_model);
  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
  )pb");
  EXPECT_THAT(expected_mapping_model, testing::EqualsProto(mapping_model));
}

// Fully specified circuit. This used to remove the constraint instead of
// detecting infeasibility since some mandatory node are not in the 0 <-> 1
// circuit.
TEST(PresolveCpModelTest, FixedButIncompleteCircuitConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        tails: [ 0, 1, 1, 2, 1, 3, 2, 3 ]
        heads: [ 1, 0, 2, 1, 3, 1, 3, 2 ]
        literals: [ 0, 1, 2, 3, 4, 5, 6, 7 ]
      }
    }
  )pb");
  ExpectInfeasibleDuringPresolve(initial_model);
}

TEST(PresolveCpModelTest, CircuitConstraintWithDuplicateLiteral) {
  // A rho shape.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        tails: 0
        heads: 0
        literals: 0  # set at true
        tails: 0
        heads: 1
        literals: 1  # will be false

        tails: 1
        heads: 2
        literals: 2
        tails: 2
        heads: 3
        literals: 3
        tails: 3
        heads: 1
        literals: 4

        tails: 1
        heads: 1
        literals: 1
        tails: 2
        heads: 2
        literals: 1
        tails: 3
        heads: 3
        literals: 1
      }
    }
  )pb");

  // There is just one possible solution, detected by the presolve.
  SatParameters params;
  params.set_max_presolve_iterations(1);
  const CpModelProto mapping_model = GetMappingModel(initial_model);
  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
  )pb");
  EXPECT_THAT(expected_mapping_model, testing::EqualsProto(mapping_model));
}

TEST(PresolveCpModelTest, RouteConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      routes {
        tails: [ 0, 0, 1, 1, 2, 2 ]
        heads: [ 1, 2, 0, 2, 1, 0 ]
        literals: [ 0, 1, 2, 3, 4, 5 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    constraints {
      routes {
        tails: [ 0, 1, 2 ]
        heads: [ 1, 2, 0 ]
        literals: [ 0, 0, 0 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

// The presolve used to fail by removing all arcs incident to node 2 and thus
// node 2 was no longer considered as unreachable.
TEST(PresolveCpModelTest, RouteConstraintWithUnreachableNode) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 0 ] }
    constraints {
      routes {
        tails: [ 0, 0, 2, 1 ]
        heads: [ 1, 2, 1, 0 ]
        literals: [ 0, 1, 1, 0 ]
      }
    }
  )pb");
  ExpectInfeasibleDuringPresolve(initial_model);
}

TEST(PresolveCpModelTest, CircuitConstraintWithDegree2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        tails: 0
        heads: 0
        literals: 0
        tails: 1
        heads: 1
        literals: 1
        tails: 0
        heads: 1
        literals: 2
        tails: 1
        heads: 0
        literals: 3
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        tails: 0
        heads: 0
        literals: 0
        tails: 1
        heads: 1
        literals: 0
        tails: 0
        heads: 1
        literals: -1
        tails: 1
        heads: 0
        literals: -1
      }
    }
  )pb");

  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, UsedToCrash) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 2, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: 1
        vars: 0
        coeffs: 1
        coeffs: -1
        domain: [ 1, 9223372036854775807 ]
      }
    }
    constraints { linear { vars: 1 coeffs: 1 domain: 1 domain: 1 } }
  )pb");
  ExpectInfeasibleDuringPresolve(initial_model);
}

TEST(PresolveCpModelTest, FixedAllDifferent) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 50 ] }
    variables { domain: [ 3, 3 ] }
    variables { domain: [ 1, 50 ] }
    variables { domain: [ 1, 50 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 2, 4, 50 ] }
    variables { domain: [ 1, 2, 4, 50 ] }
    variables { domain: [ 1, 2, 4, 50 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");

  SatParameters extra_params;
  extra_params.set_symmetry_level(0);
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, extra_params);
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, AllDifferentWithExpressionsSharingVariable) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 50 ] }
    variables { domain: [ 2, 20 ] }
    variables { domain: [ 1, 50 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
        exprs { vars: 1 coeffs: 2 offset: -3 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 50 ] }
    variables { domain: [ 2, 2, 4, 20 ] }
    variables { domain: [ 1, 50 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
        exprs { vars: 1 coeffs: 2 offset: -3 }
      }
    }
  )pb");

  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, DetectDifferentVariablesAndAddNoOverlap) {
  CpModelProto cp_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 91, 905 ] }
    variables { domain: [ 638, 937 ] }
    variables { domain: [ 0, 69 ] }
    variables { domain: [ 575, 930 ] }
    constraints {
      linear {
        vars: [ 3, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ -863, -506 ]
      }
    }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 569, 909 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, -1 ]
        domain: [ -846, -32 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 3 ]
        coeffs: [ -1, 1 ]
        domain: [ -868, -22 ]
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ -839, -300 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 1, 4 ]
        coeffs: [ 1, -1 ]
        domain: [ 310, 330 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 2, 4 ]
        coeffs: [ -1, 1 ]
        domain: [ 275, 292 ]
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 2, 4 ]
        coeffs: [ -1, 1 ]
        domain: [ -362, -123 ]
      }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3, 4 ]
      values: [ 1, 397, 836, 69, 713 ]
    }
  )pb");
  ASSERT_TRUE(SolutionIsFeasible(cp_model, cp_model.solution_hint().values()));

  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &cp_model, &mapping_model);
  std::vector<int> mapping;
  CpModelPresolver presolver(&context, &mapping);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();
  presolver.DetectDifferentVariables();
  context.WriteVariableDomainsToProto();

  bool has_no_overlap_constraint = false;
  for (const ConstraintProto& constraint : cp_model.constraints()) {
    if (constraint.has_no_overlap()) {
      has_no_overlap_constraint = true;
      break;
    }
  }
  EXPECT_TRUE(has_no_overlap_constraint);
  EXPECT_TRUE(SolutionIsFeasible(cp_model, cp_model.solution_hint().values()));
  EXPECT_EQ(Solve(cp_model).status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, PermutationMandatoryValues) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 4 ] }
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
  model.GetOrCreate<SatParameters>()->set_expand_alldiff_constraints(false);
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  std::vector<int> mapping;
  PresolveContext context(&model, &presolved_model, &mapping_model);
  PresolveCpModel(&context, &mapping);

  const IntegerVariableProto expected_var = ParseTestProto(R"pb(
    domain: [ 4, 4 ])pb");
  EXPECT_THAT(expected_var, testing::EqualsProto(mapping_model.variables(3)));
}

TEST(PresolveCpModelTest, CircuitCornerCase1) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        tails: 1
        tails: 2
        tails: 0
        heads: 2
        heads: 0
        heads: 1
        literals: 0
        literals: 1
        literals: 2
      }
    }
  )pb");

  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_EQ(0, presolved_model.constraints_size());
}

TEST(PresolveCpModelTest, CircuitCornerCase2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        tails: 0
        heads: 1
        literals: 0
        tails: 1
        heads: 1
        literals: 1
        tails: 0
        heads: 2
        literals: 2
        tails: 2
        heads: 2
        literals: 3
      }
    }
  )pb");

  Model tmp_model;
  EXPECT_EQ(SolveCpModel(initial_model, &tmp_model).status(),
            CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, ObjectiveWithLargeCoefficient) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective: {
      vars: [ -1, -2, -3 ]
      scaling_factor: -1.0
      coeffs: [ 194833170077, 3656800, 19394221124 ]
    }
  )pb");
  SatParameters params;
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective: {
      vars: [ 0, 1, 2 ]
      scaling_factor: -1.0
      coeffs: [ -194833170077, -3656800, -19394221124 ]

      # We simplify the domain.
      domain: -214231048001
      domain: 0
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, MipSimplificationExample) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 4 ]
        domain: [ 4, 4 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 4 ]
        domain: [ 4, 4 ]
      }
    }
  )pb");
  const CpModelProto mapping_model = GetMappingModel(initial_model);
  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
  )pb");
  EXPECT_THAT(expected_mapping_model, testing::EqualsProto(mapping_model));
}

TEST(PresolveCpModelTest, TriviallyUnsatCumulative) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 2, 2 ] }  # size
    variables { domain: [ 0, 9 ] }  # end
    variables { domain: [ 2, 2 ] }  # capacity
    variables { domain: [ 5, 5 ] }  # demand
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
        end { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      cumulative {
        capacity: { offset: 2 }
        demands: { offset: 5 }
        intervals: [ 0 ]
      }
    }
  )pb");
  ExpectInfeasibleDuringPresolve(initial_model);
}

TEST(PresolveCpModelTest, ZeroDemandCumulative) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 2 }
        demands { offset: 1 }
        demands { offset: 2 }
        demands { offset: 0 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 2 }
        demands { offset: 1 }
        demands { offset: 2 }
        intervals: [ 0, 1 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CapacityExceedsDemands) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 1 ] }  # optional literal
    variables { domain: [ 7, 8 ] }  # capacity
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { vars: 4 coeffs: 1 }
        demands { offset: 1 }
        demands { offset: 2 }
        demands { offset: 4 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 1 ] }  # optional literal
    variables { domain: [ 0, 1 ] }  # capacity representative
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CumulativeDivideByGcd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 1 ] }  # optional literal
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 13 }
        demands { offset: 3 }
        demands { offset: 6 }
        demands { offset: 9 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 1 ] }  # optional literal
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 4 }
        demands { offset: 1 }
        demands { offset: 2 }
        demands { offset: 3 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, CumulativeDivideByGcdBug) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    name: "Multi-device peak-memory minimization."
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 13 ] }
    variables { domain: [ 13, 13 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 1 }
        size { offset: 1 }
      }
    }
    constraints {
      enforcement_literal: 3
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 1 }
        size { offset: 1 }
      }
    }
    constraints {
      enforcement_literal: 7
      interval {
        start { vars: 8 coeffs: 1 }
        end { vars: 10 coeffs: 1 }
        size { vars: 9 coeffs: 1 }
      }
    }
    constraints {
      enforcement_literal: 7
      linear {
        vars: [ 8, 9, 10 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints { linear { vars: 3 coeffs: 1 domain: 1 domain: 1 } }
    constraints {
      enforcement_literal: 3
      linear { vars: 0 vars: 4 coeffs: 1 coeffs: -1 domain: 0 domain: 0 }
    }
    constraints {
      linear {
        vars: 3
        vars: 7
        coeffs: 1
        coeffs: -1
        domain: -9223372036854775808
        domain: 0
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: 8
        vars: 0
        coeffs: 1
        coeffs: -1
        domain: -9223372036854775808
        domain: 0
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: 11
        vars: 10
        coeffs: 1
        coeffs: -1
        domain: -9223372036854775808
        domain: 0
      }
    }
    constraints {
      enforcement_literal: 7
      linear {
        vars: 8
        vars: 11
        coeffs: 1
        coeffs: -1
        domain: -9223372036854775808
        domain: -1
      }
    }
    constraints {
      enforcement_literal: 3
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
      enforcement_literal: 3
      linear {
        vars: 6
        vars: 10
        coeffs: 1
        coeffs: -1
        domain: -9223372036854775808
        domain: 0
      }
    }
    constraints {
      cumulative {
        capacity { vars: 12 coeffs: 1 }
        intervals: 2
        demands { vars: 13 coeffs: 1 }
      }
    }
    objective { vars: 12 coeffs: 1 }
  )pb");
  const CpSolverResponse response = Solve(initial_model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(13.0, response.objective_value());
}

TEST(PresolveCpModelTest, NonConflictingDemands) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }   # start 0
    variables { domain: [ 2, 6 ] }   # start 1
    variables { domain: [ 4, 8 ] }   # start 2
    variables { domain: [ 8, 10 ] }  # start 3
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      # Fixed interval that creates the potential overload.
      interval {
        start { offset: 5 }
        end { offset: 7 }
        size { offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 2 }
        demands: { offset: 1 }
        demands: { offset: 1 }
        demands: { offset: 1 }
        demands: { offset: 1 }
        demands: { offset: 1 }
        intervals: [ 0, 1, 2, 3, 4 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 2, 6 ] }
    variables { domain: [ 4, 8 ] }
    variables { domain: [ 8, 10 ] }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { offset: 5 }
        end { offset: 7 }
        size { offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 2 }
        intervals: [ 0, 1, 2 ]
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NonConflictingDemandsInTheMiddle) {
  // Initially, all intervals are connected through overlap.
  // Then interval 3 should be removed as it can never cause an overlap.
  // Then the cumulative should be split in 2.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }    # start 0
    variables { domain: [ 0, 4 ] }    # start 1
    variables { domain: [ 0, 5 ] }    # start 2
    variables { domain: [ 6, 9 ] }    # start 3
    variables { domain: [ 10, 15 ] }  # start 4
    variables { domain: [ 11, 15 ] }  # start 5
    variables { domain: [ 11, 15 ] }  # start 6
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 3 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        end { vars: 6 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 2 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
        intervals: [ 0, 1, 2, 3, 4, 5, 6 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 6, 9 ] }
    variables { domain: [ 10, 15 ] }
    variables { domain: [ 11, 15 ] }
    variables { domain: [ 11, 15 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 0 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        end { vars: 1 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 2 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 4 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 5 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 6 coeffs: 1 }
        end { vars: 6 coeffs: 1 offset: 2 }
        size { offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 2 }
        intervals: [ 0, 1, 2 ]
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
      }
    }
    constraints {
      cumulative {
        capacity { offset: 2 }
        intervals: [ 3, 4, 5 ]
        demands { offset: 1 }
        demands { offset: 1 }
        demands { offset: 1 }
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ConvertToNoOverlap) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 7 ] }  # capacity
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { vars: 3 coeffs: 1 }
        demands { offset: 4 }
        demands { offset: 4 }
        demands { offset: 4 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 4, 7 ] }  # capacity
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ConvertToNoOverlapVariableDemand) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 4, 6 ] }  # variable demand
    variables { domain: [ 0, 7 ] }  # capacity
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { vars: 4 coeffs: 1 }
        demands { vars: 3 coeffs: 1 }
        demands { vars: 3 coeffs: 1 }
        demands { vars: 3 coeffs: 1 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 4, 6 ] }  # variable demand
    variables { domain: [ 4, 7 ] }  # capacity
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      linear { vars: 3 vars: 4 coeffs: 1 coeffs: -1 domain: -3 domain: 0 }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, NoConvertToNoOverlap) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 7, 8 ] }  # capacity
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { vars: 3 coeffs: 1 }
        demands { offset: 4 }
        demands { offset: 4 }
        demands { offset: 4 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  SatParameters extra_params;
  extra_params.set_symmetry_level(0);
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, extra_params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 9 ] }  # start
    variables { domain: [ 0, 1 ] }  # capacity
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        size { offset: 2 }
        end { vars: 0 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 1 coeffs: 1 }
        size { offset: 2 }
        end { vars: 1 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        size { offset: 2 }
        end { vars: 2 coeffs: 1 offset: 2 }
      }
    }
    constraints {
      cumulative {
        capacity { vars: 3 coeffs: 1 offset: 7 }
        demands { offset: 4 }
        demands { offset: 4 }
        demands { offset: 4 }
        intervals: [ 0, 1, 2 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, ConversionToBoolOr) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 3 ]
        domain: [ 1, 100 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
      domain: [ 0, 3 ]
      scaling_factor: 1
    }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints { bool_or { literals: [ -3, -2, -1 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, ConversionToAtMostOnePositive) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, 2, 3 ]
        domain: [ 0, 3 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
      domain: [ 0, 3 ]
      scaling_factor: 1
    }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, ConversionToAtMostOneNegative) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 2, 3 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  // Note that the order of the literal in the constraint do not change.
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
      domain: [ 0, 3 ]
      scaling_factor: 1
    }
    constraints { at_most_one { literals: [ -3, -2, -1 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, ExtractAtMostOne) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 3, 2, 1 ]
        domain: [ 1, 3 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 3 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
      domain: [ 0, 5 ]
      scaling_factor: 1
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 3, 2, 1 ]
        domain: [ 1, 3 ]
      }
    }
    constraints {
      enforcement_literal: 0
      bool_and { literals: -2 }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, DuplicateLiteralsBoolOr) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints { bool_or { literals: [ 0, 1, 0, 2 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, FalseLiteralBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  SatParameters params;
  params.set_symmetry_level(0);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, TrueLiteralBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  SatParameters params;
  params.set_symmetry_level(0);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 1, 2, 3, 0 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, TwoTrueLiteralBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2, 3, 4 ] } }
  )pb");
  SatParameters params;
  params.set_symmetry_level(0);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, OneActiveLiteralToFalseBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1 ] } }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 0 ] }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, BoolXorNotPresolvedIfEnforcementUnknown) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 2
      bool_xor { literals: [ 0, 1 ] }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  EXPECT_THAT(presolved_model, testing::EqualsProto(initial_model));
}

TEST(PresolveCpModelTest, BoolXorChangedToBoolOrIfAlwaysFalseWhenEnforced) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_xor {}
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BoolXorChangedToBoolOrIfAlwaysFalseWhenEnforced2) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_xor { literals: [ 1, 1 ] }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, BoolXorChangedToBoolOrIfAlwaysFalseWhenEnforced3) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    constraints {
      enforcement_literal: [ 0, 1, 2 ]
      bool_xor { literals: [ 1, -2, 3 ] }
    }
  )pb");
  const CpModelProto presolved_model = PresolveOneConstraint(initial_model, 0);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 1 ] }
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, OneActiveLiteralToTrueBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2 ] } }
  )pb");
  const CpModelProto presolved_model = GetReducedDomains(initial_model);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 1, 1 ] }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, TwoActiveLiteralsAndTrueBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2 ] } }
  )pb");
  SatParameters params;
  params.set_symmetry_level(0);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, TwoActiveLiteralsAndFalseBoolXor) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2 ] } }
  )pb");
  SatParameters params;
  params.set_symmetry_level(0);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, SetPPCRedundentConstraints) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, SetPPCDominatedConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints { at_most_one { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, SetPPCFixVariables) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints { at_most_one { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, DuplicateInAtMostOne) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2, 3, 2 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, CanonicalBinaryVarAndTable) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ -1, -1, 1, 1 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        values: [ 0, -1, 1, -1, 2, -1, 2, 1 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        values: [ 0, 0, 1, 0, 2, 0, 2, 1 ]
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, DuplicateVariablesInTable) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        values: [
          0, 0, 0, 0, 1, -1, 0, 0, 1, 0, 0, 0, 1, -1, 1, 1, 2, -2, 2, 2
        ]
      }
    }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        values: [ 0, 0, 1, 0, 1, 1, 2, 2 ]
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, CanonicalAffineVar) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0, 2, 2, 4, 4 ] }
    variables { domain: [ 1, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 2 ]
        domain: [ 3, 1000 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 1, 10 ] }
    constraints {
      linear {
        vars: [ 1, 0 ]
        coeffs: [ 1, 1 ]
        domain: [ 2, 12 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, IdempotentElement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1, 3, 4 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 5, 5 ]
      }
    }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 0 coeffs: 1 }
        exprs { offset: 1 }
        exprs { offset: 1 }
        exprs { offset: 3 }
        exprs { offset: 3 }
        exprs { offset: 4 }
        exprs { offset: 12 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, AffineElement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 7 ] }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 1 coeffs: 1 }
        exprs { offset: 1 }
        exprs { offset: 2 }
        exprs { offset: 3 }
        exprs { offset: 4 }
        exprs { offset: 5 }
        exprs { offset: 6 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 1, 6 ] }
    constraints {
      linear {
        vars: [ 1, 0 ]
        coeffs: [ 1, -1 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, AffineElementWithScaledBooleanIndex) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0, 3, 3 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 1 coeffs: 1 }
        exprs { offset: 2 }
        exprs { offset: 0 }
        exprs { offset: 0 }
        exprs { offset: 0 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0, 3, 3 ] }
    variables { domain: [ 0, 0, 2, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear { vars: 0 vars: 2 coeffs: 1 coeffs: -3 domain: 0 domain: 0 }
    }
    constraints {
      linear { vars: 1 vars: 2 coeffs: 1 coeffs: 2 domain: 2 domain: 2 }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, AffineElementWithNonIntegerSlope) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 6 ] }
    variables { domain: [ -2, 2 ] }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 1 coeffs: 1 }
        exprs { offset: 2 }
        exprs { offset: 5 }
        exprs { offset: 6 }
        exprs { offset: 0 }
        exprs { offset: 7 }
        exprs { offset: 8 }
        exprs { offset: -2 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0, 3, 3, 6, 6 ] }
    variables { domain: [ -2, -2, 0, 0, 2, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear { vars: 0 vars: 2 coeffs: 1 coeffs: 3 domain: 6 domain: 6 }
    }
    constraints {
      linear { vars: 1 vars: 2 coeffs: 1 coeffs: -2 domain: -2 domain: -2 }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, ReduceDomainsInAutomaton) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1, 3, 3, 6, 10 ] }
    variables { domain: [ 1, 1, 3, 3, 6, 6 ] }
    variables { domain: [ 1, 3, 6, 6 ] }
    constraints {
      automaton {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        transition_tail: [ 4, 1, 1, 1, 2, 3, 4 ]
        transition_head: [ 4, 2, 3, 4, 2, 3, 4 ]
        transition_label: [ 4, 1, 3, 6, 1, 3, 6 ]
        starting_state: 1
        final_states: [ 2, 3, 4 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1, 3, 3, 6, 6 ] }
    variables { domain: [ 1, 1, 3, 3, 6, 6 ] }
    variables { domain: [ 1, 1, 3, 3, 6, 6 ] }
    constraints {
      automaton {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        transition_tail: [ 4, 1, 1, 1, 2, 3, 4 ]
        transition_head: [ 4, 2, 3, 4, 2, 3, 4 ]
        transition_label: [ 4, 1, 3, 6, 1, 3, 6 ]
        starting_state: 1
        final_states: [ 2, 3, 4 ]
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, UnsatIntegerLinearConstraint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 7, 6, 5 ]
        domain: [ 1, 4 ]
      }
    }
  )pb");
  ExpectInfeasibleDuringPresolve(initial_model);
}

TEST(PresolveCpModelTest, LinMaxCanBeRemoved) {
  // The target variable is not constraining after simple propagation and not
  // used anywhere else.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -15, 8 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -4, 11 ] }
    constraints {
      lin_max {
        target: { vars: 0 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
        exprs: { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 1, 2 ] } }
  )pb");
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  std::vector<int> mapping;
  Model model;
  auto* params = model.GetOrCreate<SatParameters>();
  params->set_permute_variable_randomly(false);
  params->set_cp_model_probing_level(0);
  PresolveContext context(&model, &presolved_model, &mapping_model);
  CpModelPresolver presolver(&context, &mapping);
  presolver.Presolve();

  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: [ -2, 8 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -4, 8 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(expected_mapping_model, testing::EqualsProto(mapping_model));
}

TEST(PresolveCpModelTest, LinMaxCannotBeRemoved) {
  // Almost the same as above, but the target of the int_max might constraint
  // the other variable via its lower bound, so we cannot remove it.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -4, 11 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -4, 8 ] }
    constraints {
      lin_max {
        target: { vars: 0 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
        exprs: { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, LinMaxCannotBeRemovedWithHoles) {
  // Almost the same as above, but the target does not contains the infered
  // domain.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -4, 2, 5, 11 ] }
    variables { domain: [ 0, 2, 4, 8 ] }
    variables { domain: [ -2, 1, 4, 7 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2, 5, 8 ] }
    variables { domain: [ 0, 2, 4, 8 ] }
    variables { domain: [ -2, 1, 4, 7 ] }
    constraints {
      lin_max {
        target: { vars: 0 coeffs: 1 }
        exprs: { vars: 1 coeffs: 1 }
        exprs: { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, DetectVarValueEncoding) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 1, 3 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: 1
        coeffs: 1
        domain: [ 2, 2 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: 1
        coeffs: 1
        domain: [ 1, 1, 3, 3 ]
      }
    }
  )pb");
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = initial_model;
  CpModelProto mapping_model;
  std::vector<int> mapping;
  PresolveContext context(&model, &presolved_model, &mapping_model);
  PresolveCpModel(&context, &mapping);
  int encoding_literal = -1;
  EXPECT_TRUE(context.HasVarValueEncoding(1, int64_t{2}, &encoding_literal));
  EXPECT_EQ(encoding_literal, 0);
}

TEST(FindDuplicateConstraintsTest, BasicTest) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -4, 11 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 2 }
      }
    }
    constraints {
      name: "name are ignored"
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 2 }
      }
    }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 2 }
      }
    }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 2 }
      }
    }
  )pb");

  std::vector<std::pair<int, int>> duplicates = FindDuplicateConstraints(model);
  EXPECT_THAT(duplicates,
              ::testing::ElementsAre(std::make_pair(1, 0), std::make_pair(2, 0),
                                     std::make_pair(3, 0)));
}

TEST(FindDuplicateConstraintsTest, LinearConstraintParallelToObjective) {
  const CpModelProto model = ParseTestProto(R"pb(
    constraints {
      name: "name are ignored"
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 3, 3, 7 ]
      }
    }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 3, 3, 7 ]
    }
  )pb");

  std::vector<std::pair<int, int>> duplicates = FindDuplicateConstraints(model);
  EXPECT_THAT(duplicates,
              ::testing::ElementsAre(std::make_pair(0, kObjectiveConstraint)));
}

TEST(DetectDuplicateConstraintsTest, DifferentRedundantEnforcement) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 4 ]
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 2, 3, 4, 1 ]
        domain: [ 0, 6 ]
      }
    }
    constraints {
      enforcement_literal: [ 5 ]
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 2, 3, 4, 1 ]
        domain: [ 0, 6 ]
      }
    }
    constraints {
      enforcement_literal: [ 4, 5 ]
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 2, 3, 4, 1 ]
        domain: [ 0, 6 ]
      }
    }
    constraints { bool_or { literals: [ -5, 5 ] } }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 5 ]
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 2, 3, 4, 1 ]
        domain: [ 0, 6 ]
      }
    }
    constraints {
      enforcement_literal: -6
      bool_and { literals: -5 }
    })pb");

  SatParameters params;
  params.set_cp_model_probing_level(2);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);

  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EncodingIssue) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0, 2, 2 ] }
    variables { domain: [ 0, 0, 3, 3 ] }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target: { offset: 1 }
        exprs { offset: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
    constraints {
      linear {
        vars: [ 1, 2, 3 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 4 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
        exprs { offset: 2 }
        exprs { offset: 0 }
        exprs { offset: 0 }
      }
    }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 5 coeffs: 1 }
        exprs { vars: 5 coeffs: 1 }
        exprs { offset: 0 }
        exprs { offset: 3 }
        exprs { offset: 0 }
      }
    }
  )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  const CpSolverResponse response = SolveWithParameters(model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

// This test was failing with the wrong optimal.
TEST(PresolveCpModelTest, FailedRandomTest) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 7, 7 ] }
    variables { domain: [ -5, 8 ] }
    variables { domain: [ -6, -2 ] }
    variables { domain: [ -8, -2 ] }
    variables { domain: [ -7, -4 ] }
    variables { domain: [ -4, 7 ] }
    constraints {
      linear {
        vars: 0
        vars: 2
        vars: 4
        vars: 5
        coeffs: -8
        coeffs: 7
        coeffs: -35
        coeffs: 80
        domain: -42
        domain: -21
      }
    }
    constraints {
      linear {
        vars: 0
        vars: 2
        vars: 4
        vars: 5
        coeffs: 40
        coeffs: -35
        coeffs: 175
        coeffs: -400
        domain: 105
        domain: 105
        domain: 110
        domain: 110
        domain: 115
        domain: 115
        domain: 120
        domain: 120
        domain: 125
        domain: 125
        domain: 130
        domain: 130
        domain: 135
        domain: 135
        domain: 140
        domain: 140
        domain: 145
        domain: 145
        domain: 150
        domain: 150
        domain: 155
        domain: 155
        domain: 160
        domain: 160
        domain: 165
        domain: 165
        domain: 170
        domain: 170
        domain: 175
        domain: 175
        domain: 180
        domain: 180
        domain: 185
        domain: 185
        domain: 190
        domain: 190
        domain: 195
        domain: 195
        domain: 200
        domain: 200
        domain: 205
        domain: 205
        domain: 210
        domain: 210
      }
    }
    constraints {
      linear {
        vars: 0
        vars: 2
        vars: 4
        vars: 5
        coeffs: -8
        coeffs: 7
        coeffs: -35
        coeffs: 80
        domain: -42
        domain: -21
      }
    }
    constraints {
      linear {
        vars: 0
        vars: 2
        vars: 4
        vars: 5
        coeffs: 40
        coeffs: -35
        coeffs: 175
        coeffs: -400
        domain: 105
        domain: 105
        domain: 110
        domain: 110
        domain: 115
        domain: 115
        domain: 120
        domain: 120
        domain: 125
        domain: 125
        domain: 130
        domain: 130
        domain: 135
        domain: 135
        domain: 140
        domain: 140
        domain: 145
        domain: 145
        domain: 150
        domain: 150
        domain: 155
        domain: 155
        domain: 160
        domain: 160
        domain: 165
        domain: 165
        domain: 170
        domain: 170
        domain: 175
        domain: 175
        domain: 180
        domain: 180
        domain: 185
        domain: 185
        domain: 190
        domain: 190
        domain: 195
        domain: 195
        domain: 200
        domain: 200
        domain: 205
        domain: 205
        domain: 210
        domain: 210
      }
    }
    objective { vars: 3 vars: 1 vars: 2 coeffs: 37 coeffs: -18 coeffs: -7 }
  )pb");
  SatParameters params;
  params.set_log_search_progress(true);
  const CpSolverResponse response = SolveWithParameters(model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.objective_value(), -419);
}

TEST(PresolveCpModelTest, DetectDuplicateVarEqValueEncoding) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 9 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 6, 6 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 5, 7, 9 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 6, 6 ]
      }
    }
    constraints {
      enforcement_literal: -2
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 5, 7, 9 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 9 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 6, 6 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 0, 5, 7, 9 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EqualityWithOnlyTwoOddBooleans) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 99 ] }
    variables { domain: [ 0, 99 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 3, 4, 4 ]
        domain: [ 60, 60 ]
      }
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 15 ] }
    variables { domain: [ 0, 15 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 15, 15 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, DualEquality) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 99 ] }
    variables { domain: [ 0, 99 ] }
    constraints {
      enforcement_literal: 0
      bool_and { literals: 1 }
    }
    # Anything that we don't know how to presolve.
    # TODO(user): could be nice to had a "unknown" constraint for this purpose.
    constraints {
      all_diff {
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ -1, 1, 1, 1 ]
    }
  )pb");
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 99 ] }
    variables { domain: [ 0, 99 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    objective {
      vars: [ 1, 2 ]
      coeffs: [ 1, 1 ]
      scaling_factor: 1
      domain: [ 0, 198 ]
    }
  )pb");

  CpModelProto presolved_model = PresolveForTest(initial_model);
  EXPECT_THAT(presolved_model, EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EmptyProduct) {
  // A rho shape.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    constraints { int_prod { target: { vars: 0 coeffs: 1 } } }
    constraints { dummy_constraint { vars: [ 0 ] } }
  )pb");
  CpModelProto presolved_model = PresolveForTest(initial_model);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1 ] }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, ElementWithTargetEqualIndex) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 1, 1 ] }  # 0
    variables { domain: [ 0, 4 ] }  # 1 - ok
    variables { domain: [ 3, 7 ] }  # 2
    variables { domain: [ 3, 3 ] }  # 3 - ok
    variables { domain: [ 4, 9 ] }  # 4 - ok
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
        exprs { vars: 5 coeffs: 1 }
      }
    }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: 1 domain: 1 domain: 3 domain: 4 }
    variables { domain: 0 domain: 4 }
    variables { domain: 3 domain: 7 }
    variables { domain: 4 domain: 9 }
    constraints {
      element {
        linear_index { vars: 0 coeffs: 1 }
        linear_target { vars: 0 coeffs: 1 }
        exprs {}
        exprs { vars: 1 coeffs: 1 }
        exprs {}
        exprs { offset: 3 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, ReduceDomainsInInverse) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_direct: [ 0, 1, 2 ]
        f_inverse: [ 3, 4, 5 ]
      }
    }
  )pb");
  const CpModelProto domains = GetReducedDomains(initial_model);
  const CpModelProto expected_domains = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 0, 2, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
  )pb");
  EXPECT_THAT(expected_domains, testing::EqualsProto(domains));
}

TEST(PresolveCpModelTest, RemoveZeroEventsFromReservoir) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 11 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        min_level: 0
        max_level: 10
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        time_exprs { vars: 3 coeffs: 1 }
        active_literals: [ 4, 4, 5, 6 ]
        level_changes: { offset: 3 }
        level_changes: { offset: 0 }
        level_changes: { offset: 3 }
        level_changes: { offset: -2 }
      }
    }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 11 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        min_level: 0
        max_level: 6
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        active_literals: [ 3, 4, 5 ]
        level_changes: { offset: 3 }
        level_changes: { offset: 3 }
        level_changes: { offset: -2 }
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, RemoveInactiveEventsFromReservoir) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 11 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        min_level: 0
        max_level: 10
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        time_exprs { vars: 3 coeffs: 1 }
        active_literals: [ 4, 4, 5, 6 ]
        level_changes: { offset: 3 }
        level_changes: { offset: -1 }
        level_changes: { offset: 3 }
        level_changes: { offset: -2 }
      }
    }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 11 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        min_level: 0
        max_level: 3
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        active_literals: [ 2, 3 ]
        level_changes: { offset: 3 }
        level_changes: { offset: -2 }
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, RemoveUnusedEncoding) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 3 ] }
    constraints { dummy_constraint { vars: [ 0, 1, 2 ] } }
    constraints {
      enforcement_literal: 0
      linear {
        vars: 3
        coeffs: 1
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: 3
        coeffs: 1
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: 3
        coeffs: 1
        domain: [ 2, 2 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, RemoveUnusedEncodingWithObjective) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    objective {
      vars: [ 3 ]
      coeffs: [ 1 ]
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2 ] } }
    constraints {
      enforcement_literal: 0
      linear {
        vars: 3
        coeffs: 1
        domain: [ 0, 0 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: 3
        coeffs: 1
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: 3
        coeffs: 1
        domain: [ 2, 2 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_use_sat_presolve(false);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 3, 4, 5 ] } }
    constraints {
      enforcement_literal: -4
      bool_and { literals: -1 }
    }
    constraints {
      enforcement_literal: -5
      bool_and { literals: -2 }
    }
    constraints {
      enforcement_literal: -6
      bool_and { literals: -3 }
    }
    objective: {
      vars: [ 3, 5 ]
      coeffs: [ -1, 1 ]
      scaling_factor: 1
      offset: 1
      integer_before_offset: 1
      domain: [ -1, 1 ]
    }
  )pb");
  EXPECT_THAT(presolved_model,
              ModelEqualsIgnoringConstraintsOrder(expected_presolved_model));
}

TEST(PresolveCpModelTest, RemovableEnforcementLiteral) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints { dummy_constraint { vars: [ 1, 2, 3 ] } }
    constraints {
      enforcement_literal: 0
      linear {
        vars: 1
        coeffs: 1
        domain: [ 0, 5 ]
      }
    }
    constraints {
      enforcement_literal: -1
      linear {
        vars: 1
        coeffs: 1
        domain: [ 4, 7 ]
      }
    }
  )pb");
  const CpModelProto presolved_model = PresolveForTest(initial_model);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 7 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, LinearAndExactlyOne) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 5 ] }
    constraints { exactly_one { literals: [ 0, 1, 2 ] } }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 2, 3, 4, 1 ]
        domain: [ 0, 6 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 4 ] }
    constraints { exactly_one { literals: [ 0, 1, 2 ] } }
    constraints {
      linear {
        vars: [ 1, 2, 3 ]
        coeffs: [ 1, 2, 1 ]
        domain: [ 0, 4 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinearAndAtMostOnePropagation) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 50 ] }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 2, 3, 4, -1 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  // Not only we need to consider the pair of constraint to know that the
  // last variable is <= 4, but once we know that, we can extract the variable
  // with coefficient 4 as an enforcement literal.
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 4 ] }
    constraints {
      enforcement_literal: -3
      linear {
        vars: [ 0, 1, 3 ]
        coeffs: [ 2, 3, -1 ]
        domain: [ 0, 5 ]
      }
    }
    constraints { at_most_one { literals: [ 0, 1, 2 ] } }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinMaxWithBoolean) {
  const int num_vars = 5;
  const int max_value = 4;
  absl::BitGen random;

  for (int runs = 0; runs < 1000; ++runs) {
    // Create num_vars variable that are either fixed or have two values.
    CpModelProto cp_model;
    for (int i = 0; i < num_vars; ++i) {
      FillDomainInProto(
          Domain::FromValues({absl::Uniform<int>(random, 0, max_value + 1),
                              absl::Uniform<int>(random, 0, max_value + 1)}),
          cp_model.add_variables());
    }

    // We randomize the variable to have duplicates.
    LinearArgumentProto* lin_max =
        cp_model.add_constraints()->mutable_lin_max();
    lin_max->mutable_target()->add_vars(
        absl::Uniform<int>(random, 0, num_vars));
    lin_max->mutable_target()->add_coeffs(1);
    for (int i = 0; i < absl::Uniform<int>(random, 0, num_vars); ++i) {
      LinearExpressionProto* expr = lin_max->add_exprs();
      expr->add_vars(absl::Uniform<int>(random, 0, num_vars));
      expr->add_coeffs(1);
    }

    int num_solutions_without_presolve = 0;
    {
      Model model;
      SatParameters parameters;
      parameters.set_enumerate_all_solutions(true);
      parameters.set_keep_all_feasible_solutions_in_presolve(true);
      parameters.set_cp_model_presolve(false);
      parameters.set_log_search_progress(true);
      model.Add(NewSatParameters(parameters));
      model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
        num_solutions_without_presolve++;
      }));
      SolveCpModel(cp_model, &model);
    }

    int num_solutions_with_presolve = 0;
    {
      Model model;
      SatParameters parameters;
      parameters.set_enumerate_all_solutions(true);
      parameters.set_keep_all_feasible_solutions_in_presolve(true);
      parameters.set_log_search_progress(true);
      model.Add(NewSatParameters(parameters));
      model.Add(NewFeasibleSolutionObserver(
          [&](const CpSolverResponse& r) { num_solutions_with_presolve++; }));
      SolveCpModel(cp_model, &model);
    }

    // Note that the solution are checked by the checker, so there is not really
    // any need to compare that we get exactly the same ones.
    ASSERT_EQ(num_solutions_with_presolve, num_solutions_without_presolve)
        << ProtobufDebugString(cp_model);
  }
}

TEST(PresolveCpModelTest, Bug174584992) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -288230376151711744, 262144 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      name: "T"
      linear { vars: 1 vars: 0 coeffs: 1 coeffs: 2 }
    }
  )pb");

  Model tmp_model;
  EXPECT_EQ(SolveCpModel(initial_model, &tmp_model).status(),
            CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, DetectInfeasibilityDuringMerging) {
  ExpectInfeasibleDuringPresolve(ParseTestProto(R"pb(
    variables { domain: [ -100, 100 ] }
    variables { domain: [ -100, 100 ] }
    variables { domain: [ -100, 100 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 3 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 3 ]
        domain: [ 11, 20 ]
      }
    }
  )pb"));
}

TEST(PresolveCpModelTest, DetectEncodingFromLinear) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -100, 100 ] }
    constraints { exactly_one { literals: [ 0, 1, 2, 3, 4 ] } }
    constraints {
      linear {
        vars: [ 0, 1, 3, 4, 5 ]
        coeffs: [ 1, 7, -2, 4, 1 ]
        domain: [ 10, 10 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  IntegerVariableProto expected_proto;
  FillDomainInProto(Domain::FromValues({3, 6, 9, 10, 12}), &expected_proto);
  // The values are 10, 10-1, 10-7, 10+2, and 10-4.
  EXPECT_THAT(presolved_model.variables(),
              testing::Contains(testing::EqualsProto(expected_proto)));
}

TEST(PresolveCpModelTest, ReplaceNonEqual) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2, 4, 6 ]
      }
    }
  )pb");
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 3 }
    variables { domain: 0 domain: 3 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {
      enforcement_literal: 2
      linear { vars: 0 coeffs: 1 domain: 0 domain: 0 }
    }
    constraints {
      enforcement_literal: -3
      linear { vars: 0 coeffs: 1 domain: 1 domain: 3 }
    }
    constraints {
      enforcement_literal: 3
      linear { vars: 1 coeffs: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: -4
      linear { vars: 1 coeffs: 1 domain: 0 domain: 2 }
    }
    constraints {
      enforcement_literal: 4
      linear { vars: 0 coeffs: 1 domain: 1 domain: 1 }
    }
    constraints {
      enforcement_literal: -5
      linear { vars: 0 coeffs: 1 domain: 0 domain: 0 domain: 2 domain: 3 }
    }
    constraints {
      enforcement_literal: 5
      linear { vars: 1 coeffs: 1 domain: 2 domain: 2 }
    }
    constraints {
      enforcement_literal: -6
      linear { vars: 1 coeffs: 1 domain: 0 domain: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: 6
      linear { vars: 0 coeffs: 1 domain: 2 domain: 2 }
    }
    constraints {
      enforcement_literal: -7
      linear { vars: 0 coeffs: 1 domain: 0 domain: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: 7
      linear { vars: 1 coeffs: 1 domain: 1 domain: 1 }
    }
    constraints {
      enforcement_literal: -8
      linear { vars: 1 coeffs: 1 domain: 0 domain: 0 domain: 2 domain: 3 }
    }
    constraints {
      enforcement_literal: 8
      linear { vars: 0 coeffs: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: -9
      linear { vars: 0 coeffs: 1 domain: 0 domain: 2 }
    }
    constraints {
      enforcement_literal: 9
      linear { vars: 1 coeffs: 1 domain: 0 domain: 0 }
    }
    constraints {
      enforcement_literal: -10
      linear { vars: 1 coeffs: 1 domain: 1 domain: 3 }
    }
    constraints {
      enforcement_literal: 2
      bool_and { literals: -4 }
    }
    constraints {
      enforcement_literal: 4
      bool_and { literals: -6 }
    }
    constraints {
      enforcement_literal: 6
      bool_and { literals: -8 }
    }
    constraints {
      enforcement_literal: 8
      bool_and { literals: -10 }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, OrToolsIssue2924) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1000 ] }  # This lower bound caused issues.
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 1 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 80, 100, 120, -1 ]
        domain: [ 95, 95 ]
      }
    }
    objective {
      vars: [ 3 ]
      coeffs: [ 1 ]
    }
  )pb");
  SatParameters params;
  params.set_log_search_progress(true);
  EXPECT_EQ(SolveWithParameters(initial_model, params).status(),
            CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, AtMostOneAndLinear) {
  // Using the at most one, the linear constraint will be always true.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 4 ] }  # variable 4
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 4 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 5 ]
      }
    }
    constraints { at_most_one { literals: [ 0, 1, 2, 5 ] } }
  )pb");

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 0, 1, 2, 5 ] } }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, AtMostOneWithSingleton) {
  // Using the at most one, the linear constraint will be always true.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 100 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 1534, 5646, 4564, 145, 178 ]
        domain: [ 47888, 53888 ]
      }
    }
    constraints { at_most_one { literals: [ 3, 4, 5 ] } }
    objective {
      vars: [ 0, 1, 2, 3, 4, 5 ]
      coeffs: [ 1534, 5646, 4564, -878, -787, -874 ]
    }
  )pb");

  // We transform the at most one to exactly one and then shift the cost to
  // the other variable so we can remove a singleton.
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 32 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 11 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 1534, 5646, 4564, 145, 178 ]
        domain: [ 47888, 53888 ]
      }
    }
    constraints {
      enforcement_literal: 3
      bool_and { literals: [ -5 ] }
    }
    objective {
      scaling_factor: 1
      offset: -874
      integer_before_offset: -874
      vars: [ 0, 1, 2, 3, 4 ]
      coeffs: [ 1534, 5646, 4564, -4, 87 ]
      domain: [ -4, 150193 ]
    }
  )pb");

  SatParameters params;
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, PresolveDiophantinePreservesSolutionHint) {
  // Diophantine equation: https://miplib.zib.de/instance_details_ej.html.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 10000000 ] }
    variables { domain: [ 0, 10000000 ] }
    variables { domain: [ 0, 10000000 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 31013, -41014, -51015 ]
        domain: [ 0, 0 ]
      }
    }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 25508, 1, 15506 ]
    }
  )pb");

  SatParameters params;
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  ASSERT_EQ(presolved_model.solution_hint().vars_size(),
            presolved_model.variables_size());
  std::vector<int64_t> solution_hint(presolved_model.variables_size());
  for (int i = 0; i < presolved_model.solution_hint().vars_size(); ++i) {
    solution_hint[presolved_model.solution_hint().vars(i)] =
        presolved_model.solution_hint().values(i);
  }
  EXPECT_TRUE(SolutionIsFeasible(presolved_model, solution_hint));
}

TEST(PresolveCpModelTest, SolveDiophantine) {
  // Diophantine equation: https://miplib.zib.de/instance_details_ej.html.
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 1, 10000000 ] }
    variables { domain: [ 0, 10000000 ] }
    variables { domain: [ 0, 10000000 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 31013, -41014, -51015 ]
        domain: [ 0, 0 ]
      }
    }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
    }
  )pb");

  SatParameters params;
  params.set_cp_model_presolve(true);
  params.set_num_workers(1);
  // Should solve in < .01 second. Note that deterministic time is not
  // completely accurate.
  params.set_max_deterministic_time(.001);
  const CpSolverResponse response_with =
      SolveWithParameters(model_proto, params);

  EXPECT_EQ(response_with.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response_with.solution(0), 25508);

  // Does not solve without presolving.
  params.set_cp_model_presolve(false);
  const CpSolverResponse response_without =
      SolveWithParameters(model_proto, params);
  EXPECT_NE(response_without.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveCpModelTest, IncompatibleLinear) {
  // a <=> x <= y
  // b <=> x >= y
  // a => not(b)
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 6 ] }
    constraints {
      enforcement_literal: [ 0 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ -6, 0 ]
      }
    }
    constraints {
      enforcement_literal: [ -1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 1, 6 ]
      }
    }
    constraints {
      enforcement_literal: [ 1 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 6 ]
      }
    }
    constraints {
      enforcement_literal: [ -2 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ -6, -1 ]
      }
    }
    constraints {
      enforcement_literal: 0
      bool_and { literals: [ -2 ] }
    }
  )pb");

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 6 ] }
    constraints {
      enforcement_literal: [ 0 ]
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, -1 ]
        domain: [ -6, -1 ]
      }
    }
    constraints {
      enforcement_literal: [ -1 ]
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, -1 ]
        domain: [ 1, 6 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, SearchStrategySurvivePresolve) {
  const CpModelProto proto = ParseTestProto(R"pb(
    variables {
      name: "x"
      domain: [ 1, 10 ]
    }
    variables {
      name: "y"
      domain: [ 3, 8 ]
    }
    search_strategy {
      exprs: { vars: 1 coeffs: 1 }
      exprs: { vars: 0 coeffs: -1 }
      domain_reduction_strategy: SELECT_MAX_VALUE
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(proto, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(proto));
}

TEST(PresolveCpModelTest, AmoRectangle) {
  // We need a large rectangle, so we generate this by hand.
  CpModelProto model;
  for (int i = 0; i < 100; ++i) {
    auto* var = model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  for (int i = 0; i < 10; ++i) {
    auto* var = model.add_variables();
    var->add_domain(0);
    var->add_domain(5);
  }
  {
    auto* amo = model.add_constraints()->mutable_at_most_one();
    for (int i = 0; i < 100; ++i) amo->add_literals(i);
  }
  {
    auto* linear = model.add_constraints()->mutable_linear();
    for (int i = 0; i < 100; ++i) {
      linear->add_vars(i);
      linear->add_coeffs(1);
    }
    linear->add_vars(100);
    linear->add_coeffs(1);
    linear->add_vars(101);
    linear->add_coeffs(1);
    linear->add_domain(0);
    linear->add_domain(5);
  }
  {
    auto* linear = model.add_constraints()->mutable_linear();
    for (int i = 0; i < 100; ++i) {
      linear->add_vars(i);
      linear->add_coeffs(3);
    }
    linear->add_vars(102);
    linear->add_coeffs(1);
    linear->add_vars(103);
    linear->add_coeffs(1);
    linear->add_domain(0);
    linear->add_domain(5);
  }
  {
    auto* linear = model.add_constraints()->mutable_linear();
    for (int i = 0; i < 100; ++i) {
      linear->add_vars(i);
      linear->add_coeffs(-2);
    }
    linear->add_vars(104);
    linear->add_coeffs(1);
    linear->add_vars(105);
    linear->add_coeffs(1);
    linear->add_domain(0);
    linear->add_domain(5);
  }

  CpModelProto expected_presolved_model;
  for (int i = 0; i < 100; ++i) {
    auto* var = expected_presolved_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  for (int i = 0; i < 10; ++i) {
    auto* var = expected_presolved_model.add_variables();
    var->add_domain(0);
    var->add_domain(5);
  }
  {
    // New new variable.
    auto* var = expected_presolved_model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  {
    auto* linear = expected_presolved_model.add_constraints()->mutable_linear();
    linear->add_vars(100);
    linear->add_coeffs(1);
    linear->add_vars(101);
    linear->add_coeffs(1);
    linear->add_vars(110);
    linear->add_coeffs(1);
    linear->add_domain(0);
    linear->add_domain(5);
  }
  {
    auto* linear = expected_presolved_model.add_constraints()->mutable_linear();
    linear->add_vars(102);
    linear->add_coeffs(1);
    linear->add_vars(103);
    linear->add_coeffs(1);
    linear->add_vars(110);
    linear->add_coeffs(3);
    linear->add_domain(0);
    linear->add_domain(5);
  }
  {
    auto* linear = expected_presolved_model.add_constraints()->mutable_linear();
    linear->add_vars(104);
    linear->add_coeffs(1);
    linear->add_vars(105);
    linear->add_coeffs(1);
    linear->add_vars(110);
    linear->add_coeffs(-2);
    linear->add_domain(0);
    linear->add_domain(5);
  }
  {
    auto* exo =
        expected_presolved_model.add_constraints()->mutable_exactly_one();
    exo->add_literals(NegatedRef(110));
    for (int i = 0; i < 100; ++i) exo->add_literals(i);
  }

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, PreserveHints) {
  const CpModelProto input_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 1, 4, 4 ] }
    variables { domain: [ 0, 0, 3, 3, 9, 9 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 1, 10 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 9 ]
    }
  )pb");

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 domain: 3 domain: 3 }
    variables { domain: 0 domain: 1 }
    constraints {
      enforcement_literal: 2
      linear { vars: 1 coeffs: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: -3
      linear { vars: 1 coeffs: 1 domain: 0 domain: 1 }
    }
    constraints {
      enforcement_literal: 0
      bool_and { literals: -3 }
    }
    solution_hint { vars: 0 vars: 1 vars: 2 values: 0 values: 3 values: 1 }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  CpModelProto presolved_model = PresolveForTest(input_model, params);
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, DuplicateColumns) {
  CpModelProto presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 1, 2, 3 ] } }
    constraints {
      linear {
        vars: [ 0, 2, 3 ]
        coeffs: [ 1, 2, 2 ]
        domain: [ 1, 10 ]
      }
    }
  )pb");

  Model model;
  CpModelProto mapping_model;
  PresolveContext context(&model, &presolved_model, &mapping_model);
  std::vector<int> mapping;
  CpModelPresolver presolver(&context, &mapping);

  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();
  presolver.DetectDuplicateColumns();
  context.WriteVariableDomainsToProto();

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 1, 4 ] } }
    constraints {
      linear {
        vars: [ 0, 4 ]
        coeffs: [ 1, 2 ]
        domain: [ 1, 10 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_model));
}

TEST(PresolveCpModelTest, TrivialAfterPresolveWithVariousOffsets) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
    floating_point_objective {
      vars: [ 0, 2 ]
      coeffs: [ 1, 1 ]
      maximize: true
    }
  )pb");

  const CpSolverResponse response = Solve(initial_model);

  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.objective_value(), 2);
}

TEST(PresolveCpModelTest, EmptyDomain) {
  // The model checker doesn't allow empty domains, but we still might generate
  // them in LNS.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [] }
  )pb");

  PresolveForTest(initial_model, SatParameters(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveCpModelTest, CanonicalizeAndRemapRoutesConstraintNodeVariables) {
  // A complete graph with 3 nodes and the following arcs:
  // 0 --l0-> 1 --l2-> 2 --l4-> 0
  // 0 <-l1-- 1 <-l3-- 2 <-l5-- 0
  //
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    # unused, should be removed.
    variables { domain: [ 0, 0 ] }
    # fixed value, should be removed.
    variables { domain: [ 5, 5 ] }
    variables { domain: [ 0, 10 ] }
    # should be replaced with an affine representative in [0, 4]
    variables { domain: [ 0, 0, 2, 2, 4, 4, 6, 6, 8, 8 ] }
    constraints {
      routes {
        tails: [ 0, 1, 1, 2, 2, 0 ]
        heads: [ 1, 0, 2, 1, 0, 2 ]
        literals: [ 0, 1, 2, 3, 4, 5 ]
        dimensions: {
          exprs {
            vars: [ 7 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 8 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 9 ]
            coeffs: [ 1 ]
          }
        }
      }
    }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 7, 8 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 8, 7 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 8, 9 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 9, 8 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 4
      linear {
        vars: [ 9, 7 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 7, 9 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
  )pb");

  SatParameters params;
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 4 ] }
    constraints {
      routes {
        tails: [ 0, 1, 1, 2, 2, 0 ]
        heads: [ 1, 0, 2, 1, 0, 2 ]
        literals: [ 0, 1, 2, 3, 4, 5 ]
        dimensions: {
          exprs { offset: 5 }
          exprs {
            vars: [ 6 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 7 ]
            coeffs: [ 2 ]
          }
        }
      }
    }
    # ... more constraints (omitted) ...
  )pb");
  EXPECT_THAT(presolved_model.variables(),
              testing::Pointwise(testing::EqualsProto(),
                                 expected_presolved_model.variables()));
  EXPECT_THAT(presolved_model.constraints(0),
              testing::EqualsProto(expected_presolved_model.constraints(0)));
}

TEST(PresolveCpModelTest, InnerObjectiveLowerBound) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 10 ] }
    variables { domain: [ -1647, 504, 3054, 3054 ] }
    constraints {
      linear {
        vars: 0
        vars: 1
        coeffs: 2
        coeffs: 1
        domain: [ 10, 10 ]
      }
    }
    objective {
      vars: 1
      coeffs: 2
      domain: [ 8, 10 ]
    }
  )pb");

  const CpSolverResponse r = Solve(initial_model);
  EXPECT_EQ(r.inner_objective_lower_bound(), 8);
}

TEST(PresolveCpModelTest, ModelWithoutVariables) {
  const CpModelProto cp_model = ParseTestProto(
      R"pb(
        constraints {
          all_diff {
            exprs { offset: 1 }
            exprs { offset: 2 }
          }
        }
      )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  params.set_cp_model_presolve(false);

  CpSolverResponse response = SolveWithParameters(cp_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
