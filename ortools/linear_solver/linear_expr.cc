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

#include "ortools/linear_solver/linear_expr.h"

#include <limits>

#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {

LinearExpr::LinearExpr(double constant) : offset_(constant), terms_() {}

LinearExpr::LinearExpr() : LinearExpr(0.0) {}

LinearExpr::LinearExpr(const MPVariable* var) : LinearExpr(0.0) {
  terms_[var] = 1.0;
}

LinearExpr& LinearExpr::operator+=(const LinearExpr& rhs) {
  for (const auto& kv : rhs.terms_) {
    terms_[kv.first] += kv.second;
  }
  offset_ += rhs.offset_;
  return *this;
}

LinearExpr& LinearExpr::operator-=(const LinearExpr& rhs) {
  for (const auto& kv : rhs.terms_) {
    terms_[kv.first] -= kv.second;
  }
  offset_ -= rhs.offset_;
  return *this;
}

LinearExpr& LinearExpr::operator*=(double rhs) {
  if (rhs == 0) {
    terms_.clear();
    offset_ = 0;
  } else if (rhs != 1) {
    for (auto& kv : terms_) {
      kv.second *= rhs;
    }
    offset_ *= rhs;
  }
  return *this;
}

LinearExpr& LinearExpr::operator/=(double rhs) {
  DCHECK_NE(rhs, 0);
  return (*this) *= 1 / rhs;
}

LinearExpr LinearExpr::operator-() const { return (*this) * -1; }

// static
LinearExpr LinearExpr::NotVar(LinearExpr var) {
  var *= -1;
  var += 1;
  return var;
}

double LinearExpr::SolutionValue() const {
  double solution = offset_;
  for (const auto& pair : terms_) {
    solution += pair.first->solution_value() * pair.second;
  }
  return solution;
}

LinearExpr operator+(LinearExpr lhs, const LinearExpr& rhs) {
  lhs += rhs;
  return lhs;
}
LinearExpr operator-(LinearExpr lhs, const LinearExpr& rhs) {
  lhs -= rhs;
  return lhs;
}
LinearExpr operator*(LinearExpr lhs, double rhs) {
  lhs *= rhs;
  return lhs;
}
LinearExpr operator/(LinearExpr lhs, double rhs) {
  lhs /= rhs;
  return lhs;
}
LinearExpr operator*(double lhs, LinearExpr rhs) {
  rhs *= lhs;
  return rhs;
}

LinearRange::LinearRange(double lower_bound, const LinearExpr& linear_expr,
                         double upper_bound)
    : lower_bound_(lower_bound),
      linear_expr_(linear_expr),
      upper_bound_(upper_bound) {
  lower_bound_ -= linear_expr_.offset();
  upper_bound_ -= linear_expr_.offset();
  linear_expr_ -= linear_expr_.offset();
}

LinearRange operator<=(const LinearExpr& lhs, const LinearExpr& rhs) {
  return LinearRange(-std::numeric_limits<double>::infinity(), lhs - rhs, 0);
}
LinearRange operator==(const LinearExpr& lhs, const LinearExpr& rhs) {
  return LinearRange(0, lhs - rhs, 0);
}
LinearRange operator>=(const LinearExpr& lhs, const LinearExpr& rhs) {
  return LinearRange(0, lhs - rhs, std::numeric_limits<double>::infinity());
}

}  // namespace operations_research
