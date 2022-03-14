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

#include "ortools/sat/optimization.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stl_util.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/encoding.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

namespace {

// Used to log messages to stdout or to the normal logging framework according
// to the given LogBehavior value.
class Logger {
 public:
  explicit Logger(LogBehavior v) : use_stdout_(v == STDOUT_LOG) {}
  void Log(const std::string& message) {
    if (use_stdout_) {
      absl::PrintF("%s\n", message);
    } else {
      LOG(INFO) << message;
    }
  }

 private:
  bool use_stdout_;
};

// Outputs the current objective value in the cnf output format.
// Note that this function scale the given objective.
std::string CnfObjectiveLine(const LinearBooleanProblem& problem,
                             Coefficient objective) {
  const double scaled_objective =
      AddOffsetAndScaleObjectiveValue(problem, objective);
  return absl::StrFormat("o %d", static_cast<int64_t>(scaled_objective));
}

struct LiteralWithCoreIndex {
  LiteralWithCoreIndex(Literal l, int i) : literal(l), core_index(i) {}
  Literal literal;
  int core_index;
};

// Deletes the given indices from a vector. The given indices must be sorted in
// increasing order. The order of the non-deleted entries in the vector is
// preserved.
template <typename Vector>
void DeleteVectorIndices(const std::vector<int>& indices, Vector* v) {
  int new_size = 0;
  int indices_index = 0;
  for (int i = 0; i < v->size(); ++i) {
    if (indices_index < indices.size() && i == indices[indices_index]) {
      ++indices_index;
    } else {
      (*v)[new_size] = (*v)[i];
      ++new_size;
    }
  }
  v->resize(new_size);
}

// In the Fu & Malik algorithm (or in WPM1), when two cores overlap, we
// artificially introduce symmetries. More precisely:
//
// The picture below shows two cores with index 0 and 1, with one blocking
// variable per '-' and with the variables ordered from left to right (by their
// assumptions index). The blocking variables will be the one added to "relax"
// the core for the next iteration.
//
// 1: -------------------------------
// 0:                     ------------------------------------
//
// The 2 following assignment of the blocking variables are equivalent.
// Remember that exactly one blocking variable per core must be assigned to 1.
//
// 1: ----------------------1--------
// 0:                    --------1---------------------------
//
// and
//
// 1: ---------------------------1---
// 0:                    ---1--------------------------------
//
// This class allows to add binary constraints excluding the second possibility.
// Basically, each time a new core is added, if two of its blocking variables
// (b1, b2) have the same assumption index of two blocking variables from
// another core (c1, c2), then we forbid the assignment c1 true and b2 true.
//
// Reference: C Ansótegui, ML Bonet, J Levy, "Sat-based maxsat algorithms",
// Artificial Intelligence, 2013 - Elsevier.
class FuMalikSymmetryBreaker {
 public:
  FuMalikSymmetryBreaker() {}

  // Must be called before a new core is processed.
  void StartResolvingNewCore(int new_core_index) {
    literal_by_core_.resize(new_core_index);
    for (int i = 0; i < new_core_index; ++i) {
      literal_by_core_[i].clear();
    }
  }

  // This should be called for each blocking literal b of the new core. The
  // assumption_index identify the soft clause associated to the given blocking
  // literal. Note that between two StartResolvingNewCore() calls,
  // ProcessLiteral() is assumed to be called with different assumption_index.
  //
  // Changing the order of the calls will not change the correctness, but will
  // change the symmetry-breaking clause produced.
  //
  // Returns a set of literals which can't be true at the same time as b (under
  // symmetry breaking).
  std::vector<Literal> ProcessLiteral(int assumption_index, Literal b) {
    if (assumption_index >= info_by_assumption_index_.size()) {
      info_by_assumption_index_.resize(assumption_index + 1);
    }

    // Compute the function result.
    // info_by_assumption_index_[assumption_index] will contain all the pairs
    // (blocking_literal, core) of the previous resolved cores at the same
    // assumption index as b.
    std::vector<Literal> result;
    for (LiteralWithCoreIndex data :
         info_by_assumption_index_[assumption_index]) {
      // literal_by_core_ will contain all the blocking literal of a given core
      // with an assumption_index that was used in one of the ProcessLiteral()
      // calls since the last StartResolvingNewCore().
      //
      // Note that there can be only one such literal by core, so we will not
      // add duplicates.
      result.insert(result.end(), literal_by_core_[data.core_index].begin(),
                    literal_by_core_[data.core_index].end());
    }

    // Update the internal data structure.
    for (LiteralWithCoreIndex data :
         info_by_assumption_index_[assumption_index]) {
      literal_by_core_[data.core_index].push_back(data.literal);
    }
    info_by_assumption_index_[assumption_index].push_back(
        LiteralWithCoreIndex(b, literal_by_core_.size()));
    return result;
  }

  // Deletes the given assumption indices.
  void DeleteIndices(const std::vector<int>& indices) {
    DeleteVectorIndices(indices, &info_by_assumption_index_);
  }

  // This is only used in WPM1 to forget all the information related to a given
  // assumption_index.
  void ClearInfo(int assumption_index) {
    CHECK_LE(assumption_index, info_by_assumption_index_.size());
    info_by_assumption_index_[assumption_index].clear();
  }

  // This is only used in WPM1 when a new assumption_index is created.
  void AddInfo(int assumption_index, Literal b) {
    CHECK_GE(assumption_index, info_by_assumption_index_.size());
    info_by_assumption_index_.resize(assumption_index + 1);
    info_by_assumption_index_[assumption_index].push_back(
        LiteralWithCoreIndex(b, literal_by_core_.size()));
  }

 private:
  std::vector<std::vector<LiteralWithCoreIndex>> info_by_assumption_index_;
  std::vector<std::vector<Literal>> literal_by_core_;

  DISALLOW_COPY_AND_ASSIGN(FuMalikSymmetryBreaker);
};

}  // namespace

void MinimizeCoreWithPropagation(TimeLimit* limit, SatSolver* solver,
                                 std::vector<Literal>* core) {
  if (solver->IsModelUnsat()) return;
  absl::btree_set<LiteralIndex> moved_last;
  std::vector<Literal> candidate(core->begin(), core->end());

  solver->Backtrack(0);
  solver->SetAssumptionLevel(0);
  if (!solver->FinishPropagation()) return;
  while (!limit->LimitReached()) {
    // We want each literal in candidate to appear last once in our propagation
    // order. We want to do that while maximizing the reutilization of the
    // current assignment prefix, that is minimizing the number of
    // decision/progagation we need to perform.
    const int target_level = MoveOneUnprocessedLiteralLast(
        moved_last, solver->CurrentDecisionLevel(), &candidate);
    if (target_level == -1) break;
    solver->Backtrack(target_level);
    while (!solver->IsModelUnsat() && !limit->LimitReached() &&
           solver->CurrentDecisionLevel() < candidate.size()) {
      const Literal decision = candidate[solver->CurrentDecisionLevel()];
      if (solver->Assignment().LiteralIsTrue(decision)) {
        candidate.erase(candidate.begin() + solver->CurrentDecisionLevel());
        continue;
      } else if (solver->Assignment().LiteralIsFalse(decision)) {
        // This is a "weird" API to get the subset of decisions that caused
        // this literal to be false with reason analysis.
        solver->EnqueueDecisionAndBacktrackOnConflict(decision);
        candidate = solver->GetLastIncompatibleDecisions();
        break;
      } else {
        solver->EnqueueDecisionAndBackjumpOnConflict(decision);
      }
    }
    if (candidate.empty() || solver->IsModelUnsat()) return;
    moved_last.insert(candidate.back().Index());
  }

  solver->Backtrack(0);
  solver->SetAssumptionLevel(0);
  if (candidate.size() < core->size()) {
    VLOG(1) << "minimization " << core->size() << " -> " << candidate.size();

    // We want to preserve the order of literal in the response.
    absl::flat_hash_set<LiteralIndex> set;
    for (const Literal l : candidate) set.insert(l.Index());
    int new_size = 0;
    for (const Literal l : *core) {
      if (set.contains(l.Index())) {
        (*core)[new_size++] = l;
      }
    }
    core->resize(new_size);
  }
}

// This algorithm works by exploiting the unsat core returned by the SAT solver
// when the problem is UNSAT. It starts by trying to solve the decision problem
// where all the objective variables are set to their value with minimal cost,
// and relax in each step some of these fixed variables until the problem
// becomes satisfiable.
SatSolver::Status SolveWithFuMalik(LogBehavior log,
                                   const LinearBooleanProblem& problem,
                                   SatSolver* solver,
                                   std::vector<bool>* solution) {
  Logger logger(log);
  FuMalikSymmetryBreaker symmetry;

  // blocking_clauses will contains a set of clauses that are currently added to
  // the initial problem.
  //
  // Initially, each clause just contains a literal associated to an objective
  // variable with non-zero cost. Setting all these literals to true will lead
  // to the lowest possible objective.
  //
  // During the algorithm, "blocking" literals will be added to each clause.
  // Moreover each clause will contain an extra "assumption" literal stored in
  // the separate assumptions vector (in its negated form).
  //
  // The meaning of a given clause will always be:
  // If the assumption literal and all blocking literals are false, then the
  // "objective" literal (which is the first one in the clause) must be true.
  // When the "objective" literal is true, its variable (which have a non-zero
  // cost) is set to the value that minimize the objective cost.
  //
  // ex: If a variable "x" as a cost of 3, its cost contribution is smaller when
  // it is set to false (since it will contribute to zero instead of 3).
  std::vector<std::vector<Literal>> blocking_clauses;
  std::vector<Literal> assumptions;

  // Initialize blocking_clauses and assumptions.
  const LinearObjective& objective = problem.objective();
  CHECK_GT(objective.coefficients_size(), 0);
  const Coefficient unique_objective_coeff(std::abs(objective.coefficients(0)));
  for (int i = 0; i < objective.literals_size(); ++i) {
    CHECK_EQ(std::abs(objective.coefficients(i)), unique_objective_coeff)
        << "The basic Fu & Malik algorithm needs constant objective coeffs.";
    const Literal literal(objective.literals(i));

    // We want to minimize the cost when this literal is true.
    const Literal min_literal =
        objective.coefficients(i) > 0 ? literal.Negated() : literal;
    blocking_clauses.push_back(std::vector<Literal>(1, min_literal));

    // Note that initialy, we do not create any extra variables.
    assumptions.push_back(min_literal);
  }

  // Print the number of variable with a non-zero cost.
  logger.Log(absl::StrFormat("c #weights:%u #vars:%d #constraints:%d",
                             assumptions.size(), problem.num_variables(),
                             problem.constraints_size()));

  // Starts the algorithm. Each loop will solve the problem under the given
  // assumptions, and if unsat, will relax exactly one of the objective
  // variables (from the unsat core) to be in its "costly" state. When the
  // algorithm terminates, the number of iterations is exactly the minimal
  // objective value.
  for (int iter = 0;; ++iter) {
    const SatSolver::Status result =
        solver->ResetAndSolveWithGivenAssumptions(assumptions);
    if (result == SatSolver::FEASIBLE) {
      ExtractAssignment(problem, *solver, solution);
      Coefficient objective = ComputeObjectiveValue(problem, *solution);
      logger.Log(CnfObjectiveLine(problem, objective));
      return SatSolver::FEASIBLE;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;

    // The interesting case: we have an unsat core.
    //
    // We need to add new "blocking" variables b_i for all the objective
    // variable appearing in the core. Moreover, we will only relax as little
    // as possible (to not miss the optimal), so we will enforce that the sum
    // of the b_i is exactly one.
    std::vector<Literal> core = solver->GetLastIncompatibleDecisions();
    MinimizeCore(solver, &core);
    solver->Backtrack(0);

    // Print the search progress.
    logger.Log(absl::StrFormat("c iter:%d core:%u", iter, core.size()));

    // Special case for a singleton core.
    if (core.size() == 1) {
      // Find the index of the "objective" variable that need to be fixed in
      // its "costly" state.
      const int index =
          std::find(assumptions.begin(), assumptions.end(), core[0]) -
          assumptions.begin();
      CHECK_LT(index, assumptions.size());

      // Fix it. We also fix all the associated blocking variables if any.
      if (!solver->AddUnitClause(core[0].Negated())) {
        return SatSolver::INFEASIBLE;
      }
      for (Literal b : blocking_clauses[index]) {
        if (!solver->AddUnitClause(b.Negated())) return SatSolver::INFEASIBLE;
      }

      // Erase this entry from the current "objective"
      std::vector<int> to_delete(1, index);
      DeleteVectorIndices(to_delete, &assumptions);
      DeleteVectorIndices(to_delete, &blocking_clauses);
      symmetry.DeleteIndices(to_delete);
    } else {
      symmetry.StartResolvingNewCore(iter);

      // We will add 2 * |core.size()| variables.
      const int old_num_variables = solver->NumVariables();
      if (core.size() == 2) {
        // Special case. If core.size() == 2, we can use only one blocking
        // variable (the other one beeing its negation). This actually do happen
        // quite often in practice, so it is worth it.
        solver->SetNumVariables(old_num_variables + 3);
      } else {
        solver->SetNumVariables(old_num_variables + 2 * core.size());
      }

      // Temporary vectors for the constraint (sum new blocking variable == 1).
      std::vector<LiteralWithCoeff> at_most_one_constraint;
      std::vector<Literal> at_least_one_constraint;

      // This will be set to false if the problem becomes unsat while adding a
      // new clause. This is unlikely, but may be possible.
      bool ok = true;

      // Loop over the core.
      int index = 0;
      for (int i = 0; i < core.size(); ++i) {
        // Since the assumptions appear in order in the core, we can find the
        // relevant "objective" variable efficiently with a simple linear scan
        // in the assumptions vector (done with index).
        index =
            std::find(assumptions.begin() + index, assumptions.end(), core[i]) -
            assumptions.begin();
        CHECK_LT(index, assumptions.size());

        // The new blocking and assumption variables for this core entry.
        const Literal a(BooleanVariable(old_num_variables + i), true);
        Literal b(BooleanVariable(old_num_variables + core.size() + i), true);
        if (core.size() == 2) {
          b = Literal(BooleanVariable(old_num_variables + 2), true);
          if (i == 1) b = b.Negated();
        }

        // Symmetry breaking clauses.
        for (Literal l : symmetry.ProcessLiteral(index, b)) {
          ok &= solver->AddBinaryClause(l.Negated(), b.Negated());
        }

        // Note(user): There is more than one way to encode the algorithm in
        // SAT. Here we "delete" the old blocking clause and add a new one. In
        // the WPM1 algorithm below, the blocking clause is decomposed into
        // 3-SAT and we don't need to delete anything.

        // First, fix the old "assumption" variable to false, which has the
        // effect of deleting the old clause from the solver.
        if (assumptions[index].Variable() >= problem.num_variables()) {
          CHECK(solver->AddUnitClause(assumptions[index].Negated()));
        }

        // Add the new blocking variable.
        blocking_clauses[index].push_back(b);

        // Add the new clause to the solver. Temporary including the
        // assumption, but removing it right afterwards.
        blocking_clauses[index].push_back(a);
        ok &= solver->AddProblemClause(blocking_clauses[index]);
        blocking_clauses[index].pop_back();

        // For the "== 1" constraint on the blocking literals.
        at_most_one_constraint.push_back(LiteralWithCoeff(b, 1.0));
        at_least_one_constraint.push_back(b);

        // The new assumption variable replace the old one.
        assumptions[index] = a.Negated();
      }

      // Add the "<= 1" side of the "== 1" constraint.
      ok &= solver->AddLinearConstraint(false, Coefficient(0), true,
                                        Coefficient(1.0),
                                        &at_most_one_constraint);

      // TODO(user): The algorithm does not really need the >= 1 side of this
      // constraint. Initial investigation shows that it doesn't really help,
      // but investigate more.
      if (/* DISABLES CODE */ (false)) {
        ok &= solver->AddProblemClause(at_least_one_constraint);
      }

      if (!ok) {
        LOG(INFO) << "Infeasible while adding a clause.";
        return SatSolver::INFEASIBLE;
      }
    }
  }
}

SatSolver::Status SolveWithWPM1(LogBehavior log,
                                const LinearBooleanProblem& problem,
                                SatSolver* solver,
                                std::vector<bool>* solution) {
  Logger logger(log);
  FuMalikSymmetryBreaker symmetry;

  // The current lower_bound on the cost.
  // It will be correct after the initialization.
  Coefficient lower_bound(static_cast<int64_t>(problem.objective().offset()));
  Coefficient upper_bound(std::numeric_limits<int64_t>::max());

  // The assumption literals and their associated cost.
  std::vector<Literal> assumptions;
  std::vector<Coefficient> costs;
  std::vector<Literal> reference;

  // Initialization.
  const LinearObjective& objective = problem.objective();
  CHECK_GT(objective.coefficients_size(), 0);
  for (int i = 0; i < objective.literals_size(); ++i) {
    const Literal literal(objective.literals(i));
    const Coefficient coeff(objective.coefficients(i));

    // We want to minimize the cost when the assumption is true.
    // Note that initially, we do not create any extra variables.
    if (coeff > 0) {
      assumptions.push_back(literal.Negated());
      costs.push_back(coeff);
    } else {
      assumptions.push_back(literal);
      costs.push_back(-coeff);
      lower_bound += coeff;
    }
  }
  reference = assumptions;

  // This is used by the "stratified" approach.
  Coefficient stratified_lower_bound =
      *std::max_element(costs.begin(), costs.end());

  // Print the number of variables with a non-zero cost.
  logger.Log(absl::StrFormat("c #weights:%u #vars:%d #constraints:%d",
                             assumptions.size(), problem.num_variables(),
                             problem.constraints_size()));

  for (int iter = 0;; ++iter) {
    // This is called "hardening" in the literature.
    // Basically, we know that there is only hardening_threshold weight left
    // to distribute, so any assumption with a greater cost than this can never
    // be false. We fix it instead of treating it as an assumption.
    solver->Backtrack(0);
    const Coefficient hardening_threshold = upper_bound - lower_bound;
    CHECK_GE(hardening_threshold, 0);
    std::vector<int> to_delete;
    int num_above_threshold = 0;
    for (int i = 0; i < assumptions.size(); ++i) {
      if (costs[i] > hardening_threshold) {
        if (!solver->AddUnitClause(assumptions[i])) {
          return SatSolver::INFEASIBLE;
        }
        to_delete.push_back(i);
        ++num_above_threshold;
      } else {
        // This impact the stratification heuristic.
        if (solver->Assignment().LiteralIsTrue(assumptions[i])) {
          to_delete.push_back(i);
        }
      }
    }
    if (!to_delete.empty()) {
      logger.Log(absl::StrFormat("c fixed %u assumptions, %d with cost > %d",
                                 to_delete.size(), num_above_threshold,
                                 hardening_threshold.value()));
      DeleteVectorIndices(to_delete, &assumptions);
      DeleteVectorIndices(to_delete, &costs);
      DeleteVectorIndices(to_delete, &reference);
      symmetry.DeleteIndices(to_delete);
    }

    // This is the "stratification" part.
    // Extract the assumptions with a cost >= stratified_lower_bound.
    std::vector<Literal> assumptions_subset;
    for (int i = 0; i < assumptions.size(); ++i) {
      if (costs[i] >= stratified_lower_bound) {
        assumptions_subset.push_back(assumptions[i]);
      }
    }

    const SatSolver::Status result =
        solver->ResetAndSolveWithGivenAssumptions(assumptions_subset);
    if (result == SatSolver::FEASIBLE) {
      // If not all assumptions were taken, continue with a lower stratified
      // bound. Otherwise we have an optimal solution!
      //
      // TODO(user): Try more advanced variant where the bound is lowered by
      // more than this minimal amount.
      const Coefficient old_lower_bound = stratified_lower_bound;
      for (Coefficient cost : costs) {
        if (cost < old_lower_bound) {
          if (stratified_lower_bound == old_lower_bound ||
              cost > stratified_lower_bound) {
            stratified_lower_bound = cost;
          }
        }
      }

      ExtractAssignment(problem, *solver, solution);
      DCHECK(IsAssignmentValid(problem, *solution));
      const Coefficient objective_offset(
          static_cast<int64_t>(problem.objective().offset()));
      const Coefficient objective = ComputeObjectiveValue(problem, *solution);
      if (objective + objective_offset < upper_bound) {
        logger.Log(CnfObjectiveLine(problem, objective));
        upper_bound = objective + objective_offset;
      }

      if (stratified_lower_bound < old_lower_bound) continue;
      return SatSolver::FEASIBLE;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;

    // The interesting case: we have an unsat core.
    //
    // We need to add new "blocking" variables b_i for all the objective
    // variables appearing in the core. Moreover, we will only relax as little
    // as possible (to not miss the optimal), so we will enforce that the sum
    // of the b_i is exactly one.
    std::vector<Literal> core = solver->GetLastIncompatibleDecisions();
    MinimizeCore(solver, &core);
    solver->Backtrack(0);

    // Compute the min cost of all the assertions in the core.
    // The lower bound will be updated by that much.
    Coefficient min_cost = kCoefficientMax;
    {
      int index = 0;
      for (int i = 0; i < core.size(); ++i) {
        index =
            std::find(assumptions.begin() + index, assumptions.end(), core[i]) -
            assumptions.begin();
        CHECK_LT(index, assumptions.size());
        min_cost = std::min(min_cost, costs[index]);
      }
    }
    lower_bound += min_cost;

    // Print the search progress.
    logger.Log(absl::StrFormat(
        "c iter:%d core:%u lb:%d min_cost:%d strat:%d", iter, core.size(),
        lower_bound.value(), min_cost.value(), stratified_lower_bound.value()));

    // This simple line helps a lot on the packup-wpms instances!
    //
    // TODO(user): That was because of a bug before in the way
    // stratified_lower_bound was decremented, not sure it helps that much now.
    if (min_cost > stratified_lower_bound) {
      stratified_lower_bound = min_cost;
    }

    // Special case for a singleton core.
    if (core.size() == 1) {
      // Find the index of the "objective" variable that need to be fixed in
      // its "costly" state.
      const int index =
          std::find(assumptions.begin(), assumptions.end(), core[0]) -
          assumptions.begin();
      CHECK_LT(index, assumptions.size());

      // Fix it.
      if (!solver->AddUnitClause(core[0].Negated())) {
        return SatSolver::INFEASIBLE;
      }

      // Erase this entry from the current "objective".
      std::vector<int> to_delete(1, index);
      DeleteVectorIndices(to_delete, &assumptions);
      DeleteVectorIndices(to_delete, &costs);
      DeleteVectorIndices(to_delete, &reference);
      symmetry.DeleteIndices(to_delete);
    } else {
      symmetry.StartResolvingNewCore(iter);

      // We will add 2 * |core.size()| variables.
      const int old_num_variables = solver->NumVariables();
      if (core.size() == 2) {
        // Special case. If core.size() == 2, we can use only one blocking
        // variable (the other one beeing its negation). This actually do happen
        // quite often in practice, so it is worth it.
        solver->SetNumVariables(old_num_variables + 3);
      } else {
        solver->SetNumVariables(old_num_variables + 2 * core.size());
      }

      // Temporary vectors for the constraint (sum new blocking variable == 1).
      std::vector<LiteralWithCoeff> at_most_one_constraint;
      std::vector<Literal> at_least_one_constraint;

      // This will be set to false if the problem becomes unsat while adding a
      // new clause. This is unlikely, but may be possible.
      bool ok = true;

      // Loop over the core.
      int index = 0;
      for (int i = 0; i < core.size(); ++i) {
        // Since the assumptions appear in order in the core, we can find the
        // relevant "objective" variable efficiently with a simple linear scan
        // in the assumptions vector (done with index).
        index =
            std::find(assumptions.begin() + index, assumptions.end(), core[i]) -
            assumptions.begin();
        CHECK_LT(index, assumptions.size());

        // The new blocking and assumption variables for this core entry.
        const Literal a(BooleanVariable(old_num_variables + i), true);
        Literal b(BooleanVariable(old_num_variables + core.size() + i), true);
        if (core.size() == 2) {
          b = Literal(BooleanVariable(old_num_variables + 2), true);
          if (i == 1) b = b.Negated();
        }

        // a false & b false => previous assumptions (which was false).
        const Literal old_a = assumptions[index];
        ok &= solver->AddTernaryClause(a, b, old_a);

        // Optional. Also add the two implications a => x and b => x where x is
        // the negation of the previous assumption variable.
        ok &= solver->AddBinaryClause(a.Negated(), old_a.Negated());
        ok &= solver->AddBinaryClause(b.Negated(), old_a.Negated());

        // Optional. Also add the implication a => not(b).
        ok &= solver->AddBinaryClause(a.Negated(), b.Negated());

        // This is the difference with the Fu & Malik algorithm.
        // If the soft clause protected by old_a has a cost greater than
        // min_cost then:
        // - its cost is disminished by min_cost.
        // - an identical clause with cost min_cost is artificially added to
        //   the problem.
        CHECK_GE(costs[index], min_cost);
        if (costs[index] == min_cost) {
          // The new assumption variable replaces the old one.
          assumptions[index] = a.Negated();

          // Symmetry breaking clauses.
          for (Literal l : symmetry.ProcessLiteral(index, b)) {
            ok &= solver->AddBinaryClause(l.Negated(), b.Negated());
          }
        } else {
          // Since the cost of the given index changes, we need to start a new
          // "equivalence" class for the symmetry breaking algo and clear the
          // old one.
          symmetry.AddInfo(assumptions.size(), b);
          symmetry.ClearInfo(index);

          // Reduce the cost of the old assumption.
          costs[index] -= min_cost;

          // We add the new assumption with a cost of min_cost.
          //
          // Note(user): I think it is nice that these are added after old_a
          // because assuming old_a will implies all the derived assumptions to
          // true, and thus they will never appear in a core until old_a is not
          // an assumption anymore.
          assumptions.push_back(a.Negated());
          costs.push_back(min_cost);
          reference.push_back(reference[index]);
        }

        // For the "<= 1" constraint on the blocking literals.
        // Note(user): we don't add the ">= 1" side because it is not needed for
        // the correctness and it doesn't seems to help.
        at_most_one_constraint.push_back(LiteralWithCoeff(b, 1.0));

        // Because we have a core, we know that at least one of the initial
        // problem variables must be true. This seems to help a bit.
        //
        // TODO(user): Experiment more.
        at_least_one_constraint.push_back(reference[index].Negated());
      }

      // Add the "<= 1" side of the "== 1" constraint.
      ok &= solver->AddLinearConstraint(false, Coefficient(0), true,
                                        Coefficient(1.0),
                                        &at_most_one_constraint);

      // Optional. Add the ">= 1" constraint on the initial problem variables.
      ok &= solver->AddProblemClause(at_least_one_constraint);

      if (!ok) {
        LOG(INFO) << "Unsat while adding a clause.";
        return SatSolver::INFEASIBLE;
      }
    }
  }
}

SatSolver::Status SolveWithRandomParameters(
    LogBehavior log, const LinearBooleanProblem& problem, int num_times,
    absl::BitGenRef random, SatSolver* solver, std::vector<bool>* solution) {
  Logger logger(log);
  const SatParameters initial_parameters = solver->parameters();

  SatParameters parameters = initial_parameters;
  TimeLimit time_limit(parameters.max_time_in_seconds());

  // We start with a low conflict limit and increase it until we are able to
  // solve the problem at least once. After this, the limit stays the same.
  int max_number_of_conflicts = 5;
  parameters.set_log_search_progress(false);

  Coefficient min_seen(std::numeric_limits<int64_t>::max());
  Coefficient max_seen(std::numeric_limits<int64_t>::min());
  Coefficient best(min_seen);
  for (int i = 0; i < num_times; ++i) {
    solver->Backtrack(0);
    RandomizeDecisionHeuristic(random, &parameters);

    parameters.set_max_number_of_conflicts(max_number_of_conflicts);
    parameters.set_max_time_in_seconds(time_limit.GetTimeLeft());
    parameters.set_random_seed(i);
    solver->SetParameters(parameters);
    solver->ResetDecisionHeuristic();

    const bool use_obj = absl::Bernoulli(random, 1.0 / 4);
    if (use_obj) UseObjectiveForSatAssignmentPreference(problem, solver);

    const SatSolver::Status result = solver->Solve();
    if (result == SatSolver::INFEASIBLE) {
      // If the problem is INFEASIBLE after we over-constrained the objective,
      // then we found an optimal solution, otherwise, even the decision problem
      // is INFEASIBLE.
      if (best == kCoefficientMax) return SatSolver::INFEASIBLE;
      return SatSolver::FEASIBLE;
    }
    if (result == SatSolver::LIMIT_REACHED) {
      // We augment the number of conflict until we have one feasible solution.
      if (best == kCoefficientMax) ++max_number_of_conflicts;
      if (time_limit.LimitReached()) return SatSolver::LIMIT_REACHED;
      continue;
    }

    CHECK_EQ(result, SatSolver::FEASIBLE);
    std::vector<bool> candidate;
    ExtractAssignment(problem, *solver, &candidate);
    CHECK(IsAssignmentValid(problem, candidate));
    const Coefficient objective = ComputeObjectiveValue(problem, candidate);
    if (objective < best) {
      *solution = candidate;
      best = objective;
      logger.Log(CnfObjectiveLine(problem, objective));

      // Overconstrain the objective.
      solver->Backtrack(0);
      if (!AddObjectiveConstraint(problem, false, Coefficient(0), true,
                                  objective - 1, solver)) {
        return SatSolver::FEASIBLE;
      }
    }
    min_seen = std::min(min_seen, objective);
    max_seen = std::max(max_seen, objective);

    logger.Log(absl::StrCat(
        "c ", objective.value(), " [", min_seen.value(), ", ", max_seen.value(),
        "] objective_preference: ", use_obj ? "true" : "false", " ",
        ProtobufShortDebugString(parameters)));
  }

  // Retore the initial parameter (with an updated time limit).
  parameters = initial_parameters;
  parameters.set_max_time_in_seconds(time_limit.GetTimeLeft());
  solver->SetParameters(parameters);
  return SatSolver::LIMIT_REACHED;
}

SatSolver::Status SolveWithLinearScan(LogBehavior log,
                                      const LinearBooleanProblem& problem,
                                      SatSolver* solver,
                                      std::vector<bool>* solution) {
  Logger logger(log);

  // This has a big positive impact on most problems.
  UseObjectiveForSatAssignmentPreference(problem, solver);

  Coefficient objective = kCoefficientMax;
  if (!solution->empty()) {
    CHECK(IsAssignmentValid(problem, *solution));
    objective = ComputeObjectiveValue(problem, *solution);
  }
  while (true) {
    if (objective != kCoefficientMax) {
      // Over constrain the objective.
      solver->Backtrack(0);
      if (!AddObjectiveConstraint(problem, false, Coefficient(0), true,
                                  objective - 1, solver)) {
        return SatSolver::FEASIBLE;
      }
    }

    // Solve the problem.
    const SatSolver::Status result = solver->Solve();
    CHECK_NE(result, SatSolver::ASSUMPTIONS_UNSAT);
    if (result == SatSolver::INFEASIBLE) {
      if (objective == kCoefficientMax) return SatSolver::INFEASIBLE;
      return SatSolver::FEASIBLE;
    }
    if (result == SatSolver::LIMIT_REACHED) {
      return SatSolver::LIMIT_REACHED;
    }

    // Extract the new best solution.
    CHECK_EQ(result, SatSolver::FEASIBLE);
    ExtractAssignment(problem, *solver, solution);
    CHECK(IsAssignmentValid(problem, *solution));
    const Coefficient old_objective = objective;
    objective = ComputeObjectiveValue(problem, *solution);
    CHECK_LT(objective, old_objective);
    logger.Log(CnfObjectiveLine(problem, objective));
  }
}

SatSolver::Status SolveWithCardinalityEncoding(
    LogBehavior log, const LinearBooleanProblem& problem, SatSolver* solver,
    std::vector<bool>* solution) {
  Logger logger(log);
  std::deque<EncodingNode> repository;

  // Create one initial node per variables with cost.
  Coefficient offset(0);
  std::vector<EncodingNode*> nodes =
      CreateInitialEncodingNodes(problem.objective(), &offset, &repository);

  // This algorithm only work with weights of the same magnitude.
  CHECK(!nodes.empty());
  const Coefficient reference = nodes.front()->weight();
  for (const EncodingNode* n : nodes) CHECK_EQ(n->weight(), reference);

  // Initialize the current objective.
  Coefficient objective = kCoefficientMax;
  Coefficient upper_bound = kCoefficientMax;
  if (!solution->empty()) {
    CHECK(IsAssignmentValid(problem, *solution));
    objective = ComputeObjectiveValue(problem, *solution);
    upper_bound = objective + offset;
  }

  // Print the number of variables with a non-zero cost.
  logger.Log(absl::StrFormat("c #weights:%u #vars:%d #constraints:%d",
                             nodes.size(), problem.num_variables(),
                             problem.constraints_size()));

  // Create the sorter network.
  solver->Backtrack(0);
  EncodingNode* root =
      MergeAllNodesWithDeque(upper_bound, nodes, solver, &repository);
  logger.Log(absl::StrFormat("c encoding depth:%d", root->depth()));

  while (true) {
    if (objective != kCoefficientMax) {
      // Over constrain the objective by fixing the variable index - 1 of the
      // root node to 0.
      const int index = offset.value() + objective.value();
      if (index == 0) return SatSolver::FEASIBLE;
      solver->Backtrack(0);
      if (!solver->AddUnitClause(root->literal(index - 1).Negated())) {
        return SatSolver::FEASIBLE;
      }
    }

    // Solve the problem.
    const SatSolver::Status result = solver->Solve();
    CHECK_NE(result, SatSolver::ASSUMPTIONS_UNSAT);
    if (result == SatSolver::INFEASIBLE) {
      if (objective == kCoefficientMax) return SatSolver::INFEASIBLE;
      return SatSolver::FEASIBLE;
    }
    if (result == SatSolver::LIMIT_REACHED) return SatSolver::LIMIT_REACHED;

    // Extract the new best solution.
    CHECK_EQ(result, SatSolver::FEASIBLE);
    ExtractAssignment(problem, *solver, solution);
    CHECK(IsAssignmentValid(problem, *solution));
    const Coefficient old_objective = objective;
    objective = ComputeObjectiveValue(problem, *solution);
    CHECK_LT(objective, old_objective);
    logger.Log(CnfObjectiveLine(problem, objective));
  }
}

SatSolver::Status SolveWithCardinalityEncodingAndCore(
    LogBehavior log, const LinearBooleanProblem& problem, SatSolver* solver,
    std::vector<bool>* solution) {
  Logger logger(log);
  SatParameters parameters = solver->parameters();

  // Create one initial nodes per variables with cost.
  Coefficient offset(0);
  std::deque<EncodingNode> repository;
  std::vector<EncodingNode*> nodes =
      CreateInitialEncodingNodes(problem.objective(), &offset, &repository);

  // Initialize the bounds.
  // This is in term of number of variables not at their minimal value.
  Coefficient lower_bound(0);
  Coefficient upper_bound(std::numeric_limits<int64_t>::max());
  if (!solution->empty()) {
    CHECK(IsAssignmentValid(problem, *solution));
    upper_bound = ComputeObjectiveValue(problem, *solution) + offset;
  }

  // Print the number of variables with a non-zero cost.
  logger.Log(absl::StrFormat("c #weights:%u #vars:%d #constraints:%d",
                             nodes.size(), problem.num_variables(),
                             problem.constraints_size()));

  // This is used by the "stratified" approach.
  Coefficient stratified_lower_bound(0);
  if (parameters.max_sat_stratification() ==
      SatParameters::STRATIFICATION_DESCENT) {
    // In this case, we initialize it to the maximum assumption weights.
    for (EncodingNode* n : nodes) {
      stratified_lower_bound = std::max(stratified_lower_bound, n->weight());
    }
  }

  // Start the algorithm.
  int max_depth = 0;
  std::string previous_core_info = "";
  for (int iter = 0;; ++iter) {
    // TODO(user): We are suboptimal here because we use for upper bound the
    // current best objective, not best_obj - 1. This code is not really used
    // but we should still fix it.
    const std::vector<Literal> assumptions = ReduceNodesAndExtractAssumptions(
        upper_bound, stratified_lower_bound, &lower_bound, &nodes, solver);
    if (assumptions.empty()) return SatSolver::FEASIBLE;

    // Display the progress.
    const std::string gap_string =
        (upper_bound == kCoefficientMax)
            ? ""
            : absl::StrFormat(" gap:%d", (upper_bound - lower_bound).value());
    logger.Log(
        absl::StrFormat("c iter:%d [%s] lb:%d%s assumptions:%u depth:%d", iter,
                        previous_core_info,
                        lower_bound.value() - offset.value() +
                            static_cast<int64_t>(problem.objective().offset()),
                        gap_string, nodes.size(), max_depth));

    // Solve under the assumptions.
    const SatSolver::Status result =
        solver->ResetAndSolveWithGivenAssumptions(assumptions);
    if (result == SatSolver::FEASIBLE) {
      // Extract the new solution and save it if it is the best found so far.
      std::vector<bool> temp_solution;
      ExtractAssignment(problem, *solver, &temp_solution);
      CHECK(IsAssignmentValid(problem, temp_solution));
      const Coefficient obj = ComputeObjectiveValue(problem, temp_solution);
      if (obj + offset < upper_bound) {
        *solution = temp_solution;
        logger.Log(CnfObjectiveLine(problem, obj));
        upper_bound = obj + offset;
      }

      // If not all assumptions were taken, continue with a lower stratified
      // bound. Otherwise we have an optimal solution.
      stratified_lower_bound =
          MaxNodeWeightSmallerThan(nodes, stratified_lower_bound);
      if (stratified_lower_bound > 0) continue;
      return SatSolver::FEASIBLE;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;

    // We have a new core.
    std::vector<Literal> core = solver->GetLastIncompatibleDecisions();
    if (parameters.minimize_core()) MinimizeCore(solver, &core);

    // Compute the min weight of all the nodes in the core.
    // The lower bound will be increased by that much.
    const Coefficient min_weight = ComputeCoreMinWeight(nodes, core);
    previous_core_info =
        absl::StrFormat("core:%u mw:%d", core.size(), min_weight.value());

    // Increase stratified_lower_bound according to the parameters.
    if (stratified_lower_bound < min_weight &&
        parameters.max_sat_stratification() ==
            SatParameters::STRATIFICATION_ASCENT) {
      stratified_lower_bound = min_weight;
    }

    ProcessCore(core, min_weight, &repository, &nodes, solver);
    max_depth = std::max(max_depth, nodes.back()->depth());
  }
}

SatSolver::Status MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
    IntegerVariable objective_var,
    const std::function<void()>& feasible_solution_observer, Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());

  // Simple linear scan algorithm to find the optimal.
  if (!sat_solver->ResetToLevelZero()) return SatSolver::INFEASIBLE;
  while (true) {
    const SatSolver::Status result = SolveIntegerProblem(model);
    if (result != SatSolver::FEASIBLE) return result;

    // The objective is the current lower bound of the objective_var.
    const IntegerValue objective = integer_trail->LowerBound(objective_var);

    // We have a solution!
    if (feasible_solution_observer != nullptr) {
      feasible_solution_observer();
    }
    if (parameters.stop_after_first_solution()) {
      return SatSolver::LIMIT_REACHED;
    }

    // Restrict the objective.
    sat_solver->Backtrack(0);
    if (!integer_trail->Enqueue(
            IntegerLiteral::LowerOrEqual(objective_var, objective - 1), {},
            {})) {
      return SatSolver::INFEASIBLE;
    }
  }
}

void RestrictObjectiveDomainWithBinarySearch(
    IntegerVariable objective_var,
    const std::function<void()>& feasible_solution_observer, Model* model) {
  const SatParameters old_params = *model->GetOrCreate<SatParameters>();
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();

  // Set the requested conflict limit.
  {
    SatParameters new_params = old_params;
    new_params.set_max_number_of_conflicts(
        old_params.binary_search_num_conflicts());
    *model->GetOrCreate<SatParameters>() = new_params;
  }

  // The assumption (objective <= value) for values in
  // [unknown_min, unknown_max] reached the conflict limit.
  bool loop = true;
  IntegerValue unknown_min = integer_trail->UpperBound(objective_var);
  IntegerValue unknown_max = integer_trail->LowerBound(objective_var);
  while (loop) {
    sat_solver->Backtrack(0);
    const IntegerValue lb = integer_trail->LowerBound(objective_var);
    const IntegerValue ub = integer_trail->UpperBound(objective_var);
    unknown_min = std::min(unknown_min, ub);
    unknown_max = std::max(unknown_max, lb);

    // We first refine the lower bound and then the upper bound.
    IntegerValue target;
    if (lb < unknown_min) {
      target = lb + (unknown_min - lb) / 2;
    } else if (unknown_max < ub) {
      target = ub - (ub - unknown_max) / 2;
    } else {
      VLOG(1) << "Binary-search, done.";
      break;
    }
    VLOG(1) << "Binary-search, objective: [" << lb << "," << ub << "]"
            << " tried: [" << unknown_min << "," << unknown_max << "]"
            << " target: obj<=" << target;
    SatSolver::Status result;
    if (target < ub) {
      const Literal assumption = integer_encoder->GetOrCreateAssociatedLiteral(
          IntegerLiteral::LowerOrEqual(objective_var, target));
      result = ResetAndSolveIntegerProblem({assumption}, model);
    } else {
      result = ResetAndSolveIntegerProblem({}, model);
    }

    switch (result) {
      case SatSolver::INFEASIBLE: {
        loop = false;
        break;
      }
      case SatSolver::ASSUMPTIONS_UNSAT: {
        // Update the objective lower bound.
        sat_solver->Backtrack(0);
        if (!integer_trail->Enqueue(
                IntegerLiteral::GreaterOrEqual(objective_var, target + 1), {},
                {})) {
          loop = false;
        }
        break;
      }
      case SatSolver::FEASIBLE: {
        // The objective is the current lower bound of the objective_var.
        const IntegerValue objective = integer_trail->LowerBound(objective_var);
        if (feasible_solution_observer != nullptr) {
          feasible_solution_observer();
        }

        // We have a solution, restrict the objective upper bound to only look
        // for better ones now.
        sat_solver->Backtrack(0);
        if (!integer_trail->Enqueue(
                IntegerLiteral::LowerOrEqual(objective_var, objective - 1), {},
                {})) {
          loop = false;
        }
        break;
      }
      case SatSolver::LIMIT_REACHED: {
        unknown_min = std::min(target, unknown_min);
        unknown_max = std::max(target, unknown_max);
        break;
      }
    }
  }

  sat_solver->Backtrack(0);
  *model->GetOrCreate<SatParameters>() = old_params;
}

namespace {

// If the given model is unsat under the given assumptions, returns one or more
// non-overlapping set of assumptions, each set making the problem infeasible on
// its own (the cores).
//
// In presence of weights, we "generalize" the notions of disjoints core using
// the WCE idea describe in "Weight-Aware Core Extraction in SAT-Based MaxSAT
// solving" Jeremias Berg And Matti Jarvisalo.
//
// The returned status can be either:
// - ASSUMPTIONS_UNSAT if the set of returned core perfectly cover the given
//   assumptions, in this case, we don't bother trying to find a SAT solution
//   with no assumptions.
// - FEASIBLE if after finding zero or more core we have a solution.
// - LIMIT_REACHED if we reached the time-limit before one of the two status
//   above could be decided.
//
// TODO(user): There is many way to combine the WCE and stratification
// heuristics. I didn't had time to properly compare the different approach. See
// the WCE papers for some ideas, but there is many more ways to try to find a
// lot of core at once and try to minimize the minimum weight of each of the
// cores.
SatSolver::Status FindCores(std::vector<Literal> assumptions,
                            std::vector<IntegerValue> assumption_weights,
                            IntegerValue stratified_threshold, Model* model,
                            std::vector<std::vector<Literal>>* cores) {
  cores->clear();
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  TimeLimit* limit = model->GetOrCreate<TimeLimit>();
  do {
    if (limit->LimitReached()) return SatSolver::LIMIT_REACHED;

    const SatSolver::Status result =
        ResetAndSolveIntegerProblem(assumptions, model);
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;
    std::vector<Literal> core = sat_solver->GetLastIncompatibleDecisions();
    if (sat_solver->parameters().minimize_core()) {
      MinimizeCoreWithPropagation(limit, sat_solver, &core);
    }
    if (core.size() == 1) {
      if (!sat_solver->AddUnitClause(core[0].Negated())) {
        return SatSolver::INFEASIBLE;
      }
    }
    if (core.empty()) return sat_solver->UnsatStatus();
    cores->push_back(core);
    if (!sat_solver->parameters().find_multiple_cores()) break;

    // Recover the original indices of the assumptions that are part of the
    // core.
    std::vector<int> indices;
    {
      absl::btree_set<Literal> temp(core.begin(), core.end());
      for (int i = 0; i < assumptions.size(); ++i) {
        if (temp.contains(assumptions[i])) {
          indices.push_back(i);
        }
      }
    }

    // Remove min_weight from the weights of all the assumptions in the core.
    //
    // TODO(user): push right away the objective bound by that much? This should
    // be better in a multi-threading context as we can share more quickly the
    // better bound.
    IntegerValue min_weight = assumption_weights[indices.front()];
    for (const int i : indices) {
      min_weight = std::min(min_weight, assumption_weights[i]);
    }
    for (const int i : indices) {
      assumption_weights[i] -= min_weight;
    }

    // Remove from assumptions all the one with a new weight smaller than the
    // current stratification threshold and see if we can find another core.
    int new_size = 0;
    for (int i = 0; i < assumptions.size(); ++i) {
      if (assumption_weights[i] < stratified_threshold) continue;
      assumptions[new_size] = assumptions[i];
      assumption_weights[new_size] = assumption_weights[i];
      ++new_size;
    }
    assumptions.resize(new_size);
    assumption_weights.resize(new_size);
  } while (!assumptions.empty());
  return SatSolver::ASSUMPTIONS_UNSAT;
}

}  // namespace

CoreBasedOptimizer::CoreBasedOptimizer(
    IntegerVariable objective_var,
    const std::vector<IntegerVariable>& variables,
    const std::vector<IntegerValue>& coefficients,
    std::function<void()> feasible_solution_observer, Model* model)
    : parameters_(model->GetOrCreate<SatParameters>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      implications_(model->GetOrCreate<BinaryImplicationGraph>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      model_(model),
      objective_var_(objective_var),
      feasible_solution_observer_(std::move(feasible_solution_observer)) {
  CHECK_EQ(variables.size(), coefficients.size());
  for (int i = 0; i < variables.size(); ++i) {
    if (coefficients[i] > 0) {
      terms_.push_back({variables[i], coefficients[i]});
    } else if (coefficients[i] < 0) {
      terms_.push_back({NegationOf(variables[i]), -coefficients[i]});
    } else {
      continue;  // coefficients[i] == 0
    }
    terms_.back().depth = 0;
  }

  // This is used by the "stratified" approach. We will only consider terms with
  // a weight not lower than this threshold. The threshold will decrease as the
  // algorithm progress.
  stratification_threshold_ = parameters_->max_sat_stratification() ==
                                      SatParameters::STRATIFICATION_NONE
                                  ? IntegerValue(1)
                                  : kMaxIntegerValue;
}

bool CoreBasedOptimizer::ProcessSolution() {
  // We don't assume that objective_var is linked with its linear term, so
  // we recompute the objective here.
  IntegerValue objective(0);
  for (ObjectiveTerm& term : terms_) {
    const IntegerValue value = integer_trail_->LowerBound(term.var);
    objective += term.weight * value;

    // Also keep in term.cover_ub the minimum value for term.var that we have
    // seens amongst all the feasible solutions found so far.
    term.cover_ub = std::min(term.cover_ub, value);
  }

  // We use the level zero upper bound of the objective to indicate an upper
  // limit for the solution objective we are looking for. Again, because the
  // objective_var is not assumed to be linked, it could take any value in the
  // current solution.
  if (objective > integer_trail_->LevelZeroUpperBound(objective_var_)) {
    return true;
  }

  if (feasible_solution_observer_ != nullptr) {
    feasible_solution_observer_();
  }
  if (parameters_->stop_after_first_solution()) {
    stop_ = true;
  }

  // Constrain objective_var. This has a better result when objective_var is
  // used in an LP relaxation for instance.
  sat_solver_->Backtrack(0);
  sat_solver_->SetAssumptionLevel(0);
  return integer_trail_->Enqueue(
      IntegerLiteral::LowerOrEqual(objective_var_, objective - 1), {}, {});
}

bool CoreBasedOptimizer::PropagateObjectiveBounds() {
  // We assumes all terms (modulo stratification) at their lower-bound.
  bool some_bound_were_tightened = true;
  while (some_bound_were_tightened) {
    some_bound_were_tightened = false;
    if (!sat_solver_->ResetToLevelZero()) return false;
    if (time_limit_->LimitReached()) return true;

    // Compute implied lb.
    IntegerValue implied_objective_lb(0);
    for (ObjectiveTerm& term : terms_) {
      const IntegerValue var_lb = integer_trail_->LowerBound(term.var);
      term.old_var_lb = var_lb;
      implied_objective_lb += term.weight * var_lb.value();
    }

    // Update the objective lower bound with our current bound.
    if (implied_objective_lb > integer_trail_->LowerBound(objective_var_)) {
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(
                                       objective_var_, implied_objective_lb),
                                   {}, {})) {
        return false;
      }

      some_bound_were_tightened = true;
    }

    // The gap is used to propagate the upper-bound of all variable that are
    // in the current objective (Exactly like done in the propagation of a
    // linear constraint with the slack). When this fix a variable to its
    // lower bound, it is called "hardening" in the max-sat literature. This
    // has a really beneficial effect on some weighted max-sat problems like
    // the haplotyping-pedigrees ones.
    const IntegerValue gap =
        integer_trail_->UpperBound(objective_var_) - implied_objective_lb;

    for (const ObjectiveTerm& term : terms_) {
      if (term.weight == 0) continue;
      const IntegerValue var_lb = integer_trail_->LowerBound(term.var);
      const IntegerValue var_ub = integer_trail_->UpperBound(term.var);
      if (var_lb == var_ub) continue;

      // Hardening. This basically just propagate the implied upper bound on
      // term.var from the current best solution. Note that the gap is
      // non-negative and the weight positive here. The test is done in order
      // to avoid any integer overflow provided (ub - lb) do not overflow, but
      // this is a precondition in our cp-model.
      if (gap / term.weight < var_ub - var_lb) {
        some_bound_were_tightened = true;
        const IntegerValue new_ub = var_lb + gap / term.weight;
        DCHECK_LT(new_ub, var_ub);
        DCHECK(!integer_trail_->IsCurrentlyIgnored(term.var));
        if (!integer_trail_->Enqueue(
                IntegerLiteral::LowerOrEqual(term.var, new_ub), {}, {})) {
          return false;
        }
      }
    }
  }
  return true;
}

// A basic algorithm is to take the next one, or at least the next one
// that invalidate the current solution. But to avoid corner cases for
// problem with a lot of terms all with different objective weights (in
// which case we will kind of introduce only one assumption per loop
// which is little), we use an heuristic and take the 90% percentile of
// the unique weights not yet included.
//
// TODO(user): There is many other possible heuristics here, and I
// didn't have the time to properly compare them.
void CoreBasedOptimizer::ComputeNextStratificationThreshold() {
  std::vector<IntegerValue> weights;
  for (ObjectiveTerm& term : terms_) {
    if (term.weight >= stratification_threshold_) continue;
    if (term.weight == 0) continue;

    const IntegerValue var_lb = integer_trail_->LevelZeroLowerBound(term.var);
    const IntegerValue var_ub = integer_trail_->LevelZeroUpperBound(term.var);
    if (var_lb == var_ub) continue;

    weights.push_back(term.weight);
  }
  if (weights.empty()) {
    stratification_threshold_ = IntegerValue(0);
    return;
  }

  gtl::STLSortAndRemoveDuplicates(&weights);
  stratification_threshold_ =
      weights[static_cast<int>(std::floor(0.9 * weights.size()))];
}

bool CoreBasedOptimizer::CoverOptimization() {
  if (!sat_solver_->ResetToLevelZero()) return false;

  // We set a fix deterministic time limit per all sub-solve and skip to the
  // next core if the sum of the subsolve is also over this limit.
  constexpr double max_dtime_per_core = 0.5;
  const double old_time_limit = parameters_->max_deterministic_time();
  parameters_->set_max_deterministic_time(max_dtime_per_core);
  auto cleanup = ::absl::MakeCleanup([old_time_limit, this]() {
    parameters_->set_max_deterministic_time(old_time_limit);
  });

  for (const ObjectiveTerm& term : terms_) {
    // We currently skip the initial objective terms as there could be many
    // of them. TODO(user): provide an option to cover-optimize them? I
    // fear that this will slow down the solver too much though.
    if (term.depth == 0) continue;

    // Find out the true lower bound of var. This is called "cover
    // optimization" in some of the max-SAT literature. It can helps on some
    // problem families and hurt on others, but the overall impact is
    // positive.
    const IntegerVariable var = term.var;
    IntegerValue best =
        std::min(term.cover_ub, integer_trail_->UpperBound(var));

    // Note(user): this can happen in some corner case because each time we
    // find a solution, we constrain the objective to be smaller than it, so
    // it is possible that a previous best is now infeasible.
    if (best <= integer_trail_->LowerBound(var)) continue;

    // Compute the global deterministic time for this core cover
    // optimization.
    const double deterministic_limit =
        time_limit_->GetElapsedDeterministicTime() + max_dtime_per_core;

    // Simple linear scan algorithm to find the optimal of var.
    SatSolver::Status result;
    while (best > integer_trail_->LowerBound(var)) {
      const Literal assumption = integer_encoder_->GetOrCreateAssociatedLiteral(
          IntegerLiteral::LowerOrEqual(var, best - 1));
      result = ResetAndSolveIntegerProblem({assumption}, model_);
      if (result != SatSolver::FEASIBLE) break;

      best = integer_trail_->LowerBound(var);
      VLOG(1) << "cover_opt var:" << var << " domain:["
              << integer_trail_->LevelZeroLowerBound(var) << "," << best << "]";
      if (!ProcessSolution()) return false;
      if (!sat_solver_->ResetToLevelZero()) return false;
      if (stop_ ||
          time_limit_->GetElapsedDeterministicTime() > deterministic_limit) {
        break;
      }
    }
    if (result == SatSolver::INFEASIBLE) return false;
    if (result == SatSolver::ASSUMPTIONS_UNSAT) {
      if (!sat_solver_->ResetToLevelZero()) return false;

      // TODO(user): If we improve the lower bound of var, we should check
      // if our global lower bound reached our current best solution in
      // order to abort early if the optimal is proved.
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(var, best),
                                   {}, {})) {
        return false;
      }
    }
  }

  return PropagateObjectiveBounds();
}

SatSolver::Status CoreBasedOptimizer::OptimizeWithSatEncoding(
    const std::vector<Literal>& literals,
    const std::vector<IntegerVariable>& vars,
    const std::vector<Coefficient>& coefficients, Coefficient offset) {
  // Create one initial nodes per variables with cost.
  // TODO(user): We could create EncodingNode out of IntegerVariable.
  //
  // Note that the nodes order and assumptions extracted from it will be stable.
  // In particular, new nodes will be appended at the end, which make the solver
  // more likely to find core involving only the first assumptions. This is
  // important at the beginning so the solver as a chance to find a lot of
  // non-overlapping small cores without the need to have dedicated
  // non-overlapping core finder.
  // TODO(user): It could still be beneficial to add one. Experiments.
  std::deque<EncodingNode> repository;
  std::vector<EncodingNode*> nodes;
  if (vars.empty()) {
    // All Booleans.
    for (int i = 0; i < literals.size(); ++i) {
      CHECK_GT(coefficients[i], 0);
      repository.emplace_back(literals[i]);
      nodes.push_back(&repository.back());
      nodes.back()->set_weight(coefficients[i]);
    }
  } else {
    // Use integer encoding.
    CHECK_EQ(vars.size(), coefficients.size());
    for (int i = 0; i < vars.size(); ++i) {
      CHECK_GT(coefficients[i], 0);
      const IntegerVariable var = vars[i];
      const IntegerValue var_lb = integer_trail_->LowerBound(var);
      const IntegerValue var_ub = integer_trail_->UpperBound(var);
      if (var_ub - var_lb == 1) {
        const Literal lit = integer_encoder_->GetOrCreateAssociatedLiteral(
            IntegerLiteral::GreaterOrEqual(var, var_ub));
        repository.emplace_back(lit);
      } else {
        // TODO(user): This might not be idea if there are holes in the domain.
        // It should work by adding duplicates literal, but we should be able to
        // be more efficient.
        int lb = 0;
        int ub = static_cast<int>(var_ub.value() - var_lb.value());
        repository.emplace_back(lb, ub, [var, var_lb, this](int x) {
          return integer_encoder_->GetOrCreateAssociatedLiteral(
              IntegerLiteral::GreaterOrEqual(var,
                                             var_lb + IntegerValue(x + 1)));
        });
      }
      nodes.push_back(&repository.back());
      nodes.back()->set_weight(coefficients[i]);
    }
  }

  // Initialize the bounds.
  // This is in term of number of variables not at their minimal value.
  Coefficient lower_bound(0);

  // This is used by the "stratified" approach.
  // TODO(user): Take into account parameters.
  Coefficient stratified_lower_bound(0);
  for (EncodingNode* n : nodes) {
    stratified_lower_bound = std::max(stratified_lower_bound, n->weight());
  }

  // Start the algorithm.
  int max_depth = 0;
  std::string previous_core_info = "";
  for (int iter = 0;;) {
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

    const Coefficient upper_bound(
        integer_trail_->UpperBound(objective_var_).value() - offset.value());
    const std::vector<Literal> assumptions = ReduceNodesAndExtractAssumptions(
        upper_bound, stratified_lower_bound, &lower_bound, &nodes, sat_solver_);
    if (assumptions.empty()) {
      stratified_lower_bound =
          MaxNodeWeightSmallerThan(nodes, stratified_lower_bound);
      if (stratified_lower_bound > 0) continue;

      // We do not have any assumptions anymore, but we still need to see
      // if the problem is feasible or not!
    }
    const IntegerValue new_obj_lb(lower_bound.value() + offset.value());
    if (new_obj_lb > integer_trail_->LowerBound(objective_var_)) {
      if (!integer_trail_->Enqueue(
              IntegerLiteral::GreaterOrEqual(objective_var_, new_obj_lb), {},
              {})) {
        return SatSolver::INFEASIBLE;
      }

      // Report the improvement.
      // Note that we have a callback that will do the same, but doing it
      // earlier allow us to add more information.
      const int num_bools = sat_solver_->NumVariables();
      const int num_fixed = sat_solver_->NumFixedVariables();
      model_->GetOrCreate<SharedResponseManager>()->UpdateInnerObjectiveBounds(
          absl::StrFormat("bool_core num_cores:%d [%s] assumptions:%u "
                          "depth:%d fixed_bools:%d/%d",
                          iter, previous_core_info, nodes.size(), max_depth,
                          num_fixed, num_bools),
          new_obj_lb, integer_trail_->LevelZeroUpperBound(objective_var_));
    }

    // Solve under the assumptions.
    //
    // TODO(user): Find multiple core like in the "main" algorithm.
    // this is just trying to solve with assumptions not involving the newly
    // found core.
    const SatSolver::Status result =
        ResetAndSolveIntegerProblem(assumptions, model_);
    if (result == SatSolver::FEASIBLE) {
      if (!ProcessSolution()) return SatSolver::INFEASIBLE;
      if (stop_) return SatSolver::LIMIT_REACHED;

      // If not all assumptions were taken, continue with a lower stratified
      // bound. Otherwise we have an optimal solution.
      stratified_lower_bound =
          MaxNodeWeightSmallerThan(nodes, stratified_lower_bound);
      if (stratified_lower_bound > 0) continue;
      return SatSolver::INFEASIBLE;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;

    // We have a new core.
    std::vector<Literal> core = sat_solver_->GetLastIncompatibleDecisions();
    if (parameters_->minimize_core()) {
      MinimizeCoreWithPropagation(time_limit_, sat_solver_, &core);
    }

    // Compute the min weight of all the nodes in the core.
    // The lower bound will be increased by that much.
    const Coefficient min_weight = ComputeCoreMinWeight(nodes, core);
    previous_core_info =
        absl::StrFormat("core:%u mw:%d d:%d", core.size(), min_weight.value(),
                        nodes.back()->depth());

    // We only count an iter when we found a core.
    ++iter;
    if (!ProcessCore(core, min_weight, &repository, &nodes, sat_solver_)) {
      return SatSolver::INFEASIBLE;
    }
    max_depth = std::max(max_depth, nodes.back()->depth());
  }

  return SatSolver::FEASIBLE;  // shouldn't reach here.
}

void CoreBasedOptimizer::PresolveObjectiveWithAtMostOne(
    std::vector<Literal>* literals, std::vector<Coefficient>* coefficients,
    Coefficient* offset) {
  // This contains non-negative value. If a literal has negative weight, then
  // we just put a positive weight on its negation and update the offset.
  const int num_literals = implications_->literal_size();
  absl::StrongVector<LiteralIndex, Coefficient> weights(num_literals);
  absl::StrongVector<LiteralIndex, bool> is_candidate(num_literals);

  // For now, we do not use weight. Note that finding the at most on in the
  // creation order of the variable make a HUGE difference on the max-sat frb
  // family.
  //
  // TODO(user): We can assign preferences to literals to favor certain at most
  // one instead of other. For now we don't, so ExpandAtMostOneWithWeight() will
  // kind of randomize the expansion amongst possible choices.
  absl::StrongVector<LiteralIndex, double> preferences;

  // Collect all literals with "negative weights", we will try to find at most
  // one between them.
  std::vector<Literal> candidates;
  const int num_terms = literals->size();
  for (int i = 0; i < num_terms; ++i) {
    const Literal lit = (*literals)[i];
    const Coefficient coeff = (*coefficients)[i];

    // For now we know the input only has positive weight, but it is easy to
    // adapt if needed.
    CHECK_GT(coeff, 0);
    weights[lit.Index()] = coeff;

    candidates.push_back(lit.Negated());
    is_candidate[lit.NegatedIndex()] = true;
  }

  int num_at_most_ones = 0;
  Coefficient overall_lb_increase(0);

  std::vector<Literal> at_most_one;
  std::vector<std::pair<Literal, Coefficient>> new_obj_terms;
  implications_->ResetWorkDone();
  for (const Literal root : candidates) {
    if (weights[root.NegatedIndex()] == 0) continue;
    if (implications_->WorkDone() > 1e8) continue;

    // We never put weight on both a literal and its negation.
    CHECK_EQ(weights[root.Index()], 0);

    // Note that for this to be as exhaustive as possible, the probing needs
    // to have added binary clauses corresponding to lvl0 propagation.
    at_most_one =
        implications_->ExpandAtMostOneWithWeight</*use_weight=*/false>(
            {root}, is_candidate, preferences);
    if (at_most_one.size() <= 1) continue;
    ++num_at_most_ones;

    // Change the objective weights. Note that all the literal in the at most
    // one will not be processed again since the weight of their negation will
    // be zero after this step.
    Coefficient max_coeff(0);
    Coefficient lb_increase(0);
    for (const Literal lit : at_most_one) {
      const Coefficient coeff = weights[lit.NegatedIndex()];
      lb_increase += coeff;
      max_coeff = std::max(max_coeff, coeff);
    }
    lb_increase -= max_coeff;

    *offset += lb_increase;
    overall_lb_increase += lb_increase;

    for (const Literal lit : at_most_one) {
      is_candidate[lit.Index()] = false;
      const Coefficient new_weight = max_coeff - weights[lit.NegatedIndex()];
      CHECK_EQ(weights[lit.Index()], 0);
      weights[lit.Index()] = new_weight;
      weights[lit.NegatedIndex()] = 0;
      if (new_weight > 0) {
        // TODO(user): While we autorize this to be in future at most one, it
        // will not appear in the "literal" list. We might also want to continue
        // until we reached the fix point.
        is_candidate[lit.NegatedIndex()] = true;
      }
    }

    // Create a new Boolean with weight max_coeff.
    const Literal new_lit(sat_solver_->NewBooleanVariable(), true);
    new_obj_terms.push_back({new_lit, max_coeff});

    // The new boolean is true only if all the one in the at most one are false.
    at_most_one.push_back(new_lit);
    sat_solver_->AddProblemClause(at_most_one);
    is_candidate.resize(implications_->literal_size(), false);
    preferences.resize(implications_->literal_size(), 1.0);
  }

  if (overall_lb_increase > 0) {
    // Report new bounds right away with extra information.
    model_->GetOrCreate<SharedResponseManager>()->UpdateInnerObjectiveBounds(
        absl::StrFormat("am1_presolve num_literals:%d num_am1:%d "
                        "increase:%lld work_done:%lld",
                        (int)candidates.size(), num_at_most_ones,
                        overall_lb_increase.value(), implications_->WorkDone()),
        IntegerValue(offset->value()),
        integer_trail_->LevelZeroUpperBound(objective_var_));
  }

  // Reconstruct the objective.
  literals->clear();
  coefficients->clear();
  for (const Literal root : candidates) {
    if (weights[root.Index()] > 0) {
      CHECK_EQ(weights[root.NegatedIndex()], 0);
      literals->push_back(root);
      coefficients->push_back(weights[root.Index()]);
    }
    if (weights[root.NegatedIndex()] > 0) {
      CHECK_EQ(weights[root.Index()], 0);
      literals->push_back(root.Negated());
      coefficients->push_back(weights[root.NegatedIndex()]);
    }
  }
  for (const auto& [lit, coeff] : new_obj_terms) {
    literals->push_back(lit);
    coefficients->push_back(coeff);
  }
}

SatSolver::Status CoreBasedOptimizer::Optimize() {
  // Hack: If the objective is fully Boolean, we use the
  // OptimizeWithSatEncoding() version as it seems to be better.
  //
  // TODO(user): Try to understand exactly why and merge both code path.
  if (!parameters_->interleave_search()) {
    Coefficient offset(0);
    std::vector<Literal> literals;
    std::vector<IntegerVariable> vars;
    std::vector<Coefficient> coefficients;
    bool all_booleans = true;
    IntegerValue range(0);
    for (const ObjectiveTerm& term : terms_) {
      const IntegerVariable var = term.var;
      const IntegerValue coeff = term.weight;
      const IntegerValue lb = integer_trail_->LowerBound(var);
      const IntegerValue ub = integer_trail_->UpperBound(var);
      offset += Coefficient((lb * coeff).value());
      if (lb == ub) continue;

      vars.push_back(var);
      coefficients.push_back(Coefficient(coeff.value()));
      if (ub - lb == 1) {
        literals.push_back(integer_encoder_->GetOrCreateAssociatedLiteral(
            IntegerLiteral::GreaterOrEqual(var, ub)));
      } else {
        all_booleans = false;
        range += ub - lb;
      }
    }
    if (all_booleans) {
      // TODO(user): It might be interesting to redo this kind of presolving
      // once high cost booleans have been fixed as we might have more at most
      // one between literal in the objective by then.
      //
      // Or alternatively, we could try this or something like it on the
      // literals from the cores as they are found. We should probably make
      // sure that if it exist, a core of size two is always added. And for
      // such core, we can always try to see if the "at most one" can be
      // extended.
      PresolveObjectiveWithAtMostOne(&literals, &coefficients, &offset);
      return OptimizeWithSatEncoding(literals, {}, coefficients, offset);
    }
    if (range < 1e8) {
      return OptimizeWithSatEncoding({}, vars, coefficients, offset);
    }
  }

  // TODO(user): The core is returned in the same order as the assumptions,
  // so we don't really need this map, we could just do a linear scan to
  // recover which node are part of the core. This however needs to be properly
  // unit tested before usage.
  absl::btree_map<LiteralIndex, int> literal_to_term_index;

  // Start the algorithm.
  stop_ = false;
  while (true) {
    // TODO(user): This always resets the solver to level zero.
    // Because of that we don't resume a solve in "chunk" perfectly. Fix.
    if (!PropagateObjectiveBounds()) return SatSolver::INFEASIBLE;
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;

    // Bulk cover optimization.
    //
    // TODO(user): If the search is aborted during this phase and we solve in
    // "chunk", we don't resume perfectly from where it was. Fix.
    if (parameters_->cover_optimization()) {
      if (!CoverOptimization()) return SatSolver::INFEASIBLE;
      if (stop_) return SatSolver::LIMIT_REACHED;
    }

    // We assumes all terms (modulo stratification) at their lower-bound.
    std::vector<int> term_indices;
    std::vector<IntegerLiteral> integer_assumptions;
    std::vector<IntegerValue> assumption_weights;
    IntegerValue objective_offset(0);
    bool some_assumptions_were_skipped = false;
    for (int i = 0; i < terms_.size(); ++i) {
      const ObjectiveTerm term = terms_[i];

      // TODO(user): These can be simply removed from the list.
      if (term.weight == 0) continue;

      // Skip fixed terms.
      // We still keep them around for a proper lower-bound computation.
      //
      // TODO(user): we could keep an objective offset instead.
      const IntegerValue var_lb = integer_trail_->LowerBound(term.var);
      const IntegerValue var_ub = integer_trail_->UpperBound(term.var);
      if (var_lb == var_ub) {
        objective_offset += term.weight * var_lb.value();
        continue;
      }

      // Only consider the terms above the threshold.
      if (term.weight >= stratification_threshold_) {
        integer_assumptions.push_back(
            IntegerLiteral::LowerOrEqual(term.var, var_lb));
        assumption_weights.push_back(term.weight);
        term_indices.push_back(i);
      } else {
        some_assumptions_were_skipped = true;
      }
    }

    // No assumptions with the current stratification? use the next one.
    if (term_indices.empty() && some_assumptions_were_skipped) {
      ComputeNextStratificationThreshold();
      continue;
    }

    // If there is only one or two assumptions left, we switch the algorithm.
    if (term_indices.size() <= 2 && !some_assumptions_were_skipped) {
      VLOG(1) << "Switching to linear scan...";
      if (!already_switched_to_linear_scan_) {
        already_switched_to_linear_scan_ = true;
        std::vector<IntegerVariable> constraint_vars;
        std::vector<int64_t> constraint_coeffs;
        for (const int index : term_indices) {
          constraint_vars.push_back(terms_[index].var);
          constraint_coeffs.push_back(terms_[index].weight.value());
        }
        constraint_vars.push_back(objective_var_);
        constraint_coeffs.push_back(-1);
        model_->Add(WeightedSumLowerOrEqual(constraint_vars, constraint_coeffs,
                                            -objective_offset.value()));
      }

      return MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          objective_var_, feasible_solution_observer_, model_);
    }

    // Display the progress.
    if (VLOG_IS_ON(1)) {
      int max_depth = 0;
      for (const ObjectiveTerm& term : terms_) {
        max_depth = std::max(max_depth, term.depth);
      }
      const int64_t lb = integer_trail_->LowerBound(objective_var_).value();
      const int64_t ub = integer_trail_->UpperBound(objective_var_).value();
      const int gap =
          lb == ub
              ? 0
              : static_cast<int>(std::ceil(
                    100.0 * (ub - lb) / std::max(std::abs(ub), std::abs(lb))));
      VLOG(1) << absl::StrCat("unscaled_next_obj_range:[", lb, ",", ub,
                              "]"
                              " gap:",
                              gap, "%", " assumptions:", term_indices.size(),
                              " strat:", stratification_threshold_.value(),
                              " depth:", max_depth,
                              " bool: ", sat_solver_->NumVariables());
    }

    // Convert integer_assumptions to Literals.
    std::vector<Literal> assumptions;
    literal_to_term_index.clear();
    for (int i = 0; i < integer_assumptions.size(); ++i) {
      assumptions.push_back(integer_encoder_->GetOrCreateAssociatedLiteral(
          integer_assumptions[i]));

      // Tricky: In some rare case, it is possible that the same literal
      // correspond to more that one assumptions. In this case, we can just
      // pick one of them when converting back a core to term indices.
      //
      // TODO(user): We can probably be smarter about the cost of the
      // assumptions though.
      literal_to_term_index[assumptions.back().Index()] = term_indices[i];
    }

    // Solve under the assumptions.
    //
    // TODO(user): If the "search" is interrupted while computing cores, we
    // currently do not resume it flawlessly. We however add any cores we found
    // before aborting.
    std::vector<std::vector<Literal>> cores;
    const SatSolver::Status result =
        FindCores(assumptions, assumption_weights, stratification_threshold_,
                  model_, &cores);
    if (result == SatSolver::INFEASIBLE) return SatSolver::INFEASIBLE;
    if (result == SatSolver::FEASIBLE) {
      if (!ProcessSolution()) return SatSolver::INFEASIBLE;
      if (stop_) return SatSolver::LIMIT_REACHED;
      if (cores.empty()) {
        ComputeNextStratificationThreshold();
        if (stratification_threshold_ == 0) return SatSolver::INFEASIBLE;
        continue;
      }
    }

    // Process the cores by creating new variables and transferring the minimum
    // weight of each core to it.
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;
    for (const std::vector<Literal>& core : cores) {
      // This just increase the lower-bound of the corresponding node.
      // TODO(user): Maybe the solver should do it right away.
      if (core.size() == 1) {
        if (!sat_solver_->AddUnitClause(core[0].Negated())) {
          return SatSolver::INFEASIBLE;
        }
        continue;
      }

      // Compute the min weight of all the terms in the core. The lower bound
      // will be increased by that much because at least one assumption in the
      // core must be true. This is also why we can start at 1 for new_var_lb.
      bool ignore_this_core = false;
      IntegerValue min_weight = kMaxIntegerValue;
      IntegerValue max_weight(0);
      IntegerValue new_var_lb(1);
      IntegerValue new_var_ub(0);
      int new_depth = 0;
      for (const Literal lit : core) {
        const int index = literal_to_term_index.at(lit.Index());

        // When this happen, the core is now trivially "minimized" by the new
        // bound on this variable, so there is no point in adding it.
        if (terms_[index].old_var_lb <
            integer_trail_->LowerBound(terms_[index].var)) {
          ignore_this_core = true;
          break;
        }

        const IntegerValue weight = terms_[index].weight;
        min_weight = std::min(min_weight, weight);
        max_weight = std::max(max_weight, weight);
        new_depth = std::max(new_depth, terms_[index].depth + 1);
        new_var_lb += integer_trail_->LowerBound(terms_[index].var);
        new_var_ub += integer_trail_->UpperBound(terms_[index].var);
      }
      if (ignore_this_core) continue;

      VLOG(1) << absl::StrFormat(
          "core:%u weight:[%d,%d] domain:[%d,%d] depth:%d", core.size(),
          min_weight.value(), max_weight.value(), new_var_lb.value(),
          new_var_ub.value(), new_depth);

      // We will "transfer" min_weight from all the variables of the core
      // to a new variable.
      const IntegerVariable new_var =
          integer_trail_->AddIntegerVariable(new_var_lb, new_var_ub);
      terms_.push_back({new_var, min_weight, new_depth});
      terms_.back().cover_ub = new_var_ub;

      // Sum variables in the core <= new_var.
      {
        std::vector<IntegerVariable> constraint_vars;
        std::vector<int64_t> constraint_coeffs;
        for (const Literal lit : core) {
          const int index = literal_to_term_index.at(lit.Index());
          terms_[index].weight -= min_weight;
          constraint_vars.push_back(terms_[index].var);
          constraint_coeffs.push_back(1);
        }
        constraint_vars.push_back(new_var);
        constraint_coeffs.push_back(-1);
        model_->Add(
            WeightedSumLowerOrEqual(constraint_vars, constraint_coeffs, 0));
      }
    }

    // Abort if we reached the time limit. Note that we still add any cores we
    // found in case the solve is split in "chunk".
    if (result == SatSolver::LIMIT_REACHED) return result;
  }
}

}  // namespace sat
}  // namespace operations_research
