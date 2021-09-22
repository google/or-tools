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
#include "absl/synchronization/mutex.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
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
}

void NeighborhoodGeneratorHelper::RecomputeHelperData() {
  // Recompute all the data in case new variables have been fixed.
  //
  // TODO(user): Ideally we should ignore trivially true/false constraint, but
  // this will duplicate already existing code :-( we should probably still do
  // at least enforcement literal and clauses? We could maybe run a light
  // presolve?
  absl::MutexLock graph_lock(&graph_mutex_);
  absl::ReaderMutexLock domain_lock(&domain_mutex_);

  var_to_constraint_.assign(model_proto_.variables_size(), {});
  constraint_to_var_.assign(model_proto_.constraints_size(), {});

  for (int ct_index = 0; ct_index < model_proto_.constraints_size();
       ++ct_index) {
    for (const int var : UsedVariables(model_proto_.constraints(ct_index))) {
      DCHECK(RefIsPositive(var));
      if (IsConstant(var)) continue;
      var_to_constraint_[var].push_back(ct_index);
      constraint_to_var_[ct_index].push_back(var);
    }

    // We replace intervals by their underlying integer variables.
    if (parameters_.lns_expand_intervals_in_constraint_graph()) {
      for (const int interval :
           UsedIntervals(model_proto_.constraints(ct_index))) {
        for (const int var :
             UsedVariables(model_proto_.constraints(interval))) {
          DCHECK(RefIsPositive(var));
          if (IsConstant(var)) continue;
          var_to_constraint_[var].push_back(ct_index);
          constraint_to_var_[ct_index].push_back(var);
        }
      }
    }
  }

  active_variables_.clear();
  active_variables_set_.assign(model_proto_.variables_size(), false);

  if (parameters_.lns_focus_on_decision_variables()) {
    for (const auto& search_strategy : model_proto_.search_strategy()) {
      for (const int var : search_strategy.variables()) {
        const int pos_var = PositiveRef(var);
        if (!active_variables_set_[pos_var] && !IsConstant(pos_var)) {
          active_variables_set_[pos_var] = true;
          active_variables_.push_back(pos_var);
        }
      }
    }

    // Revert to no focus if active_variables_ is empty().
    if (!active_variables_.empty()) return;
  }

  // Add all non-constant variables.
  for (int i = 0; i < model_proto_.variables_size(); ++i) {
    if (!IsConstant(i)) {
      active_variables_.push_back(i);
      active_variables_set_[i] = true;
    }
  }
}

bool NeighborhoodGeneratorHelper::IsActive(int var) const {
  return active_variables_set_[var];
}

bool NeighborhoodGeneratorHelper::IsConstant(int var) const {
  return model_proto_with_only_variables_.variables(var).domain_size() == 2 &&
         model_proto_with_only_variables_.variables(var).domain(0) ==
             model_proto_with_only_variables_.variables(var).domain(1);
}

bool NeighborhoodGeneratorHelper::CopyAndFixVariables(
    const CpModelProto& source_model,
    const absl::flat_hash_set<int>& fixed_variables_set,
    const CpSolverResponse& initial_solution,
    CpModelProto* output_model) const {
  output_model->mutable_variables()->Clear();
  output_model->mutable_variables()->Reserve(source_model.variables_size());
  for (int i = 0; i < source_model.variables_size(); ++i) {
    IntegerVariableProto* var_proto = output_model->add_variables();
    const IntegerVariableProto& source_var_proto = source_model.variables(i);
    // We only copy the variable names in debug mode.
    if (DEBUG_MODE && !source_var_proto.name().empty()) {
      var_proto->set_name(source_var_proto.name());
    }
    if (fixed_variables_set.contains(i)) {
      const int64_t value = initial_solution.solution(i);
      if (!DomainInProtoContains(source_model.variables(i), value)) {
        return false;
      }
      var_proto->add_domain(value);
      var_proto->add_domain(value);
    } else {
      *var_proto->mutable_domain() = source_var_proto.domain();
    }
  }
  return true;
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
      if (interval_ct.interval().has_start_view()) {
        bool is_constant = true;
        for (const int v : interval_ct.interval().start_view().vars()) {
          if (!IsConstant(v)) {
            is_constant = false;
            break;
          }
        }
        for (const int v : interval_ct.interval().size_view().vars()) {
          if (!IsConstant(v)) {
            is_constant = false;
            break;
          }
        }
        for (const int v : interval_ct.interval().end_view().vars()) {
          if (!IsConstant(v)) {
            is_constant = false;
            break;
          }
        }
        if (is_constant) continue;
      } else {
        if (IsConstant(PositiveRef(interval_ct.interval().start())) &&
            IsConstant(PositiveRef(interval_ct.interval().size())) &&
            IsConstant(PositiveRef(interval_ct.interval().end()))) {
          continue;
        }
      }
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
    const CpSolverResponse& initial_solution,
    const absl::flat_hash_set<int>& variables_to_fix) const {
  Neighborhood neighborhood;

  bool copy_is_successful = true;
  {
    absl::ReaderMutexLock domain_lock(&domain_mutex_);
    copy_is_successful =
        CopyAndFixVariables(model_proto_with_only_variables_, variables_to_fix,
                            initial_solution, &neighborhood.delta);
  }

  if (!copy_is_successful) {
    return NoNeighborhood();
  }

  AddSolutionHinting(initial_solution, &neighborhood.delta);

  neighborhood.is_generated = true;
  neighborhood.is_reduced = !variables_to_fix.empty();
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
  std::vector<int> active_constraints;
  for (int ct = 0; ct < helper_.ModelProto().constraints_size(); ++ct) {
    if (helper_.ModelProto().constraints(ct).constraint_case() ==
        ConstraintProto::CONSTRAINT_NOT_SET) {
      continue;
    }
    active_constraints.push_back(ct);
  }

  if (active_constraints.empty() ||
      helper_.DifficultyMeansFullNeighborhood(difficulty)) {
    return helper_.FullNeighborhood();
  }

  std::shuffle(active_constraints.begin(), active_constraints.end(), random);

  const int num_model_vars = helper_.ModelProto().variables_size();
  const int num_model_constraints = helper_.ModelProto().constraints_size();

  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;

  {
    absl::ReaderMutexLock graph_lock(&helper_.graph_mutex_);
    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(difficulty * num_active_vars);
    CHECK_GT(target_size, 0);

    for (const int constraint_index : active_constraints) {
      CHECK_LT(constraint_index, num_model_constraints);
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

  // Start by a random constraint.
  next_constraints.push_back(
      absl::Uniform<int>(random, 0, num_model_constraints));
  added_constraints[next_constraints.back()] = true;

  std::vector<int> random_variables;
  {
    absl::ReaderMutexLock graph_lock(&helper_.graph_mutex_);
    const int num_active_vars =
        helper_.ActiveVariablesWhileHoldingLock().size();
    const int target_size = std::ceil(difficulty * num_active_vars);
    CHECK_GT(target_size, 0);

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
      CHECK_LT(constraint_index, num_model_constraints);
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

LinearExpressionProto GetStart(const IntervalConstraintProto& interval) {
  if (interval.has_start_view()) return interval.start_view();
  LinearExpressionProto result;
  result.add_vars(interval.start());
  result.add_coeffs(1);
  return result;
}

LinearExpressionProto GetSize(const IntervalConstraintProto& interval) {
  if (interval.has_size_view()) return interval.size_view();
  LinearExpressionProto result;
  result.add_vars(interval.size());
  result.add_coeffs(1);
  return result;
}

LinearExpressionProto GetEnd(const IntervalConstraintProto& interval) {
  if (interval.has_end_view()) return interval.end_view();
  LinearExpressionProto result;
  result.add_vars(interval.end());
  result.add_coeffs(1);
  return result;
}

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
    const LinearExpressionProto size_var = GetSize(interval_ct.interval());
    if (GetLinearExpressionValue(size_var, initial_solution) == 0) continue;

    const LinearExpressionProto start_var = GetStart(interval_ct.interval());
    const int64_t start_value =
        GetLinearExpressionValue(start_var, initial_solution);

    start_interval_pairs.push_back({start_value, i});
  }
  std::sort(start_interval_pairs.begin(), start_interval_pairs.end());

  // Add precedence between the remaining intervals, forcing their order.
  for (int i = 0; i + 1 < start_interval_pairs.size(); ++i) {
    const LinearExpressionProto before_start =
        GetStart(helper.ModelProto()
                     .constraints(start_interval_pairs[i].second)
                     .interval());
    const LinearExpressionProto before_end =
        GetEnd(helper.ModelProto()
                   .constraints(start_interval_pairs[i].second)
                   .interval());
    const LinearExpressionProto after_start =
        GetStart(helper.ModelProto()
                     .constraints(start_interval_pairs[i + 1].second)
                     .interval());

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
    const LinearExpressionProto start_var = GetStart(interval_ct.interval());
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
      case ConstraintProto::kIntMax:
      case ConstraintProto::kLinMax:
      case ConstraintProto::kIntMin:
      case ConstraintProto::kLinMin:
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
