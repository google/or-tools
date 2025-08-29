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
#include <sstream>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/fp_roundtrip_conv_testing.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::ContainerEq;
using ::testing::ExplainMatchResult;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::PrintToString;
using ::testing::UnorderedElementsAre;

using internal::kObjectsFromOtherModelStorage;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Struct used as parameter in BoundedLinearExpressionEquiv matcher.
struct LinearTerms {
  LinearTerms() = default;
  LinearTerms(std::initializer_list<LinearTerm> terms) : terms(terms) {}
  std::vector<LinearTerm> terms;
};

std::ostream& operator<<(std::ostream& ostr, const LinearTerms& terms) {
  if (terms.terms.empty()) {
    ostr << "0";
    return ostr;
  }

  bool first = true;
  for (const auto& term : terms.terms) {
    if (first) {
      first = false;
    } else {
      ostr << " + ";
    }
    ostr << term.coefficient << "*" << term.variable;
  }
  return ostr;
}

// Return true if the two BoundedLinearExpression are equivalent. This is the
// case when they have the same bounds (after removing the offset) and the same
// coefficients. But this is also the case when they exchange their bounds and
// change all the signs.
//
// For example these bounded expressions are equivalent:
//   3 <= 2 * x - y + 2 <= 5
//   4 <= 2 * x - y + 3 <= 6
//   -5 <= -2 * x + y - 2 <= -3
//
// If one number is NaN the bounded expression will also be considered
// different.
//
// Usage:
//   EXPECT_THAT(-2 <= 3 * x + 2 * y <= 4,
//               BoundedLinearExpressionEquiv(-2, LinearTerms({{x, 3}, {y, 2}}),
//               4));
MATCHER_P3(BoundedLinearExpressionEquiv, lower_bound, terms, upper_bound,
           absl::StrFormat("%s equivalent to %s <= %s <= %s",
                           negation ? "isn't" : "is",
                           PrintToString(lower_bound), PrintToString(terms),
                           PrintToString(upper_bound))) {
  // We detect if we need to switch and negate bounds, and also negate terms.
  const bool negation = arg.lower_bound_minus_offset() != lower_bound;
  {
    const double expected_lower_bound_minus_offset =
        negation ? -upper_bound : lower_bound;
    // We use the `!(x == y)` trick here so that NaN are seen as
    // errors. Comparison with NaN is always false, hence the negation of
    // equality will be true is at least one operand is NaN.
    if (!(arg.lower_bound_minus_offset() ==
          expected_lower_bound_minus_offset)) {
      *result_listener << "lower_bound - offset = "
                       << arg.lower_bound_minus_offset()
                       << " != " << expected_lower_bound_minus_offset;
      return false;
    }
  }
  {
    const double expected_upper_bound_minus_offset =
        negation ? -lower_bound : upper_bound;
    // We use the `!(x == y)` trick here so that NaN are seen as
    // errors. Comparison with NaN is always false, hence the negation of
    // equality will be true is at least one operand is NaN.
    if (!(arg.upper_bound_minus_offset() ==
          expected_upper_bound_minus_offset)) {
      *result_listener << "upper_bound - offset = "
                       << arg.upper_bound_minus_offset()
                       << " != " << expected_upper_bound_minus_offset;
      return false;
    }
  }
  VariableMap<double> expected_terms;
  for (const auto& term : terms.terms) {
    expected_terms.emplace(term.variable,
                           negation ? -term.coefficient : term.coefficient);
  }
  return ExplainMatchResult(ContainerEq(expected_terms), arg.expression.terms(),
                            result_listener);
}

TEST(BoundedLinearExpressionMatcher, EmptyExpressions) {
  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), -kInf, kInf),
              BoundedLinearExpressionEquiv(-kInf, LinearTerms(), kInf));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), -3.0, 5.0),
              BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), -5.0, 3.0),
              BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0));

  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), kNaN, 5.0),
              Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0)));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), -3.0, kNaN),
              Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0)));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), kNaN, kNaN),
              Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0)));

  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), -2.0, 5.0),
              Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0)));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression(), -3.0, 6.0),
              Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0)));
}

TEST(BoundedLinearExpressionMatcher, OffsetOnlyExpressions) {
  EXPECT_THAT(BoundedLinearExpression(LinearExpression({}, 3.0), -kInf, kInf),
              BoundedLinearExpressionEquiv(-kInf, LinearTerms(), kInf));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression({}, 4.0), -3.0, 5.0),
              BoundedLinearExpressionEquiv(-7.0, LinearTerms(), 1.0));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression({}, -4.0), -5.0, 3.0),
              BoundedLinearExpressionEquiv(-7.0, LinearTerms(), 1.0));

  EXPECT_THAT(BoundedLinearExpression(LinearExpression({}, kNaN), -3.0, 5.0),
              Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0)));

  EXPECT_THAT(BoundedLinearExpression(LinearExpression({}, 1.0), -3.0, 5.0),
              Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms(), 5.0)));
}

TEST(BoundedLinearExpressionMatcher, OffsetAndTermsExpressions) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  EXPECT_THAT(BoundedLinearExpression(
                  LinearExpression({{a, 1.0}, {b, -2.0}}, 3.0), -kInf, kInf),
              BoundedLinearExpressionEquiv(
                  -kInf, LinearTerms({{a, 1.0}, {b, -2.0}}), kInf));
  EXPECT_THAT(BoundedLinearExpression(
                  LinearExpression({{a, 1.0}, {b, -2.0}}, 4.0), -3.0, 5.0),
              BoundedLinearExpressionEquiv(
                  -7.0, LinearTerms({{a, 1.0}, {b, -2.0}}), 1.0));
  EXPECT_THAT(BoundedLinearExpression(
                  LinearExpression({{a, -1.0}, {b, 2.0}}, -4.0), -5.0, 3.0),
              BoundedLinearExpressionEquiv(
                  -7.0, LinearTerms({{a, 1.0}, {b, -2.0}}), 1.0));

  EXPECT_THAT(BoundedLinearExpression(
                  LinearExpression({{a, kNaN}, {b, -2.0}}, 0.0), -3.0, 5.0),
              Not(BoundedLinearExpressionEquiv(
                  -3.0, LinearTerms({{a, 1.0}, {b, -2.0}}), 5.0)));

  EXPECT_THAT(BoundedLinearExpression(
                  LinearExpression({{a, 1.0}, {b, -2.0}}, 0.0), -3.0, 5.0),
              Not(BoundedLinearExpressionEquiv(
                  -3.0, LinearTerms({{a, -1.0}, {b, -2.0}}), 5.0)));
  EXPECT_THAT(BoundedLinearExpression(
                  LinearExpression({{a, 1.0}, {b, -2.0}}, 0.0), -3.0, 5.0),
              Not(BoundedLinearExpressionEquiv(
                  -3.0, LinearTerms({{a, 1.0}, {b, -2.0}, {c, 3.0}}), 5.0)));
  EXPECT_THAT(
      BoundedLinearExpression(LinearExpression({{a, 1.0}, {b, -2.0}}, 0.0),
                              -3.0, 5.0),
      Not(BoundedLinearExpressionEquiv(-3.0, LinearTerms({{a, 1.0}}), 5.0)));
  EXPECT_THAT(BoundedLinearExpression(
                  LinearExpression({{a, -1.0}, {b, 2.0}}, -4.0), -5.0, 3.0),
              Not(BoundedLinearExpressionEquiv(-7.0, LinearTerms(), 1.0)));
  EXPECT_THAT(BoundedLinearExpression(LinearExpression({}, -4.0), -5.0, 3.0),
              Not(BoundedLinearExpressionEquiv(
                  -7.0, LinearTerms({{a, 1.0}, {b, -2.0}}), 1.0)));
}

// Struct used as parameter in BoundedLinearExpressionEquiv matcher.
struct QuadraticTerms {
  QuadraticTerms() = default;
  QuadraticTerms(std::initializer_list<QuadraticTerm> terms) : terms(terms) {}
  std::vector<QuadraticTerm> terms;
};

std::ostream& operator<<(std::ostream& ostr, const QuadraticTerms& terms) {
  if (terms.terms.empty()) {
    ostr << "0";
    return ostr;
  }

  bool first = true;
  for (const auto& term : terms.terms) {
    if (first) {
      first = false;
    } else {
      ostr << " + ";
    }
    ostr << term.coefficient() << "*" << term.first_variable();
    if (term.first_variable() == term.second_variable()) {
      ostr << "²";
    } else {
      ostr << "*" << term.second_variable();
    }
  }
  return ostr;
}

// Return true if the two BoundedQuadraticExpression are equivalent. This is the
// case when they have the same bounds (after removing the offset) and the same
// coefficients. But this is also the case when they exchange their bounds and
// change all the signs.
//
// For example these bounded expressions are equivalent:
//   3 <= 2 * x * y - y + 2 <= 5
//   4 <= 2 * x * y - y + 3 <= 6
//   -5 <= -2 * x * y + y - 2 <= -3
//
// If one number is NaN the bounded expression will also be considered
// different.
//
// Usage:
//   EXPECT_THAT(-2 <= 3 * x * y + 2 * y <= 4,
//               BoundedQuadraticExpressionEquiv(-2, QuadraticTerms({{x, y,
//               3}}), LinearTerms({{x, 3}, {y, 2}}), 4));
MATCHER_P4(BoundedQuadraticExpressionEquiv, lower_bound, quadratic_terms,
           linear_terms, upper_bound,
           absl::StrFormat("%s equivalent to %s <= %s + %s <= %s",
                           negation ? "isn't" : "is",
                           PrintToString(lower_bound),
                           PrintToString(quadratic_terms),
                           PrintToString(linear_terms),
                           PrintToString(upper_bound))) {
  // We detect if we need to switch and negate bounds, and also negate terms.
  const bool negation = arg.lower_bound_minus_offset() != lower_bound;
  {
    const double expected_lower_bound_minus_offset =
        negation ? -upper_bound : lower_bound;
    // We use the `!(x == y)` trick here so that NaN are seen as
    // errors. Comparison with NaN is always false, hence the negation of
    // equality will be true is at least one operand is NaN.
    if (!(arg.lower_bound_minus_offset() ==
          expected_lower_bound_minus_offset)) {
      *result_listener << "lower_bound - offset = "
                       << arg.lower_bound_minus_offset()
                       << " != " << expected_lower_bound_minus_offset;
      return false;
    }
  }
  {
    const double expected_upper_bound_minus_offset =
        negation ? -lower_bound : upper_bound;
    // We use the `!(x == y)` trick here so that NaN are seen as
    // errors. Comparison with NaN is always false, hence the negation of
    // equality will be true is at least one operand is NaN.
    if (!(arg.upper_bound_minus_offset() ==
          expected_upper_bound_minus_offset)) {
      *result_listener << "upper_bound - offset = "
                       << arg.upper_bound_minus_offset()
                       << " != " << expected_upper_bound_minus_offset;
      return false;
    }
  }
  VariableMap<double> expected_linear_terms;
  for (const auto& term : linear_terms.terms) {
    expected_linear_terms.insert(
        {term.variable, negation ? -term.coefficient : term.coefficient});
  }
  QuadraticTermMap<double> expected_quadratic_terms;
  for (const auto& term : quadratic_terms.terms) {
    expected_quadratic_terms.insert(
        {term.GetKey(), negation ? -term.coefficient() : term.coefficient()});
  }
  return ExplainMatchResult(ContainerEq(expected_linear_terms),
                            arg.expression.linear_terms(), result_listener) &
         ExplainMatchResult(ContainerEq(expected_quadratic_terms),
                            arg.expression.quadratic_terms(), result_listener);
}

TEST(BoundedQuadraticExpressionMatcher, EmptyExpressions) {
  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), -kInf, kInf),
              BoundedQuadraticExpressionEquiv(-kInf, QuadraticTerms(),
                                              LinearTerms(), kInf));
  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), -3.0, 5.0),
              BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                              LinearTerms(), 5.0));
  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), -5.0, 3.0),
              BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                              LinearTerms(), 5.0));

  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), kNaN, 5.0),
              Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                                  LinearTerms(), 5.0)));
  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), -3.0, kNaN),
              Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                                  LinearTerms(), 5.0)));
  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), kNaN, kNaN),
              Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                                  LinearTerms(), 5.0)));

  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), -2.0, 5.0),
              Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                                  LinearTerms(), 5.0)));
  EXPECT_THAT(BoundedQuadraticExpression(QuadraticExpression(), -3.0, 6.0),
              Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                                  LinearTerms(), 5.0)));
}

TEST(BoundedQuadraticExpressionMatcher, OffsetOnlyExpressions) {
  EXPECT_THAT(
      BoundedQuadraticExpression(QuadraticExpression({}, {}, 3.0), -kInf, kInf),
      BoundedQuadraticExpressionEquiv(-kInf, QuadraticTerms(), LinearTerms(),
                                      kInf));
  EXPECT_THAT(
      BoundedQuadraticExpression(QuadraticExpression({}, {}, 4.0), -3.0, 5.0),
      BoundedQuadraticExpressionEquiv(-7.0, QuadraticTerms(), LinearTerms(),
                                      1.0));
  EXPECT_THAT(
      BoundedQuadraticExpression(QuadraticExpression({}, {}, -4.0), -5.0, 3.0),
      BoundedQuadraticExpressionEquiv(-7.0, QuadraticTerms(), LinearTerms(),
                                      1.0));

  EXPECT_THAT(
      BoundedQuadraticExpression(QuadraticExpression({}, {}, kNaN), -3.0, 5.0),
      Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(), LinearTerms(),
                                          5.0)));

  EXPECT_THAT(
      BoundedQuadraticExpression(QuadraticExpression({}, {}, 1.0), -3.0, 5.0),
      Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(), LinearTerms(),
                                          5.0)));
}

TEST(BoundedQuadraticExpressionMatcher, OffsetAndLinearTermsOnlyExpressions) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, 1.0}, {b, -2.0}}, 3.0), -kInf, kInf),
      BoundedQuadraticExpressionEquiv(
          -kInf, QuadraticTerms(), LinearTerms({{a, 1.0}, {b, -2.0}}), kInf));
  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, 1.0}, {b, -2.0}}, 4.0), -3.0, 5.0),
      BoundedQuadraticExpressionEquiv(-7.0, QuadraticTerms(),
                                      LinearTerms({{a, 1.0}, {b, -2.0}}), 1.0));
  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, -1.0}, {b, 2.0}}, -4.0), -5.0, 3.0),
      BoundedQuadraticExpressionEquiv(-7.0, QuadraticTerms(),
                                      LinearTerms({{a, 1.0}, {b, -2.0}}), 1.0));

  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, kNaN}, {b, -2.0}}, 0.0), -3.0, 5.0),
      Not(BoundedQuadraticExpressionEquiv(
          -3.0, QuadraticTerms(), LinearTerms({{a, 1.0}, {b, -2.0}}), 5.0)));

  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, 1.0}, {b, -2.0}}, 0.0), -3.0, 5.0),
      Not(BoundedQuadraticExpressionEquiv(
          -3.0, QuadraticTerms(), LinearTerms({{a, -1.0}, {b, -2.0}}), 5.0)));
  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, 1.0}, {b, -2.0}}, 0.0), -3.0, 5.0),
      Not(BoundedQuadraticExpressionEquiv(
          -3.0, QuadraticTerms(), LinearTerms({{a, 1.0}, {b, -2.0}, {c, 3.0}}),
          5.0)));
  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, 1.0}, {b, -2.0}}, 0.0), -3.0, 5.0),
      Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms(),
                                          LinearTerms({{a, 1.0}}), 5.0)));
  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({}, {{a, -1.0}, {b, 2.0}}, -4.0), -5.0, 3.0),
      Not(BoundedQuadraticExpressionEquiv(-7.0, QuadraticTerms(), LinearTerms(),
                                          1.0)));
  EXPECT_THAT(
      BoundedQuadraticExpression(QuadraticExpression({}, {}, -4.0), -5.0, 3.0),
      Not(BoundedQuadraticExpressionEquiv(
          -7.0, QuadraticTerms(), LinearTerms({{a, 1.0}, {b, -2.0}}), 1.0)));
}

TEST(BoundedQuadraticExpressionMatcher, OffsetAndTermsExpressions) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  EXPECT_THAT(BoundedQuadraticExpression(
                  QuadraticExpression({{a, a, 4.0}, {a, b, 5.0}},
                                      {{a, 1.0}, {b, -2.0}}, 3.0),
                  -kInf, kInf),
              BoundedQuadraticExpressionEquiv(
                  -kInf, QuadraticTerms({{a, a, 4.0}, {a, b, 5.0}}),
                  LinearTerms({{a, 1.0}, {b, -2.0}}), kInf));
  EXPECT_THAT(BoundedQuadraticExpression(
                  QuadraticExpression({{a, a, 4.0}, {a, b, 5.0}},
                                      {{a, 1.0}, {b, -2.0}}, 4.0),
                  -3.0, 5.0),
              BoundedQuadraticExpressionEquiv(
                  -7.0, QuadraticTerms({{a, a, 4.0}, {a, b, 5.0}}),
                  LinearTerms({{a, 1.0}, {b, -2.0}}), 1.0));
  EXPECT_THAT(BoundedQuadraticExpression(
                  QuadraticExpression({{a, a, 4.0}, {a, b, 5.0}},
                                      {{a, -1.0}, {b, 2.0}}, -4.0),
                  -5.0, 3.0),
              BoundedQuadraticExpressionEquiv(
                  -1.0, QuadraticTerms({{a, a, 4.0}, {a, b, 5.0}}),
                  LinearTerms({{a, -1.0}, {b, 2.0}}), 7.0));

  EXPECT_THAT(BoundedQuadraticExpression(
                  QuadraticExpression({{a, a, kNaN}, {a, b, 5.0}},
                                      {{a, 1.0}, {b, -2.0}}, 0.0),
                  -3.0, 5.0),
              Not(BoundedQuadraticExpressionEquiv(
                  -3.0, QuadraticTerms({{a, a, 4.0}, {a, b, 5.0}}),
                  LinearTerms({{a, 1.0}, {b, -2.0}}), 5.0)));

  EXPECT_THAT(BoundedQuadraticExpression(
                  QuadraticExpression({{a, a, 4.0}, {a, b, 5.0}},
                                      {{a, 1.0}, {b, -2.0}}, 0.0),
                  -3.0, 5.0),
              Not(BoundedQuadraticExpressionEquiv(
                  -3.0, QuadraticTerms({{a, a, -4.0}, {a, b, 5.0}}),
                  LinearTerms({{a, 1.0}, {b, -2.0}}), 5.0)));
  EXPECT_THAT(BoundedQuadraticExpression(
                  QuadraticExpression({{a, a, 4.0}, {a, b, 5.0}},
                                      {{a, 1.0}, {b, -2.0}}, 0.0),
                  -3.0, 5.0),
              Not(BoundedQuadraticExpressionEquiv(
                  -3.0, QuadraticTerms({{a, a, 4.0}, {a, b, 5.0}, {a, c, 6.0}}),
                  LinearTerms({{a, 1.0}, {b, -2.0}, {c, 3.0}}), 5.0)));
  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({{a, a, 4.0}, {a, b, 5.0}}, {{a, 1.0}, {b, -2.0}},
                              0.0),
          -3.0, 5.0),
      Not(BoundedQuadraticExpressionEquiv(-3.0, QuadraticTerms({{a, a, 4.0}}),
                                          LinearTerms({{a, 1.0}}), 5.0)));
  EXPECT_THAT(
      BoundedQuadraticExpression(
          QuadraticExpression({{a, a, 4.0}, {a, b, 5.0}}, {{a, -1.0}, {b, 2.0}},
                              -4.0),
          -5.0, 3.0),
      Not(BoundedQuadraticExpressionEquiv(
          -7.0, QuadraticTerms(), LinearTerms({{a, -1.0}, {b, 2.0}}), 1.0)));
  EXPECT_THAT(
      BoundedQuadraticExpression(QuadraticExpression({}, {}, -4.0), -5.0, 3.0),
      Not(BoundedQuadraticExpressionEquiv(
          -7.0, QuadraticTerms({{a, a, 4.0}, {a, b, 5.0}}), LinearTerms(),
          1.0)));
}

TEST(Variable, OutputStreaming) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable());

  auto to_string = [](Variable v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
  };

  EXPECT_EQ(to_string(a), "a");
  EXPECT_EQ(to_string(b), absl::StrCat("__var#", b.id(), "__"));
}

TEST(Variable, Accessors) {
  ModelStorage storage;
  {
    const VariableId v_id =
        storage.AddVariable(/*lower_bound=*/-kInf, /*upper_bound=*/kInf,
                            /*is_integer=*/false, "continuous");
    const Variable v(&storage, v_id);
    EXPECT_EQ(v.name(), "continuous");
    EXPECT_EQ(v.id(), v_id.value());
    EXPECT_EQ(v.typed_id(), v_id);
    EXPECT_EQ(v.lower_bound(), -kInf);
    EXPECT_EQ(v.upper_bound(), kInf);
    EXPECT_FALSE(v.is_integer());
  }
  {
    const VariableId v_id =
        storage.AddVariable(/*lower_bound=*/3.0, /*upper_bound=*/5.0,
                            /*is_integer=*/true, "integer");
    const Variable v(&storage, v_id);
    EXPECT_EQ(v.name(), "integer");
    EXPECT_EQ(v.id(), v_id.value());
    EXPECT_EQ(v.typed_id(), v_id);
    EXPECT_EQ(v.lower_bound(), 3.0);
    EXPECT_EQ(v.upper_bound(), 5.0);
    EXPECT_TRUE(v.is_integer());
  }
}

TEST(Variable, NameAfterDeletion) {
  Model model;
  const Variable x = model.AddVariable("x");
  ASSERT_EQ(x.name(), "x");

  model.DeleteVariable(x);
  EXPECT_EQ(x.name(), "[variable deleted from model]");
}

TEST(VariablesEqualityTest, SameModelAndVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable a_copy = a;

  // First test with EXPECT_ macros. These works even if the `operator bool`
  // conversion is explicit.
  EXPECT_EQ(a, a);
  EXPECT_EQ(a, a_copy);

  // Then test with writing `==` directly.
  EXPECT_TRUE(a == a);
  EXPECT_TRUE(a == a_copy);

  // And the operator `!=`.
  EXPECT_FALSE(a != a);
  EXPECT_FALSE(a != a_copy);
}

TEST(VariablesEqualityTest, SameModelTwoVariables) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with EXPECT_NE macro. It works even if the `operator bool`
  // conversion is explicit.
  EXPECT_NE(a, b);

  // Then test with writing `==` directly to test the implicit cast.
  EXPECT_FALSE(a == b);

  // Same for `!=`.
  EXPECT_TRUE(a != b);
}

TEST(VariablesEqualityTest, DifferentModelsSameVariable) {
  // Create two variables with the same name and index but in two different
  // models.
  ModelStorage model_a;
  const Variable a_a(&model_a, model_a.AddVariable("a"));
  ModelStorage model_b;
  const Variable b_a(&model_b, model_b.AddVariable("a"));

  // First test with EXPECT_NE macro. It works even if the `operator bool`
  // conversion is explicit.
  EXPECT_NE(a_a, b_a);

  // Then test with writing `==` directly to test the implicit cast.
  EXPECT_FALSE(a_a == b_a);

  // Same for `!=`.
  EXPECT_TRUE(a_a != b_a);
}

TEST(VariablesEqualityTest, VariablesAsKeysInMap) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // Test using the variables as keys.
  absl::flat_hash_map<Variable, int> map;
  map.emplace(a, 1);
  map[b] = 2;

  EXPECT_EQ(map.at(a), 1);
  EXPECT_EQ(map.at(b), 2);
}

TEST(LinearTermTest, FromVariableAndDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term(a, 3.0);
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 3.0);
}

TEST(LinearTermTest, Negation) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term = -LinearTerm(a, 3);
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, -3.0);
}

TEST(LinearTermTest, DoubleTimesVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term = 3 * a;
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 3.0);
}

TEST(LinearTermTest, VariableTimesDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term = a * 3;
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 3.0);
}

TEST(LinearTermTest, DoubleTimesLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term = 2 * LinearTerm(a, 3);
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 6.0);
}

TEST(LinearTermTest, LinearTermTimesDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term = LinearTerm(a, 3) * 2;
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 6.0);
}

TEST(LinearTermTest, LinearTermTimesAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  LinearTerm term(a, 3);
  term *= 2;
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 6.0);
}

TEST(LinearTermTest, VariableDividedByDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term = a / 2;
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 0.5);
}

TEST(LinearTermTest, LinearTermDividedByDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearTerm term = LinearTerm(a, 4) / 2;
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 2.0);
}

TEST(LinearTermTest, LinearTermDividedByAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  LinearTerm term(a, 4);
  term /= 2;
  EXPECT_EQ(term.variable, a);
  EXPECT_EQ(term.coefficient, 2.0);
}

// Define a reset function and a set of macros to test the constructor counters
// of LinearExpression. When the counters are disabled, define empty
// equivalents.
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
void ResetExpressionCounters() {
  LinearExpression::ResetCounters();
  QuadraticExpression::ResetCounters();
}
#define EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(T, x) \
  EXPECT_EQ(T::num_calls_default_constructor_, x);
#define EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(T, x) \
  EXPECT_EQ(T::num_calls_copy_constructor_, x);
#define EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(T, x) \
  EXPECT_EQ(T::num_calls_move_constructor_, x);
#define EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(T, x) \
  EXPECT_EQ(T::num_calls_initializer_list_constructor_, x);
#define EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(x) \
  EXPECT_EQ(QuadraticExpression::num_calls_linear_expression_constructor_, x);
#define EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(x)              \
  EXPECT_EQ(LinearExpression::num_calls_default_constructor_ +             \
                LinearExpression::num_calls_copy_constructor_ +            \
                LinearExpression::num_calls_move_constructor_ +            \
                LinearExpression::num_calls_initializer_list_constructor_, \
            x)                                                             \
      << "num_calls_default_constructor: "                                 \
      << LinearExpression::num_calls_default_constructor_                  \
      << "\nnum_calls_copy_constructor: "                                  \
      << LinearExpression::num_calls_copy_constructor_                     \
      << "\nnum_calls_move_constructor: "                                  \
      << LinearExpression::num_calls_move_constructor_                     \
      << "\nnum_calls_initializer_list_constructor: "                      \
      << LinearExpression::num_calls_initializer_list_constructor_
#define EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(x)               \
  EXPECT_EQ(QuadraticExpression::num_calls_default_constructor_ +              \
                QuadraticExpression::num_calls_copy_constructor_ +             \
                QuadraticExpression::num_calls_move_constructor_ +             \
                QuadraticExpression::num_calls_initializer_list_constructor_ + \
                QuadraticExpression::num_calls_linear_expression_constructor_, \
            x)                                                                 \
      << "num_calls_default_constructor: "                                     \
      << QuadraticExpression::num_calls_default_constructor_                   \
      << "\nnum_calls_copy_constructor: "                                      \
      << QuadraticExpression::num_calls_copy_constructor_                      \
      << "\nnum_calls_move_constructor: "                                      \
      << QuadraticExpression::num_calls_move_constructor_                      \
      << "\nnum_calls_initializer_list_constructor: "                          \
      << QuadraticExpression::num_calls_initializer_list_constructor_          \
      << "\nnum_calls_linear_expression_constructor: "                         \
      << QuadraticExpression::num_calls_linear_expression_constructor_
#else
void ResetExpressionCounters() {}
#define EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(T, x)
#define EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(T, x)
#define EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(T, x)
#define EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(T, x)
#define EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(x)
#define EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(x)
#define EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(x)
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS

TEST(VariableTest, Negation) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  ResetExpressionCounters();
  {
    const LinearExpression expr = -a;

    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
    EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -1}}, 0)));
  }

  ResetExpressionCounters();
  {
    const QuadraticExpression expr = -a;

    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
    EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, -1}}, 0)));
  }
}

TEST(LinearExpressionTest, DefaultValue) {
  const LinearExpression expr;
  EXPECT_EQ(expr.offset(), 0.0);
  EXPECT_THAT(expr.terms(), IsEmpty());
  EXPECT_EQ(expr.storage(), nullptr);
  EXPECT_THAT(expr.terms(), IsEmpty());
}

TEST(LinearExpressionTest, EmptyInitializerList) {
  const LinearExpression expr({}, 5);
  EXPECT_EQ(expr.offset(), 5.0);
  EXPECT_THAT(expr.terms(), IsEmpty());
  EXPECT_EQ(expr.storage(), nullptr);
  EXPECT_THAT(expr.terms(), IsEmpty());
}

TEST(LinearExpressionTest, TermsFromSameModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearExpression expr({{a, 3}, {b, 5}, {a, -2}}, -1);
  EXPECT_EQ(expr.offset(), -1.0);
  EXPECT_THAT(expr.terms(), UnorderedElementsAre(Pair(a, 1), Pair(b, 5)));
  EXPECT_EQ(expr.storage(), &storage);
  EXPECT_THAT(expr.terms(), UnorderedElementsAre(Pair(a, 1), Pair(b, 5)));
}

TEST(LinearExpressionDeathTest, TermsFromDifferentModels) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));

  EXPECT_DEATH(LinearExpression({{a, 3}, {b, 5}}, -1),
               kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionTest, ReassignDifferentModels) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));
  const LinearExpression expr_a = a + 2.0;

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));
  LinearExpression expr_b_to_overwrite = 3.0 * b + 1.0;

  expr_b_to_overwrite = expr_a;
  EXPECT_THAT(expr_b_to_overwrite, IsIdentical(LinearExpression({{a, 1}}, 2)));
  EXPECT_EQ(expr_b_to_overwrite.storage(), &model_a);
}

TEST(LinearExpressionTest, MoveConstruction) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));
  LinearExpression expr_a = a + 2.0;
  const LinearExpression expr_b = std::move(expr_a);

  EXPECT_THAT(expr_b, IsIdentical(LinearExpression({{a, 1}}, 2)));
  EXPECT_EQ(expr_b.storage(), &model_a);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_THAT(expr_a.terms(), IsEmpty());
  EXPECT_EQ(expr_a.storage(), nullptr);
}

TEST(LinearExpressionTest, MoveAssignment) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));
  LinearExpression expr_a = a + 2.0;

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));
  LinearExpression expr_b_to_overwrite = 3.0 * b + 1.0;

  expr_b_to_overwrite = std::move(expr_a);

  EXPECT_THAT(expr_b_to_overwrite, IsIdentical(LinearExpression({{a, 1}}, 2)));
  EXPECT_EQ(expr_b_to_overwrite.storage(), &model_a);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_THAT(expr_a.terms(), IsEmpty());
  EXPECT_EQ(expr_a.storage(), nullptr);
}

TEST(LinearExpressionTest, EvaluateEmpty) {
  const LinearExpression empty_expr;
  {
    ModelStorage storage;
    const Variable a(&storage, storage.AddVariable("a"));
    VariableMap<double> variable_values;
    variable_values[a] = 10.0;
    EXPECT_EQ(empty_expr.Evaluate(variable_values), 0.0);
    EXPECT_EQ(empty_expr.EvaluateWithDefaultZero(variable_values), 0.0);
  }
  {
    const VariableMap<double> empty_values;
    EXPECT_EQ(empty_expr.Evaluate(empty_values), 0.0);
    EXPECT_EQ(empty_expr.EvaluateWithDefaultZero(empty_values), 0.0);
  }
}

TEST(LinearExpressionTest, EvaluateOnlyOffset) {
  const LinearExpression constant_expr(8.0);
  {
    ModelStorage storage;
    const Variable a(&storage, storage.AddVariable("a"));
    VariableMap<double> variable_values;
    variable_values[a] = 10.0;
    EXPECT_EQ(constant_expr.Evaluate(variable_values), 8.0);
    EXPECT_EQ(constant_expr.EvaluateWithDefaultZero(variable_values), 8.0);
  }
  {
    const VariableMap<double> empty_values;
    EXPECT_EQ(constant_expr.Evaluate(empty_values), 8.0);
    EXPECT_EQ(constant_expr.EvaluateWithDefaultZero(empty_values), 8.0);
  }
}

TEST(LinearExpressionTest, SimpleEvaluate) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearExpression expr = 3.0 * a + 5.0 * b - 2.0;
  VariableMap<double> variable_values;
  variable_values[a] = 10.0;
  variable_values[b] = 100.0;
  EXPECT_EQ(expr.Evaluate(variable_values), 528.0);
}

TEST(LinearExpressionTest, SimpleEvaluateWithDefault) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearExpression expr = 3.0 * a + 5.0 * b - 2.0;
  VariableMap<double> variable_values;
  variable_values[b] = 100.0;
  EXPECT_EQ(expr.EvaluateWithDefaultZero(variable_values), 498.0);
}

TEST(LinearExpressionTest, StableEvaluateAndEvaluateWithDefault) {
  // Here we test that the floating point sum of numbers is done in the sorted
  // order of the variables ids. To do so we rely on a specific floating point
  // number sequence (obtained with a Python script doing random tries) which
  // floating point sum changes depending on the order of operations:
  //
  // 56.66114901664141 + 76.288516611269 + 73.11902164661139 +
  //   0.677336454040622 + 43.75820160525244 = 250.50422533381482
  // 56.66114901664141 + 76.288516611269 + 73.11902164661139 +
  //   43.75820160525244 + 0.677336454040622 = 250.50422533381484
  // 56.66114901664141 + 76.288516611269 + 0.677336454040622 +
  //   73.11902164661139 + 43.75820160525244 = 250.50422533381487
  // 76.288516611269 + 0.677336454040622 + 73.11902164661139 +
  //   43.75820160525244 + 56.66114901664141 = 250.5042253338149
  //
  // Here we will use the first value as the offset of the linear expression (to
  // test that it always taken into account in the same order).
  constexpr double kOffset = 56.66114901664141;
  const std::vector<double> coeffs = {76.288516611269, 73.11902164661139,
                                      0.677336454040622, 43.75820160525244};

  ModelStorage storage;
  std::vector<Variable> vars;
  VariableMap<double> variable_values;
  for (int i = 0; i < coeffs.size(); ++i) {
    vars.emplace_back(&storage, storage.AddVariable(absl::StrCat("v_", i)));
    variable_values.try_emplace(vars.back(), 1.0);
  }

  LinearExpression expr = kOffset;
  for (const int i : {3, 2, 0, 1}) {
    expr += coeffs[i] * vars[i];
  }

  // Expected value for the sum which is:
  //   - offset first
  //   - then all terms sums in the order of variables' indices
  // See the table in the comment above.
  constexpr double kExpected = 250.50422533381482;

  // Test Evaluate();
  {
    const double got = expr.Evaluate(variable_values);
    EXPECT_EQ(got, kExpected)
        << "got: " << RoundTripDoubleFormat(got)
        << " expected: " << RoundTripDoubleFormat(kExpected);
  }

  // Test EvaluateWithDefaultZero();
  {
    const double got = expr.EvaluateWithDefaultZero(variable_values);
    EXPECT_EQ(got, kExpected)
        << "got: " << RoundTripDoubleFormat(got)
        << " expected: " << RoundTripDoubleFormat(kExpected);
  }
}

TEST(LinearExpressionDeathTest, EvaluateMisingVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearExpression expr = 3.0 * a + 5.0 * b - 2.0;
  VariableMap<double> variable_values;
  variable_values[b] = 100.0;
  EXPECT_DEATH(expr.Evaluate(variable_values), "");
}

TEST(LinearExpressionDeathTest, EvaluateDifferentModels) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));
  const LinearExpression expr = 3.0 * a - 2.0;

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));
  VariableMap<double> variable_values;
  variable_values[b] = 100.0;

  EXPECT_DEATH(expr.Evaluate(variable_values), kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionDeathTest, EvaluateWithDefaultZeroDifferentModels) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));
  const LinearExpression expr = 3.0 * a - 2.0;

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));
  VariableMap<double> variable_values;
  variable_values[b] = 100.0;

  EXPECT_EQ(expr.EvaluateWithDefaultZero(variable_values), -2.0);
}

TEST(LinearExpressionTest, FromDouble) {
  const LinearExpression expr = 4.0;

  EXPECT_THAT(expr, IsIdentical(LinearExpression({}, 4)));
  EXPECT_EQ(expr.storage(), nullptr);
  EXPECT_THAT(expr.terms(), IsEmpty());
}

TEST(LinearExpressionTest, FromVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearExpression expr = a;

  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}}, 0)));
  EXPECT_EQ(expr.storage(), &storage);
  EXPECT_THAT(expr.terms(), UnorderedElementsAre(Pair(a, 1.0)));
}

TEST(LinearExpressionTest, FromLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const LinearExpression expr = LinearTerm(a, 3.0);

  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}}, 0)));
  EXPECT_EQ(expr.storage(), &storage);
  EXPECT_THAT(expr.terms(), UnorderedElementsAre(Pair(a, 3.0)));
}

TEST(LinearExpressionTest, OutputStreaming) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  auto to_string = [](LinearExpression expression) {
    std::ostringstream oss;
    oss << expression;
    return oss.str();
  };

  EXPECT_EQ(to_string(LinearExpression()), "0");
  EXPECT_EQ(to_string(LinearExpression({}, -1)), "-1");
  EXPECT_EQ(to_string(LinearExpression({}, -1)), "-1");
  EXPECT_EQ(to_string(LinearExpression({{a, 0}}, -1)), "-1");
  EXPECT_EQ(to_string(LinearExpression({{a, 3}, {b, 5}, {a, -2}}, -1)),
            "a + 5*b - 1");
  EXPECT_EQ(to_string(LinearExpression({{a, -1.0}, {b, -1.0}}, -2.0)),
            "-a - b - 2");
  EXPECT_EQ(to_string(LinearExpression({{a, kNaN}, {b, -kNaN}}, -kNaN)),
            "nan*a + nan*b + nan");
  EXPECT_EQ(to_string(LinearExpression({{a, kRoundTripTestNumber}})),
            absl::StrCat(kRoundTripTestNumberStr, "*a"));
  EXPECT_EQ(to_string(LinearExpression({}, kRoundTripTestNumber)),
            kRoundTripTestNumberStr);
}

TEST(LinearExpressionTest, Negation) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = -LinearExpression({{a, 3}, {b, -2}}, 5);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -3}, {b, 2}}, -5)));
}

TEST(LinearExpressionTest, AdditionAssignmentDouble) {
  LinearExpression expr;

  ResetExpressionCounters();
  expr += 3;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({}, 3)));

  ResetExpressionCounters();
  expr += -2;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({}, 1)));
}

TEST(LinearExpressionTest, AdditionAssignmentVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  LinearExpression expr;
  ResetExpressionCounters();
  expr += a;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr += a;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr += b;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 1}}, 0)));
}

TEST(LinearExpressionDeathTest, AdditionAssignmentVariableOtherModel) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));

  LinearExpression expr;
  expr += a;
  EXPECT_DEATH(expr += b, kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionTest, AdditionAssignmentLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  LinearExpression expr;
  ResetExpressionCounters();
  expr += LinearTerm(a, 3);
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr += LinearTerm(a, -2);
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr += LinearTerm(b, -5);
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}, {b, -5}}, 0)));
}

TEST(LinearExpressionDeathTest, AdditionAssignmentLinearTermOtherModel) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));

  LinearExpression expr;
  expr += LinearTerm(a, 3);
  EXPECT_DEATH(expr += LinearTerm(b, 2), kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionTest, AdditionAssignmentSelf) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression expr({{a, 2}, {b, 4}}, 2);
  ResetExpressionCounters();
  expr += expr;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 4}, {b, 8}}, 4)));
}

TEST(LinearExpressionTest, AdditionAssignmentOtherExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  // First test with a default expression, not associated with any ModelStorage.
  LinearExpression expr;
  expr += LinearExpression({{a, 2}, {b, 4}}, 2);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 4}}, 2)));

  // Then add another expression with variables from the same ModelStorage.
  expr += LinearExpression({{a, -3}, {c, 6}}, -4);
  EXPECT_THAT(expr,
              IsIdentical(LinearExpression({{a, -1}, {b, 4}, {c, 6}}, -2)));

  // Then add another expression without variables (i.e. having null
  // ModelStorage).
  expr += LinearExpression({}, 3);
  EXPECT_THAT(expr,
              IsIdentical(LinearExpression({{a, -1}, {b, 4}, {c, 6}}, 1)));
}

TEST(LinearExpressionDeathTest, AdditionAssignmentOtherExpressionAndModel) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));

  LinearExpression expr({{a, 1}}, 0);
  const LinearExpression other({{b, 1}}, 0);
  EXPECT_DEATH(expr += other, kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionTest, SubtractionAssignmentDouble) {
  LinearExpression expr;

  ResetExpressionCounters();
  expr -= 3;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({}, -3)));

  ResetExpressionCounters();
  expr -= -2;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({}, -1)));
}

TEST(LinearExpressionTest, SubtractionAssignmentVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  LinearExpression expr;
  ResetExpressionCounters();
  expr -= a;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -1}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr -= a;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -2}}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr -= b;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -2}, {b, -1}}, 0)));
}

TEST(LinearExpressionDeathTest, SubtractionAssignmentVariableOtherModel) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));

  LinearExpression expr;
  expr -= a;
  EXPECT_DEATH(expr -= b, kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionTest, SubtractionAssignmentLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  LinearExpression expr;
  ResetExpressionCounters();
  expr -= LinearTerm(a, 3);
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -3}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr -= LinearTerm(a, -2);
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -1}}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr -= LinearTerm(b, 5);
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -1}, {b, -5}}, 0)));
}

TEST(LinearExpressionDeathTest, SubtractionAssignmentLinearTermOtherModel) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));

  LinearExpression expr;
  expr -= LinearTerm(a, 3);
  EXPECT_DEATH(expr -= LinearTerm(b, 2), kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionTest, SubtractionAssignmentOtherExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  // First test with a default expression, not associated with any Model.
  LinearExpression expr;
  expr -= LinearExpression({{a, 2}, {b, 4}}, 2);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -2}, {b, -4}}, -2)));

  // Then subtract another expression with variables from the same Model.
  expr -= LinearExpression({{a, -3}, {c, 6}}, -4);
  EXPECT_THAT(expr,
              IsIdentical(LinearExpression({{a, 1}, {b, -4}, {c, -6}}, 2)));

  // Then subtract another expression without variables (i.e. having null
  // ModelStorage).
  expr -= LinearExpression({}, 3);
  EXPECT_THAT(expr,
              IsIdentical(LinearExpression({{a, 1}, {b, -4}, {c, -6}}, -1)));
}

TEST(LinearExpressionTest, SubtractionAssignmentSelf) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression expr({{a, 2}, {b, 4}}, 2);
  ResetExpressionCounters();
  expr -= expr;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 0}, {b, 0}}, 0)));
}

TEST(LinearExpressionDeathTest, SubtractionAssignmentOtherExpressionAndModel) {
  ModelStorage model_a;
  const Variable a(&model_a, model_a.AddVariable("a"));

  ModelStorage model_b;
  const Variable b(&model_b, model_b.AddVariable("b"));

  LinearExpression expr({{a, 1}}, 0);
  const LinearExpression other({{b, 1}}, 0);
  EXPECT_DEATH(expr -= other, kObjectsFromOtherModelStorage);
}

TEST(LinearExpressionTest, VariablePlusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = a + 3;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}}, 3)));
}

TEST(LinearExpressionTest, DoublePlusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 + a;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}}, 3)));
}

TEST(LinearExpressionTest, LinearTermPlusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 2 * a + 3;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}}, 3)));
}

TEST(LinearExpressionTest, DoublePlusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 + 2 * a;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}}, 3)));
}

TEST(LinearExpressionTest, LinearTermPlusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 * a + 2 * a;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 5}}, 0)));
}

TEST(LinearExpressionTest, LinearTermPlusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 * a + b;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}, {b, 1}}, 0)));
}

TEST(LinearExpressionTest, VariablePlusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = a + 2 * a;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}}, 0)));
}

TEST(LinearExpressionTest, VariablePlusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = a + b;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}, {b, 1}}, 0)));
}

TEST(LinearExpressionTest, ExpressionPlusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (2 * a + b + 1) + 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 1}}, 6)));
}

TEST(LinearExpressionTest, DoublePlusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = 5 + (2 * a + b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 1}}, 6)));
}

TEST(LinearExpressionTest, ExpressionPlusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (2 * a + b + 1) + b;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 2}}, 1)));
}

TEST(LinearExpressionTest, VariablePlusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = b + (2 * a + b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 2}}, 1)));
}

TEST(LinearExpressionTest, ExpressionPlusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (2 * a + b + 1) + 3 * b;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 4}}, 1)));
}

TEST(LinearExpressionTest, LinearTermPlusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 * b + (2 * a + b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 4}}, 1)));
}

TEST(LinearExpressionTest, ExpressionPlusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (3 * b + a + 2) + (2 * a + b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}, {b, 4}}, 3)));
}

TEST(LinearExpressionTest, VariableMinusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = a - 3;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}}, -3)));
}

TEST(LinearExpressionTest, DoubleMinusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 - a;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -1}}, 3)));
}

TEST(LinearExpressionTest, LinearTermMinusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 2 * a - 3;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}}, -3)));
}

TEST(LinearExpressionTest, DoubleMinusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 - 2 * a;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -2}}, 3)));
}

TEST(LinearExpressionTest, LinearTermMinusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 * a - 2 * a;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}}, 0)));
}

TEST(LinearExpressionTest, LinearTermMinusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 * a - b;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}, {b, -1}}, 0)));
}

TEST(LinearExpressionTest, VariableMinusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LinearExpression expr = a - 2 * a;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -1}}, 0)));
}

TEST(LinearExpressionTest, VariableMinusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = a - b;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}, {b, -1}}, 0)));
}

TEST(LinearExpressionTest, ExpressionMinusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (2 * a + b + 1) - 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 1}}, -4)));
}

TEST(LinearExpressionTest, DoubleMinusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = 5 - (2 * a + b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -2}, {b, -1}}, 4)));
}

TEST(LinearExpressionTest, ExpressionMinusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (2 * a + 2 * b + 1) - b;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, 1}}, 1)));
}

TEST(LinearExpressionTest, VariableMinusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = b - (2 * a + 2 * b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -2}, {b, -1}}, -1)));
}

TEST(LinearExpressionTest, ExpressionMinusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (2 * a + b + 1) - 3 * b;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 2}, {b, -2}}, 1)));
}

TEST(LinearExpressionTest, LinearTermMinusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = 3 * b - (2 * a + b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -2}, {b, 2}}, -1)));
}

TEST(LinearExpressionTest, ExpressionMinusExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = (3 * b + a + 2) - (2 * a + b + 1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, -1}, {b, 2}}, 1)));
}

TEST(LinearExpressionTest, ExpressionTimesAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression expr({{a, 3}, {b, 2}}, -2);
  ResetExpressionCounters();
  expr *= 2;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 6}, {b, 4}}, -4)));
}

TEST(LinearExpressionTest, DoubleTimesExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = 2 * LinearExpression({{a, 3}, {b, 2}}, -2);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 6}, {b, 4}}, -4)));
}

TEST(LinearExpressionTest, ExpressionTimesDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = LinearExpression({{a, 3}, {b, 2}}, -2) * 2;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 6}, {b, 4}}, -4)));
}

TEST(LinearExpressionTest, ExpressionDividedByAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression expr({{a, 3}, {b, 2}}, -2);
  ResetExpressionCounters();
  expr /= 2;
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1.5}, {b, 1}}, -1)));
}

TEST(LinearExpressionTest, ExpressionDividedByDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LinearExpression expr = LinearExpression({{a, 3}, {b, 2}}, -2) / 2;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1.5}, {b, 1}}, -1)));
}

TEST(LinearExpressionTest, AddSumInts) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<int> to_add = {2, 7};
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}}, 14)));
}

TEST(LinearExpressionTest, AddSumDoubles) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<double> to_add({2.0, 7.0});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}}, 14)));
}

TEST(LinearExpressionTest, AddSumVariables) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<Variable> to_add({b, c, b});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}, {b, 2}, {c, 1}}, 5)));
}

TEST(LinearExpressionTest, AddSumLinearTerms) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<LinearTerm> to_add({2.0 * b, 1.0 * c, 4.0 * b});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}, {b, 6}, {c, 1}}, 5)));
}

TEST(LinearExpressionTest, AddSumLinearExpressions) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<LinearExpression> to_add({a + b, 4.0 * b - 1.0});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 4}, {b, 5}}, 4)));
}

TEST(LinearExpressionTest, Sum) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));
  const std::vector<Variable> summand({a, b, c, b});
  const auto expr = Sum(summand);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 1}, {b, 2}, {c, 1}}, 0)));
}

TEST(LinearExpressionTest, AddInnerProductIntInt) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<int> first = {2, 3, 4};
  const std::vector<int> second = {1, -1, 10};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}}, 44)));
}

TEST(LinearExpressionTest, AddInnerProductDoubleDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<double> first = {2.0, 3.0, 4.0};
  const std::vector<double> second = {1.0, -1.0, 10.0};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 3}}, 44)));
}

TEST(LinearExpressionTest, AddInnerProductDoubleVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<double> first = {2.0, 3.0, 4.0};
  const std::vector<Variable> second = {a, b, a};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 9}, {b, 3}}, 5)));
}

TEST(LinearExpressionTest, AddInnerProductVariableInt) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<Variable> first = {a, b, a};
  const std::vector<int> second = {2, 3, 4};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 9}, {b, 3}}, 5)));
}

TEST(LinearExpressionTest, AddInnerProductIntLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<int> first = {2, 3, 4};
  const std::vector<LinearTerm> second = {2.0 * a, 4.0 * b, 1.0 * a};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 11}, {b, 12}}, 5)));
}

TEST(LinearExpressionTest, AddInnerProductDoubleLinearExpr) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  LinearExpression expr = 3.0 * a + 5.0;
  const std::vector<LinearExpression> first = {3.0 * b + 1, a + b};
  const std::vector<double> second = {-1.0, 2.0};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 5}, {b, -1}}, 4)));
}

TEST(LinearExpressionTest, InnerProduct) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const std::vector<Variable> first = {a, b, a};
  const std::vector<double> second = {2.0, 3.0, 4.0};
  const auto expr = InnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(LinearExpression({{a, 6}, {b, 3}}, 0)));
}

TEST(LinearExpressionDeathTest, AddInnerProductSizeMismatchLeftMore) {
  const std::vector<double> left = {2.0, 3.0, 4.0};
  const std::vector<double> right = {1.0, -1.0};
  LinearExpression expr;
  EXPECT_DEATH(expr.AddInnerProduct(left, right), "left had more");
}

TEST(LinearExpressionDeathTest, AddInnerProductSizeMismatchRightMore) {
  const std::vector<double> left = {2.0, 3.0};
  const std::vector<double> right = {1.0, -1.0, 10.0};
  LinearExpression expr;
  EXPECT_DEATH(expr.AddInnerProduct(left, right), "right had more");
}

TEST(LinearExpressionTest, ExpressionGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LowerBoundedLinearExpression comparison = 3 * a + b + 2 >= 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}, {b, 1}}, 2)));
  EXPECT_EQ(comparison.lower_bound, 5.0);
}

TEST(LinearExpressionTest, DoubleLesserEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const LowerBoundedLinearExpression comparison = 5 <= 3 * a + b + 2;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}, {b, 1}}, 2)));
  EXPECT_EQ(comparison.lower_bound, 5.0);
}

TEST(LinearExpressionTest, LinearTermGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LowerBoundedLinearExpression comparison = 3 * a >= 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}}, 0)));
  EXPECT_EQ(comparison.lower_bound, 5.0);
}

TEST(LinearExpressionTest, DoubleLesserEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LowerBoundedLinearExpression comparison = 5 <= 3 * a;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}}, 0)));
  EXPECT_EQ(comparison.lower_bound, 5.0);
}

TEST(LinearExpressionTest, VariableGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LowerBoundedLinearExpression comparison = a >= 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 1}}, 0)));
  EXPECT_EQ(comparison.lower_bound, 5.0);
}

TEST(LinearExpressionTest, DoubleLesserEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const LowerBoundedLinearExpression comparison = 5 <= a;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 1}}, 0)));
  EXPECT_EQ(comparison.lower_bound, 5.0);
}

TEST(LinearExpressionTest, ExpressionLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const UpperBoundedLinearExpression comparison = 3 * a + b + 2 <= 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}, {b, 1}}, 2)));
  EXPECT_EQ(comparison.upper_bound, 5.0);
}

TEST(LinearExpressionTest, DoubleGreaterEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const UpperBoundedLinearExpression comparison = 5 >= 3 * a + b + 2;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}, {b, 1}}, 2)));
  EXPECT_EQ(comparison.upper_bound, 5.0);
}

TEST(LinearExpressionTest, LinearTermLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const UpperBoundedLinearExpression comparison = 3 * a <= 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}}, 0)));
  EXPECT_EQ(comparison.upper_bound, 5.0);
}

TEST(LinearExpressionTest, DoubleGreaterEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const UpperBoundedLinearExpression comparison = 5 >= 3 * a;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 3}}, 0)));
  EXPECT_EQ(comparison.upper_bound, 5.0);
}

TEST(LinearExpressionTest, VariableLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const UpperBoundedLinearExpression comparison = a <= 5;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 1}}, 0)));
  EXPECT_EQ(comparison.upper_bound, 5.0);
}

TEST(LinearExpressionTest, DoubleGreaterEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const UpperBoundedLinearExpression comparison = 5 >= a;
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(LinearExpression({{a, 1}}, 0)));
  EXPECT_EQ(comparison.upper_bound, 5.0);
}

TEST(LinearExpressionTest, LowerBoundedExpressionLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = (2 <= 2 * a + 3 * b + 5) <= 4;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -3.0, LinearTerms({{a, 2}, {b, 3}}), -1.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 5);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, DoubleLesserEqualExpressionLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 <= 2 * a + 3 * b + 5 <= 4;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -3.0, LinearTerms({{a, 2}, {b, 3}}), -1.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 5);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, DoubleGreaterEqualLowerBoundedExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 4 >= (2 * a + 3 * b + 5 >= 2);
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -3.0, LinearTerms({{a, 2}, {b, 3}}), -1.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 5);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, DoubleGreaterEqualExpressionGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 4 >= 2 * a + 3 * b + 5 >= 2;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -3.0, LinearTerms({{a, 2}, {b, 3}}), -1.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 5);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, DoubleLesserEqualUpperBoundedExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 <= (2 * a + 3 * b + 5 <= 4);
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -3.0, LinearTerms({{a, 2}, {b, 3}}), -1.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 5);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, UpperBoundedExpressionGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = (4 >= 2 * a + 3 * b + 5) >= 2;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -3.0, LinearTerms({{a, 2}, {b, 3}}), -1.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 5);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, ExpressionLesserEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 <= a + 3;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, 1}, {b, 3}}), -2.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 2);
}

TEST(LinearExpressionTest, ExpressionLesserEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 <= 2 * b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, 2}, {b, 1}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermLesserEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * b <= 2 * a + 3 * b + 5;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, 2}, {b, 1}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableLesserEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = b <= 2 * a + 3 * b + 5;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, 2}, {b, 2}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, ExpressionLesserEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 <= b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, 2}, {b, 2}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, ExpressionGreaterEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 >= a + 3;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -2.0, LinearTerms({{a, 1}, {b, 3}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 2);
}

TEST(LinearExpressionTest, ExpressionGreaterEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 >= 2 * b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, 2}, {b, 1}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermGreaterEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * b >= 2 * a + 3 * b + 5;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, 2}, {b, 1}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableGreaterEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = b >= 2 * a + 3 * b + 5;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, 2}, {b, 2}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, ExpressionGreaterEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 >= b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, 2}, {b, 2}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermLesserEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a <= 2 * b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, 2}, {b, -2}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableLesserEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = b <= 2 * a;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, -2}, {b, 1}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableLesserEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = b <= a;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, -1}, {b, 1}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermLesserEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a <= b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -kInf, LinearTerms({{a, 2}, {b, -1}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermGreaterEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a >= 2 * b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              0.0, LinearTerms({{a, 2}, {b, -2}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableGreaterEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = b >= 2 * a;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              0.0, LinearTerms({{a, -2}, {b, 1}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableGreaterEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = b >= a;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              0.0, LinearTerms({{a, -1}, {b, 1}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermGreaterEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a >= b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              0.0, LinearTerms({{a, 2}, {b, -1}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, ExpressionEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 == 3 * a + b + 2;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -3.0, LinearTerms({{a, -1}, {b, 2}}), -3.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 2);
}

TEST(LinearExpressionTest, ExpressionEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 == 3 * a;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, -1}, {b, 3}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 3 * a == 2 * a + 3 * b + 5;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, -1}, {b, 3}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, ExpressionEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 == a;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, 1}, {b, 3}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = a == 2 * a + 3 * b + 5;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -5.0, LinearTerms({{a, 1}, {b, 3}}), -5.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, ExpressionEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a + 3 * b + 5 == 3;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -2.0, LinearTerms({{a, 2}, {b, 3}}), -2.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, DoubleEqualExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 3 == 2 * a + 3 * b + 5;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              -2.0, LinearTerms({{a, 2}, {b, 3}}), -2.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a == 3 * a;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(0.0, LinearTerms({{a, -1}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a == a;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(0.0, LinearTerms({{a, 1}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = a == 2 * a;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(0.0, LinearTerms({{a, -1}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, LinearTermEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 2 * a == 3;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(3.0, LinearTerms({{a, 2}}), 3.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, DoubleEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 3 == 2 * a;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(3.0, LinearTerms({{a, 2}}), 3.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = a == b;
  EXPECT_THAT(comparison, BoundedLinearExpressionEquiv(
                              0.0, LinearTerms({{a, 1}, {b, -1}}), 0.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, VariableEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = a == 3;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(3.0, LinearTerms({{a, 1}}), 3.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(LinearExpressionTest, DoubleEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 3 == a;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(3.0, LinearTerms({{a, 1}}), 3.0));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(BoundedLinearExpressionTest, FromLowerBoundedExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = 3 <= a;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(3.0, LinearTerms({{a, 1}}), kInf));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(BoundedLinearExpressionTest, FromUpperBoundedExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedLinearExpression comparison = a <= 5;
  EXPECT_THAT(comparison,
              BoundedLinearExpressionEquiv(-kInf, LinearTerms({{a, 1}}), 5));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 1);
}

TEST(BoundedLinearExpressionTest, OutputStreaming) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  auto to_string = [](BoundedLinearExpression bounded_expression) {
    std::ostringstream oss;
    oss << bounded_expression;
    return oss.str();
  };

  EXPECT_EQ(to_string(BoundedLinearExpression(LinearExpression(), -1, 2)),
            "-1 ≤ 0 ≤ 2");
  EXPECT_EQ(to_string(BoundedLinearExpression(LinearExpression({}, -1), -1, 2)),
            "-1 ≤ -1 ≤ 2");
  EXPECT_EQ(to_string(BoundedLinearExpression(
                LinearExpression({{a, 1}, {b, 5}}, -1), -1, 2)),
            "-1 ≤ a + 5*b - 1 ≤ 2");
  EXPECT_EQ(to_string(BoundedLinearExpression(
                LinearExpression({{a, 1}, {b, 5}}, 0), -1, 2)),
            "-1 ≤ a + 5*b ≤ 2");
  EXPECT_EQ(
      to_string(BoundedLinearExpression(LinearExpression({a, 2}), -kInf, 2)),
      "2*a ≤ 2");
  EXPECT_EQ(
      to_string(BoundedLinearExpression(LinearExpression({a, 2}), -1, kInf)),
      "2*a ≥ -1");
  EXPECT_EQ(to_string(BoundedLinearExpression(LinearExpression({a, 2}), 3, 3)),
            "2*a = 3");
  EXPECT_EQ(to_string(BoundedLinearExpression(LinearExpression({a, 2}),
                                              kRoundTripTestNumber, kInf)),
            absl::StrCat("2*a ≥ ", kRoundTripTestNumberStr));
  EXPECT_EQ(to_string(BoundedLinearExpression(LinearExpression({a, 2}), -kInf,
                                              kRoundTripTestNumber)),
            absl::StrCat("2*a ≤ ", kRoundTripTestNumberStr));
  EXPECT_EQ(to_string(BoundedLinearExpression(LinearExpression({a, 2}), 0.0,
                                              kRoundTripTestNumber)),
            absl::StrCat("0 ≤ 2*a ≤ ", kRoundTripTestNumberStr));
  EXPECT_EQ(to_string(BoundedLinearExpression(LinearExpression({a, 2}),
                                              kRoundTripTestNumber, 3000.0)),
            absl::StrCat(kRoundTripTestNumberStr, " ≤ 2*a ≤ 3000"));
}

////////////////////////////////////////////////////////////////////////////////
// Quadratic tests
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// QuadraticTermKey
////////////////////////////////////////////////////////////////////////////////

TEST(QuadraticTermKeyTest, Constructors) {
  ModelStorage storage;
  const VariableId u_id = storage.AddVariable();
  const VariableId v_id = storage.AddVariable();

  {
    const QuadraticTermKey in_order_key(&storage, {u_id, v_id});
    EXPECT_EQ(in_order_key.storage(), &storage);
    EXPECT_EQ(in_order_key.typed_id(), QuadraticProductId(u_id, v_id));

    const QuadraticTermKey out_of_order_key(&storage, {v_id, u_id});
    EXPECT_EQ(in_order_key, in_order_key);
  }

  const Variable u(&storage, u_id);
  const Variable v(&storage, v_id);
  {
    const QuadraticTermKey in_order_key(u, v);
    EXPECT_EQ(in_order_key.storage(), &storage);
    EXPECT_EQ(in_order_key.typed_id(), QuadraticProductId(u_id, v_id));

    const QuadraticTermKey out_of_order_key(v, u);
    EXPECT_EQ(in_order_key, in_order_key);
  }
}

TEST(QuadraticTermKeyDeathTest, ConstructorChecksOnDifferentModels) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable b(&other_storage, storage.AddVariable("b"));

  EXPECT_DEATH(QuadraticTermKey(a, b), internal::kObjectsFromOtherModelStorage);
}

TEST(QuadraticTermKeyTest, Accessors) {
  ModelStorage storage;
  const VariableId u_id = storage.AddVariable();
  const VariableId v_id = storage.AddVariable();

  const QuadraticProductId id(u_id, v_id);
  const QuadraticTermKey key(&storage, id);
  EXPECT_EQ(key.storage(), &storage);
  const ModelStorage& const_model = storage;
  EXPECT_EQ(key.storage(), &const_model);
  EXPECT_EQ(key.typed_id(), id);
  EXPECT_EQ(key.first().typed_id(), u_id);
  EXPECT_EQ(key.second().typed_id(), v_id);
  EXPECT_EQ(key.first().storage(), &const_model);
  EXPECT_EQ(key.second().storage(), &const_model);
}

TEST(QuadraticTermKeyTest, OutputStreaming) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable());

  auto to_string = [](QuadraticTermKey k) {
    std::ostringstream oss;
    oss << k;
    return oss.str();
  };

  EXPECT_EQ(to_string(QuadraticTermKey(a, a)), "(a, a)");
  EXPECT_EQ(to_string(QuadraticTermKey(a, b)),
            absl::StrCat("(a, __var#", b.id(), "__)"));
}

TEST(QuadraticTermKeyTest, EqualityComparison) {
  ModelStorage storage;
  const VariableId u_id = storage.AddVariable();
  const VariableId v_id = storage.AddVariable();
  const QuadraticProductId qp_id(u_id, v_id);
  const QuadraticTermKey key(&storage, qp_id);
  EXPECT_TRUE(key == QuadraticTermKey(&storage, qp_id));
  EXPECT_FALSE(key != QuadraticTermKey(&storage, qp_id));
  EXPECT_FALSE(key ==
               QuadraticTermKey(&storage, QuadraticProductId(u_id, u_id)));
  EXPECT_TRUE(key !=
              QuadraticTermKey(&storage, QuadraticProductId(u_id, u_id)));

  const ModelStorage other_storage;
  EXPECT_FALSE(key == QuadraticTermKey(&other_storage, qp_id));
  EXPECT_TRUE(key != QuadraticTermKey(&other_storage, qp_id));
}

////////////////////////////////////////////////////////////////////////////////
// QuadraticTerm (no arithmetic)
////////////////////////////////////////////////////////////////////////////////

TEST(QuadraticTermTest, FromVariablesAndDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  {
    // Verify that the same variable can appear in both slots
    const QuadraticTerm term(a, a, 1.2);
    EXPECT_EQ(term.first_variable(), a);
    EXPECT_EQ(term.second_variable(), a);
    EXPECT_EQ(term.coefficient(), 1.2);
  }
  {
    const QuadraticTerm term(b, a, 1.2);
    EXPECT_EQ(term.first_variable(), b);
    EXPECT_EQ(term.second_variable(), a);
    EXPECT_EQ(term.coefficient(), 1.2);
  }
}

////////////////////////////////////////////////////////////////////////////////
// QuadraticExpression (no arithmetic)
////////////////////////////////////////////////////////////////////////////////

TEST(QuadraticExpressionTest, DefaultValue) {
  const QuadraticExpression expr;
  EXPECT_EQ(expr.offset(), 0.0);
  EXPECT_THAT(expr.linear_terms(), IsEmpty());
  EXPECT_EQ(expr.storage(), nullptr);
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
}

TEST(QuadraticExpressionTest, EmptyInitializerList) {
  const QuadraticExpression expr({}, {}, 0.0);
  EXPECT_EQ(expr.offset(), 0.0);
  EXPECT_THAT(expr.linear_terms(), IsEmpty());
  EXPECT_EQ(expr.storage(), nullptr);
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
}

TEST(QuadraticExpressionTest, FromDouble) {
  const QuadraticExpression expr = 4.0;

  EXPECT_EQ(expr.offset(), 4.0);
  EXPECT_THAT(expr.linear_terms(), IsEmpty());
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
  EXPECT_EQ(expr.storage(), nullptr);
  EXPECT_THAT(expr.linear_terms(), IsEmpty());
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
}

TEST(QuadraticExpressionTest, FromVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const QuadraticExpression expr = a;

  EXPECT_EQ(expr.offset(), 0.0);
  EXPECT_THAT(expr.linear_terms(), UnorderedElementsAre(Pair(a, 1.0)));
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
  EXPECT_EQ(expr.storage(), &storage);
  EXPECT_THAT(expr.linear_terms(), UnorderedElementsAre(Pair(a, 1.0)));
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
}

TEST(QuadraticExpressionTest, FromLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const QuadraticExpression expr = LinearTerm(a, 3.0);

  EXPECT_EQ(expr.offset(), 0.0);
  EXPECT_THAT(expr.linear_terms(), UnorderedElementsAre(Pair(a, 3.0)));
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
  EXPECT_EQ(expr.storage(), &storage);
  EXPECT_THAT(expr.linear_terms(), UnorderedElementsAre(Pair(a, 3.0)));
  EXPECT_THAT(expr.quadratic_terms(), IsEmpty());
}

TEST(QuadraticExpressionTest, FromLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  LinearExpression linear_expr({{a, 1.2}, {b, 1.3}}, 1.4);
  ResetExpressionCounters();

  const QuadraticExpression quadratic_expr = std::move(linear_expr);

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_THAT(quadratic_expr,
              IsIdentical(QuadraticExpression({}, {{a, 1.2}, {b, 1.3}}, 1.4)));
  // We attempt to test that the we successfully moved out of linear_expr. Since
  // there is an implicit move constructor call, we can only test that the
  // terms_ field is empty, but not that offset_ is set to zero as well.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_THAT(linear_expr.terms(), IsEmpty());
}

TEST(QuadraticExpressionTest, FromQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  const QuadraticExpression expr = QuadraticTerm(a, a, 3.0);

  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, a, 3}}, {}, 0)));
  EXPECT_EQ(expr.storage(), &storage);
  EXPECT_THAT(expr.linear_terms(), IsEmpty());
  EXPECT_THAT(expr.quadratic_terms(),
              UnorderedElementsAre(Pair(QuadraticTermKey(a, a), 3.0)));
}

TEST(QuadraticExpressionTest, TermsFromSameModelOk) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  const QuadraticExpression expr({{a, b, 1.2}, {c, a, 2.5}, {b, a, -1.1}},
                                 {{a, 1.3}}, 1.2);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression(
                        {{a, b, 1.2 - 1.1}, {a, c, 2.5}}, {{a, 1.3}}, 1.2)));
  EXPECT_EQ(expr.storage(), &storage);
}

TEST(QuadraticExpressionDeathTest, TermsFromDifferentModelsFail) {
  ModelStorage first_model;
  const Variable a(&first_model, first_model.AddVariable("a"));

  ModelStorage second_model;
  const Variable b(&second_model, second_model.AddVariable("b"));
  const Variable c(&second_model, second_model.AddVariable("c"));

  EXPECT_DEATH(QuadraticExpression({}, {{a, 3.0}, {b, 5.0}}, 0.0),
               kObjectsFromOtherModelStorage);
  EXPECT_DEATH(QuadraticExpression({{a, b, 1.2}}, {}, 0.0),
               kObjectsFromOtherModelStorage);
  EXPECT_DEATH(QuadraticExpression({{a, a, 1.4}, {b, c, 1.3}}, {}, 0.0),
               kObjectsFromOtherModelStorage);
  EXPECT_DEATH(QuadraticExpression({{b, c, 1.3}}, {{a, 1.4}}, 0.0),
               kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, ReassignDifferentModels) {
  ModelStorage first_model;
  const Variable a(&first_model, first_model.AddVariable("a"));
  const Variable b(&first_model, first_model.AddVariable("b"));
  const QuadraticExpression first_expr({{a, b, 1.0}}, {}, 5.7);

  ModelStorage second_model;
  const Variable c(&second_model, second_model.AddVariable("c"));
  QuadraticExpression second_expr_to_overwrite({}, {{c, 1.2}}, 3.4);

  second_expr_to_overwrite = first_expr;
  EXPECT_THAT(second_expr_to_overwrite,
              IsIdentical(QuadraticExpression({{a, b, 1.0}}, {}, 5.7)));
  EXPECT_EQ(second_expr_to_overwrite.storage(), &first_model);
}

TEST(QuadraticExpressionTest, MoveConstruction) {
  ModelStorage first_model;
  const Variable a(&first_model, first_model.AddVariable("a"));
  const Variable b(&first_model, first_model.AddVariable("b"));
  QuadraticExpression first_expr({{a, b, 1.0}}, {{a, 3.0}}, 5.7);

  const QuadraticExpression second_expr = std::move(first_expr);

  EXPECT_THAT(second_expr,
              IsIdentical(QuadraticExpression({{a, b, 1.0}}, {{a, 3.0}}, 5.7)));
  EXPECT_EQ(second_expr.storage(), &first_model);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_THAT(first_expr, IsIdentical(QuadraticExpression()));
  EXPECT_EQ(first_expr.storage(), nullptr);
}

TEST(QuadraticExpressionTest, MoveAssignment) {
  ModelStorage first_model;
  const Variable a(&first_model, first_model.AddVariable("a"));
  const Variable b(&first_model, first_model.AddVariable("b"));
  QuadraticExpression first_expr({{a, b, 1.0}}, {{a, 3.0}}, 5.7);

  ModelStorage second_model;
  const Variable c(&second_model, second_model.AddVariable("c"));
  QuadraticExpression second_expr_to_overwrite({}, {{c, 1.2}}, 3.4);

  second_expr_to_overwrite = std::move(first_expr);
  EXPECT_THAT(second_expr_to_overwrite,
              IsIdentical(QuadraticExpression({{a, b, 1.0}}, {{a, 3.0}}, 5.7)));
  EXPECT_EQ(second_expr_to_overwrite.storage(), &first_model);

  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_THAT(first_expr, IsIdentical(QuadraticExpression()));
  EXPECT_EQ(first_expr.storage(), nullptr);
}

TEST(QuadraticExpressionTest, EvaluateEmpty) {
  const QuadraticExpression expr;
  {
    ModelStorage storage;
    const Variable a(&storage, storage.AddVariable("a"));
    const Variable b(&storage, storage.AddVariable("b"));
    VariableMap<double> variable_values;
    variable_values[a] = 10.0;
    variable_values[b] = 11.0;
    EXPECT_EQ(expr.Evaluate(variable_values), 0.0);
    EXPECT_EQ(expr.EvaluateWithDefaultZero(variable_values), 0.0);
  }
  {
    VariableMap<double> variable_values;
    EXPECT_EQ(expr.Evaluate(variable_values), 0.0);
    EXPECT_EQ(expr.EvaluateWithDefaultZero(variable_values), 0.0);
  }
}

TEST(QuadraticExpressionTest, EvaluateOnlyLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticExpression expr({}, {{a, 1.2}}, 3.4);
  {
    VariableMap<double> variable_values;
    variable_values[a] = 10.0;
    variable_values[b] = 11.0;
    EXPECT_THAT(expr.Evaluate(variable_values), 10 * 1.2 + 3.4);
    EXPECT_THAT(expr.EvaluateWithDefaultZero(variable_values), 10 * 1.2 + 3.4);
  }
  {
    VariableMap<double> variable_values;
    EXPECT_THAT(expr.EvaluateWithDefaultZero(variable_values), 3.4);
  }
}

TEST(QuadraticExpressionTest, SimpleEvaluate) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticExpression expr({{a, b, 1.0}}, {{a, 3.0}, {b, 5.0}}, 2.0);
  VariableMap<double> variable_values;
  variable_values[a] = 10.0;
  variable_values[b] = 100.0;
  EXPECT_THAT(expr.Evaluate(variable_values),
              1.0 * 10.0 * 100.0 + 3.0 * 10.0 + 5.0 * 100.0 + 2.0);
}

TEST(QuadraticExpressionTest, SimpleEvaluateWithDefault) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticExpression expr({{a, a, 4.0}, {b, a, 1.0}},
                                 {{b, 5.0}, {a, 3.0}}, 2.0);
  VariableMap<double> variable_values;
  variable_values[a] = 10.0;
  EXPECT_THAT(expr.EvaluateWithDefaultZero(variable_values),
              4.0 * 10.0 * 10.0 + 3.0 * 10.0 + 2.0);
}

TEST(QuadraticExpressionTest, StableEvaluateAndEvaluateWithDefault) {
  // Here we test that the floating point sum of numbers is done in the sorted
  // order of the variables ids and variables id pairs. To do so we rely on a
  // specific floating point number sequence (obtained with a Python script
  // doing random tries) which floating point sum changes depending on the order
  // of operations:
  //
  // 56.66114901664141 + 76.288516611269 + 73.11902164661139 +
  //   0.677336454040622 + 43.75820160525244 = 250.50422533381482
  // 56.66114901664141 + 76.288516611269 + 73.11902164661139 +
  //   43.75820160525244 + 0.677336454040622 = 250.50422533381484
  // 56.66114901664141 + 76.288516611269 + 0.677336454040622 +
  //   73.11902164661139 + 43.75820160525244 = 250.50422533381487
  // 76.288516611269 + 0.677336454040622 + 73.11902164661139 +
  //   43.75820160525244 + 56.66114901664141 = 250.5042253338149
  //
  // Here we will use the first value as the offset of the linear expression (to
  // test that it always taken into account in the same order).
  constexpr double kOffset = 56.66114901664141;
  const std::vector<double> linear_coeffs = {
      76.288516611269, 73.11902164661139, 0.677336454040622, 43.75820160525244};
  const std::vector<double> quadratic_coeffs = {
      76.288516611269, 0.677336454040622, 73.11902164661139, 43.75820160525244,
      56.66114901664141};

  ModelStorage storage;
  std::vector<Variable> vars;
  VariableMap<double> variable_values;
  for (int i = 0; i < linear_coeffs.size(); ++i) {
    vars.emplace_back(&storage, storage.AddVariable(absl::StrCat("v_", i)));
    variable_values.try_emplace(vars.back(), 1.0);
  }

  QuadraticExpression expr = kOffset;
  for (const int i : {3, 2, 0, 1}) {
    expr += linear_coeffs[i] * vars[i];
  }
  const std::vector<std::pair<int, int>> quad_term_keys = {
      {0, 0}, {0, 1}, {0, 2}, {1, 1}, {1, 2}};
  for (const int i : {4, 0, 3, 1, 2}) {
    const auto [v1, v2] = quad_term_keys[i];
    expr += quadratic_coeffs[i] * vars[v1] * vars[v2];
  }

  // Expected value for the sum which is:
  //   - offset first
  //   - then all linear terms sums in the order of variables' indices
  //   - then all quadratic terms sums in the order of variables' indices' pairs
  double expected = kOffset;
  for (const double v : linear_coeffs) {
    expected += v;
  }
  for (const double v : quadratic_coeffs) {
    expected += v;
  }

  // Test Evaluate();
  {
    const double got = expr.Evaluate(variable_values);
    EXPECT_EQ(got, expected)
        << "got: " << RoundTripDoubleFormat(got)
        << " expected: " << RoundTripDoubleFormat(expected);
  }

  // Test EvaluateWithDefaultZero();
  {
    const double got = expr.EvaluateWithDefaultZero(variable_values);
    EXPECT_EQ(got, expected)
        << "got: " << RoundTripDoubleFormat(got)
        << " expected: " << RoundTripDoubleFormat(expected);
  }
}

TEST(QuadraticExpressionDeathTest, EvaluateMissingVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticExpression expr({{b, a, 1.0}}, {{b, 5.0}, {a, 3.0}}, 2.0);
  VariableMap<double> variable_values;
  variable_values[a] = 10.0;
  EXPECT_DEATH(expr.Evaluate(variable_values), "");
}

TEST(QuadraticExpressionDeathTest, EvaluateDifferentModels) {
  ModelStorage first_model;
  const Variable a(&first_model, first_model.AddVariable("a"));
  const Variable b(&first_model, first_model.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.0}}, {}, 2.0);

  ModelStorage second_model;
  const Variable c(&second_model, second_model.AddVariable("c"));
  VariableMap<double> variable_values;
  variable_values[c] = 100.0;

  EXPECT_DEATH(expr.Evaluate(variable_values), kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, EvaluateWithDefaultZeroDifferentModels) {
  ModelStorage first_model;
  const Variable a(&first_model, first_model.AddVariable("a"));
  const Variable b(&first_model, first_model.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.0}}, {}, 2.0);

  ModelStorage second_model;
  const Variable c(&second_model, second_model.AddVariable("c"));
  VariableMap<double> variable_values;
  variable_values[c] = 100.0;

  EXPECT_EQ(expr.EvaluateWithDefaultZero(variable_values), 2.0);
}

TEST(QuadraticExpressionTest, OutputStreaming) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  auto to_string = [](QuadraticExpression expr) {
    std::ostringstream oss;
    oss << expr;
    return oss.str();
  };

  EXPECT_EQ(to_string(QuadraticExpression()), "0");
  EXPECT_EQ(to_string(QuadraticExpression({}, {}, -1)), "-1");
  EXPECT_EQ(to_string(QuadraticExpression(
                {}, {{a, 3}, {b, -5}, {a, -2}, {b, 0}}, -1)),
            "a - 5*b - 1");
  EXPECT_EQ(to_string(QuadraticExpression(
                {{a, b, -1.2}, {a, a, -1.3}, {b, b, 1.0}}, {{a, 1.4}}, 1.5)),
            "-1.3*a² - 1.2*a*b + b² + 1.4*a + 1.5");
  EXPECT_EQ(
      to_string(QuadraticExpression({{a, b, kRoundTripTestNumber}}, {}, 0.0)),
      absl::StrCat(kRoundTripTestNumberStr, "*a*b"));
  EXPECT_EQ(
      to_string(QuadraticExpression({}, {{a, kRoundTripTestNumber}}, 0.0)),
      absl::StrCat(kRoundTripTestNumberStr, "*a"));
  EXPECT_EQ(to_string(QuadraticExpression({}, {}, kRoundTripTestNumber)),
            kRoundTripTestNumberStr);
}

////////////////////////////////////////////////////////////////////////////////
// Arithmetic (non-member)
////////////////////////////////////////////////////////////////////////////////

// ----------------------------- Addition (+) ----------------------------------

TEST(QuadraticExpressionTest, DoublePlusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = 3.4 + term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2}}, {}, 3.4)));
}

TEST(QuadraticExpressionTest, DoublePlusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = 7.8 + std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{b, 3.4}},
                                                      7.8 + 5.6)));
}

TEST(QuadraticExpressionTest, VariablePlusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = b + term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{b, 1}}, 0)));
}

TEST(QuadraticExpressionDeathTest, VariablePlusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(b, c, 1.2);

  EXPECT_DEATH(a + other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, VariablePlusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = b + std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}}, {{b, 1 * 3.4 + 1}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest, VariablePlusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{b, c, 1.2}}, {{b, 3.4}}, 5.6);

  EXPECT_DEATH(a + other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearTermPlusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm first_term(a, 1.2);
  const QuadraticTerm second_term(b, a, 3.4);
  ResetExpressionCounters();

  const QuadraticExpression result = first_term + second_term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 3.4}}, {{a, 1.2}}, 0)));
}

TEST(QuadraticExpressionDeathTest, LinearTermPlusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm term(a, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearTerm other_term(c, 1.2);

  EXPECT_DEATH(term + other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearTermPlusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm term(a, 1.2);
  QuadraticExpression expr({{a, b, 3.4}}, {{b, 5.6}}, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = term + std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 3.4}}, {{a, 1.2}, {b, 5.6}}, 7.8)));
}

TEST(QuadraticExpressionDeathTest,
     LinearTermPlusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearTerm term(a, 1.2);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{b, c, 3.4}}, {{b, 5.6}}, 7.8);

  EXPECT_DEATH(term + other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearExpressionPlusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  LinearExpression expr({{a, 1.2}}, 3.4);
  const QuadraticTerm term(b, a, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) + term;

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 5.6}}, {{a, 1.2}}, 3.4)));
}

TEST(QuadraticExpressionDeathTest,
     LinearExpressionPlusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearExpression expr({{a, 1.2}}, 3.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(c, c, 5.6);

  EXPECT_DEATH(expr + other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearExpressionPlusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearExpression first_expr({{a, 1.2}}, 3.4);
  QuadraticExpression second_expr({{a, b, 5.6}}, {{b, 7.8}}, 9.0);
  ResetExpressionCounters();

  const QuadraticExpression result = first_expr + std::move(second_expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 5.6}}, {{a, 1.2}, {b, 7.8}}, 3.4 + 9.0)));
}

TEST(QuadraticExpressionDeathTest,
     LinearExpressionPlusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearExpression expr({{a, 1.2}}, 3.4);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{b, c, 5.6}}, {{b, 7.8}}, 9.0);

  EXPECT_DEATH(expr + other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermPlusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = term + 3.4;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2}}, {}, 3.4)));
}

TEST(QuadraticExpressionTest, QuadraticTermPlusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = term + a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{a, 1}}, 0)));
}

TEST(QuadraticExpressionDeathTest, QuadraticTermPlusVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable other_var(&other_storage, other_storage.AddVariable("c"));

  EXPECT_DEATH(term + other_var, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermPlusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm first_term(a, b, 1.2);
  const LinearTerm second_term(a, 3.4);
  ResetExpressionCounters();

  const QuadraticExpression result = first_term + second_term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{a, 3.4}}, 0)));
}

TEST(QuadraticExpressionDeathTest, QuadraticTermPlusLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearTerm other_term({c, 3.4});

  EXPECT_DEATH(term + other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermPlusLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  LinearExpression expr({{a, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = term + std::move(expr);

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{a, 3.4}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticTermPlusLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearExpression other_expr({{c, 1.2}}, 1.3);

  EXPECT_DEATH(term + other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermPlusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm first_term(a, b, 1.2);
  const QuadraticTerm second_term(b, b, 3.4);
  ResetExpressionCounters();

  const QuadraticExpression result = first_term + second_term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}, {b, b, 3.4}}, {}, 0)));
}

TEST(QuadraticExpressionDeathTest, QuadraticTermPlusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(c, c, 1.2);

  EXPECT_DEATH(term + other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermPlusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  QuadraticExpression expr({{a, b, 3.4}}, {{b, 5.6}}, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = term + std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2 + 3.4}},
                                                      {{b, 5.6}}, 7.8)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticTermPlusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const QuadraticTerm term(a, a, 1.2);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{b, c, 1.2}}, {{b, 1.3}}, 1.4);

  EXPECT_DEATH(term + other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionPlusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) + 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{b, 3.4}},
                                                      5.6 + 7.8)));
}

TEST(QuadraticExpressionTest, QuadraticExpressionPlusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) + a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}}, {{a, 1}, {b, 3.4}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest, QuadraticExpressionPlusVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable other_var(&other_storage, other_storage.AddVariable("c"));

  EXPECT_DEATH(expr + other_var, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionPlusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  const LinearTerm term(a, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) + term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}}, {{a, 7.8}, {b, 3.4}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionPlusLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearTerm other_term(c, 7.8);

  EXPECT_DEATH(expr + other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionPlusLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression first_expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  const LinearExpression second_expr({{a, 7.8}}, 9.0);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(first_expr) + second_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}}, {{a, 7.8}, {b, 3.4}}, 5.6 + 9)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionPlusLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearExpression other_expr({{c, 7.8}}, 9.0);

  EXPECT_DEATH(expr + other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionPlusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  const QuadraticTerm term(a, a, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) + term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, a, 7.8}, {a, b, 1.2}}, {{b, 3.4}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionPlusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(c, c, 7.8);

  EXPECT_DEATH(expr + other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionPlusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression first_expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  QuadraticExpression second_expr({{b, b, 7.8}}, {{a, 9.0}}, 1.3);
  ResetExpressionCounters();

  const QuadraticExpression result =
      std::move(first_expr) + std::move(second_expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}, {b, b, 7.8}},
                                              {{a, 9}, {b, 3.4}}, 5.6 + 1.3)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionPlusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{c, c, 1.2}}, {{c, 3.4}}, 5.6);

  EXPECT_DEATH(expr + other_expr, kObjectsFromOtherModelStorage);
}

// --------------------------- Subtraction (-) ---------------------------------

TEST(QuadraticTermTest, QuadraticTermNegation) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term = QuadraticTerm(a, b, 1.2);

  const QuadraticTerm result = -term;

  EXPECT_EQ(result.first_variable(), a);
  EXPECT_EQ(result.second_variable(), b);
  EXPECT_EQ(result.coefficient(), -1.2);
}

TEST(QuadraticExpressionTest, QuadraticExpressionNegation) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = -std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, -1.2}},
                                                      {{b, -3.4}}, -5.6)));
}

TEST(QuadraticExpressionTest, DoubleMinusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = 3.4 - term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, -1.2}}, {}, 3.4)));
}

TEST(QuadraticExpressionTest, DoubleMinusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = 7.8 - std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, -1.2}},
                                                      {{b, -3.4}}, 7.8 - 5.6)));
}

TEST(QuadraticExpressionTest, VariableMinusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = b - term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, -1.2}}, {{b, 1}}, 0)));
}

TEST(QuadraticExpressionDeathTest, VariableMinusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(b, c, 1.2);

  EXPECT_DEATH(a - other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, VariableMinusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = b - std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, -1.2}},
                                                      {{b, 1 - 3.4}}, -5.6)));
}

TEST(QuadraticExpressionDeathTest, VariableMinusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{b, c, 1.2}}, {{b, 1.3}}, 1.4);

  EXPECT_DEATH(a - other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearTermMinusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm first_term(a, 1.2);
  const QuadraticTerm second_term(b, a, 3.4);
  ResetExpressionCounters();

  const QuadraticExpression result = first_term - second_term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, -3.4}}, {{a, 1.2}}, 0)));
}

TEST(QuadraticExpressionDeathTest, LinearTermMinusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearTerm term(a, 1.2);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(b, c, 1.2);

  EXPECT_DEATH(term - other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearTermMinusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm term(a, 1.2);
  QuadraticExpression expr({{a, b, 3.4}}, {{b, 5.6}}, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = term - std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, -3.4}}, {{a, 1.2}, {b, -5.6}}, -7.8)));
}

TEST(QuadraticExpressionDeathTest,
     LinearTermMinusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearTerm term(a, 1.2);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{b, c, 1.2}}, {{b, 1.3}}, 1.4);

  EXPECT_DEATH(term - other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearExpressionMinusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  LinearExpression expr({{a, 1.2}}, 3.4);
  const QuadraticTerm term(b, a, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) - term;

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, -5.6}},
                                                      {{a, 1.2}}, 3.4)));
}

TEST(QuadraticExpressionDeathTest,
     LinearExpressionMinusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearExpression expr({{a, 1.2}}, 3.4);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(b, c, 5.6);

  EXPECT_DEATH(expr - other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearExpressionMinusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearExpression first_expr({{a, 1.2}}, 3.4);
  QuadraticExpression second_expr({{a, b, 5.6}}, {{b, 7.8}}, 9.0);
  ResetExpressionCounters();

  const QuadraticExpression result = first_expr - std::move(second_expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, -5.6}}, {{a, 1.2}, {b, -7.8}}, 3.4 - 9.0)));
}

TEST(QuadraticExpressionDeathTest,
     LinearExpressionMinusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearExpression expr({{a, 1.2}}, 3.4);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{b, c, 1.2}}, {{b, 1.3}}, 1.4);

  EXPECT_DEATH(expr - other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermMinusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = term - 3.4;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}}, {}, -3.4)));
}

TEST(QuadraticExpressionTest, QuadraticTermMinusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  ResetExpressionCounters();

  const QuadraticExpression result = term - a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{a, -1}}, 0)));
}

TEST(QuadraticExpressionDeathTest, QuadraticTermMinusVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable other_var(&other_storage, other_storage.AddVariable("c"));

  EXPECT_DEATH(term - other_var, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermMinusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm first_term(a, b, 1.2);
  const LinearTerm second_term(a, 3.4);
  ResetExpressionCounters();

  const QuadraticExpression result = first_term - second_term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{a, -3.4}}, 0)));
}

TEST(QuadraticExpressionDeathTest, QuadraticTermMinusLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearTerm other_term({c, 3.4});

  EXPECT_DEATH(term - other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermMinusLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  LinearExpression expr({{a, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = term - std::move(expr);

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2}},
                                                      {{a, -3.4}}, -5.6)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticTermMinusLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearExpression other_expr({{c, 1.2}}, 1.3);

  EXPECT_DEATH(term - other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermMinusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm first_term(a, b, 1.2);
  const QuadraticTerm second_term(b, b, 3.4);
  ResetExpressionCounters();

  const QuadraticExpression result = first_term - second_term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}, {b, b, -3.4}}, {}, 0)));
}

TEST(QuadraticExpressionDeathTest, QuadraticTermMinusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(c, c, 1.2);

  EXPECT_DEATH(term - other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticTermMinusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);
  QuadraticExpression expr({{a, b, 3.4}}, {{b, 5.6}}, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = term - std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2 - 3.4}},
                                                      {{b, -5.6}}, -7.8)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticTermMinusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{c, c, 1.2}}, {{c, 1.3}}, 1.4);

  EXPECT_DEATH(term - other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionMinusDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) - 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{b, 3.4}},
                                                      5.6 - 7.8)));
}

TEST(QuadraticExpressionTest, QuadraticExpressionMinusVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) - a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}}, {{a, -1}, {b, 3.4}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest, QuadraticExpressionMinusVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable other_var(&other_storage, other_storage.AddVariable("c"));

  EXPECT_DEATH(expr - other_var, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionMinusLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  const LinearTerm term(a, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) - term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}}, {{a, -7.8}, {b, 3.4}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionMinusLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearTerm other_term(c, 7.8);

  EXPECT_DEATH(expr - other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionMinusLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression first_expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  const LinearExpression second_expr({{a, 7.8}}, 9.0);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(first_expr) - second_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2}}, {{a, -7.8}, {b, 3.4}}, 5.6 - 9)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionMinusLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearExpression other_expr({{c, 7.8}}, 9.0);

  EXPECT_DEATH(expr - other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionMinusQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  const QuadraticTerm term(a, a, 7.8);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) - term;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, a, -7.8}, {a, b, 1.2}}, {{b, 3.4}}, 5.6)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionMinusQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(c, c, 7.8);

  EXPECT_DEATH(expr - other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, QuadraticExpressionMinusQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression first_expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  QuadraticExpression second_expr({{b, b, 7.8}}, {{a, 9.0}}, 1.3);
  ResetExpressionCounters();

  const QuadraticExpression result =
      std::move(first_expr) - std::move(second_expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result,
              IsIdentical(QuadraticExpression({{a, b, 1.2}, {b, b, -7.8}},
                                              {{a, -9}, {b, 3.4}}, 5.6 - 1.3)));
}

TEST(QuadraticExpressionDeathTest,
     QuadraticExpressionMinusQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_expr({{c, c, 1.2}}, {{c, 3.4}}, 5.6);

  EXPECT_DEATH(expr - other_expr, kObjectsFromOtherModelStorage);
}

// ---------------------------- Multiplication (*) -----------------------------

TEST(QuadraticTermTest, DoubleTimesQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  const QuadraticTerm result = 3.4 * term;

  EXPECT_EQ(result.first_variable(), a);
  EXPECT_EQ(result.second_variable(), b);
  EXPECT_THAT(result.coefficient(), 3.4 * 1.2);
}

TEST(QuadraticExpressionTest, DoubleTimesQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = 7.8 * std::move(expr);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 7.8 * 1.2}}, {{b, 7.8 * 3.4}}, 7.8 * 5.6)));
}

TEST(QuadraticTermTest, VariableTimesVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm result = a * b;

  EXPECT_EQ(result.coefficient(), 1.0);
  EXPECT_EQ(result.first_variable(), a);
  EXPECT_EQ(result.second_variable(), b);
}

TEST(QuadraticTermDeathTest, VariableTimesVariableOtherModel) {
  ModelStorage storage;
  const Variable var(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable other_var(&other_storage, other_storage.AddVariable("c"));

  EXPECT_DEATH(var * other_var, kObjectsFromOtherModelStorage);
}

TEST(QuadraticTermTest, VariableTimesLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm term(a, 1.2);

  const QuadraticTerm result = b * term;

  EXPECT_EQ(result.coefficient(), 1.2);
  EXPECT_EQ(result.first_variable(), b);
  EXPECT_EQ(result.second_variable(), a);
}

TEST(QuadraticTermDeathTest, VariableTimesLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const LinearTerm other_term(b, 1.2);

  EXPECT_DEATH(a * other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, VariableTimesLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  {
    const LinearExpression expr({{a, 1.2}, {b, 3.4}}, 5.6);
    ResetExpressionCounters();

    const QuadraticExpression result = a * expr;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                            {{a, a, 1.2}, {a, b, 3.4}}, {{a, 5.6}}, 0)));
  }

  // Now we test that we do not introduce extra terms if there is a zero offset.
  {
    const LinearExpression expr_no_offset({{a, 1.2}, {b, 3.4}}, 0.0);
    ResetExpressionCounters();

    const QuadraticExpression result_no_offset = a * expr_no_offset;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result_no_offset, IsIdentical(QuadraticExpression(
                                      {{a, a, 1.2}, {a, b, 3.4}}, {}, 0)));
  }
}

TEST(QuadraticExpressionDeathTest, VariableTimesLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const LinearExpression other_expr({{b, 1.2}}, 3.4);

  EXPECT_DEATH(a * other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticTermTest, LinearTermTimesVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm term = LinearTerm(a, 1.2);

  const QuadraticTerm result = term * b;

  EXPECT_EQ(result.coefficient(), 1.2);
  EXPECT_EQ(result.first_variable(), a);
  EXPECT_EQ(result.second_variable(), b);
}

TEST(QuadraticTermDeathTest, LinearTermTimesVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearTerm term = LinearTerm(a, 1.2);

  ModelStorage other_storage;
  const Variable other_var(&other_storage,
                           other_storage.AddVariable("other_var"));

  EXPECT_DEATH(term * other_var, kObjectsFromOtherModelStorage);
}

TEST(QuadraticTermTest, LinearTermTimesLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm first_term(a, 1.2);
  const LinearTerm second_term(b, 3.4);

  const QuadraticTerm result = first_term * second_term;

  EXPECT_THAT(result.coefficient(), 1.2 * 3.4);
  EXPECT_EQ(result.first_variable(), a);
  EXPECT_EQ(result.second_variable(), b);
}

TEST(QuadraticTermDeathTest, LinearTermTimesLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearTerm term(a, 1.2);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const LinearTerm other_term(b, 1.2);

  EXPECT_DEATH(term * other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearTermTimesLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm term(a, 1.2);
  {
    const LinearExpression expr({{a, 3.4}, {b, 5.6}}, 7.8);
    ResetExpressionCounters();

    const QuadraticExpression result = term * expr;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                            {{a, a, 1.2 * 3.4}, {a, b, 1.2 * 5.6}},
                            {{a, 1.2 * 7.8}}, 0)));
  }

  // Now we test that we do not introduce extra terms if there is a zero offset.
  {
    const LinearExpression expr_no_offset({{a, 3.4}, {b, 5.6}}, 0.0);
    ResetExpressionCounters();

    const QuadraticExpression result_no_offset = term * expr_no_offset;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result_no_offset,
                IsIdentical(QuadraticExpression(
                    {{a, a, 1.2 * 3.4}, {a, b, 1.2 * 5.6}}, {}, 0)));
  }
}

TEST(QuadraticExpressionDeathTest, LinearTermTimesLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearTerm term(a, 1.2);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const LinearExpression other_expr({{b, 3.4}}, 5.6);

  EXPECT_DEATH(term * other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearExpressionTimesVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  {
    const LinearExpression expr({{a, 1.2}, {b, 3.4}}, 5.6);
    ResetExpressionCounters();

    const QuadraticExpression result = expr * a;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                            {{a, a, 1.2}, {a, b, 3.4}}, {{a, 5.6}}, 0)));
  }

  // Now we test that we do not introduce extra terms if there is a zero offset.
  {
    const LinearExpression expr_no_offset({{a, 1.2}, {b, 3.4}}, 0.0);
    ResetExpressionCounters();

    const QuadraticExpression result_no_offset = expr_no_offset * a;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result_no_offset, IsIdentical(QuadraticExpression(
                                      {{a, a, 1.2}, {a, b, 3.4}}, {}, 0)));
  }
}

TEST(QuadraticExpressionDeathTest, LinearExpressionTimesVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearExpression expr({{a, 1.2}}, 3.4);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));

  EXPECT_DEATH(expr * b, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearExpressionTimesLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const LinearTerm term(a, 7.8);
  {
    const LinearExpression expr({{a, 1.2}, {b, 3.4}}, 5.6);
    ResetExpressionCounters();

    const QuadraticExpression result = expr * term;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                            {{a, a, 1.2 * 7.8}, {a, b, 3.4 * 7.8}},
                            {{a, 5.6 * 7.8}}, 0)));
  }

  // Now we test that we do not introduce extra terms if there is a zero offset.
  {
    const LinearExpression expr_no_offset({{a, 1.2}, {b, 3.4}}, 0.0);
    ResetExpressionCounters();

    const QuadraticExpression result_no_offset = expr_no_offset * term;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result_no_offset,
                IsIdentical(QuadraticExpression(
                    {{a, a, 1.2 * 7.8}, {a, b, 3.4 * 7.8}}, {}, 0)));
  }
}

TEST(QuadraticExpressionDeathTest, LinearExpressionTimesLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearExpression expr({{a, 1.2}}, 3.4);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const LinearTerm other_term(b, 5.6);

  EXPECT_DEATH(expr * other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, LinearExpressionTimesLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  {
    const LinearExpression expr({{a, 1.2}, {b, 3.4}}, 5.6);
    const LinearExpression other_expr({{a, 7.8}}, 9.0);
    ResetExpressionCounters();

    const QuadraticExpression result = expr * other_expr;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(result,
                IsIdentical(QuadraticExpression(
                    {{a, a, 1.2 * 7.8}, {a, b, 3.4 * 7.8}},
                    {{a, 1.2 * 9 + 5.6 * 7.8}, {b, 3.4 * 9}}, 5.6 * 9)));
  }

  // Now we test that we do not introduce extra terms if there is a zero offset
  // from the left-hand-side expression.
  {
    const LinearExpression expr_no_offset({{a, 1.2}, {b, 3.4}}, 0.0);
    const LinearExpression other_expr({{a, 7.8}}, 9.0);
    ResetExpressionCounters();

    const QuadraticExpression result_no_lhs_offset =
        expr_no_offset * other_expr;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(
        result_no_lhs_offset,
        IsIdentical(QuadraticExpression({{a, a, 1.2 * 7.8}, {a, b, 3.4 * 7.8}},
                                        {{a, 1.2 * 9}, {b, 3.4 * 9}}, 0)));
  }
  // Now we test that we do not introduce extra terms if there is a zero offset
  // from the right-hand-side expression.
  {
    const LinearExpression expr({{a, 1.2}, {b, 3.4}}, 5.6);
    const LinearExpression other_expr_no_offset({{a, 7.8}}, 0.0);
    ResetExpressionCounters();

    const QuadraticExpression result_no_rhs_offset =
        expr * other_expr_no_offset;

    EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
    EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
    EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
    EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
    EXPECT_THAT(
        result_no_rhs_offset,
        IsIdentical(QuadraticExpression({{a, a, 1.2 * 7.8}, {a, b, 3.4 * 7.8}},
                                        {{a, 5.6 * 7.8}}, 0)));
  }
}

TEST(QuadraticExpressionDeathTest,
     LinearExpressionTimesLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const LinearExpression expr({{a, 1.2}}, 3.4);

  ModelStorage other_storage;
  const Variable b(&other_storage, other_storage.AddVariable("b"));
  const LinearExpression other_expr({{b, 5.6}}, 7.8);

  EXPECT_DEATH(expr * other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticTermTest, QuadraticTermTimesDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  const QuadraticTerm result = term * 3.4;

  EXPECT_THAT(result.coefficient(), 1.2 * 3.4);
  EXPECT_EQ(result.first_variable(), a);
  EXPECT_EQ(result.second_variable(), b);
}

TEST(QuadraticExpressionTest, QuadraticExpressionTimesDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) * 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2 * 7.8}}, {{b, 3.4 * 7.8}}, 5.6 * 7.8)));
}

// ------------------------------- Division (/) --------------------------------
// 1 QuadraticTerm, 1 QuadraticExpression

TEST(QuadraticTermTest, QuadraticTermDividedByDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const QuadraticTerm term(a, b, 1.2);

  const QuadraticTerm result = term / 3.4;

  EXPECT_THAT(result.coefficient(), 1.2 / 3.4);
  EXPECT_EQ(result.first_variable(), a);
  EXPECT_EQ(result.second_variable(), b);
}

TEST(QuadraticExpressionTest, QuadraticExpressionDividedByDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  const QuadraticExpression result = std::move(expr) / 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_THAT(result, IsIdentical(QuadraticExpression(
                          {{a, b, 1.2 / 7.8}}, {{b, 3.4 / 7.8}}, 5.6 / 7.8)));
}

////////////////////////////////////////////////////////////////////////////////
// Arithmetic (assignment operators)
////////////////////////////////////////////////////////////////////////////////

// ----------------------------- Addition (+) ----------------------------------

TEST(QuadraticExpressionTest, AdditionAssignmentDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  expr += 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{b, 3.4}},
                                                    5.6 + 7.8)));
}

TEST(QuadraticExpressionTest, AdditionAssignmentVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  ResetExpressionCounters();

  expr += a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 1}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr += a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 2}}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr += b;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 2}, {b, 1}}, 0)));
}

TEST(QuadraticExpressionDeathTest, AdditionAssignmentVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable other_var(&other_storage,
                           other_storage.AddVariable("other_var"));

  EXPECT_DEATH(expr += other_var, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, AdditionAssignmentLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  ResetExpressionCounters();

  expr += LinearTerm(a, 3);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 3}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr += LinearTerm(a, -2);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 1}}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr += LinearTerm(b, -5);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 1}, {b, -5}}, 0)));
}

TEST(QuadraticExpressionDeathTest, AdditionAssignmentLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearTerm other_term(c, 1.0);

  EXPECT_DEATH(expr += other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, AdditionAssignmentLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  const LinearExpression another_expr({{a, 2}, {b, 4}}, 2);
  ResetExpressionCounters();

  expr += another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 2}, {b, 4}}, 2)));

  // Then add another expression with variables from the same ModelStorage.
  const LinearExpression yet_another_expr({{a, -3}, {b, 6}}, -4);
  ResetExpressionCounters();

  expr += yet_another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(
      expr, IsIdentical(QuadraticExpression({}, {{a, 2 - 3}, {b, 4 + 6}}, -2)));

  // Then add another expression without variables (i.e. having null
  // ModelStorage).
  const LinearExpression no_vars_expr({}, 3);
  ResetExpressionCounters();

  expr += no_vars_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(
      expr, IsIdentical(QuadraticExpression({}, {{a, 2 - 3}, {b, 4 + 6}}, 1)));
}

TEST(QuadraticExpressionDeathTest,
     AdditionAssignmentLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearExpression other_expr({{c, 1.0}}, 2.0);

  EXPECT_DEATH(expr += other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, AdditionAssignmentQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  ResetExpressionCounters();

  expr += QuadraticTerm(a, b, 3);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, b, 3}}, {}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr += QuadraticTerm(a, a, -2);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({{a, a, -2}, {a, b, 3}}, {}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr += QuadraticTerm(a, b, -4);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(
      expr, IsIdentical(QuadraticExpression({{a, a, -2}, {a, b, -1}}, {}, 0)));
}

TEST(QuadraticExpressionDeathTest, AdditionAssignmentQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(c, c, 1.2);

  EXPECT_DEATH(expr += other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, AdditionAssignmentQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  const QuadraticExpression another_expr({{a, c, 2.4}}, {{a, 2}, {b, 4}}, 2);
  ResetExpressionCounters();

  expr += another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, c, 2.4}},
                                                    {{a, 2}, {b, 4}}, 2)));

  // Then add another expression with variables from the same ModelStorage.
  const QuadraticExpression yet_another_expr({{c, b, 1.1}}, {{a, -3}, {c, 6}},
                                             -4);
  ResetExpressionCounters();

  expr += yet_another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({{a, c, 2.4}, {b, c, 1.1}},
                                              {{a, -1}, {b, 4}, {c, 6}}, -2)));

  // Then add another expression without variables (i.e. having null
  // ModelStorage).
  const QuadraticExpression no_vars_expr({}, {}, 3);
  ResetExpressionCounters();

  expr += no_vars_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({{a, c, 2.4}, {b, c, 1.1}},
                                              {{a, -1}, {b, 4}, {c, 6}}, 1)));
}

TEST(QuadraticExpressionDeathTest,
     AdditionAssignmentQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_term({{c, c, 1.2}}, {{c, 3.4}}, 5.6);

  EXPECT_DEATH(expr += other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, AdditionAssignmentSelf) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({}, {{a, 2}, {b, 4}}, 2.0);
  ResetExpressionCounters();

  expr += expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, 4}, {b, 8}}, 4)));
}

// --------------------------- Subtraction (-) ---------------------------------

TEST(QuadraticExpressionTest, SubtractionAssignmentDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  expr -= 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, b, 1.2}}, {{b, 3.4}},
                                                    5.6 - 7.8)));
}

TEST(QuadraticExpressionTest, SubtractionAssignmentVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  ResetExpressionCounters();

  expr -= a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, -1}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr -= a;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, -2}}, 0)));

  // Subtract another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr -= b;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({}, {{a, -2}, {b, -1}}, 0)));
}

TEST(QuadraticExpressionDeathTest, SubtractionAssignmentVariableOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));

  EXPECT_DEATH(expr -= c, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, SubtractionAssignmentLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  ResetExpressionCounters();

  expr -= LinearTerm(a, 3);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, -3}}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // subtracting the same variable.
  ResetExpressionCounters();
  expr -= LinearTerm(a, -2);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, -1}}, 0)));

  // Subtract another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr -= LinearTerm(b, -5);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({}, {{a, -1}, {b, 5}}, 0)));
}

TEST(QuadraticExpressionDeathTest, SubtractionAssignmentLinearTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearTerm other_term(c, 1.0);

  EXPECT_DEATH(expr -= other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, SubtractionAssignmentLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  const LinearExpression another_expr({{a, 2}, {b, 4}}, 2);
  ResetExpressionCounters();

  expr -= another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({}, {{a, -2}, {b, -4}}, -2)));

  // Then add another expression with variables from the same ModelStorage.
  const LinearExpression yet_another_expr({{a, -3}, {b, 6}}, -4);
  ResetExpressionCounters();

  expr -= yet_another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({}, {{a, 1}, {b, -10}}, 2)));

  // Then subtract another expression without variables (i.e. having null
  // ModelStorage).
  const LinearExpression no_vars_expr({}, 3);
  ResetExpressionCounters();

  expr -= no_vars_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({}, {{a, 1}, {b, -10}}, -1)));
}

TEST(QuadraticExpressionDeathTest,
     SubtractionAssignmentLinearExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const LinearExpression other_expr({{c, 1.0}}, 2.0);

  EXPECT_DEATH(expr -= other_expr, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, SubtractionAssignmentQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  ResetExpressionCounters();

  expr -= QuadraticTerm(a, b, 3);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, b, -3}}, {}, 0)));

  // Reuse the previous expression now connected to a ModelStorage to test
  // adding the same variable.
  ResetExpressionCounters();
  expr -= QuadraticTerm(a, a, -2);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({{a, a, 2}, {a, b, -3}}, {}, 0)));

  // Add another variable from the same ModelStorage.
  ResetExpressionCounters();
  expr -= QuadraticTerm(a, b, -4);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({{a, a, 2}, {a, b, 1}}, {}, 0)));
}

TEST(QuadraticExpressionDeathTest,
     SubtractionAssignmentQuadraticTermOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticTerm other_term(c, c, 1.2);

  EXPECT_DEATH(expr -= other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, SubtractionAssignmentQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  // First test with a default expression, not associated with any ModelStorage.
  QuadraticExpression expr;
  const QuadraticExpression another_expr({{a, c, 2.4}}, {{a, 2}, {b, 4}}, 2);
  ResetExpressionCounters();

  expr -= another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, c, -2.4}},
                                                    {{a, -2}, {b, -4}}, -2)));

  // Then add another expression with variables from the same ModelStorage.
  QuadraticExpression yet_another_expr({{c, b, 1.1}}, {{a, -3}, {c, 6}}, -4);
  ResetExpressionCounters();

  expr -= yet_another_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({{a, c, -2.4}, {b, c, -1.1}},
                                              {{a, 1}, {b, -4}, {c, -6}}, 2)));

  // Then add another expression without variables (i.e. having null
  // ModelStorage).
  const QuadraticExpression no_vars_expr({}, {}, 3);
  ResetExpressionCounters();

  expr -= no_vars_expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr,
              IsIdentical(QuadraticExpression({{a, c, -2.4}, {b, c, -1.1}},
                                              {{a, 1}, {b, -4}, {c, -6}}, -1)));
}

TEST(QuadraticExpressionDeathTest,
     SubtractionAssignmentQuadraticExpressionOtherModel) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 1.3}}, 1.4);

  ModelStorage other_storage;
  const Variable c(&other_storage, other_storage.AddVariable("c"));
  const QuadraticExpression other_term({{c, c, 1.2}}, {{c, 3.4}}, 5.6);

  EXPECT_DEATH(expr -= other_term, kObjectsFromOtherModelStorage);
}

TEST(QuadraticExpressionTest, SubtractionAssignmentSelf) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  expr -= expr;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression({{a, b, 1.2 - 1.2}},
                                                    {{b, 3.4 - 3.4}}, 0)));
}

// ---------------------------- Multiplication (*) -----------------------------

TEST(QuadraticTermTest, QuadraticTermTimesDoubleAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticTerm term = QuadraticTerm(a, b, 1.2);

  term *= 2.0;

  EXPECT_EQ(term.first_variable(), a);
  EXPECT_EQ(term.second_variable(), b);
  EXPECT_EQ(term.coefficient(), 2.4);
}

TEST(QuadraticExpressionTest, QuadraticExpressionTimesDoubleAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  expr *= 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression(
                        {{a, b, 1.2 * 7.8}}, {{b, 3.4 * 7.8}}, 5.6 * 7.8)));
}

// ------------------------------- Division (/) --------------------------------

TEST(QuadraticTermTest, QuadraticTermDividedByDoubleAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticTerm term = QuadraticTerm(a, b, 1.2);

  term /= 2.0;

  EXPECT_EQ(term.first_variable(), a);
  EXPECT_EQ(term.second_variable(), b);
  EXPECT_EQ(term.coefficient(), 0.6);
}

TEST(QuadraticExpressionTest, QuadraticExpressionDividedByDoubleAssignment) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr({{a, b, 1.2}}, {{b, 3.4}}, 5.6);
  ResetExpressionCounters();

  expr /= 7.8;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_THAT(expr, IsIdentical(QuadraticExpression(
                        {{a, b, 1.2 / 7.8}}, {{b, 3.4 / 7.8}}, 5.6 / 7.8)));
}

TEST(QuadraticExpressionTest, AddSumInts) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<int> to_add = {2, 7};
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 3 * a + 14));
}

TEST(QuadraticExpressionTest, AddSumDoubles) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<double> to_add({2.0, 7.0});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 3 * a + 14));
}

TEST(QuadraticExpressionTest, AddSumVariables) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<Variable> to_add({b, c, b});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 3 * a + 2 * b + c + 5));
}

TEST(QuadraticExpressionTest, AddSumLinearTerms) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const Variable c(&storage, storage.AddVariable("c"));

  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<LinearTerm> to_add({2.0 * b, 1.0 * c, 4.0 * b});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 3 * a + 6 * b + c + 5));
}

TEST(QuadraticExpressionTest, AddSumLinearExpressions) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<LinearExpression> to_add({a + b, 4.0 * b - 1.0});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 4 * a + 5 * b + 4));
}

TEST(QuadraticExpressionTest, AddSumQuadraticTerms) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<QuadraticTerm> to_add({a * a, 2 * a * b});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(3 * a * a + 2 * a * b + 3 * a + 5));
}

TEST(QuadraticExpressionTest, AddSumQuadraticExpressions) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<QuadraticExpression> to_add(
      {a * a - 1, 2 * a * b + 3 * b * b + 2});
  expr.AddSum(to_add);
  EXPECT_THAT(expr, IsIdentical(3 * a * a + 2 * a * b + 3 * b * b + 3 * a + 6));
}

TEST(QuadraticExpressionTest, Sum) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const std::vector<QuadraticExpression> summand(
      {a * a, 2 * a * b + 3 * b + 4, 5 * b * a + 6 * b + 7});
  EXPECT_THAT(QuadraticExpression::Sum(summand),
              IsIdentical(a * a + 7 * a * b + 9 * b + 11));
}

TEST(QuadraticExpressionTest, AddInnerProductIntInt) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<int> first = {2, 3, 4};
  const std::vector<int> second = {1, -1, 10};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 3 * a + 44));
}

TEST(QuadraticExpressionTest, AddInnerProductDoubleDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<double> first = {2.0, 3.0, 4.0};
  const std::vector<double> second = {1.0, -1.0, 10.0};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 3 * a + 44));
}

TEST(QuadraticExpressionTest, AddInnerProductDoubleVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<double> first = {2.0, 3.0, 4.0};
  const std::vector<Variable> second = {a, b, a};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 9 * a + 3 * b + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductIntLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<int> first = {2, 3, 4};
  const std::vector<LinearTerm> second = {2.0 * a, 4.0 * b, 1.0 * a};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 11 * a + 12 * b + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductDoubleLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<double> first = {-1.0, 2.0};
  const std::vector<LinearExpression> second = {3.0 * b + 1, a + b};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 5 * a - b + 4));
}

TEST(QuadraticExpressionTest, AddInnerProductDoubleQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<double> first = {-1.0, 2.0};
  const std::vector<QuadraticTerm> second = {3.0 * a * a, 4 * a * b};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(-a * a + 8 * a * b + 3 * a + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductDoubleQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<double> first = {-1.0, 2.0};
  const std::vector<QuadraticExpression> second = {3.0 * a * b + 1,
                                                   4 * a * a + b};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(10 * a * a - 3 * a * b + 3 * a + 2 * b + 4));
}

TEST(QuadraticExpressionTest, AddInnerProductVariableVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<Variable> first = {a, b, a};
  const std::vector<Variable> second = {a, a, b};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(3 * a * a + 2 * a * b + 3 * a + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductVariableLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<Variable> first = {a, b, a};
  const std::vector<LinearTerm> second = {2 * a, 3 * a, 4 * b};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(4 * a * a + 7 * a * b + 3 * a + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductVariableLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<Variable> first = {a, b, a};
  const std::vector<LinearExpression> second = {2 * a + 3, 4 * a + 5 * b, 6};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr,
              IsIdentical(4 * a * a + 4 * a * b + 5 * b * b + 12 * a + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductLinearTermLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<LinearTerm> first = {1 * a, 2 * a, 3 * b};
  const std::vector<LinearTerm> second = {1 * a, 2 * b, 3 * b};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(3 * a * a + 4 * a * b + 9 * b * b + 3 * a + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductLinearTermLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<LinearTerm> first = {1 * a, 2 * b, 3 * a};
  const std::vector<LinearExpression> second = {2 * a + 3, 4 * a + 5 * b, 6};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr,
              IsIdentical(4 * a * a + 8 * a * b + 10 * b * b + 24 * a + 5));
}

TEST(QuadraticExpressionTest, AddInnerProductLinearExpressionLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  QuadraticExpression expr = 2.0 * a * a + 3.0 * a + 5.0;
  const std::vector<LinearExpression> first = {3 * b + a + 1, 2 * a - 2};
  const std::vector<LinearExpression> second = {2 * a + 3, 3 * a + 5 * b};
  expr.AddInnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(10 * a * a + 16 * a * b + 2 * a - b + 8));
}

TEST(QuadraticExpressionTest, InnerProduct) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));
  const std::vector<Variable> first = {a, b, a};
  const std::vector<LinearTerm> second = {2.0 * a, 3.0 * a, 4.0 * b};
  const auto expr = QuadraticExpression::InnerProduct(first, second);
  EXPECT_THAT(expr, IsIdentical(2 * a * a + 7 * a * b));
}

TEST(QuadraticExpressionDeathTest, AddInnerProductSizeMismatchLeftMore) {
  const std::vector<double> left = {2.0, 3.0, 4.0};
  const std::vector<double> right = {1.0, -1.0};
  QuadraticExpression expr;
  EXPECT_DEATH(expr.AddInnerProduct(left, right), "left had more");
}

TEST(QuadraticExpressionDeathTest, AddInnerProductSizeMismatchRightMore) {
  const std::vector<double> left = {2.0, 3.0};
  const std::vector<double> right = {1.0, -1.0, 10.0};
  QuadraticExpression expr;
  EXPECT_DEATH(expr.AddInnerProduct(left, right), "right had more");
}

////////////////////////////////////////////////////////////////////////////////
// Quadratic greater than (>=) operators
////////////////////////////////////////////////////////////////////////////////

TEST(QuadraticExpressionTest, DoubleGreaterEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 3;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const UpperBoundedQuadraticExpression comparison = lhs >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 2}}, {}, 0)));
  EXPECT_EQ(comparison.upper_bound, 3);
}

TEST(QuadraticExpressionTest, DoubleGreaterEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 3;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const UpperBoundedQuadraticExpression comparison = lhs >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 2}}, {{b, 3}}, 4)));
  EXPECT_EQ(comparison.upper_bound, 3);
}

TEST(QuadraticExpressionTest,
     DoubleGreaterEqualLowerBoundedQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 6;
  LowerBoundedQuadraticExpression rhs = (2 * a * b + 3 * b + 4 >= 5);

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(1, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms({{b, 3}}), 2));
}

TEST(QuadraticExpressionTest, VariableGreaterEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const Variable lhs = a;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, -2}}),
                                              LinearTerms({{a, 1}}), kInf));
}

TEST(QuadraticExpressionTest, VariableGreaterEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const Variable lhs = a;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              4, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 1}, {b, -3}}), kInf));
}

TEST(QuadraticExpressionTest, LinearTermGreaterEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearTerm lhs = 3 * a;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, -2}}),
                                              LinearTerms({{a, 3}}), kInf));
}

TEST(QuadraticExpressionTest, LinearTermGreaterEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearTerm lhs = 3 * a;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              4, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}, {b, -3}}), kInf));
}

TEST(QuadraticExpressionTest, LinearExpressionGreaterEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression lhs = 3 * a + 4;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) >= rhs;

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(-4, QuadraticTerms({{a, b, -2}}),
                                              LinearTerms({{a, 3}}), kInf));
}

TEST(QuadraticExpressionTest, LinearExpressionGreaterEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression lhs = 3 * a + 4;
  QuadraticExpression rhs = 2 * a * b + 4 * b + 5;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              1, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}, {b, -4}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticTermGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const double rhs = 4;

  ResetExpressionCounters();
  const LowerBoundedQuadraticExpression comparison = lhs >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 3}}, {}, 0)));
  EXPECT_EQ(comparison.lower_bound, 4);
}

TEST(QuadraticExpressionTest, QuadraticTermGreaterEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const Variable rhs = a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, 3}}),
                                              LinearTerms({{a, -1}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticTermGreaterEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const LinearTerm rhs = 4 * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, 3}}),
                                              LinearTerms({{a, -4}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticTermGreaterEqualLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  LinearExpression rhs = 4 * a + 5;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= std::move(rhs);

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(5, QuadraticTerms({{a, b, 3}}),
                                              LinearTerms({{a, -4}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticTermGreaterEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const QuadraticTerm rhs = 4 * a * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              0, QuadraticTerms({{a, a, -4}, {a, b, 3}}),
                              LinearTerms(), kInf));
}

TEST(QuadraticExpressionTest, QuadraticTermGreaterEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  QuadraticExpression rhs = 4 * a * a + 5 * b + 6;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              6, QuadraticTerms({{a, a, -4}, {a, b, 3}}),
                              LinearTerms({{b, -5}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticExpressionGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const double rhs = 6;

  ResetExpressionCounters();
  const LowerBoundedQuadraticExpression comparison = std::move(lhs) >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 3}}, {{b, 4}}, 5)));
  EXPECT_EQ(comparison.lower_bound, 6);
}

TEST(QuadraticExpressionTest, QuadraticExpressionGreaterEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const Variable rhs = a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -5, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -1}, {b, 4}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticExpressionGreaterEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const LinearTerm rhs = 6 * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -5, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -6}, {b, 4}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticExpressionGreaterEqualLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  LinearExpression rhs = 6 * a + 7;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              2, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -6}, {b, 4}}), kInf));
}

TEST(QuadraticExpressionTest, QuadraticExpressionGreaterEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const QuadraticTerm rhs = 6 * a * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -5, QuadraticTerms({{a, a, -6}, {a, b, 3}}),
                              LinearTerms({{b, 4}}), kInf));
}

TEST(QuadraticExpressionTest,
     QuadraticExpressionGreaterEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  QuadraticExpression rhs = 6 * a * a + 7 * a + 8;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) >= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              3, QuadraticTerms({{a, a, -6}, {a, b, 3}}),
                              LinearTerms({{a, -7}, {b, 4}}), kInf));
}

TEST(QuadraticExpressionTest,
     UpperBoundedQuadraticExpressionGreaterEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  UpperBoundedQuadraticExpression lhs = (2 * a * b + 3 * b + 4 <= 5);
  const double rhs = 1;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) >= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(-3, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms({{b, 3}}), 1));
}

////////////////////////////////////////////////////////////////////////////////
// Quadratic less than (<=) operators
////////////////////////////////////////////////////////////////////////////////

TEST(QuadraticExpressionTest, DoubleLesserEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 3;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const LowerBoundedQuadraticExpression comparison = lhs <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 2}}, {}, 0)));
  EXPECT_EQ(comparison.lower_bound, 3);
}

TEST(QuadraticExpressionTest, DoubleLesserEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 3;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const LowerBoundedQuadraticExpression comparison = lhs <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 2}}, {{b, 3}}, 4)));
  EXPECT_EQ(comparison.lower_bound, 3);
}

TEST(QuadraticExpressionTest,
     DoubleLesserEqualUpperBoundedQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 1;
  UpperBoundedQuadraticExpression rhs = (2 * a * b + 3 * b + 4 <= 5);

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(-3, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms({{b, 3}}), 1));
}

TEST(QuadraticExpressionTest, VariableLesserEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const Variable lhs = a;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 1}}), 0));
}

TEST(QuadraticExpressionTest, VariableLesserEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const Variable lhs = a;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 1}, {b, -3}}), 4));
}

TEST(QuadraticExpressionTest, LinearTermLesserEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearTerm lhs = 3 * a;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}}), 0));
}

TEST(QuadraticExpressionTest, LinearTermLesserEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearTerm lhs = 3 * a;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}, {b, -3}}), 4));
}

TEST(QuadraticExpressionTest, LinearExpressionLesserEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression lhs = 3 * a + 4;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) <= rhs;

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}}), -4));
}

TEST(QuadraticExpressionTest, LinearExpressionLesserEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression lhs = 3 * a + 4;
  QuadraticExpression rhs = 2 * a * b + 4 * b + 5;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}, {b, -4}}), 1));
}

TEST(QuadraticExpressionTest, QuadraticTermLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const double rhs = 4;

  ResetExpressionCounters();
  const UpperBoundedQuadraticExpression comparison = lhs <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 3}}, {}, 0)));
  EXPECT_EQ(comparison.upper_bound, 4);
}

TEST(QuadraticExpressionTest, QuadraticTermLesserEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const Variable rhs = a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -1}}), 0));
}

TEST(QuadraticExpressionTest, QuadraticTermLesserEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const LinearTerm rhs = 4 * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -4}}), 0));
}

TEST(QuadraticExpressionTest, QuadraticTermLesserEqualLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  LinearExpression rhs = 4 * a + 5;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= std::move(rhs);

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -4}}), 5));
}

TEST(QuadraticExpressionTest, QuadraticTermLesserEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const QuadraticTerm rhs = 4 * a * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, a, -4}, {a, b, 3}}),
                              LinearTerms(), 0));
}

TEST(QuadraticExpressionTest, QuadraticTermLesserEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  QuadraticExpression rhs = 4 * a * a + 5 * b + 6;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, a, -4}, {a, b, 3}}),
                              LinearTerms({{b, -5}}), 6));
}

TEST(QuadraticExpressionTest, QuadraticExpressionLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const double rhs = 6;

  ResetExpressionCounters();
  const UpperBoundedQuadraticExpression comparison = std::move(lhs) <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison.expression,
              IsIdentical(QuadraticExpression({{a, b, 3}}, {{b, 4}}, 5)));
  EXPECT_EQ(comparison.upper_bound, 6);
}

TEST(QuadraticExpressionTest, QuadraticExpressionLesserEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const Variable rhs = a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -1}, {b, 4}}), -5));
}

TEST(QuadraticExpressionTest, QuadraticExpressionLesserEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const LinearTerm rhs = 6 * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -6}, {b, 4}}), -5));
}

TEST(QuadraticExpressionTest, QuadraticExpressionLesserEqualLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  LinearExpression rhs = 6 * a + 7;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -6}, {b, 4}}), 2));
}

TEST(QuadraticExpressionTest, QuadraticExpressionLesserEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const QuadraticTerm rhs = 6 * a * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, a, -6}, {a, b, 3}}),
                              LinearTerms({{b, 4}}), -5));
}

TEST(QuadraticExpressionTest,
     QuadraticExpressionLesserEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  QuadraticExpression rhs = 6 * a * a + 7 * a + 8;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) <= std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -kInf, QuadraticTerms({{a, a, -6}, {a, b, 3}}),
                              LinearTerms({{a, -7}, {b, 4}}), 3));
}

TEST(QuadraticExpressionTest,
     LowerBoundedQuadraticExpressionLesserEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LowerBoundedQuadraticExpression lhs = (2 * a * b + 3 * b + 4 >= 5);
  const double rhs = 6;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) <= rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(1, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms({{b, 3}}), 2));
}

////////////////////////////////////////////////////////////////////////////////
// Quadratic equals (==) operators
////////////////////////////////////////////////////////////////////////////////

TEST(QuadraticExpressionTest, DoubleEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 3;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(3, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms(), 3));
}

TEST(QuadraticExpressionTest, DoubleEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const double lhs = 3;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(-1, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms({{b, 3}}), -1));
}

TEST(QuadraticExpressionTest, VariableEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const Variable lhs = a;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms({{a, -1}}), 0));
}

TEST(QuadraticExpressionTest, VariableEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const Variable lhs = a;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              4, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 1}, {b, -3}}), 4));
}

TEST(QuadraticExpressionTest, LinearTermEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearTerm lhs = 3 * a;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, 2}}),
                                              LinearTerms({{a, -3}}), 0));
}

TEST(QuadraticExpressionTest, LinearTermEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const LinearTerm lhs = 3 * a;
  QuadraticExpression rhs = 2 * a * b + 3 * b + 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              4, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}, {b, -3}}), 4));
}

TEST(QuadraticExpressionTest, LinearExpressionEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression lhs = 3 * a + 4;
  const QuadraticTerm rhs = 2 * a * b;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) == rhs;

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 4);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(-4, QuadraticTerms({{a, b, -2}}),
                                              LinearTerms({{a, 3}}), -4));
}

TEST(QuadraticExpressionTest, LinearExpressionEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  LinearExpression lhs = 3 * a + 4;
  QuadraticExpression rhs = 2 * a * b + 4 * b + 5;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) == std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              1, QuadraticTerms({{a, b, -2}}),
                              LinearTerms({{a, 3}, {b, -4}}), 1));
}

TEST(QuadraticExpressionTest, QuadraticTermEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const double rhs = 4;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(4, QuadraticTerms({{a, b, 3}}),
                                              LinearTerms(), 4));
}

TEST(QuadraticExpressionTest, QuadraticTermEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const Variable rhs = a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, -3}}),
                                              LinearTerms({{a, 1}}), 0));
}

TEST(QuadraticExpressionTest, QuadraticTermEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const LinearTerm rhs = 4 * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(0, QuadraticTerms({{a, b, -3}}),
                                              LinearTerms({{a, 4}}), 0));
}

TEST(QuadraticExpressionTest, QuadraticTermEqualLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  LinearExpression rhs = 4 * a + 5;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == std::move(rhs);

  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(5, QuadraticTerms({{a, b, 3}}),
                                              LinearTerms({{a, -4}}), 5));
}

TEST(QuadraticExpressionTest, QuadraticTermEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  const QuadraticTerm rhs = 4 * a * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              0, QuadraticTerms({{a, a, 4}, {a, b, -3}}),
                              LinearTerms(), 0));
}

TEST(QuadraticExpressionTest, QuadraticTermEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const QuadraticTerm lhs = 3 * a * b;
  QuadraticExpression rhs = 4 * a * a + 5 * b + 6;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = lhs == std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -6, QuadraticTerms({{a, a, 4}, {a, b, -3}}),
                              LinearTerms({{b, 5}}), -6));
}

TEST(QuadraticExpressionTest, QuadraticExpressionEqualDouble) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const double rhs = 6;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(-1, QuadraticTerms({{a, b, -3}}),
                                              LinearTerms({{b, -4}}), -1));
}

TEST(QuadraticExpressionTest, QuadraticExpressionEqualVariable) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const Variable rhs = a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -5, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -1}, {b, 4}}), -5));
}

TEST(QuadraticExpressionTest, QuadraticExpressionEqualLinearTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const LinearTerm rhs = 6 * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -5, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -6}, {b, 4}}), -5));
}

TEST(QuadraticExpressionTest, QuadraticExpressionEqualLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  LinearExpression rhs = 6 * a + 7;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) == std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              2, QuadraticTerms({{a, b, 3}}),
                              LinearTerms({{a, -6}, {b, 4}}), 2));
}

TEST(QuadraticExpressionTest, QuadraticExpressionEqualQuadraticTerm) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  const QuadraticTerm rhs = 6 * a * a;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = std::move(lhs) == rhs;

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              -5, QuadraticTerms({{a, a, -6}, {a, b, 3}}),
                              LinearTerms({{b, 4}}), -5));
}

TEST(QuadraticExpressionTest, QuadraticExpressionEqualQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  QuadraticExpression lhs = 3 * a * b + 4 * b + 5;
  QuadraticExpression rhs = 6 * a * a + 7 * a + 8;

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison =
      std::move(lhs) == std::move(rhs);

  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 3);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(comparison, BoundedQuadraticExpressionEquiv(
                              3, QuadraticTerms({{a, a, -6}, {a, b, 3}}),
                              LinearTerms({{a, -7}, {b, 4}}), 3));
}

TEST(BoundedQuadraticExpressionTest, FromVariablesEquality) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  const internal::VariablesEquality linear_comparison = a == b;
  ResetExpressionCounters();

  const BoundedQuadraticExpression quadratic_comparison(linear_comparison);
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(quadratic_comparison,
              BoundedQuadraticExpressionEquiv(
                  0, QuadraticTerms(), LinearTerms({{a, 1}, {b, -1}}), 0));
}

TEST(LowerBoundedQuadraticExpressionTest, FromLowerBoundedLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  LowerBoundedLinearExpression linear_comparison = 2 * a >= 3;
  ResetExpressionCounters();

  const LowerBoundedQuadraticExpression quadratic_comparison(
      std::move(linear_comparison));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(quadratic_comparison.expression,
              IsIdentical(QuadraticExpression({}, {{a, 2}}, 0)));
  EXPECT_EQ(quadratic_comparison.lower_bound, 3);
}

TEST(BoundedQuadraticExpressionTest, FromLowerBoundedLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  LowerBoundedLinearExpression linear_comparison = 2 * a >= 3;
  ResetExpressionCounters();

  const BoundedQuadraticExpression quadratic_comparison(
      std::move(linear_comparison));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(quadratic_comparison,
              BoundedQuadraticExpressionEquiv(3, QuadraticTerms(),
                                              LinearTerms({{a, 2}}), kInf));
}

TEST(UpperBoundedQuadraticExpressionTest, FromUpperBoundedLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  UpperBoundedLinearExpression linear_comparison = 2 * a <= 3;
  ResetExpressionCounters();

  const UpperBoundedQuadraticExpression quadratic_comparison(
      std::move(linear_comparison));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(quadratic_comparison.expression,
              IsIdentical(QuadraticExpression({}, {{a, 2}}, 0)));
  EXPECT_EQ(quadratic_comparison.upper_bound, 3);
}

TEST(BoundedQuadraticExpressionTest, FromUpperBoundedLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  UpperBoundedLinearExpression linear_comparison = 2 * a <= 3;
  ResetExpressionCounters();

  const BoundedQuadraticExpression quadratic_comparison(
      std::move(linear_comparison));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(quadratic_comparison,
              BoundedQuadraticExpressionEquiv(-kInf, QuadraticTerms(),
                                              LinearTerms({{a, 2}}), 3));
}

TEST(BoundedQuadraticExpressionTest, FromBoundedLinearExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  BoundedLinearExpression linear_comparison = (2 <= 3 * a <= 4);
  ResetExpressionCounters();

  const BoundedQuadraticExpression quadratic_comparison(
      std::move(linear_comparison));
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(LinearExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(LinearExpression, 0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(1);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_THAT(quadratic_comparison,
              BoundedQuadraticExpressionEquiv(2, QuadraticTerms(),
                                              LinearTerms({{a, 3}}), 4));
}

TEST(BoundedQuadraticExpressionTest, FromLowerBoundedQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = 3 <= a * a;
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(3.0, QuadraticTerms({{a, a, 1}}),
                                              LinearTerms(), kInf));
}

TEST(BoundedQuadraticExpressionTest, FromUpperBoundedQuadraticExpression) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));

  ResetExpressionCounters();
  const BoundedQuadraticExpression comparison = a * a <= 5;
  EXPECT_THAT(comparison,
              BoundedQuadraticExpressionEquiv(
                  -kInf, QuadraticTerms({{a, a, 1}}), LinearTerms(), 5));
  EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS(0);
  EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR(0);
  EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_COPY_CONSTRUCTOR(QuadraticExpression, 0);
  EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR(QuadraticExpression, 2);
  EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR(QuadraticExpression, 1);
}

TEST(BoundedQuadraticExpressionTest, OutputStreaming) {
  ModelStorage storage;
  const Variable a(&storage, storage.AddVariable("a"));
  const Variable b(&storage, storage.AddVariable("b"));

  auto to_string = [](BoundedQuadraticExpression bounded_expression) {
    std::ostringstream oss;
    oss << bounded_expression;
    return oss.str();
  };

  EXPECT_EQ(to_string(BoundedQuadraticExpression(QuadraticExpression(), -1, 2)),
            "-1 ≤ 0 ≤ 2");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({}, {}, -1), -1, 2)),
            "-1 ≤ -1 ≤ 2");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({}, {{a, 1}, {b, 5}}, -1), -1, 2)),
            "-1 ≤ a + 5*b - 1 ≤ 2");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({}, {{a, 1}, {b, 5}}, 0), -1, 2)),
            "-1 ≤ a + 5*b ≤ 2");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({}, {{a, 2}}, 0), -kInf, 2)),
            "2*a ≤ 2");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({}, {{a, 2}}, 0), -1, kInf)),
            "2*a ≥ -1");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({}, {{a, 2}}, 0), 3, 3)),
            "2*a = 3");

  EXPECT_EQ(
      to_string(BoundedQuadraticExpression(
          QuadraticExpression({{a, a, 3}, {a, b, 4}}, {{a, 1}, {b, 5}}, -1), -1,
          2)),
      "-1 ≤ 3*a² + 4*a*b + a + 5*b - 1 ≤ 2");
  EXPECT_EQ(
      to_string(BoundedQuadraticExpression(
          QuadraticExpression({{a, a, 3}, {a, b, 4}}, {{a, 1}, {b, 5}}, 0), -1,
          2)),
      "-1 ≤ 3*a² + 4*a*b + a + 5*b ≤ 2");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({{a, a, 2}}, {}, 0), -kInf, 2)),
            "2*a² ≤ 2");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({{a, a, 2}}, {}, 0), -1, kInf)),
            "2*a² ≥ -1");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({{a, a, 2}}, {}, 0), 3, 3)),
            "2*a² = 3");
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({{a, a, 2}}, {}, 0), -kInf,
                kRoundTripTestNumber)),
            absl::StrCat("2*a² ≤ ", kRoundTripTestNumberStr));
  EXPECT_EQ(
      to_string(BoundedQuadraticExpression(
          QuadraticExpression({{a, a, 2}}, {}, 0), kRoundTripTestNumber, kInf)),
      absl::StrCat("2*a² ≥ ", kRoundTripTestNumberStr));
  EXPECT_EQ(to_string(BoundedQuadraticExpression(
                QuadraticExpression({{a, a, 2}}, {}, 0), kRoundTripTestNumber,
                kRoundTripTestNumber)),
            absl::StrCat("2*a² = ", kRoundTripTestNumberStr));
  EXPECT_EQ(
      to_string(BoundedQuadraticExpression(
          QuadraticExpression({{a, a, 2}}, {}, 0), kRoundTripTestNumber, 3000)),
      absl::StrCat(kRoundTripTestNumberStr, " ≤ 2*a² ≤ 3000"));
  EXPECT_EQ(
      to_string(BoundedQuadraticExpression(
          QuadraticExpression({{a, a, 2}}, {}, 0), 0.0, kRoundTripTestNumber)),
      absl::StrCat("0 ≤ 2*a² ≤ ", kRoundTripTestNumberStr));
}

#undef EXPECT_NUM_CALLS_DEFAULT_CONSTRUCTOR
#undef EXPECT_NUM_CALLS_COPY_CONSTRUCTOR
#undef EXPECT_NUM_CALLS_MOVE_CONSTRUCTOR
#undef EXPECT_NUM_CALLS_INITIALIZER_LIST_CONSTRUCTOR
#undef EXPECT_NUM_CALLS_LINEAR_TO_QUADRATIC_CONSTRUCTOR
#undef EXPECT_NUM_CALLS_TO_LINEAR_EXPRESSION_CONSTRUCTORS
#undef EXPECT_NUM_CALLS_TO_QUADRATIC_EXPRESSION_CONSTRUCTORS

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
