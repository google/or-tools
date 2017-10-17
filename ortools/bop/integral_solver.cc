// Copyright 2010-2017 Google
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

#include "ortools/bop/integral_solver.h"

#include <math.h>
#include <vector>
#include "ortools/bop/bop_solver.h"
#include "ortools/lp_data/lp_decomposer.h"

namespace operations_research {
namespace bop {

using operations_research::glop::ColIndex;
using operations_research::glop::DenseRow;
using operations_research::glop::Fractional;
using operations_research::glop::LinearProgram;
using operations_research::glop::LPDecomposer;
using operations_research::glop::SparseColumn;
using operations_research::glop::SparseMatrix;
using operations_research::glop::RowIndex;
using operations_research::glop::kInfinity;

namespace {
// TODO(user): Use an existing one or move it to util.
bool IsIntegerWithinTolerance(Fractional x) {
  const double kTolerance = 1e-10;
  return std::abs(x - round(x)) <= kTolerance;
}

// Returns true when all the variables of the problem are Boolean, and all the
// constraints have integer coefficients.
// TODO(user): Move to SAT util.
bool ProblemIsBooleanAndHasOnlyIntegralConstraints(
    const LinearProgram& linear_problem) {
  const glop::SparseMatrix& matrix = linear_problem.GetSparseMatrix();

  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    const Fractional lower_bound = linear_problem.variable_lower_bounds()[col];
    const Fractional upper_bound = linear_problem.variable_upper_bounds()[col];

    if (lower_bound <= -1.0 || upper_bound >= 2.0) {
      // Integral variable.
      return false;
    }

    for (const SparseColumn::Entry e : matrix.column(col)) {
      if (!IsIntegerWithinTolerance(e.coefficient())) {
        // Floating coefficient.
        return false;
      }
    }
  }
  return true;
}

// Builds a LinearBooleanProblem based on a LinearProgram with all the variables
// being booleans and all the constraints having only integral coefficients.
// TODO(user): Move to SAT util.
void BuildBooleanProblemWithIntegralConstraints(
    const LinearProgram& linear_problem, const DenseRow& initial_solution,
    LinearBooleanProblem* boolean_problem,
    std::vector<bool>* boolean_initial_solution) {
  CHECK(boolean_problem != nullptr);
  boolean_problem->Clear();

  const glop::SparseMatrix& matrix = linear_problem.GetSparseMatrix();
  // Create Boolean variables.
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    boolean_problem->add_var_names(linear_problem.GetVariableName(col));
  }
  boolean_problem->set_num_variables(matrix.num_cols().value());
  boolean_problem->set_name(linear_problem.name());

  // Create constraints.
  for (RowIndex row(0); row < matrix.num_rows(); ++row) {
    LinearBooleanConstraint* const constraint =
        boolean_problem->add_constraints();
    constraint->set_name(linear_problem.GetConstraintName(row));
    if (linear_problem.constraint_lower_bounds()[row] != -kInfinity) {
      constraint->set_lower_bound(
          linear_problem.constraint_lower_bounds()[row]);
    }
    if (linear_problem.constraint_upper_bounds()[row] != kInfinity) {
      constraint->set_upper_bound(
          linear_problem.constraint_upper_bounds()[row]);
    }
  }

  // Store the constraint coefficients.
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    for (const SparseColumn::Entry e : matrix.column(col)) {
      LinearBooleanConstraint* const constraint =
          boolean_problem->mutable_constraints(e.row().value());
      constraint->add_literals(col.value() + 1);
      constraint->add_coefficients(e.coefficient());
    }
  }

  // Add the unit constraints to fix the variables since the variable bounds
  // are always [0, 1] in a BooleanLinearProblem.
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    // TODO(user): double check the rounding, and add unit test for this.
    const int lb = std::round(linear_problem.variable_lower_bounds()[col]);
    const int ub = std::round(linear_problem.variable_upper_bounds()[col]);
    if (lb == ub) {
      LinearBooleanConstraint* ct = boolean_problem->add_constraints();
      ct->set_lower_bound(ub);
      ct->set_upper_bound(ub);
      ct->add_literals(col.value() + 1);
      ct->add_coefficients(1.0);
    }
  }

  // Create the minimization objective.
  std::vector<double> coefficients;
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    const Fractional coeff = linear_problem.objective_coefficients()[col];
    if (coeff != 0.0) coefficients.push_back(coeff);
  }
  double scaling_factor = 0.0;
  double relative_error = 0.0;
  GetBestScalingOfDoublesToInt64(coefficients, kint64max, &scaling_factor,
                                 &relative_error);
  const int64 gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
  LinearObjective* const objective = boolean_problem->mutable_objective();
  objective->set_offset(linear_problem.objective_offset() * scaling_factor /
                        gcd);

  // Note that here we set the scaling factor for the inverse operation of
  // getting the "true" objective value from the scaled one. Hence the inverse.
  objective->set_scaling_factor(1.0 / scaling_factor * gcd);
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    const Fractional coeff = linear_problem.objective_coefficients()[col];
    const int64 value = static_cast<int64>(round(coeff * scaling_factor)) / gcd;
    if (value != 0) {
      objective->add_literals(col.value() + 1);
      objective->add_coefficients(value);
    }
  }

  // If the problem was a maximization one, we need to modify the objective.
  if (linear_problem.IsMaximizationProblem()) {
    sat::ChangeOptimizationDirection(boolean_problem);
  }

  // Fill the Boolean initial solution.
  if (!initial_solution.empty()) {
    CHECK(boolean_initial_solution != nullptr);
    CHECK_EQ(boolean_problem->num_variables(), initial_solution.size());
    boolean_initial_solution->assign(boolean_problem->num_variables(), false);
    for (int i = 0; i < initial_solution.size(); ++i) {
      (*boolean_initial_solution)[i] = (initial_solution[ColIndex(i)] != 0);
    }
  }
}

//------------------------------------------------------------------------------
// IntegralVariable
//------------------------------------------------------------------------------
// Model an integral variable using Boolean variables.
// TODO(user): Enable discrete representation by value, i.e. use three Boolean
//              variables when only possible values are 10, 12, 32.
//              In the same way, when only two consecutive values are possible
//              use only one Boolean variable with an offset.
class IntegralVariable {
 public:
  IntegralVariable();

  // Creates the minimal number of Boolean variables to represent an integral
  // variable with range [lower_bound, upper_bound]. start_var_index corresponds
  // to the next available Boolean variable index. If three Boolean variables
  // are needed to model the integral variable, the used variables will have
  // indices start_var_index, start_var_index +1, and start_var_index +2.
  void BuildFromRange(int start_var_index, Fractional lower_bound,
                      Fractional upper_bound);

  void Clear();
  void set_offset(int64 offset) { offset_ = offset; }
  void set_weight(VariableIndex var, int64 weight);

  int GetNumberOfBooleanVariables() const { return bits_.size(); }

  const std::vector<VariableIndex>& bits() const { return bits_; }
  const std::vector<int64>& weights() const { return weights_; }
  int64 offset() const { return offset_; }

  // Returns the value of the integral variable based on the Boolean conversion
  // and the Boolean solution to the problem.
  int64 GetSolutionValue(const BopSolution& solution) const;

  // Returns the values of the Boolean variables based on the Boolean conversion
  // and the integral value of this variable. This only works for variables that
  // were constructed using BuildFromRange() (for which can_be_reversed_ is
  // true).
  std::vector<bool> GetBooleanSolutionValues(int64 integral_value) const;

  std::string DebugString() const;

 private:
  // The value of the integral variable is expressed as
  //   sum_i(weights[i] * Value(bits[i])) + offset.
  // Note that weights can be negative to represent negative values.
  std::vector<VariableIndex> bits_;
  std::vector<int64> weights_;
  int64 offset_;
  // True if the values of the boolean variables representing this integral
  // variable can be deduced from the integral variable's value. Namely, this is
  // true for variables built using BuildFromRange() but usually false for
  // variables built using set_weight().
  bool can_be_reversed_;
};

IntegralVariable::IntegralVariable()
    : bits_(), weights_(), offset_(0), can_be_reversed_(true) {}

void IntegralVariable::BuildFromRange(int start_var_index,
                                      Fractional lower_bound,
                                      Fractional upper_bound) {
  Clear();

  // Integral variable. Split the variable into the minimum number of bits
  // required to model the upper bound.
  CHECK_NE(-kInfinity, lower_bound);
  CHECK_NE(kInfinity, upper_bound);

  const int64 integral_lower_bound = static_cast<int64>(ceil(lower_bound));
  const int64 integral_upper_bound = static_cast<int64>(floor(upper_bound));
  offset_ = integral_lower_bound;
  const int64 delta = integral_upper_bound - integral_lower_bound;
  const int num_used_bits = MostSignificantBitPosition64(delta) + 1;
  for (int i = 0; i < num_used_bits; ++i) {
    bits_.push_back(VariableIndex(start_var_index + i));
    weights_.push_back(1ULL << i);
  }
}

void IntegralVariable::Clear() {
  bits_.clear();
  weights_.clear();
  offset_ = 0;
  can_be_reversed_ = true;
}

void IntegralVariable::set_weight(VariableIndex var, int64 weight) {
  bits_.push_back(var);
  weights_.push_back(weight);
  can_be_reversed_ = false;
}

int64 IntegralVariable::GetSolutionValue(const BopSolution& solution) const {
  int64 value = offset_;
  for (int i = 0; i < bits_.size(); ++i) {
    value += weights_[i] * solution.Value(bits_[i]);
  }
  return value;
}

std::vector<bool> IntegralVariable::GetBooleanSolutionValues(
    int64 integral_value) const {
  if (can_be_reversed_) {
    DCHECK(std::is_sorted(weights_.begin(), weights_.end()));
    std::vector<bool> boolean_values(weights_.size(), false);
    int64 remaining_value = integral_value - offset_;
    for (int i = weights_.size() - 1; i >= 0; --i) {
      if (remaining_value >= weights_[i]) {
        boolean_values[i] = true;
        remaining_value -= weights_[i];
      }
    }
    CHECK_EQ(0, remaining_value)
        << "Couldn't map integral value to boolean variables.";
    return boolean_values;
  }
  return std::vector<bool>();
}

std::string IntegralVariable::DebugString() const {
  std::string str;
  CHECK_EQ(bits_.size(), weights_.size());
  for (int i = 0; i < bits_.size(); ++i) {
    str += StringPrintf("%lld [%d] ", weights_[i], bits_[i].value());
  }
  str += StringPrintf(" Offset: %lld", offset_);
  return str;
}

//------------------------------------------------------------------------------
// IntegralProblemConverter
//------------------------------------------------------------------------------
// This class is used to convert a LinearProblem containing integral variables
// into a LinearBooleanProblem that Bop can consume.
// The converter tries to reuse existing Boolean variables as much as possible,
// but there are no guarantees to model all integral variables using the total
// minimal number of Boolean variables.
// Consider for instance the constraint "x - 2 * y = 0".
// Depending on the declaration order, two different outcomes are possible:
//   - When x is considered first, the converter will generate new variables
//     for both x and y as we only consider integral weights, i.e. y = x / 2.
//   - When y is considered first, the converter will reuse Boolean variables
//     from y to model x as x = 2 * y (integral weight).
//
// Note that the converter only deals with integral variables, i.e. no
// continuous variables.
class IntegralProblemConverter {
 public:
  IntegralProblemConverter();

  // Converts the LinearProgram into a LinearBooleanProblem. If an initial
  // solution is given (i.e. if its size is not zero), converts it into a
  // Boolean solution.
  // Returns false when the conversion fails.
  bool ConvertToBooleanProblem(const LinearProgram& linear_problem,
                               const DenseRow& initial_solution,
                               LinearBooleanProblem* boolean_problem,
                               std::vector<bool>* boolean_initial_solution);

  // Returns the value of a variable of the original problem based on the
  // Boolean conversion and the Boolean solution to the problem.
  int64 GetSolutionValue(ColIndex global_col,
                         const BopSolution& solution) const;

 private:
  // Returns true when the linear_problem_ can be converted into a Boolean
  // problem. Note that floating weights and continuous variables are not
  // supported.
  bool CheckProblem(const LinearProgram& linear_problem) const;

  // Initializes the type of each variable of the linear_problem_.
  void InitVariableTypes(const LinearProgram& linear_problem,
                         LinearBooleanProblem* boolean_problem);

  // Converts all variables of the problem.
  void ConvertAllVariables(const LinearProgram& linear_problem,
                           LinearBooleanProblem* boolean_problem);

  // Adds all variables constraints, i.e. lower and upper bounds of variables.
  void AddVariableConstraints(const LinearProgram& linear_problem,
                              LinearBooleanProblem* boolean_problem);

  // Converts all constraints from LinearProgram to LinearBooleanProblem.
  void ConvertAllConstraints(const LinearProgram& linear_problem,
                             LinearBooleanProblem* boolean_problem);

  // Converts the objective from LinearProgram to LinearBooleanProblem.
  void ConvertObjective(const LinearProgram& linear_problem,
                        LinearBooleanProblem* boolean_problem);

  // Converts the integral variable represented by col in the linear_problem_
  // into an IntegralVariable using existing Boolean variables.
  // Returns false when existing Boolean variables are not enough to model
  // the integral variable.
  bool ConvertUsingExistingBooleans(const LinearProgram& linear_problem,
                                    ColIndex col,
                                    IntegralVariable* integral_var);

  // Creates the integral_var using the given linear_problem_ constraint.
  // The constraint is an equality constraint and contains only one integral
  // variable (already the case in the model or thanks to previous
  // booleanization of other integral variables), i.e.
  //    bound <= w * integral_var + sum(w_i * b_i) <= bound
  // The remaining integral variable can then be expressed:
  //    integral_var == (bound + sum(-w_i * b_i)) / w
  // Note that all divisions by w have to be integral as Bop only deals with
  // integral coefficients.
  bool CreateVariableUsingConstraint(const LinearProgram& linear_problem,
                                     RowIndex constraint,
                                     IntegralVariable* integral_var);

  // Adds weighted integral variable represented by col to the current dense
  // constraint.
  Fractional AddWeightedIntegralVariable(
      ColIndex col, Fractional weight,
      ITIVector<VariableIndex, Fractional>* dense_weights);

  // Scales weights and adds all non-zero scaled weights and literals to t.
  // t is a constraint or the objective.
  // Returns the bound error due to the scaling.
  // The weight is scaled using:
  //   static_cast<int64>(round(weight * scaling_factor)) / gcd;
  template <class T>
  double ScaleAndSparsifyWeights(
      double scaling_factor, int64 gcd,
      const ITIVector<VariableIndex, Fractional>& dense_weights, T* t);

  // Returns true when at least one element is non-zero.
  bool HasNonZeroWeigths(
      const ITIVector<VariableIndex, Fractional>& dense_weights) const;

  bool problem_is_boolean_and_has_only_integral_constraints_;

  // global_to_boolean_[i] represents the Boolean variable index in Bop; when
  // negative -global_to_boolean_[i] - 1 represents the index of the
  // integral variable in integral_variables_.
  ITIVector</*global_col*/ glop::ColIndex, /*boolean_col*/ int>
      global_to_boolean_;
  std::vector<IntegralVariable> integral_variables_;
  std::vector<ColIndex> integral_indices_;
  int num_boolean_variables_;

  enum VariableType { BOOLEAN, INTEGRAL, INTEGRAL_EXPRESSED_AS_BOOLEAN };
  ITIVector<glop::ColIndex, VariableType> variable_types_;
};

IntegralProblemConverter::IntegralProblemConverter()
    : global_to_boolean_(),
      integral_variables_(),
      integral_indices_(),
      num_boolean_variables_(0),
      variable_types_() {}

bool IntegralProblemConverter::ConvertToBooleanProblem(
    const LinearProgram& linear_problem, const DenseRow& initial_solution,
    LinearBooleanProblem* boolean_problem,
    std::vector<bool>* boolean_initial_solution) {
  bool use_initial_solution = (initial_solution.size() > 0);
  if (use_initial_solution) {
    CHECK_EQ(initial_solution.size(), linear_problem.num_variables())
        << "The initial solution should have the same number of variables as "
           "the LinearProgram.";
    CHECK(boolean_initial_solution != nullptr);
  }
  if (!CheckProblem(linear_problem)) {
    return false;
  }

  problem_is_boolean_and_has_only_integral_constraints_ =
      ProblemIsBooleanAndHasOnlyIntegralConstraints(linear_problem);
  if (problem_is_boolean_and_has_only_integral_constraints_) {
    BuildBooleanProblemWithIntegralConstraints(linear_problem, initial_solution,
                                               boolean_problem,
                                               boolean_initial_solution);
    return true;
  }

  InitVariableTypes(linear_problem, boolean_problem);
  ConvertAllVariables(linear_problem, boolean_problem);
  boolean_problem->set_num_variables(num_boolean_variables_);
  boolean_problem->set_name(linear_problem.name());

  AddVariableConstraints(linear_problem, boolean_problem);
  ConvertAllConstraints(linear_problem, boolean_problem);
  ConvertObjective(linear_problem, boolean_problem);

  // A BooleanLinearProblem is always in the minimization form.
  if (linear_problem.IsMaximizationProblem()) {
    sat::ChangeOptimizationDirection(boolean_problem);
  }

  if (use_initial_solution) {
    boolean_initial_solution->assign(boolean_problem->num_variables(), false);
    for (ColIndex global_col(0); global_col < global_to_boolean_.size();
         ++global_col) {
      const int col = global_to_boolean_[global_col];
      if (col >= 0) {
        (*boolean_initial_solution)[col] = (initial_solution[global_col] != 0);
      } else {
        const IntegralVariable& integral_variable =
            integral_variables_[-col - 1];
        const std::vector<VariableIndex>& boolean_cols =
            integral_variable.bits();
        const std::vector<bool>& boolean_values =
            integral_variable.GetBooleanSolutionValues(
                round(initial_solution[global_col]));
        if (!boolean_values.empty()) {
          CHECK_EQ(boolean_cols.size(), boolean_values.size());
          for (int i = 0; i < boolean_values.size(); ++i) {
            const int boolean_col = boolean_cols[i].value();
            (*boolean_initial_solution)[boolean_col] = boolean_values[i];
          }
        }
      }
    }
  }

  return true;
}

int64 IntegralProblemConverter::GetSolutionValue(
    ColIndex global_col, const BopSolution& solution) const {
  if (problem_is_boolean_and_has_only_integral_constraints_) {
    return solution.Value(VariableIndex(global_col.value()));
  }

  const int pos = global_to_boolean_[global_col];
  return pos >= 0 ? solution.Value(VariableIndex(pos))
                  : integral_variables_[-pos - 1].GetSolutionValue(solution);
}

bool IntegralProblemConverter::CheckProblem(
    const LinearProgram& linear_problem) const {
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    if (!linear_problem.IsVariableInteger(col)) {
      LOG(ERROR) << "Variable " << linear_problem.GetVariableName(col)
                 << " is continuous. This is not supported by BOP.";
      return false;
    }
    if (linear_problem.variable_lower_bounds()[col] == -kInfinity) {
      LOG(ERROR) << "Variable " << linear_problem.GetVariableName(col)
                 << " has no lower bound. This is not supported by BOP.";
      return false;
    }
    if (linear_problem.variable_upper_bounds()[col] == kInfinity) {
      LOG(ERROR) << "Variable " << linear_problem.GetVariableName(col)
                 << " has no upper bound. This is not supported by BOP.";
      return false;
    }
  }
  return true;
}

void IntegralProblemConverter::InitVariableTypes(
    const LinearProgram& linear_problem,
    LinearBooleanProblem* boolean_problem) {
  global_to_boolean_.assign(linear_problem.num_variables().value(), 0);
  variable_types_.assign(linear_problem.num_variables().value(), INTEGRAL);
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    const Fractional lower_bound = linear_problem.variable_lower_bounds()[col];
    const Fractional upper_bound = linear_problem.variable_upper_bounds()[col];

    if (lower_bound > -1.0 && upper_bound < 2.0) {
      // Boolean variable.
      variable_types_[col] = BOOLEAN;
      global_to_boolean_[col] = num_boolean_variables_;
      ++num_boolean_variables_;
      boolean_problem->add_var_names(linear_problem.GetVariableName(col));
    } else {
      // Integral variable.
      variable_types_[col] = INTEGRAL;
      integral_indices_.push_back(col);
    }
  }
}

void IntegralProblemConverter::ConvertAllVariables(
    const LinearProgram& linear_problem,
    LinearBooleanProblem* boolean_problem) {
  for (const ColIndex col : integral_indices_) {
    CHECK_EQ(INTEGRAL, variable_types_[col]);
    IntegralVariable integral_var;
    if (!ConvertUsingExistingBooleans(linear_problem, col, &integral_var)) {
      const Fractional lower_bound =
          linear_problem.variable_lower_bounds()[col];
      const Fractional upper_bound =
          linear_problem.variable_upper_bounds()[col];
      integral_var.BuildFromRange(num_boolean_variables_, lower_bound,
                                  upper_bound);
      num_boolean_variables_ += integral_var.GetNumberOfBooleanVariables();
      const std::string var_name = linear_problem.GetVariableName(col);
      for (int i = 0; i < integral_var.bits().size(); ++i) {
        boolean_problem->add_var_names(var_name + StringPrintf("_%d", i));
      }
    }
    integral_variables_.push_back(integral_var);
    global_to_boolean_[col] = -integral_variables_.size();
    variable_types_[col] = INTEGRAL_EXPRESSED_AS_BOOLEAN;
  }
}

void IntegralProblemConverter::ConvertAllConstraints(
    const LinearProgram& linear_problem,
    LinearBooleanProblem* boolean_problem) {
  // TODO(user): This is the way it's done in glop/proto_utils.cc but having
  //              to transpose looks unnecessary costly.
  glop::SparseMatrix transpose;
  transpose.PopulateFromTranspose(linear_problem.GetSparseMatrix());

  double max_relative_error = 0.0;
  double max_bound_error = 0.0;
  double max_scaling_factor = 0.0;
  double relative_error = 0.0;
  double scaling_factor = 0.0;
  std::vector<double> coefficients;
  for (RowIndex row(0); row < linear_problem.num_constraints(); ++row) {
    Fractional offset = 0.0;
    ITIVector<VariableIndex, Fractional> dense_weights(num_boolean_variables_,
                                                       0.0);
    for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
      // Cast in ColIndex due to the transpose.
      offset += AddWeightedIntegralVariable(RowToColIndex(e.row()),
                                            e.coefficient(), &dense_weights);
    }
    if (!HasNonZeroWeigths(dense_weights)) {
      continue;
    }

    // Compute the scaling for non-integral weights.
    coefficients.clear();
    for (VariableIndex var(0); var < num_boolean_variables_; ++var) {
      if (dense_weights[var] != 0.0) {
        coefficients.push_back(dense_weights[var]);
      }
    }
    GetBestScalingOfDoublesToInt64(coefficients, kint64max, &scaling_factor,
                                   &relative_error);
    const int64 gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
    max_relative_error = std::max(relative_error, max_relative_error);
    max_scaling_factor = std::max(scaling_factor / gcd, max_scaling_factor);

    LinearBooleanConstraint* constraint = boolean_problem->add_constraints();
    constraint->set_name(linear_problem.GetConstraintName(row));
    const double bound_error =
        ScaleAndSparsifyWeights(scaling_factor, gcd, dense_weights, constraint);
    max_bound_error = std::max(max_bound_error, bound_error);

    const Fractional lower_bound =
        linear_problem.constraint_lower_bounds()[row];
    if (lower_bound != -kInfinity) {
      const Fractional offset_lower_bound = lower_bound - offset;
      const double offset_scaled_lower_bound =
          round(offset_lower_bound * scaling_factor - bound_error);
      if (offset_scaled_lower_bound >= static_cast<double>(kint64max)) {
        LOG(WARNING) << "A constraint is trivially unsatisfiable.";
        return;
      }
      if (offset_scaled_lower_bound > -static_cast<double>(kint64max)) {
        // Otherwise, the constraint is not needed.
        constraint->set_lower_bound(
            static_cast<int64>(offset_scaled_lower_bound) / gcd);
      }
    }
    const Fractional upper_bound =
        linear_problem.constraint_upper_bounds()[row];
    if (upper_bound != kInfinity) {
      const Fractional offset_upper_bound = upper_bound - offset;
      const double offset_scaled_upper_bound =
          round(offset_upper_bound * scaling_factor + bound_error);
      if (offset_scaled_upper_bound <= -static_cast<double>(kint64max)) {
        LOG(WARNING) << "A constraint is trivially unsatisfiable.";
        return;
      }
      if (offset_scaled_upper_bound < static_cast<double>(kint64max)) {
        // Otherwise, the constraint is not needed.
        constraint->set_upper_bound(
            static_cast<int64>(offset_scaled_upper_bound) / gcd);
      }
    }
  }
}

void IntegralProblemConverter::ConvertObjective(
    const LinearProgram& linear_problem,
    LinearBooleanProblem* boolean_problem) {
  LinearObjective* objective = boolean_problem->mutable_objective();
  Fractional offset = 0.0;
  ITIVector<VariableIndex, Fractional> dense_weights(num_boolean_variables_,
                                                     0.0);
  // Compute the objective weights for the binary variable model.
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    offset += AddWeightedIntegralVariable(
        col, linear_problem.objective_coefficients()[col], &dense_weights);
  }

  // Compute the scaling for non-integral weights.
  std::vector<double> coefficients;
  for (VariableIndex var(0); var < num_boolean_variables_; ++var) {
    if (dense_weights[var] != 0.0) {
      coefficients.push_back(dense_weights[var]);
    }
  }
  double scaling_factor = 0.0;
  double max_relative_error = 0.0;
  double relative_error = 0.0;
  GetBestScalingOfDoublesToInt64(coefficients, kint64max, &scaling_factor,
                                 &relative_error);
  const int64 gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
  max_relative_error = std::max(relative_error, max_relative_error);
  VLOG(1) << "objective relative error: " << relative_error;
  VLOG(1) << "objective scaling factor: " << scaling_factor / gcd;

  ScaleAndSparsifyWeights(scaling_factor, gcd, dense_weights, objective);

  // Note that here we set the scaling factor for the inverse operation of
  // getting the "true" objective value from the scaled one. Hence the inverse.
  objective->set_scaling_factor(1.0 / scaling_factor * gcd);
  objective->set_offset((linear_problem.objective_offset() + offset) *
                        scaling_factor / gcd);
}

void IntegralProblemConverter::AddVariableConstraints(
    const LinearProgram& linear_problem,
    LinearBooleanProblem* boolean_problem) {
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    const Fractional lower_bound = linear_problem.variable_lower_bounds()[col];
    const Fractional upper_bound = linear_problem.variable_upper_bounds()[col];
    const int pos = global_to_boolean_[col];
    if (pos >= 0) {
      // Boolean variable.
      CHECK_EQ(BOOLEAN, variable_types_[col]);
      const bool is_fixed = (lower_bound > -1.0 && upper_bound < 1.0) ||
                            (lower_bound > 0.0 && upper_bound < 2.0);
      if (is_fixed) {
        // Set the variable.
        const int fixed_value = lower_bound > -1.0 && upper_bound < 1.0 ? 0 : 1;
        LinearBooleanConstraint* constraint =
            boolean_problem->add_constraints();
        constraint->set_lower_bound(fixed_value);
        constraint->set_upper_bound(fixed_value);
        constraint->add_literals(pos + 1);
        constraint->add_coefficients(1);
      }
    } else {
      CHECK_EQ(INTEGRAL_EXPRESSED_AS_BOOLEAN, variable_types_[col]);
      // Integral variable.
      if (lower_bound != -kInfinity || upper_bound != kInfinity) {
        const IntegralVariable& integral_var = integral_variables_[-pos - 1];
        LinearBooleanConstraint* constraint =
            boolean_problem->add_constraints();
        for (int i = 0; i < integral_var.bits().size(); ++i) {
          constraint->add_literals(integral_var.bits()[i].value() + 1);
          constraint->add_coefficients(integral_var.weights()[i]);
        }
        if (lower_bound != -kInfinity) {
          constraint->set_lower_bound(static_cast<int64>(ceil(lower_bound)) -
                                      integral_var.offset());
        }
        if (upper_bound != kInfinity) {
          constraint->set_upper_bound(static_cast<int64>(floor(upper_bound)) -
                                      integral_var.offset());
        }
      }
    }
  }
}

bool IntegralProblemConverter::ConvertUsingExistingBooleans(
    const LinearProgram& linear_problem, ColIndex col,
    IntegralVariable* integral_var) {
  CHECK(nullptr != integral_var);
  CHECK_EQ(INTEGRAL, variable_types_[col]);

  const SparseMatrix& matrix = linear_problem.GetSparseMatrix();
  const SparseMatrix& transpose = linear_problem.GetTransposeSparseMatrix();
  for (const SparseColumn::Entry var_entry : matrix.column(col)) {
    const RowIndex constraint = var_entry.row();
    const Fractional lb = linear_problem.constraint_lower_bounds()[constraint];
    const Fractional ub = linear_problem.constraint_upper_bounds()[constraint];
    if (lb != ub) {
      // To replace an integral variable by a weighted sum of Boolean variables,
      // the constraint has to be an equality.
      continue;
    }

    if (transpose.column(RowToColIndex(constraint)).num_entries() <= 1) {
      // Can't replace the integer variable by Boolean variables when there are
      // no Boolean variables.
      // TODO(user): We could actually simplify the problem when the variable
      //              is constant, but this should be done by the preprocessor,
      //              not here. Consider activating the MIP preprocessing.
      continue;
    }

    bool only_one_integral_variable = true;
    for (const SparseColumn::Entry constraint_entry :
         transpose.column(RowToColIndex(constraint))) {
      const ColIndex var_index = RowToColIndex(constraint_entry.row());
      if (var_index != col && variable_types_[var_index] == INTEGRAL) {
        only_one_integral_variable = false;
        break;
      }
    }
    if (only_one_integral_variable &&
        CreateVariableUsingConstraint(linear_problem, constraint,
                                      integral_var)) {
      return true;
    }
  }

  integral_var->Clear();
  return false;
}

bool IntegralProblemConverter::CreateVariableUsingConstraint(
    const LinearProgram& linear_problem, RowIndex constraint,
    IntegralVariable* integral_var) {
  CHECK(nullptr != integral_var);
  integral_var->Clear();

  const SparseMatrix& transpose = linear_problem.GetTransposeSparseMatrix();
  ITIVector<VariableIndex, Fractional> dense_weights(num_boolean_variables_,
                                                     0.0);
  Fractional scale = 1.0;
  int64 variable_offset = 0;
  for (const SparseColumn::Entry constraint_entry :
       transpose.column(RowToColIndex(constraint))) {
    const ColIndex col = RowToColIndex(constraint_entry.row());
    if (variable_types_[col] == INTEGRAL) {
      scale = constraint_entry.coefficient();
    } else if (variable_types_[col] == BOOLEAN) {
      const int pos = global_to_boolean_[col];
      CHECK_LE(0, pos);
      dense_weights[VariableIndex(pos)] -= constraint_entry.coefficient();
    } else {
      CHECK_EQ(INTEGRAL_EXPRESSED_AS_BOOLEAN, variable_types_[col]);
      const int pos = global_to_boolean_[col];
      CHECK_GT(0, pos);
      const IntegralVariable& local_integral_var =
          integral_variables_[-pos - 1];
      variable_offset -=
          constraint_entry.coefficient() * local_integral_var.offset();
      for (int i = 0; i < local_integral_var.bits().size(); ++i) {
        dense_weights[local_integral_var.bits()[i]] -=
            constraint_entry.coefficient() * local_integral_var.weights()[i];
      }
    }
  }

  // Rescale using the weight of the integral variable.
  const Fractional lb = linear_problem.constraint_lower_bounds()[constraint];
  const Fractional offset = (lb + variable_offset) / scale;
  if (!IsIntegerWithinTolerance(offset)) {
    return false;
  }
  integral_var->set_offset(static_cast<int64>(offset));

  for (VariableIndex var(0); var < dense_weights.size(); ++var) {
    if (dense_weights[var] != 0.0) {
      const Fractional weight = dense_weights[var] / scale;
      if (!IsIntegerWithinTolerance(weight)) {
        return false;
      }
      integral_var->set_weight(var, static_cast<int64>(weight));
    }
  }

  return true;
}

Fractional IntegralProblemConverter::AddWeightedIntegralVariable(
    ColIndex col, Fractional weight,
    ITIVector<VariableIndex, Fractional>* dense_weights) {
  CHECK(nullptr != dense_weights);

  if (weight == 0.0) {
    return 0;
  }

  Fractional offset = 0;
  const int pos = global_to_boolean_[col];
  if (pos >= 0) {
    // Boolean variable.
    (*dense_weights)[VariableIndex(pos)] += weight;
  } else {
    // Integral variable.
    const IntegralVariable& integral_var = integral_variables_[-pos - 1];
    for (int i = 0; i < integral_var.bits().size(); ++i) {
      (*dense_weights)[integral_var.bits()[i]] +=
          integral_var.weights()[i] * weight;
    }
    offset += weight * integral_var.offset();
  }
  return offset;
}

template <class T>
double IntegralProblemConverter::ScaleAndSparsifyWeights(
    double scaling_factor, int64 gcd,
    const ITIVector<VariableIndex, Fractional>& dense_weights, T* t) {
  CHECK(nullptr != t);

  double bound_error = 0.0;
  for (VariableIndex var(0); var < dense_weights.size(); ++var) {
    if (dense_weights[var] != 0.0) {
      const double scaled_weight = dense_weights[var] * scaling_factor;
      bound_error += fabs(round(scaled_weight) - scaled_weight);
      t->add_literals(var.value() + 1);
      t->add_coefficients(static_cast<int64>(round(scaled_weight)) / gcd);
    }
  }

  return bound_error;
}
bool IntegralProblemConverter::HasNonZeroWeigths(
    const ITIVector<VariableIndex, Fractional>& dense_weights) const {
  for (const Fractional weight : dense_weights) {
    if (weight != 0.0) {
      return true;
    }
  }
  return false;
}

bool CheckSolution(const LinearProgram& linear_problem,
                   const glop::DenseRow& variable_values) {
  glop::DenseColumn constraint_values(linear_problem.num_constraints(), 0);

  const SparseMatrix& matrix = linear_problem.GetSparseMatrix();
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    const Fractional lower_bound = linear_problem.variable_lower_bounds()[col];
    const Fractional upper_bound = linear_problem.variable_upper_bounds()[col];
    const Fractional value = variable_values[col];
    if (lower_bound > value || upper_bound < value) {
      LOG(ERROR) << "Variable " << col << " out of bound: " << value
                 << "  should be in " << lower_bound << " .. " << upper_bound;
      return false;
    }

    for (const SparseColumn::Entry entry : matrix.column(col)) {
      constraint_values[entry.row()] += entry.coefficient() * value;
    }
  }

  for (RowIndex row(0); row < linear_problem.num_constraints(); ++row) {
    const Fractional lb = linear_problem.constraint_lower_bounds()[row];
    const Fractional ub = linear_problem.constraint_upper_bounds()[row];
    const Fractional value = constraint_values[row];
    if (lb > value || ub < value) {
      LOG(ERROR) << "Constraint " << row << " out of bound: " << value
                 << "  should be in " << lb << " .. " << ub;
      return false;
    }
  }

  return true;
}

// Solves the given linear program and returns the solve status.
BopSolveStatus InternalSolve(const LinearProgram& linear_problem,
                             const BopParameters& parameters,
                             const DenseRow& initial_solution,
                             TimeLimit* time_limit, DenseRow* variable_values,
                             Fractional* objective_value,
                             Fractional* best_bound) {
  CHECK(variable_values != nullptr);
  CHECK(objective_value != nullptr);
  CHECK(best_bound != nullptr);
  const bool use_initial_solution = (initial_solution.size() > 0);
  if (use_initial_solution) {
    CHECK_EQ(initial_solution.size(), linear_problem.num_variables());
  }

  // Those values will only make sense when a solution is found, however we
  // resize here such that one can access the values even if they don't mean
  // anything.
  variable_values->resize(linear_problem.num_variables(), 0);

  LinearBooleanProblem boolean_problem;
  std::vector<bool> boolean_initial_solution;
  IntegralProblemConverter converter;
  if (!converter.ConvertToBooleanProblem(linear_problem, initial_solution,
                                         &boolean_problem,
                                         &boolean_initial_solution)) {
    return BopSolveStatus::INVALID_PROBLEM;
  }

  BopSolver bop_solver(boolean_problem);
  bop_solver.SetParameters(parameters);
  BopSolveStatus status = BopSolveStatus::NO_SOLUTION_FOUND;
  if (use_initial_solution) {
    BopSolution bop_solution(boolean_problem, "InitialSolution");
    CHECK_EQ(boolean_initial_solution.size(), boolean_problem.num_variables());
    for (int i = 0; i < boolean_initial_solution.size(); ++i) {
      bop_solution.SetValue(VariableIndex(i), boolean_initial_solution[i]);
    }
    status = bop_solver.SolveWithTimeLimit(bop_solution, time_limit);
  } else {
    status = bop_solver.SolveWithTimeLimit(time_limit);
  }
  if (status == BopSolveStatus::OPTIMAL_SOLUTION_FOUND ||
      status == BopSolveStatus::FEASIBLE_SOLUTION_FOUND) {
    // Compute objective value.
    const BopSolution& solution = bop_solver.best_solution();
    CHECK(solution.IsFeasible());

    *objective_value = linear_problem.objective_offset();
    for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
      const int64 value = converter.GetSolutionValue(col, solution);
      (*variable_values)[col] = value;
      *objective_value += value * linear_problem.objective_coefficients()[col];
    }

    CheckSolution(linear_problem, *variable_values);

    // TODO(user): Check that the scaled best bound from Bop is a valid one
    //              even after conversion. If yes, remove the optimality test.
    *best_bound = status == BopSolveStatus::OPTIMAL_SOLUTION_FOUND
                      ? *objective_value
                      : bop_solver.GetScaledBestBound();
  }
  return status;
}

void RunOneBop(const BopParameters& parameters, int problem_index,
               const DenseRow& initial_solution, TimeLimit* time_limit,
               LPDecomposer* decomposer, DenseRow* variable_values,
               Fractional* objective_value, Fractional* best_bound,
               BopSolveStatus* status) {
  CHECK(decomposer != nullptr);
  CHECK(variable_values != nullptr);
  CHECK(objective_value != nullptr);
  CHECK(best_bound != nullptr);
  CHECK(status != nullptr);

  LinearProgram problem;
  decomposer->ExtractLocalProblem(problem_index, &problem);
  DenseRow local_initial_solution;
  if (initial_solution.size() > 0) {
    local_initial_solution =
        decomposer->ExtractLocalAssignment(problem_index, initial_solution);
  }
  // TODO(user): Investigate a better approximation of the time needed to
  //              solve the problem than just the number of variables.
  const double total_num_variables = std::max(
      1.0, static_cast<double>(
               decomposer->original_problem().num_variables().value()));
  const double time_per_variable =
      parameters.max_time_in_seconds() / total_num_variables;
  const double deterministic_time_per_variable =
      parameters.max_deterministic_time() / total_num_variables;
  const int local_num_variables = std::max(1, problem.num_variables().value());

  NestedTimeLimit subproblem_time_limit(
      time_limit, std::max(time_per_variable * local_num_variables,
                           parameters.decomposed_problem_min_time_in_seconds()),
      deterministic_time_per_variable * local_num_variables);

  *status = InternalSolve(problem, parameters, local_initial_solution,
                          subproblem_time_limit.GetTimeLimit(), variable_values,
                          objective_value, best_bound);
}
}  // anonymous namespace

IntegralSolver::IntegralSolver()
    : parameters_(), variable_values_(), objective_value_(0.0) {}

BopSolveStatus IntegralSolver::Solve(const LinearProgram& linear_problem) {
  return Solve(linear_problem, DenseRow());
}

BopSolveStatus IntegralSolver::SolveWithTimeLimit(
    const LinearProgram& linear_problem, TimeLimit* time_limit) {
  return SolveWithTimeLimit(linear_problem, DenseRow(), time_limit);
}

BopSolveStatus IntegralSolver::Solve(
    const LinearProgram& linear_problem,
    const DenseRow& user_provided_intial_solution) {
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(parameters_);
  return SolveWithTimeLimit(linear_problem, user_provided_intial_solution,
                            time_limit.get());
}

BopSolveStatus IntegralSolver::SolveWithTimeLimit(
    const LinearProgram& linear_problem,
    const DenseRow& user_provided_intial_solution, TimeLimit* time_limit) {
  // We make a copy so that we can clear it if the presolve is active.
  DenseRow initial_solution = user_provided_intial_solution;
  if (initial_solution.size() > 0) {
    CHECK_EQ(initial_solution.size(), linear_problem.num_variables())
        << "The initial solution should have the same number of variables as "
           "the LinearProgram.";
  }

  // Some code path requires to copy the given linear_problem. When this
  // happens, we will simply change the target of this pointer.
  LinearProgram const* lp = &linear_problem;


  BopSolveStatus status;
  if (lp->num_variables() >= parameters_.decomposer_num_variables_threshold()) {
    LPDecomposer decomposer;
    decomposer.Decompose(lp);
    const int num_sub_problems = decomposer.GetNumberOfProblems();
    if (num_sub_problems > 1) {
      // The problem can be decomposed. Solve each sub-problem and aggregate the
      // result.
      std::vector<DenseRow> variable_values(num_sub_problems);
      std::vector<Fractional> objective_values(num_sub_problems,
                                               Fractional(0.0));
      std::vector<Fractional> best_bounds(num_sub_problems, Fractional(0.0));
      std::vector<BopSolveStatus> statuses(num_sub_problems,
                                      BopSolveStatus::INVALID_PROBLEM);

        for (int i = 0; i < num_sub_problems; ++i) {
          RunOneBop(parameters_, i, initial_solution, time_limit, &decomposer,
                    &(variable_values[i]), &(objective_values[i]),
                    &(best_bounds[i]), &(statuses[i]));
        }

      // Aggregate results.
      status = BopSolveStatus::OPTIMAL_SOLUTION_FOUND;
      objective_value_ = lp->objective_offset();
      best_bound_ = 0.0;
      for (int i = 0; i < num_sub_problems; ++i) {
        objective_value_ += objective_values[i];
        best_bound_ += best_bounds[i];
        if (statuses[i] == BopSolveStatus::NO_SOLUTION_FOUND ||
            statuses[i] == BopSolveStatus::INFEASIBLE_PROBLEM ||
            statuses[i] == BopSolveStatus::INVALID_PROBLEM) {
          return statuses[i];
        }

        if (statuses[i] == BopSolveStatus::FEASIBLE_SOLUTION_FOUND) {
          status = BopSolveStatus::FEASIBLE_SOLUTION_FOUND;
        }
      }
      variable_values_ = decomposer.AggregateAssignments(variable_values);
      CheckSolution(*lp, variable_values_);
    } else {
      status =
          InternalSolve(*lp, parameters_, initial_solution, time_limit,
                        &variable_values_, &objective_value_, &best_bound_);
    }
  } else {
    status = InternalSolve(*lp, parameters_, initial_solution, time_limit,
                           &variable_values_, &objective_value_, &best_bound_);
  }

  return status;
}

}  // namespace bop
}  // namespace operations_research
