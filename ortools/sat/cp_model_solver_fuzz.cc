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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "gtest/gtest.h"  // IWYU pragma: keep
#include "ortools/base/fuzztest.h"
#include "ortools/base/path.h"  // IWYU pragma: keep
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/saturated_arithmetic.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace operations_research::sat {

namespace {

std::string GetTestDataDir() {
  return file::JoinPathRespectAbsolute(::testing::SrcDir(),
                                       "_main/ortools/sat/fuzz_testdata");
}

void Solve(const CpModelProto& proto) {
  SatParameters params;
  params.set_max_time_in_seconds(4.0);
  params.set_debug_crash_if_presolve_breaks_hint(true);

  // Enable all fancy heuristics.
  params.set_linearization_level(2);
  params.set_use_try_edge_reasoning_in_no_overlap_2d(true);
  params.set_exploit_all_precedences(true);
  params.set_use_hard_precedences_in_cumulative(true);
  params.set_max_num_intervals_for_timetable_edge_finding(1000);
  params.set_use_overload_checker_in_cumulative(true);
  params.set_use_strong_propagation_in_disjunctive(true);
  params.set_use_timetable_edge_finding_in_cumulative(true);
  params.set_max_pairs_pairwise_reasoning_in_no_overlap_2d(50000);
  params.set_use_timetabling_in_no_overlap_2d(true);
  params.set_use_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_area_energetic_reasoning_in_no_overlap_2d(true);
  params.set_use_conservative_scale_overload_checker(true);
  params.set_use_dual_scheduling_heuristics(true);

  const CpSolverResponse response =
      operations_research::sat::SolveWithParameters(proto, params);

  params.set_cp_model_presolve(false);
  const CpSolverResponse response_no_presolve =
      operations_research::sat::SolveWithParameters(proto, params);

  CHECK_EQ(response.status() == CpSolverStatus::MODEL_INVALID,
           response_no_presolve.status() == CpSolverStatus::MODEL_INVALID)
      << "Model being invalid should not depend on presolve";

  if (response.status() == CpSolverStatus::MODEL_INVALID) {
    return;
  }

  if (response.status() == CpSolverStatus::UNKNOWN ||
      response_no_presolve.status() == CpSolverStatus::UNKNOWN) {
    return;
  }

  CHECK_EQ(response.status() == CpSolverStatus::INFEASIBLE,
           response_no_presolve.status() == CpSolverStatus::INFEASIBLE)
      << "Presolve should not change feasibility";
}

fuzztest::Domain<IntegerVariableProto> CpVariableDomain() {
  fuzztest::Domain<int64_t> bound_domain =
      fuzztest::InRange<int64_t>(-std::numeric_limits<int64_t>::max() / 2,
                                 std::numeric_limits<int64_t>::max() / 2);
  fuzztest::Domain<std::pair<int64_t, int64_t>> complex_domain_gap_and_size =
      fuzztest::PairOf(fuzztest::InRange<int64_t>(
                           1, std::numeric_limits<int64_t>::max() / 2),
                       fuzztest::InRange<int64_t>(
                           0, std::numeric_limits<int64_t>::max() / 2));
  fuzztest::Domain<IntegerVariableProto> var_domain = fuzztest::ReversibleMap(
      [](int64_t bound1, int64_t bound2,
         std::vector<std::pair<int64_t, int64_t>> complex_domain_segments) {
        IntegerVariableProto var;
        var.add_domain(std::min(bound1, bound2));
        var.add_domain(std::max(bound1, bound2));

        for (const auto& [gap, size] : complex_domain_segments) {
          var.add_domain(CapAdd(var.domain(var.domain().size() - 1), gap));
          var.add_domain(CapAdd(var.domain(var.domain().size() - 1), size));
        }
        return var;
      },
      [](IntegerVariableProto var) {
        using Type = std::tuple<int64_t, int64_t,
                                std::vector<std::pair<int64_t, int64_t>>>;
        Type domain_inputs;
        if (var.domain().size() < 2 || var.domain().size() % 2 != 0) {
          return std::optional<Type>();
        }
        domain_inputs = {var.domain(0), var.domain(1), {}};
        std::vector<std::pair<int64_t, int64_t>>& complex_domain_segments =
            std::get<2>(domain_inputs);
        for (int i = 2; i + 1 < var.domain().size(); i += 2) {
          const int64_t gap = CapSub(var.domain(i), var.domain(i - 1));
          const int64_t size = CapSub(var.domain(i + 1), var.domain(i));
          if (gap <= 0 || size < 0) {
            return std::optional<Type>();
          }
          complex_domain_segments.push_back({gap, size});
        }
        return std::optional<Type>(domain_inputs);
      },
      bound_domain, bound_domain,
      fuzztest::VectorOf(complex_domain_gap_and_size));

  return fuzztest::Filter(
      [](IntegerVariableProto var) {
        return var.domain(var.domain().size() - 1) <=
               std::numeric_limits<int64_t>::max() / 2;
      },
      var_domain);
}

fuzztest::Domain<std::vector<IntegerVariableProto>>
ModelProtoVariablesDomain() {
  return fuzztest::Filter(
      [](const std::vector<IntegerVariableProto>& vars) {
        // Check if the variables in isolation doesn't make for a invalid model.
        CpModelProto model;
        for (const IntegerVariableProto& var : vars) {
          *model.add_variables() = var;
        }
        return ValidateCpModel(model).empty();
      },
      fuzztest::VectorOf(CpVariableDomain()));
}

// Fuzzing repeats solve() 100 times, and timeout after 600s.
// With a time limit of 4s, we should be fine.
FUZZ_TEST(CpModelProtoFuzzer, Solve)
    .WithDomains(
        fuzztest::Arbitrary<CpModelProto>()
            .WithRepeatedProtobufField("variables", ModelProtoVariablesDomain())
            .WithRepeatedProtobufField(
                "constraints",
                fuzztest::VectorOf(fuzztest::Arbitrary<ConstraintProto>()
                                       .WithOneofAlwaysSet("constraint")
                                       .WithFieldUnset("name")
                                       .WithFieldUnset("dummy_constraint")))
            .WithFieldUnset("name")
            .WithFieldUnset("symmetry")
            .WithProtobufField("objective",
                               fuzztest::Arbitrary<CpObjectiveProto>()
                                   .WithFieldUnset("scaling_was_exact")
                                   .WithFieldUnset("integer_scaling_factor")
                                   .WithFieldUnset("integer_before_offset")
                                   .WithFieldUnset("integer_after_offset")))
    .WithSeeds([]() {
      return fuzztest::ReadFilesFromDirectory<CpModelProto>(GetTestDataDir());
    });

}  // namespace
}  // namespace operations_research::sat
