// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/parameters_validation.h"

#include <stdint.h>

#include <cmath>
#include <limits>
#include <string>

#include "absl/strings/str_cat.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

#define TEST_IN_RANGE(name, min, max)                                       \
  if (params.name() < min || params.name() > max) {                         \
    return absl::StrCat("parameter '", #name, "' should be in [", min, ",", \
                        max, "]. Current value is ", params.name());        \
  }

#define TEST_NON_NEGATIVE(name)                                         \
  if (params.name() < 0) {                                              \
    return absl::StrCat("Parameters ", #name, " must be non-negative"); \
  }

#define TEST_POSITIVE(name)                                         \
  if (params.name() <= 0) {                                         \
    return absl::StrCat("Parameters ", #name, " must be positive"); \
  }

#define TEST_NOT_NAN(name)                                 \
  if (std::isnan(params.name())) {                         \
    return absl::StrCat("parameter '", #name, "' is NaN"); \
  }

#define TEST_IS_FINITE(name)                                             \
  if (!std::isfinite(params.name())) {                                   \
    return absl::StrCat("parameter '", #name, "' is NaN or not finite"); \
  }

std::string ValidateParameters(const SatParameters& params) {
  // Test that all floating point parameters are not NaN or +/- infinity.
  TEST_IS_FINITE(absolute_gap_limit);
  TEST_IS_FINITE(blocking_restart_multiplier);
  TEST_IS_FINITE(clause_activity_decay);
  TEST_IS_FINITE(clause_cleanup_ratio);
  TEST_IS_FINITE(cut_active_count_decay);
  TEST_IS_FINITE(cut_max_active_count_value);
  TEST_IS_FINITE(feasibility_jump_batch_dtime);
  TEST_IS_FINITE(glucose_decay_increment);
  TEST_IS_FINITE(glucose_max_decay);
  TEST_IS_FINITE(initial_variables_activity);
  TEST_IS_FINITE(inprocessing_dtime_ratio);
  TEST_IS_FINITE(inprocessing_minimization_dtime);
  TEST_IS_FINITE(inprocessing_probing_dtime);
  TEST_IS_FINITE(max_clause_activity_value);
  TEST_IS_FINITE(max_variable_activity_value);
  TEST_IS_FINITE(merge_at_most_one_work_limit);
  TEST_IS_FINITE(merge_no_overlap_work_limit);
  TEST_IS_FINITE(min_orthogonality_for_lp_constraints);
  TEST_IS_FINITE(mip_check_precision);
  TEST_IS_FINITE(mip_drop_tolerance);
  TEST_IS_FINITE(mip_max_bound);
  TEST_IS_FINITE(mip_max_valid_magnitude);
  TEST_IS_FINITE(mip_var_scaling);
  TEST_IS_FINITE(mip_wanted_precision);
  TEST_IS_FINITE(pb_cleanup_ratio);
  TEST_IS_FINITE(presolve_probing_deterministic_time_limit);
  TEST_IS_FINITE(probing_deterministic_time_limit);
  TEST_IS_FINITE(propagation_loop_detection_factor);
  TEST_IS_FINITE(random_branches_ratio);
  TEST_IS_FINITE(random_polarity_ratio);
  TEST_IS_FINITE(relative_gap_limit);
  TEST_IS_FINITE(restart_dl_average_ratio);
  TEST_IS_FINITE(restart_lbd_average_ratio);
  TEST_IS_FINITE(shared_tree_open_leaves_per_worker);
  TEST_IS_FINITE(shaving_search_deterministic_time);
  TEST_IS_FINITE(strategy_change_increase_ratio);
  TEST_IS_FINITE(symmetry_detection_deterministic_time_limit);
  TEST_IS_FINITE(variable_activity_decay);

  TEST_IS_FINITE(lns_initial_difficulty);
  TEST_IS_FINITE(lns_initial_deterministic_limit);
  TEST_IN_RANGE(lns_initial_difficulty, 0.0, 1.0);

  TEST_POSITIVE(at_most_one_max_expansion_size);

  TEST_NOT_NAN(max_time_in_seconds);
  TEST_NOT_NAN(max_deterministic_time);

  // Parallelism.
  const int kMaxReasonableParallelism = 10'000;
  TEST_IN_RANGE(num_workers, 0, kMaxReasonableParallelism);
  TEST_IN_RANGE(num_search_workers, 0, kMaxReasonableParallelism);
  TEST_IN_RANGE(shared_tree_num_workers, -1, kMaxReasonableParallelism);
  TEST_IN_RANGE(interleave_batch_size, 0, kMaxReasonableParallelism);
  TEST_IN_RANGE(shared_tree_open_leaves_per_worker, 1,
                kMaxReasonableParallelism);
  TEST_IN_RANGE(shared_tree_balance_tolerance, 0,
                log2(kMaxReasonableParallelism));

  // TODO(user): Consider using annotations directly in the proto for these
  // validation. It is however not open sourced.
  TEST_IN_RANGE(mip_max_activity_exponent, 1, 62);
  TEST_IN_RANGE(mip_max_bound, 0, 1e17);
  TEST_IN_RANGE(solution_pool_size, 1, std::numeric_limits<int32_t>::max());

  // Feasibility jump.
  TEST_NOT_NAN(feasibility_jump_decay);
  TEST_NOT_NAN(feasibility_jump_var_randomization_probability);
  TEST_NOT_NAN(feasibility_jump_var_perburbation_range_ratio);
  TEST_IN_RANGE(feasibility_jump_decay, 0.0, 1.0);
  TEST_IN_RANGE(feasibility_jump_var_randomization_probability, 0.0, 1.0);
  TEST_IN_RANGE(feasibility_jump_var_perburbation_range_ratio, 0.0, 1.0);

  // Violation ls.
  TEST_NOT_NAN(violation_ls_compound_move_probability);
  TEST_IN_RANGE(num_violation_ls, 0, kMaxReasonableParallelism);
  TEST_IN_RANGE(violation_ls_perturbation_period, 1, 1'000'000'000);
  TEST_IN_RANGE(violation_ls_compound_move_probability, 0.0, 1.0);

  TEST_POSITIVE(glucose_decay_increment_period);
  TEST_POSITIVE(shared_tree_max_nodes_per_worker);
  TEST_POSITIVE(shared_tree_open_leaves_per_worker);
  TEST_POSITIVE(mip_var_scaling);

  // Test LP tolerances.
  TEST_IS_FINITE(lp_primal_tolerance);
  TEST_IS_FINITE(lp_dual_tolerance);
  TEST_NON_NEGATIVE(lp_primal_tolerance);
  TEST_NON_NEGATIVE(lp_dual_tolerance);

  TEST_NON_NEGATIVE(linearization_level);
  TEST_NON_NEGATIVE(max_deterministic_time);
  TEST_NON_NEGATIVE(max_time_in_seconds);
  TEST_NON_NEGATIVE(mip_wanted_precision);
  TEST_NON_NEGATIVE(new_constraints_batch_size);
  TEST_NON_NEGATIVE(presolve_probing_deterministic_time_limit);
  TEST_NON_NEGATIVE(probing_deterministic_time_limit);
  TEST_NON_NEGATIVE(symmetry_detection_deterministic_time_limit);

  if (params.enumerate_all_solutions() &&
      (params.num_search_workers() > 1 || params.num_workers() > 1)) {
    return "Enumerating all solutions does not work in parallel";
  }

  if (params.enumerate_all_solutions() &&
      (!params.subsolvers().empty() || !params.extra_subsolvers().empty() ||
       !params.ignore_subsolvers().empty())) {
    return "Enumerating all solutions does not work with custom subsolvers";
  }

  if (params.num_search_workers() >= 1 && params.num_workers() >= 1) {
    return "Do not specify both num_search_workers and num_workers";
  }

  if (params.use_shared_tree_search()) {
    return "use_shared_tree_search must only be set on workers' parameters";
  }

  if (params.enumerate_all_solutions() && params.interleave_search()) {
    return "Enumerating all solutions does not work with interleaved search";
  }

  for (const SatParameters& new_subsolver : params.subsolver_params()) {
    if (new_subsolver.name().empty()) {
      return "New subsolver parameter defined without a name";
    }
  }

  const auto strategies = GetNamedParameters(params);
  for (const std::string& subsolver : params.subsolvers()) {
    if (subsolver == "core_or_no_lp") continue;  // Used by fz free search.
    if (!strategies.contains(subsolver)) {
      return absl::StrCat("subsolver \'", subsolver, "\' is not valid");
    }
  }

  for (const std::string& subsolver : params.extra_subsolvers()) {
    if (!strategies.contains(subsolver)) {
      return absl::StrCat("subsolver \'", subsolver, "\' is not valid");
    }
  }

  return "";
}

#undef TEST_IN_RANGE
#undef TEST_POSITIVE
#undef TEST_NON_NEGATIVE
#undef TEST_NOT_NAN
#undef TEST_IS_FINITE

}  // namespace sat
}  // namespace operations_research
