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

#include "ortools/sat/cp_model_lns.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/meta/type_traits.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/check.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/connected_components.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/adaptative_parameter_value.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

NeighborhoodGeneratorHelper::NeighborhoodGeneratorHelper(
    CpModelProto const* model_proto, SatParameters const* parameters,
    SharedResponseManager* shared_response, SharedBoundsManager* shared_bounds)
    : SubSolver(""),
      parameters_(*parameters),
      model_proto_(*model_proto),
      shared_bounds_(shared_bounds),
      shared_response_(shared_response) {
  CHECK(shared_response_ != nullptr);
  if (shared_bounds_ != nullptr) {
    shared_bounds_id_ = shared_bounds_->RegisterNewId();
  }
  *model_proto_with_only_variables_.mutable_variables() =
      model_proto_.variables();
  InitializeHelperData();
  RecomputeHelperData();
  Synchronize();
  last_logging_time_ = absl::Now();
}

void NeighborhoodGeneratorHelper::Synchronize() {
  if (shared_bounds_ != nullptr) {
    std::vector<int> model_variables;
    std::vector<int64_t> new_lower_bounds;
    std::vector<int64_t> new_upper_bounds;
    shared_bounds_->GetChangedBounds(shared_bounds_id_, &model_variables,
                                     &new_lower_bounds, &new_upper_bounds);

    bool new_variables_have_been_fixed = false;

    {
      absl::MutexLock domain_lock(&domain_mutex_);

      for (int i = 0; i < model_variables.size(); ++i) {
        const int var = model_variables[i];
        const int64_t new_lb = new_lower_bounds[i];
        const int64_t new_ub = new_upper_bounds[i];
        if (VLOG_IS_ON(3)) {
          const auto& domain =
              model_proto_with_only_variables_.variables(var).domain();
          const int64_t old_lb = domain.Get(0);
          const int64_t old_ub = domain.Get(domain.size() - 1);
          VLOG(3) << "Variable: " << var << " old domain: [" << old_lb << ", "
                  << old_ub << "] new domain: [" << new_lb << ", " << new_ub
                  << "]";
        }
        const Domain old_domain = ReadDomainFromProto(
            model_proto_with_only_variables_.variables(var));
        const Domain new_domain =
            old_domain.IntersectionWith(Domain(new_lb, new_ub));
        if (new_domain.IsEmpty()) {
          // This can mean two things:
          // 1/ This variable is a normal one and the problem is UNSAT or
          // 2/ This variable is optional, and its associated literal must be
          //    set to false.
          //
          // Currently, we wait for any full solver to pick the crossing bounds
          // and do the correct stuff on their own. We do not want to have empty
          // domain in the proto as this would means INFEASIBLE. So we just
          // ignore such bounds here.
          //
          // TODO(user): We could set the optional literal to false directly in
          // the bound sharing manager. We do have to be careful that all the
          // different solvers have the same optionality definition though.
          continue;
        }
        FillDomainInProto(
            new_domain,
            model_proto_with_only_variables_.mutable_variables(var));
        new_variables_have_been_fixed |= new_domain.IsFixed();
      }
    }

    // Only trigger the computation if needed.
    if (new_variables_have_been_fixed) {
      RecomputeHelperData();
    }
  }
}

bool NeighborhoodGeneratorHelper::ObjectiveDomainIsConstraining() const {
  if (!model_proto_.has_objective()) return false;
  if (model_proto_.objective().domain().empty()) return false;

  int64_t min_activity = 0;
  int64_t max_activity = 0;
  const int num_terms = model_proto_.objective().vars().size();
  for (int i = 0; i < num_terms; ++i) {
    const int var = PositiveRef(model_proto_.objective().vars(i));
    const int64_t coeff = model_proto_.objective().coeffs(i);
    const auto& var_domain =
        model_proto_with_only_variables_.variables(var).domain();
    const int64_t v1 = coeff * var_domain[0];
    const int64_t v2 = coeff * var_domain[var_domain.size() - 1];
    min_activity += std::min(v1, v2);
    max_activity += std::max(v1, v2);
  }

  const Domain obj_domain = ReadDomainFromProto(model_proto_.objective());
  const Domain inferred_domain =
      Domain(min_activity, max_activity)
          .IntersectionWith(
              Domain(std::numeric_limits<int64_t>::min(), obj_domain.Max()));
  return !inferred_domain.IsIncludedIn(obj_domain);
}

void NeighborhoodGeneratorHelper::InitializeHelperData() {
  type_to_constraints_.clear();
  const int num_constraints = model_proto_.constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const int type = model_proto_.constraints(c).constraint_case();
    if (type >= type_to_constraints_.size()) {
      type_to_constraints_.resize(type + 1);
    }
    type_to_constraints_[type].push_back(c);
  }

  const int num_variables = model_proto_.variables().size();
  is_in_objective_.resize(num_variables, false);
  if (model_proto_.has_objective()) {
    for (const int ref : model_proto_.objective().vars()) {
      is_in_objective_[PositiveRef(ref)] = true;
    }
  }
}

// Recompute all the data when new variables have been fixed. Note that this
// shouldn't be called if there is no change as it is in O(problem size).
void NeighborhoodGeneratorHelper::RecomputeHelperData() {
  absl::MutexLock graph_lock(&graph_mutex_);
  absl::ReaderMutexLock domain_lock(&domain_mutex_);

  // Do basic presolving to have a more precise graph.
  // Here we just remove trivially true constraints.
  //
  // Note(user): We do that each time a new variable is fixed. It might be too
  // much, but on the miplib and in 1200s, we do that only about 1k time on the
  // worst case problem.
  //
  // TODO(user): Change API to avoid a few copy?
  // TODO(user): We could keep the context in the class.
  // TODO(user): We can also start from the previous simplified model instead.
  {
    Model local_model;
    CpModelProto mapping_proto;
    simplied_model_proto_.Clear();
    *simplied_model_proto_.mutable_variables() =
        model_proto_with_only_variables_.variables();
    PresolveContext context(&local_model, &simplied_model_proto_,
                            &mapping_proto);
    ModelCopy copier(&context);

    // TODO(user): Not sure what to do if the model is UNSAT.
    // This  shouldn't matter as it should be dealt with elsewhere.
    copier.ImportAndSimplifyConstraints(model_proto_, {});
  }

  // Compute the constraint <-> variable graph.
  //
  // TODO(user): Remove duplicate constraints?
  const auto& constraints = simplied_model_proto_.constraints();
  var_to_constraint_.assign(model_proto_.variables_size(), {});
  constraint_to_var_.assign(constraints.size(), {});
  int reduced_ct_index = 0;
  for (int ct_index = 0; ct_index < constraints.size(); ++ct_index) {
    // We remove the interval constraints since we should have an equivalent
    // linear constraint somewhere else. This is not the case if we have a fixed
    // size optional interval variable. But it should not matter as the
    // intervals are replaced by their underlying variables in the scheduling
    // constrainst.
    if (constraints[ct_index].constraint_case() == ConstraintProto::kInterval) {
      continue;
    }

    for (const int var : UsedVariables(constraints[ct_index])) {
      if (IsConstant(var)) continue;
      constraint_to_var_[reduced_ct_index].push_back(var);
    }

    // We replace intervals by their underlying integer variables. Note that
    // this is needed for a correct decomposition into independent part.
    for (const int interval : UsedIntervals(constraints[ct_index])) {
      for (const int var : UsedVariables(constraints[interval])) {
        if (IsConstant(var)) continue;
        constraint_to_var_[reduced_ct_index].push_back(var);
      }
    }

    // We remove constraint of size 0 and 1 since they are not useful for LNS
    // based on this graph.
    if (constraint_to_var_[reduced_ct_index].size() <= 1) {
      constraint_to_var_[reduced_ct_index].clear();
      continue;
    }

    // Keep this constraint.
    for (const int var : constraint_to_var_[reduced_ct_index]) {
      var_to_constraint_[var].push_back(reduced_ct_index);
    }
    ++reduced_ct_index;
  }
  constraint_to_var_.resize(reduced_ct_index);

  // We mark as active all non-constant variables.
  // Non-active variable will never be fixed in standard LNS fragment.
  active_variables_.clear();
  const int num_variables = model_proto_.variables_size();
  active_variables_set_.assign(num_variables, false);
  for (int i = 0; i < num_variables; ++i) {
    if (!IsConstant(i)) {
      active_variables_.push_back(i);
      active_variables_set_[i] = true;
    }
  }

  // Compute connected components.
  // Note that fixed variable are just ignored.
  DenseConnectedComponentsFinder union_find;
  union_find.SetNumberOfNodes(num_variables);
  for (const std::vector<int>& var_in_constraint : constraint_to_var_) {
    if (var_in_constraint.size() <= 1) continue;
    for (int i = 1; i < var_in_constraint.size(); ++i) {
      union_find.AddEdge(var_in_constraint[0], var_in_constraint[i]);
    }
  }

  // If we have a lower bound on the objective, then this "objective constraint"
  // might link components together.
  if (ObjectiveDomainIsConstraining()) {
    const auto& refs = model_proto_.objective().vars();
    const int num_terms = refs.size();
    for (int i = 1; i < num_terms; ++i) {
      union_find.AddEdge(PositiveRef(refs[0]), PositiveRef(refs[i]));
    }
  }

  // Compute all components involving non-fixed variables.
  //
  // TODO(user): If a component has no objective, we can fix it to any feasible
  // solution. This will automatically be done by LNS fragment covering such
  // component though.
  components_.clear();
  var_to_component_index_.assign(num_variables, -1);
  for (int var = 0; var < num_variables; ++var) {
    if (IsConstant(var)) continue;
    const int root = union_find.FindRoot(var);
    DCHECK_LT(root, var_to_component_index_.size());
    int& index = var_to_component_index_[root];
    if (index == -1) {
      index = components_.size();
      components_.push_back({});
    }
    var_to_component_index_[var] = index;
    components_[index].push_back(var);
  }

  // Display information about the reduced problem.
  //
  // TODO(user): Exploit connected component while generating fragments.
  // TODO(user): Do not generate fragment not touching the objective.
  if (!shared_response_->LoggingIsEnabled()) return;

  std::vector<int> component_sizes;
  for (const std::vector<int>& component : components_) {
    component_sizes.push_back(component.size());
  }
  std::sort(component_sizes.begin(), component_sizes.end(),
            std::greater<int>());
  std::string compo_message;
  if (component_sizes.size() > 1) {
    if (component_sizes.size() <= 10) {
      compo_message =
          absl::StrCat(" compo:", absl::StrJoin(component_sizes, ","));
    } else {
      component_sizes.resize(10);
      compo_message =
          absl::StrCat(" compo:", absl::StrJoin(component_sizes, ","), ",...");
    }
  }

  // TODO(user): This is not ideal, as if two reductions appears in a row and
  // nothing else is done for a while, we will never see the "latest" size
  // in the log until it is reduced again.
  shared_response_->LogPeriodicMessage(
      "Model",
      absl::StrCat("var:", active_variables_.size(), "/", num_variables,
                   " constraints:", simplied_model_proto_.constraints().size(),
                   "/", model_proto_.constraints().size(), compo_message),
      parameters_.model_reduction_log_frequency_in_seconds(),
      &last_logging_time_);
}

bool NeighborhoodGeneratorHelper::IsActive(int var) const {
  return active_variables_set_[var];
}

bool NeighborhoodGeneratorHelper::IsConstant(int var) const {
  return model_proto_with_only_variables_.variables(var).domain_size() == 2 &&
         model_proto_with_only_variables_.variables(var).domain(0) ==
             model_proto_with_only_variables_.variables(var).domain(1);
}

Neighborhood NeighborhoodGeneratorHelper::FullNeighborhood() const {
  Neighborhood neighborhood;
  neighborhood.is_reduced = false;
  neighborhood.is_generated = true;
  {
    absl::ReaderMutexLock lock(&domain_mutex_);
    *neighborhood.delta.mutable_variables() =
        model_proto_with_only_variables_.variables();
  }
  return neighborhood;
}

Neighborhood NeighborhoodGeneratorHelper::NoNeighborhood() const {
  Neighborhood neighborhood;
  neighborhood.is_generated = false;
  return neighborhood;
}

std::vector<int> NeighborhoodGeneratorHelper::GetActiveIntervals(
    const CpSolverResponse& initial_solution) const {
  std::vector<int> active_intervals;
  absl::ReaderMutexLock lock(&domain_mutex_);
  for (const int i : TypeToConstraints(ConstraintProto::kInterval)) {
    const ConstraintProto& interval_ct = ModelProto().constraints(i);
    // We only look at intervals that are performed in the solution. The
    // unperformed intervals should be automatically freed during the generation
    // phase.
    if (interval_ct.enforcement_literal().size() == 1) {
      const int enforcement_ref = interval_ct.enforcement_literal(0);
      const int enforcement_var = PositiveRef(enforcement_ref);
      const int value = initial_solution.solution(enforcement_var);
      if (RefIsPositive(enforcement_ref) == (value == 0)) {
        continue;
      }
    }

    // We filter out fixed intervals. Because of presolve, if there is an
    // enforcement literal, it cannot be fixed.
    if (interval_ct.enforcement_literal().empty()) {
      bool is_constant = true;
      for (const int v : interval_ct.interval().start().vars()) {
        if (!IsConstant(v)) {
          is_constant = false;
          break;
        }
      }
      for (const int v : interval_ct.interval().size().vars()) {
        if (!IsConstant(v)) {
          is_constant = false;
          break;
        }
      }
      for (const int v : interval_ct.interval().end().vars()) {
        if (!IsConstant(v)) {
          is_constant = false;
          break;
        }
      }
      if (is_constant) continue;
    }

    active_intervals.push_back(i);
  }
  return active_intervals;
}

std::vector<std::vector<int>>
NeighborhoodGeneratorHelper::GetUniqueIntervalSets() const {
  std::vector<std::vector<int>> intervals_in_constraints;
  absl::flat_hash_set<std::vector<int>> added_intervals_sets;
  const auto add_interval_list_only_once =
      [&intervals_in_constraints,
       &added_intervals_sets](const auto& intervals) {
        std::vector<int> candidate({intervals.begin(), intervals.end()});
        gtl::STLSortAndRemoveDuplicates(&candidate);
        if (added_intervals_sets.insert(candidate).second) {
          intervals_in_constraints.push_back(candidate);
        }
      };
  for (const int ct_index : TypeToConstraints(ConstraintProto::kNoOverlap)) {
    add_interval_list_only_once(
        model_proto_.constraints(ct_index).no_overlap().intervals());
  }
  for (const int ct_index : TypeToConstraints(ConstraintProto::kCumulative)) {
    add_interval_list_only_once(
        model_proto_.constraints(ct_index).cumulative().intervals());
  }
  for (const int ct_index : TypeToConstraints(ConstraintProto::kNoOverlap2D)) {
    add_interval_list_only_once(
        model_proto_.constraints(ct_index).no_overlap_2d().x_intervals());
    add_interval_list_only_once(
        model_proto_.constraints(ct_index).no_overlap_2d().y_intervals());
  }
  return intervals_in_constraints;
}

namespace {

int64_t GetLinearExpressionValue(const LinearExpressionProto& expr,
                                 const CpSolverResponse& initial_solution) {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    result += expr.coeffs(i) * initial_solution.solution(expr.vars(i));
  }
  return result;
}

struct StartEndInterval {
  int64_t start;
  int64_t end;
  int interval_index;
  bool operator<(const StartEndInterval& o) const {
    return std::tie(start, end, interval_index) <
           std::tie(o.start, o.end, o.interval_index);
  }
};

// Selects all intervals in a random time window to meet the difficulty
// requirement.
std::vector<int> SelectIntervalsInRandomTimeWindow(
    const std::vector<int>& intervals, const CpModelProto& model_proto,
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<StartEndInterval> start_end_intervals;
  for (const int i : intervals) {
    const ConstraintProto& interval_ct = model_proto.constraints(i);
    // We only look at intervals that are performed in the solution. The
    // unperformed intervals should be automatically freed during the
    // generation phase.
    if (interval_ct.enforcement_literal().size() == 1) {
      const int enforcement_ref = interval_ct.enforcement_literal(0);
      const int enforcement_var = PositiveRef(enforcement_ref);
      const int64_t value = initial_solution.solution(enforcement_var);
      if (RefIsPositive(enforcement_ref) == (value == 0)) {
        continue;
      }
    }
    const int64_t start_value = GetLinearExpressionValue(
        interval_ct.interval().start(), initial_solution);
    const int64_t end_value = GetLinearExpressionValue(
        interval_ct.interval().end(), initial_solution);
    start_end_intervals.push_back({start_value, end_value, i});
  }

  if (start_end_intervals.empty()) return {};

  std::sort(start_end_intervals.begin(), start_end_intervals.end());
  const int relaxed_size = std::floor(difficulty * start_end_intervals.size());

  std::uniform_int_distribution<int> random_var(
      0, start_end_intervals.size() - relaxed_size - 1);
  // TODO(user): Consider relaxing more than one time window
  // intervals. This seems to help with Giza models.
  const int random_start_index = random_var(random);

  // We want to minimize the time window relaxed, so we now sort the interval
  // after the first selected intervals by end value.
  // TODO(user): We could do things differently (include all tasks <= some
  // end). The difficulty is that the number of relaxed tasks will differ from
  // the target. We could also tie break tasks randomly.
  std::sort(start_end_intervals.begin() + random_start_index,
            start_end_intervals.end(),
            [](const StartEndInterval& a, const StartEndInterval& b) {
              return std::tie(a.end, a.interval_index) <
                     std::tie(b.end, b.interval_index);
            });
  std::vector<int> result;
  for (int i = random_start_index; i < random_start_index + relaxed_size; ++i) {
    result.push_back(start_end_intervals[i].interval_index);
  }
  return result;
}

struct Demand {
  int interval_index;
  int64_t start;
  int64_t end;
  int64_t height;

  // Because of the binary splitting of the capacity in the procedure used to
  // extract precedences out of a cumulative constraint, processing bigger
  // heigts first will decrease its probability of being split across the 2
  // halves of the current split.
  bool operator<(const Demand& other) const {
    return std::tie(start, height, end) <
           std::tie(other.start, other.height, other.end);
  }
};

void InsertPrecedencesFromSortedListOfDemands(
    const std::vector<Demand>& demands,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  for (int i = 0; i + 1 < demands.size(); ++i) {
    DCHECK_LE(demands[i].end, demands[i + 1].start);
    precedences->insert(
        {demands[i].interval_index, demands[i + 1].interval_index});
  }
}

void InsertNoOverlapPrecedences(
    const absl::flat_hash_set<int>& ignored_intervals,
    const CpSolverResponse& initial_solution, const CpModelProto& model_proto,
    int no_overlap_index,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  std::vector<Demand> demands;
  const NoOverlapConstraintProto& no_overlap =
      model_proto.constraints(no_overlap_index).no_overlap();
  for (const int interval_index : no_overlap.intervals()) {
    if (ignored_intervals.contains(interval_index)) continue;
    const ConstraintProto& interval_ct =
        model_proto.constraints(interval_index);
    // We only look at intervals that are performed in the solution. The
    // unperformed intervals should be automatically freed during the generation
    // phase.
    if (interval_ct.enforcement_literal().size() == 1) {
      const int enforcement_ref = interval_ct.enforcement_literal(0);
      const int enforcement_var = PositiveRef(enforcement_ref);
      const int value = initial_solution.solution(enforcement_var);
      if (RefIsPositive(enforcement_ref) == (value == 0)) {
        continue;
      }
    }

    const int64_t start_value = GetLinearExpressionValue(
        interval_ct.interval().start(), initial_solution);
    const int64_t end_value = GetLinearExpressionValue(
        interval_ct.interval().size(), initial_solution);
    demands.push_back({interval_index, start_value, end_value, 1});
  }

  std::sort(demands.begin(), demands.end());
  InsertPrecedencesFromSortedListOfDemands(demands, precedences);
}

void ProcessDemandListFromCumulativeConstraint(
    const std::vector<Demand>& demands, int64_t capacity,
    std::deque<std::pair<std::vector<Demand>, int64_t>>* to_process,
    absl::BitGenRef random,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  if (demands.size() <= 1) return;

  // Checks if any pairs of tasks cannot overlap.
  int64_t sum_of_min_two_capacities = 2;
  if (capacity > 1) {
    int64_t min1 = std::numeric_limits<int64_t>::max();
    int64_t min2 = std::numeric_limits<int64_t>::max();
    for (const Demand& demand : demands) {
      if (demand.height <= min1) {
        min2 = min1;
        min1 = demand.height;
      } else if (demand.height < min2) {
        min2 = demand.height;
      }
    }
    sum_of_min_two_capacities = min1 + min2;
  }

  DCHECK_GT(sum_of_min_two_capacities, 1);
  if (sum_of_min_two_capacities > capacity) {
    InsertPrecedencesFromSortedListOfDemands(demands, precedences);
    return;
  }

  std::vector<int64_t> unique_starts;
  for (const Demand& demand : demands) {
    DCHECK(unique_starts.empty() || demand.start >= unique_starts.back());
    if (unique_starts.empty() || unique_starts.back() < demand.start) {
      unique_starts.push_back(demand.start);
    }
  }
  DCHECK(std::is_sorted(unique_starts.begin(), unique_starts.end()));
  const int num_points = unique_starts.size();

  // Split the capacity in 2 and dispatch all demands on the 2 parts.
  const int64_t capacity1 = capacity / 2;
  std::vector<int64_t> usage1(num_points);
  std::vector<Demand> demands1;

  const int64_t capacity2 = capacity - capacity1;
  std::vector<int64_t> usage2(num_points);
  std::vector<Demand> demands2;

  int usage_index = 0;
  for (const Demand& d : demands) {
    // Since we process demand by increasing start, the usage_index only
    // need to increase.
    while (usage_index < num_points && unique_starts[usage_index] < d.start) {
      usage_index++;
    }
    DCHECK_LT(usage_index, num_points);
    DCHECK_EQ(unique_starts[usage_index], d.start);
    const int64_t slack1 = capacity1 - usage1[usage_index];
    const int64_t slack2 = capacity2 - usage2[usage_index];

    // We differ from the ICAPS article. If it fits in both sub-cumulatives, We
    // choose the smallest slack. If it fits into at most one, we choose the
    // biggest slack. If both slacks are equal, we choose randomly.
    const bool prefer2 =
        slack1 == slack2
            ? absl::Bernoulli(random, 0.5)
            : (d.height <= std::min(slack1, slack2) ? slack2 < slack1
                                                    : slack2 > slack1);

    auto& selected_usage = prefer2 ? usage2 : usage1;
    auto& residual_usage = prefer2 ? usage1 : usage2;
    std::vector<Demand>& selected_demands = prefer2 ? demands2 : demands1;
    std::vector<Demand>& residual_demands = prefer2 ? demands1 : demands2;
    const int64_t selected_slack = prefer2 ? slack2 : slack1;

    const int64_t assigned_to_selected = std::min(selected_slack, d.height);
    DCHECK_GT(assigned_to_selected, 0);
    for (int i = usage_index; i < num_points; ++i) {
      if (d.end <= unique_starts[i]) break;
      selected_usage[i] += assigned_to_selected;
    }
    selected_demands.push_back(
        {d.interval_index, d.start, d.end, assigned_to_selected});

    if (d.height > selected_slack) {
      const int64_t residual = d.height - selected_slack;
      DCHECK_GT(residual, 0);
      DCHECK_LE(residual, prefer2 ? slack1 : slack2);
      for (int i = usage_index; i < num_points; ++i) {
        if (d.end <= unique_starts[i]) break;
        residual_usage[i] += residual;
      }
      residual_demands.push_back({d.interval_index, d.start, d.end, residual});
    }
  }

  if (demands1.size() > 1) {
    to_process->emplace_back(std::move(demands1), capacity1);
  }
  if (demands2.size() > 1) {
    to_process->emplace_back(std::move(demands2), capacity2);
  }
}

void InsertCumulativePrecedences(
    const absl::flat_hash_set<int>& ignored_intervals,
    const CpSolverResponse& initial_solution, const CpModelProto& model_proto,
    int cumulative_index, absl::BitGenRef random,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  const CumulativeConstraintProto& cumulative =
      model_proto.constraints(cumulative_index).cumulative();

  std::vector<Demand> demands;
  for (int i = 0; i < cumulative.intervals().size(); ++i) {
    const int interval_index = cumulative.intervals(i);
    if (ignored_intervals.contains(interval_index)) continue;

    const ConstraintProto& interval_ct =
        model_proto.constraints(interval_index);
    // We only look at intervals that are performed in the solution. The
    // unperformed intervals should be automatically freed during the generation
    // phase.
    if (interval_ct.enforcement_literal().size() == 1) {
      const int enforcement_ref = interval_ct.enforcement_literal(0);
      const int enforcement_var = PositiveRef(enforcement_ref);
      const int value = initial_solution.solution(enforcement_var);
      if (RefIsPositive(enforcement_ref) == (value == 0)) {
        continue;
      }
    }

    const int64_t start_value = GetLinearExpressionValue(
        interval_ct.interval().start(), initial_solution);
    const int64_t end_value = GetLinearExpressionValue(
        interval_ct.interval().end(), initial_solution);
    const int64_t demand_value =
        GetLinearExpressionValue(cumulative.demands(i), initial_solution);
    if (start_value == end_value || demand_value == 0) continue;

    demands.push_back({interval_index, start_value, end_value, demand_value});
  }
  std::sort(demands.begin(), demands.end());

  const int64_t capacity_value =
      GetLinearExpressionValue(cumulative.capacity(), initial_solution);
  DCHECK_GT(capacity_value, 0);

  // Copying all these demands is memory intensive. Let's be careful here.
  std::deque<std::pair<std::vector<Demand>, int64_t>> to_process;
  to_process.emplace_back(std::move(demands), capacity_value);

  while (!to_process.empty()) {
    auto& next_task = to_process.front();
    ProcessDemandListFromCumulativeConstraint(next_task.first,
                                              /*capacity=*/next_task.second,
                                              &to_process, random, precedences);
    to_process.pop_front();
  }
}

struct Rectangle {
  int interval_index;
  int64_t x_start;
  int64_t x_end;
  int64_t y_start;
  int64_t y_end;

  bool operator<(const Rectangle& other) const {
    return std::tie(x_start, x_end) < std::tie(other.x_start, other.x_end);
  }
};

void InsertRectanglePredecences(
    const std::vector<Rectangle>& rectangles,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  // TODO(user): Refine set of interesting points.
  absl::flat_hash_set<int64_t> interesting_points;
  for (const Rectangle& r : rectangles) {
    interesting_points.insert(r.y_end - 1);
  }
  std::vector<Demand> demands;
  for (const int64_t t : interesting_points) {
    demands.clear();
    for (const Rectangle& r : rectangles) {
      if (r.y_start > t || r.y_end <= t) continue;
      demands.push_back({r.interval_index, r.x_start, r.x_end, 1});
    }
    std::sort(demands.begin(), demands.end());
    InsertPrecedencesFromSortedListOfDemands(demands, precedences);
  }
}

void InsertNoOverlap2dPrecedences(
    const absl::flat_hash_set<int>& ignored_intervals,
    const CpSolverResponse& initial_solution, const CpModelProto& model_proto,
    int no_overlap_2d_index,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  std::vector<Demand> demands;
  const NoOverlap2DConstraintProto& no_overlap_2d =
      model_proto.constraints(no_overlap_2d_index).no_overlap_2d();
  std::vector<Rectangle> x_main;
  std::vector<Rectangle> y_main;
  for (int i = 0; i < no_overlap_2d.x_intervals_size(); ++i) {
    // Ignore unperformed rectangles.
    const int x_interval_index = no_overlap_2d.x_intervals(i);
    if (ignored_intervals.contains(x_interval_index)) continue;
    const ConstraintProto& x_interval_ct =
        model_proto.constraints(x_interval_index);
    if (x_interval_ct.enforcement_literal().size() == 1) {
      const int enforcement_ref = x_interval_ct.enforcement_literal(0);
      const int enforcement_var = PositiveRef(enforcement_ref);
      const int value = initial_solution.solution(enforcement_var);
      if (RefIsPositive(enforcement_ref) == (value == 0)) {
        continue;
      }
    }

    const int y_interval_index = no_overlap_2d.y_intervals(i);
    if (ignored_intervals.contains(y_interval_index)) continue;
    const ConstraintProto& y_interval_ct =
        model_proto.constraints(y_interval_index);
    if (y_interval_ct.enforcement_literal().size() == 1) {
      const int enforcement_ref = y_interval_ct.enforcement_literal(0);
      const int enforcement_var = PositiveRef(enforcement_ref);
      const int value = initial_solution.solution(enforcement_var);
      if (RefIsPositive(enforcement_ref) == (value == 0)) {
        continue;
      }
    }

    const int64_t x_start_value = GetLinearExpressionValue(
        x_interval_ct.interval().start(), initial_solution);
    const int64_t x_end_value = GetLinearExpressionValue(
        x_interval_ct.interval().end(), initial_solution);
    const int64_t y_start_value = GetLinearExpressionValue(
        y_interval_ct.interval().start(), initial_solution);
    const int64_t y_end_value = GetLinearExpressionValue(
        y_interval_ct.interval().end(), initial_solution);

    // Ignore rectangles with zero area.
    if (x_start_value == x_end_value || y_start_value == y_end_value) continue;

    x_main.push_back({x_interval_index, x_start_value, x_end_value,
                      y_start_value, y_end_value});
    y_main.push_back({y_interval_index, y_start_value, y_end_value,
                      x_start_value, x_end_value});
  }

  if (x_main.empty() || y_main.empty()) return;

  std::sort(x_main.begin(), x_main.end());
  InsertRectanglePredecences(x_main, precedences);
  std::sort(y_main.begin(), y_main.end());
  InsertRectanglePredecences(y_main, precedences);
}

}  // namespace

// TODO(user): We could scan for model precedences and add them to the list
// of precedences. This could enable more simplifications in the transitive
// reduction phase.
std::vector<std::pair<int, int>>
NeighborhoodGeneratorHelper::GetSchedulingPrecedences(
    const absl::flat_hash_set<int>& ignored_intervals,
    const CpSolverResponse& initial_solution, absl::BitGenRef random) const {
  absl::flat_hash_set<std::pair<int, int>> precedences;
  for (const int c : TypeToConstraints(ConstraintProto::kNoOverlap)) {
    InsertNoOverlapPrecedences(ignored_intervals, initial_solution,
                               ModelProto(), c, &precedences);
  }
  for (const int c : TypeToConstraints(ConstraintProto::kCumulative)) {
    InsertCumulativePrecedences(ignored_intervals, initial_solution,
                                ModelProto(), c, random, &precedences);
  }
  for (const int c : TypeToConstraints(ConstraintProto::kNoOverlap2D)) {
    InsertNoOverlap2dPrecedences(ignored_intervals, initial_solution,
                                 ModelProto(), c, &precedences);
  }

  // TODO(user): Reduce precedence graph
  std::vector<std::pair<int, int>> result(precedences.begin(),
                                          precedences.end());
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::vector<int>> NeighborhoodGeneratorHelper::GetRoutingPaths(
    const CpSolverResponse& initial_solution) const {
  struct HeadAndArcLiteral {
    int head;
    int literal;
  };

  std::vector<std::vector<int>> result;
  absl::flat_hash_map<int, HeadAndArcLiteral> tail_to_head_and_arc_literal;

  for (const int i : TypeToConstraints(ConstraintProto::kCircuit)) {
    const CircuitConstraintProto& ct = ModelProto().constraints(i).circuit();

    // Collect arcs.
    int min_node = std::numeric_limits<int>::max();
    tail_to_head_and_arc_literal.clear();
    for (int i = 0; i < ct.literals_size(); ++i) {
      const int literal = ct.literals(i);
      const int head = ct.heads(i);
      const int tail = ct.tails(i);
      const int bool_var = PositiveRef(literal);
      const int64_t value = initial_solution.solution(bool_var);
      // Skip unselected arcs.
      if (RefIsPositive(literal) == (value == 0)) continue;
      // Ignore self loops.
      if (head == tail) continue;
      tail_to_head_and_arc_literal[tail] = {head, bool_var};
      min_node = std::min(tail, min_node);
    }
    if (tail_to_head_and_arc_literal.empty()) continue;

    // Unroll the path.
    int current_node = min_node;
    std::vector<int> path;
    do {
      auto it = tail_to_head_and_arc_literal.find(current_node);
      CHECK(it != tail_to_head_and_arc_literal.end());
      current_node = it->second.head;
      path.push_back(it->second.literal);
    } while (current_node != min_node);
    result.push_back(std::move(path));
  }

  std::vector<HeadAndArcLiteral> route_starts;
  for (const int i : TypeToConstraints(ConstraintProto::kRoutes)) {
    const RoutesConstraintProto& ct = ModelProto().constraints(i).routes();
    tail_to_head_and_arc_literal.clear();
    route_starts.clear();

    // Collect route starts and arcs.
    for (int i = 0; i < ct.literals_size(); ++i) {
      const int literal = ct.literals(i);
      const int head = ct.heads(i);
      const int tail = ct.tails(i);
      const int bool_var = PositiveRef(literal);
      const int64_t value = initial_solution.solution(bool_var);
      // Skip unselected arcs.
      if (RefIsPositive(literal) == (value == 0)) continue;
      // Ignore self loops.
      if (head == tail) continue;
      if (tail == 0) {
        route_starts.push_back({head, bool_var});
      } else {
        tail_to_head_and_arc_literal[tail] = {head, bool_var};
      }
    }

    // Unroll all routes.
    for (const HeadAndArcLiteral& head_var : route_starts) {
      std::vector<int> path;
      int current_node = head_var.head;
      path.push_back(head_var.literal);
      do {
        auto it = tail_to_head_and_arc_literal.find(current_node);
        CHECK(it != tail_to_head_and_arc_literal.end());
        current_node = it->second.head;
        path.push_back(it->second.literal);
      } while (current_node != 0);
      result.push_back(std::move(path));
    }
  }

  return result;
}

Neighborhood NeighborhoodGeneratorHelper::FixGivenVariables(
    const CpSolverResponse& base_solution,
    const absl::flat_hash_set<int>& variables_to_fix) const {
  Neighborhood neighborhood;

  // Fill in neighborhood.delta all variable domains.
  {
    absl::ReaderMutexLock domain_lock(&domain_mutex_);

    const int num_variables =
        model_proto_with_only_variables_.variables().size();
    neighborhood.delta.mutable_variables()->Reserve(num_variables);
    for (int i = 0; i < num_variables; ++i) {
      const IntegerVariableProto& current_var =
          model_proto_with_only_variables_.variables(i);
      IntegerVariableProto* new_var = neighborhood.delta.add_variables();

      // We only copy the name in debug mode.
      if (DEBUG_MODE) new_var->set_name(current_var.name());

      const Domain domain = ReadDomainFromProto(current_var);
      const int64_t base_value = base_solution.solution(i);

      // It seems better to always start from a feasible point, so if the base
      // solution is no longer valid under the new up to date bound, we make
      // sure to relax the domain so that it is.
      if (!domain.Contains(base_value)) {
        // TODO(user): this can happen when variables_to_fix.contains(i). But we
        // should probably never consider as "active" such variable in the first
        // place.
        //
        // TODO(user): This might lead to incompatibility when the base solution
        // is not compatible with variable we fixed in a connected component! so
        // maybe it is not great to do that. Initial investigation didn't see
        // a big change. More work needed.
        FillDomainInProto(domain.UnionWith(Domain(base_solution.solution(i))),
                          new_var);
      } else if (variables_to_fix.contains(i)) {
        new_var->add_domain(base_value);
        new_var->add_domain(base_value);
      } else {
        FillDomainInProto(domain, new_var);
      }
    }
  }

  // Fill some statistic fields and detect if we cover a full component.
  //
  // TODO(user): If there is just one component, we can skip some computation.
  {
    absl::ReaderMutexLock graph_lock(&graph_mutex_);
    std::vector<int> count(components_.size(), 0);
    const int num_variables = neighborhood.delta.variables().size();
    for (int var = 0; var < num_variables; ++var) {
      const auto& domain = neighborhood.delta.variables(var).domain();
      if (domain.size() != 2 || domain[0] != domain[1]) {
        ++neighborhood.num_relaxed_variables;
        if (is_in_objective_[var]) {
          ++neighborhood.num_relaxed_variables_in_objective;
        }
        const int c = var_to_component_index_[var];
        if (c != -1) count[c]++;
      }
    }

    for (int i = 0; i < components_.size(); ++i) {
      if (count[i] == components_[i].size()) {
        neighborhood.variables_that_can_be_fixed_to_local_optimum.insert(
            neighborhood.variables_that_can_be_fixed_to_local_optimum.end(),
            components_[i].begin(), components_[i].end());
      }
    }
  }

  // If the objective domain might cut the optimal solution, we cannot exploit
  // the connected components. We compute this outside the mutex to avoid
  // any deadlock risk.
  //
  // TODO(user): We could handle some complex domain (size > 2).
  if (model_proto_.has_objective() &&
      (model_proto_.objective().domain().size() != 2 ||
       shared_response_->GetInnerObjectiveLowerBound() <
           model_proto_.objective().domain(0))) {
    neighborhood.variables_that_can_be_fixed_to_local_optimum.clear();
  }

  AddSolutionHinting(base_solution, &neighborhood.delta);

  neighborhood.is_generated = true;
  neighborhood.is_reduced = !variables_to_fix.empty();
  neighborhood.is_simple = true;

  // TODO(user): force better objective? Note that this is already done when the
  // hint above is successfully loaded (i.e. if it passes the presolve
  // correctly) since the solver will try to find better solution than the
  // current one.
  return neighborhood;
}

void NeighborhoodGeneratorHelper::AddSolutionHinting(
    const CpSolverResponse& initial_solution, CpModelProto* model_proto) const {
  // Set the current solution as a hint.
  model_proto->clear_solution_hint();
  const auto is_fixed = [model_proto](int var) {
    const IntegerVariableProto& var_proto = model_proto->variables(var);
    return var_proto.domain_size() == 2 &&
           var_proto.domain(0) == var_proto.domain(1);
  };
  for (int var = 0; var < model_proto->variables_size(); ++var) {
    if (is_fixed(var)) continue;

    model_proto->mutable_solution_hint()->add_vars(var);
    model_proto->mutable_solution_hint()->add_values(
        initial_solution.solution(var));
  }
}

Neighborhood NeighborhoodGeneratorHelper::RemoveMarkedConstraints(
    const std::vector<int>& constraints_to_remove) const {
  Neighborhood neighborhood = FullNeighborhood();

  if (constraints_to_remove.empty()) return neighborhood;
  neighborhood.is_reduced = false;
  neighborhood.constraints_to_ignore = constraints_to_remove;
  return neighborhood;
}

Neighborhood NeighborhoodGeneratorHelper::RelaxGivenVariables(
    const CpSolverResponse& initial_solution,
    const std::vector<int>& relaxed_variables) const {
  std::vector<bool> relaxed_variables_set(model_proto_.variables_size(), false);
  for (const int var : relaxed_variables) relaxed_variables_set[var] = true;
  absl::flat_hash_set<int> fixed_variables;
  {
    absl::ReaderMutexLock graph_lock(&graph_mutex_);
    for (const int i : active_variables_) {
      if (!relaxed_variables_set[i]) {
        fixed_variables.insert(i);
      }
    }
  }
  return FixGivenVariables(initial_solution, fixed_variables);
}

Neighborhood NeighborhoodGeneratorHelper::FixAllVariables(
    const CpSolverResponse& initial_solution) const {
  const std::vector<int>& all_variables = ActiveVariables();
  const absl::flat_hash_set<int> fixed_variables(all_variables.begin(),
                                                 all_variables.end());
  return FixGivenVariables(initial_solution, fixed_variables);
}

bool NeighborhoodGenerator::ReadyToGenerate() const {
  return (helper_.shared_response().SolutionsRepository().NumSolutions() > 0);
}

double NeighborhoodGenerator::GetUCBScore(int64_t total_num_calls) const {
  absl::ReaderMutexLock mutex_lock(&generator_mutex_);
  DCHECK_GE(total_num_calls, num_calls_);
  if (num_calls_ <= 10) return std::numeric_limits<double>::infinity();
  return current_average_ + sqrt((2 * log(total_num_calls)) / num_calls_);
}

void NeighborhoodGenerator::Synchronize() {
  absl::MutexLock mutex_lock(&generator_mutex_);

  // To make the whole update process deterministic, we currently sort the
  // SolveData.
  std::sort(solve_data_.begin(), solve_data_.end());

  // This will be used to update the difficulty of this neighborhood.
  int num_fully_solved_in_batch = 0;
  int num_not_fully_solved_in_batch = 0;

  for (const SolveData& data : solve_data_) {
    AdditionalProcessingOnSynchronize(data);
    ++num_calls_;

    // INFEASIBLE or OPTIMAL means that we "fully solved" the local problem.
    // If we didn't, then we cannot be sure that there is no improving solution
    // in that neighborhood.
    if (data.status == CpSolverStatus::INFEASIBLE ||
        data.status == CpSolverStatus::OPTIMAL) {
      ++num_fully_solved_calls_;
      ++num_fully_solved_in_batch;
    } else {
      ++num_not_fully_solved_in_batch;
    }

    // It seems to make more sense to compare the new objective to the base
    // solution objective, not the best one. However this causes issue in the
    // logic below because on some problems the neighborhood can always lead
    // to a better "new objective" if the base solution wasn't the best one.
    //
    // This might not be a final solution, but it does work ok for now.
    const IntegerValue best_objective_improvement = IntegerValue(CapSub(
        data.initial_best_objective.value(), data.new_objective.value()));
    if (best_objective_improvement > 0) {
      num_consecutive_non_improving_calls_ = 0;
    } else {
      ++num_consecutive_non_improving_calls_;
    }

    // TODO(user): Weight more recent data.
    // degrade the current average to forget old learnings.
    const double gain_per_time_unit =
        std::max(0.0, static_cast<double>(best_objective_improvement.value())) /
        (1.0 + data.deterministic_time);
    if (num_calls_ <= 100) {
      current_average_ += (gain_per_time_unit - current_average_) / num_calls_;
    } else {
      current_average_ = 0.9 * current_average_ + 0.1 * gain_per_time_unit;
    }

    deterministic_time_ += data.deterministic_time;
  }

  // Update the difficulty.
  difficulty_.Update(/*num_decreases=*/num_not_fully_solved_in_batch,
                     /*num_increases=*/num_fully_solved_in_batch);

  // Bump the time limit if we saw no better solution in the last few calls.
  // This means that as the search progress, we likely spend more and more time
  // trying to solve individual neighborhood.
  //
  // TODO(user): experiment with resetting the time limit if a solution is
  // found.
  if (num_consecutive_non_improving_calls_ > 50) {
    num_consecutive_non_improving_calls_ = 0;
    deterministic_limit_ *= 1.02;

    // We do not want the limit to go to high. Intuitively, the goal is to try
    // out a lot of neighborhoods, not just spend a lot of time on a few.
    deterministic_limit_ = std::min(60.0, deterministic_limit_);
  }

  solve_data_.clear();
}

namespace {

template <class T>
void GetRandomSubset(double relative_size, std::vector<T>* base,
                     absl::BitGenRef random) {
  if (base->empty()) return;

  // TODO(user): we could generate this more efficiently than using random
  // shuffle.
  std::shuffle(base->begin(), base->end(), random);
  const int target_size = std::round(relative_size * base->size());
  base->resize(target_size);
}

}  // namespace

Neighborhood RelaxRandomVariablesGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<int> fixed_variables = helper_.ActiveVariables();
  GetRandomSubset(1.0 - difficulty, &fixed_variables, random);
  return helper_.FixGivenVariables(
      initial_solution, {fixed_variables.begin(), fixed_variables.end()});
}

Neighborhood RelaxRandomConstraintsGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  if (helper_.DifficultyMeansFullNeighborhood(difficulty)) {
    return helper_.FullNeighborhood();
  }

  std::vector<int> relaxed_variables;
  {
    absl::ReaderMutexLock graph_lock(&helper_.graph_mutex_);
    const int num_active_constraints = helper_.ConstraintToVar().size();
    std::vector<int> active_constraints(num_active_constraints);
    for (int c = 0; c < num_active_constraints; ++c) {
      active_constraints[c] = c;
    }
    std::shuffle(active_constraints.begin(), active_constraints.end(), random);

    const int num_model_vars = helper_.ModelProto().variables_size();
    std::vector<bool> visited_variables_set(num_model_vars, false);

    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(difficulty * num_active_vars);
    DCHECK_GT(target_size, 0);

    for (const int constraint_index : active_constraints) {
      for (const int var : helper_.ConstraintToVar()[constraint_index]) {
        if (visited_variables_set[var]) continue;
        visited_variables_set[var] = true;
        if (helper_.IsActive(var)) {
          relaxed_variables.push_back(var);
          if (relaxed_variables.size() == target_size) break;
        }
      }
      if (relaxed_variables.size() == target_size) break;
    }
  }

  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

Neighborhood VariableGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  if (helper_.DifficultyMeansFullNeighborhood(difficulty)) {
    return helper_.FullNeighborhood();
  }

  const int num_model_vars = helper_.ModelProto().variables_size();
  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<int> visited_variables;

  // It is important complexity wise to never scan a constraint twice!
  const int num_model_constraints = helper_.ModelProto().constraints_size();
  std::vector<bool> scanned_constraints(num_model_constraints, false);

  std::vector<int> random_variables;
  {
    absl::ReaderMutexLock graph_lock(&helper_.graph_mutex_);

    // The number of active variables can decrease asynchronously.
    // We read the exact number while locked.
    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(difficulty * num_active_vars);
    if (target_size == 0) return helper_.FullNeighborhood();

    const int first_var =
        helper_.ActiveVariablesWhileHoldingLock()[absl::Uniform<int>(
            random, 0, num_active_vars)];

    visited_variables_set[first_var] = true;
    visited_variables.push_back(first_var);
    relaxed_variables.push_back(first_var);

    for (int i = 0; i < visited_variables.size(); ++i) {
      random_variables.clear();
      // Collect all the variables that appears in the same constraints as
      // visited_variables[i].
      for (const int ct : helper_.VarToConstraint()[visited_variables[i]]) {
        if (scanned_constraints[ct]) continue;
        scanned_constraints[ct] = true;
        for (const int var : helper_.ConstraintToVar()[ct]) {
          if (visited_variables_set[var]) continue;
          visited_variables_set[var] = true;
          random_variables.push_back(var);
        }
      }
      // We always randomize to change the partial subgraph explored
      // afterwards.
      std::shuffle(random_variables.begin(), random_variables.end(), random);
      for (const int var : random_variables) {
        if (relaxed_variables.size() < target_size) {
          visited_variables.push_back(var);
          if (helper_.IsActive(var)) {
            relaxed_variables.push_back(var);
          }
        } else {
          break;
        }
      }
      if (relaxed_variables.size() >= target_size) break;
    }
  }
  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

Neighborhood ConstraintGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  const int num_model_constraints = helper_.ModelProto().constraints_size();
  if (num_model_constraints == 0 ||
      helper_.DifficultyMeansFullNeighborhood(difficulty)) {
    return helper_.FullNeighborhood();
  }

  const int num_model_vars = helper_.ModelProto().variables_size();
  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;

  std::vector<bool> added_constraints(num_model_constraints, false);
  std::vector<int> next_constraints;

  std::vector<int> random_variables;
  {
    absl::ReaderMutexLock graph_lock(&helper_.graph_mutex_);
    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(difficulty * num_active_vars);
    if (target_size == 0) return helper_.FullNeighborhood();

    // Start by a random constraint.
    const int num_active_constraints = helper_.ConstraintToVar().size();
    if (num_active_constraints != 0) {
      next_constraints.push_back(
          absl::Uniform<int>(random, 0, num_active_constraints));
      added_constraints[next_constraints.back()] = true;
    }

    while (relaxed_variables.size() < target_size) {
      // Stop if we have a full connected component.
      if (next_constraints.empty()) break;

      // Pick a random unprocessed constraint.
      const int i = absl::Uniform<int>(random, 0, next_constraints.size());
      const int constraint_index = next_constraints[i];
      std::swap(next_constraints[i], next_constraints.back());
      next_constraints.pop_back();

      // Add all the variable of this constraint and increase the set of next
      // possible constraints.
      DCHECK_LT(constraint_index, num_active_constraints);
      random_variables = helper_.ConstraintToVar()[constraint_index];
      std::shuffle(random_variables.begin(), random_variables.end(), random);
      for (const int var : random_variables) {
        if (visited_variables_set[var]) continue;
        visited_variables_set[var] = true;
        if (helper_.IsActive(var)) {
          relaxed_variables.push_back(var);
        }
        if (relaxed_variables.size() == target_size) break;

        for (const int ct : helper_.VarToConstraint()[var]) {
          if (added_constraints[ct]) continue;
          added_constraints[ct] = true;
          next_constraints.push_back(ct);
        }
      }
    }
  }
  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

namespace {

void AddPrecedence(const LinearExpressionProto& before,
                   const LinearExpressionProto& after, CpModelProto* model) {
  LinearConstraintProto* linear = model->add_constraints()->mutable_linear();
  linear->add_domain(std::numeric_limits<int64_t>::min());
  linear->add_domain(after.offset() - before.offset());
  for (int i = 0; i < before.vars_size(); ++i) {
    linear->add_vars(before.vars(i));
    linear->add_coeffs(before.coeffs(i));
  }
  for (int i = 0; i < after.vars_size(); ++i) {
    linear->add_vars(after.vars(i));
    linear->add_coeffs(-after.coeffs(i));
  }
}

}  // namespace

Neighborhood GenerateSchedulingNeighborhoodFromIntervalPrecedences(
    const absl::Span<const std::pair<int, int>> precedences,
    const CpSolverResponse& initial_solution,
    const NeighborhoodGeneratorHelper& helper) {
  Neighborhood neighborhood = helper.FullNeighborhood();

  neighborhood.is_reduced = !precedences.empty();
  if (!neighborhood.is_reduced) {  // Returns the full neighborhood.
    helper.AddSolutionHinting(initial_solution, &neighborhood.delta);
    neighborhood.is_generated = true;
    return neighborhood;
  }

  // Collect seen intervals.
  absl::flat_hash_set<int> seen_intervals;
  for (const std::pair<int, int>& prec : precedences) {
    seen_intervals.insert(prec.first);
    seen_intervals.insert(prec.second);
  }

  // Fix the presence/absence of unseen intervals.
  bool enforcement_literals_fixed = false;
  for (const int i : helper.TypeToConstraints(ConstraintProto::kInterval)) {
    if (seen_intervals.contains(i)) continue;

    const ConstraintProto& interval_ct = helper.ModelProto().constraints(i);
    if (interval_ct.enforcement_literal().empty()) continue;

    DCHECK_EQ(interval_ct.enforcement_literal().size(), 1);
    const int enforcement_ref = interval_ct.enforcement_literal(0);
    const int enforcement_var = PositiveRef(enforcement_ref);
    const int value = initial_solution.solution(enforcement_var);

    // If the interval is not enforced, we just relax it. If it belongs to an
    // exactly one constraint, and the enforced interval is not relaxed, then
    // propagation will force this interval to stay not enforced. Otherwise,
    // LNS will be able to change which interval will be enforced among all
    // alternatives.
    if (RefIsPositive(enforcement_ref) == (value == 0)) continue;

    // Fix the value.
    neighborhood.delta.mutable_variables(enforcement_var)->clear_domain();
    neighborhood.delta.mutable_variables(enforcement_var)->add_domain(value);
    neighborhood.delta.mutable_variables(enforcement_var)->add_domain(value);
    enforcement_literals_fixed = true;
  }

  for (const std::pair<int, int>& prec : precedences) {
    const LinearExpressionProto& before_end =
        helper.ModelProto().constraints(prec.first).interval().end();
    const LinearExpressionProto& after_start =
        helper.ModelProto().constraints(prec.second).interval().start();
    DCHECK_LE(GetLinearExpressionValue(before_end, initial_solution),
              GetLinearExpressionValue(after_start, initial_solution));
    AddPrecedence(before_end, after_start, &neighborhood.delta);
  }

  // Set the current solution as a hint.
  helper.AddSolutionHinting(initial_solution, &neighborhood.delta);
  neighborhood.is_generated = true;

  return neighborhood;
}

Neighborhood GenerateSchedulingNeighborhoodFromRelaxedIntervals(
    const absl::Span<const int> intervals_to_relax,
    const CpSolverResponse& initial_solution, absl::BitGenRef random,
    const NeighborhoodGeneratorHelper& helper) {
  Neighborhood neighborhood = helper.FullNeighborhood();

  // We will extend the set with some interval that we cannot fix.
  absl::flat_hash_set<int> ignored_intervals(intervals_to_relax.begin(),
                                             intervals_to_relax.end());

  // Fix the presence/absence of non-relaxed intervals.
  for (const int i : helper.TypeToConstraints(ConstraintProto::kInterval)) {
    DCHECK_GE(i, 0);
    if (ignored_intervals.contains(i)) continue;

    const ConstraintProto& interval_ct = helper.ModelProto().constraints(i);
    if (interval_ct.enforcement_literal().empty()) continue;

    DCHECK_EQ(interval_ct.enforcement_literal().size(), 1);
    const int enforcement_ref = interval_ct.enforcement_literal(0);
    const int enforcement_var = PositiveRef(enforcement_ref);
    const int value = initial_solution.solution(enforcement_var);

    // If the interval is not enforced, we just relax it. If it belongs to an
    // exactly one constraint, and the enforced interval is not relaxed, then
    // propagation will force this interval to stay not enforced. Otherwise,
    // LNS will be able to change which interval will be enforced among all
    // alternatives.
    if (RefIsPositive(enforcement_ref) == (value == 0)) {
      ignored_intervals.insert(i);
      continue;
    }

    // Fix the value.
    neighborhood.delta.mutable_variables(enforcement_var)->clear_domain();
    neighborhood.delta.mutable_variables(enforcement_var)->add_domain(value);
    neighborhood.delta.mutable_variables(enforcement_var)->add_domain(value);
  }

  if (ignored_intervals.size() >=
      helper.TypeToConstraints(ConstraintProto::kInterval)
          .size()) {  // Returns the full neighborhood.
    helper.AddSolutionHinting(initial_solution, &neighborhood.delta);
    neighborhood.is_generated = true;
    return neighborhood;
  }

  neighborhood.is_reduced = true;

  // We differ from the ICAPS05 paper as we do not consider ignored intervals
  // when generating the precedence graph, instead of building the full graph,
  // then removing intervals, and reconstructing the precedence graph
  // heuristically after that.
  const std::vector<std::pair<int, int>> precedences =
      helper.GetSchedulingPrecedences(ignored_intervals, initial_solution,
                                      random);
  for (const std::pair<int, int>& prec : precedences) {
    const LinearExpressionProto& before_end =
        helper.ModelProto().constraints(prec.first).interval().end();
    const LinearExpressionProto& after_start =
        helper.ModelProto().constraints(prec.second).interval().start();
    DCHECK_LE(GetLinearExpressionValue(before_end, initial_solution),
              GetLinearExpressionValue(after_start, initial_solution));
    AddPrecedence(before_end, after_start, &neighborhood.delta);
  }

  // Set the current solution as a hint.
  helper.AddSolutionHinting(initial_solution, &neighborhood.delta);
  neighborhood.is_generated = true;

  return neighborhood;
}

Neighborhood RandomIntervalSchedulingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<int> intervals_to_relax =
      helper_.GetActiveIntervals(initial_solution);
  GetRandomSubset(difficulty, &intervals_to_relax, random);

  return GenerateSchedulingNeighborhoodFromRelaxedIntervals(
      intervals_to_relax, initial_solution, random, helper_);
}

Neighborhood RandomPrecedenceSchedulingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<std::pair<int, int>> precedences =
      helper_.GetSchedulingPrecedences({}, initial_solution, random);
  GetRandomSubset(1.0 - difficulty, &precedences, random);
  return GenerateSchedulingNeighborhoodFromIntervalPrecedences(
      precedences, initial_solution, helper_);
}

Neighborhood SchedulingTimeWindowNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  const std::vector<int> active_intervals =
      helper_.GetActiveIntervals(initial_solution);

  if (active_intervals.empty()) return helper_.FullNeighborhood();

  const std::vector<int> intervals_to_relax =
      SelectIntervalsInRandomTimeWindow(active_intervals, helper_.ModelProto(),
                                        initial_solution, difficulty, random);
  return GenerateSchedulingNeighborhoodFromRelaxedIntervals(
      intervals_to_relax, initial_solution, random, helper_);
}

Neighborhood SchedulingResourceWindowsNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  intervals_to_relax_.clear();
  for (const std::vector<int>& intervals : intervals_in_constraints_) {
    const std::vector<int> selected = SelectIntervalsInRandomTimeWindow(
        intervals, helper_.ModelProto(), initial_solution, difficulty, random);
    intervals_to_relax_.insert(selected.begin(), selected.end());
  }

  if (intervals_to_relax_.empty()) return helper_.FullNeighborhood();

  const std::vector<int> intervals(
      {intervals_to_relax_.begin(), intervals_to_relax_.end()});
  return GenerateSchedulingNeighborhoodFromRelaxedIntervals(
      intervals, initial_solution, random, helper_);
}

Neighborhood RoutingRandomNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  const std::vector<std::vector<int>> all_paths =
      helper_.GetRoutingPaths(initial_solution);

  // Collect all unique variables.
  absl::flat_hash_set<int> all_path_variables;
  for (auto& path : all_paths) {
    all_path_variables.insert(path.begin(), path.end());
  }
  std::vector<int> fixed_variables(all_path_variables.begin(),
                                   all_path_variables.end());
  std::sort(fixed_variables.begin(), fixed_variables.end());
  GetRandomSubset(1.0 - difficulty, &fixed_variables, random);
  return helper_.FixGivenVariables(
      initial_solution, {fixed_variables.begin(), fixed_variables.end()});
}

Neighborhood RoutingPathNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<std::vector<int>> all_paths =
      helper_.GetRoutingPaths(initial_solution);

  // Collect all unique variables.
  absl::flat_hash_set<int> all_path_variables;
  for (const auto& path : all_paths) {
    all_path_variables.insert(path.begin(), path.end());
  }

  // Select variables to relax.
  const int num_variables_to_relax =
      static_cast<int>(all_path_variables.size() * difficulty);
  absl::flat_hash_set<int> relaxed_variables;
  while (relaxed_variables.size() < num_variables_to_relax) {
    DCHECK(!all_paths.empty());
    const int path_index = absl::Uniform<int>(random, 0, all_paths.size());
    std::vector<int>& path = all_paths[path_index];
    const int path_size = path.size();
    const int segment_length =
        std::min(path_size, absl::Uniform<int>(random, 4, 8));
    const int segment_start =
        absl::Uniform<int>(random, 0, path_size - segment_length);
    for (int i = segment_start; i < segment_start + segment_length; ++i) {
      relaxed_variables.insert(path[i]);
    }

    // Remove segment and clean up empty paths.
    path.erase(path.begin() + segment_start,
               path.begin() + segment_start + segment_length);
    if (path.empty()) {
      std::swap(all_paths[path_index], all_paths.back());
      all_paths.pop_back();
    }
  }

  // Compute the set of variables to fix.
  absl::flat_hash_set<int> fixed_variables;
  for (const int var : all_path_variables) {
    if (!relaxed_variables.contains(var)) fixed_variables.insert(var);
  }
  return helper_.FixGivenVariables(initial_solution, fixed_variables);
}

Neighborhood RoutingFullPathNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<std::vector<int>> all_paths =
      helper_.GetRoutingPaths(initial_solution);
  // Remove a corner case where all paths are empty.
  if (all_paths.empty()) {
    return helper_.NoNeighborhood();
  }

  // Collect all unique variables.
  absl::flat_hash_set<int> all_path_variables;
  for (const auto& path : all_paths) {
    all_path_variables.insert(path.begin(), path.end());
  }

  // Select variables to relax.
  const int num_variables_to_relax =
      static_cast<int>(all_path_variables.size() * difficulty);
  absl::flat_hash_set<int> relaxed_variables;

  // Relax the start and end of each path to ease relocation.
  for (const auto& path : all_paths) {
    relaxed_variables.insert(path.front());
    relaxed_variables.insert(path.back());
  }

  // Randomize paths.
  for (auto& path : all_paths) {
    std::shuffle(path.begin(), path.end(), random);
  }

  // Relax all variables (if possible) in one random path.
  const int path_to_clean = absl::Uniform<int>(random, 0, all_paths.size());
  while (relaxed_variables.size() < num_variables_to_relax &&
         !all_paths[path_to_clean].empty()) {
    relaxed_variables.insert(all_paths[path_to_clean].back());
    all_paths[path_to_clean].pop_back();
  }
  if (all_paths[path_to_clean].empty()) {
    std::swap(all_paths[path_to_clean], all_paths.back());
    all_paths.pop_back();
  }

  // Relax more variables until the target is reached.
  while (relaxed_variables.size() < num_variables_to_relax) {
    DCHECK(!all_paths.empty());
    const int path_index = absl::Uniform<int>(random, 0, all_paths.size());
    relaxed_variables.insert(all_paths[path_index].back());

    // Remove variable and clean up empty paths.
    all_paths[path_index].pop_back();
    if (all_paths[path_index].empty()) {
      std::swap(all_paths[path_index], all_paths.back());
      all_paths.pop_back();
    }
  }

  // Compute the set of variables to fix.
  absl::flat_hash_set<int> fixed_variables;
  for (const int var : all_path_variables) {
    if (!relaxed_variables.contains(var)) fixed_variables.insert(var);
  }
  return helper_.FixGivenVariables(initial_solution, fixed_variables);
}

bool RelaxationInducedNeighborhoodGenerator::ReadyToGenerate() const {
  if (incomplete_solutions_ != nullptr) {
    return incomplete_solutions_->HasNewSolution();
  }

  if (response_manager_ != nullptr) {
    if (response_manager_->SolutionsRepository().NumSolutions() == 0) {
      return false;
    }
  }

  // At least one relaxation solution should be available to generate a
  // neighborhood.
  if (lp_solutions_ != nullptr && lp_solutions_->NumSolutions() > 0) {
    return true;
  }

  if (relaxation_solutions_ != nullptr &&
      relaxation_solutions_->NumSolutions() > 0) {
    return true;
  }
  return false;
}

Neighborhood RelaxationInducedNeighborhoodGenerator::Generate(
    const CpSolverResponse& /*initial_solution*/, double /*difficulty*/,
    absl::BitGenRef random) {
  Neighborhood neighborhood = helper_.FullNeighborhood();
  neighborhood.is_generated = false;

  const bool lp_solution_available =
      (lp_solutions_ != nullptr && lp_solutions_->NumSolutions() > 0);

  const bool relaxation_solution_available =
      (relaxation_solutions_ != nullptr &&
       relaxation_solutions_->NumSolutions() > 0);

  const bool incomplete_solution_available =
      (incomplete_solutions_ != nullptr &&
       incomplete_solutions_->HasNewSolution());

  if (!lp_solution_available && !relaxation_solution_available &&
      !incomplete_solution_available) {
    return neighborhood;
  }

  RINSNeighborhood rins_neighborhood;
  // Randomly select the type of relaxation if both lp and relaxation solutions
  // are available.
  // TODO(user): Tune the probability value for this.
  std::bernoulli_distribution random_bool(0.5);
  const bool use_lp_relaxation =
      (lp_solution_available && relaxation_solution_available)
          ? random_bool(random)
          : lp_solution_available;
  if (use_lp_relaxation) {
    rins_neighborhood =
        GetRINSNeighborhood(response_manager_,
                            /*relaxation_solutions=*/nullptr, lp_solutions_,
                            incomplete_solutions_, random);
    neighborhood.source_info =
        incomplete_solution_available ? "incomplete" : "lp";
  } else {
    CHECK(relaxation_solution_available || incomplete_solution_available);
    rins_neighborhood = GetRINSNeighborhood(
        response_manager_, relaxation_solutions_,
        /*lp_solutions=*/nullptr, incomplete_solutions_, random);
    neighborhood.source_info =
        incomplete_solution_available ? "incomplete" : "relaxation";
  }

  if (rins_neighborhood.fixed_vars.empty() &&
      rins_neighborhood.reduced_domain_vars.empty()) {
    return neighborhood;
  }

  absl::ReaderMutexLock graph_lock(&helper_.graph_mutex_);
  // Fix the variables in the local model.
  for (const std::pair</*model_var*/ int, /*value*/ int64_t>& fixed_var :
       rins_neighborhood.fixed_vars) {
    const int var = fixed_var.first;
    const int64_t value = fixed_var.second;
    if (var >= neighborhood.delta.variables_size()) continue;
    if (!helper_.IsActive(var)) continue;

    if (!DomainInProtoContains(neighborhood.delta.variables(var), value)) {
      // TODO(user): Instead of aborting, pick the closest point in the domain?
      return neighborhood;
    }

    neighborhood.delta.mutable_variables(var)->clear_domain();
    neighborhood.delta.mutable_variables(var)->add_domain(value);
    neighborhood.delta.mutable_variables(var)->add_domain(value);
    neighborhood.is_reduced = true;
  }

  for (const std::pair</*model_var*/ int,
                       /*domain*/ std::pair<int64_t, int64_t>>& reduced_var :
       rins_neighborhood.reduced_domain_vars) {
    const int var = reduced_var.first;
    const int64_t lb = reduced_var.second.first;
    const int64_t ub = reduced_var.second.second;
    if (var >= neighborhood.delta.variables_size()) continue;
    if (!helper_.IsActive(var)) continue;
    Domain domain = ReadDomainFromProto(neighborhood.delta.variables(var));
    domain = domain.IntersectionWith(Domain(lb, ub));
    if (domain.IsEmpty()) {
      // TODO(user): Instead of aborting, pick the closest point in the
      // domain?
      return neighborhood;
    }
    FillDomainInProto(domain, neighborhood.delta.mutable_variables(var));
    neighborhood.is_reduced = true;
  }
  neighborhood.is_generated = true;
  return neighborhood;
}

}  // namespace sat
}  // namespace operations_research
