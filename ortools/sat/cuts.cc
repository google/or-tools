// Copyright 2010-2018 Google LLC
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

#include "ortools/sat/cuts.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "ortools/algorithms/knapsack_solver_for_cuts.h"
#include "ortools/base/integral_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {

// Minimum amount of violation of the cut constraint by the solution. This
// is needed to avoid numerical issues and adding cuts with minor effect.
const double kMinCutViolation = 1e-4;

// Returns a constraint that disallow all given variables to be at their current
// upper bound. The arguments must form a non-trival constraint of the form
// sum terms (coeff * var) <= upper_bound.
LinearConstraint GenerateKnapsackCutForCover(
    const std::vector<IntegerVariable>& vars,
    const std::vector<IntegerValue>& coeffs, const IntegerValue upper_bound,
    const IntegerTrail& integer_trail) {
  CHECK_EQ(vars.size(), coeffs.size());
  CHECK_GT(vars.size(), 0);
  LinearConstraint cut;
  IntegerValue cut_upper_bound = IntegerValue(0);
  IntegerValue max_coeff = coeffs[0];
  // slack = \sum_{i}(coeffs[i] * upper_bound[i]) - upper_bound.
  IntegerValue slack = -upper_bound;
  for (int i = 0; i < vars.size(); ++i) {
    const IntegerValue var_upper_bound =
        integer_trail.LevelZeroUpperBound(vars[i]);
    cut_upper_bound += var_upper_bound;
    cut.vars.push_back(vars[i]);
    cut.coeffs.push_back(IntegerValue(1));
    max_coeff = std::max(max_coeff, coeffs[i]);
    slack += coeffs[i] * var_upper_bound;
  }
  CHECK_GT(slack, 0.0) << "Invalid cover for knapsack cut.";
  cut_upper_bound -= CeilRatio(slack, max_coeff);
  cut.lb = kMinIntegerValue;
  cut.ub = cut_upper_bound;
  VLOG(2) << "Generated Knapsack Constraint:" << cut.DebugString();
  return cut;
}

bool SolutionSatisfiesConstraint(
    const LinearConstraint& constraint,
    const gtl::ITIVector<IntegerVariable, double>& lp_values) {
  const double activity = ComputeActivity(constraint, lp_values);
  const double tolerance = 1e-6;
  return (activity <= constraint.ub.value() + tolerance &&
          activity >= constraint.lb.value() - tolerance)
             ? true
             : false;
}

bool AllCoefficientsMagnitudeAreTheSame(const LinearConstraint& constraint) {
  if (constraint.vars.size() <= 1) return true;

  const int64 magnitude = std::abs(constraint.coeffs[0].value());
  for (int i = 1; i < constraint.coeffs.size(); ++i) {
    if (std::abs(constraint.coeffs[i].value()) != magnitude) {
      return false;
    }
  }
  return true;
}

bool AllVarsTakeIntegerValue(
    const std::vector<IntegerVariable> vars,
    const gtl::ITIVector<IntegerVariable, double>& lp_values) {
  for (IntegerVariable var : vars) {
    if (std::abs(lp_values[var] - std::round(lp_values[var])) > 1e-6) {
      return false;
    }
  }
  return true;
}

// Returns smallest cover size for the given constraint taking into account
// level zero bounds. Smallest Cover size is computed as follows.
// 1. Compute the upper bound if all variables are shifted to have zero lower
//    bound.
// 2. Sort all terms (coefficient * shifted upper bound) in non decreasing
//    order.
// 3. Add terms in cover untill term sum is smaller or equal to upper bound.
// 4. Add the last item which violates the upper bound. This forms the smallest
//    cover. Return the size of this cover.
int GetSmallestCoverSize(const LinearConstraint& constraint,
                         const IntegerTrail& integer_trail) {
  IntegerValue ub = constraint.ub;
  std::vector<IntegerValue> sorted_terms;
  for (int i = 0; i < constraint.vars.size(); ++i) {
    const IntegerValue coeff = constraint.coeffs[i];
    const IntegerVariable var = constraint.vars[i];
    const IntegerValue var_ub = integer_trail.LevelZeroUpperBound(var);
    const IntegerValue var_lb = integer_trail.LevelZeroLowerBound(var);
    ub -= var_lb * coeff;
    sorted_terms.push_back(coeff * (var_ub - var_lb));
  }
  std::sort(sorted_terms.begin(), sorted_terms.end(),
            std::greater<IntegerValue>());
  int smallest_cover_size = 0;
  IntegerValue sorted_term_sum = IntegerValue(0);
  while (sorted_term_sum <= ub &&
         smallest_cover_size < constraint.vars.size()) {
    sorted_term_sum += sorted_terms[smallest_cover_size++];
  }
  return smallest_cover_size;
}

bool ConstraintIsEligibleForLifting(const LinearConstraint& constraint,
                                    const IntegerTrail& integer_trail) {
  for (const IntegerVariable var : constraint.vars) {
    if (integer_trail.LevelZeroLowerBound(var) != IntegerValue(0) ||
        integer_trail.LevelZeroUpperBound(var) != IntegerValue(1)) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool LiftKnapsackCut(
    const LinearConstraint& constraint,
    const gtl::ITIVector<IntegerVariable, double>& lp_values,
    const std::vector<IntegerValue>& cut_vars_original_coefficients,
    const IntegerTrail& integer_trail, TimeLimit* time_limit,
    LinearConstraint* cut) {
  std::set<IntegerVariable> vars_in_cut;
  for (IntegerVariable var : cut->vars) {
    vars_in_cut.insert(var);
  }

  std::vector<std::pair<IntegerValue, IntegerVariable>> non_zero_vars;
  std::vector<std::pair<IntegerValue, IntegerVariable>> zero_vars;
  for (int i = 0; i < constraint.vars.size(); ++i) {
    const IntegerVariable var = constraint.vars[i];
    if (integer_trail.LevelZeroLowerBound(var) != IntegerValue(0) ||
        integer_trail.LevelZeroUpperBound(var) != IntegerValue(1)) {
      continue;
    }
    if (vars_in_cut.find(var) != vars_in_cut.end()) continue;
    const IntegerValue coeff = constraint.coeffs[i];
    if (lp_values[var] <= 1e-6) {
      zero_vars.push_back({coeff, var});
    } else {
      non_zero_vars.push_back({coeff, var});
    }
  }

  // Decide lifting sequence (nonzeros, zeros in nonincreasing order
  // of coefficient ).
  std::sort(non_zero_vars.rbegin(), non_zero_vars.rend());
  std::sort(zero_vars.rbegin(), zero_vars.rend());

  std::vector<std::pair<IntegerValue, IntegerVariable>> lifting_sequence(
      std::move(non_zero_vars));

  lifting_sequence.insert(lifting_sequence.end(), zero_vars.begin(),
                          zero_vars.end());

  // Form Knapsack.
  std::vector<double> lifting_profits;
  std::vector<double> lifting_weights;
  for (int i = 0; i < cut->vars.size(); ++i) {
    lifting_profits.push_back(cut->coeffs[i].value());
    lifting_weights.push_back(cut_vars_original_coefficients[i].value());
  }

  // Lift the cut.
  bool is_lifted = false;
  bool is_solution_optimal = false;
  KnapsackSolverForCuts knapsack_solver("Knapsack cut lifter");
  for (auto entry : lifting_sequence) {
    is_solution_optimal = false;
    const IntegerValue var_original_coeff = entry.first;
    const IntegerVariable var = entry.second;
    const IntegerValue lifting_capacity = constraint.ub - entry.first;
    if (lifting_capacity <= IntegerValue(0)) continue;
    knapsack_solver.Init(lifting_profits, lifting_weights,
                         lifting_capacity.value());
    knapsack_solver.set_node_limit(100);
    // NOTE: Since all profits and weights are integer, solution of
    // knapsack is also integer.
    // TODO(user): Use an integer solver or heuristic.
    knapsack_solver.Solve(time_limit, &is_solution_optimal);
    const double knapsack_upper_bound =
        std::round(knapsack_solver.GetUpperBound());
    const IntegerValue cut_coeff = cut->ub - knapsack_upper_bound;
    if (cut_coeff > IntegerValue(0)) {
      is_lifted = true;
      cut->vars.push_back(var);
      cut->coeffs.push_back(cut_coeff);
      lifting_profits.push_back(cut_coeff.value());
      lifting_weights.push_back(var_original_coeff.value());
    }
  }
  return is_lifted;
}

LinearConstraint GetPreprocessedLinearConstraint(
    const LinearConstraint& constraint,
    const gtl::ITIVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail) {
  IntegerValue ub = constraint.ub;
  LinearConstraint constraint_with_left_vars;
  for (int i = 0; i < constraint.vars.size(); ++i) {
    const IntegerVariable var = constraint.vars[i];
    const IntegerValue var_ub = integer_trail.LevelZeroUpperBound(var);
    const IntegerValue coeff = constraint.coeffs[i];
    if (var_ub.value() - lp_values[var] <= 1.0 - kMinCutViolation) {
      constraint_with_left_vars.vars.push_back(var);
      constraint_with_left_vars.coeffs.push_back(coeff);
    } else {
      // Variable not in cut
      const IntegerValue var_lb = integer_trail.LevelZeroLowerBound(var);
      ub -= coeff * var_lb;
    }
  }
  constraint_with_left_vars.ub = ub;
  constraint_with_left_vars.lb = constraint.lb;
  return constraint_with_left_vars;
}

bool ConstraintIsTriviallyTrue(const LinearConstraint& constraint,
                               const IntegerTrail& integer_trail) {
  IntegerValue term_sum = IntegerValue(0);
  for (int i = 0; i < constraint.vars.size(); ++i) {
    const IntegerVariable var = constraint.vars[i];
    const IntegerValue var_ub = integer_trail.LevelZeroUpperBound(var);
    const IntegerValue coeff = constraint.coeffs[i];
    term_sum += coeff * var_ub;
  }
  if (term_sum <= constraint.ub) {
    VLOG(2) << "Filtered by cover filter";
    return true;
  }
  return false;
}

bool CanBeFilteredUsingCutLowerBound(
    const LinearConstraint& preprocessed_constraint,
    const gtl::ITIVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail) {
  std::vector<double> variable_upper_bound_distances;
  for (const IntegerVariable var : preprocessed_constraint.vars) {
    const IntegerValue var_ub = integer_trail.LevelZeroUpperBound(var);
    variable_upper_bound_distances.push_back(var_ub.value() - lp_values[var]);
  }
  // Compute the min cover size.
  const int smallest_cover_size =
      GetSmallestCoverSize(preprocessed_constraint, integer_trail);

  std::nth_element(
      variable_upper_bound_distances.begin(),
      variable_upper_bound_distances.begin() + smallest_cover_size - 1,
      variable_upper_bound_distances.end());
  double cut_lower_bound = 0.0;
  for (int i = 0; i < smallest_cover_size; ++i) {
    cut_lower_bound += variable_upper_bound_distances[i];
  }
  if (cut_lower_bound >= 1.0 - kMinCutViolation) {
    VLOG(2) << "Filtered by kappa heuristic";
    return true;
  }
  return false;
}

double GetKnapsackUpperBound(std::vector<KnapsackItem> items,
                             const double capacity) {
  // Sort items by value by weight ratio.
  std::sort(items.begin(), items.end(), std::greater<KnapsackItem>());
  double left_capacity = capacity;
  double profit = 0.0;
  for (const KnapsackItem item : items) {
    if (item.weight <= left_capacity) {
      profit += item.profit;
      left_capacity -= item.weight;
    } else {
      profit += (left_capacity / item.weight) * item.profit;
      break;
    }
  }
  return profit;
}

bool CanBeFilteredUsingKnapsackUpperBound(
    const LinearConstraint& constraint,
    const gtl::ITIVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail) {
  std::vector<KnapsackItem> items;
  double capacity = -constraint.ub.value() - 1.0;
  double sum_variable_profit = 0;
  for (int i = 0; i < constraint.vars.size(); ++i) {
    const IntegerVariable var = constraint.vars[i];
    const IntegerValue var_ub = integer_trail.LevelZeroUpperBound(var);
    const IntegerValue var_lb = integer_trail.LevelZeroLowerBound(var);
    const IntegerValue coeff = constraint.coeffs[i];
    KnapsackItem item;
    item.profit = var_ub.value() - lp_values[var];
    item.weight = (coeff * (var_ub - var_lb)).value();
    items.push_back(item);
    capacity += (coeff * var_ub).value();
    sum_variable_profit += item.profit;
  }

  // Return early if the required upper bound is negative since all the profits
  // are non negative.
  if (sum_variable_profit - 1.0 + kMinCutViolation < 0.0) return false;

  // Get the knapsack upper bound.
  const double knapsack_upper_bound =
      GetKnapsackUpperBound(std::move(items), capacity);
  if (knapsack_upper_bound < sum_variable_profit - 1.0 + kMinCutViolation) {
    VLOG(2) << "Filtered by knapsack upper bound";
    return true;
  }
  return false;
}

bool CanFormValidKnapsackCover(
    const LinearConstraint& preprocessed_constraint,
    const gtl::ITIVector<IntegerVariable, double>& lp_values,
    const IntegerTrail& integer_trail) {
  if (ConstraintIsTriviallyTrue(preprocessed_constraint, integer_trail)) {
    return false;
  }
  if (CanBeFilteredUsingCutLowerBound(preprocessed_constraint, lp_values,
                                      integer_trail)) {
    return false;
  }
  if (CanBeFilteredUsingKnapsackUpperBound(preprocessed_constraint, lp_values,
                                           integer_trail)) {
    return false;
  }
  return true;
}

void ConvertToKnapsackForm(
    const LinearConstraint& constraint,
    std::vector<LinearConstraint>* knapsack_constraints) {
  if (AllCoefficientsMagnitudeAreTheSame(constraint)) {
    // The knapsack cut generated on such constraints can not be stronger than
    // the constraint themselves.
    return;
  }
  if (constraint.ub < kMaxIntegerValue) {
    LinearConstraint canonical_knapsack_form;

    // Negate the variables with negative coefficients.
    for (int i = 0; i < constraint.vars.size(); ++i) {
      const IntegerVariable var = constraint.vars[i];
      const IntegerValue coeff = constraint.coeffs[i];
      if (coeff > IntegerValue(0)) {
        canonical_knapsack_form.AddTerm(var, coeff);
      } else {
        canonical_knapsack_form.AddTerm(NegationOf(var), -coeff);
      }
    }
    canonical_knapsack_form.ub = constraint.ub;
    canonical_knapsack_form.lb = kMinIntegerValue;
    knapsack_constraints->push_back(canonical_knapsack_form);
  }

  if (constraint.lb > kMinIntegerValue) {
    LinearConstraint canonical_knapsack_form;

    // Negate the variables with positive coefficients.
    for (int i = 0; i < constraint.vars.size(); ++i) {
      const IntegerVariable var = constraint.vars[i];
      const IntegerValue coeff = constraint.coeffs[i];
      if (coeff > IntegerValue(0)) {
        canonical_knapsack_form.AddTerm(NegationOf(var), coeff);
      } else {
        canonical_knapsack_form.AddTerm(var, -coeff);
      }
    }
    canonical_knapsack_form.ub = -constraint.lb;
    canonical_knapsack_form.lb = kMinIntegerValue;
    knapsack_constraints->push_back(canonical_knapsack_form);
  }
}

// TODO(user): Move the cut generator into a class and reuse variables.
CutGenerator CreateKnapsackCoverCutGenerator(
    const std::vector<LinearConstraint>& base_constraints,
    const std::vector<IntegerVariable>& vars, Model* model) {
  CutGenerator result;
  result.vars = vars;
  result.type = "Knapsack";

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  std::vector<LinearConstraint> knapsack_constraints;
  for (const LinearConstraint& constraint : base_constraints) {
    if (constraint.vars.empty()) continue;
    ConvertToKnapsackForm(constraint, &knapsack_constraints);
  }
  VLOG(1) << "#knapsack constraints: " << knapsack_constraints.size();
  // TODO(user): do not add generator if there are no knapsack constraints.
  result.generate_cuts = [knapsack_constraints, vars, model, integer_trail](
                             const gtl::ITIVector<IntegerVariable, double>&
                                 lp_values) {
    std::vector<LinearConstraint> cuts;
    if (AllVarsTakeIntegerValue(vars, lp_values)) return cuts;

    KnapsackSolverForCuts knapsack_solver(
        "Knapsack on demand cover cut generator");
    int64 skipped_constraints = 0;
    int64 lifted_cuts = 0;
    // Iterate through all knapsack constraints.
    for (const LinearConstraint& constraint : knapsack_constraints) {
      if (model->GetOrCreate<TimeLimit>()->LimitReached()) break;
      VLOG(2) << "Processing constraint: " << constraint.DebugString();
      const LinearConstraint preprocessed_constraint =
          GetPreprocessedLinearConstraint(constraint, lp_values,
                                          *integer_trail);
      if (!CanFormValidKnapsackCover(preprocessed_constraint, lp_values,
                                     *integer_trail)) {
        skipped_constraints++;
        continue;
      }

      // Profits are (upper_bounds[i] - lp_values[i]) for knapsack variables.
      std::vector<double> profits;
      profits.reserve(preprocessed_constraint.vars.size());

      // Weights are (coeffs[i] * (upper_bound[i] - lower_bound[i])).
      std::vector<double> weights;
      weights.reserve(preprocessed_constraint.vars.size());

      double capacity = -preprocessed_constraint.ub.value() - 1.0;

      // Compute and store the sum of variable profits. This is the constant
      // part of the objective of the problem we are trying to solve. Hence
      // this part is not supplied to the knapsack_solver and is subtracted
      // when we receive the knapsack solution.
      double sum_variable_profit = 0;

      // Compute the profits, the weights and the capacity for the knapsack
      // instance.
      for (int i = 0; i < preprocessed_constraint.vars.size(); ++i) {
        const IntegerVariable var = preprocessed_constraint.vars[i];
        const double coefficient = preprocessed_constraint.coeffs[i].value();
        const double var_ub = ToDouble(integer_trail->LevelZeroUpperBound(var));
        const double var_lb = ToDouble(integer_trail->LevelZeroLowerBound(var));
        const double variable_profit = var_ub - lp_values[var];
        profits.push_back(variable_profit);

        sum_variable_profit += variable_profit;

        const double weight = coefficient * (var_ub - var_lb);
        weights.push_back(weight);
        capacity += weight + coefficient * var_lb;
      }
      if (capacity < 0.0) continue;

      std::vector<IntegerVariable> cut_vars;
      std::vector<IntegerValue> cut_vars_original_coefficients;

      VLOG(2) << "Knapsack size: " << profits.size();
      knapsack_solver.Init(profits, weights, capacity);

      // Set the time limit for the knapsack solver.
      const double time_limit_for_knapsack_solver =
          model->GetOrCreate<TimeLimit>()->GetTimeLeft();

      // Solve the instance and subtract the constant part to compute the
      // sum_of_distance_to_ub_for_vars_in_cover.
      // TODO(user): Consider solving the instance approximately.
      bool is_solution_optimal = false;
      knapsack_solver.set_solution_upper_bound_threshold(
          sum_variable_profit - 1.0 + kMinCutViolation);
      // TODO(user): Consider providing lower bound threshold as
      // sum_variable_profit - 1.0 + kMinCutViolation.
      // TODO(user): Set node limit for knapsack solver.
      std::unique_ptr<TimeLimit> time_limit_for_solver =
          absl::make_unique<TimeLimit>(time_limit_for_knapsack_solver);
      const double sum_of_distance_to_ub_for_vars_in_cover =
          sum_variable_profit -
          knapsack_solver.Solve(time_limit_for_solver.get(),
                                &is_solution_optimal);
      if (is_solution_optimal) {
        VLOG(2) << "Knapsack Optimal solution found yay !";
      }
      if (time_limit_for_solver->LimitReached()) {
        VLOG(1) << "Knapsack Solver run out of time limit.";
      }
      if (sum_of_distance_to_ub_for_vars_in_cover < 1.0 - kMinCutViolation) {
        // Constraint is eligible for the cover.

        IntegerValue constraint_ub_for_cut = preprocessed_constraint.ub;
        std::set<IntegerVariable> vars_in_cut;
        for (int i = 0; i < preprocessed_constraint.vars.size(); ++i) {
          const IntegerVariable var = preprocessed_constraint.vars[i];
          const IntegerValue coefficient = preprocessed_constraint.coeffs[i];
          if (!knapsack_solver.best_solution(i)) {
            cut_vars.push_back(var);
            cut_vars_original_coefficients.push_back(coefficient);
            vars_in_cut.insert(var);
          } else {
            const IntegerValue var_lb = integer_trail->LevelZeroLowerBound(var);
            constraint_ub_for_cut -= coefficient * var_lb;
          }
        }
        LinearConstraint cut = GenerateKnapsackCutForCover(
            cut_vars, cut_vars_original_coefficients, constraint_ub_for_cut,
            *integer_trail);

        // Check if the constraint has only binary variables.
        if (ConstraintIsEligibleForLifting(cut, *integer_trail)) {
          if (LiftKnapsackCut(constraint, lp_values,
                              cut_vars_original_coefficients, *integer_trail,
                              model->GetOrCreate<TimeLimit>(), &cut)) {
            lifted_cuts++;
          }
        }

        CHECK(!SolutionSatisfiesConstraint(cut, lp_values));
        cuts.push_back(cut);
      }
    }
    if (skipped_constraints > 0) {
      VLOG(2) << "Skipped constraints: " << skipped_constraints;
    }
    if (!cuts.empty()) VLOG(1) << "#KnapsackCuts: " << cuts.size();
    if (lifted_cuts > 0) VLOG(1) << "#LiftedKnapsackCuts: " << lifted_cuts;
    return cuts;
  };

  return result;
}

std::function<IntegerValue(IntegerValue)> GetSuperAdditiveRoundingFunction(
    bool use_letchford_lodi_version, IntegerValue rhs_remainder,
    IntegerValue divisor, IntegerValue max_scaling) {
  // Compute the larger t <= max_scaling such that
  // t * rhs_remainder >= divisor / 2.
  const IntegerValue t =
      rhs_remainder == 0
          ? max_scaling
          : std::min(max_scaling, CeilRatio(divisor / 2, rhs_remainder));

  // Adjust after the multiplication by t.
  rhs_remainder *= t;
  max_scaling /= t;

  // This is the only difference compared to a discretized MIR function.
  if (use_letchford_lodi_version && max_scaling > 2) max_scaling = 2;

  CHECK_GE(max_scaling, 1);
  const IntegerValue size = divisor - rhs_remainder;
  if (max_scaling == 1) {
    // TODO(user): Use everywhere a two step computation to avoid overflow?
    // First divide by divisor, then multiply by t.
    return [t, divisor](IntegerValue coeff) {
      return FloorRatio(t * coeff, divisor);
    };
  } else if (size <= max_scaling) {
    return [size, rhs_remainder, t, divisor](IntegerValue coeff) {
      const IntegerValue ratio = FloorRatio(t * coeff, divisor);
      const IntegerValue remainder = t * coeff - ratio * divisor;
      const IntegerValue diff = remainder - rhs_remainder;
      return size * ratio + std::max(IntegerValue(0), diff);
    };
  } else {
    // We divide (size = divisor - rhs_remainder) into (max_scaling - 1) buckets
    // and increase the function by 1 / max_scaling for each of them.
    //
    // Note that for different values of max_scaling, we get a family of
    // functions that do not dominate each others. So potentially, a max scaling
    // as low as 2 could lead to the better cut (this is exactly the Letchford &
    // Lodi function).
    ///
    // Another intersting fact, is that if we want to compute the maximum alpha
    // for a constraint with 2 terms like:
    //    divisor * Y + (ratio * divisor + remainder) * X
    //               <= rhs_ratio * divisor + rhs_remainder
    // so that we have the cut:
    //              Y + (ratio + alpha) * X  <= rhs_ratio
    // This is the same as computing the maximum alpha such that for all integer
    // X > 0 we have CeilRatio(alpha * divisor * X, divisor)
    //    <= CeilRatio(remainder * X - rhs_remainder, divisor).
    // We can prove that this alpha is of the form (n - 1) / n, and it will
    // be reached by such function for a max_scaling of n.
    //
    // TODO(user): This function is not always maximal when
    // size % (max_scaling - 1) == 0. Improve?
    return [size, rhs_remainder, t, divisor, max_scaling](IntegerValue coeff) {
      const IntegerValue ratio = FloorRatio(t * coeff, divisor);
      const IntegerValue remainder = t * coeff - ratio * divisor;
      const IntegerValue diff = remainder - rhs_remainder;
      const IntegerValue bucket =
          diff > 0 ? CeilRatio(diff * (max_scaling - 1), size)
                   : IntegerValue(0);
      return max_scaling * ratio + bucket;
    };
  }
}

void IntegerRoundingCut(RoundingOptions options, std::vector<double> lp_values,
                        std::vector<IntegerValue> lower_bounds,
                        std::vector<IntegerValue> upper_bounds,
                        LinearConstraint* cut) {
  const int size = lp_values.size();
  if (size == 0) return;
  CHECK_EQ(lower_bounds.size(), size);
  CHECK_EQ(upper_bounds.size(), size);
  CHECK_EQ(cut->vars.size(), size);
  CHECK_EQ(cut->coeffs.size(), size);
  CHECK_EQ(cut->lb, kMinIntegerValue);

  // Shift each variable using its lower/upper bound so that no variable can
  // change sign. We eventually do a change of variable to its negation so
  // that all variable are non-negative.
  bool overflow = false;
  std::vector<bool> change_sign_at_postprocessing(size, false);
  IntegerValue max_initial_magnitude(1);
  for (int i = 0; i < size && !overflow; ++i) {
    if (cut->coeffs[i] == 0) continue;

    // Note that since we use ToDouble() this code works fine with lb/ub at
    // min/max integer value.
    {
      const double value = lp_values[i];
      const IntegerValue lb = lower_bounds[i];
      const IntegerValue ub = upper_bounds[i];
      if (std::abs(value - ToDouble(lb)) > std::abs(value - ToDouble(ub))) {
        // Change the variable sign.
        change_sign_at_postprocessing[i] = true;
        cut->coeffs[i] = -cut->coeffs[i];
        lp_values[i] = -lp_values[i];

        std::swap(lower_bounds[i], upper_bounds[i]);
        lower_bounds[i] = -lower_bounds[i];
        upper_bounds[i] = -upper_bounds[i];
      }
    }

    // Always shift to lb.
    // coeff * X = coeff * (X - shift) + coeff * shift.
    lp_values[i] -= ToDouble(lower_bounds[i]);
    if (!AddProductTo(-cut->coeffs[i], lower_bounds[i], &cut->ub)) {
      overflow = true;
      break;
    }

    // Deal with fixed variable, no need to shift back in this case, we can
    // just remove the term.
    if (lower_bounds[i] == upper_bounds[i]) {
      cut->coeffs[i] = IntegerValue(0);
      lp_values[i] = 0.0;
    }

    max_initial_magnitude =
        std::max(max_initial_magnitude, IntTypeAbs(cut->coeffs[i]));
  }
  if (overflow) {
    VLOG(1) << "Issue, overflow.";
    *cut = LinearConstraint(IntegerValue(0), IntegerValue(0));
    return;
  }

  // Our heuristic will try to generate a few different cuts, and we will keep
  // the most violated one.
  double best_scaled_violation = 0.01;
  LinearConstraint best_cut(IntegerValue(0), IntegerValue(0));

  for (int i = 0; i < size; ++i) {
    // Skip shifted variable almost at their lower bound.
    if (lp_values[i] <= 1e-4) continue;
    const IntegerValue divisor = IntTypeAbs(cut->coeffs[i]);

    // Skip if we don't have the potential to generate a good enough cut.
    const IntegerValue initial_rhs_remainder =
        cut->ub - FloorRatio(cut->ub, divisor) * divisor;
    if (ToDouble(initial_rhs_remainder) / ToDouble(max_initial_magnitude) <=
        best_scaled_violation) {
      continue;
    }

    // TODO(user): We could avoid this copy.
    LinearConstraint temp_cut = *cut;

    // We will adjust coefficient that are just under an exact multiple of
    // divisor to an exact multiple. This is meant to get rid of small errors
    // that appears due to rounding error in our exact computation of the
    // initial constraint given to this class.
    //
    // Each adjustement will cause the initial_rhs_remainder to increase, and we
    // do not want to increase it above divisor. Our threshold below guarantees
    // this. Note that the higher the rhs_remainder becomes, the more the
    // function f() has a chance to reduce the violation, so it is not always a
    // good idea to use all the slack we have between initial_rhs_remainder and
    // divisor.
    //
    // TODO(user): If possible, it might be better to complement these
    // variables. Even if the adjusted lp_values end up larger, if we loose less
    // when taking f(), then we will have a better violation.
    const IntegerValue adjust_threshold =
        (divisor - initial_rhs_remainder - 1) / IntegerValue(size);
    if (adjust_threshold > 0) {
      for (int i = 0; i < size; ++i) {
        const IntegerValue coeff = temp_cut.coeffs[i];
        const IntegerValue diff(
            CapSub(upper_bounds[i].value(), lower_bounds[i].value()));

        // Adjust coeff of the form k * divisor - epsilon.
        const IntegerValue remainder =
            CeilRatio(coeff, divisor) * divisor - coeff;
        if (CapProd(diff.value(), remainder.value()) > adjust_threshold) {
          continue;
        }
        temp_cut.ub += remainder * diff;
        temp_cut.coeffs[i] += remainder;
      }
    }

    // Create the super-additive function f().
    const IntegerValue rhs_remainder =
        temp_cut.ub - FloorRatio(temp_cut.ub, divisor) * divisor;
    if (rhs_remainder == 0) continue;

    const auto f = GetSuperAdditiveRoundingFunction(
        !options.use_mir, rhs_remainder, divisor, options.max_scaling);

    // Apply f() to the cut and compute the cut violation.
    temp_cut.ub = f(temp_cut.ub);
    double violation = -ToDouble(temp_cut.ub);
    double max_magnitude = 1.0;
    for (int i = 0; i < temp_cut.coeffs.size(); ++i) {
      const IntegerValue coeff = temp_cut.coeffs[i];
      if (coeff == 0) continue;
      const IntegerValue new_coeff = f(coeff);
      temp_cut.coeffs[i] = new_coeff;
      max_magnitude = std::max(max_magnitude, std::abs(ToDouble(new_coeff)));
      violation += ToDouble(new_coeff) * lp_values[i];
    }
    violation /= max_magnitude;

    if (violation > 0.0) {
      VLOG(2) << "lp_value: " << lp_values[i] << " divisor: " << divisor
              << " cut_violation: " << violation;
    }
    if (violation > best_scaled_violation) {
      best_scaled_violation = violation;
      best_cut = temp_cut;
    }
  }

  // Remove the bound shifts so the constraint is expressed in the original
  // variables and do some basic post-processing.
  *cut = best_cut;
  for (int i = 0; i < cut->coeffs.size(); ++i) {
    const IntegerValue coeff = cut->coeffs[i];
    if (coeff == 0) continue;
    cut->ub = IntegerValue(
        CapAdd((coeff * lower_bounds[i]).value(), cut->ub.value()));
    if (change_sign_at_postprocessing[i]) {
      cut->coeffs[i] = -coeff;
    }
  }
  RemoveZeroTerms(cut);
  DivideByGCD(cut);
}

}  // namespace sat
}  // namespace operations_research
