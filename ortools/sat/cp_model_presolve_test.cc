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
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::EqualsProto;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Le;

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
    const CpModelProto& initial_model, const SatParameters& extra_params,
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
CpModelProto GetMappingModel(const CpModelProto& initial_model,
                             const SatParameters& extra_params) {
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
CpModelProto GetReducedDomains(const CpModelProto& initial_model) {
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

void ExpectInfeasibleDuringPresolve(const CpModelProto& initial_model) {
  PresolveForTest(initial_model, SatParameters(), CpSolverStatus::INFEASIBLE);
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
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
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
  // NOTE(user): I tried a random small problem. Note that the conversion to LP
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
  CHECK(ClampVariableBounds(SatParameters(), &mp_model, &logger));
  const std::vector<double> zero_errors(mp_model.variable_size(), 0.0);
  ConvertMPModelProtoToCpModelProto(
      SatParameters(), MPModelProtoWrapper(std::move(mp_model)),
      /*num_clamped=*/0, zero_errors, zero_errors, &initial_model, &logger);

  const CpModelProto mapping_model =
      GetMappingModel(initial_model, SatParameters());

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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, MoreAdvancedPresolve) {
  // We can remove variable zero from the max since it does not change the
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  EXPECT_THAT(presolved_model, testing::EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinMaxExprEqualTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: -16777224 domain: 1 }
    constraints {
      lin_max {
        target { vars: 0 coeffs: -1 }
        exprs { vars: 0 coeffs: -1 }
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
        exprs { vars: 0 coeffs: -1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
        exprs { vars: 0 coeffs: -1 }
        exprs { vars: 0 coeffs: 1 }
      }
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  // Make sure we can only remove the variable 1 here.
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto mapping_model =
      GetMappingModel(initial_model, SatParameters());
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

  const CpModelProto mapping_model =
      GetMappingModel(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto mapping_model =
      GetMappingModel(initial_model, SatParameters());
  const CpModelProto expected_mapping_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 0 }
    variables { domain: 1 domain: 1 }
  )pb");
  EXPECT_THAT(mapping_model, testing::EqualsProto(expected_mapping_model));
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
  CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
    constraints { bool_or { literals: [ 0, -2, 2, -4, 4, -6 ] } }
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
  params.set_max_domain_size_for_linear2_expansion(0);
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
  params.set_max_domain_size_for_linear2_expansion(0);
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  // Note that the variable #2 is removed, but this does not remove any feasible
  // solution since its value will be uniquely determined via the removed
  // constraint x0 + 2x1 + x2 = 3. The objective domain constrains x0 + 2x1
  // to take feasible values for x3.
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
  const CpModelProto mapping_model =
      GetMappingModel(initial_model, SatParameters());
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
// detecting infeasibility since some mandatory nodes are not in the 0 <-> 1
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
  const CpModelProto mapping_model =
      GetMappingModel(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  presolver.DetectDifferentVariables();
  context.WriteVariableDomainsToProto();

  bool has_no_overlap_constraint = false;
  for (const ConstraintProto& constraint : cp_model.constraints()) {
    if (constraint.has_no_overlap()) {
      has_no_overlap_constraint = true;
      break;
    }
  }
  EXPECT_FALSE(has_no_overlap_constraint);
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto mapping_model =
      GetMappingModel(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, -1 ]
        domain: [ 7, 17 ]
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
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, -1 ]
        domain: [ 14, 24 ]
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
    constraints { bool_or { literals: [ -1, -2, -3 ] } }
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

TEST(PresolveCpModelTest, LinearRedundantUnderKnapsack) {
  // With sum(x) <= 3, the max activity of the second constraint is 3 * 13 = 39
  // which is below its rhs, so it is redundant. This cannot be detected by
  // looking at the variable domains only (max activity 3 * 46 = 138).
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 10, 11, 12, 13 ]
        domain: [ 0, 40 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
  )pb");
  EXPECT_THAT(expected_presolved_model, testing::EqualsProto(presolved_model));
}

TEST(PresolveCpModelTest, LinearNotRedundantUnderKnapsack) {
  // Same as above, but with a smaller rhs. Note that the domain of x3 will be
  // reduced to [0, 2] first, and the max activity under sum(x) <= 3 is then
  // 13 * 2 + 12 = 38 > 37.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 10, 11, 12, 13 ]
        domain: [ 0, 37 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(presolved_model.constraints_size(), 2);
}

TEST(PresolveCpModelTest, LinearWithHoleInRhsNotRedundantUnderKnapsack) {
  // Eliminating singleton variable y (var 4) with domain [0, 10] U [40, 50]
  // leaves a linear constraint with a hole (10, 40) in its rhs. Under
  // sum(x) <= 3, the activity is in [0, 39], which fits inside [rhs.Min(),
  // rhs.Max()] = [0, 50] but spans the hole (10, 40), so the constraint is NOT
  // redundant (deleting it makes postsolve fail when activity is e.g. 39).
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 10, 40, 50 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 10, 11, 12, 13, -1 ]
        domain: [ 0, 0 ]
      }
    }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ -10, -11, -12, -13 ]
    }
  )pb");
  const CpSolverResponse response = Solve(initial_model);
  ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.objective_value(), -10.0);
  EXPECT_EQ(response.solution(4), 10);
  EXPECT_TRUE(SolutionIsFeasible(
      initial_model, std::vector<int64_t>(response.solution().begin(),
                                          response.solution().end())));
}

TEST(PresolveCpModelTest, LinearWithHoleInRhsRedundantUnderKnapsack) {
  // Even though min_activity == rhs.Min() == 0 and max_activity == rhs.Max() ==
  // 138, rhs has a hole (40, 60). Under sum(x) <= 3, the activity is in
  // [0, 39], which lies completely inside the first interval [0, 40] of rhs, so
  // the second constraint is redundant.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 10, 11, 12, 13 ]
        domain: [ 0, 40, 60, 138 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
  )pb");
  EXPECT_THAT(presolved_model, EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, LinearWithMiddleIntervalInRhsUnderKnapsack) {
  // Under 2 <= sum(x) <= 3, the activity of 10*x0 + 11*x1 + 12*x2 + 13*x3 is
  // bounded in [20, 39]. It is redundant when [20, 39] lies inside a middle
  // interval of rhs, and not redundant when 20 falls in a hole.
  const CpModelProto redundant_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 2, 3 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 10, 11, 12, 13 ]
        domain: [ 0, 5, 20, 39, 60, 138 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 2, 3 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveForTest(redundant_model, params),
              EqualsProto(expected_presolved_model));

  CpModelProto non_redundant_model = redundant_model;
  non_redundant_model.mutable_constraints(1)->mutable_linear()->set_domain(2,
                                                                           21);
  EXPECT_GT(PresolveForTest(non_redundant_model, params).constraints_size(), 1);
}

TEST(PresolveCpModelTest,
     LinearRedundantUnderSparsifiedKnapsackInSameIteration) {
  // DetectDominatedLinearConstraints subtracts x0 + x1 + x2 == 2 from
  // x0 + x1 + x2 - 2*x3 - 2*x4 - 2*x5 in [-4, 100], leaving -2*x3 - 2*x4 - 2*x5
  // in [-6, 98] (negative leading coeff and GCD 2). Lazy canonicalization on
  // entry to DetectLinearRedundantUnderKnapsack normalizes it to
  // x3 + x4 + x5 in [0, 3], allowing 10*x3 + 11*x4 + 12*x5 <= 37 (max activity
  // 3 * 12 = 36) to be eliminated in the same presolve iteration.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5 ]
        coeffs: [ 1, 1, 1, -2, -2, -2 ]
        domain: [ -4, 100 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4, 5 ]
        coeffs: [ 10, 11, 12 ]
        domain: [ 0, 37 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_max_presolve_iterations(1);
  const CpModelProto expected_presolved_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4, 5 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 3 ]
      }
    }
  )pb");
  EXPECT_THAT(PresolveForTest(initial_model, params),
              EqualsProto(expected_presolved_model));
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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

  std::vector<std::pair<int, int>> duplicates =
      FindDuplicateConstraints(model, false, true, false);
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

  std::vector<std::pair<int, int>> duplicates =
      FindDuplicateConstraints(model, false, true, false);
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

  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  EXPECT_THAT(presolved_model, EqualsProto(expected_presolved_model));
}

TEST(PresolveCpModelTest, EmptyProduct) {
  // A rho shape.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    constraints { int_prod { target: { vars: 0 coeffs: 1 } } }
    constraints { dummy_constraint { vars: [ 0 ] } }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
        f_expr_direct: { vars: 0 coeffs: 1 }
        f_expr_direct: { vars: 1 coeffs: 1 }
        f_expr_direct: { vars: 2 coeffs: 1 }
        f_expr_inverse: { vars: 3 coeffs: 1 }
        f_expr_inverse: { vars: 4 coeffs: 1 }
        f_expr_inverse: { vars: 5 coeffs: 1 }
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
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
      model.Add(NewFeasibleSolutionObserver(
          [&](const CpSolverResponse&) { num_solutions_without_presolve++; }));
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
          [&](const CpSolverResponse&) { num_solutions_with_presolve++; }));
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

TEST(PresolveCpModelTest,
     AtMostOneAndLinearExtractsAndKeepsMultiEnforcedIntsGrouped) {
  // With AMO(x0..x7), setting any x_i = 1 makes 2*b0 + 3*b1 + 2*y0 + 3*y1 +
  // 4*y2 + 5*y3 - 33*sum(x_i) <= 0 trivially satisfied (max positive activity
  // is 5 + 2*(2 + 3 + 4 + 5) = 33). ProcessAtMostOneAndLinear extracts
  // [~x0..~x7] as enforcement literals, and PropagateDomainsInLinear keeps all
  // 6 remaining variables grouped in a single normalized linear equality
  // instead of splitting across 8 enforcement literals.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ]
        coeffs: [ -33, -33, -33, -33, -33, -33, -33, -33, 2, 3, 2, 3, 4, 5 ]
        domain: [ -264, 0 ]
      }
    }
    constraints { at_most_one { literals: [ 0, 1, 2, 3, 4, 5, 6, 7 ] } }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  int num_grouped_int_linears = 0;
  int num_linear1 = 0;
  int num_bool_and = 0;
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kBoolAnd) ++num_bool_and;
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    if (ct.linear().vars_size() == 1) ++num_linear1;
    if (ct.linear().vars_size() == 6) {
      ++num_grouped_int_linears;
      EXPECT_EQ(ct.enforcement_literal_size(), 8);
      EXPECT_THAT(ct.linear().coeffs(), testing::ElementsAre(1, 1, 1, 1, 1, 1));
      EXPECT_THAT(ct.linear().domain(), testing::ElementsAre(0, 0));
    }
  }
  EXPECT_EQ(num_bool_and, 0);
  EXPECT_EQ(num_linear1, 0);
  EXPECT_EQ(num_grouped_int_linears, 1);

  int num_solutions_without_presolve = 0;
  {
    Model model;
    SatParameters solve_params = params;
    solve_params.set_enumerate_all_solutions(true);
    solve_params.set_cp_model_presolve(false);
    model.Add(NewSatParameters(solve_params));
    model.Add(NewFeasibleSolutionObserver(
        [&](const CpSolverResponse&) { ++num_solutions_without_presolve; }));
    SolveCpModel(initial_model, &model);
  }
  int num_solutions_with_presolve = 0;
  {
    Model model;
    SatParameters solve_params = params;
    solve_params.set_enumerate_all_solutions(true);
    model.Add(NewSatParameters(solve_params));
    model.Add(NewFeasibleSolutionObserver(
        [&](const CpSolverResponse&) { ++num_solutions_with_presolve; }));
    SolveCpModel(initial_model, &model);
  }
  EXPECT_EQ(num_solutions_without_presolve, 2593);
  EXPECT_EQ(num_solutions_with_presolve, num_solutions_without_presolve);
}

TEST(PresolveCpModelTest, AtMostOneAndLinearExtractsWhenCollapsingToClause) {
  // 3*x0 + 3*x1 + y0 + y1 + y2 - z0 - z1 - z2 >= 2 with AMO(y0, y1, y2) and
  // AMO(z0, z1, z2): without the AMOs neither extraction nor infeasibility is
  // visible, whereas with the AMOs x0 = 1 or x1 = 1 makes min activity >= 2
  // (trivial) and x0 = x1 = 0 makes max activity <= 1 < 2 (infeasible), so the
  // linear constraint collapses to the single clause x0 OR x1.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7 ]
        coeffs: [ 3, 3, 1, 1, 1, -1, -1, -1 ]
        domain: [ 2, 9 ]
      }
    }
    constraints { at_most_one { literals: [ 2, 3, 4 ] } }
    constraints { at_most_one { literals: [ 5, 6, 7 ] } }
  )pb");

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { at_most_one { literals: [ 2, 3, 4 ] } }
    constraints { at_most_one { literals: [ 5, 6, 7 ] } }
    constraints {
      enforcement_literal: -2
      bool_and { literals: [ 0 ] }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
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
        domain: [ 1, 5 ]
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

// This mimics the structure of bab5.mps.gz: a few dense linear constraints
// in which large groups of Booleans share the same coefficient. Each group
// should be replaced by a single integer variable (its sum).
//
// Each Boolean also appears in a small cardinality constraint with 5 other
// members of its group. Intersecting the common part with these used to split
// each group into many small blocks.
//
// The first two Booleans have a "rare" coefficient pattern which coincides with
// the one of the first group on half of the dense constraints only. Starting
// the greedy search from them must not create a block mixing this group with
// these two variables on just these constraints.
TEST(PresolveCpModelTest, FindBigVerticalLinearOverlapWithRareColumns) {
  constexpr int kNumGroups = 3;
  constexpr int kGroupSize = 36;
  constexpr int kSmallSize = 6;
  constexpr int kNumRare = 2;
  constexpr int kNumConstraints = 10;
  constexpr int kNumVars = kNumRare + kNumGroups * kGroupSize;

  for (int seed = 0; seed < 5; ++seed) {
    CpModelProto initial_model;
    for (int v = 0; v < kNumVars; ++v) {
      IntegerVariableProto* var = initial_model.add_variables();
      var->add_domain(0);
      var->add_domain(1);
    }
    for (int c = 0; c < kNumConstraints; ++c) {
      LinearConstraintProto* linear =
          initial_model.add_constraints()->mutable_linear();
      int64_t max_activity = 0;
      const auto add_term = [&](int var, int64_t coeff) {
        linear->add_vars(var);
        linear->add_coeffs(coeff);
        max_activity += coeff;
      };
      // Group g has coefficient 3 * (c + 1) + g, so the ratio between two
      // constraints is different for each group.
      for (int v = 0; v < kNumRare; ++v) {
        add_term(v, c < kNumConstraints / 2 ? 3 * (c + 1) : 7 * (c + 1));
      }
      for (int g = 0; g < kNumGroups; ++g) {
        for (int i = 0; i < kGroupSize; ++i) {
          add_term(kNumRare + g * kGroupSize + i, 3 * (c + 1) + g);
        }
      }
      linear->add_domain(0);
      linear->add_domain(max_activity / 2);
    }
    for (int g = 0; g < kNumGroups; ++g) {
      for (int start = 0; start < kGroupSize; start += kSmallSize) {
        LinearConstraintProto* linear =
            initial_model.add_constraints()->mutable_linear();
        for (int i = start; i < start + kSmallSize; ++i) {
          linear->add_vars(kNumRare + g * kGroupSize + i);
          linear->add_coeffs(1);
        }
        linear->add_domain(0);
        linear->add_domain(kSmallSize / 2);
      }
    }

    SatParameters params;
    params.set_random_seed(seed);
    params.set_keep_all_feasible_solutions_in_presolve(true);
    const CpModelProto presolved_model = PresolveForTest(initial_model, params);

    // We should have exactly 3 new variables, one per group. Note that the
    // upper bound of their domain can be reduced by propagation.
    int num_integers = 0;
    for (const IntegerVariableProto& var : presolved_model.variables()) {
      if (var.domain(0) == 0 && var.domain(1) == 1) continue;
      ++num_integers;
      EXPECT_EQ(var.domain(0), 0) << "seed=" << seed;
      EXPECT_GT(var.domain(1), kSmallSize) << "seed=" << seed;
      EXPECT_LE(var.domain(1), kGroupSize) << "seed=" << seed;
    }
    EXPECT_EQ(num_integers, kNumGroups) << "seed=" << seed;

    // And the dense constraints should now only contain the 3 sums and the 2
    // rare variables. Note that the definitions of the sums are the only
    // remaining constraints with more than kSmallSize terms.
    int num_dense = 0;
    for (const ConstraintProto& ct : presolved_model.constraints()) {
      if (ct.constraint_case() != ConstraintProto::kLinear) continue;
      if (ct.linear().domain(0) == 0 && ct.linear().domain(1) == 0) {
        EXPECT_EQ(ct.linear().vars_size(), kGroupSize + 1) << "seed=" << seed;
        continue;
      }
      EXPECT_LE(ct.linear().vars_size(), kSmallSize) << "seed=" << seed;
      if (ct.linear().vars_size() == kNumGroups + kNumRare) ++num_dense;
    }
    EXPECT_EQ(num_dense, kNumConstraints) << "seed=" << seed;
  }
}

TEST(PresolveTest, MaxAndMinPairReplacedWithLinearEquality) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      lin_max {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints {
      lin_max {
        target { vars: 3 coeffs: -1 }
        exprs { vars: 0 coeffs: -1 }
        exprs { vars: 1 coeffs: -1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2, 3 ] } }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  int num_lin_max = 0;
  int num_linear = 0;
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kLinMax) ++num_lin_max;
    if (ct.constraint_case() == ConstraintProto::kLinear) ++num_linear;
  }
  EXPECT_EQ(num_lin_max, 1);
  EXPECT_EQ(num_linear, 1);
}

CpModelProto MakeModelWithSizeFourAtMostOnes(int num_groups) {
  constexpr int kGroupSize = 4;
  CpModelProto model;
  for (int v = 0; v < num_groups * kGroupSize; ++v) {
    IntegerVariableProto* var = model.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  for (int g = 0; g < num_groups; ++g) {
    ConstraintProto* amo = model.add_constraints();
    for (int i = 0; i < kGroupSize; ++i) {
      amo->mutable_at_most_one()->add_literals(g * kGroupSize + i);
    }
  }
  return model;
}

TEST(PresolveCpModelTest, DoNotAggregateHeterogeneousGubInLinearOverlap) {
  constexpr int kNumGroups = 12;
  constexpr int kGroupSize = 4;
  constexpr int kNumConstraints = 14;
  CpModelProto initial_model = MakeModelWithSizeFourAtMostOnes(kNumGroups);

  // Each size-4 at_most_one has heterogeneous weights {1, 2, 3, 4}, repeated
  // identically across half of the constraints (horizontal overlap) and
  // proportionally across the rest (vertical overlap).
  for (int c = 0; c < kNumConstraints; ++c) {
    LinearConstraintProto* linear =
        initial_model.add_constraints()->mutable_linear();
    const int64_t row_mult = c < kNumConstraints / 2 ? 1 : (c + 1);
    int64_t max_activity = 0;
    for (int g = 0; g < kNumGroups; ++g) {
      for (int i = 0; i < kGroupSize; ++i) {
        linear->add_vars(g * kGroupSize + i);
        linear->add_coeffs(row_mult * (i + 1));
      }
      max_activity += row_mult * kGroupSize;
    }
    linear->add_domain(0);
    linear->add_domain(max_activity / 2 + 1);
  }

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  for (const IntegerVariableProto& var : presolved_model.variables()) {
    ASSERT_EQ(var.domain_size(), 2);
    EXPECT_GE(var.domain(0), 0);
    EXPECT_LE(var.domain(1), 1);
  }
}

TEST(PresolveCpModelTest, DoNotAggregateSplitGubInLinearOverlap) {
  constexpr int kNumGroups = 12;
  constexpr int kGroupSize = 4;
  constexpr int kNumConstraints = 14;
  CpModelProto initial_model = MakeModelWithSizeFourAtMostOnes(kNumGroups);

  // One variable per size-4 at_most_one has unit weight 1 across all
  // constraints, while the remaining 3 variables of each at_most_one appear in
  // the same constraints with non-proportional weights (splitting every GUB).
  for (int c = 0; c < kNumConstraints; ++c) {
    LinearConstraintProto* linear =
        initial_model.add_constraints()->mutable_linear();
    int64_t max_activity = 0;
    for (int g = 0; g < kNumGroups; ++g) {
      int64_t max_group_coeff = 1;
      linear->add_vars(g * kGroupSize);
      linear->add_coeffs(1);
      for (int i = 1; i < kGroupSize; ++i) {
        const int64_t coeff = 2 + ((c * 3 + g + i) % 7);
        linear->add_vars(g * kGroupSize + i);
        linear->add_coeffs(coeff);
        max_group_coeff = std::max(max_group_coeff, coeff);
      }
      max_activity += max_group_coeff;
    }
    linear->add_domain(0);
    linear->add_domain(max_activity / 2 + 1);
  }

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  for (const IntegerVariableProto& var : presolved_model.variables()) {
    ASSERT_EQ(var.domain_size(), 2);
    EXPECT_GE(var.domain(0), 0);
    EXPECT_LE(var.domain(1), 1);
  }
}

TEST(PresolveCpModelTest, DetectDuplicateLinMaxWithPermutedExprs) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      lin_max {
        target { vars: 2 coeffs: 1 }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ -1, 1 ]
        }
      }
    }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ -1, 1 ]
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
        }
      }
    }
    objective {
      vars: [ 2, 3 ]
      coeffs: [ 1, 1 ]
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  int num_lin_max = 0;
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kLinMax) {
      ++num_lin_max;
      EXPECT_TRUE(LinMaxIsCanonical(ct));
    }
  }
  EXPECT_EQ(num_lin_max, 1);
}

TEST(PresolveCpModelTest, LinMaxCanonicalAfterDegreeTwoSubstitution) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ -1, 1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ -1, 1 ]
        }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 3 ] } }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kLinMax) {
      EXPECT_TRUE(LinMaxIsCanonical(ct));
    }
  }
}

TEST(PresolveCpModelTest, LinMaxWithLiteralAndNegationInInitialProto) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: [ 0, -1 ]
      lin_max {
        target {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2 ] } }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    EXPECT_NE(ct.constraint_case(), ConstraintProto::kLinMax);
  }
}

TEST(PresolveCpModelTest, LinMaxWithLiteralAndNegationAfterSubstitution) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: [ 0, 1 ]
      lin_max {
        target {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 2, 3 ] } }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    EXPECT_NE(ct.constraint_case(), ConstraintProto::kLinMax);
  }
}

TEST(PresolveTest, LinMaxAbsoluteValueNonNegative) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      lin_max {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 0 coeffs: -1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1 ] } }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  EXPECT_EQ(presolved_model.variables(1).domain(0), 0);
  EXPECT_EQ(presolved_model.variables(1).domain(1), 10);
}

TEST(PresolveTest, ExpandLinMaxInObjectiveSimple) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 10, 10 ]
      }
    }
    constraints {
      lin_max {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 2 ] } }
    objective {
      vars: [ 3 ]
      coeffs: [ 1 ]
    }
  )pb");
  const CpModelProto presolved_model =
      PresolveForTest(initial_model, SatParameters());
  int num_lin_max = 0;
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kLinMax) ++num_lin_max;
  }
  EXPECT_EQ(num_lin_max, 0);
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMEndToEnd) {
  // x + y + z + 101*b1 - 101*b2 == 1 on Booleans: 101 is coprime to 2, 3, 5,
  // overall gcd is 1, and 3 variables share the smallest coefficient 1, so
  // neither modulo presolve nor Diophantine presolve simplifies it without
  // big-M decoupling. After decoupling, b1 == b2 is extracted as an affine
  // equivalence (eliminating a variable) and x + y + z == 1 becomes
  // exactly_one, leaving no big-M in the presolved model.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4 ]
        coeffs: [ 1, 1, 1, 101, -101 ]
        domain: [ 1, 1 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);

  EXPECT_LT(presolved_model.variables_size(), initial_model.variables_size());
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kLinear) {
      EXPECT_THAT(ct.linear().coeffs(), Each(AllOf(Ge(-1), Le(1))));
    }
  }

  int num_solutions_with_presolve = 0;
  {
    Model model;
    SatParameters enumerate_params;
    enumerate_params.set_enumerate_all_solutions(true);
    enumerate_params.set_keep_all_feasible_solutions_in_presolve(true);
    model.Add(NewSatParameters(enumerate_params));
    model.Add(NewFeasibleSolutionObserver(
        [&](const CpSolverResponse&) { ++num_solutions_with_presolve; }));
    SolveCpModel(initial_model, &model);
  }
  // 3 choices for (x, y, z) summing to 1, times 2 choices for b1 == b2.
  EXPECT_EQ(num_solutions_with_presolve, 6);
}

TEST(PresolveCpModelTest, DecoupleOpposingBigMRandomEquivalence) {
  absl::BitGen random;
  for (int iter = 0; iter < 100; ++iter) {
    CpModelProto cp_model;
    for (int v = 0; v < 4; ++v) {
      const int64_t lb = absl::Uniform<int64_t>(random, -1, 2);
      const int64_t ub = lb + absl::Uniform<int64_t>(random, 1, 4);
      FillDomainInProto(Domain(lb, ub), cp_model.add_variables());
    }
    // Variable 4 is an optional enforcement literal in [0, 1].
    FillDomainInProto(Domain(0, 1), cp_model.add_variables());

    ConstraintProto* ct = cp_model.add_constraints();
    if (absl::Bernoulli(random, 0.4)) {
      ct->add_enforcement_literal(absl::Bernoulli(random, 0.5) ? 4 : -5);
    }
    LinearConstraintProto* lin = ct->mutable_linear();

    const int64_t g = absl::Uniform<int64_t>(random, 2, 40);
    std::vector<int64_t> coeffs(4);
    coeffs[0] = absl::Uniform<int64_t>(random, 1, 4) *
                (absl::Bernoulli(random, 0.5) ? 1 : -1);
    coeffs[1] = absl::Uniform<int64_t>(random, 1, 4) *
                (absl::Bernoulli(random, 0.5) ? 1 : -1);
    if (absl::Bernoulli(random, 0.8)) {
      // Multiples of g with opposing signs.
      coeffs[2] = g * absl::Uniform<int64_t>(random, 1, 4);
      coeffs[3] = -g * absl::Uniform<int64_t>(random, 1, 4);
    } else {
      // Coprime big-Ms (tests subset-sum gap when gcd == 1).
      coeffs[2] = g * 3;
      coeffs[3] = -(g * 3 + absl::Uniform<int64_t>(random, 1, 6));
    }
    for (int v = 0; v < 4; ++v) {
      lin->add_vars(v);
      lin->add_coeffs(coeffs[v]);
    }

    int64_t sample_rhs = absl::Uniform<int64_t>(random, -5, 6);
    for (int v = 0; v < 4; ++v) {
      const Domain d = ReadDomainFromProto(cp_model.variables(v));
      sample_rhs += coeffs[v] * ((d.Min() + d.Max()) / 2);
    }
    const int64_t width = absl::Uniform<int64_t>(random, 0, 5);
    FillDomainInProto(Domain(sample_rhs, sample_rhs + width), lin);

    const auto count_solutions = [&](bool presolve) {
      Model model;
      SatParameters parameters;
      parameters.set_enumerate_all_solutions(true);
      parameters.set_keep_all_feasible_solutions_in_presolve(true);
      parameters.set_cp_model_presolve(presolve);
      model.Add(NewSatParameters(parameters));
      int count = 0;
      model.Add(NewFeasibleSolutionObserver(
          [&](const CpSolverResponse&) { ++count; }));
      SolveCpModel(cp_model, &model);
      return count;
    };

    ASSERT_EQ(count_solutions(/*presolve=*/true),
              count_solutions(/*presolve=*/false))
        << ProtobufDebugString(cp_model);
  }
}

TEST(PresolveCpModelTest,
     DecoupleOpposingBigMEnforcedSmallGcdInfeasibleFullPresolve) {
  // Regression test for maintaining ConstraintVariableUsageIsConsistent() when
  // peeling off the forced big-M term exposes GCD infeasibility in the
  // remaining small terms of an enforced constraint.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ -3, 3 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ -1024, -1024, 5769 ]
        domain: [ -2773, -2767 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved = PresolveForTest(initial_model, params);
  EXPECT_THAT(presolved.constraints(), IsEmpty());

  Model model;
  const CpSolverResponse response = SolveCpModel(initial_model, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(response.solution(3), 0);
}

CpModelProto ModelWithKeepAliveBools(int num_vars) {
  CpModelProto model;
  LinearConstraintProto* keep_alive = model.add_constraints()->mutable_linear();
  int64_t total_coeff = 0;
  for (int v = 0; v < num_vars; ++v) {
    FillDomainInProto(Domain(0, 1), model.add_variables());
    const int64_t coeff = v + 2;
    keep_alive->add_vars(v);
    keep_alive->add_coeffs(coeff);
    total_coeff += coeff;
  }
  FillDomainInProto(Domain(total_coeff / 3, (2 * total_coeff) / 3), keep_alive);
  return model;
}

TEST(PresolveCpModelTest,
     LargeMultiEnforcedBoolAndStrengthensAndFoldsBackWithoutExtraVariables) {
  constexpr int kNumEnforcements = 20;
  constexpr int kNumRhs = 500;
  CpModelProto initial_model =
      ModelWithKeepAliveBools(kNumEnforcements + kNumRhs);

  ConstraintProto* bool_and = initial_model.add_constraints();
  for (int i = 0; i < kNumEnforcements; ++i) {
    bool_and->add_enforcement_literal(i);
  }
  for (int j = 0; j < kNumRhs; ++j) {
    bool_and->mutable_bool_and()->add_literals(kNumEnforcements + j);
  }

  // (x_0 /\ x_1) => x_2 makes enforcement literal x_2 redundant in bool_and
  // via self-subsuming resolution in PresolvePureSatPart.
  ConstraintProto* ternary = initial_model.add_constraints();
  ternary->mutable_bool_or()->add_literals(NegatedRef(0));
  ternary->mutable_bool_or()->add_literals(NegatedRef(1));
  ternary->mutable_bool_or()->add_literals(2);

  SatParameters params;
  const CpModelProto presolved = PresolveForTest(initial_model, params);
  EXPECT_EQ(presolved.variables_size(), initial_model.variables_size());

  std::vector<const ConstraintProto*> bool_ands;
  for (const ConstraintProto& ct : presolved.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kBoolAnd) {
      bool_ands.push_back(&ct);
    }
  }
  ASSERT_EQ(bool_ands.size(), 1);
  std::vector<int> expected_enforcements;
  for (int i = 0; i < kNumEnforcements; ++i) {
    if (i == 2) continue;
    expected_enforcements.push_back(i);
  }
  EXPECT_THAT(bool_ands[0]->enforcement_literal(),
              testing::UnorderedElementsAreArray(expected_enforcements));
  EXPECT_EQ(bool_ands[0]->bool_and().literals_size(), kNumRhs);

  Model model;
  const CpSolverResponse response = SolveCpModel(initial_model, &model);
  ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_TRUE(SolutionIsFeasible(initial_model, response.solution()));
}

TEST(PresolveCpModelTest,
     LargeMultiEnforcedBoolAndContradictionCollapsesToClause) {
  constexpr int kNumEnforcements = 20;
  constexpr int kNumRhs = 500;
  CpModelProto initial_model =
      ModelWithKeepAliveBools(kNumEnforcements + kNumRhs);

  // Two unconstrained helper Booleans eliminated by BVE in PresolvePureSatPart.
  const int helper_h = initial_model.variables_size();
  FillDomainInProto(Domain(0, 1), initial_model.add_variables());
  const int helper_a = initial_model.variables_size();
  FillDomainInProto(Domain(0, 1), initial_model.add_variables());

  ConstraintProto* bool_and = initial_model.add_constraints();
  for (int i = 0; i < kNumEnforcements; ++i) {
    bool_and->add_enforcement_literal(i);
  }
  for (int j = 0; j < kNumRhs; ++j) {
    bool_and->mutable_bool_and()->add_literals(kNumEnforcements + j);
  }
  bool_and->mutable_bool_and()->add_literals(helper_h);

  // Eliminating helper_a yields (~helper_h \/ ~r_0), and eliminating helper_h
  // then yields (~temp_var \/ ~r_0), which resolves with (~temp_var \/ r_0)
  // to fix temp_var = false and collapse bool_and to (~e_0 \/ ... \/ ~e_19).
  const int r0 = kNumEnforcements;
  ConstraintProto* c1 = initial_model.add_constraints();
  c1->mutable_bool_or()->add_literals(NegatedRef(helper_h));
  c1->mutable_bool_or()->add_literals(NegatedRef(r0));
  c1->mutable_bool_or()->add_literals(helper_a);

  ConstraintProto* c2 = initial_model.add_constraints();
  c2->mutable_bool_or()->add_literals(NegatedRef(helper_h));
  c2->mutable_bool_or()->add_literals(NegatedRef(r0));
  c2->mutable_bool_or()->add_literals(NegatedRef(helper_a));

  SatParameters params;
  const CpModelProto presolved = PresolveForTest(initial_model, params);
  // Both helper_h and helper_a are eliminated, and no auxiliary temp_var is
  // materialized into the presolved model.
  EXPECT_EQ(presolved.variables_size(), kNumEnforcements + kNumRhs);
  std::vector<const ConstraintProto*> bool_ands;
  std::vector<const ConstraintProto*> bool_ors;
  for (const ConstraintProto& ct : presolved.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kBoolAnd) {
      bool_ands.push_back(&ct);
    } else if (ct.constraint_case() == ConstraintProto::kBoolOr) {
      bool_ors.push_back(&ct);
    }
  }
  EXPECT_THAT(bool_ands, IsEmpty());
  ASSERT_EQ(bool_ors.size(), 1);
  std::vector<int> expected_clause;
  for (int i = 0; i < kNumEnforcements; ++i) {
    expected_clause.push_back(NegatedRef(i));
  }
  EXPECT_THAT(bool_ors[0]->bool_or().literals(),
              testing::UnorderedElementsAreArray(expected_clause));

  Model model;
  const CpSolverResponse response = SolveCpModel(initial_model, &model);
  ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_TRUE(SolutionIsFeasible(initial_model, response.solution()));
}

TEST(PresolveCpModelTest,
     ManyMediumMultiEnforcedBoolAndsExceedingExpansionBudgetStillPresolve) {
  constexpr int kNumConstraints = 2500;
  constexpr int kSharedEnforcements = 4;
  constexpr int kRhsSize = 18;
  const int rhs_start = kSharedEnforcements + kNumConstraints;
  CpModelProto initial_model =
      ModelWithKeepAliveBools(rhs_start + kNumConstraints + kRhsSize);

  // Each constraint has 5 enforcements and 18 RHS literals:
  // encoded = 42 < expanded = 108, so every constraint is encoded linearly via
  // a temporary Boolean without needing any expansion literal cap.
  for (int k = 0; k < kNumConstraints; ++k) {
    ConstraintProto* ct = initial_model.add_constraints();
    for (int i = 0; i < kSharedEnforcements; ++i) {
      ct->add_enforcement_literal(i);
    }
    ct->add_enforcement_literal(kSharedEnforcements + k);
    for (int j = 0; j < kRhsSize; ++j) {
      ct->mutable_bool_and()->add_literals(rhs_start + k + j);
    }
  }

  // (x_0 /\ x_1) => x_2 strengthens every bool_and, including the last one.
  ConstraintProto* ternary = initial_model.add_constraints();
  ternary->mutable_bool_or()->add_literals(NegatedRef(0));
  ternary->mutable_bool_or()->add_literals(NegatedRef(1));
  ternary->mutable_bool_or()->add_literals(2);

  SatParameters params;
  const CpModelProto presolved = PresolveForTest(initial_model, params);
  EXPECT_EQ(presolved.variables_size(), initial_model.variables_size());

  const int last_unique_enf = kSharedEnforcements + kNumConstraints - 1;
  bool found_last = false;
  for (const ConstraintProto& ct : presolved.constraints()) {
    if (ct.constraint_case() != ConstraintProto::kBoolAnd) continue;
    if (std::find(ct.enforcement_literal().begin(),
                  ct.enforcement_literal().end(),
                  last_unique_enf) != ct.enforcement_literal().end()) {
      found_last = true;
      EXPECT_THAT(ct.enforcement_literal(),
                  testing::UnorderedElementsAre(0, 1, 3, last_unique_enf));
    }
  }
  EXPECT_TRUE(found_last);
}

TEST(PresolveCpModelTest,
     LargeMultiEnforcedBoolAndMaterializesFallbackAndPreservesHint) {
  constexpr int kNumEnforcements = 8;
  constexpr int kNumRhs = 20;
  CpModelProto initial_model;
  auto* objective = initial_model.mutable_objective();
  auto* hint = initial_model.mutable_solution_hint();

  for (int i = 0; i < kNumEnforcements; ++i) {
    FillDomainInProto(Domain(0, 1), initial_model.add_variables());
    objective->add_vars(i);
    objective->add_coeffs(-1000);
    hint->add_vars(i);
    hint->add_values(1);
  }
  for (int j = 0; j < kNumRhs; ++j) {
    const int var = kNumEnforcements + j;
    FillDomainInProto(Domain(0, 1), initial_model.add_variables());
    objective->add_vars(var);
    objective->add_coeffs(j + 1);
    hint->add_vars(var);
    hint->add_values(1);
  }
  const int var_p = initial_model.variables_size();
  FillDomainInProto(Domain(0, 1), initial_model.add_variables());
  objective->add_vars(var_p);
  objective->add_coeffs(50);
  hint->add_vars(var_p);
  hint->add_values(1);

  const int var_q = initial_model.variables_size();
  FillDomainInProto(Domain(0, 1), initial_model.add_variables());
  objective->add_vars(var_q);
  objective->add_coeffs(60);
  hint->add_vars(var_q);
  hint->add_values(0);

  // Removable variable eliminated by BVE against (~temp_var \/ helper_h) and
  // (~helper_h \/ var_p \/ var_q), producing ternary (~temp_var \/ var_p \/
  // var_q) and recording temp_var in sat_postsolver.
  const int helper_h = initial_model.variables_size();
  FillDomainInProto(Domain(0, 1), initial_model.add_variables());
  hint->add_vars(helper_h);
  hint->add_values(1);

  ConstraintProto* bool_and = initial_model.add_constraints();
  for (int i = 0; i < kNumEnforcements; ++i) {
    bool_and->add_enforcement_literal(i);
  }
  for (int j = 0; j < kNumRhs; ++j) {
    bool_and->mutable_bool_and()->add_literals(kNumEnforcements + j);
  }
  bool_and->mutable_bool_and()->add_literals(helper_h);

  ConstraintProto* ternary = initial_model.add_constraints();
  ternary->mutable_bool_or()->add_literals(NegatedRef(helper_h));
  ternary->mutable_bool_or()->add_literals(var_p);
  ternary->mutable_bool_or()->add_literals(var_q);

  for (const bool full_solver_postsolve : {false, true}) {
    Model model;
    SatParameters params;
    params.set_debug_crash_if_presolve_breaks_hint(true);
    params.set_debug_postsolve_with_full_solver(full_solver_postsolve);
    model.Add(NewSatParameters(params));
    const CpSolverResponse response = SolveCpModel(initial_model, &model);
    ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
    EXPECT_DOUBLE_EQ(response.objective_value(), -7740.0);
    EXPECT_TRUE(SolutionIsFeasible(initial_model, response.solution()));
  }
}

TEST(PresolveTest, RoutesNodeExpressionsCanonicalizedUnderAffineAndFixing) {
  // Variables:
  //   0, 1, 2: arc booleans for a 2-node route (0 -> 1 -> 0).
  //   3: fixed to 4 via a linear constraint.
  //   4, 5: related by 4 - 2 * 5 == 1, with 5 in [0, 5] and 4 in [0, 15].
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 15 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 4, 4 ]
      }
    }
    constraints {
      linear {
        vars: [ 4, 5 ]
        coeffs: [ 1, -2 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      routes {
        tails: [ 0, 1, 1 ]
        heads: [ 1, 0, 1 ]
        literals: [ 0, 1, 2 ]
        dimensions {
          exprs {
            vars: [ 3 ]
            coeffs: [ 2 ]
            offset: 1
          }
          exprs {
            vars: [ 4, 3 ]
            coeffs: [ 3, 1 ]
            offset: -2
          }
        }
      }
    }
    constraints {
      linear {
        vars: [ 5 ]
        coeffs: [ 1 ]
        domain: [ 1, 3 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(ValidateCpModel(presolved_model), "");

  int num_routes = 0;
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() != ConstraintProto::kRoutes) continue;
    ++num_routes;
    ASSERT_EQ(ct.routes().dimensions_size(), 1);
    ASSERT_EQ(ct.routes().dimensions(0).exprs_size(), 2);
    // Node 0 expression: 2 * 4 + 1 = 9 (constant).
    EXPECT_TRUE(ct.routes().dimensions(0).exprs(0).vars().empty());
    EXPECT_EQ(ct.routes().dimensions(0).exprs(0).offset(), 9);
    // Node 1 expression: 3 * (2 * v5 + 1) + 4 - 2 = 6 * v5 + 5.
    ASSERT_EQ(ct.routes().dimensions(0).exprs(1).vars_size(), 1);
    EXPECT_EQ(ct.routes().dimensions(0).exprs(1).coeffs(0), 6);
    EXPECT_EQ(ct.routes().dimensions(0).exprs(1).offset(), 5);
  }
  EXPECT_EQ(num_routes, 1);

  Model model;
  const CpSolverResponse response = SolveCpModel(initial_model, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(PresolveTest, DuplicateLinearDetectedAfterLazyAffineAndFixing) {
  // Constraint 0 fixes v3 = 2 and Constraint 1 sets v1 = v0 + 1.
  // Constraint 2: 2 * v0 + 2 * v2 + v3 in [4, 10] -> 2 * v0 + 2 * v2 in [2, 8]
  //               -> v0 + v2 in [1, 4].
  // Constraint 3: -3 * v1 - 3 * v2 in [-15, -9]
  //               -> -3 * (v0 + 1) - 3 * v2 in [-15, -9]
  //               -> v0 + v2 in [2, 4].
  // Merging duplicates yields a single linear constraint v0 + v2 in [2, 4].
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 2, 2 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 0 ]
        coeffs: [ 1, -1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 2, 3 ]
        coeffs: [ 2, 2, 1 ]
        domain: [ 4, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ -3, -3 ]
        domain: [ -15, -9 ]
      }
    }
    constraints { dummy_constraint { vars: [ 0, 2 ] } }
  )pb");
  SatParameters params;
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(ValidateCpModel(presolved_model), "");

  int num_linears = 0;
  for (const ConstraintProto& ct : presolved_model.constraints()) {
    if (ct.constraint_case() != ConstraintProto::kLinear) continue;
    ++num_linears;
    EXPECT_THAT(ct.linear(), EqualsProto(R"pb(
                  vars: [ 0, 1 ]
                  coeffs: [ 1, 1 ]
                  domain: [ 2, 4 ]
                )pb"));
  }
  EXPECT_EQ(num_linears, 1);
}

TEST(PresolveTest, PresolveDisabledStillCanonicalizesLinearsAndExpansion) {
  // With cp_model_presolve = false, fixed variables in linear constraints are
  // still folded out via CanonicalizeConstraintsLazily() without needing
  // CanonicalizeAllLinears().
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 3, 3 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 2, -4, -2 ]
        domain: [ -10, -2 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(ValidateCpModel(presolved_model), "");
  ASSERT_EQ(presolved_model.constraints_size(), 1);
  // 2 * 3 - 4 * v1 - 2 * v2 in [-10, -2] -> -4 * v1 - 2 * v2 in [-16, -8]
  // -> divide by GCD 2 and make first coeff positive: 2 * v1 + v2 in [4, 8].
  EXPECT_THAT(presolved_model.constraints(0).linear(), EqualsProto(R"pb(
                vars: [ 1, 2 ]
                coeffs: [ 2, 1 ]
                domain: [ 4, 8 ]
              )pb"));
}

TEST(PresolveTest, LratWithNoPresolvePreservesProblemClausesForProofMerge) {
  // Regression test: when check_lrat_proof is enabled with cp_model_presolve =
  // false, ModelCopy records problem clauses in lrat1.bin while later unit
  // clauses fix variables in domains. CanonicalizeConstraintsLazily() must not
  // simplify or delete those kBoolOr clauses without LRAT proof steps.
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 0, 1 ] } }
    constraints { bool_or { literals: [ -1, 2 ] } }
    constraints { bool_or { literals: [ -2, -3 ] } }
    constraints { bool_or { literals: [ 0 ] } }
    constraints { bool_or { literals: [ -1, 1, 2 ] } }
    constraints { bool_or { literals: [ 1 ] } }
    constraints { bool_or { literals: [ 2 ] } }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  params.set_inprocessing_use_sat_sweeping(false);
  params.set_symmetry_level(1);
  params.set_linearization_level(1);
  params.set_use_chronological_backtracking(false);
  params.set_check_lrat_proof(true);
  params.set_debug_crash_if_lrat_check_fails(true);
  const CpSolverResponse response = SolveWithParameters(model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(PresolveTest,
     ElementExpansionRemovesUnusedAffineVarsBeforeOrbitopeSymmetry) {
  // 3 symmetric slots j in {0, 1, 2}, each picking an offer index t_j in
  // [0, 3] with two constant-array element lookups (RAM and Price). Offers 2
  // and 3 each have both a unique RAM value and a unique Price value, so
  // ExpandConstantArrayElement equates both target value-encoding literals to
  // [t_j == offer]. Cleaning up those unused affine variables after expansion
  // preserves the 3-column orbitope and breaks objective symmetry.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 10, 40 ] }
    variables { domain: [ 10, 40 ] }
    variables { domain: [ 10, 40 ] }
    variables { domain: [ 100, 400 ] }
    variables { domain: [ 100, 400 ] }
    variables { domain: [ 100, 400 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      element {
        linear_index {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
        linear_target {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
        exprs { offset: 10 }
        exprs { offset: 10 }
        exprs { offset: 30 }
        exprs { offset: 40 }
      }
    }
    constraints {
      element {
        linear_index {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        linear_target {
          vars: [ 4 ]
          coeffs: [ 1 ]
        }
        exprs { offset: 10 }
        exprs { offset: 10 }
        exprs { offset: 30 }
        exprs { offset: 40 }
      }
    }
    constraints {
      element {
        linear_index {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
        linear_target {
          vars: [ 5 ]
          coeffs: [ 1 ]
        }
        exprs { offset: 10 }
        exprs { offset: 10 }
        exprs { offset: 30 }
        exprs { offset: 40 }
      }
    }
    constraints {
      element {
        linear_index {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
        linear_target {
          vars: [ 6 ]
          coeffs: [ 1 ]
        }
        exprs { offset: 100 }
        exprs { offset: 100 }
        exprs { offset: 300 }
        exprs { offset: 400 }
      }
    }
    constraints {
      element {
        linear_index {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        linear_target {
          vars: [ 7 ]
          coeffs: [ 1 ]
        }
        exprs { offset: 100 }
        exprs { offset: 100 }
        exprs { offset: 300 }
        exprs { offset: 400 }
      }
    }
    constraints {
      element {
        linear_index {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
        linear_target {
          vars: [ 8 ]
          coeffs: [ 1 ]
        }
        exprs { offset: 100 }
        exprs { offset: 100 }
        exprs { offset: 300 }
        exprs { offset: 400 }
      }
    }
    constraints {
      linear {
        vars: [ 9, 10, 11 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 2, 3 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 9 ]
        coeffs: [ 1, -30 ]
        domain: [ 0, 40 ]
      }
    }
    constraints {
      linear {
        vars: [ 4, 10 ]
        coeffs: [ 1, -30 ]
        domain: [ 0, 40 ]
      }
    }
    constraints {
      linear {
        vars: [ 5, 11 ]
        coeffs: [ 1, -30 ]
        domain: [ 0, 40 ]
      }
    }
    objective {
      vars: [ 6, 7, 8 ]
      coeffs: [ 1, 1, 1 ]
    }
  )pb");
  SatParameters stop_params;
  stop_params.set_stop_after_presolve(true);
  const CpSolverResponse presolve_response =
      SolveWithParameters(initial_model, stop_params);
  EXPECT_GE(presolve_response.best_objective_bound(), 700.0);

  const CpSolverResponse solve_response =
      SolveWithParameters(initial_model, SatParameters());
  EXPECT_EQ(solve_response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_DOUBLE_EQ(solve_response.objective_value(), 700.0);
}

TEST(PresolveTest, InverseWithAffineSubstitutedVariableAndNoExpansion) {
  // v2 = 2 * v1 is only referenced in the inverse constraint via f_direct[1].
  // Canonicalizing kInverse expressions substitutes v2 -> 2 * v1, allowing
  // PresolveInverse to tighten v1 to [0, 2], filter 1 out of f_inverse[1] and
  // f_inverse[3], and eliminate v2 completely.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    constraints {
      linear {
        vars: [ 2, 1 ]
        coeffs: [ 1, -2 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      inverse {
        f_expr_direct { vars: 0 coeffs: 1 }
        f_expr_direct { vars: 2 coeffs: 1 }
        f_expr_direct { vars: 3 coeffs: 1 }
        f_expr_direct { vars: 4 coeffs: 1 }
        f_expr_direct { vars: 5 coeffs: 1 }
        f_expr_inverse { vars: 6 coeffs: 1 }
        f_expr_inverse { vars: 7 coeffs: 1 }
        f_expr_inverse { vars: 8 coeffs: 1 }
        f_expr_inverse { vars: 9 coeffs: 1 }
        f_expr_inverse { vars: 10 coeffs: 1 }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1, 3, 4, 5, 6, 7, 8, 9, 10 ] } }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(ValidateCpModel(presolved_model), "");
  EXPECT_EQ(presolved_model.variables_size(), 10);
  EXPECT_THAT(presolved_model.variables(1).domain(),
              testing::ElementsAre(0, 2));
  EXPECT_THAT(presolved_model.variables(6).domain(),
              testing::ElementsAre(0, 0, 2, 4));
  ASSERT_EQ(presolved_model.constraints_size(), 1);
  ASSERT_EQ(presolved_model.constraints(0).constraint_case(),
            ConstraintProto::kInverse);
  EXPECT_THAT(presolved_model.constraints(0).inverse().f_expr_direct(1),
              EqualsProto(R"pb(vars: 1 coeffs: 2)pb"));
}

TEST(PresolveTest, EnforcedAllDiffCanonicalizesAffineAndFixedExpressions) {
  // v2 = 2 * v1 + 1 and v3 = 5 appear only inside an enforced all_diff
  // constraint: v0 => all_diff(v2 + v0, v1, v3). Canonicalizing enforced
  // all_diff expressions eliminates v2 and v3 without emitting residual
  // affine equality constraints.
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 5 ] }
    constraints {
      linear {
        vars: [ 2, 1 ]
        coeffs: [ 1, -2 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      enforcement_literal: [ 0 ]
      all_diff {
        exprs {
          vars: [ 2, 0 ]
          coeffs: [ 1, 1 ]
        }
        exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints { dummy_constraint { vars: [ 0, 1 ] } }
  )pb");
  SatParameters params;
  params.set_disable_constraint_expansion(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  const CpModelProto presolved_model = PresolveForTest(initial_model, params);
  EXPECT_EQ(ValidateCpModel(presolved_model), "");
  EXPECT_EQ(presolved_model.variables_size(), 2);
  ASSERT_EQ(presolved_model.constraints_size(), 1);
  EXPECT_THAT(presolved_model.constraints(0), EqualsProto(R"pb(
                enforcement_literal: [ 0 ]
                all_diff {
                  exprs { vars: 1 coeffs: 2 offset: 2 }
                  exprs { vars: 1 coeffs: 1 }
                  exprs { offset: 5 }
                }
              )pb"));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
