// Copyright 2010-2018 Google LLC
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

#include "ortools/sat/cp_model.h"

namespace operations_research {
namespace sat {

void RankingSampleSat() {
  CpModelBuilder cp_model;
  const int kHorizon = 100;
  const int kNumTasks = 4;

  auto add_task_ranking = [&cp_model](const std::vector<IntVar>& starts,
                                      const std::vector<BoolVar>& presences,
                                      const std::vector<IntVar>& ranks) {
    const int num_tasks = starts.size();

    // Creates precedence variables between pairs of intervals.
    std::vector<std::vector<BoolVar>> precedences(num_tasks);
    for (int i = 0; i < num_tasks; ++i) {
      precedences[i].resize(num_tasks);
      for (int j = 0; j < num_tasks; ++j) {
        if (i == j) {
          precedences[i][i] = presences[i];
        } else {
          BoolVar prec = cp_model.NewBoolVar();
          precedences[i][j] = prec;
          cp_model.AddLessOrEqual(starts[i], starts[j]).OnlyEnforceIf(prec);
        }
      }
    }

    // Treats optional intervals.
    for (int i = 0; i < num_tasks - 1; ++i) {
      for (int j = i + 1; j < num_tasks; ++j) {
        // Makes sure that if i is not performed, all precedences are
        // false.
        cp_model.AddImplication(Not(presences[i]), Not(precedences[i][j]));
        cp_model.AddImplication(Not(presences[i]), Not(precedences[j][i]));
        // Makes sure that if j is not performed, all precedences are
        // false.
        cp_model.AddImplication(Not(presences[j]), Not(precedences[i][j]));
        cp_model.AddImplication(Not(presences[i]), Not(precedences[j][i]));
        //  The following bool_or will enforce that for any two intervals:
        //    i precedes j or j precedes i or at least one interval is not
        //        performed.
        cp_model.AddBoolOr({precedences[i][j], precedences[j][i],
                            Not(presences[i]), Not(presences[j])});
        // Redundant constraint: it propagates early that at most one
        // precedence is true.
        cp_model.AddImplication(precedences[i][j], Not(precedences[j][i]));
        cp_model.AddImplication(precedences[j][i], Not(precedences[i][j]));
      }
    }
    // Links precedences and ranks.
    for (int i = 0; i < num_tasks; ++i) {
      LinearExpr sum_of_predecessors(-1);
      for (int j = 0; j < num_tasks; ++j) {
        sum_of_predecessors.AddVar(precedences[j][i]);
      }
      cp_model.AddEquality(ranks[i], sum_of_predecessors);
    }
  };

  std::vector<IntVar> starts;
  std::vector<IntVar> ends;
  std::vector<IntervalVar> intervals;
  std::vector<BoolVar> presences;
  std::vector<IntVar> ranks;

  const Domain horizon(0, kHorizon);
  const Domain possible_ranks(-1, kNumTasks - 1);

  for (int t = 0; t < kNumTasks; ++t) {
    const IntVar start = cp_model.NewIntVar(horizon);
    const IntVar duration = cp_model.NewConstant(t + 1);
    const IntVar end = cp_model.NewIntVar(horizon);
    const BoolVar presence =
        t < kNumTasks / 2 ? cp_model.TrueVar() : cp_model.NewBoolVar();
    const IntervalVar interval =
        cp_model.NewOptionalIntervalVar(start, duration, end, presence);
    const IntVar rank = cp_model.NewIntVar(possible_ranks);

    starts.push_back(start);
    ends.push_back(end);
    intervals.push_back(interval);
    presences.push_back(presence);
    ranks.push_back(rank);
  }

  // Adds NoOverlap constraint.
  cp_model.AddNoOverlap(intervals);

  // Ranks tasks.
  add_task_ranking(starts, presences, ranks);

  // Adds a constraint on ranks.
  cp_model.AddLessThan(ranks[0], ranks[1]);

  // Creates makespan variables.
  const IntVar makespan = cp_model.NewIntVar(horizon);
  for (int t = 0; t < kNumTasks; ++t) {
    cp_model.AddLessOrEqual(ends[t], makespan).OnlyEnforceIf(presences[t]);
  }

  // Create objective: minimize 2 * makespan - 7 * sum of presences.
  // That is you gain 7 by interval performed, but you pay 2 by day of delays.
  LinearExpr objective;
  objective.AddTerm(makespan, 2);
  for (int t = 0; t < kNumTasks; ++t) {
    objective.AddTerm(presences[t], -7);
  }
  cp_model.Minimize(objective);

  // Solving part.
  const CpSolverResponse response = Solve(cp_model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "Optimal cost: " << response.objective_value();
    LOG(INFO) << "Makespan: " << SolutionIntegerValue(response, makespan);
    for (int t = 0; t < kNumTasks; ++t) {
      if (SolutionBooleanValue(response, presences[t])) {
        LOG(INFO) << "task " << t << " starts at "
                  << SolutionIntegerValue(response, starts[t]) << " with rank "
                  << SolutionIntegerValue(response, ranks[t]);
      } else {
        LOG(INFO) << "task " << t << " is not performed and ranked at "
                  << SolutionIntegerValue(response, ranks[t]);
      }
    }
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::RankingSampleSat();

  return EXIT_SUCCESS;
}
