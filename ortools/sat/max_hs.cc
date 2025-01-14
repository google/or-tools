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

#include "ortools/sat/max_hs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#if !defined(__PORTABLE_PLATFORM__) && defined(USE_SCIP)
#include "ortools/linear_solver/solve_mp_model.h"
#endif  // __PORTABLE_PLATFORM__
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

// TODO(user): Remove this flag when experiments are stable.
ABSL_FLAG(
    int, max_hs_strategy, 0,
    "MaxHsStrategy: 0 extract only objective variable, 1 extract all variables "
    "colocated with objective variables, 2 extract all variables in the "
    "linearization");

namespace operations_research {
namespace sat {

HittingSetOptimizer::HittingSetOptimizer(
    const CpModelProto& model_proto,
    const ObjectiveDefinition& objective_definition,
    const std::function<void()>& feasible_solution_observer, Model* model)
    : model_proto_(model_proto),
      objective_definition_(objective_definition),
      feasible_solution_observer_(feasible_solution_observer),
      model_(model),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      parameters_(*model->GetOrCreate<SatParameters>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      shared_response_(model->GetOrCreate<SharedResponseManager>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      integer_encoder_(model_->GetOrCreate<IntegerEncoder>()) {
  request_.set_solver_specific_parameters("limits/gap = 0");
  request_.set_solver_type(MPModelRequest::SCIP_MIXED_INTEGER_PROGRAMMING);
}

bool HittingSetOptimizer::ImportFromOtherWorkers() {
  auto* level_zero_callbacks = model_->GetOrCreate<LevelZeroCallbackHelper>();
  for (const auto& cb : level_zero_callbacks->callbacks) {
    if (!cb()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }
  }
  return true;
}

// Slightly different algo than FindCores() which aim to extract more cores, but
// not necessarily non-overlaping ones.
SatSolver::Status HittingSetOptimizer::FindMultipleCoresForMaxHs(
    std::vector<Literal> assumptions,
    std::vector<std::vector<Literal>>* cores) {
  cores->clear();
  const double saved_dlimit = time_limit_->GetDeterministicLimit();
  auto cleanup = ::absl::MakeCleanup([this, saved_dlimit]() {
    time_limit_->ChangeDeterministicLimit(saved_dlimit);
  });

  bool first_loop = true;
  do {
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;

    // The order of assumptions do not matter.
    // Randomizing it should improve diversity.
    std::shuffle(assumptions.begin(), assumptions.end(), *random_);

    const SatSolver::Status result =
        ResetAndSolveIntegerProblem(assumptions, model_);
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;
    std::vector<Literal> core = sat_solver_->GetLastIncompatibleDecisions();
    if (sat_solver_->parameters().core_minimization_level() > 0) {
      MinimizeCoreWithPropagation(time_limit_, sat_solver_, &core);
    }
    CHECK(!core.empty());
    cores->push_back(core);
    if (!parameters_.find_multiple_cores()) break;

    // Pick a random literal from the core and remove it from the set of
    // assumptions.
    CHECK(!core.empty());
    const Literal random_literal =
        core[absl::Uniform<int>(*random_, 0, core.size())];
    for (int i = 0; i < assumptions.size(); ++i) {
      if (assumptions[i] == random_literal) {
        std::swap(assumptions[i], assumptions.back());
        assumptions.pop_back();
        break;
      }
    }

    // Once we found at least one core, we impose a time limit to not spend
    // too much time finding more.
    if (first_loop) {
      time_limit_->ChangeDeterministicLimit(std::min(
          saved_dlimit, time_limit_->GetElapsedDeterministicTime() + 1.0));
      first_loop = false;
    }
  } while (!assumptions.empty());

  return SatSolver::ASSUMPTIONS_UNSAT;
}

int HittingSetOptimizer::GetExtractedIndex(IntegerVariable var) const {
  if (var.value() >= sat_var_to_mp_var_.size()) return kUnextracted;
  return sat_var_to_mp_var_[var];
}

void HittingSetOptimizer::ExtractObjectiveVariables() {
  const std::vector<IntegerVariable>& variables = objective_definition_.vars;
  const std::vector<IntegerValue>& coefficients = objective_definition_.coeffs;
  MPModelProto* hs_model = request_.mutable_model();

  // Create the initial objective constraint.
  // It is used to constraint the objective during search.
  if (obj_constraint_ == nullptr) {
    obj_constraint_ = hs_model->add_constraint();
    obj_constraint_->set_lower_bound(-std::numeric_limits<double>::infinity());
    obj_constraint_->set_upper_bound(std::numeric_limits<double>::infinity());
  }

  // Extract the objective variables.
  for (int i = 0; i < variables.size(); ++i) {
    IntegerVariable var = variables[i];
    IntegerValue coeff = coefficients[i];

    // Link the extracted variable to the positive variable.
    if (!VariableIsPositive(var)) {
      var = NegationOf(var);
      coeff = -coeff;
    }

    // Normalized objective variables expects positive coefficients.
    if (coeff > 0) {
      normalized_objective_variables_.push_back(var);
      normalized_objective_coefficients_.push_back(coeff);
    } else {
      normalized_objective_variables_.push_back(NegationOf(var));
      normalized_objective_coefficients_.push_back(-coeff);
    }

    // Extract.
    const int index = hs_model->variable_size();
    obj_constraint_->add_var_index(index);
    obj_constraint_->add_coefficient(ToDouble(coeff));

    MPVariableProto* var_proto = hs_model->add_variable();
    var_proto->set_lower_bound(ToDouble(integer_trail_->LowerBound(var)));
    var_proto->set_upper_bound(ToDouble(integer_trail_->UpperBound(var)));
    var_proto->set_objective_coefficient(ToDouble(coeff));
    var_proto->set_is_integer(true);

    // Store extraction info.
    const int max_index = std::max(var.value(), NegationOf(var).value());
    if (max_index >= sat_var_to_mp_var_.size()) {
      sat_var_to_mp_var_.resize(max_index + 1, -1);
    }
    sat_var_to_mp_var_[var] = index;
    sat_var_to_mp_var_[NegationOf(var)] = index;
    extracted_variables_info_.push_back({var, var_proto});
  }
}

void HittingSetOptimizer::ExtractAdditionalVariables(
    absl::Span<const IntegerVariable> to_extract) {
  MPModelProto* hs_model = request_.mutable_model();

  VLOG(2) << "Extract " << to_extract.size() << " additional variables";
  for (IntegerVariable tmp_var : to_extract) {
    if (GetExtractedIndex(tmp_var) != kUnextracted) continue;

    // Use the positive variable for the domain.
    const IntegerVariable var = PositiveVariable(tmp_var);

    const int index = hs_model->variable_size();
    MPVariableProto* var_proto = hs_model->add_variable();
    var_proto->set_lower_bound(ToDouble(integer_trail_->LowerBound(var)));
    var_proto->set_upper_bound(ToDouble(integer_trail_->UpperBound(var)));
    var_proto->set_is_integer(true);

    // Store extraction info.
    const int max_index = std::max(var.value(), NegationOf(var).value());
    if (max_index >= sat_var_to_mp_var_.size()) {
      sat_var_to_mp_var_.resize(max_index + 1, -1);
    }
    sat_var_to_mp_var_[var] = index;
    sat_var_to_mp_var_[NegationOf(var)] = index;
    extracted_variables_info_.push_back({var, var_proto});
  }
}

// This code will use heuristics to decide which non-objective variables to
// extract:
//  0: no additional variables.
//  1: any variable appearing in the same constraint as an objective variable.
//  2: all variables appearing in the linear relaxation.
//
// TODO(user): We could also decide to extract all if small enough.
std::vector<IntegerVariable>
HittingSetOptimizer::ComputeAdditionalVariablesToExtract() {
  absl::flat_hash_set<IntegerVariable> result_set;
  if (absl::GetFlag(FLAGS_max_hs_strategy) == 0) return {};
  const bool extract_all = absl::GetFlag(FLAGS_max_hs_strategy) == 2;

  for (const std::vector<Literal>& literals : relaxation_.at_most_ones) {
    bool found_at_least_one = extract_all;
    for (const Literal literal : literals) {
      if (GetExtractedIndex(integer_encoder_->GetLiteralView(literal)) !=
          kUnextracted) {
        found_at_least_one = true;
      }
      if (found_at_least_one) break;
    }
    if (!found_at_least_one) continue;
    for (const Literal literal : literals) {
      const IntegerVariable var = integer_encoder_->GetLiteralView(literal);
      if (GetExtractedIndex(var) == kUnextracted) {
        result_set.insert(PositiveVariable(var));
      }
    }
  }

  for (const LinearConstraint& linear : relaxation_.linear_constraints) {
    bool found_at_least_one = extract_all;
    for (const IntegerVariable var : linear.VarsAsSpan()) {
      if (GetExtractedIndex(var) != kUnextracted) {
        found_at_least_one = true;
      }
      if (found_at_least_one) break;
    }
    if (!found_at_least_one) continue;
    for (const IntegerVariable var : linear.VarsAsSpan()) {
      if (GetExtractedIndex(var) == kUnextracted) {
        result_set.insert(PositiveVariable(var));
      }
    }
  }

  std::vector<IntegerVariable> result(result_set.begin(), result_set.end());
  std::sort(result.begin(), result.end());

  return result;
}

void HittingSetOptimizer::ProjectAndAddAtMostOne(
    absl::Span<const Literal> literals) {
  LinearConstraintBuilder builder(model_, 0, 1);
  for (const Literal& literal : literals) {
    if (!builder.AddLiteralTerm(literal, 1)) {
      VLOG(3) << "Could not extract literal " << literal;
    }
  }

  if (ProjectAndAddLinear(builder.Build()) != nullptr) {
    num_extracted_at_most_ones_++;
  }
}

MPConstraintProto* HittingSetOptimizer::ProjectAndAddLinear(
    const LinearConstraint& linear) {
  int num_extracted_variables = 0;
  for (int i = 0; i < linear.num_terms; ++i) {
    if (GetExtractedIndex(PositiveVariable(linear.vars[i])) != kUnextracted) {
      num_extracted_variables++;
    }
  }
  if (num_extracted_variables <= 1) return nullptr;

  MPConstraintProto* ct = request_.mutable_model()->add_constraint();
  ProjectLinear(linear, ct);
  return ct;
}

void HittingSetOptimizer::ProjectLinear(const LinearConstraint& linear,
                                        MPConstraintProto* ct) {
  IntegerValue lb = linear.lb;
  IntegerValue ub = linear.ub;

  for (int i = 0; i < linear.num_terms; ++i) {
    const IntegerVariable var = linear.vars[i];
    const IntegerValue coeff = linear.coeffs[i];
    const int index = GetExtractedIndex(PositiveVariable(var));
    const bool negated = !VariableIsPositive(var);
    if (index != kUnextracted) {
      ct->add_var_index(index);
      ct->add_coefficient(negated ? -ToDouble(coeff) : ToDouble(coeff));
    } else {
      const IntegerValue var_lb = integer_trail_->LevelZeroLowerBound(var);
      const IntegerValue var_ub = integer_trail_->LevelZeroUpperBound(var);

      if (coeff > 0) {
        if (lb != kMinIntegerValue) lb -= coeff * var_ub;
        if (ub != kMaxIntegerValue) ub -= coeff * var_lb;
      } else {
        if (lb != kMinIntegerValue) lb -= coeff * var_lb;
        if (ub != kMaxIntegerValue) ub -= coeff * var_ub;
      }
    }
  }

  ct->set_lower_bound(ToDouble(lb));
  ct->set_upper_bound(ToDouble(ub));
}

bool HittingSetOptimizer::ComputeInitialMpModel() {
  if (!ImportFromOtherWorkers()) return false;

  ExtractObjectiveVariables();

  // Linearize the constraints from the model.
  ActivityBoundHelper activity_bound_helper;
  activity_bound_helper.AddAllAtMostOnes(model_proto_);
  for (const auto& ct : model_proto_.constraints()) {
    TryToLinearizeConstraint(model_proto_, ct, /*linearization_level=*/2,
                             model_, &relaxation_, &activity_bound_helper);
  }

  ExtractAdditionalVariables(ComputeAdditionalVariablesToExtract());

  // Build the MPModel from the linear relaxation.
  for (const auto& literals : relaxation_.at_most_ones) {
    ProjectAndAddAtMostOne(literals);
  }
  if (num_extracted_at_most_ones_ > 0) {
    VLOG(2) << "Projected " << num_extracted_at_most_ones_ << "/"
            << relaxation_.at_most_ones.size() << " at_most_ones constraints";
  }

  for (int i = 0; i < relaxation_.linear_constraints.size(); ++i) {
    MPConstraintProto* ct =
        ProjectAndAddLinear(relaxation_.linear_constraints[i]);
    if (ct != nullptr) linear_extract_info_.push_back({i, ct});
  }
  if (!linear_extract_info_.empty()) {
    VLOG(2) << "Projected " << linear_extract_info_.size() << "/"
            << relaxation_.linear_constraints.size() << " linear constraints";
  }
  return true;
}

void HittingSetOptimizer::TightenMpModel() {
  // Update the MP variables bounds from the SAT level 0 bounds.
  for (const auto& [var, var_proto] : extracted_variables_info_) {
    var_proto->set_lower_bound(ToDouble(integer_trail_->LowerBound(var)));
    var_proto->set_upper_bound(ToDouble(integer_trail_->UpperBound(var)));
  }

  int tightened = 0;
  for (const auto& [index, ct] : linear_extract_info_) {
    const double original_lb = ct->lower_bound();
    const double original_ub = ct->upper_bound();
    ct->Clear();
    ProjectLinear(relaxation_.linear_constraints[index], ct);
    if (original_lb != ct->lower_bound() || original_ub != ct->upper_bound()) {
      tightened++;
    }
  }
  if (tightened > 0) {
    VLOG(2) << "Tightened " << tightened << " linear constraints";
  }
}

bool HittingSetOptimizer::ProcessSolution() {
  const std::vector<IntegerVariable>& variables = objective_definition_.vars;
  const std::vector<IntegerValue>& coefficients = objective_definition_.coeffs;

  // We don't assume that objective_var is linked with its linear term, so
  // we recompute the objective here.
  IntegerValue objective(0);
  for (int i = 0; i < variables.size(); ++i) {
    objective +=
        coefficients[i] * IntegerValue(model_->Get(Value(variables[i])));
  }
  if (objective >
      integer_trail_->UpperBound(objective_definition_.objective_var)) {
    return true;
  }

  if (feasible_solution_observer_ != nullptr) {
    feasible_solution_observer_();
  }

  // Constrain objective_var. This has a better result when objective_var is
  // used in an LP relaxation for instance.
  sat_solver_->Backtrack(0);
  sat_solver_->SetAssumptionLevel(0);
  if (!integer_trail_->Enqueue(
          IntegerLiteral::LowerOrEqual(objective_definition_.objective_var,
                                       objective - 1),
          {}, {})) {
    return false;
  }
  return true;
}

void HittingSetOptimizer::AddCoresToTheMpModel(
    absl::Span<const std::vector<Literal>> cores) {
  MPModelProto* hs_model = request_.mutable_model();

  for (const std::vector<Literal>& core : cores) {
    // For cores of size 1, we can just constrain the bound of the variable.
    if (core.size() == 1) {
      for (const int index : assumption_to_indices_.at(core.front().Index())) {
        const IntegerVariable var = normalized_objective_variables_[index];
        const double new_bound = ToDouble(integer_trail_->LowerBound(var));
        if (VariableIsPositive(var)) {
          hs_model->mutable_variable(index)->set_lower_bound(new_bound);
        } else {
          hs_model->mutable_variable(index)->set_upper_bound(-new_bound);
        }
      }
      continue;
    }

    // Add the corresponding constraint to hs_model.
    MPConstraintProto* at_least_one = hs_model->add_constraint();
    at_least_one->set_lower_bound(1.0);
    for (const Literal lit : core) {
      for (const int index : assumption_to_indices_.at(lit.Index())) {
        const IntegerVariable var = normalized_objective_variables_[index];
        const double sat_lb = ToDouble(integer_trail_->LowerBound(var));
        // normalized_objective_variables_[index] is mapped onto
        //     hs_model.variable[index] * sign.
        const double sign = VariableIsPositive(var) ? 1.0 : -1.0;
        // We round hs_value to the nearest integer. This should help in the
        // hash_map part.
        const double hs_value =
            std::round(response_.variable_value(index)) * sign;

        if (hs_value == sat_lb) {
          at_least_one->add_var_index(index);
          at_least_one->add_coefficient(sign);
          at_least_one->set_lower_bound(at_least_one->lower_bound() + hs_value);
        } else {
          // The operation type (< or >) is consistent for the same variable,
          // so we do not need this information in the key.
          const std::pair<int, int64_t> key = {index,
                                               static_cast<int64_t>(hs_value)};
          const int new_bool_var_index = hs_model->variable_size();
          const auto [it, inserted] =
              mp_integer_literals_.insert({key, new_bool_var_index});

          at_least_one->add_var_index(it->second);
          at_least_one->add_coefficient(1.0);

          if (inserted) {
            // Creates the implied bound constraint.
            MPVariableProto* bool_var = hs_model->add_variable();
            bool_var->set_lower_bound(0);
            bool_var->set_upper_bound(1);
            bool_var->set_is_integer(true);

            // (bool_var == 1) => x * sign > hs_value.
            // (x * sign - sat_lb) - (hs_value - sat_lb + 1) * bool_var >= 0.
            MPConstraintProto* implied_bound = hs_model->add_constraint();
            implied_bound->set_lower_bound(sat_lb);
            implied_bound->add_var_index(index);
            implied_bound->add_coefficient(sign);
            implied_bound->add_var_index(new_bool_var_index);
            implied_bound->add_coefficient(sat_lb - hs_value - 1.0);
          }
        }
      }
    }
  }
}

std::vector<Literal> HittingSetOptimizer::BuildAssumptions(
    IntegerValue stratified_threshold,
    IntegerValue* next_stratified_threshold) {
  std::vector<Literal> assumptions;
  // This code assumes that the variables from the objective are extracted
  // first, and in the order of the objective definition.
  for (int i = 0; i < normalized_objective_variables_.size(); ++i) {
    const IntegerVariable var = normalized_objective_variables_[i];
    const IntegerValue coeff = normalized_objective_coefficients_[i];

    // Correct the sign of the value queried from the MP solution.
    // Note that normalized_objective_variables_[i] is mapped onto
    //     hs_model.variable[i] * sign.
    const IntegerValue hs_value(
        static_cast<int64_t>(std::round(response_.variable_value(i))) *
        (VariableIsPositive(var) ? 1 : -1));

    // Non binding, ignoring.
    if (hs_value == integer_trail_->UpperBound(var)) continue;

    // Only consider the terms above the threshold.
    if (coeff < stratified_threshold) {
      *next_stratified_threshold = std::max(*next_stratified_threshold, coeff);
    } else {
      // It is possible that different variables have the same associated
      // literal. So we do need to consider this case.
      assumptions.push_back(integer_encoder_->GetOrCreateAssociatedLiteral(
          IntegerLiteral::LowerOrEqual(var, hs_value)));
      assumption_to_indices_[assumptions.back().Index()].push_back(i);
    }
  }
  return assumptions;
}

// This is the "generalized" hitting set problem we will solve. Each time
// we find a core, a new constraint will be added to this problem.
//
// TODO(user): remove code duplication with MinimizeWithCoreAndLazyEncoding();
SatSolver::Status HittingSetOptimizer::Optimize() {
#if !defined(__PORTABLE_PLATFORM__) && defined(USE_SCIP)
  if (!ComputeInitialMpModel()) return SatSolver::INFEASIBLE;

  // This is used by the "stratified" approach. We will only consider terms with
  // a weight not lower than this threshold. The threshold will decrease as the
  // algorithm progress.
  IntegerValue stratified_threshold = kMaxIntegerValue;

  // Start the algorithm.
  SatSolver::Status result;
  for (int iter = 0;; ++iter) {
    // TODO(user): Even though we keep the same solver, currently the solve is
    // not really done incrementally. It might be hard to improve though.
    //
    // TODO(user): deal with time limit.

    // Get the best external bound and constraint the objective of the MPModel.
    if (shared_response_ != nullptr) {
      const IntegerValue best_lower_bound =
          shared_response_->GetInnerObjectiveLowerBound();
      obj_constraint_->set_lower_bound(ToDouble(best_lower_bound));
    }

    if (!ImportFromOtherWorkers()) return SatSolver::INFEASIBLE;
    TightenMpModel();

    // TODO(user): C^c is broken when using SCIP.
    response_ = SolveMPModel(request_);
    if (response_.status() != MPSolverResponseStatus::MPSOLVER_OPTIMAL) {
      // We currently abort if we have a non-optimal result.
      // This is correct if we had a limit reached, but not in the other
      // cases.
      //
      // TODO(user): It is actually easy to use a FEASIBLE result. If when
      // passing it to SAT it is no feasbile, we can still create cores. If it
      // is feasible, we have a solution, but we cannot increase the lower
      // bound.
      return SatSolver::LIMIT_REACHED;
    }
    if (response_.status() != MPSolverResponseStatus::MPSOLVER_OPTIMAL) {
      continue;
    }

    const IntegerValue mip_objective(
        static_cast<int64_t>(std::round(response_.objective_value())));
    VLOG(2) << "--" << iter
            << "-- constraints:" << request_.mutable_model()->constraint_size()
            << " variables:" << request_.mutable_model()->variable_size()
            << " hs_lower_bound:"
            << objective_definition_.ScaleIntegerObjective(mip_objective)
            << " strat:" << stratified_threshold;

    // Update the objective lower bound with our current bound.
    //
    // Note(user): This is not needed for correctness, but it might cause
    // more propagation and is nice to have for reporting/logging purpose.
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(objective_definition_.objective_var,
                                           mip_objective),
            {}, {})) {
      result = SatSolver::INFEASIBLE;
      break;
    }

    sat_solver_->Backtrack(0);
    sat_solver_->SetAssumptionLevel(0);
    assumption_to_indices_.clear();
    IntegerValue next_stratified_threshold(0);
    const std::vector<Literal> assumptions =
        BuildAssumptions(stratified_threshold, &next_stratified_threshold);

    // No assumptions with the current stratified_threshold? use the new one.
    if (assumptions.empty() && next_stratified_threshold > 0) {
      CHECK_LT(next_stratified_threshold, stratified_threshold);
      stratified_threshold = next_stratified_threshold;
      --iter;  // "false" iteration, the lower bound does not increase.
      continue;
    }

    // TODO(user): Use the real weights and exploit the extra cores.
    // TODO(user): If we extract more than the objective variables, we could
    // use the solution values from the MPModel as hints to the SAT model.
    result = FindMultipleCoresForMaxHs(assumptions, &temp_cores_);
    if (result == SatSolver::FEASIBLE) {
      if (!ProcessSolution()) return SatSolver::INFEASIBLE;
      if (parameters_.stop_after_first_solution()) {
        return SatSolver::LIMIT_REACHED;
      }
      if (temp_cores_.empty()) {
        // If not all assumptions were taken, continue with a lower stratified
        // bound. Otherwise we have an optimal solution.
        stratified_threshold = next_stratified_threshold;
        if (stratified_threshold == 0) break;
        --iter;  // "false" iteration, the lower bound does not increase.
        continue;
      }
    } else if (result == SatSolver::LIMIT_REACHED) {
      // Hack: we use a local limit internally that we restore at the end.
      // However we still return LIMIT_REACHED in this case...
      if (time_limit_->LimitReached()) break;
    } else if (result != SatSolver::ASSUMPTIONS_UNSAT) {
      break;
    }

    sat_solver_->Backtrack(0);
    sat_solver_->SetAssumptionLevel(0);
    AddCoresToTheMpModel(temp_cores_);
  }

  return result;
#else   // !__PORTABLE_PLATFORM__ && USE_SCIP
  LOG(FATAL) << "Not supported.";
#endif  // !__PORTABLE_PLATFORM__ && USE_SCIP
}

}  // namespace sat
}  // namespace operations_research
