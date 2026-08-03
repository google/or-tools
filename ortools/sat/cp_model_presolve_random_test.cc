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

// This file tests the various presolves by asserting that the result of a
// randomly generated linear integer program is the same with or without the
// presolve step. The linear programs are generated in a way that tries to cover
// all the corner cases of the preprocessor (for the linear part).

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/helpers.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

ABSL_FLAG(std::string, dump_dir, "",
          "If non-empty, a dir where all the models are dumped.");

namespace operations_research {
namespace sat {
namespace {

int64_t GetRandomNonZero(int max_magnitude, absl::BitGen* random) {
  if (absl::Bernoulli(*random, 0.5)) {
    return absl::Uniform<int64_t>(*random, -max_magnitude, -1);
  }
  return absl::Uniform<int64_t>(*random, 1, max_magnitude);
}

int64_t GetRandomNonZeroAndNonInvertible(int max_magnitude,
                                         absl::BitGen* random) {
  if (absl::Bernoulli(*random, 0.5)) {
    return absl::Uniform<int64_t>(*random, -max_magnitude, -1);
  }
  return absl::Uniform<int64_t>(*random, 2, max_magnitude);
}

// Generate an initial linear program that will be extended later with new
// variables and constraints that the preprocessors should be able to remove.
CpModelProto GenerateRandomBaseProblem(absl::BitGen* random) {
  CpModelProto result;
  result.set_name("Random IP");
  const int num_variables = absl::Uniform(*random, 1, 20);
  const int num_constraints = absl::Uniform(*random, 1, 20);

  for (int i = 0; i < num_variables; ++i) {
    sat::IntegerVariableProto* var = result.add_variables();
    var->add_domain(absl::Uniform<int64_t>(*random, -10, 10));
    var->add_domain(absl::Uniform<int64_t>(*random, var->domain(0), 10));
  }
  for (int i = 0; i < num_constraints; ++i) {
    auto* ct = result.add_constraints()->mutable_linear();
    ct->add_domain(absl::Uniform<int64_t>(*random, -100, 100));
    ct->add_domain(absl::Uniform<int64_t>(*random, ct->domain(0), 100));
    for (int v = 0; v < num_variables; ++v) {
      if (absl::Bernoulli(*random, 0.2)) {  // Sparser.
        ct->add_vars(v);
        ct->add_coeffs(GetRandomNonZero(10, random));
      }
    }
  }

  std::vector<int> all_variables(num_variables);
  std::iota(begin(all_variables), end(all_variables), 0);
  std::shuffle(begin(all_variables), end(all_variables), *random);
  for (const int v : all_variables) {
    if (absl::Bernoulli(*random, 0.5)) {
      result.mutable_objective()->add_vars(v);
      result.mutable_objective()->add_coeffs(
          absl::Uniform<int64_t>(*random, -100, 100));
    }
  }
  result.mutable_objective()->set_offset(
      absl::Uniform<double>(*random, -100, 100));
  result.mutable_objective()->set_scaling_factor(
      absl::Uniform<double>(*random, -100, 100));

  return result;
}

// Adds a row to the given problem which is a duplicate (with a random
// proportionality factor) of a random row.
void AddRandomDuplicateRow(absl::BitGen* random, CpModelProto* proto) {
  const int64_t factor = GetRandomNonZero(10, random);
  const LinearConstraintProto& source =
      proto
          ->constraints(absl::Uniform<int64_t>(*random, 0,
                                               proto->constraints().size() - 1))
          .linear();
  auto* ct = proto->add_constraints()->mutable_linear();
  FillDomainInProto(ReadDomainFromProto(source).MultiplicationBy(factor), ct);
  for (int i = 0; i < source.vars().size(); ++i) {
    ct->add_vars(source.vars(i));
    ct->add_coeffs(source.coeffs(i) * factor);
  }
}

// Adds a column to the given problem which is a duplicate (with a random
// proportionality factor) of a random column.
//
// Note(user): This is not super efficient as we rescan the whole problem for
// this.
void AddRandomDuplicateColumn(absl::BitGen* random, CpModelProto* proto) {
  const int new_var = proto->variables().size();
  const int source_var = absl::Uniform<int>(*random, 0, new_var - 1);

  sat::IntegerVariableProto* var = proto->add_variables();
  var->add_domain(absl::Uniform<int64_t>(*random, -10, 10));
  var->add_domain(absl::Uniform<int64_t>(*random, var->domain(0), 10));

  const int64_t factor = GetRandomNonZero(10, random);
  for (int c = 0; c < proto->constraints().size(); ++c) {
    LinearConstraintProto* linear =
        proto->mutable_constraints(c)->mutable_linear();
    for (int i = 0; i < linear->vars().size(); ++i) {
      if (linear->vars(i) == source_var) {
        linear->add_vars(new_var);
        linear->add_coeffs(linear->coeffs(i) * factor);
        break;
      }
    }
  }
}

// Adds a random x == coeff * y + offset affine relation to the model.
void AddRandomAffineRelation(absl::BitGen* random, CpModelProto* proto) {
  const int num_vars = proto->variables().size();
  const int a = absl::Uniform<int>(*random, 0, num_vars - 1);
  const int b = absl::Uniform<int>(*random, 0, num_vars - 1);
  if (a == b) return;
  LinearConstraintProto* linear = proto->add_constraints()->mutable_linear();
  const int64_t offset = absl::Uniform<int>(*random, -5, 5);
  linear->add_domain(offset);
  linear->add_domain(offset);
  linear->add_vars(a);
  linear->add_coeffs(1);
  linear->add_vars(b);
  linear->add_coeffs(GetRandomNonZero(5, random));
}

// Calls GenerateRandomBaseProblem() and extends the problem in various random
// ways.
CpModelProto GenerateRandomProblem(const std::string& env_name) {
  absl::BitGen random;
  CpModelProto result = GenerateRandomBaseProblem(&random);
  for (int i = 0; i < absl::Uniform<int>(random, 0, 10); ++i) {
    switch (absl::Uniform<int>(random, 0, 2)) {
      case 0:
        AddRandomDuplicateRow(&random, &result);
        break;
      case 1:
        AddRandomDuplicateColumn(&random, &result);
        break;
      case 2:
        AddRandomAffineRelation(&random, &result);
        break;
    }
  }
  return result;
}

// Parameterized test to test different random lp.
class RandomPreprocessorTest : public ::testing::TestWithParam<int> {
 protected:
  std::string GetSeedEnvName() {
    return absl::StrFormat("TestCase%d", GetParam());
  }
};

TEST_P(RandomPreprocessorTest, SolveWithAndWithoutPresolve) {
  const CpModelProto model_proto = GenerateRandomProblem(GetSeedEnvName());
  if (!absl::GetFlag(FLAGS_dump_dir).empty()) {
    const std::string name =
        file::JoinPath(absl::GetFlag(FLAGS_dump_dir),
                       absl::StrCat(GetSeedEnvName(), ".pb.txt"));
    LOG(INFO) << "Dumping  model to '" << name << "'.";
    CHECK_OK(file::SetTextProto(name, model_proto, file::Defaults()));
  }

  SatParameters params;
  params.set_cp_model_presolve(true);
  const CpSolverResponse response_with =
      SolveWithParameters(model_proto, params);
  params.set_cp_model_presolve(false);
  const CpSolverResponse response_without =
      SolveWithParameters(model_proto, params);
  EXPECT_EQ(response_with.status(), response_without.status());
  EXPECT_NEAR(response_with.objective_value(),
              // 1e-10 yields flakiness (<0.1%): see gpaste/5821350335741952.
              response_without.objective_value(), 1e-9);
}

// Note that because we just generate linear model, this doesn't exercise all
// the expansion code which is likely to lose the hint. Still it is a start.
TEST_P(RandomPreprocessorTest, TestHintSurvivePresolve) {
  CpModelProto model_proto = GenerateRandomProblem(GetSeedEnvName());

  // We only deal with feasible problem. Note that many are just INFEASIBLE, so
  // maybe we should generate something smarter.
  const CpSolverResponse first_solve = Solve(model_proto);
  if (first_solve.status() != CpSolverStatus::OPTIMAL &&
      first_solve.status() != CpSolverStatus::FEASIBLE) {
    return;
  }

  const int num_vars = model_proto.variables().size();
  for (int i = 0; i < num_vars; ++i) {
    model_proto.mutable_solution_hint()->add_vars(i);
    model_proto.mutable_solution_hint()->add_values(first_solve.solution(i));
  }

  // We just check that the hint is correct.
  SatParameters params;
  params.set_debug_crash_on_bad_hint(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  params.set_stop_after_first_solution(true);
  const CpSolverResponse with_hint = SolveWithParameters(model_proto, params);

  // Lets also test that the tightened domains contains the hint.
  model_proto.clear_objective();
  model_proto.clear_solution_hint();
  SatParameters tighten_params;
  tighten_params.set_keep_all_feasible_solutions_in_presolve(true);
  tighten_params.set_fill_tightened_domains_in_response(true);
  const CpSolverResponse with_tighten =
      SolveWithParameters(model_proto, tighten_params);
  EXPECT_EQ(with_tighten.tightened_variables().size(), num_vars);
  for (int i = 0; i < num_vars; i++) {
    EXPECT_TRUE(ReadDomainFromProto(with_tighten.tightened_variables(i))
                    .Contains(first_solve.solution(i)));
  }
}

TEST_P(RandomPreprocessorTest, SolveDiophantineWithAndWithoutPresolve) {
  absl::BitGen random;
  CpModelProto model_proto;
  model_proto.set_name("Random Diophantine");
  const int num_variables = absl::Uniform(random, 1, 6);
  for (int i = 0; i < num_variables; ++i) {
    IntegerVariableProto* var = model_proto.add_variables();
    int64_t min = absl::Uniform<int64_t>(random, -10, 10);
    int64_t max = absl::Uniform<int64_t>(random, -10, 10);
    if (max < min) std::swap(min, max);
    var->add_domain(min);
    var->add_domain(max);
  }
  const bool has_indicator = absl::Bernoulli(random, 0.5);
  if (has_indicator) {
    IntegerVariableProto* var = model_proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }

  auto* constraint = model_proto.add_constraints();
  if (has_indicator) constraint->add_enforcement_literal(num_variables);
  auto* lin = constraint->mutable_linear();
  lin->add_domain(absl::Uniform<int64_t>(random, -10, 10));
  lin->add_domain(lin->domain(0));
  for (int v = 0; v < num_variables; ++v) {
    lin->add_vars(v);
    lin->add_coeffs(GetRandomNonZeroAndNonInvertible(10, &random));
  }

  model_proto.mutable_objective()->set_scaling_factor(1);
  for (int v = 0; v < num_variables; ++v) {
    if (absl::Bernoulli(random, 0.5)) {
      model_proto.mutable_objective()->add_vars(v);
      model_proto.mutable_objective()->add_coeffs(
          absl::Uniform<int64_t>(random, -10, 10));
    }
  }
  if (has_indicator) {
    // Indicator should be deactivated only if the equation is unfeasible.
    model_proto.mutable_objective()->add_vars(num_variables);
    model_proto.mutable_objective()->add_coeffs(-10000);
  }

  if (!absl::GetFlag(FLAGS_dump_dir).empty()) {
    const std::string name =
        file::JoinPath(absl::GetFlag(FLAGS_dump_dir),
                       absl::StrCat(GetSeedEnvName(), ".pb.txt"));
    LOG(INFO) << "Dumping  model to '" << name << "'.";
    CHECK_OK(file::SetTextProto(name, model_proto, file::Defaults()));
  }

  SatParameters params;
  params.set_cp_model_presolve(true);
  const CpSolverResponse response_with =
      SolveWithParameters(model_proto, params);
  params.set_cp_model_presolve(false);
  const CpSolverResponse response_without =
      SolveWithParameters(model_proto, params);
  EXPECT_EQ(response_with.status(), response_without.status());
  EXPECT_NEAR(response_with.objective_value(),
              response_without.objective_value(), 1e-9);
}

INSTANTIATE_TEST_SUITE_P(All, RandomPreprocessorTest,
                         ::testing::Range(0, DEBUG_MODE ? 500 : 5000));

}  // namespace
}  // namespace sat
}  // namespace operations_research
