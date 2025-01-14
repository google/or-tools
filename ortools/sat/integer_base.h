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

#ifndef OR_TOOLS_SAT_INTEGER_BASE_H_
#define OR_TOOLS_SAT_INTEGER_BASE_H_

#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Value type of an integer variable. An integer variable is always bounded
// on both sides, and this type is also used to store the bounds [lb, ub] of the
// range of each integer variable.
//
// Note that both bounds are inclusive, which allows to write many propagation
// algorithms for just one of the bound and apply it to the negated variables to
// get the symmetric algorithm for the other bound.
DEFINE_STRONG_INT64_TYPE(IntegerValue);

// The max range of an integer variable is [kMinIntegerValue, kMaxIntegerValue].
//
// It is symmetric so the set of possible ranges stays the same when we take the
// negation of a variable. Moreover, we need some IntegerValue that fall outside
// this range on both side so that we can usually take care of integer overflow
// by simply doing "saturated arithmetic" and if one of the bound overflow, the
// two bounds will "cross" each others and we will get an empty range.
constexpr IntegerValue kMaxIntegerValue(
    std::numeric_limits<IntegerValue::ValueType>::max() - 1);
constexpr IntegerValue kMinIntegerValue(-kMaxIntegerValue.value());

inline double ToDouble(IntegerValue value) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (value >= kMaxIntegerValue) return kInfinity;
  if (value <= kMinIntegerValue) return -kInfinity;
  return static_cast<double>(value.value());
}

template <class IntType>
inline IntType IntTypeAbs(IntType t) {
  return IntType(std::abs(t.value()));
}

inline IntegerValue CeilRatio(IntegerValue dividend,
                              IntegerValue positive_divisor) {
  DCHECK_GT(positive_divisor, 0);
  const IntegerValue result = dividend / positive_divisor;
  const IntegerValue adjust =
      static_cast<IntegerValue>(result * positive_divisor < dividend);
  return result + adjust;
}

inline IntegerValue FloorRatio(IntegerValue dividend,
                               IntegerValue positive_divisor) {
  DCHECK_GT(positive_divisor, 0);
  const IntegerValue result = dividend / positive_divisor;
  const IntegerValue adjust =
      static_cast<IntegerValue>(result * positive_divisor > dividend);
  return result - adjust;
}

// Overflows and saturated arithmetic.

inline IntegerValue CapProdI(IntegerValue a, IntegerValue b) {
  return IntegerValue(CapProd(a.value(), b.value()));
}

inline IntegerValue CapSubI(IntegerValue a, IntegerValue b) {
  return IntegerValue(CapSub(a.value(), b.value()));
}

inline IntegerValue CapAddI(IntegerValue a, IntegerValue b) {
  return IntegerValue(CapAdd(a.value(), b.value()));
}

inline bool ProdOverflow(IntegerValue t, IntegerValue value) {
  return AtMinOrMaxInt64(CapProd(t.value(), value.value()));
}

inline bool AtMinOrMaxInt64I(IntegerValue t) {
  return AtMinOrMaxInt64(t.value());
}

// Returns dividend - FloorRatio(dividend, divisor) * divisor;
//
// This function is around the same speed than the computation above, but it
// never causes integer overflow. Note also that when calling FloorRatio() then
// PositiveRemainder(), the compiler should optimize the modulo away and just
// reuse the one from the first integer division.
inline IntegerValue PositiveRemainder(IntegerValue dividend,
                                      IntegerValue positive_divisor) {
  DCHECK_GT(positive_divisor, 0);
  const IntegerValue m = dividend % positive_divisor;
  return m < 0 ? m + positive_divisor : m;
}

inline bool AddTo(IntegerValue a, IntegerValue* result) {
  if (AtMinOrMaxInt64I(a)) return false;
  const IntegerValue add = CapAddI(a, *result);
  if (AtMinOrMaxInt64I(add)) return false;
  *result = add;
  return true;
}

// Computes result += a * b, and return false iff there is an overflow.
inline bool AddProductTo(IntegerValue a, IntegerValue b, IntegerValue* result) {
  const IntegerValue prod = CapProdI(a, b);
  if (AtMinOrMaxInt64I(prod)) return false;
  const IntegerValue add = CapAddI(prod, *result);
  if (AtMinOrMaxInt64I(add)) return false;
  *result = add;
  return true;
}

// Index of an IntegerVariable.
//
// Each time we create an IntegerVariable we also create its negation. This is
// done like that so internally we only stores and deal with lower bound. The
// upper bound being the lower bound of the negated variable.
DEFINE_STRONG_INDEX_TYPE(IntegerVariable);
const IntegerVariable kNoIntegerVariable(-1);
inline IntegerVariable NegationOf(IntegerVariable i) {
  return IntegerVariable(i.value() ^ 1);
}

inline bool VariableIsPositive(IntegerVariable i) {
  return (i.value() & 1) == 0;
}

inline IntegerVariable PositiveVariable(IntegerVariable i) {
  return IntegerVariable(i.value() & (~1));
}

// Special type for storing only one thing for var and NegationOf(var).
DEFINE_STRONG_INDEX_TYPE(PositiveOnlyIndex);
inline PositiveOnlyIndex GetPositiveOnlyIndex(IntegerVariable var) {
  return PositiveOnlyIndex(var.value() / 2);
}

inline std::string IntegerTermDebugString(IntegerVariable var,
                                          IntegerValue coeff) {
  coeff = VariableIsPositive(var) ? coeff : -coeff;
  return absl::StrCat(coeff.value(), "*X", var.value() / 2);
}

// Returns the vector of the negated variables.
std::vector<IntegerVariable> NegationOf(absl::Span<const IntegerVariable> vars);

// The integer equivalent of a literal.
// It represents an IntegerVariable and an upper/lower bound on it.
//
// Overflow: all the bounds below kMinIntegerValue and kMaxIntegerValue are
// treated as kMinIntegerValue - 1 and kMaxIntegerValue + 1.
struct IntegerLiteral {
  // Because IntegerLiteral should never be created at a bound less constrained
  // than an existing IntegerVariable bound, we don't allow GreaterOrEqual() to
  // have a bound lower than kMinIntegerValue, and LowerOrEqual() to have a
  // bound greater than kMaxIntegerValue. The other side is not constrained
  // to allow for a computed bound to overflow. Note that both the full initial
  // domain and the empty domain can always be represented.
  static IntegerLiteral GreaterOrEqual(IntegerVariable i, IntegerValue bound);
  static IntegerLiteral LowerOrEqual(IntegerVariable i, IntegerValue bound);

  // These two static integer literals represent an always true and an always
  // false condition.
  static IntegerLiteral TrueLiteral();
  static IntegerLiteral FalseLiteral();

  // Clients should prefer the static construction methods above.
  IntegerLiteral() : var(kNoIntegerVariable), bound(0) {}
  IntegerLiteral(IntegerVariable v, IntegerValue b) : var(v), bound(b) {
    DCHECK_GE(bound, kMinIntegerValue);
    DCHECK_LE(bound, kMaxIntegerValue + 1);
  }

  bool IsValid() const { return var != kNoIntegerVariable; }
  bool IsAlwaysTrue() const { return var == kNoIntegerVariable && bound <= 0; }
  bool IsAlwaysFalse() const { return var == kNoIntegerVariable && bound > 0; }

  // The negation of x >= bound is x <= bound - 1.
  IntegerLiteral Negated() const;

  bool operator==(IntegerLiteral o) const {
    return var == o.var && bound == o.bound;
  }
  bool operator!=(IntegerLiteral o) const {
    return var != o.var || bound != o.bound;
  }

  std::string DebugString() const {
    return VariableIsPositive(var)
               ? absl::StrCat("I", var.value() / 2, ">=", bound.value())
               : absl::StrCat("I", var.value() / 2, "<=", -bound.value());
  }

  // Note that bound should be in [kMinIntegerValue, kMaxIntegerValue + 1].
  IntegerVariable var = kNoIntegerVariable;
  IntegerValue bound = IntegerValue(0);
};

inline std::ostream& operator<<(std::ostream& os, IntegerLiteral i_lit) {
  os << i_lit.DebugString();
  return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                absl::Span<const IntegerLiteral> literals) {
  os << "[";
  bool first = true;
  for (const IntegerLiteral literal : literals) {
    if (first) {
      first = false;
    } else {
      os << ",";
    }
    os << literal.DebugString();
  }
  os << "]";
  return os;
}

// Represents [coeff * variable + constant] or just a [constant].
//
// In some places it is useful to manipulate such expression instead of having
// to create an extra integer variable. This is mainly used for scheduling
// related constraints.
struct AffineExpression {
  // Helper to construct an AffineExpression.
  AffineExpression() = default;
  AffineExpression(IntegerValue cst)  // NOLINT(runtime/explicit)
      : constant(cst) {}
  AffineExpression(IntegerVariable v)  // NOLINT(runtime/explicit)
      : var(v), coeff(1) {}
  AffineExpression(IntegerVariable v, IntegerValue c)
      : var(c >= 0 ? v : NegationOf(v)), coeff(IntTypeAbs(c)) {}
  AffineExpression(IntegerVariable v, IntegerValue c, IntegerValue cst)
      : var(c >= 0 ? v : NegationOf(v)), coeff(IntTypeAbs(c)), constant(cst) {}

  // Returns the integer literal corresponding to expression >= value or
  // expression <= value.
  //
  // On constant expressions, they will return IntegerLiteral::TrueLiteral()
  // or IntegerLiteral::FalseLiteral().
  IntegerLiteral GreaterOrEqual(IntegerValue bound) const;
  IntegerLiteral LowerOrEqual(IntegerValue bound) const;

  AffineExpression Negated() const {
    if (var == kNoIntegerVariable) return AffineExpression(-constant);
    return AffineExpression(NegationOf(var), coeff, -constant);
  }

  AffineExpression MultipliedBy(IntegerValue multiplier) const {
    // Note that this also works if multiplier is negative.
    return AffineExpression(var, coeff * multiplier, constant * multiplier);
  }

  bool operator==(AffineExpression o) const {
    return var == o.var && coeff == o.coeff && constant == o.constant;
  }

  // Returns the value of this affine expression given its variable value.
  IntegerValue ValueAt(IntegerValue var_value) const {
    return coeff * var_value + constant;
  }

  // Returns the affine expression value under a given LP solution.
  double LpValue(const util_intops::StrongVector<IntegerVariable, double>&
                     lp_values) const {
    if (var == kNoIntegerVariable) return ToDouble(constant);
    return ToDouble(coeff) * lp_values[var] + ToDouble(constant);
  }

  bool IsConstant() const { return var == kNoIntegerVariable; }

  std::string DebugString() const {
    if (var == kNoIntegerVariable) return absl::StrCat(constant.value());
    if (constant == 0) {
      return absl::StrCat("(", coeff.value(), " * X", var.value(), ")");
    } else {
      return absl::StrCat("(", coeff.value(), " * X", var.value(), " + ",
                          constant.value(), ")");
    }
  }

  // The coefficient MUST be positive. Use NegationOf(var) if needed.
  //
  // TODO(user): Make this private to enforce the invariant that coeff cannot be
  // negative.
  IntegerVariable var = kNoIntegerVariable;  // kNoIntegerVariable for constant.
  IntegerValue coeff = IntegerValue(0);      // Zero for constant.
  IntegerValue constant = IntegerValue(0);
};

template <typename H>
H AbslHashValue(H h, const AffineExpression& e) {
  if (e.var != kNoIntegerVariable) {
    h = H::combine(std::move(h), e.var);
    h = H::combine(std::move(h), e.coeff);
  }
  h = H::combine(std::move(h), e.constant);

  return h;
}

// A model singleton that holds the root level integer variable domains.
// we just store a single domain for both var and its negation.
struct IntegerDomains
    : public util_intops::StrongVector<PositiveOnlyIndex, Domain> {};

// A model singleton used for debugging. If this is set in the model, then we
// can check that various derived constraint do not exclude this solution (if it
// is a known optimal solution for instance).
struct DebugSolution {
  // This is the value of all proto variables.
  // It should be of the same size of the PRESOLVED model and should correspond
  // to a solution to the presolved model.
  std::vector<int64_t> proto_values;

  // This is filled from proto_values at load-time, and using the
  // cp_model_mapping, we cache the solution of the integer variables that are
  // mapped. Note that it is possible that not all integer variable are mapped.
  //
  // TODO(user): When this happen we should be able to infer the value of these
  // derived variable in the solution. For now, we only do that for the
  // objective variable.
  util_intops::StrongVector<IntegerVariable, bool> ivar_has_value;
  util_intops::StrongVector<IntegerVariable, IntegerValue> ivar_values;
};

// A value and a literal.
struct ValueLiteralPair {
  struct CompareByLiteral {
    bool operator()(const ValueLiteralPair& a,
                    const ValueLiteralPair& b) const {
      return a.literal < b.literal;
    }
  };
  struct CompareByValue {
    bool operator()(const ValueLiteralPair& a,
                    const ValueLiteralPair& b) const {
      return (a.value < b.value) ||
             (a.value == b.value && a.literal < b.literal);
    }
  };

  bool operator==(const ValueLiteralPair& o) const {
    return value == o.value && literal == o.literal;
  }

  std::string DebugString() const;

  IntegerValue value = IntegerValue(0);
  Literal literal = Literal(kNoLiteralIndex);
};

std::ostream& operator<<(std::ostream& os, const ValueLiteralPair& p);

DEFINE_STRONG_INDEX_TYPE(IntervalVariable);
const IntervalVariable kNoIntervalVariable(-1);

// ============================================================================
// Implementation.
// ============================================================================

inline IntegerLiteral IntegerLiteral::GreaterOrEqual(IntegerVariable i,
                                                     IntegerValue bound) {
  return IntegerLiteral(
      i, bound > kMaxIntegerValue ? kMaxIntegerValue + 1 : bound);
}

inline IntegerLiteral IntegerLiteral::LowerOrEqual(IntegerVariable i,
                                                   IntegerValue bound) {
  return IntegerLiteral(
      NegationOf(i), bound < kMinIntegerValue ? kMaxIntegerValue + 1 : -bound);
}

inline IntegerLiteral IntegerLiteral::TrueLiteral() {
  return IntegerLiteral(kNoIntegerVariable, IntegerValue(-1));
}

inline IntegerLiteral IntegerLiteral::FalseLiteral() {
  return IntegerLiteral(kNoIntegerVariable, IntegerValue(1));
}

inline IntegerLiteral IntegerLiteral::Negated() const {
  // Note that bound >= kMinIntegerValue, so -bound + 1 will have the correct
  // capped value.
  return IntegerLiteral(
      NegationOf(IntegerVariable(var)),
      bound > kMaxIntegerValue ? kMinIntegerValue : -bound + 1);
}

// var * coeff + constant >= bound.
inline IntegerLiteral AffineExpression::GreaterOrEqual(
    IntegerValue bound) const {
  if (var == kNoIntegerVariable) {
    return constant >= bound ? IntegerLiteral::TrueLiteral()
                             : IntegerLiteral::FalseLiteral();
  }
  DCHECK_GT(coeff, 0);
  return IntegerLiteral::GreaterOrEqual(var,
                                        CeilRatio(bound - constant, coeff));
}

// var * coeff + constant <= bound.
inline IntegerLiteral AffineExpression::LowerOrEqual(IntegerValue bound) const {
  if (var == kNoIntegerVariable) {
    return constant <= bound ? IntegerLiteral::TrueLiteral()
                             : IntegerLiteral::FalseLiteral();
  }
  DCHECK_GT(coeff, 0);
  return IntegerLiteral::LowerOrEqual(var, FloorRatio(bound - constant, coeff));
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_BASE_H_
