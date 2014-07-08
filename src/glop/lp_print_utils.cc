
#include "glop/lp_print_utils.h"

#include <cmath>
#include <cstdio>
#include <limits>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/join.h"
#include "glop/lp_types.h"

namespace operations_research {
namespace glop {

// This template function returns an epsilon against which
// is tested a rational approximation of a floating-point value.
// We can get all the precision we need on float and double
// because we'll be computing the continued fraction
// approximation using long doubles.
template <typename T>
inline T ContinuedFractionEpsilon() {
  return std::numeric_limits<T>::epsilon();
}

// We can't compare a rational approximation of a long double to the maximum
// precision, because some precision is lost when multiplying/dividing.
// We therefore accept to be less by precise.
template <>
inline long double ContinuedFractionEpsilon<long double>() {
  const long double kPrecisionMultiplier = 8192.0;
  return std::numeric_limits<long double>::epsilon() * kPrecisionMultiplier;
}

// Computes a rational approximation numerator/denominator for value x
// using a continued fraction algorithm. The absolute difference between the
// output fraction and the input "x" will not exceed "precision".
Fraction RationalApproximation(const double x, const double precision) {
  DCHECK_LT(x, kInfinity);
  DCHECK_GT(x, -kInfinity);
  // All computations are made on long doubles to guarantee the maximum
  // precision for the approximations. This way, the approximations when
  // Fractional is float or double are as accurate as possible.
  long double abs_x = fabs(x);
  long double y = abs_x;
  int64 previous_numerator = 0;
  int64 previous_denominator = 1;
  int64 numerator = 1;
  int64 denominator = 0;
  while (true) {
    const int64 term = static_cast<int64>(std::floor(y));
    const int64 new_numerator = term * numerator + previous_numerator;
    const int64 new_denominator = term * denominator + previous_denominator;
    // If there was an overflow, we prefer returning a not-so-good approximation
    // rather than something that is completely wrong.
    if (new_numerator < 0 || new_denominator < 0) break;
    previous_numerator = numerator;
    previous_denominator = denominator;
    numerator = new_numerator;
    denominator = new_denominator;
    long double numerator_approximation = abs_x * denominator;
    if (std::fabs(numerator_approximation - numerator) <=
        precision * numerator_approximation)
      break;
    y = 1 / (y - term);
  }
  return Fraction((x < 0) ? -numerator : numerator, denominator);
}

// Returns a std::string "num/den" representing the rational approximation of x.
// The absolute difference between the output fraction and the input "x" will
// not exceed "precision".
std::string StringifyRational(const double x, const double precision) {
  if (x == kInfinity) {
    return "inf";
  } else if (x == -kInfinity) {
    return "-inf";
  }
  Fraction fraction = RationalApproximation(x, precision);
  const int64 numerator = fraction.first;
  const int64 denominator = fraction.second;
  std::string output;
  if (x >= 0) {
    StringAppendF(&output, "%lld", numerator);
  } else {
    StringAppendF(&output, "-%lld", -numerator);
  }
  if (denominator != 1) {
    StringAppendF(&output, "/%lld", denominator);
  }
  return output;
}

std::string Stringify(const Fractional x, bool fraction) {
  if (fraction) {
    return StringifyRational(ToDouble(x), ContinuedFractionEpsilon<double>());
  } else {
    return Stringify(x);
  }
}

// Returns a std::string that pretty-prints a monomial ax with coefficient
// a and variable name x
std::string StringifyMonomial(const Fractional a, const std::string& x, bool fraction) {
  std::string output;
  if (a != 0.0) {
    if (a > 0.0) {
      output += " + ";
      if (a != 1.0) {
        output += Stringify(a, fraction);
        output += " ";
      }
    } else if (a < 0.0) {
      output += " - ";
      if (a != -1.0) {
        output += Stringify(-a, fraction);
        output += " ";
      }
    }
    output += x;
  }
  return output;
}

}  // namespace glop
}  // namespace operations_research
