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

#include "ortools/linear_solver/linear_expr.h"

#include <limits>

#include "absl/strings/str_join.h"
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

namespace {

void AppendTerm(const double coef, const std::string& var_name,
                const bool is_first, std::string* s) {
  if (is_first) {
    if (coef == 1.0) {
      absl::StrAppend(s, var_name);
    } else if (coef == -1.0) {
      absl::StrAppend(s, "-", var_name);
    } else {
      absl::StrAppend(s, coef, "*", var_name);
    }
  } else {
    const std::string op = coef < 0 ? "-" : "+";
    const double abs_coef = std::abs(coef);
    if (abs_coef == 1.0) {
      absl::StrAppend(s, " ", op, " ", var_name);
    } else {
      absl::StrAppend(s, " ", op, " ", abs_coef, "*", var_name);
    }
  }
}

void AppendOffset(const double offset, const bool is_first, std::string* s) {
  if (is_first) {
    absl::StrAppend(s, offset);
  } else {
    if (offset != 0.0) {
      const std::string op = offset < 0 ? "-" : "+";
      absl::StrAppend(s, " ", op, " ", std::abs(offset));
    }
  }
}

}  // namespace

std::string LinearExpr::ToString() const {
  std::vector<const MPVariable*> vars_in_order;
  for (const auto& var_val_pair : terms_) {
    vars_in_order.push_back(var_val_pair.first);
  }
  std::sort(vars_in_order.begin(), vars_in_order.end(),
            [](const MPVariable* v, const MPVariable* u) {
              return v->index() < u->index();
            });
  std::string result;
  bool is_first = true;
  for (const MPVariable* var : vars_in_order) {
    // MPSolver gives names to all variables, even if you don't.
    DCHECK(!var->name().empty());
    AppendTerm(terms_.at(var), var->name(), is_first, &result);
    is_first = false;
  }
  AppendOffset(offset_, is_first, &result);
  // TODO(user): support optionally cropping long strings.
  return result;
}

std::ostream& operator<<(std::ostream& stream, const LinearExpr& linear_expr) {
  stream << linear_expr.ToString();
  return stream;
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
