// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CPP_FORMATTERS_H_
#define OR_TOOLS_MATH_OPT_CPP_FORMATTERS_H_

#include <cmath>
#include <iostream>
#include <ostream>

#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::math_opt {

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

inline std::ostream& operator<<(std::ostream& out,
                                const LeadingCoefficientFormatter formatter) {
  const double coeff = formatter.coeff;
  if (formatter.is_first) {
    if (coeff == 1.0) {
      // Do nothing.
    } else if (coeff == -1.0) {
      out << "-";
    } else {
      out << RoundTripDoubleFormat(coeff) << "*";
    }
  } else {
    if (coeff == 1.0) {
      out << " + ";
    } else if (coeff == -1.0) {
      out << " - ";
    } else if (std::isnan(coeff)) {
      out << " + nan*";
    } else if (coeff >= 0) {
      out << " + " << RoundTripDoubleFormat(coeff) << "*";
    } else {
      out << " - " << RoundTripDoubleFormat(-coeff) << "*";
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

inline std::ostream& operator<<(std::ostream& out,
                                const ConstantFormatter formatter) {
  const double constant = formatter.constant;
  if (formatter.is_first) {
    out << RoundTripDoubleFormat(constant);
  } else if (constant == 0) {
    // Do nothing.
  } else if (std::isnan(constant)) {
    out << " + nan";
  } else if (constant > 0) {
    out << " + " << RoundTripDoubleFormat(constant);
  } else {
    out << " - " << RoundTripDoubleFormat(-constant);
  }
  return out;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_FORMATTERS_H_
