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

// Sports scheduling problem.
//
// We want to solve the problem of scheduling of team matches in a
// double round robin tournament.  Given a number of teams, we want
// each team to encounter all other teams, twice, once at home, and
// once away. Furthermore, you cannot meet the same team twice in the
// same half-season.
//
// Finally, there are constraints on the sequence of home or aways:
//  - You cannot have 3 consecutive homes or three consecutive aways.
//  - A break is a sequence of two homes or two aways, the overall objective
//    of the optimization problem is to minimize the total number of breaks.
//  - If team A meets team B, the reverse match cannot happen less that 6 weeks
//    after.
//
// In the opponent model, we use three matrices of variables, each with
// num_teams rows and 2*(num_teams - 1) columns: the var at position [i][j]
// corresponds to the match of team #i at day #j. There are
// 2*(num_teams - 1) columns because each team meets num_teams - 1
// opponents twice.
//
// - The 'opponent' var [i][j] is the index of the opposing team.
// - The 'home_away' var [i][j] is a boolean: 1 for 'playing away',
//   0 for 'playing at home'.
// - The 'signed_opponent' var [i][j] is the 'opponent' var [i][j] +
//   num_teams * the 'home_away' var [i][j].
//
// In the fixture model, we have a cube of Boolean variables fixtures.
//   fixtures[d][i][j] is true if team i plays team j at home on day d.
// We also introduces a variable at_home[d][i] which is true if team i
// plays any opponent at home on day d.

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"

// Problem main flags.
ABSL_FLAG(int, num_teams, 10, "Number of teams in the problem.");
ABSL_FLAG(std::string, params,
          "log_search_progress:true,max_time_in_seconds:20", "Sat parameters.");
ABSL_FLAG(int, model, 1, "1 = opponent model, 2 = fixture model");

namespace operations_research {
namespace sat {

void OpponentModel(int num_teams) {
  const int num_days = 2 * num_teams - 2;
  const int kNoRematch = 6;

  CpModelBuilder builder;

  // Calendar variables.
  std::vector<std::vector<IntVar>> opponents(num_teams);
  std::vector<std::vector<BoolVar>> home_aways(num_teams);
  std::vector<std::vector<IntVar>> signed_opponents(num_teams);

  for (int t = 0; t < num_teams; ++t) {
    for (int d = 0; d < num_days; ++d) {
      Domain opponent_domain(0, num_teams - 1);
      Domain signed_opponent_domain(0, 2 * num_teams - 1);
      IntVar opp = builder.NewIntVar(opponent_domain)
                       .WithName(absl::StrCat("opponent_", t, "_", d));
      BoolVar home =
          builder.NewBoolVar().WithName(absl::StrCat("home_aways", t, "_", d));
      IntVar signed_opp =
          builder.NewIntVar(signed_opponent_domain)
              .WithName(absl::StrCat("signed_opponent_", t, "_", d));

      opponents[t].push_back(opp);
      home_aways[t].push_back(home);
      signed_opponents[t].push_back(signed_opp);

      // One team cannot meet itself.
      builder.AddNotEqual(opp, t);
      builder.AddNotEqual(signed_opp, t);
      builder.AddNotEqual(signed_opp, t + num_teams);

      // Link opponent, home_away, and signed_opponent.
      builder.AddEquality(opp, signed_opp).OnlyEnforceIf(Not(home));
      builder.AddEquality(LinearExpr(opp).AddConstant(num_teams), signed_opp)
          .OnlyEnforceIf(home);
    }
  }

  // One day constraints.
  for (int d = 0; d < num_days; ++d) {
    std::vector<IntVar> day_opponents;
    std::vector<IntVar> day_home_aways;
    for (int t = 0; t < num_teams; ++t) {
      day_opponents.push_back(opponents[t][d]);
      day_home_aways.push_back(home_aways[t][d]);
    }

    builder.AddInverseConstraint(day_opponents, day_opponents);

    for (int first_team = 0; first_team < num_teams; ++first_team) {
      IntVar first_home = day_home_aways[first_team];
      IntVar second_home = builder.NewBoolVar();
      builder.AddVariableElement(day_opponents[first_team], day_home_aways,
                                 second_home);
      builder.AddEquality(LinearExpr::Sum({first_home, second_home}), 1);
    }

    builder.AddEquality(LinearExpr::Sum(day_home_aways), num_teams / 2);
  }

  // One team constraints.
  for (int t = 0; t < num_teams; ++t) {
    builder.AddAllDifferent(signed_opponents[t]);
    const std::vector<IntVar> first_part(opponents[t].begin(),
                                         opponents[t].begin() + num_teams - 1);
    builder.AddAllDifferent(first_part);
    const std::vector<IntVar> second_part(opponents[t].begin() + num_teams - 1,
                                          opponents[t].end());

    builder.AddAllDifferent(second_part);

    for (int day = num_teams - kNoRematch; day < num_teams - 1; ++day) {
      const std::vector<IntVar> moving(opponents[t].begin() + day,
                                       opponents[t].begin() + day + kNoRematch);
      builder.AddAllDifferent(moving);
    }

    builder.AddEquality(LinearExpr::BooleanSum(home_aways[t]), num_teams - 1);

    // Forbid sequence of 3 homes or 3 aways.
    for (int start = 0; start < num_days - 2; ++start) {
      builder.AddBoolOr({home_aways[t][start], home_aways[t][start + 1],
                         home_aways[t][start + 2]});
      builder.AddBoolOr({Not(home_aways[t][start]),
                         Not(home_aways[t][start + 1]),
                         Not(home_aways[t][start + 2])});
    }
  }

  // Objective.
  std::vector<BoolVar> breaks;
  for (int t = 0; t < num_teams; ++t) {
    for (int d = 0; d < num_days - 1; ++d) {
      BoolVar break_var =
          builder.NewBoolVar().WithName(absl::StrCat("break_", t, "_", d));
      builder.AddBoolOr(
          {Not(home_aways[t][d]), Not(home_aways[t][d + 1]), break_var});
      builder.AddBoolOr({home_aways[t][d], home_aways[t][d + 1], break_var});
      breaks.push_back(break_var);
    }
  }

  builder.Minimize(LinearExpr::BooleanSum(breaks));

  Model model;
  if (!absl::GetFlag(FLAGS_params).empty()) {
    model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));
  }

  const CpSolverResponse response = SolveCpModel(builder.Build(), &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    for (int t = 0; t < num_teams; ++t) {
      std::string output;
      for (int d = 0; d < num_days; ++d) {
        const int opponent = SolutionIntegerValue(response, opponents[t][d]);
        const bool home = SolutionBooleanValue(response, home_aways[t][d]);
        if (home) {
          absl::StrAppendFormat(&output, " %2d@", opponent);
        } else {
          absl::StrAppendFormat(&output, " %2d ", opponent);
        }
      }
      LOG(INFO) << output;
    }
  }
}

void FixtureModel(int num_teams) {
  const int num_days = 2 * num_teams - 2;
  //  const int kNoRematch = 6;
  const int matches_per_day = num_teams - 1;

  CpModelBuilder builder;

  // Does team i receive team j at home on day d?
  std::vector<std::vector<std::vector<BoolVar>>> fixtures(num_days);
  for (int d = 0; d < num_days; ++d) {
    fixtures[d].resize(num_teams);
    for (int i = 0; i < num_teams; ++i) {
      fixtures[d][i].resize(num_teams);
      for (int j = 0; j < num_teams; ++j) {
        if (i == j) {
          fixtures[d][i][i] = builder.FalseVar();
        } else {
          fixtures[d][i][j] = builder.NewBoolVar();
        }
      }
    }
  }

  // Is team t at home on day d?
  std::vector<std::vector<BoolVar>> at_home(num_days);
  for (int d = 0; d < num_days; ++d) {
    for (int t = 0; t < num_teams; ++t) {
      at_home[d].push_back(builder.NewBoolVar());
    }
  }

  // Each day, Team t plays another team, either at home or away.
  for (int d = 0; d < num_days; ++d) {
    for (int team = 0; team < num_teams; ++team) {
      std::vector<BoolVar> possible_opponents;
      for (int other = 0; other < num_teams; ++other) {
        if (team == other) continue;
        possible_opponents.push_back(fixtures[d][team][other]);
        possible_opponents.push_back(fixtures[d][other][team]);
      }
      builder.AddEquality(LinearExpr::BooleanSum(possible_opponents), 1);
    }
  }

  // Each fixture happens once per season.
  for (int team = 0; team < num_teams; ++team) {
    for (int other = 0; other < num_teams; ++other) {
      if (team == other) continue;
      std::vector<BoolVar> possible_days;
      for (int d = 0; d < num_days; ++d) {
        possible_days.push_back(fixtures[d][team][other]);
      }
      builder.AddEquality(LinearExpr::BooleanSum(possible_days), 1);
    }
  }

  // Meet each opponent once per season.
  for (int team = 0; team < num_teams; ++team) {
    for (int other = 0; other < num_teams; ++other) {
      if (team == other) continue;
      std::vector<BoolVar> first_half;
      std::vector<BoolVar> second_half;
      for (int d = 0; d < matches_per_day; ++d) {
        first_half.push_back(fixtures[d][team][other]);
        first_half.push_back(fixtures[d][other][team]);
        second_half.push_back(fixtures[d + matches_per_day][team][other]);
        second_half.push_back(fixtures[d + matches_per_day][other][team]);
      }
      builder.AddEquality(LinearExpr::BooleanSum(first_half), 1);
      builder.AddEquality(LinearExpr::BooleanSum(second_half), 1);
    }
  }

  // Maintain at_home[day][team].
  for (int d = 0; d < num_days; ++d) {
    for (int team = 0; team < num_teams; ++team) {
      for (int other = 0; other < num_teams; ++other) {
        if (team == other) continue;
        builder.AddImplication(fixtures[d][team][other], at_home[d][team]);
        builder.AddImplication(fixtures[d][team][other],
                               Not(at_home[d][other]));
      }
    }
  }

  // Forbid sequence of 3 homes or 3 aways.
  for (int team = 0; team < num_teams; ++team) {
    for (int d = 0; d < num_days - 2; ++d) {
      builder.AddBoolOr(
          {at_home[d][team], at_home[d + 1][team], at_home[d + 2][team]});
      builder.AddBoolOr({Not(at_home[d][team]), Not(at_home[d + 1][team]),
                         Not(at_home[d + 2][team])});
    }
  }

  // Objective.
  std::vector<BoolVar> breaks;
  for (int t = 0; t < num_teams; ++t) {
    for (int d = 0; d < num_days - 1; ++d) {
      BoolVar break_var = builder.NewBoolVar();
      builder.AddBoolOr(
          {Not(at_home[d][t]), Not(at_home[d + 1][t]), break_var});
      builder.AddBoolOr({at_home[d][t], at_home[d + 1][t], break_var});
      builder.AddBoolOr(
          {Not(at_home[d][t]), at_home[d + 1][t], Not(break_var)});
      builder.AddBoolOr(
          {at_home[d][t], Not(at_home[d + 1][t]), Not(break_var)});
      breaks.push_back(break_var);
    }
  }

  builder.AddGreaterOrEqual(LinearExpr::BooleanSum(breaks), 2 * num_teams - 4);

  builder.Minimize(LinearExpr::BooleanSum(breaks));

  Model model;
  if (!absl::GetFlag(FLAGS_params).empty()) {
    model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));
  }

  const CpSolverResponse response = SolveCpModel(builder.Build(), &model);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis program runs a sports scheduling problem."
    "There is no output besides the LOGs of the solver.";

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(kUsage);
  absl::ParseCommandLine(argc, argv);
  CHECK_EQ(0, absl::GetFlag(FLAGS_num_teams) % 2)
      << "The number of teams must be even";
  CHECK_GE(absl::GetFlag(FLAGS_num_teams), 2) << "At least 2 teams";
  if (absl::GetFlag(FLAGS_model) == 1) {
    operations_research::sat::OpponentModel(absl::GetFlag(FLAGS_num_teams));
  } else {
    operations_research::sat::FixtureModel(absl::GetFlag(FLAGS_num_teams));
  }
  return EXIT_SUCCESS;
}
