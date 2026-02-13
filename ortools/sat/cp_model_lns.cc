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

#include "ortools/sat/cp_model_lns.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "ortools/base/macros/buildenv.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_copy.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_solver_helpers.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/adaptative_parameter_value.h"
#include "ortools/util/bitset.h"
#include "ortools/util/integer_pq.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

NeighborhoodGeneratorHelper::NeighborhoodGeneratorHelper(
    CpModelProto const* model_proto, SatParameters const* parameters,
    SharedResponseManager* shared_response,
    ModelSharedTimeLimit* global_time_limit, SharedBoundsManager* shared_bounds)
    : SubSolver("neighborhood_helper", HELPER),
      parameters_(*parameters),
      model_proto_(*model_proto),
      shared_bounds_(shared_bounds),
      global_time_limit_(global_time_limit),
      shared_response_(shared_response) {
  // Initialize proto memory.
  local_arena_storage_.assign(Neighborhood::kDefaultArenaSizePerVariable *
                                  model_proto_.variables_size(),
                              0);
  local_arena_ = std::make_unique<google::protobuf::Arena>(
      local_arena_storage_.data(), local_arena_storage_.size());
  simplified_model_proto_ =
      google::protobuf::Arena::Create<CpModelProto>(local_arena_.get());

  CHECK(shared_response_ != nullptr);
  if (shared_bounds_ != nullptr) {
    shared_bounds_id_ = shared_bounds_->RegisterNewId("neighborhood_helper");
  }
  *model_proto_with_only_variables_.mutable_variables() =
      model_proto_.variables();
  InitializeHelperData();
  RecomputeHelperData();
  Synchronize();
}

void NeighborhoodGeneratorHelper::Synchronize() {
  if (shared_response_->ProblemIsSolved() ||
      global_time_limit_->LimitReached()) {
    return;
  }
  if (shared_bounds_ != nullptr) {
    std::vector<int> model_variables;
    std::vector<int64_t> new_lower_bounds;
    std::vector<int64_t> new_upper_bounds;
    shared_bounds_->GetChangedBounds(shared_bounds_id_, &model_variables,
                                     &new_lower_bounds, &new_upper_bounds);

    bool new_variables_have_been_fixed = false;

    if (!model_variables.empty()) {
      absl::MutexLock domain_lock(domain_mutex_);

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
  has_positive_objective_coefficient_.resize(num_variables, false);
  if (model_proto_.has_objective()) {
    for (int i = 0; i < model_proto_.objective().vars_size(); ++i) {
      const int ref = model_proto_.objective().vars(i);
      const int64_t coeff = model_proto_.objective().coeffs(i);
      DCHECK_NE(coeff, 0);
      is_in_objective_[PositiveRef(ref)] = true;
      has_positive_objective_coefficient_[PositiveRef(ref)] =
          ref == PositiveRef(ref) ? coeff > 0 : coeff < 0;
    }
  }
}

// Recompute all the data when new variables have been fixed. Note that this
// shouldn't be called if there is no change as it is in O(problem size).
void NeighborhoodGeneratorHelper::RecomputeHelperData() {
  absl::MutexLock graph_lock(graph_mutex_);
  absl::ReaderMutexLock domain_lock(domain_mutex_);

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
    // We want to replace the simplified_model_proto_ by a new one. Since
    // deleting an object in the arena doesn't free the memory, we also delete
    // and recreate the arena, but reusing the same storage.
    int64_t new_size = local_arena_->SpaceUsed();
    new_size += new_size / 2;
    simplified_model_proto_->Clear();
    local_arena_.reset();
    local_arena_storage_.resize(new_size);
    local_arena_ = std::make_unique<google::protobuf::Arena>(
        local_arena_storage_.data(), local_arena_storage_.size());
    simplified_model_proto_ =
        google::protobuf::Arena::Create<CpModelProto>(local_arena_.get());
    *simplified_model_proto_->mutable_variables() =
        model_proto_with_only_variables_.variables();
    PresolveContext context(&local_model, simplified_model_proto_,
                            &mapping_proto);
    ModelCopy copier(&context);

    // TODO(user): Not sure what to do if the model is UNSAT.
    // This  shouldn't matter as it should be dealt with elsewhere.
    copier.ImportAndSimplifyConstraints(model_proto_, {});
  }

  // Compute the constraint <-> variable graph.
  //
  // TODO(user): Remove duplicate constraints?
  const auto& constraints = simplified_model_proto_->constraints();
  constraint_to_var_.clear();
  constraint_to_var_.reserve(constraints.size());
  for (int ct_index = 0; ct_index < constraints.size(); ++ct_index) {
    // We remove the interval constraints since we should have an equivalent
    // linear constraint somewhere else. This is not the case if we have a fixed
    // size optional interval variable. But it should not matter as the
    // intervals are replaced by their underlying variables in the scheduling
    // constraints.
    if (constraints[ct_index].constraint_case() == ConstraintProto::kInterval) {
      continue;
    }

    tmp_row_.clear();
    for (const int var : UsedVariables(constraints[ct_index])) {
      if (IsConstant(var)) continue;
      tmp_row_.push_back(var);
    }

    // We replace intervals by their underlying integer variables. Note that
    // this is needed for a correct decomposition into independent part.
    bool need_sort = false;
    for (const int interval : UsedIntervals(constraints[ct_index])) {
      need_sort = true;
      for (const int var : UsedVariables(constraints[interval])) {
        if (IsConstant(var)) continue;
        tmp_row_.push_back(var);
      }
    }

    // We remove constraint of size 0 and 1 since they are not useful for LNS
    // based on this graph.
    if (tmp_row_.size() <= 1) {
      continue;
    }

    // Keep this constraint.
    if (need_sort) {
      gtl::STLSortAndRemoveDuplicates(&tmp_row_);
    }
    constraint_to_var_.Add(tmp_row_);
  }

  // Initialize var to constraints, and make sure it has an entry for all
  // variables.
  var_to_constraint_.ResetFromTranspose(
      constraint_to_var_,
      /*min_transpose_size=*/model_proto_.variables().size());

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

  active_objective_variables_.clear();
  for (const int var : model_proto_.objective().vars()) {
    DCHECK(RefIsPositive(var));
    if (active_variables_set_[var]) {
      active_objective_variables_.push_back(var);
    }
  }

  // Compute connected components.
  // Note that fixed variable are just ignored.
  DenseConnectedComponentsFinder union_find;
  union_find.SetNumberOfNodes(num_variables);
  for (int c = 0; c < constraint_to_var_.size(); ++c) {
    const auto row = constraint_to_var_[c];
    if (row.size() <= 1) continue;
    for (int i = 1; i < row.size(); ++i) {
      union_find.AddEdge(row[0], row[i]);
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
  shared_response_->LogMessageWithThrottling(
      "Model", absl::StrCat("var:", active_variables_.size(), "/",
                            num_variables, " constraints:",
                            simplified_model_proto_->constraints().size(), "/",
                            model_proto_.constraints().size(), compo_message));
}

bool NeighborhoodGeneratorHelper::IsActive(int var) const {
  return active_variables_set_[var];
}

bool NeighborhoodGeneratorHelper::IsConstant(int var) const {
  const auto& var_proto = model_proto_with_only_variables_.variables(var);
  return var_proto.domain_size() == 2 &&
         var_proto.domain(0) == var_proto.domain(1);
}

Neighborhood NeighborhoodGeneratorHelper::FullNeighborhood() const {
  Neighborhood neighborhood;
  neighborhood.is_reduced = false;
  neighborhood.is_generated = true;
  {
    absl::ReaderMutexLock lock(domain_mutex_);
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

bool NeighborhoodGeneratorHelper::IntervalIsActive(
    int index, const CpSolverResponse& initial_solution) const {
  const ConstraintProto& interval_ct = ModelProto().constraints(index);
  // We only look at intervals that are performed in the solution. The
  // unperformed intervals should be automatically freed during the generation
  // phase.
  if (interval_ct.enforcement_literal().size() == 1) {
    const int enforcement_ref = interval_ct.enforcement_literal(0);
    const int enforcement_var = PositiveRef(enforcement_ref);
    const int value = initial_solution.solution(enforcement_var);
    if (RefIsPositive(enforcement_ref) == (value == 0)) return false;
  }

  for (const int v : interval_ct.interval().start().vars()) {
    if (!IsConstant(v)) return true;
  }
  for (const int v : interval_ct.interval().size().vars()) {
    if (!IsConstant(v)) return true;
  }
  for (const int v : interval_ct.interval().end().vars()) {
    if (!IsConstant(v)) return true;
  }
  return false;
}

std::vector<int> NeighborhoodGeneratorHelper::KeepActiveIntervals(
    absl::Span<const int> unfiltered_intervals,
    const CpSolverResponse& initial_solution) const {
  std::vector<int> filtered_intervals;
  filtered_intervals.reserve(unfiltered_intervals.size());
  absl::ReaderMutexLock lock(domain_mutex_);
  for (const int i : unfiltered_intervals) {
    if (IntervalIsActive(i, initial_solution)) filtered_intervals.push_back(i);
  }
  return filtered_intervals;
}

std::vector<int> NeighborhoodGeneratorHelper::GetActiveIntervals(
    const CpSolverResponse& initial_solution) const {
  return KeepActiveIntervals(TypeToConstraints(ConstraintProto::kInterval),
                             initial_solution);
}

std::vector<NeighborhoodGeneratorHelper::ActiveRectangle>
NeighborhoodGeneratorHelper::GetActiveRectangles(
    const CpSolverResponse& initial_solution) const {
  const std::vector<int> active_intervals =
      GetActiveIntervals(initial_solution);
  const absl::flat_hash_set<int> active_intervals_set(active_intervals.begin(),
                                                      active_intervals.end());

  absl::flat_hash_map<std::pair<int, int>, std::vector<int>> active_rectangles;
  for (const int ct_index : TypeToConstraints(ConstraintProto::kNoOverlap2D)) {
    const NoOverlap2DConstraintProto& ct =
        model_proto_.constraints(ct_index).no_overlap_2d();
    for (int i = 0; i < ct.x_intervals_size(); ++i) {
      const int x_i = ct.x_intervals(i);
      const int y_i = ct.y_intervals(i);
      if (active_intervals_set.contains(x_i) ||
          active_intervals_set.contains(y_i)) {
        active_rectangles[{x_i, y_i}].push_back(ct_index);
      }
    }
  }

  std::vector<ActiveRectangle> results;
  results.reserve(active_rectangles.size());
  for (const auto& [rectangle, no_overlap_2d_constraints] : active_rectangles) {
    ActiveRectangle& result = results.emplace_back();
    result.x_interval = rectangle.first;
    result.y_interval = rectangle.second;
    result.no_overlap_2d_constraints = {no_overlap_2d_constraints.begin(),
                                        no_overlap_2d_constraints.end()};
  }
  return results;
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

void RestrictAffineExpression(const LinearExpressionProto& expr,
                              const Domain& restriction,
                              CpModelProto* mutable_proto) {
  CHECK_LE(expr.vars().size(), 1);
  if (expr.vars().empty()) return;
  const Domain implied_domain = restriction.AdditionWith(Domain(-expr.offset()))
                                    .InverseMultiplicationBy(expr.coeffs(0));
  const Domain domain =
      ReadDomainFromProto(mutable_proto->variables(expr.vars(0)))
          .IntersectionWith(implied_domain);
  if (!domain.IsEmpty()) {
    FillDomainInProto(domain, mutable_proto->mutable_variables(expr.vars(0)));
  }
}

struct StartEndIndex {
  int64_t start;
  int64_t end;
  int index_in_input_vector;
  double noise;
  bool operator<(const StartEndIndex& o) const {
    return std::tie(start, end, noise, index_in_input_vector) <
           std::tie(o.start, o.end, o.noise, o.index_in_input_vector);
  }
};

struct TimePartition {
  std::vector<int> indices_before_selected;
  std::vector<int> selected_indices;
  std::vector<int> indices_after_selected;
};

// Selects all intervals in a random time window to meet the difficulty
// requirement.
TimePartition PartitionIndicesAroundRandomTimeWindow(
    absl::Span<const int> intervals, const CpModelProto& model_proto,
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<StartEndIndex> start_end_indices;
  for (int index = 0; index < intervals.size(); ++index) {
    const int interval = intervals[index];
    const ConstraintProto& interval_ct = model_proto.constraints(interval);
    const int64_t start_value = GetLinearExpressionValue(
        interval_ct.interval().start(), initial_solution);
    const int64_t end_value = GetLinearExpressionValue(
        interval_ct.interval().end(), initial_solution);
    start_end_indices.push_back(
        {start_value, end_value, index, absl::Uniform(random, 0., 1.0)});
  }

  if (start_end_indices.empty()) return {};

  std::sort(start_end_indices.begin(), start_end_indices.end());
  const int relaxed_size = std::floor(difficulty * start_end_indices.size());

  std::uniform_int_distribution<int> random_var(
      0, start_end_indices.size() - relaxed_size - 1);
  // TODO(user): Consider relaxing more than one time window
  // intervals. This seems to help with Giza models.
  const int random_start_index = random_var(random);

  // We want to minimize the time window relaxed, so we now sort the interval
  // after the first selected intervals by end value.
  // TODO(user): We could do things differently (include all tasks <= some
  // end). The difficulty is that the number of relaxed tasks will differ from
  // the target. We could also tie break tasks randomly.
  std::sort(start_end_indices.begin() + random_start_index,
            start_end_indices.end(),
            [](const StartEndIndex& a, const StartEndIndex& b) {
              return std::tie(a.end, a.noise, a.index_in_input_vector) <
                     std::tie(b.end, b.noise, b.index_in_input_vector);
            });
  TimePartition result;
  int i = 0;
  for (; i < random_start_index; ++i) {
    result.indices_before_selected.push_back(
        start_end_indices[i].index_in_input_vector);
  }
  for (; i < random_start_index + relaxed_size; ++i) {
    result.selected_indices.push_back(
        start_end_indices[i].index_in_input_vector);
  }
  for (; i < start_end_indices.size(); ++i) {
    result.indices_after_selected.push_back(
        start_end_indices[i].index_in_input_vector);
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
  // heights first will decrease its probability of being split across the 2
  // halves of the current split.
  bool operator<(const Demand& other) const {
    return std::tie(start, height, end) <
           std::tie(other.start, other.height, other.end);
  }

  std::string DebugString() const {
    return absl::StrCat("{i=", interval_index, " span=[", start, ",", end, "]",
                        " d=", height, "}");
  }
};

void InsertPrecedencesFromSortedListOfNonOverlapingIntervals(
    const std::vector<Demand>& demands,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  for (int i = 0; i + 1 < demands.size(); ++i) {
    DCHECK_LE(demands[i].end, demands[i + 1].start);
    precedences->insert(
        {demands[i].interval_index, demands[i + 1].interval_index});
  }
}

bool IsPresent(const ConstraintProto& interval_ct,
               const CpSolverResponse& initial_solution) {
  if (interval_ct.enforcement_literal().size() != 1) return true;

  const int enforcement_ref = interval_ct.enforcement_literal(0);
  const int enforcement_var = PositiveRef(enforcement_ref);
  const int64_t value = initial_solution.solution(enforcement_var);
  return RefIsPositive(enforcement_ref) == (value == 1);
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
    if (!IsPresent(interval_ct, initial_solution)) continue;

    const int64_t start_value = GetLinearExpressionValue(
        interval_ct.interval().start(), initial_solution);
    const int64_t end_value = GetLinearExpressionValue(
        interval_ct.interval().end(), initial_solution);
    DCHECK_LE(start_value, end_value);
    demands.push_back({interval_index, start_value, end_value, 1});
  }

  // TODO(user): We actually only need interval_index, start.
  // No need to fill the other fields here.
  std::sort(demands.begin(), demands.end());
  InsertPrecedencesFromSortedListOfNonOverlapingIntervals(demands, precedences);
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
    InsertPrecedencesFromSortedListOfNonOverlapingIntervals(demands,
                                                            precedences);
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
    if (!IsPresent(interval_ct, initial_solution)) continue;

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

  if (demands.empty()) return;

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

struct IndexedRectangle {
  int interval_index;
  Rectangle r;

  bool operator<(const IndexedRectangle& other) const {
    return std::tie(r.x_min, r.x_max) < std::tie(other.r.x_min, other.r.x_max);
  }
};

void InsertRectanglePredecences(
    absl::Span<const IndexedRectangle> rectangles,
    absl::flat_hash_set<std::pair<int, int>>* precedences) {
  // TODO(user): Refine set of interesting points.
  std::vector<IntegerValue> interesting_points;
  for (const IndexedRectangle& idx_r : rectangles) {
    interesting_points.push_back(idx_r.r.y_max - 1);
  }
  gtl::STLSortAndRemoveDuplicates(&interesting_points);
  std::vector<Demand> demands;
  for (const IntegerValue t : interesting_points) {
    demands.clear();
    for (const IndexedRectangle& idx_r : rectangles) {
      if (idx_r.r.y_min > t || idx_r.r.y_max <= t) continue;
      demands.push_back({idx_r.interval_index, idx_r.r.x_min.value(),
                         idx_r.r.x_max.value(), 1});
    }
    std::sort(demands.begin(), demands.end());
    InsertPrecedencesFromSortedListOfNonOverlapingIntervals(demands,
                                                            precedences);
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
  std::vector<IndexedRectangle> x_main;
  std::vector<IndexedRectangle> y_main;
  for (int i = 0; i < no_overlap_2d.x_intervals_size(); ++i) {
    // Ignore unperformed rectangles.
    const int x_interval_index = no_overlap_2d.x_intervals(i);
    if (ignored_intervals.contains(x_interval_index)) continue;
    const ConstraintProto& x_interval_ct =
        model_proto.constraints(x_interval_index);
    if (!IsPresent(x_interval_ct, initial_solution)) continue;

    const int y_interval_index = no_overlap_2d.y_intervals(i);
    if (ignored_intervals.contains(y_interval_index)) continue;
    const ConstraintProto& y_interval_ct =
        model_proto.constraints(y_interval_index);
    if (!IsPresent(y_interval_ct, initial_solution)) continue;

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

    x_main.push_back({.interval_index = x_interval_index,
                      .r = {.x_min = x_start_value,
                            .x_max = x_end_value,
                            .y_min = y_start_value,
                            .y_max = y_end_value}});
    y_main.push_back({.interval_index = y_interval_index,
                      .r = {.x_min = y_start_value,
                            .x_max = y_end_value,
                            .y_min = x_start_value,
                            .y_max = x_end_value}});
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

std::vector<std::vector<int>>
NeighborhoodGeneratorHelper::GetRoutingPathBooleanVariables(
    const CpSolverResponse& initial_solution) const {
  struct HeadAndArcBooleanVariable {
    int head;
    int bool_var;
  };

  std::vector<std::vector<int>> result;
  absl::flat_hash_map<int, HeadAndArcBooleanVariable>
      tail_to_head_and_arc_bool_var;

  for (const int i : TypeToConstraints(ConstraintProto::kCircuit)) {
    const CircuitConstraintProto& ct = ModelProto().constraints(i).circuit();

    // Collect arcs.
    int min_node = std::numeric_limits<int>::max();
    tail_to_head_and_arc_bool_var.clear();
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
      tail_to_head_and_arc_bool_var[tail] = {head, bool_var};
      min_node = std::min(tail, min_node);
    }
    if (tail_to_head_and_arc_bool_var.empty()) continue;

    // Unroll the path.
    int current_node = min_node;
    std::vector<int> path;
    do {
      auto it = tail_to_head_and_arc_bool_var.find(current_node);
      CHECK(it != tail_to_head_and_arc_bool_var.end());
      current_node = it->second.head;
      path.push_back(it->second.bool_var);
    } while (current_node != min_node);
    result.push_back(std::move(path));
  }

  std::vector<HeadAndArcBooleanVariable> route_starts;
  for (const int i : TypeToConstraints(ConstraintProto::kRoutes)) {
    const RoutesConstraintProto& ct = ModelProto().constraints(i).routes();
    tail_to_head_and_arc_bool_var.clear();
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
        tail_to_head_and_arc_bool_var[tail] = {head, bool_var};
      }
    }

    // Unroll all routes.
    for (const HeadAndArcBooleanVariable& head_var : route_starts) {
      std::vector<int> path;
      int current_node = head_var.head;
      path.push_back(head_var.bool_var);
      do {
        auto it = tail_to_head_and_arc_bool_var.find(current_node);
        CHECK(it != tail_to_head_and_arc_bool_var.end());
        current_node = it->second.head;
        path.push_back(it->second.bool_var);
      } while (current_node != 0);
      result.push_back(std::move(path));
    }
  }

  return result;
}

Neighborhood NeighborhoodGeneratorHelper::FixGivenVariables(
    const CpSolverResponse& base_solution,
    const Bitset64<int>& variables_to_fix) const {
  const int num_variables = variables_to_fix.size();
  Neighborhood neighborhood(num_variables);
  neighborhood.delta.mutable_variables()->Reserve(num_variables);

  // TODO(user): Maybe relax all variables in the objective when the number
  // is small or negligible compared to the number of variables.
  const int unique_objective_variable =
      model_proto_.has_objective() && model_proto_.objective().vars_size() == 1
          ? model_proto_.objective().vars(0)
          : -1;

  // Fill in neighborhood.delta all variable domains.
  int num_fixed = 0;
  {
    absl::ReaderMutexLock domain_lock(domain_mutex_);
    for (int i = 0; i < num_variables; ++i) {
      const IntegerVariableProto& current_var =
          model_proto_with_only_variables_.variables(i);
      IntegerVariableProto* new_var = neighborhood.delta.add_variables();

      // We only copy the name in debug mode.
      if (DEBUG_MODE) new_var->set_name(current_var.name());

      if (variables_to_fix[i] && i != unique_objective_variable) {
        ++num_fixed;

        // Note the use of DomainInProtoContains() instead of
        // ReadDomainFromProto() as the later is slower and allocate memory.
        const int64_t base_value = base_solution.solution(i);
        if (DomainInProtoContains(current_var, base_value)) {
          new_var->add_domain(base_value);
          new_var->add_domain(base_value);
        } else {
          // If under the updated domain, the base solution is no longer valid,
          // We should probably regenerate this neighborhood. But for now we
          // just do a best effort and take the closest value.
          const Domain domain = ReadDomainFromProto(current_var);
          int64_t closest_value = domain.Min();
          int64_t closest_dist = std::abs(closest_value - base_value);
          for (const ClosedInterval interval : domain) {
            for (const int64_t value : {interval.start, interval.end}) {
              const int64_t dist = std::abs(value - base_value);
              if (dist < closest_dist) {
                closest_value = value;
                closest_dist = dist;
              }
            }
          }
          FillDomainInProto(Domain(closest_value, closest_value), new_var);
        }
      } else {
        *new_var->mutable_domain() = current_var.domain();
      }
    }
  }

  // Fill some statistic fields and detect if we cover a full component.
  //
  // TODO(user): If there is just one component, we can skip some computation.
  {
    absl::ReaderMutexLock graph_lock(graph_mutex_);
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

  const int num_relaxed = num_variables - num_fixed;
  neighborhood.delta.mutable_solution_hint()->mutable_vars()->Reserve(
      num_relaxed);
  neighborhood.delta.mutable_solution_hint()->mutable_values()->Reserve(
      num_relaxed);
  AddSolutionHinting(base_solution, &neighborhood.delta);

  neighborhood.is_generated = true;
  neighborhood.is_reduced = num_fixed > 0;
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

Neighborhood NeighborhoodGeneratorHelper::RelaxGivenVariables(
    const CpSolverResponse& initial_solution,
    absl::Span<const int> relaxed_variables) const {
  Bitset64<int> fixed_variables(NumVariables());
  {
    absl::ReaderMutexLock graph_lock(graph_mutex_);
    for (const int i : active_variables_) {
      fixed_variables.Set(i);
    }
  }
  for (const int var : relaxed_variables) fixed_variables.Clear(var);
  return FixGivenVariables(initial_solution, fixed_variables);
}

Neighborhood NeighborhoodGeneratorHelper::FixAllVariables(
    const CpSolverResponse& initial_solution) const {
  Bitset64<int> fixed_variables(NumVariables());
  {
    absl::ReaderMutexLock graph_lock(graph_mutex_);
    for (const int i : active_variables_) {
      fixed_variables.Set(i);
    }
  }
  return FixGivenVariables(initial_solution, fixed_variables);
}

CpModelProto NeighborhoodGeneratorHelper::UpdatedModelProtoCopy() const {
  CpModelProto updated_model = model_proto_;
  {
    absl::MutexLock domain_lock(domain_mutex_);
    *updated_model.mutable_variables() =
        model_proto_with_only_variables_.variables();
  }
  return updated_model;
}

bool NeighborhoodGenerator::ReadyToGenerate() const {
  return helper_.shared_response().HasFeasibleSolution();
}

double NeighborhoodGenerator::GetUCBScore(int64_t total_num_calls) const {
  absl::ReaderMutexLock mutex_lock(generator_mutex_);
  DCHECK_GE(total_num_calls, num_calls_);
  if (num_calls_ <= 10) return std::numeric_limits<double>::infinity();
  return current_average_ + sqrt((2 * log(total_num_calls)) / num_calls_);
}

absl::Span<const double> NeighborhoodGenerator::Synchronize() {
  absl::MutexLock mutex_lock(generator_mutex_);

  // To make the whole update process deterministic, we currently sort the
  // SolveData.
  std::sort(solve_data_.begin(), solve_data_.end());

  // This will be used to update the difficulty of this neighborhood.
  int num_fully_solved_in_batch = 0;
  int num_not_fully_solved_in_batch = 0;

  tmp_dtimes_.clear();
  for (const SolveData& data : solve_data_) {
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
      next_time_limit_bump_ = 50;
    } else {
      ++num_consecutive_non_improving_calls_;
    }

    // Confusing: this one is however comparing to the base solution objective.
    if (data.base_objective > data.new_objective) {
      ++num_improving_calls_;
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

    tmp_dtimes_.push_back(data.deterministic_time);
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
  if (num_consecutive_non_improving_calls_ > next_time_limit_bump_) {
    next_time_limit_bump_ = num_consecutive_non_improving_calls_ + 50;
    deterministic_limit_ *= 1.02;

    // We do not want the limit to go to high. Intuitively, the goal is to try
    // out a lot of neighborhoods, not just spend a lot of time on a few.
    deterministic_limit_ = std::min(60.0, deterministic_limit_);
  }

  solve_data_.clear();
  return tmp_dtimes_;
}

std::vector<int>
NeighborhoodGeneratorHelper::ImprovableObjectiveVariablesWhileHoldingLock(
    const CpSolverResponse& initial_solution) const {
  std::vector<int> result;
  absl::ReaderMutexLock lock(domain_mutex_);
  for (const int var : active_objective_variables_) {
    const auto& domain =
        model_proto_with_only_variables_.variables(var).domain();
    bool at_best_value = false;
    if (has_positive_objective_coefficient_[var]) {
      at_best_value = initial_solution.solution(var) == domain[0];
    } else {
      at_best_value =
          initial_solution.solution(var) == domain[domain.size() - 1];
    }
    if (!at_best_value) result.push_back(var);
  }
  return result;
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
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<int> fixed_variables = helper_.ActiveVariables();
  GetRandomSubset(1.0 - data.difficulty, &fixed_variables, random);

  Bitset64<int> to_fix(helper_.NumVariables());
  for (const int var : fixed_variables) to_fix.Set(var);
  return helper_.FixGivenVariables(initial_solution, to_fix);
}

Neighborhood RelaxRandomConstraintsGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  if (helper_.DifficultyMeansFullNeighborhood(data.difficulty)) {
    return helper_.FullNeighborhood();
  }

  std::vector<int> relaxed_variables;
  {
    absl::ReaderMutexLock graph_lock(helper_.graph_mutex_);
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
    const int target_size = std::ceil(data.difficulty * num_active_vars);
    if (target_size == num_active_vars) return helper_.FullNeighborhood();
    // TODO(user): Clean-up when target_size == 0.

    for (const int constraint_index : active_constraints) {
      // TODO(user): randomize order of variable addition when close to the
      // limit.
      for (const int var : helper_.ConstraintToVar()[constraint_index]) {
        if (visited_variables_set[var]) continue;
        visited_variables_set[var] = true;
        if (helper_.IsActive(var)) {
          relaxed_variables.push_back(var);
          if (relaxed_variables.size() >= target_size) break;
        }
      }
      if (relaxed_variables.size() >= target_size) break;
    }
  }

  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

// Note that even if difficulty means full neighborhood, we go through the
// generation process to never get out of a connected components.
Neighborhood VariableGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  const int num_model_vars = helper_.ModelProto().variables_size();
  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<int> visited_variables;

  // It is important complexity wise to never scan a constraint twice!
  const int num_model_constraints = helper_.ModelProto().constraints_size();
  std::vector<bool> scanned_constraints(num_model_constraints, false);

  std::vector<int> random_variables;
  {
    absl::ReaderMutexLock graph_lock(helper_.graph_mutex_);

    std::vector<int> initial_vars =
        helper_.ImprovableObjectiveVariablesWhileHoldingLock(initial_solution);
    if (initial_vars.empty()) {
      initial_vars = helper_.ActiveVariablesWhileHoldingLock();
    }
    // The number of active variables can decrease asynchronously.
    // We read the exact number while locked.
    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(data.difficulty * num_active_vars);
    if (target_size == num_active_vars) return helper_.FullNeighborhood();

    const int first_var =
        initial_vars[absl::Uniform<int>(random, 0, initial_vars.size())];
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

// Note that even if difficulty means full neighborhood, we go through the
// generation process to never get out of a connected components.
Neighborhood ArcGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  const int num_model_vars = helper_.ModelProto().variables_size();
  if (num_model_vars == 0) return helper_.NoNeighborhood();

  // We copy the full graph var <-> constraints so that we can:
  //   - reduce it in place
  //   - not hold the mutex too long.
  // TODO(user): should we compress it or use a different representation ?
  CompactVectorVector<int, int> vars_to_constraints;
  CompactVectorVector<int, int> constraints_to_vars;
  int num_active_vars = 0;
  std::vector<int> active_objective_vars;
  {
    absl::ReaderMutexLock graph_lock(helper_.graph_mutex_);
    num_active_vars = helper_.ActiveVariablesWhileHoldingLock().size();
    active_objective_vars =
        helper_.ImprovableObjectiveVariablesWhileHoldingLock(initial_solution);
    constraints_to_vars = helper_.ConstraintToVar();
    vars_to_constraints = helper_.VarToConstraint();
  }

  const int target_size = std::ceil(data.difficulty * num_active_vars);
  if (target_size == 0) return helper_.NoNeighborhood();

  // We pick a variable from the objective.
  const int num_objective_variables = active_objective_vars.size();
  if (num_objective_variables == 0) return helper_.NoNeighborhood();
  const int first_var = active_objective_vars[absl::Uniform<int>(
      random, 0, num_objective_variables)];

  std::vector<bool> relaxed_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  // Active vars are relaxed variables with some unexplored neighbors.
  std::vector<int> active_vars;

  relaxed_variables_set[first_var] = true;
  relaxed_variables.push_back(first_var);
  active_vars.push_back(first_var);

  while (relaxed_variables.size() < target_size) {
    if (active_vars.empty()) break;  // We have exhausted our component.

    const int tail_index = absl::Uniform<int>(random, 0, active_vars.size());
    const int tail_var = active_vars[tail_index];
    int head_var = tail_var;
    while (!vars_to_constraints[tail_var].empty() && head_var == tail_var) {
      const auto cts = vars_to_constraints[tail_var];
      const int pos_ct = absl::Uniform<int>(random, 0, cts.size());
      const int ct = cts[pos_ct];
      while (!constraints_to_vars[ct].empty() && head_var == tail_var) {
        const auto vars = constraints_to_vars[ct];
        const int pos_var = absl::Uniform<int>(random, 0, vars.size());
        const int candidate = vars[pos_var];

        // We remove the variable as it is either already relaxed, or will be
        // relaxed.
        constraints_to_vars.RemoveBySwap(ct, pos_var);
        if (!relaxed_variables_set[candidate]) {
          head_var = candidate;
        }
      }
      if (constraints_to_vars[ct].empty()) {
        // This constraint has no more un-relaxed variables.
        vars_to_constraints.RemoveBySwap(tail_var, pos_ct);
      }
    }

    // Variable is no longer active ?
    if (vars_to_constraints[tail_var].empty()) {
      std::swap(active_vars[tail_index], active_vars.back());
      active_vars.pop_back();
    }

    if (head_var != tail_var) {
      relaxed_variables_set[head_var] = true;
      relaxed_variables.push_back(head_var);
      active_vars.push_back(head_var);
    }
  }
  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

// Note that even if difficulty means full neighborhood, we go through the
// generation process to never get out of a connected components.
Neighborhood ConstraintGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  const int num_model_constraints = helper_.ModelProto().constraints_size();
  if (num_model_constraints == 0) {
    return helper_.FullNeighborhood();
  }

  const int num_model_vars = helper_.ModelProto().variables_size();
  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;

  std::vector<bool> added_constraints(num_model_constraints, false);
  std::vector<int> next_constraints;

  std::vector<int> random_variables;
  {
    absl::ReaderMutexLock graph_lock(helper_.graph_mutex_);
    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(data.difficulty * num_active_vars);
    if (target_size == num_active_vars) return helper_.FullNeighborhood();

    // Start from a random active constraint.
    const int num_active_constraints = helper_.ConstraintToVar().size();
    if (num_active_constraints == 0) return helper_.NoNeighborhood();
    next_constraints.push_back(
        absl::Uniform<int>(random, 0, num_active_constraints));
    added_constraints[next_constraints.back()] = true;

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
      random_variables.assign(
          helper_.ConstraintToVar()[constraint_index].begin(),
          helper_.ConstraintToVar()[constraint_index].end());
      std::shuffle(random_variables.begin(), random_variables.end(), random);
      for (const int var : random_variables) {
        if (visited_variables_set[var]) continue;
        visited_variables_set[var] = true;
        if (helper_.IsActive(var)) {
          relaxed_variables.push_back(var);
        }
        if (relaxed_variables.size() >= target_size) break;

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

Neighborhood DecompositionGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  int max_width = 0;
  int size_at_min_width_after_100;
  int min_width_after_100 = std::numeric_limits<int>::max();
  int num_zero_score = 0;
  std::vector<int> relaxed_variables;

  // Note(user): The algo is slower than the other graph generator, so we
  // might not want to lock the graph for so long? it is just a reader lock
  // though.
  {
    absl::ReaderMutexLock graph_lock(helper_.graph_mutex_);

    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(data.difficulty * num_active_vars);
    if (target_size == num_active_vars) return helper_.FullNeighborhood();

    const int num_vars = helper_.VarToConstraint().size();
    const int num_constraints = helper_.ConstraintToVar().size();
    if (num_constraints == 0 || num_vars == 0) {
      return helper_.FullNeighborhood();
    }

    // We will grow this incrementally.
    // Index in the graph are first variables then constraints.
    const int num_nodes = num_vars + num_constraints;
    std::vector<bool> added(num_nodes, false);
    std::vector<bool> added_or_connected(num_nodes, false);

    // We will process var/constraint node by minimum "score".
    struct QueueElement {
      int Index() const { return index; }
      bool operator<(const QueueElement& o) const {
        if (score == o.score) return tie_break < o.tie_break;
        return score < o.score;
      }

      int index;
      int score = 0;
      double tie_break = 0.0;
    };
    std::vector<QueueElement> elements(num_nodes);
    IntegerPriorityQueue<QueueElement> pq(num_nodes);

    // Initialize elements.
    for (int i = 0; i < num_nodes; ++i) {
      elements[i].index = i;
      elements[i].tie_break = absl::Uniform<double>(random, 0.0, 1.0);
    }

    // We start from a random active variable.
    //
    // Note that while num_vars contains all variables, all the fixed variables
    // will have no associated constraint, so we don't want to start from a
    // random variable.
    //
    // TODO(user): Does starting by a constraint make sense too?
    const int first_index =
        helper_.ActiveVariablesWhileHoldingLock()[absl::Uniform<int>(
            random, 0, num_active_vars)];
    elements[first_index].score = helper_.VarToConstraint()[first_index].size();
    pq.Add(elements[first_index]);
    added_or_connected[first_index] = true;

    // Pop max-degree from queue and update.
    std::vector<int> to_update;
    while (!pq.IsEmpty() && relaxed_variables.size() < target_size) {
      // Just for logging.
      if (relaxed_variables.size() > 100 && pq.Size() < min_width_after_100) {
        min_width_after_100 = pq.Size();
        size_at_min_width_after_100 = relaxed_variables.size();
      }

      const int index = pq.Top().index;
      const int score = pq.Top().score;
      pq.Pop();
      added[index] = true;

      // When the score is zero, we don't need to update anything since the
      // frontier does not grow.
      if (score == 0) {
        if (index < num_vars) relaxed_variables.push_back(index);
        ++num_zero_score;
        continue;
      }

      // Note that while it might looks bad, the overall complexity of this is
      // in O(num_edge) since we scan each index once and each newly connected
      // vertex once.
      int num_added = 0;
      to_update.clear();
      if (index < num_vars) {
        relaxed_variables.push_back(index);
        for (const int c : helper_.VarToConstraint()[index]) {
          const int c_index = num_vars + c;
          if (added_or_connected[c_index]) continue;
          ++num_added;
          added_or_connected[c_index] = true;
          to_update.push_back(c_index);
          for (const int v : helper_.ConstraintToVar()[c]) {
            if (added[v]) continue;
            if (added_or_connected[v]) {
              to_update.push_back(v);
              elements[v].score--;
            } else {
              elements[c_index].score++;
            }
          }
        }
      } else {
        for (const int v : helper_.ConstraintToVar()[index - num_vars]) {
          if (added_or_connected[v]) continue;
          ++num_added;
          added_or_connected[v] = true;
          to_update.push_back(v);
          for (const int c : helper_.VarToConstraint()[v]) {
            if (added[num_vars + c]) continue;
            if (added_or_connected[num_vars + c]) {
              elements[num_vars + c].score--;
              to_update.push_back(num_vars + c);
            } else {
              elements[v].score++;
            }
          }
        }
      }

      // The score is exactly the frontier increase in size.
      // This is the same as the min-degree heuristic for the elimination order.
      // Except we only consider connected nodes.
      CHECK_EQ(num_added, score);

      gtl::STLSortAndRemoveDuplicates(&to_update);
      for (const int index : to_update) {
        DCHECK(!added[index]);
        if (pq.Contains(index)) {
          pq.ChangePriority(elements[index]);
        } else {
          pq.Add(elements[index]);
        }
      }

      max_width = std::max(max_width, pq.Size());
    }

    // Just for logging.
    if (pq.Size() < min_width_after_100) {
      min_width_after_100 = pq.Size();
      size_at_min_width_after_100 = relaxed_variables.size();
    }

    VLOG(2) << "#relaxed " << relaxed_variables.size() << " #zero_score "
            << num_zero_score << " max_width " << max_width
            << " (size,min_width)_after_100 (" << size_at_min_width_after_100
            << "," << min_width_after_100 << ") "
            << " final_width " << pq.Size();
  }

  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

namespace {

// Create a constraint sum (X - LB) + sum (UB - X) <= rhs.
ConstraintProto DistanceToBoundsSmallerThanConstraint(
    absl::Span<const std::pair<int, int64_t>> dist_to_lower_bound,
    absl::Span<const std::pair<int, int64_t>> dist_to_upper_bound,
    const int64_t rhs) {
  DCHECK_GE(rhs, 0);
  ConstraintProto new_constraint;
  LinearConstraintProto* linear = new_constraint.mutable_linear();
  int64_t lhs_constant_value = 0;
  for (const auto [var, lb] : dist_to_lower_bound) {
    // We add X - LB
    linear->add_coeffs(1);
    linear->add_vars(var);
    lhs_constant_value -= lb;
  }
  for (const auto [var, ub] : dist_to_upper_bound) {
    // We add UB - X
    lhs_constant_value += ub;
    linear->add_coeffs(-1);
    linear->add_vars(var);
  }
  linear->add_domain(std::numeric_limits<int64_t>::min());
  linear->add_domain(rhs - lhs_constant_value);
  return new_constraint;
}

}  // namespace

Neighborhood LocalBranchingLpBasedNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  const std::vector<int> active_variables = helper_.ActiveVariables();
  if (active_variables.empty()) return helper_.NoNeighborhood();

  {
    // Quick corner case in case the difficulty is too high. This is mainly
    // useful when testing with only that kind of LNS to abort early on
    // super-easy problems.
    const int size = active_variables.size();
    if (static_cast<int>(std::ceil(data.difficulty * size)) == size) {
      return helper_.FullNeighborhood();
    }
  }

  // These are candidate for relaxation. The score will be filled later. Active
  // variable not kept in candidate will be added to other_variables.
  std::vector<std::pair<int, double>> candidates_with_score;
  std::vector<int> other_variables;

  // Our extra relaxation constraint will be: sums of distance to the respective
  // bound smaller than a constant that depends on the difficulty.
  std::vector<std::pair<int, int64_t>> dist_to_lower_bound;
  std::vector<std::pair<int, int64_t>> dist_to_upper_bound;

  // For the "easy" part of the extra constraint, we either look only at the
  // binary variables. Or we extend that to all variables at their bound.
  const bool only_look_at_binary = absl::Bernoulli(random, 0.5);

  // We copy the model early to have access to reduced domains.
  // TODO(user): that might not be the most efficient if we abort just below.
  CpModelProto local_cp_model = helper_.UpdatedModelProtoCopy();

  // Loop over active variables.
  bool some_non_binary_at_bound = false;
  for (const int var : active_variables) {
    DCHECK_LT(var, initial_solution.solution().size());
    DCHECK_LT(var, local_cp_model.variables().size());
    const IntegerVariableProto& var_proto = local_cp_model.variables(var);
    const int64_t base_value = initial_solution.solution(var);
    const bool is_binary = var_proto.domain_size() == 2 &&
                           var_proto.domain(0) == 0 && var_proto.domain(1) == 1;
    if (only_look_at_binary && !is_binary) {
      other_variables.push_back(var);
      continue;
    }

    DCHECK(!var_proto.domain().empty());
    const int64_t domain_min = var_proto.domain(0);
    const int64_t domain_max = var_proto.domain(var_proto.domain().size() - 1);
    if (base_value <= domain_min) {
      if (!is_binary) some_non_binary_at_bound = true;
      candidates_with_score.push_back({var, 0.0});
      dist_to_lower_bound.push_back({var, domain_min});
    } else if (base_value >= domain_max) {
      if (!is_binary) some_non_binary_at_bound = true;
      candidates_with_score.push_back({var, 0.0});
      dist_to_upper_bound.push_back({var, domain_max});
    } else {
      other_variables.push_back(var);
    }
  }

  bool use_hamming_for_others = false;
  if (!other_variables.empty() && absl::Bernoulli(random, 0.5)) {
    use_hamming_for_others = true;
  }
  if (!use_hamming_for_others && candidates_with_score.empty()) {
    return helper_.NoNeighborhood();
  }

  // With this option, we will create a bunch of Boolean variable
  // and add the constraints : "bool==0 => var == value_in_base_solution".
  if (use_hamming_for_others) {
    for (const int var : other_variables) {
      const int indicator = local_cp_model.variables().size();
      auto* var_proto = local_cp_model.add_variables();
      var_proto->add_domain(0);
      var_proto->add_domain(1);
      auto* new_ct = local_cp_model.add_constraints();
      new_ct->add_enforcement_literal(NegatedRef(indicator));

      const int64_t base_value = initial_solution.solution(var);
      new_ct->mutable_linear()->add_domain(base_value);
      new_ct->mutable_linear()->add_domain(base_value);
      new_ct->mutable_linear()->add_vars(var);
      new_ct->mutable_linear()->add_coeffs(1);

      // Add it to the distance constraint.
      dist_to_lower_bound.push_back({indicator, 0});
      candidates_with_score.push_back({var, 0.0});
    }

    // Clear other_variables so that they are not added at the end.
    other_variables.clear();
  }

  // Constrain the distance to the bounds.
  const int size = dist_to_upper_bound.size() + dist_to_lower_bound.size();
  const int target_size = static_cast<int>(std::ceil(data.difficulty * size));
  DCHECK_LE(target_size, candidates_with_score.size());
  *local_cp_model.add_constraints() = DistanceToBoundsSmallerThanConstraint(
      dist_to_lower_bound, dist_to_upper_bound, target_size);

  Model model("lb_relax_lns_lp");
  auto* const params = model.GetOrCreate<SatParameters>();

  // Parameters to enable solving the LP only.
  params->set_num_workers(1);
  params->set_linearization_level(2);
  params->set_stop_after_root_propagation(true);
  params->set_add_lp_constraints_lazily(false);

  // Parameters to attempt to speed up solve.
  params->set_cp_model_presolve(false);
  params->set_cp_model_probing_level(0);

  // Parameters to limit time spent in the solve. The max number of iterations
  // is relaxed from the default since we rely more on deterministic time.
  params->set_root_lp_iterations(100000);

  // TODO(user): This is a lot longer than a normal LNS, so it might cause
  // issue with the current round-robbin selection based on number of calls.
  params->set_max_deterministic_time(10);
  model.GetOrCreate<TimeLimit>()->ResetLimitFromParameters(*params);
  if (global_time_limit_ != nullptr) {
    global_time_limit_->UpdateLocalLimit(model.GetOrCreate<TimeLimit>());
  }

  // Tricky: we want the inner_objective_lower_bound in the response to be in
  // term of the current problem, not the user facing one.
  if (local_cp_model.has_objective()) {
    local_cp_model.mutable_objective()->set_integer_before_offset(0);
    local_cp_model.mutable_objective()->set_integer_after_offset(0);
    local_cp_model.mutable_objective()->set_integer_scaling_factor(0);
  }

  // Dump?
  if (absl::GetFlag(FLAGS_cp_model_dump_submodels)) {
    const std::string dump_name =
        absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                     "lb_relax_lns_lp_", data.task_id, ".pb.txt");
    LOG(INFO) << "Dumping linear relaxed model to '" << dump_name << "'.";
    CHECK(WriteModelProtoToFile(local_cp_model, dump_name));
  }

  // Solve.
  //
  // TODO(user): Shall we pass the objective upper bound so we have more
  // chance to fix variable via reduced cost fixing.
  //
  // TODO(user): Does the current solution can provide a warm-start for the
  // LP?
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  {
    response_manager->InitializeObjective(local_cp_model);
    LoadCpModel(local_cp_model, &model);
    SolveLoadedCpModel(local_cp_model, &model);
  }

  // Update dtime.
  data.deterministic_time +=
      model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime();

  // Analyze the status of this first "solve".
  //
  // TODO(user): If we run into this case, it also means that every other LNS
  // that tries to more variable than here will never be able to improve.
  if (local_cp_model.has_objective()) {
    const CpSolverResponse response = response_manager->GetResponse();
    if (response.status() == CpSolverStatus::INFEASIBLE) {
      data.status = CpSolverStatus::INFEASIBLE;
      AddSolveData(data);
      return helper_.NoNeighborhood();
    }

    const int64_t inner_lb = response.inner_objective_lower_bound();
    const int64_t current_inner_obj = ComputeInnerObjective(
        local_cp_model.objective(), initial_solution.solution());
    if (inner_lb >= current_inner_obj) {
      // In this case, we cannot improve on the base solution.
      // We could try to find a different solution for diversity, but we do have
      // other neighborhood for that. Lets abort early.
      data.status = CpSolverStatus::OPTIMAL;  // We cannot improve.
      AddSolveData(data);
      return helper_.NoNeighborhood();
    }
  }

  // Compute differences between LP solution and initial solution, with a small
  // random noise for tie breaking.
  const auto var_mapping = model.GetOrCreate<CpModelMapping>();
  const auto lp_solution = model.GetOrCreate<ModelLpValues>();
  if (lp_solution->empty()) {
    // We likely didn't solve the LP at all, so lets not use this neighborhood.
    return helper_.NoNeighborhood();
  }
  for (auto& [var, score] : candidates_with_score) {
    const IntegerVariable integer = var_mapping->Integer(var);
    DCHECK_LT(integer, lp_solution->size());
    DCHECK_LT(var, initial_solution.solution().size());
    const double difference =
        std::abs(lp_solution->at(var_mapping->Integer(var)) -
                 initial_solution.solution(var));
    score = difference + absl::Uniform<double>(random, 0.0, 1e-6);
  }

  // Take the target_size variables with largest differences.
  absl::c_sort(candidates_with_score, [](const std::pair<int, double>& a,
                                         const std::pair<int, double>& b) {
    return a.second > b.second;
  });

  std::vector<int> vars_to_relax;
  vars_to_relax.reserve(target_size);
  DCHECK_LE(target_size, candidates_with_score.size());
  for (int i = 0; i < target_size; ++i) {
    vars_to_relax.push_back(candidates_with_score[i].first);
  }

  // We will also relax all "other variables". We assume their values are likely
  // tied to the other ones.
  vars_to_relax.insert(vars_to_relax.end(), other_variables.begin(),
                       other_variables.end());
  Neighborhood result =
      helper_.RelaxGivenVariables(initial_solution, vars_to_relax);

  // Lets the name reflect the type.
  //
  // TODO(user): Unfortunately like this we have a common difficulty for all
  // variant, we should probably fix that.
  result.source_info = "lb_relax_lns";
  absl::StrAppend(&result.source_info,
                  some_non_binary_at_bound ? "_int" : "_bool");
  if (use_hamming_for_others) {
    absl::StrAppend(&result.source_info, "_h");
  }

  return result;
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
    absl::Span<const int> intervals_to_relax,
    absl::Span<const int> variables_to_fix,
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

  // fix the extra variables passed as parameters.
  for (const int var : variables_to_fix) {
    const int value = initial_solution.solution(var);
    neighborhood.delta.mutable_variables(var)->clear_domain();
    neighborhood.delta.mutable_variables(var)->add_domain(value);
    neighborhood.delta.mutable_variables(var)->add_domain(value);
  }

  // Set the current solution as a hint.
  helper.AddSolutionHinting(initial_solution, &neighborhood.delta);
  neighborhood.is_generated = true;

  return neighborhood;
}

Neighborhood RandomIntervalSchedulingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<int> intervals_to_relax =
      helper_.GetActiveIntervals(initial_solution);
  GetRandomSubset(data.difficulty, &intervals_to_relax, random);

  return GenerateSchedulingNeighborhoodFromRelaxedIntervals(
      intervals_to_relax, {}, initial_solution, random, helper_);
}

Neighborhood RandomPrecedenceSchedulingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<std::pair<int, int>> precedences =
      helper_.GetSchedulingPrecedences({}, initial_solution, random);
  GetRandomSubset(1.0 - data.difficulty, &precedences, random);
  return GenerateSchedulingNeighborhoodFromIntervalPrecedences(
      precedences, initial_solution, helper_);
}

namespace {
void AppendVarsFromAllIntervalIndices(absl::Span<const int> indices,
                                      absl::Span<const int> all_intervals,
                                      const CpModelProto& model_proto,
                                      std::vector<int>* variables) {
  for (const int index : indices) {
    const std::vector<int> vars =
        UsedVariables(model_proto.constraints(all_intervals[index]));
    variables->insert(variables->end(), vars.begin(), vars.end());
  }
}
}  // namespace

Neighborhood SchedulingTimeWindowNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  const std::vector<int> active_intervals =
      helper_.GetActiveIntervals(initial_solution);

  if (active_intervals.empty()) return helper_.FullNeighborhood();

  const TimePartition partition = PartitionIndicesAroundRandomTimeWindow(
      active_intervals, helper_.ModelProto(), initial_solution, data.difficulty,
      random);
  std::vector<int> intervals_to_relax;
  intervals_to_relax.reserve(partition.selected_indices.size());
  std::vector<int> variables_to_fix;
  intervals_to_relax.insert(intervals_to_relax.end(),
                            partition.selected_indices.begin(),
                            partition.selected_indices.end());

  if (helper_.Parameters().push_all_tasks_toward_start()) {
    intervals_to_relax.insert(intervals_to_relax.end(),
                              partition.indices_before_selected.begin(),
                              partition.indices_before_selected.end());
    AppendVarsFromAllIntervalIndices(partition.indices_before_selected,
                                     active_intervals, helper_.ModelProto(),
                                     &variables_to_fix);
  }

  gtl::STLSortAndRemoveDuplicates(&intervals_to_relax);
  gtl::STLSortAndRemoveDuplicates(&variables_to_fix);
  return GenerateSchedulingNeighborhoodFromRelaxedIntervals(
      intervals_to_relax, variables_to_fix, initial_solution, random, helper_);
}

Neighborhood SchedulingResourceWindowsNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<int> intervals_to_relax;
  std::vector<int> variables_to_fix;
  std::vector<int> active_intervals;
  for (const std::vector<int>& intervals : intervals_in_constraints_) {
    active_intervals = helper_.KeepActiveIntervals(intervals, initial_solution);
    const TimePartition partition = PartitionIndicesAroundRandomTimeWindow(
        active_intervals, helper_.ModelProto(), initial_solution,
        data.difficulty, random);
    intervals_to_relax.insert(intervals_to_relax.end(),
                              partition.selected_indices.begin(),
                              partition.selected_indices.end());

    if (helper_.Parameters().push_all_tasks_toward_start()) {
      intervals_to_relax.insert(intervals_to_relax.end(),
                                partition.indices_before_selected.begin(),
                                partition.indices_before_selected.end());
      AppendVarsFromAllIntervalIndices(partition.indices_before_selected,
                                       active_intervals, helper_.ModelProto(),
                                       &variables_to_fix);
    }
  }

  if (intervals_to_relax.empty() && variables_to_fix.empty()) {
    return helper_.FullNeighborhood();
  }

  gtl::STLSortAndRemoveDuplicates(&intervals_to_relax);
  gtl::STLSortAndRemoveDuplicates(&variables_to_fix);
  return GenerateSchedulingNeighborhoodFromRelaxedIntervals(
      intervals_to_relax, variables_to_fix, initial_solution, random, helper_);
}

Neighborhood RandomRectanglesPackingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<ActiveRectangle> rectangles_to_freeze =
      helper_.GetActiveRectangles(initial_solution);
  GetRandomSubset(1.0 - data.difficulty, &rectangles_to_freeze, random);

  Bitset64<int> variables_to_freeze(helper_.NumVariables());
  for (const ActiveRectangle& rectangle : rectangles_to_freeze) {
    InsertVariablesFromInterval(helper_.ModelProto(), rectangle.x_interval,
                                variables_to_freeze);
    InsertVariablesFromInterval(helper_.ModelProto(), rectangle.y_interval,
                                variables_to_freeze);
  }
  return helper_.FixGivenVariables(initial_solution, variables_to_freeze);
}

Neighborhood RectanglesPackingRelaxOneNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  // First pick one rectangle.
  const std::vector<ActiveRectangle> all_active_rectangles =
      helper_.GetActiveRectangles(initial_solution);
  if (all_active_rectangles.size() <= 1) return helper_.FullNeighborhood();

  const ActiveRectangle& base_rectangle =
      all_active_rectangles[absl::Uniform<int>(random, 0,
                                               all_active_rectangles.size())];

  const auto get_rectangle = [&initial_solution, helper = &helper_](
                                 const ActiveRectangle& rectangle) {
    const int x_interval_idx = rectangle.x_interval;
    const int y_interval_idx = rectangle.y_interval;
    const ConstraintProto& x_interval_ct =
        helper->ModelProto().constraints(x_interval_idx);
    const ConstraintProto& y_interval_ct =
        helper->ModelProto().constraints(y_interval_idx);
    return Rectangle{.x_min = GetLinearExpressionValue(
                         x_interval_ct.interval().start(), initial_solution),
                     .x_max = GetLinearExpressionValue(
                         x_interval_ct.interval().end(), initial_solution),
                     .y_min = GetLinearExpressionValue(
                         y_interval_ct.interval().start(), initial_solution),
                     .y_max = GetLinearExpressionValue(
                         y_interval_ct.interval().end(), initial_solution)};
  };

  const Rectangle center_rect = get_rectangle(base_rectangle);

  // Now compute a neighborhood around that rectangle. In this neighborhood
  // we prefer a "Square" region around the initial rectangle center rather than
  // a circle.
  //
  // Note that we only consider two rectangles as potential neighbors if they
  // are part of the same no_overlap_2d constraint.
  Bitset64<int> variables_to_freeze(helper_.NumVariables());
  std::vector<std::pair<int, double>> distances;
  distances.reserve(all_active_rectangles.size());
  for (int i = 0; i < all_active_rectangles.size(); ++i) {
    const ActiveRectangle& rectangle = all_active_rectangles[i];
    InsertVariablesFromInterval(helper_.ModelProto(), rectangle.x_interval,
                                variables_to_freeze);
    InsertVariablesFromInterval(helper_.ModelProto(), rectangle.y_interval,
                                variables_to_freeze);

    const Rectangle rect = get_rectangle(rectangle);
    const bool same_no_overlap_as_center_rect = absl::c_any_of(
        base_rectangle.no_overlap_2d_constraints, [&rectangle](const int c) {
          return rectangle.no_overlap_2d_constraints.contains(c);
        });
    if (same_no_overlap_as_center_rect) {
      distances.push_back(
          {i, CenterToCenterLInfinityDistance(center_rect, rect)});
    }
  }
  absl::c_stable_sort(distances, [](const auto& a, const auto& b) {
    return a.second < b.second;
  });

  const int num_to_sample = data.difficulty * all_active_rectangles.size();
  const int num_to_relax = std::min<int>(distances.size(), num_to_sample);
  Rectangle relaxed_bounding_box = center_rect;
  absl::flat_hash_set<int> boxes_to_relax;
  for (int i = 0; i < num_to_relax; ++i) {
    const int rectangle_idx = distances[i].first;
    const ActiveRectangle& rectangle = all_active_rectangles[rectangle_idx];
    relaxed_bounding_box.GrowToInclude(get_rectangle(rectangle));
    boxes_to_relax.insert(rectangle_idx);
  }

  // Heuristic: we relax a bit the bounding box in order to allow some
  // movements, this is needed to not have a trivial neighborhood if we relax a
  // single box for instance.
  const IntegerValue x_size = relaxed_bounding_box.SizeX();
  const IntegerValue y_size = relaxed_bounding_box.SizeY();
  relaxed_bounding_box.x_min = CapSubI(relaxed_bounding_box.x_min, x_size / 2);
  relaxed_bounding_box.x_max = CapAddI(relaxed_bounding_box.x_max, x_size / 2);
  relaxed_bounding_box.y_min = CapSubI(relaxed_bounding_box.y_min, y_size / 2);
  relaxed_bounding_box.y_max = CapAddI(relaxed_bounding_box.y_max, y_size / 2);

  for (const int b : boxes_to_relax) {
    const ActiveRectangle& rectangle = all_active_rectangles[b];
    RemoveVariablesFromInterval(helper_.ModelProto(), rectangle.x_interval,
                                variables_to_freeze);
    RemoveVariablesFromInterval(helper_.ModelProto(), rectangle.y_interval,
                                variables_to_freeze);
  }
  Neighborhood neighborhood =
      helper_.FixGivenVariables(initial_solution, variables_to_freeze);

  neighborhood.is_simple = false;
  neighborhood.is_reduced = true;
  neighborhood.variables_that_can_be_fixed_to_local_optimum.clear();

  // The call above add the relaxed variables to the neighborhood using the
  // current bounds at level 0. For big problems, this might create a hard model
  // with a large complicated landscape of fixed boxes with a lot of potential
  // places to place the relaxed boxes. Therefore we update the domain so the
  // boxes can only stay around the area we decided to relax.
  for (const int b : boxes_to_relax) {
    {
      const IntervalConstraintProto& x_interval =
          helper_.ModelProto()
              .constraints(all_active_rectangles[b].x_interval)
              .interval();
      const Domain x_domain = Domain(relaxed_bounding_box.x_min.value(),
                                     relaxed_bounding_box.x_max.value());
      RestrictAffineExpression(x_interval.start(), x_domain,
                               &neighborhood.delta);
      RestrictAffineExpression(x_interval.end(), x_domain, &neighborhood.delta);
    }
    {
      const IntervalConstraintProto& y_interval =
          helper_.ModelProto()
              .constraints(all_active_rectangles[b].y_interval)
              .interval();
      const Domain y_domain = Domain(relaxed_bounding_box.y_min.value(),
                                     relaxed_bounding_box.y_max.value());
      RestrictAffineExpression(y_interval.start(), y_domain,
                               &neighborhood.delta);
      RestrictAffineExpression(y_interval.end(), y_domain, &neighborhood.delta);
    }
  }
  return neighborhood;
}

Neighborhood RectanglesPackingRelaxTwoNeighborhoodsGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  // First pick a pair of rectangles.
  std::vector<ActiveRectangle> all_active_rectangles =
      helper_.GetActiveRectangles(initial_solution);
  if (all_active_rectangles.size() <= 2) return helper_.FullNeighborhood();

  const int first_idx =
      absl::Uniform<int>(random, 0, all_active_rectangles.size());
  int second_idx =
      absl::Uniform<int>(random, 0, all_active_rectangles.size() - 1);
  if (second_idx >= first_idx) {
    second_idx++;
  }

  const ActiveRectangle& chosen_rectangle_1 = all_active_rectangles[first_idx];
  const ActiveRectangle& chosen_rectangle_2 = all_active_rectangles[second_idx];

  const auto get_rectangle = [&initial_solution, helper = &helper_](
                                 const ActiveRectangle& rectangle) {
    const int x_interval_idx = rectangle.x_interval;
    const int y_interval_idx = rectangle.y_interval;
    const ConstraintProto& x_interval_ct =
        helper->ModelProto().constraints(x_interval_idx);
    const ConstraintProto& y_interval_ct =
        helper->ModelProto().constraints(y_interval_idx);
    return Rectangle{.x_min = GetLinearExpressionValue(
                         x_interval_ct.interval().start(), initial_solution),
                     .x_max = GetLinearExpressionValue(
                         x_interval_ct.interval().end(), initial_solution),
                     .y_min = GetLinearExpressionValue(
                         y_interval_ct.interval().start(), initial_solution),
                     .y_max = GetLinearExpressionValue(
                         y_interval_ct.interval().end(), initial_solution)};
  };

  const Rectangle rect1 = get_rectangle(chosen_rectangle_1);
  const Rectangle rect2 = get_rectangle(chosen_rectangle_2);

  // Now compute a neighborhood around each rectangle. Note that we only
  // consider two rectangles as potential neighbors if they are part of the same
  // no_overlap_2d constraint.
  //
  // TODO(user): This computes the distance between the center of the
  // rectangles. We could use the real distance between the closest points, but
  // not sure it is worth the extra complexity.
  Bitset64<int> variables_to_freeze(helper_.NumVariables());
  std::vector<std::pair<int, double>> distances1;
  std::vector<std::pair<int, double>> distances2;
  distances1.reserve(all_active_rectangles.size());
  distances2.reserve(all_active_rectangles.size());
  for (int i = 0; i < all_active_rectangles.size(); ++i) {
    const ActiveRectangle& rectangle = all_active_rectangles[i];
    InsertVariablesFromInterval(helper_.ModelProto(), rectangle.x_interval,
                                variables_to_freeze);
    InsertVariablesFromInterval(helper_.ModelProto(), rectangle.y_interval,
                                variables_to_freeze);

    const Rectangle rect = get_rectangle(rectangle);
    const bool same_no_overlap_as_rect1 =
        absl::c_any_of(chosen_rectangle_1.no_overlap_2d_constraints,
                       [&rectangle](const int c) {
                         return rectangle.no_overlap_2d_constraints.contains(c);
                       });
    const bool same_no_overlap_as_rect2 =
        absl::c_any_of(chosen_rectangle_2.no_overlap_2d_constraints,
                       [&rectangle](const int c) {
                         return rectangle.no_overlap_2d_constraints.contains(c);
                       });
    if (same_no_overlap_as_rect1) {
      distances1.push_back({i, CenterToCenterL2Distance(rect1, rect)});
    }
    if (same_no_overlap_as_rect2) {
      distances2.push_back({i, CenterToCenterL2Distance(rect2, rect)});
    }
  }
  const int num_to_sample_each =
      data.difficulty * all_active_rectangles.size() / 2;
  std::sort(distances1.begin(), distances1.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
  std::sort(distances2.begin(), distances2.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
  for (auto& samples : {distances1, distances2}) {
    const int num_potential_samples = samples.size();
    for (int i = 0; i < std::min(num_potential_samples, num_to_sample_each);
         ++i) {
      const int rectangle_idx = samples[i].first;
      const ActiveRectangle& rectangle = all_active_rectangles[rectangle_idx];
      RemoveVariablesFromInterval(helper_.ModelProto(), rectangle.x_interval,
                                  variables_to_freeze);
      RemoveVariablesFromInterval(helper_.ModelProto(), rectangle.y_interval,
                                  variables_to_freeze);
    }
  }

  return helper_.FixGivenVariables(initial_solution, variables_to_freeze);
}

Neighborhood RandomPrecedencesPackingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<ActiveRectangle> rectangles_to_relax =
      helper_.GetActiveRectangles(initial_solution);
  GetRandomSubset(data.difficulty, &rectangles_to_relax, random);
  std::vector<int> intervals_to_relax;
  for (const ActiveRectangle& rect : rectangles_to_relax) {
    intervals_to_relax.push_back(rect.x_interval);
    intervals_to_relax.push_back(rect.y_interval);
  }
  gtl::STLSortAndRemoveDuplicates(&intervals_to_relax);

  return GenerateSchedulingNeighborhoodFromRelaxedIntervals(
      intervals_to_relax, {}, initial_solution, random, helper_);
}

Neighborhood SlicePackingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  const std::vector<ActiveRectangle> active_rectangles =
      helper_.GetActiveRectangles(initial_solution);
  const bool use_first_dimension = absl::Bernoulli(random, 0.5);
  std::vector<int> projected_intervals;
  projected_intervals.reserve(active_rectangles.size());
  for (const ActiveRectangle& rect : active_rectangles) {
    projected_intervals.push_back(use_first_dimension ? rect.x_interval
                                                      : rect.y_interval);
  }

  const TimePartition partition = PartitionIndicesAroundRandomTimeWindow(
      projected_intervals, helper_.ModelProto(), initial_solution,
      data.difficulty, random);
  std::vector<bool> indices_to_fix(active_rectangles.size(), true);
  for (const int index : partition.selected_indices) {
    indices_to_fix[index] = false;
  }

  Bitset64<int> variables_to_freeze(helper_.NumVariables());
  for (int index = 0; index < active_rectangles.size(); ++index) {
    if (indices_to_fix[index]) {
      InsertVariablesFromInterval(helper_.ModelProto(),
                                  active_rectangles[index].x_interval,
                                  variables_to_freeze);
      InsertVariablesFromInterval(helper_.ModelProto(),
                                  active_rectangles[index].y_interval,
                                  variables_to_freeze);
    }
  }

  return helper_.FixGivenVariables(initial_solution, variables_to_freeze);
}

Neighborhood RoutingRandomNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  const std::vector<std::vector<int>> all_paths =
      helper_.GetRoutingPathBooleanVariables(initial_solution);

  // Collect all unique variables.
  std::vector<int> variables_to_fix;
  for (const auto& path : all_paths) {
    variables_to_fix.insert(variables_to_fix.end(), path.begin(), path.end());
  }
  gtl::STLSortAndRemoveDuplicates(&variables_to_fix);
  GetRandomSubset(1.0 - data.difficulty, &variables_to_fix, random);

  Bitset64<int> to_fix(helper_.NumVariables());
  for (const int var : variables_to_fix) to_fix.Set(var);
  return helper_.FixGivenVariables(initial_solution, to_fix);
}

Neighborhood RoutingPathNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<std::vector<int>> all_paths =
      helper_.GetRoutingPathBooleanVariables(initial_solution);

  // Remove a corner case where all paths are empty.
  if (all_paths.empty()) {
    return helper_.NoNeighborhood();
  }

  // Collect all unique variables.
  std::vector<int> all_path_variables;
  int sum_of_path_sizes = 0;
  for (const auto& path : all_paths) {
    sum_of_path_sizes += path.size();
  }
  all_path_variables.reserve(sum_of_path_sizes);
  for (const auto& path : all_paths) {
    all_path_variables.insert(all_path_variables.end(), path.begin(),
                              path.end());
  }
  gtl::STLSortAndRemoveDuplicates(&all_path_variables);

  // Select target number of variables to relax.
  const int num_variables_to_relax =
      static_cast<int>(all_path_variables.size() * data.difficulty);
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
  Bitset64<int> to_fix(helper_.NumVariables());
  for (const int var : all_path_variables) {
    if (!relaxed_variables.contains(var)) to_fix.Set(var);
  }
  return helper_.FixGivenVariables(initial_solution, to_fix);
}

Neighborhood RoutingFullPathNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, SolveData& data,
    absl::BitGenRef random) {
  std::vector<std::vector<int>> all_paths =
      helper_.GetRoutingPathBooleanVariables(initial_solution);

  // Remove a corner case where all paths are empty.
  if (all_paths.empty()) {
    return helper_.NoNeighborhood();
  }

  // Collect all unique variables.
  std::vector<int> all_path_variables;
  int sum_of_path_sizes = 0;
  for (const auto& path : all_paths) {
    sum_of_path_sizes += path.size();
  }
  all_path_variables.reserve(sum_of_path_sizes);
  for (const auto& path : all_paths) {
    all_path_variables.insert(all_path_variables.end(), path.begin(),
                              path.end());
  }
  gtl::STLSortAndRemoveDuplicates(&all_path_variables);

  // Select target number of variables to relax.
  const int num_variables_to_relax =
      static_cast<int>(all_path_variables.size() * data.difficulty);
  absl::flat_hash_set<int> relaxed_variables;

  // Relax the start and end of each path to ease relocation.
  // TODO(user): Restrict this if the difficulty is very low.
  for (const auto& path : all_paths) {
    relaxed_variables.insert(path.front());
    relaxed_variables.insert(path.back());
  }

  // Relax all variables, if possible, of one random path.
  const int path_index = absl::Uniform<int>(random, 0, all_paths.size());
  std::shuffle(all_paths[path_index].begin(), all_paths[path_index].end(),
               random);
  while (relaxed_variables.size() < num_variables_to_relax &&
         !all_paths[path_index].empty()) {
    relaxed_variables.insert(all_paths[path_index].back());
    all_paths[path_index].pop_back();
  }

  // Relax more variables until the target is reached.
  if (relaxed_variables.size() < num_variables_to_relax) {
    std::shuffle(all_path_variables.begin(), all_path_variables.end(), random);
    while (relaxed_variables.size() < num_variables_to_relax) {
      relaxed_variables.insert(all_path_variables.back());
      all_path_variables.pop_back();
    }
  }

  // Compute the set of variables to fix.
  Bitset64<int> to_fix(helper_.NumVariables());
  for (const int var : all_path_variables) {
    if (!relaxed_variables.contains(var)) to_fix.Set(var);
  }
  return helper_.FixGivenVariables(initial_solution, to_fix);
}

bool RelaxationInducedNeighborhoodGenerator::ReadyToGenerate() const {
  return (incomplete_solutions_->HasSolution() ||
          lp_solutions_->NumSolutions() > 0);
}

Neighborhood RelaxationInducedNeighborhoodGenerator::Generate(
    const CpSolverResponse& /*initial_solution*/, SolveData& data,
    absl::BitGenRef random) {
  Neighborhood neighborhood = helper_.FullNeighborhood();
  neighborhood.is_generated = false;

  const ReducedDomainNeighborhood reduced_domains =
      GetRinsRensNeighborhood(response_manager_, lp_solutions_,
                              incomplete_solutions_, data.difficulty, random);

  if (reduced_domains.fixed_vars.empty() &&
      reduced_domains.reduced_domain_vars.empty()) {
    return neighborhood;
  }
  neighborhood.source_info = reduced_domains.source_info;

  absl::ReaderMutexLock graph_lock(helper_.graph_mutex_);
  // Fix the variables in the local model.
  for (const std::pair</*model_var*/ int, /*value*/ int64_t>& fixed_var :
       reduced_domains.fixed_vars) {
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
       reduced_domains.reduced_domain_vars) {
    const int var = reduced_var.first;
    const int64_t lb = reduced_var.second.first;
    const int64_t ub = reduced_var.second.second;
    if (var >= neighborhood.delta.variables_size()) continue;
    if (!helper_.IsActive(var)) continue;
    const Domain domain =
        ReadDomainFromProto(neighborhood.delta.variables(var));
    Domain new_domain = domain.IntersectionWith(Domain(lb, ub));
    if (new_domain.IsEmpty()) {
      new_domain = Domain::FromValues(
          {domain.ClosestValue(lb), domain.ClosestValue(ub)});
    }
    FillDomainInProto(domain, neighborhood.delta.mutable_variables(var));
    neighborhood.is_reduced = true;
  }
  neighborhood.is_generated = true;
  return neighborhood;
}

}  // namespace sat
}  // namespace operations_research
