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


// Sports scheduling problem.
//
// We want to solve the problem of scheduling of team matches in a
// double round robin tournament.  Given a number of teams, we want
// each team to encounter all other teams, twice, once at home, and
// once away. Furthermore, you cannot meet the same team twice in the
// same half-season.
//
// Finally, there are constraints on the sequence of home or aways:
//   - You cannot have 3 consecutive homes or three consecutive aways.
//   - A break is a sequence of two homes or two aways, the overall objective
//     of the optimization problem is to minimize the total number of breaks.
//
// We model this problem with three matrices of variables, each with
// num_teams rows and 2*(num_teams - 1) columns: the var [i][j]
// corresponds to the match of team #i at day #j. There are
// 2*(num_teams - 1) columns because each team meets num_teams - 1
// opponents twice.

// - The 'opponent' var [i][j] is the index of the opposing team.
// - The 'home_away' var [i][j] is a boolean: 1 for 'playing away',
//   0 for 'playing at home'.
// - The 'opponent_and_home_away' var [i][j] is the 'opponent' var [i][j] +
//   num_teams * the 'home_away' var [i][j].
// This aggregated variable will be useful to state constraints of the model
// and to do search on it.
//
// We use an original approch in this model as most of the constraints will
// be pre-computed and asserted using an AllowedAssignment constraint (see
// Solver::MakeAllowedAssignment() in constraint_solver.h).
// In particular:
//   - Each day, we have a perfect matching between teams
//     (A meets B <=> B meets A, and A is at home <=> B is away).
//     A cannot meet itself.
//   - For each team, over the length of the tournament, we have constraints
//     on the sequence of home-aways. We will precompute all possible sequences
//     of home_aways, as well as the corresponding number of breaks for that
//     team.
//   - For a given team and a given day, the link between the opponent var,
//     the home_away var and the aggregated var (see third matrix of variables)
//     is also maintained using a AllowedAssignment constraint.
//
// Usage: run this with --helpshort for a short usage manual.

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

// Problem main flags.
DEFINE_int32(num_teams, 10, "Number of teams in the problem.");

// General solving parameters.
DEFINE_int32(time_limit, 20000, "Time limit in ms.");

// Search tweaking parameters. These are defined to illustrate their effect.
DEFINE_bool(run_all_heuristics, true,
            "Run all heuristics in impact search, see DefaultPhaseParameters"
            " in constraint_solver/constraint_solver.h for details.");
DEFINE_int32(heuristics_period, 30,
             "Frequency to run all heuristics, see DefaultPhaseParameters"
             " in constraint_solver/constraint_solver.h for details.");
DEFINE_double(restart_log_size, 8.0,
              "Threshold for automatic restarting the search in default phase,"
              " see DefaultPhaseParameters in "
              "constraint_solver/constraint_solver.h for details.");

namespace operations_research {
// ---------- Utility functions to help create the model ----------

// ----- Constraints for one day and one team -----

// Computes the tuple set that links opponents, home_away, and
// signed_opponent on a single day for a single team.
void ComputeOneDayOneTeamTuples(int num_teams, IntTupleSet* const tuples) {
  for (int home_away = 0; home_away <= 1; ++home_away) {
    for (int opponent = 0; opponent < num_teams; ++opponent) {
      tuples->Insert3(opponent, home_away, opponent + home_away * num_teams);
    }
  }
}

void AddOneDayOneTeamConstraint(Solver* const solver, IntVar* const opponent,
                                IntVar* const home_away,
                                IntVar* const signed_opponent,
                                const IntTupleSet& intra_day_tuples) {
  std::vector<IntVar*> tmp_vars;
  tmp_vars.push_back(opponent);
  tmp_vars.push_back(home_away);
  tmp_vars.push_back(signed_opponent);
  solver->AddConstraint(
      solver->MakeAllowedAssignments(tmp_vars, intra_day_tuples));
}

// ----- Constraints for one day and all teams -----

// Computes all valid combination of signed_opponent for a single
// day and all teams.
void ComputeOneDayTuples(int num_teams, IntTupleSet* const day_tuples) {
  LOG(INFO) << "Compute possible opponents and locations for any day.";
  Solver solver("ComputeOneDayTuples");

  // We create the variables.
  std::vector<IntVar*> opponents;
  std::vector<IntVar*> home_aways;
  std::vector<IntVar*> signed_opponents;
  solver.MakeIntVarArray(num_teams, 0, num_teams - 1, "opponent_", &opponents);
  solver.MakeBoolVarArray(num_teams, "home_away_", &home_aways);
  solver.MakeIntVarArray(num_teams, 0, 2 * num_teams - 1, "signed_opponent_",
                         &signed_opponents);

  // All Diff constraint.
  solver.AddConstraint(solver.MakeAllDifferent(opponents));

  // Cannot play against itself
  for (int i = 0; i < num_teams; ++i) {
    solver.AddConstraint(solver.MakeNonEquality(opponents[i], i));
  }

  // Matching constraint (vars[i] == j <=> vars[j] == i).
  for (int i = 0; i < num_teams; ++i) {
    for (int j = 0; j < num_teams; ++j) {
      if (i != j) {
        solver.AddConstraint(
            solver.MakeEquality(solver.MakeIsEqualCstVar(opponents[i], j),
                                solver.MakeIsEqualCstVar(opponents[j], i)));
      }
    }
  }

  // num_teams/2 teams are home.
  solver.AddConstraint(solver.MakeSumEquality(home_aways, num_teams / 2));

  // Link signed_opponents, home_away and opponents
  IntTupleSet one_day_one_team_tuples(3);
  ComputeOneDayOneTeamTuples(num_teams, &one_day_one_team_tuples);
  for (int team_index = 0; team_index < num_teams; ++team_index) {
    std::vector<IntVar*> tmp_vars;
    tmp_vars.push_back(opponents[team_index]);
    tmp_vars.push_back(home_aways[team_index]);
    tmp_vars.push_back(signed_opponents[team_index]);
    solver.AddConstraint(
        solver.MakeAllowedAssignments(tmp_vars, one_day_one_team_tuples));
  }

  // if A meets B at home, B meets A away.
  for (int first_team = 0; first_team < num_teams; ++first_team) {
    IntVar* const first_home_away = home_aways[first_team];
    IntVar* const second_home_away =
        solver.MakeElement(home_aways, opponents[first_team])->Var();
    IntVar* const reverse_second_home_away =
        solver.MakeDifference(1, second_home_away)->Var();
    solver.AddConstraint(
        solver.MakeEquality(first_home_away, reverse_second_home_away));
  }

  // Search for solutions and fill day_tuples.
  DecisionBuilder* const db = solver.MakePhase(
      signed_opponents, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    std::vector<int> solution;
    for (int i = 0; i < num_teams; ++i) {
      solution.push_back(signed_opponents[i]->Value());
    }
    day_tuples->Insert(solution);
  }
  solver.EndSearch();
  LOG(INFO) << day_tuples->NumTuples()
            << " solutions to the one-day matching problem";
}

// Adds all constraints relating to one teams and the complete schedule.
void AddOneTeamConstraints(Solver* const solver,
                           const std::vector<IntVar*>& opponents,
                           const std::vector<IntVar*>& home_aways,
                           const std::vector<IntVar*>& signed_opponents,
                           const IntTupleSet& home_away_tuples,
                           IntVar* const break_var, int num_teams) {
  const int half_season = num_teams - 1;

  // Each team meet all opponents once by half season.
  for (int half = 0; half <= 1; ++half) {
    for (int team_index = 0; team_index < num_teams; ++team_index) {
      std::vector<IntVar*> tmp_vars;
      for (int day = 0; day < half_season; ++day) {
        tmp_vars.push_back(opponents[half * half_season + day]);
      }
      solver->AddConstraint(solver->MakeAllDifferent(tmp_vars));
    }
  }

  // We meet each opponent once at home and once away per full season.
  for (int team_index = 0; team_index < num_teams; ++team_index) {
    solver->AddConstraint(solver->MakeAllDifferent(signed_opponents));
  }

  // Constraint per team on home_aways;
  for (int i = 0; i < num_teams; ++i) {
    std::vector<IntVar*> tmp_vars(home_aways);
    tmp_vars.push_back(break_var);
    solver->AddConstraint(
        solver->MakeAllowedAssignments(tmp_vars, home_away_tuples));
  }
}

// ----- Constraints for one team and all days -----

// Computes all valid tuples for home_away variables for a single team
// on the full lenght of the season.
void ComputeOneTeamHomeAwayTuples(int num_teams,
                                  IntTupleSet* const home_away_tuples) {
  LOG(INFO) << "Compute possible sequence of home and aways for any team.";
  const int half_season = num_teams - 1;
  const int full_season = 2 * half_season;

  Solver solver("compute_home_aways");
  std::vector<IntVar*> home_aways;
  solver.MakeBoolVarArray(full_season, "home_away_", &home_aways);
  for (int day = 0; day < full_season - 2; ++day) {
    std::vector<IntVar*> tmp_vars;
    tmp_vars.push_back(home_aways[day]);
    tmp_vars.push_back(home_aways[day + 1]);
    tmp_vars.push_back(home_aways[day + 2]);
    IntVar* const partial_sum = solver.MakeSum(tmp_vars)->Var();
    solver.AddConstraint(solver.MakeBetweenCt(partial_sum, 1, 2));
  }
  DecisionBuilder* const db = solver.MakePhase(
      home_aways, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    std::vector<int> solution;
    for (int i = 0; i < full_season; ++i) {
      solution.push_back(home_aways[i]->Value());
    }
    int breaks = 0;
    for (int i = 0; i < full_season - 1; ++i) {
      breaks += (solution[i] == solution[i + 1]);
    }
    solution.push_back(breaks);
    home_away_tuples->Insert(solution);
  }
  solver.EndSearch();
  LOG(INFO) << home_away_tuples->NumTuples()
            << " combination of home_aways for a team on the full season";
}

// ---------- Main solving method ----------

// Solves the sports scheduling problem with a given number of teams.
void SportsScheduling(int num_teams) {
  const int half_season = num_teams - 1;
  const int full_season = 2 * half_season;

  Solver solver("Sports Scheduling");

  // ----- Variables -----

  // The index of the opponent of a team on a given day.
  std::vector<std::vector<IntVar*> > opponents(num_teams);
  // The location of the match (home or away).
  std::vector<std::vector<IntVar*> > home_aways(num_teams);
  // Disambiguated version of the opponent variable incorporating the
  // home_away result.
  std::vector<std::vector<IntVar*> > signed_opponents(num_teams);
  for (int team_index = 0; team_index < num_teams; ++team_index) {
    solver.MakeIntVarArray(full_season, 0, num_teams - 1,
                           StringPrintf("opponent_%d_", team_index),
                           &opponents[team_index]);
    solver.MakeBoolVarArray(full_season,
                            StringPrintf("home_away_%d_", team_index),
                            &home_aways[team_index]);
    solver.MakeIntVarArray(full_season, 0, 2 * num_teams - 1,
                           StringPrintf("signed_opponent_%d", team_index),
                           &signed_opponents[team_index]);
  }
  // ----- Constraints -----

  // Constraints on a given day.
  IntTupleSet one_day_tuples(num_teams);
  ComputeOneDayTuples(num_teams, &one_day_tuples);
  for (int day = 0; day < full_season; ++day) {
    std::vector<IntVar*> all_vars;
    for (int i = 0; i < num_teams; ++i) {
      all_vars.push_back(signed_opponents[i][day]);
    }
    solver.AddConstraint(
        solver.MakeAllowedAssignments(all_vars, one_day_tuples));
  }

  // Links signed_opponents, home_away and opponents.
  IntTupleSet one_day_one_team_tuples(3);
  ComputeOneDayOneTeamTuples(num_teams, &one_day_one_team_tuples);
  for (int day = 0; day < full_season; ++day) {
    for (int team_index = 0; team_index < num_teams; ++team_index) {
      AddOneDayOneTeamConstraint(
          &solver, opponents[team_index][day], home_aways[team_index][day],
          signed_opponents[team_index][day], one_day_one_team_tuples);
    }
  }

  // Constraints on a team.
  IntTupleSet home_away_tuples(full_season + 1);
  ComputeOneTeamHomeAwayTuples(num_teams, &home_away_tuples);
  std::vector<IntVar*> team_breaks;
  solver.MakeIntVarArray(num_teams, 0, full_season, "team_break_",
                         &team_breaks);
  for (int team = 0; team < num_teams; ++team) {
    AddOneTeamConstraints(&solver, opponents[team], home_aways[team],
                          signed_opponents[team], home_away_tuples,
                          team_breaks[team], num_teams);
  }

  // ----- Search -----

  std::vector<SearchMonitor*> monitors;

  // Objective.
  IntVar* const objective_var =
      solver.MakeSum(team_breaks)->VarWithName("SumOfBreaks");
  OptimizeVar* const objective_monitor = solver.MakeMinimize(objective_var, 1);
  monitors.push_back(objective_monitor);

  // Store all variables in a single array.
  std::vector<IntVar*> all_signed_opponents;
  for (int team_index = 0; team_index < num_teams; ++team_index) {
    for (int day = 0; day < full_season; ++day) {
      all_signed_opponents.push_back(signed_opponents[team_index][day]);
    }
  }

  // Build default phase decision builder.
  DefaultPhaseParameters parameters;
  parameters.run_all_heuristics = FLAGS_run_all_heuristics;
  parameters.heuristic_period = FLAGS_heuristics_period;
  parameters.restart_log_size = FLAGS_restart_log_size;
  DecisionBuilder* const db =
      solver.MakeDefaultPhase(all_signed_opponents, parameters);

  // Search log.
  SearchMonitor* const log = solver.MakeSearchLog(1000000, objective_monitor);
  monitors.push_back(log);

  // Search limit.
  SearchLimit* const limit = solver.MakeTimeLimit(FLAGS_time_limit);
  monitors.push_back(limit);

  // Solution collector.
  SolutionCollector* const collector = solver.MakeLastSolutionCollector();
  for (int team_index = 0; team_index < num_teams; ++team_index) {
    collector->Add(opponents[team_index]);
    collector->Add(home_aways[team_index]);
  }
  monitors.push_back(collector);

  // And search.
  solver.Solve(db, monitors);

  // Display solution.
  if (collector->solution_count() == 1) {
    LOG(INFO) << "Solution found in " << solver.wall_time() << " ms, and "
              << solver.failures() << " failures.";
    for (int team_index = 0; team_index < num_teams; ++team_index) {
      std::string line;
      for (int day = 0; day < full_season; ++day) {
        const int opponent = collector->Value(0, opponents[team_index][day]);
        const int home_away = collector->Value(0, home_aways[team_index][day]);
        line += StringPrintf("%2d%s ", opponent, home_away ? "@" : " ");
      }
      LOG(INFO) << line;
    }
  }
}
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis program runs a sports scheduling problem."
    "There is no output besides the debug LOGs of the solver.";

int main(int argc, char** argv) {
  gflags::SetUsageMessage(kUsage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(0, FLAGS_num_teams % 2) << "The number of teams must be even";
  CHECK_GE(FLAGS_num_teams, 2) << "At least 2 teams";
  CHECK_LT(FLAGS_num_teams, 16) << "The model does not scale beyond 14 teams";
  operations_research::SportsScheduling(FLAGS_num_teams);
  return 0;
}
