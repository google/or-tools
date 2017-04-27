// Copyright 2010-2014 Google
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

#include "ortools/bop/bop_portfolio.h"

#include "ortools/base/stringprintf.h"
#include "ortools/base/stl_util.h"
#include "ortools/bop/bop_fs.h"
#include "ortools/bop/bop_lns.h"
#include "ortools/bop/bop_ls.h"
#include "ortools/bop/bop_util.h"
#include "ortools/bop/complete_optimizer.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/symmetry.h"

namespace operations_research {
namespace bop {
namespace {
void BuildObjectiveTerms(const LinearBooleanProblem& problem,
                         BopConstraintTerms* objective_terms) {
  CHECK(objective_terms != nullptr);

  if (!objective_terms->empty()) return;

  const LinearObjective& objective = problem.objective();
  const size_t num_objective_terms = objective.literals_size();
  CHECK_EQ(num_objective_terms, objective.coefficients_size());
  for (int i = 0; i < num_objective_terms; ++i) {
    CHECK_GT(objective.literals(i), 0);
    CHECK_NE(objective.coefficients(i), 0);

    const VariableIndex var_id(objective.literals(i) - 1);
    const int64 weight = objective.coefficients(i);
    objective_terms->push_back(BopConstraintTerm(var_id, weight));
  }
}
}  // anonymous namespace

//------------------------------------------------------------------------------
// PortfolioOptimizer
//------------------------------------------------------------------------------
PortfolioOptimizer::PortfolioOptimizer(
    const ProblemState& problem_state, const BopParameters& parameters,
    const BopSolverOptimizerSet& optimizer_set, const std::string& name)
    : BopOptimizerBase(name),
      random_(),
      state_update_stamp_(ProblemState::kInitialStampValue),
      objective_terms_(),
      selector_(),
      optimizers_(),
      sat_propagator_(),
      parameters_(parameters),
      lower_bound_(-glop::kInfinity),
      upper_bound_(glop::kInfinity),
      number_of_consecutive_failing_optimizers_(0) {
  CreateOptimizers(problem_state.original_problem(), parameters, optimizer_set);
}

PortfolioOptimizer::~PortfolioOptimizer() {
  if (parameters_.log_search_progress() || VLOG_IS_ON(1)) {
    std::string stats_string;
    for (OptimizerIndex i(0); i < optimizers_.size(); ++i) {
      if (selector_->NumCallsForOptimizer(i) > 0) {
        stats_string += selector_->PrintStats(i);
      }
    }
    if (!stats_string.empty()) {
      LOG(INFO) << "Stats. #new_solutions/#calls by optimizer:\n" +
                       stats_string;
    }
  }

  // Note that unique pointers are not used due to unsupported emplace_back
  // in ITIVectors.
  STLDeleteElements(&optimizers_);
}

BopOptimizerBase::Status PortfolioOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // Load any new information into the sat_propagator_.
  const bool first_time = (sat_propagator_.NumVariables() == 0);
  const BopOptimizerBase::Status status =
      LoadStateProblemToSatSolver(problem_state, &sat_propagator_);
  if (status != BopOptimizerBase::CONTINUE) return status;
  if (first_time) {
    // We configure the sat_propagator_ to use the objective as an assignment
    // preference
    UseObjectiveForSatAssignmentPreference(problem_state.original_problem(),
                                           &sat_propagator_);
  }

  lower_bound_ = problem_state.GetScaledLowerBound();
  upper_bound_ = problem_state.solution().IsFeasible()
                     ? problem_state.solution().GetScaledCost()
                     : glop::kInfinity;
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status PortfolioOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  for (OptimizerIndex i(0); i < optimizers_.size(); ++i) {
    selector_->SetOptimizerRunnability(
        i, optimizers_[i]->ShouldBeRun(problem_state));
  }

  const int64 init_cost = problem_state.solution().IsFeasible()
                              ? problem_state.solution().GetCost()
                              : kint64max;
  const double init_deterministic_time =
      time_limit->GetElapsedDeterministicTime();

  const OptimizerIndex selected_optimizer_id = selector_->SelectOptimizer();
  if (selected_optimizer_id == kInvalidOptimizerIndex) {
    LOG(INFO) << "All the optimizers are done.";
    return BopOptimizerBase::ABORT;
  }
  BopOptimizerBase* const selected_optimizer =
      optimizers_[selected_optimizer_id];
  if (parameters.log_search_progress() || VLOG_IS_ON(1)) {
    LOG(INFO) << "      " << lower_bound_ << " .. " << upper_bound_ << " "
              << name() << " - " << selected_optimizer->name()
              << ". Time limit: " << time_limit->GetTimeLeft() << " -- "
              << time_limit->GetDeterministicTimeLeft();
  }
  const BopOptimizerBase::Status optimization_status =
      selected_optimizer->Optimize(parameters, problem_state, learned_info,
                                   time_limit);

  // ABORT means that this optimizer can't be run until we found a new solution.
  if (optimization_status == BopOptimizerBase::ABORT) {
    selector_->TemporarilyMarkOptimizerAsUnselectable(selected_optimizer_id);
  }

  // The gain is defined as 1 for the first solution.
  // TODO(user): Is 1 the right value? It might be better to use a percentage
  //              of the gap, or use the same gain as for the second solution.
  const int64 gain = optimization_status == BopOptimizerBase::SOLUTION_FOUND
                         ? (init_cost == kint64max
                                ? 1
                                : init_cost - learned_info->solution.GetCost())
                         : 0;
  const double spent_deterministic_time =
      time_limit->GetElapsedDeterministicTime() - init_deterministic_time;
  selector_->UpdateScore(gain, spent_deterministic_time);

  if (optimization_status == BopOptimizerBase::INFEASIBLE ||
      optimization_status == BopOptimizerBase::OPTIMAL_SOLUTION_FOUND) {
    return optimization_status;
  }

  // Stop the portfolio optimizer after too many unsuccessful calls.
  if (parameters.has_max_number_of_consecutive_failing_optimizer_calls() &&
      problem_state.solution().IsFeasible()) {
    number_of_consecutive_failing_optimizers_ =
        optimization_status == BopOptimizerBase::SOLUTION_FOUND
            ? 0
            : number_of_consecutive_failing_optimizers_ + 1;
    if (number_of_consecutive_failing_optimizers_ >
        parameters.max_number_of_consecutive_failing_optimizer_calls()) {
      return BopOptimizerBase::ABORT;
    }
  }

  // TODO(user): don't penalize the SatCoreBasedOptimizer or the
  // LinearRelaxation when they improve the lower bound.
  // TODO(user): Do we want to re-order the optimizers in the selector when
  //              the status is BopOptimizerBase::INFORMATION_FOUND?
  return BopOptimizerBase::CONTINUE;
}

void PortfolioOptimizer::AddOptimizer(
    const LinearBooleanProblem& problem, const BopParameters& parameters,
    const BopOptimizerMethod& optimizer_method) {
  switch (optimizer_method.type()) {
    case BopOptimizerMethod::SAT_CORE_BASED:
      optimizers_.push_back(new SatCoreBasedOptimizer("SatCoreBasedOptimizer"));
      break;
    case BopOptimizerMethod::SAT_LINEAR_SEARCH:
      optimizers_.push_back(new GuidedSatFirstSolutionGenerator(
          "SatOptimizer", GuidedSatFirstSolutionGenerator::Policy::kNotGuided));
      break;
    case BopOptimizerMethod::LINEAR_RELAXATION:
      optimizers_.push_back(
          new LinearRelaxation(parameters, "LinearRelaxation"));
      break;
    case BopOptimizerMethod::LOCAL_SEARCH: {
      for (int i = 1; i <= parameters.max_num_decisions_in_ls(); ++i) {
        optimizers_.push_back(new LocalSearchOptimizer(StringPrintf("LS_%d", i),
                                                       i, &sat_propagator_));
      }
    } break;
    case BopOptimizerMethod::RANDOM_FIRST_SOLUTION:
      optimizers_.push_back(new BopRandomFirstSolutionGenerator(
          "SATRandomFirstSolution", parameters, &sat_propagator_,
          random_.get()));
      break;
    case BopOptimizerMethod::RANDOM_VARIABLE_LNS:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.push_back(new BopAdaptiveLNSOptimizer(
          "RandomVariableLns",
          /*use_lp_to_guide_sat=*/false,
          new ObjectiveBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RANDOM_VARIABLE_LNS_GUIDED_BY_LP:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.push_back(new BopAdaptiveLNSOptimizer(
          "RandomVariableLnsWithLp",
          /*use_lp_to_guide_sat=*/true,
          new ObjectiveBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RANDOM_CONSTRAINT_LNS:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.push_back(new BopAdaptiveLNSOptimizer(
          "RandomConstraintLns",
          /*use_lp_to_guide_sat=*/false,
          new ConstraintBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RANDOM_CONSTRAINT_LNS_GUIDED_BY_LP:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.push_back(new BopAdaptiveLNSOptimizer(
          "RandomConstraintLnsWithLp",
          /*use_lp_to_guide_sat=*/true,
          new ConstraintBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RELATION_GRAPH_LNS:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.push_back(new BopAdaptiveLNSOptimizer(
          "RelationGraphLns",
          /*use_lp_to_guide_sat=*/false,
          new RelationGraphBasedNeighborhood(problem, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RELATION_GRAPH_LNS_GUIDED_BY_LP:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.push_back(new BopAdaptiveLNSOptimizer(
          "RelationGraphLnsWithLp",
          /*use_lp_to_guide_sat=*/true,
          new RelationGraphBasedNeighborhood(problem, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::COMPLETE_LNS:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.push_back(
          new BopCompleteLNSOptimizer("LNS", objective_terms_));
      break;
    case BopOptimizerMethod::USER_GUIDED_FIRST_SOLUTION:
      optimizers_.push_back(new GuidedSatFirstSolutionGenerator(
          "SATUserGuidedFirstSolution",
          GuidedSatFirstSolutionGenerator::Policy::kUserGuided));
      break;
    case BopOptimizerMethod::LP_FIRST_SOLUTION:
      optimizers_.push_back(new GuidedSatFirstSolutionGenerator(
          "SATLPFirstSolution",
          GuidedSatFirstSolutionGenerator::Policy::kLpGuided));
      break;
    case BopOptimizerMethod::OBJECTIVE_FIRST_SOLUTION:
      optimizers_.push_back(new GuidedSatFirstSolutionGenerator(
          "SATObjectiveFirstSolution",
          GuidedSatFirstSolutionGenerator::Policy::kObjectiveGuided));
      break;
    default:
      LOG(FATAL) << "Unknown optimizer type.";
  }
}

void PortfolioOptimizer::CreateOptimizers(
    const LinearBooleanProblem& problem, const BopParameters& parameters,
    const BopSolverOptimizerSet& optimizer_set) {
  random_.reset(new MTRandom(parameters.random_seed()));

  if (parameters.use_symmetry()) {
    VLOG(1) << "Finding symmetries of the problem.";
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    sat::FindLinearBooleanProblemSymmetries(problem, &generators);
    std::unique_ptr<sat::SymmetryPropagator> propagator(
        new sat::SymmetryPropagator);
    for (int i = 0; i < generators.size(); ++i) {
      propagator->AddSymmetry(std::move(generators[i]));
    }
    sat_propagator_.AddPropagator(std::move(propagator));
  }

  const int max_num_optimizers =
      optimizer_set.methods_size() + parameters.max_num_decisions_in_ls() - 1;
  optimizers_.reserve(max_num_optimizers);
  for (const BopOptimizerMethod& optimizer_method : optimizer_set.methods()) {
    const OptimizerIndex old_size(optimizers_.size());
    AddOptimizer(problem, parameters, optimizer_method);
  }

  selector_.reset(new OptimizerSelector(optimizers_));
}

//------------------------------------------------------------------------------
// OptimizerSelector
//------------------------------------------------------------------------------
OptimizerSelector::OptimizerSelector(
    const ITIVector<OptimizerIndex, BopOptimizerBase*>& optimizers)
    : run_infos_(), selected_index_(optimizers.size()) {
  for (OptimizerIndex i(0); i < optimizers.size(); ++i) {
    info_positions_.push_back(run_infos_.size());
    run_infos_.push_back(RunInfo(i, optimizers[i]->name()));
  }
}

OptimizerIndex OptimizerSelector::SelectOptimizer() {
  CHECK_GE(selected_index_, 0);

  do {
    ++selected_index_;
  } while (selected_index_ < run_infos_.size() &&
           !run_infos_[selected_index_].RunnableAndSelectable());

  if (selected_index_ >= run_infos_.size()) {
    // Select the first possible optimizer.
    selected_index_ = -1;
    for (int i = 0; i < run_infos_.size(); ++i) {
      if (run_infos_[i].RunnableAndSelectable()) {
        selected_index_ = i;
        break;
      }
    }
    if (selected_index_ == -1) return kInvalidOptimizerIndex;
  } else {
    // Select the next possible optimizer. If none, select the first one.
    // Check that the time is smaller than all previous optimizers which are
    // runnable.
    bool too_much_time_spent = false;
    const double time_spent =
        run_infos_[selected_index_].time_spent_since_last_solution;
    for (int i = 0; i < selected_index_; ++i) {
      const RunInfo& info = run_infos_[i];
      if (info.RunnableAndSelectable() &&
          info.time_spent_since_last_solution < time_spent) {
        too_much_time_spent = true;
        break;
      }
    }
    if (too_much_time_spent) {
      // TODO(user): Remove this recursive call, even if in practice it's
      //              safe because the max depth is the number of optimizers.
      return SelectOptimizer();
    }
  }

  // Select the optimizer.
  ++run_infos_[selected_index_].num_calls;
  return run_infos_[selected_index_].optimizer_index;
}

void OptimizerSelector::UpdateScore(int64 gain, double time_spent) {
  const bool new_solution_found = gain != 0;
  if (new_solution_found) NewSolutionFound(gain);
  UpdateDeterministicTime(time_spent);

  const double new_score = time_spent == 0.0 ? 0.0 : gain / time_spent;
  const double kErosion = 0.2;
  const double kMinScore = 1E-6;

  RunInfo& info = run_infos_[selected_index_];
  const double old_score = info.score;
  info.score =
      std::max(kMinScore, old_score * (1 - kErosion) + kErosion * new_score);

  if (new_solution_found) {  // Solution found
    UpdateOrder();
    selected_index_ = run_infos_.size();
  }
}

void OptimizerSelector::TemporarilyMarkOptimizerAsUnselectable(
    OptimizerIndex optimizer_index) {
  run_infos_[info_positions_[optimizer_index]].selectable = false;
}

void OptimizerSelector::SetOptimizerRunnability(OptimizerIndex optimizer_index,
                                                bool runnable) {
  run_infos_[info_positions_[optimizer_index]].runnable = runnable;
}

std::string OptimizerSelector::PrintStats(OptimizerIndex optimizer_index) const {
  const RunInfo& info = run_infos_[info_positions_[optimizer_index]];
  return StringPrintf(
      "    %40s : %3d/%-3d  (%6.2f%%)  Total gain: %6lld  Total Dtime: %0.3f "
      "score: %f\n",
      info.name.c_str(), info.num_successes, info.num_calls,
      100.0 * info.num_successes / info.num_calls, info.total_gain,
      info.time_spent, info.score);
}

int OptimizerSelector::NumCallsForOptimizer(
    OptimizerIndex optimizer_index) const {
  const RunInfo& info = run_infos_[info_positions_[optimizer_index]];
  return info.num_calls;
}

void OptimizerSelector::DebugPrint() const {
  std::string str;
  for (int i = 0; i < run_infos_.size(); ++i) {
    const RunInfo& info = run_infos_[i];
    LOG(INFO) << "               " << info.name << "  " << info.total_gain
              << " /  " << info.time_spent << " = " << info.score << "   "
              << info.selectable << "  " << info.time_spent_since_last_solution;
  }
}

void OptimizerSelector::NewSolutionFound(int64 gain) {
  run_infos_[selected_index_].num_successes++;
  run_infos_[selected_index_].total_gain += gain;

  for (int i = 0; i < run_infos_.size(); ++i) {
    run_infos_[i].time_spent_since_last_solution = 0;
    run_infos_[i].selectable = true;
  }
}

void OptimizerSelector::UpdateDeterministicTime(double time_spent) {
  run_infos_[selected_index_].time_spent += time_spent;
  run_infos_[selected_index_].time_spent_since_last_solution += time_spent;
}

void OptimizerSelector::UpdateOrder() {
  // Re-sort optimizers.
  std::stable_sort(run_infos_.begin(), run_infos_.end(),
                   [](const RunInfo& a, const RunInfo& b) -> bool {
                     if (a.total_gain == 0 && b.total_gain == 0)
                       return a.time_spent < b.time_spent;
                     return a.score > b.score;
                   });

  // Update the positions.
  for (int i = 0; i < run_infos_.size(); ++i) {
    info_positions_[run_infos_[i].optimizer_index] = i;
  }
}

}  // namespace bop
}  // namespace operations_research
