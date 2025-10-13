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

#include "ortools/sat/shaving_solver.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "ortools/graph/connected_components.h"
#include "ortools/sat/cp_model_copy.h"
#include "ortools/sat/cp_model_lns.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_solver_helpers.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

ObjectiveShavingSolver::ObjectiveShavingSolver(
    const SatParameters& local_parameters, NeighborhoodGeneratorHelper* helper,
    SharedClasses* shared)
    : SubSolver(local_parameters.name(), FULL_PROBLEM),
      local_params_(local_parameters),
      helper_(helper),
      shared_(shared),
      local_proto_(shared->model_proto) {}

ObjectiveShavingSolver::~ObjectiveShavingSolver() {
  shared_->stat_tables->AddTimingStat(*this);
}

bool ObjectiveShavingSolver::TaskIsAvailable() {
  if (shared_->SearchIsDone()) return false;

  // We only support one task at the time.
  absl::MutexLock mutex_lock(mutex_);
  return !task_in_flight_;
}

std::function<void()> ObjectiveShavingSolver::GenerateTask(int64_t task_id) {
  {
    absl::MutexLock mutex_lock(mutex_);
    stop_current_chunk_.store(false);
    task_in_flight_ = true;
    objective_lb_ = shared_->response->GetInnerObjectiveLowerBound();
    objective_ub_ = shared_->response->GetInnerObjectiveUpperBound();
  }
  return [this, task_id]() {
    if (ResetAndSolveModel(task_id)) {
      const CpSolverResponse local_response =
          local_sat_model_->GetOrCreate<SharedResponseManager>()->GetResponse();

      if (local_response.status() == CpSolverStatus::OPTIMAL ||
          local_response.status() == CpSolverStatus::FEASIBLE) {
        std::vector<int64_t> solution_values(local_response.solution().begin(),
                                             local_response.solution().end());
        if (local_params_.cp_model_presolve()) {
          const int num_original_vars = shared_->model_proto.variables_size();
          PostsolveResponseWrapper(local_params_, num_original_vars,
                                   mapping_proto_, postsolve_mapping_,
                                   &solution_values);
        }
        shared_->response->NewSolution(solution_values, Info());
      } else if (local_response.status() == CpSolverStatus::INFEASIBLE) {
        absl::MutexLock mutex_lock(mutex_);
        shared_->response->UpdateInnerObjectiveBounds(
            Info(), current_objective_target_ub_ + 1, objective_ub_);
      }
    }

    absl::MutexLock mutex_lock(mutex_);
    task_in_flight_ = false;
    if (local_sat_model_ != nullptr) {
      const double dtime = local_sat_model_->GetOrCreate<TimeLimit>()
                               ->GetElapsedDeterministicTime();
      AddTaskDeterministicDuration(dtime);
      shared_->time_limit->AdvanceDeterministicTime(dtime);
    }
  };
}

void ObjectiveShavingSolver::Synchronize() {
  absl::MutexLock mutex_lock(mutex_);
  if (!task_in_flight_) return;

  // We are just waiting for the inner code to check the time limit or
  // to return nicely.
  if (stop_current_chunk_) return;

  // TODO(user): Also stop if we have enough newly fixed / improved root level
  // bounds so that we think it is worth represolving and restarting.
  if (shared_->SearchIsDone()) {
    stop_current_chunk_.store(true);
  }

  // The current objective lower bound has been improved, restarting.
  if (shared_->response->GetInnerObjectiveLowerBound() > objective_lb_) {
    stop_current_chunk_.store(true);
  }

  // A solution has been found that is better than the current target
  // objective upper bound. Restarting to use a smaller delta.
  if (shared_->response->GetInnerObjectiveUpperBound() <=
          current_objective_target_ub_ &&
      current_objective_target_ub_ != objective_lb_) {
    stop_current_chunk_.store(true);
  }

  // If the range has been reduced enough to warrant a delta of 1, while the
  // current search uses a delta > 1. Restarting to switch to the delta of 1.
  if (current_objective_target_ub_ != objective_lb_ &&
      shared_->response->GetInnerObjectiveUpperBound() -
              shared_->response->GetInnerObjectiveLowerBound() <=
          local_params_.shaving_search_threshold()) {
    stop_current_chunk_.store(true);
  }
}

std::string ObjectiveShavingSolver::Info() {
  return absl::StrCat(name(), " (vars=", local_proto_.variables().size(),
                      " csts=", local_proto_.constraints().size(), ")");
}

bool ObjectiveShavingSolver::ResetAndSolveModel(int64_t task_id) {
  local_sat_model_ = std::make_unique<Model>(name());
  *local_sat_model_->GetOrCreate<SatParameters>() = local_params_;
  local_sat_model_->GetOrCreate<SatParameters>()->set_random_seed(
      CombineSeed(local_params_.random_seed(), task_id));

  auto* time_limit = local_sat_model_->GetOrCreate<TimeLimit>();
  shared_->time_limit->UpdateLocalLimit(time_limit);
  time_limit->RegisterSecondaryExternalBooleanAsLimit(&stop_current_chunk_);

  auto* random = local_sat_model_->GetOrCreate<ModelRandomGenerator>();

  // We copy the model.
  local_proto_ = shared_->model_proto;
  *local_proto_.mutable_variables() =
      helper_->FullNeighborhood().delta.variables();

  // Store the current lb in local variable.
  IntegerValue objective_lb;
  IntegerValue chosen_objective_ub;
  {
    absl::MutexLock mutex_lock(mutex_);
    objective_lb = objective_lb_;
    if (objective_ub_ - objective_lb <=
        local_params_.shaving_search_threshold()) {
      current_objective_target_ub_ = objective_lb;
    } else {
      const IntegerValue mid = (objective_ub_ - objective_lb) / 2;
      current_objective_target_ub_ =
          objective_lb + absl::LogUniform<int64_t>(*random, 0, mid.value());
    }
    chosen_objective_ub = current_objective_target_ub_;
    VLOG(2) << name() << ": from [" << objective_lb.value() << ".."
            << objective_ub_.value() << "] <= " << chosen_objective_ub.value();
  }

  // We replace the objective by a constraint, objective in [lb, target_ub].
  // We modify local_proto_ to a pure feasibility problem.
  // Not having the objective open up more presolve reduction.
  Domain obj_domain = Domain(objective_lb.value(), chosen_objective_ub.value());
  if (local_proto_.objective().domain_size() > 1) {
    // Intersect with the first interval of the objective domain.
    obj_domain =
        obj_domain.IntersectionWith(Domain(local_proto_.objective().domain(0),
                                           local_proto_.objective().domain(1)));
  }
  if (local_proto_.objective().vars().size() == 1 &&
      local_proto_.objective().coeffs(0) == 1) {
    auto* obj_var =
        local_proto_.mutable_variables(local_proto_.objective().vars(0));
    const Domain reduced_var_domain = obj_domain.IntersectionWith(
        Domain(obj_var->domain(0), obj_var->domain(1)));
    if (reduced_var_domain.IsEmpty()) {
      return false;
    }
    FillDomainInProto(reduced_var_domain, obj_var);
  } else {
    auto* obj = local_proto_.add_constraints()->mutable_linear();
    *obj->mutable_vars() = local_proto_.objective().vars();
    *obj->mutable_coeffs() = local_proto_.objective().coeffs();
    if (obj_domain.IsEmpty()) {
      return false;
    }
    FillDomainInProto(obj_domain, obj);
  }

  // Clear the objective.
  local_proto_.clear_objective();
  local_proto_.set_name(
      absl::StrCat(local_proto_.name(), "_obj_shaving_", objective_lb.value()));

  // Dump?
  if (absl::GetFlag(FLAGS_cp_model_dump_submodels)) {
    const std::string name =
        absl::StrCat(absl::GetFlag(FLAGS_cp_model_dump_prefix),
                     "objective_shaving_", objective_lb.value(), ".pb.txt");
    LOG(INFO) << "Dumping objective shaving model to '" << name << "'.";
    CHECK(WriteModelProtoToFile(local_proto_, name));
  }

  // Presolve if asked.
  if (local_params_.cp_model_presolve()) {
    mapping_proto_.Clear();
    postsolve_mapping_.clear();
    auto context = std::make_unique<PresolveContext>(
        local_sat_model_.get(), &local_proto_, &mapping_proto_);
    const CpSolverStatus presolve_status =
        PresolveCpModel(context.get(), &postsolve_mapping_);
    if (presolve_status == CpSolverStatus::INFEASIBLE) {
      absl::MutexLock mutex_lock(mutex_);
      shared_->response->UpdateInnerObjectiveBounds(
          Info(), chosen_objective_ub + 1, kMaxIntegerValue);
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
  if (time_limit->LimitReached()) return false;

  LoadCpModel(local_proto_, local_sat_model_.get());
  return true;
}

VariablesShavingSolver::VariablesShavingSolver(
    const SatParameters& local_parameters, NeighborhoodGeneratorHelper* helper,
    SharedClasses* shared)
    : SubSolver(local_parameters.name(), INCOMPLETE),
      local_params_(local_parameters),
      shared_(shared),
      stop_current_chunk_(false),
      model_proto_(shared->model_proto) {
  if (shared_->bounds != nullptr) {
    shared_bounds_id_ = shared_->bounds->RegisterNewId();
  }

  absl::MutexLock mutex_lock(mutex_);
  for (const IntegerVariableProto& var_proto : model_proto_.variables()) {
    var_domains_.push_back(ReadDomainFromProto(var_proto));
  }
}

VariablesShavingSolver::~VariablesShavingSolver() {
  shared_->stat_tables->AddTimingStat(*this);

  if (!VLOG_IS_ON(1)) return;
  if (shared_ == nullptr || shared_->stats == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  absl::MutexLock mutex_lock(mutex_);
  stats.push_back({"variable_shaving/num_vars_tried", num_vars_tried_});
  stats.push_back({"variable_shaving/num_vars_shaved", num_vars_shaved_});
  stats.push_back(
      {"variable_shaving/num_infeasible_found", num_infeasible_found_});
  shared_->stats->AddStats(stats);
}

bool VariablesShavingSolver::TaskIsAvailable() {
  return !shared_->SearchIsDone();
}

void VariablesShavingSolver::ProcessLocalResponse(
    const CpSolverResponse& local_response, const State& state) {
  if (shared_->bounds == nullptr) return;
  if (state.shave_using_objective) {
    if (local_response.status() == CpSolverStatus::INFEASIBLE) return;
    const int64_t obj_lb = local_response.inner_objective_lower_bound();

    absl::MutexLock lock(mutex_);
    const Domain domain = var_domains_[state.var_index];
    if (state.minimize) {
      const int64_t lb = obj_lb;
      if (lb > domain.Min()) {
        ++num_vars_shaved_;
        shared_->bounds->ReportPotentialNewBounds(name(), {state.var_index},
                                                  {lb}, {domain.Max()});
        VLOG(1) << name() << ": var(" << state.var_index << ") " << domain
                << " >= " << lb;
      }
    } else {
      const int64_t ub = -obj_lb;
      if (ub < domain.Max()) {
        ++num_vars_shaved_;
        shared_->bounds->ReportPotentialNewBounds(name(), {state.var_index},
                                                  {domain.Min()}, {ub});
        VLOG(1) << name() << ": var(" << state.var_index << ") " << domain
                << " <= " << ub;
      }
    }
    return;
  }

  if (local_response.status() != CpSolverStatus::INFEASIBLE) return;
  absl::MutexLock lock(mutex_);
  const Domain domain = var_domains_[state.var_index];
  Domain new_domain = domain;
  ++num_infeasible_found_;
  new_domain = domain.IntersectionWith(state.reduced_domain.Complement());
  VLOG(1) << name() << ": var(" << state.var_index << ") " << domain << " ==> "
          << new_domain;

  if (domain != new_domain) {
    ++num_vars_shaved_;
    if (!new_domain.IsEmpty()) {
      shared_->bounds->ReportPotentialNewBounds(
          name(), {state.var_index}, {new_domain.Min()}, {new_domain.Max()});
    }
    var_domains_[state.var_index] = new_domain;
    if (var_domains_[state.var_index].IsEmpty()) {
      shared_->response->NotifyThatImprovingProblemIsInfeasible(
          "Unsat during variables shaving");
      return;
    }
  }
}

std::function<void()> VariablesShavingSolver::GenerateTask(int64_t task_id) {
  return [this, task_id]() mutable {
    Model local_sat_model;
    CpModelProto shaving_proto;
    State state;
    if (ResetAndSolveModel(task_id, &state, &local_sat_model, &shaving_proto)) {
      const CpSolverResponse local_response =
          local_sat_model.GetOrCreate<SharedResponseManager>()->GetResponse();
      ProcessLocalResponse(local_response, state);
    }

    absl::MutexLock mutex_lock(mutex_);
    const double dtime =
        local_sat_model.GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime();
    AddTaskDeterministicDuration(dtime);
    shared_->time_limit->AdvanceDeterministicTime(dtime);
  };
}

void VariablesShavingSolver::Synchronize() {
  absl::MutexLock mutex_lock(mutex_);
  // We are just waiting for the inner code to check the time limit or
  // to return nicely.
  if (stop_current_chunk_) return;

  if (shared_->SearchIsDone()) {
    stop_current_chunk_.store(true);
  }

  if (shared_->bounds != nullptr) {
    std::vector<int> model_variables;
    std::vector<int64_t> new_lower_bounds;
    std::vector<int64_t> new_upper_bounds;
    shared_->bounds->GetChangedBounds(shared_bounds_id_, &model_variables,
                                      &new_lower_bounds, &new_upper_bounds);

    for (int i = 0; i < model_variables.size(); ++i) {
      const int var = model_variables[i];
      const int64_t new_lb = new_lower_bounds[i];
      const int64_t new_ub = new_upper_bounds[i];
      const Domain& old_domain = var_domains_[var];
      const Domain new_domain =
          old_domain.IntersectionWith(Domain(new_lb, new_ub));
      if (new_domain.IsEmpty()) {
        shared_->response->NotifyThatImprovingProblemIsInfeasible(
            "Unsat during variables shaving");
        continue;
      }
      var_domains_[var] = new_domain;
    }
  }
}

std::string VariablesShavingSolver::Info() {
  return absl::StrCat(name(), " (vars=", model_proto_.variables().size(),
                      " csts=", model_proto_.constraints().size(), ")");
}

int64_t VariablesShavingSolver::DomainSize(int var) const
    ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
  return var_domains_[var].Size();
}

bool VariablesShavingSolver::VarIsFixed(int int_var) const
    ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
  return var_domains_[int_var].IsFixed();
}

bool VariablesShavingSolver::ConstraintIsInactive(int c) const
    ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
  for (const int ref : model_proto_.constraints(c).enforcement_literal()) {
    const int var = PositiveRef(ref);
    if (VarIsFixed(var) && var_domains_[var].Min() == (var == ref ? 0 : 1)) {
      return true;
    }
  }
  return false;
}

bool VariablesShavingSolver::FindNextVar(State* state)
    ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
  const int num_vars = var_domains_.size();
  ++current_index_;

  // We start by shaving the objective in order to increase the lower bound.
  if (model_proto_.has_objective()) {
    const int num_obj_vars = model_proto_.objective().vars().size();
    if (num_obj_vars > 1) {
      while (current_index_ < num_obj_vars) {
        const int var = model_proto_.objective().vars(current_index_);
        if (VarIsFixed(var)) {
          ++current_index_;
          continue;
        }
        state->var_index = var;
        state->minimize = model_proto_.objective().coeffs(current_index_) > 0;
        state->shave_using_objective = true;
        return true;
      }
    }
  }

  // Otherwise loop over all variables.
  // TODO(user): maybe we should just order all possible State, putting the
  // objective first, and just loop.
  for (int i = 0; i < num_vars; ++i) {
    const int var = (current_index_ / 2 + i) % num_vars;
    if (VarIsFixed(var)) {
      ++current_index_;
      continue;
    }

    // Let's not shave the single var objective. There are enough workers
    // looking at it.
    if (model_proto_.has_objective() &&
        model_proto_.objective().vars_size() == 1 &&
        var == model_proto_.objective().vars(0)) {
      continue;
    }

    state->var_index = var;
    state->minimize = current_index_ % 2 == 0;
    state->shave_using_objective = current_index_ / num_vars < 2;
    return true;
  }
  return false;
}

void VariablesShavingSolver::CopyModelConnectedToVar(
    State* state, Model* local_model, CpModelProto* shaving_proto,
    bool* has_no_overlap_2d) ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
  auto var_to_node = [](int var) { return var; };
  auto ct_to_node = [num_vars = model_proto_.variables_size()](int c) {
    return c + num_vars;
  };

  // Heuristic: we will ignore some complex constraint and "RELAX" them.
  const int root_index = var_to_node(state->var_index);
  std::vector<int> ignored_constraints;

  // Build the connected component graph.
  //
  // TODO(user): Add some kind of difficulty, and do a BFS instead so that
  // we don't pull in the full model when everything is connected. We can
  // reuse the helper_ graph for this.
  DenseConnectedComponentsFinder cc_finder;
  cc_finder.SetNumberOfNodes(model_proto_.constraints_size() +
                             model_proto_.variables_size());
  for (int c = 0; c < model_proto_.constraints_size(); ++c) {
    if (ConstraintIsInactive(c)) continue;

    const ConstraintProto& ct = model_proto_.constraints(c);
    if (ct.constraint_case() == ConstraintProto::kNoOverlap2D) {
      // Make sure x and y part are connected.
      *has_no_overlap_2d = true;
      const int num_intervals = ct.no_overlap_2d().x_intervals().size();
      for (int i = 0; i < num_intervals; ++i) {
        const int x_interval = ct.no_overlap_2d().x_intervals(i);
        const int y_interval = ct.no_overlap_2d().y_intervals(i);
        cc_finder.AddEdge(ct_to_node(x_interval), ct_to_node(y_interval));
      }
      ignored_constraints.push_back(c);
      continue;
    }

    const int ct_node = ct_to_node(c);
    for (const int var : UsedVariables(ct)) {
      if (VarIsFixed(var)) continue;
      cc_finder.AddEdge(ct_node, var_to_node(var));
    }
    for (const int interval : UsedIntervals(ct)) {
      cc_finder.AddEdge(ct_node, ct_to_node(interval));
    }
  }

  DCHECK(shaving_proto->variables().empty());
  DCHECK(shaving_proto->constraints().empty());

  const auto active_constraints = [&cc_finder, root_index, &ct_to_node](int c) {
    return cc_finder.Connected(root_index, ct_to_node(c));
  };

  PresolveContext context(local_model, shaving_proto, nullptr);
  std::vector<int> interval_mapping;
  ImportModelAndDomainsWithBasicPresolveIntoContext(
      model_proto_, var_domains_, active_constraints, &context,
      &interval_mapping);

  // Now copy the ignored constraints "partially".
  for (const int c : ignored_constraints) {
    CHECK(!active_constraints(c));
    const ConstraintProto& ct = model_proto_.constraints(c);

    // We will only include non-ignored intervals there !
    if (ct.constraint_case() == ConstraintProto::kNoOverlap2D) {
      NoOverlap2DConstraintProto* new_no_overlap_2d =
          shaving_proto->add_constraints()->mutable_no_overlap_2d();
      const int num_intervals = ct.no_overlap_2d().x_intervals().size();
      for (int i = 0; i < num_intervals; ++i) {
        const int x_interval = ct.no_overlap_2d().x_intervals(i);
        const int y_interval = ct.no_overlap_2d().y_intervals(i);
        if (!active_constraints(x_interval)) continue;
        if (!active_constraints(y_interval)) continue;

        new_no_overlap_2d->add_x_intervals(interval_mapping[x_interval]);
        new_no_overlap_2d->add_y_intervals(interval_mapping[y_interval]);
      }
    }
  }

  if (VLOG_IS_ON(2)) {
    int num_active_variables = 0;
    for (int i = 0; i < model_proto_.variables().size(); ++i) {
      if (cc_finder.Connected(root_index, var_to_node(i))) {
        ++num_active_variables;
      }
    }
    int num_active_constraints = 0;
    for (int c = 0; c < model_proto_.constraints().size(); ++c) {
      if (cc_finder.Connected(root_index, ct_to_node(c))) {
        ++num_active_constraints;
      }
    }

    LOG(INFO) << "#shaving_constraints:" << shaving_proto->constraints_size()
              << " #active_constraints:" << num_active_constraints << "/"
              << model_proto_.constraints_size()
              << " #active_variables:" << num_active_variables << "/"
              << model_proto_.variables_size();
  }

  shaving_proto->clear_objective();

  if (state->shave_using_objective) {
    if (state->minimize) {
      shaving_proto->mutable_objective()->add_vars(state->var_index);
      shaving_proto->mutable_objective()->add_coeffs(1);
    } else {
      shaving_proto->mutable_objective()->add_vars(state->var_index);
      shaving_proto->mutable_objective()->add_coeffs(-1);
    }
  } else {
    const Domain domain =
        ReadDomainFromProto(shaving_proto->variables(state->var_index));

    int64_t delta = 0;
    if (domain.Size() > local_params_.shaving_search_threshold()) {
      const int64_t mid_range = (domain.Max() - domain.Min()) / 2;
      auto* random = local_model->GetOrCreate<ModelRandomGenerator>();
      delta = absl::LogUniform<int64_t>(*random, 0, mid_range);
    }

    if (state->minimize) {
      state->reduced_domain =
          domain.IntersectionWith({domain.Min(), domain.Min() + delta});
    } else {
      state->reduced_domain =
          domain.IntersectionWith({domain.Max() - delta, domain.Max()});
    }
    FillDomainInProto(state->reduced_domain,
                      shaving_proto->mutable_variables(state->var_index));
  }
}

bool VariablesShavingSolver::ResetAndSolveModel(int64_t task_id, State* state,
                                                Model* local_model,
                                                CpModelProto* shaving_proto) {
  SatParameters* params = local_model->GetOrCreate<SatParameters>();
  *params = local_params_;
  params->set_random_seed(CombineSeed(local_params_.random_seed(), task_id));

  bool has_no_overlap_2d = false;
  {
    absl::MutexLock lock(mutex_);
    if (!FindNextVar(state)) return false;
    CopyModelConnectedToVar(state, local_model, shaving_proto,
                            &has_no_overlap_2d);
    ++num_vars_tried_;
  }

  // Use the current best solution as hint.
  {
    auto sol = shared_->response->SolutionPool().BestSolutions().GetSolution(0);
    if (sol != nullptr) {
      const std::vector<int64_t>& solution = sol->variable_values;
      auto* hint = shaving_proto->mutable_solution_hint();
      hint->clear_vars();
      hint->clear_values();
      for (int var = 0; var < solution.size(); ++var) {
        hint->add_vars(var);
        hint->add_values(solution[var]);
      }
    }
  }

  auto* time_limit = local_model->GetOrCreate<TimeLimit>();
  shared_->time_limit->UpdateLocalLimit(time_limit);
  time_limit->RegisterSecondaryExternalBooleanAsLimit(&stop_current_chunk_);
  time_limit->ChangeDeterministicLimit(
      time_limit->GetElapsedDeterministicTime() +
      local_params_.shaving_search_deterministic_time());

  shaving_proto->set_name(absl::StrCat("shaving_var_", state->var_index,
                                       (state->minimize ? "_min" : "_max")));

  // Presolve if asked.
  if (local_params_.cp_model_presolve()) {
    std::vector<int> postsolve_mapping;
    CpModelProto mapping_proto;
    auto context = std::make_unique<PresolveContext>(local_model, shaving_proto,
                                                     &mapping_proto);
    const CpSolverStatus presolve_status =
        PresolveCpModel(context.get(), &postsolve_mapping);
    if (presolve_status == CpSolverStatus::INFEASIBLE) {
      CpSolverResponse tmp_response;
      tmp_response.set_status(CpSolverStatus::INFEASIBLE);
      ProcessLocalResponse(tmp_response, *state);
      return false;
    }
  }

  // Hack: remove "non useful interval" from scheduling constraints.
  // For now we only do that for no-overlap 2d, but we should generalize.
  if (has_no_overlap_2d) {
    std::vector<int> postsolve_mapping;
    CpModelProto mapping_proto;
    auto context = std::make_unique<PresolveContext>(local_model, shaving_proto,
                                                     &mapping_proto);
    context->InitializeNewDomains();
    context->UpdateNewConstraintsVariableUsage();
    const int num_constraints = shaving_proto->constraints().size();
    std::vector<bool> useful_interval(num_constraints, false);
    std::vector<int> no_overalp_2d;
    for (int c = 0; c < num_constraints; ++c) {
      const ConstraintProto& ct = shaving_proto->constraints(c);
      if (ct.constraint_case() == ConstraintProto::kInterval) {
        for (const int var : UsedVariables(ct)) {
          if (context->VarToConstraints(var).size() > 1) {
            useful_interval[c] = true;
            break;
          }
        }
      } else if (ct.constraint_case() == ConstraintProto::kNoOverlap2D) {
        no_overalp_2d.push_back(c);
      }
    }
    for (const int c : no_overalp_2d) {
      NoOverlap2DConstraintProto* data =
          shaving_proto->mutable_constraints(c)->mutable_no_overlap_2d();
      int new_size = 0;
      const int num_intervals = data->x_intervals().size();
      for (int i = 0; i < num_intervals; ++i) {
        const int x = data->x_intervals(i);
        const int y = data->y_intervals(i);
        if (!useful_interval[x] && !useful_interval[y]) continue;
        data->set_x_intervals(new_size, x);
        data->set_y_intervals(new_size, y);
        ++new_size;
      }
      data->mutable_x_intervals()->Truncate(new_size);
      data->mutable_y_intervals()->Truncate(new_size);
    }
  }

  if (absl::GetFlag(FLAGS_cp_model_dump_submodels)) {
    const std::string shaving_name = absl::StrCat(
        absl::GetFlag(FLAGS_cp_model_dump_prefix), "shaving_var_",
        state->var_index, (state->minimize ? "_min" : "_max"), ".pb.txt");
    LOG(INFO) << "Dumping shaving model to '" << shaving_name << "'.";
    CHECK(WriteModelProtoToFile(*shaving_proto, shaving_name));
  }

  auto* local_response_manager =
      local_model->GetOrCreate<SharedResponseManager>();
  local_response_manager->InitializeObjective(*shaving_proto);
  local_response_manager->SetSynchronizationMode(true);

  // Tricky: If we aborted during the presolve above, some constraints might
  // be in a non-canonical form (like having duplicates, etc...) and it seem
  // not all our propagator code deal with that properly. So it is important
  // to abort right away here.
  //
  // We had a bug when the LoadCpModel() below was returning infeasible on
  // such non fully-presolved model.
  if (time_limit->LimitReached()) return false;

  LoadCpModel(*shaving_proto, local_model);
  QuickSolveWithHint(*shaving_proto, local_model);
  SolveLoadedCpModel(*shaving_proto, local_model);
  return true;
}

}  // namespace sat
}  // namespace operations_research
