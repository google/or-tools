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
// randomly generated linear program is the same with or without the presolve
// step. The linear programs are generated in a way that tries to cover all the
// corner cases of the preprocessors.

#include <random>
#include <string>
#include <variant>
#include <vector>

#include "absl/functional/overload.h"
#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils_testing.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::_;

Fractional GenerateInterestingCoefficient(absl::BitGenRef random) {
  static std::vector<Fractional> coeffs = {-2.0, -1.0, 1.0, 2.0};
  if (absl::Bernoulli(random, 1.0 / 10)) {
    return absl::Uniform(random, -10.0, 10.0);
  }
  return coeffs[absl::Uniform<int>(random, 0, coeffs.size())];
}

Fractional GenerateInterestingCost(absl::BitGenRef random) {
  static std::vector<Fractional> costs = {0.0, 0.0, -1.0, 1.0, 2.0, -2.0};
  return costs[absl::Uniform<int>(random, 0, costs.size())];
}

void GenerateInterestingBounds(absl::BitGenRef random, Fractional* lb,
                               Fractional* ub) {
  static std::vector<Fractional> lb_values = {-kInfinity, -2.0, -1.0,
                                              0.0,        1.0,  2.0};
  static std::vector<Fractional> ub_values = {kInfinity, 0.0, 0.0, 1.0, 2.0};
  *lb = lb_values[absl::Uniform<int>(random, 0, lb_values.size())];
  *ub = ub_values[absl::Uniform<int>(random, 0, ub_values.size())];
  if (*lb != -kInfinity) {
    // In this case, ub_values is interpreted as an interval size.
    *ub += *lb;
  }
}

// Generate an initial linear program that will be extended later with new
// variables and constraints that the preprocessors should be able to remove.
void GenerateRandomBaseProblem(absl::BitGenRef random, LinearProgram* lp) {
  const RowIndex num_rows(1 + absl::Uniform(random, 0, 10));
  const ColIndex num_cols(1 + absl::Uniform(random, 0, 10));
  lp->Clear();
  for (ColIndex col(0); col < num_cols; ++col) {
    lp->CreateNewVariable();
    Fractional lb;
    Fractional ub;
    GenerateInterestingBounds(random, &lb, &ub);
    lp->SetVariableBounds(col, lb, ub);
    lp->SetObjectiveCoefficient(col, GenerateInterestingCost(random));
    for (RowIndex row(0); row < num_rows; ++row) {
      if (absl::Bernoulli(random, 1.0 / 2)) {
        lp->SetCoefficient(row, col, GenerateInterestingCoefficient(random));
      }
    }
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    Fractional lb;
    Fractional ub;
    GenerateInterestingBounds(random, &lb, &ub);
    lp->SetConstraintBounds(row, lb, ub);
  }
  lp->SetMaximizationProblem(absl::Bernoulli(random, 1.0 / 2));
  lp->CleanUp();
}

// Adds a row to the given problem which is a duplicate (with a random
// proportionality factor) of a random row.
void AddRandomDuplicateRow(absl::BitGenRef random, LinearProgram* lp) {
  const RowIndex row = lp->CreateNewConstraint();
  Fractional lb;
  Fractional ub;
  GenerateInterestingBounds(random, &lb, &ub);
  lp->SetConstraintBounds(row, lb, ub);

  const Fractional proportionality_factor = absl::Uniform(random, -10.0, 10.0);
  const RowIndex source(
      absl::Uniform<int>(random, 0, lp->num_constraints().value()));
  const ColIndex num_cols = lp->num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional coeff = lp->GetSparseColumn(col).LookUpCoefficient(source);
    if (coeff != 0.0) {
      lp->SetCoefficient(row, col, proportionality_factor * coeff);
    }
  }
}

// Adds a column to the given problem which is a duplicate (with a random
// proportionality factor) of a random column.
void AddRandomDuplicateColumn(absl::BitGenRef random, LinearProgram* lp) {
  const ColIndex col = lp->CreateNewVariable();
  Fractional lb;
  Fractional ub;
  GenerateInterestingBounds(random, &lb, &ub);
  lp->SetVariableBounds(col, lb, ub);

  const Fractional proportionality_factor = absl::Uniform(random, -10.0, 10.0);
  const ColIndex source(
      absl::Uniform<int>(random, 0, lp->num_variables().value()));
  for (const SparseColumn::Entry e : lp->GetSparseColumn(source)) {
    lp->SetCoefficient(e.row(), col, proportionality_factor * e.coefficient());
  }
}

// Calls GenerateRandomBaseProblem() and extends the problem in various random
// ways.
//
// TODO(user): Add the extension functions and call them from time to time:
// - Add singleton rows/columns.
// - Perturb the coefficients/bounds by small random values to test the
//   tolerances.
void GenerateRandomProblem(absl::BitGenRef random, LinearProgram* lp) {
  GenerateRandomBaseProblem(random, lp);
  const int num_random_extensions = absl::Uniform(random, 0, 10);
  for (int i = 0; i < num_random_extensions; ++i) {
    switch (absl::Uniform(random, 0, 10)) {
      case 0:
        AddRandomDuplicateRow(random, lp);
        break;
      case 1:
        AddRandomDuplicateColumn(random, lp);
        break;
      default:
        // Do nothing for all the other cases.
        break;
    }
  }
}

// Parameterized test to test different random lp.
class RandomPreprocessorTest : public ::testing::TestWithParam<int> {
 protected:
};

std::string StatusDebugString(ProblemStatus with, ProblemStatus without) {
  return absl::StrFormat("Problem %s with presolve and %s without.",
                         GetProblemStatusString(with),
                         GetProblemStatusString(without));
}

TEST_P(RandomPreprocessorTest, SolveWithAndWithoutPresolve) {
  std::mt19937 random(/*seed=*/GetParam());
  LinearProgram lp;
  GenerateRandomProblem(random, &lp);
  LPSolver lp_solver;
  GlopParameters params;

  params.set_use_preprocessing(true);
  lp_solver.SetParameters(params);
  const SolveStatus status_with_presolve = lp_solver.Solve(lp);
  const Fractional objective_with_presolve = lp_solver.GetObjectiveValue();

  params.set_use_preprocessing(false);
  lp_solver.SetParameters(params);
  lp_solver.Clear();
  const SolveStatus status_without_presolve = lp_solver.Solve(lp);
  const Fractional objective_without_presolve = lp_solver.GetObjectiveValue();

  // Test if the two returned statuses are compatible.
  std::visit(
      absl::Overload{
          [&](const SolveStatus::Optimal&) {
            EXPECT_THAT(status_without_presolve,
                        SolveStatusWith<SolveStatus::Optimal>(_));

            // In this case, also compare the two objective values. Note that
            // this is very unlikely to be false because the solution is checked
            // against the initial linear program (which is const) before
            // returning a status ProblemStatus::OPTIMAL.
            EXPECT_THAT(objective_without_presolve,
                        WithinSameAbsoluteOrRelativeTolerance(
                            objective_with_presolve, Fractional(1e-9)));
          },
          [&](const SolveStatus::PrimalInfeasible&) {
            EXPECT_THAT(status_without_presolve,
                        AnyOf(SolveStatusWith<SolveStatus::PrimalInfeasible>(_),
                              SolveStatusWith<SolveStatus::DualInfeasible>(_),
                              SolveStatusWith<SolveStatus::DualUnbounded>(_)));
          },
          [&](const SolveStatus::DualInfeasible&) {
            EXPECT_THAT(
                status_without_presolve,
                AnyOf(SolveStatusWith<SolveStatus::DualInfeasible>(_),
                      SolveStatusWith<SolveStatus::PrimalInfeasible>(_),
                      SolveStatusWith<SolveStatus::PrimalUnbounded>(_)));
          },
          [&](const SolveStatus::InfeasibleOrUnbounded&) {
            EXPECT_THAT(
                status_without_presolve,
                AnyOf(SolveStatusWith<SolveStatus::PrimalInfeasible>(_),
                      SolveStatusWith<SolveStatus::DualInfeasible>(_),
                      SolveStatusWith<SolveStatus::DualUnbounded>(_),
                      SolveStatusWith<SolveStatus::PrimalUnbounded>(_)));
          },
          [&](const SolveStatus::PrimalUnbounded&) {
            EXPECT_THAT(
                status_without_presolve,
                AnyOf(SolveStatusWith<SolveStatus::DualInfeasible>(_),
                      SolveStatusWith<SolveStatus::PrimalUnbounded>(_)));
          },
          [&](const SolveStatus::DualUnbounded&) {
            EXPECT_THAT(status_without_presolve,
                        AnyOf(SolveStatusWith<SolveStatus::PrimalInfeasible>(_),
                              SolveStatusWith<SolveStatus::DualUnbounded>(_)));
          },
          [&](const auto& alternative) {
            // Verify that we haven't missed a case.
            using A = decltype(alternative);
            static_assert(
                std::is_same_v<A, const SolveStatus::Init&> ||
                std::is_same_v<A, const SolveStatus::PrimalFeasible&> ||
                std::is_same_v<A, const SolveStatus::DualFeasible&> ||
                std::is_same_v<A, const SolveStatus::Imprecise&> ||
                std::is_same_v<A, const SolveStatus::Abnormal&> ||
                std::is_same_v<A, const SolveStatus::InvalidProblem&>);
            // TODO(user): Only a few problems are problematic, and I don't know
            // how to solve them. One looks like:
            //
            //       x = y
            //   and x - (1 - epsilon) y > 1
            //
            // Simplex usually say it is infeasible, but then a large x and
            // y works, and the presolve return this solution... Hopefully, we
            // convert it to IMPRECISE because it doesn't not satisfy our
            // criteria, but it is still hard to deal properly. So skipping for
            // now. The question can actually be reduced to:
            //
            //   is epsilon * x > 1 feasible
            //   given that x in [-infty, +infty]
            //
            // Not easy to define properly what to do.
            if (status_with_presolve.Is<SolveStatus::Imprecise>() &&
                !status_without_presolve.Is<SolveStatus::Optimal>()) {
              LOG(WARNING) << "Issue for problem: " << lp.Dump();
              return;
            }
            LOG(ERROR) << lp.Dump();
            ADD_FAILURE() << "Unexpected solve status! "
                          << "status_with_presolve: " << status_with_presolve
                          << " status_without_presolve: "
                          << status_without_presolve;
          },
      },
      status_with_presolve.value);
}

// Times (with 30 shards; and 10 runs), on 2014-06-10, on forge:
// - fastbuild: max = 92.5s, avg = 42.6s.
// - opt: max = 140.2s, avg = 80.0s.
INSTANTIATE_TEST_SUITE_P(All, RandomPreprocessorTest,
                         ::testing::Range(0, DEBUG_MODE ? 20000 : 200000));

}  // namespace
}  // namespace glop
}  // namespace operations_research
