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

#include "ortools/sat/optimization.h"

#include <deque>
#include <queue>

#include "ortools/base/stringprintf.h"
#include "google/protobuf/descriptor.h"
#include "ortools/sat/encoding.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/util.h"

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
      printf("%s\n", message.c_str());
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
  return StringPrintf("o %lld", static_cast<int64>(scaled_objective));
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
// artifically introduce symmetries. More precisely:
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

void MinimizeCore(SatSolver* solver, std::vector<Literal>* core) {
  std::vector<Literal> temp = *core;
  std::reverse(temp.begin(), temp.end());
  solver->Backtrack(0);

  // Note that this Solve() is really fast, since the solver should detect that
  // the assumptions are unsat with unit propagation only. This is just a
  // convenient way to remove assumptions that are propagated by the one before
  // them.
  const SatSolver::Status status =
      solver->ResetAndSolveWithGivenAssumptions(temp);
  if (status != SatSolver::ASSUMPTIONS_UNSAT) {
    if (status != SatSolver::LIMIT_REACHED) {
      // This should almost never happen, but it is not impossible. The reason
      // is that the solver may delete some learned clauses required by the unit
      // propagation to show that the core is unsat.
      LOG(WARNING) << "This should only happen rarely! otherwise, investigate. "
                   << "Returned status is " << SatStatusString(status);
    }
    return;
  }
  temp = solver->GetLastIncompatibleDecisions();
  if (temp.size() < core->size()) {
    VLOG(1) << "old core size " << core->size();
    std::reverse(temp.begin(), temp.end());
    *core = temp;
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
  logger.Log(StringPrintf("c #weights:%zu #vars:%d #constraints:%d",
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
    if (result == SatSolver::MODEL_SAT) {
      ExtractAssignment(problem, *solver, solution);
      Coefficient objective = ComputeObjectiveValue(problem, *solution);
      logger.Log(CnfObjectiveLine(problem, objective));
      return SatSolver::MODEL_SAT;
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
    logger.Log(StringPrintf("c iter:%d core:%zu", iter, core.size()));

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
        return SatSolver::MODEL_UNSAT;
      }
      for (Literal b : blocking_clauses[index]) {
        if (!solver->AddUnitClause(b.Negated())) return SatSolver::MODEL_UNSAT;
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
        LOG(INFO) << "Unsat while adding a clause.";
        return SatSolver::MODEL_UNSAT;
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

  // The curent lower_bound on the cost.
  // It will be correct after the initialization.
  Coefficient lower_bound(static_cast<int64>(problem.objective().offset()));
  Coefficient upper_bound(kint64max);

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
  logger.Log(StringPrintf("c #weights:%zu #vars:%d #constraints:%d",
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
          return SatSolver::MODEL_UNSAT;
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
      logger.Log(StringPrintf("c fixed %zu assumptions, %d with cost > %lld",
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
    if (result == SatSolver::MODEL_SAT) {
      // If not all assumptions where taken, continue with a lower stratified
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
          static_cast<int64>(problem.objective().offset()));
      const Coefficient objective = ComputeObjectiveValue(problem, *solution);
      if (objective + objective_offset < upper_bound) {
        logger.Log(CnfObjectiveLine(problem, objective));
        upper_bound = objective + objective_offset;
      }

      if (stratified_lower_bound < old_lower_bound) continue;
      return SatSolver::MODEL_SAT;
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
    logger.Log(
        StringPrintf("c iter:%d core:%zu lb:%lld min_cost:%lld strat:%lld",
                     iter, core.size(), lower_bound.value(), min_cost.value(),
                     stratified_lower_bound.value()));

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
        return SatSolver::MODEL_UNSAT;
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
        // - an identical clause with cost min_cost is artifically added to
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
        return SatSolver::MODEL_UNSAT;
      }
    }
  }
}

SatSolver::Status SolveWithRandomParameters(LogBehavior log,
                                            const LinearBooleanProblem& problem,
                                            int num_times, SatSolver* solver,
                                            std::vector<bool>* solution) {
  Logger logger(log);
  const SatParameters initial_parameters = solver->parameters();

  MTRandom random("A random seed.");
  SatParameters parameters = initial_parameters;
  TimeLimit time_limit(parameters.max_time_in_seconds());

  // We start with a low conflict limit and increase it until we are able to
  // solve the problem at least once. After this, the limit stays the same.
  int max_number_of_conflicts = 5;
  parameters.set_log_search_progress(false);

  Coefficient min_seen(std::numeric_limits<int64>::max());
  Coefficient max_seen(std::numeric_limits<int64>::min());
  Coefficient best(min_seen);
  for (int i = 0; i < num_times; ++i) {
    solver->Backtrack(0);
    RandomizeDecisionHeuristic(&random, &parameters);

    parameters.set_max_number_of_conflicts(max_number_of_conflicts);
    parameters.set_max_time_in_seconds(time_limit.GetTimeLeft());
    parameters.set_random_seed(i);
    solver->SetParameters(parameters);
    solver->ResetDecisionHeuristic();

    const bool use_obj = random.OneIn(4);
    if (use_obj) UseObjectiveForSatAssignmentPreference(problem, solver);

    const SatSolver::Status result = solver->Solve();
    if (result == SatSolver::MODEL_UNSAT) {
      // If the problem is UNSAT after we over-constrained the objective, then
      // we found an optimal solution, otherwise, even the decision problem is
      // UNSAT.
      if (best == kCoefficientMax) return SatSolver::MODEL_UNSAT;
      return SatSolver::MODEL_SAT;
    }
    if (result == SatSolver::LIMIT_REACHED) {
      // We augment the number of conflict until we have one feasible solution.
      if (best == kCoefficientMax) ++max_number_of_conflicts;
      if (time_limit.LimitReached()) return SatSolver::LIMIT_REACHED;
      continue;
    }

    CHECK_EQ(result, SatSolver::MODEL_SAT);
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
        return SatSolver::MODEL_SAT;
      }
    }
    min_seen = std::min(min_seen, objective);
    max_seen = std::max(max_seen, objective);

    logger.Log(StringPrintf("c %lld [%lld, %lld] objective preference: %s %s",
                            objective.value(), min_seen.value(),
                            max_seen.value(), use_obj ? "true" : "false",
                            parameters.ShortDebugString().c_str()));
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
        return SatSolver::MODEL_SAT;
      }
    }

    // Solve the problem.
    const SatSolver::Status result = solver->Solve();
    CHECK_NE(result, SatSolver::ASSUMPTIONS_UNSAT);
    if (result == SatSolver::MODEL_UNSAT) {
      if (objective == kCoefficientMax) return SatSolver::MODEL_UNSAT;
      return SatSolver::MODEL_SAT;
    }
    if (result == SatSolver::LIMIT_REACHED) {
      return SatSolver::LIMIT_REACHED;
    }

    // Extract the new best solution.
    CHECK_EQ(result, SatSolver::MODEL_SAT);
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
  logger.Log(StringPrintf("c #weights:%zu #vars:%d #constraints:%d",
                          nodes.size(), problem.num_variables(),
                          problem.constraints_size()));

  // Create the sorter network.
  solver->Backtrack(0);
  EncodingNode* root =
      MergeAllNodesWithDeque(upper_bound, nodes, solver, &repository);
  logger.Log(StringPrintf("c encoding depth:%d", root->depth()));

  while (true) {
    if (objective != kCoefficientMax) {
      // Over constrain the objective by fixing the variable index - 1 of the
      // root node to 0.
      const int index = offset.value() + objective.value();
      if (index == 0) return SatSolver::MODEL_SAT;
      solver->Backtrack(0);
      if (!solver->AddUnitClause(root->literal(index - 1).Negated())) {
        return SatSolver::MODEL_SAT;
      }
    }

    // Solve the problem.
    const SatSolver::Status result = solver->Solve();
    CHECK_NE(result, SatSolver::ASSUMPTIONS_UNSAT);
    if (result == SatSolver::MODEL_UNSAT) {
      if (objective == kCoefficientMax) return SatSolver::MODEL_UNSAT;
      return SatSolver::MODEL_SAT;
    }
    if (result == SatSolver::LIMIT_REACHED) return SatSolver::LIMIT_REACHED;

    // Extract the new best solution.
    CHECK_EQ(result, SatSolver::MODEL_SAT);
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
  Coefficient upper_bound(kint64max);
  if (!solution->empty()) {
    CHECK(IsAssignmentValid(problem, *solution));
    upper_bound = ComputeObjectiveValue(problem, *solution) + offset;
  }

  // Print the number of variables with a non-zero cost.
  logger.Log(StringPrintf("c #weights:%zu #vars:%d #constraints:%d",
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
    const std::vector<Literal> assumptions = ReduceNodesAndExtractAssumptions(
        upper_bound, stratified_lower_bound, &lower_bound, &nodes, solver);
    if (assumptions.empty()) return SatSolver::MODEL_SAT;

    // Display the progress.
    const std::string gap_string =
        (upper_bound == kCoefficientMax)
            ? ""
            : StringPrintf(" gap:%lld", (upper_bound - lower_bound).value());
    logger.Log(
        StringPrintf("c iter:%d [%s] lb:%lld%s assumptions:%zu depth:%d",
                     iter, previous_core_info.c_str(),
                     lower_bound.value() - offset.value() +
                         static_cast<int64>(problem.objective().offset()),
                     gap_string.c_str(), nodes.size(), max_depth));

    // Solve under the assumptions.
    const SatSolver::Status result =
        solver->ResetAndSolveWithGivenAssumptions(assumptions);
    if (result == SatSolver::MODEL_SAT) {
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

      // If not all assumptions where taken, continue with a lower stratified
      // bound. Otherwise we have an optimal solution.
      stratified_lower_bound =
          MaxNodeWeightSmallerThan(nodes, stratified_lower_bound);
      if (stratified_lower_bound > 0) continue;
      return SatSolver::MODEL_SAT;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;

    // We have a new core.
    std::vector<Literal> core = solver->GetLastIncompatibleDecisions();
    if (parameters.minimize_core()) MinimizeCore(solver, &core);

    // Compute the min weight of all the nodes in the core.
    // The lower bound will be increased by that much.
    const Coefficient min_weight = ComputeCoreMinWeight(nodes, core);
    previous_core_info =
        StringPrintf("core:%zu mw:%lld", core.size(), min_weight.value());

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

SatSolver::Status MinimizeIntegerVariableWithLinearScan(
    IntegerVariable objective_var,
    const std::function<void(const Model&)>& feasible_solution_observer,
    Model* model) {
  return MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
      true, objective_var, {}, feasible_solution_observer, model);
}

namespace {
void LogSolveInfo(SatSolver::Status result, const SatSolver& sat_solver,
                  const WallTimer& wall_timer, const UserTimer& user_timer,
                  int64 objective, int64 best_bound) {
  printf("status: %s\n",
         result == SatSolver::MODEL_SAT ? "OPTIMAL"
                                        : SatStatusString(result).c_str());
  if (objective < kint64max) {
    printf("objective: %lld\n", objective);
  } else {
    printf("objective: NA\n");
  }
  printf("best_bound: %lld\n", best_bound);
  printf("booleans: %d\n", sat_solver.NumVariables());
  printf("conflicts: %lld\n", sat_solver.num_failures());
  printf("branches: %lld\n", sat_solver.num_branches());
  printf("propagations: %lld\n", sat_solver.num_propagations());
  printf("walltime: %f\n", wall_timer.Get());
  printf("usertime: %f\n", user_timer.Get());
  printf("deterministic_time: %f\n", sat_solver.deterministic_time());
}
}  // namespace

SatSolver::Status MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
    bool log_info, IntegerVariable objective_var,
    const std::function<LiteralIndex()>& next_decision,
    const std::function<void(const Model&)>& feasible_solution_observer,
    Model* model) {
  // Timing.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  if (log_info) {
    LOG(INFO) << "#Boolean_variables:" << sat_solver->NumVariables();
  }

  // Simple linear scan algorithm to find the optimal.
  SatSolver::Status result;
  bool model_is_feasible = false;
  IntegerValue objective(kint64max);
  while (true) {
    result = SolveIntegerProblemWithLazyEncoding(/*assumptions=*/{},
                                                 next_decision, model);
    if (result != SatSolver::MODEL_SAT) break;

    // The objective is the current lower bound of the objective_var.
    objective = integer_trail->LowerBound(objective_var);

    // We have a solution!
    model_is_feasible = true;
    if (feasible_solution_observer != nullptr) {
      feasible_solution_observer(*model);
    }

    // Restrict the objective.
    sat_solver->Backtrack(0);
    if (!integer_trail->Enqueue(
            IntegerLiteral::LowerOrEqual(objective_var, objective - 1), {},
            {})) {
      result = SatSolver::MODEL_UNSAT;
      break;
    }
  }

  IntegerValue best_bound;
  CHECK_NE(result, SatSolver::MODEL_SAT);
  if (result == SatSolver::MODEL_UNSAT && model_is_feasible) {
    // We proved the optimal and use the MODEL_SAT value for this.
    result = SatSolver::MODEL_SAT;
    best_bound = objective;
  } else {
    sat_solver->Backtrack(0);
    best_bound = integer_trail->LowerBound(objective_var);
  }

  if (log_info) {
    LogSolveInfo(result, *sat_solver, wall_timer, user_timer, objective.value(),
                 best_bound.value());
  }
  return result;
}

SatSolver::Status MinimizeWithCoreAndLazyEncoding(
    bool log_info, IntegerVariable objective_var,
    const std::vector<IntegerVariable>& variables,
    const std::vector<IntegerValue>& coefficients,
    const std::function<LiteralIndex()>& next_decision,
    const std::function<void(const Model&)>& feasible_solution_observer,
    Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();

  // This will be called each time a feasible solution is found. Returns false
  // if a conflict was detected while trying to constrain the objective to a
  // smaller value.
  int num_solutions = 0;
  IntegerValue best_objective = integer_trail->UpperBound(objective_var);
  const auto process_solution = [&]() {
    const IntegerValue objective(model->Get(Value(objective_var)));
    if (objective >= best_objective && num_solutions > 0) return true;

    ++num_solutions;
    best_objective = objective;
    if (feasible_solution_observer != nullptr) {
      feasible_solution_observer(*model);
    }

    // TODO(user): Investigate if constraining the objective is better.
    if (/* DISABLES CODE */ (false)) {
      sat_solver->Backtrack(0);
      sat_solver->SetAssumptionLevel(0);
      if (!integer_trail->Enqueue(
              IntegerLiteral::LowerOrEqual(objective_var, objective - 1), {},
              {})) {
        return false;
      }
    }
    return true;
  };

  // We express the objective as a linear sum of terms. These will evolve as the
  // algorithm progress.
  struct ObjectiveTerm {
    IntegerVariable var;
    IntegerValue weight;

    // These fields are only used for logging/debugging.
    int depth;
    IntegerValue old_var_lb;
  };
  std::vector<ObjectiveTerm> terms;
  CHECK_EQ(variables.size(), coefficients.size());
  for (int i = 0; i < variables.size(); ++i) {
    if (coefficients[i] > 0) {
      terms.push_back({variables[i], coefficients[i]});
    } else if (coefficients[i] < 0) {
      terms.push_back({NegationOf(variables[i]), -coefficients[i]});
    }
    terms.back().depth = 0;
  }

  // This is used by the "stratified" approach. We will only consider terms with
  // a weight not lower than this threshold. The threshold will decrease as the
  // algorithm progress.
  IntegerValue stratified_threshold = kMaxIntegerValue;

  // TODO(user): The core is returned in the same order as the assumptions,
  // so we don't really need this map, we could just do a linear scan to
  // recover which node are part of the core.
  std::map<LiteralIndex, int> assumption_to_term_index;

  // Start the algorithm.
  int max_depth = 0;
  SatSolver::Status result;
  for (int iter = 0;; ++iter) {
    sat_solver->Backtrack(0);
    sat_solver->SetAssumptionLevel(0);

    // We assumes all terms at their lower-bound.
    std::vector<Literal> assumptions;
    assumption_to_term_index.clear();
    IntegerValue next_stratified_threshold(0);
    IntegerValue implied_objective_lb;
    for (int i = 0; i < terms.size(); ++i) {
      const ObjectiveTerm term = terms[i];
      const IntegerValue var_lb = integer_trail->LowerBound(term.var);
      terms[i].old_var_lb = var_lb;
      implied_objective_lb += term.weight * var_lb.value();

      // TODO(user): These can be simply removed from the list.
      if (term.weight == 0) continue;

      // Skip fixed terms.
      // We still keep them around for a proper lower-bound computation.
      // TODO(user): we could keep an objective offset instead.
      if (var_lb == integer_trail->UpperBound(term.var)) continue;

      // Only consider the terms above the threshold.
      if (term.weight < stratified_threshold) {
        next_stratified_threshold =
            std::max(next_stratified_threshold, term.weight);
      } else {
        assumptions.push_back(integer_encoder->GetOrCreateAssociatedLiteral(
            IntegerLiteral::LowerOrEqual(term.var, var_lb)));
        InsertOrDie(&assumption_to_term_index, assumptions.back().Index(), i);
      }
    }

    // Update the objective lower bound with our current bound.
    //
    // Note(user): This is not needed for correctness, but it might cause
    // more propagation and is nice to have for reporting/logging purpose.
    if (!integer_trail->Enqueue(
            IntegerLiteral::GreaterOrEqual(objective_var, implied_objective_lb),
            {}, {})) {
      result = SatSolver::MODEL_UNSAT;
      break;
    }

    // No assumptions with the current stratified_threshold? use the new one.
    if (assumptions.empty() && next_stratified_threshold > 0) {
      stratified_threshold = next_stratified_threshold;
      --iter;  // "false" iteration, the lower bound does not increase.
      continue;
    }

    // Display the progress.
    const IntegerValue objective_lb = integer_trail->LowerBound(objective_var);
    if (log_info) {
      LOG(INFO) << StringPrintf(
          "  iter:%d lb:%lld (%lld) gap:%lld assumptions:%zu strat:%lld "
          "depth:%d",
          iter, objective_lb.value(), implied_objective_lb.value(),
          (best_objective - objective_lb).value(), assumptions.size(),
          stratified_threshold.value(), max_depth);
    }

    // Solve under the assumptions.
    result =
        SolveIntegerProblemWithLazyEncoding(assumptions, next_decision, model);
    if (result == SatSolver::MODEL_SAT) {
      if (!process_solution()) {
        result = SatSolver::MODEL_UNSAT;
        break;
      }

      // If not all assumptions where taken, continue with a lower stratified
      // bound. Otherwise we have an optimal solution.
      stratified_threshold = next_stratified_threshold;
      if (stratified_threshold == 0) break;
      --iter;  // "false" iteration, the lower bound does not increase.
      continue;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) break;

    // We have a new core.
    std::vector<Literal> core = sat_solver->GetLastIncompatibleDecisions();
    if (sat_solver->parameters().minimize_core()) {
      MinimizeCore(sat_solver, &core);
    }
    CHECK(!core.empty());

    // This just increase the lower-bound of the corresponding node, which
    // should already be done by the solver.
    if (core.size() == 1) continue;

    sat_solver->Backtrack(0);
    sat_solver->SetAssumptionLevel(0);

    // Compute the min weight of all the terms in the core. The lower bound will
    // be increased by that much because at least one assumption in the core
    // must be true. This is also why we can start at 1 for new_var_lb.
    IntegerValue min_weight = kMaxIntegerValue;
    IntegerValue max_weight(0);
    IntegerValue new_var_lb(1);
    IntegerValue new_var_ub(0);
    int new_depth = 0;
    for (const Literal lit : core) {
      const int index = FindOrDie(assumption_to_term_index, lit.Index());
      min_weight = std::min(min_weight, terms[index].weight);
      max_weight = std::max(max_weight, terms[index].weight);
      new_depth = std::max(new_depth, terms[index].depth + 1);
      new_var_lb += integer_trail->LowerBound(terms[index].var);
      new_var_ub += integer_trail->UpperBound(terms[index].var);
      CHECK_EQ(terms[index].old_var_lb,
               integer_trail->LowerBound(terms[index].var));
    }
    max_depth = std::max(max_depth, new_depth);
    if (log_info) {
      LOG(INFO) << StringPrintf(
          "    core:%zu weight:[%lld,%lld] domain:[%lld,%lld] depth:%d",
          core.size(), min_weight.value(), max_weight.value(),
          new_var_lb.value(), new_var_ub.value(), new_depth);
    }

    // We will "transfer" min_weight from all the variables of the core
    // to a new variable.
    const IntegerVariable new_var =
        model->Add(NewIntegerVariable(new_var_lb.value(), new_var_ub.value()));
    terms.push_back({new_var, min_weight, new_depth});

    // Sum variables in the core <= new_var.
    // TODO(user): Experiment with FixedWeightedSum() instead.
    {
      std::vector<IntegerVariable> constraint_vars;
      std::vector<int64> constraint_coeffs;
      for (const Literal lit : core) {
        const int index = FindOrDie(assumption_to_term_index, lit.Index());
        terms[index].weight -= min_weight;
        constraint_vars.push_back(terms[index].var);
        constraint_coeffs.push_back(1);
      }
      constraint_vars.push_back(new_var);
      constraint_coeffs.push_back(-1);
      model->Add(
          WeightedSumLowerOrEqual(constraint_vars, constraint_coeffs, 0));
    }

    // Re-express the objective with the new terms.
    // TODO(user): Do more experiments to decide if this is better.
    // TODO(user): Experiment with FixedWeightedSum().
    if (/* DISABLES CODE */ (false)) {
      std::vector<IntegerVariable> constraint_vars;
      std::vector<int64> constraint_coeffs;
      for (const ObjectiveTerm node : terms) {
        if (node.weight == 0) continue;
        constraint_vars.push_back(node.var);
        constraint_coeffs.push_back(node.weight.value());
      }
      constraint_vars.push_back(objective_var);
      constraint_coeffs.push_back(-1);
      model->Add(FixedWeightedSum(constraint_vars, constraint_coeffs, 0));
    }

    // Find out the true lower bound of new_var. This is called "cover
    // optimization" in the max-SAT literature.
    //
    // TODO(user): Do more experiments to decide if this is better. This
    // approach kind of mix the basic linear-scan one with the core based
    // approach.
    if (/* DISABLES CODE */ (false)) {
      IntegerValue best = new_var_ub;

      // Simple linear scan algorithm to find the optimal of new_var.
      while (best > new_var_lb) {
        const Literal a = integer_encoder->GetOrCreateAssociatedLiteral(
            IntegerLiteral::LowerOrEqual(new_var, best - 1));
        result = SolveIntegerProblemWithLazyEncoding({a}, next_decision, model);
        if (result != SatSolver::MODEL_SAT) break;
        best = integer_trail->LowerBound(new_var);
        if (!process_solution()) {
          result = SatSolver::MODEL_UNSAT;
          break;
        }
      }
      if (result == SatSolver::ASSUMPTIONS_UNSAT) {
        sat_solver->Backtrack(0);
        sat_solver->SetAssumptionLevel(0);
        if (!integer_trail->Enqueue(
                IntegerLiteral::GreaterOrEqual(new_var, best), {}, {})) {
          result = SatSolver::MODEL_UNSAT;
          break;
        }
      }
    }
  }

  // Returns MODEL_SAT if we found the optimal.
  return num_solutions > 0 && result == SatSolver::MODEL_UNSAT
             ? SatSolver::MODEL_SAT
             : result;
}

SatSolver::Status MinimizeWeightedLiteralSumWithCoreAndLazyEncoding(
    bool log_info, const std::vector<Literal>& literals,
    const std::vector<int64>& int64_coeffs,
    const std::function<LiteralIndex()>& next_decision,
    const std::function<void(const Model&)>& feasible_solution_observer,
    Model* model) {
  // Timing.
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  // TODO(user): Rename Coefficient into IntegerValue everywhere.
  std::vector<Coefficient> coeffs(int64_coeffs.begin(), int64_coeffs.end());

  // Create one initial nodes per variables with cost.
  Coefficient lower_bound(0);
  Coefficient upper_bound(kint64max);
  Coefficient offset(0);
  std::deque<EncodingNode> repository;
  std::vector<EncodingNode*> nodes =
      CreateInitialEncodingNodes(literals, coeffs, &offset, &repository);

  // Print the number of variables with a non-zero cost.
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  if (log_info) {
    LOG(INFO) << StringPrintf("c #weights:%zu #vars:%d", nodes.size(),
                              sat_solver->NumVariables());
  }

  // This is used by the "stratified" approach.
  Coefficient stratified_lower_bound(0);
  if (sat_solver->parameters().max_sat_stratification() ==
      SatParameters::STRATIFICATION_DESCENT) {
    // In this case, we initialize it to the maximum assumption weights.
    for (EncodingNode* n : nodes) {
      stratified_lower_bound = std::max(stratified_lower_bound, n->weight());
    }
  }

  // Start the algorithm.
  int max_depth = 0;
  std::string previous_core_info = "";
  SatSolver::Status result = SatSolver::MODEL_SAT;  // Not important.
  for (int iter = 0;; ++iter) {
    const std::vector<Literal> assumptions = ReduceNodesAndExtractAssumptions(
        upper_bound, stratified_lower_bound, &lower_bound, &nodes, sat_solver);

    // No assumptions with the current stratified_lower_bound, lower it if
    // possible.
    if (assumptions.empty()) {
      stratified_lower_bound =
          MaxNodeWeightSmallerThan(nodes, stratified_lower_bound);
      if (stratified_lower_bound > 0) {
        --iter;
        continue;
      }
    }

    // Display the progress.
    const std::string gap_string =
        (upper_bound == kCoefficientMax)
            ? ""
            : StringPrintf(" gap:%lld", (upper_bound - lower_bound).value());
    if (log_info) {
      LOG(INFO) << StringPrintf(
          "c iter:%d [%s] lb:%lld%s assumptions:%zu depth:%d", iter,
          previous_core_info.c_str(), lower_bound.value() - offset.value(),
          gap_string.c_str(), nodes.size(), max_depth);
    }

    // No assumptions means that there is no solution with cost < upper_bound.
    if (assumptions.empty()) {
      if (log_info) LOG(INFO) << "c no assumptions.";
      result = (lower_bound == upper_bound) ? SatSolver::MODEL_SAT
                                            : SatSolver::MODEL_UNSAT;
      break;
    }

    // Solve under the assumptions.
    result =
        SolveIntegerProblemWithLazyEncoding(assumptions, next_decision, model);
    if (result == SatSolver::MODEL_SAT) {
      // Extract the new solution and save it if it is the best found so far.
      Coefficient obj(0);
      for (int i = 0; i < literals.size(); ++i) {
        if (sat_solver->Assignment().LiteralIsTrue(literals[i])) {
          obj += coeffs[i];
        }
      }
      if (obj + offset < upper_bound) {
        if (feasible_solution_observer != nullptr) {
          feasible_solution_observer(*model);
        }
        upper_bound = obj + offset;
        if (log_info) LOG(INFO) << "c ub:" << upper_bound;
      }

      // If not all assumptions where taken, continue with a lower stratified
      // bound. Otherwise we have an optimal solution.
      stratified_lower_bound =
          MaxNodeWeightSmallerThan(nodes, stratified_lower_bound);
      if (stratified_lower_bound == 0) break;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) break;

    // We have a new core.
    std::vector<Literal> core = sat_solver->GetLastIncompatibleDecisions();
    if (sat_solver->parameters().minimize_core()) {
      MinimizeCore(sat_solver, &core);
    }

    // Compute the min weight of all the nodes in the core.
    // The lower bound will be increased by that much.
    const Coefficient min_weight = ComputeCoreMinWeight(nodes, core);
    previous_core_info =
        StringPrintf("core:%zu mw:%lld", core.size(), min_weight.value());

    // Increase stratified_lower_bound according to the parameters.
    if (stratified_lower_bound < min_weight &&
        sat_solver->parameters().max_sat_stratification() ==
            SatParameters::STRATIFICATION_ASCENT) {
      stratified_lower_bound = min_weight;
    }

    ProcessCore(core, min_weight, &repository, &nodes, sat_solver);
    max_depth = std::max(max_depth, nodes.back()->depth());
  }

  if (log_info) {
    LogSolveInfo(result, *sat_solver, wall_timer, user_timer,
                 upper_bound.value(), lower_bound.value() - offset.value());
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
