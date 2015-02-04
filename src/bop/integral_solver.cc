// Copyright 2010-2014 Google
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

#include "bop/integral_solver.h"

#include <math.h>
#include <vector>

#include "bop/bop_solver.h"
#include "lp_data/lp_decomposer.h"

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

// Returns true when all the variables of the problem are boolean, and all the
// constraints have integer coefficients.
// TODO(user): Move to SAT util.
bool BooleanProblemWithIntegralConstraints(
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
    const LinearProgram& linear_problem,
    LinearBooleanProblem* boolean_problem) {
  CHECK(boolean_problem != nullptr);
  boolean_problem->Clear();

  const glop::SparseMatrix& matrix = linear_problem.GetSparseMatrix();
  // Create boolean variables.
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
}

//------------------------------------------------------------------------------
// IntegralVariable
//------------------------------------------------------------------------------
// Model an integral variable using boolean variables.
// TODO(user): Enable discrete representation by value, i.e. use three boolean
//              variables when only possible values are 10, 12, 32.
//              In the same way, when only two consecutive values are possible
//              use only one boolean variable with an offset.
class IntegralVariable {
 public:
  IntegralVariable();

  // Creates the minimal number of boolean variables to represent an integral
  // variable with range [lower_bound, upper_bound]. start_var_index corresponds
  // to the next available boolean variable index. If three boolean variables
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

  // Returns the value of the integral variable based on the boolean conversion
  // solution and the boolean solution to the problem.
  int64 GetSolutionValue(const BopSolution& solution) const;

  std::string DebugString() const;

 private:
  // The value of the integral variable is expressed as
  //   sum_i(weights[i] * Value(bits[i])) + offset.
  // Note that weights can be negative to represent negative values.
  std::vector<VariableIndex> bits_;
  std::vector<int64> weights_;
  int64 offset_;
};

IntegralVariable::IntegralVariable() : bits_(), weights_(), offset_(0) {}

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
}

void IntegralVariable::set_weight(VariableIndex var, int64 weight) {
  bits_.push_back(var);
  weights_.push_back(weight);
}

int64 IntegralVariable::GetSolutionValue(const BopSolution& solution) const {
  int64 value = offset_;
  for (int i = 0; i < bits_.size(); ++i) {
    value += weights_[i] * solution.Value(bits_[i]);
  }
  return value;
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
// The converter tries to reuse existing boolean variables as much as possible,
// but there are no guarantees to model all integral variables using the total
// minimal number of boolean variables.
// Consider for instance the constraint "x - 2 * y = 0".
// Depending on the declaration order, two different outcomes are possible:
//   - When x is considered first, the converter will generate new variables
//     for both x and y as we only consider integral weights, i.e. y = x / 2.
//   - When y is considered first, the converter will reuse boolean variables
//     from y to model x as x = 2 * y (integral weight).
//
// Note that the converter only deals with integral constraint and variable
// coefficients, and integral variables, i.e. no continuous variables.
class IntegralProblemConverter {
 public:
  IntegralProblemConverter();

  // Converts the LinearProgram into a LinearBooleanProblem.
  // Returns false when the conversion fails.
  bool ConvertToBooleanProblem(const LinearProgram& linear_problem,
                               LinearBooleanProblem* boolean_problem);

  // Returns the value of the integral variable based on the boolean conversion
  // and the boolean solution to the problem.
  int64 GetSolutionValue(ColIndex col, const BopSolution& solution) const;

 private:
  // Returns true when the linear_problem_ can be converted into a boolean
  // problem. Note that floating weigths and continuous variables are not
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

  // Converts all constraints from LinearProgram to boolean problem.
  void ConvertAllConstraints(const LinearProgram& linear_problem,
                             LinearBooleanProblem* boolean_problem);

  // Converts the objective from LinearProgram to boolean problem.
  void ConvertObjective(const LinearProgram& linear_problem,
                        LinearBooleanProblem* boolean_problem);

  // Converts the integral variable represented by col in the linear_problem_
  // into an IntegralVariable using existing boolean variables.
  // Returns false when existing boolean variables are not enough to model
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

  bool boolean_with_int_constraints_;

  // boolean_variables_[i] represents the boolean variable index in Bop; when
  // negative -boolean_variables_[i] - 1 represents the index of the
  // integral variable in integral_variables_.
  ITIVector<glop::ColIndex, int> boolean_variables_;
  std::vector<IntegralVariable> integral_variables_;
  std::vector<ColIndex> integral_indices_;
  int num_boolean_variables_;

  enum VariableType { BOOLEAN, INTEGRAL, INTEGRAL_EXPRESSED_AS_BOOLEAN };
  ITIVector<glop::ColIndex, VariableType> variable_types_;
};

IntegralProblemConverter::IntegralProblemConverter()
    : boolean_variables_(),
      integral_variables_(),
      integral_indices_(),
      num_boolean_variables_(0),
      variable_types_() {}

bool IntegralProblemConverter::ConvertToBooleanProblem(
    const LinearProgram& linear_problem,
    LinearBooleanProblem* boolean_problem) {
  if (!CheckProblem(linear_problem)) {
    return false;
  }

  boolean_with_int_constraints_ =
      BooleanProblemWithIntegralConstraints(linear_problem);
  if (boolean_with_int_constraints_) {
    BuildBooleanProblemWithIntegralConstraints(linear_problem, boolean_problem);
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
  return true;
}

int64 IntegralProblemConverter::GetSolutionValue(
    ColIndex col, const BopSolution& solution) const {
  if (boolean_with_int_constraints_) {
    return solution.Value(VariableIndex(col.value()));
  }

  const int pos = boolean_variables_[col];
  return pos >= 0 ? solution.Value(VariableIndex(pos))
                  : integral_variables_[-pos - 1].GetSolutionValue(solution);
}

bool IntegralProblemConverter::CheckProblem(
    const LinearProgram& linear_problem) const {
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    if (!linear_problem.is_variable_integer()[col]) {
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
  boolean_variables_.assign(linear_problem.num_variables().value(), 0);
  variable_types_.assign(linear_problem.num_variables().value(), INTEGRAL);
  for (ColIndex col(0); col < linear_problem.num_variables(); ++col) {
    const Fractional lower_bound = linear_problem.variable_lower_bounds()[col];
    const Fractional upper_bound = linear_problem.variable_upper_bounds()[col];

    if (lower_bound > -1.0 && upper_bound < 2.0) {
      // Boolean variable.
      variable_types_[col] = BOOLEAN;
      boolean_variables_[col] = num_boolean_variables_;
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
    boolean_variables_[col] = -integral_variables_.size();
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
      if (offset_lower_bound * scaling_factor >
          static_cast<double>(kint64max)) {
        LOG(WARNING) << "A constraint is trivially unsatisfiable.";
        return;
      }
      if (offset_lower_bound * scaling_factor >
          -static_cast<double>(kint64max)) {
        // Otherwise, the constraint is not needed.
        constraint->set_lower_bound(
            static_cast<int64>(
                round(offset_lower_bound * scaling_factor - bound_error)) /
            gcd);
      }
    }
    const Fractional upper_bound =
        linear_problem.constraint_upper_bounds()[row];
    if (upper_bound != kInfinity) {
      const Fractional offset_upper_bound = upper_bound - offset;
      if (offset_upper_bound * scaling_factor <
          -static_cast<double>(kint64max)) {
        LOG(WARNING) << "A constraint is trivially unsatisfiable.";
        return;
      }
      if (offset_upper_bound * scaling_factor <
          static_cast<double>(kint64max)) {
        // Otherwise, the constraint is not needed.
        constraint->set_upper_bound(
            static_cast<int64>(
                round(offset_upper_bound * scaling_factor + bound_error)) /
            gcd);
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
    const int pos = boolean_variables_[col];
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
      // To replace an integral variable by a weighted sum of boolean variables,
      // the constraint has to be an equality.
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
      const int pos = boolean_variables_[col];
      CHECK_LE(0, pos);
      dense_weights[VariableIndex(pos)] -= constraint_entry.coefficient();
    } else {
      CHECK_EQ(INTEGRAL_EXPRESSED_AS_BOOLEAN, variable_types_[col]);
      const int pos = boolean_variables_[col];
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
  const int pos = boolean_variables_[col];
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
}  // anonymous namespace

IntegralSolver::IntegralSolver()
    : parameters_(), variable_values_(), objective_value_(0.0) {}

// TODO(user): Parallelize the solve of sub-problems.
BopSolveStatus IntegralSolver::Solve(const LinearProgram& linear_problem) {
  if (linear_problem.num_variables() >=
      parameters_.decomposer_num_variables_threshold()) {
    LPDecomposer decomposer;
    decomposer.Decompose(&linear_problem);
    const int num_sub_problems = decomposer.GetNumberOfProblems();
    if (num_sub_problems > 1) {
      // The problem can be decomposed. Solve each sub-problem and aggregate the
      // result.
      BopSolveStatus status = BopSolveStatus::OPTIMAL_SOLUTION_FOUND;
      LinearProgram sub_problem;
      std::vector<DenseRow> local_variable_values(num_sub_problems);
      // TODO(user): Investigate a better approximation of the time needed to
      //              solve the problem than just the number of variables.
      const double total_num_variables =
          std::max(1.0, static_cast<double>(linear_problem.num_variables().value()));
      const double time_per_variable =
          parameters_.max_time_in_seconds() / total_num_variables;
      const double deterministic_time_per_variable =
          parameters_.max_deterministic_time() / total_num_variables;
      objective_value_ = linear_problem.objective_offset();
      best_bound_ = 0.0;
      for (int i = 0; i < num_sub_problems; ++i) {
        decomposer.BuildProblem(i, &sub_problem);
        Fractional local_objective_value = 0.0;
        Fractional local_best_bound = 0.0;
        BopParameters local_parameters = parameters_;
        const int num_variables = std::max(1, sub_problem.num_variables().value());
        local_parameters.set_max_time_in_seconds(time_per_variable *
                                                 num_variables);
        local_parameters.set_max_deterministic_time(
            deterministic_time_per_variable * num_variables);
        const BopSolveStatus local_status = InternalSolve(
            sub_problem, local_parameters, &(local_variable_values[i]),
            &local_objective_value, &local_best_bound);
        if (local_status == BopSolveStatus::NO_SOLUTION_FOUND ||
            local_status == BopSolveStatus::INFEASIBLE_PROBLEM ||
            local_status == BopSolveStatus::INVALID_PROBLEM) {
          return local_status;
        }
        if (local_status == BopSolveStatus::FEASIBLE_SOLUTION_FOUND) {
          status = BopSolveStatus::FEASIBLE_SOLUTION_FOUND;
        }
        objective_value_ += local_objective_value;
        best_bound_ += local_best_bound;
      }
      variable_values_ = decomposer.AggregateAssignments(local_variable_values);

      CheckSolution(linear_problem, variable_values_);
      return status;
    }
  }

  // The problem can't be decomposed, solve the original problem.
  return InternalSolve(linear_problem, parameters_, &variable_values_,
                       &objective_value_, &best_bound_);
}

BopSolveStatus IntegralSolver::InternalSolve(
    const LinearProgram& linear_problem, const BopParameters& parameters,
    DenseRow* variable_values, Fractional* objective_value,
    Fractional* best_bound) {
  CHECK(variable_values != nullptr);
  CHECK(objective_value != nullptr);
  CHECK(best_bound != nullptr);

  // Those values will only make sense when a solution is found, however we
  // resize here such that one can access the values even if they don't mean
  // anything.
  variable_values->resize(linear_problem.num_variables(), 0);

  LinearBooleanProblem boolean_problem;
  IntegralProblemConverter converter;
  if (!converter.ConvertToBooleanProblem(linear_problem, &boolean_problem)) {
    return BopSolveStatus::INVALID_PROBLEM;
  }

  BopSolver bop_solver(boolean_problem);
  bop_solver.SetParameters(parameters);
  const BopSolveStatus status = bop_solver.Solve();
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
}  // namespace bop
}  // namespace operations_research
