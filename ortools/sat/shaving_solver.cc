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

#include <algorithm>
#include <cstddef>
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
#include "google/protobuf/arena.h"
#include "ortools/base/macros/buildenv.h"
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
      shared_(shared) {
  ResetModel();
}

ObjectiveShavingSolver::~ObjectiveShavingSolver() {
  shared_->stat_tables->AddTimingStat(*this);
}

bool ObjectiveShavingSolver::TaskIsAvailable() {
  if (shared_->SearchIsDone()) return false;

  // We only support one task at a time.
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
          local_model_->GetOrCreate<SharedResponseManager>()->GetResponse();

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
    if (local_model_ != nullptr) {
      const double dtime =
          local_model_->GetOrCreate<TimeLimit>()->GetElapsedDeterministicTime();
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

  // TODO(user): Also stop if we have enough newly fixed / improved root-level
  // bounds so that we think it is worth represolving and restarting.
  if (shared_->SearchIsDone()) {
    stop_current_chunk_.store(true);
  }

  // The current objective lower bound has been improved, restarting.
  //
  // TODO(user): If we are exploring a range with current_objective_target_ub_
  // still greater, it probably does not make sense to restart.
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
  return absl::StrCat(name(), " (vars=", local_proto_->variables().size(),
                      " csts=", local_proto_->constraints().size(), ")");
}

void ObjectiveShavingSolver::ResetModel() {
  if (shared_->model_proto.GetArena() != nullptr) {
    arena_ = std::make_unique<google::protobuf::Arena>(
        google::protobuf::ArenaOptions(
            {.start_block_size = static_cast<size_t>(
                 shared_->model_proto.GetArena()->SpaceUsed())}));
  } else {
    arena_ = std::make_unique<google::protobuf::Arena>();
  }
  local_proto_ = google::protobuf::Arena::Create<CpModelProto>(arena_.get());
  *local_proto_ = shared_->model_proto;
}

bool ObjectiveShavingSolver::ResetAndSolveModel(int64_t task_id) {
  local_model_ = std::make_unique<Model>(name());
  *local_model_->GetOrCreate<SatParameters>() = local_params_;
  local_model_->GetOrCreate<SatParameters>()->set_random_seed(
      CombineSeed(local_params_.random_seed(), task_id));

  auto* time_limit = local_model_->GetOrCreate<TimeLimit>();
  shared_->time_limit->UpdateLocalLimit(time_limit);
  // Don't let our shaving model stop the main solve.
  local_model_->GetOrCreate<ModelSharedTimeLimit>()->DisableStop();
  time_limit->RegisterSecondaryExternalBooleanAsLimit(&stop_current_chunk_);

  auto* random = local_model_->GetOrCreate<ModelRandomGenerator>();

  // We copy the model.
  ResetModel();
  *local_proto_->mutable_variables() =
      helper_->FullNeighborhood().delta.variables();

  // Store the current lb in a local variable.
  IntegerValue objective_lb;
  IntegerValue chosen_objective_ub;
  {
    absl::MutexLock mutex_lock(mutex_);
    objective_lb = objective_lb_;
    if (objective_ub_ - objective_lb <=
        local_params_.objective_shaving_search_threshold()) {
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
  // Not having the objective opens up more presolve reductions.
  Domain obj_domain = Domain(objective_lb.value(), chosen_objective_ub.value());
  if (local_proto_->objective().domain_size() > 1) {
    // Intersect with the first interval of the objective domain.
    obj_domain = obj_domain.IntersectionWith(
        Domain(local_proto_->objective().domain(0),
               local_proto_->objective().domain(1)));
  }
  if (local_proto_->objective().vars().size() == 1 &&
      local_proto_->objective().coeffs(0) == 1) {
    auto* obj_var =
        local_proto_->mutable_variables(local_proto_->objective().vars(0));
    const Domain reduced_var_domain = obj_domain.IntersectionWith(
        Domain(obj_var->domain(0), obj_var->domain(1)));
    if (reduced_var_domain.IsEmpty()) {
      return false;
    }
    FillDomainInProto(reduced_var_domain, obj_var);
  } else {
    auto* obj = local_proto_->add_constraints()->mutable_linear();
    *obj->mutable_vars() = local_proto_->objective().vars();
    *obj->mutable_coeffs() = local_proto_->objective().coeffs();
    if (obj_domain.IsEmpty()) {
      return false;
    }
    FillDomainInProto(obj_domain, obj);
  }

  // Clear the objective.
  local_proto_->clear_objective();
  local_proto_->set_name(absl::StrCat(local_proto_->name(), "_obj_shaving_",
                                      chosen_objective_ub.value()));

  // Dump?
  if (absl::GetFlag(FLAGS_cp_model_dump_submodels)) {
    const std::string name = absl::StrCat(
        absl::GetFlag(FLAGS_cp_model_dump_prefix), "objective_shaving_",
        chosen_objective_ub.value(), ".pb.txt");
    LOG(INFO) << "Dumping objective shaving model to '" << name << "'.";
    CHECK(WriteModelProtoToFile(*local_proto_, name));
  }

  // Presolve if asked.
  if (local_params_.cp_model_presolve()) {
    mapping_proto_.Clear();
    postsolve_mapping_.clear();
    auto context = std::make_unique<PresolveContext>(
        local_model_.get(), local_proto_, &mapping_proto_);
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
  // be in a non-canonical form (like having duplicates, etc.) and it seems
  // not all our propagator code deals with that properly. So it is important
  // to abort right away here.
  //
  // We had a bug when the LoadCpModel() below was returning infeasible on
  // such a non-fully-presolved model.
  if (time_limit->LimitReached()) return false;

  LoadCpModel(*local_proto_, local_model_.get());
  SolveLoadedCpModel(*local_proto_, local_model_.get());
  return true;
}

VariablesShavingSolver::ShavingGraph::ShavingGraph(
    const CpModelProto& model_proto)
    : model_proto_(model_proto) {
  const int num_variables = model_proto_.variables_size();
  const int num_constraints = model_proto_.constraints_size();

  CompactVectorVectorBuilder<int, int> var_to_constraints_builder;
  CompactVectorVectorBuilder<int, int> constraint_to_variables_builder;
  CompactVectorVectorBuilder<int, int> interval_to_constraints_builder;
  CompactVectorVectorBuilder<int, int> constraint_to_intervals_builder;
  CompactVectorVectorBuilder<int, int> interval_to_no_overlap_2d_pairs_builder;
  CompactVectorVectorBuilder<int, int> no_overlap_2d_pair_to_variables_builder;
  CompactVectorVectorBuilder<int, int>
      constraint_to_no_overlap_2d_pairs_builder;

  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = model_proto_.constraints(c);
    if (ct.constraint_case() == ConstraintProto::kNoOverlap2D) {
      const NoOverlap2DConstraintProto& no_overlap_2d = ct.no_overlap_2d();
      DCHECK_EQ(no_overlap_2d.x_intervals_size(),
                no_overlap_2d.y_intervals_size());
      for (int i = 0; i < no_overlap_2d.x_intervals_size(); ++i) {
        const int x_interval = no_overlap_2d.x_intervals(i);
        const int y_interval = no_overlap_2d.y_intervals(i);
        const int pair_id = no_overlap_2d_pairs_.size();
        no_overlap_2d_pairs_.push_back({c, x_interval, y_interval});

        // Count only this rectangle's variables and the shared enforcement
        // literals, not those of every rectangle in the constraint.
        std::vector<int> pair_variables;
        for (const int interval : {x_interval, y_interval}) {
          const std::vector<int> interval_variables =
              UsedVariables(model_proto_.constraints(interval));
          pair_variables.insert(pair_variables.end(),
                                interval_variables.begin(),
                                interval_variables.end());
          interval_to_no_overlap_2d_pairs_builder.Add(interval, pair_id);
        }
        for (const int ref : ct.enforcement_literal()) {
          pair_variables.push_back(PositiveRef(ref));
        }
        std::sort(pair_variables.begin(), pair_variables.end());
        pair_variables.erase(
            std::unique(pair_variables.begin(), pair_variables.end()),
            pair_variables.end());
        for (const int var : pair_variables) {
          no_overlap_2d_pair_to_variables_builder.Add(pair_id, var);
        }
        constraint_to_no_overlap_2d_pairs_builder.Add(c, pair_id);
      }
      continue;
    }

    for (const int var : UsedVariables(ct)) {
      constraint_to_variables_builder.Add(c, var);
      var_to_constraints_builder.Add(var, c);
    }

    for (const int interval : UsedIntervals(ct)) {
      constraint_to_intervals_builder.Add(c, interval);
      interval_to_constraints_builder.Add(interval, c);
    }
  }

  var_to_constraints_.ResetFromBuilder(std::move(var_to_constraints_builder),
                                       num_variables);
  constraint_to_variables_.ResetFromBuilder(
      std::move(constraint_to_variables_builder), num_constraints);
  interval_to_constraints_.ResetFromBuilder(
      std::move(interval_to_constraints_builder), num_constraints);
  constraint_to_intervals_.ResetFromBuilder(
      std::move(constraint_to_intervals_builder), num_constraints);
  interval_to_no_overlap_2d_pairs_.ResetFromBuilder(
      std::move(interval_to_no_overlap_2d_pairs_builder), num_constraints);
  no_overlap_2d_pair_to_variables_.ResetFromBuilder(
      std::move(no_overlap_2d_pair_to_variables_builder),
      no_overlap_2d_pairs_.size());
  constraint_to_no_overlap_2d_pairs_.ResetFromBuilder(
      std::move(constraint_to_no_overlap_2d_pairs_builder), num_constraints);

  selected_variables_epoch_.resize(num_variables);
  queued_variables_epoch_.resize(num_variables);
  queued_intervals_epoch_.resize(num_constraints);
  active_constraints_epoch_.resize(num_constraints);
  active_no_overlap_2d_constraints_epoch_.resize(num_constraints);
  active_no_overlap_2d_pairs_epoch_.resize(no_overlap_2d_pairs_.size());
}

bool VariablesShavingSolver::ShavingGraph::ConstraintIsInactive(
    int c, const std::vector<Domain>& domains) const {
  for (const int ref : model_proto_.constraints(c).enforcement_literal()) {
    const int var = PositiveRef(ref);
    if (domains[var].IsFixed() &&
        domains[var].FixedValue() == (var == ref ? 0 : 1)) {
      return true;
    }
  }
  return false;
}

bool VariablesShavingSolver::ShavingGraph::IsConstraintActive(
    int c, const Neighborhood& neighborhood) const {
  return active_constraints_epoch_[c] == neighborhood.epoch;
}

bool VariablesShavingSolver::ShavingGraph::IsNoOverlap2DConstraintActive(
    int c, const Neighborhood& neighborhood) const {
  return active_no_overlap_2d_constraints_epoch_[c] == neighborhood.epoch;
}

bool VariablesShavingSolver::ShavingGraph::IsNoOverlap2DPairActive(
    int pair_id, const Neighborhood& neighborhood) const {
  return active_no_overlap_2d_pairs_epoch_[pair_id] == neighborhood.epoch;
}

VariablesShavingSolver::ShavingGraph::Neighborhood
VariablesShavingSolver::ShavingGraph::Extract(
    int root_var, const std::vector<Domain>& domains, int max_variables,
    int max_constraints) {
  DCHECK_EQ(domains.size(), model_proto_.variables_size());
  DCHECK_GE(root_var, 0);
  DCHECK_LT(root_var, model_proto_.variables_size());
  DCHECK(!domains[root_var].IsFixed());

  if (++current_epoch_ == 0) {
    std::fill(selected_variables_epoch_.begin(),
              selected_variables_epoch_.end(), 0);
    std::fill(queued_variables_epoch_.begin(), queued_variables_epoch_.end(),
              0);
    std::fill(queued_intervals_epoch_.begin(), queued_intervals_epoch_.end(),
              0);
    std::fill(active_constraints_epoch_.begin(),
              active_constraints_epoch_.end(), 0);
    std::fill(active_no_overlap_2d_constraints_epoch_.begin(),
              active_no_overlap_2d_constraints_epoch_.end(), 0);
    std::fill(active_no_overlap_2d_pairs_epoch_.begin(),
              active_no_overlap_2d_pairs_epoch_.end(), 0);
    current_epoch_ = 1;
  }
  active_no_overlap_2d_constraints_.clear();

  Neighborhood result;
  result.epoch = current_epoch_;
  std::vector<std::pair<bool, int>> pending_nodes;

  const auto add_variable = [&](int var, bool expand) {
    if (domains[var].IsFixed()) return;
    if (selected_variables_epoch_[var] != result.epoch) {
      selected_variables_epoch_[var] = result.epoch;
      ++result.num_active_variables;
    }
    if (expand && queued_variables_epoch_[var] != result.epoch) {
      queued_variables_epoch_[var] = result.epoch;
      pending_nodes.push_back({false, var});
    }
  };

  const auto add_interval = [&](int interval) {
    DCHECK_GE(interval, 0);
    DCHECK_LT(interval, model_proto_.constraints_size());
    if (queued_intervals_epoch_[interval] == result.epoch) return;
    queued_intervals_epoch_[interval] = result.epoch;
    pending_nodes.push_back({true, interval});
  };

  const auto fits_budget = [&](int new_variables, int new_constraints) {
    return (max_variables == 0 ||
            result.num_active_variables + new_variables <= max_variables) &&
           (max_constraints == 0 ||
            result.num_active_constraints + new_constraints <= max_constraints);
  };

  const auto add_constraint = [&](int c) {
    DCHECK_NE(model_proto_.constraints(c).constraint_case(),
              ConstraintProto::kNoOverlap2D);
    if (IsConstraintActive(c, result) || ConstraintIsInactive(c, domains)) {
      return;
    }

    int new_variables = 0;
    for (const int var : constraint_to_variables_[c]) {
      if (!domains[var].IsFixed() &&
          selected_variables_epoch_[var] != result.epoch) {
        ++new_variables;
      }
    }
    if (!fits_budget(new_variables, 1)) return;

    active_constraints_epoch_[c] = result.epoch;
    ++result.num_active_constraints;
    for (const int var : constraint_to_variables_[c]) {
      add_variable(var, /*expand=*/true);
    }
    for (const int interval : constraint_to_intervals_[c]) {
      add_interval(interval);
    }
    if (model_proto_.constraints(c).constraint_case() ==
        ConstraintProto::kInterval) {
      add_interval(c);
    }
  };

  const auto add_no_overlap_2d_pair = [&](int pair_id) {
    if (IsNoOverlap2DPairActive(pair_id, result)) return;
    const NoOverlap2DPair& pair = no_overlap_2d_pairs_[pair_id];
    if (ConstraintIsInactive(pair.constraint, domains)) return;

    const bool new_constraint =
        !IsNoOverlap2DConstraintActive(pair.constraint, result);
    int new_variables = 0;
    for (const int var : no_overlap_2d_pair_to_variables_[pair_id]) {
      if (!domains[var].IsFixed() &&
          selected_variables_epoch_[var] != result.epoch) {
        ++new_variables;
      }
    }
    if (!fits_budget(new_variables, new_constraint ? 1 : 0)) return;

    active_no_overlap_2d_pairs_epoch_[pair_id] = result.epoch;
    if (new_constraint) {
      active_no_overlap_2d_constraints_epoch_[pair.constraint] = result.epoch;
      active_no_overlap_2d_constraints_.push_back(pair.constraint);
      ++result.num_active_constraints;
    }

    // Enforcement literals are copied with the partial constraint, but are
    // boundary variables: expanding them would connect unrelated rectangles.
    for (const int var : no_overlap_2d_pair_to_variables_[pair_id]) {
      add_variable(var, /*expand=*/false);
    }
    add_interval(pair.x_interval);
    add_interval(pair.y_interval);
  };

  add_variable(root_var, /*expand=*/true);
  for (int next_node = 0; next_node < pending_nodes.size(); ++next_node) {
    const auto [is_interval, index] = pending_nodes[next_node];

    if (!is_interval) {
      for (const int c : var_to_constraints_[index]) {
        add_constraint(c);
      }
      continue;
    }

    // An interval is represented by its defining constraint node.
    add_constraint(index);
    for (const int c : interval_to_constraints_[index]) {
      add_constraint(c);
    }
    for (const int pair_id : interval_to_no_overlap_2d_pairs_[index]) {
      add_no_overlap_2d_pair(pair_id);
    }
  }

  return result;
}

VariablesShavingSolver::VariablesShavingSolver(
    const SatParameters& local_parameters, SharedClasses* shared)
    : SubSolver(local_parameters.name(), INCOMPLETE),
      local_params_(local_parameters),
      shared_(shared),
      stop_current_chunk_(false),
      model_proto_(shared->model_proto),
      graph_(model_proto_) {
  if (shared_->bounds != nullptr) {
    shared_bounds_id_ = shared_->bounds->RegisterNewId(local_parameters.name());
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

bool VariablesShavingSolver::FindNextVar(State* state)
    ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
  const int num_vars = var_domains_.size();
  ++current_index_;  // Starts at -1.

  // We start by shaving the objective in order to increase the lower bound.
  // We abort for models with a large number of objective variables. We also
  // ignore when the objective is a single variable.
  int num_objective_vars_to_scan = 0;
  if (model_proto_.has_objective()) {
    const int num_obj_vars = model_proto_.objective().vars().size();
    if (num_obj_vars > 1 && num_obj_vars * 10 < num_vars) {
      num_objective_vars_to_scan = num_obj_vars;
    }
  }

  while (current_index_ < num_objective_vars_to_scan) {
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

  // Otherwise loop over all variables. The method returns false when all
  // variables are fixed.
  // TODO(user): maybe we should just order all possible States, putting the
  // objective first, and just loop.
  const int starting_index = current_index_;
  while (current_index_ - starting_index < num_vars * 2) {
    const int normalized_index = current_index_ - num_objective_vars_to_scan;
    const int loop_index = normalized_index / (2 * num_vars);
    const int var_index = (normalized_index / 2) % num_vars;
    const bool minimize = normalized_index % 2 == 0;

    // Skip fixed variables.
    if (VarIsFixed(var_index)) {
      ++current_index_;
      // Skip maximization too.
      if (minimize) ++current_index_;
      continue;
    }

    // Let's not shave the single var objective. There are enough workers
    // looking at it.
    if (model_proto_.has_objective() &&
        model_proto_.objective().vars_size() == 1 &&
        var_index == model_proto_.objective().vars(0)) {
      current_index_ += 2;  // Skip minimize and maximize.
      continue;
    }

    state->var_index = var_index;
    state->minimize = minimize;
    // Starting from the second loop, we use the objective to do the shaving.
    //
    // As we will test both minimization and maximization, we only consider
    // shaving using the objective when the domain has more than 2 values.
    //
    // TODO(user): Explore other policies like alternating.
    state->shave_using_objective = loop_index > 0 && DomainSize(var_index) > 2;
    return true;
  }

  return false;
}

bool VariablesShavingSolver::CopyModelNeighborhood(State* state,
                                                   Model* local_model,
                                                   CpModelProto* shaving_proto,
                                                   bool* has_no_overlap_2d)
    ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
  DCHECK(shaving_proto->variables().empty());
  DCHECK(shaving_proto->constraints().empty());

  const ShavingGraph::Neighborhood neighborhood = graph_.Extract(
      state->var_index, var_domains_,
      local_params_.variables_shaving_max_neighborhood_variables(),
      local_params_.variables_shaving_max_neighborhood_constraints());
  const auto active_constraints = [this, &neighborhood](int c) {
    return graph_.IsConstraintActive(c, neighborhood);
  };

  std::vector<int> interval_mapping;
  if (!CopyModelAdvanced(model_proto_, /*clear_internal_fields=*/false,
                         var_domains_, active_constraints, &interval_mapping,
                         shaving_proto, local_model)) {
    return false;
  }

  // NoOverlap2D is copied separately to keep only complete rectangle pairs.
  for (const int c : graph_.active_no_overlap_2d_constraints()) {
    const ConstraintProto& ct = model_proto_.constraints(c);
    const auto pair_ids = graph_.constraint_to_no_overlap_2d_pairs()[c];
    bool has_mapped_pair = false;
    for (const int pair_id : pair_ids) {
      if (!graph_.IsNoOverlap2DPairActive(pair_id, neighborhood)) continue;
      const ShavingGraph::NoOverlap2DPair& pair =
          graph_.no_overlap_2d_pairs()[pair_id];
      if (interval_mapping[pair.x_interval] != -1 &&
          interval_mapping[pair.y_interval] != -1) {
        has_mapped_pair = true;
        break;
      }
    }
    if (!has_mapped_pair) continue;

    ConstraintProto* new_ct = shaving_proto->add_constraints();
    new_ct->mutable_enforcement_literal()->CopyFrom(ct.enforcement_literal());
    NoOverlap2DConstraintProto* new_no_overlap_2d =
        new_ct->mutable_no_overlap_2d();
    for (const int pair_id : pair_ids) {
      if (!graph_.IsNoOverlap2DPairActive(pair_id, neighborhood)) continue;
      const ShavingGraph::NoOverlap2DPair& pair =
          graph_.no_overlap_2d_pairs()[pair_id];
      if (interval_mapping[pair.x_interval] == -1 ||
          interval_mapping[pair.y_interval] == -1) {
        continue;
      }
      new_no_overlap_2d->add_x_intervals(interval_mapping[pair.x_interval]);
      new_no_overlap_2d->add_y_intervals(interval_mapping[pair.y_interval]);
    }
    *has_no_overlap_2d = true;
  }

  if (VLOG_IS_ON(2)) {
    LOG(INFO) << "#shaving_constraints:" << shaving_proto->constraints_size()
              << " #active_constraints:" << neighborhood.num_active_constraints
              << "/" << model_proto_.constraints_size()
              << " #active_variables:" << neighborhood.num_active_variables
              << "/" << model_proto_.variables_size();
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
    const Domain& domain = var_domains_[state->var_index];
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

  return true;
}

bool VariablesShavingSolver::ResetAndSolveModel(int64_t task_id, State* state,
                                                Model* local_model,
                                                CpModelProto* shaving_proto) {
  SatParameters* params = local_model->GetOrCreate<SatParameters>();
  *params = local_params_;
  params->set_random_seed(CombineSeed(local_params_.random_seed(), task_id));

  auto report_unsat = [this, &state]() {
    CpSolverResponse tmp_response;
    tmp_response.set_status(CpSolverStatus::INFEASIBLE);
    ProcessLocalResponse(tmp_response, *state);
    return false;
  };

  bool has_no_overlap_2d = false;
  bool unsat_during_copy = false;
  {
    absl::MutexLock lock(mutex_);
    if (!FindNextVar(state)) return false;
    if (!CopyModelNeighborhood(state, local_model, shaving_proto,
                               &has_no_overlap_2d)) {
      unsat_during_copy = true;  // Report unsat after releasing lock.
    }
    ++num_vars_tried_;
  }
  if (unsat_during_copy) return report_unsat();

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
  local_model->GetOrCreate<ModelSharedTimeLimit>()->DisableStop();
  time_limit->RegisterSecondaryExternalBooleanAsLimit(&stop_current_chunk_);
  time_limit->ChangeDeterministicLimit(
      time_limit->GetElapsedDeterministicTime() +
      local_params_.shaving_search_deterministic_time());

  shaving_proto->set_name(absl::StrCat("shaving_var_", state->var_index,
                                       (state->minimize ? "_min" : "_max")));

  if (absl::GetFlag(FLAGS_cp_model_dump_submodels)) {
    const std::string shaving_name = absl::StrCat(
        absl::GetFlag(FLAGS_cp_model_dump_prefix), "shaving_var_",
        state->var_index, (state->minimize ? "_min" : "_max"), ".pb.txt");
    LOG(INFO) << "Dumping shaving model to '" << shaving_name << "'.";
    CHECK(WriteModelProtoToFile(*shaving_proto, shaving_name));
  }

  // Presolve if asked.
  if (local_params_.cp_model_presolve()) {
    std::vector<int> postsolve_mapping;
    CpModelProto mapping_proto;
    auto context = std::make_unique<PresolveContext>(local_model, shaving_proto,
                                                     &mapping_proto);
    const CpSolverStatus presolve_status =
        PresolveCpModel(context.get(), &postsolve_mapping);
    if (presolve_status == CpSolverStatus::INFEASIBLE) {
      return report_unsat();
    }
  }

  // Hack: remove "non-useful intervals" from scheduling constraints.
  // For now we only do that for no-overlap 2d, but we should generalize.
  if (has_no_overlap_2d) {
    std::vector<int> postsolve_mapping;
    CpModelProto mapping_proto;
    auto context = std::make_unique<PresolveContext>(local_model, shaving_proto,
                                                     &mapping_proto);
    context->InitializeNewDomains();
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

  auto* local_response_manager =
      local_model->GetOrCreate<SharedResponseManager>();
  local_response_manager->InitializeObjective(*shaving_proto);
  local_response_manager->SetSynchronizationMode(true);

  // Tricky: If we aborted during the presolve above, some constraints might
  // be in a non-canonical form (like having duplicates, etc.) and it seems
  // not all our propagator code deals with that properly. So it is important
  // to abort right away here.
  //
  // We had a bug when the LoadCpModel() below was returning infeasible on
  // such a non-fully-presolved model.
  if (time_limit->LimitReached()) return false;

  LoadCpModel(*shaving_proto, local_model);
  QuickSolveWithHint(*shaving_proto, local_model);
  SolveLoadedCpModel(*shaving_proto, local_model);
  return true;
}

}  // namespace sat
}  // namespace operations_research
