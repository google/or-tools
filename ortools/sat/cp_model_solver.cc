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

#include "ortools/sat/cp_model_solver.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/helpers.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/macros/buildenv.h"
#include "ortools/base/macros/os_support.h"
#include "ortools/base/options.h"
#include "ortools/base/timer.h"
#include "ortools/base/types.h"
#include "ortools/base/version.h"  // IWYU pragma: keep
#include "ortools/port/proto_utils.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/combine_solutions.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_copy.h"
#include "ortools/sat/cp_model_lns.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_postsolve.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_solver_helpers.h"
#include "ortools/sat/cp_model_solver_logging.h"
#include "ortools/sat/cp_model_symmetries.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/feasibility_jump.h"
#include "ortools/sat/feasibility_pump.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/parameters_validation.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/primary_variables.h"
#include "ortools/sat/routing_cuts.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_local_search.h"
#include "ortools/sat/shaving_solver.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/sat/work_assignment.h"
#include "ortools/util/logging.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/sigint.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/testing_utils.h"
#include "ortools/util/time_limit.h"

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
static_assert(operations_research::kTargetOsSupportsThreads);
#include "ortools/base/threadpool.h"
#else
static_assert(!operations_research::kTargetOsSupportsThreads);
#endif

ABSL_FLAG(
    bool, cp_model_export_model, false,
    "DEBUG ONLY. When set to true, SolveCpModel() will dump its input model "
    "protos in text format to 'FLAGS_cp_model_dump_prefix'{name}.pb.txt.");

ABSL_FLAG(bool, cp_model_dump_text_proto, true,
          "DEBUG ONLY, dump models in text proto instead of binary proto.");

ABSL_FLAG(
    bool, cp_model_dump_problematic_lns, false,
    "DEBUG ONLY. Similar to --cp_model_dump_submodels, but only dump fragment "
    "for which we got an issue while validating the postsolved solution. This "
    "allows to debug presolve issues without dumping all the models.");

ABSL_FLAG(bool, cp_model_dump_response, false,
          "DEBUG ONLY. If true, the final response of each solve will be "
          "dumped to 'FLAGS_cp_model_dump_prefix'response.pb.txt");

ABSL_FLAG(std::string, cp_model_params, "",
          "This is interpreted as a text SatParameters proto. The "
          "specified fields will override the normal ones for all solves.");

ABSL_FLAG(bool, cp_model_ignore_objective, false,
          "If true, ignore the objective.");
ABSL_FLAG(bool, cp_model_ignore_hints, false,
          "If true, ignore any supplied hints.");
ABSL_FLAG(bool, cp_model_use_hint_for_debug_only, false,
          "If true, ignore any supplied hints, but if the hint is valid and "
          "complete, validate that no buggy propagator make it infeasible.");
ABSL_FLAG(bool, cp_model_fingerprint_model, true, "Fingerprint the model.");

ABSL_FLAG(bool, cp_model_check_intermediate_solutions, false,
          "When true, all intermediate solutions found by the solver will be "
          "checked. This can be expensive, therefore it is off by default.");

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
    filename =
        absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix), name, ".bin");
    LOG(INFO) << "Dumping " << name << " binary proto to '" << filename << "'.";
  }
  CHECK(WriteModelProtoToFile(proto, filename));
}

IntegerValue ExprMin(const LinearExpressionProto& expr,
                     const CpModelProto& model) {
  IntegerValue result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const IntegerVariableProto& var_proto = model.variables(expr.vars(i));
    if (expr.coeffs(i) > 0) {
      result += expr.coeffs(i) * var_proto.domain(0);
    } else {
      result += expr.coeffs(i) * var_proto.domain(var_proto.domain_size() - 1);
    }
  }
  return result;
}

IntegerValue ExprMax(const LinearExpressionProto& expr,
                     const CpModelProto& model) {
  IntegerValue result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const IntegerVariableProto& var_proto = model.variables(expr.vars(i));
    if (expr.coeffs(i) > 0) {
      result += expr.coeffs(i) * var_proto.domain(var_proto.domain_size() - 1);
    } else {
      result += expr.coeffs(i) * var_proto.domain(0);
    }
  }
  return result;
}

void DumpNoOverlap2dProblem(const ConstraintProto& ct,
                            const CpModelProto& model_proto) {
  std::vector<RectangleInRange> non_fixed_boxes;
  std::vector<Rectangle> fixed_boxes;
  for (int i = 0; i < ct.no_overlap_2d().x_intervals_size(); ++i) {
    const int x_interval = ct.no_overlap_2d().x_intervals(i);
    const int y_interval = ct.no_overlap_2d().y_intervals(i);

    const ConstraintProto& x_ct = model_proto.constraints(x_interval);
    const ConstraintProto& y_ct = model_proto.constraints(y_interval);

    RectangleInRange box = {
        .box_index = i,
        .bounding_area =
            Rectangle{
                .x_min = ExprMin(x_ct.interval().start(), model_proto),
                .x_max = ExprMax(x_ct.interval().end(), model_proto),
                .y_min = ExprMin(y_ct.interval().start(), model_proto),
                .y_max = ExprMax(y_ct.interval().end(), model_proto),
            },

        .x_size = ExprMin(x_ct.interval().size(), model_proto),
        .y_size = ExprMin(y_ct.interval().size(), model_proto)};
    if (box.x_size == box.bounding_area.SizeX() &&
        box.y_size == box.bounding_area.SizeY()) {
      fixed_boxes.push_back(box.bounding_area);
    } else {
      non_fixed_boxes.push_back(box);
    }
  }
  VLOG(2) << "NoOverlap2D with " << fixed_boxes.size() << " fixed boxes and "
          << non_fixed_boxes.size() << " non-fixed boxes.";

  Rectangle bounding_box = non_fixed_boxes.front().bounding_area;
  for (const RectangleInRange& r : non_fixed_boxes) {
    bounding_box.GrowToInclude(r.bounding_area);
  }
  VLOG(3) << "Fixed boxes: " << RenderDot(bounding_box, fixed_boxes);
  std::vector<Rectangle> non_fixed_boxes_to_render;
  for (const auto& r : non_fixed_boxes) {
    non_fixed_boxes_to_render.push_back(r.bounding_area);
  }
  VLOG(3) << "Non-fixed boxes: "
          << RenderDot(bounding_box, non_fixed_boxes_to_render);
  VLOG(3) << "BB: " << bounding_box
          << " non-fixed boxes: " << absl::StrJoin(non_fixed_boxes, ", ");
  VLOG(3) << "BB size: " << bounding_box.SizeX() << "x" << bounding_box.SizeY()
          << " non-fixed boxes sizes: "
          << absl::StrJoin(non_fixed_boxes, ", ",
                           [](std::string* out, const RectangleInRange& r) {
                             absl::StrAppend(out, r.bounding_area.SizeX(), "x",
                                             r.bounding_area.SizeY());
                           });
  std::vector<Rectangle> sizes_to_render;
  IntegerValue x = bounding_box.x_min;
  IntegerValue y = 0;
  for (const auto& r : non_fixed_boxes) {
    sizes_to_render.push_back(Rectangle{
        .x_min = x, .x_max = x + r.x_size, .y_min = y, .y_max = y + r.y_size});
    x += r.x_size;
    if (x > bounding_box.x_max) {
      x = 0;
      y += r.y_size;
    }
  }
  VLOG(3) << "Sizes: " << RenderDot(bounding_box, sizes_to_render);
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
  const VariableRelationships relationships =
      ComputeVariableRelationships(model_proto);

  for (const ConstraintProto& ct : model_proto.constraints()) {
    // We split the linear constraints into 3 buckets has it gives more insight
    // on the type of problem we are facing.
    char const* name;
    if (ct.constraint_case() == ConstraintProto::kLinear) {
      if (ct.linear().vars_size() == 0) name = "kLinear0";
      if (ct.linear().vars_size() == 1) name = "kLinear1";
      if (ct.linear().vars_size() == 2) name = "kLinear2";
      if (ct.linear().vars_size() == 3) name = "kLinear3";
      if (ct.linear().vars_size() > 3) name = "kLinearN";
    } else if (ct.constraint_case() == ConstraintProto::kBoolAnd &&
               ct.enforcement_literal().size() > 1) {
      // BoolAnd of the form "n literals => m literals" with n > 1 are just a
      // compact way to encode m clauses of size n + 1. We report them
      // separately as they are not handled in the same way internally.
      name = "kBoolAndClauses";
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
      if (VLOG_IS_ON(2)) {
        DumpNoOverlap2dProblem(ct, model_proto);
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
    absl::StrAppend(&result, "Search strategy: on ",
                    strategy.exprs().size() + strategy.variables().size(),
                    " variables, ",
                    DecisionStrategyProto::VariableSelectionStrategy_Name(
                        strategy.variable_selection_strategy()),
                    ", ",
                    DecisionStrategyProto::DomainReductionStrategy_Name(
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
                    objective_string, " (",
                    FormatCounter(model_proto.variables_size() -
                                  relationships.secondary_variables.size()),
                    " primary variables)\n");
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
    int64_t min = kint64max;
    int64_t max = kint64min;
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
        absl::StrCat("  - ", FormatCounter(num_constants), " constants in {",
                     absl::StrJoin(constant_values, ","), "} \n");
    absl::StrAppend(&result, Summarize(temp));
  }

  absl::StrAppend(&result, "#Constraints: ",
                  FormatCounter(model_proto.constraints().size()), "\n");

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
      absl::StrAppend(&constraints.back(),
                      " (#fixed: ", FormatCounter(num_fixed_intervals), ")");
    } else if (name == "kNoOverlap2D") {
      absl::StrAppend(&constraints.back(), " (#rectangles: ",
                      FormatCounter(no_overlap_2d_num_rectangles));
      if (no_overlap_2d_num_optional_rectangles > 0) {
        absl::StrAppend(&constraints.back(), ", #optional: ",
                        FormatCounter(no_overlap_2d_num_optional_rectangles));
      }
      if (no_overlap_2d_num_linear_areas > 0) {
        absl::StrAppend(&constraints.back(), ", #linear_areas: ",
                        FormatCounter(no_overlap_2d_num_linear_areas));
      }
      if (no_overlap_2d_num_quadratic_areas > 0) {
        absl::StrAppend(&constraints.back(), ", #quadratic_areas: ",
                        FormatCounter(no_overlap_2d_num_quadratic_areas));
      }
      if (no_overlap_2d_num_fixed_rectangles > 0) {
        absl::StrAppend(&constraints.back(), ", #fixed_rectangles: ",
                        FormatCounter(no_overlap_2d_num_fixed_rectangles));
      }
      absl::StrAppend(&constraints.back(), ")");
    } else if (name == "kCumulative") {
      absl::StrAppend(&constraints.back(), " (#intervals: ",
                      FormatCounter(cumulative_num_intervals));
      if (cumulative_num_optional_intervals > 0) {
        absl::StrAppend(&constraints.back(), ", #optional: ",
                        FormatCounter(cumulative_num_optional_intervals));
      }
      if (cumulative_num_variable_sizes > 0) {
        absl::StrAppend(&constraints.back(), ", #variable_sizes: ",
                        FormatCounter(cumulative_num_variable_sizes));
      }
      if (cumulative_num_variable_demands > 0) {
        absl::StrAppend(&constraints.back(), ", #variable_demands: ",
                        cumulative_num_variable_demands);
      }
      if (cumulative_num_fixed_intervals > 0) {
        absl::StrAppend(&constraints.back(), ", #fixed_intervals: ",
                        FormatCounter(cumulative_num_fixed_intervals));
      }
      absl::StrAppend(&constraints.back(), ")");
    } else if (name == "kNoOverlap") {
      absl::StrAppend(&constraints.back(), " (#intervals: ",
                      FormatCounter(no_overlap_num_intervals));
      if (no_overlap_num_optional_intervals > 0) {
        absl::StrAppend(&constraints.back(), ", #optional: ",
                        FormatCounter(no_overlap_num_optional_intervals));
      }
      if (no_overlap_num_variable_sizes > 0) {
        absl::StrAppend(&constraints.back(), ", #variable_sizes: ",
                        FormatCounter(no_overlap_num_variable_sizes));
      }
      if (no_overlap_num_fixed_intervals > 0) {
        absl::StrAppend(&constraints.back(), ", #fixed_intervals: ",
                        FormatCounter(no_overlap_num_fixed_intervals));
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
  absl::StrAppend(&result,
                  "\nstatus: ", CpSolverStatus_Name(response.status()));

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

void LogSubsolverNames(absl::Span<const std::unique_ptr<SubSolver>> subsolvers,
                       absl::Span<const std::string> ignored,
                       SolverLogger* logger) {
  if (!logger->LoggingIsEnabled()) return;

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

  // TODO(user): We might not want to sort the subsolver by name to keep our
  // ordered list by importance? not sure.
  auto display_subsolver_list = [logger](absl::Span<const std::string> names,
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
          absl::StrJoin(counted_names.begin(), counted_names.end(), ", "), "]");
    }
  };

  display_subsolver_list(full_problem_solver_names, "full problem subsolver");
  display_subsolver_list(first_solution_solver_names,
                         "first solution subsolver");
  display_subsolver_list(incomplete_solver_names, "interleaved subsolver");
  display_subsolver_list(helper_solver_names, "helper subsolver");
  if (!ignored.empty()) {
    display_subsolver_list(ignored, "ignored subsolver");
  }

  SOLVER_LOG(logger, "");
}

void LaunchSubsolvers(Model* global_model, SharedClasses* shared,
                      std::vector<std::unique_ptr<SubSolver>>& subsolvers,
                      absl::Span<const std::string> ignored) {
  // Initial logging.
  SOLVER_LOG(shared->logger, "");
  SatParameters& params = *global_model->GetOrCreate<SatParameters>();
  if (params.interleave_search()) {
    SOLVER_LOG(shared->logger,
               absl::StrFormat("Starting deterministic search at %.2fs with "
                               "%i workers and batch size of %d.",
                               shared->wall_timer->Get(), params.num_workers(),
                               params.interleave_batch_size()));
  } else {
    SOLVER_LOG(
        shared->logger,
        absl::StrFormat("Starting search at %.2fs with %i workers.",
                        shared->wall_timer->Get(), params.num_workers()));
  }
  LogSubsolverNames(subsolvers, ignored, shared->logger);

  // Launch the main search loop.
  if (params.interleave_search()) {
    int batch_size = params.interleave_batch_size();
    if (batch_size == 0) {
      batch_size = params.num_workers() == 1 ? 1 : params.num_workers() * 3;
      SOLVER_LOG(
          shared->logger,
          "Setting number of tasks in each batch of interleaved search to ",
          batch_size);
    }
    DeterministicLoop(subsolvers, params.num_workers(), batch_size,
                      params.max_num_deterministic_batches());
  } else {
    NonDeterministicLoop(subsolvers, params.num_workers(), shared->time_limit);
  }

  // We need to delete the subsolvers in order to fill the stat tables. Note
  // that first solution should already be deleted. We delete manually as
  // windows release vectors in the opposite order.
  for (int i = 0; i < subsolvers.size(); ++i) {
    subsolvers[i].reset();
  }

  if (shared->shared_tree_manager != nullptr) {
    shared->shared_tree_manager->CloseLratProof();
  }
  if (shared->response->ProblemIsSolved() &&
      !shared->response->HasFeasibleSolution()) {
    LratMerger(global_model)
        .Merge(shared->lrat_proof_status->GetProofFilenames());
  }

  shared->LogFinalStatistics();
}

bool VarIsFixed(const CpModelProto& model_proto, int i) {
  return model_proto.variables(i).domain_size() == 2 &&
         model_proto.variables(i).domain(0) ==
             model_proto.variables(i).domain(1);
}

// Note that we restrict the objective to be <= so that the hint is still
// feasible. Alternatively, we could look for < hint value if we only want
// better solution.
bool RestrictObjectiveUsingHint(CpModelProto* model_proto) {
  if (!model_proto->has_objective()) return true;
  if (!model_proto->has_solution_hint()) return true;

  // We will abort if the hint is not complete, ignoring fixed variables.
  const int num_vars = model_proto->variables().size();
  int num_filled = 0;
  std::vector<bool> filled(num_vars, false);
  std::vector<int64_t> solution(num_vars, 0);
  for (int var = 0; var < num_vars; ++var) {
    if (VarIsFixed(*model_proto, var)) {
      solution[var] = model_proto->variables(var).domain(0);
      filled[var] = true;
      ++num_filled;
    }
  }
  const auto& hint_proto = model_proto->solution_hint();
  const int num_hinted = hint_proto.vars().size();
  for (int i = 0; i < num_hinted; ++i) {
    const int var = hint_proto.vars(i);
    CHECK(RefIsPositive(var));
    if (filled[var]) continue;

    const int64_t value = hint_proto.values(i);
    solution[var] = value;
    filled[var] = true;
    ++num_filled;
  }
  if (num_filled != num_vars) return true;

  const int64_t obj_upper_bound =
      ComputeInnerObjective(model_proto->objective(), solution);
  const Domain restriction = Domain(kint64min, obj_upper_bound);

  if (restriction.IsEmpty()) return false;

  if (model_proto->objective().domain().empty()) {
    FillDomainInProto(restriction, model_proto->mutable_objective());
  } else {
    const Domain new_domain = ReadDomainFromProto(model_proto->objective())
                                  .IntersectionWith(restriction);
    if (new_domain.IsEmpty()) return false;
    FillDomainInProto(new_domain, model_proto->mutable_objective());
  }
  return true;
}

// Returns true iff the partial variable assignment is complete (ignoring fixed
// variables) and feasible. If so, sets the full assignment in `solution`.
bool PartialVariableAssignmentIsCompleteAndFeasible(
    const CpModelProto& model_proto, const PartialVariableAssignment& hint,
    std::vector<int64_t>& solution, SolverLogger* logger = nullptr) {
  int num_active_variables = 0;
  int num_hinted_variables = 0;
  for (int var = 0; var < model_proto.variables_size(); ++var) {
    if (VarIsFixed(model_proto, var)) continue;
    ++num_active_variables;
  }

  for (int i = 0; i < hint.vars_size(); ++i) {
    const int ref = hint.vars(i);
    if (VarIsFixed(model_proto, PositiveRef(ref))) continue;
    ++num_hinted_variables;
  }
  CHECK_LE(num_hinted_variables, num_active_variables);

  if (num_active_variables != num_hinted_variables) {
    if (logger != nullptr) {
      SOLVER_LOG(
          logger, "The solution hint is incomplete: ", num_hinted_variables,
          " out of ", num_active_variables, " non fixed variables hinted.");
    }
    return false;
  }

  solution.assign(model_proto.variables_size(), 0);
  // Pre-assign from fixed domains.
  for (int var = 0; var < model_proto.variables_size(); ++var) {
    if (VarIsFixed(model_proto, var)) {
      solution[var] = model_proto.variables(var).domain(0);
    }
  }

  for (int i = 0; i < hint.vars_size(); ++i) {
    const int ref = hint.vars(i);
    const int var = PositiveRef(ref);
    const int64_t value = hint.values(i);
    const int64_t hinted_value = RefIsPositive(ref) ? value : -value;
    const Domain domain = ReadDomainFromProto(model_proto.variables(var));
    if (!domain.Contains(hinted_value)) {
      if (logger != nullptr) {
        SOLVER_LOG(
            logger,
            "The solution hint is complete but it contains values outside "
            "of the domain of the variables.");
      }
      return false;
    }
    solution[var] = hinted_value;
  }

  const bool is_feasible = SolutionIsFeasible(model_proto, solution);
  bool breaks_assumptions = false;
  if (is_feasible) {
    for (const int literal_ref : model_proto.assumptions()) {
      if (solution[PositiveRef(literal_ref)] !=
          (RefIsPositive(literal_ref) ? 1 : 0)) {
        breaks_assumptions = true;
        break;
      }
    }
  }
  if (is_feasible && breaks_assumptions) {
    if (logger != nullptr) {
      SOLVER_LOG(
          logger,
          "The solution hint is complete and feasible, but it breaks the "
          "assumptions of the model.");
    }
    return false;
  }
  return is_feasible;
}

// Returns true iff there is a hint, and (ignoring fixed variables) if it is
// complete and feasible.
bool SolutionHintIsCompleteAndFeasible(
    const CpModelProto& model_proto, SolverLogger* logger = nullptr,
    SharedResponseManager* manager = nullptr) {
  if (!model_proto.has_solution_hint() && model_proto.variables_size() > 0) {
    return false;
  }
  std::vector<int64_t> solution;
  if (PartialVariableAssignmentIsCompleteAndFeasible(
          model_proto, model_proto.solution_hint(), solution, logger)) {
    if (manager != nullptr && !solution.empty()) {
      // Add it to the pool right away! Note that we already have a log in this
      // case, so we don't log anything more.
      manager->NewSolution(solution, "complete_hint");
    } else if (logger != nullptr) {
      std::string message = "The solution hint is complete and is feasible.";
      if (model_proto.has_objective()) {
        absl::StrAppend(
            &message, " Its objective value is ",
            absl::StrFormat(
                "%.9g",
                ScaleObjectiveValue(
                    model_proto.objective(),
                    ComputeInnerObjective(model_proto.objective(), solution))),
            ".");
      }
      SOLVER_LOG(logger, message);
    }
    return true;
  } else {
    // TODO(user): Change the code to make the solution checker more
    // informative by returning a message instead of just VLOGing it.
    if (logger != nullptr) {
      SOLVER_LOG(logger,
                 "The solution hint is incomplete or is infeasible! We "
                 "will try to repair it.");
    }
    return false;
  }
}

// Encapsulate a full CP-SAT solve without presolve in the SubSolver API.
class FullProblemSolver : public SubSolver {
 public:
  FullProblemSolver(absl::string_view name,
                    const SatParameters& local_parameters, bool split_in_chunks,
                    SharedClasses* shared, bool stop_at_first_solution = false)
      : SubSolver(name, stop_at_first_solution ? FIRST_SOLUTION : FULL_PROBLEM),
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

    // TODO(user): For now we do not count LNS statistics. We could easily
    // by registering the SharedStatistics class with LNS local model.
    shared_->RegisterSharedClassesInLocalModel(&local_model_);

    std::unique_ptr<LratProofHandler> lrat_proof_handler =
        LratProofHandler::MaybeCreate(&local_model_);
    if (lrat_proof_handler != nullptr) {
      local_model_.Register<LratProofHandler>(lrat_proof_handler.get());
      local_model_.TakeOwnership(lrat_proof_handler.release());
    }

    // Setup the local logger, in multi-thread log_search_progress should be
    // false by default, but we might turn it on for debugging. It is on by
    // default in single-thread mode.
    auto* logger = local_model_.GetOrCreate<SolverLogger>();
    logger->EnableLogging(local_parameters.log_search_progress());
    logger->SetLogToStdOut(local_parameters.log_to_stdout());
  }

  ~FullProblemSolver() override {
    CpSolverResponse response;
    shared_->response->FillSolveStatsInResponse(&local_model_, &response);
    shared_->response->AppendResponseToBeMerged(response);
    shared_->stat_tables->AddTimingStat(*this);
    shared_->stat_tables->AddLpStat(name(), &local_model_);
    shared_->stat_tables->AddSearchStat(name(), &local_model_);
    shared_->stat_tables->AddClausesStat(name(), &local_model_);
    LratProofHandler* lrat_proof_handler =
        local_model_.Mutable<LratProofHandler>();
    if (lrat_proof_handler != nullptr) {
      lrat_proof_handler->Close(
          local_model_.GetOrCreate<SatSolver>()->ModelIsUnsat());
    }
  }

  bool IsDone() override {
    // On large problem, deletion can take a while, so is is better to do it
    // while waiting for the slower worker to finish.
    if (shared_->SearchIsDone()) return true;

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

    absl::MutexLock mutex_lock(mutex_);
    if (previous_task_is_completed_) {
      if (solving_first_chunk_) return true;
      if (split_in_chunks_) return true;
    }
    return false;
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) override {
    {
      absl::MutexLock mutex_lock(mutex_);
      previous_task_is_completed_ = false;
    }
    return [this]() {
      auto* time_limit = local_model_.GetOrCreate<TimeLimit>();
      if (solving_first_chunk_) {
        const double init_dtime = time_limit->GetElapsedDeterministicTime();
        LoadCpModel(shared_->model_proto, &local_model_);

        // Level zero variable bounds sharing. It is important to register
        // that after the probing that takes place in LoadCpModel() otherwise
        // we will have a mutex contention issue when all the thread probes
        // at the same time.
        if (shared_->bounds != nullptr) {
          RegisterVariableBoundsLevelZeroExport(
              shared_->model_proto, shared_->bounds.get(), &local_model_);
          RegisterVariableBoundsLevelZeroImport(
              shared_->model_proto, shared_->bounds.get(), &local_model_);
        }

        if (shared_->linear2_bounds != nullptr) {
          RegisterLinear2BoundsImport(shared_->linear2_bounds.get(),
                                      &local_model_);
        }

        // Note that this is done after the loading, so we will never export
        // problem clauses.
        if (shared_->clauses != nullptr) {
          const int id = shared_->clauses->RegisterNewId(
              local_model_.Name(),
              /*may_terminate_early=*/stop_at_first_solution_ &&
                  local_model_.GetOrCreate<CpModelProto>()->has_objective());

          RegisterClausesLevelZeroImport(id, shared_->clauses.get(),
                                         &local_model_);
          RegisterClausesExport(id, shared_->clauses.get(), &local_model_);

          // Hack to export all equivalences found so far.
          //
          // TODO(user): We probably want to do probing "AFTER" this. But then
          // we might not want to export all binary clauses found by probing.
          local_model_.GetOrCreate<BinaryImplicationGraph>()
              ->ExportAllEquivalences();
        }

        auto* logger = local_model_.GetOrCreate<SolverLogger>();
        SOLVER_LOG(logger, "");
        SOLVER_LOG(logger, absl::StrFormat(
                               "Starting subsolver \'%s\' hint search at %.2fs",
                               name(), shared_->wall_timer->Get()));

        if (local_model_.GetOrCreate<SatParameters>()->repair_hint()) {
          MinimizeL1DistanceWithHint(shared_->model_proto, &local_model_);
        } else {
          QuickSolveWithHint(shared_->model_proto, &local_model_);
        }

        SOLVER_LOG(logger,
                   absl::StrFormat("Starting subsolver \'%s\' search at %.2fs",
                                   name(), shared_->wall_timer->Get()));

        // No need for mutex since we only run one task at the time.
        solving_first_chunk_ = false;

        // Make sure we count the loading/hint dtime.
        absl::MutexLock mutex_lock(mutex_);
        dtime_since_last_sync_ +=
            time_limit->GetElapsedDeterministicTime() - init_dtime;

        // Abort first chunk and allow to schedule the next.
        if (split_in_chunks_) {
          previous_task_is_completed_ = true;
          return;
        }
      }

      if (split_in_chunks_) {
        // Configure time limit for chunk solving. Note that we do not want
        // to do that for the hint search for now.
        auto* params = local_model_.GetOrCreate<SatParameters>();
        params->set_max_deterministic_time(1);
        time_limit->ResetLimitFromParameters(*params);
        shared_->time_limit->UpdateLocalLimit(time_limit);
      }

      const double saved_dtime = time_limit->GetElapsedDeterministicTime();
      SolveLoadedCpModel(shared_->model_proto, &local_model_);

      absl::MutexLock mutex_lock(mutex_);
      previous_task_is_completed_ = true;
      dtime_since_last_sync_ +=
          time_limit->GetElapsedDeterministicTime() - saved_dtime;
    };
  }

  // TODO(user): A few of the information sharing we do between threads does not
  // happen here (bound sharing, RINS neighborhood, objective). Fix that so we
  // can have a deterministic parallel mode.
  void Synchronize() override {
    absl::MutexLock mutex_lock(mutex_);
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

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
static_assert(operations_research::kTargetOsSupportsThreads);

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
    shared_->RegisterSharedClassesInLocalModel(local_model_.get());
  }

  ~FeasibilityPumpSolver() override {
    shared_->stat_tables->AddTimingStat(*this);
  }

  bool IsDone() override { return shared_->SearchIsDone(); }

  bool TaskIsAvailable() override {
    if (shared_->SearchIsDone()) return false;
    absl::MutexLock mutex_lock(mutex_);
    return previous_task_is_completed_;
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) override {
    {
      absl::MutexLock mutex_lock(mutex_);
      previous_task_is_completed_ = false;
    }
    return [this]() {
      {
        absl::MutexLock mutex_lock(mutex_);
        if (solving_first_chunk_) {
          LoadFeasibilityPump(shared_->model_proto, local_model_.get());
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
        absl::MutexLock mutex_lock(mutex_);
        dtime_since_last_sync_ +=
            time_limit->GetElapsedDeterministicTime() - saved_dtime;
      }

      // Abort if the problem is solved.
      if (shared_->SearchIsDone()) {
        return;
      }

      absl::MutexLock mutex_lock(mutex_);
      previous_task_is_completed_ = true;
    };
  }

  void Synchronize() override {
    absl::MutexLock mutex_lock(mutex_);
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
            const SatParameters& lns_parameters_base,
            const SatParameters& lns_parameters_stalling,
            NeighborhoodGeneratorHelper* helper, SharedClasses* shared)
      : SubSolver(generator->name(), INCOMPLETE),
        generator_(std::move(generator)),
        helper_(helper),
        lns_parameters_base_(lns_parameters_base),
        lns_parameters_stalling_(lns_parameters_stalling),
        shared_(shared) {}

  ~LnsSolver() override {
    shared_->stat_tables->AddTimingStat(*this);
    shared_->stat_tables->AddLnsStat(
        name(),
        /*num_fully_solved_calls=*/generator_->num_fully_solved_calls(),
        /*num_calls=*/generator_->num_calls(),
        /*num_improving_calls=*/generator_->num_improving_calls(),
        /*difficulty=*/generator_->difficulty(),
        /*deterministic_limit=*/generator_->deterministic_limit());
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
      std::seed_seq seed{low, high, lns_parameters_base_.random_seed()};
      random_engine_t random(seed);

      NeighborhoodGenerator::SolveData data;
      data.task_id = task_id;
      data.difficulty = generator_->difficulty();
      data.deterministic_limit = generator_->deterministic_limit();
      data.initial_best_objective =
          shared_->response->GetBestSolutionObjective();

      // Choose a base solution for this neighborhood.
      const auto base_solution =
          shared_->response->SolutionPool().GetSolutionToImprove(random);
      CpSolverResponse base_response;
      if (base_solution != nullptr) {
        base_response.set_status(CpSolverStatus::FEASIBLE);
        base_response.mutable_solution()->Assign(
            base_solution->variable_values.begin(),
            base_solution->variable_values.end());

        // Note: We assume that the solution rank is the solution internal
        // objective.
        data.base_objective = base_solution->rank;
      } else {
        base_response.set_status(CpSolverStatus::UNKNOWN);

        // If we do not have a solution, we use the current objective upper
        // bound so that our code that compute an "objective" improvement
        // works.
        data.base_objective = data.initial_best_objective;
      }

      Neighborhood neighborhood =
          generator_->Generate(base_response, data, random);

      if (!neighborhood.is_generated) return;

      SatParameters local_params;

      // TODO(user): This could be a good candidate for bandits.
      const int64_t stall = generator_->num_consecutive_non_improving_calls();
      const int search_index = stall < 10 ? 0 : task_id % 2;
      switch (search_index) {
        case 0:
          local_params = lns_parameters_base_;
          break;
        default:
          local_params = lns_parameters_stalling_;
          break;
      }
      const std::string_view search_info =
          absl::StripPrefix(absl::string_view(local_params.name()), "lns_");
      local_params.set_max_deterministic_time(data.deterministic_limit);

      std::string source_info =
          neighborhood.source_info.empty() ? name() : neighborhood.source_info;
      const int64_t num_calls = std::max(int64_t{1}, generator_->num_calls());
      const double fully_solved_proportion =
          static_cast<double>(generator_->num_fully_solved_calls()) /
          static_cast<double>(num_calls);
      const std::string lns_info = absl::StrFormat(
          "%s (d=%0.2e s=%i t=%0.2f p=%0.2f stall=%d h=%s)", source_info,
          data.difficulty, task_id, data.deterministic_limit,
          fully_solved_proportion, stall, search_info);

      Model local_model(lns_info);
      *(local_model.GetOrCreate<SatParameters>()) = local_params;
      TimeLimit* local_time_limit = local_model.GetOrCreate<TimeLimit>();
      local_time_limit->ResetLimitFromParameters(local_params);
      shared_->time_limit->UpdateLocalLimit(local_time_limit);
      // Don't let our LNS model to stop the main solve.
      local_model.GetOrCreate<ModelSharedTimeLimit>()->DisableStop();

      // Presolve and solve the LNS fragment.
      size_t buffer_size;
      {
        absl::MutexLock l(next_arena_size_mutex_);
        buffer_size = next_arena_size_;
      }
      google::protobuf::Arena arena(
          google::protobuf::ArenaOptions({.start_block_size = buffer_size}));
      CpModelProto& lns_fragment =
          *google::protobuf::Arena::Create<CpModelProto>(&arena);
      CpModelProto& mapping_proto =
          *google::protobuf::Arena::Create<CpModelProto>(&arena);

      std::vector<int> variable_mapping;
      std::vector<int64_t> fixed_values;
      {
        bool use_hint = true;
        if (generator_->num_consecutive_non_improving_calls() > 10 &&
            absl::Bernoulli(random, 0.5)) {
          // If we seem to be stalling, lets try to solve without the hint in
          // order to diversify our solution pool. Otherwise non-improving
          // neighborhood will just return the base solution always.
          use_hint = false;
        }
        if (neighborhood.is_simple &&
            neighborhood.num_relaxed_variables_in_objective == 0) {
          // If we didn't relax the objective, there can be no improving
          // solution. However, we might have some diversity if they are
          // multiple feasible solutions.
          //
          // TODO(user): How can we tweak the search to favor diversity.
          if (generator_->num_consecutive_non_improving_calls() > 10) {
            // We have been staling, try to find diverse solution?
            use_hint = false;
          } else {
            // Just regenerate.
            // Note that we do not change the difficulty.
            return;
          }
        }

        // TODO(user): the mapping removes fixed variables but the model
        // copy can fix new ones. Should we update the mapping and do a new
        // copy, and so on until fix point?
        if (!GenerateMapping(neighborhood.delta, variable_mapping,
                             fixed_values)) {
          return;
        }
        ModelCopy copier(&lns_fragment, &local_model, variable_mapping);
        if (!copier.ImportVariables(neighborhood.delta)) return;

        if (use_hint) {
          if (neighborhood.delta.has_solution_hint()) {
            copier.ImportSolutionHint(neighborhood.delta);
          } else {
            copier.ImportSolutionHint(helper_->ModelProto());
          }
        }

        // Copy and simplify the constraints from the initial model.
        if (!copier.ImportAndSimplifyConstraints(helper_->ModelProto())) {
          return;
        }

        // Copy and simplify the constraints from the delta model.
        if (!neighborhood.delta.constraints().empty() &&
            !copier.ImportAndSimplifyConstraints(neighborhood.delta)) {
          return;
        }

        // Copy the rest of the model, except symmetries (we don't want to use
        // the symmetry of the main problem in the LNS presolved problem).
        if (!copier.ImportEverythingExceptVariablesConstraintsAndHint(
                helper_->ModelProto(), /*copy_symmetry=*/false)) {
          return;
        }

        if (!copier.FinishCopy(neighborhood.delta)) return;
      }

      lns_fragment.set_name(absl::StrCat("lns_", task_id, "_", source_info));

      bool hint_feasible_before_presolve = false;
      if (lns_parameters_base_.debug_crash_if_presolve_breaks_hint()) {
        hint_feasible_before_presolve =
            SolutionHintIsCompleteAndFeasible(lns_fragment, /*logger=*/nullptr);
      }

      // If we use a hint, we will restrict the objective to be <= to the one
      // of the hint. This is helpful on some model where doing so can cause
      // the presolve to restrict the domain of many variables. Note that the
      // hint will still be feasible as we use <= and not <.
      if (!RestrictObjectiveUsingHint(&lns_fragment)) {
        return;
      }

      CpModelProto debug_copy;
      if (absl::GetFlag(FLAGS_cp_model_dump_problematic_lns)) {
        // We need to make a copy because the presolve is destructive.
        // It is why we do not do that by default.
        debug_copy = lns_fragment;
      }

      if (absl::GetFlag(FLAGS_cp_model_dump_submodels)) {
        // TODO(user): export the delta too if needed.
        const std::string lns_name =
            absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                         lns_fragment.name(), ".pb.txt");
        LOG(INFO) << "Dumping LNS model to '" << lns_name << "'.";
        CHECK(WriteModelProtoToFile(lns_fragment, lns_name));
      }

      DCHECK_EQ(ValidateCpModel(lns_fragment), "");

      const int num_vars_before_presolve = lns_fragment.variables_size();
      std::vector<int> postsolve_mapping;

      auto context = std::make_unique<PresolveContext>(
          &local_model, &lns_fragment, &mapping_proto);
      const CpSolverStatus presolve_status =
          PresolveCpModel(context.get(), &postsolve_mapping);

      // It is important to stop here to avoid using a model for which the
      // presolve was interrupted in the middle.
      if (local_time_limit->LimitReached()) return;

      // Release the context.
      context.reset(nullptr);
      neighborhood.delta.Clear();

      if (lns_parameters_base_.debug_crash_if_presolve_breaks_hint() &&
          hint_feasible_before_presolve &&
          !SolutionHintIsCompleteAndFeasible(lns_fragment,
                                             /*logger=*/nullptr)) {
        LOG(FATAL) << "Presolve broke a feasible LNS hint. The model name is '"
                   << lns_fragment.name()
                   << "' (use the --cp_model_dump_submodels flag to dump it).";
      }

      // TODO(user): Depending on the problem, we should probably use the
      // parameters that work best (core, linearization_level, etc...) or
      // maybe we can just randomize them like for the base solution used.
      auto* local_response_manager =
          local_model.GetOrCreate<SharedResponseManager>();
      local_response_manager->InitializeObjective(lns_fragment);
      local_response_manager->SetSynchronizationMode(true);

      CpSolverResponse local_response;
      if (presolve_status == CpSolverStatus::UNKNOWN) {
        // Sometimes when presolve is aborted in the middle, we don't want to
        // load the model as it might fail some DCHECK.
        if (local_time_limit->LimitReached()) return;

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
      std::vector<int64_t> solution_values;

      data.status = local_response.status();
      // TODO(user): we actually do not need to postsolve if the solution is
      // not going to be used...
      if (data.status == CpSolverStatus::OPTIMAL ||
          data.status == CpSolverStatus::FEASIBLE) {
        std::vector<int64_t> local_solution_values(
            local_response.solution().begin(), local_response.solution().end());
        PostsolveResponseWrapper(local_params, num_vars_before_presolve,
                                 mapping_proto, postsolve_mapping,
                                 &local_solution_values);
        // Map the solution back to the original variables.
        const int num_vars = helper_->ModelProto().variables().size();
        solution_values.reserve(num_vars);
        for (int i = 0; i < num_vars; ++i) {
          const int mapped_ref =
              variable_mapping.empty() ? i : variable_mapping[i];
          if (mapped_ref != kNoVariableMapping) {
            int64_t value = local_solution_values[PositiveRef(mapped_ref)];
            if (RefIsPositive(mapped_ref)) {
              solution_values.push_back(value);
            } else {
              // Only Boolean variables can be mapped to a negative ref.
              DCHECK(value == 0 || value == 1);
              solution_values.push_back(1 - value);
            }
          } else {
            DCHECK_NE(fixed_values[i], kint64min);
            solution_values.push_back(fixed_values[i]);
          }
        }
        local_response.mutable_solution()->Assign(solution_values.begin(),
                                                  solution_values.end());
      } else {
        CHECK(local_response.solution().empty());
      }

      data.deterministic_time +=
          local_time_limit->GetElapsedDeterministicTime();

      bool new_solution = false;
      bool display_lns_info = VLOG_IS_ON(2);
      if (!local_response.solution().empty()) {
        // A solution that does not pass our validator indicates a bug. We
        // abort and dump the problematic model to facilitate debugging.
        //
        // TODO(user): In a production environment, we should probably just
        // ignore this fragment and continue.
        const bool feasible =
            SolutionIsFeasible(shared_->model_proto, solution_values);
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
        // instance spot5_1401.fzn. Be smarter about that. We use
        // !VariablesTouchSymmetries() which is a bit better. Maybe we can use
        // VariablesSplitSymmetries() but that currently require more work and
        // in general, we would need to disable the symmetry that we fix here.
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
            !solution_values.empty() && neighborhood.is_simple &&
            shared_->bounds != nullptr &&
            !neighborhood.variables_that_can_be_fixed_to_local_optimum
                 .empty() &&
            !helper_->VariablesTouchSymmetries(
                neighborhood.variables_that_can_be_fixed_to_local_optimum)) {
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
              shared_->model_proto.objective(), solution_values));
        }

        // Report any feasible solution we have. Optimization: We don't do that
        // if we just recovered the base solution.
        if (data.status == CpSolverStatus::OPTIMAL ||
            data.status == CpSolverStatus::FEASIBLE) {
          if (absl::MakeSpan(solution_values) !=
              absl::MakeSpan(base_response.solution())) {
            new_solution = true;
            PushAndMaybeCombineSolution(shared_->response, shared_->model_proto,
                                        solution_values, solution_info,
                                        base_solution);
          }
        }
        if (!neighborhood.is_reduced &&
            (data.status == CpSolverStatus::OPTIMAL ||
             data.status == CpSolverStatus::INFEASIBLE)) {
          shared_->response->NotifyThatImprovingProblemIsInfeasible(
              solution_info);
        }
      }

      generator_->AddSolveData(data);

      if (VLOG_IS_ON(2) && display_lns_info) {
        std::string s = absl::StrCat("              LNS ", name(), ":");
        if (new_solution) {
          const double base_obj = ScaleObjectiveValue(
              shared_->model_proto.objective(),
              ComputeInnerObjective(shared_->model_proto.objective(),
                                    base_response.solution()));
          const double new_obj = ScaleObjectiveValue(
              shared_->model_proto.objective(),
              ComputeInnerObjective(shared_->model_proto.objective(),
                                    solution_values));
          absl::StrAppend(&s, " [new_sol:", base_obj, " -> ", new_obj, "]");
        }
        if (neighborhood.is_simple) {
          absl::StrAppend(
              &s, " [",
              "relaxed:", FormatCounter(neighborhood.num_relaxed_variables),
              " in_obj:",
              FormatCounter(neighborhood.num_relaxed_variables_in_objective),
              " compo:",
              neighborhood.variables_that_can_be_fixed_to_local_optimum.size(),
              "]");
        }
        SOLVER_LOG(
            shared_->logger, s,
            " [d:", absl::StrFormat("%0.2e", data.difficulty), ", id:", task_id,
            ", dtime:", absl::StrFormat("%0.2f", data.deterministic_time), "/",
            data.deterministic_limit,
            ", status:", CpSolverStatus_Name(data.status),
            ", #calls:", generator_->num_calls(),
            ", p:", fully_solved_proportion, "]");
      }
      {
        absl::MutexLock l(next_arena_size_mutex_);
        next_arena_size_ = arena.SpaceUsed();
      }
    };
  }

  void Synchronize() override {
    double sum = 0.0;
    const absl::Span<const double> dtimes = generator_->Synchronize();
    std::vector<double> sorted_dtimes(dtimes.begin(), dtimes.end());
    absl::c_sort(sorted_dtimes);
    for (const double dtime : sorted_dtimes) {
      sum += dtime;
      AddTaskDeterministicDuration(dtime);
    }
    shared_->time_limit->AdvanceDeterministicTime(sum);
  }

 private:
  // Generates a mapping which removes fixed variables (except one fixed
  // literal).
  bool GenerateMapping(CpModelProto& proto_with_variables,
                       std::vector<int>& variable_mapping,
                       std::vector<int64_t>& fixed_values) {
    int new_var_index = 0;
    std::vector<int> representatives;
    if (shared_->clauses != nullptr) {
      representatives = shared_->clauses->GetRepresentatives();
    }
    auto get_representative = [&](int var) {
      if (var >= representatives.size()) return var;
      return representatives[var];
    };

    // Fixed variables can be removed from the model. If a variable is fixed
    // then the equivalent variables can be fixed too.
    const int num_vars = proto_with_variables.variables_size();
    auto is_fixed = [&](int ref) {
      const int var = PositiveRef(ref);
      const auto& domain = proto_with_variables.variables(var).domain();
      return domain[0] == domain[domain.size() - 1];
    };
    auto fixed_literal_value = [&](int ref) {
      const int var = PositiveRef(ref);
      const int value = proto_with_variables.variables(var).domain(0);
      return RefIsPositive(ref) ? value : 1 - value;
    };
    auto fix_literal = [&](int literal, int value) {
      if (!RefIsPositive(literal)) {
        literal = PositiveRef(literal);
        value = 1 - value;
      }
      if (is_fixed(literal) && fixed_literal_value(literal) != value) {
        return false;
      }
      auto* domain =
          proto_with_variables.mutable_variables(literal)->mutable_domain();
      domain->Clear();
      domain->Add(value);
      domain->Add(value);
      return true;
    };
    for (int i = 0; i < num_vars; ++i) {
      if (proto_with_variables.variables(i).domain().empty()) return false;
      if (is_fixed(i)) {
        const int rep = get_representative(i);
        if (rep != i) {
          if (!fix_literal(rep, fixed_literal_value(i))) return false;
        }
      }
    }
    for (int i = 0; i < num_vars; ++i) {
      const int rep = get_representative(i);
      if (rep != i && is_fixed(rep)) {
        if (!fix_literal(i, fixed_literal_value(rep))) return false;
      }
    }

    int first_fixed_literal = -1;
    variable_mapping.assign(num_vars, kNoVariableMapping);
    fixed_values.reserve(num_vars);
    for (int i = 0; i < num_vars; ++i) {
      bool add_to_mapping = true;
      if (is_fixed(i)) {
        const int64_t value = proto_with_variables.variables(i).domain(0);
        fixed_values.push_back(value);
        add_to_mapping = false;
        if (first_fixed_literal == -1 && (value == 0 || value == 1)) {
          first_fixed_literal = i;
          add_to_mapping = true;
        }
      } else {
        fixed_values.push_back(kint64min);
      }
      if (add_to_mapping && variable_mapping[i] == kNoVariableMapping) {
        const int rep = get_representative(i);
        const int rep_var = PositiveRef(rep);
        if (variable_mapping[rep_var] == kNoVariableMapping) {
          variable_mapping[i] = new_var_index++;
          variable_mapping[rep_var] = RefIsPositive(rep)
                                          ? variable_mapping[i]
                                          : NegatedRef(variable_mapping[i]);
        } else {
          variable_mapping[i] = RefIsPositive(rep)
                                    ? variable_mapping[rep_var]
                                    : NegatedRef(variable_mapping[rep_var]);
        }
      }
    }
    return true;
  }

  std::unique_ptr<NeighborhoodGenerator> generator_;
  NeighborhoodGeneratorHelper* helper_;
  const SatParameters lns_parameters_base_;
  const SatParameters lns_parameters_stalling_;
  SharedClasses* shared_;
  // This is a optimization to allocate the arena for the LNS fragment already
  // at roughly the right size. We will update it with the last size of the
  // latest LNS fragment.
  absl::Mutex next_arena_size_mutex_;
  int64_t next_arena_size_ ABSL_GUARDED_BY(next_arena_size_mutex_) =
      helper_->ModelProto().GetArena() == nullptr
          ? Neighborhood::kDefaultArenaSizePerVariable
                * helper_->ModelProto().variables_size()
          : helper_->ModelProto().GetArena()->SpaceUsed();
};

void SolveCpModelParallel(SharedClasses* shared, Model* global_model) {
  const SatParameters& params = *global_model->GetOrCreate<SatParameters>();
  if (global_model->GetOrCreate<TimeLimit>()->LimitReached()) return;

  // If specified by the user, we might disable some parameters based on their
  // name.
  SubsolverNameFilter name_filter(params);

  // The list of all the SubSolver that will be used in this parallel search.
  // These will be synchronized in order. Note that we will assemble this at
  // the end from the other list below.
  std::vector<std::unique_ptr<SubSolver>> subsolvers;

  // We distinguish subsolver depending on their behavior:
  // - 'full' if a full thread is needed and they are not interleaved.
  // - 'first_solution' if they will be destroyed as soon as we have a solution.
  // - 'interleaved' if the work is cut into small chunk so that a few threads
  //    can work on many of such subsolvers alternatively.
  // - 'reentrant' if one subsolver can generate many such task.
  //
  // TODO(user): Maybe we should just interleave everything for an easier
  // configuration.
  std::vector<std::unique_ptr<SubSolver>> full_worker_subsolvers;
  std::vector<std::unique_ptr<SubSolver>> first_solution_full_subsolvers;
  std::vector<std::unique_ptr<SubSolver>> reentrant_interleaved_subsolvers;
  std::vector<std::unique_ptr<SubSolver>> interleaved_subsolvers;

  // Add a synchronization point for the shared classes.
  subsolvers.push_back(std::make_unique<SynchronizationPoint>(
      "synchronization_agent", [shared]() {
        shared->response->Synchronize();
        shared->ls_hints->Synchronize();
        if (shared->bounds != nullptr) {
          shared->bounds->Synchronize();
        }
        if (shared->lp_solutions != nullptr) {
          shared->lp_solutions->Synchronize();
        }
        if (shared->clauses != nullptr) {
          shared->clauses->Synchronize();
        }
      }));

  const auto name_to_params = GetNamedParameters(params);
  const SatParameters& lns_params_base = name_to_params.at("lns_base");
  const SatParameters& lns_params_stalling = name_to_params.at("lns_stalling");
  const SatParameters& lns_params_routing = name_to_params.at("lns_routing");

  // Add the NeighborhoodGeneratorHelper as a special subsolver so that its
  // Synchronize() is called before any LNS neighborhood solvers.
  auto unique_helper = std::make_unique<NeighborhoodGeneratorHelper>(
      &shared->model_proto, &params, shared->response, shared->time_limit,
      shared->bounds.get(), shared->clauses.get());
  NeighborhoodGeneratorHelper* helper = unique_helper.get();
  subsolvers.push_back(std::move(unique_helper));

  // Add full problem solvers.
  const std::vector<SatParameters> full_worker_params = GetFullWorkerParameters(
      shared->model_proto, shared->logger,
      global_model->GetOrCreate<SatParameters>(), &name_filter);
  if (params.shared_tree_num_workers() > 0) {
    shared->InitSharedTreeManager(global_model);
  }

  for (const SatParameters& local_params : full_worker_params) {
    if (!name_filter.Keep(local_params.name())) continue;

    // TODO(user): This is currently not supported here.
    if (params.optimize_with_max_hs()) continue;

    // TODO(user): these should probably be interleaved_subsolvers.
    if (local_params.use_objective_shaving_search()) {
      full_worker_subsolvers.push_back(std::make_unique<ObjectiveShavingSolver>(
          local_params, helper, shared));
      continue;
    }

    full_worker_subsolvers.push_back(std::make_unique<FullProblemSolver>(
        local_params.name(), local_params,
        /*split_in_chunks=*/params.interleave_search(), shared));
  }

  // Add FeasibilityPumpSolver if enabled.
  int num_interleaved_subsolver_that_do_not_need_solution = 0;
  if (params.use_feasibility_pump() && name_filter.Keep("feasibility_pump")) {
    ++num_interleaved_subsolver_that_do_not_need_solution;
    interleaved_subsolvers.push_back(
        std::make_unique<FeasibilityPumpSolver>(params, shared));
  }

  // Add variables shaving if enabled.
  // TODO(user): Like for feasibility jump, alternates better the variable that
  // we shave with the parameters that we use, and the time limit effort.
  int shaving_level = params.variables_shaving_level() >= 0
                          ? params.variables_shaving_level()
                          : params.num_workers() / 20;
  if (shaving_level > 0) {
    if (shaving_level > 3) shaving_level = 3;
    const std::string names[] = {"variables_shaving", "variables_shaving_no_lp",
                                 "variables_shaving_max_lp"};
    for (int i = 0; i < shaving_level; ++i) {
      if (name_filter.Keep(names[i])) {
        const SatParameters& local_params = name_to_params.at(names[i]);
        ++num_interleaved_subsolver_that_do_not_need_solution;
        reentrant_interleaved_subsolvers.push_back(
            std::make_unique<VariablesShavingSolver>(local_params, helper,
                                                     shared));
        continue;
      }
    }
  }

  // Add rins/rens.
  // This behave like a LNS, it just construct starting solution differently.
  if (params.use_rins_lns() && name_filter.Keep("rins/rens")) {
    // Note that we always create the SharedLPSolutionRepository. This meets
    // the requirement of having a SharedLPSolutionRepository to
    // create RINS/RENS lns generators.

    // TODO(user): Do we create a variable number of these workers.
    ++num_interleaved_subsolver_that_do_not_need_solution;
    reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
        std::make_unique<RelaxationInducedNeighborhoodGenerator>(
            helper, shared->response, shared->lp_solutions.get(),
            shared->incomplete_solutions.get(), name_filter.LastName()),
        lns_params_base, lns_params_stalling, helper, shared));
  }

  const bool has_no_overlap_or_cumulative =
      !helper->TypeToConstraints(ConstraintProto::kNoOverlap).empty() ||
      !helper->TypeToConstraints(ConstraintProto::kCumulative).empty();

  // Add incomplete subsolvers that require an objective.
  //
  // They are all re-entrant, so we do not need to specify more than the number
  // of workers. And they will all be interleaved, so it is okay to have many
  // even if we have a single thread for interleaved workers.
  if (params.use_lns() && shared->model_proto.has_objective() &&
      !shared->model_proto.objective().vars().empty()) {
    // Enqueue all the possible LNS neighborhood subsolvers.
    // Each will have their own metrics.
    if (name_filter.Keep("lns_rnd_var")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RelaxRandomVariablesGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("lns_rnd_cst")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RelaxRandomConstraintsGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("lns_graph_var")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<VariableGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("lns_graph_arc")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<ArcGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("lns_small_component")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<SmallComponentNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("lns_graph_cst")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<ConstraintGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("lns_graph_dec")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<DecompositionGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (params.use_lb_relax_lns() &&
        params.num_workers() >= params.lb_relax_num_workers_threshold() &&
        name_filter.Keep("lns_lb_relax")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<LocalBranchingLpBasedNeighborhoodGenerator>(
              helper, name_filter.LastName(), shared->time_limit, shared),
          lns_params_base, lns_params_stalling, helper, shared));
    }

    // Scheduling (no_overlap and cumulative) specific LNS.
    if (has_no_overlap_or_cumulative) {
      if (name_filter.Keep("lns_scheduling_intervals")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RandomIntervalSchedulingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("lns_scheduling_time_window")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<SchedulingTimeWindowNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      const std::vector<std::vector<int>> intervals_in_constraints =
          helper->GetUniqueIntervalSets();
      if (intervals_in_constraints.size() > 2 &&
          name_filter.Keep("lns_scheduling_resource_windows")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<SchedulingResourceWindowsNeighborhoodGenerator>(
                helper, intervals_in_constraints, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
    }

    // Packing (no_overlap_2d) Specific LNS.
    const bool has_no_overlap2d =
        !helper->TypeToConstraints(ConstraintProto::kNoOverlap2D).empty();
    if (has_no_overlap2d) {
      if (name_filter.Keep("lns_packing_random")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RandomRectanglesPackingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("lns_packing_square")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RectanglesPackingRelaxOneNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("lns_packing_swap")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RectanglesPackingRelaxTwoNeighborhoodsGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("lns_packing_precedences")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RandomPrecedencesPackingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("lns_packing_slice")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<SlicePackingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
    }

    // Generic scheduling/packing LNS.
    if (has_no_overlap_or_cumulative || has_no_overlap2d) {
      if (name_filter.Keep("lns_scheduling_precedences")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RandomPrecedenceSchedulingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
    }

    const int num_circuit = static_cast<int>(
        helper->TypeToConstraints(ConstraintProto::kCircuit).size());
    const int num_routes = static_cast<int>(
        helper->TypeToConstraints(ConstraintProto::kRoutes).size());
    if (num_circuit + num_routes > 0) {
      if (name_filter.Keep("lns_routing_random")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RoutingRandomNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_routing, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("lns_routing_path")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RoutingPathNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_routing, lns_params_stalling, helper, shared));
      }
    }
    if (num_routes > 0 || num_circuit > 1) {
      if (name_filter.Keep("lns_routing_full_path")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RoutingFullPathNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_routing, lns_params_stalling, helper, shared));
      }
    }
  }

  // Add violation LS workers.
  //
  // Compared to LNS, these are not re-entrant, so we need to schedule the
  // correct number for parallelism.
  if (shared->model_proto.has_objective() &&
      !shared->model_proto.objective().vars().empty()) {
    // If not forced by the parameters, we want one LS every 3 threads that
    // work on interleaved stuff. Note that by default they are many LNS, so
    // that shouldn't be too many.
    const int num_thread_for_interleaved_workers =
        params.num_workers() - full_worker_subsolvers.size();
    int num_violation_ls = params.has_num_violation_ls()
                               ? params.num_violation_ls()
                               : (num_thread_for_interleaved_workers + 2) / 3;

    // If there is no rentrant solver, maybe increase the number to reach max
    // parallelism.
    if (reentrant_interleaved_subsolvers.empty()) {
      num_violation_ls =
          std::max(num_violation_ls,
                   num_thread_for_interleaved_workers -
                       static_cast<int>(interleaved_subsolvers.size()));
    }

    const absl::string_view ls_name = "ls";
    const absl::string_view lin_ls_name = "ls_lin";

    const int num_ls_lin =
        name_filter.Keep(lin_ls_name) ? (num_violation_ls + 1) / 3 : 0;
    const int num_ls_default =
        name_filter.Keep(ls_name) ? num_violation_ls - num_ls_lin : 0;

    if (num_ls_default > 0) {
      std::shared_ptr<SharedLsStates> states = std::make_shared<SharedLsStates>(
          ls_name, params, shared->stat_tables);
      for (int i = 0; i < num_ls_default; ++i) {
        SatParameters local_params = params;
        local_params.set_random_seed(
            CombineSeed(params.random_seed(), interleaved_subsolvers.size()));
        local_params.set_feasibility_jump_linearization_level(0);
        interleaved_subsolvers.push_back(
            std::make_unique<FeasibilityJumpSolver>(
                ls_name, SubSolver::INCOMPLETE, shared->model_proto,
                local_params, states, shared->time_limit, shared->response,
                shared->bounds.get(), shared->clauses.get(), shared->ls_hints,
                shared->stat_tables));
      }
    }

    if (has_no_overlap_or_cumulative && name_filter.Keep("ls_scheduling")) {
      interleaved_subsolvers.push_back(
          std::make_unique<SchedulingLocalSearchSolver>(
              "ls_scheduling", SubSolver::INCOMPLETE, shared->model_proto,
              params, shared->time_limit, shared->response,
              shared->stat_tables));
    }

    if (num_ls_lin > 0) {
      std::shared_ptr<SharedLsStates> lin_states =
          std::make_shared<SharedLsStates>(lin_ls_name, params,
                                           shared->stat_tables);
      for (int i = 0; i < num_ls_lin; ++i) {
        SatParameters local_params = params;
        local_params.set_random_seed(
            CombineSeed(params.random_seed(), interleaved_subsolvers.size()));
        local_params.set_feasibility_jump_linearization_level(2);
        interleaved_subsolvers.push_back(
            std::make_unique<FeasibilityJumpSolver>(
                lin_ls_name, SubSolver::INCOMPLETE, shared->model_proto,
                local_params, lin_states, shared->time_limit, shared->response,
                shared->bounds.get(), shared->clauses.get(), shared->ls_hints,
                shared->stat_tables));
      }
    }
  }

  // Adds first solution subsolvers.
  // We have two kind, either full_worker_subsolvers or feasibility jump ones.
  //
  // These will be stopped and deleted as soon as the first solution is found,
  // leaving the resource for the other subsolvers (if we have an objective).
  {
    int num_thread_available =
        params.num_workers() - static_cast<int>(full_worker_subsolvers.size());

    // We reserve 1 thread for all interleaved subsolved that can work without
    // a first solution. If we have feasibility jump, because these will be
    // interleaved, we don't do that.
    if (!params.use_feasibility_jump() &&
        num_interleaved_subsolver_that_do_not_need_solution > 0) {
      --num_thread_available;
    }
    num_thread_available = std::max(num_thread_available, 0);
    // If we are in interleaved mode with one worker, num_thread_available is
    // always zero. We force it to 1 so that we at least have a
    // feasibility_jump subsolver.
    if (params.interleave_search() && params.num_workers() == 1) {
      // TODO(user): the 1 should be a parameter.
      num_thread_available = 1;
    }

    const std::vector<SatParameters> all_params =
        RepeatParameters(name_filter.Filter(GetFirstSolutionBaseParams(params)),
                         num_thread_available);

    std::shared_ptr<SharedLsStates> fj_states;
    std::shared_ptr<SharedLsStates> fj_lin_states;

    // Build the requested subsolvers.
    for (const SatParameters& local_params : all_params) {
      if (local_params.use_feasibility_jump()) {
        // Create the SharedLsStates if not already done.
        std::shared_ptr<SharedLsStates> states;
        if (local_params.feasibility_jump_linearization_level() == 0) {
          if (fj_states == nullptr) {
            fj_states = std::make_shared<SharedLsStates>(
                local_params.name(), params, shared->stat_tables);
          }
          states = fj_states;
        } else {
          if (fj_lin_states == nullptr) {
            fj_lin_states = std::make_shared<SharedLsStates>(
                local_params.name(), params, shared->stat_tables);
          }
          states = fj_lin_states;
        }

        interleaved_subsolvers.push_back(
            std::make_unique<FeasibilityJumpSolver>(
                local_params.name(), SubSolver::FIRST_SOLUTION,
                shared->model_proto, local_params, states, shared->time_limit,
                shared->response, shared->bounds.get(), shared->clauses.get(),
                shared->ls_hints, shared->stat_tables));
      } else {
        first_solution_full_subsolvers.push_back(
            std::make_unique<FullProblemSolver>(
                local_params.name(), local_params,
                /*split_in_chunks=*/local_params.interleave_search(), shared,
                /*stop_on_first_solution=*/true));
      }
    }
  }

  // Now that we are done with the logic, move all subsolver into a single
  // list. Note that the position of the "synchronization" subsolver matter.
  // Some are already in subsolvers, and we will add the gap one last.
  const auto move_all =
      [&subsolvers](std::vector<std::unique_ptr<SubSolver>>& from) {
        for (int i = 0; i < from.size(); ++i) {
          subsolvers.push_back(std::move(from[i]));
        }
        from.clear();
      };
  move_all(full_worker_subsolvers);
  move_all(first_solution_full_subsolvers);
  move_all(reentrant_interleaved_subsolvers);
  move_all(interleaved_subsolvers);

  // Add a synchronization point for the gap integral that is executed last.
  // This way, after each batch, the proper deterministic time is updated and
  // then the function to integrate take the value of the new gap.
  if (shared->model_proto.has_objective() &&
      !shared->model_proto.objective().vars().empty()) {
    subsolvers.push_back(std::make_unique<SynchronizationPoint>(
        "update_gap_integral",
        [shared]() { shared->response->UpdateGapIntegral(); }));
  }

  LaunchSubsolvers(global_model, shared, subsolvers, name_filter.AllIgnored());
}

#else
static_assert(!operations_research::kTargetOsSupportsThreads);
#endif  // ORTOOLS_TARGET_OS_SUPPORTS_THREADS

// If the option use_sat_inprocessing is true, then before post-solving a
// solution, we need to make sure we add any new clause required for postsolving
// to the mapping_model.
void AddPostsolveClauses(absl::Span<const int> postsolve_mapping, Model* model,
                         CpModelProto* mapping_proto) {
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

}  // namespace

std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& callback) {
  return [callback = callback](Model* model) {
    model->GetOrCreate<SharedResponseManager>()->AddSolutionCallback(callback);
  };
}

std::function<void(Model*)> NewFeasibleSolutionLogCallback(
    const std::function<std::string(const CpSolverResponse& response)>&
        callback) {
  return [callback = callback](Model* model) {
    model->GetOrCreate<SharedResponseManager>()->AddLogCallback(callback);
  };
}

std::function<void(Model*)> NewBestBoundCallback(
    const std::function<void(double)>& callback) {
  return [callback = callback](Model* model) {
    model->GetOrCreate<SharedResponseManager>()->AddBestBoundCallback(callback);
  };
}

namespace {
template <typename T>
void ParseFromStringOrDie(absl::string_view str, T* proto) {
  if constexpr (std::is_base_of_v<google::protobuf::Message, T>) {
    CHECK(google::protobuf::TextFormat::ParseFromString(str, proto)) << str;
  } else {
    LOG(FATAL) << "Calling NewSatParameters() with a textual proto is not "
                  "supported when using Lite Protobuf.";
  }
}
}  // namespace

// TODO(user): Support it on android.
std::function<SatParameters(Model*)> NewSatParameters(
    absl::string_view params) {
  sat::SatParameters parameters;
  if (!params.empty()) {
    ParseFromStringOrDie<SatParameters>(params, &parameters);
  }
  return NewSatParameters(parameters);
}

std::function<SatParameters(Model*)> NewSatParameters(
    const sat::SatParameters& parameters) {
  return [parameters = parameters](Model* model) {
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

void StopSearch(Model* model) {
  model->GetOrCreate<ModelSharedTimeLimit>()->Stop();
}

namespace {
void RegisterSearchStatisticCallback(Model* global_model) {
  global_model->GetOrCreate<SharedResponseManager>()
      ->AddStatisticsPostprocessor([](Model* local_model,
                                      CpSolverResponse* response) {
        auto* sat_solver = local_model->Get<SatSolver>();
        if (sat_solver != nullptr) {
          response->set_num_booleans(sat_solver->NumVariables());
          response->set_num_fixed_booleans(sat_solver->NumFixedVariables());
          response->set_num_branches(sat_solver->num_branches());
          response->set_num_conflicts(sat_solver->num_failures());
          response->set_num_binary_propagations(sat_solver->num_propagations());
          response->set_num_restarts(sat_solver->num_restarts());
        } else {
          response->set_num_booleans(0);
          response->set_num_fixed_booleans(0);
          response->set_num_branches(0);
          response->set_num_conflicts(0);
          response->set_num_binary_propagations(0);
          response->set_num_restarts(0);
        }

        auto* integer_trail = local_model->Get<IntegerTrail>();
        response->set_num_integers(
            integer_trail == nullptr
                ? 0
                : integer_trail->NumIntegerVariables().value() / 2);
        response->set_num_integer_propagations(
            integer_trail == nullptr ? 0 : integer_trail->num_enqueues());

        int64_t num_lp_iters = 0;
        auto* lp_constraints =
            local_model->GetOrCreate<LinearProgrammingConstraintCollection>();
        for (const LinearProgrammingConstraint* lp : *lp_constraints) {
          num_lp_iters += lp->total_num_simplex_iterations();
        }
        response->set_num_lp_iterations(num_lp_iters);
      });
}

void MergeParamsWithFlagsAndDefaults(SatParameters* params) {
  if constexpr (std::is_base_of_v<google::protobuf::Message, SatParameters>) {
    // Override parameters?
    if (!absl::GetFlag(FLAGS_cp_model_params).empty()) {
      SatParameters flag_params;
      ParseFromStringOrDie<SatParameters>(absl::GetFlag(FLAGS_cp_model_params),
                                          &flag_params);
      params->MergeFrom(flag_params);
    }
  }
}

void FixVariablesToHintValue(const PartialVariableAssignment& solution_hint,
                             PresolveContext* context, SolverLogger* logger) {
  SOLVER_LOG(logger, "Fixing ", solution_hint.vars().size(),
             " variables to their value in the solution hints.");
  context->InitializeNewDomains();
  for (int i = 0; i < solution_hint.vars_size(); ++i) {
    const int var = solution_hint.vars(i);
    const int64_t value = solution_hint.values(i);
    if (!context->IntersectDomainWith(var, Domain(value))) {
      const IntegerVariableProto& var_proto =
          context->WorkingModel().variables(var);
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

void ClearInternalFields(CpModelProto* model_proto, SolverLogger* logger) {
  if (model_proto->has_symmetry()) {
    SOLVER_LOG(logger, "Ignoring internal symmetry field");
    model_proto->clear_symmetry();
  }
  if (model_proto->has_objective()) {
    CpObjectiveProto* objective = model_proto->mutable_objective();
    if (objective->integer_scaling_factor() != 0 ||
        objective->integer_before_offset() != 0 ||
        objective->integer_after_offset() != 0) {
      SOLVER_LOG(logger, "Ignoring internal objective.integer_* fields.");
      objective->clear_integer_scaling_factor();
      objective->clear_integer_before_offset();
      objective->clear_integer_after_offset();
    }
  }
}

class CpModelSolver {
 public:
  struct Options {
    bool validate_parameters = true;
    bool validate_input_proto = true;
    bool copy_input_proto = true;
    bool dump_presolved_proto = true;

    static Options Default() { return Options(); }
  };

  CpModelSolver(const CpModelProto& model_proto, Model* model,
                std::string* log_string,
                const Options& options = Options::Default())
      : model_proto_(model_proto),
        model_(model),
        options_(options),
        params_(*model->GetOrCreate<SatParameters>()),
        shared_time_limit_(model->GetOrCreate<ModelSharedTimeLimit>()),
        shared_response_manager_(model->GetOrCreate<SharedResponseManager>()),
        log_string_(log_string) {
    shared_response_manager_->set_dump_prefix(
        absl::GetFlag(FLAGS_cp_model_dump_prefix));
    // Note: Allocating in an arena significantly speeds up destruction (free)
    // for large messages.
    presolved_model_proto_ =
        google::protobuf::Arena::Create<CpModelProto>(&arena_);
    mapping_proto_ = google::protobuf::Arena::Create<CpModelProto>(&arena_);
  }
  virtual ~CpModelSolver() = default;

  bool Initialize() {
    // Enable the logging component.
    InitializeSolverLogger();
    RegisterSearchStatisticCallback(model_);

    // Note that the postprocessors are executed in reverse order, so this will
    // always dump the response just before it is returned since it is the first
    // one we register.
    AddDumpFinalResponsePostprocessor();

    // Always display the final response stats if requested.
    // This also copy the logs to the response if requested.
    AddSetStatsAndLogsInFinalResponsePostprocessor();

    // Always add the timing information to a response. Note that it is
    // important to add this after the log/dump postprocessor since we execute
    // them in reverse order.
    AddSetTimingInResponsePostprocessor();

    // Validate parameters.
    // Note that the few parameters we use before that are Booleans and thus
    // "safe". We need to delay the validation to return a proper response.
    if (!ValidateParameters()) return false;

    // Initialize the time limit from the parameters.
    model_->GetOrCreate<TimeLimit>()->ResetLimitFromParameters(params_);

    // Register SIGINT handler if requested by the parameters.
    AddSigintHandler();

    if (!ValidateInputProto()) return false;

    return CopyInputProto();
  }

  void Solve() {
    if (Presolve()) {
      InitializeDebugSolutionFromHint();
      // Note that this needs to be done before SharedClasses are created.
      DetectPresolvedModelSymmetry();
      shared_ = std::make_unique<SharedClasses>(presolved_model_proto_, model_);
      AddFillTightenedDomainInResponsePostprocessor();
      AddCheckSolutionCallback();
      AddSolutionPostsolveCallback();
      DumpPresolvedProto();
      if (!StopAfterPresolve()) {
        LoadPresolvedModel();
        SolvePresolvedModel();
      }
    }
  }

  virtual CpSolverResponse GetResponse() {
    return shared_response_manager_->GetResponse();
  }

 protected:
  // Initialization.
  virtual void InitializeSolverLogger();
  void AddDumpFinalResponsePostprocessor();
  void AddSetStatsAndLogsInFinalResponsePostprocessor();
  void AddSetTimingInResponsePostprocessor();
  bool ValidateParameters();
  void AddSigintHandler();
  bool ValidateInputProto();
  bool CopyInputProto();

  // Presolve.
  virtual bool Presolve();
  void MaybeFixVariablesToHintValue(PresolveContext* context);
  void AddFloatingPointObjectiveResponsePostprocessor();
  bool FixAssumptionsIfNotSupported(PresolveContext* context);
  virtual CpSolverStatus PresolveCpModel(PresolveContext* context);

  void InitializeDebugSolutionFromHint();
  void DetectPresolvedModelSymmetry();
  void AddFillTightenedDomainInResponsePostprocessor();
  void AddCheckSolutionCallback();
  virtual void AddSolutionPostsolveCallback();

  void DumpPresolvedProto();
  bool StopAfterPresolve();
  virtual void LoadPresolvedModel();
  void SolvePresolvedModel();

  const CpModelProto& model_proto_;
  Model* model_;
  const Options options_;
  const SatParameters& params_;
  SolverLogger* logger_;
  SharedTimeLimit* shared_time_limit_;
  SharedResponseManager* shared_response_manager_;
  std::unique_ptr<SharedClasses> shared_;

  bool hint_feasible_before_presolve_ = false;
  std::unique_ptr<LratProofHandler> presolve_lrat_proof_handler_;
  google::protobuf::Arena arena_;
  CpModelProto* presolved_model_proto_;
  CpModelProto* mapping_proto_;
  std::unique_ptr<SolutionCrushProto> solution_crush_proto_;
  std::vector<int> postsolve_mapping_;
  std::vector<int64_t> debug_solution_from_hint_;

  std::string* log_string_;
};

void CpModelSolver::InitializeSolverLogger() {
  logger_ = model_->GetOrCreate<SolverLogger>();
  logger_->EnableLogging(params_.log_search_progress());
  logger_->SetLogToStdOut(params_.log_to_stdout());
  if (params_.log_to_response()) {
    logger_->AddInfoLoggingCallback([&](absl::string_view message) {
      absl::StrAppend(log_string_, message, "\n");
    });
  }
  if (logger_->LoggingIsEnabled()) {
    SolverProgressLogger* progress_logger =
        model_->GetOrCreate<SolverProgressLogger>();
    progress_logger->SetIsOptimization(
        model_proto_.has_objective() ||
        model_proto_.has_floating_point_objective());
    shared_response_manager_->AddStatusChangeCallback(
        [progress_logger](const SolverStatusChangeInfo& info) {
          progress_logger->UpdateProgress(info);
        });
  }
}

void CpModelSolver::AddDumpFinalResponsePostprocessor() {
  if constexpr (std::is_base_of_v<google::protobuf::Message,
                                  CpSolverResponse>) {
    if (absl::GetFlag(FLAGS_cp_model_dump_response)) {
      shared_response_manager_->AddFinalResponsePostprocessor(
          [](CpSolverResponse* response) {
            const std::string file = absl::StrCat(
                absl::GetFlag(FLAGS_cp_model_dump_prefix), "response.pb.txt");
            LOG(INFO) << "Dumping response proto to '" << file << "'.";
            CHECK(WriteModelProtoToFile(*response, file));
          });
    }
  }
}

void CpModelSolver::AddSetStatsAndLogsInFinalResponsePostprocessor() {
  shared_response_manager_->AddFinalResponsePostprocessor(
      [&](CpSolverResponse* response) {
        SOLVER_LOG(
            logger_,
            CpSolverResponseStats(
                *response, model_proto_.has_objective() ||
                               model_proto_.has_floating_point_objective()));
        if (!log_string_->empty()) {
          response->set_solve_log(*log_string_);
        }
      });
}

void CpModelSolver::AddSetTimingInResponsePostprocessor() {
  auto* wall_timer = model_->GetOrCreate<WallTimer>();
  auto* user_timer = model_->GetOrCreate<UserTimer>();
  shared_response_manager_->AddResponsePostprocessor(
      [&, wall_timer, user_timer](CpSolverResponse* response) {
        response->set_wall_time(wall_timer->Get());
        response->set_user_time(user_timer->Get());
        response->set_deterministic_time(
            shared_time_limit_->GetElapsedDeterministicTime());
      });
}

bool CpModelSolver::ValidateParameters() {
  if (!options_.validate_parameters) return true;
  const std::string error = sat::ValidateParameters(params_);
  if (!error.empty()) {
    SOLVER_LOG(logger_, "Invalid parameters: ", error);

    // TODO(user): We currently reuse the MODEL_INVALID status even though it
    // is not the best name for this. Maybe we can add a PARAMETERS_INVALID
    // when it become needed. Or rename to INVALID_INPUT ?
    CpSolverResponse status_response;
    status_response.set_status(CpSolverStatus::MODEL_INVALID);
    status_response.set_solution_info(error);
    shared_response_manager_->FillSolveStatsInResponse(model_,
                                                       &status_response);
    shared_response_manager_->AppendResponseToBeMerged(status_response);
    return false;
  }
  return true;
}

void CpModelSolver::AddSigintHandler() {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
  static_assert(operations_research::kTargetOsSupportsThreads);
  if (params_.catch_sigint_signal()) {
    model_->GetOrCreate<SigintHandler>()->Register(
        [&]() { shared_time_limit_->Stop(); });
  }
#else
  static_assert(!operations_research::kTargetOsSupportsThreads);
#endif  // defined( ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
}

bool CpModelSolver::ValidateInputProto() {
  if (DEBUG_MODE && !ProbablyRunningInsideUnitTest()) {
    LOG_EVERY_N_SEC(WARNING, 0.1)
        << "WARNING: CP-SAT is running in debug mode. The solver will "
           "be slow because we will do a lot of extra checks. Compile in "
           "optimization mode to gain an order of magnitude speedup.";
  }
  SOLVER_LOG(logger_, "");
  SOLVER_LOG(logger_, "Starting ", CpSatSolverVersion());
  SOLVER_LOG(logger_, "Parameters: ", ProtobufShortDebugString(params_));

  // Internally we adapt the parameters so that things are disabled if
  // they do not make sense.
  AdaptGlobalParameters(model_proto_, model_);

  if (logger_->LoggingIsEnabled() && params_.use_absl_random()) {
    model_->GetOrCreate<ModelRandomGenerator>()->LogSalt();
  }

  // Validate model_proto.
  // TODO(user): provide an option to skip this step for speed?
  const std::string error = ValidateInputCpModel(params_, model_proto_);
  if (!error.empty()) {
    SOLVER_LOG(logger_, "Invalid model: ", error);
    CpSolverResponse status_response;
    status_response.set_status(CpSolverStatus::MODEL_INVALID);
    status_response.set_solution_info(error);
    shared_response_manager_->FillSolveStatsInResponse(model_,
                                                       &status_response);
    shared_response_manager_->AppendResponseToBeMerged(status_response);
    return false;
  }
  return true;
}

bool CpModelSolver::CopyInputProto() {
  if (!options_.copy_input_proto) return true;

  SOLVER_LOG(logger_, "");
  SOLVER_LOG(logger_, "Initial ", CpModelStats(model_proto_));

  // Presolve and expansions.
  SOLVER_LOG(logger_, "");
  SOLVER_LOG(logger_,
             absl::StrFormat("Starting initial copy and canonicalization of "
                             "the input proto at %.2fs",
                             model_->GetOrCreate<WallTimer>()->Get()));

  // The lrat proof handler is needed is some cases, during the initial copy
  // and the presolve.
  //
  // Note that this is the "presolve one", each worker will have its own.
  presolve_lrat_proof_handler_ = LratProofHandler::MaybeCreate(
      model_, /*enable_rat_proofs=*/params_.cp_model_pure_sat_presolve());
  if (presolve_lrat_proof_handler_ != nullptr) {
    model_->Register<LratProofHandler>(presolve_lrat_proof_handler_.get());
  }

  if (!CopyModel(model_proto_, presolved_model_proto_, model_)) {
    const std::string info = "Problem proved infeasible during initial copy.";
    SOLVER_LOG(logger_, info);
    if (model_->Mutable<LratProofHandler>() != nullptr) {
      model_->Mutable<LratProofHandler>()->Close(/*model_is_unsat=*/true);
    }
    CpSolverResponse status_response;
    status_response.set_status(CpSolverStatus::INFEASIBLE);
    status_response.set_solution_info(info);
    shared_response_manager_->AppendResponseToBeMerged(status_response);
    SharedLratProofStatus* lrat_proof_status =
        model_->GetOrCreate<SharedLratProofStatus>();
    LratMerger(model_).Merge(lrat_proof_status->GetProofFilenames());
    lrat_proof_status->Log(logger_);
    return false;
  }

  // This uses the relations from the model_proto to fill the node expressions
  // of presolved_model_proto_. This is useful to have as many binary relations
  // as possible (presolved_model_proto_ can have less relations because the
  // model copier can remove the ones which are always true).
  const auto [num_routes, num_dimensions] =
      MaybeFillMissingRoutesConstraintNodeExpressions(model_proto_,
                                                      *presolved_model_proto_);
  if (num_dimensions > 0) {
    SOLVER_LOG(logger_, "Routes: ", num_dimensions,
               " dimension(s) automatically inferred for ", num_routes,
               " routes constraint(s).");
  }

  ClearInternalFields(presolved_model_proto_, logger_);

  if (absl::GetFlag(FLAGS_cp_model_ignore_objective) &&
      (presolved_model_proto_->has_objective() ||
       presolved_model_proto_->has_floating_point_objective())) {
    SOLVER_LOG(logger_, "Ignoring objective");
    presolved_model_proto_->clear_objective();
    presolved_model_proto_->clear_floating_point_objective();
  }

  if (absl::GetFlag(FLAGS_cp_model_ignore_hints) &&
      presolved_model_proto_->has_solution_hint()) {
    SOLVER_LOG(logger_, "Ignoring solution hint");
    presolved_model_proto_->clear_solution_hint();
  }
  return true;
}

bool CpModelSolver::Presolve() {
  SOLVER_LOG(logger_, absl::StrFormat("Starting presolve at %.2fs",
                                      model_->GetOrCreate<WallTimer>()->Get()));
  // We need a solution crush proto to crush the solution found in parallel of
  // presolve, if any.
  if (params_.cp_model_presolve() && params_.cp_model_direct_solve()) {
    solution_crush_proto_ = std::make_unique<SolutionCrushProto>();
  }
  auto context = std::make_unique<PresolveContext>(
      model_, presolved_model_proto_, mapping_proto_,
      solution_crush_proto_.get());

  // Checks for hints early in case they are forced to be hard constraints.
  MaybeFixVariablesToHintValue(context.get());

  // If the objective was a floating point one, do some postprocessing on the
  // final response.
  AddFloatingPointObjectiveResponsePostprocessor();

  // For the case where the assumptions are currently not supported, we just
  // assume they are fixed, and will always report all of them in the UNSAT
  // core if the problem turn out to be UNSAT.
  //
  // If the mode is not degraded, we will hopefully report a small subset
  // in case there is no feasible solution under these assumptions.
  if (!FixAssumptionsIfNotSupported(context.get())) {
    return false;
  }

  // Do the actual presolve.
  const CpSolverStatus presolve_status = PresolveCpModel(context.get());

  // Delete the presolve_lrat_proof_handler_.
  // This is needed to properly write the first proof file.
  if (presolve_lrat_proof_handler_ != nullptr) {
    presolve_lrat_proof_handler_->Close(presolve_status ==
                                        CpSolverStatus::INFEASIBLE);
    model_->Unregister<LratProofHandler>();
    presolve_lrat_proof_handler_.reset(nullptr);
  }

  // Delete the context as soon as the presolve is done. Note that only
  // postsolve_mapping_ and mapping_proto_ are needed for postsolve.
  context.reset(nullptr);

  if (presolve_status != CpSolverStatus::UNKNOWN) {
    if (presolve_status == CpSolverStatus::INFEASIBLE &&
        hint_feasible_before_presolve_ &&
        params_.debug_crash_if_presolve_breaks_hint()) {
      LOG(FATAL) << "Presolve bug: model with feasible hint found UNSAT "
                    "after presolve.";
    }
    SOLVER_LOG(logger_, "Problem closed by presolve.");
    CpSolverResponse status_response;
    status_response.set_status(presolve_status);
    shared_response_manager_->FillSolveStatsInResponse(model_,
                                                       &status_response);
    shared_response_manager_->AppendResponseToBeMerged(status_response);
    SharedLratProofStatus* lrat_proof_status =
        model_->GetOrCreate<SharedLratProofStatus>();
    LratMerger(model_).Merge(lrat_proof_status->GetProofFilenames());
    lrat_proof_status->Log(logger_);
    return false;
  }

  SOLVER_LOG(logger_, "");
  SOLVER_LOG(logger_, "Presolved ", CpModelStats(*presolved_model_proto_));
  return true;
}

void CpModelSolver::MaybeFixVariablesToHintValue(PresolveContext* context) {
  // Note that this still use the original user-given hint, which is not
  // clamped. We also don't have hint for potential new variable created during
  // copy/canonicalization. But we should be able to recover their value quite
  // quickly when we fix the hint.
  if (params_.fix_variables_to_their_hinted_value() &&
      model_proto_.has_solution_hint()) {
    FixVariablesToHintValue(model_proto_.solution_hint(), context, logger_);
  }

  // If the hint is complete, we can use the solution checker to do more
  // validation. Note that after the model has been validated, we are sure there
  // are do duplicate variables in the solution hint, so we can just check the
  // size.
  hint_feasible_before_presolve_ = false;
  if (!context->ModelIsUnsat()) {
    hint_feasible_before_presolve_ =
        SolutionHintIsCompleteAndFeasible(model_proto_, logger_);
  }
}

bool CpModelSolver::FixAssumptionsIfNotSupported(PresolveContext* context) {
  if (!model_proto_.assumptions().empty() &&
      (params_.num_workers() > 1 || model_proto_.has_objective() ||
       model_proto_.has_floating_point_objective() ||
       params_.enumerate_all_solutions() || params_.interleave_search())) {
    SOLVER_LOG(
        logger_,
        "Warning: solving with assumptions was requested in a non-fully "
        "supported setting.\nWe will assumes these assumptions true while "
        "solving, but if the model is infeasible, you will not get a useful "
        "'sufficient_assumptions_for_infeasibility' field in the response, it "
        "will include all assumptions.");

    shared_response_manager_->AddFinalResponsePostprocessor(
        [&](CpSolverResponse* response) {
          if (response->status() != CpSolverStatus::INFEASIBLE) return;

          // For now, just pass in all assumptions.
          *response->mutable_sufficient_assumptions_for_infeasibility() =
              model_proto_.assumptions();
        });

    // Clear them from the new proto.
    presolved_model_proto_->clear_assumptions();

    context->InitializeNewDomains();
    for (const int ref : model_proto_.assumptions()) {
      if (!context->SetLiteralToTrue(ref)) {
        CpSolverResponse status_response;
        status_response.set_status(CpSolverStatus::INFEASIBLE);
        status_response.add_sufficient_assumptions_for_infeasibility(ref);
        shared_response_manager_->FillSolveStatsInResponse(model_,
                                                           &status_response);
        shared_response_manager_->AppendResponseToBeMerged(status_response);
        return false;
      }
    }
  }
  return true;
}

void CpModelSolver::AddFloatingPointObjectiveResponsePostprocessor() {
  if (model_proto_.has_floating_point_objective()) {
    shared_response_manager_->AddFinalResponsePostprocessor(
        [&](CpSolverResponse* response) {
          if (response->solution().empty()) return;

          // Compute the true objective of the best returned solution.
          const auto& float_obj = model_proto_.floating_point_objective();
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
          if (!mapping_proto_->has_objective()) return;
          const CpObjectiveProto& integer_obj = mapping_proto_->objective();
          *response->mutable_integer_objective() = integer_obj;

          // If requested, compute a correct lb from the one on the integer
          // objective. We only do that if some error were introduced by the
          // scaling algorithm.
          if (params_.mip_compute_true_objective_bound() &&
              !integer_obj.scaling_was_exact()) {
            const int64_t integer_lb = response->inner_objective_lower_bound();
            const double lb = ComputeTrueObjectiveLowerBound(
                model_proto_, integer_obj, integer_lb);
            SOLVER_LOG(logger_, "[Scaling] scaled_objective_bound: ",
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
            if (gap > params_.absolute_gap_limit()) {
              SOLVER_LOG(logger_,
                         "[Scaling] Warning: OPTIMAL was reported, yet the "
                         "objective gap (",
                         gap, ") is greater than requested absolute limit (",
                         params_.absolute_gap_limit(), ").");
            }
          }
        });
  }
}

CpSolverStatus CpModelSolver::PresolveCpModel(PresolveContext* context) {
  return sat::PresolveCpModel(context, &postsolve_mapping_);
}

void CpModelSolver::InitializeDebugSolutionFromHint() {
  if (absl::GetFlag(FLAGS_cp_model_use_hint_for_debug_only) &&
      hint_feasible_before_presolve_) {
    SOLVER_LOG(logger_, "Using solution hint only as debug solution");
    debug_solution_from_hint_.resize(
        presolved_model_proto_->solution_hint().values_size());
    for (int i = 0; i < presolved_model_proto_->solution_hint().values_size();
         ++i) {
      debug_solution_from_hint_[presolved_model_proto_->solution_hint().vars(
          i)] = presolved_model_proto_->solution_hint().values(i);
    }
  }
}

void CpModelSolver::DetectPresolvedModelSymmetry() {
  // TODO(user): We could actually report a complete feasible hint before this
  // point. But the proper fix is to report it even before the presolve.
  if (params_.symmetry_level() > 1 && !params_.stop_after_presolve() &&
      !shared_time_limit_->LimitReached()) {
    if (params_.keep_symmetry_in_presolve() &&
        presolved_model_proto_->has_symmetry()) {
      // Symmetry should be already computed and correct, so we don't redo it.
      // Moreover it is possible we will not find them again as the constraints
      // might have changed.
    } else {
      TimeLimit time_limit;
      shared_time_limit_->UpdateLocalLimit(&time_limit);
      DetectAndAddSymmetryToProto(params_, *presolved_model_proto_,
                                  presolved_model_proto_->mutable_symmetry(),
                                  logger_, &time_limit);
    }

    // TODO(user): Some code just check presolved_model_proto_->has_symmetry().
    // If we don't have any generator, better to just clear the field.
    if (presolved_model_proto_->symmetry().permutations().empty()) {
      presolved_model_proto_->clear_symmetry();
    }
  }
}

void CpModelSolver::AddFillTightenedDomainInResponsePostprocessor() {
  if (params_.fill_tightened_domains_in_response()) {
    shared_response_manager_->AddResponsePostprocessor(
        [&](CpSolverResponse* response) {
          // Collect the info we know about presolved_model_proto_ bounds.
          // Note that this is not really needed as we should have the same
          // information in the mapping_proto_.
          std::vector<Domain> bounds;
          for (const IntegerVariableProto& vars :
               presolved_model_proto_->variables()) {
            bounds.push_back(ReadDomainFromProto(vars));
          }

          // Intersect with the SharedBoundsManager if it exist.
          if (shared_->bounds != nullptr) {
            shared_->bounds->UpdateDomains(&bounds);
          }

          // Postsolve and fill the field.
          FillTightenedDomainInResponse(model_proto_, *mapping_proto_,
                                        postsolve_mapping_, bounds, response,
                                        logger_);
        });
  }
}

void CpModelSolver::AddCheckSolutionCallback() {
  // Solution checking.
  // We either check all solutions, or only the last one.
  // Checking all solution might be expensive if we creates many.
  auto check_solution = [&](const CpSolverResponse& response) {
    if (response.solution().empty()) return;

    bool solution_is_feasible = true;
    if (params_.cp_model_presolve()) {
      // We pass presolve data for more informative message in case the solution
      // is not feasible.
      solution_is_feasible =
          SolutionIsFeasible(model_proto_, response.solution(), mapping_proto_,
                             &postsolve_mapping_);
    } else {
      solution_is_feasible =
          SolutionIsFeasible(model_proto_, response.solution());
    }
    if (solution_is_feasible && response.status() == CpSolverStatus::OPTIMAL) {
      solution_is_feasible =
          SolutionCanBeOptimal(model_proto_, response.solution());
    }

    // We dump the response when infeasible, this might help debugging.
    if (!solution_is_feasible) {
      const std::string file = absl::StrCat(
          absl::GetFlag(FLAGS_cp_model_dump_prefix), "wrong_response.pb.txt");
      LOG(INFO) << "Dumping infeasible response proto to '" << file << "'.";
      CHECK(WriteModelProtoToFile(response, file));

      // Crash.
      LOG(FATAL) << "Infeasible solution!"
                 << " source: '" << response.solution_info() << "'"
                 << " dumped CpSolverResponse to '" << file << "'.";
    }
  };
  if (DEBUG_MODE ||
      absl::GetFlag(FLAGS_cp_model_check_intermediate_solutions)) {
    shared_response_manager_->AddSolutionCallback(std::move(check_solution));
  } else {
    shared_response_manager_->AddFinalResponsePostprocessor(
        [checker = std::move(check_solution)](CpSolverResponse* response) {
          checker(*response);
        });
  }
}

void CpModelSolver::AddSolutionPostsolveCallback() {
  // Solution postsolving.
  if (params_.cp_model_presolve()) {
    shared_response_manager_->AddSolutionPostprocessor(
        [&](std::vector<int64_t>* solution) {
          AddPostsolveClauses(postsolve_mapping_, model_, mapping_proto_);
          PostsolveResponseWrapper(params_, model_proto_.variables_size(),
                                   *mapping_proto_, postsolve_mapping_,
                                   solution);
        });
    shared_response_manager_->AddResponsePostprocessor([&](CpSolverResponse*
                                                               response) {
      // Map back the sufficient assumptions for infeasibility.
      for (int& ref :
           *(response->mutable_sufficient_assumptions_for_infeasibility())) {
        ref = RefIsPositive(ref)
                  ? postsolve_mapping_[ref]
                  : NegatedRef(postsolve_mapping_[PositiveRef(ref)]);
      }
    });
  } else {
    shared_response_manager_->AddResponsePostprocessor(
        [&](CpSolverResponse* response) {
          // Truncate the solution in case model expansion added more variables.
          const int initial_size = model_proto_.variables_size();
          if (!response->solution().empty()) {
            response->mutable_solution()->Truncate(initial_size);
          }
        });
  }

  // Make sure everything stops when we have a first solution if requested.
  if (params_.stop_after_first_solution()) {
    shared_response_manager_->AddSolutionCallback(
        [&](const CpSolverResponse&) { shared_time_limit_->Stop(); });
  }
}

void CpModelSolver::DumpPresolvedProto() {
  if constexpr (std::is_base_of_v<google::protobuf::Message, CpModelProto> &&
                std::is_base_of_v<google::protobuf::Message, MPModelProto>) {
    if (absl::GetFlag(FLAGS_cp_model_dump_models)) {
      DumpModelProto(*presolved_model_proto_, "presolved_model");
      DumpModelProto(*mapping_proto_, "mapping_model");

      // Debug model with 1-based indices easier to read but cannot be parsed
      // back! This follow the SAT convention for literal like +3/-3 and
      // variable starts at 1. Note also that without the model name, the domain
      // of variable #i will be at line i.
      {
        CpModelProto copy = *presolved_model_proto_;
        copy.clear_name();
        for (ConstraintProto& ct : *copy.mutable_constraints()) {
          ApplyToAllVariableIndices(
              [](int* ref) {
                if (*ref >= 0) ++*ref;
              },
              &ct);
          ApplyToAllLiteralIndices(
              [](int* ref) {
                if (*ref >= 0) ++*ref;
              },
              &ct);
        }
        DumpModelProto(copy, "debug_1_based_model");
      }

      // If the model is convertible to a MIP, we dump it too.
      //
      // TODO(user): We could try to dump our linear relaxation too.
      MPModelProto mip_model;
      if (ConvertCpModelProtoToMPModelProto(*presolved_model_proto_,
                                            &mip_model)) {
        DumpModelProto(mip_model, "presolved_mp_model");
      }

      // If the model is convertible to a pure SAT one, we dump it too.
      std::string cnf_string;
      if (ConvertCpModelProtoToCnf(*presolved_model_proto_, &cnf_string)) {
        const std::string suffix =
            presolved_model_proto_->has_objective() ? "_no_objective" : "";
        const std::string filename =
            absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                         "presolved_cnf_model", suffix, ".cnf");
        LOG(INFO) << "Dumping presolved cnf model to '" << filename << "'.";
        const absl::Status status =
            file::SetContents(filename, cnf_string, file::Defaults());
        if (!status.ok()) LOG(ERROR) << status;
      } else if (ConvertCpModelProtoToWCnf(*presolved_model_proto_,
                                           &cnf_string)) {
        const std::string filename =
            absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                         "presolved_wcnf_model.wcnf");
        LOG(INFO) << "Dumping presolved wcnf model to '" << filename << "'.";
        const absl::Status status =
            file::SetContents(filename, cnf_string, file::Defaults());
        if (!status.ok()) LOG(ERROR) << status;
      }
    }
  }
}

bool CpModelSolver::StopAfterPresolve() {
  if (params_.stop_after_presolve() || shared_time_limit_->LimitReached()) {
    int64_t num_terms = 0;
    for (const ConstraintProto& ct : presolved_model_proto_->constraints()) {
      num_terms += static_cast<int>(UsedVariables(ct).size());
    }
    SOLVER_LOG(logger_, "Stopped after presolve.", "\nPresolvedNumVariables: ",
               presolved_model_proto_->variables().size(),
               "\nPresolvedNumConstraints: ",
               presolved_model_proto_->constraints().size(),
               "\nPresolvedNumTerms: ", num_terms);

    CpSolverResponse status_response;
    shared_response_manager_->FillSolveStatsInResponse(model_,
                                                       &status_response);
    shared_response_manager_->AppendResponseToBeMerged(status_response);
    return true;
  }
  return false;
}

void CpModelSolver::LoadPresolvedModel() {
  SOLVER_LOG(logger_, "");
  SOLVER_LOG(logger_, "Preloading model.");

  // If specified, we load the initial objective domain right away in the
  // response manager. Note that the presolve will always fill it with the
  // trivial min/max value if the user left it empty. This avoids to display
  // [-infinity, infinity] for the initial objective search space.
  if (presolved_model_proto_->has_objective()) {
    shared_response_manager_->InitializeObjective(*presolved_model_proto_);
    shared_response_manager_->SetGapLimitsFromParameters(params_);
  }

  // Start counting the primal integral from the current deterministic time and
  // initial objective domain gap that we just filled.
  shared_response_manager_->UpdateGapIntegral();

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
  bool hint_feasible_after_presolve;
  if (!params_.enumerate_all_solutions()) {
    hint_feasible_after_presolve = SolutionHintIsCompleteAndFeasible(
        *presolved_model_proto_, logger_, shared_response_manager_);
  } else {
    hint_feasible_after_presolve = SolutionHintIsCompleteAndFeasible(
        *presolved_model_proto_, logger_, nullptr);
  }

  if (hint_feasible_before_presolve_ && !hint_feasible_after_presolve &&
      params_.debug_crash_if_presolve_breaks_hint()) {
    LOG(FATAL) << "Presolve broke a feasible hint";
  }

  if (!debug_solution_from_hint_.empty()) {
    model_->GetOrCreate<SharedResponseManager>()->LoadDebugSolution(
        debug_solution_from_hint_);
    presolved_model_proto_->clear_solution_hint();
  } else {
    LoadDebugSolution(*presolved_model_proto_, model_);
  }
}

void CpModelSolver::SolvePresolvedModel() {
  if (!model_->GetOrCreate<TimeLimit>()->LimitReached()) {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
    static_assert(operations_research::kTargetOsSupportsThreads);
    if (params_.num_workers() > 1 || params_.interleave_search() ||
        !params_.subsolvers().empty() || !params_.filter_subsolvers().empty() ||
        params_.use_ls_only()) {
      SolveCpModelParallel(shared_.get(), model_);
#else   // defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
    static_assert(!operations_research::kTargetOsSupportsThreads);
    if (/* DISABLES CODE */ (false)) {
      // We ignore the multithreading parameter in this case.
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
    } else {
      shared_response_manager_->SetUpdateGapIntegralOnEachChange(true);

      // To avoid duplicating code, the single-thread version reuse most of
      // the multi-thread architecture.
      std::vector<std::unique_ptr<SubSolver>> subsolvers;
      subsolvers.push_back(std::make_unique<FullProblemSolver>(
          "main", params_, /*split_in_chunks=*/false, shared_.get()));
      LaunchSubsolvers(model_, shared_.get(), subsolvers, {});
    }
  }
}

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)

// A CpModelSolver run in parallel of presolve by a DefaultCpModelSolver.
// This solver assumes that the parameters and the input proto have already been
// validated by the DefaultCpModelSolver. It also assumes that cp_model_presolve
// is false.
class DirectSolverThread : public CpModelSolver {
 public:
  // - `model_proto`: the user input proto.
  // - `model_copy_proto`: the user input proto copy, as computed by the
  //    DefaultCpModelSolver (by CopyInputProto(), which does some very basic
  //    presolve).
  // - `model`: the model to use for this solver.
  // - `global_model`: the model used by the DefaultCpModelSolver.
  DirectSolverThread(const CpModelProto& model_proto,
                     const CpModelProto& model_copy_proto, Model* model,
                     Model* global_model, std::string* log_string)
      : CpModelSolver(
            model_proto, model, log_string,
            // This is called just before presolve in the DefaultCpModelSolver
            // below. At this point the parameters and the input proto have
            // already been validated by the DefaultCpModelSolver. The input
            // proto has also already been copied, into model_copy_proto.
            Options{.validate_parameters = false,
                    .validate_input_proto = false,
                    .copy_input_proto = false,
                    .dump_presolved_proto = false}),
        global_model_(global_model) {
    CHECK(!params_.cp_model_presolve());
    // Note that we need to make a copy of a model_copy_proto because it will be
    // modified by the presolve in DefaultCpModelSolver, upon return of this
    // constructor.
    *presolved_model_proto_ = model_copy_proto;
    num_variables_before_expansion_ = model_copy_proto.variables_size();
    CHECK(Initialize());
    thread_pool_ = std::make_unique<ThreadPool>(/*num_threads=*/1);
    thread_pool_->Schedule([this] { Solve(); });
  }

  // Returns a response if the time limit is reached, if the problem is solved,
  // or if a first solution is found and stop_after_first_solution is true. In
  // this case the DefaultCpModelSolver should stop too and return this
  // response. Otherwise, returns nullopt.
  std::optional<CpSolverResponse> Stop() {
    const bool limit_reached = shared_time_limit_->LimitReached();
    shared_time_limit_->Stop();
    thread_pool_.reset();

    if (limit_reached || shared_response_manager_->ProblemIsSolved() ||
        (shared_response_manager_->HasFeasibleSolution() &&
         params_.stop_after_first_solution())) {
      SolverLogger* logger = model_->GetOrCreate<SolverLogger>();
      logger->EnableLogging(params_.log_search_progress());
      logger->SetLogToStdOut(params_.log_to_stdout());
      if (shared_ != nullptr) {
        shared_->LogFinalStatistics();
      }
      return shared_response_manager_->GetResponse();
    }
    return std::nullopt;
  }

  std::vector<std::vector<int64_t>> GetSolutions() {
    std::vector<std::vector<int64_t>> solutions;
    if (shared_response_manager_->HasFeasibleSolution()) {
      auto& solution_repository =
          shared_response_manager_->SolutionPool().BestSolutions();
      for (const auto& solution : solution_repository.GetBestNSolutions(
               solution_repository.NumSolutions())) {
        solutions.push_back(solution->variable_values);
        // Truncate the solution in case model expansion added more variables.
        solutions.back().resize(num_variables_before_expansion_);
      }
    }
    return solutions;
  }

  void GetVariableBounds(std::vector<int64_t>* lower_bounds,
                         std::vector<int64_t>* upper_bounds) {
    auto* shared_bounds = model_->Mutable<SharedBoundsManager>();
    if (shared_bounds != nullptr) {
      shared_bounds->GetAllBounds(lower_bounds, upper_bounds);
    } else {
      lower_bounds->clear();
      upper_bounds->clear();
    }
  }

  std::pair<int64_t, int64_t> GetObjectiveBounds() {
    auto& objective = presolved_model_proto_->objective();
    int64_t lb =
        shared_response_manager_->GetInnerObjectiveLowerBound().value();
    int64_t ub =
        shared_response_manager_->GetInnerObjectiveUpperBound().value();
    lb = lb == kint64min ? lb : PostsolveInnerObjectiveValue(objective, lb);
    ub = ub == kint64max ? ub : PostsolveInnerObjectiveValue(objective, ub);
    return {lb, ub};
  }

 protected:
  void InitializeSolverLogger() override {
    // Create a SolverLogger with logging disabled and no logging to stdout.
    logger_ = model_->GetOrCreate<SolverLogger>();

    // If logging is enabled in the DefaultCpModelSolver, log the direct solve
    // progress with a "[DirectSolve]" prefix.
    if (global_model_->GetOrCreate<SolverLogger>()->LoggingIsEnabled()) {
      SolverProgressLogger* progress_logger =
          new SolverProgressLogger(global_model_);
      global_model_->TakeOwnership(progress_logger);
      progress_logger->SetLogPrefix("[DirectSolve] ");
      shared_response_manager_->AddStatusChangeCallback(
          [progress_logger](const SolverStatusChangeInfo& info) {
            progress_logger->UpdateProgress(info);
          });
    }
  }

  void AddSolutionPostsolveCallback() override {
    CpModelSolver::AddSolutionPostsolveCallback();
    if (params_.stop_after_first_solution()) {
      // Make sure to also stop the DefaultCpModelSolver.
      shared_response_manager_->AddSolutionCallback(
          [&](const CpSolverResponse&) {
            global_model_->GetOrCreate<ModelSharedTimeLimit>()->Stop();
          });
    }
  }

  Model* global_model_;
  int num_variables_before_expansion_;
  std::unique_ptr<ThreadPool> thread_pool_;
};

void RemapBounds(std::vector<int64_t>& lower_bounds,
                 std::vector<int64_t>& upper_bounds,
                 absl::Span<const int> old_to_new_mapping) {
  CHECK_EQ(lower_bounds.size(), old_to_new_mapping.size());
  CHECK_EQ(upper_bounds.size(), old_to_new_mapping.size());
  int new_num_vars = 0;
  for (int new_var : old_to_new_mapping) {
    new_num_vars = std::max(new_num_vars, new_var + 1);
  }
  std::vector<int64_t> new_lower_bounds(new_num_vars, kMinIntegerValue.value());
  std::vector<int64_t> new_upper_bounds(new_num_vars, kMaxIntegerValue.value());
  for (int old_var = 0; old_var < old_to_new_mapping.size(); ++old_var) {
    const int new_var = old_to_new_mapping[old_var];
    if (new_var >= 0) {
      new_lower_bounds[new_var] =
          std::max(new_lower_bounds[new_var], lower_bounds[old_var]);
      new_upper_bounds[new_var] =
          std::min(new_upper_bounds[new_var], upper_bounds[old_var]);
    }
  }
  std::swap(lower_bounds, new_lower_bounds);
  std::swap(upper_bounds, new_upper_bounds);
}

#endif  // ORTOOLS_TARGET_OS_SUPPORTS_THREADS

// A CpModelSolver which can start another one in parallel of presolve.
// More precisely, this solver starts a DirectSolverThread just before presolve,
// and stops it just after. It then launches the subsolvers on the presolved
// model, unless the time limit is reached or the DirectSolverThread found a
// solution which can be returned right away (e.g. if the problem is solved, or
// stop_after_first_solution is true).
class DefaultCpModelSolver : public CpModelSolver {
 public:
  DefaultCpModelSolver(const CpModelProto& model_proto, Model* model,
                       std::string* log_string)
      : CpModelSolver(model_proto, model, log_string) {}

  CpSolverResponse GetResponse() override {
    if (!response_.has_value()) {
      response_ = shared_response_manager_->GetResponse();
    }
    return *response_;
  }

 protected:
  bool Presolve() override {
    const bool ok = CpModelSolver::Presolve();
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
    if (direct_solver_ != nullptr) {
      // Save the response so that `GetResponse()` returns this one, if any,
      // instead of the response of the DefaultCpModelSolver.
      response_ = direct_solver_->Stop();
      // Save the solutions and bounds from the direct solver, so that they can
      // be merged with those of the DefaultCpModelSolver in
      // LoadPresolvedModel().
      direct_solver_solutions_ = direct_solver_->GetSolutions();
      direct_solver_->GetVariableBounds(&direct_solver_lower_bounds_,
                                        &direct_solver_upper_bounds_);
      direct_solver_objective_bounds_ = direct_solver_->GetObjectiveBounds();
      direct_solver_.reset();
      direct_solver_model_.reset();
      if (response_.has_value()) {
        // Return false in order to skip solving the presolved model.
        return false;
      }
    }
#endif
    return ok;
  }

  CpSolverStatus PresolveCpModel(PresolveContext* context) override {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
    // Launch solvers without presolve in separate threads just before doing the
    // actual presolve.
    static_assert(operations_research::kTargetOsSupportsThreads);
    if (params_.cp_model_direct_solve() && params_.num_workers() > 1 &&
        params_.cp_model_presolve() && !params_.check_lrat_proof() &&
        !params_.output_lrat_proof() && !params_.enumerate_all_solutions() &&
        model_proto_.assumptions().empty()) {
      StartDirectSolver();
    }
#endif
    return CpModelSolver::PresolveCpModel(context);
  }

  void LoadPresolvedModel() override {
    CpModelSolver::LoadPresolvedModel();
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
    // Crush and import the solutions found by the direct solver, if any.
    if (solution_crush_proto_ != nullptr) {
      SharedResponseManager* shared_response_manager =
          model_->GetOrCreate<SharedResponseManager>();
      for (const auto& solution : direct_solver_solutions_) {
        SolutionCrush solution_crush;
        solution_crush.LoadSolution(solution);
        for (const auto& step : solution_crush_proto_->steps()) {
          solution_crush.ApplySolutionCrushStep(step);
        }
        PartialVariableAssignment hint;
        solution_crush.StoreSolutionAsHint(hint);
        std::vector<int64_t> values;
        if (PartialVariableAssignmentIsCompleteAndFeasible(
                *presolved_model_proto_, hint, values)) {
          shared_response_manager->NewSolution(
              values, "direct_solver", /*model=*/nullptr, /*source_id=*/-1,
              /*with_callbacks=*/false);
        } else if (params_.debug_crash_if_presolve_breaks_hint()) {
          LOG(FATAL) << "SolutionCrush broke a feasible solution found in "
                        "parallel of presolve";
        }
      }
      direct_solver_solutions_.clear();

      auto* shared_bounds = model_->Mutable<SharedBoundsManager>();
      if (shared_bounds != nullptr && !direct_solver_lower_bounds_.empty()) {
        for (const auto& step : solution_crush_proto_->steps()) {
          switch (step.step_case()) {
            case SolutionCrushStep::kResize: {
              direct_solver_lower_bounds_.resize(step.resize().new_size(),
                                                 kMinIntegerValue.value());
              direct_solver_upper_bounds_.resize(step.resize().new_size(),
                                                 kMaxIntegerValue.value());
              break;
            }
            case SolutionCrushStep::kRemapVariables: {
              RemapBounds(direct_solver_lower_bounds_,
                          direct_solver_upper_bounds_,
                          step.remap_variables().old_to_new_mapping());
              break;
            }
            default:
              break;
          }
        }
        std::vector<int> variables;
        variables.reserve(direct_solver_lower_bounds_.size());
        for (int i = 0; i < direct_solver_lower_bounds_.size(); ++i) {
          variables.push_back(i);
        }
        // TODO(user): Note that if the presolve introduce a new variable X = Y
        // + Z, it will have "trivial" bounds, and we will not compute the
        // information as tightely as we could. I.e. no code will compute tight
        // bounds on X from the ones on Y and Z.
        shared_bounds->ReportPotentialNewBounds("direct_solver", variables,
                                                direct_solver_lower_bounds_,
                                                direct_solver_upper_bounds_);
        direct_solver_lower_bounds_.clear();
        direct_solver_upper_bounds_.clear();
      }
      solution_crush_proto_.reset();

      if (presolved_model_proto_->has_objective()) {
        auto& objective = presolved_model_proto_->objective();
        int64_t lb = direct_solver_objective_bounds_.first;
        int64_t ub = direct_solver_objective_bounds_.second;
        lb = lb == kint64min ? lb : PresolveInnerObjectiveValue(objective, lb);
        ub = ub == kint64max ? ub : PresolveInnerObjectiveValue(objective, ub);
        shared_response_manager->UpdateInnerObjectiveBounds("direct_solver", lb,
                                                            ub);
      }
    }
#endif  // ORTOOLS_TARGET_OS_SUPPORTS_THREADS
  }

 private:
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_THREADS)
  void StartDirectSolver() {
    direct_solver_model_ = std::make_unique<Model>();
    SatParameters& direct_solver_params =
        *direct_solver_model_->GetOrCreate<SatParameters>();
    direct_solver_params = params_;
    direct_solver_params.set_cp_model_presolve(false);
    direct_solver_params.set_stop_after_presolve(false);
    // TODO(user): adjust these parameters as needed (including the
    // number and type of subsolvers).

    direct_solver_model_->Register<WallTimer>(model_->GetOrCreate<WallTimer>());
    direct_solver_model_->Register<UserTimer>(model_->GetOrCreate<UserTimer>());

    direct_solver_ = std::make_unique<DirectSolverThread>(
        model_proto_, *presolved_model_proto_, direct_solver_model_.get(),
        model_, log_string_);
  }

  std::unique_ptr<Model> direct_solver_model_;
  std::unique_ptr<DirectSolverThread> direct_solver_;
  std::vector<std::vector<int64_t>> direct_solver_solutions_;
  std::vector<int64_t> direct_solver_lower_bounds_;
  std::vector<int64_t> direct_solver_upper_bounds_;
  std::pair<int64_t, int64_t> direct_solver_objective_bounds_ = {kint64min,
                                                                 kint64max};
#endif
  std::optional<CpSolverResponse> response_;
};

}  // namespace

CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model) {
  MergeParamsWithFlagsAndDefaults(model->GetOrCreate<SatParameters>());
  model->GetOrCreate<WallTimer>()->Start();
  model->GetOrCreate<UserTimer>()->Start();

  if constexpr (std::is_base_of_v<google::protobuf::Message, CpModelProto>) {
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
  }

  std::string log_string;
  DefaultCpModelSolver solver(model_proto, model, &log_string);
  if (solver.Initialize()) {
    solver.Solve();
  }

  return solver.GetResponse();
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

CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     absl::string_view params) {
  Model model;
  model.Add(NewSatParameters(params));
  return SolveCpModel(model_proto, &model);
}

}  // namespace sat
}  // namespace operations_research
