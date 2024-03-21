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

#include "ortools/sat/cp_model_solver.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#endif  // __PORTABLE_PLATFORM__
#include "absl/base/thread_annotations.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/connected_components.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_lns.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_postsolve.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_symmetries.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/feasibility_jump.h"
#include "ortools/sat/feasibility_pump.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/lb_tree_search.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_model.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/max_hs.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/parameters_validation.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/sat/work_assignment.h"
#include "ortools/util/logging.h"
#include "ortools/util/random_engine.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/util/sigint.h"
#endif  // __PORTABLE_PLATFORM__
#include "ortools/base/version.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

#if defined(_MSC_VER)
ABSL_FLAG(std::string, cp_model_dump_prefix, ".\\",
          "Prefix filename for all dumped files");
#else
ABSL_FLAG(std::string, cp_model_dump_prefix, "/tmp/",
          "Prefix filename for all dumped files");
#endif
ABSL_FLAG(bool, cp_model_dump_models, false,
          "DEBUG ONLY. When set to true, SolveCpModel() will dump its model "
          "protos (original model, presolved model, mapping model) in text "
          "format to 'FLAGS_cp_model_dump_prefix'{model|presolved_model|"
          "mapping_model}.pb.txt.");

ABSL_FLAG(
    bool, cp_model_export_model, false,
    "DEBUG ONLY. When set to true, SolveCpModel() will dump its input model "
    "protos in text format to 'FLAGS_cp_model_dump_prefix'{name}.pb.txt.");

ABSL_FLAG(bool, cp_model_dump_text_proto, true,
          "DEBUG ONLY, dump models in text proto instead of binary proto.");

ABSL_FLAG(bool, cp_model_dump_lns, false,
          "DEBUG ONLY. When set to true, solve will dump all "
          "lns models proto in text format to "
          "'FLAGS_cp_model_dump_prefix'lns_xxx.pb.txt.");

ABSL_FLAG(
    bool, cp_model_dump_problematic_lns, false,
    "DEBUG ONLY. Similar to --cp_model_dump_lns, but only dump fragment for "
    "which we got an issue while validating the postsolved solution. This "
    "allows to debug presolve issues without dumping all the models.");

ABSL_FLAG(bool, cp_model_dump_response, false,
          "DEBUG ONLY. If true, the final response of each solve will be "
          "dumped to 'FLAGS_cp_model_dump_prefix'response.pb.txt");

ABSL_FLAG(std::string, cp_model_params, "",
          "This is interpreted as a text SatParameters proto. The "
          "specified fields will override the normal ones for all solves.");

ABSL_FLAG(bool, debug_model_copy, false,
          "If true, copy the input model as if with no basic presolve");

ABSL_FLAG(bool, cp_model_check_intermediate_solutions, false,
          "When true, all intermediate solutions found by the solver will be "
          "checked. This can be expensive, therefore it is off by default.");

ABSL_FLAG(
    std::string, cp_model_load_debug_solution, "",
    "DEBUG ONLY. When this is set to a non-empty file name, "
    "we will interpret this as an internal solution which can be used for "
    "debugging. For instance we use it to identify wrong cuts/reasons.");

ABSL_FLAG(bool, cp_model_ignore_objective, false,
          "If true, ignore the objective.");
ABSL_FLAG(bool, cp_model_ignore_hints, false,
          "If true, ignore any supplied hints.");
ABSL_FLAG(bool, cp_model_fingerprint_model, true, "Fingerprint the model.");

namespace operations_research {
namespace sat {

std::string CpSatSolverVersion() {
  return absl::StrCat("CP-SAT solver v", OrToolsVersionString());
}

namespace {

// Makes the string fit in one line by cutting it in the middle if necessary.
std::string Summarize(absl::string_view input) {
  if (input.size() < 105) return std::string(input);
  const int half = 50;
  return absl::StrCat(input.substr(0, half), " ... ",
                      input.substr(input.size() - half, half));
}

template <class M>
void DumpModelProto(const M& proto, absl::string_view name) {
  std::string filename;
  if (absl::GetFlag(FLAGS_cp_model_dump_text_proto)) {
    filename = absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix), name,
                            ".pb.txt");
    LOG(INFO) << "Dumping " << name << " text proto to '" << filename << "'.";
  } else {
    const std::string filename =
        absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix), name, ".bin");
    LOG(INFO) << "Dumping " << name << " binary proto to '" << filename << "'.";
  }
  CHECK(WriteModelProtoToFile(proto, filename));
}

}  // namespace.

// =============================================================================
// Public API.
// =============================================================================

std::string CpModelStats(const CpModelProto& model_proto) {
  // Note that we only store pointer to "constant" string literals. This is
  // slightly faster and take less space for model with millions of constraints.
  absl::flat_hash_map<char const*, int> name_to_num_constraints;
  absl::flat_hash_map<char const*, int> name_to_num_reified;
  absl::flat_hash_map<char const*, int> name_to_num_multi_reified;
  absl::flat_hash_map<char const*, int> name_to_num_literals;
  absl::flat_hash_map<char const*, int> name_to_num_terms;
  absl::flat_hash_map<char const*, int> name_to_num_complex_domain;
  absl::flat_hash_map<char const*, int> name_to_num_expressions;

  int no_overlap_2d_num_rectangles = 0;
  int no_overlap_2d_num_optional_rectangles = 0;
  int no_overlap_2d_num_linear_areas = 0;
  int no_overlap_2d_num_quadratic_areas = 0;
  int no_overlap_2d_num_fixed_rectangles = 0;

  int cumulative_num_intervals = 0;
  int cumulative_num_optional_intervals = 0;
  int cumulative_num_variable_sizes = 0;
  int cumulative_num_variable_demands = 0;
  int cumulative_num_fixed_intervals = 0;

  int no_overlap_num_intervals = 0;
  int no_overlap_num_optional_intervals = 0;
  int no_overlap_num_variable_sizes = 0;
  int no_overlap_num_fixed_intervals = 0;

  int num_fixed_intervals = 0;

  for (const ConstraintProto& ct : model_proto.constraints()) {
    // We split the linear constraints into 3 buckets has it gives more insight
    // on the type of problem we are facing.
    char const* name;
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
      if (ct.linear().vars_size() == 0) name = "kLinear0";
      if (ct.linear().vars_size() == 1) name = "kLinear1";
      if (ct.linear().vars_size() == 2) name = "kLinear2";
      if (ct.linear().vars_size() == 3) name = "kLinear3";
      if (ct.linear().vars_size() > 3) name = "kLinearN";
    } else {
      name = ConstraintCaseName(ct.constraint_case()).data();
    }

    name_to_num_constraints[name]++;
    if (!ct.enforcement_literal().empty()) {
      name_to_num_reified[name]++;
      if (ct.enforcement_literal().size() > 1) {
        name_to_num_multi_reified[name]++;
      }
    }

    auto variable_is_fixed = [&model_proto](int ref) {
      const IntegerVariableProto& proto =
          model_proto.variables(PositiveRef(ref));
      return proto.domain_size() == 2 && proto.domain(0) == proto.domain(1);
    };

    auto expression_is_fixed =
        [&variable_is_fixed](const LinearExpressionProto& expr) {
          for (const int ref : expr.vars()) {
            if (!variable_is_fixed(ref)) {
              return false;
            }
          }
          return true;
        };

    auto interval_has_fixed_size = [&model_proto, &expression_is_fixed](int c) {
      return expression_is_fixed(model_proto.constraints(c).interval().size());
    };

    auto constraint_is_optional = [&model_proto](int i) {
      return !model_proto.constraints(i).enforcement_literal().empty();
    };

    auto interval_is_fixed = [&variable_is_fixed,
                              expression_is_fixed](const ConstraintProto& ct) {
      if (ct.constraint_case() != ConstraintProto::ConstraintCase::kInterval) {
        return false;
      }
      for (const int lit : ct.enforcement_literal()) {
        if (!variable_is_fixed(lit)) return false;
      }
      return (expression_is_fixed(ct.interval().start()) &&
              expression_is_fixed(ct.interval().size()) &&
              expression_is_fixed(ct.interval().end()));
    };

    // For pure Boolean constraints, we also display the total number of literal
    // involved as this gives a good idea of the problem size.
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
      name_to_num_literals[name] += ct.bool_or().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kBoolAnd) {
      name_to_num_literals[name] +=
          ct.enforcement_literal().size() + ct.bool_and().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kAtMostOne) {
      name_to_num_literals[name] += ct.at_most_one().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kExactlyOne) {
      name_to_num_literals[name] += ct.exactly_one().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kLinMax) {
      name_to_num_expressions[name] += ct.lin_max().exprs().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kInterval) {
      if (interval_is_fixed(ct)) num_fixed_intervals++;
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kNoOverlap2D) {
      const int num_boxes = ct.no_overlap_2d().x_intervals_size();
      no_overlap_2d_num_rectangles += num_boxes;
      for (int i = 0; i < num_boxes; ++i) {
        const int x_interval = ct.no_overlap_2d().x_intervals(i);
        const int y_interval = ct.no_overlap_2d().y_intervals(i);
        if (constraint_is_optional(x_interval) ||
            constraint_is_optional(y_interval)) {
          no_overlap_2d_num_optional_rectangles++;
        }
        const int num_fixed = interval_has_fixed_size(x_interval) +
                              interval_has_fixed_size(y_interval);
        if (num_fixed == 0) {
          no_overlap_2d_num_quadratic_areas++;
        } else if (num_fixed == 1) {
          no_overlap_2d_num_linear_areas++;
        }
        if (interval_is_fixed(model_proto.constraints(x_interval)) &&
            interval_is_fixed(model_proto.constraints(y_interval))) {
          no_overlap_2d_num_fixed_rectangles++;
        }
      }
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kNoOverlap) {
      const int num_intervals = ct.no_overlap().intervals_size();
      no_overlap_num_intervals += num_intervals;
      for (int i = 0; i < num_intervals; ++i) {
        const int interval = ct.no_overlap().intervals(i);
        if (constraint_is_optional(interval)) {
          no_overlap_num_optional_intervals++;
        }
        if (!interval_has_fixed_size(interval)) {
          no_overlap_num_variable_sizes++;
        }
        if (interval_is_fixed(model_proto.constraints(interval))) {
          no_overlap_num_fixed_intervals++;
        }
      }
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kCumulative) {
      const int num_intervals = ct.cumulative().intervals_size();
      cumulative_num_intervals += num_intervals;
      for (int i = 0; i < num_intervals; ++i) {
        const int interval = ct.cumulative().intervals(i);
        if (constraint_is_optional(interval)) {
          cumulative_num_optional_intervals++;
        }
        if (!interval_has_fixed_size(interval)) {
          cumulative_num_variable_sizes++;
        }
        if (!expression_is_fixed(ct.cumulative().demands(i))) {
          cumulative_num_variable_demands++;
        }
        if (interval_is_fixed(model_proto.constraints(interval))) {
          cumulative_num_fixed_intervals++;
        }
      }
    }

    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear &&
        ct.linear().vars_size() > 3) {
      name_to_num_terms[name] += ct.linear().vars_size();
    }
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear &&
        ct.linear().vars_size() > 1 && ct.linear().domain().size() > 2) {
      name_to_num_complex_domain[name]++;
    }
  }

  int num_constants = 0;
  absl::btree_set<int64_t> constant_values;
  absl::btree_map<Domain, int> num_vars_per_domains;
  for (const IntegerVariableProto& var : model_proto.variables()) {
    if (var.domain_size() == 2 && var.domain(0) == var.domain(1)) {
      ++num_constants;
      constant_values.insert(var.domain(0));
    } else {
      num_vars_per_domains[ReadDomainFromProto(var)]++;
    }
  }

  std::string result;
  const std::string model_fingerprint_str =
      (absl::GetFlag(FLAGS_cp_model_fingerprint_model))
          ? absl::StrFormat(" (model_fingerprint: %#x)",
                            FingerprintModel(model_proto))
          : "";

  if (model_proto.has_objective() ||
      model_proto.has_floating_point_objective()) {
    absl::StrAppend(&result, "optimization model '", model_proto.name(),
                    "':", model_fingerprint_str, "\n");
  } else {
    absl::StrAppend(&result, "satisfaction model '", model_proto.name(),
                    "':", model_fingerprint_str, "\n");
  }

  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    absl::StrAppend(
        &result, "Search strategy: on ",
        strategy.exprs().size() + strategy.variables().size(), " variables, ",
        ProtoEnumToString<DecisionStrategyProto::VariableSelectionStrategy>(
            strategy.variable_selection_strategy()),
        ", ",
        ProtoEnumToString<DecisionStrategyProto::DomainReductionStrategy>(
            strategy.domain_reduction_strategy()),
        "\n");
  }

  auto count_variables_by_type =
      [&model_proto](const google::protobuf::RepeatedField<int>& vars,
                     int* num_booleans, int* num_integers) {
        for (const int ref : vars) {
          const auto& var_proto = model_proto.variables(PositiveRef(ref));
          if (var_proto.domain_size() == 2 && var_proto.domain(0) == 0 &&
              var_proto.domain(1) == 1) {
            (*num_booleans)++;
          }
        }
        *num_integers = vars.size() - *num_booleans;
      };

  {
    int num_boolean_variables_in_objective = 0;
    int num_integer_variables_in_objective = 0;
    if (model_proto.has_objective()) {
      count_variables_by_type(model_proto.objective().vars(),
                              &num_boolean_variables_in_objective,
                              &num_integer_variables_in_objective);
    }
    if (model_proto.has_floating_point_objective()) {
      count_variables_by_type(model_proto.floating_point_objective().vars(),
                              &num_boolean_variables_in_objective,
                              &num_integer_variables_in_objective);
    }

    std::vector<std::string> obj_vars_strings;
    if (num_boolean_variables_in_objective > 0) {
      obj_vars_strings.push_back(absl::StrCat(
          "#bools: ", FormatCounter(num_boolean_variables_in_objective)));
    }
    if (num_integer_variables_in_objective > 0) {
      obj_vars_strings.push_back(absl::StrCat(
          "#ints: ", FormatCounter(num_integer_variables_in_objective)));
    }

    const std::string objective_string =
        model_proto.has_objective()
            ? absl::StrCat(" (", absl::StrJoin(obj_vars_strings, " "),
                           " in objective)")
            : (model_proto.has_floating_point_objective()
                   ? absl::StrCat(" (", absl::StrJoin(obj_vars_strings, " "),
                                  " in floating point objective)")
                   : "");
    absl::StrAppend(&result,
                    "#Variables: ", FormatCounter(model_proto.variables_size()),
                    objective_string, "\n");
  }
  if (num_vars_per_domains.contains(Domain(0, 1))) {
    // We always list Boolean first.
    const int num_bools = num_vars_per_domains[Domain(0, 1)];
    const std::string temp =
        absl::StrCat("  - ", FormatCounter(num_bools), " Booleans in ",
                     Domain(0, 1).ToString(), "\n");
    absl::StrAppend(&result, Summarize(temp));
    num_vars_per_domains.erase(Domain(0, 1));
  }
  if (num_vars_per_domains.size() < 100) {
    for (const auto& entry : num_vars_per_domains) {
      const std::string temp =
          absl::StrCat("  - ", FormatCounter(entry.second), " in ",
                       entry.first.ToString(), "\n");
      absl::StrAppend(&result, Summarize(temp));
    }
  } else {
    int64_t max_complexity = 0;
    int64_t min = std::numeric_limits<int64_t>::max();
    int64_t max = std::numeric_limits<int64_t>::min();
    for (const auto& entry : num_vars_per_domains) {
      min = std::min(min, entry.first.Min());
      max = std::max(max, entry.first.Max());
      max_complexity = std::max(
          max_complexity, static_cast<int64_t>(entry.first.NumIntervals()));
    }
    absl::StrAppend(&result, "  - ", FormatCounter(num_vars_per_domains.size()),
                    " different domains in [", min, ",", max,
                    "] with a largest complexity of ", max_complexity, ".\n");
  }

  if (num_constants > 0) {
    const std::string temp =
        absl::StrCat("  - ", num_constants, " constants in {",
                     absl::StrJoin(constant_values, ","), "} \n");
    absl::StrAppend(&result, Summarize(temp));
  }

  std::vector<std::string> constraints;
  constraints.reserve(name_to_num_constraints.size());
  for (const auto& [c_name, count] : name_to_num_constraints) {
    const std::string name(c_name);
    constraints.push_back(absl::StrCat("#", name, ": ", FormatCounter(count)));
    if (name_to_num_reified.contains(c_name)) {
      if (name_to_num_multi_reified.contains(c_name)) {
        absl::StrAppend(
            &constraints.back(),
            " (#enforced: ", FormatCounter(name_to_num_reified[c_name]),
            " #multi: ", FormatCounter(name_to_num_multi_reified[c_name]), ")");
      } else {
        absl::StrAppend(&constraints.back(), " (#enforced: ",
                        FormatCounter(name_to_num_reified[c_name]), ")");
      }
    }
    if (name_to_num_literals.contains(c_name)) {
      absl::StrAppend(&constraints.back(), " (#literals: ",
                      FormatCounter(name_to_num_literals[c_name]), ")");
    }
    if (name_to_num_terms.contains(c_name)) {
      absl::StrAppend(&constraints.back(),
                      " (#terms: ", FormatCounter(name_to_num_terms[c_name]),
                      ")");
    }
    if (name_to_num_expressions.contains(c_name)) {
      absl::StrAppend(&constraints.back(), " (#expressions: ",
                      FormatCounter(name_to_num_expressions[c_name]), ")");
    }
    if (name_to_num_complex_domain.contains(c_name)) {
      absl::StrAppend(&constraints.back(), " (#complex_domain: ",
                      FormatCounter(name_to_num_complex_domain[c_name]), ")");
    }
    if (name == "kInterval" && num_fixed_intervals > 0) {
      absl::StrAppend(&constraints.back(), " (#fixed: ", num_fixed_intervals,
                      ")");
    } else if (name == "kNoOverlap2D") {
      absl::StrAppend(&constraints.back(),
                      " (#rectangles: ", no_overlap_2d_num_rectangles);
      if (no_overlap_2d_num_optional_rectangles > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #optional: ", no_overlap_2d_num_optional_rectangles);
      }
      if (no_overlap_2d_num_linear_areas > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #linear_areas: ", no_overlap_2d_num_linear_areas);
      }
      if (no_overlap_2d_num_quadratic_areas > 0) {
        absl::StrAppend(&constraints.back(), ", #quadratic_areas: ",
                        no_overlap_2d_num_quadratic_areas);
      }
      if (no_overlap_2d_num_fixed_rectangles > 0) {
        absl::StrAppend(&constraints.back(), ", #fixed_rectangles: ",
                        no_overlap_2d_num_fixed_rectangles);
      }
      absl::StrAppend(&constraints.back(), ")");
    } else if (name == "kCumulative") {
      absl::StrAppend(&constraints.back(),
                      " (#intervals: ", cumulative_num_intervals);
      if (cumulative_num_optional_intervals > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #optional: ", cumulative_num_optional_intervals);
      }
      if (cumulative_num_variable_sizes > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #variable_sizes: ", cumulative_num_variable_sizes);
      }
      if (cumulative_num_variable_demands > 0) {
        absl::StrAppend(&constraints.back(), ", #variable_demands: ",
                        cumulative_num_variable_demands);
      }
      if (cumulative_num_fixed_intervals > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #fixed_intervals: ", cumulative_num_fixed_intervals);
      }
      absl::StrAppend(&constraints.back(), ")");
    } else if (name == "kNoOverlap") {
      absl::StrAppend(&constraints.back(),
                      " (#intervals: ", no_overlap_num_intervals);
      if (no_overlap_num_optional_intervals > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #optional: ", no_overlap_num_optional_intervals);
      }
      if (no_overlap_num_variable_sizes > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #variable_sizes: ", no_overlap_num_variable_sizes);
      }
      if (no_overlap_num_fixed_intervals > 0) {
        absl::StrAppend(&constraints.back(),
                        ", #fixed_intervals: ", no_overlap_num_fixed_intervals);
      }
      absl::StrAppend(&constraints.back(), ")");
    }
  }
  std::sort(constraints.begin(), constraints.end());
  absl::StrAppend(&result, absl::StrJoin(constraints, "\n"));

  return result;
}

std::string CpSolverResponseStats(const CpSolverResponse& response,
                                  bool has_objective) {
  std::string result;
  absl::StrAppend(&result, "CpSolverResponse summary:");
  absl::StrAppend(&result, "\nstatus: ",
                  ProtoEnumToString<CpSolverStatus>(response.status()));

  if (has_objective && response.status() != CpSolverStatus::INFEASIBLE) {
    absl::StrAppendFormat(&result, "\nobjective: %.16g",
                          response.objective_value());
    absl::StrAppendFormat(&result, "\nbest_bound: %.16g",
                          response.best_objective_bound());
  } else {
    absl::StrAppend(&result, "\nobjective: NA");
    absl::StrAppend(&result, "\nbest_bound: NA");
  }

  absl::StrAppend(&result, "\nintegers: ", response.num_integers());
  absl::StrAppend(&result, "\nbooleans: ", response.num_booleans());
  absl::StrAppend(&result, "\nconflicts: ", response.num_conflicts());
  absl::StrAppend(&result, "\nbranches: ", response.num_branches());

  // TODO(user): This is probably better named "binary_propagation", but we just
  // output "propagations" to be consistent with sat/analyze.sh.
  absl::StrAppend(&result,
                  "\npropagations: ", response.num_binary_propagations());
  absl::StrAppend(
      &result, "\ninteger_propagations: ", response.num_integer_propagations());

  absl::StrAppend(&result, "\nrestarts: ", response.num_restarts());
  absl::StrAppend(&result, "\nlp_iterations: ", response.num_lp_iterations());
  absl::StrAppend(&result, "\nwalltime: ", response.wall_time());
  absl::StrAppend(&result, "\nusertime: ", response.user_time());
  absl::StrAppend(&result,
                  "\ndeterministic_time: ", response.deterministic_time());
  absl::StrAppend(&result, "\ngap_integral: ", response.gap_integral());
  if (!response.solution().empty()) {
    absl::StrAppendFormat(
        &result, "\nsolution_fingerprint: %#x",
        FingerprintRepeatedField(response.solution(), kDefaultFingerprintSeed));
  }
  absl::StrAppend(&result, "\n");
  return result;
}

namespace {

// This should be called on the presolved model. It will read the file
// specified by --cp_model_load_debug_solution and properly fill the
// model->Get<DebugSolution>() proto vector.
void LoadDebugSolution(const CpModelProto& model_proto, Model* model) {
#if !defined(__PORTABLE_PLATFORM__)
  if (absl::GetFlag(FLAGS_cp_model_load_debug_solution).empty()) return;

  CpSolverResponse response;
  SOLVER_LOG(model->GetOrCreate<SolverLogger>(),
             "Reading debug solution from '",
             absl::GetFlag(FLAGS_cp_model_load_debug_solution), "'.");
  CHECK_OK(file::GetTextProto(absl::GetFlag(FLAGS_cp_model_load_debug_solution),
                              &response, file::Defaults()));

  // Make sure we load a solution with the same number of variable has in the
  // presolved model.
  CHECK_EQ(response.solution().size(), model_proto.variables().size());
  model->GetOrCreate<SharedResponseManager>()->LoadDebugSolution(
      response.solution());
#endif  // __PORTABLE_PLATFORM__
}

// This both copy the "main" DebugSolution to a local_model and also cache
// the value of the integer variables in that solution.
void InitializeDebugSolution(const CpModelProto& model_proto, Model* model) {
  auto* shared_response = model->Get<SharedResponseManager>();
  if (shared_response == nullptr) return;
  if (shared_response->DebugSolution().empty()) return;

  // Copy the proto values.
  DebugSolution& debug_sol = *model->GetOrCreate<DebugSolution>();
  debug_sol.proto_values = shared_response->DebugSolution();

  // Fill the values by integer variable.
  const int num_integers =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value();
  debug_sol.ivar_has_value.assign(num_integers, false);
  debug_sol.ivar_values.assign(num_integers, 0);

  std::vector<Literal> boolean_solution;

  const auto& mapping = *model->GetOrCreate<CpModelMapping>();
  for (int i = 0; i < debug_sol.proto_values.size(); ++i) {
    if (mapping.IsBoolean(i)) {
      Literal l = mapping.Literal(i);
      if (debug_sol.proto_values[i] == 0) {
        l = l.Negated();
      }
      boolean_solution.push_back(l);
    }

    if (!mapping.IsInteger(i)) continue;
    const IntegerVariable var = mapping.Integer(i);
    debug_sol.ivar_has_value[var] = true;
    debug_sol.ivar_has_value[NegationOf(var)] = true;
    debug_sol.ivar_values[var] = debug_sol.proto_values[i];
    debug_sol.ivar_values[NegationOf(var)] = -debug_sol.proto_values[i];
  }

  // If the solution is fully boolean (there is no integer variable), and
  // we have a decision problem (so no new boolean should be created), we load
  // it in the sat solver for debugging too.
  if (boolean_solution.size() == debug_sol.proto_values.size() &&
      !model_proto.has_objective()) {
    LOG(INFO) << "Loaded pure Boolean debugging solution.";
    model->GetOrCreate<SatSolver>()->LoadDebugSolution(boolean_solution);
  }

  // The objective variable is usually not part of the proto, but it is still
  // nice to have it, so we recompute it here.
  auto* objective_def = model->Get<ObjectiveDefinition>();
  if (objective_def != nullptr) {
    const IntegerVariable objective_var = objective_def->objective_var;
    const int64_t objective_value =
        ComputeInnerObjective(model_proto.objective(), debug_sol.proto_values);
    debug_sol.ivar_has_value[objective_var] = true;
    debug_sol.ivar_has_value[NegationOf(objective_var)] = true;
    debug_sol.ivar_values[objective_var] = objective_value;
    debug_sol.ivar_values[NegationOf(objective_var)] = -objective_value;
  }

  // We also register a DEBUG callback to check our reasons.
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  const auto checker = [mapping, encoder, debug_sol, model](
                           absl::Span<const Literal> clause,
                           absl::Span<const IntegerLiteral> integers) {
    bool is_satisfied = false;
    int num_bools = 0;
    int num_ints = 0;
    std::vector<std::tuple<Literal, IntegerLiteral, int>> to_print;
    for (const Literal l : clause) {
      // First case, this Boolean is mapped.
      {
        const int proto_var =
            mapping.GetProtoVariableFromBooleanVariable(l.Variable());
        if (proto_var != -1) {
          to_print.push_back({l, IntegerLiteral(), proto_var});
          if (debug_sol.proto_values[proto_var] == (l.IsPositive() ? 1 : 0)) {
            is_satisfied = true;
            break;
          }
          ++num_bools;
          continue;
        }
      }

      // Second case, it is associated to IntVar >= value.
      // We can use any of them, so if one is false, we use this one.
      bool all_true = true;
      for (const IntegerLiteral associated : encoder->GetIntegerLiterals(l)) {
        const int proto_var = mapping.GetProtoVariableFromIntegerVariable(
            PositiveVariable(associated.var));
        if (proto_var == -1) break;
        int64_t value = debug_sol.proto_values[proto_var];
        to_print.push_back({l, associated, proto_var});

        if (!VariableIsPositive(associated.var)) value = -value;
        if (value < associated.bound) {
          ++num_ints;
          all_true = false;
          break;
        }
      }
      if (all_true) {
        is_satisfied = true;
        break;
      }
    }
    for (const IntegerLiteral i_lit : integers) {
      const int proto_var = mapping.GetProtoVariableFromIntegerVariable(
          PositiveVariable(i_lit.var));
      if (proto_var == -1) {
        is_satisfied = true;
        break;
      }

      int64_t value = debug_sol.proto_values[proto_var];
      to_print.push_back({Literal(kNoLiteralIndex), i_lit, proto_var});

      if (!VariableIsPositive(i_lit.var)) value = -value;
      // Note the sign is inversed, we cannot have all literal false and all
      // integer literal true.
      if (value >= i_lit.bound) {
        is_satisfied = true;
        break;
      }
    }
    if (!is_satisfied) {
      LOG(INFO) << "Reason clause is not satisfied by loaded solution:";
      LOG(INFO) << "Worker '" << model->Name() << "', level="
                << model->GetOrCreate<SatSolver>()->CurrentDecisionLevel();
      LOG(INFO) << "literals (neg): " << clause;
      LOG(INFO) << "integer literals: " << integers;
      for (const auto [l, i_lit, proto_var] : to_print) {
        LOG(INFO) << l << " " << i_lit << " var=" << proto_var
                  << " value_in_sol=" << debug_sol.proto_values[proto_var];
      }
    }
    return is_satisfied;
  };
  const auto lit_checker = [checker](absl::Span<const Literal> clause) {
    return checker(clause, {});
  };

  model->GetOrCreate<Trail>()->RegisterDebugChecker(lit_checker);
  model->GetOrCreate<IntegerTrail>()->RegisterDebugChecker(checker);
}

std::vector<int64_t> GetSolutionValues(const CpModelProto& model_proto,
                                       const Model& model) {
  auto* mapping = model.Get<CpModelMapping>();
  auto* trail = model.Get<Trail>();

  std::vector<int64_t> solution;
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsInteger(i)) {
      const IntegerVariable var = mapping->Integer(i);

      // For ignored or not fully instantiated variable, we just use the
      // lower bound.
      solution.push_back(model.Get(LowerBound(var)));
    } else {
      DCHECK(mapping->IsBoolean(i));
      const Literal literal = mapping->Literal(i);
      if (trail->Assignment().LiteralIsAssigned(literal)) {
        solution.push_back(model.Get(Value(literal)));
      } else {
        // Just use the lower bound if the variable is not fully instantiated.
        solution.push_back(0);
      }
    }
  }

  if (DEBUG_MODE ||
      absl::GetFlag(FLAGS_cp_model_check_intermediate_solutions)) {
    // TODO(user): Checks against initial model.
    CHECK(SolutionIsFeasible(model_proto, solution));
  }
  return solution;
}

namespace {

IntegerVariable GetOrCreateVariableWithTightBound(
    const std::vector<std::pair<IntegerVariable, int64_t>>& terms,
    Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  int64_t sum_min = 0;
  int64_t sum_max = 0;
  for (const std::pair<IntegerVariable, int64_t>& var_coeff : terms) {
    const int64_t min_domain = model->Get(LowerBound(var_coeff.first));
    const int64_t max_domain = model->Get(UpperBound(var_coeff.first));
    const int64_t coeff = var_coeff.second;
    const int64_t prod1 = min_domain * coeff;
    const int64_t prod2 = max_domain * coeff;
    sum_min += std::min(prod1, prod2);
    sum_max += std::max(prod1, prod2);
  }
  return model->Add(NewIntegerVariable(sum_min, sum_max));
}

IntegerVariable GetOrCreateVariableLinkedToSumOf(
    const std::vector<std::pair<IntegerVariable, int64_t>>& terms,
    bool use_equality, Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  // Add var == terms or var >= terms if use_equality = false.
  const IntegerVariable new_var =
      GetOrCreateVariableWithTightBound(terms, model);

  // TODO(user): use the same format, i.e. LinearExpression in both code!
  std::vector<IntegerVariable> vars;
  std::vector<int64_t> coeffs;
  for (const auto [var, coeff] : terms) {
    vars.push_back(var);
    coeffs.push_back(coeff);
  }
  vars.push_back(new_var);
  coeffs.push_back(-1);

  // We want == 0 or <= 0 if use_equality = false.
  const bool lb_required = use_equality;
  const bool ub_required = true;

  // Split if linear is large.
  if (vars.size() > model->GetOrCreate<SatParameters>()->linear_split_size()) {
    SplitAndLoadIntermediateConstraints(lb_required, ub_required, &vars,
                                        &coeffs, model);
  }

  // Load the top-level constraint.
  if (lb_required) {
    model->Add(WeightedSumGreaterOrEqual(vars, coeffs, 0));
  }
  if (ub_required) {
    model->Add(WeightedSumLowerOrEqual(vars, coeffs, 0));
  }

  return new_var;
}

}  // namespace

// Adds one LinearProgrammingConstraint per connected component of the model.
IntegerVariable AddLPConstraints(bool objective_need_to_be_tight,
                                 const CpModelProto& model_proto, Model* m) {
  // Non const as we will std::move() stuff out of there.
  LinearRelaxation relaxation = ComputeLinearRelaxation(model_proto, m);
  if (m->GetOrCreate<SatSolver>()->ModelIsUnsat()) return kNoIntegerVariable;

  // The bipartite graph of LP constraints might be disconnected:
  // make a partition of the variables into connected components.
  // Constraint nodes are indexed by [0..num_lp_constraints),
  // variable nodes by [num_lp_constraints..num_lp_constraints+num_variables).
  //
  // TODO(user): look into biconnected components.
  const int num_lp_constraints =
      static_cast<int>(relaxation.linear_constraints.size());
  const int num_lp_cut_generators =
      static_cast<int>(relaxation.cut_generators.size());
  const int num_integer_variables =
      m->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value();

  DenseConnectedComponentsFinder components;
  components.SetNumberOfNodes(num_lp_constraints + num_lp_cut_generators +
                              num_integer_variables);
  auto get_constraint_index = [](int ct_index) { return ct_index; };
  auto get_cut_generator_index = [num_lp_constraints](int cut_index) {
    return num_lp_constraints + cut_index;
  };
  auto get_var_index = [num_lp_constraints,
                        num_lp_cut_generators](IntegerVariable var) {
    return num_lp_constraints + num_lp_cut_generators +
           PositiveVariable(var).value();
  };
  for (int i = 0; i < num_lp_constraints; i++) {
    for (const IntegerVariable var :
         relaxation.linear_constraints[i].VarsAsSpan()) {
      components.AddEdge(get_constraint_index(i), get_var_index(var));
    }
  }
  for (int i = 0; i < num_lp_cut_generators; ++i) {
    for (const IntegerVariable var : relaxation.cut_generators[i].vars) {
      components.AddEdge(get_cut_generator_index(i), get_var_index(var));
    }
  }

  const int num_components = components.GetNumberOfComponents();
  std::vector<int> component_sizes(num_components, 0);
  const std::vector<int> index_to_component = components.GetComponentIds();
  for (int i = 0; i < num_lp_constraints; i++) {
    ++component_sizes[index_to_component[get_constraint_index(i)]];
  }
  for (int i = 0; i < num_lp_cut_generators; i++) {
    ++component_sizes[index_to_component[get_cut_generator_index(i)]];
  }

  // TODO(user): Optimize memory layout.
  std::vector<std::vector<IntegerVariable>> component_to_var(num_components);
  for (IntegerVariable var(0); var < num_integer_variables; var += 2) {
    DCHECK(VariableIsPositive(var));
    component_to_var[index_to_component[get_var_index(var)]].push_back(var);
  }

  // Make sure any constraint that touch the objective is not discarded even
  // if it is the only one in its component. This is important to propagate
  // as much as possible the objective bound by using any bounds the LP give
  // us on one of its components. This is critical on the zephyrus problems for
  // instance.
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
    const IntegerVariable var =
        mapping->Integer(model_proto.objective().vars(i));
    ++component_sizes[index_to_component[get_var_index(var)]];
  }

  // Dispatch every constraint to its LinearProgrammingConstraint.
  std::vector<LinearProgrammingConstraint*> lp_constraints(num_components,
                                                           nullptr);
  for (int i = 0; i < num_lp_constraints; i++) {
    const int c = index_to_component[get_constraint_index(i)];
    if (component_sizes[c] <= 1) continue;
    if (lp_constraints[c] == nullptr) {
      lp_constraints[c] =
          new LinearProgrammingConstraint(m, component_to_var[c]);
      m->TakeOwnership(lp_constraints[c]);
    }
    // Load the constraint.
    lp_constraints[c]->AddLinearConstraint(
        std::move(relaxation.linear_constraints[i]));
  }

  // Dispatch every cut generator to its LinearProgrammingConstraint.
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int c = index_to_component[get_cut_generator_index(i)];
    if (lp_constraints[c] == nullptr) {
      lp_constraints[c] =
          new LinearProgrammingConstraint(m, component_to_var[c]);
      m->TakeOwnership(lp_constraints[c]);
    }
    lp_constraints[c]->AddCutGenerator(std::move(relaxation.cut_generators[i]));
  }

  // Add the objective.
  std::vector<std::vector<std::pair<IntegerVariable, int64_t>>>
      component_to_cp_terms(num_components);
  std::vector<std::pair<IntegerVariable, int64_t>> top_level_cp_terms;
  int num_components_containing_objective = 0;
  if (model_proto.has_objective()) {
    // First pass: set objective coefficients on the lp constraints, and store
    // the cp terms in one vector per component.
    for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
      const IntegerVariable var =
          mapping->Integer(model_proto.objective().vars(i));
      const int64_t coeff = model_proto.objective().coeffs(i);
      const int c = index_to_component[get_var_index(var)];
      if (lp_constraints[c] != nullptr) {
        lp_constraints[c]->SetObjectiveCoefficient(var, IntegerValue(coeff));
        component_to_cp_terms[c].push_back(std::make_pair(var, coeff));
      } else {
        // Component is too small. We still need to store the objective term.
        top_level_cp_terms.push_back(std::make_pair(var, coeff));
      }
    }
    // Second pass: Build the cp sub-objectives per component.
    for (int c = 0; c < num_components; ++c) {
      if (component_to_cp_terms[c].empty()) continue;
      const IntegerVariable sub_obj_var = GetOrCreateVariableLinkedToSumOf(
          component_to_cp_terms[c], objective_need_to_be_tight, m);
      top_level_cp_terms.push_back(std::make_pair(sub_obj_var, 1));
      lp_constraints[c]->SetMainObjectiveVariable(sub_obj_var);
      num_components_containing_objective++;
    }
  }

  const IntegerVariable main_objective_var =
      model_proto.has_objective()
          ? GetOrCreateVariableLinkedToSumOf(top_level_cp_terms,
                                             objective_need_to_be_tight, m)
          : kNoIntegerVariable;

  // Register LP constraints. Note that this needs to be done after all the
  // constraints have been added.
  for (LinearProgrammingConstraint* lp_constraint : lp_constraints) {
    if (lp_constraint == nullptr) continue;
    lp_constraint->RegisterWith(m);
    VLOG(3) << "LP constraint: " << lp_constraint->DimensionString() << ".";
  }

  VLOG(3) << top_level_cp_terms.size()
          << " terms in the main objective linear equation ("
          << num_components_containing_objective << " from LP constraints).";
  return main_objective_var;
}

}  // namespace

// Used by NewFeasibleSolutionObserver or NewFeasibleSolutionLogCallback
// to register observers.
struct SolutionObservers {
  std::vector<std::function<void(const CpSolverResponse& response)>> observers;
  std::vector<std::function<std::string(const CpSolverResponse& response)>>
      log_callbacks;
};

std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& observer) {
  return [=](Model* model) {
    model->GetOrCreate<SolutionObservers>()->observers.push_back(observer);
  };
}

std::function<void(Model*)> NewFeasibleSolutionLogCallback(
    const std::function<std::string(const CpSolverResponse& response)>&
        callback) {
  return [=](Model* model) {
    model->GetOrCreate<SolutionObservers>()->log_callbacks.push_back(callback);
  };
}

#if !defined(__PORTABLE_PLATFORM__)
// TODO(user): Support it on android.
std::function<SatParameters(Model*)> NewSatParameters(
    const std::string& params) {
  sat::SatParameters parameters;
  if (!params.empty()) {
    CHECK(google::protobuf::TextFormat::ParseFromString(params, &parameters))
        << params;
  }
  return NewSatParameters(parameters);
}
#endif  // __PORTABLE_PLATFORM__

std::function<SatParameters(Model*)> NewSatParameters(
    const sat::SatParameters& parameters) {
  return [=](Model* model) {
    // Tricky: It is important to initialize the model parameters before any
    // of the solver object are created, so that by default they use the given
    // parameters.
    //
    // TODO(user): A notable exception to this is the TimeLimit which is
    // currently not initializing itself from the SatParameters in the model. It
    // will also starts counting from the time of its creation. It will be good
    // to find a solution that is less error prone.
    *model->GetOrCreate<SatParameters>() = parameters;
    return parameters;
  };
}

namespace {

// Registers a callback that will export variables bounds fixed at level 0 of
// the search. This should not be registered to a LNS search.
void RegisterVariableBoundsLevelZeroExport(
    const CpModelProto& /*model_proto*/,
    SharedBoundsManager* shared_bounds_manager, Model* model) {
  CHECK(shared_bounds_manager != nullptr);

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  auto* trail = model->Get<Trail>();
  auto* integer_trail = model->Get<IntegerTrail>();

  int saved_trail_index = 0;
  std::vector<int> model_variables;
  std::vector<int64_t> new_lower_bounds;
  std::vector<int64_t> new_upper_bounds;
  absl::flat_hash_set<int> visited_variables;
  const std::string name = model->Name();

  auto broadcast_level_zero_bounds =
      [=](const std::vector<IntegerVariable>& modified_vars) mutable {
        // Inspect the modified IntegerVariables.
        for (const IntegerVariable& var : modified_vars) {
          const IntegerVariable positive_var = PositiveVariable(var);
          const int model_var =
              mapping->GetProtoVariableFromIntegerVariable(positive_var);

          if (model_var == -1) continue;
          const auto [_, inserted] = visited_variables.insert(model_var);
          if (!inserted) continue;

          const int64_t new_lb =
              integer_trail->LevelZeroLowerBound(positive_var).value();
          const int64_t new_ub =
              integer_trail->LevelZeroUpperBound(positive_var).value();

          // TODO(user): We could imagine an API based on atomic<int64_t>
          // that could preemptively check if this new bounds are improving.
          model_variables.push_back(model_var);
          new_lower_bounds.push_back(new_lb);
          new_upper_bounds.push_back(new_ub);
        }

        // Inspect the newly modified Booleans.
        for (; saved_trail_index < trail->Index(); ++saved_trail_index) {
          const Literal fixed_literal = (*trail)[saved_trail_index];
          const int model_var = mapping->GetProtoVariableFromBooleanVariable(
              fixed_literal.Variable());

          if (model_var == -1) continue;
          const auto [_, inserted] = visited_variables.insert(model_var);
          if (!inserted) continue;

          model_variables.push_back(model_var);
          if (fixed_literal.IsPositive()) {
            new_lower_bounds.push_back(1);
            new_upper_bounds.push_back(1);
          } else {
            new_lower_bounds.push_back(0);
            new_upper_bounds.push_back(0);
          }
        }

        if (!model_variables.empty()) {
          shared_bounds_manager->ReportPotentialNewBounds(
              model->Name(), model_variables, new_lower_bounds,
              new_upper_bounds);

          // Clear for next call.
          model_variables.clear();
          new_lower_bounds.clear();
          new_upper_bounds.clear();
          visited_variables.clear();

          // If we are not in interleave_search we synchronize right away.
          if (!model->Get<SatParameters>()->interleave_search()) {
            shared_bounds_manager->Synchronize();
          }
        }
      };

  // The callback will just be called on NEWLY modified var. So initially,
  // we do want to read all variables.
  //
  // TODO(user): Find a better way? It seems nicer to register this before
  // any variable is modified. But then we don't want to call it each time
  // we reach level zero during probing. It should be better to only call
  // it when a new variable has been fixed.
  const IntegerVariable num_vars =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables();
  std::vector<IntegerVariable> all_variables;
  all_variables.reserve(num_vars.value());
  for (IntegerVariable var(0); var < num_vars; ++var) {
    all_variables.push_back(var);
  }
  broadcast_level_zero_bounds(all_variables);

  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(broadcast_level_zero_bounds);
}

// Registers a callback to import new variables bounds stored in the
// shared_bounds_manager. These bounds are imported at level 0 of the search
// in the linear scan minimize function.
void RegisterVariableBoundsLevelZeroImport(
    const CpModelProto& model_proto, SharedBoundsManager* shared_bounds_manager,
    Model* model) {
  CHECK(shared_bounds_manager != nullptr);
  const std::string name = model->Name();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* trail = model->GetOrCreate<Trail>();
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const int id = shared_bounds_manager->RegisterNewId();

  const auto& import_level_zero_bounds = [&model_proto, shared_bounds_manager,
                                          name, sat_solver, integer_trail,
                                          trail, id, mapping]() {
    std::vector<int> model_variables;
    std::vector<int64_t> new_lower_bounds;
    std::vector<int64_t> new_upper_bounds;
    shared_bounds_manager->GetChangedBounds(
        id, &model_variables, &new_lower_bounds, &new_upper_bounds);
    bool new_bounds_have_been_imported = false;
    for (int i = 0; i < model_variables.size(); ++i) {
      const int model_var = model_variables[i];

      // If this is a Boolean, fix it if not already done.
      // Note that it is important not to use AddUnitClause() as we do not
      // want to propagate after each addition.
      if (mapping->IsBoolean(model_var)) {
        Literal lit = mapping->Literal(model_var);
        if (new_upper_bounds[i] == 0) lit = lit.Negated();
        if (trail->Assignment().LiteralIsTrue(lit)) continue;
        if (trail->Assignment().LiteralIsFalse(lit)) {
          sat_solver->NotifyThatModelIsUnsat();
          return false;
        }
        new_bounds_have_been_imported = true;
        trail->EnqueueWithUnitReason(lit);
        continue;
      }

      // Deal with integer.
      if (!mapping->IsInteger(model_var)) continue;
      const IntegerVariable var = mapping->Integer(model_var);
      const IntegerValue new_lb(new_lower_bounds[i]);
      const IntegerValue new_ub(new_upper_bounds[i]);
      const IntegerValue old_lb = integer_trail->LowerBound(var);
      const IntegerValue old_ub = integer_trail->UpperBound(var);
      const bool changed_lb = new_lb > old_lb;
      const bool changed_ub = new_ub < old_ub;
      if (!changed_lb && !changed_ub) continue;

      new_bounds_have_been_imported = true;
      if (VLOG_IS_ON(3)) {
        const IntegerVariableProto& var_proto =
            model_proto.variables(model_var);
        const std::string& var_name =
            var_proto.name().empty()
                ? absl::StrCat("anonymous_var(", model_var, ")")
                : var_proto.name();
        LOG(INFO) << "  '" << name << "' imports new bounds for " << var_name
                  << ": from [" << old_lb << ", " << old_ub << "] to ["
                  << new_lb << ", " << new_ub << "]";
      }

      if (changed_lb &&
          !integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(var, new_lb),
                                  {}, {})) {
        return false;
      }
      if (changed_ub &&
          !integer_trail->Enqueue(IntegerLiteral::LowerOrEqual(var, new_ub), {},
                                  {})) {
        return false;
      }
    }
    if (new_bounds_have_been_imported && !sat_solver->FinishPropagation()) {
      return false;
    }
    return true;
  };
  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_level_zero_bounds);
}

// Registers a callback that will report improving objective best bound.
// It will be called each time new objective bound are propagated at level zero.
void RegisterObjectiveBestBoundExport(
    IntegerVariable objective_var,
    SharedResponseManager* shared_response_manager, Model* model) {
  auto* integer_trail = model->Get<IntegerTrail>();
  const auto broadcast_objective_lower_bound =
      [objective_var, integer_trail, shared_response_manager, model,
       best_obj_lb =
           kMinIntegerValue](const std::vector<IntegerVariable>&) mutable {
        const IntegerValue objective_lb =
            integer_trail->LevelZeroLowerBound(objective_var);
        if (objective_lb > best_obj_lb) {
          best_obj_lb = objective_lb;
          shared_response_manager->UpdateInnerObjectiveBounds(
              model->Name(), objective_lb,
              integer_trail->LevelZeroUpperBound(objective_var));
          // If we are not in interleave_search we synchronize right away.
          if (!model->Get<SatParameters>()->interleave_search()) {
            shared_response_manager->Synchronize();
          }
        }
      };
  model->GetOrCreate<GenericLiteralWatcher>()
      ->RegisterLevelZeroModifiedVariablesCallback(
          broadcast_objective_lower_bound);
}

// Registers a callback to import new objective bounds. It will be called each
// time the search main loop is back to level zero. Note that it the presence of
// assumptions, this will not happen until the set of assumptions is changed.
void RegisterObjectiveBoundsImport(
    SharedResponseManager* shared_response_manager, Model* model) {
  auto* solver = model->GetOrCreate<SatSolver>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* objective = model->GetOrCreate<ObjectiveDefinition>();
  const std::string name = model->Name();
  const auto import_objective_bounds = [name, solver, integer_trail, objective,
                                        shared_response_manager]() {
    if (solver->AssumptionLevel() != 0) return true;
    bool propagate = false;

    const IntegerValue external_lb =
        shared_response_manager->SynchronizedInnerObjectiveLowerBound();
    const IntegerValue current_lb =
        integer_trail->LowerBound(objective->objective_var);
    if (external_lb > current_lb) {
      if (!integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(
                                      objective->objective_var, external_lb),
                                  {}, {})) {
        return false;
      }
      propagate = true;
    }

    const IntegerValue external_ub =
        shared_response_manager->SynchronizedInnerObjectiveUpperBound();
    const IntegerValue current_ub =
        integer_trail->UpperBound(objective->objective_var);
    if (external_ub < current_ub) {
      if (!integer_trail->Enqueue(IntegerLiteral::LowerOrEqual(
                                      objective->objective_var, external_ub),
                                  {}, {})) {
        return false;
      }
      propagate = true;
    }

    if (!propagate) return true;

    VLOG(3) << "'" << name << "' imports objective bounds: external ["
            << objective->ScaleIntegerObjective(external_lb) << ", "
            << objective->ScaleIntegerObjective(external_ub) << "], current ["
            << objective->ScaleIntegerObjective(current_lb) << ", "
            << objective->ScaleIntegerObjective(current_ub) << "]";

    return solver->FinishPropagation();
  };

  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_objective_bounds);
}

// Registers a callback that will export binary clauses discovered during
// search.
void RegisterClausesExport(int id, SharedClausesManager* shared_clauses_manager,
                           Model* model) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const auto& share_binary_clause = [mapping, id, shared_clauses_manager](
                                        Literal l1, Literal l2) {
    const int var1 =
        mapping->GetProtoVariableFromBooleanVariable(l1.Variable());
    if (var1 == -1) return;
    const int var2 =
        mapping->GetProtoVariableFromBooleanVariable(l2.Variable());
    if (var2 == -1) return;
    const int lit1 = l1.IsPositive() ? var1 : NegatedRef(var1);
    const int lit2 = l2.IsPositive() ? var2 : NegatedRef(var2);
    shared_clauses_manager->AddBinaryClause(id, lit1, lit2);
  };
  model->GetOrCreate<BinaryImplicationGraph>()->SetAdditionCallback(
      share_binary_clause);
}

// Registers a callback to import new clauses stored in the
// shared_clausess_manager. These clauses are imported at level 0 of the search
// in the linear scan minimize function.
// it returns the id of the worker in the shared clause manager.
//
// TODO(user): Can we import them in the core worker ?
int RegisterClausesLevelZeroImport(int id,
                                   SharedClausesManager* shared_clauses_manager,
                                   Model* model) {
  CHECK(shared_clauses_manager != nullptr);
  CpModelMapping* const mapping = model->GetOrCreate<CpModelMapping>();
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* implications = model->GetOrCreate<BinaryImplicationGraph>();
  const auto& import_level_zero_clauses = [shared_clauses_manager, id, mapping,
                                           sat_solver, implications]() {
    std::vector<std::pair<int, int>> new_binary_clauses;
    shared_clauses_manager->GetUnseenBinaryClauses(id, &new_binary_clauses);
    implications->EnableSharing(false);
    for (const auto& [ref1, ref2] : new_binary_clauses) {
      const Literal l1 = mapping->Literal(ref1);
      const Literal l2 = mapping->Literal(ref2);
      if (!sat_solver->AddBinaryClause(l1, l2)) {
        return false;
      }
    }
    implications->EnableSharing(true);
    return true;
  };
  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      import_level_zero_clauses);
  return id;
}

void LoadBaseModel(const CpModelProto& model_proto, Model* model) {
  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  CHECK(shared_response_manager != nullptr);
  auto* sat_solver = model->GetOrCreate<SatSolver>();

  // Simple function for the few places where we do "return unsat()".
  const auto unsat = [shared_response_manager, sat_solver, model] {
    sat_solver->NotifyThatModelIsUnsat();
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        absl::StrCat(model->Name(), " [loading]"));
  };

  // We will add them all at once after model_proto is loaded.
  model->GetOrCreate<IntegerEncoder>()->DisableImplicationBetweenLiteral();

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  const bool view_all_booleans_as_integers =
      (parameters.linearization_level() >= 2) ||
      (parameters.search_branching() == SatParameters::FIXED_SEARCH &&
       model_proto.search_strategy().empty()) ||
      parameters.optimize_with_max_hs();
  LoadVariables(model_proto, view_all_booleans_as_integers, model);
  DetectOptionalVariables(model_proto, model);

  // TODO(user): The core algo and symmetries seems to be problematic in some
  // cases. See for instance: neos-691058.mps.gz. This is probably because as
  // we modify the model, our symmetry might be wrong? investigate.
  //
  // TODO(user): More generally, we cannot load the symmetry if we create
  // new Booleans and constraints that link them to some Booleans of the model.
  // Creating Booleans related to integer variable is fine since we only deal
  // with Boolean only symmetry here. It is why we disable this when we have
  // linear relaxation as some of them create new constraints.
  if (!parameters.optimize_with_core() && parameters.symmetry_level() > 1 &&
      !parameters.enumerate_all_solutions() &&
      parameters.linearization_level() == 0) {
    LoadBooleanSymmetries(model_proto, model);
  }

  ExtractEncoding(model_proto, model);
  PropagateEncodingFromEquivalenceRelations(model_proto, model);

  // Check the model is still feasible before continuing.
  if (sat_solver->ModelIsUnsat()) return unsat();

  // Fully encode variables as needed by the search strategy.
  AddFullEncodingFromSearchBranching(model_proto, model);
  if (sat_solver->ModelIsUnsat()) return unsat();

  // Reserve space for the precedence relations.
  model->GetOrCreate<PrecedenceRelations>()->Resize(
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value());

  // Load the constraints.
  int num_ignored_constraints = 0;
  absl::flat_hash_set<ConstraintProto::ConstraintCase> unsupported_types;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) {
      ++num_ignored_constraints;
      continue;
    }

    if (!LoadConstraint(ct, model)) {
      unsupported_types.insert(ct.constraint_case());
      continue;
    }

    // We propagate after each new Boolean constraint but not the integer
    // ones. So we call FinishPropagation() manually here.
    //
    // Note that we only do that in debug mode as this can be really slow on
    // certain types of problems with millions of constraints.
    if (DEBUG_MODE) {
      if (sat_solver->FinishPropagation()) {
        Trail* trail = model->GetOrCreate<Trail>();
        const int old_num_fixed = trail->Index();
        if (trail->Index() > old_num_fixed) {
          VLOG(3) << "Constraint fixed " << trail->Index() - old_num_fixed
                  << " Boolean variable(s): " << ProtobufDebugString(ct);
        }
      }
    }
    if (sat_solver->ModelIsUnsat()) {
      VLOG(2) << "UNSAT during extraction (after adding '"
              << ConstraintCaseName(ct.constraint_case()) << "'). "
              << ProtobufDebugString(ct);
      return unsat();
    }
  }
  if (num_ignored_constraints > 0) {
    VLOG(3) << num_ignored_constraints << " constraints were skipped.";
  }
  if (!unsupported_types.empty()) {
    VLOG(1) << "There is unsupported constraints types in this model: ";
    std::vector<absl::string_view> names;
    for (const ConstraintProto::ConstraintCase type : unsupported_types) {
      names.push_back(ConstraintCaseName(type));
    }
    std::sort(names.begin(), names.end());
    for (const absl::string_view name : names) {
      VLOG(1) << " - " << name;
    }
    return unsat();
  }

  model->GetOrCreate<IntegerEncoder>()
      ->AddAllImplicationsBetweenAssociatedLiterals();
  if (!sat_solver->FinishPropagation()) return unsat();

  model->GetOrCreate<ProductDetector>()->ProcessImplicationGraph(
      model->GetOrCreate<BinaryImplicationGraph>());
  model->GetOrCreate<PrecedenceRelations>()->Build();
}

void LoadFeasibilityPump(const CpModelProto& model_proto, Model* model) {
  LoadBaseModel(model_proto, model);

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  if (parameters.linearization_level() == 0) return;

  // Add linear constraints to Feasibility Pump.
  const LinearRelaxation relaxation =
      ComputeLinearRelaxation(model_proto, model);
  if (model->GetOrCreate<SatSolver>()->ModelIsUnsat()) return;

  const int num_lp_constraints =
      static_cast<int>(relaxation.linear_constraints.size());
  if (num_lp_constraints == 0) return;
  auto* feasibility_pump = model->GetOrCreate<FeasibilityPump>();
  for (int i = 0; i < num_lp_constraints; i++) {
    feasibility_pump->AddLinearConstraint(relaxation.linear_constraints[i]);
  }

  if (model_proto.has_objective()) {
    for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
      const IntegerVariable var =
          mapping->Integer(model_proto.objective().vars(i));
      const int64_t coeff = model_proto.objective().coeffs(i);
      feasibility_pump->SetObjectiveCoefficient(var, IntegerValue(coeff));
    }
  }
}

// Loads a CpModelProto inside the given model.
// This should only be called once on a given 'Model' class.
//
// TODO(user): move to cp_model_loader.h/.cc
void LoadCpModel(const CpModelProto& model_proto, Model* model) {
  LoadBaseModel(model_proto, model);

  // We want to load the debug solution before the initial propag.
  // But at this point the objective is not loaded yet, so we will not have
  // a value for the objective integer variable, so we do it again later.
  InitializeDebugSolution(model_proto, model);

  // Simple function for the few places where we do "return unsat()".
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  const auto unsat = [shared_response_manager, sat_solver, model] {
    sat_solver->NotifyThatModelIsUnsat();
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        absl::StrCat(model->Name(), " [loading]"));
  };

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());

  // Auto detect "at least one of" constraints in the PrecedencesPropagator.
  // Note that we do that before we finish loading the problem (objective and
  // LP relaxation), because propagation will be faster at this point and it
  // should be enough for the purpose of this auto-detection.
  if (parameters.auto_detect_greater_than_at_least_one_of()) {
    model->GetOrCreate<GreaterThanAtLeastOneOfDetector>()
        ->AddGreaterThanAtLeastOneOfConstraints(model);
    if (!sat_solver->FinishPropagation()) return unsat();
  }

  // Note that this is already done in the presolve, but it is important to redo
  // it here to collect literal => integer >= bound constraints that are used in
  // many places. Without it, we don't detect them if they depends on long chain
  // of implications.
  //
  // TODO(user): We don't have a good deterministic time on all constraints,
  // so this might take more time than wanted.
  if (parameters.cp_model_probing_level() > 1) {
    Prober* prober = model->GetOrCreate<Prober>();
    prober->ProbeBooleanVariables(/*deterministic_time_limit=*/1.0);
    if (!model->GetOrCreate<BinaryImplicationGraph>()
             ->ComputeTransitiveReduction()) {
      return unsat();
    }
  }
  if (sat_solver->ModelIsUnsat()) return unsat();

  // Note that it is important to do that after the probing.
  ExtractElementEncoding(model_proto, model);

  // Compute decomposed energies on demands helper.
  IntervalsRepository* repository = model->Mutable<IntervalsRepository>();
  if (repository != nullptr) {
    repository->InitAllDecomposedEnergies();
  }

  // We need to know beforehand if the objective var can just be >= terms or
  // needs to be == terms.
  bool objective_need_to_be_tight = false;
  if (model_proto.has_objective() &&
      !model_proto.objective().domain().empty()) {
    int64_t min_value = 0;
    int64_t max_value = 0;
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    const CpObjectiveProto& obj = model_proto.objective();
    for (int i = 0; i < obj.vars_size(); ++i) {
      const int64_t coeff = obj.coeffs(i);
      const IntegerVariable var = mapping->Integer(obj.vars(i));
      if (coeff > 0) {
        min_value += coeff * integer_trail->LowerBound(var).value();
        max_value += coeff * integer_trail->UpperBound(var).value();
      } else {
        min_value += coeff * integer_trail->UpperBound(var).value();
        max_value += coeff * integer_trail->LowerBound(var).value();
      }
    }
    const Domain user_domain = ReadDomainFromProto(model_proto.objective());
    const Domain automatic_domain = Domain(min_value, max_value);
    objective_need_to_be_tight = !automatic_domain.IsIncludedIn(user_domain);
  }

  // Create an objective variable and its associated linear constraint if
  // needed.
  IntegerVariable objective_var = kNoIntegerVariable;
  if (parameters.linearization_level() > 0) {
    // Linearize some part of the problem and register LP constraint(s).
    objective_var =
        AddLPConstraints(objective_need_to_be_tight, model_proto, model);
    if (sat_solver->ModelIsUnsat()) return unsat();
  } else if (model_proto.has_objective()) {
    const CpObjectiveProto& obj = model_proto.objective();
    std::vector<std::pair<IntegerVariable, int64_t>> terms;
    terms.reserve(obj.vars_size());
    for (int i = 0; i < obj.vars_size(); ++i) {
      terms.push_back(
          std::make_pair(mapping->Integer(obj.vars(i)), obj.coeffs(i)));
    }
    if (parameters.optimize_with_core() && !objective_need_to_be_tight) {
      objective_var = GetOrCreateVariableWithTightBound(terms, model);
    } else {
      objective_var = GetOrCreateVariableLinkedToSumOf(
          terms, objective_need_to_be_tight, model);
    }
  }

  // Create the objective definition inside the Model so that it can be accessed
  // by the heuristics than needs it.
  if (objective_var != kNoIntegerVariable) {
    const CpObjectiveProto& objective_proto = model_proto.objective();
    auto* objective_definition = model->GetOrCreate<ObjectiveDefinition>();

    objective_definition->scaling_factor = objective_proto.scaling_factor();
    if (objective_definition->scaling_factor == 0.0) {
      objective_definition->scaling_factor = 1.0;
    }
    objective_definition->offset = objective_proto.offset();
    objective_definition->objective_var = objective_var;

    const int size = objective_proto.vars_size();
    objective_definition->vars.resize(size);
    objective_definition->coeffs.resize(size);
    for (int i = 0; i < objective_proto.vars_size(); ++i) {
      // Note that if there is no mapping, then the variable will be
      // kNoIntegerVariable.
      objective_definition->vars[i] = mapping->Integer(objective_proto.vars(i));
      objective_definition->coeffs[i] = IntegerValue(objective_proto.coeffs(i));

      // Fill the objective heuristics data.
      const int ref = objective_proto.vars(i);
      if (mapping->IsInteger(ref)) {
        const IntegerVariable var = mapping->Integer(objective_proto.vars(i));
        objective_definition->objective_impacting_variables.insert(
            objective_proto.coeffs(i) > 0 ? var : NegationOf(var));
      }
    }

    // Register an objective special propagator.
    model->TakeOwnership(
        new LevelZeroEquality(objective_var, objective_definition->vars,
                              objective_definition->coeffs, model));
  }

  // Intersect the objective domain with the given one if any.
  if (!model_proto.objective().domain().empty()) {
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    const Domain user_domain = ReadDomainFromProto(model_proto.objective());
    const Domain automatic_domain =
        integer_trail->InitialVariableDomain(objective_var);
    VLOG(3) << "Objective offset:" << model_proto.objective().offset()
            << " scaling_factor:" << model_proto.objective().scaling_factor();
    VLOG(3) << "Automatic internal objective domain: " << automatic_domain;
    VLOG(3) << "User specified internal objective domain: " << user_domain;
    CHECK_NE(objective_var, kNoIntegerVariable);
    if (!integer_trail->UpdateInitialDomain(objective_var, user_domain)) {
      VLOG(2) << "UNSAT due to the objective domain.";
      return unsat();
    }
  }

  // Note that we do one last propagation at level zero once all the
  // constraints were added.
  SOLVER_LOG(model->GetOrCreate<SolverLogger>(),
             "Initial num_bool: ", sat_solver->NumVariables());
  if (!sat_solver->FinishPropagation()) return unsat();

  if (model_proto.has_objective()) {
    // Report the initial objective variable bounds.
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    shared_response_manager->UpdateInnerObjectiveBounds(
        absl::StrCat(model->Name(), " (initial_propagation)"),
        integer_trail->LowerBound(objective_var),
        integer_trail->UpperBound(objective_var));

    // Watch improved objective best bounds.
    RegisterObjectiveBestBoundExport(objective_var, shared_response_manager,
                                     model);

    // Import objective bounds.
    // TODO(user): Support objective bounds import in LNS and Core based
    // search.
    if (model->GetOrCreate<SatParameters>()->share_objective_bounds()) {
      RegisterObjectiveBoundsImport(shared_response_manager, model);
    }
  }

  // Initialize the search strategies.
  auto* search_heuristics = model->GetOrCreate<SearchHeuristics>();
  search_heuristics->user_search =
      ConstructUserSearchStrategy(model_proto, model);
  search_heuristics->heuristic_search =
      ConstructHeuristicSearchStrategy(model_proto, model);
  search_heuristics->integer_completion_search =
      ConstructIntegerCompletionSearchStrategy(mapping->GetVariableMapping(),
                                               objective_var, model);
  search_heuristics->fixed_search = ConstructFixedSearchStrategy(
      search_heuristics->user_search, search_heuristics->heuristic_search,
      search_heuristics->integer_completion_search);
  if (VLOG_IS_ON(3)) {
    search_heuristics->fixed_search =
        InstrumentSearchStrategy(model_proto, mapping->GetVariableMapping(),
                                 search_heuristics->fixed_search, model);
  }
  search_heuristics->hint_search =
      ConstructHintSearchStrategy(model_proto, mapping, model);

  // Create the CoreBasedOptimizer class if needed.
  if (parameters.optimize_with_core()) {
    // TODO(user): Remove code duplication with the solution_observer in
    // SolveLoadedCpModel().
    const auto solution_observer = [&model_proto, model,
                                    shared_response_manager,
                                    best_obj_ub = kMaxIntegerValue]() mutable {
      const std::vector<int64_t> solution =
          GetSolutionValues(model_proto, *model);
      const IntegerValue obj_ub =
          ComputeInnerObjective(model_proto.objective(), solution);
      if (obj_ub < best_obj_ub) {
        best_obj_ub = obj_ub;
        shared_response_manager->NewSolution(solution, model->Name(), model);
      }
    };

    const auto& objective = *model->GetOrCreate<ObjectiveDefinition>();
    if (parameters.optimize_with_max_hs()) {
      HittingSetOptimizer* max_hs = new HittingSetOptimizer(
          model_proto, objective, solution_observer, model);
      model->Register<HittingSetOptimizer>(max_hs);
      model->TakeOwnership(max_hs);
    } else {
      CoreBasedOptimizer* core =
          new CoreBasedOptimizer(objective_var, objective.vars,
                                 objective.coeffs, solution_observer, model);
      model->Register<CoreBasedOptimizer>(core);
      model->TakeOwnership(core);
    }
  }

  InitializeDebugSolution(model_proto, model);
}

// Solves an already loaded cp_model_proto.
// The final CpSolverResponse must be read from the shared_response_manager.
//
// TODO(user): This should be transformed so that it can be called many times
// and resume from the last search state as if it wasn't interrupted. That would
// allow use to easily interleave different heuristics in the same thread.
void SolveLoadedCpModel(const CpModelProto& model_proto, Model* model) {
  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  if (shared_response_manager->ProblemIsSolved()) return;

  const SatParameters& parameters = *model->GetOrCreate<SatParameters>();
  if (parameters.stop_after_root_propagation()) return;

  auto solution_observer = [&model_proto, model, shared_response_manager,
                            best_obj_ub = kMaxIntegerValue]() mutable {
    const std::vector<int64_t> solution =
        GetSolutionValues(model_proto, *model);
    if (model_proto.has_objective()) {
      const IntegerValue obj_ub =
          ComputeInnerObjective(model_proto.objective(), solution);
      if (obj_ub < best_obj_ub) {
        best_obj_ub = obj_ub;
        shared_response_manager->NewSolution(solution, model->Name(), model);
      }
    } else {
      shared_response_manager->NewSolution(solution, model->Name(), model);
    }
  };

  // Make sure we are not at a positive level.
  if (!model->GetOrCreate<SatSolver>()->ResetToLevelZero()) {
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        model->Name());
    return;
  }

  // Reconfigure search heuristic if it was changed.
  ConfigureSearchHeuristics(model);

  const auto& mapping = *model->GetOrCreate<CpModelMapping>();
  SatSolver::Status status;

  if (parameters.use_probing_search()) {
    ContinuousProber prober(model_proto, model);
    while (true) {
      status = prober.Probe();
      if (status == SatSolver::INFEASIBLE) {
        shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
            model->Name());
        break;
      }
      if (status == SatSolver::FEASIBLE) {
        solution_observer();
      } else {
        break;
      }
    }
  } else if (!model_proto.has_objective()) {
    while (true) {
      if (parameters.use_shared_tree_search()) {
        auto* subtree_worker = model->GetOrCreate<SharedTreeWorker>();
        status = subtree_worker->Search(solution_observer);
      } else {
        status = ResetAndSolveIntegerProblem(
            mapping.Literals(model_proto.assumptions()), model);
      }
      if (status != SatSolver::Status::FEASIBLE) break;
      solution_observer();
      if (!parameters.enumerate_all_solutions()) break;
      model->Add(ExcludeCurrentSolutionAndBacktrack());
    }
    if (status == SatSolver::INFEASIBLE) {
      shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
          model->Name());
    }
    if (status == SatSolver::ASSUMPTIONS_UNSAT) {
      shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
          model->Name());

      // Extract a good subset of assumptions and add it to the response.
      auto* time_limit = model->GetOrCreate<TimeLimit>();
      auto* sat_solver = model->GetOrCreate<SatSolver>();
      std::vector<Literal> core = sat_solver->GetLastIncompatibleDecisions();
      MinimizeCoreWithPropagation(time_limit, sat_solver, &core);
      std::vector<int> core_in_proto_format;
      for (const Literal l : core) {
        core_in_proto_format.push_back(
            mapping.GetProtoVariableFromBooleanVariable(l.Variable()));
        if (!l.IsPositive()) {
          core_in_proto_format.back() = NegatedRef(core_in_proto_format.back());
        }
      }
      shared_response_manager->AddUnsatCore(core_in_proto_format);
    }
  } else {
    // Optimization problem.
    const auto& objective = *model->GetOrCreate<ObjectiveDefinition>();
    const IntegerVariable objective_var = objective.objective_var;
    CHECK_NE(objective_var, kNoIntegerVariable);

    if (parameters.optimize_with_lb_tree_search()) {
      auto* search = model->GetOrCreate<LbTreeSearch>();
      status = search->Search(solution_observer);
    } else if (parameters.optimize_with_core()) {
      // TODO(user): This doesn't work with splitting in chunk for now. It
      // shouldn't be too hard to fix.
      if (parameters.optimize_with_max_hs()) {
        status = model->Mutable<HittingSetOptimizer>()->Optimize();
      } else {
        status = model->Mutable<CoreBasedOptimizer>()->Optimize();
      }
    } else if (parameters.use_shared_tree_search()) {
      auto* subtree_worker = model->GetOrCreate<SharedTreeWorker>();
      status = subtree_worker->Search(solution_observer);
    } else {
      // TODO(user): This parameter breaks the splitting in chunk of a Solve().
      // It should probably be moved into another SubSolver altogether.
      if (parameters.binary_search_num_conflicts() >= 0) {
        RestrictObjectiveDomainWithBinarySearch(objective_var,
                                                solution_observer, model);
      }
      status = MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          objective_var, solution_observer, model);
    }

    // The search is done in both case.
    //
    // TODO(user): Remove the weird translation INFEASIBLE->FEASIBLE in the
    // function above?
    if (status == SatSolver::INFEASIBLE || status == SatSolver::FEASIBLE) {
      shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
          model->Name());
    }
  }
}

// Try to find a solution by following the hint and using a low conflict limit.
// The CpModelProto must already be loaded in the Model.
void QuickSolveWithHint(const CpModelProto& model_proto, Model* model) {
  if (!model_proto.has_solution_hint()) return;

  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  if (shared_response_manager->ProblemIsSolved()) return;

  // Temporarily change the parameters.
  auto* parameters = model->GetOrCreate<SatParameters>();

  // If the model was loaded with "optimize_with_core" then the objective
  // variable is not linked to its linear expression. Because of that, we can
  // return a solution that does not satisfy the objective domain.
  //
  // TODO(user): This is fixable, but then do we need the hint when optimizing
  // with core?
  if (parameters->optimize_with_core()) return;

  const SatParameters saved_params = *parameters;
  parameters->set_max_number_of_conflicts(parameters->hint_conflict_limit());
  parameters->set_search_branching(SatParameters::HINT_SEARCH);
  parameters->set_optimize_with_core(false);
  auto cleanup = ::absl::MakeCleanup(
      [parameters, saved_params]() { *parameters = saved_params; });

  // Solve decision problem.
  ConfigureSearchHeuristics(model);
  const auto& mapping = *model->GetOrCreate<CpModelMapping>();
  const SatSolver::Status status = ResetAndSolveIntegerProblem(
      mapping.Literals(model_proto.assumptions()), model);

  const std::string& solution_info = model->Name();
  if (status == SatSolver::Status::FEASIBLE) {
    const std::vector<int64_t> solution =
        GetSolutionValues(model_proto, *model);
    shared_response_manager->NewSolution(
        solution, absl::StrCat(solution_info, " [hint]"), model);

    if (!model_proto.has_objective()) {
      if (parameters->enumerate_all_solutions()) {
        model->Add(ExcludeCurrentSolutionAndBacktrack());
      }
    } else {
      // Restrict the objective.
      const IntegerVariable objective_var =
          model->GetOrCreate<ObjectiveDefinition>()->objective_var;
      model->GetOrCreate<SatSolver>()->Backtrack(0);
      IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
      if (!integer_trail->Enqueue(
              IntegerLiteral::LowerOrEqual(
                  objective_var,
                  shared_response_manager->GetInnerObjectiveUpperBound()),
              {}, {})) {
        shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
            absl::StrCat(solution_info, " [hint]"));
      }
    }
    return;
  }

  // This code is here to debug bad presolve during LNS that corrupt the hint.
  // Note that sometime the deterministic limit is hit before the hint can be
  // completed, so we don't report that has an error.
  //
  // Tricky: We can only test that if we don't already have a feasible solution
  // like we do if the hint is complete.
  if (parameters->debug_crash_on_bad_hint() &&
      shared_response_manager->SolutionsRepository().NumSolutions() == 0 &&
      !model->GetOrCreate<TimeLimit>()->LimitReached() &&
      status != SatSolver::Status::FEASIBLE) {
    LOG(FATAL) << "QuickSolveWithHint() didn't find a feasible solution."
               << " The model name is '" << model_proto.name() << "'."
               << " Status: " << status << ".";
  }

  if (status == SatSolver::INFEASIBLE) {
    shared_response_manager->NotifyThatImprovingProblemIsInfeasible(
        absl::StrCat(solution_info, " [hint]"));
    return;
  }
}

// Solve a model with a different objective consisting of minimizing the L1
// distance with the provided hint. Note that this method creates an in-memory
// copy of the model and loads a local Model object from the copied model.
void MinimizeL1DistanceWithHint(const CpModelProto& model_proto, Model* model) {
  Model local_model;

  // Forward some shared class.
  local_model.Register<ModelSharedTimeLimit>(
      model->GetOrCreate<ModelSharedTimeLimit>());
  local_model.Register<WallTimer>(model->GetOrCreate<WallTimer>());

  if (!model_proto.has_solution_hint()) return;

  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  if (shared_response_manager->ProblemIsSolved()) return;

  auto* parameters = local_model.GetOrCreate<SatParameters>();
  // TODO(user): As of now the repair hint doesn't support when
  // enumerate_all_solutions is set since the solution is created on a different
  // model.
  if (parameters->enumerate_all_solutions()) return;

  // Change the parameters.
  const SatParameters saved_params = *model->GetOrCreate<SatParameters>();
  *parameters = saved_params;
  parameters->set_max_number_of_conflicts(parameters->hint_conflict_limit());
  parameters->set_optimize_with_core(false);

  // Update the model to introduce penalties to go away from hinted values.
  CpModelProto updated_model_proto = model_proto;
  updated_model_proto.clear_objective();

  // TODO(user): For boolean variables we can avoid creating new variables.
  for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
    const int var = model_proto.solution_hint().vars(i);
    const int64_t value = model_proto.solution_hint().values(i);

    // Add a new var to represent the difference between var and value.
    const int new_var_index = updated_model_proto.variables_size();
    IntegerVariableProto* var_proto = updated_model_proto.add_variables();
    const int64_t min_domain = model_proto.variables(var).domain(0) - value;
    const int64_t max_domain =
        model_proto.variables(var).domain(
            model_proto.variables(var).domain_size() - 1) -
        value;
    var_proto->add_domain(min_domain);
    var_proto->add_domain(max_domain);

    // new_var = var - value.
    ConstraintProto* const linear_constraint_proto =
        updated_model_proto.add_constraints();
    LinearConstraintProto* linear = linear_constraint_proto->mutable_linear();
    linear->add_vars(new_var_index);
    linear->add_coeffs(1);
    linear->add_vars(var);
    linear->add_coeffs(-1);
    linear->add_domain(-value);
    linear->add_domain(-value);

    // abs_var = abs(new_var).
    const int abs_var_index = updated_model_proto.variables_size();
    IntegerVariableProto* abs_var_proto = updated_model_proto.add_variables();
    const int64_t abs_min_domain = 0;
    const int64_t abs_max_domain =
        std::max(std::abs(min_domain), std::abs(max_domain));
    abs_var_proto->add_domain(abs_min_domain);
    abs_var_proto->add_domain(abs_max_domain);
    auto* abs_ct = updated_model_proto.add_constraints()->mutable_lin_max();
    abs_ct->mutable_target()->add_vars(abs_var_index);
    abs_ct->mutable_target()->add_coeffs(1);
    LinearExpressionProto* left = abs_ct->add_exprs();
    left->add_vars(new_var_index);
    left->add_coeffs(1);
    LinearExpressionProto* right = abs_ct->add_exprs();
    right->add_vars(new_var_index);
    right->add_coeffs(-1);

    updated_model_proto.mutable_objective()->add_vars(abs_var_index);
    updated_model_proto.mutable_objective()->add_coeffs(1);
  }

  auto* local_response_manager =
      local_model.GetOrCreate<SharedResponseManager>();
  local_response_manager->InitializeObjective(updated_model_proto);

  // Solve optimization problem.
  LoadCpModel(updated_model_proto, &local_model);

  ConfigureSearchHeuristics(&local_model);
  const auto& mapping = *local_model.GetOrCreate<CpModelMapping>();
  const SatSolver::Status status = ResetAndSolveIntegerProblem(
      mapping.Literals(updated_model_proto.assumptions()), &local_model);

  const std::string& solution_info = model->Name();
  if (status == SatSolver::Status::FEASIBLE) {
    const std::vector<int64_t> solution =
        GetSolutionValues(model_proto, local_model);
    if (DEBUG_MODE) {
      const std::vector<int64_t> updated_solution =
          GetSolutionValues(updated_model_proto, local_model);
      LOG(INFO) << "Found solution with repaired hint penalty = "
                << ComputeInnerObjective(updated_model_proto.objective(),
                                         updated_solution);
    }
    shared_response_manager->NewSolution(
        solution, absl::StrCat(solution_info, " [repaired]"), &local_model);
  }
}

// TODO(user): If this ever shows up in the profile, we could avoid copying
// the mapping_proto if we are careful about how we modify the variable domain
// before postsolving it. Note that 'num_variables_in_original_model' refers to
// the model before presolve.
void PostsolveResponseWithFullSolver(int num_variables_in_original_model,
                                     CpModelProto mapping_proto,
                                     const std::vector<int>& postsolve_mapping,
                                     std::vector<int64_t>* solution) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Fix the correct variable in the mapping_proto.
  for (int i = 0; i < solution->size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    var_proto->clear_domain();
    var_proto->add_domain((*solution)[i]);
    var_proto->add_domain((*solution)[i]);
  }

  // Postosolve parameters.
  // TODO(user): this problem is usually trivial, but we may still want to
  // impose a time limit or copy some of the parameters passed by the user.
  Model postsolve_model;
  postsolve_model.Register<WallTimer>(&wall_timer);
  {
    SatParameters& params = *postsolve_model.GetOrCreate<SatParameters>();
    params.set_linearization_level(0);
    params.set_cp_model_probing_level(0);
  }

  auto* response_manager = postsolve_model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(mapping_proto);

  LoadCpModel(mapping_proto, &postsolve_model);
  SolveLoadedCpModel(mapping_proto, &postsolve_model);
  const CpSolverResponse postsolve_response = response_manager->GetResponse();
  CHECK(postsolve_response.status() == CpSolverStatus::FEASIBLE ||
        postsolve_response.status() == CpSolverStatus::OPTIMAL)
      << CpSolverResponseStats(postsolve_response);

  // We only copy the solution from the postsolve_response to the response.
  CHECK_LE(num_variables_in_original_model,
           postsolve_response.solution().size());
  solution->assign(
      postsolve_response.solution().begin(),
      postsolve_response.solution().begin() + num_variables_in_original_model);
}

void PostsolveResponseWrapper(const SatParameters& params,
                              int num_variable_in_original_model,
                              const CpModelProto& mapping_proto,
                              const std::vector<int>& postsolve_mapping,
                              std::vector<int64_t>* solution) {
  if (params.debug_postsolve_with_full_solver()) {
    PostsolveResponseWithFullSolver(num_variable_in_original_model,
                                    mapping_proto, postsolve_mapping, solution);
  } else {
    PostsolveResponse(num_variable_in_original_model, mapping_proto,
                      postsolve_mapping, solution);
  }
}

#if !defined(__PORTABLE_PLATFORM__)

// Small wrapper containing all the shared classes between our subsolver
// threads. Note that all these classes can also be retrieved with something
// like global_model->GetOrCreate<Class>() but it is not thread-safe to do so.
//
// All the classes here should be thread-safe, or at least safe in the way they
// are accessed. For instance the model_proto will be kept constant for the
// whole duration of the solve.
struct SharedClasses {
  SharedClasses(const CpModelProto* proto, Model* global_model)
      : model_proto(proto),
        wall_timer(global_model->GetOrCreate<WallTimer>()),
        time_limit(global_model->GetOrCreate<ModelSharedTimeLimit>()),
        logger(global_model->GetOrCreate<SolverLogger>()),
        stats(global_model->GetOrCreate<SharedStatistics>()),
        response(global_model->GetOrCreate<SharedResponseManager>()),
        shared_tree_manager(global_model->GetOrCreate<SharedTreeManager>()) {}

  // These are never nullptr.
  const CpModelProto* const model_proto;
  WallTimer* const wall_timer;
  ModelSharedTimeLimit* const time_limit;
  SolverLogger* const logger;
  SharedStatistics* const stats;
  SharedResponseManager* const response;
  SharedTreeManager* const shared_tree_manager;

  // These can be nullptr depending on the options.
  std::unique_ptr<SharedBoundsManager> bounds;
  std::unique_ptr<SharedLPSolutionRepository> lp_solutions;
  std::unique_ptr<SharedIncompleteSolutionManager> incomplete_solutions;
  std::unique_ptr<SharedClausesManager> clauses;

  // For displaying summary at the end.
  SharedStatTables stat_tables;

  bool SearchIsDone() {
    if (response->ProblemIsSolved()) {
      // This is for cases where the time limit is checked more often.
      time_limit->Stop();
      return true;
    }
    if (time_limit->LimitReached()) return true;
    return false;
  }
};

// Encapsulate a full CP-SAT solve without presolve in the SubSolver API.
class FullProblemSolver : public SubSolver {
 public:
  FullProblemSolver(absl::string_view name,
                    const SatParameters& local_parameters, bool split_in_chunks,
                    SharedClasses* shared, bool stop_at_first_solution = false)
      : SubSolver(stop_at_first_solution ? absl::StrCat("fs_", name) : name,
                  stop_at_first_solution ? FIRST_SOLUTION : FULL_PROBLEM),
        shared_(shared),
        split_in_chunks_(split_in_chunks),
        stop_at_first_solution_(stop_at_first_solution),
        local_model_(SubSolver::name()) {
    // Setup the local model parameters and time limit.
    *(local_model_.GetOrCreate<SatParameters>()) = local_parameters;
    shared_->time_limit->UpdateLocalLimit(
        local_model_.GetOrCreate<TimeLimit>());

    if (stop_at_first_solution) {
      local_model_.GetOrCreate<TimeLimit>()
          ->RegisterSecondaryExternalBooleanAsLimit(
              shared_->response->first_solution_solvers_should_stop());
    }

    if (shared->response != nullptr) {
      local_model_.Register<SharedResponseManager>(shared->response);
    }

    if (shared->lp_solutions != nullptr) {
      local_model_.Register<SharedLPSolutionRepository>(
          shared->lp_solutions.get());
    }

    if (shared->incomplete_solutions != nullptr) {
      local_model_.Register<SharedIncompleteSolutionManager>(
          shared->incomplete_solutions.get());
    }

    if (shared->bounds != nullptr) {
      local_model_.Register<SharedBoundsManager>(shared->bounds.get());
    }

    if (shared->clauses != nullptr) {
      local_model_.Register<SharedClausesManager>(shared->clauses.get());
    }

    if (local_parameters.use_shared_tree_search()) {
      local_model_.Register<SharedTreeManager>(shared->shared_tree_manager);
    }

    // TODO(user): For now we do not count LNS statistics. We could easily
    // by registering the SharedStatistics class with LNS local model.
    local_model_.Register<SharedStatistics>(shared_->stats);
  }

  ~FullProblemSolver() override {
    CpSolverResponse response;
    FillSolveStatsInResponse(&local_model_, &response);
    shared_->response->AppendResponseToBeMerged(response);
    shared_->stat_tables.AddTimingStat(*this);
    shared_->stat_tables.AddLpStat(name(), &local_model_);
    shared_->stat_tables.AddSearchStat(name(), &local_model_);
    shared_->stat_tables.AddClausesStat(name(), &local_model_);
  }

  bool IsDone() override {
    return stop_at_first_solution_ &&
           shared_->response->first_solution_solvers_should_stop()->load();
  }

  bool TaskIsAvailable() override {
    if (IsDone()) return false;

    // Tricky: we don't want this in IsDone() otherwise the order of destruction
    // is unclear, and currently we always report the stats of the last
    // destroyed full solver (i.e. the first created with is the one with the
    // parameter provided by the user).
    if (shared_->SearchIsDone()) return false;

    absl::MutexLock mutex_lock(&mutex_);
    if (previous_task_is_completed_) {
      if (solving_first_chunk_) return true;
      if (split_in_chunks_) return true;
    }
    return false;
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) override {
    {
      absl::MutexLock mutex_lock(&mutex_);
      previous_task_is_completed_ = false;
    }
    return [this]() {
      if (solving_first_chunk_) {
        LoadCpModel(*shared_->model_proto, &local_model_);

        // Level zero variable bounds sharing. It is important to register
        // that after the probing that takes place in LoadCpModel() otherwise
        // we will have a mutex contention issue when all the thread probes
        // at the same time.
        if (shared_->bounds != nullptr) {
          RegisterVariableBoundsLevelZeroExport(
              *shared_->model_proto, shared_->bounds.get(), &local_model_);
          RegisterVariableBoundsLevelZeroImport(
              *shared_->model_proto, shared_->bounds.get(), &local_model_);
        }

        // Note that this is done after the loading, so we will never export
        // problem clauses.
        if (shared_->clauses != nullptr) {
          const int id = shared_->clauses->RegisterNewId();
          shared_->clauses->SetWorkerNameForId(id, local_model_.Name());

          RegisterClausesLevelZeroImport(id, shared_->clauses.get(),
                                         &local_model_);
          RegisterClausesExport(id, shared_->clauses.get(), &local_model_);
        }

        if (local_model_.GetOrCreate<SatParameters>()->repair_hint()) {
          MinimizeL1DistanceWithHint(*shared_->model_proto, &local_model_);
        } else {
          QuickSolveWithHint(*shared_->model_proto, &local_model_);
        }

        // No need for mutex since we only run one task at the time.
        solving_first_chunk_ = false;

        if (split_in_chunks_) {
          // Abort first chunk and allow to schedule the next.
          absl::MutexLock mutex_lock(&mutex_);
          previous_task_is_completed_ = true;
          return;
        }
      }

      auto* time_limit = local_model_.GetOrCreate<TimeLimit>();
      if (split_in_chunks_) {
        // Configure time limit for chunk solving. Note that we do not want
        // to do that for the hint search for now.
        auto* params = local_model_.GetOrCreate<SatParameters>();
        params->set_max_deterministic_time(1);
        time_limit->ResetLimitFromParameters(*params);
        shared_->time_limit->UpdateLocalLimit(time_limit);
      }

      const double saved_dtime = time_limit->GetElapsedDeterministicTime();
      SolveLoadedCpModel(*shared_->model_proto, &local_model_);

      absl::MutexLock mutex_lock(&mutex_);
      previous_task_is_completed_ = true;
      dtime_since_last_sync_ +=
          time_limit->GetElapsedDeterministicTime() - saved_dtime;
    };
  }

  // TODO(user): A few of the information sharing we do between threads does not
  // happen here (bound sharing, RINS neighborhood, objective). Fix that so we
  // can have a deterministic parallel mode.
  void Synchronize() override {
    absl::MutexLock mutex_lock(&mutex_);
    AddTaskDeterministicDuration(dtime_since_last_sync_);
    shared_->time_limit->AdvanceDeterministicTime(dtime_since_last_sync_);
    dtime_since_last_sync_ = 0.0;
  }

 private:
  SharedClasses* shared_;
  const bool split_in_chunks_;
  const bool stop_at_first_solution_;
  Model local_model_;

  // The first chunk is special. It is the one in which we load the model and
  // try to follow the hint.
  bool solving_first_chunk_ = true;

  absl::Mutex mutex_;
  double dtime_since_last_sync_ ABSL_GUARDED_BY(mutex_) = 0.0;
  bool previous_task_is_completed_ ABSL_GUARDED_BY(mutex_) = true;
};

class ObjectiveShavingSolver : public SubSolver {
 public:
  ObjectiveShavingSolver(const SatParameters& local_parameters,
                         NeighborhoodGeneratorHelper* helper,
                         SharedClasses* shared)
      : SubSolver(local_parameters.name(), FULL_PROBLEM),
        local_params_(local_parameters),
        helper_(helper),
        shared_(shared),
        local_proto_(*shared->model_proto) {}

  ~ObjectiveShavingSolver() override {
    shared_->stat_tables.AddTimingStat(*this);
  }

  bool TaskIsAvailable() override {
    if (shared_->SearchIsDone()) return false;

    // We only support one task at the time.
    absl::MutexLock mutex_lock(&mutex_);
    return !task_in_flight_;
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) override {
    {
      absl::MutexLock mutex_lock(&mutex_);
      stop_current_chunk_.store(false);
      task_in_flight_ = true;
      objective_lb_ = shared_->response->GetInnerObjectiveLowerBound();
    }
    return [this]() {
      if (ResetModel()) {
        SolveLoadedCpModel(local_proto_, local_repo_.get());
        const CpSolverResponse local_response =
            local_repo_->GetOrCreate<SharedResponseManager>()->GetResponse();

        if (local_response.status() == CpSolverStatus::OPTIMAL ||
            local_response.status() == CpSolverStatus::FEASIBLE) {
          std::vector<int64_t> solution_values(
              local_response.solution().begin(),
              local_response.solution().end());
          if (local_params_.cp_model_presolve()) {
            const int num_original_vars =
                shared_->model_proto->variables_size();
            PostsolveResponseWrapper(local_params_, num_original_vars,
                                     mapping_proto_, postsolve_mapping_,
                                     &solution_values);
          }
          shared_->response->NewSolution(solution_values, Info());
        } else if (local_response.status() == CpSolverStatus::INFEASIBLE) {
          absl::MutexLock mutex_lock(&mutex_);
          shared_->response->UpdateInnerObjectiveBounds(
              Info(), objective_lb_ + 1, kMaxIntegerValue);
        }
      }

      absl::MutexLock mutex_lock(&mutex_);
      task_in_flight_ = false;
      if (local_repo_ != nullptr) {
        const double dtime = local_repo_->GetOrCreate<TimeLimit>()
                                 ->GetElapsedDeterministicTime();
        AddTaskDeterministicDuration(dtime);
        shared_->time_limit->AdvanceDeterministicTime(dtime);
      }
    };
  }

  void Synchronize() override {
    absl::MutexLock mutex_lock(&mutex_);
    if (!task_in_flight_) return;

    // We are just waiting for the inner code to check the time limit or
    // to return nicely.
    if (stop_current_chunk_) return;

    // TODO(user): Also stop if we have enough newly fixed / improved root level
    // bounds so that we think it is worth represolving and restarting.
    if (shared_->SearchIsDone()) {
      stop_current_chunk_.store(true);
    }
    if (shared_->response->GetInnerObjectiveLowerBound() > objective_lb_) {
      stop_current_chunk_.store(true);
    }
  }

 private:
  std::string Info() {
    return absl::StrCat(name(), " (vars=", local_proto_.variables().size(),
                        " csts=", local_proto_.constraints().size(), ")");
  }

  bool ResetModel() {
    local_repo_ = std::make_unique<Model>(name());
    *local_repo_->GetOrCreate<SatParameters>() = local_params_;

    auto* time_limit = local_repo_->GetOrCreate<TimeLimit>();
    shared_->time_limit->UpdateLocalLimit(time_limit);
    time_limit->RegisterSecondaryExternalBooleanAsLimit(&stop_current_chunk_);

    // We copy the model.
    local_proto_ = *shared_->model_proto;
    *local_proto_.mutable_variables() =
        helper_->FullNeighborhood().delta.variables();

    // We replace the objective by a constraint, objective == lb.
    // TODO(user): We could use objective <= lb, it might be better or worse
    // depending on the model. It is also a bit tricker to make sure a feasible
    // solution is feasible.
    // We modify local_proto_ to a pure feasibility problem.
    // Not having the objective open up more presolve reduction.
    if (local_proto_.objective().vars().size() == 1 &&
        local_proto_.objective().coeffs(0) == 1) {
      auto* obj_var =
          local_proto_.mutable_variables(local_proto_.objective().vars(0));
      obj_var->clear_domain();
      absl::MutexLock mutex_lock(&mutex_);
      obj_var->add_domain(objective_lb_.value());
      obj_var->add_domain(objective_lb_.value());
    } else {
      auto* obj = local_proto_.add_constraints()->mutable_linear();
      *obj->mutable_vars() = local_proto_.objective().vars();
      *obj->mutable_coeffs() = local_proto_.objective().coeffs();
      absl::MutexLock mutex_lock(&mutex_);
      obj->add_domain(objective_lb_.value());
      obj->add_domain(objective_lb_.value());
    }

    // Clear the objective.
    local_proto_.clear_objective();

    // Presolve if asked.
    if (local_params_.cp_model_presolve()) {
      mapping_proto_.Clear();
      postsolve_mapping_.clear();
      auto context = std::make_unique<PresolveContext>(
          local_repo_.get(), &local_proto_, &mapping_proto_);
      const CpSolverStatus presolve_status =
          PresolveCpModel(context.get(), &postsolve_mapping_);
      if (presolve_status == CpSolverStatus::INFEASIBLE) {
        absl::MutexLock mutex_lock(&mutex_);
        shared_->response->UpdateInnerObjectiveBounds(Info(), objective_lb_ + 1,
                                                      kMaxIntegerValue);
        return false;
      }
    }

    // Tricky: If we aborted during the presolve above, some constraints might
    // be in a non-canonical form (like having duplicates, etc...) and it seem
    // not all our propagator code deal with that properly. So it is important
    // to abort right away here.
    //
    // We had a bug when the LoadCpModel() below was returning infeasible on
    // such non fully-presolved model.
    if (local_repo_->GetOrCreate<TimeLimit>()->LimitReached()) return false;

    LoadCpModel(local_proto_, local_repo_.get());
    return true;
  }

  // This is fixed at construction.
  SatParameters local_params_;
  NeighborhoodGeneratorHelper* helper_;
  SharedClasses* shared_;

  // Allow to control the local time limit in addition to a potential user
  // defined external Boolean.
  std::atomic<bool> stop_current_chunk_;

  // Local singleton repository and presolved local model.
  std::unique_ptr<Model> local_repo_;
  CpModelProto local_proto_;

  // For postsolving a feasible solution or improving objective lb.
  std::vector<int> postsolve_mapping_;
  CpModelProto mapping_proto_;

  absl::Mutex mutex_;
  IntegerValue objective_lb_ ABSL_GUARDED_BY(mutex_);
  bool task_in_flight_ ABSL_GUARDED_BY(mutex_) = false;
};

class FeasibilityPumpSolver : public SubSolver {
 public:
  FeasibilityPumpSolver(const SatParameters& local_parameters,
                        SharedClasses* shared)
      : SubSolver("feasibility_pump", INCOMPLETE),
        shared_(shared),
        local_model_(std::make_unique<Model>(name())) {
    // Setup the local model parameters and time limit.
    *(local_model_->GetOrCreate<SatParameters>()) = local_parameters;
    shared_->time_limit->UpdateLocalLimit(
        local_model_->GetOrCreate<TimeLimit>());

    if (shared->response != nullptr) {
      local_model_->Register<SharedResponseManager>(shared->response);
    }

    if (shared->lp_solutions != nullptr) {
      local_model_->Register<SharedLPSolutionRepository>(
          shared->lp_solutions.get());
    }

    if (shared->incomplete_solutions != nullptr) {
      local_model_->Register<SharedIncompleteSolutionManager>(
          shared->incomplete_solutions.get());
    }

    // Level zero variable bounds sharing.
    if (shared_->bounds != nullptr) {
      RegisterVariableBoundsLevelZeroImport(
          *shared_->model_proto, shared_->bounds.get(), local_model_.get());
    }
  }

  ~FeasibilityPumpSolver() override {
    shared_->stat_tables.AddTimingStat(*this);
  }

  bool TaskIsAvailable() override {
    if (shared_->SearchIsDone()) return false;
    absl::MutexLock mutex_lock(&mutex_);
    return previous_task_is_completed_;
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) override {
    {
      absl::MutexLock mutex_lock(&mutex_);
      previous_task_is_completed_ = false;
    }
    return [this]() {
      {
        absl::MutexLock mutex_lock(&mutex_);
        if (solving_first_chunk_) {
          LoadFeasibilityPump(*shared_->model_proto, local_model_.get());
          // No new task will be scheduled for this worker if there is no
          // linear relaxation.
          if (local_model_->Get<FeasibilityPump>() == nullptr) return;
          solving_first_chunk_ = false;
          // Abort first chunk and allow to schedule the next.
          previous_task_is_completed_ = true;
          return;
        }
      }

      auto* time_limit = local_model_->GetOrCreate<TimeLimit>();
      const double saved_dtime = time_limit->GetElapsedDeterministicTime();
      auto* feasibility_pump = local_model_->Mutable<FeasibilityPump>();
      if (!feasibility_pump->Solve()) {
        shared_->response->NotifyThatImprovingProblemIsInfeasible(name());
      }

      {
        absl::MutexLock mutex_lock(&mutex_);
        dtime_since_last_sync_ +=
            time_limit->GetElapsedDeterministicTime() - saved_dtime;
      }

      // Abort if the problem is solved.
      if (shared_->SearchIsDone()) {
        shared_->time_limit->Stop();
        return;
      }

      absl::MutexLock mutex_lock(&mutex_);
      previous_task_is_completed_ = true;
    };
  }

  void Synchronize() override {
    absl::MutexLock mutex_lock(&mutex_);
    AddTaskDeterministicDuration(dtime_since_last_sync_);
    shared_->time_limit->AdvanceDeterministicTime(dtime_since_last_sync_);
    dtime_since_last_sync_ = 0.0;
  }

 private:
  SharedClasses* shared_;
  std::unique_ptr<Model> local_model_;

  absl::Mutex mutex_;

  // The first chunk is special. It is the one in which we load the linear
  // constraints.
  bool solving_first_chunk_ ABSL_GUARDED_BY(mutex_) = true;

  double dtime_since_last_sync_ ABSL_GUARDED_BY(mutex_) = 0.0;
  bool previous_task_is_completed_ ABSL_GUARDED_BY(mutex_) = true;
};

// A Subsolver that generate LNS solve from a given neighborhood.
class LnsSolver : public SubSolver {
 public:
  LnsSolver(std::unique_ptr<NeighborhoodGenerator> generator,
            const SatParameters& lns_parameters,
            NeighborhoodGeneratorHelper* helper, SharedClasses* shared)
      : SubSolver(generator->name(), INCOMPLETE),
        generator_(std::move(generator)),
        helper_(helper),
        lns_parameters_(lns_parameters),
        shared_(shared) {}

  ~LnsSolver() override {
    shared_->stat_tables.AddTimingStat(*this);
    shared_->stat_tables.AddLnsStat(name(), *generator_);
  }

  bool TaskIsAvailable() override {
    if (shared_->SearchIsDone()) return false;
    return generator_->ReadyToGenerate();
  }

  std::function<void()> GenerateTask(int64_t task_id) override {
    return [task_id, this]() {
      if (shared_->SearchIsDone()) return;

      // Create a random number generator whose seed depends both on the task_id
      // and on the parameters_.random_seed() so that changing the later will
      // change the LNS behavior.
      const int32_t low = static_cast<int32_t>(task_id);
      const int32_t high = static_cast<int32_t>(task_id >> 32);
      std::seed_seq seed{low, high, lns_parameters_.random_seed()};
      random_engine_t random(seed);

      NeighborhoodGenerator::SolveData data;
      data.difficulty = generator_->difficulty();
      data.deterministic_limit = generator_->deterministic_limit();

      // Choose a base solution for this neighborhood.
      CpSolverResponse base_response;
      {
        const SharedSolutionRepository<int64_t>& repo =
            shared_->response->SolutionsRepository();
        if (repo.NumSolutions() > 0) {
          base_response.set_status(CpSolverStatus::FEASIBLE);
          const SharedSolutionRepository<int64_t>::Solution solution =
              repo.GetRandomBiasedSolution(random);
          for (const int64_t value : solution.variable_values) {
            base_response.add_solution(value);
          }

          // Note: We assume that the solution rank is the solution internal
          // objective.
          data.initial_best_objective = repo.GetSolution(0).rank;
          data.base_objective = solution.rank;
        } else {
          base_response.set_status(CpSolverStatus::UNKNOWN);

          // If we do not have a solution, we use the current objective upper
          // bound so that our code that compute an "objective" improvement
          // works.
          //
          // TODO(user): this is non-deterministic. Fix.
          data.initial_best_objective =
              shared_->response->GetInnerObjectiveUpperBound();
          data.base_objective = data.initial_best_objective;
        }
      }

      Neighborhood neighborhood =
          generator_->Generate(base_response, data.difficulty, random);

      if (!neighborhood.is_generated) return;

      SatParameters local_params(lns_parameters_);
      local_params.set_max_deterministic_time(data.deterministic_limit);

      // TODO(user): Tune these.
      // TODO(user): This could be a good candidate for bandits.
      const int64_t stall = generator_->num_consecutive_non_improving_calls();
      const int search_index = stall < 10 ? 0 : task_id % 2;
      absl::string_view search_info;
      switch (search_index) {
        case 0:
          local_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
          local_params.set_linearization_level(0);
          search_info = "auto_l0";
          break;
        default:
          local_params.set_search_branching(SatParameters::PORTFOLIO_SEARCH);
          local_params.set_search_random_variable_pool_size(5);
          search_info = "folio_rnd";
          break;
      }

      std::string source_info =
          neighborhood.source_info.empty() ? name() : neighborhood.source_info;
      const int64_t num_calls = std::max(int64_t{1}, generator_->num_calls());
      const double fully_solved_proportion =
          static_cast<double>(generator_->num_fully_solved_calls()) /
          static_cast<double>(num_calls);
      const std::string lns_info = absl::StrFormat(
          "%s (d=%0.2f s=%i t=%0.2f p=%0.2f stall=%d h=%s)", source_info,
          data.difficulty, task_id, data.deterministic_limit,
          fully_solved_proportion, stall, search_info);

      Model local_model(lns_info);
      *(local_model.GetOrCreate<SatParameters>()) = local_params;
      TimeLimit* local_time_limit = local_model.GetOrCreate<TimeLimit>();
      local_time_limit->ResetLimitFromParameters(local_params);
      shared_->time_limit->UpdateLocalLimit(local_time_limit);

      // Presolve and solve the LNS fragment.
      CpModelProto lns_fragment;
      CpModelProto mapping_proto;
      auto context = std::make_unique<PresolveContext>(
          &local_model, &lns_fragment, &mapping_proto);

      *lns_fragment.mutable_variables() = neighborhood.delta.variables();
      {
        ModelCopy copier(context.get());

        // Copy and simplify the constraints from the initial model.
        if (!copier.ImportAndSimplifyConstraints(helper_->ModelProto())) {
          return;
        }

        // Copy and simplify the constraints from the delta model.
        if (!neighborhood.delta.constraints().empty() &&
            !copier.ImportAndSimplifyConstraints(neighborhood.delta)) {
          return;
        }

        // This is not strictly needed, but useful for properly debugging an
        // infeasible LNS.
        context->WriteVariableDomainsToProto();
      }

      // Copy the rest of the model and overwrite the name.
      CopyEverythingExceptVariablesAndConstraintsFieldsIntoContext(
          helper_->ModelProto(), context.get());
      lns_fragment.set_name(absl::StrCat("lns_", task_id, "_", source_info));

      // Overwrite solution hinting.
      if (neighborhood.delta.has_solution_hint()) {
        *lns_fragment.mutable_solution_hint() =
            neighborhood.delta.solution_hint();
      }
      if (generator_->num_consecutive_non_improving_calls() > 10 &&
          absl::Bernoulli(random, 0.5)) {
        // If we seems to be stalling, lets try to solve without the hint in
        // order to diversify our solution pool. Otherwise non-improving
        // neighborhood will just return the base solution always.
        lns_fragment.clear_solution_hint();
      }
      if (neighborhood.is_simple &&
          neighborhood.num_relaxed_variables_in_objective == 0) {
        // If we didn't relax the objective, there can be no improving solution.
        // However, we might have some diversity if they are multiple feasible
        // solution.
        //
        // TODO(user): How can we teak the search to favor diversity.
        if (generator_->num_consecutive_non_improving_calls() > 10) {
          // We have been staling, try to find diverse solution?
          lns_fragment.clear_solution_hint();
        } else {
          // Just regenerate.
          // Note that we do not change the difficulty.
          return;
        }
      }

      CpModelProto debug_copy;
      if (absl::GetFlag(FLAGS_cp_model_dump_problematic_lns)) {
        // We need to make a copy because the presolve is destructive.
        // It is why we do not do that by default.
        debug_copy = lns_fragment;
      }

      if (absl::GetFlag(FLAGS_cp_model_dump_lns)) {
        // TODO(user): export the delta too if needed.
        const std::string lns_name =
            absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                         lns_fragment.name(), ".pb.txt");
        LOG(INFO) << "Dumping LNS model to '" << lns_name << "'.";
        CHECK(WriteModelProtoToFile(lns_fragment, lns_name));
      }

      std::vector<int> postsolve_mapping;
      const CpSolverStatus presolve_status =
          PresolveCpModel(context.get(), &postsolve_mapping);

      // Release the context.
      context.reset(nullptr);
      neighborhood.delta.Clear();

      // TODO(user): Depending on the problem, we should probably use the
      // parameters that work bests (core, linearization_level, etc...) or
      // maybe we can just randomize them like for the base solution used.
      auto* local_response_manager =
          local_model.GetOrCreate<SharedResponseManager>();
      local_response_manager->InitializeObjective(lns_fragment);
      local_response_manager->SetSynchronizationMode(true);

      CpSolverResponse local_response;
      if (presolve_status == CpSolverStatus::UNKNOWN) {
        LoadCpModel(lns_fragment, &local_model);
        QuickSolveWithHint(lns_fragment, &local_model);
        SolveLoadedCpModel(lns_fragment, &local_model);
        local_response = local_response_manager->GetResponse();
        // In case the LNS model is empty after presolve, the solution
        // repository does not add the solution, and thus does not store the
        // solution info. In that case, we put it back.
        if (local_response.solution_info().empty()) {
          local_response.set_solution_info(
              absl::StrCat(lns_info, " [presolve]"));
        }
      } else {
        // TODO(user): Clean this up? when the model is closed by presolve,
        // we don't have a nice api to get the response with stats. That said
        // for LNS, we don't really need it.
        if (presolve_status == CpSolverStatus::INFEASIBLE) {
          local_response_manager->NotifyThatImprovingProblemIsInfeasible(
              "presolve");
        }
        local_response = local_response_manager->GetResponse();
        local_response.set_status(presolve_status);
      }
      const std::string solution_info = local_response.solution_info();
      std::vector<int64_t> solution_values(local_response.solution().begin(),
                                           local_response.solution().end());

      data.status = local_response.status();
      // TODO(user): we actually do not need to postsolve if the solution is
      // not going to be used...
      if (data.status == CpSolverStatus::OPTIMAL ||
          data.status == CpSolverStatus::FEASIBLE) {
        PostsolveResponseWrapper(
            local_params, helper_->ModelProto().variables_size(), mapping_proto,
            postsolve_mapping, &solution_values);
        local_response.mutable_solution()->Assign(solution_values.begin(),
                                                  solution_values.end());
      }

      data.deterministic_time = local_time_limit->GetElapsedDeterministicTime();

      bool new_solution = false;
      bool display_lns_info = VLOG_IS_ON(2);
      if (!local_response.solution().empty()) {
        // A solution that does not pass our validator indicates a bug. We
        // abort and dump the problematic model to facilitate debugging.
        //
        // TODO(user): In a production environment, we should probably just
        // ignore this fragment and continue.
        const bool feasible =
            SolutionIsFeasible(*shared_->model_proto, solution_values);
        if (!feasible) {
          if (absl::GetFlag(FLAGS_cp_model_dump_problematic_lns)) {
            const std::string name =
                absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                             debug_copy.name(), ".pb.txt");
            LOG(INFO) << "Dumping problematic LNS model to '" << name << "'.";
            CHECK(WriteModelProtoToFile(debug_copy, name));
          }
          LOG(ERROR) << "Infeasible LNS solution! " << solution_info
                     << " solved with params " << local_params;
          return;
        }

        // Special case if we solved a part of the full problem!
        //
        // TODO(user): This do not work if they are symmetries loaded into SAT.
        // For now we just disable this if there is any symmetry. See for
        // instance spot5_1401.fzn. Be smarter about that.
        //
        // The issue is that as we fix level zero variables from a partial
        // solution, the symmetry propagator could wrongly fix other variables
        // since it assumes that if we could infer such fixing, then we could
        // do the same in any symmetric situation.
        //
        // Note sure how to address that, we could disable symmetries if there
        // is a lot of connected components. Or use a different mechanism than
        // just fixing variables. Or remove symmetry on the fly?
        //
        // TODO(user): At least enable it if there is no Boolean symmetries
        // since we currently do not use the other ones past the presolve.
        //
        // TODO(user): We could however fix it in the LNS Helper!
        if (data.status == CpSolverStatus::OPTIMAL &&
            !shared_->model_proto->has_symmetry() && !solution_values.empty() &&
            neighborhood.is_simple &&
            !neighborhood.variables_that_can_be_fixed_to_local_optimum
                 .empty()) {
          display_lns_info = true;
          shared_->bounds->FixVariablesFromPartialSolution(
              solution_values,
              neighborhood.variables_that_can_be_fixed_to_local_optimum);
        }

        // Finish to fill the SolveData now that the local solve is done.
        data.new_objective = data.base_objective;
        if (data.status == CpSolverStatus::OPTIMAL ||
            data.status == CpSolverStatus::FEASIBLE) {
          data.new_objective = IntegerValue(ComputeInnerObjective(
              shared_->model_proto->objective(), solution_values));
        }

        // Report any feasible solution we have. Optimization: We don't do that
        // if we just recovered the base solution.
        if (data.status == CpSolverStatus::OPTIMAL ||
            data.status == CpSolverStatus::FEASIBLE) {
          const std::vector<int64_t> base_solution(
              base_response.solution().begin(), base_response.solution().end());
          if (solution_values != base_solution) {
            new_solution = true;
            shared_->response->NewSolution(solution_values, solution_info,
                                           /*model=*/nullptr);
          }
        }
        if (!neighborhood.is_reduced &&
            (data.status == CpSolverStatus::OPTIMAL ||
             data.status == CpSolverStatus::INFEASIBLE)) {
          shared_->response->NotifyThatImprovingProblemIsInfeasible(
              solution_info);
          shared_->time_limit->Stop();
        }
      }

      generator_->AddSolveData(data);

      if (VLOG_IS_ON(1) && display_lns_info) {
        std::string s = absl::StrCat("              LNS ", name(), ":");
        if (new_solution) {
          const double base_obj = ScaleObjectiveValue(
              shared_->model_proto->objective(),
              ComputeInnerObjective(shared_->model_proto->objective(),
                                    base_response.solution()));
          const double new_obj = ScaleObjectiveValue(
              shared_->model_proto->objective(),
              ComputeInnerObjective(shared_->model_proto->objective(),
                                    solution_values));
          absl::StrAppend(&s, " [new_sol:", base_obj, " -> ", new_obj, "]");
        }
        if (neighborhood.is_simple) {
          absl::StrAppend(
              &s, " [", "relaxed:", neighborhood.num_relaxed_variables,
              " in_obj:", neighborhood.num_relaxed_variables_in_objective,
              " compo:",
              neighborhood.variables_that_can_be_fixed_to_local_optimum.size(),
              "]");
        }
        SOLVER_LOG(shared_->logger, s, " [d:", data.difficulty,
                   ", id:", task_id, ", dtime:", data.deterministic_time, "/",
                   data.deterministic_limit,
                   ", status:", ProtoEnumToString<CpSolverStatus>(data.status),
                   ", #calls:", generator_->num_calls(),
                   ", p:", fully_solved_proportion, "]");
      }
    };
  }

  void Synchronize() override {
    const double dtime = generator_->Synchronize();
    AddTaskDeterministicDuration(dtime);
    shared_->time_limit->AdvanceDeterministicTime(dtime);
  }

 private:
  std::unique_ptr<NeighborhoodGenerator> generator_;
  NeighborhoodGeneratorHelper* helper_;
  const SatParameters lns_parameters_;
  SharedClasses* shared_;
};

void SolveCpModelParallel(const CpModelProto& model_proto,
                          Model* global_model) {
  const SatParameters& params = *global_model->GetOrCreate<SatParameters>();
  CHECK(!params.enumerate_all_solutions())
      << "Enumerating all solutions in parallel is not supported.";
  if (global_model->GetOrCreate<TimeLimit>()->LimitReached()) return;

  SharedClasses shared(&model_proto, global_model);

  if (params.share_level_zero_bounds()) {
    shared.bounds = std::make_unique<SharedBoundsManager>(model_proto);
    shared.bounds->set_dump_prefix(absl::GetFlag(FLAGS_cp_model_dump_prefix));
    shared.bounds->LoadDebugSolution(
        global_model->GetOrCreate<SharedResponseManager>()->DebugSolution());
  }

  shared.lp_solutions = std::make_unique<SharedLPSolutionRepository>(
      /*num_solutions_to_keep=*/10);
  global_model->Register<SharedLPSolutionRepository>(shared.lp_solutions.get());

  const bool testing = params.use_lns_only() || params.test_feasibility_jump();

  // We currently only use the feasibility pump if it is enabled and some other
  // parameters are not on.
  const bool use_feasibility_pump = params.use_feasibility_pump() &&
                                    params.linearization_level() > 0 &&
                                    !testing && !params.interleave_search();
  if (use_feasibility_pump || params.use_rins_lns()) {
    shared.incomplete_solutions =
        std::make_unique<SharedIncompleteSolutionManager>();
    global_model->Register<SharedIncompleteSolutionManager>(
        shared.incomplete_solutions.get());
  }

  // Set up synchronization mode in parallel.
  const bool always_synchronize =
      !params.interleave_search() || params.num_workers() <= 1;
  shared.response->SetSynchronizationMode(always_synchronize);

  if (params.share_binary_clauses()) {
    shared.clauses = std::make_unique<SharedClausesManager>(always_synchronize);
  }

  // The list of all the SubSolver that will be used in this parallel search.
  std::vector<std::unique_ptr<SubSolver>> subsolvers;
  std::vector<std::unique_ptr<SubSolver>> incomplete_subsolvers;

  // Add a synchronization point for the shared classes.
  subsolvers.push_back(std::make_unique<SynchronizationPoint>(
      "synchronization_agent", [&shared]() {
        shared.response->Synchronize();
        shared.response->MutableSolutionsRepository()->Synchronize();
        if (shared.bounds != nullptr) {
          shared.bounds->Synchronize();
        }
        if (shared.lp_solutions != nullptr) {
          shared.lp_solutions->Synchronize();
        }
        if (shared.clauses != nullptr) {
          shared.clauses->Synchronize();
        }
      }));

  // Add the NeighborhoodGeneratorHelper as a special subsolver so that its
  // Synchronize() is called before any LNS neighborhood solvers.
  auto unique_helper = std::make_unique<NeighborhoodGeneratorHelper>(
      &model_proto, &params, shared.response, shared.bounds.get());
  NeighborhoodGeneratorHelper* helper = unique_helper.get();
  subsolvers.push_back(std::move(unique_helper));

  int num_full_problem_solvers = 0;
  if (testing) {
    // Register something to find a first solution. Note that this is mainly
    // used for experimentation, and using no_lp usually result in a faster
    // first solution.
    //
    // TODO(user): merge code with standard solver. Just make sure that all
    // full solvers die after the first solution has been found.
    if (!params.test_feasibility_jump()) {
      SatParameters local_params = params;
      local_params.set_stop_after_first_solution(true);
      local_params.set_linearization_level(0);
      subsolvers.push_back(std::make_unique<FullProblemSolver>(
          "first_solution", local_params,
          /*split_in_chunks=*/false, &shared));
    }
  } else {
    for (const SatParameters& local_params : GetWorkSharingParams(
             params, model_proto, params.shared_tree_num_workers())) {
      subsolvers.push_back(std::make_unique<FullProblemSolver>(
          local_params.name(), local_params,
          /*split_in_chunks=*/params.interleave_search(), &shared));
      num_full_problem_solvers++;
    }
    for (const SatParameters& local_params :
         GetDiverseSetOfParameters(params, model_proto)) {
      // TODO(user): This is currently not supported here.
      if (params.optimize_with_max_hs()) continue;
      ++num_full_problem_solvers;

      if (local_params.use_objective_shaving_search()) {
        subsolvers.push_back(std::make_unique<ObjectiveShavingSolver>(
            local_params, helper, &shared));
        continue;
      }

      subsolvers.push_back(std::make_unique<FullProblemSolver>(
          local_params.name(), local_params,
          /*split_in_chunks=*/params.interleave_search(), &shared));
    }
  }

  // Add FeasibilityPumpSolver if enabled.
  if (use_feasibility_pump) {
    incomplete_subsolvers.push_back(
        std::make_unique<FeasibilityPumpSolver>(params, &shared));
  }

  const SatParameters lns_params = GetNamedParameters(params).at("lns");

  // By default we use the user provided parameters.
  // TODO(user): for now this is not deterministic so we disable it on
  // interleave search. Fix.
  if (!testing && params.use_rins_lns() && !params.interleave_search()) {
    // Note that we always create the SharedLPSolutionRepository. This meets
    // the requirement of having a SharedLPSolutionRepository to
    // create RINS/RENS lns generators.

    // TODO(user): Do we create a variable number of these workers.
    incomplete_subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<RelaxationInducedNeighborhoodGenerator>(
            helper, shared.response, shared.lp_solutions.get(),
            shared.incomplete_solutions.get(), "rins/rens"),
        lns_params, helper, &shared));
  }
  const int num_incomplete_solvers =
      params.num_workers() - num_full_problem_solvers;
  const LinearModel* linear_model = global_model->Get<LinearModel>();
  if (!params.interleave_search() && model_proto.has_objective()) {
    int num_violation_ls = params.has_num_violation_ls()
                               ? params.num_violation_ls()
                               : num_incomplete_solvers / 8 + 1;
    if (params.test_feasibility_jump()) {
      num_violation_ls = params.num_workers();
    }
    for (int i = 0; i < num_violation_ls; ++i) {
      SatParameters local_params = params;
      local_params.set_random_seed(ValidSumSeed(params.random_seed(), i));
      incomplete_subsolvers.push_back(std::make_unique<FeasibilityJumpSolver>(
          "violation_ls", SubSolver::INCOMPLETE, linear_model, local_params,
          shared.time_limit, shared.response, shared.bounds.get(), shared.stats,
          &shared.stat_tables));
    }
  }

  // Adds first solution subsolvers.
  //
  // The logic is the following. Before the first solution is found, we have (in
  // order):
  //   - num_full_problem_solvers full problem solvers
  //   - num_workers - num_full_problem_solvers -
  //         num_dedicated_incomplete_solvers first solution solvers.
  //   - num_workers - num_full_problem_solvers incomplete solvers. Only
  //     num_dedicated_incomplete_solvers are active before the first solution
  //     is found.
  //
  // After the first solution is found, all first solution solvers die, the we
  // have num_full_problem_solvers null problem solvers, and the rest are
  // incomplete solvers.
  //
  // TODO(user): Check with interleave_search.
  if (!model_proto.has_objective() || model_proto.objective().vars().empty() ||
      !params.interleave_search() || params.test_feasibility_jump()) {
    const int max_num_incomplete_solvers_running_before_the_first_solution =
        params.num_workers() <= 16 ? 1 : 2;
    const int num_reserved_incomplete_solvers =
        params.test_feasibility_jump()
            ? 0
            : std::min(
                  max_num_incomplete_solvers_running_before_the_first_solution,
                  static_cast<int>(incomplete_subsolvers.size()));
    const int num_available = params.num_workers() - num_full_problem_solvers -
                              num_reserved_incomplete_solvers;

    // TODO(user): FeasibilityJumpSolver are split in chunk as so we could
    // schedule more than the available number of threads. They will just be
    // interleaved. We will get an higher diversity, but use more memory.
    const int num_feasibility_jump =
        (params.interleave_search() || !params.use_feasibility_jump())
            ? 0
            : (params.test_feasibility_jump() ? num_available
                                              : (num_available + 1) / 2);
    const int num_first_solution_subsolvers =
        num_available - num_feasibility_jump;

    // TODO(user): Limit number of options by improving restart
    // heuristic and randomizing other option at each restart?
    for (int i = 0; i < num_feasibility_jump; ++i) {
      // We alternate with a bunch of heuristic.
      SatParameters local_params = params;
      local_params.set_random_seed(ValidSumSeed(params.random_seed(), i));
      std::string name = "fj";

      // Long restart or quick restart.
      if (i % 2 == 0) {
        absl::StrAppend(&name, "_short");
        local_params.set_feasibility_jump_restart_factor(1);
      } else {
        absl::StrAppend(&name, "_long");
        local_params.set_feasibility_jump_restart_factor(100);
      }

      // Linear or not.
      if (i / 2 % 2 == 0) {
        local_params.set_feasibility_jump_linearization_level(0);
      } else {
        absl::StrAppend(&name, "_lin");
        local_params.set_feasibility_jump_linearization_level(2);
      }

      // Default restart, random restart, or perturb restart.
      if (i / 4 % 3 == 0) {
        absl::StrAppend(&name, "_default");
        local_params.set_feasibility_jump_enable_restarts(true);
        local_params.set_feasibility_jump_var_randomization_probability(0.0);
      } else if (i / 4 % 3 == 1) {
        absl::StrAppend(&name, "_random");
        local_params.set_feasibility_jump_enable_restarts(true);
        local_params.set_feasibility_jump_var_randomization_probability(0.05);
      } else {
        absl::StrAppend(&name, "_perturb");
        local_params.set_feasibility_jump_enable_restarts(false);
        local_params.set_feasibility_jump_var_randomization_probability(0.05);
      }

      incomplete_subsolvers.push_back(std::make_unique<FeasibilityJumpSolver>(
          name, SubSolver::FIRST_SOLUTION, linear_model, local_params,
          shared.time_limit, shared.response, shared.bounds.get(), shared.stats,
          &shared.stat_tables));
    }
    for (const SatParameters& local_params : GetFirstSolutionParams(
             params, model_proto, num_first_solution_subsolvers)) {
      subsolvers.push_back(std::make_unique<FullProblemSolver>(
          local_params.name(), local_params,
          /*split_in_chunks=*/params.interleave_search(), &shared,
          /*stop_on_first_solution=*/true));
    }
  }

  // Now that first solutions solvers are in place, we can move the
  // incomplete_subsolvers into subsolvers.
  for (int i = 0; i < incomplete_subsolvers.size(); ++i) {
    subsolvers.push_back(std::move(incomplete_subsolvers[i]));
  }
  incomplete_subsolvers.clear();

  //  Add incomplete subsolvers that require an objective.
  if (model_proto.has_objective() && !model_proto.objective().vars().empty() &&
      !params.test_feasibility_jump()) {
    // Enqueue all the possible LNS neighborhood subsolvers.
    // Each will have their own metrics.
    subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<RelaxRandomVariablesGenerator>(helper, "rnd_var_lns"),
        lns_params, helper, &shared));
    subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<RelaxRandomConstraintsGenerator>(helper,
                                                          "rnd_cst_lns"),
        lns_params, helper, &shared));
    subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<VariableGraphNeighborhoodGenerator>(helper,
                                                             "graph_var_lns"),
        lns_params, helper, &shared));
    subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<ArcGraphNeighborhoodGenerator>(helper,
                                                        "graph_arc_lns"),
        lns_params, helper, &shared));
    subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<ConstraintGraphNeighborhoodGenerator>(helper,
                                                               "graph_cst_lns"),
        lns_params, helper, &shared));
    subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<DecompositionGraphNeighborhoodGenerator>(
            helper, "graph_dec_lns"),
        lns_params, helper, &shared));

    if (params.use_lb_relax_lns()) {
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<LocalBranchingLpBasedNeighborhoodGenerator>(
              helper, "lb_relax_lns",
              [](const CpModelProto cp_model, Model* model) {
                model->GetOrCreate<SharedResponseManager>()
                    ->InitializeObjective(cp_model);
                LoadCpModel(cp_model, model);
                SolveLoadedCpModel(cp_model, model);
              },
              shared.time_limit),
          lns_params, helper, &shared));
    }

    const bool has_no_overlap_or_cumulative =
        !helper->TypeToConstraints(ConstraintProto::kNoOverlap).empty() ||
        !helper->TypeToConstraints(ConstraintProto::kCumulative).empty();

    // Scheduling (no_overlap and cumulative) specific LNS.
    if (has_no_overlap_or_cumulative) {
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RandomIntervalSchedulingNeighborhoodGenerator>(
              helper, "scheduling_intervals_lns"),
          lns_params, helper, &shared));
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<SchedulingTimeWindowNeighborhoodGenerator>(
              helper, "scheduling_time_window_lns"),
          lns_params, helper, &shared));
      const std::vector<std::vector<int>> intervals_in_constraints =
          helper->GetUniqueIntervalSets();
      if (intervals_in_constraints.size() > 2) {
        subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<SchedulingResourceWindowsNeighborhoodGenerator>(
                helper, intervals_in_constraints,
                "scheduling_resource_windows_lns"),
            lns_params, helper, &shared));
      }
    }

    // Packing (no_overlap_2d) Specific LNS.
    const bool has_no_overlap2d =
        !helper->TypeToConstraints(ConstraintProto::kNoOverlap2D).empty();
    if (has_no_overlap2d) {
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RandomRectanglesPackingNeighborhoodGenerator>(
              helper, "packing_rectangles_lns"),
          lns_params, helper, &shared));
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RandomPrecedencesPackingNeighborhoodGenerator>(
              helper, "packing_precedences_lns"),
          lns_params, helper, &shared));
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<SlicePackingNeighborhoodGenerator>(
              helper, "packing_slice_lns"),
          lns_params, helper, &shared));
    }

    // Generic scheduling/packing LNS.
    if (has_no_overlap_or_cumulative || has_no_overlap2d) {
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RandomPrecedenceSchedulingNeighborhoodGenerator>(
              helper, "scheduling_precedences_lns"),
          lns_params, helper, &shared));
    }

    const int num_circuit = static_cast<int>(
        helper->TypeToConstraints(ConstraintProto::kCircuit).size());
    const int num_routes = static_cast<int>(
        helper->TypeToConstraints(ConstraintProto::kRoutes).size());
    if (num_circuit + num_routes > 0) {
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RoutingRandomNeighborhoodGenerator>(
              helper, "routing_random_lns"),
          lns_params, helper, &shared));

      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RoutingPathNeighborhoodGenerator>(
              helper, "routing_path_lns"),
          lns_params, helper, &shared));
    }
    if (num_routes > 0 || num_circuit > 1) {
      subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RoutingFullPathNeighborhoodGenerator>(
              helper, "routing_full_path_lns"),
          lns_params, helper, &shared));
    }
  }

  // Add a synchronization point for the gap integral that is executed last.
  // This way, after each batch, the proper deterministic time is updated and
  // then the function to integrate take the value of the new gap.
  if (model_proto.has_objective() && !model_proto.objective().vars().empty()) {
    subsolvers.push_back(std::make_unique<SynchronizationPoint>(
        "update_gap_integral",
        [&shared]() { shared.response->UpdateGapIntegral(); }));
  }

  // Log the name of all our SubSolvers.
  auto* logger = global_model->GetOrCreate<SolverLogger>();
  if (logger->LoggingIsEnabled()) {
    // Collect subsolver names per type (full, lns, 1st solution).
    std::vector<std::string> full_problem_solver_names;
    std::vector<std::string> incomplete_solver_names;
    std::vector<std::string> first_solution_solver_names;
    std::vector<std::string> helper_solver_names;
    for (int i = 0; i < subsolvers.size(); ++i) {
      const auto& subsolver = subsolvers[i];
      switch (subsolver->type()) {
        case SubSolver::FULL_PROBLEM:
          full_problem_solver_names.push_back(subsolver->name());
          break;
        case SubSolver::INCOMPLETE:
          incomplete_solver_names.push_back(subsolver->name());
          break;
        case SubSolver::FIRST_SOLUTION:
          first_solution_solver_names.push_back(subsolver->name());
          break;
        case SubSolver::HELPER:
          helper_solver_names.push_back(subsolver->name());
          break;
      }
    }
    SOLVER_LOG(logger, "");

    if (params.interleave_search()) {
      SOLVER_LOG(logger,
                 absl::StrFormat("Starting deterministic search at %.2fs with "
                                 "%i workers and batch size of %d.",
                                 shared.wall_timer->Get(), params.num_workers(),
                                 params.interleave_batch_size()));
    } else {
      SOLVER_LOG(logger, absl::StrFormat(
                             "Starting search at %.2fs with %i workers.",
                             shared.wall_timer->Get(), params.num_workers()));
    }

    // TODO(user): We might not want to sort the subsolver by name to keep our
    // ordered list by importance? not sure.
    auto display_subsolver_list = [logger](
                                      const std::vector<std::string>& names,
                                      const absl::string_view type_name) {
      if (!names.empty()) {
        absl::btree_map<std::string, int> solvers_and_count;
        for (const auto& name : names) {
          solvers_and_count[name]++;
        }
        std::vector<std::string> counted_names;
        for (const auto& [name, count] : solvers_and_count) {
          if (count == 1) {
            counted_names.push_back(name);
          } else {
            counted_names.push_back(absl::StrCat(name, "(", count, ")"));
          }
        }
        SOLVER_LOG(
            logger, names.size(), " ",
            absl::StrCat(type_name, names.size() == 1 ? "" : "s"), ": [",
            absl::StrJoin(counted_names.begin(), counted_names.end(), ", "),
            "]");
      }
    };

    display_subsolver_list(full_problem_solver_names, "full problem subsolver");
    display_subsolver_list(first_solution_solver_names,
                           "first solution subsolver");
    display_subsolver_list(incomplete_solver_names, "incomplete subsolver");
    display_subsolver_list(helper_solver_names, "helper subsolver");
  }

  // Launch the main search loop.
  if (params.interleave_search()) {
    int batch_size = params.interleave_batch_size();
    if (batch_size == 0) {
      batch_size = params.num_workers() == 1 ? 1 : params.num_workers() * 3;
      SOLVER_LOG(
          logger,
          "Setting number of tasks in each batch of interleaved search to ",
          batch_size);
    }
    DeterministicLoop(subsolvers, params.num_workers(), batch_size);
  } else {
    NonDeterministicLoop(subsolvers, params.num_workers());
  }

  // We need to delete the other subsolver in order to fill the stat tables.
  // Note that first solution should already be deleted.
  // We delete manually as windows release vectors in the opposite order.
  for (int i = 0; i < subsolvers.size(); ++i) {
    subsolvers[i].reset();
  }

  // Log statistics.
  if (logger->LoggingIsEnabled()) {
    logger->FlushPendingThrottledLogs(/*ignore_rates=*/true);
    SOLVER_LOG(logger, "");

    shared.stat_tables.Display(logger);

    shared.response->DisplayImprovementStatistics();

    std::vector<std::vector<std::string>> table;
    table.push_back(
        {"Solution repositories", "Added", "Queried", "Ignored", "Synchro"});
    table.push_back(shared.response->SolutionsRepository().TableLineStats());
    if (shared.lp_solutions != nullptr) {
      table.push_back(shared.lp_solutions->TableLineStats());
    }
    if (shared.incomplete_solutions != nullptr) {
      table.push_back(shared.incomplete_solutions->TableLineStats());
    }
    SOLVER_LOG(logger, FormatTable(table));

    if (shared.bounds) {
      shared.bounds->LogStatistics(logger);
    }

    if (shared.clauses) {
      shared.clauses->LogStatistics(logger);
    }
  }
}

#endif  // __PORTABLE_PLATFORM__

// If the option use_sat_inprocessing is true, then before post-solving a
// solution, we need to make sure we add any new clause required for postsolving
// to the mapping_model.
void AddPostsolveClauses(const std::vector<int>& postsolve_mapping,
                         Model* model, CpModelProto* mapping_proto) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  auto* postsolve = model->GetOrCreate<PostsolveClauses>();
  for (const auto& clause : postsolve->clauses) {
    auto* ct = mapping_proto->add_constraints()->mutable_bool_or();
    for (const Literal l : clause) {
      int var = mapping->GetProtoVariableFromBooleanVariable(l.Variable());
      CHECK_NE(var, -1);
      var = postsolve_mapping[var];
      ct->add_literals(l.IsPositive() ? var : NegatedRef(var));
    }
  }
  postsolve->clauses.clear();
}

void TestSolutionHintForFeasibility(const CpModelProto& model_proto,
                                    SolverLogger* logger,
                                    SharedResponseManager* manager = nullptr) {
  if (!model_proto.has_solution_hint()) return;

  // TODO(user): If the hint specifies all non-fixed variables we could also
  // do the check.
  if (model_proto.solution_hint().vars_size() != model_proto.variables_size()) {
    SOLVER_LOG(logger, "The solution hint is incomplete: ",
               model_proto.solution_hint().vars_size(), " out of ",
               model_proto.variables_size(), " variables hinted.");
    return;
  }

  std::vector<int64_t> solution(model_proto.variables_size(), 0);
  for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
    const int ref = model_proto.solution_hint().vars(i);
    const int64_t value = model_proto.solution_hint().values(i);
    solution[PositiveRef(ref)] = RefIsPositive(ref) ? value : -value;
  }
  if (SolutionIsFeasible(model_proto, solution)) {
    if (manager != nullptr) {
      // Add it to the pool right away! Note that we already have a log in this
      // case, so we don't log anything more.
      manager->NewSolution(solution, "complete_hint", nullptr);
    } else {
      SOLVER_LOG(logger, "The solution hint is complete and is feasible.");
    }
  } else {
    // TODO(user): Change the code to make the solution checker more
    // informative by returning a message instead of just VLOGing it.
    SOLVER_LOG(logger,
               "The solution hint is complete, but it is infeasible! we "
               "will try to repair it.");
  }
}

}  // namespace

CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model) {
  auto* wall_timer = model->GetOrCreate<WallTimer>();
  auto* user_timer = model->GetOrCreate<UserTimer>();
  wall_timer->Start();
  user_timer->Start();

#if !defined(__PORTABLE_PLATFORM__)
  // Dump initial model?
  if (absl::GetFlag(FLAGS_cp_model_dump_models)) {
    DumpModelProto(model_proto, "model");
  }
  if (absl::GetFlag(FLAGS_cp_model_export_model)) {
    if (model_proto.name().empty()) {
      DumpModelProto(model_proto, "unnamed_model");
    } else {
      DumpModelProto(model_proto, model_proto.name());
    }
  }

  // Override parameters?
  if (!absl::GetFlag(FLAGS_cp_model_params).empty()) {
    SatParameters params = *model->GetOrCreate<SatParameters>();
    SatParameters flag_params;
    CHECK(google::protobuf::TextFormat::ParseFromString(
        absl::GetFlag(FLAGS_cp_model_params), &flag_params));
    params.MergeFrom(flag_params);
    *(model->GetOrCreate<SatParameters>()) = params;
  }
#endif  // __PORTABLE_PLATFORM__

  // Enable the logging component.
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  SolverLogger* logger = model->GetOrCreate<SolverLogger>();
  logger->EnableLogging(params.log_search_progress() || VLOG_IS_ON(1));
  logger->SetLogToStdOut(params.log_to_stdout());
  std::string log_string;
  if (params.log_to_response()) {
    logger->AddInfoLoggingCallback([&log_string](absl::string_view message) {
      absl::StrAppend(&log_string, message, "\n");
    });
  }

  auto* shared_response_manager = model->GetOrCreate<SharedResponseManager>();
  shared_response_manager->set_dump_prefix(
      absl::GetFlag(FLAGS_cp_model_dump_prefix));

#if !defined(__PORTABLE_PLATFORM__)
  // Note that the postprocessors are executed in reverse order, so this
  // will always dump the response just before it is returned since it is
  // the first one we register.
  if (absl::GetFlag(FLAGS_cp_model_dump_response)) {
    shared_response_manager->AddFinalResponsePostprocessor(
        [](CpSolverResponse* response) {
          const std::string file = absl::StrCat(
              absl::GetFlag(FLAGS_cp_model_dump_prefix), "response.pb.txt");
          LOG(INFO) << "Dumping response proto to '" << file << "'.";
          CHECK(WriteModelProtoToFile(*response, file));
        });
  }
#endif  // __PORTABLE_PLATFORM__

  // Always display the final response stats if requested.
  // This also copy the logs to the response if requested.
  shared_response_manager->AddFinalResponsePostprocessor(
      [logger, &model_proto, &log_string](CpSolverResponse* response) {
        SOLVER_LOG(logger, CpSolverResponseStats(
                               *response,
                               model_proto.has_objective() ||
                                   model_proto.has_floating_point_objective()));
        if (!log_string.empty()) {
          response->set_solve_log(log_string);
        }
      });

  // Always add the timing information to a response. Note that it is important
  // to add this after the log/dump postprocessor since we execute them in
  // reverse order.
  auto* shared_time_limit = model->GetOrCreate<ModelSharedTimeLimit>();
  shared_response_manager->AddResponsePostprocessor(
      [&wall_timer, &user_timer,
       &shared_time_limit](CpSolverResponse* response) {
        response->set_wall_time(wall_timer->Get());
        response->set_user_time(user_timer->Get());
        response->set_deterministic_time(
            shared_time_limit->GetElapsedDeterministicTime());
      });

  // Validate parameters.
  //
  // Note that the few parameters we use before that are Booleans and thus
  // "safe". We need to delay the validation to return a proper response.
  {
    const std::string error = ValidateParameters(params);
    if (!error.empty()) {
      SOLVER_LOG(logger, "Invalid parameters: ", error);

      // TODO(user): We currently reuse the MODEL_INVALID status even though it
      // is not the best name for this. Maybe we can add a PARAMETERS_INVALID
      // when it become needed. Or rename to INVALID_INPUT ?
      CpSolverResponse status_response;
      status_response.set_status(CpSolverStatus::MODEL_INVALID);
      status_response.set_solution_info(error);
      FillSolveStatsInResponse(model, &status_response);
      shared_response_manager->AppendResponseToBeMerged(status_response);
      return shared_response_manager->GetResponse();
    }
  }

  // Initialize the time limit from the parameters.
  model->GetOrCreate<TimeLimit>()->ResetLimitFromParameters(params);

#if !defined(__PORTABLE_PLATFORM__)
  // Register SIGINT handler if requested by the parameters.
  if (params.catch_sigint_signal()) {
    model->GetOrCreate<SigintHandler>()->Register(
        [&shared_time_limit]() { shared_time_limit->Stop(); });
  }
#endif  // __PORTABLE_PLATFORM__

  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, "Starting ", CpSatSolverVersion());
  SOLVER_LOG(logger, "Parameters: ", ProtobufShortDebugString(params));

  // Update params.num_workers() if the old field was used.
  if (params.num_workers() == 0) {
    model->GetOrCreate<SatParameters>()->set_num_workers(
        params.num_search_workers());
  }

  // Initialize the number of workers if set to 0.
  if (params.num_workers() == 0) {
#if !defined(__PORTABLE_PLATFORM__)
    // Sometimes, hardware_concurrency will return 0. So always default to 1.
    const int num_cores =
        params.enumerate_all_solutions() || !model_proto.assumptions().empty()
            ? 1
            : std::max<int>(std::thread::hardware_concurrency(), 1);
#else
    const int num_cores = 1;
#endif
    SOLVER_LOG(logger, "Setting number of workers to ", num_cores);
    model->GetOrCreate<SatParameters>()->set_num_workers(num_cores);
  }

  if (logger->LoggingIsEnabled() && params.use_absl_random()) {
    model->GetOrCreate<ModelRandomGenerator>()->LogSalt();
  }

  // Validate model_proto.
  // TODO(user): provide an option to skip this step for speed?
  {
    const std::string error = ValidateInputCpModel(params, model_proto);
    if (!error.empty()) {
      SOLVER_LOG(logger, "Invalid model: ", error);
      CpSolverResponse status_response;
      status_response.set_status(CpSolverStatus::MODEL_INVALID);
      status_response.set_solution_info(error);
      FillSolveStatsInResponse(model, &status_response);
      shared_response_manager->AppendResponseToBeMerged(status_response);
      return shared_response_manager->GetResponse();
    }
  }

  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, "Initial ", CpModelStats(model_proto));

  // Presolve and expansions.
  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger,
             absl::StrFormat("Starting presolve at %.2fs", wall_timer->Get()));

  // Note: Allocating in an arena significantly speed up destruction (free) for
  // large messages.
  google::protobuf::Arena arena;
  CpModelProto* new_cp_model_proto =
      google::protobuf::Arena::Create<CpModelProto>(&arena);
  CpModelProto* mapping_proto =
      google::protobuf::Arena::Create<CpModelProto>(&arena);
  auto context = std::make_unique<PresolveContext>(model, new_cp_model_proto,
                                                   mapping_proto);

  if (absl::GetFlag(FLAGS_debug_model_copy)) {
    *new_cp_model_proto = model_proto;
  } else if (!ImportModelWithBasicPresolveIntoContext(model_proto,
                                                      context.get())) {
    VLOG(1) << "Model found infeasible during copy";
    // TODO(user): At this point, the model is trivial, but we could exit
    // early.
  }

  if (absl::GetFlag(FLAGS_cp_model_ignore_objective) &&
      (context->working_model->has_objective() ||
       context->working_model->has_floating_point_objective())) {
    SOLVER_LOG(logger, "Ignoring objective");
    context->working_model->clear_objective();
    context->working_model->clear_floating_point_objective();
  }

  if (absl::GetFlag(FLAGS_cp_model_ignore_hints) &&
      context->working_model->has_solution_hint()) {
    SOLVER_LOG(logger, "Ignoring solution hint");
    context->working_model->clear_solution_hint();
  }

  // Checks for hints early in case they are forced to be hard constraints.
  if (params.fix_variables_to_their_hinted_value() &&
      model_proto.has_solution_hint()) {
    SOLVER_LOG(logger, "Fixing ", model_proto.solution_hint().vars().size(),
               " variables to their value in the solution hints.");
    for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
      const int var = model_proto.solution_hint().vars(i);
      const int64_t value = model_proto.solution_hint().values(i);
      if (!context->IntersectDomainWith(var, Domain(value))) {
        const IntegerVariableProto& var_proto =
            context->working_model->variables(var);
        const std::string var_name = var_proto.name().empty()
                                         ? absl::StrCat("var(", var, ")")
                                         : var_proto.name();

        const Domain var_domain = ReadDomainFromProto(var_proto);
        SOLVER_LOG(logger, "Hint found infeasible when assigning variable '",
                   var_name, "' with domain", var_domain.ToString(),
                   " the value ", value);
        break;
      }
    }
  }

  // If the hint is complete, we can use the solution checker to do more
  // validation. Note that after the model has been validated, we are sure there
  // are do duplicate variables in the solution hint, so we can just check the
  // size.
  if (!context->ModelIsUnsat()) {
    TestSolutionHintForFeasibility(model_proto, logger);
  }

  // If the objective was a floating point one, do some postprocessing on the
  // final response.
  if (model_proto.has_floating_point_objective()) {
    shared_response_manager->AddFinalResponsePostprocessor(
        [&params, &model_proto, mapping_proto,
         &logger](CpSolverResponse* response) {
          if (response->solution().empty()) return;

          // Compute the true objective of the best returned solution.
          const auto& float_obj = model_proto.floating_point_objective();
          double value = float_obj.offset();
          const int num_terms = float_obj.vars().size();
          for (int i = 0; i < num_terms; ++i) {
            value += float_obj.coeffs(i) *
                     static_cast<double>(response->solution(float_obj.vars(i)));
          }
          response->set_objective_value(value);

          // Also copy the scaled objective which must be in the mapping model.
          // This can be useful for some client, like if they want to do
          // multi-objective optimization in stages.
          if (!mapping_proto->has_objective()) return;
          const CpObjectiveProto& integer_obj = mapping_proto->objective();
          *response->mutable_integer_objective() = integer_obj;

          // If requested, compute a correct lb from the one on the integer
          // objective. We only do that if some error were introduced by the
          // scaling algorithm.
          if (params.mip_compute_true_objective_bound() &&
              !integer_obj.scaling_was_exact()) {
            const int64_t integer_lb = response->inner_objective_lower_bound();
            const double lb = ComputeTrueObjectiveLowerBound(
                model_proto, integer_obj, integer_lb);
            SOLVER_LOG(logger, "[Scaling] scaled_objective_bound: ",
                       response->best_objective_bound(),
                       " corrected_bound: ", lb,
                       " delta: ", response->best_objective_bound() - lb);

            // To avoid small errors that can be confusing, we take the
            // min/max with the objective value.
            if (float_obj.maximize()) {
              response->set_best_objective_bound(
                  std::max(lb, response->objective_value()));
            } else {
              response->set_best_objective_bound(
                  std::min(lb, response->objective_value()));
            }
          }

          // Check the absolute gap, and display warning if needed.
          // TODO(user): Change status to IMPRECISE?
          if (response->status() == CpSolverStatus::OPTIMAL) {
            const double gap = std::abs(response->objective_value() -
                                        response->best_objective_bound());
            if (gap > params.absolute_gap_limit()) {
              SOLVER_LOG(logger,
                         "[Scaling] Warning: OPTIMAL was reported, yet the "
                         "objective gap (",
                         gap, ") is greater than requested absolute limit (",
                         params.absolute_gap_limit(), ").");
            }
          }
        });
  }

  if (!model_proto.assumptions().empty() &&
      (params.num_workers() > 1 || model_proto.has_objective() ||
       model_proto.has_floating_point_objective() ||
       params.enumerate_all_solutions())) {
    SOLVER_LOG(
        logger,
        "Warning: solving with assumptions was requested in a non-fully "
        "supported setting.\nWe will assumes these assumptions true while "
        "solving, but if the model is infeasible, you will not get a useful "
        "'sufficient_assumptions_for_infeasibility' field in the response, it "
        "will include all assumptions.");

    // For the case where the assumptions are currently not supported, we just
    // assume they are fixed, and will always report all of them in the UNSAT
    // core if the problem turn out to be UNSAT.
    //
    // If the mode is not degraded, we will hopefully report a small subset
    // in case there is no feasible solution under these assumptions.
    shared_response_manager->AddFinalResponsePostprocessor(
        [&model_proto](CpSolverResponse* response) {
          if (response->status() != CpSolverStatus::INFEASIBLE) return;

          // For now, just pass in all assumptions.
          *response->mutable_sufficient_assumptions_for_infeasibility() =
              model_proto.assumptions();
        });

    // Clear them from the new proto.
    new_cp_model_proto->clear_assumptions();

    context->InitializeNewDomains();
    for (const int ref : model_proto.assumptions()) {
      if (!context->SetLiteralToTrue(ref)) {
        CpSolverResponse status_response;
        status_response.set_status(CpSolverStatus::INFEASIBLE);
        status_response.add_sufficient_assumptions_for_infeasibility(ref);
        FillSolveStatsInResponse(model, &status_response);
        shared_response_manager->AppendResponseToBeMerged(status_response);
        return shared_response_manager->GetResponse();
      }
    }
  }

  // Do the actual presolve.
  std::vector<int> postsolve_mapping;
  const CpSolverStatus presolve_status =
      PresolveCpModel(context.get(), &postsolve_mapping);

  if (presolve_status != CpSolverStatus::UNKNOWN) {
    SOLVER_LOG(logger, "Problem closed by presolve.");
    CpSolverResponse status_response;
    status_response.set_status(presolve_status);
    FillSolveStatsInResponse(model, &status_response);
    shared_response_manager->AppendResponseToBeMerged(status_response);
    return shared_response_manager->GetResponse();
  }

  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, "Presolved ", CpModelStats(*new_cp_model_proto));

  if (params.cp_model_presolve()) {
    shared_response_manager->AddSolutionPostprocessor(
        [&model_proto, &params, mapping_proto, &model,
         &postsolve_mapping](std::vector<int64_t>* solution) {
          AddPostsolveClauses(postsolve_mapping, model, mapping_proto);
          PostsolveResponseWrapper(params, model_proto.variables_size(),
                                   *mapping_proto, postsolve_mapping, solution);
        });
    shared_response_manager->AddResponsePostprocessor(
        [&model_proto, &params, mapping_proto,
         &postsolve_mapping](CpSolverResponse* response) {
          // Map back the sufficient assumptions for infeasibility.
          for (int& ref :
               *(response
                     ->mutable_sufficient_assumptions_for_infeasibility())) {
            ref = RefIsPositive(ref)
                      ? postsolve_mapping[ref]
                      : NegatedRef(postsolve_mapping[PositiveRef(ref)]);
          }
          if (!response->solution().empty()) {
            CHECK(SolutionIsFeasible(
                model_proto,
                std::vector<int64_t>(response->solution().begin(),
                                     response->solution().end()),
                mapping_proto, &postsolve_mapping))
                << "postsolved solution";
          }
          if (params.fill_tightened_domains_in_response()) {
            // TODO(user): for now, we just use the domain inferred during
            // presolve.
            if (mapping_proto->variables().size() >=
                model_proto.variables().size()) {
              for (int i = 0; i < model_proto.variables().size(); ++i) {
                *response->add_tightened_variables() =
                    mapping_proto->variables(i);
              }
            }
          }
        });
  } else {
    shared_response_manager->AddFinalResponsePostprocessor(
        [&model_proto](CpSolverResponse* response) {
          if (!response->solution().empty()) {
            CHECK(SolutionIsFeasible(
                model_proto, std::vector<int64_t>(response->solution().begin(),
                                                  response->solution().end())));
          }
        });
    shared_response_manager->AddResponsePostprocessor(
        [&model_proto, &params](CpSolverResponse* response) {
          // Truncate the solution in case model expansion added more variables.
          const int initial_size = model_proto.variables_size();
          if (response->solution_size() > 0) {
            response->mutable_solution()->Truncate(initial_size);
            if (DEBUG_MODE ||
                absl::GetFlag(FLAGS_cp_model_check_intermediate_solutions)) {
              CHECK(SolutionIsFeasible(
                  model_proto,
                  std::vector<int64_t>(response->solution().begin(),
                                       response->solution().end())));
            }
          }
          if (params.fill_tightened_domains_in_response()) {
            *response->mutable_tightened_variables() = model_proto.variables();
          }
        });
  }

  // Delete the context.
  context.reset(nullptr);

  const auto& observers = model->GetOrCreate<SolutionObservers>()->observers;
  if (!observers.empty()) {
    shared_response_manager->AddSolutionCallback(
        [&observers](const CpSolverResponse& response) {
          for (const auto& observer : observers) {
            observer(response);
          }
        });
  }

  const auto& log_callbacks =
      model->GetOrCreate<SolutionObservers>()->log_callbacks;
  for (const auto& callback : log_callbacks) {
    shared_response_manager->AddLogCallback(callback);
  }

  // Make sure everything stops when we have a first solution if requested.
  if (params.stop_after_first_solution()) {
    shared_response_manager->AddSolutionCallback(
        [shared_time_limit](const CpSolverResponse&) {
          shared_time_limit->Stop();
        });
  }

#if !defined(__PORTABLE_PLATFORM__)
  if (absl::GetFlag(FLAGS_cp_model_dump_models)) {
    DumpModelProto(*new_cp_model_proto, "presolved_model");
    DumpModelProto(*mapping_proto, "mapping_model");

    // If the model is convertible to a MIP, we dump it too.
    //
    // TODO(user): We could try to dump our linear relaxation too.
    MPModelProto mip_model;
    if (ConvertCpModelProtoToMPModelProto(*new_cp_model_proto, &mip_model)) {
      DumpModelProto(mip_model, "presolved_mp_model");
    }

    // If the model is convertible to a pure SAT one, we dump it too.
    std::string cnf_string;
    if (ConvertCpModelProtoToCnf(*new_cp_model_proto, &cnf_string)) {
      const std::string filename = absl::StrCat(
          absl::GetFlag(FLAGS_cp_model_dump_prefix), "presolved_cnf_model.cnf");
      LOG(INFO) << "Dumping cnf model to '" << filename << "'.";
      const absl::Status status =
          file::SetContents(filename, cnf_string, file::Defaults());
      if (!status.ok()) LOG(ERROR) << status;
    }
  }
#endif  // __PORTABLE_PLATFORM__

  if (params.stop_after_presolve() || shared_time_limit->LimitReached()) {
    int64_t num_terms = 0;
    for (const ConstraintProto& ct : new_cp_model_proto->constraints()) {
      num_terms += static_cast<int>(UsedVariables(ct).size());
    }
    SOLVER_LOG(
        logger, "Stopped after presolve.",
        "\nPresolvedNumVariables: ", new_cp_model_proto->variables().size(),
        "\nPresolvedNumConstraints: ", new_cp_model_proto->constraints().size(),
        "\nPresolvedNumTerms: ", num_terms);

    CpSolverResponse status_response;
    FillSolveStatsInResponse(model, &status_response);
    shared_response_manager->AppendResponseToBeMerged(status_response);
    return shared_response_manager->GetResponse();
  }

  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, "Preloading model.");

  // If specified, we load the initial objective domain right away in the
  // response manager. Note that the presolve will always fill it with the
  // trivial min/max value if the user left it empty. This avoids to display
  // [-infinity, infinity] for the initial objective search space.
  if (new_cp_model_proto->has_objective()) {
    shared_response_manager->InitializeObjective(*new_cp_model_proto);
    shared_response_manager->SetGapLimitsFromParameters(params);
  }

  // Start counting the primal integral from the current deterministic time and
  // initial objective domain gap that we just filled.
  shared_response_manager->UpdateGapIntegral();

  // Re-test a complete solution hint to see if it survived the presolve.
  // If it is feasible, we load it right away.
  //
  // Tricky: when we enumerate all solutions, we cannot properly exclude the
  // current solution if we didn't find it via full propagation, so we don't
  // load it in this case.
  //
  // TODO(user): Even for an optimization, if we load the solution right away,
  // we might not have the same behavior as the initial search that follow the
  // hint will be infeasible, so the activities of the variables will be
  // different.
  if (!params.enumerate_all_solutions()) {
    TestSolutionHintForFeasibility(*new_cp_model_proto, logger,
                                   shared_response_manager);
  } else {
    TestSolutionHintForFeasibility(*new_cp_model_proto, logger, nullptr);
  }

  if (params.symmetry_level() > 1) {
    DetectAndAddSymmetryToProto(params, new_cp_model_proto, logger);
  }

  LoadDebugSolution(*new_cp_model_proto, model);

  // Linear model (used by feasibility_jump and violation_ls)
  if (params.num_workers() > 1 || params.test_feasibility_jump() ||
      params.num_violation_ls() > 0) {
    LinearModel* linear_model = new LinearModel(*new_cp_model_proto);
    model->TakeOwnership(linear_model);
    model->Register(linear_model);
  }

#if defined(__PORTABLE_PLATFORM__)
  if (/* DISABLES CODE */ (false)) {
    // We ignore the multithreading parameter in this case.
#else   // __PORTABLE_PLATFORM__
  if (params.num_workers() > 1 || params.interleave_search() ||
      !params.subsolvers().empty() || params.test_feasibility_jump()) {
    SolveCpModelParallel(*new_cp_model_proto, model);
#endif  // __PORTABLE_PLATFORM__
  } else if (!model->GetOrCreate<TimeLimit>()->LimitReached()) {
    SOLVER_LOG(logger, "");
    SOLVER_LOG(logger, absl::StrFormat("Starting to load the model at %.2fs",
                                       wall_timer->Get()));
    shared_response_manager->SetUpdateGapIntegralOnEachChange(true);

    // We use a local_model to share statistic report mechanism with the
    // parallel case. When this model will be destroyed, we will collect some
    // stats that are used to debug/improve internal algorithm.
    //
    // TODO(user): Reuse a Subsolver to get the same display as for the
    // parallel case. Right now we don't have as much stats for single thread!
    Model local_model;
    local_model.Register<SolverLogger>(logger);
    local_model.Register<TimeLimit>(model->GetOrCreate<TimeLimit>());
    local_model.Register<SatParameters>(model->GetOrCreate<SatParameters>());
    local_model.Register<SharedStatistics>(
        model->GetOrCreate<SharedStatistics>());
    local_model.Register<SharedResponseManager>(shared_response_manager);

    LoadCpModel(*new_cp_model_proto, &local_model);

    SOLVER_LOG(logger, "");
    SOLVER_LOG(logger, absl::StrFormat("Starting sequential search at %.2fs",
                                       wall_timer->Get()));
    if (params.repair_hint()) {
      MinimizeL1DistanceWithHint(*new_cp_model_proto, &local_model);
    } else {
      QuickSolveWithHint(*new_cp_model_proto, &local_model);
    }
    SolveLoadedCpModel(*new_cp_model_proto, &local_model);

    // Display table data.
    if (logger->LoggingIsEnabled()) {
      logger->FlushPendingThrottledLogs(/*ignore_rates=*/true);
      SOLVER_LOG(logger, "");
      SharedStatTables tables;
      tables.AddLpStat("default", &local_model);
      tables.AddSearchStat("default", &local_model);
      tables.AddClausesStat("default", &local_model);
      tables.Display(logger);
    }

    // Export statistics.
    CpSolverResponse status_response;
    FillSolveStatsInResponse(&local_model, &status_response);
    shared_response_manager->AppendResponseToBeMerged(status_response);
  }

  // Extra logging if needed. Note that these are mainly activated on
  // --vmodule *some_file*=1 and are here for development.
  if (logger->LoggingIsEnabled()) {
    model->GetOrCreate<SharedStatistics>()->Log(logger);
  }

  return shared_response_manager->GetResponse();
}

CpSolverResponse Solve(const CpModelProto& model_proto) {
  Model model;
  return SolveCpModel(model_proto, &model);
}

CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const SatParameters& params) {
  Model model;
  model.Add(NewSatParameters(params));
  return SolveCpModel(model_proto, &model);
}

#if !defined(__PORTABLE_PLATFORM__)
CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const std::string& params) {
  Model model;
  model.Add(NewSatParameters(params));
  return SolveCpModel(model_proto, &model);
}
#endif  // !__PORTABLE_PLATFORM__

void LoadAndSolveCpModelForTest(const CpModelProto& model_proto, Model* model) {
  model->GetOrCreate<SharedResponseManager>()->InitializeObjective(model_proto);
  LoadCpModel(model_proto, model);
  SolveLoadedCpModel(model_proto, model);
}

}  // namespace sat
}  // namespace operations_research
