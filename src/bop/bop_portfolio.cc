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

  objective_terms->clear();
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
      random_(parameters.random_seed()),
      objective_terms_(),
      selector_(),
      optimizers_(),
      optimizer_initial_scores_(),
      sat_propagator_(problem_state.original_problem()),
      feasible_solution_(false),
      lower_bound_(-glop::kInfinity),
      upper_bound_(glop::kInfinity) {
  CreateOptimizers(problem_state, parameters, optimizer_set);
}

PortfolioOptimizer::~PortfolioOptimizer() {}

BopOptimizerBase::Status PortfolioOptimizer::Synchronize(
    const ProblemState& problem_state) {
  if (!sat_propagator_.LoadBooleanProblem()) {
    return BopOptimizerBase::INFEASIBLE;
  }

  // Set a new constraint on the objective cost to only look for better
  // solutions.
  if (problem_state.solution().IsFeasible() &&
      !sat_propagator_.OverConstrainObjective(problem_state.solution())) {
    // Stop optimization as SAT proved optimality.
    return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
  }

  for (const std::unique_ptr<BopOptimizerBase>& optimizer : optimizers_) {
    if (problem_state.solution().IsFeasible() ||
        !optimizer->NeedAFeasibleSolution()) {
      const BopOptimizerBase::Status sync_status =
          optimizer->Synchronize(problem_state);
      if (sync_status == BopOptimizerBase::OPTIMAL_SOLUTION_FOUND ||
          sync_status == BopOptimizerBase::INFEASIBLE) {
        return sync_status;
      }
    }
  }

  feasible_solution_ = problem_state.solution().IsFeasible();
  lower_bound_ = problem_state.GetScaledLowerBound();
  upper_bound_ = feasible_solution_ ? problem_state.solution().GetScaledCost()
                                    : glop::kInfinity;
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status PortfolioOptimizer::Optimize(
    const BopParameters& parameters, LearnedInfo* learned_info,
    TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);

  learned_info->Clear();

  if (!feasible_solution_) {
    // Mark optimizers that can't run because they need an initial solution.
    for (int i = 0; i < optimizers_.size(); ++i) {
      if (optimizers_[i]->NeedAFeasibleSolution()) {
        selector_->MarkItemNonSelectable(i);
      }
    }
  }

  if (!selector_->SelectItem()) {
    return BopOptimizerBase::ABORT;
  }

  const int selected_optimizer_id = selector_->selected_item_id();
  const std::unique_ptr<BopOptimizerBase>& selected_optimizer =
      optimizers_[selected_optimizer_id];
  LOG(INFO) << "      " << lower_bound_ << " .. " << upper_bound_ << " "
            << name() << " - " << selected_optimizer->name()
            << ". Time limit: " << time_limit->GetTimeLeft() << " -- "
            << time_limit->GetDeterministicTimeLeft();
  const BopOptimizerBase::Status optimization_status =
      selected_optimizer->Optimize(parameters, learned_info, time_limit);
  selector_->UpdateSelectedItem(
      optimization_status == BopOptimizerBase::SOLUTION_FOUND,
      !selected_optimizer->RunOncePerSolution() &&
          optimization_status != BopOptimizerBase::ABORT);

  if (optimization_status == BopOptimizerBase::SOLUTION_FOUND) {
    selector_->StartNewRoundOfSelections();
  }

  return BopOptimizerBase::CONTINUE;
}

void PortfolioOptimizer::AddOptimizer(
    const BopParameters& parameters,
    const BopOptimizerMethod& optimizer_method) {
  switch (optimizer_method.type()) {
    case BopOptimizerMethod::SAT_CORE_BASED:
      optimizers_.emplace_back(
          new SatCoreBasedOptimizer("SatCoreBasedOptimizer"));
      break;
    case BopOptimizerMethod::LINEAR_RELAXATION:
      optimizers_.emplace_back(new LinearRelaxation(
          parameters, "LinearRelaxation", optimizer_method.time_limit_ratio()));
      break;
    case BopOptimizerMethod::LOCAL_SEARCH: {
      double initial_local_search_score = optimizer_method.initial_score();
      for (int i = 1; i <= parameters.max_num_decisions(); ++i) {
        optimizers_.emplace_back(
            new LocalSearchOptimizer(StringPrintf("LS_%d", i), i));
        optimizer_initial_scores_.push_back(initial_local_search_score);
        initial_local_search_score /= 2.0;
      }
    } break;
    case BopOptimizerMethod::RANDOM_FIRST_SOLUTION:
      optimizers_.emplace_back(new BopRandomFirstSolutionGenerator(
          parameters.random_seed(), "SATRandomFirstSolution",
          optimizer_method.time_limit_ratio()));
      break;
    case BopOptimizerMethod::RANDOM_CONSTRAINT_LNS:
      optimizers_.emplace_back(new BopRandomConstraintLNSOptimizer(
          "Random_Constraint_LNS", objective_terms_, parameters.random_seed()));
      break;
    case BopOptimizerMethod::RANDOM_LNS_PROPAGATION:
      optimizers_.emplace_back(new BopRandomLNSOptimizer(
          "Random_LNS_Using_Variables_Connections", objective_terms_,
          parameters.random_seed(), false, &sat_propagator_));
      break;
    case BopOptimizerMethod::RANDOM_LNS_SAT:
      optimizers_.emplace_back(new BopRandomLNSOptimizer(
          "Random_LNS_Using_SAT", objective_terms_, parameters.random_seed(),
          true, &sat_propagator_));
      break;
    case BopOptimizerMethod::COMPLETE_LNS:
      optimizers_.emplace_back(new BopCompleteLNSOptimizer(
          "LNS", objective_terms_, sat_propagator_.sat_solver()));
      break;
    case BopOptimizerMethod::LP_FIRST_SOLUTION:
      optimizers_.emplace_back(new BopSatLpFirstSolutionGenerator(
          "SATLPFirstSolution", optimizer_method.time_limit_ratio()));
      break;
    case BopOptimizerMethod::OBJECTIVE_FIRST_SOLUTION:
      optimizers_.emplace_back(new BopSatObjectiveFirstSolutionGenerator(
          "SATObjectiveFirstSolution", optimizer_method.time_limit_ratio()));
      break;
    default:
      LOG(FATAL) << "Unknown optimizer type.";
  }

  if (optimizer_method.type() != BopOptimizerMethod::LOCAL_SEARCH) {
    optimizer_initial_scores_.push_back(optimizer_method.initial_score());
  }
}

void PortfolioOptimizer::CreateOptimizers(
    const ProblemState& problem_state, const BopParameters& parameters,
    const BopSolverOptimizerSet& optimizer_set) {
  BuildObjectiveTerms(problem_state.original_problem(), &objective_terms_);

  if (parameters.use_symmetry()) {
    LOG(INFO) << "Finding symmetries of the problem.";
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    sat::FindLinearBooleanProblemSymmetries(problem_state.original_problem(),
                                            &generators);
    sat_propagator_.sat_solver()->AddSymmetries(&generators);
  }

  for (const BopOptimizerMethod& optimizer_method : optimizer_set.methods()) {
    AddOptimizer(parameters, optimizer_method);
  }
  selector_.reset(new AdaptativeItemSelector(parameters.random_seed(),
                                             optimizer_initial_scores_));
}

//------------------------------------------------------------------------------
// AdaptativeItemSelector
//------------------------------------------------------------------------------

AdaptativeItemSelector::AdaptativeItemSelector(
    int random_seed, const std::vector<double>& initial_scores)
    : random_(random_seed), selected_item_id_(kNoSelection), items_() {
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
  const double selected_score_sum = random_.UniformDouble(score_sum);
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
