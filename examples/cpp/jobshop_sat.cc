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

#include <math.h>

#include <algorithm>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/join.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/base/strutil.h"
#include "ortools/data/jobshop_scheduling.pb.h"
#include "ortools/data/jobshop_scheduling_parser.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"

DEFINE_string(input, "", "Jobshop data file name.");
DEFINE_string(params, "", "Sat parameters in text proto format.");
DEFINE_bool(use_optional_variables, true,
            "Whether we use optional variables for bounds of an optional "
            "interval or not.");
DEFINE_bool(display_model, false, "Display jobshop proto before solving.");
using operations_research::data::jssp::Job;
using operations_research::data::jssp::JsspInputProblem;
using operations_research::data::jssp::Task;

namespace operations_research {
namespace sat {

// Compute a valid horizon from a problem.
int64 ComputeHorizon(const JsspInputProblem& problem) {
  int64 sum_of_durations = 0;
  int64 max_latest_end = 0;
  int64 max_earliest_start = 0;
  for (const Job& job : problem.jobs()) {
    if (job.has_latest_end()) {
      max_latest_end =
          std::max<int64>(max_latest_end, job.latest_end().value());
    } else {
      max_latest_end = kint64max;
    }
    if (job.has_earliest_start()) {
      max_earliest_start =
          std::max<int64>(max_earliest_start, job.earliest_start().value());
    }
    for (const Task& task : job.tasks()) {
      int64 max_duration = 0;
      for (int64 d : task.duration()) {
        max_duration = std::max(max_duration, d);
      }
      sum_of_durations += max_duration;
    }
  }
  return std::min(max_latest_end, sum_of_durations + max_earliest_start);
  // TODO(user): Uses transitions.
}

// Solve a JobShop scheduling problem using SAT.
void Solve(const JsspInputProblem& problem) {
  if (FLAGS_display_model) {
    LOG(INFO) << problem.DebugString();
  }

  CpModelProto cp_model;
  cp_model.set_name("jobshop_scheduling");

  // Helpers.
  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_optional_variable = [&cp_model](int64 lb, int64 ub, int lit) {
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    var->add_enforcement_literal(lit);
    return index;
  };

  auto new_constant = [&cp_model, &new_variable](int64 v) {
    return new_variable(v, v);
  };

  auto new_interval = [&cp_model](int start, int duration, int end) {
    const int index = cp_model.constraints_size();
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->mutable_interval()->set_start(start);
    ct->mutable_interval()->set_size(duration);
    ct->mutable_interval()->set_end(end);
    return index;
  };

  auto new_optional_interval = [&cp_model](int start, int duration, int end,
                                           int presence) {
    const int index = cp_model.constraints_size();
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(presence);
    ct->mutable_interval()->set_start(start);
    ct->mutable_interval()->set_size(duration);
    ct->mutable_interval()->set_end(end);
    return index;
  };

  auto add_precedence = [&cp_model](int before, int after) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(after);
    lin->add_coeffs(1L);
    lin->add_vars(before);
    lin->add_coeffs(-1L);
    lin->add_domain(0L);
    lin->add_domain(kint64max);
  };

  auto add_conditional_equality = [&cp_model](int left, int right, int lit) {
    ConstraintProto* const ct = cp_model.add_constraints();
    ct->add_enforcement_literal(lit);
    LinearConstraintProto* const lin = ct->mutable_linear();
    lin->add_vars(left);
    lin->add_coeffs(1L);
    lin->add_vars(right);
    lin->add_coeffs(-1L);
    lin->add_domain(0L);
    lin->add_domain(0);
  };

  auto add_sum_equal_one = [&cp_model](const std::vector<int>& vars) {
    LinearConstraintProto* lin = cp_model.add_constraints()->mutable_linear();
    for (const int v : vars) {
      lin->add_vars(v);
      lin->add_coeffs(1L);
    }
    lin->add_domain(1L);
    lin->add_domain(1L);
  };

  auto new_lateness_var = [&cp_model, &new_variable](int var, int64 due_date,
                                                     int64 horizon) {
    CHECK_LE(due_date, horizon);
    if (due_date == 0) {  // Short cut.
      return var;
    }

    // shifted_var = var - due_date.
    const int shifted_var = new_variable(-due_date, horizon - due_date);
    LinearConstraintProto* arg = cp_model.add_constraints()->mutable_linear();
    arg->add_vars(shifted_var);
    arg->add_coeffs(1);
    arg->add_vars(var);
    arg->add_coeffs(-1);
    arg->add_domain(-due_date);
    arg->add_domain(-due_date);

    // lateness_var = max(shifted_var, 0).
    const int lateness_var = new_variable(0, horizon - due_date);
    const int zero_var = new_variable(0, 0);
    IntegerArgumentProto* const int_max =
        cp_model.add_constraints()->mutable_int_max();
    int_max->set_target(lateness_var);
    int_max->add_vars(shifted_var);
    int_max->add_vars(zero_var);
    return lateness_var;
  };

  const int num_jobs = problem.jobs_size();
  const int num_machines = problem.machines_size();
  const int64 horizon = ComputeHorizon(problem);

  std::vector<int> starts;
  std::vector<int> ends;

  const int makespan = new_variable(0, horizon);

  DecisionStrategyProto* const heuristic = cp_model.add_search_strategy();
  heuristic->set_domain_reduction_strategy(
      DecisionStrategyProto::SELECT_MIN_VALUE);
  heuristic->set_variable_selection_strategy(
      DecisionStrategyProto::CHOOSE_LOWEST_MIN);

  CpObjectiveProto* const obj = cp_model.mutable_objective();

  std::vector<std::vector<int>> machine_to_intervals(num_machines);
  for (int j = 0; j < num_jobs; ++j) {
    const Job& job = problem.jobs(j);
    int previous_end = -1;
    const int64 hard_start =
        job.has_earliest_start() ? job.earliest_start().value() : 0L;
    const int64 hard_end =
        job.has_latest_end() ? job.latest_end().value() : horizon;

    for (int t = 0; t < job.tasks_size(); ++t) {
      const Task& task = job.tasks(t);
      const int num_alternatives = task.machine_size();
      CHECK_EQ(num_alternatives, task.duration_size());

      // Add the "main" task interval. It will englobe all the alternative ones
      // if there is many, or be a normal task otherwise.
      int64 min_duration = task.duration(0);
      int64 max_duration = task.duration(0);
      for (int i = 1; i < num_alternatives; ++i) {
        min_duration = std::min<int64>(min_duration, task.duration(i));
        max_duration = std::max<int64>(max_duration, task.duration(i));
      }
      const int start = new_variable(hard_start, hard_end);
      const int duration = new_variable(min_duration, max_duration);
      const int end = new_variable(hard_start, hard_end);
      const int interval = new_interval(start, duration, end);

      // Chain the task belonging to the same job.
      if (previous_end != -1) {
        add_precedence(previous_end, start);
      }
      previous_end = end;

      // Add start to the heuristic.
      heuristic->add_variables(start);

      if (num_alternatives == 1) {
        machine_to_intervals[task.machine(0)].push_back(interval);
      } else {
        std::vector<int> presences;
        for (int a = 0; a < num_alternatives; ++a) {
          const int presence = new_variable(0, 1);
          const int local_start =
              FLAGS_use_optional_variables
                  ? new_optional_variable(hard_start, hard_end, presence)
                  : start;
          const int local_duration = new_constant(task.duration(a));
          const int local_end =
              FLAGS_use_optional_variables
                  ? new_optional_variable(hard_start, hard_end, presence)
                  : end;
          const int local_interval = new_optional_interval(
              local_start, local_duration, local_end, presence);
          // Link local and global variables.
          if (FLAGS_use_optional_variables) {
            add_conditional_equality(start, local_start, presence);
            add_conditional_equality(end, local_end, presence);
            // TODO(user): Experiment with the following implication.
            add_conditional_equality(duration, local_duration, presence);
          }
          // Add local interval to machine.
          machine_to_intervals[task.machine(a)].push_back(local_interval);
          // Collect presence variables.
          presences.push_back(presence);
        }
        add_sum_equal_one(presences);
      }
    }

    // The makespan will be greater than the end of each job.
    if (problem.makespan_cost_per_time_unit() != 0L) {
      add_precedence(previous_end, makespan);
    }

    // Earliness costs are not supported.
    CHECK_EQ(0L, job.earliness_cost_per_time_unit());
    // Lateness cost.
    if (job.lateness_cost_per_time_unit() != 0L) {
      const int lateness_var =
          new_lateness_var(previous_end, job.late_due_date(), horizon);
      obj->add_vars(lateness_var);
      obj->add_coeffs(job.lateness_cost_per_time_unit());
    }
  }

  // Add one no_overlap constraint per machine.
  for (int m = 0; m < num_machines; ++m) {
    NoOverlapConstraintProto* const no_overlap =
        cp_model.add_constraints()->mutable_no_overlap();
    for (const int i : machine_to_intervals[m]) {
      no_overlap->add_intervals(i);
    }
  }

  // Add objective.
  if (problem.makespan_cost_per_time_unit() != 0L) {
    obj->add_coeffs(problem.makespan_cost_per_time_unit());
    obj->add_vars(makespan);
  }
  if (problem.has_scaling_factor()) {
    obj->set_scaling_factor(problem.scaling_factor().value());
  }

  LOG(INFO) << "#machines:" << num_machines;
  LOG(INFO) << "#jobs:" << num_jobs;
  LOG(INFO) << "horizon:" << horizon;

  LOG(INFO) << CpModelStats(cp_model);

  Model model;
  model.Add(NewSatParameters(FLAGS_params));

  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  base::SetFlag(&FLAGS_logtostderr, true);
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_input.empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  operations_research::data::jssp::JsspParser parser;
  CHECK(parser.ParseFile(FLAGS_input));
  operations_research::sat::Solve(parser.problem());
  return EXIT_SUCCESS;
}
