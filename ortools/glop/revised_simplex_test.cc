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

#include "ortools/glop/revised_simplex.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/flags.h"
#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/glop/preprocessor_testing.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse_row.h"
#include "ortools/util/fp_utils_testing.h"
#include "ortools/util/time_limit.h"

ABSL_DECLARE_FLAG(bool, simplex_stop_after_first_basis);
ABSL_FLAG(std::optional<std::mt19937::result_type>, revised_simplex_test_seed,
          std::nullopt, "Fixed seed to use to reproduce errors.");

namespace operations_research {
namespace glop {
namespace {

using ::testing::_;
using ::testing::AnyOf;
using ::testing::ElementsAre;

// Wrapper of std::mt19937 that picks a random seed that can be fixed to
// reproduce issues with the --revised_simplex_test_seed flag.
//
// It implements the UniformRandomBitGenerator named requirement so it can be
// used where a std::mt19937 would be used. See
// https://en.cppreference.com/cpp/named_req/UniformRandomBitGenerator.
//
// The `seed` should be logged and/or included in error messages of tests so
// that the flag can be used later on to reproduce issues (it is only logged as
// verbose log by this class). For example using SCOPED_TRACE() to make sure
// this is only visible when the test fails:
//
//   RandomWithFixableSeed fixable_seed(bit_gen);
//   SCOPED_TRACE(fixable_seed.seed);
//   ...
//
// For tests with loops, they should:
// * use a single RandomWithFixableSeed per loop,
// * and test `seed.fixed` to only execute a single loop when the seed is fixed
//   (as all loops would use the same seed anyway, making repetition useless),
// for example:
//
//   for (int i = 0; ...; ++i) {
//     RandomWithFixableSeed fixable_seed(bit_gen);
//     if (fixable_seed.fixed && i >= 1) break;
//     ...
//     EXPECT_THAT(...) << fixable_seed.seed;
//   }
//
struct RandomWithFixableSeed {
  // The seed used to initialize the std::mt19937.
  struct Seed {
    // The seed value.
    std::mt19937::result_type value ABSL_REQUIRE_EXPLICIT_INIT;

    // True if the seed used has been fixed by the --revised_simplex_test_seed
    // flag.
    //
    // This value is always the same as the one returned by fixed().
    bool fixed ABSL_REQUIRE_EXPLICIT_INIT;

    friend std::ostream& operator<<(std::ostream& out, const Seed& seed) {
      out << "RandomWithFixableSeed::Seed{.value=" << seed.value
          << ", fixed: " << (seed.fixed ? "true" : "false") << "}";
      if (!seed.fixed) {
        out << "; use --revised_simplex_test_seed=" << seed.value
            << " to fix seed";
      }
      return out;
    }
  };

  // UniformRandomBitGenerator API.
  using result_type = std::mt19937::result_type;

  // UniformRandomBitGenerator API.
  static constexpr result_type min() { return std::mt19937::min(); }
  static constexpr result_type max() { return std::mt19937::max(); }

  // Uses bit_gen to generate the seed for std::mt19937 when
  // --revised_simplex_test_seed is not set.
  explicit RandomWithFixableSeed(absl::BitGenRef bit_gen)
      : seed(NewSeed(bit_gen)), random(seed.value) {
    VLOG(1) << seed;
  }

  // UniformRandomBitGenerator API.
  result_type operator()() { return random(); }

  // The seed used to initialize the std::mt19937.
  const Seed seed;

  // The random generator initialized with seed_.
  std::mt19937 random;

 private:
  // Returns the value of --revised_simplex_test_seed.
  //
  // This function guarantees to always return a reference to the same constant
  // value.
  //
  // This is different to absl::GetFlag() that:
  // * returns a copy of the flag value,
  // * and the flag value can changed (with absl::SetFlag()).
  static const std::optional<std::mt19937::result_type>& fixed_seed() {
    static const absl::NoDestructor<std::optional<std::mt19937::result_type>>
        flag_value(absl::GetFlag(FLAGS_revised_simplex_test_seed));
    return *flag_value;
  }

  static Seed NewSeed(absl::BitGenRef bit_gen) {
    if (fixed_seed().has_value()) {
      return Seed{
          .value = *fixed_seed(),
          .fixed = true,
      };
    }
    return Seed{
        .value = absl::Uniform(bit_gen, min(), max()),
        .fixed = false,
    };
  }
};

// Verify that RandomWithFixableSeed implements the UniformRandomBitGenerator
// named requirement using the corresponding concept.
static_assert(std::uniform_random_bit_generator<RandomWithFixableSeed>);

// This compares the Solve() result with the given expectation. Note that we
// call revised_simplex->Solve() a few times with different parameters.
//
// Note: the interface for the RevisedSimplex class only provides
// minimization
//
// TODO(user): Factor out the solution checking code from lp_solver.cc and check
// that all the solution values and statuses (for the variable and constraints)
// make sense.
void CheckSolve(absl::string_view test_problem, ProblemStatus expected_status,
                Fractional expected_result,
                const GlopParameters& base_parameters) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  ASSERT_TRUE(ParseLp(test_problem, linear_program.get()));
  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  // We test with a few parameters.
  for (int i = 0; i < 4; ++i) {
    GlopParameters parameters = base_parameters;
    if (i != 0 && parameters.has_use_dual_simplex() &&
        parameters.use_dual_simplex()) {
      // TODO(user): for now the dual only support STEEPEST_EDGE.
      continue;
    }
    if (i == 1) {
      parameters.set_feasibility_rule(GlopParameters::DANTZIG);
      parameters.set_optimization_rule(GlopParameters::DANTZIG);
    }
    if (i == 2) {
      parameters.set_feasibility_rule(GlopParameters::DANTZIG);
      parameters.set_optimization_rule(GlopParameters::STEEPEST_EDGE);
    }
    if (i == 3) {
      // HACK: sometimes a test cannot pass with the dual: if we just know
      // the problem is dual-feasible, we cannot deduce anything on the primal!
      // In order to make this pass, we only modify unset parameters.
      if (parameters.has_use_dual_simplex()) continue;
      parameters.set_use_dual_simplex(!parameters.use_dual_simplex());
    }

    SCOPED_TRACE(absl::StrFormat("CheckSolve case %d:\n%s", i,
                                 absl::StrCat(parameters)));

    simplex->SetParameters(parameters);
    simplex->ClearStateForNextSolve();
    const SolveStatus solve_status =
        simplex->Solve(*linear_program, *time_limit);
    EXPECT_THAT(solve_status, Not(SolveStatusWith<SolveStatus::Abnormal>(_)));

    if (parameters.use_dual_simplex()) {
      // We need to do some translation for the dual.
      if (expected_status == ProblemStatus::PRIMAL_FEASIBLE) {
        EXPECT_THAT(solve_status,
                    SolveStatusWith<SolveStatus::DualFeasible>(_));
      } else if (expected_status == ProblemStatus::PRIMAL_UNBOUNDED) {
        EXPECT_THAT(solve_status,
                    SolveStatusWith<SolveStatus::DualInfeasible>(_));
      } else if (expected_status == ProblemStatus::PRIMAL_INFEASIBLE) {
        EXPECT_THAT(solve_status,
                    AnyOf(SolveStatusWith<SolveStatus::DualInfeasible>(_),
                          SolveStatusWith<SolveStatus::DualUnbounded>(_)));
      } else {
        EXPECT_THAT(solve_status, SolveStatusProblemStatusIs(expected_status));
      }
    } else {
      EXPECT_THAT(solve_status, SolveStatusProblemStatusIs(expected_status));
    }

    if (solve_status.Is<SolveStatus::Optimal>()) {
      const double kComparisonEpsilon = sqrt(kEpsilon);
      const Fractional result = simplex->GetObjectiveValue();
      EXPECT_THAT(result, WithinSameAbsoluteOrRelativeTolerance(
                              expected_result, kComparisonEpsilon));
    }

    EXPECT_NEAR(simplex->DeterministicTime(),
                time_limit->GetElapsedDeterministicTime(), 1e-15);
  }
}

TEST(RevisedSimplexTest, FixedVariable1) {
  const std::string kLinearProgram =
      "min: x1 + x2;"
      "8 <= x1 <= 8;"
      "0 <= x2 <= inf;"
      "r1: 2x1 + x2 <= 2;"
      "r2: 3x1 + 4x2 >= 12;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_INFEASIBLE;
  const Fractional kExpectedResult = 0.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, UpperBoundedVariable1) {
  const std::string kLinearProgram =
      "min: -x;"
      "-12 <= x <= -8;"
      "r1: x <= 20;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = 8;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, UpperBoundedVariable2) {
  const std::string kLinearProgram =
      "min: x;"
      "-12 <= x <= -8;"
      "r1: x <= 20;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -12;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, UpperBoundedVariable3) {
  const std::string kLinearProgram =
      "min: x;"
      "x <= 0;"
      "r1: x <= 20;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_UNBOUNDED;
  const Fractional kExpectedResult = 0.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, Infeasible0) {
  const std::string kLinearProgram =
      "max: 3x1 + 2x2;"
      "2x1 + x2 <= 2;"
      "3x1 + 4x2 >= 12;"
      "x1 >= 0;"
      "x2 >= 0;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_INFEASIBLE;
  const Fractional kExpectedResult = 0.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, Infeasible1) {
  const std::string kLinearProgram =
      "min: x;"
      "x >= 0;"
      "r1: x <= 100;"
      "r2: x >= 200;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_INFEASIBLE;
  const Fractional kExpectedResult = 0.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, Infeasible2) {
  const std::string kLinearProgram =
      "min: x;"
      "x >= 200;"
      "r1: x <= 100;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_INFEASIBLE;
  const Fractional kExpectedResult = 0.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, Unbounded1) {
  const std::string kLinearProgram =
      "min: -x;"
      "r1: x >= 100;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_UNBOUNDED;
  const Fractional kExpectedResult = 0.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, Unbounded2) {
  const std::string kLinearProgram =
      "min: x;"
      "r1: x <= 100;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_UNBOUNDED;
  const Fractional kExpectedResult = 0.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, ConstrainedVariableBounds) {
  const std::string kLinearProgram =
      "min: -x - y;"
      "0 <= x <= 10;"
      "0 <= y <= 10;"
      "r1: x + y <= 50;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -20;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, ConstrainedVariableBounds2) {
  const std::string kLinearProgram =
      "min: -x - y;"
      "x <= 10;"
      "y <= -8;"
      "r1: x + y <= 50;"
      "r2: 2x + 3y <= -5;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -5.0 / 3.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, EqualityConstraint) {
  const std::string kLinearProgram =
      "min: 2x + 3y;"
      "r1: x + y = 2;"
      "r2: x - y = 0;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = 5;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.26 ex.2.1.a.
// Optimal Solution: 10.5; x = 2.5, y = 1.5, z = 0
TEST(RevisedSimplexTest, Chvatalp26_21a) {
  const std::string kLinearProgram =
      "min: -3x - 2y - 4z;"
      "x >= 0;"
      "y >= 0;"
      "z >= 0;"
      "r1:  x + y + 2z <= 4;"
      "r2: 2x +     3z <= 5;"
      "r3: 2x + y + 3z <= 7;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -10.5;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// This solve a trivial problem x=1 with optimal value 10 (cost_of_x=7 +
// offset=3) for various parameters. See the comments in the ObjectiveLimit test
// below.
void ObjectiveLimitTest(Fractional objective_lower_limit,
                        Fractional objective_upper_limit, bool maximize,
                        Fractional objective_scaling_factor, bool use_dual,
                        ProblemStatus expected_status) {
  std::unique_ptr<LinearProgram> lp(new LinearProgram);
  const ColIndex a = lp->CreateNewVariable();
  lp->SetObjectiveScalingFactor(objective_scaling_factor);
  lp->SetVariableBounds(a, 1.0, 1.0);
  lp->SetObjectiveCoefficient(a, objective_scaling_factor * 7.0);
  lp->SetObjectiveOffset(objective_scaling_factor * 3.0);
  lp->SetMaximizationProblem(maximize);

  GlopParameters parameters;
  parameters.set_use_dual_simplex(use_dual);
  parameters.set_objective_lower_limit(objective_lower_limit);
  parameters.set_objective_upper_limit(objective_upper_limit);

  std::unique_ptr<RevisedSimplex> solver(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(lp.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  solver->SetParameters(parameters);
  EXPECT_THAT(solver->Solve(*lp, *time_limit),
              SolveStatusProblemStatusIs(expected_status));
  EXPECT_EQ(10.0, solver->GetObjectiveValue());

  EXPECT_EQ(solver->DeterministicTime(),
            time_limit->GetElapsedDeterministicTime());
}

TEST(RevisedSimplexTest, ObjectiveLimit) {
  // Some definition to shorten the lines below.
  const bool kMinimize = false;
  const bool kMaximize = true;
  const bool kPrimal = false;
  const bool kDual = true;
  const Fractional kPositiveScalingFactor = 1.0;
  const Fractional kNegativeScalingFactor = -1.0;

  // First cases where the objective limits do not matter:
  // The solver will not be able to prove that the objective is lower than
  // the lower limit or greater than the upper limit since 10 is in [0, 20].
  ObjectiveLimitTest(0.0, 20.0, kMaximize, kPositiveScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 20.0, kMaximize, kPositiveScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 20.0, kMinimize, kPositiveScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 20.0, kMinimize, kPositiveScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);

  // Same as above with negative scaling factor.
  ObjectiveLimitTest(0.0, 20.0, kMaximize, kNegativeScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 20.0, kMaximize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 20.0, kMinimize, kNegativeScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 20.0, kMinimize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);

  // With an upper limit of 9, the solver will abort early if it proves
  // that the objective is greater than 9 (in the cases where the objective
  // value moves up during the algorithm).
  ObjectiveLimitTest(0.0, 9.0, kMaximize, kPositiveScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 9.0, kMaximize, kPositiveScalingFactor, kPrimal,
                     ProblemStatus::PRIMAL_FEASIBLE);
  ObjectiveLimitTest(0.0, 9.0, kMinimize, kPositiveScalingFactor, kDual,
                     ProblemStatus::DUAL_FEASIBLE);
  ObjectiveLimitTest(0.0, 9.0, kMinimize, kPositiveScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);

  // With negative scaling factor, statuses are reversed.
  ObjectiveLimitTest(0.0, 9.0, kMaximize, kNegativeScalingFactor, kDual,
                     ProblemStatus::DUAL_FEASIBLE);
  ObjectiveLimitTest(0.0, 9.0, kMaximize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 9.0, kMinimize, kNegativeScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(0.0, 9.0, kMinimize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::PRIMAL_FEASIBLE);

  // With a lower limit of 11, the solver will abort early if it proves
  // that the objective is lower than 11 (in the cases where the objective
  // value moves down during the algorithm).
  ObjectiveLimitTest(11.0, 20.0, kMaximize, kPositiveScalingFactor, kDual,
                     ProblemStatus::DUAL_FEASIBLE);
  ObjectiveLimitTest(11.0, 20.0, kMaximize, kPositiveScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(11.0, 20.0, kMinimize, kPositiveScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(11.0, 20.0, kMinimize, kPositiveScalingFactor, kPrimal,
                     ProblemStatus::PRIMAL_FEASIBLE);

  // With negative scaling factor, statuses are reversed.
  ObjectiveLimitTest(11.0, 20.0, kMaximize, kNegativeScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(11.0, 20.0, kMaximize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::PRIMAL_FEASIBLE);
  ObjectiveLimitTest(11.0, 20.0, kMinimize, kNegativeScalingFactor, kDual,
                     ProblemStatus::DUAL_FEASIBLE);
  ObjectiveLimitTest(11.0, 20.0, kMinimize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);

  // We test the limit before the optimality, so at the optimal +/- epsilon we
  // should have a transition.
  ObjectiveLimitTest(10.0, 10.0, kMaximize, kNegativeScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(10.0, 10.0, kMaximize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(10.0, 10.0, kMinimize, kNegativeScalingFactor, kDual,
                     ProblemStatus::OPTIMAL);
  ObjectiveLimitTest(10.0, 10.0, kMinimize, kNegativeScalingFactor, kPrimal,
                     ProblemStatus::OPTIMAL);
  const double epsilon = 1e-4;
  ObjectiveLimitTest(10.0 + epsilon, 10.0 - epsilon, kMaximize,
                     kNegativeScalingFactor, kDual,
                     ProblemStatus::DUAL_FEASIBLE);
  ObjectiveLimitTest(10.0 + epsilon, 10.0 - epsilon, kMaximize,
                     kNegativeScalingFactor, kPrimal,
                     ProblemStatus::PRIMAL_FEASIBLE);
  ObjectiveLimitTest(10.0 + epsilon, 10.0 - epsilon, kMinimize,
                     kNegativeScalingFactor, kDual,
                     ProblemStatus::DUAL_FEASIBLE);
  ObjectiveLimitTest(10.0 + epsilon, 10.0 - epsilon, kMinimize,
                     kNegativeScalingFactor, kPrimal,
                     ProblemStatus::PRIMAL_FEASIBLE);
}

TEST(RevisedSimplexTest, ComputeBasicVariablesForState_Chvatalp26_21b) {
  const std::string kLinearProgram =
      "min: -5x1 - 6x2 - 9x3 - 8x4;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x3 >= 0;"
      "x4 >=0;"
      "r1: x1 + 2x2 +3x3 +  x4 <= 5;"
      "r2: x1 +  x2 +2x3 + 3x4 <= 3;";

  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  ASSERT_TRUE(ParseLp(kLinearProgram, linear_program.get()));
  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(linear_program.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  const BasisState state = {
      {VariableStatus::BASIC, VariableStatus::BASIC,
       VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_LOWER_BOUND,
       VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_LOWER_BOUND}};

  EXPECT_THAT(simplex->ComputeBasicVariablesForState(*linear_program, state),
              AbnormalityStatusIsOK());
  EXPECT_EQ(-17.0, simplex->GetObjectiveValue());
  EXPECT_EQ(1.0, simplex->GetVariableValue(ColIndex(0)));
  EXPECT_EQ(2.0, simplex->GetVariableValue(ColIndex(1)));
  EXPECT_EQ(0.0, simplex->GetVariableValue(ColIndex(2)));
  EXPECT_EQ(0.0, simplex->GetVariableValue(ColIndex(3)));
  EXPECT_EQ(-5.0, simplex->GetVariableValue(ColIndex(4)));
  EXPECT_EQ(-3.0, simplex->GetVariableValue(ColIndex(5)));
}

TEST(RevisedSimplexTest,
     ComputeBasicVariablesForState_InvalidStates_Chvatalp26_21b) {
  const std::string kLinearProgram =
      "min: -5x1 - 6x2 - 9x3 - 8x4;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x3 >= 0;"
      "x4 >= 0;"
      "r1: x1 + 2x2 +3x3 +  x4 <= 5;"
      "r2: x1 +  x2 +2x3 + 3x4 <= 3;";

  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  ASSERT_TRUE(ParseLp(kLinearProgram, linear_program.get()));
  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(linear_program.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  // There are three basic variables. In this case the third variable is made
  // non basic.
  const BasisState state = {
      {VariableStatus::BASIC, VariableStatus::BASIC, VariableStatus::BASIC,
       VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_UPPER_BOUND,
       VariableStatus::FREE}};

  EXPECT_THAT(simplex->ComputeBasicVariablesForState(*linear_program, state),
              AbnormalityStatusIsOK());
  EXPECT_EQ(-15.0, simplex->GetObjectiveValue());
  EXPECT_EQ(0.0, simplex->GetVariableValue(ColIndex(0)));
  EXPECT_EQ(VariableStatus::AT_LOWER_BOUND,
            simplex->GetVariableStatus(ColIndex(0)));

  EXPECT_NEAR(1.0, simplex->GetVariableValue(ColIndex(1)), 1e-10);
  EXPECT_EQ(VariableStatus::BASIC, simplex->GetVariableStatus(ColIndex(1)));

  EXPECT_NEAR(1.0, simplex->GetVariableValue(ColIndex(2)), 1e-10);
  EXPECT_EQ(VariableStatus::BASIC, simplex->GetVariableStatus(ColIndex(2)));

  EXPECT_EQ(0.0, simplex->GetVariableValue(ColIndex(3)));
  EXPECT_EQ(VariableStatus::AT_LOWER_BOUND,
            simplex->GetVariableStatus(ColIndex(3)));

  EXPECT_EQ(-5.0, simplex->GetVariableValue(ColIndex(4)));
  EXPECT_EQ(VariableStatus::AT_LOWER_BOUND,
            simplex->GetVariableStatus(ColIndex(4)));

  EXPECT_EQ(-3.0, simplex->GetVariableValue(ColIndex(5)));
  EXPECT_EQ(VariableStatus::AT_LOWER_BOUND,
            simplex->GetVariableStatus(ColIndex(5)));
}

// Chvatal p.26 ex.2.1.b.
// Optimal Solution: 17.0; x1 = 1.0, x2 = 2.0, x3 = 0, x4 = 0
TEST(RevisedSimplexTest, Chvatalp26_21b) {
  const std::string kLinearProgram =
      "min: -5x1 - 6x2 - 9x3 - 8x4;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x3 >= 0;"
      "x4 >=0;"
      "r1: x1 + 2x2 +3x3 +  x4 <= 5;"
      "r2: x1 +  x2 +2x3 + 3x4 <= 3;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -17;

  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.26 ex.2.1.c.
// Optimal Solution: 2.0; x1 = 1.0, x2 = 0
TEST(RevisedSimplexTest, Chvatalp26_21c) {
  const std::string kLinearProgram =
      "min: -2x1 - x2;"
      " x1 >= 0;"
      " x2 >= 0;"
      "r1: 2x1 + 3x2 <= 3;"
      "r2:  x1 + 5x2 <= 1;"
      "r3: 2x1 +  x2 <= 4;"
      "r4: 4x1 +  x2 <= 5;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -2;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.26 ex.2.2
// Optimal Solution: 8.0; x1 = 1.0, x2 = 2.0, x3 = 0, x4 = 0
TEST(RevisedSimplexTest, Chvatalp26_22) {
  const std::string kLinearProgram =
      "min: -2x1 - 3x2 - 5x3 - 4x4;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x3 >= 0;"
      "x4 >= 0;"
      "r1: x1 + 2x2 + 3x3 +  x4 <= 5;"
      "r2: x1 +  x2 + 2x3 + 3x4 <= 3;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -8;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.54
// Optimal Solution: 29.0; x1 = 0, x2 = 14, x3 = 0, x4 = 5
TEST(RevisedSimplexTest, Chvatalp54) {
  const std::string kLinearProgram =
      "min: -4x1 - x2 - 5x3 - 3x4;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x3 >= 0;"
      "x4 >= 0;"
      "r1: x1 -  x2 -  x3 + 3x4 <= 1;"
      "r2: 5x1 +  x2 + 3x3 + 8x4 <= 55;"
      "r3: -x1 + 2x2 + 3x3 - 5x4 <= 3;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -29;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.69 ex.5.2
// Optimal Solution: -0.6; x1 = 0.6, x2 = 0
TEST(RevisedSimplexTest, Chvatalp69_52) {
  const std::string kLinearProgram =
      "min: x1 + 2x2;"
      "x1 >= 0;"
      "x2 >= 0;"
      "r1: -3x1 +  x2 <= -1;"
      "r2:   x1 -  x2 <=  1;"
      "r3: -2x1 + 7x2 <=  6;"
      "r4:  9x1 - 4x2 <=  6;"
      "r5: -5x1 + 2x2 <= -3;"
      "r6:  7x1 - 3x2 <=  6;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = 0.6;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.69 ex.5.3.a.
// Optimal Solution: 507/59; x1 = 39/59, x2 = 0, x3 = 91/59,
//                           x4 = 166/59, x5 = 37/59
TEST(RevisedSimplexTest, Chvatalp69_53a) {
  const std::string kLinearProgram =
      "min: -7x1 - 6x2 - 5x3 + 2x4 - 3x5;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x3 >= 0;"
      "x4 >= 0;"
      "x5 >= 0;"
      "r1:  x1 + 3x2 + 5x3 - 2x4 + 2x5 <= 4;"
      "r2: 4x1 + 2x2 - 2x3 +  x4 +  x5 <= 3;"
      "r3: 2x1 + 4x2 + 4x3 - 2x4 + 5x5 <= 5;"
      "r4: 3x1 +  x2 + 2x3 -  x4 - 2x5 <= 1;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -507.0 / 59.0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.69 ex.5.3b
// Optimal Solution: 17; x1 = 0, x2 = 0, x3 = 2.5, x4 = 3.5, x5 = 0, x6 = 0.5
TEST(RevisedSimplexTest, Chvatalp69_53b) {
  const std::string kLinearProgram =
      "min: -4x1 - 5x2 -  x3 - 3x4 + 5x5 - 8x6;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x3 >= 0;"
      "x4 >= 0;"
      "x5 >= 0;"
      "x6 >= 0;"
      "r1:  x1       - 4x3 + 3x4 +  x5 +  x6 <= 1;"
      "r2: 5x1 + 3x2 +  x3       - 5x5 + 3x6 <= 4;"
      "r3: 4x1 + 5x2 - 3x3 + 3x4 - 4x5 +  x6 <= 4;"
      "r4:     -  x2       + 2x4 +  x5 - 5x6 <= 5;"
      "r5:-2x1 +  x2 +  x3 +  x4 + 2x5 + 2x6 <= 7;"
      "r6: 2x1 - 3x2 + 2x3 -  x4 + 4x5 + 5x6 <= 5;";
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -17;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.135 ex.8.1.b
// Unbounded
TEST(RevisedSimplexTest, Chvatalp135_81b) {
  const std::string kLinearProgram =
      "min: -3x1 - x2 - 4x3 - 2x4;"
      "x3 >= 0;"
      "x4 >= 0;"
      "r1:  x1 + 4x2 + 3x3 + 3x4 <= 2;"
      "r2: -x1 - 3x2 +  x3 -  x4 >= 2;"
      "r3:  x1 + 2x2 + 3x3 + 2x4 <= 3;"
      "r4: -x1 - 3x2 + 2x3 -  x4 >= 3;";
  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_UNBOUNDED;
  const Fractional kExpectedResult = 0;
  GlopParameters parameters;
  CheckSolve(kLinearProgram, kExpectedStatus, kExpectedResult, parameters);
}

// Chvatal p.135 ex.8.1.a
// Optimal Solution: 4.4; x1 = 6.8, x2 = 0, x3 = 0, x4 = 15, x5 = 2, x6 = 0,
//                        x7 = 0, x8 = 0.8
void CheckChvatalp135_81a(ProblemStatus expected_status,
                          Fractional expected_result,
                          const GlopParameters& parameters) {
  const std::string kLinearProgram =
      "min:  3x1 + x2 + x3 - 2x4 + x5 - x6 - x7 + 4x8;"
      "0 <= x1 <= 8;"
      "0 <= x2 <= 6;"
      "0 <= x3 <= 4;"
      "0 <= x4 <= 15;"
      "0 <= x5 <= 2;"
      "0 <= x6 <= 10;"
      "0 <= x7 <= 10;"
      "0 <= x8 <= 3;"
      "r1: x1      + 3x3 +  x4 - 5x5 - 2x6 + 4x7 - 6x8 =  7;"
      "r2:      x2 + 2x3 +  x4 - 4x5 -  x6 + 3x7 - 5x8 =  3;";
  CheckSolve(kLinearProgram, expected_status, expected_result, parameters);
}

TEST(RevisedSimplexTest, Chvatalp135_81a) {
  const ProblemStatus kExpectedStatus = ProblemStatus::OPTIMAL;
  const Fractional kExpectedResult = -4.4;
  GlopParameters parameters;
  CheckChvatalp135_81a(kExpectedStatus, kExpectedResult, parameters);
}

// Change parameters to improve code coverage.
TEST(RevisedSimplexTest, Chvatalp135_81aForCoverage) {
  GlopParameters parameters;
  parameters.set_use_scaling(false);
  parameters.set_feasibility_rule(GlopParameters::DANTZIG);
  parameters.set_optimization_rule(GlopParameters::DANTZIG);
  parameters.set_basis_refactorization_period(1);
  parameters.set_max_number_of_iterations(2);
  parameters.set_use_dual_simplex(false);

  const ProblemStatus kExpectedStatus = ProblemStatus::PRIMAL_FEASIBLE;
  const Fractional kExpectedResult = -2;

  CheckChvatalp135_81a(kExpectedStatus, kExpectedResult, parameters);
}

// Change parameters further to improve code coverage.
TEST(RevisedSimplexTest, Chvatalp135_81aForCoverage2) {
  GlopParameters parameters;
  parameters.set_use_scaling(false);
  parameters.set_basis_refactorization_period(1);
  absl::SetFlag(&FLAGS_simplex_stop_after_first_basis, true);

  const ProblemStatus kExpectedStatus = ProblemStatus::INIT;
  const Fractional kExpectedResult = 0;

  CheckChvatalp135_81a(kExpectedStatus, kExpectedResult, parameters);
}

TEST(RevisedSimplexTest, NoConstraints) {
  std::unique_ptr<LinearProgram> lp(new LinearProgram);
  const ColIndex a = lp->CreateNewVariable();
  const ColIndex b = lp->CreateNewVariable();
  lp->SetVariableBounds(a, -1, 3);
  lp->SetObjectiveCoefficient(a, -1);
  lp->SetVariableBounds(b, -2, 2);
  lp->SetObjectiveCoefficient(b, 1);

  std::unique_ptr<RevisedSimplex> solver(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(lp.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  EXPECT_THAT(solver->Solve(*lp, *time_limit),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(-5, solver->GetObjectiveValue());

  EXPECT_EQ(solver->DeterministicTime(),
            time_limit->GetElapsedDeterministicTime());
}

TEST(RevisedSimplexTest, RandomSingletonOnlyProblem) {
  // Generate a problem with only singleton columns that has an optimal
  // solution. The all-zero solution is feasible, and the problem is bounded by
  // construction.
  const ColIndex kNumCols(100000);
  const RowIndex kNumRows(100);

  absl::BitGen bit_gen;
  RandomWithFixableSeed fixable_seed(bit_gen);
  SCOPED_TRACE(fixable_seed.seed);

  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  for (ColIndex col(0); col < kNumCols; ++col) {
    const RowIndex row(absl::Uniform(fixable_seed, 0, kNumRows.value()));
    const Fractional coeff = absl::Uniform<double>(fixable_seed, -100.0, 100.0);
    const Fractional objective =
        absl::Uniform<double>(fixable_seed, -100.0, 100.0);
    Fractional lower_bound = absl::Uniform<double>(fixable_seed, -1000.0, 0.0);
    Fractional upper_bound = absl::Uniform<double>(fixable_seed, 0.0, 1000.0);
    if (absl::Bernoulli(fixable_seed, 0.1)) {
      // Set one of the bounds to kInfinity. This makes sure the problem stays
      // bounded.
      if (objective > 0.0) {
        upper_bound = kInfinity;
      } else {
        lower_bound = -kInfinity;
      }
    }

    // Create the corresponding singleton column.
    EXPECT_EQ(col, linear_program->CreateNewVariable());
    linear_program->SetVariableBounds(col, lower_bound, upper_bound);
    linear_program->SetObjectiveCoefficient(col, objective);
    linear_program->SetCoefficient(row, col, coeff);
  }
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  const std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(linear_program.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  linear_program->CleanUp();

  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
              SolveStatusWith<SolveStatus::Optimal>(_));

  // This tests that UseSingletonColumnInInitialBasis() works fine and that
  // the optimal was reached without any iterations!
  EXPECT_EQ(0, simplex->GetNumberOfIterations());
  EXPECT_EQ(simplex->DeterministicTime(),
            time_limit->GetElapsedDeterministicTime());
}

TEST(RevisedSimplexTest, PrimalUnbounded) {
  std::unique_ptr<LinearProgram> lp(new LinearProgram);
  const ColIndex a = lp->CreateNewVariable();
  const ColIndex b = lp->CreateNewVariable();
  lp->SetVariableBounds(a, 0.0, kInfinity);
  lp->SetVariableBounds(b, -kInfinity, 0.0);
  lp->SetConstraintBounds(RowIndex(0), 0.0, 0.0);
  lp->SetObjectiveCoefficient(b, 1.0);
  lp->SetCoefficient(RowIndex(0), a, 1.0);
  lp->SetCoefficient(RowIndex(0), b, 1.0);

  std::unique_ptr<RevisedSimplex> solver(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(lp.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  EXPECT_THAT(solver->Solve(*lp, *time_limit),
              SolveStatusWith<SolveStatus::PrimalUnbounded>(_));
  EXPECT_EQ(-kInfinity, solver->GetObjectiveValue());

  DenseRow ray = solver->GetPrimalRay();
  EXPECT_EQ(ColIndex(3), ray.size());
  EXPECT_EQ(1.0, ray[ColIndex(0)]);
  EXPECT_EQ(-1.0, ray[ColIndex(1)]);
  EXPECT_EQ(solver->DeterministicTime(),
            time_limit->GetElapsedDeterministicTime());
}

TEST(RevisedSimplexTest, DualUnboundedMaximization) {
  const std::string kLinearProgram =
      "max: 2x1 - x2;"
      "x1 >= 0;"
      "x2 >= 0;"
      "x1 + x2 >= 1;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  std::unique_ptr<RevisedSimplex> solver(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  EXPECT_THAT(solver->Solve(lp, *time_limit),
              SolveStatusWith<SolveStatus::PrimalUnbounded>(_));
  EXPECT_EQ(kInfinity, solver->GetObjectiveValue());

  DenseRow ray = solver->GetPrimalRay();
  EXPECT_EQ(ColIndex(3), ray.size());
  EXPECT_EQ(1.0, ray[ColIndex(0)]);
  EXPECT_EQ(0.0, ray[ColIndex(1)]);
  EXPECT_EQ(solver->DeterministicTime(),
            time_limit->GetElapsedDeterministicTime());
}

TEST(RevisedSimplexTest, DualUnbounded) {
  std::unique_ptr<LinearProgram> lp(new LinearProgram);
  const ColIndex a = lp->CreateNewVariable();
  const ColIndex b = lp->CreateNewVariable();
  const RowIndex r = lp->CreateNewConstraint();
  const RowIndex s = lp->CreateNewConstraint();

  lp->SetVariableBounds(a, 0.0, kInfinity);
  lp->SetVariableBounds(b, 0.0, kInfinity);

  // r: a + b = 1;
  lp->SetConstraintBounds(r, 1.0, 1.0);
  lp->SetCoefficient(r, a, 1.0);
  lp->SetCoefficient(r, b, 1.0);

  // s: a + b = 0;
  lp->SetConstraintBounds(s, 0.0, 0.0);
  lp->SetCoefficient(s, a, 1.0);
  lp->SetCoefficient(s, b, 1.0);

  std::unique_ptr<RevisedSimplex> solver(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(lp.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  parameters.set_use_dual_simplex(true);
  solver->SetParameters(parameters);
  EXPECT_THAT(solver->Solve(*lp, *time_limit),
              SolveStatusWith<SolveStatus::DualUnbounded>(_));
  EXPECT_EQ(ProblemStatus::DUAL_UNBOUNDED, solver->GetProblemStatus());
  EXPECT_EQ(kInfinity, solver->GetObjectiveValue());

  // This vector is such that ray.A >= 0 and ray.rhs < 0.
  DenseColumn ray = solver->GetDualRay();
  EXPECT_THAT(ray, ElementsAre(-1.0, 1.0));
  EXPECT_EQ(solver->DeterministicTime(),
            time_limit->GetElapsedDeterministicTime());
}

TEST(RevisedSimplexTest, PrimalOrDualAlgorithmAutoSelection) {
  // Generate a random problem with a clear feasible solution (0).
  const ColIndex kNumCols(100);
  const RowIndex kNumRows(100);

  absl::BitGen bit_gen;
  RandomWithFixableSeed fixable_seed(bit_gen);
  SCOPED_TRACE(fixable_seed.seed);

  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  for (ColIndex col(0); col < kNumCols; ++col) {
    EXPECT_EQ(col, linear_program->CreateNewVariable());
    linear_program->SetVariableBounds(col, 0.0, 1.0);
    linear_program->SetObjectiveCoefficient(
        col, absl::Uniform<double>(fixable_seed, -1.0, 0.0));
  }
  for (RowIndex row(0); row < kNumRows; ++row) {
    linear_program->SetConstraintBounds(row, 0.0, 1.0);
    for (ColIndex col(0); col < kNumCols; ++col) {
      linear_program->SetCoefficient(
          row, col, absl::Uniform<double>(fixable_seed, 0.0, 1.0));
    }
  }
  linear_program->CleanUp();

  // Set the default algorithm to the dual simplex but allow the solver to
  // change it during incremental solve.
  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(linear_program.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  parameters.set_use_dual_simplex(true);
  parameters.set_allow_simplex_algorithm_change(true);
  simplex->SetParameters(parameters);

  // Solve it.
  EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_TRUE(simplex->GetParameters().use_dual_simplex());
  EXPECT_GT(simplex->GetNumberOfIterations(), 10);

  // Now change one cost coefficient and expect the solver to use the primal
  // algorithm instead.
  linear_program->SetObjectiveCoefficient(ColIndex(10), -10);
  EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_FALSE(simplex->GetParameters().use_dual_simplex());
  EXPECT_GT(simplex->GetNumberOfIterations(), 10);

  // Solving it again should revert to the default dual algorithm and result
  // in no iterations this time (since we already have the optimal solution).
  EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_TRUE(simplex->GetParameters().use_dual_simplex());
  EXPECT_EQ(simplex->GetNumberOfIterations(), 0);
  EXPECT_EQ(simplex->DeterministicTime(),
            time_limit->GetElapsedDeterministicTime());
}

TEST(RevisedSimplexTest, PushTest) {
  const std::string kLinearProgram =
      "min: -x1 - x2;"
      "x1 >= 0;"
      "x2 >= 0;"
      "r1: x1 + x2 <= 1";
  GlopParameters parameters;
  parameters.set_push_to_vertex(true);
  parameters.set_crossover_bound_snapping_distance(0.0);

  operations_research::glop::BasisState basis_state;
  basis_state.statuses.push_back(VariableStatus::BASIC);
  basis_state.statuses.push_back(VariableStatus::BASIC);
  // Constraint status is flipped.
  basis_state.statuses.push_back(VariableStatus::AT_LOWER_BOUND);

  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  DenseRow initial_values;
  initial_values.push_back(0.5);
  initial_values.push_back(0.5);
  initial_values.push_back(-1.0);

  RevisedSimplex simplex;
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  simplex.SetParameters(parameters);
  simplex.LoadStateForNextSolve(basis_state);
  simplex.SetStartingVariableValuesForNextSolve(initial_values);

  ASSERT_THAT(simplex.Solve(linear_program, *time_limit),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(simplex.GetObjectiveValue(), -1);

  std::vector<Fractional> sol(2);
  sol[0] = simplex.GetVariableValue(ColIndex(0));
  sol[1] = simplex.GetVariableValue(ColIndex(1));
  EXPECT_THAT(sol, AnyOf(ElementsAre(1.0, 0.0), ElementsAre(0.0, 1.0)));
}

TEST(RevisedSimplexTest, PushTestWithFreeVariables) {
  const std::string kLinearProgram =
      "min: -x1 - x2;"
      "x1 >= 0;"
      "x2 >= 0;"
      "-inf <= x3 <= inf;"
      "r1: x1 + x2 <= 1";
  GlopParameters parameters;
  parameters.set_push_to_vertex(true);
  parameters.set_crossover_bound_snapping_distance(0.0);

  operations_research::glop::BasisState basis_state;
  basis_state.statuses.push_back(VariableStatus::BASIC);
  basis_state.statuses.push_back(VariableStatus::BASIC);
  basis_state.statuses.push_back(VariableStatus::FREE);
  // Constraint status is flipped.
  basis_state.statuses.push_back(VariableStatus::AT_LOWER_BOUND);

  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  DenseRow initial_values;
  initial_values.push_back(0.5);
  initial_values.push_back(0.5);
  initial_values.push_back(1000.0);
  initial_values.push_back(-1.0);

  RevisedSimplex simplex;
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  simplex.SetParameters(parameters);
  simplex.LoadStateForNextSolve(basis_state);
  simplex.SetStartingVariableValuesForNextSolve(initial_values);

  ASSERT_THAT(simplex.Solve(linear_program, *time_limit),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(simplex.GetObjectiveValue(), -1);

  std::vector<Fractional> sol(3);
  sol[0] = simplex.GetVariableValue(ColIndex(0));
  sol[1] = simplex.GetVariableValue(ColIndex(1));
  sol[2] = simplex.GetVariableValue(ColIndex(2));
  EXPECT_THAT(sol,
              AnyOf(ElementsAre(1.0, 0.0, 0.0), ElementsAre(0.0, 1.0, 0.0)));
}

/*******************************************************************************
 * INCREMENTALITY TESTS.
 ******************************************************************************/

// TODO(user): Use a fixture and TestWithParam for these tests.

struct ExpectedSolution {
  ProblemStatus status;
  Fractional objective_value;
  int64_t num_iterations;
};

// Generates a random problem.
void CreateRandomProblem(ColIndex num_cols, RowIndex num_rows,
                         LinearProgram* linear_program,
                         absl::BitGenRef random) {
  linear_program->Clear();
  for (ColIndex col(0); col < num_cols; ++col) {
    EXPECT_EQ(col, linear_program->CreateNewVariable());
    linear_program->SetVariableBounds(col, 0.0, 1.0);
    linear_program->SetObjectiveCoefficient(
        col, absl::Uniform<double>(random, 0.0, 1.0));
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    linear_program->SetConstraintBounds(row, 1.0, 1.0);
    for (ColIndex col(0); col < num_cols; ++col) {
      linear_program->SetCoefficient(row, col,
                                     absl::Uniform<double>(random, 0.0, 1.0));
    }
  }
  linear_program->CleanUp();

  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
}

// Generates a random problem with a non-trivial first solution and solves it to
// get the expected solution, status and number of iterations.
ExpectedSolution SolveRandomProblem(ColIndex num_cols, RowIndex num_rows,
                                    LinearProgram* linear_program,
                                    absl::BitGenRef random,
                                    bool use_dual_simplex) {
  CreateRandomProblem(num_cols, num_rows, linear_program, random);

  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters parameters;
  parameters.set_use_dual_simplex(use_dual_simplex);
  simplex->SetParameters(parameters);
  const SolveStatus status = simplex->Solve(*linear_program, *time_limit);
  EXPECT_THAT(status, Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
  return {status.problem_status(), simplex->GetObjectiveValue(),
          simplex->GetNumberOfIterations()};
}

void RandomTestsOfIncrementalityOnSameProblem(ColIndex num_cols,
                                              RowIndex num_rows, int num_tests,
                                              bool use_dual_simplex) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  absl::BitGen bit_gen;
  for (int test = 0; test < num_tests; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d / %d; %s.", test + 1,
                                    num_tests,
                                    absl::FormatStreamed(fixable_seed.seed)));
    // Initialize data for this test.
    const ExpectedSolution solution =
        SolveRandomProblem(num_cols, num_rows, linear_program.get(),
                           fixable_seed, use_dual_simplex);
    // Note that we've already run the preprocessor in SolveRandomProblem and
    // linear_program already has slack variables added to it. We don't need to
    // re-run it here again.
    EXPECT_GT(solution.num_iterations, 0) << err;
    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    GlopParameters parameters;
    parameters.set_use_dual_simplex(use_dual_simplex);
    parameters.set_max_number_of_iterations(1);
    simplex->SetParameters(parameters);

    // Run the revised simplex 1 iteration at a time, for the same number of
    // total iterations which is required when running simplex continuously.
    //
    // NOTE(user): It is possible that the number of iterations changes
    // due to calling revised simplex incrementally, so we run it for 5 more
    // iterations which is the lowest possible number to make the test pass as
    // of 12/12/2016. Note that the incremental version may need less than
    // solution.num_iterations, in which case the last few iterations will just
    // re-check the solution.
    //
    // TODO(user): Investigate and fix as there is no good reason for this. This
    // is because we are not fully incremental and some quantities are
    // recomputed from scratch. There is various places where this happens:
    // - In the primal phase I, the variable values are re-evaluated at each new
    //   incremental call.
    // - As of 2016/10/20, it has been observed that with primal simplex in
    //   incremental mode, the number of required iterations may increase by 1
    //   due to "reoptimizations" (i.e., making the solution more precise to
    //   transform it from PRIMAL_FEASIBLE to OPTIMAL) not being triggered after
    //   hitting the iteration time limit in the last call of Solve().
    std::optional<SolveStatus> last_status;
    for (int i = 0; i < solution.num_iterations + 5; ++i) {
      SolveStatus status = simplex->Solve(*linear_program, *time_limit);
      EXPECT_THAT(status, Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
          << err;
      last_status = status;
    }
    ASSERT_NE(last_status, std::nullopt);
    EXPECT_EQ(solution.status, last_status->problem_status()) << err;
    // Validate final state.
    if (ProblemStatus::OPTIMAL == solution.status) {
      // TODO(user): During warm-start we recompute some quantities, thus the
      // path and the exact value of the solution may differ when using
      // incrementality. Replace with a strict equality once this is fixed.
      EXPECT_NEAR(solution.objective_value, simplex->GetObjectiveValue(), 1e-6)
          << err;
    }
    EXPECT_EQ(simplex->DeterministicTime(),
              time_limit->GetElapsedDeterministicTime());
  }
}

TEST(RevisedSimplexTest, PrimalIsFullyIncrementalOnRandomTinyProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(10),  // Number of variables.
      RowIndex(5),   // Number of constraints.
      25,            // Number of repeats.
      false);        // Use dual simplex?
}

TEST(RevisedSimplexTest, DualIsFullyIncrementalOnRandomTinyProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(10),  // Number of variables.
      RowIndex(5),   // Number of constraints.
      25,            // Number of repeats.
      true);         // Use dual simplex?
}

TEST(RevisedSimplexTest, PrimalIsFullyIncrementalOnRandomSmallProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(50),  // Number of variables.
      RowIndex(25),  // Number of constraints.
      10,            // Number of repeats.
      false);        // Use dual simplex?
}

TEST(RevisedSimplexTest, DualIsFullyIncrementalOnRandomSmallProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(50),  // Number of variables.
      RowIndex(25),  // Number of constraints.
      10,            // Number of repeats.
      true);         // Use dual simplex?
}

TEST(RevisedSimplexTest, PrimalIsFullyIncrementalOnRandomMediumProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(100),  // Number of variables.
      RowIndex(50),   // Number of constraints.
      2,              // Number of repeats.
      false);         // Use dual simplex?
}

TEST(RevisedSimplexTest, DualIsFullyIncrementalOnRandomMediumProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(100),  // Number of variables.
      RowIndex(50),   // Number of constraints.
      2,              // Number of repeats.
      true);          // Use dual simplex?
}

void RandomTestsOfBasisLoadOnSameProblem(ColIndex num_cols, RowIndex num_rows,
                                         int num_tests, bool use_dual_simplex) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);

  absl::BitGen bit_gen;
  for (int test = 0; test < num_tests; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d / %d; %s.", test + 1,
                                    num_tests,
                                    absl::FormatStreamed(fixable_seed.seed)));
    // Initialize data for this test.
    const ExpectedSolution solution =
        SolveRandomProblem(num_cols, num_rows, linear_program.get(),
                           fixable_seed, use_dual_simplex);
    GlopParameters parameters;
    parameters.set_use_dual_simplex(use_dual_simplex);
    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    parameters.set_max_number_of_iterations(1);

    // Run the revised simplex 1 iteration at a time, for the same number of
    // total iterations which is required when running simplex continuously.
    //
    // Note(user): Like in RandomTestsOfIncrementalityOnSameProblem(),
    // the number of iterations can actually vary slighlty, so we go a bit
    // further than the original number. See the comment in that function for
    // more details.
    //
    // Note: we need to call each time simplex->SetParameters() to reset the
    // random seed, otherwise the iterations might be slightly different.
    for (int i = 0; i < solution.num_iterations + 5; ++i) {
      // Run 1 iteration of simplex to get the expected state after 1 iteration.
      const BasisState prev_state = simplex->GetState();
      simplex->SetParameters(parameters);
      EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
                  Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
          << err;
      const BasisState expected_state = simplex->GetState();

      // Go back 1 iteration and run 1 iteration of simplex using the same
      // instance of the revised simplex object. Verify the state is the same.
      simplex->SetParameters(parameters);
      simplex->LoadStateForNextSolve(prev_state);
      EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
                  Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
          << err;
      const BasisState& observed_state = simplex->GetState();
      EXPECT_THAT(observed_state.statuses,
                  ::testing::ContainerEq(expected_state.statuses))
          << err;

      // Go back 1 iteration and run 1 iteration of simplex using a fresh
      // instance of the revised simplex object. Verify the state is the same.
      std::unique_ptr<RevisedSimplex> fresh_simplex(new RevisedSimplex);
      fresh_simplex->SetParameters(parameters);
      fresh_simplex->LoadStateForNextSolve(prev_state);
      EXPECT_THAT(fresh_simplex->Solve(*linear_program, *time_limit),
                  Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
          << err;
      const BasisState& fresh_observed_state = fresh_simplex->GetState();
      EXPECT_THAT(fresh_observed_state.statuses,
                  ::testing::ContainerEq(expected_state.statuses))
          << err;
    }

    // Validate final state.
    EXPECT_EQ(solution.status, simplex->GetProblemStatus()) << err;
    if (ProblemStatus::OPTIMAL == solution.status) {
      // TODO(user): During warm-start we recompute some quantities, thus the
      // path and the exact value of the solution may differ when using
      // incrementality. Replace with a strict equality once this is fixed.
      EXPECT_NEAR(solution.objective_value, simplex->GetObjectiveValue(), 1e-10)
          << err;
    }
  }
}

TEST(RevisedSimplexTest, PrimalSupportsLoadingBasisStateOnRandomTinyProblems) {
  RandomTestsOfBasisLoadOnSameProblem(ColIndex(10),  // Number of variables.
                                      RowIndex(5),   // Number of constraints.
                                      25,            // Number of repeats.
                                      false);        // Use dual simplex?
}

TEST(RevisedSimplexTest, DualSupportsLoadingBasisStateOnRandomTinyProblems) {
  RandomTestsOfBasisLoadOnSameProblem(ColIndex(10),  // Number of variables.
                                      RowIndex(5),   // Number of constraints.
                                      25,            // Number of repeats.
                                      true);         // Use dual simplex?
}

TEST(RevisedSimplexTest, PrimalSupportsLoadingBasisStateOnRandomSmallProblems) {
  RandomTestsOfBasisLoadOnSameProblem(ColIndex(50),  // Number of variables.
                                      RowIndex(25),  // Number of constraints.
                                      100,           // Number of repeats.
                                      false);        // Use dual simplex?
}

TEST(RevisedSimplexTest, DualSupportsLoadingBasisStateOnRandomSmallProblems) {
  RandomTestsOfBasisLoadOnSameProblem(ColIndex(50),  // Number of variables.
                                      RowIndex(25),  // Number of constraints.
                                      10,            // Number of repeats.
                                      true);         // Use dual simplex?
}

// TODO(user): This test fails on the 2nd of the 2 random problems.
// The basises do not match.
TEST(RevisedSimplexTest,
     DISABLED_PrimalSupportsLoadingBasisStateOnRandomMediumProblems) {
  RandomTestsOfBasisLoadOnSameProblem(ColIndex(100),  // Number of variables.
                                      RowIndex(50),   // Number of constraints.
                                      2,              // Number of repeats.
                                      false);         // Use dual simplex?
}

TEST(RevisedSimplexTest, DualSupportsLoadingBasisStateOnRandomMediumProblems) {
  RandomTestsOfBasisLoadOnSameProblem(ColIndex(100),  // Number of variables.
                                      RowIndex(50),   // Number of constraints.
                                      2,              // Number of repeats.
                                      true);          // Use dual simplex?
}

void RandomTestsOfPrimalEqualsDual(ColIndex num_cols, RowIndex num_rows,
                                   int num_tests) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  absl::BitGen bit_gen;
  for (int test = 0; test < num_tests; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d / %d; %s.", test + 1,
                                    num_tests,
                                    absl::FormatStreamed(fixable_seed.seed)));
    // Initialize data for this test.
    const ExpectedSolution solution_primal = SolveRandomProblem(
        num_cols, num_rows, linear_program.get(), fixable_seed, false);

    std::unique_ptr<RevisedSimplex> simplex_dual(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    GlopParameters parameters;
    parameters.set_use_dual_simplex(true);
    simplex_dual->SetParameters(parameters);
    const SolveStatus status =
        simplex_dual->Solve(*linear_program, *time_limit);
    EXPECT_THAT(status, Not(SolveStatusWith<SolveStatus::Abnormal>(_))) << err;
    EXPECT_TRUE((solution_primal.status == ProblemStatus::OPTIMAL &&
                 status.problem_status() == ProblemStatus::OPTIMAL) ||
                (solution_primal.status == ProblemStatus::PRIMAL_UNBOUNDED &&
                 status.problem_status() == ProblemStatus::DUAL_INFEASIBLE) ||
                (solution_primal.status == ProblemStatus::PRIMAL_INFEASIBLE &&
                 status.problem_status() == ProblemStatus::DUAL_UNBOUNDED) ||
                (solution_primal.status == ProblemStatus::PRIMAL_INFEASIBLE &&
                 status.problem_status() == ProblemStatus::DUAL_INFEASIBLE))
        << err;
    if (ProblemStatus::OPTIMAL == solution_primal.status) {
      EXPECT_NEAR(solution_primal.objective_value,
                  simplex_dual->GetObjectiveValue(), 1e-10)
          << err;
    }
  }
}

TEST(RevisedSimplexTest, PrimalAndDualGiveSameSolutionOnRandomSmallProblems) {
  RandomTestsOfPrimalEqualsDual(ColIndex(50),  // Number of variables.
                                RowIndex(25),  // Number of constraints.
                                10);           // Number of repeats
}

TEST(RevisedSimplexTest, PrimalAndDualGiveSameSolutionOnRandomMediumProblems) {
  RandomTestsOfPrimalEqualsDual(ColIndex(100),  // Number of variables.
                                RowIndex(50),   // Number of constraints.
                                2);             // Number of repeats
}

void RandomTestsOfIncrementalChanges(ColIndex num_cols, RowIndex num_rows,
                                     ColIndex add_cols, RowIndex add_rows,
                                     int num_tests, bool use_dual_simplex) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  absl::BitGen bit_gen;
  for (int test = 0; test < num_tests; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d / %d; %s.", test + 1,
                                    num_tests,
                                    absl::FormatStreamed(fixable_seed.seed)));
    // Initialize data for this test.
    CreateRandomProblem(num_cols, num_rows, linear_program.get(), fixable_seed);

    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    GlopParameters parameters;
    parameters.set_use_dual_simplex(use_dual_simplex);
    simplex->SetParameters(parameters);
    EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
                Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
        << err;

    linear_program->DeleteSlackVariables();
    for (ColIndex new_col(0); new_col < add_cols; ++new_col) {
      const ColIndex col(num_cols + new_col);
      EXPECT_EQ(col, linear_program->CreateNewVariable());
      linear_program->SetVariableBounds(col, 0.0, 1.0);
      linear_program->SetObjectiveCoefficient(
          col, absl::Uniform<double>(fixable_seed, 0.0, 1.0));
      for (RowIndex row(0); row < num_rows; ++row) {
        linear_program->SetCoefficient(
            row, col, absl::Uniform<double>(fixable_seed, 0.0, 1.0));
      }
    }
    for (RowIndex new_row(0); new_row < add_rows; ++new_row) {
      const RowIndex row(num_rows + new_row);
      linear_program->SetConstraintBounds(row, 1.0, 1.0);
      for (ColIndex col(0); col < num_cols + add_cols; ++col) {
        linear_program->SetCoefficient(
            row, col, absl::Uniform<double>(fixable_seed, 0.0, 1.0));
      }
    }

    linear_program->CleanUp();

    AddSlackVariablesPreprocessor preprocessor(&parameters);
    preprocessor.SetTimeLimit(time_limit.get());
    EXPECT_THAT(preprocessor.Run(linear_program.get()),
                PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                     /*status=*/std::nullopt));
    const SolveStatus status = simplex->Solve(*linear_program, *time_limit);
    EXPECT_THAT(status, Not(SolveStatusWith<SolveStatus::Abnormal>(_))) << err;

    // Note that the number of iterations is not necessarily lower.
    std::unique_ptr<RevisedSimplex> simplex_redo(new RevisedSimplex);
    simplex_redo->SetParameters(parameters);
    const SolveStatus redo_status =
        simplex_redo->Solve(*linear_program, *time_limit);
    EXPECT_THAT(redo_status, Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
        << err;

    EXPECT_EQ(status, redo_status) << err;

    if (ProblemStatus::OPTIMAL == simplex->GetProblemStatus()) {
      EXPECT_NEAR(simplex->GetObjectiveValue(),
                  simplex_redo->GetObjectiveValue(), 1e-10)
          << err;
    }
  }
}

TEST(RevisedSimplexTest, IncrementalRowAdditionsOnRandomTinyProblems) {
  RandomTestsOfIncrementalChanges(ColIndex(10),  // Number of variables.
                                  RowIndex(5),   // Number of constraints.
                                  ColIndex(0),   // Number of new variables.
                                  RowIndex(2),   // Number of new constraints.
                                  25,            // Number of repeats
                                  true);         // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalRowAdditionsOnRandomSmallProblems) {
  RandomTestsOfIncrementalChanges(ColIndex(50),  // Number of variables.
                                  RowIndex(25),  // Number of constraints.
                                  ColIndex(0),   // Number of new variables.
                                  RowIndex(5),   // Number of new constraints.
                                  10,            // Number of repeats
                                  true);         // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalRowAdditionsOnRandomMediumProblems) {
  RandomTestsOfIncrementalChanges(ColIndex(100),  // Number of variables.
                                  RowIndex(50),   // Number of constraints.
                                  ColIndex(0),    // Number of new variables.
                                  RowIndex(5),    // Number of new constraints.
                                  2,              // Number of repeats
                                  true);          // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalColAdditionsOnRandomTinyProblems) {
  RandomTestsOfIncrementalChanges(ColIndex(10),  // Number of variables.
                                  RowIndex(5),   // Number of constraints.
                                  ColIndex(2),   // Number of new variables.
                                  RowIndex(0),   // Number of new constraints.
                                  25,            // Number of repeats
                                  false);        // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalColAdditionsOnRandomSmallProblems) {
  RandomTestsOfIncrementalChanges(ColIndex(50),  // Number of variables.
                                  RowIndex(25),  // Number of constraints.
                                  ColIndex(5),   // Number of new variables.
                                  RowIndex(0),   // Number of new constraints.
                                  10,            // Number of repeats
                                  false);        // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalColAdditionsOnRandomMediumProblems) {
  RandomTestsOfIncrementalChanges(ColIndex(100),  // Number of variables.
                                  RowIndex(50),   // Number of constraints.
                                  ColIndex(5),    // Number of new variables.
                                  RowIndex(0),    // Number of new constraints.
                                  2,              // Number of repeats
                                  false);         // Use dual simplex
}

TEST(RevisedSimplexTest, EmptyProblemAndIncrementalSolve) {
  LinearProgram linear_program;
  linear_program.AddSlackVariablesWhereNecessary(false);

  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  EXPECT_THAT(simplex->Solve(linear_program, *time_limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
  EXPECT_THAT(simplex->Solve(linear_program, *time_limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
}

namespace {
std::vector<DenseRow> ConvertSparseRowsToDenseRows(
    RowIndex num_rows, ColIndex num_cols,
    const RevisedSimplexDictionary& input) {
  std::vector<DenseRow> output(num_rows.value());
  RowIndex r(0);
  for (const SparseRow& row : input) {
    row.CopyToDenseVector(num_cols, &output[r.value()]);
    ++r;
  }
  return output;
}
}  // namespace

void CheckComputeDictionary(absl::string_view test_problem,
                            absl::Span<const DenseRow> expected_output) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  ASSERT_TRUE(ParseLp(test_problem, linear_program.get()));
  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  GlopParameters parameters;
  parameters.set_max_number_of_iterations(1);
  simplex->SetParameters(parameters);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  AddSlackVariablesPreprocessor preprocessor(&parameters);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(linear_program.get()),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));

  const std::vector<DenseRow> output = ConvertSparseRowsToDenseRows(
      simplex->GetProblemNumRows(), simplex->GetProblemNumCols(),
      RevisedSimplexDictionary(nullptr, simplex.get()));
  EXPECT_EQ(expected_output.size(), output.size());
  const Fractional kTolerance = 1e-5;
  for (int r = 0; r < expected_output.size(); ++r) {
    const DenseRow& expected_row = expected_output[r];
    const DenseRow& output_row = output[r];
    EXPECT_EQ(expected_row.size(), output_row.size());
    for (ColIndex c(0); c < expected_row.size(); ++c) {
      EXPECT_THAT(output_row[c],
                  testing::DoubleNear(expected_row[c], kTolerance));
    }
  }
}

TEST(RevisedSimplexComputeDictionaryTest, FixedVariable1) {
  const std::string kLinearProgram =
      "min: x1 + x2;"
      "8 <= x1 <= 8;"
      "0 <= x2 <= inf;"
      "r1: 2x1 + x2 <= 2;"
      "r2: 3x1 + 4x2 >= 12;";
  const std::vector<DenseRow> kExpectedOutput = {{2, 1, 1, 0},  //
                                                 {3, 4, 0, 1}};
  CheckComputeDictionary(kLinearProgram, kExpectedOutput);
}

TEST(RevisedSimplexComputeDictionaryTest, Chvatalp26_21a) {
  const std::string kLinearProgram =
      "min: -3x - 2y - 4z;"
      "x >= 0;"
      "y >= 0;"
      "z >= 0;"
      "r1:  x + y + 2z <= 4;"
      "r2: 2x +     3z <= 5;"
      "r3: 2x + y + 3z <= 7;";
  const std::vector<DenseRow> kExpectedOutput = {{2, 0, 3, 0, 1, 0},   //
                                                 {1, 0, 1, -1, 0, 1},  //
                                                 {1, 1, 2, 1, 0, 0}};
  CheckComputeDictionary(kLinearProgram, kExpectedOutput);
}

TEST(RevisedSimplexComputeDictionaryTest, Chvatalp135_81b) {
  const std::string kLinearProgram =
      "min: -3x1 - x2 - 4x3 - 2x4;"
      "x3 >= 0;"
      "x4 >= 0;"
      "r1:  x1 + 4x2 + 3x3 + 3x4 <= 2;"
      "r2: -x1 - 3x2 +  x3 -  x4 >= 2;"
      "r3:  x1 + 2x2 + 3x3 + 2x4 <= 3;"
      "r4: -x1 - 3x2 + 2x3 -  x4 >= 3;";
  const std::vector<DenseRow> kExpectedOutput = {
      {-0.333333, 0, 4.33333, 1.66667, 1, 1.33333, 0, 0},   //
      {0.333333, 1, -0.33333, 0.33333, 0, -0.33333, 0, 0},  //
      {0.333333, 0, 3.66667, 1.33333, 0, 0.66666, 1, 0},    //
      {0, 0, 1, 0, 0, -1, 0, 1}};
  CheckComputeDictionary(kLinearProgram, kExpectedOutput);
}

TEST(RevisedSimplexComputeDictionaryTest, Chvatalp135_81a) {
  const std::string kLinearProgram =
      "min:  3x1 + x2 + x3 - 2x4 + x5 - x6 - x7 + 4x8;"
      "0 <= x1 <= 8;"
      "0 <= x2 <= 6;"
      "0 <= x3 <= 4;"
      "0 <= x4 <= 15;"
      "0 <= x5 <= 2;"
      "0 <= x6 <= 10;"
      "0 <= x7 <= 10;"
      "0 <= x8 <= 3;"
      "r1: x1      + 3x3 +  x4 - 5x5 - 2x6 + 4x7 - 6x8 =  7;"
      "r2:      x2 + 2x3 +  x4 - 4x5 -  x6 + 3x7 - 5x8 =  3;";
  const std::vector<DenseRow> kExpectedOutput = {
      {1, -1, 1, 0, -1, -1, 1, -1, 1, -1},  //
      {0, 1, 2, 1, -4, -1, 3, -5, 0, 1}};
  CheckComputeDictionary(kLinearProgram, kExpectedOutput);
}

TEST(RevisedSimplexDictionaryTest, BeginAndEndIterators) {
  const std::string kLinearProgram =
      "min: x1 + x2;"
      "8 <= x1 <= 8;"
      "0 <= x2 <= inf;"
      "r1: 2x1 + x2 <= 2;"
      "r2: 3x1 + 4x2 >= 12;";
  const std::vector<DenseRow> kExpectedOutput = {{2, 1, 1, 0},  //
                                                 {3, 4, 0, 1}};
  CheckComputeDictionary(kLinearProgram, kExpectedOutput);
}

TEST(RevisedSimplexDictionaryTest, LeftInverseForUnitRow) {
  const std::string kLinearProgram = R"(
    max:   x1 + x2 + x3;
    -8 <= -x1       -x3 <= -1;
    -7 <= -x1  -x2      <= -1;
           x1 +2x2      <= 12;
    x1 >= 0;
    x2 >= 0;
    x3 >= 0;
  )";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
  lp.AddSlackVariablesWhereNecessary(false);

  RevisedSimplex simplex;
  std::unique_ptr<TimeLimit> limit = TimeLimit::Infinite();
  ASSERT_THAT(simplex.Solve(lp, *limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));

  // The basis matrix is:
  // | -1       |
  // |    1  -1 |
  // |        2 |
  const RowToColMapping basis = {ColIndex(2), ColIndex(4), ColIndex(1)};
  EXPECT_EQ(simplex.GetBasisVector(), basis);

  ScatteredRow row0, row1, row2;
  simplex.GetBasisFactorization().LeftSolveForUnitRow(ColIndex(0), &row0);
  simplex.GetBasisFactorization().LeftSolveForUnitRow(ColIndex(1), &row1);
  simplex.GetBasisFactorization().LeftSolveForUnitRow(ColIndex(2), &row2);
  EXPECT_EQ(row0.values, (DenseRow{-1.0, 0.0, 0.0}));
  EXPECT_EQ(row1.values, (DenseRow{0.0, 1.0, 0.5}));
  EXPECT_EQ(row2.values, (DenseRow{0.0, 0.0, 0.5}));
}

TEST(RevisedSimplexTest, PolishSolution) {
  const std::string kLinearProgram = R"(
    x + 2y = 2;
    0 <= x <= 1;
    0 <= y <= 1;
  )";

  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
  lp.AddSlackVariablesWhereNecessary(false);

  // This test relies on having this particular solution to show the difference
  // that polish makes.
  BasisState state;
  state.statuses = {VariableStatus::AT_UPPER_BOUND, VariableStatus::BASIC,
                    VariableStatus::FIXED_VALUE};

  RevisedSimplex simplex;
  GlopParameters params;
  params.set_primal_polish(true);
  simplex.SetParameters(params);

  std::unique_ptr<TimeLimit> limit = TimeLimit::Infinite();
  simplex.LoadStateForNextSolve(state);
  ASSERT_THAT(simplex.Solve(lp, *limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
  EXPECT_EQ(simplex.GetVariableValue(ColIndex(0)), 1.0);
  EXPECT_EQ(simplex.GetVariableValue(ColIndex(1)), 0.5);

  // With polish, we always get an integer solution in this case.
  simplex.ClearStateForNextSolve();
  simplex.SetIntegralityScale(ColIndex(0), 1.0);
  simplex.SetIntegralityScale(ColIndex(1), 1.0);
  ASSERT_THAT(simplex.Solve(lp, *limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
  EXPECT_EQ(simplex.GetVariableValue(ColIndex(0)), 0.0);
  EXPECT_EQ(simplex.GetVariableValue(ColIndex(1)), 1.0);
}

TEST(RevisedSimplexTest, BasisStateWithWrongNumberOfBasicElement) {
  const RowIndex num_rows(60);
  const ColIndex num_cols(100);
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);

  absl::BitGen bit_gen;
  RandomWithFixableSeed fixable_seed(bit_gen);
  SCOPED_TRACE(fixable_seed.seed);

  CreateRandomProblem(num_cols, num_rows, linear_program.get(), fixable_seed);

  BasisState saved_state;
  int initial_num_iterations;
  {
    // Initial solve from scratch.
    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
                Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
    initial_num_iterations = simplex->GetNumberOfIterations();
    saved_state = simplex->GetState();
  }

  LOG(INFO) << "initial num iterations " << initial_num_iterations;

  std::vector<int> num_iterations_all_tests;
  constexpr int kNumTests = 50;
  for (int test = 0; test < kNumTests; ++test) {
    // We swap the status of a few random columns.
    BasisState new_state = saved_state;
    for (int i = 0; i < 5; ++i) {
      const ColIndex col =
          ColIndex(absl::Uniform<int>(fixable_seed, 0, num_cols.value()));
      VariableStatus& status = new_state.statuses[col];
      status = (status == VariableStatus::BASIC) ? VariableStatus::FREE
                                                 : VariableStatus::BASIC;
    }

    // Solve with slightly altered state.
    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    simplex->LoadStateForNextSolve(new_state);
    EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
                Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
    const int num_iterations = simplex->GetNumberOfIterations();
    num_iterations_all_tests.push_back(num_iterations);
    LOG(INFO) << "Test #" << test << " num_iterations: " << num_iterations;

    // There is no guarantee but currently the number of iterations with such
    // small changes is small. Measurements with 5000 tests shows a poisson
    // distribution with outliers going up to 60% of initial_num_iterations.
    //
    // We check after the loop that most tests have small number of iterations.
    EXPECT_LT(num_iterations, initial_num_iterations);
  }

  // Check the 80-percentile.
  absl::c_sort(num_iterations_all_tests);
  EXPECT_LT(num_iterations_all_tests[static_cast<int>(kNumTests * 80 / 100)],
            initial_num_iterations / 2);
}

TEST(RevisedSimplexTest, BasisStateWithNoElement) {
  const RowIndex num_rows(60);
  const ColIndex num_cols(100);
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);

  absl::BitGen bit_gen;
  RandomWithFixableSeed fixable_seed(bit_gen);
  SCOPED_TRACE(fixable_seed.seed);

  CreateRandomProblem(num_cols, num_rows, linear_program.get(), fixable_seed);

  BasisState state;
  state.statuses.assign(num_cols + RowToColIndex(num_rows),
                        VariableStatus::FREE);

  std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  simplex->LoadStateForNextSolve(state);
  EXPECT_THAT(simplex->Solve(*linear_program, *time_limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
