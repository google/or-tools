
// Utilities to display linear expression in a human-readable way.

#ifndef OR_TOOLS_GLOP_LP_PRINT_UTILS_H_
#define OR_TOOLS_GLOP_LP_PRINT_UTILS_H_

#include <string>

#include "base/integral_types.h"
#include "base/stringprintf.h"
#include "glop/lp_types.h"

namespace operations_research {
namespace glop {

// Computes a rational approximation numerator/denominator for value x
// using a continued fraction algorithm. The absolute difference between the
// output fraction and the input "x" will not exceed "precision".
Fraction RationalApproximation(const double x, const double precision);

// Returns a std::string representing a floating-point number in decimal,
// with a precision corresponding to the type of the argument.
inline std::string Stringify(const float a) { return StringPrintf("%.7g", a); }

inline std::string Stringify(const double a) { return StringPrintf("%.16g", a); }

inline std::string Stringify(const long double a) {
  return StringPrintf("%.19Lg", a);
}

// Returns a std::string "num/den" representing the rational approximation of x.
// The absolute difference between the output fraction and the input "x" will
// not exceed "precision".
std::string StringifyRational(const double x, const double precision);

// If fraction is true, returns a std::string corresponding to the rational
// approximation or a decimal approximation otherwise. Note that the absolute
// difference between the output fraction and "x" will never exceed
// std::numeric_limits<T>::epsilon().
std::string Stringify(const Fractional x, bool fraction);

// Pretty prints a monomial a*x using Stringify(x, fraction) to display a,
// taking care of the sign of x, whether a is 0, 1, -1, integer. Note that the
// absolute difference between the output fraction and "x" will never exceed
// std::numeric_limits<T>::epsilon().
std::string StringifyMonomial(const Fractional a, const std::string& x, bool fraction);

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_LP_PRINT_UTILS_H_
