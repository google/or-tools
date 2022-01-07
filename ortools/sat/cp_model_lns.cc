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

#include "ortools/sat/cp_model_lns.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "ortools/graph/connected_components.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

NeighborhoodGeneratorHelper::NeighborhoodGeneratorHelper(
    CpModelProto const* model_proto, SatParameters const* parameters,
    SharedResponseManager* shared_response, SharedTimeLimit* shared_time_limit,
    SharedBoundsManager* shared_bounds)
    : SubSolver(""),
      parameters_(*parameters),
      model_proto_(*model_proto),
      shared_time_limit_(shared_time_limit),
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
    // linear constraint somewhere else.
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
    CHECK_LT(root, var_to_component_index_.size());
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
  shared_response_->LogMessage(
      absl::StrCat("var:", active_variables_.size(), "/", num_variables,
                   " constraints:", simplied_model_proto_.constraints().size(),
                   "/", model_proto_.constraints().size(), compo_message));
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
    const IntegerValue best_objective_improvement =
        IsRelaxationGenerator()
            ? IntegerValue(CapSub(data.new_objective_bound.value(),
                                  data.initial_best_objective_bound.value()))
            : IntegerValue(CapSub(data.initial_best_objective.value(),
                                  data.new_objective.value()));
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

void GetRandomSubset(double relative_size, std::vector<int>* base,
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
    CHECK_GT(target_size, 0);

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
    CHECK_GT(target_size, 0) << difficulty << " " << num_active_vars;

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
    CHECK_GT(target_size, 0);

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
      CHECK_LT(constraint_index, num_active_constraints);
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

int64_t GetLinearExpressionValue(const LinearExpressionProto& expr,
                                 const CpSolverResponse& initial_solution) {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    result += expr.coeffs(i) * initial_solution.solution(expr.vars(i));
  }
  return result;
}

void AddLinearExpressionToConstraint(const int64_t coeff,
                                     const LinearExpressionProto& expr,
                                     LinearConstraintProto* constraint,
                                     int64_t* rhs_offset) {
  *rhs_offset -= coeff * expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    constraint->add_vars(expr.vars(i));
    constraint->add_coeffs(expr.coeffs(i) * coeff);
  }
}

void AddPrecedenceConstraints(const absl::Span<const int> intervals,
                              const absl::flat_hash_set<int>& ignored_intervals,
                              const CpSolverResponse& initial_solution,
                              const NeighborhoodGeneratorHelper& helper,
                              Neighborhood* neighborhood) {
  // Sort all non-relaxed intervals of this constraint by current start
  // time.
  std::vector<std::pair<int64_t, int>> start_interval_pairs;
  for (const int i : intervals) {
    if (ignored_intervals.contains(i)) continue;
    const ConstraintProto& interval_ct = helper.ModelProto().constraints(i);

    // TODO(user): we ignore size zero for now.
    const LinearExpressionProto& size_var = interval_ct.interval().size();
    if (GetLinearExpressionValue(size_var, initial_solution) == 0) continue;

    const LinearExpressionProto& start_var = interval_ct.interval().start();
    const int64_t start_value =
        GetLinearExpressionValue(start_var, initial_solution);

    start_interval_pairs.push_back({start_value, i});
  }
  std::sort(start_interval_pairs.begin(), start_interval_pairs.end());

  // Add precedence between the remaining intervals, forcing their order.
  for (int i = 0; i + 1 < start_interval_pairs.size(); ++i) {
    const LinearExpressionProto& before_start =
        helper.ModelProto()
            .constraints(start_interval_pairs[i].second)
            .interval()
            .start();
    const LinearExpressionProto& before_end =
        helper.ModelProto()
            .constraints(start_interval_pairs[i].second)
            .interval()
            .end();
    const LinearExpressionProto& after_start =
        helper.ModelProto()
            .constraints(start_interval_pairs[i + 1].second)
            .interval()
            .start();

    // If the end was smaller we keep it that way, otherwise we just order the
    // start variables.
    LinearConstraintProto* linear =
        neighborhood->delta.add_constraints()->mutable_linear();
    linear->add_domain(std::numeric_limits<int64_t>::min());
    int64_t rhs_offset = 0;
    if (GetLinearExpressionValue(before_end, initial_solution) <=
        GetLinearExpressionValue(after_start, initial_solution)) {
      // If the end was smaller than the next start, keep it that way.
      AddLinearExpressionToConstraint(1, before_end, linear, &rhs_offset);
    } else {
      // Otherwise, keep the same minimum separation. This is done in order
      // to "simplify" the neighborhood.
      rhs_offset = GetLinearExpressionValue(before_start, initial_solution) -
                   GetLinearExpressionValue(after_start, initial_solution);
      AddLinearExpressionToConstraint(1, before_start, linear, &rhs_offset);
    }

    AddLinearExpressionToConstraint(-1, after_start, linear, &rhs_offset);
    linear->add_domain(rhs_offset);

    // The linear constraint should be satisfied by the current solution.
    if (DEBUG_MODE) {
      int64_t activity = 0;
      for (int i = 0; i < linear->vars().size(); ++i) {
        activity +=
            linear->coeffs(i) * initial_solution.solution(linear->vars(i));
      }
      CHECK_GE(activity, linear->domain(0));
      CHECK_LE(activity, linear->domain(1));
    }
  }
}
}  // namespace

Neighborhood GenerateSchedulingNeighborhoodForRelaxation(
    const absl::Span<const int> intervals_to_relax,
    const CpSolverResponse& initial_solution,
    const NeighborhoodGeneratorHelper& helper) {
  Neighborhood neighborhood = helper.FullNeighborhood();
  neighborhood.is_reduced =
      (intervals_to_relax.size() <
       helper.TypeToConstraints(ConstraintProto::kInterval).size());

  // We will extend the set with some interval that we cannot fix.
  absl::flat_hash_set<int> ignored_intervals(intervals_to_relax.begin(),
                                             intervals_to_relax.end());

  // Fix the presence/absence of non-relaxed intervals.
  for (const int i : helper.TypeToConstraints(ConstraintProto::kInterval)) {
    DCHECK_GE(i, 0);
    if (ignored_intervals.contains(i)) continue;

    const ConstraintProto& interval_ct = helper.ModelProto().constraints(i);
    if (interval_ct.enforcement_literal().empty()) continue;

    CHECK_EQ(interval_ct.enforcement_literal().size(), 1);
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

  for (const int c : helper.TypeToConstraints(ConstraintProto::kNoOverlap)) {
    AddPrecedenceConstraints(
        helper.ModelProto().constraints(c).no_overlap().intervals(),
        ignored_intervals, initial_solution, helper, &neighborhood);
  }
  for (const int c : helper.TypeToConstraints(ConstraintProto::kCumulative)) {
    AddPrecedenceConstraints(
        helper.ModelProto().constraints(c).cumulative().intervals(),
        ignored_intervals, initial_solution, helper, &neighborhood);
  }
  for (const int c : helper.TypeToConstraints(ConstraintProto::kNoOverlap2D)) {
    AddPrecedenceConstraints(
        helper.ModelProto().constraints(c).no_overlap_2d().x_intervals(),
        ignored_intervals, initial_solution, helper, &neighborhood);
    AddPrecedenceConstraints(
        helper.ModelProto().constraints(c).no_overlap_2d().y_intervals(),
        ignored_intervals, initial_solution, helper, &neighborhood);
  }

  // Set the current solution as a hint.
  helper.AddSolutionHinting(initial_solution, &neighborhood.delta);
  neighborhood.is_generated = true;

  return neighborhood;
}

Neighborhood SchedulingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<int> intervals_to_relax =
      helper_.GetActiveIntervals(initial_solution);
  GetRandomSubset(difficulty, &intervals_to_relax, random);

  return GenerateSchedulingNeighborhoodForRelaxation(intervals_to_relax,
                                                     initial_solution, helper_);
}

Neighborhood SchedulingTimeWindowNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<std::pair<int64_t, int>> start_interval_pairs;
  const std::vector<int> active_intervals =
      helper_.GetActiveIntervals(initial_solution);
  std::vector<int> intervals_to_relax;

  if (active_intervals.empty()) return helper_.FullNeighborhood();

  for (const int i : active_intervals) {
    const ConstraintProto& interval_ct = helper_.ModelProto().constraints(i);
    const LinearExpressionProto& start_var = interval_ct.interval().start();
    const int64_t start_value =
        GetLinearExpressionValue(start_var, initial_solution);
    start_interval_pairs.push_back({start_value, i});
  }
  std::sort(start_interval_pairs.begin(), start_interval_pairs.end());
  const int relaxed_size = std::floor(difficulty * start_interval_pairs.size());

  std::uniform_int_distribution<int> random_var(
      0, start_interval_pairs.size() - relaxed_size - 1);
  const int random_start_index = random_var(random);

  // TODO(user): Consider relaxing more than one time window
  // intervals. This seems to help with Giza models.
  for (int i = random_start_index; i < relaxed_size; ++i) {
    intervals_to_relax.push_back(start_interval_pairs[i].second);
  }

  return GenerateSchedulingNeighborhoodForRelaxation(intervals_to_relax,
                                                     initial_solution, helper_);
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
    const CpSolverResponse& initial_solution, double difficulty,
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
  for (const std::pair</*model_var*/ int, /*value*/ int64_t> fixed_var :
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
                       /*domain*/ std::pair<int64_t, int64_t>>
           reduced_var : rins_neighborhood.reduced_domain_vars) {
    const int var = reduced_var.first;
    const int64_t lb = reduced_var.second.first;
    const int64_t ub = reduced_var.second.second;
    if (var >= neighborhood.delta.variables_size()) continue;
    if (!helper_.IsActive(var)) continue;
    Domain domain = ReadDomainFromProto(neighborhood.delta.variables(var));
    domain = domain.IntersectionWith(Domain(lb, ub));
    if (domain.IsEmpty()) {
      // TODO(user): Instead of aborting, pick the closest point in the domain?
      return neighborhood;
    }
    FillDomainInProto(domain, neighborhood.delta.mutable_variables(var));
    neighborhood.is_reduced = true;
  }
  neighborhood.is_generated = true;
  return neighborhood;
}

Neighborhood ConsecutiveConstraintsRelaxationNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  std::vector<int> removable_constraints;
  const int num_constraints = helper_.ModelProto().constraints_size();
  removable_constraints.reserve(num_constraints);
  for (int c = 0; c < num_constraints; ++c) {
    // Removing intervals is not easy because other constraint might require
    // them, so for now, we don't remove them.
    if (helper_.ModelProto().constraints(c).constraint_case() ==
        ConstraintProto::kInterval) {
      continue;
    }
    removable_constraints.push_back(c);
  }

  const int target_size =
      std::round((1.0 - difficulty) * removable_constraints.size());

  const int random_start_index =
      absl::Uniform<int>(random, 0, removable_constraints.size());
  std::vector<int> removed_constraints;
  removed_constraints.reserve(target_size);
  int c = random_start_index;
  while (removed_constraints.size() < target_size) {
    removed_constraints.push_back(removable_constraints[c]);
    ++c;
    if (c == removable_constraints.size()) {
      c = 0;
    }
  }

  return helper_.RemoveMarkedConstraints(removed_constraints);
}

WeightedRandomRelaxationNeighborhoodGenerator::
    WeightedRandomRelaxationNeighborhoodGenerator(
        NeighborhoodGeneratorHelper const* helper, const std::string& name)
    : NeighborhoodGenerator(name, helper) {
  std::vector<int> removable_constraints;
  const int num_constraints = helper_.ModelProto().constraints_size();
  constraint_weights_.reserve(num_constraints);
  // TODO(user): Experiment with different starting weights.
  for (int c = 0; c < num_constraints; ++c) {
    switch (helper_.ModelProto().constraints(c).constraint_case()) {
      case ConstraintProto::kCumulative:
      case ConstraintProto::kAllDiff:
      case ConstraintProto::kElement:
      case ConstraintProto::kRoutes:
      case ConstraintProto::kCircuit:
        constraint_weights_.push_back(3.0);
        num_removable_constraints_++;
        break;
      case ConstraintProto::kBoolOr:
      case ConstraintProto::kBoolAnd:
      case ConstraintProto::kBoolXor:
      case ConstraintProto::kIntProd:
      case ConstraintProto::kIntDiv:
      case ConstraintProto::kIntMod:
      case ConstraintProto::kLinMax:
      case ConstraintProto::kNoOverlap:
      case ConstraintProto::kNoOverlap2D:
        constraint_weights_.push_back(2.0);
        num_removable_constraints_++;
        break;
      case ConstraintProto::kLinear:
      case ConstraintProto::kTable:
      case ConstraintProto::kAutomaton:
      case ConstraintProto::kInverse:
      case ConstraintProto::kReservoir:
      case ConstraintProto::kAtMostOne:
      case ConstraintProto::kExactlyOne:
        constraint_weights_.push_back(1.0);
        num_removable_constraints_++;
        break;
      case ConstraintProto::CONSTRAINT_NOT_SET:
      case ConstraintProto::kDummyConstraint:
      case ConstraintProto::kInterval:
        // Removing intervals is not easy because other constraint might require
        // them, so for now, we don't remove them.
        constraint_weights_.push_back(0.0);
        break;
    }
  }
}

void WeightedRandomRelaxationNeighborhoodGenerator::
    AdditionalProcessingOnSynchronize(const SolveData& solve_data) {
  const IntegerValue best_objective_improvement =
      solve_data.new_objective_bound - solve_data.initial_best_objective_bound;

  const std::vector<int>& removed_constraints =
      removed_constraints_[solve_data.neighborhood_id];

  // Heuristic: We change the weights of the removed constraints if the
  // neighborhood is solved (status is OPTIMAL or INFEASIBLE) or we observe an
  // improvement in objective bounds. Otherwise we assume that the
  // difficulty/time wasn't right for us to record feedbacks.
  //
  // If the objective bounds are improved, we bump up the weights. If the
  // objective bounds are worse and the problem status is OPTIMAL, we bump down
  // the weights. Otherwise if the new objective bounds are same as current
  // bounds (which happens a lot on some instances), we do not update the
  // weights as we do not have a clear signal whether the constraints removed
  // were good choices or not.
  // TODO(user): We can improve this heuristic with more experiments.
  if (best_objective_improvement > 0) {
    // Bump up the weights of all removed constraints.
    for (int c : removed_constraints) {
      if (constraint_weights_[c] <= 90.0) {
        constraint_weights_[c] += 10.0;
      } else {
        constraint_weights_[c] = 100.0;
      }
    }
  } else if (solve_data.status == CpSolverStatus::OPTIMAL &&
             best_objective_improvement < 0) {
    // Bump down the weights of all removed constraints.
    for (int c : removed_constraints) {
      if (constraint_weights_[c] > 0.5) {
        constraint_weights_[c] -= 0.5;
      }
    }
  }
  removed_constraints_.erase(solve_data.neighborhood_id);
}

Neighborhood WeightedRandomRelaxationNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    absl::BitGenRef random) {
  const int target_size =
      std::round((1.0 - difficulty) * num_removable_constraints_);

  std::vector<int> removed_constraints;

  // Generate a random number between (0,1) = u[i] and use score[i] =
  // u[i]^(1/w[i]) and then select top k items with largest scores.
  // Reference: https://utopia.duth.gr/~pefraimi/research/data/2007EncOfAlg.pdf
  std::vector<std::pair<double, int>> constraint_removal_scores;
  std::uniform_real_distribution<double> random_var(0.0, 1.0);
  for (int c = 0; c < constraint_weights_.size(); ++c) {
    if (constraint_weights_[c] <= 0) continue;
    const double u = random_var(random);
    const double score = std::pow(u, (1 / constraint_weights_[c]));
    constraint_removal_scores.push_back({score, c});
  }
  std::sort(constraint_removal_scores.rbegin(),
            constraint_removal_scores.rend());
  for (int i = 0; i < target_size; ++i) {
    removed_constraints.push_back(constraint_removal_scores[i].second);
  }

  Neighborhood result = helper_.RemoveMarkedConstraints(removed_constraints);

  absl::MutexLock mutex_lock(&generator_mutex_);
  result.id = next_available_id_;
  next_available_id_++;
  removed_constraints_.insert({result.id, removed_constraints});
  return result;
}

}  // namespace sat
}  // namespace operations_research
