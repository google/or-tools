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

#include "ortools/math_opt/cpp/variable_and_expressions.h"

#include <initializer_list>
#include <limits>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/cpp/formatters.h"
#include "ortools/math_opt/storage/model_storage.h"
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
#include "ortools/math_opt/storage/model_storage_item.h"
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research {
namespace math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////

double Variable::lower_bound() const {
  return storage()->variable_lower_bound(typed_id());
}

double Variable::upper_bound() const {
  return storage()->variable_upper_bound(typed_id());
}

bool Variable::is_integer() const {
  return storage()->is_variable_integer(typed_id());
}

absl::string_view Variable::name() const {
  if (storage()->has_variable(typed_id())) {
    return storage()->variable_name(typed_id());
  }
  return "[variable deleted from model]";
}

LinearExpression Variable::operator-() const {
  return LinearExpression({LinearTerm(*this, -1.0)}, 0.0);
}

////////////////////////////////////////////////////////////////////////////////
// LinearTerm
////////////////////////////////////////////////////////////////////////////////

LinearTerm::LinearTerm(Variable variable, const double coefficient)
    : variable(std::move(variable)), coefficient(coefficient) {}

LinearTerm LinearTerm::operator-() const {
  return LinearTerm(variable, -coefficient);
}

LinearTerm& LinearTerm::operator*=(const double d) {
  coefficient *= d;
  return *this;
}

LinearTerm& LinearTerm::operator/=(const double d) {
  coefficient /= d;
  return *this;
}

LinearTerm operator*(const double coefficient, LinearTerm term) {
  term *= coefficient;
  return term;
}

LinearTerm operator*(LinearTerm term, const double coefficient) {
  term *= coefficient;
  return term;
}

LinearTerm operator*(const double coefficient, Variable variable) {
  return LinearTerm(std::move(variable), coefficient);
}

LinearTerm operator*(Variable variable, const double coefficient) {
  return LinearTerm(std::move(variable), coefficient);
}

LinearTerm operator/(LinearTerm term, const double coefficient) {
  term /= coefficient;
  return term;
}

LinearTerm operator/(Variable variable, const double coefficient) {
  return LinearTerm(std::move(variable), 1 / coefficient);
}

////////////////////////////////////////////////////////////////////////////////
// LinearExpression
////////////////////////////////////////////////////////////////////////////////

LinearExpression::LinearExpression(LinearExpression&& other) noexcept
    : ModelStorageItemContainer(
          static_cast<ModelStorageItemContainer&&>(other)),
      terms_(std::move(other.terms_)),
      offset_(std::exchange(other.offset_, 0.0)) {
  other.terms_.clear();
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_move_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
}

LinearExpression& LinearExpression::operator=(
    LinearExpression&& other) noexcept {
  ModelStorageItemContainer::operator=(
      static_cast<ModelStorageItemContainer&&>(other));
  terms_ = std::move(other.terms_);
  other.terms_.clear();
  offset_ = std::exchange(other.offset_, 0.0);
  return *this;
}

LinearExpression::LinearExpression(std::initializer_list<LinearTerm> terms,
                                   const double offset)
    : offset_(offset) {
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_initializer_list_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
  for (const auto& term : terms) {
    SetOrCheckStorage(term.variable);
    // The same variable may appear multiple times in the input list; we must
    // accumulate the coefficients.
    terms_[term.variable] += term.coefficient;
  }
}

LinearExpression::LinearExpression(double offset)
    : LinearExpression({}, offset) {}

LinearExpression::LinearExpression(Variable variable)
    : LinearExpression({LinearTerm(variable, 1.0)}, 0.0) {}

LinearExpression::LinearExpression(const LinearTerm& term)
    : LinearExpression({term}, 0.0) {}

LinearExpression operator-(LinearExpression expr) {
  expr.offset_ = -expr.offset_;
  for (auto& term : expr.terms_) {
    term.second = -term.second;
  }
  return expr;
}

LinearExpression operator+(const Variable lhs, const double rhs) {
  return LinearTerm(lhs, 1.0) + rhs;
}

LinearExpression operator+(const double lhs, const Variable rhs) {
  return lhs + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const LinearTerm& lhs, const double rhs) {
  return LinearExpression({lhs}, rhs);
}

LinearExpression operator+(const double lhs, const LinearTerm& rhs) {
  return LinearExpression({rhs}, lhs);
}

LinearExpression operator+(const LinearTerm& lhs, const Variable rhs) {
  return lhs + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) + rhs;
}

LinearExpression operator+(const LinearTerm& lhs, const LinearTerm& rhs) {
  return LinearExpression({lhs, rhs}, 0);
}

LinearExpression operator+(LinearExpression lhs, const double rhs) {
  lhs += rhs;
  return lhs;
}

LinearExpression operator+(const double lhs, LinearExpression rhs) {
  rhs += lhs;
  return rhs;
}

LinearExpression operator+(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) + std::move(rhs);
}

LinearExpression operator+(LinearExpression lhs, const LinearTerm& rhs) {
  lhs += rhs;
  return lhs;
}

LinearExpression operator+(LinearTerm lhs, LinearExpression rhs) {
  rhs += lhs;
  return rhs;
}

LinearExpression operator+(LinearExpression lhs, const LinearExpression& rhs) {
  lhs += rhs;
  return lhs;
}

LinearExpression operator-(const Variable lhs, const double rhs) {
  return LinearTerm(lhs, 1.0) - rhs;
}

LinearExpression operator-(const double lhs, const Variable rhs) {
  return lhs - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const LinearTerm& lhs, const double rhs) {
  return LinearExpression({lhs}, -rhs);
}

LinearExpression operator-(const double lhs, const LinearTerm& rhs) {
  return LinearExpression({-rhs}, lhs);
}

LinearExpression operator-(const LinearTerm& lhs, const Variable rhs) {
  return lhs - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) - rhs;
}

LinearExpression operator-(const LinearTerm& lhs, const LinearTerm& rhs) {
  return LinearExpression({lhs, -rhs}, 0);
}

LinearExpression operator-(LinearExpression lhs, const double rhs) {
  lhs -= rhs;
  return lhs;
}

LinearExpression operator-(const double lhs, LinearExpression rhs) {
  auto ret = -std::move(rhs);
  ret += lhs;
  return ret;
}

LinearExpression operator-(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) - std::move(rhs);
}

LinearExpression operator-(LinearExpression lhs, const LinearTerm& rhs) {
  lhs -= rhs;
  return lhs;
}

LinearExpression operator-(LinearTerm lhs, LinearExpression rhs) {
  auto ret = -std::move(rhs);
  ret += lhs;
  return ret;
}

LinearExpression operator-(LinearExpression lhs, const LinearExpression& rhs) {
  lhs -= rhs;
  return lhs;
}

LinearExpression operator*(LinearExpression lhs, const double rhs) {
  lhs *= rhs;
  return lhs;
}

LinearExpression operator*(const double lhs, LinearExpression rhs) {
  rhs *= lhs;
  return rhs;
}

LinearExpression operator/(LinearExpression lhs, const double rhs) {
  lhs /= rhs;
  return lhs;
}

LinearExpression& LinearExpression::operator+=(const LinearExpression& other) {
  // Here we know that each key in other.terms_ has already been checked and
  // thus we don't need to compare in the loop. Of course this only applies if
  // the other has terms.
  if (!other.terms_.empty()) {
    SetOrCheckStorage(other);
    for (const auto& [v, coeff] : other.terms_) {
      terms_[v] += coeff;
    }
  }
  offset_ += other.offset_;
  return *this;
}

LinearExpression& LinearExpression::operator+=(const LinearTerm& term) {
  SetOrCheckStorage(term.variable);
  terms_[term.variable] += term.coefficient;
  return *this;
}

LinearExpression& LinearExpression::operator+=(const Variable variable) {
  SetOrCheckStorage(variable);
  return *this += LinearTerm(variable, 1.0);
}

LinearExpression& LinearExpression::operator+=(const double value) {
  offset_ += value;
  return *this;
}

LinearExpression& LinearExpression::operator-=(const LinearExpression& other) {
  // See operator+=.
  if (!other.terms_.empty()) {
    SetOrCheckStorage(other);
    for (const auto& [v, coeff] : other.terms_) {
      terms_[v] -= coeff;
    }
  }
  offset_ -= other.offset_;
  return *this;
}

LinearExpression& LinearExpression::operator-=(const LinearTerm& term) {
  SetOrCheckStorage(term.variable);
  terms_[term.variable] -= term.coefficient;
  return *this;
}

LinearExpression& LinearExpression::operator-=(const Variable variable) {
  SetOrCheckStorage(variable);
  return *this -= LinearTerm(variable, 1.0);
}

LinearExpression& LinearExpression::operator-=(const double value) {
  offset_ -= value;
  return *this;
}

LinearExpression& LinearExpression::operator*=(const double value) {
  offset_ *= value;
  for (auto& term : terms_) {
    term.second *= value;
  }
  return *this;
}

LinearExpression& LinearExpression::operator/=(const double value) {
  offset_ /= value;
  for (auto& term : terms_) {
    term.second /= value;
  }
  return *this;
}

const VariableMap<double>& LinearExpression::terms() const { return terms_; }

double LinearExpression::offset() const { return offset_; }

////////////////////////////////////////////////////////////////////////////////
// VariablesEquality
////////////////////////////////////////////////////////////////////////////////

namespace internal {

VariablesEquality::VariablesEquality(Variable lhs, Variable rhs)
    : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

VariablesEquality::operator bool() const {
  return lhs.typed_id() == rhs.typed_id() && lhs.storage() == rhs.storage();
}

}  // namespace internal

internal::VariablesEquality operator==(const Variable& lhs,
                                       const Variable& rhs) {
  return internal::VariablesEquality(lhs, rhs);
}

bool operator!=(const Variable& lhs, const Variable& rhs) {
  return !(lhs == rhs);
}

/////////////////////////////////////////////////////////////////////////////////
// LowerBoundedLinearExpression
// UpperBoundedLinearExpression
// BoundedLinearExpression
////////////////////////////////////////////////////////////////////////////////

LowerBoundedLinearExpression::LowerBoundedLinearExpression(
    LinearExpression expression, const double lower_bound)
    : expression(std::move(expression)), lower_bound(lower_bound) {}

UpperBoundedLinearExpression::UpperBoundedLinearExpression(
    LinearExpression expression, const double upper_bound)
    : expression(std::move(expression)), upper_bound(upper_bound) {}

BoundedLinearExpression::BoundedLinearExpression(LinearExpression expression,
                                                 const double lower_bound,
                                                 const double upper_bound)
    : expression(std::move(expression)),
      lower_bound(lower_bound),
      upper_bound(upper_bound) {}

BoundedLinearExpression::BoundedLinearExpression(
    const internal::VariablesEquality& eq)
    : expression({{eq.lhs, 1.0}, {eq.rhs, -1.0}}, 0.0),
      lower_bound(0.0),
      upper_bound(0.0) {}

BoundedLinearExpression::BoundedLinearExpression(
    LowerBoundedLinearExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound),
      upper_bound(std::numeric_limits<double>::infinity()) {}

BoundedLinearExpression::BoundedLinearExpression(
    UpperBoundedLinearExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      lower_bound(-std::numeric_limits<double>::infinity()),
      upper_bound(ub_expression.upper_bound) {}

double BoundedLinearExpression::lower_bound_minus_offset() const {
  return lower_bound - expression.offset();
}

double BoundedLinearExpression::upper_bound_minus_offset() const {
  return upper_bound - expression.offset();
}

LowerBoundedLinearExpression operator>=(LinearExpression expression,
                                        const double constant) {
  return LowerBoundedLinearExpression(std::move(expression), constant);
}

LowerBoundedLinearExpression operator<=(const double constant,
                                        LinearExpression expression) {
  return LowerBoundedLinearExpression(std::move(expression), constant);
}

LowerBoundedLinearExpression operator>=(const LinearTerm& term,
                                        const double constant) {
  return LowerBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

LowerBoundedLinearExpression operator<=(const double constant,
                                        const LinearTerm& term) {
  return LowerBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

LowerBoundedLinearExpression operator>=(const Variable variable,
                                        const double constant) {
  return LinearTerm(variable, 1.0) >= constant;
}

LowerBoundedLinearExpression operator<=(const double constant,
                                        const Variable variable) {
  return constant <= LinearTerm(variable, 1.0);
}

UpperBoundedLinearExpression operator<=(LinearExpression expression,
                                        const double constant) {
  return UpperBoundedLinearExpression(std::move(expression), constant);
}

UpperBoundedLinearExpression operator>=(const double constant,
                                        LinearExpression expression) {
  return UpperBoundedLinearExpression(std::move(expression), constant);
}

UpperBoundedLinearExpression operator<=(const LinearTerm& term,
                                        const double constant) {
  return UpperBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

UpperBoundedLinearExpression operator>=(const double constant,
                                        const LinearTerm& term) {
  return UpperBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

UpperBoundedLinearExpression operator<=(const Variable variable,
                                        const double constant) {
  return LinearTerm(variable, 1.0) <= constant;
}

UpperBoundedLinearExpression operator>=(const double constant,
                                        const Variable variable) {
  return constant >= LinearTerm(variable, 1.0);
}

BoundedLinearExpression operator<=(LowerBoundedLinearExpression lhs,
                                   const double rhs) {
  return BoundedLinearExpression(std::move(lhs.expression),
                                 /*lower_bound=*/lhs.lower_bound,
                                 /*upper_bound=*/rhs);
}

BoundedLinearExpression operator>=(const double lhs,
                                   LowerBoundedLinearExpression rhs) {
  return BoundedLinearExpression(std::move(rhs.expression),
                                 /*lower_bound=*/rhs.lower_bound,
                                 /*upper_bound=*/lhs);
}

BoundedLinearExpression operator>=(UpperBoundedLinearExpression lhs,
                                   const double rhs) {
  return BoundedLinearExpression(std::move(lhs.expression),
                                 /*lower_bound=*/rhs,
                                 /*upper_bound=*/lhs.upper_bound);
}

BoundedLinearExpression operator<=(const double lhs,
                                   UpperBoundedLinearExpression rhs) {
  return BoundedLinearExpression(std::move(rhs.expression),
                                 /*lower_bound=*/lhs,
                                 /*upper_bound=*/rhs.upper_bound);
}

BoundedLinearExpression operator<=(LinearExpression lhs,
                                   const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator>=(LinearExpression lhs,
                                   const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator<=(LinearExpression lhs,
                                   const LinearTerm& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator>=(LinearExpression lhs,
                                   const LinearTerm& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator<=(const LinearTerm& lhs,
                                   LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(
      std::move(rhs), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator>=(const LinearTerm& lhs,
                                   LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(
      std::move(rhs), /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator<=(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) <= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator>=(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) >= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator<=(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) <= std::move(rhs);
}

BoundedLinearExpression operator>=(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) >= std::move(rhs);
}

BoundedLinearExpression operator<=(const LinearTerm& lhs,
                                   const LinearTerm& rhs) {
  return BoundedLinearExpression(
      LinearExpression({lhs, -rhs}, 0.0),
      /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator>=(const LinearTerm& lhs,
                                   const LinearTerm& rhs) {
  return BoundedLinearExpression(
      LinearExpression({lhs, -rhs}, 0.0), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator<=(const LinearTerm& lhs, const Variable rhs) {
  return lhs <= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator>=(const LinearTerm& lhs, const Variable rhs) {
  return lhs >= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator<=(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) <= rhs;
}

BoundedLinearExpression operator>=(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) >= rhs;
}

BoundedLinearExpression operator<=(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) <= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator>=(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) >= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator==(LinearExpression lhs,
                                   const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(std::move(lhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(LinearExpression lhs,
                                   const LinearTerm& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(std::move(lhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const LinearTerm& lhs,
                                   LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(std::move(rhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) == LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator==(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) == std::move(rhs);
}

BoundedLinearExpression operator==(LinearExpression lhs, const double rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(std::move(lhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const double lhs, LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(std::move(rhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const LinearTerm& lhs,
                                   const LinearTerm& rhs) {
  return BoundedLinearExpression(LinearExpression({lhs, -rhs}, 0.0),
                                 /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const LinearTerm& lhs, const Variable rhs) {
  return lhs == LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator==(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) == rhs;
}

BoundedLinearExpression operator==(const LinearTerm& lhs, const double rhs) {
  return BoundedLinearExpression(LinearExpression({lhs}, -rhs),
                                 /*lower_bound=*/0.0, /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const double lhs, const LinearTerm& rhs) {
  return BoundedLinearExpression(LinearExpression({rhs}, -lhs),
                                 /*lower_bound=*/0.0, /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const Variable lhs, const double rhs) {
  return LinearTerm(lhs, 1.0) == rhs;
}

BoundedLinearExpression operator==(const double lhs, const Variable rhs) {
  return lhs == LinearTerm(rhs, 1.0);
}

////////////////////////////////////////////////////////////////////////////////
// QuadraticTermKey
////////////////////////////////////////////////////////////////////////////////

QuadraticTermKey::QuadraticTermKey(const ModelStorageCPtr storage,
                                   const QuadraticProductId id)
    : ModelStorageItem(storage), variable_ids_(id) {
  if (variable_ids_.first > variable_ids_.second) {
    // See https://en.cppreference.com/w/cpp/named_req/Swappable for details.
    using std::swap;
    swap(variable_ids_.first, variable_ids_.second);
  }
}

QuadraticTermKey::QuadraticTermKey(const Variable first_variable,
                                   const Variable second_variable)
    : QuadraticTermKey(first_variable.storage(), {first_variable.typed_id(),
                                                  second_variable.typed_id()}) {
  CHECK_EQ(first_variable.storage(), second_variable.storage())
      << internal::kObjectsFromOtherModelStorage;
}

QuadraticProductId QuadraticTermKey::typed_id() const { return variable_ids_; }

std::ostream& operator<<(std::ostream& ostr, const QuadraticTermKey& key) {
  ostr << "(" << Variable(key.storage(), key.typed_id().first) << ", "
       << Variable(key.storage(), key.typed_id().second) << ")";
  return ostr;
}

bool operator==(const QuadraticTermKey lhs, const QuadraticTermKey rhs) {
  return lhs.storage() == rhs.storage() && lhs.typed_id() == rhs.typed_id();
}

bool operator!=(const QuadraticTermKey lhs, const QuadraticTermKey rhs) {
  return !(lhs == rhs);
}

////////////////////////////////////////////////////////////////////////////////
// QuadraticTerm (no arithmetic)
////////////////////////////////////////////////////////////////////////////////

QuadraticTerm::QuadraticTerm(Variable first_variable, Variable second_variable,
                             const double coefficient)
    : first_variable_(std::move(first_variable)),
      second_variable_(std::move(second_variable)),
      coefficient_(coefficient) {
  CHECK_EQ(first_variable_.storage(), second_variable_.storage())
      << internal::kObjectsFromOtherModelStorage;
}

double QuadraticTerm::coefficient() const { return coefficient_; }

Variable QuadraticTerm::first_variable() const { return first_variable_; }

Variable QuadraticTerm::second_variable() const { return second_variable_; }

QuadraticTermKey QuadraticTerm::GetKey() const {
  return QuadraticTermKey(
      first_variable_.storage(),
      std::make_pair(first_variable_.typed_id(), second_variable_.typed_id()));
}

////////////////////////////////////////////////////////////////////////////////
// QuadraticExpression (no arithmetic)
////////////////////////////////////////////////////////////////////////////////

QuadraticExpression::QuadraticExpression(QuadraticExpression&& other) noexcept
    : ModelStorageItemContainer(
          static_cast<ModelStorageItemContainer&&>(other)),
      quadratic_terms_(std::move(other.quadratic_terms_)),
      linear_terms_(std::move(other.linear_terms_)),
      offset_(std::exchange(other.offset_, 0.0)) {
  other.quadratic_terms_.clear();
  other.linear_terms_.clear();
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_move_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
}

QuadraticExpression& QuadraticExpression::operator=(
    QuadraticExpression&& other) noexcept {
  ModelStorageItemContainer::operator=(
      static_cast<ModelStorageItemContainer&&>(other));
  quadratic_terms_ = std::move(other.quadratic_terms_);
  other.quadratic_terms_.clear();
  linear_terms_ = std::move(other.linear_terms_);
  other.linear_terms_.clear();
  offset_ = std::exchange(other.offset_, 0.0);
  return *this;
}

QuadraticExpression::QuadraticExpression(
    const std::initializer_list<QuadraticTerm> quadratic_terms,
    const std::initializer_list<LinearTerm> linear_terms, const double offset)
    : offset_(offset) {
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_initializer_list_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
  for (const LinearTerm& term : linear_terms) {
    SetOrCheckStorage(term.variable);
    linear_terms_[term.variable] += term.coefficient;
  }
  for (const QuadraticTerm& term : quadratic_terms) {
    const QuadraticTermKey key = term.GetKey();
    SetOrCheckStorage(key);
    quadratic_terms_[key] += term.coefficient();
  }
}

QuadraticExpression::QuadraticExpression(const double offset)
    : QuadraticExpression({}, {}, offset) {}

QuadraticExpression::QuadraticExpression(const Variable variable)
    : QuadraticExpression({}, {LinearTerm(variable, 1.0)}, 0.0) {}

QuadraticExpression::QuadraticExpression(const LinearTerm& term)
    : QuadraticExpression({}, {term}, 0.0) {}

QuadraticExpression::QuadraticExpression(LinearExpression expr)
    : ModelStorageItemContainer(expr.storage()),
      linear_terms_(std::move(expr.terms_)),
      offset_(expr.offset_) {
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_linear_expression_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
}

QuadraticExpression::QuadraticExpression(const QuadraticTerm& term)
    : QuadraticExpression({term}, {}, 0.0) {}

double QuadraticExpression::offset() const { return offset_; }

const VariableMap<double>& QuadraticExpression::linear_terms() const {
  return linear_terms_;
}

const QuadraticTermMap<double>& QuadraticExpression::quadratic_terms() const {
  return quadratic_terms_;
}

////////////////////////////////////////////////////////////////////////////////
// Arithmetic operators (non-member).
//
// These are NOT required to explicitly CHECK that the underlying model storages
// agree between linear_terms_ and quadratic_terms_ unless they are a friend of
// QuadraticExpression. As much as possible, defer to the assignment operators
// and the initializer list constructor for QuadraticExpression.
////////////////////////////////////////////////////////////////////////////////

// ----------------------------- Addition (+) ----------------------------------

QuadraticExpression operator+(const double lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({rhs}, {}, lhs);
}

QuadraticExpression operator+(const double lhs, QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(const Variable lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({rhs}, {LinearTerm(lhs, 1.0)}, 0.0);
}

QuadraticExpression operator+(const Variable lhs, QuadraticExpression rhs) {
  rhs += LinearTerm(lhs, 1.0);
  return rhs;
}

QuadraticExpression operator+(const LinearTerm& lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({rhs}, {lhs}, 0.0);
}

QuadraticExpression operator+(const LinearTerm& lhs, QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(LinearExpression lhs, const QuadraticTerm& rhs) {
  QuadraticExpression expr(std::move(lhs));
  expr += rhs;
  return expr;
}

QuadraticExpression operator+(const LinearExpression& lhs,
                              QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(const QuadraticTerm& lhs, const double rhs) {
  return QuadraticExpression({lhs}, {}, rhs);
}

QuadraticExpression operator+(const QuadraticTerm& lhs, const Variable rhs) {
  return QuadraticExpression({lhs}, {LinearTerm(rhs, 1.0)}, 0.0);
}

QuadraticExpression operator+(const QuadraticTerm& lhs, const LinearTerm& rhs) {
  return QuadraticExpression({lhs}, {rhs}, 0.0);
}

QuadraticExpression operator+(const QuadraticTerm& lhs, LinearExpression rhs) {
  QuadraticExpression expr(std::move(rhs));
  expr += lhs;
  return expr;
}

QuadraticExpression operator+(const QuadraticTerm& lhs,
                              const QuadraticTerm& rhs) {
  return QuadraticExpression({lhs, rhs}, {}, 0.0);
}

QuadraticExpression operator+(const QuadraticTerm& lhs,
                              QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(QuadraticExpression lhs, const double rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs, const Variable rhs) {
  lhs += LinearTerm(rhs, 1.0);
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs, const LinearTerm& rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs,
                              const LinearExpression& rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs,
                              const QuadraticTerm& rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs,
                              const QuadraticExpression& rhs) {
  lhs += rhs;
  return lhs;
}

// --------------------------- Subtraction (-) ---------------------------------

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator-(QuadraticTerm term) {
  term.coefficient_ *= -1.0;
  return term;
}

// NOTE: A friend of QuadraticExpression, but does not touch variables
QuadraticExpression operator-(QuadraticExpression expr) {
  expr.offset_ = -expr.offset_;
  for (auto& term : expr.linear_terms_) {
    term.second = -term.second;
  }
  for (auto& term : expr.quadratic_terms_) {
    term.second = -term.second;
  }
  return expr;
}

QuadraticExpression operator-(const double lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({-rhs}, {}, lhs);
}

QuadraticExpression operator-(const double lhs, QuadraticExpression rhs) {
  auto expr = -std::move(rhs);
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(const Variable lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({-rhs}, {LinearTerm(lhs, 1.0)}, 0.0);
}

QuadraticExpression operator-(const Variable lhs, QuadraticExpression rhs) {
  return LinearTerm(lhs, 1.0) - std::move(rhs);
}

QuadraticExpression operator-(const LinearTerm& lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({-rhs}, {lhs}, 0.0);
}

QuadraticExpression operator-(const LinearTerm& lhs, QuadraticExpression rhs) {
  auto expr = -std::move(rhs);
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(LinearExpression lhs, const QuadraticTerm& rhs) {
  QuadraticExpression expr(std::move(lhs));
  expr -= rhs;
  return expr;
}

QuadraticExpression operator-(const LinearExpression& lhs,
                              QuadraticExpression rhs) {
  auto expr = -std::move(rhs);
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(const QuadraticTerm& lhs, const double rhs) {
  return QuadraticExpression({lhs}, {}, -rhs);
}

QuadraticExpression operator-(const QuadraticTerm& lhs, const Variable rhs) {
  return QuadraticExpression({lhs}, {LinearTerm(rhs, -1.0)}, 0.0);
}

QuadraticExpression operator-(const QuadraticTerm& lhs, const LinearTerm& rhs) {
  return QuadraticExpression({lhs}, {-rhs}, 0.0);
}

QuadraticExpression operator-(const QuadraticTerm& lhs, LinearExpression rhs) {
  QuadraticExpression expr(-std::move(rhs));
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(const QuadraticTerm& lhs,
                              const QuadraticTerm& rhs) {
  return QuadraticExpression({lhs, -rhs}, {}, 0.0);
}

QuadraticExpression operator-(const QuadraticTerm& lhs,
                              QuadraticExpression rhs) {
  rhs *= -1.0;
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator-(QuadraticExpression lhs, const double rhs) {
  lhs -= rhs;
  return lhs;
}

// NOTE: Out-of-order for compilation purposes
QuadraticExpression operator-(QuadraticExpression lhs, const LinearTerm& rhs) {
  lhs -= rhs;
  return lhs;
}

QuadraticExpression operator-(QuadraticExpression lhs, const Variable rhs) {
  lhs -= LinearTerm(rhs, 1.0);
  return lhs;
}

// NOTE: operator-(QuadraticExpression, const LinearTerm) appears above

QuadraticExpression operator-(QuadraticExpression lhs,
                              const LinearExpression& rhs) {
  lhs -= rhs;
  return lhs;
}

QuadraticExpression operator-(QuadraticExpression lhs,
                              const QuadraticTerm& rhs) {
  lhs -= rhs;
  return lhs;
}

QuadraticExpression operator-(QuadraticExpression lhs,
                              const QuadraticExpression& rhs) {
  lhs -= rhs;
  return lhs;
}

// ---------------------------- Multiplication (*) -----------------------------

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator*(const double lhs, QuadraticTerm rhs) {
  rhs.coefficient_ *= lhs;
  return rhs;
}

QuadraticExpression operator*(const double lhs, QuadraticExpression rhs) {
  rhs *= lhs;
  return rhs;
}

QuadraticTerm operator*(Variable lhs, Variable rhs) {
  return QuadraticTerm(std::move(lhs), std::move(rhs), 1.0);
}

QuadraticTerm operator*(Variable lhs, LinearTerm rhs) {
  return QuadraticTerm(std::move(lhs), std::move(rhs.variable),
                       rhs.coefficient);
}

QuadraticExpression operator*(Variable lhs, const LinearExpression& rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : rhs.terms()) {
    expr += QuadraticTerm(lhs, var, coeff);
  }
  if (rhs.offset() != 0) {
    expr += LinearTerm(std::move(lhs), rhs.offset());
  }
  return expr;
}

QuadraticTerm operator*(LinearTerm lhs, Variable rhs) {
  return QuadraticTerm(std::move(lhs.variable), std::move(rhs),
                       lhs.coefficient);
}

QuadraticTerm operator*(LinearTerm lhs, LinearTerm rhs) {
  return QuadraticTerm(std::move(lhs.variable), std::move(rhs.variable),
                       lhs.coefficient * rhs.coefficient);
}

QuadraticExpression operator*(LinearTerm lhs, const LinearExpression& rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : rhs.terms()) {
    expr += QuadraticTerm(lhs.variable, var, lhs.coefficient * coeff);
  }
  if (rhs.offset() != 0) {
    expr += LinearTerm(std::move(lhs.variable), lhs.coefficient * rhs.offset());
  }
  return expr;
}

QuadraticExpression operator*(const LinearExpression& lhs, Variable rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : lhs.terms()) {
    expr += QuadraticTerm(var, rhs, coeff);
  }
  if (lhs.offset() != 0) {
    expr += LinearTerm(std::move(rhs), lhs.offset());
  }
  return expr;
}

QuadraticExpression operator*(const LinearExpression& lhs, LinearTerm rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : lhs.terms()) {
    expr += QuadraticTerm(var, rhs.variable, coeff * rhs.coefficient);
  }
  if (lhs.offset() != 0) {
    expr += LinearTerm(std::move(rhs.variable), lhs.offset() * rhs.coefficient);
  }
  return expr;
}

QuadraticExpression operator*(const LinearExpression& lhs,
                              const LinearExpression& rhs) {
  QuadraticExpression expr = lhs.offset() * rhs.offset();
  if (rhs.offset() != 0) {
    for (const auto& [var, coeff] : lhs.terms()) {
      expr += LinearTerm(var, coeff * rhs.offset());
    }
  }
  if (lhs.offset() != 0) {
    for (const auto& [var, coeff] : rhs.terms()) {
      expr += LinearTerm(var, lhs.offset() * coeff);
    }
  }
  for (const auto& [lhs_var, lhs_coeff] : lhs.terms()) {
    for (const auto& [rhs_var, rhs_coeff] : rhs.terms()) {
      expr += QuadraticTerm(lhs_var, rhs_var, lhs_coeff * rhs_coeff);
    }
  }
  return expr;
}

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator*(QuadraticTerm lhs, const double rhs) {
  lhs.coefficient_ *= rhs;
  return lhs;
}

QuadraticExpression operator*(QuadraticExpression lhs, const double rhs) {
  lhs *= rhs;
  return lhs;
}

// ------------------------------- Division (/) --------------------------------

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator/(QuadraticTerm lhs, const double rhs) {
  lhs.coefficient_ /= rhs;
  return lhs;
}

QuadraticExpression operator/(QuadraticExpression lhs, const double rhs) {
  lhs /= rhs;
  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
// In-place arithmetic operators.
//
// These must guarantee that the underlying model storages for linear_terms_ and
// quadratic_terms_ agree upon exit of the function, using CheckModelsAgree(),
// the list initializer constructor for QuadraticExpression, or similar logic.
////////////////////////////////////////////////////////////////////////////////

QuadraticExpression& QuadraticExpression::operator+=(const double value) {
  offset_ += value;
  // NOTE: Not touching terms, no need to check models
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(const Variable variable) {
  SetOrCheckStorage(variable);
  linear_terms_[variable] += 1;
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(const LinearTerm& term) {
  SetOrCheckStorage(term.variable);
  linear_terms_[term.variable] += term.coefficient;
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(
    const LinearExpression& expr) {
  offset_ += expr.offset();
  // See comment in LinearExpression::operator+=.
  if (!expr.terms().empty()) {
    SetOrCheckStorage(expr);
    for (const auto& [v, coeff] : expr.terms()) {
      linear_terms_[v] += coeff;
    }
  }
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(
    const QuadraticTerm& term) {
  const QuadraticTermKey key = term.GetKey();
  SetOrCheckStorage(key);
  quadratic_terms_[key] += term.coefficient();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(
    const QuadraticExpression& expr) {
  offset_ += expr.offset();
  // See comment in LinearExpression::operator+=.
  if (!expr.linear_terms().empty() || !expr.quadratic_terms().empty()) {
    SetOrCheckStorage(expr);
    for (const auto& [v, coeff] : expr.linear_terms()) {
      linear_terms_[v] += coeff;
    }
    for (const auto& [k, coeff] : expr.quadratic_terms()) {
      quadratic_terms_[k] += coeff;
    }
  }
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(const double value) {
  offset_ -= value;
  // NOTE: Not touching terms, no need to check models
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(const Variable variable) {
  SetOrCheckStorage(variable);
  linear_terms_[variable] -= 1;
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(const LinearTerm& term) {
  SetOrCheckStorage(term.variable);
  linear_terms_[term.variable] -= term.coefficient;
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(
    const LinearExpression& expr) {
  offset_ -= expr.offset();
  // See comment in LinearExpression::operator+=.
  if (!expr.terms().empty()) {
    SetOrCheckStorage(expr);
    for (const auto& [v, coeff] : expr.terms()) {
      linear_terms_[v] -= coeff;
    }
  }
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(
    const QuadraticTerm& term) {
  const QuadraticTermKey key = term.GetKey();
  SetOrCheckStorage(key);
  quadratic_terms_[key] -= term.coefficient();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(
    const QuadraticExpression& expr) {
  offset_ -= expr.offset();
  // See comment in LinearExpression::operator+=.
  if (!expr.linear_terms().empty() || !expr.quadratic_terms().empty()) {
    SetOrCheckStorage(expr);
    for (const auto& [v, coeff] : expr.linear_terms()) {
      linear_terms_[v] -= coeff;
    }
    for (const auto& [k, coeff] : expr.quadratic_terms()) {
      quadratic_terms_[k] -= coeff;
    }
  }
  return *this;
}

QuadraticTerm& QuadraticTerm::operator*=(const double value) {
  coefficient_ *= value;
  // NOTE: Not touching variables in term, just modifying coefficient, so no
  // need to check that models agree.
  return *this;
}

QuadraticExpression& QuadraticExpression::operator*=(const double value) {
  offset_ *= value;
  for (auto& term : linear_terms_) {
    term.second *= value;
  }
  for (auto& term : quadratic_terms_) {
    term.second *= value;
  }
  // NOTE: Not adding/removing/altering variables in expression, just modifying
  // coefficients, so no need to check that models agree.
  return *this;
}

QuadraticTerm& QuadraticTerm::operator/=(const double value) {
  coefficient_ /= value;
  // NOTE: Not touching variables in term, just modifying coefficient, so no
  // need to check that models agree.
  return *this;
}

QuadraticExpression& QuadraticExpression::operator/=(const double value) {
  offset_ /= value;
  for (auto& term : linear_terms_) {
    term.second /= value;
  }
  for (auto& term : quadratic_terms_) {
    term.second /= value;
  }
  // NOTE: Not adding/removing/altering variables in expression, just modifying
  // coefficients, so no need to check that models agree.
  return *this;
}

/////////////////////////////////////////////////////////////////////////////////
// LowerBoundedQuadraticExpression
// UpperBoundedQuadraticExpression
// BoundedQuadraticExpression
////////////////////////////////////////////////////////////////////////////////

LowerBoundedQuadraticExpression::LowerBoundedQuadraticExpression(
    QuadraticExpression expression, const double lower_bound)
    : expression(std::move(expression)), lower_bound(lower_bound) {}

LowerBoundedQuadraticExpression::LowerBoundedQuadraticExpression(
    LowerBoundedLinearExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound) {}

UpperBoundedQuadraticExpression::UpperBoundedQuadraticExpression(
    QuadraticExpression expression, const double upper_bound)
    : expression(std::move(expression)), upper_bound(upper_bound) {}

UpperBoundedQuadraticExpression::UpperBoundedQuadraticExpression(
    UpperBoundedLinearExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      upper_bound(ub_expression.upper_bound) {}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    QuadraticExpression expression, const double lower_bound,
    const double upper_bound)
    : expression(std::move(expression)),
      lower_bound(lower_bound),
      upper_bound(upper_bound) {}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    internal::VariablesEquality var_equality)
    : lower_bound(0), upper_bound(0) {
  expression += var_equality.lhs;
  expression -= var_equality.rhs;
}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    LowerBoundedLinearExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound),
      upper_bound(std::numeric_limits<double>::infinity()) {}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    UpperBoundedLinearExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      lower_bound(-std::numeric_limits<double>::infinity()),
      upper_bound(ub_expression.upper_bound) {}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    BoundedLinearExpression bounded_expression)
    : expression(std::move(bounded_expression.expression)),
      lower_bound(bounded_expression.lower_bound),
      upper_bound(bounded_expression.upper_bound) {}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    LowerBoundedQuadraticExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound),
      upper_bound(std::numeric_limits<double>::infinity()) {}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    UpperBoundedQuadraticExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      lower_bound(-std::numeric_limits<double>::infinity()),
      upper_bound(ub_expression.upper_bound) {}

double BoundedQuadraticExpression::lower_bound_minus_offset() const {
  return lower_bound - expression.offset();
}

double BoundedQuadraticExpression::upper_bound_minus_offset() const {
  return upper_bound - expression.offset();
}

LowerBoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                           const double rhs) {
  return LowerBoundedQuadraticExpression(std::move(lhs), rhs);
}

LowerBoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                           const double rhs) {
  return LowerBoundedQuadraticExpression(lhs, rhs);
}

LowerBoundedQuadraticExpression operator<=(const double lhs,
                                           QuadraticExpression rhs) {
  return LowerBoundedQuadraticExpression(std::move(rhs), lhs);
}

LowerBoundedQuadraticExpression operator<=(const double lhs,
                                           const QuadraticTerm rhs) {
  return LowerBoundedQuadraticExpression(rhs, lhs);
}

UpperBoundedQuadraticExpression operator>=(const double lhs,
                                           QuadraticExpression rhs) {
  return UpperBoundedQuadraticExpression(std::move(rhs), lhs);
}

UpperBoundedQuadraticExpression operator>=(const double lhs,
                                           const QuadraticTerm rhs) {
  return UpperBoundedQuadraticExpression(rhs, lhs);
}

UpperBoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                           const double rhs) {
  return UpperBoundedQuadraticExpression(std::move(lhs), rhs);
}

UpperBoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                           const double rhs) {
  return UpperBoundedQuadraticExpression(lhs, rhs);
}

BoundedQuadraticExpression operator>=(UpperBoundedQuadraticExpression lhs,
                                      const double rhs) {
  return BoundedQuadraticExpression(std::move(lhs.expression), rhs,
                                    lhs.upper_bound);
}

BoundedQuadraticExpression operator>=(const double lhs,
                                      LowerBoundedQuadraticExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs.expression), rhs.lower_bound,
                                    lhs);
}

BoundedQuadraticExpression operator<=(LowerBoundedQuadraticExpression lhs,
                                      const double rhs) {
  return BoundedQuadraticExpression(std::move(lhs.expression), lhs.lower_bound,
                                    rhs);
}

BoundedQuadraticExpression operator<=(const double lhs,
                                      UpperBoundedQuadraticExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs.expression), lhs,
                                    rhs.upper_bound);
}

BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const QuadraticTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const LinearTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const Variable rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const QuadraticTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const LinearTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const Variable rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}

BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const QuadraticTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}

BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}

BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const LinearTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}

BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const Variable rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}

BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const double rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}

BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      LinearExpression rhs) {
  return BoundedQuadraticExpression(
      std::move(rhs) - lhs, -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      const LinearTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      const Variable rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      LinearExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs) - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      const LinearTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      const Variable rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}

BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      LinearExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs) - lhs, 0, 0);
}

BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const LinearTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const Variable rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const double rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

BoundedQuadraticExpression operator>=(const LinearExpression& lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator>=(LinearExpression lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(const LinearExpression& lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(LinearExpression lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator==(const LinearExpression& lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}

BoundedQuadraticExpression operator==(LinearExpression lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - std::move(lhs), 0, 0);
}

// LinearTerm --

BoundedQuadraticExpression operator>=(const LinearTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator>=(const LinearTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(const LinearTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(const LinearTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator==(const LinearTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}

BoundedQuadraticExpression operator==(const LinearTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

// Variable --

BoundedQuadraticExpression operator>=(const Variable lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator>=(const Variable lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}

BoundedQuadraticExpression operator<=(const Variable lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator<=(const Variable lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}

BoundedQuadraticExpression operator==(const Variable lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}

BoundedQuadraticExpression operator==(const Variable lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

// Double --
BoundedQuadraticExpression operator==(const double lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}

BoundedQuadraticExpression operator==(const double lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
LinearExpression::LinearExpression() { ++num_calls_default_constructor_; }

LinearExpression::LinearExpression(const LinearExpression& other)
    : ModelStorageItemContainer(other.storage()),
      terms_(other.terms_),
      offset_(other.offset_) {
  ++num_calls_copy_constructor_;
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
  double result = offset_;
  for (const auto& variable : SortedKeys(terms_)) {
    const auto found = variable_values.find(variable);
    CHECK(found != variable_values.end())
        << internal::kObjectsFromOtherModelStorage;
    result += terms_.at(variable) * found->second;
  }
  return result;
}

double LinearExpression::EvaluateWithDefaultZero(
    const VariableMap<double>& variable_values) const {
  double result = offset_;
  for (const auto& variable : SortedKeys(terms_)) {
    result +=
        terms_.at(variable) * gtl::FindWithDefault(variable_values, variable);
  }
  return result;
}

std::ostream& operator<<(std::ostream& ostr,
                         const LinearExpression& expression) {
  // TODO(b/169415597): improve linear expression format:
  //  - make sure to quote the variable name so that we support:
  //    * variable names contains +, -, ...
  //    * variable names resembling anonymous variable names.
  const std::vector<Variable> sorted_variables = SortedKeys(expression.terms_);
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
  const double lb = bounded_expression.lower_bound;
  const double ub = bounded_expression.upper_bound;
  if (lb == ub) {
    ostr << bounded_expression.expression << " = " << RoundTripDoubleFormat(lb);
  } else if (lb == -kInf) {
    ostr << bounded_expression.expression << " ≤ " << RoundTripDoubleFormat(ub);
  } else if (ub == kInf) {
    ostr << bounded_expression.expression << " ≥ " << RoundTripDoubleFormat(lb);
  } else {
    ostr << RoundTripDoubleFormat(lb) << " ≤ " << bounded_expression.expression
         << " ≤ " << RoundTripDoubleFormat(ub);
  }
  return ostr;
}

double QuadraticExpression::Evaluate(
    const VariableMap<double>& variable_values) const {
  double result = offset();
  for (const auto& variable : SortedKeys(linear_terms_)) {
    const auto found = variable_values.find(variable);
    CHECK(found != variable_values.end())
        << internal::kObjectsFromOtherModelStorage;
    result += linear_terms_.at(variable) * found->second;
  }
  for (const auto& variables : SortedKeys(quadratic_terms_)) {
    const auto found_first = variable_values.find(variables.first());
    CHECK(found_first != variable_values.end())
        << internal::kObjectsFromOtherModelStorage;
    const auto found_second = variable_values.find(variables.second());
    CHECK(found_second != variable_values.end())
        << internal::kObjectsFromOtherModelStorage;
    result += quadratic_terms_.at(variables) * found_first->second *
              found_second->second;
  }
  return result;
}

double QuadraticExpression::EvaluateWithDefaultZero(
    const VariableMap<double>& variable_values) const {
  double result = offset();
  for (const auto& variable : SortedKeys(linear_terms_)) {
    result += linear_terms_.at(variable) *
              gtl::FindWithDefault(variable_values, variable);
  }
  for (const auto& variables : SortedKeys(quadratic_terms_)) {
    result += quadratic_terms_.at(variables) *
              gtl::FindWithDefault(variable_values, variables.first()) *
              gtl::FindWithDefault(variable_values, variables.second());
  }
  return result;
}

std::ostream& operator<<(std::ostream& ostr, const QuadraticExpression& expr) {
  // TODO(b/169415597): improve quadratic expression formatting. See b/170991498
  // for desired improvements for LinearExpression streaming which are also
  // applicable here.
  bool first = true;
  for (const auto vs : SortedKeys(expr.quadratic_terms())) {
    const double coeff = expr.quadratic_terms().at(vs);
    if (coeff != 0) {
      ostr << LeadingCoefficientFormatter(coeff, first);
      first = false;
    }
    const Variable first_variable = vs.first();
    const Variable second_variable = vs.second();
    if (first_variable == second_variable) {
      ostr << first_variable << "²";
    } else {
      ostr << first_variable << "*" << second_variable;
    }
  }
  for (const auto v : SortedKeys(expr.linear_terms())) {
    const double coeff = expr.linear_terms().at(v);
    if (coeff != 0) {
      ostr << LeadingCoefficientFormatter(coeff, first) << v;
      first = false;
    }
  }
  ostr << ConstantFormatter(expr.offset(), first);
  return ostr;
}

std::ostream& operator<<(std::ostream& ostr,
                         const BoundedQuadraticExpression& bounded_expression) {
  const double lb = bounded_expression.lower_bound;
  const double ub = bounded_expression.upper_bound;
  if (lb == ub) {
    ostr << bounded_expression.expression << " = " << RoundTripDoubleFormat(lb);
  } else if (lb == -kInf) {
    ostr << bounded_expression.expression << " ≤ " << RoundTripDoubleFormat(ub);
  } else if (ub == kInf) {
    ostr << bounded_expression.expression << " ≥ " << RoundTripDoubleFormat(lb);
  } else {
    ostr << RoundTripDoubleFormat(lb) << " ≤ " << bounded_expression.expression
         << " ≤ " << RoundTripDoubleFormat(ub);
  }
  return ostr;
}

#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
QuadraticExpression::QuadraticExpression() { ++num_calls_default_constructor_; }

QuadraticExpression::QuadraticExpression(const QuadraticExpression& other)
    : ModelStorageItemContainer(other),
      quadratic_terms_(other.quadratic_terms_),
      linear_terms_(other.linear_terms_),
      offset_(other.offset_) {
  ++num_calls_copy_constructor_;
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
