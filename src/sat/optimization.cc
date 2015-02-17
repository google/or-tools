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

#include "sat/optimization.h"

#include <deque>
#include <queue>

#include "google/protobuf/descriptor.h"
#include "sat/encoding.h"

namespace operations_research {
namespace sat {

namespace {

// Used to log messages to stdout or to the normal logging framework according
// to the given LogBehavior value.
class Logger {
 public:
  explicit Logger(LogBehavior v) : use_stdout_(v == STDOUT_LOG) {}
  void Log(std::string message) {
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
void DeleteVectorIndices(const std::vector<int> indices, Vector* v) {
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
  if (solver->ResetAndSolveWithGivenAssumptions(temp) !=
      SatSolver::ASSUMPTIONS_UNSAT) {
    // This should almost never happen, but it is not impossible. The reason is
    // that the solver may delete some learned clauses required by the unit
    // propagation to show that the core is unsat.
    LOG(WARNING) << "This should only happen rarely! otherwise, investigate";
    return;
  }
  CHECK_EQ(solver->ResetAndSolveWithGivenAssumptions(temp),
           SatSolver::ASSUMPTIONS_UNSAT);
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
                                   SatSolver* solver, std::vector<bool>* solution) {
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
      if (!solver->AddUnitClause(core[0].Negated()))
        return SatSolver::MODEL_UNSAT;
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
        const Literal a(VariableIndex(old_num_variables + i), true);
        Literal b(VariableIndex(old_num_variables + core.size() + i), true);
        if (core.size() == 2) {
          b = Literal(VariableIndex(old_num_variables + 2), true);
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
      if (false) ok &= solver->AddProblemClause(at_least_one_constraint);

      if (!ok) {
        LOG(INFO) << "Unsat while adding a clause.";
        return SatSolver::MODEL_UNSAT;
      }
    }
  }
}

SatSolver::Status SolveWithWPM1(LogBehavior log,
                                const LinearBooleanProblem& problem,
                                SatSolver* solver, std::vector<bool>* solution) {
  Logger logger(log);
  FuMalikSymmetryBreaker symmetry;

  // The curent lower_bound on the cost.
  // It will be correct after the initialization.
  Coefficient lower_bound(problem.objective().offset());
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
        if (solver->Assignment().IsLiteralTrue(assumptions[i])) {
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
      Coefficient objective = ComputeObjectiveValue(problem, *solution);
      if (objective + problem.objective().offset() < upper_bound) {
        logger.Log(CnfObjectiveLine(problem, objective));
        upper_bound = objective + problem.objective().offset();
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
        const Literal a(VariableIndex(old_num_variables + i), true);
        Literal b(VariableIndex(old_num_variables + core.size() + i), true);
        if (core.size() == 2) {
          b = Literal(VariableIndex(old_num_variables + 2), true);
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

void RandomizeDecisionHeuristic(MTRandom* random, SatParameters* parameters) {
  // Random preferred variable order.
  const google::protobuf::EnumDescriptor* order_d =
      SatParameters::VariableOrder_descriptor();
  parameters->set_preferred_variable_order(
      static_cast<SatParameters::VariableOrder>(
          order_d->value(random->Uniform(order_d->value_count()))->number()));

  // Random polarity initial value.
  const google::protobuf::EnumDescriptor* polarity_d =
      SatParameters::Polarity_descriptor();
  parameters->set_initial_polarity(static_cast<SatParameters::Polarity>(
      polarity_d->value(random->Uniform(polarity_d->value_count()))->number()));

  // Other random parameters.
  parameters->set_use_phase_saving(random->OneIn(2));
  parameters->set_random_polarity_ratio(random->OneIn(2) ? 0.01 : 0.0);
  parameters->set_random_branches_ratio(random->OneIn(2) ? 0.01 : 0.0);
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

namespace {

bool EncodingNodeByWeight(const EncodingNode* a, const EncodingNode* b) {
  return a->weight() < b->weight();
}

bool EncodingNodeByDepth(const EncodingNode* a, const EncodingNode* b) {
  return a->depth() < b->depth();
}

bool EmptyEncodingNode(const EncodingNode* a) { return a->size() == 0; }

}  // namespace

SatSolver::Status SolveWithCardinalityEncodingAndCore(
    LogBehavior log, const LinearBooleanProblem& problem, SatSolver* solver,
    std::vector<bool>* solution) {
  Logger logger(log);
  SatParameters parameters = solver->parameters();
  std::deque<EncodingNode> repository;

  // Create one initial nodes per variables with cost.
  Coefficient offset(0);
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
    // Remove the left-most variables fixed to one from each node.
    // Also update the lower_bound. Note that Reduce() needs the solver to be
    // at the root node in order to work.
    solver->Backtrack(0);
    for (EncodingNode* n : nodes) {
      lower_bound += n->Reduce(*solver) * n->weight();
    }

    // Fix the nodes right-most variables that are above the gap.
    if (upper_bound != kCoefficientMax) {
      const Coefficient gap = upper_bound - lower_bound;
      if (gap == 0) return SatSolver::MODEL_SAT;
      for (EncodingNode* n : nodes) {
        n->ApplyUpperBound((gap / n->weight()).value(), solver);
      }
    }

    // Remove the empty nodes.
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(), EmptyEncodingNode),
                nodes.end());

    // Sort the nodes.
    switch (parameters.max_sat_assumption_order()) {
      case SatParameters::DEFAULT_ASSUMPTION_ORDER:
        break;
      case SatParameters::ORDER_ASSUMPTION_BY_DEPTH:
        std::sort(nodes.begin(), nodes.end(), EncodingNodeByDepth);
        break;
      case SatParameters::ORDER_ASSUMPTION_BY_WEIGHT:
        std::sort(nodes.begin(), nodes.end(), EncodingNodeByWeight);
        break;
    }
    if (parameters.max_sat_reverse_assumption_order()) {
      // TODO(user): with DEFAULT_ASSUMPTION_ORDER, this will lead to a somewhat
      // weird behavior, since we will reverse the nodes at each iterations...
      std::reverse(nodes.begin(), nodes.end());
    }

    // Extract the assumptions from the nodes.
    std::vector<Literal> assumptions;
    for (EncodingNode* n : nodes) {
      if (n->weight() >= stratified_lower_bound) {
        assumptions.push_back(n->literal(0).Negated());
      }
    }

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
      const Coefficient old_lower_bound = stratified_lower_bound;
      for (EncodingNode* n : nodes) {
        if (n->weight() < old_lower_bound) {
          if (stratified_lower_bound == old_lower_bound ||
              n->weight() > stratified_lower_bound) {
            stratified_lower_bound = n->weight();
          }
        }
      }
      if (stratified_lower_bound < old_lower_bound) continue;
      return SatSolver::MODEL_SAT;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;

    // We have a new core.
    std::vector<Literal> core = solver->GetLastIncompatibleDecisions();
    if (parameters.minimize_core()) MinimizeCore(solver, &core);

    // Compute the min weight of all the nodes in the core.
    // The lower bound will be increased by that much.
    Coefficient min_weight = kCoefficientMax;
    {
      int index = 0;
      for (int i = 0; i < core.size(); ++i) {
        for (; index < nodes.size() &&
                   nodes[index]->literal(0).Negated() != core[i];
             ++index) {
        }
        CHECK_LT(index, nodes.size());
        min_weight = std::min(min_weight, nodes[index]->weight());
      }
    }
    previous_core_info =
        StringPrintf("core:%zu mw:%lld", core.size(), min_weight.value());

    // Increase stratified_lower_bound according to the parameters.
    if (stratified_lower_bound < min_weight &&
        parameters.max_sat_stratification() ==
            SatParameters::STRATIFICATION_ASCENT) {
      stratified_lower_bound = min_weight;
    }

    // Backtrack to be able to add new constraints.
    solver->Backtrack(0);

    int new_node_index = 0;
    if (core.size() == 1) {
      // The core will be reduced at the beginning of the next loop.
      // Find the associated node, and call IncreaseNodeSize() on it.
      CHECK(solver->Assignment().IsLiteralFalse(core[0]));
      for (EncodingNode* n : nodes) {
        if (n->literal(0).Negated() == core[0]) {
          IncreaseNodeSize(n, solver);
          break;
        }
      }
    } else {
      // Remove from nodes the EncodingNode in the core, merge them, and add the
      // resulting EncodingNode at the back.
      int index = 0;
      std::vector<EncodingNode*> to_merge;
      for (int i = 0; i < core.size(); ++i) {
        // Since the nodes appear in order in the core, we can find the
        // relevant "objective" variable efficiently with a simple linear scan
        // in the nodes vector (done with index).
        for (; nodes[index]->literal(0).Negated() != core[i]; ++index) {
          CHECK_LT(index, nodes.size());
          nodes[new_node_index] = nodes[index];
          ++new_node_index;
        }
        CHECK_LT(index, nodes.size());
        to_merge.push_back(nodes[index]);

        // Special case if the weight > min_weight. we keep it, but reduce its
        // cost. This is the same "trick" as in WPM1 used to deal with weight.
        // We basically split a clause with a larger weight in two identical
        // clauses, one with weight min_weight that will be merged and one with
        // the remaining weight.
        if (nodes[index]->weight() > min_weight) {
          nodes[index]->set_weight(nodes[index]->weight() - min_weight);
          nodes[new_node_index] = nodes[index];
          ++new_node_index;
        }
        ++index;
      }
      for (; index < nodes.size(); ++index) {
        nodes[new_node_index] = nodes[index];
        ++new_node_index;
      }
      nodes.resize(new_node_index);
      nodes.push_back(LazyMergeAllNodeWithPQ(to_merge, solver, &repository));
      IncreaseNodeSize(nodes.back(), solver);
      max_depth = std::max(max_depth, nodes.back()->depth());
      nodes.back()->set_weight(min_weight);
      CHECK(solver->AddUnitClause(nodes.back()->literal(0)));
    }
  }
}

}  // namespace sat
}  // namespace operations_research
