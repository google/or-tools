// Copyright 2010-2013 Google
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

#include "google/protobuf/descriptor.h"

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
      objective.value() * problem.objective().scaling_factor() +
      problem.objective().offset();
  return StringPrintf("o %lld", static_cast<int64>(scaled_objective));
}

}  // namespace

// This algorithm works by exploiting the unsat core returned by the SAT solver
// when the problem is UNSAT. It starts by trying to solve the decision problem
// where all the objective variables are set to their value with minimal cost,
// and relax in each step some of these fixed variables until the problem
// becomes satisfiable.
SatSolver::Status SolveWithFuMalik(const LinearBooleanProblem& problem,
                                   SatSolver* solver, std::vector<bool>* solution,
                                   LogBehavior log) {
  Logger logger(log);
  CHECK_EQ(problem.type(), LinearBooleanProblem::MINIMIZATION);

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
  const Coefficient unique_objective_coeff(objective.coefficients(0));
  for (int i = 0; i < objective.literals_size(); ++i) {
    CHECK_EQ(objective.coefficients(i), unique_objective_coeff)
        << "The basic Fu & Malik algorithm needs constant objective coeffs.";
    const Literal literal(objective.literals(i));

    // We want to minimize the cost when this literal is true.
    const Literal min_literal =
        unique_objective_coeff > 0 ? literal.Negated() : literal;
    blocking_clauses.push_back({min_literal});

    // Note that initialy, we do not create any extra variables.
    assumptions.push_back(min_literal);
  }

  // Print the number of variable with a non-zero cost.
  logger.Log(StringPrintf("c #weigths:%zu", assumptions.size()));

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
    const std::vector<Literal> core = solver->GetLastIncompatibleDecisions();
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

      // Fix it. We also fix all the associated blocking variables if any. Note
      // that if blocking_clauses[index] contains just one variable, it will be
      // fixed twice, but that is ok.
      if (!solver->AddUnitClause(core[0].Negated()))
        return SatSolver::MODEL_UNSAT;
      for (Literal b : blocking_clauses[index]) {
        if (!solver->AddUnitClause(b.Negated())) return SatSolver::MODEL_UNSAT;
      }

      // Erase this entry from the current "objective"
      assumptions.erase(assumptions.begin() + index);
      blocking_clauses.erase(blocking_clauses.begin() + index);
    } else {
      // We will add 2 * |core.size()| variables.
      const int old_num_variables = solver->NumVariables();
      solver->SetNumVariables(old_num_variables + 2 * core.size());

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
        const Literal b(VariableIndex(old_num_variables + i), true);
        const Literal a(VariableIndex(old_num_variables + core.size() + i),
                        true);

        // There are two ways to encode the algorithm in SAT.
        // TODO(user): The first algo seems better, but experiment more.
        if (true) {
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
        } else {
          // a false & b false => previous assumptions (which was false).
          const Literal old_a = assumptions[index];
          ok &= solver->AddProblemClause({a, b, old_a});

          // Also add the two implications a => x and b => x where x is the
          // negation of the previous assumption variable.
          ok &= solver->AddProblemClause({a.Negated(), old_a.Negated()});
          ok &= solver->AddProblemClause({b.Negated(), old_a.Negated()});

          // TODO(user): Also add the exclusion between a and b?
          if (false) {
            ok &= solver->AddProblemClause({a.Negated(), b.Negated()});
            ok &= solver->AddProblemClause({b.Negated(), a.Negated()});
          }
        }

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
  const std::vector<double> ratios = {0.0, 0.0, 0.0, 0.01, 1.0};
  parameters->set_random_polarity_ratio(ratios[random->Uniform(ratios.size())]);

  // IMPORTANT: SetParameters() will reinitialize the seed, so we must change
  // it.
  parameters->set_random_seed(parameters->random_seed() + 1);
}

SatSolver::Status SolveWithRandomParameters(const LinearBooleanProblem& problem,
                                            int num_times, SatSolver* solver,
                                            std::vector<bool>* solution,
                                            LogBehavior log) {
  Logger logger(log);
  CHECK_EQ(problem.type(), LinearBooleanProblem::MINIMIZATION);
  const SatParameters initial_parameters = solver->parameters();

  MTRandom random("A random seed.");
  SatParameters parameters = initial_parameters;
  TimeLimit time_limit(parameters.max_time_in_seconds());

  // We start with a low limit (increased on each SatSolver::LIMIT_REACHED).
  parameters.set_log_search_progress(false);
  parameters.set_max_number_of_conflicts(10);

  Coefficient min_seen(std::numeric_limits<int64>::max());
  Coefficient max_seen(std::numeric_limits<int64>::min());
  Coefficient best(min_seen);
  for (int i = 0; i < num_times; ++i) {
    solver->Backtrack(0);
    RandomizeDecisionHeuristic(&random, &parameters);
    parameters.set_max_time_in_seconds(time_limit.GetTimeLeft());
    solver->SetParameters(parameters);
    solver->ResetDecisionHeuristic();

    const bool use_obj = random.OneIn(4);
    if (use_obj) UseObjectiveForSatAssignmentPreference(problem, solver);

    const SatSolver::Status result = solver->Solve();
    if (result == SatSolver::MODEL_UNSAT) return SatSolver::MODEL_UNSAT;
    if (result == SatSolver::LIMIT_REACHED) {
      parameters.set_max_number_of_conflicts(
          static_cast<int64>(1.1 * parameters.max_number_of_conflicts()));
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

SatSolver::Status SolveWithLinearScan(const LinearBooleanProblem& problem,
                                      SatSolver* solver, std::vector<bool>* solution,
                                      LogBehavior log) {
  Logger logger(log);
  CHECK_EQ(problem.type(), LinearBooleanProblem::MINIMIZATION);

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

}  // namespace sat
}  // namespace operations_research
