// Copyright 2010-2017 Google
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

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void RankingSample() {
  CpModelProto cp_model;
  const int kHorizon = 100;
  const int kNumTasks = 4;

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_constant = [&new_variable](int64 v) { return new_variable(v, v); };

  auto is_fixed_to_true = [&cp_model](int v) {
    return cp_model.variables(v).domain(0) == cp_model.variables(v).domain(1) &&
           cp_model.variables(v).domain_size() == 2;
  };

  auto new_optional_interval = [&cp_model, &is_fixed_to_true](
                                   int start, int duration, int end,
                                   int presence) {
    const int index = cp_model.constraints_size();
    ConstraintProto* const ct = cp_model.add_constraints();
    if (!is_fixed_to_true(presence)) {
      ct->add_enforcement_literal(presence);
    }
    IntervalConstraintProto* const interval = ct->mutable_interval();
    interval->set_start(start);
    interval->set_size(duration);
    interval->set_end(end);
    return index;
  };

  auto add_no_overlap = [&cp_model](const std::vector<int>& intervals) {
    NoOverlapConstraintProto* const no_overlap =
        cp_model.add_constraints()->mutable_no_overlap();
    for (const int i : intervals) {
      no_overlap->add_intervals(i);
    }
  };

  auto add_strict_precedence = [&cp_model](int before, int after) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(after);
    lin->add_coeffs(1L);
    lin->add_vars(before);
    lin->add_coeffs(-1L);
    lin->add_domain(1);
    lin->add_domain(kint64max);
  };

  auto add_conditional_precedence_with_delay = [&cp_model, &is_fixed_to_true](
                                                   int before, int after,
                                                   int literal, int64 delay) {
    ConstraintProto* const ct = cp_model.add_constraints();
    if (!is_fixed_to_true(literal)) {
      ct->add_enforcement_literal(literal);
    }
    LinearConstraintProto* const lin = ct->mutable_linear();
    lin->add_vars(after);
    lin->add_coeffs(1L);
    lin->add_vars(before);
    lin->add_coeffs(-1L);
    lin->add_domain(delay);
    lin->add_domain(kint64max);
  };

  auto add_conditional_precedence = [&add_conditional_precedence_with_delay](
                                        int before, int after, int literal) {
    add_conditional_precedence_with_delay(before, after, literal, 0);
  };

  auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
    BoolArgumentProto* const bool_or =
        cp_model.add_constraints()->mutable_bool_or();
    for (const int lit : literals) {
      bool_or->add_literals(lit);
    }
  };

  auto add_implication = [&cp_model](int a, int b) {
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(a);
    ct->mutable_bool_and()->add_literals(b);
  };

  auto add_task_ranking =
      [&cp_model, &new_variable, &add_conditional_precedence, &is_fixed_to_true,
       &add_implication, &add_bool_or](const std::vector<int>& starts,
                                       const std::vector<int>& presences,
                                       const std::vector<int>& ranks) {
        const int num_tasks = starts.size();

        // Creates precedence variables between pairs of intervals.
        std::vector<std::vector<int>> precedences(num_tasks);
        for (int i = 0; i < num_tasks; ++i) {
          precedences[i].resize(num_tasks);
          for (int j = 0; j < num_tasks; ++j) {
            if (i == j) {
              precedences[i][i] = presences[i];
            } else {
              const int prec = new_variable(0, 1);
              precedences[i][j] = prec;
              add_conditional_precedence(starts[i], starts[j], prec);
            }
          }
        }

        // Treats optional intervals.
        for (int i = 0; i < num_tasks - 1; ++i) {
          for (int j = i + 1; j < num_tasks; ++j) {
            std::vector<int> tmp_array = {precedences[i][j], precedences[j][i]};
            if (!is_fixed_to_true(presences[i])) {
              tmp_array.push_back(NegatedRef(presences[i]));
              // Makes sure that if i is not performed, all precedences are
              // false.
              add_implication(NegatedRef(presences[i]),
                              NegatedRef(precedences[i][j]));
              add_implication(NegatedRef(presences[i]),
                              NegatedRef(precedences[j][i]));
            }
            if (!is_fixed_to_true(presences[j])) {
              tmp_array.push_back(NegatedRef(presences[j]));
              // Makes sure that if j is not performed, all precedences are
              // false.
              add_implication(NegatedRef(presences[j]),
                              NegatedRef(precedences[i][j]));
              add_implication(NegatedRef(presences[i]),
                              NegatedRef(precedences[j][i]));
            }
            //  The following bool_or will enforce that for any two intervals:
            //    i precedes j or j precedes i or at least one interval is not
            //        performed.
            add_bool_or(tmp_array);
            // Redundant constraint: it propagates early that at most one
            // precedence
            // is true.
            add_implication(precedences[i][j], NegatedRef(precedences[j][i]));
            add_implication(precedences[j][i], NegatedRef(precedences[i][j]));
          }
        }
        // Links precedences and ranks.
        for (int i = 0; i < num_tasks; ++i) {
          LinearConstraintProto* const lin =
              cp_model.add_constraints()->mutable_linear();
          lin->add_vars(ranks[i]);
          lin->add_coeffs(1);
          for (int j = 0; j < num_tasks; ++j) {
            lin->add_vars(precedences[j][i]);
            lin->add_coeffs(-1);
          }
          lin->add_domain(-1);
          lin->add_domain(-1);
        }
      };

  std::vector<int> starts;
  std::vector<int> ends;
  std::vector<int> intervals;
  std::vector<int> presences;
  std::vector<int> ranks;

  for (int t = 0; t < kNumTasks; ++t) {
    const int start = new_variable(0, kHorizon);
    const int duration = new_constant(t + 1);
    const int end = new_variable(0, kHorizon);
    const int presence =
        t < kNumTasks / 2 ? new_constant(1) : new_variable(0, 1);
    const int interval = new_optional_interval(start, duration, end, presence);
    const int rank = new_variable(-1, kNumTasks - 1);
    starts.push_back(start);
    ends.push_back(end);
    intervals.push_back(interval);
    presences.push_back(presence);
    ranks.push_back(rank);
  }

  // Adds NoOverlap constraint.
  add_no_overlap(intervals);

  // Ranks tasks.
  add_task_ranking(starts, presences, ranks);

  // Adds a constraint on ranks.
  add_strict_precedence(ranks[0], ranks[1]);

  // Creates makespan variables.
  const int makespan = new_variable(0, kHorizon);
  for (int t = 0; t < kNumTasks; ++t) {
    add_conditional_precedence(ends[t], makespan, presences[t]);
  }

  // Create objective: minimize 2 * makespan - 3 * sum of presences.
  CpObjectiveProto* const obj = cp_model.mutable_objective();
  obj->add_vars(makespan);
  obj->add_coeffs(2);
  for (int t = 0; t < kNumTasks; ++t) {
    obj->add_vars(presences[t]);
    obj->add_coeffs(-7);
  }

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "Optimal cost: " << response.objective_value();
    LOG(INFO) << "Makespan: " << response.solution(makespan);
    for (int t = 0; t < kNumTasks; ++t) {
      if (response.solution(presences[t])) {
        LOG(INFO) << "Tasks " << t << " starts at "
                  << response.solution(starts[t]) << " with rank "
                  << response.solution(ranks[t]);
      } else {
        LOG(INFO) << "Tasks " << t << " is not performed and ranked at "
                  << response.solution(ranks[t]);
      }
    }
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::RankingSample();

  return EXIT_SUCCESS;
}
