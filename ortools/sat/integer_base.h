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

#ifndef ORTOOLS_SAT_INTEGER_BASE_H_
#define ORTOOLS_SAT_INTEGER_BASE_H_

#include <stdlib.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
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

// Callbacks that will be called when the search goes back to level 0.
// Callbacks should return false if the propagation fails.
//
// We will call this after propagation has reached a fixed point. Note however
// that if any callbacks "propagate" something, the callbacks following it might
// not see a state where the propagation have been called again.
// TODO(user): maybe we should re-propagate before calling the next callback.
struct LevelZeroCallbackHelper {
  std::vector<std::function<bool()>> callbacks;
};

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

// When the case positive_divisor == 1 is frequent, this is faster.
inline IntegerValue FloorRatioWithTest(IntegerValue dividend,
                                       IntegerValue positive_divisor) {
  if (positive_divisor == 1) return dividend;
  return FloorRatio(dividend, positive_divisor);
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

// Computes result += a * a, and return false iff there is an overflow.
inline bool AddSquareTo(IntegerValue a, IntegerValue* result) {
  return AddProductTo(a, a, result);
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
  return absl::StrCat(coeff.value(), "*I", GetPositiveOnlyIndex(var));
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
    if (var == kNoIntegerVariable) return IsAlwaysTrue() ? "<true>" : "<false>";
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

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const AffineExpression& expr) {
    if (expr.constant == 0) {
      absl::Format(&sink, "(%v)", IntegerTermDebugString(expr.var, expr.coeff));
    } else {
      absl::Format(&sink, "(%v + %d)",
                   IntegerTermDebugString(expr.var, expr.coeff),
                   expr.constant.value());
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

// A linear expression with at most two variables (coeffs can be zero).
// And some utility to canonicalize them.
struct LinearExpression2 {
  // Construct a zero expression.
  LinearExpression2() = default;

  LinearExpression2(IntegerVariable v1, IntegerVariable v2, IntegerValue c1,
                    IntegerValue c2) {
    vars[0] = v1;
    vars[1] = v2;
    coeffs[0] = c1;
    coeffs[1] = c2;
  }

  // Build (v1 - v2)
  static LinearExpression2 Difference(IntegerVariable v1, IntegerVariable v2) {
    return LinearExpression2(v1, v2, 1, -1);
  }

  // Take the negation of this expression.
  void Negate() {
    if (vars[0] != kNoIntegerVariable) {
      vars[0] = NegationOf(vars[0]);
    }
    if (vars[1] != kNoIntegerVariable) {
      vars[1] = NegationOf(vars[1]);
    }
  }

  // This will not change any bounds on the LinearExpression2.
  // That is we will not potentially Negate() the expression like
  // CanonicalizeAndUpdateBounds() might do.
  // Note that since kNoIntegerVariable=-1 and we sort the variables, if we have
  // one zero and one non-zero we will always have the zero first.
  void SimpleCanonicalization();

  // Fully canonicalizes the expression and updates the given bounds
  // accordingly. This is the same as SimpleCanonicalization(), DivideByGcd()
  // and the NegateForCanonicalization() with a proper updates of the bounds.
  // Returns whether the expression was negated.
  bool CanonicalizeAndUpdateBounds(IntegerValue& lb, IntegerValue& ub);

  // Deduce an affine expression for the lower bound for the i-th (i=0 or 1)
  // variable from a lower bound on the LinearExpression2. Returns `affine` so
  // that (expr >= lb) => (expr.vars[var_index] >= affine) with the condition
  // that expr.vars[1-var_index] >= other_var_lb.
  AffineExpression GetAffineLowerBound(int var_index, IntegerValue expr_lb,
                                       IntegerValue other_var_lb) const;

  // Divides the expression by the gcd of both coefficients, and returns it.
  // Note that we always return something >= 1 even if both coefficients are
  // zero.
  IntegerValue DivideByGcd();

  bool IsCanonicalized() const;

  // Makes sure expr and -expr have the same canonical representation by
  // negating the expression of it is in the non-canonical form. Returns true if
  // the expression was negated.
  bool NegateForCanonicalization();

  void MakeVariablesPositive();

  absl::Span<const IntegerVariable> non_zero_vars() const {
    const int first = coeffs[0] == 0 ? 1 : 0;
    const int last = coeffs[1] == 0 ? 0 : 1;
    return absl::MakeSpan(&vars[first], last - first + 1);
  }

  absl::Span<const IntegerValue> non_zero_coeffs() const {
    const int first = coeffs[0] == 0 ? 1 : 0;
    const int last = coeffs[1] == 0 ? 0 : 1;
    return absl::MakeSpan(&coeffs[first], last - first + 1);
  }

  bool operator==(const LinearExpression2& o) const {
    return vars[0] == o.vars[0] && vars[1] == o.vars[1] &&
           coeffs[0] == o.coeffs[0] && coeffs[1] == o.coeffs[1];
  }

  bool operator<(const LinearExpression2& o) const {
    return std::tie(vars[0], vars[1], coeffs[0], coeffs[1]) <
           std::tie(o.vars[0], o.vars[1], o.coeffs[0], o.coeffs[1]);
  }

  IntegerValue coeffs[2];
  IntegerVariable vars[2] = {kNoIntegerVariable, kNoIntegerVariable};

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const LinearExpression2& expr) {
    if (expr.coeffs[0] == 0) {
      if (expr.coeffs[1] == 0) {
        absl::Format(&sink, "0");
      } else {
        absl::Format(&sink, "%v",
                     IntegerTermDebugString(expr.vars[1], expr.coeffs[1]));
      }
    } else {
      absl::Format(&sink, "%v + %v",
                   IntegerTermDebugString(expr.vars[0], expr.coeffs[0]),
                   IntegerTermDebugString(expr.vars[1], expr.coeffs[1]));
    }
  }
};

// Encodes (a - b <= ub) in (linear2 <= ub) format.
// Note that the returned expression is canonicalized and divided by its GCD.
inline std::pair<LinearExpression2, IntegerValue> EncodeDifferenceLowerThan(
    AffineExpression a, AffineExpression b, IntegerValue ub) {
  LinearExpression2 expr;
  expr.vars[0] = a.var;
  expr.coeffs[0] = a.coeff;
  expr.vars[1] = b.var;
  expr.coeffs[1] = -b.coeff;
  IntegerValue rhs = ub + b.constant - a.constant;

  // Canonicalize.
  expr.SimpleCanonicalization();
  const IntegerValue gcd = expr.DivideByGcd();
  rhs = FloorRatio(rhs, gcd);
  return {std::move(expr), rhs};
}

template <typename H>
H AbslHashValue(H h, const LinearExpression2& e) {
  h = H::combine(std::move(h), e.vars[0]);
  h = H::combine(std::move(h), e.vars[1]);
  h = H::combine(std::move(h), e.coeffs[0]);
  h = H::combine(std::move(h), e.coeffs[1]);
  return h;
}

// Note that we only care about binary relation, not just simple variable bound.
enum class RelationStatus { IS_TRUE, IS_FALSE, IS_UNKNOWN };
class BestBinaryRelationBounds {
 public:
  // Register the fact that expr \in [lb, ub] is true.
  //
  // If lb==kMinIntegerValue it only register that expr <= ub (and symmetrically
  // for ub==kMaxIntegerValue).
  //
  // Returns for each of the bound if it was restricted (added/updated), if it
  // was ignored because a better or equal bound was already present, or if it
  // was rejected because it was invalid (e.g. the expression was a degenerate
  // linear2 or the bound was a min/max value).
  enum class AddResult {
    ADDED,
    UPDATED,
    NOT_BETTER,
    INVALID,
  };
  std::pair<AddResult, AddResult> Add(LinearExpression2 expr, IntegerValue lb,
                                      IntegerValue ub);

  // Returns the known status of expr <= bound.
  RelationStatus GetStatus(LinearExpression2 expr, IntegerValue lb,
                           IntegerValue ub) const;

  // Same as GetUpperBound() but assume the expression is already canonicalized.
  // This is slightly faster.
  IntegerValue UpperBoundWhenCanonicalized(LinearExpression2 expr) const;

  int64_t num_bounds() const { return best_bounds_.size(); }

  std::vector<std::pair<LinearExpression2, IntegerValue>>
  GetSortedNonTrivialUpperBounds() const;

  std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>>
  GetSortedNonTrivialBounds() const;

 private:
  // The best bound on the given "canonicalized" expression.
  absl::flat_hash_map<LinearExpression2, std::pair<IntegerValue, IntegerValue>>
      best_bounds_;
};

// A model singleton that holds the root level integer variable domains.
// we just store a single domain for both var and its negation.
struct IntegerDomains
    : public util_intops::StrongVector<PositiveOnlyIndex, Domain> {};

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

// This functions appears in hot spot, and so it is important to inline it.
//
// TODO(user): Maybe introduce a CanonicalizedLinear2 class so we automatically
// get the better function, and it documents when we have canonicalized
// expression.
inline IntegerValue BestBinaryRelationBounds::UpperBoundWhenCanonicalized(
    LinearExpression2 expr) const {
  DCHECK_EQ(expr.DivideByGcd(), 1);
  DCHECK(expr.IsCanonicalized());
  const bool negated = expr.NegateForCanonicalization();
  const auto it = best_bounds_.find(expr);
  if (it != best_bounds_.end()) {
    const auto [known_lb, known_ub] = it->second;
    if (negated) {
      return -known_lb;
    } else {
      return known_ub;
    }
  }
  return kMaxIntegerValue;
}

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
  return IntegerLiteral::GreaterOrEqual(
      var, coeff == 1 ? bound - constant : CeilRatio(bound - constant, coeff));
}

// var * coeff + constant <= bound.
inline IntegerLiteral AffineExpression::LowerOrEqual(IntegerValue bound) const {
  if (var == kNoIntegerVariable) {
    return constant <= bound ? IntegerLiteral::TrueLiteral()
                             : IntegerLiteral::FalseLiteral();
  }
  DCHECK_GT(coeff, 0);
  return IntegerLiteral::LowerOrEqual(
      var, coeff == 1 ? bound - constant : FloorRatio(bound - constant, coeff));
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_INTEGER_BASE_H_
