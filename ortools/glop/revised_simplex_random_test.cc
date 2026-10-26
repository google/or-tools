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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/no_destructor.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/glop/preprocessor_testing.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/port/attributes.h"
#include "ortools/util/time_limit.h"

ABSL_FLAG(std::optional<std::mt19937::result_type>, revised_simplex_test_seed,
          std::nullopt, "Fixed seed to use to reproduce errors.");

namespace operations_research {
namespace glop {
namespace {

using ::testing::_;

// Duration of tests that use a loop where they generate random data and repeat
// a test on them.
constexpr absl::Duration kRepeatedTestsDuration = absl::Seconds(5);

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
    std::mt19937::result_type value ORTOOLS_REQUIRE_EXPLICIT_INIT;

    // True if the seed used has been fixed by the --revised_simplex_test_seed
    // flag.
    //
    // This value is always the same as the one returned by fixed().
    bool fixed ORTOOLS_REQUIRE_EXPLICIT_INIT;

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

void RandomTestsOfIncrementalityOnSameProblem(
    const ColIndex num_cols, const RowIndex num_rows,
    const absl::Duration test_duration, const bool use_dual_simplex) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  absl::BitGen bit_gen;
  const absl::Time deadline = absl::Now() + test_duration;
  int test = 0;
  int skipped = 0;
  for (; test == 0 || absl::Now() < deadline; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d; %s.", test + 1,
                                    absl::FormatStreamed(fixable_seed.seed)));
    // Initialize data for this test.
    const ExpectedSolution solution =
        SolveRandomProblem(num_cols, num_rows, linear_program.get(),
                           fixable_seed, use_dual_simplex);
    // Unscaled random equality LPs near the feasibility boundary can
    // legitimately return IMPRECISE when calling RevisedSimplex directly
    // without LPSolver's scaling/retry wrapper.
    if (solution.status == ProblemStatus::IMPRECISE) {
      VLOG(1) << "Skipping borderline random LP (continuous solve IMPRECISE);"
              << err;
      ++skipped;
      continue;
    }
    // Note that we've already run the preprocessor in SolveRandomProblem and
    // linear_program already has slack variables added to it. We don't need to
    // re-run it here again.

    // We can't expect solution.num_iterations to be non-zero when the problem
    // is not optimal. Typically an infeasible problem may be detected on the
    // first iteration (depending on the initial basis choice), in which case
    // num_iterations will be zero.
    if (solution.status == ProblemStatus::OPTIMAL) {
      EXPECT_GT(solution.num_iterations, 0) << err;
    }
    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    GlopParameters parameters;
    parameters.set_use_dual_simplex(use_dual_simplex);
    parameters.set_max_number_of_iterations(1);
    simplex->SetParameters(parameters);

    // Run the revised simplex 1 iteration at a time, for at least
    // solution.num_iterations + 5 calls (so post-convergence re-checks are also
    // exercised), and continue stepping if the 1-iteration warm-start path
    // still hits the 1-iteration limit due to recomputed quantities taking a
    // slightly longer pivot trajectory.
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
    const int64_t min_steps = solution.num_iterations + 5;
    const int64_t max_steps = std::max<int64_t>(
        2 * solution.num_iterations + 20, solution.num_iterations + 50);
    std::optional<SolveStatus> last_status;
    for (int64_t i = 0;
         i < max_steps &&
         (i < min_steps ||
          (last_status.has_value() && last_status->interruption_cause() ==
                                          InterruptionCause::kIterationLimit));
         ++i) {
      SolveStatus status = simplex->Solve(*linear_program, *time_limit);
      EXPECT_THAT(status, Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
          << err;
      last_status = status;
    }
    ASSERT_NE(last_status, std::nullopt);
    if (last_status->Is<SolveStatus::Imprecise>()) {
      VLOG(1) << "Skipping borderline random LP (incremental solve IMPRECISE);"
              << err;
      ++skipped;
      continue;
    }
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
  LOG(INFO) << "#tests=" << test << " (skipped=" << skipped << ")";
}

TEST(RevisedSimplexTest, PrimalIsFullyIncrementalOnRandomTinyProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(10),            // Number of variables.
      RowIndex(5),             // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex?
}

TEST(RevisedSimplexTest, DualIsFullyIncrementalOnRandomTinyProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(10),            // Number of variables.
      RowIndex(5),             // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex?
}

TEST(RevisedSimplexTest, PrimalIsFullyIncrementalOnRandomSmallProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(50),            // Number of variables.
      RowIndex(25),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex?
}

TEST(RevisedSimplexTest, DualIsFullyIncrementalOnRandomSmallProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(50),            // Number of variables.
      RowIndex(25),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex?
}

TEST(RevisedSimplexTest, PrimalIsFullyIncrementalOnRandomMediumProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(100),           // Number of variables.
      RowIndex(50),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex?
}

TEST(RevisedSimplexTest, DualIsFullyIncrementalOnRandomMediumProblems) {
  RandomTestsOfIncrementalityOnSameProblem(
      ColIndex(100),           // Number of variables.
      RowIndex(50),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex?
}

void RandomTestsOfBasisLoadOnSameProblem(const ColIndex num_cols,
                                         const RowIndex num_rows,
                                         const absl::Duration test_duration,
                                         const bool use_dual_simplex) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);

  absl::BitGen bit_gen;
  const absl::Time deadline = absl::Now() + test_duration;
  int test = 0;
  int skipped = 0;
  for (; test == 0 || absl::Now() < deadline; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d; %s.", test + 1,
                                    absl::FormatStreamed(fixable_seed.seed)));
    // Initialize data for this test.
    const ExpectedSolution solution =
        SolveRandomProblem(num_cols, num_rows, linear_program.get(),
                           fixable_seed, use_dual_simplex);
    if (solution.status == ProblemStatus::IMPRECISE) {
      VLOG(1) << "Skipping borderline random LP (continuous solve IMPRECISE);"
              << err;
      ++skipped;
      continue;
    }
    GlopParameters parameters;
    parameters.set_use_dual_simplex(use_dual_simplex);
    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    parameters.set_max_number_of_iterations(1);

    // Run the revised simplex 1 iteration at a time, for the same number of
    // total iterations which is required when running simplex continuously.
    //
    // NOTE(user): Like in RandomTestsOfIncrementalityOnSameProblem(),
    // the number of iterations can actually vary slightly, so we run at least 5
    // extra iterations and continue if the 1-iteration limit is still reached.
    // See the comment in that function for more details.
    //
    // Note: we need to call each time simplex->SetParameters() to reset the
    // random seed, otherwise the iterations might be slightly different.
    const int64_t min_steps = solution.num_iterations + 5;
    const int64_t max_steps = std::max<int64_t>(
        2 * solution.num_iterations + 20, solution.num_iterations + 50);
    std::optional<SolveStatus> last_status;
    for (int64_t i = 0;
         i < max_steps &&
         (i < min_steps ||
          (last_status.has_value() && last_status->interruption_cause() ==
                                          InterruptionCause::kIterationLimit));
         ++i) {
      // Run 1 iteration of simplex to get the expected state after 1 iteration.
      const BasisState prev_state = simplex->GetState();
      simplex->SetParameters(parameters);
      const SolveStatus step_status =
          simplex->Solve(*linear_program, *time_limit);
      EXPECT_THAT(step_status, Not(SolveStatusWith<SolveStatus::Abnormal>(_)))
          << err;
      last_status = step_status;
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

    if (simplex->GetProblemStatus() == ProblemStatus::IMPRECISE) {
      VLOG(1) << "Skipping borderline random LP (incremental solve IMPRECISE);"
              << err;
      ++skipped;
      continue;
    }

    // Validate final state.
    EXPECT_EQ(solution.status, simplex->GetProblemStatus()) << err;
    if (ProblemStatus::OPTIMAL == solution.status) {
      // TODO(user): During warm-start we recompute some quantities, thus the
      // path and the exact value of the solution may differ when using
      // incrementality. Replace with a strict equality once this is fixed.
      EXPECT_NEAR(solution.objective_value, simplex->GetObjectiveValue(), 1e-6)
          << err;
    }
  }
  LOG(INFO) << "#tests=" << test << " (skipped=" << skipped << ")";
}

TEST(RevisedSimplexTest, PrimalSupportsLoadingBasisStateOnRandomTinyProblems) {
  RandomTestsOfBasisLoadOnSameProblem(
      ColIndex(10),            // Number of variables.
      RowIndex(5),             // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex?
}

TEST(RevisedSimplexTest, DualSupportsLoadingBasisStateOnRandomTinyProblems) {
  RandomTestsOfBasisLoadOnSameProblem(
      ColIndex(10),            // Number of variables.
      RowIndex(5),             // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex?
}

TEST(RevisedSimplexTest, PrimalSupportsLoadingBasisStateOnRandomSmallProblems) {
  RandomTestsOfBasisLoadOnSameProblem(
      ColIndex(50),            // Number of variables.
      RowIndex(25),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex?
}

TEST(RevisedSimplexTest, DualSupportsLoadingBasisStateOnRandomSmallProblems) {
  RandomTestsOfBasisLoadOnSameProblem(
      ColIndex(50),            // Number of variables.
      RowIndex(25),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex?
}

// TODO(user): This test fails on the 2nd of the 2 random problems.
// The basises do not match.
TEST(RevisedSimplexTest,
     DISABLED_PrimalSupportsLoadingBasisStateOnRandomMediumProblems) {
  RandomTestsOfBasisLoadOnSameProblem(
      ColIndex(100),           // Number of variables.
      RowIndex(50),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex?
}

TEST(RevisedSimplexTest, DualSupportsLoadingBasisStateOnRandomMediumProblems) {
  RandomTestsOfBasisLoadOnSameProblem(
      ColIndex(100),           // Number of variables.
      RowIndex(50),            // Number of constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex?
}

void RandomTestsOfPrimalEqualsDual(const ColIndex num_cols,
                                   const RowIndex num_rows,
                                   const absl::Duration test_duration) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  absl::BitGen bit_gen;
  const absl::Time deadline = absl::Now() + test_duration;
  int test = 0;
  int skipped = 0;
  for (; test == 0 || absl::Now() < deadline; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d; %s.", test + 1,
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
    if (solution_primal.status == ProblemStatus::IMPRECISE ||
        status.Is<SolveStatus::Imprecise>()) {
      VLOG(1) << "Skipping borderline random LP returning IMPRECISE (primal="
              << solution_primal.status << ", dual=" << status.problem_status()
              << ");" << err;
      ++skipped;
      continue;
    }
    EXPECT_TRUE((solution_primal.status == ProblemStatus::OPTIMAL &&
                 status.problem_status() == ProblemStatus::OPTIMAL) ||
                (solution_primal.status == ProblemStatus::PRIMAL_UNBOUNDED &&
                 status.problem_status() == ProblemStatus::DUAL_INFEASIBLE) ||
                (solution_primal.status == ProblemStatus::PRIMAL_INFEASIBLE &&
                 status.problem_status() == ProblemStatus::DUAL_UNBOUNDED) ||
                (solution_primal.status == ProblemStatus::PRIMAL_INFEASIBLE &&
                 status.problem_status() == ProblemStatus::DUAL_INFEASIBLE))
        << "primal status: " << solution_primal.status
        << " dual status: " << status.problem_status() << ';' << err;
    if (ProblemStatus::OPTIMAL == solution_primal.status) {
      EXPECT_NEAR(solution_primal.objective_value,
                  simplex_dual->GetObjectiveValue(), 1e-6)
          << err;
    }
  }
  LOG(INFO) << "#tests=" << test << " (skipped=" << skipped << ")";
}

TEST(RevisedSimplexTest, PrimalAndDualGiveSameSolutionOnRandomSmallProblems) {
  RandomTestsOfPrimalEqualsDual(
      ColIndex(50),             // Number of variables.
      RowIndex(25),             // Number of constraints.
      kRepeatedTestsDuration);  // Duration of the test.
}

TEST(RevisedSimplexTest, PrimalAndDualGiveSameSolutionOnRandomMediumProblems) {
  RandomTestsOfPrimalEqualsDual(
      ColIndex(100),            // Number of variables.
      RowIndex(50),             // Number of constraints.
      kRepeatedTestsDuration);  // Duration of the test.
}

void RandomTestsOfIncrementalChanges(const ColIndex num_cols,
                                     const RowIndex num_rows,
                                     const ColIndex add_cols,
                                     const RowIndex add_rows,
                                     const absl::Duration test_duration,
                                     const bool use_dual_simplex) {
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);
  absl::BitGen bit_gen;
  const absl::Time deadline = absl::Now() + test_duration;
  int test = 0;
  int skipped = 0;
  for (; test == 0 || absl::Now() < deadline; ++test) {
    RandomWithFixableSeed fixable_seed(bit_gen);
    if (fixable_seed.seed.fixed && test >= 1) break;

    std::string err(absl::StrFormat(" test no %d; %s.", test + 1,
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

    if (status.Is<SolveStatus::Imprecise>() ||
        redo_status.Is<SolveStatus::Imprecise>()) {
      VLOG(1) << "Skipping borderline random LP returning IMPRECISE "
                 "(incremental="
              << status << ", redo=" << redo_status << ");" << err;
      ++skipped;
      continue;
    }

    EXPECT_EQ(status, redo_status) << err;

    if (ProblemStatus::OPTIMAL == simplex->GetProblemStatus()) {
      EXPECT_NEAR(simplex->GetObjectiveValue(),
                  simplex_redo->GetObjectiveValue(), 1e-6)
          << err;
    }
  }
  LOG(INFO) << "#tests=" << test << " (skipped=" << skipped << ")";
}

TEST(RevisedSimplexTest, IncrementalRowAdditionsOnRandomTinyProblems) {
  RandomTestsOfIncrementalChanges(
      ColIndex(10),            // Number of variables.
      RowIndex(5),             // Number of constraints.
      ColIndex(0),             // Number of new variables.
      RowIndex(2),             // Number of new constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalRowAdditionsOnRandomSmallProblems) {
  RandomTestsOfIncrementalChanges(
      ColIndex(50),            // Number of variables.
      RowIndex(25),            // Number of constraints.
      ColIndex(0),             // Number of new variables.
      RowIndex(5),             // Number of new constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalRowAdditionsOnRandomMediumProblems) {
  RandomTestsOfIncrementalChanges(
      ColIndex(100),           // Number of variables.
      RowIndex(50),            // Number of constraints.
      ColIndex(0),             // Number of new variables.
      RowIndex(5),             // Number of new constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      true);                   // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalColAdditionsOnRandomTinyProblems) {
  RandomTestsOfIncrementalChanges(
      ColIndex(10),            // Number of variables.
      RowIndex(5),             // Number of constraints.
      ColIndex(2),             // Number of new variables.
      RowIndex(0),             // Number of new constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalColAdditionsOnRandomSmallProblems) {
  RandomTestsOfIncrementalChanges(
      ColIndex(50),            // Number of variables.
      RowIndex(25),            // Number of constraints.
      ColIndex(5),             // Number of new variables.
      RowIndex(0),             // Number of new constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex
}

TEST(RevisedSimplexTest, IncrementalColAdditionsOnRandomMediumProblems) {
  RandomTestsOfIncrementalChanges(
      ColIndex(100),           // Number of variables.
      RowIndex(50),            // Number of constraints.
      ColIndex(5),             // Number of new variables.
      RowIndex(0),             // Number of new constraints.
      kRepeatedTestsDuration,  // Duration of the test.
      false);                  // Use dual simplex
}

TEST(RevisedSimplexTest, BasisStateWithWrongNumberOfBasicElement) {
  const RowIndex num_rows(60);
  const ColIndex num_cols(100);
  std::unique_ptr<LinearProgram> linear_program(new LinearProgram);

  absl::BitGen bit_gen;
  RandomWithFixableSeed fixable_seed(bit_gen);
  SCOPED_TRACE(fixable_seed.seed);

  BasisState saved_state;
  int initial_num_iterations = 0;
  for (int attempt = 0; attempt < 100; ++attempt) {
    CreateRandomProblem(num_cols, num_rows, linear_program.get(), fixable_seed);

    // Initial solve from scratch.
    std::unique_ptr<RevisedSimplex> simplex(new RevisedSimplex);
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    const SolveStatus status = simplex->Solve(*linear_program, *time_limit);
    EXPECT_THAT(status, Not(SolveStatusWith<SolveStatus::Abnormal>(_)));
    initial_num_iterations = simplex->GetNumberOfIterations();
    saved_state = simplex->GetState();
    if (status.Is<SolveStatus::Optimal>() && initial_num_iterations >= 20) {
      break;
    }
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
