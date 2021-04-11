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

#include "ortools/math_opt/validators/scalar_validator.h"

#include <cmath>
#include <limits>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace operations_research {
namespace math_opt {
namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();
}  // namespace

absl::Status CheckScalarNoNanNoInf(const double d) {
  if (!std::isfinite(d)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected no NaN or inf but found value: ", d));
  }
  return absl::OkStatus();
}

absl::Status CheckScalar(const double value, const DoubleOptions& options) {
  if (std::isnan(value)) {
    return absl::InvalidArgumentError("Invalid NaN value");
  }
  if (!options.allow_positive_infinity && value == kInf) {
    return absl::InvalidArgumentError("Invalid positive infinite value");
  }
  if (!options.allow_negative_infinity && value == -kInf) {
    return absl::InvalidArgumentError("Invalid negative infinite value");
  }
  if (!options.allow_positive && value > 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid positive value = ", value));
  }
  if (!options.allow_negative && value < 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid negative value = ", value));
  }
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
