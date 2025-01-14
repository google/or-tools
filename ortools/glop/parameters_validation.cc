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

#include "ortools/glop/parameters_validation.h"

#include <cmath>
#include <string>

#include "absl/strings/str_cat.h"
#include "ortools/glop/parameters.pb.h"

namespace operations_research::glop {

#define TEST_FINITE_AND_NON_NEGATIVE(name)                                \
  if (!std::isfinite(params.name())) {                                    \
    return absl::StrCat("parameter '", #name, "' is NaN or not finite");  \
  }                                                                       \
  if (params.name() < 0) {                                                \
    return absl::StrCat("Parameters '", #name, "' must be non-negative"); \
  }

// We need an integer version of the test as std::isnan can fail to compile
// on windows platforms when passed integer values.
#define TEST_INTEGER_NON_NEGATIVE(name)                                   \
  if (params.name() < 0) {                                                \
    return absl::StrCat("Parameters '", #name, "' must be non-negative"); \
  }

#define TEST_NON_NEGATIVE(name)                                           \
  if (std::isnan(params.name())) {                                        \
    return absl::StrCat("parameter '", #name, "' is NaN");                \
  }                                                                       \
  if (params.name() < 0) {                                                \
    return absl::StrCat("Parameters '", #name, "' must be non-negative"); \
  }

#define TEST_NOT_NAN(name)                                 \
  if (std::isnan(params.name())) {                         \
    return absl::StrCat("parameter '", #name, "' is NaN"); \
  }

std::string ValidateParameters(const GlopParameters& params) {
  TEST_FINITE_AND_NON_NEGATIVE(degenerate_ministep_factor);
  TEST_FINITE_AND_NON_NEGATIVE(drop_tolerance);
  TEST_FINITE_AND_NON_NEGATIVE(dual_feasibility_tolerance);
  TEST_FINITE_AND_NON_NEGATIVE(dual_small_pivot_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(dualizer_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(harris_tolerance_ratio);
  TEST_FINITE_AND_NON_NEGATIVE(lu_factorization_pivot_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(markowitz_singularity_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(max_number_of_reoptimizations);
  TEST_FINITE_AND_NON_NEGATIVE(minimum_acceptable_pivot);
  TEST_FINITE_AND_NON_NEGATIVE(preprocessor_zero_tolerance);
  TEST_FINITE_AND_NON_NEGATIVE(primal_feasibility_tolerance);
  TEST_FINITE_AND_NON_NEGATIVE(ratio_test_zero_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(recompute_edges_norm_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(recompute_reduced_costs_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(refactorization_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(relative_cost_perturbation);
  TEST_FINITE_AND_NON_NEGATIVE(relative_max_cost_perturbation);
  TEST_FINITE_AND_NON_NEGATIVE(small_pivot_threshold);
  TEST_FINITE_AND_NON_NEGATIVE(solution_feasibility_tolerance);

  TEST_NOT_NAN(objective_lower_limit);
  TEST_NOT_NAN(objective_upper_limit);

  TEST_NON_NEGATIVE(crossover_bound_snapping_distance);
  TEST_NON_NEGATIVE(initial_condition_number_threshold);
  TEST_NON_NEGATIVE(max_deterministic_time);
  TEST_NON_NEGATIVE(max_time_in_seconds);

  TEST_FINITE_AND_NON_NEGATIVE(max_valid_magnitude);
  if (params.max_valid_magnitude() > 1e100) {
    return "max_valid_magnitude must be <= 1e100";
  }

  TEST_FINITE_AND_NON_NEGATIVE(drop_magnitude);
  if (params.drop_magnitude() < 1e-100) {
    return "drop magnitude must be finite and >= 1e-100";
  }

  TEST_INTEGER_NON_NEGATIVE(basis_refactorization_period);
  TEST_INTEGER_NON_NEGATIVE(devex_weights_reset_period);
  TEST_INTEGER_NON_NEGATIVE(num_omp_threads);
  TEST_INTEGER_NON_NEGATIVE(random_seed);

  if (params.markowitz_zlatev_parameter() < 1) {
    return "markowitz_zlatev_parameter must be >= 1";
  }

  return "";
}

#undef TEST_NOT_NAN
#undef TEST_INTEGER_NON_NEGATIVE
#undef TEST_NON_NEGATIVE
#undef TEST_FINITE_AND_NON_NEGATIVE

}  // namespace operations_research::glop
