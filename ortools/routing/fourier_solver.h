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

#ifndef ORTOOLS_ROUTING_FOURIER_SOLVER_H_
#define ORTOOLS_ROUTING_FOURIER_SOLVER_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/util/strong_integers.h"

namespace operations_research::routing {

// For a fixed matrix of coefficients rows, allows to computes
// max_r(sum_c(rows[r][c] * values[c])) efficiently for any vector of values.
// A straightforward computation would best leverage SIMD instructions when
// there are many columns. This class computes kBlockSize scalar products in
// parallel, which optimizes the many rows and few columns cases.
// The constructor reorganizes the input rows into a blocked layout, so that
// subsequent calls to Evaluate() can benefit from more efficient memory access.
//
// For instance, suppose the kBlockSize is 4 and rows is a 7 x 5 matrix:
// 11 12 13 14 15
// 21 22 23 24 25
// 31 32 33 34 35
// 41 42 43 44 45
// 51 52 53 54 55
// 61 62 63 64 65
// 71 72 73 74 75
//
// This class will separate the matrix into 4 x 1 submatrices:
// 11 | 12 | 13 | 14 | 15
// 21 | 22 | 23 | 24 | 25
// 31 | 32 | 33 | 34 | 35
// 41 | 42 | 43 | 44 | 45
// ---+----+----+----+----
// 51 | 52 | 53 | 54 | 55
// 61 | 62 | 63 | 64 | 65
// 71 | 72 | 73 | 74 | 75
// XX | XX | XX | XX | XX
// NOTE: we need to expand the matrix until the number of rows is a multiple of
// kBlockSize. We do that by adding copies of an existing row, which does not
// change the semantics "maximum over linear expressions".
//
// Those blocks are aggregated into a single vector of blocks:
// {{11, 21, 31, 41}, {12, 22, 32, 42}, {13, 23, 33, 43}, {14, 24, 34, 44}.
//  {15, 25, 35, 45}, {51, 61, 71, XX}, {52, 62, 72, XX}, {53, 63, 73, XX},
//  {54, 64, 74, XX}, {55, 65, 75, XX}}.
//
// The general formula to map rows to blocks: rows[r][v] is mapped to
// blocks_[r / kBlockSize * num_variables_ + v].coefficient[r % kBlockSize].
// blocks_[(br, v)].coefficient[c] = row[br * kBlockSize + c][v].
//
// When evaluating a vector of values, instead of computing:
// max_{r in [0, num_rows)}
//     sum_{c in [0, num_variables_)} rows[r][c] * values[c],
// we compute:
// max_{r' in [0, ceil(num_rows / kBlockSize))}
//     BlockMaximum(sum_{i in [0, num_variables)}
//                      blocks[r' * num_variables + i] * values[i]),
// with BlockMaximum(block) = max_{j in [0, kBlockSize)} block[j].
class MaxLinearExpressionEvaluator {
 public:
  // Makes an object that can evaluate the expression
  // max_r(sum_c(rows[r][c] * values[c])) for any vector of values.
  explicit MaxLinearExpressionEvaluator(
      const std::vector<std::vector<double>>& rows);
  // Returns max_r(sum_c(rows[r][c] * values[c])).
  double Evaluate(absl::Span<const double> values) const;

 private:
  // This number was found by running the associated microbenchmarks.
  // It is larger than one cacheline or SIMD register, surprisingly.
  static constexpr int kBlockSize = 16;
  struct Block {
    std::array<double, kBlockSize> coefficients;
    // Returns *this += other * value.
    Block& BlockMultiplyAdd(const Block& other, double value) {
      // The loop bounds are known in advance, we rely on the compiler to unroll
      // and SIMD optimize it.
      for (int i = 0; i < kBlockSize; ++i) {
        coefficients[i] += other.coefficients[i] * value;
      }
      return *this;
    }
    Block& MaximumWith(const Block& other) {
      for (int i = 0; i < kBlockSize; ++i) {
        coefficients[i] = std::max(coefficients[i], other.coefficients[i]);
      }
      return *this;
    }
    double Maximum() const {
      return *std::max_element(coefficients.begin(), coefficients.end());
    }
  };
  std::vector<Block> blocks_;
  const int64_t num_variables_;
  const int64_t num_rows_;
};

// TODO(b/492476073): Present this as a projection algorithm, where optimizing
// is projected on z (the variable defining the cost)
//
// This class implements the Fourier elimination-based method for linear
// programming. It allows to compute feasibility and minimums of related LPs,
// that only differ in the values of parameters described as symbolic variables.
//
// TODO(b/492476073): define the notion of symbolic variables. Maybe use this
// framework to present the problem: the initial space has n + n_s + 1
// dimensions, we want to project the feasible polyhedron in the space of
// symbolic variables + objective in n_s + 1 dimensions.
// TODO(b/492476073): separate comments in more categories: usage, algorithm,
// implementation details.
//
// Usage. The expected workflow is:
// - describe an LP problem, in any order:
//   - add variables, symbolic or not, with AddVariable()
//   - add constraints, AddConstraint() and SetCoefficient()
//   - add objective with SetObjectiveCoefficient()
// - solve the problem with Fourier elimination using Solve().
//   This projects the polyhedron over all variables described by constraints
//   onto a polyhedron over the space of symbolic variables and objective.
//   Solve() may return false if the problem is infeasible, i.e. there is no
//   valuation of the symbolic variables that satisfies the constraints.
// - if the problem is feasible, the following loop may be repeated an arbitrary
//   number of times:
//   - set a value for all symbolic variables with SetSymbolicVariableValue()
//   - evaluate the feasibility of the problem with EvaluateFeasibility()
//   - evaluate the objective of the problem with EvaluateObjective()
//
// TODO(b/492476073): mention complexity of naive and improved algorithms.
// The class is only useful if there are symbolic variables, because Fourier
// elimination is not the best way to solve LPs. Moreover, there may only be
// few variables and constraints, because the approach transforms the initial
// problem into a representation of the polyhedron over symbolic variables,
// which may have a number of constraints exponential in the number of
// constraints of the original problem. The implementation actually limits
// the number of finite bounds in constraints and variables to be <= 64.
// An interesting use case is to set symbolic variables as bounds of other
// variables, and evaluate the LP for different bound configurations:
// - describe the problem with x, y, z, constraints and objective.
// - add symbolic variables x_min, x_max, ...
// - add constraints x_min <= x, x <= x_max, ...
// - solve the general problem with Fourier elimination.
// - for each configuration of interest, evaluate feasibility and minimum
//   of the LP where x_min, x_max, ... are set to values of interest.
// TODO(b/492476073): cite relevant literature. Fourier's elimination by H.P.
// Williams, at least a paper citing Kohler such as Duffin.
// No need to cite more recent work.
//
// Implementation details:
// - the objective is encoded as the first variable z, and a constraint
//   z >= objective linear expression. It is always a minimization.
// - all constraints are stored as lb <= expression.
// - variable bounds are stored as constraints.
// - even though the problem is floating-point, we rescale constraints using
//   the gcd of coefficients and bound, in a bid to avoid coefficient blow-up.
class FourierSolver {
 public:
  FourierSolver();

  DEFINE_STRONG_INDEX_TYPE(ColIndex);
  DEFINE_STRONG_INDEX_TYPE(RowIndex);
  static constexpr double kInfinity = std::numeric_limits<double>::infinity();

  ColIndex AddVariable(double lb, double ub, bool is_symbolic = false);

  struct CoefficientVariable {
    double coef;
    ColIndex var;
  };
  RowIndex AddConstraint(
      double lb, double ub,
      std::initializer_list<CoefficientVariable> coefficients);
  RowIndex AddConstraint(double lb, double ub,
                         std::vector<CoefficientVariable> coefficients);
  void SetObjectiveCoefficient(ColIndex col, double coef);

  bool Solve();
  void SetSymbolicVariableValue(ColIndex var, double value);
  bool EvaluateFeasibility() const;
  double EvaluateObjective() const;

 private:
  struct Constraint {
    util_intops::StrongVector<ColIndex, double> coefs;
    double lb;
    double ub;
    uint64_t combination;
  };
  // TODO(b/492476073): do something more general for floating point values.
  // Finds the gcd of all coefficients of the constraint and its bounds,
  // divides those values by the gcd, returns the gcd.
  // Returns 1 when a value does not fit an int64_t, ignoring infinities.
  static double RescaleConstraint(Constraint& ct);
  // Normalizes the problem:
  // - sets all constraint length to the same number of variables,
  // - turns all constraints into the lb <= linexpr form, duplicating
  //   constraints with both lower and upper bounds.
  // - rescales with a per-constraint gcd.
  // - turns variable bounds into constraints x.min <= x and -x >= -x.max.
  // - removes trivially feasible constraints.
  // TODO(b/492476073): if a variable 'x' appears with the same coefficient
  // sign, all the constraints where 'x' appears can be removed. Other cases?
  // Returns false if the problem is trivially infeasible.
  bool PreprocessConstraints();
  // Removes constraints have the same linear expression and a weaker bound.
  void RemoveDominatedConstraints();

  int num_variables_;
  double objective_scale_ = 1.0;  // Actual objective = objective * scale_.

  // TODO(b/492476073): Document all data members w.r.t class documentation.
  struct Bounds {
    double lb;
    double ub;
  };
  util_intops::StrongVector<ColIndex, Bounds> variable_bounds_;
  util_intops::StrongVector<ColIndex, bool> variable_is_symbolic_;

  util_intops::StrongVector<RowIndex, Constraint> constraints_;

  std::vector<Constraint> pos_constraints_;
  std::vector<Constraint> neg_constraints_;
  util_intops::StrongVector<RowIndex, Constraint> zero_constraints_;

  enum class Status { kModeling, kFeasible, kInfeasible };
  Status status_;

  std::unique_ptr<MaxLinearExpressionEvaluator> fea_evaluator_;
  std::unique_ptr<MaxLinearExpressionEvaluator> obj_evaluator_;
  util_intops::StrongVector<ColIndex, int>
      evaluator_index_of_symbolic_variable_;
  std::vector<double> evaluator_values_;
};

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_FOURIER_SOLVER_H_
