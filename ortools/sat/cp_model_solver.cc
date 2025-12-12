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
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

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
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/base/timer.h"
#include "ortools/base/version.h"
#include "ortools/port/os.h"
#include "ortools/port/proto_utils.h"
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
#include "ortools/sat/linear_model.h"
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
#include "ortools/sat/shaving_solver.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/sat/work_assignment.h"
#include "ortools/util/logging.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/sigint.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

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
  int i = 0;
  for (const auto& r : non_fixed_boxes) {
    sizes_to_render.push_back(Rectangle{
        .x_min = x, .x_max = x + r.x_size, .y_min = y, .y_max = y + r.y_size});
    x += r.x_size;
    if (x > bounding_box.x_max) {
      x = 0;
      y += r.y_size;
    }
    ++i;
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
        absl::StrCat("  - ", FormatCounter(num_constants), " constants in {",
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

  shared->shared_tree_manager->CloseLratProof();
  if (params.check_merged_lrat_proof() && shared->response->ProblemIsSolved() &&
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
void RestrictObjectiveUsingHint(CpModelProto* model_proto) {
  if (!model_proto->has_objective()) return;
  if (!model_proto->has_solution_hint()) return;

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
  if (num_filled != num_vars) return;

  const int64_t obj_upper_bound =
      ComputeInnerObjective(model_proto->objective(), solution);
  const Domain restriction =
      Domain(std::numeric_limits<int64_t>::min(), obj_upper_bound);

  if (model_proto->objective().domain().empty()) {
    FillDomainInProto(restriction, model_proto->mutable_objective());
  } else {
    FillDomainInProto(ReadDomainFromProto(model_proto->objective())
                          .IntersectionWith(restriction),
                      model_proto->mutable_objective());
  }
}

// Returns true iff there is a hint, and (ignoring fixed variables) if it is
// complete and feasible.
bool SolutionHintIsCompleteAndFeasible(
    const CpModelProto& model_proto, SolverLogger* logger = nullptr,
    SharedResponseManager* manager = nullptr) {
  if (!model_proto.has_solution_hint() && model_proto.variables_size() > 0) {
    return false;
  }

  int num_active_variables = 0;
  int num_hinted_variables = 0;
  for (int var = 0; var < model_proto.variables_size(); ++var) {
    if (VarIsFixed(model_proto, var)) continue;
    ++num_active_variables;
  }

  for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
    const int ref = model_proto.solution_hint().vars(i);
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

  std::vector<int64_t> solution(model_proto.variables_size(), 0);
  // Pre-assign from fixed domains.
  for (int var = 0; var < model_proto.variables_size(); ++var) {
    if (VarIsFixed(model_proto, var)) {
      solution[var] = model_proto.variables(var).domain(0);
    }
  }

  for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
    const int ref = model_proto.solution_hint().vars(i);
    const int var = PositiveRef(ref);
    const int64_t value = model_proto.solution_hint().values(i);
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
  if (is_feasible) {
    if (manager != nullptr && !solution.empty()) {
      // Add it to the pool right away! Note that we already have a log in this
      // case, so we don't log anything more.
      manager->NewSolution(solution, "complete_hint", nullptr);
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
                 "The solution hint is complete, but it is infeasible! we "
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

#if ORTOOLS_TARGET_OS_SUPPORTS_THREADS

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
        shared_->time_limit->Stop();
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

      // Tricky: we don't want to use the symmetry of the main problem in the
      // LNS presolved problem ! And currently no code clears/update it.
      //
      // TODO(user): Find a cleaner way like clear it as part of the presolve.
      // Also, do not copy that in the first place.
      lns_fragment.clear_symmetry();

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
      bool hint_feasible_before_presolve = false;
      if (lns_parameters_base_.debug_crash_if_presolve_breaks_hint()) {
        hint_feasible_before_presolve =
            SolutionHintIsCompleteAndFeasible(lns_fragment, /*logger=*/nullptr);
      }

      // If we use a hint, we will restrict the objective to be <= to the one
      // of the hint. This is helpful on some model where doing so can cause
      // the presolve to restrict the domain of many variables. Note that the
      // hint will still be feasible as we use <= and not <.
      RestrictObjectiveUsingHint(&lns_fragment);

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

      std::vector<int> postsolve_mapping;
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
      // parameters that work bests (core, linearization_level, etc...) or
      // maybe we can just randomize them like for the base solution used.
      auto* local_response_manager =
          local_model.GetOrCreate<SharedResponseManager>();
      local_response_manager->InitializeObjective(lns_fragment);
      local_response_manager->SetSynchronizationMode(true);

      CpSolverResponse local_response;
      if (presolve_status == CpSolverStatus::UNKNOWN) {
        // Sometimes when presolve is aborted in the middle, we don't want to
        // load the model as it might fail some DCHECK.
        if (shared_->SearchIsDone()) return;

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
            !shared_->model_proto.has_symmetry() && !solution_values.empty() &&
            neighborhood.is_simple && shared_->bounds != nullptr &&
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
          shared_->time_limit->Stop();
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
    for (const double dtime : dtimes) {
      sum += dtime;
      AddTaskDeterministicDuration(dtime);
    }
    shared_->time_limit->AdvanceDeterministicTime(sum);
  }

 private:
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

  if (params.check_drat_proof() || params.output_drat_proof()) {
    LOG(FATAL) << "DRAT proofs are not supported with several workers";
  }

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
      shared->bounds.get());
  NeighborhoodGeneratorHelper* helper = unique_helper.get();
  subsolvers.push_back(std::move(unique_helper));

  // How many shared tree workers to run?
  const int num_shared_tree_workers = shared->shared_tree_manager->NumWorkers();

  // Add shared tree workers if asked.
  if (num_shared_tree_workers >= 2 &&
      shared->model_proto.assumptions().empty()) {
    for (const SatParameters& local_params : RepeatParameters(
             name_filter.Filter({name_to_params.at("shared_tree")}),
             num_shared_tree_workers)) {
      full_worker_subsolvers.push_back(std::make_unique<FullProblemSolver>(
          local_params.name(), local_params,
          /*split_in_chunks=*/params.interleave_search(), shared));
    }
  }

  // Add full problem solvers.
  for (const SatParameters& local_params : GetFullWorkerParameters(
           params, shared->model_proto,
           /*num_already_present=*/full_worker_subsolvers.size(),
           &name_filter)) {
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

  // Add incomplete subsolvers that require an objective.
  //
  // They are all re-entrant, so we do not need to specify more than the number
  // of workers. And they will all be interleaved, so it is okay to have many
  // even if we have a single thread for interleaved workers.
  if (params.use_lns() && shared->model_proto.has_objective() &&
      !shared->model_proto.objective().vars().empty()) {
    // Enqueue all the possible LNS neighborhood subsolvers.
    // Each will have their own metrics.
    if (name_filter.Keep("rnd_var_lns")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RelaxRandomVariablesGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("rnd_cst_lns")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<RelaxRandomConstraintsGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("graph_var_lns")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<VariableGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("graph_arc_lns")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<ArcGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("graph_cst_lns")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<ConstraintGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (name_filter.Keep("graph_dec_lns")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<DecompositionGraphNeighborhoodGenerator>(
              helper, name_filter.LastName()),
          lns_params_base, lns_params_stalling, helper, shared));
    }
    if (params.use_lb_relax_lns() &&
        params.num_workers() >= params.lb_relax_num_workers_threshold() &&
        name_filter.Keep("lb_relax_lns")) {
      reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
          std::make_unique<LocalBranchingLpBasedNeighborhoodGenerator>(
              helper, name_filter.LastName(), shared->time_limit, shared),
          lns_params_base, lns_params_stalling, helper, shared));
    }

    const bool has_no_overlap_or_cumulative =
        !helper->TypeToConstraints(ConstraintProto::kNoOverlap).empty() ||
        !helper->TypeToConstraints(ConstraintProto::kCumulative).empty();

    // Scheduling (no_overlap and cumulative) specific LNS.
    if (has_no_overlap_or_cumulative) {
      if (name_filter.Keep("scheduling_intervals_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RandomIntervalSchedulingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("scheduling_time_window_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<SchedulingTimeWindowNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      const std::vector<std::vector<int>> intervals_in_constraints =
          helper->GetUniqueIntervalSets();
      if (intervals_in_constraints.size() > 2 &&
          name_filter.Keep("scheduling_resource_windows_lns")) {
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
      if (name_filter.Keep("packing_random_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RandomRectanglesPackingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("packing_square_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RectanglesPackingRelaxOneNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("packing_swap_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RectanglesPackingRelaxTwoNeighborhoodsGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("packing_precedences_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RandomPrecedencesPackingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("packing_slice_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<SlicePackingNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_base, lns_params_stalling, helper, shared));
      }
    }

    // Generic scheduling/packing LNS.
    if (has_no_overlap_or_cumulative || has_no_overlap2d) {
      if (name_filter.Keep("scheduling_precedences_lns")) {
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
      if (name_filter.Keep("routing_random_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RoutingRandomNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_routing, lns_params_stalling, helper, shared));
      }
      if (name_filter.Keep("routing_path_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RoutingPathNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_routing, lns_params_stalling, helper, shared));
      }
    }
    if (num_routes > 0 || num_circuit > 1) {
      if (name_filter.Keep("routing_full_path_lns")) {
        reentrant_interleaved_subsolvers.push_back(std::make_unique<LnsSolver>(
            std::make_unique<RoutingFullPathNeighborhoodGenerator>(
                helper, name_filter.LastName()),
            lns_params_routing, lns_params_stalling, helper, shared));
      }
    }
  }

  // Used by LS and feasibility jump.
  // This will automatically be created (only once) if needed.
  const auto get_linear_model = [&]() {
    auto* candidate = global_model->Get<LinearModel>();
    if (candidate != nullptr) return candidate;

    // Create it and transfer ownership.
    LinearModel* linear_model = new LinearModel(shared->model_proto);
    global_model->TakeOwnership(linear_model);
    global_model->Register(linear_model);
    return global_model->Get<LinearModel>();
  };

  // Add violation LS workers.
  //
  // Compared to LNS, these are not re-entrant, so we need to schedule the
  // correct number for parallelism.
  if (shared->model_proto.has_objective()) {
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
                ls_name, SubSolver::INCOMPLETE, get_linear_model(),
                local_params, states, shared->time_limit, shared->response,
                shared->bounds.get(), shared->ls_hints, shared->stats,
                shared->stat_tables));
      }
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
                lin_ls_name, SubSolver::INCOMPLETE, get_linear_model(),
                local_params, lin_states, shared->time_limit, shared->response,
                shared->bounds.get(), shared->ls_hints, shared->stats,
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
                get_linear_model(), local_params, states, shared->time_limit,
                shared->response, shared->bounds.get(), shared->ls_hints,
                shared->stats, shared->stat_tables));
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
  for (int i = 0; i < solution_hint.vars_size(); ++i) {
    const int var = solution_hint.vars(i);
    const int64_t value = solution_hint.values(i);
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

}  // namespace

CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model) {
  auto* wall_timer = model->GetOrCreate<WallTimer>();
  auto* user_timer = model->GetOrCreate<UserTimer>();
  wall_timer->Start();
  user_timer->Start();

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

  MergeParamsWithFlagsAndDefaults(model->GetOrCreate<SatParameters>());
  const SatParameters& params = *model->GetOrCreate<SatParameters>();

  // Enable the logging component.
  SolverLogger* logger = model->GetOrCreate<SolverLogger>();
  logger->EnableLogging(params.log_search_progress());
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

  if (logger->LoggingIsEnabled()) {
    SolverProgressLogger* progress_logger =
        model->GetOrCreate<SolverProgressLogger>();
    progress_logger->SetIsOptimization(
        model_proto.has_objective() ||
        model_proto.has_floating_point_objective());
    shared_response_manager->AddStatusChangeCallback(
        [progress_logger](const SolverStatusChangeInfo& info) {
          progress_logger->UpdateProgress(info);
        });
  }
  RegisterSearchStatisticCallback(model);

  if constexpr (std::is_base_of_v<google::protobuf::Message,
                                  CpSolverResponse>) {
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
  }

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
  ModelSharedTimeLimit* shared_time_limit =
      model->GetOrCreate<ModelSharedTimeLimit>();
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
      shared_response_manager->FillSolveStatsInResponse(model,
                                                        &status_response);
      shared_response_manager->AppendResponseToBeMerged(status_response);
      return shared_response_manager->GetResponse();
    }
  }

  // Initialize the time limit from the parameters.
  model->GetOrCreate<TimeLimit>()->ResetLimitFromParameters(params);

#if ORTOOLS_TARGET_OS_SUPPORTS_THREADS
  // Register SIGINT handler if requested by the parameters.
  if (params.catch_sigint_signal()) {
    model->GetOrCreate<SigintHandler>()->Register(
        [shared_time_limit]() { shared_time_limit->Stop(); });
  }
#endif  // ORTOOLS_TARGET_OS_SUPPORTS_THREADS

  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, "Starting ", CpSatSolverVersion());
  SOLVER_LOG(logger, "Parameters: ", ProtobufShortDebugString(params));

  // Internally we adapt the parameters so that things are disabled if
  // they do not make sense.
  AdaptGlobalParameters(model_proto, model);

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
      shared_response_manager->FillSolveStatsInResponse(model,
                                                        &status_response);
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

  std::unique_ptr<LratProofHandler> lrat_proof_handler =
      LratProofHandler::MaybeCreate(model);
  if (!ImportModelWithBasicPresolveIntoContext(model_proto, context.get(),
                                               lrat_proof_handler.get())) {
    const std::string info = "Problem proven infeasible during initial copy.";
    SOLVER_LOG(logger, info);
    if (lrat_proof_handler != nullptr) {
      lrat_proof_handler->Close(/*model_is_unsat=*/true);
    }
    CpSolverResponse status_response;
    status_response.set_status(CpSolverStatus::INFEASIBLE);
    status_response.set_solution_info(info);
    shared_response_manager->AppendResponseToBeMerged(status_response);
    return shared_response_manager->GetResponse();
  }

  // This uses the relations from the model_proto to fill the node expressions
  // of new_cp_model_proto. This is useful to have as many binary relations as
  // possible (new_cp_model_proto can have less relations because the model
  // copier can remove the ones which are always true).
  const auto [num_routes, num_dimensions] =
      MaybeFillMissingRoutesConstraintNodeExpressions(model_proto,
                                                      *new_cp_model_proto);
  if (num_dimensions > 0) {
    SOLVER_LOG(logger, "Routes: ", num_dimensions,
               " dimension(s) automatically inferred for ", num_routes,
               " routes constraint(s).");
  }

  ClearInternalFields(context->working_model, logger);

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
    FixVariablesToHintValue(model_proto.solution_hint(), context.get(), logger);
  }

  // If the hint is complete, we can use the solution checker to do more
  // validation. Note that after the model has been validated, we are sure there
  // are do duplicate variables in the solution hint, so we can just check the
  // size.
  bool hint_feasible_before_presolve = false;
  if (!context->ModelIsUnsat()) {
    hint_feasible_before_presolve =
        SolutionHintIsCompleteAndFeasible(model_proto, logger);
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
       params.enumerate_all_solutions() || params.interleave_search())) {
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
        shared_response_manager->FillSolveStatsInResponse(model,
                                                          &status_response);
        shared_response_manager->AppendResponseToBeMerged(status_response);
        return shared_response_manager->GetResponse();
      }
    }
  }

  // Do the actual presolve.
  std::vector<int> postsolve_mapping;
  const CpSolverStatus presolve_status =
      PresolveCpModel(context.get(), &postsolve_mapping);

  // Delete the context as soon as the presolve is done. Note that only
  // postsolve_mapping and mapping_proto are needed for postsolve.
  context.reset(nullptr);
  if (lrat_proof_handler != nullptr) {
    lrat_proof_handler->Close(presolve_status == CpSolverStatus::INFEASIBLE);
    lrat_proof_handler.reset();
  }

  if (presolve_status != CpSolverStatus::UNKNOWN) {
    if (presolve_status == CpSolverStatus::INFEASIBLE &&
        hint_feasible_before_presolve &&
        params.debug_crash_if_presolve_breaks_hint()) {
      LOG(FATAL) << "Presolve bug: model with feasible hint found UNSAT "
                    "after presolve.";
    }
    SOLVER_LOG(logger, "Problem closed by presolve.");
    CpSolverResponse status_response;
    status_response.set_status(presolve_status);
    shared_response_manager->FillSolveStatsInResponse(model, &status_response);
    shared_response_manager->AppendResponseToBeMerged(status_response);
    return shared_response_manager->GetResponse();
  }

  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, "Presolved ", CpModelStats(*new_cp_model_proto));

  std::vector<int64_t> debug_solution_from_hint;
  if (absl::GetFlag(FLAGS_cp_model_use_hint_for_debug_only) &&
      hint_feasible_before_presolve) {
    SOLVER_LOG(logger, "Using solution hint only as debug solution");
    debug_solution_from_hint.resize(
        new_cp_model_proto->solution_hint().values_size());
    for (int i = 0; i < new_cp_model_proto->solution_hint().values_size();
         ++i) {
      debug_solution_from_hint[new_cp_model_proto->solution_hint().vars(i)] =
          new_cp_model_proto->solution_hint().values(i);
    }
    new_cp_model_proto->clear_solution_hint();
  }

  // Detect the symmetry of the presolved model.
  // Note that this needs to be done before SharedClasses are created.
  //
  // TODO(user): We could actually report a complete feasible hint before this
  // point. But the proper fix is to report it even before the presolve.
  if (params.symmetry_level() > 1 && !params.stop_after_presolve() &&
      !shared_time_limit->LimitReached()) {
    if (params.keep_symmetry_in_presolve() &&
        new_cp_model_proto->has_symmetry()) {
      // Symmetry should be already computed and correct, so we don't redo it.
      // Moreover it is possible we will not find them again as the constraints
      // might have changed.
    } else {
      TimeLimit time_limit;
      shared_time_limit->UpdateLocalLimit(&time_limit);
      DetectAndAddSymmetryToProto(params, new_cp_model_proto, logger,
                                  &time_limit);
    }
  }

  // TODO(user): reduce this function size and find a better place for this?
  SharedClasses shared(new_cp_model_proto, model);

  if (params.fill_tightened_domains_in_response()) {
    shared_response_manager->AddResponsePostprocessor(
        [&model_proto, new_cp_model_proto, mapping_proto, &postsolve_mapping,
         logger, &shared](CpSolverResponse* response) {
          // Collect the info we know about new_cp_model_proto bounds.
          // Note that this is not really needed as we should have the same
          // information in the mapping_proto.
          std::vector<Domain> bounds;
          for (const IntegerVariableProto& vars :
               new_cp_model_proto->variables()) {
            bounds.push_back(ReadDomainFromProto(vars));
          }

          // Intersect with the SharedBoundsManager if it exist.
          if (shared.bounds != nullptr) {
            shared.bounds->UpdateDomains(&bounds);
          }

          // Postsolve and fill the field.
          FillTightenedDomainInResponse(model_proto, *mapping_proto,
                                        postsolve_mapping, bounds, response,
                                        logger);
        });
  }

  // Solution checking.
  // We either check all solutions, or only the last one.
  // Checking all solution might be expensive if we creates many.
  auto check_solution = [&model_proto, &params, mapping_proto,
                         &postsolve_mapping](const CpSolverResponse& response) {
    if (response.solution().empty()) return;

    bool solution_is_feasible = true;
    if (params.cp_model_presolve()) {
      // We pass presolve data for more informative message in case the solution
      // is not feasible.
      solution_is_feasible = SolutionIsFeasible(
          model_proto, response.solution(), mapping_proto, &postsolve_mapping);
    } else {
      solution_is_feasible =
          SolutionIsFeasible(model_proto, response.solution());
    }
    if (solution_is_feasible && response.status() == CpSolverStatus::OPTIMAL) {
      solution_is_feasible =
          SolutionCanBeOptimal(model_proto, response.solution());
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
    shared_response_manager->AddSolutionCallback(std::move(check_solution));
  } else {
    shared_response_manager->AddFinalResponsePostprocessor(
        [checker = std::move(check_solution)](CpSolverResponse* response) {
          checker(*response);
        });
  }

  // Solution postsolving.
  if (params.cp_model_presolve()) {
    shared_response_manager->AddSolutionPostprocessor(
        [&model_proto, &params, mapping_proto, &model,
         &postsolve_mapping](std::vector<int64_t>* solution) {
          AddPostsolveClauses(postsolve_mapping, model, mapping_proto);
          PostsolveResponseWrapper(params, model_proto.variables_size(),
                                   *mapping_proto, postsolve_mapping, solution);
        });
    shared_response_manager->AddResponsePostprocessor(
        [&postsolve_mapping](CpSolverResponse* response) {
          // Map back the sufficient assumptions for infeasibility.
          for (int& ref :
               *(response
                     ->mutable_sufficient_assumptions_for_infeasibility())) {
            ref = RefIsPositive(ref)
                      ? postsolve_mapping[ref]
                      : NegatedRef(postsolve_mapping[PositiveRef(ref)]);
          }
        });
  } else {
    shared_response_manager->AddResponsePostprocessor(
        [&model_proto](CpSolverResponse* response) {
          // Truncate the solution in case model expansion added more variables.
          const int initial_size = model_proto.variables_size();
          if (!response->solution().empty()) {
            response->mutable_solution()->Truncate(initial_size);
          }
        });
  }

  // Make sure everything stops when we have a first solution if requested.
  if (params.stop_after_first_solution()) {
    shared_response_manager->AddSolutionCallback(
        [shared_time_limit](const CpSolverResponse&) {
          shared_time_limit->Stop();
        });
  }

  if constexpr (std::is_base_of_v<google::protobuf::Message, CpModelProto> &&
                std::is_base_of_v<google::protobuf::Message, MPModelProto>) {
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
        const std::string suffix =
            new_cp_model_proto->has_objective() ? "_no_objective" : "";
        const std::string filename =
            absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                         "presolved_cnf_model", suffix, ".cnf");
        LOG(INFO) << "Dumping presolved cnf model to '" << filename << "'.";
        const absl::Status status =
            file::SetContents(filename, cnf_string, file::Defaults());
        if (!status.ok()) LOG(ERROR) << status;
      } else if (ConvertCpModelProtoToWCnf(*new_cp_model_proto, &cnf_string)) {
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
    shared_response_manager->FillSolveStatsInResponse(model, &status_response);
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
  bool hint_feasible_after_presolve;
  if (!params.enumerate_all_solutions()) {
    hint_feasible_after_presolve = SolutionHintIsCompleteAndFeasible(
        *new_cp_model_proto, logger, shared_response_manager);
  } else {
    hint_feasible_after_presolve =
        SolutionHintIsCompleteAndFeasible(*new_cp_model_proto, logger, nullptr);
  }

  if (hint_feasible_before_presolve && !hint_feasible_after_presolve &&
      params.debug_crash_if_presolve_breaks_hint()) {
    LOG(FATAL) << "Presolve broke a feasible hint";
  }

  if (!debug_solution_from_hint.empty()) {
    model->GetOrCreate<SharedResponseManager>()->LoadDebugSolution(
        debug_solution_from_hint);
  } else {
    LoadDebugSolution(*new_cp_model_proto, model);
  }

  if (!model->GetOrCreate<TimeLimit>()->LimitReached()) {
#if ORTOOLS_TARGET_OS_SUPPORTS_THREADS
    if (params.num_workers() > 1 || params.interleave_search() ||
        !params.subsolvers().empty() || !params.filter_subsolvers().empty() ||
        params.use_ls_only()) {
      SolveCpModelParallel(&shared, model);
#else   // ORTOOLS_TARGET_OS_SUPPORTS_THREADS
    if (/* DISABLES CODE */ (false)) {
      // We ignore the multithreading parameter in this case.
#endif  // ORTOOLS_TARGET_OS_SUPPORTS_THREADS
    } else {
      shared_response_manager->SetUpdateGapIntegralOnEachChange(true);

      // To avoid duplicating code, the single-thread version reuse most of
      // the multi-thread architecture.
      std::vector<std::unique_ptr<SubSolver>> subsolvers;
      subsolvers.push_back(std::make_unique<FullProblemSolver>(
          "main", params, /*split_in_chunks=*/false, &shared));
      LaunchSubsolvers(model, &shared, subsolvers, {});
    }
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

CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     absl::string_view params) {
  Model model;
  model.Add(NewSatParameters(params));
  return SolveCpModel(model_proto, &model);
}

}  // namespace sat
}  // namespace operations_research
