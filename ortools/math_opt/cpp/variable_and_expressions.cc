// Copyright 2010-2021 Google LLC
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

#include "ortools/math_opt/cpp/variable_and_expressions.h"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_int.h"

namespace operations_research {
namespace math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
LinearExpression::LinearExpression() { ++num_calls_default_constructor_; }

LinearExpression::LinearExpression(const LinearExpression& other)
    : terms_(other.terms_), offset_(other.offset_) {
  ++num_calls_copy_constructor_;
}

LinearExpression::LinearExpression(LinearExpression&& other)
    : terms_(std::move(other.terms_)),
      offset_(std::exchange(other.offset_, 0.0)) {
  ++num_calls_move_constructor_;
}

LinearExpression& LinearExpression::operator=(const LinearExpression& other) {
  terms_ = other.terms_;
  offset_ = other.offset_;
  return *this;
}

ABSL_CONST_INIT thread_local int
    LinearExpression::num_calls_default_constructor_ = 0;
ABSL_CONST_INIT thread_local int LinearExpression::num_calls_copy_constructor_ =
    0;
ABSL_CONST_INIT thread_local int LinearExpression::num_calls_move_constructor_ =
    0;
ABSL_CONST_INIT thread_local int
    LinearExpression::num_calls_initializer_list_constructor_ = 0;

void LinearExpression::ResetCounters() {
  num_calls_default_constructor_ = 0;
  num_calls_copy_constructor_ = 0;
  num_calls_move_constructor_ = 0;
  num_calls_initializer_list_constructor_ = 0;
}
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS

double LinearExpression::Evaluate(
    const VariableMap<double>& variable_values) const {
  if (variable_values.storage() != nullptr && storage() != nullptr) {
    CHECK_EQ(variable_values.storage(), storage())
        << internal::kObjectsFromOtherModelStorage;
  }
  double result = offset_;
  for (const auto& [variable_id, coef] : terms_.raw_map()) {
    result += coef * variable_values.raw_map().at(variable_id);
  }
  return result;
}

double LinearExpression::EvaluateWithDefaultZero(
    const VariableMap<double>& variable_values) const {
  if (variable_values.storage() != nullptr && storage() != nullptr) {
    CHECK_EQ(variable_values.storage(), storage())
        << internal::kObjectsFromOtherModelStorage;
  }
  double result = offset_;
  for (const auto& [variable_id, coef] : terms_.raw_map()) {
    result +=
        coef * gtl::FindWithDefault(variable_values.raw_map(), variable_id);
  }
  return result;
}

namespace {

// Streaming formatter for a coefficient of a linear/quadratic term, along with
// any leading "+"/"-"'s to connect it with preceding terms in a sum, and
// potentially a "*" postfix. The `is_first` parameter specifies if the term is
// the first appearing in the sum, in which case the handling of the +/-
// connectors is different.
struct LeadingCoefficientFormatter {
  LeadingCoefficientFormatter(const double coeff, const bool is_first)
      : coeff(coeff), is_first(is_first) {}
  const double coeff;
  const bool is_first;
};

std::ostream& operator<<(std::ostream& out,
                         const LeadingCoefficientFormatter formatter) {
  const double coeff = formatter.coeff;
  if (formatter.is_first) {
    if (coeff == 1.0) {
      // Do nothing.
    } else if (coeff == -1.0) {
      out << "-";
    } else {
      out << coeff << "*";
    }
  } else {
    if (coeff == 1.0) {
      out << " + ";
    } else if (coeff == -1.0) {
      out << " - ";
    } else if (std::isnan(coeff)) {
      out << " + nan*";
    } else if (coeff >= 0) {
      out << " + " << coeff << "*";
    } else {
      out << " - " << -coeff << "*";
    }
  }
  return out;
}

// Streaming formatter for a constant in a linear/quadratic expression.
struct ConstantFormatter {
  ConstantFormatter(const double constant, const bool is_first)
      : constant(constant), is_first(is_first) {}
  const double constant;
  const bool is_first;
};

std::ostream& operator<<(std::ostream& out, const ConstantFormatter formatter) {
  const double constant = formatter.constant;
  if (formatter.is_first) {
    out << constant;
  } else if (constant == 0) {
    // Do nothing.
  } else if (std::isnan(constant)) {
    out << " + nan";
  } else if (constant > 0) {
    out << " + " << constant;
  } else {
    out << " - " << -constant;
  }
  return out;
}

}  // namespace

std::ostream& operator<<(std::ostream& ostr,
                         const LinearExpression& expression) {
  // TODO(b/169415597): improve linear expression format:
  //  - use bijective formatting in base10 of the double factors.
  //  - make sure to quote the variable name so that we support:
  //    * variable names contains +, -, ...
  //    * variable names resembling anonymous variable names.
  const std::vector<Variable> sorted_variables = expression.terms_.SortedKeys();
  bool first = true;
  for (const auto v : sorted_variables) {
    const double coeff = expression.terms_.at(v);
    if (coeff != 0) {
      ostr << LeadingCoefficientFormatter(coeff, first) << v;
      first = false;
    }
  }
  ostr << ConstantFormatter(expression.offset(), first);

  return ostr;
}

std::ostream& operator<<(std::ostream& ostr,
                         const BoundedLinearExpression& bounded_expression) {
  // TODO(b/170991498): use bijective conversion from double to base-10 string
  // to make sure we can reproduce bugs.
  const double lb = bounded_expression.lower_bound;
  const double ub = bounded_expression.upper_bound;
  if (lb == ub) {
    ostr << bounded_expression.expression << " = " << lb;
  } else if (lb == -kInf) {
    ostr << bounded_expression.expression << " ≤ " << ub;
  } else if (ub == kInf) {
    ostr << bounded_expression.expression << " ≥ " << lb;
  } else {
    ostr << lb << " ≤ " << bounded_expression.expression << " ≤ " << ub;
  }
  return ostr;
}

double QuadraticExpression::Evaluate(
    const VariableMap<double>& variable_values) const {
  if (variable_values.storage() != nullptr && storage() != nullptr) {
    CHECK_EQ(variable_values.storage(), storage())
        << internal::kObjectsFromOtherModelStorage;
  }
  double result = offset();
  for (const auto& [variable_id, coef] : linear_terms_.raw_map()) {
    result += coef * variable_values.raw_map().at(variable_id);
  }
  for (const auto& [variable_ids, coef] : quadratic_terms_.raw_map()) {
    result += coef * variable_values.raw_map().at(variable_ids.first) *
              variable_values.raw_map().at(variable_ids.second);
  }
  return result;
}

double QuadraticExpression::EvaluateWithDefaultZero(
    const VariableMap<double>& variable_values) const {
  if (variable_values.storage() != nullptr && storage() != nullptr) {
    CHECK_EQ(variable_values.storage(), storage())
        << internal::kObjectsFromOtherModelStorage;
  }
  double result = offset();
  for (const auto& [variable_id, coef] : linear_terms_.raw_map()) {
    result +=
        coef * gtl::FindWithDefault(variable_values.raw_map(), variable_id);
  }
  for (const auto& [variable_ids, coef] : quadratic_terms_.raw_map()) {
    result +=
        coef *
        gtl::FindWithDefault(variable_values.raw_map(), variable_ids.first) *
        gtl::FindWithDefault(variable_values.raw_map(), variable_ids.second);
  }
  return result;
}

std::ostream& operator<<(std::ostream& ostr, const QuadraticExpression& expr) {
  // TODO(b/169415597): improve quadratic expression formatting. See b/170991498
  // for desired improvements for LinearExpression streaming which are also
  // applicable here.
  bool first = true;
  for (const auto v : expr.quadratic_terms().SortedKeys()) {
    const double coeff = expr.quadratic_terms().at(v);
    if (coeff != 0) {
      ostr << LeadingCoefficientFormatter(coeff, first);
      first = false;
    }
    const Variable first_variable(expr.quadratic_terms().storage(),
                                  v.typed_id().first);
    const Variable second_variable(expr.quadratic_terms().storage(),
                                   v.typed_id().second);
    if (first_variable == second_variable) {
      ostr << first_variable << "²";
    } else {
      ostr << first_variable << "*" << second_variable;
    }
  }
  for (const auto v : expr.linear_terms().SortedKeys()) {
    const double coeff = expr.linear_terms().at(v);
    if (coeff != 0) {
      ostr << LeadingCoefficientFormatter(coeff, first) << v;
      first = false;
    }
  }
  ostr << ConstantFormatter(expr.offset(), first);
  return ostr;
}

#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
QuadraticExpression::QuadraticExpression() { ++num_calls_default_constructor_; }

QuadraticExpression::QuadraticExpression(const QuadraticExpression& other)
    : quadratic_terms_(other.quadratic_terms_),
      linear_terms_(other.linear_terms_),
      offset_(other.offset_) {
  ++num_calls_copy_constructor_;
}

QuadraticExpression::QuadraticExpression(QuadraticExpression&& other)
    : quadratic_terms_(std::move(other.quadratic_terms_)),
      linear_terms_(std::move(other.linear_terms_)),
      offset_(std::exchange(other.offset_, 0.0)) {
  ++num_calls_move_constructor_;
}

QuadraticExpression& QuadraticExpression::operator=(
    const QuadraticExpression& other) {
  quadratic_terms_ = other.quadratic_terms_;
  linear_terms_ = other.linear_terms_;
  offset_ = other.offset_;
  return *this;
}

ABSL_CONST_INIT thread_local int
    QuadraticExpression::num_calls_default_constructor_ = 0;
ABSL_CONST_INIT thread_local int
    QuadraticExpression::num_calls_copy_constructor_ = 0;
ABSL_CONST_INIT thread_local int
    QuadraticExpression::num_calls_move_constructor_ = 0;
ABSL_CONST_INIT thread_local int
    QuadraticExpression::num_calls_initializer_list_constructor_ = 0;
ABSL_CONST_INIT thread_local int
    QuadraticExpression::num_calls_linear_expression_constructor_ = 0;

void QuadraticExpression::ResetCounters() {
  num_calls_default_constructor_ = 0;
  num_calls_copy_constructor_ = 0;
  num_calls_move_constructor_ = 0;
  num_calls_initializer_list_constructor_ = 0;
  num_calls_linear_expression_constructor_ = 0;
}
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS

}  // namespace math_opt
}  // namespace operations_research
