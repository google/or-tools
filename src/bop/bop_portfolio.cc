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

#include "bop/bop_portfolio.h"

#include "bop/bop_fs.h"
#include "bop/bop_lns.h"
#include "bop/bop_ls.h"
#include "bop/bop_util.h"
#include "bop/complete_optimizer.h"
#include "sat/boolean_problem.h"

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
      optimizer_initial_scores_(),
      sat_propagator_(),
      parameters_(parameters),
      lower_bound_(-glop::kInfinity),
      upper_bound_(glop::kInfinity) {
  CreateOptimizers(problem_state.original_problem(), parameters, optimizer_set);
}

PortfolioOptimizer::~PortfolioOptimizer() {
  if (parameters_.log_search_progress() || VLOG_IS_ON(1)) {
    std::string stats_string;
    for (int i = 0; i < optimizers_.size(); ++i) {
      const std::string& name = optimizers_[i]->name();
      const int a = stats_num_solutions_by_optimizer_[name];
      const int b = stats_num_calls_by_optimizer_[name];
      stats_string += StringPrintf(
          "    %40s : %3d/%-3d  (%6.2f%%)  score: %f\n", name.c_str(), a, b,
          100.0 * a / b, selector_->CurrentScore(i));
    }
    LOG(INFO) << "Stats. #new_solutions/#calls by optimizer:\n" + stats_string;
  }
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

  // Mark optimizers that can't run.
  for (int i = 0; i < optimizers_.size(); ++i) {
    if (!optimizers_[i]->ShouldBeRun(problem_state)) {
      selector_->MarkItemNonSelectable(i);
    }
  }

  if (!selector_->SelectItem()) {
    return BopOptimizerBase::ABORT;
  }

  const int selected_optimizer_id = selector_->selected_item_id();
  const std::unique_ptr<BopOptimizerBase>& selected_optimizer =
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

  // Statistics.
  stats_num_calls_by_optimizer_[selected_optimizer->name()]++;
  if (optimization_status == BopOptimizerBase::OPTIMAL_SOLUTION_FOUND ||
      optimization_status == BopOptimizerBase::SOLUTION_FOUND) {
    stats_num_solutions_by_optimizer_[selected_optimizer->name()]++;
  }

  if (optimization_status == BopOptimizerBase::INFEASIBLE ||
      optimization_status == BopOptimizerBase::OPTIMAL_SOLUTION_FOUND) {
    return optimization_status;
  }

  // TODO(user): don't penalize the SatCoreBasedOptimizer or the
  // LinearRelaxation when they improve the lower bound.
  selector_->UpdateSelectedItem(
      /*success=*/optimization_status == BopOptimizerBase::SOLUTION_FOUND,
      /*can_still_be_selected=*/optimization_status != BopOptimizerBase::ABORT);

  if (optimization_status == BopOptimizerBase::SOLUTION_FOUND ||
      optimization_status == BopOptimizerBase::INFORMATION_FOUND) {
    selector_->StartNewRoundOfSelections();
  }

  return BopOptimizerBase::CONTINUE;
}

void PortfolioOptimizer::AddOptimizer(
    const LinearBooleanProblem& problem, const BopParameters& parameters,
    const BopOptimizerMethod& optimizer_method) {
  switch (optimizer_method.type()) {
    case BopOptimizerMethod::SAT_CORE_BASED:
      optimizers_.emplace_back(
          new SatCoreBasedOptimizer("SatCoreBasedOptimizer"));
      break;
    case BopOptimizerMethod::SAT_LINEAR_SEARCH:
      optimizers_.emplace_back(new GuidedSatFirstSolutionGenerator(
          "SatOptimizer", GuidedSatFirstSolutionGenerator::Policy::kNotGuided));
      break;
    case BopOptimizerMethod::LINEAR_RELAXATION:
      optimizers_.emplace_back(
          new LinearRelaxation(parameters, "LinearRelaxation"));
      break;
    case BopOptimizerMethod::LOCAL_SEARCH: {
      double initial_local_search_score = optimizer_method.initial_score();
      for (int i = 1; i <= parameters.max_num_decisions(); ++i) {
        optimizers_.emplace_back(new LocalSearchOptimizer(
            StringPrintf("LS_%d", i), i, &sat_propagator_));
        optimizer_initial_scores_.push_back(initial_local_search_score);
        initial_local_search_score /= 2.0;
      }
    } break;
    case BopOptimizerMethod::RANDOM_FIRST_SOLUTION:
      optimizers_.emplace_back(new BopRandomFirstSolutionGenerator(
          "SATRandomFirstSolution", &sat_propagator_, random_.get()));
      break;
    case BopOptimizerMethod::RANDOM_VARIABLE_LNS:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.emplace_back(new BopAdaptiveLNSOptimizer(
          "RandomVariableLns",
          /*use_lp_to_guide_sat=*/false,
          new ObjectiveBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RANDOM_VARIABLE_LNS_GUIDED_BY_LP:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.emplace_back(new BopAdaptiveLNSOptimizer(
          "RandomVariableLnsWithLp",
          /*use_lp_to_guide_sat=*/true,
          new ObjectiveBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RANDOM_CONSTRAINT_LNS:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.emplace_back(new BopAdaptiveLNSOptimizer(
          "RandomConstraintLns",
          /*use_lp_to_guide_sat=*/false,
          new ConstraintBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::RANDOM_CONSTRAINT_LNS_GUIDED_BY_LP:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.emplace_back(new BopAdaptiveLNSOptimizer(
          "RandomConstraintLnsWithLp",
          /*use_lp_to_guide_sat=*/true,
          new ConstraintBasedNeighborhood(&objective_terms_, random_.get()),
          &sat_propagator_));
      break;
    case BopOptimizerMethod::COMPLETE_LNS:
      BuildObjectiveTerms(problem, &objective_terms_);
      optimizers_.emplace_back(
          new BopCompleteLNSOptimizer("LNS", objective_terms_));
      break;
    case BopOptimizerMethod::USER_GUIDED_FIRST_SOLUTION:
      optimizers_.emplace_back(new GuidedSatFirstSolutionGenerator(
          "SATUserGuidedFirstSolution",
          GuidedSatFirstSolutionGenerator::Policy::kUserGuided));
      break;
    case BopOptimizerMethod::LP_FIRST_SOLUTION:
      optimizers_.emplace_back(new GuidedSatFirstSolutionGenerator(
          "SATLPFirstSolution",
          GuidedSatFirstSolutionGenerator::Policy::kLpGuided));
      break;
    case BopOptimizerMethod::OBJECTIVE_FIRST_SOLUTION:
      optimizers_.emplace_back(new GuidedSatFirstSolutionGenerator(
          "SATObjectiveFirstSolution",
          GuidedSatFirstSolutionGenerator::Policy::kObjectiveGuided));
      break;
    default:
      LOG(FATAL) << "Unknown optimizer type.";
  }

  if (optimizer_method.type() != BopOptimizerMethod::LOCAL_SEARCH) {
    optimizer_initial_scores_.push_back(optimizer_method.initial_score());
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
    sat_propagator_.AddSymmetries(&generators);
  }

  const int max_num_optimizers =
      optimizer_set.methods_size() + parameters.max_num_decisions() - 1;
  optimizers_.reserve(max_num_optimizers);
  optimizer_initial_scores_.reserve(max_num_optimizers);
  for (const BopOptimizerMethod& optimizer_method : optimizer_set.methods()) {
    const int old_size = optimizers_.size();
    AddOptimizer(problem, parameters, optimizer_method);

    // Set the local time limits of the newly added optimizers.
    //
    // TODO(user): Find a way to make the behavior deterministic independently
    // of the overall time limit. For instance, only use deterministic time and
    // make that independent of the overall time limit.
    const double ratio = optimizer_method.time_limit_ratio();
    for (int i = old_size; i < optimizers_.size(); ++i) {
      optimizers_[i]->SetLocalTimeLimits(
          ratio * parameters.max_time_in_seconds(),
          ratio * parameters.max_deterministic_time());
    }
  }
  selector_.reset(
      new AdaptativeItemSelector(optimizer_initial_scores_, random_.get()));
}

//------------------------------------------------------------------------------
// AdaptativeItemSelector
//------------------------------------------------------------------------------

AdaptativeItemSelector::AdaptativeItemSelector(
    const std::vector<double>& initial_scores, MTRandom* random)
    : random_(random), selected_item_id_(kNoSelection), items_() {
  items_.reserve(initial_scores.size());
  for (const double score : initial_scores) {
    items_.push_back(Item(score));
  }
  StartNewRoundOfSelections();
}

const int AdaptativeItemSelector::kNoSelection(-1);
const double AdaptativeItemSelector::kErosion(0.2);
const double AdaptativeItemSelector::kScoreMin(1e-10);

void AdaptativeItemSelector::StartNewRoundOfSelections() {
  for (int item_id = 0; item_id < items_.size(); ++item_id) {
    Item& item = items_[item_id];
    if (item.num_selections > 0) {
      item.round_score = std::max(kScoreMin, (1 - kErosion) * item.round_score +
                                            kErosion * item.num_successes);
    }
    item.current_score = item.round_score;
    item.can_be_selected = true;
    item.num_selections = 0;
    item.num_successes = 0;
  }
}

bool AdaptativeItemSelector::SelectItem() {
  // Compute the sum score of selectable items.
  double score_sum = 0.0;
  for (const Item& item : items_) {
    if (item.can_be_selected) {
      score_sum += item.current_score;
    }
  }

  // Select item.
  const double selected_score_sum = random_->UniformDouble(score_sum);
  double selection_sum = 0.0;
  selected_item_id_ = kNoSelection;
  for (int item_id = 0; item_id < items_.size(); ++item_id) {
    const Item& item = items_[item_id];
    if (item.can_be_selected) {
      selection_sum += item.current_score;
    }
    if (selection_sum > selected_score_sum) {
      selected_item_id_ = item_id;
      break;
    }
  }

  if (selected_item_id_ != kNoSelection) {
    items_[selected_item_id_].num_selections++;
    return true;
  }

  return false;
}

void AdaptativeItemSelector::UpdateSelectedItem(bool success,
                                                bool can_still_be_selected) {
  Item& item = items_[selected_item_id_];
  item.num_successes += success ? 1 : 0;
  item.can_be_selected = can_still_be_selected;
  item.current_score = std::max(kScoreMin, (1 - kErosion) * item.current_score +
                                          (success ? kErosion : 0));
}

void AdaptativeItemSelector::MarkItemNonSelectable(int item) {
  items_[item].can_be_selected = false;
}
}  // namespace bop
}  // namespace operations_research
