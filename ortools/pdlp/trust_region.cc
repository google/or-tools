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

#include "ortools/pdlp/trust_region.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ortools/base/mathutil.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_optimization_utils.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"

namespace operations_research::pdlp {

using ::Eigen::VectorXd;

namespace {

// The functions in this file that are templated on a `TrustRegionProblem` use
// the templated class to specify the following trust-region problem with bound
// constraints:
// min_x Objective^T * (x - CenterPoint)
// s.t. LowerBound <= x <= UpperBound
//      || x - Centerpoint ||_W <= target_radius
// where ||y||_W = sqrt(sum_i NormWeight[i] * y[i]^2)
// The templated `TrustRegionProblem` type should provide methods:
//   `double Objective(int64_t index) const;`
//   `double LowerBound(int64_t index) const;`
//   `double UpperBound(int64_t index) const;`
//   `double CenterPoint(int64_t index) const;`
//   `double NormWeight(int64_t index) const;`
// which give the values of the corresponding terms in the problem
// specification. See `VectorTrustRegionProblem` for an example. The
// *TrustRegionProblem classes below implement several instances of
// `TrustRegionProblem`.
// On the other hand, the functions that are templated on a
// `DiagonalTrustRegionProblem` use the templated class to specify the following
// trust-region problem with bound constraints:
// min_x (1 / 2) * (x - CenterPoint)^T * ObjectiveMatrix * (x - CenterPoint)
//       + Objective^T * (x - CenterPoint)
// s.t.  LowerBound <= x <= UpperBound
//       || x - CenterPoint ||_W <= target_radius,
// where ||y||_W = sqrt(sum_i NormWeight[i] * y[i]^2) and ObjectiveMatrix is
// assumed to be a diagonal matrix with nonnegative entries. Templated
// `DiagonalTrustRegionProblem` types should provide all the methods provided by
// templated `TrustRegionProblem` types, as well as:
//   `double ObjectiveMatrixDiagonalAt(int64_t index) const;`
// which gives the value of the objective matrix diagonal at a specified index.
// See `DiagonalTrustRegionProblemFromQp` for an example that sets up the
// diagonal trust region problem from an existing `ShardedQuadraticProgram`.

// `VectorTrustRegionProblem` uses explicit vectors to define the trust region
// problem. It captures const references to the vectors used in the constructor,
// which should outlive the class instance.
class VectorTrustRegionProblem {
 public:
  VectorTrustRegionProblem(const VectorXd* objective,
                           const VectorXd* lower_bound,
                           const VectorXd* upper_bound,
                           const VectorXd* center_point,
                           const VectorXd* norm_weight)
      : objective_(*objective),
        lower_bound_(*lower_bound),
        upper_bound_(*upper_bound),
        center_point_(*center_point),
        norm_weight_(*norm_weight) {}
  double Objective(int64_t index) const { return objective_(index); }
  double LowerBound(int64_t index) const { return lower_bound_(index); }
  double UpperBound(int64_t index) const { return upper_bound_(index); }
  double CenterPoint(int64_t index) const { return center_point_(index); }
  double NormWeight(int64_t index) const { return norm_weight_(index); }

 private:
  const VectorXd& objective_;
  const VectorXd& lower_bound_;
  const VectorXd& upper_bound_;
  const VectorXd& center_point_;
  const VectorXd& norm_weight_;
};

// `JointTrustRegionProblem` defines the joint primal/dual trust region problem
// given a `QuadraticProgram`, primal and dual solutions, primal and dual
// gradients, and the primal weight. The joint problem (implicitly) concatenates
// the primal and dual vectors. The class captures const references to the
// constructor arguments (except `primal_weight`), which should outlive the
// class instance.
// The corresponding trust region problem is
// min `primal_gradient`^T * (x - `primal_solution`)
//     - `dual_gradient`^T * (y - `dual_solution`)
// s.t. `qp.variable_lower_bounds` <= x <= `qp.variable_upper_bounds`
//      `qp`.implicit_dual_lower_bounds <= y <= `qp`.implicit_dual_upper_bounds
//      || (x, y) - (`primal_solution`, `dual_solution`) ||_2 <= `target_radius`
// where the implicit dual bounds are those given in
// https://developers.google.com/optimization/lp/pdlp_math#dual_variable_bounds
class JointTrustRegionProblem {
 public:
  JointTrustRegionProblem(const QuadraticProgram* qp,
                          const VectorXd* primal_solution,
                          const VectorXd* dual_solution,
                          const VectorXd* primal_gradient,
                          const VectorXd* dual_gradient,
                          const double primal_weight)
      : qp_(*qp),
        primal_size_(qp_.variable_lower_bounds.size()),
        primal_solution_(*primal_solution),
        dual_solution_(*dual_solution),
        primal_gradient_(*primal_gradient),
        dual_gradient_(*dual_gradient),
        primal_weight_(primal_weight) {}
  double Objective(int64_t index) const {
    return index < primal_size_ ? primal_gradient_[index]
                                : -dual_gradient_[index - primal_size_];
  }
  double LowerBound(int64_t index) const {
    return index < primal_size_ ? qp_.variable_lower_bounds[index]
           : std::isfinite(qp_.constraint_upper_bounds[index - primal_size_])
               ? -std::numeric_limits<double>::infinity()
               : 0.0;
  }
  double UpperBound(int64_t index) const {
    return index < primal_size_ ? qp_.variable_upper_bounds[index]
           : std::isfinite(qp_.constraint_lower_bounds[index - primal_size_])
               ? std::numeric_limits<double>::infinity()
               : 0.0;
  }
  double CenterPoint(int64_t index) const {
    return index < primal_size_ ? primal_solution_[index]
                                : dual_solution_[index - primal_size_];
  }
  double NormWeight(int64_t index) const {
    return index < primal_size_ ? 0.5 * primal_weight_ : 0.5 / primal_weight_;
  }

 private:
  const QuadraticProgram& qp_;
  const int64_t primal_size_;
  const VectorXd& primal_solution_;
  const VectorXd& dual_solution_;
  const VectorXd& primal_gradient_;
  const VectorXd& dual_gradient_;
  const double primal_weight_;
};

struct TrustRegionResultStepSize {
  // The step_size of the solution.
  double solution_step_size;
  // The value objective_vector^T * (solution - center_point).
  double objective_value;
};

// `problem` is sharded according to `sharder`. Within each shard, this function
// selects the subset of elements corresponding to
// `indexed_components_by_shard`, and takes the median of the critical step
// sizes of these elements, producing an array A of shard medians. Then returns
// the median of the array A. CHECK-fails if `indexed_components_by_shard` is
// empty for all shards.
template <typename TrustRegionProblem>
double MedianOfShardMedians(
    const TrustRegionProblem& problem,
    const std::vector<std::vector<int64_t>>& indexed_components_by_shard,
    const Sharder& sharder) {
  std::vector<std::optional<double>> shard_medians(sharder.NumShards(),
                                                   std::nullopt);
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const auto& indexed_shard_components =
        indexed_components_by_shard[shard.Index()];
    if (!indexed_shard_components.empty()) {
      shard_medians[shard.Index()] = internal::EasyMedian(
          indexed_shard_components, [&](const int64_t index) {
            return internal::CriticalStepSize(problem, index);
          });
    }
  });
  std::vector<double> non_empty_medians;
  for (const auto& median : shard_medians) {
    if (median.has_value()) {
      non_empty_medians.push_back(*median);
    }
  }
  CHECK(!non_empty_medians.empty());
  return internal::EasyMedian(non_empty_medians,
                              [](const double x) { return x; });
}

struct InitialState {
  std::vector<std::vector<int64_t>> undecided_components_by_shard;
  double radius_coefficient_of_decided_components;
};

template <typename TrustRegionProblem>
InitialState ComputeInitialState(const TrustRegionProblem& problem,
                                 const Sharder& sharder) {
  InitialState result;
  result.undecided_components_by_shard.resize(sharder.NumShards());
  result.radius_coefficient_of_decided_components =
      sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
        const int64_t shard_start = sharder.ShardStart(shard.Index());
        const int64_t shard_size = sharder.ShardSize(shard.Index());
        return internal::ComputeInitialUndecidedComponents(
            problem, shard_start, shard_start + shard_size,
            result.undecided_components_by_shard[shard.Index()]);
      });
  return result;
}

template <typename TrustRegionProblem>
double RadiusSquaredOfUndecidedComponents(
    const TrustRegionProblem& problem, const double step_size,
    const Sharder& sharder,
    const std::vector<std::vector<int64_t>>& undecided_components_by_shard) {
  return sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
    return internal::RadiusSquaredOfUndecidedComponents(
        problem, step_size, undecided_components_by_shard[shard.Index()]);
  });
}

template <typename TrustRegionProblem>
double RemoveCriticalStepsAboveThreshold(
    const TrustRegionProblem& problem, const double step_size_threshold,
    const Sharder& sharder,
    std::vector<std::vector<int64_t>>& undecided_components_by_shard) {
  return sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
    return internal::RemoveCriticalStepsAboveThreshold(
        problem, step_size_threshold,
        undecided_components_by_shard[shard.Index()]);
  });
}

template <typename TrustRegionProblem>
double RemoveCriticalStepsBelowThreshold(
    const TrustRegionProblem& problem, const double step_size_threshold,
    const Sharder& sharder,
    std::vector<std::vector<int64_t>>& undecided_components_by_shard) {
  return sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
    return internal::RemoveCriticalStepsBelowThreshold(
        problem, step_size_threshold,
        undecided_components_by_shard[shard.Index()]);
  });
}

int64_t NumUndecidedComponents(
    const std::vector<std::vector<int64_t>>& undecided_components_by_shard) {
  int64_t num_undecided_components = 0;
  for (const auto& undecided_components : undecided_components_by_shard) {
    num_undecided_components += undecided_components.size();
  }
  return num_undecided_components;
}

int64_t MaxUndecidedComponentsInAnyShard(
    const std::vector<std::vector<int64_t>>& undecided_components_by_shard) {
  int64_t max = 0;
  for (const auto& undecided_components : undecided_components_by_shard) {
    max = std::max<int64_t>(max, undecided_components.size());
  }
  return max;
}

template <typename TrustRegionProblem>
VectorXd ComputeSolution(const TrustRegionProblem& problem,
                         const double step_size, const Sharder& sharder) {
  VectorXd solution(sharder.NumElements());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const int64_t shard_start = sharder.ShardStart(shard.Index());
    const int64_t shard_size = sharder.ShardSize(shard.Index());
    for (int64_t index = shard_start; index < shard_start + shard_size;
         ++index) {
      solution[index] = internal::ProjectedValue(problem, index, step_size);
    }
  });
  return solution;
}

template <typename TrustRegionProblem>
double ComputeObjectiveValue(const TrustRegionProblem& problem,
                             const double step_size, const Sharder& sharder) {
  return sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
    const int64_t shard_start = sharder.ShardStart(shard.Index());
    const int64_t shard_size = sharder.ShardSize(shard.Index());
    double shard_value = 0.0;
    for (int64_t index = shard_start; index < shard_start + shard_size;
         ++index) {
      shard_value += problem.Objective(index) *
                     (internal::ProjectedValue(problem, index, step_size) -
                      problem.CenterPoint(index));
    }
    return shard_value;
  });
}

// Solves the following trust-region problem with bound constraints:
// min_x Objective^T * (x - CenterPoint)
// s.t. LowerBound <= x <= UpperBound
//      || x - Centerpoint ||_W <= target_radius
// where ||y||_W = sqrt(sum_i NormWeight[i] * y[i]^2)
// given by a `TrustRegionProblem` (see description at the top of this file),
// using an exact linear-time method. The size of `sharder` is used to determine
// the size of the problem. Assumes that there is always a feasible solution,
// that is, that `problem.LowerBound(i)` <= `problem.CenterPoint(i)` <=
// `problem.UpperBound(i)`, and that `problem.NormWeight(i) > 0`, for
// 0 <= i < `sharder.NumElements()`.
//
// The linear-time method is based on the observation that the optimal x will be
// of the form x(delta) =
// proj(center_point - delta * objective_vector / norm_weights, bounds)
// for some delta such that || x(delta) - center_point ||_W = target_radius
// (except for corner cases where the radius constraint is inactive) and the
// vector division is element-wise. Therefore we find the critical threshold for
// each coordinate, and repeatedly: (1) take the median delta, (2) check the
// corresponding radius, and (3) eliminate half of the data points from
// consideration.
template <typename TrustRegionProblem>
TrustRegionResultStepSize SolveTrustRegionStepSize(
    const TrustRegionProblem& problem, const double target_radius,
    const Sharder& sharder) {
  CHECK_GE(target_radius, 0.0);

  const bool norm_weights_are_positive =
      sharder.ParallelTrueForAllShards([&](const Sharder::Shard& shard) {
        const int64_t shard_start = sharder.ShardStart(shard.Index());
        const int64_t shard_size = sharder.ShardSize(shard.Index());
        for (int64_t index = shard_start; index < shard_start + shard_size;
             ++index) {
          if (problem.NormWeight(index) <= 0.0) return false;
        }
        return true;
      });
  CHECK(norm_weights_are_positive);

  if (target_radius == 0.0) {
    return {.solution_step_size = 0.0, .objective_value = 0.0};
  }

  const bool objective_is_all_zeros =
      sharder.ParallelTrueForAllShards([&](const Sharder::Shard& shard) {
        const int64_t shard_start = sharder.ShardStart(shard.Index());
        const int64_t shard_size = sharder.ShardSize(shard.Index());
        for (int64_t index = shard_start; index < shard_start + shard_size;
             ++index) {
          if (problem.Objective(index) != 0.0) return false;
        }
        return true;
      });
  if (objective_is_all_zeros) {
    return {.solution_step_size = 0.0, .objective_value = 0.0};
  }

  InitialState initial_state = ComputeInitialState(problem, sharder);

  // The contribution to the weighted radius squared from the variables that we
  // know are at their bounds in the solution.
  double fixed_radius_squared = 0.0;

  // This value times step_size^2 gives the contribution to the weighted radius
  // squared from the variables determined in the solution by the formula
  // center_point - step_size * objective / norm_weights. These variables are
  // not at their bounds in the solution, except in degenerate cases.
  double variable_radius_coefficient =
      initial_state.radius_coefficient_of_decided_components;

  // For each shard, the components of the variables that aren't accounted for
  // in `fixed_radius_squared` or `variable_radius_coefficient`, i.e., we don't
  // know if they're at their bounds in the solution.
  std::vector<std::vector<int64_t>> undecided_components_by_shard(
      std::move(initial_state.undecided_components_by_shard));

  // These are counters for the number of variables we inspect overall during
  // the solve, including in the initialization. The "worst case" accounts for
  // imbalance across the shards by charging each round for the maximum number
  // of elements in a shard, because shards with fewer elements may correspond
  // to idle threads.
  int64_t actual_elements_seen = sharder.NumElements();
  int64_t worst_case_elements_seen = sharder.NumElements();

  while (NumUndecidedComponents(undecided_components_by_shard) > 0) {
    worst_case_elements_seen +=
        MaxUndecidedComponentsInAnyShard(undecided_components_by_shard) *
        sharder.NumShards();
    actual_elements_seen +=
        NumUndecidedComponents(undecided_components_by_shard);

    const double step_size_threshold =
        MedianOfShardMedians(problem, undecided_components_by_shard, sharder);
    const double radius_squared_of_undecided_components =
        RadiusSquaredOfUndecidedComponents(
            problem, /*step_size=*/step_size_threshold, sharder,
            undecided_components_by_shard);

    const double radius_squared_at_threshold =
        radius_squared_of_undecided_components + fixed_radius_squared +
        variable_radius_coefficient * MathUtil::Square(step_size_threshold);

    if (radius_squared_at_threshold > MathUtil::Square(target_radius)) {
      variable_radius_coefficient += RemoveCriticalStepsAboveThreshold(
          problem, step_size_threshold, sharder, undecided_components_by_shard);
    } else {
      fixed_radius_squared += RemoveCriticalStepsBelowThreshold(
          problem, step_size_threshold, sharder, undecided_components_by_shard);
    }
  }
  VLOG(1) << "Total passes through variables: "
          << actual_elements_seen / static_cast<double>(sharder.NumElements());
  VLOG(1) << "Theoretical slowdown because of shard imbalance: "
          << static_cast<double>(worst_case_elements_seen) /
                     actual_elements_seen -
                 1.0;

  // Now that we know exactly which variables are fixed at their bounds,
  // compute the step size that will give us the exact target radius.
  // This is the solution to: `fixed_radius_squared` +
  // `variable_radius_coefficient` * `step_size`^2 == `target_radius`^2.
  double step_size = 0.0;
  if (variable_radius_coefficient > 0.0) {
    step_size =
        std::sqrt((MathUtil::Square(target_radius) - fixed_radius_squared) /
                  variable_radius_coefficient);
  } else {
    // All variables are fixed at their bounds. So we can take a very large
    // finite step. We don't use infinity as the step in order to avoid 0 *
    // infinity = NaN when zeros are present in the objective vector. It's ok if
    // the result of step_size * objective_vector has infinity components
    // because these are projected correctly to bounds.
    step_size = std::numeric_limits<double>::max();
  }

  return {
      .solution_step_size = step_size,
      .objective_value = ComputeObjectiveValue(problem, step_size, sharder)};
}

}  // namespace

TrustRegionResult SolveTrustRegion(const VectorXd& objective_vector,
                                   const VectorXd& variable_lower_bounds,
                                   const VectorXd& variable_upper_bounds,
                                   const VectorXd& center_point,
                                   const VectorXd& norm_weights,
                                   const double target_radius,
                                   const Sharder& sharder) {
  VectorTrustRegionProblem problem(&objective_vector, &variable_lower_bounds,
                                   &variable_upper_bounds, &center_point,
                                   &norm_weights);
  TrustRegionResultStepSize solution =
      SolveTrustRegionStepSize(problem, target_radius, sharder);
  return TrustRegionResult{
      .solution_step_size = solution.solution_step_size,
      .objective_value = solution.objective_value,
      .solution =
          ComputeSolution(problem, solution.solution_step_size, sharder),
  };
}

// A generic trust region problem of the form:
//   min_{x} (1 / 2) * (x - `center_point`)^T Q (x - `center_point`)
//           + c^T (x - `center_point`)
//   s.t.    `lower_bounds` <= (x - `center_point`) <= `upper_bounds`
//           ||x - `center_point`||_W <= radius
// where ||z||_W = sqrt(sum_i w_i z_i^2) is a weighted Euclidean norm.
// It is assumed that the objective matrix Q is a nonnegative diagonal matrix.
class DiagonalTrustRegionProblem {
 public:
  // A reference to the objects passed in the constructor is kept, so they must
  // outlive the `DiagonalTrustRegionProblem` instance.
  DiagonalTrustRegionProblem(const VectorXd* objective_vector,
                             const VectorXd* objective_matrix_diagonal,
                             const VectorXd* lower_bounds,
                             const VectorXd* upper_bounds,
                             const VectorXd* center_point,
                             const VectorXd* norm_weights)
      : objective_vector_(*objective_vector),
        objective_matrix_diagonal_(*objective_matrix_diagonal),
        variable_lower_bounds_(*lower_bounds),
        variable_upper_bounds_(*upper_bounds),
        center_point_(*center_point),
        norm_weight_(*norm_weights) {}

  double CenterPoint(int64_t index) const { return center_point_[index]; }

  double NormWeight(int64_t index) const { return norm_weight_[index]; }

  double LowerBound(int64_t index) const {
    return variable_lower_bounds_[index];
  }

  double UpperBound(int64_t index) const {
    return variable_upper_bounds_[index];
  }

  double Objective(int64_t index) const { return objective_vector_[index]; }

  double ObjectiveMatrixDiagonalAt(int64_t index) const {
    return objective_matrix_diagonal_[index];
  }

 private:
  const VectorXd& objective_vector_;
  const VectorXd& objective_matrix_diagonal_;
  const VectorXd& variable_lower_bounds_;
  const VectorXd& variable_upper_bounds_;
  const VectorXd& center_point_;
  const VectorXd& norm_weight_;
};

// `DiagonalTrustRegionProblemFromQp` accepts a diagonal quadratic program and
// information about the current solution and gradient and sets up the following
// trust-region subproblem:
// min_{x, y} (x - `primal_solution`)^T Q (x - `primal_solution`)
//            + `primal_gradient`^T (x - `primal_solution`)
//            - `dual_gradient`^T (y - `dual_solution`)
// s.t.       l <= x - `primal_solution` <= u
//            l_implicit <= y - `dual_solution` <= u_implicit
//            ||(x, y) - (`primal_solution`, `dual_solution`)||_W <= r,
// where
//   ||(x, y)||_W = sqrt(0.5 * `primal_weight` ||x||^2 +
//                       (0.5 / `primal_weight`) ||y||^2).
// This class implements the same methods as `DiagonalTrustRegionProblem`, but
// without the need to explicitly copy vectors.
class DiagonalTrustRegionProblemFromQp {
 public:
  // A reference to the objects passed in the constructor is kept, so they must
  // outlive the `DiagonalTrustRegionProblemFromQp` instance.
  DiagonalTrustRegionProblemFromQp(const QuadraticProgram* qp,
                                   const VectorXd* primal_solution,
                                   const VectorXd* dual_solution,
                                   const VectorXd* primal_gradient,
                                   const VectorXd* dual_gradient,
                                   const double primal_weight)
      : qp_(*qp),
        primal_solution_(*primal_solution),
        dual_solution_(*dual_solution),
        primal_gradient_(*primal_gradient),
        dual_gradient_(*dual_gradient),
        primal_size_(primal_solution->size()),
        primal_weight_(primal_weight) {}

  double CenterPoint(int64_t index) const {
    return (index < primal_size_) ? primal_solution_[index]
                                  : dual_solution_[index - primal_size_];
  }

  double NormWeight(int64_t index) const {
    return (index < primal_size_) ? 0.5 * primal_weight_ : 0.5 / primal_weight_;
  }

  double LowerBound(int64_t index) const {
    if (index < primal_size_) {
      return qp_.variable_lower_bounds[index];
    } else {
      return std::isfinite(qp_.constraint_upper_bounds[index - primal_size_])
                 ? -std::numeric_limits<double>::infinity()
                 : 0.0;
    }
  }

  double UpperBound(int64_t index) const {
    if (index < primal_size_) {
      return qp_.variable_upper_bounds[index];
    } else {
      return std::isfinite(qp_.constraint_lower_bounds[index - primal_size_])
                 ? std::numeric_limits<double>::infinity()
                 : 0.0;
    }
  }

  double Objective(int64_t index) const {
    return (index < primal_size_) ? primal_gradient_[index]
                                  : -dual_gradient_[index - primal_size_];
  }

  double ObjectiveMatrixDiagonalAt(int64_t index) const {
    if (qp_.objective_matrix.has_value()) {
      return (index < primal_size_) ? qp_.objective_matrix->diagonal()[index]
                                    : 0.0;
    } else {
      return 0.0;
    }
  }

 private:
  const QuadraticProgram& qp_;
  const VectorXd& primal_solution_;
  const VectorXd& dual_solution_;
  const VectorXd& primal_gradient_;
  const VectorXd& dual_gradient_;
  const int64_t primal_size_;
  const double primal_weight_;
};

// Computes a single coordinate projection of the scaled difference,
// sqrt(NormWeight(i)) * (x[i] - CenterPoint(i)), to the corresponding box
// constraints. As a function of scaling_factor, the difference is equal to
//   (Q[i, i] / NormWeight(i)) + `scaling_factor`)^{-1} *
//   (-c[i] / sqrt(NormWeight(i))),
// where Q, c are the objective matrix and vector, respectively.
template <typename DiagonalTrustRegionProblem>
double ProjectedValueOfScaledDifference(
    const DiagonalTrustRegionProblem& problem, const int64_t index,
    const double scaling_factor) {
  const double weight = problem.NormWeight(index);
  return std::min(
      std::max((-problem.Objective(index) / std::sqrt(weight)) /
                   (problem.ObjectiveMatrixDiagonalAt(index) / weight +
                    scaling_factor),
               std::sqrt(weight) *
                   (problem.LowerBound(index) - problem.CenterPoint(index))),
      std::sqrt(weight) *
          (problem.UpperBound(index) - problem.CenterPoint(index)));
}

// Computes the norm of the projection of the difference vector,
// x - center_point, to the corresponding box constraints. We are using the
// standard Euclidean norm (instead of the weighted norm) because the solver
// implicitly reformulates the problem to one with a Euclidean ball constraint
// first.
template <typename DiagonalTrustRegionProblem>
double NormOfDeltaProjection(const DiagonalTrustRegionProblem& problem,
                             const Sharder& sharder,
                             const double scaling_factor) {
  const double squared_norm =
      sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
        const int64_t shard_start = sharder.ShardStart(shard.Index());
        const int64_t shard_end =
            shard_start + sharder.ShardSize(shard.Index());
        double sum = 0.0;
        for (int64_t i = shard_start; i < shard_end; ++i) {
          const double projected_coordinate =
              ProjectedValueOfScaledDifference(problem, i, scaling_factor);
          sum += MathUtil::Square(projected_coordinate);
        }
        return sum;
      });
  return std::sqrt(squared_norm);
}

// Finds an approximately optimal scaling factor for the solution of the trust
// region subproblem, which can be passed on to `ProjectedCoordinate()` to find
// an approximately optimal solution to the trust region subproblem. The value
// returned is guaranteed to be within `solve_tol` * max(1, s*) of the optimal
// scaling s*.
// TODO(user): figure out what accuracy is useful to callers and redo the
// stopping criterion accordingly.
template <typename DiagonalTrustRegionProblem>
double FindScalingFactor(const DiagonalTrustRegionProblem& problem,
                         const Sharder& sharder, const double target_radius,
                         const double solve_tol) {
  // Determine a search interval using monotonicity of the squared norm of the
  // candidate solution with respect to the scaling factor.
  double scaling_factor_lower_bound = 0.0;
  double scaling_factor_upper_bound = 1.0;
  while (NormOfDeltaProjection(problem, sharder, scaling_factor_upper_bound) >=
         target_radius) {
    scaling_factor_lower_bound = scaling_factor_upper_bound;
    scaling_factor_upper_bound *= 2;
  }
  // Invariant: `scaling_factor_upper_bound >= scaling_factor_lower_bound`.
  while ((scaling_factor_upper_bound - scaling_factor_lower_bound) >=
         solve_tol * std::max(1.0, scaling_factor_lower_bound)) {
    const double middle =
        (scaling_factor_lower_bound + scaling_factor_upper_bound) / 2.0;
    // Norm is monotonically non-increasing as a function of scaling_factor.
    if (NormOfDeltaProjection(problem, sharder, middle) <= target_radius) {
      scaling_factor_upper_bound = middle;
    } else {
      scaling_factor_lower_bound = middle;
    }
  }
  return (scaling_factor_upper_bound + scaling_factor_lower_bound) / 2.0;
}

// Solves the diagonal trust region problem using a binary search algorithm.
// See comment above `SolveDiagonalTrustRegion()` in trust_region.h for the
// meaning of `solve_tol`.
template <typename DiagonalTrustRegionProblem>
TrustRegionResult SolveDiagonalTrustRegionProblem(
    const DiagonalTrustRegionProblem& problem, const Sharder& sharder,
    const double target_radius, const double solve_tol) {
  CHECK_GE(target_radius, 0.0);
  const bool norm_weights_are_positive =
      sharder.ParallelTrueForAllShards([&](const Sharder::Shard& shard) {
        const int64_t shard_start = sharder.ShardStart(shard.Index());
        for (int64_t i = shard_start;
             i < shard_start + sharder.ShardSize(shard.Index()); ++i) {
          if (problem.NormWeight(i) <= 0) {
            return false;
          }
        }
        return true;
      });
  CHECK(norm_weights_are_positive);
  if (target_radius == 0.0) {
    VectorXd solution(sharder.NumElements());
    sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
      const int64_t shard_start = sharder.ShardStart(shard.Index());
      const int64_t shard_size = sharder.ShardSize(shard.Index());
      for (int64_t i = shard_start; i < shard_start + shard_size; ++i) {
        solution[i] = problem.CenterPoint(i);
      }
    });
    return {.solution_step_size = 0.0,
            .objective_value = 0.0,
            .solution = std::move(solution)};
  }
  const double optimal_scaling =
      FindScalingFactor(problem, sharder, target_radius, solve_tol);
  VectorXd solution(sharder.NumElements());
  sharder.ParallelForEachShard([&](const Sharder::Shard& shard) {
    const int64_t shard_start = sharder.ShardStart(shard.Index());
    const int64_t shard_size = sharder.ShardSize(shard.Index());
    for (int64_t i = shard_start; i < shard_start + shard_size; ++i) {
      const double weight = problem.NormWeight(i);
      const double projected_value =
          ProjectedValueOfScaledDifference(problem, i, optimal_scaling);
      solution[i] =
          problem.CenterPoint(i) + std::sqrt(1 / weight) * projected_value;
    }
  });
  const double final_objective_value =
      sharder.ParallelSumOverShards([&](const Sharder::Shard& shard) {
        double local_sum = 0.0;
        const int64_t shard_start = sharder.ShardStart(shard.Index());
        for (int64_t i = shard_start;
             i < shard_start + sharder.ShardSize(shard.Index()); ++i) {
          const double diff = solution[i] - problem.CenterPoint(i);
          local_sum +=
              0.5 * diff * problem.ObjectiveMatrixDiagonalAt(i) * diff +
              diff * problem.Objective(i);
        }
        return local_sum;
      });
  return {.solution_step_size = optimal_scaling,
          .objective_value = final_objective_value,
          .solution = solution};
}

TrustRegionResult SolveDiagonalTrustRegion(
    const VectorXd& objective_vector, const VectorXd& objective_matrix_diagonal,
    const VectorXd& variable_lower_bounds,
    const VectorXd& variable_upper_bounds, const VectorXd& center_point,
    const VectorXd& norm_weights, const double target_radius,
    const Sharder& sharder, const double solve_tolerance) {
  DiagonalTrustRegionProblem problem(
      &objective_vector, &objective_matrix_diagonal, &variable_lower_bounds,
      &variable_upper_bounds, &center_point, &norm_weights);
  return SolveDiagonalTrustRegionProblem(problem, sharder, target_radius,
                                         solve_tolerance);
}

TrustRegionResult SolveDiagonalQpTrustRegion(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& primal_solution,
    const VectorXd& dual_solution, const VectorXd& primal_gradient,
    const VectorXd& dual_gradient, const double primal_weight,
    double target_radius, const double solve_tolerance) {
  const int64_t problem_size = sharded_qp.PrimalSize() + sharded_qp.DualSize();
  DiagonalTrustRegionProblemFromQp problem(&sharded_qp.Qp(), &primal_solution,
                                           &dual_solution, &primal_gradient,
                                           &dual_gradient, primal_weight);

  const Sharder joint_sharder(sharded_qp.PrimalSharder(), problem_size);
  const bool norm_weights_are_positive =
      joint_sharder.ParallelTrueForAllShards([&](const Sharder::Shard& shard) {
        const int64_t shard_start = joint_sharder.ShardStart(shard.Index());
        for (int64_t i = shard_start;
             i < shard_start + joint_sharder.ShardSize(shard.Index()); ++i) {
          if (problem.NormWeight(i) <= 0) {
            return false;
          }
        }
        return true;
      });
  CHECK(norm_weights_are_positive);
  return SolveDiagonalTrustRegionProblem(problem, joint_sharder, target_radius,
                                         solve_tolerance);
}

namespace {

struct MaxNormBoundResult {
  // `LagrangianPart::value` from `ComputePrimalGradient` and
  // `ComputeDualGradient`, respectively.
  double part_of_lagrangian_value = 0.0;
  // For the primal, the value
  // ∇_x L(primal_solution, dual_solution)^T (x^* - primal_solution) where
  // x^* is the solution of the primal trust region subproblem.
  // For the dual, the value
  // ∇_y L(primal_solution, dual_solution)^T (y^* - dual_solution) where
  // y^* is the solution of the dual trust region subproblem.
  // This will be a non-positive value for the primal and a non-negative
  // value for the dual.
  double trust_region_objective_delta = 0.0;
};

MaxNormBoundResult ComputeMaxNormPrimalTrustRegionBound(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& primal_solution,
    const double primal_radius, const VectorXd& dual_product) {
  LagrangianPart primal_part =
      ComputePrimalGradient(sharded_qp, primal_solution, dual_product);
  internal::PrimalTrustRegionProblem primal_problem(
      &sharded_qp.Qp(), &primal_solution, &primal_part.gradient);
  TrustRegionResultStepSize trust_region_result = SolveTrustRegionStepSize(
      primal_problem, primal_radius, sharded_qp.PrimalSharder());
  return {.part_of_lagrangian_value = primal_part.value,
          .trust_region_objective_delta = trust_region_result.objective_value};
}

MaxNormBoundResult ComputeMaxNormDualTrustRegionBound(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& dual_solution,
    const double dual_radius, const VectorXd& primal_product) {
  LagrangianPart dual_part =
      ComputeDualGradient(sharded_qp, dual_solution, primal_product);
  internal::DualTrustRegionProblem dual_problem(
      &sharded_qp.Qp(), &dual_solution, &dual_part.gradient);
  TrustRegionResultStepSize trust_region_result = SolveTrustRegionStepSize(
      dual_problem, dual_radius, sharded_qp.DualSharder());
  return {.part_of_lagrangian_value = dual_part.value,
          .trust_region_objective_delta = -trust_region_result.objective_value};
}

// Returns the largest radius that the primal could move (in Euclidean distance)
// to match `weighted_distance`. This is the largest value of ||x||_2 such
// that there exists a y such that max{||x||_P, ||y||_D} <= `weighted_distance`.
double MaximumPrimalDistanceGivenWeightedDistance(
    const double weighted_distance, const double primal_weight) {
  return std::sqrt(2) * weighted_distance / std::sqrt(primal_weight);
}

// Returns the largest radius that the dual could move (in Euclidean distance)
// to match `weighted_distance`. This is the largest value of ||y||_2 such
// that there exists an x such that max{||x||_P, ||y||_D} <=
// `weighted_distance`.
double MaximumDualDistanceGivenWeightedDistance(const double weighted_distance,
                                                const double primal_weight) {
  return std::sqrt(2) * weighted_distance * std::sqrt(primal_weight);
}

LocalizedLagrangianBounds ComputeMaxNormLocalizedLagrangianBounds(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& primal_solution,
    const VectorXd& dual_solution, const double primal_weight,
    const double radius, const Eigen::VectorXd& primal_product,
    const Eigen::VectorXd& dual_product) {
  const double primal_radius =
      MaximumPrimalDistanceGivenWeightedDistance(radius, primal_weight);
  const double dual_radius =
      MaximumDualDistanceGivenWeightedDistance(radius, primal_weight);

  // The max norm means that the optimization over the primal and the dual can
  // be done independently.

  MaxNormBoundResult primal_result = ComputeMaxNormPrimalTrustRegionBound(
      sharded_qp, primal_solution, primal_radius, dual_product);

  MaxNormBoundResult dual_result = ComputeMaxNormDualTrustRegionBound(
      sharded_qp, dual_solution, dual_radius, primal_product);

  const double lagrangian_value = primal_result.part_of_lagrangian_value +
                                  dual_result.part_of_lagrangian_value;

  return LocalizedLagrangianBounds{
      .lagrangian_value = lagrangian_value,
      .lower_bound =
          lagrangian_value + primal_result.trust_region_objective_delta,
      .upper_bound =
          lagrangian_value + dual_result.trust_region_objective_delta,
      .radius = radius};
}

LocalizedLagrangianBounds ComputeEuclideanNormLocalizedLagrangianBounds(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& primal_solution,
    const VectorXd& dual_solution, const double primal_weight,
    const double radius, const Eigen::VectorXd& primal_product,
    const Eigen::VectorXd& dual_product,
    const bool use_diagonal_qp_trust_region_solver,
    const double diagonal_qp_trust_region_solver_tolerance) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  const LagrangianPart primal_part =
      ComputePrimalGradient(sharded_qp, primal_solution, dual_product);
  const LagrangianPart dual_part =
      ComputeDualGradient(sharded_qp, dual_solution, primal_product);

  VectorXd trust_region_solution;
  const double lagrangian_value = primal_part.value + dual_part.value;

  Sharder joint_sharder(
      sharded_qp.PrimalSharder(),
      /*num_elements=*/sharded_qp.PrimalSize() + sharded_qp.DualSize());

  if (use_diagonal_qp_trust_region_solver) {
    DiagonalTrustRegionProblemFromQp problem(
        &qp, &primal_solution, &dual_solution, &primal_part.gradient,
        &dual_part.gradient, primal_weight);

    trust_region_solution = SolveDiagonalTrustRegionProblem(
                                problem, joint_sharder, radius,
                                diagonal_qp_trust_region_solver_tolerance)
                                .solution;
  } else {
    JointTrustRegionProblem joint_problem(&qp, &primal_solution, &dual_solution,
                                          &primal_part.gradient,
                                          &dual_part.gradient, primal_weight);

    TrustRegionResultStepSize trust_region_result =
        SolveTrustRegionStepSize(joint_problem, radius, joint_sharder);

    trust_region_solution = ComputeSolution(
        joint_problem, trust_region_result.solution_step_size, joint_sharder);
  }

  auto primal_trust_region_solution =
      trust_region_solution.segment(0, sharded_qp.PrimalSize());
  auto dual_trust_region_solution = trust_region_solution.segment(
      sharded_qp.PrimalSize(), sharded_qp.DualSize());

  // ∇_x L(`primal_solution`, `dual_solution`)^T (x - `primal_solution`)
  double primal_objective_delta =
      sharded_qp.PrimalSharder().ParallelSumOverShards(
          [&](const Sharder::Shard& shard) {
            return shard(primal_part.gradient)
                .dot(shard(primal_trust_region_solution) -
                     shard(primal_solution));
          });

  // Take into account the quadratic's contribution if the diagonal QP solver
  // is enabled.
  if (use_diagonal_qp_trust_region_solver &&
      sharded_qp.Qp().objective_matrix.has_value()) {
    primal_objective_delta += sharded_qp.PrimalSharder().ParallelSumOverShards(
        [&](const Sharder::Shard& shard) {
          const int shard_start =
              sharded_qp.PrimalSharder().ShardStart(shard.Index());
          const int shard_size =
              sharded_qp.PrimalSharder().ShardSize(shard.Index());
          double sum = 0.0;
          for (int i = shard_start; i < shard_start + shard_size; ++i) {
            sum += 0.5 * sharded_qp.Qp().objective_matrix->diagonal()[i] *
                   MathUtil::Square(primal_trust_region_solution[i] -
                                    primal_solution[i]);
          }
          return sum;
        });
  }

  // ∇_y L(`primal_solution`, `dual_solution`)^T (y - `dual_solution`)
  const double dual_objective_delta =
      sharded_qp.DualSharder().ParallelSumOverShards(
          [&](const Sharder::Shard& shard) {
            return shard(dual_part.gradient)
                .dot(shard(dual_trust_region_solution) - shard(dual_solution));
          });

  return LocalizedLagrangianBounds{
      .lagrangian_value = lagrangian_value,
      .lower_bound = lagrangian_value + primal_objective_delta,
      .upper_bound = lagrangian_value + dual_objective_delta,
      .radius = radius};
}

}  // namespace

LocalizedLagrangianBounds ComputeLocalizedLagrangianBounds(
    const ShardedQuadraticProgram& sharded_qp, const VectorXd& primal_solution,
    const VectorXd& dual_solution, const PrimalDualNorm primal_dual_norm,
    const double primal_weight, const double radius,
    const VectorXd* primal_product, const VectorXd* dual_product,
    const bool use_diagonal_qp_trust_region_solver,
    const double diagonal_qp_trust_region_solver_tolerance) {
  const QuadraticProgram& qp = sharded_qp.Qp();
  VectorXd primal_product_storage;
  VectorXd dual_product_storage;

  if (primal_product == nullptr) {
    primal_product_storage = TransposedMatrixVectorProduct(
        sharded_qp.TransposedConstraintMatrix(), primal_solution,
        sharded_qp.TransposedConstraintMatrixSharder());
    primal_product = &primal_product_storage;
  }
  if (dual_product == nullptr) {
    dual_product_storage =
        TransposedMatrixVectorProduct(qp.constraint_matrix, dual_solution,
                                      sharded_qp.ConstraintMatrixSharder());
    dual_product = &dual_product_storage;
  }

  switch (primal_dual_norm) {
    case PrimalDualNorm::kMaxNorm:
      return ComputeMaxNormLocalizedLagrangianBounds(
          sharded_qp, primal_solution, dual_solution, primal_weight, radius,
          *primal_product, *dual_product);
    case PrimalDualNorm::kEuclideanNorm:
      return ComputeEuclideanNormLocalizedLagrangianBounds(
          sharded_qp, primal_solution, dual_solution, primal_weight, radius,
          *primal_product, *dual_product, use_diagonal_qp_trust_region_solver,
          diagonal_qp_trust_region_solver_tolerance);
  }
  LOG(FATAL) << "Unrecognized primal dual norm";

  return LocalizedLagrangianBounds();
}

}  // namespace operations_research::pdlp
