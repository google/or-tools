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

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "examples/cpp/jobshop_sat_model.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/wrappers.pb.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/types.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/scheduling/jobshop_scheduling.pb.h"
#include "ortools/scheduling/jobshop_scheduling_parser.h"

ABSL_FLAG(std::string, input, "", "Jobshop data file name.");
ABSL_FLAG(std::string, params, "", "Sat parameters in text proto format.");
ABSL_FLAG(bool, use_optional_variables, false,
          "Whether we use optional variables for bounds of an optional "
          "interval or not.");
ABSL_FLAG(bool, use_interval_makespan, false,
          "Whether we encode the makespan using an interval or not.");
ABSL_FLAG(bool, use_variable_duration_to_encode_transition, true,
          "Whether we move the transition cost to the alternative duration.");
ABSL_FLAG(
    bool, use_cumulative_relaxation, true,
    "Whether we regroup multiple machines to create a cumulative relaxation.");
ABSL_FLAG(bool, display_model, false, "Display jobshop proto before solving.");
ABSL_FLAG(bool, display_sat_model, false, "Display sat proto before solving.");
ABSL_FLAG(int64_t, horizon, -1, "Override horizon computation.");

using operations_research::scheduling::jssp::Job;
using operations_research::scheduling::jssp::JobPrecedence;
using operations_research::scheduling::jssp::JsspInputProblem;
using operations_research::scheduling::jssp::Machine;
using operations_research::scheduling::jssp::Task;
using operations_research::scheduling::jssp::TransitionTimeMatrix;

namespace operations_research {
namespace sat {

// Solve a JobShop scheduling problem using CP-SAT.
void Solve(const JsspInputProblem& problem) {
  if (absl::GetFlag(FLAGS_display_model)) {
    LOG(INFO) << problem;
  }

  JobshopModelOptions options;
  options.use_optional_variables = absl::GetFlag(FLAGS_use_optional_variables);
  options.use_interval_makespan = absl::GetFlag(FLAGS_use_interval_makespan);
  options.use_variable_duration_to_encode_transition =
      absl::GetFlag(FLAGS_use_variable_duration_to_encode_transition);
  options.use_cumulative_relaxation =
      absl::GetFlag(FLAGS_use_cumulative_relaxation);
  options.horizon = absl::GetFlag(FLAGS_horizon);

  CpModelBuilder cp_model;
  const JobshopModelMapping model =
      CreateJobshopModel(problem, options, &cp_model);

  if (absl::GetFlag(FLAGS_display_sat_model)) {
    LOG(INFO) << cp_model.Proto();
  }

  // Setup parameters.
  SatParameters parameters;
  parameters.set_log_search_progress(true);

  // Tells the solver we have a makespan objective.
  // Also take decision based on precedence, this usually work better.
  parameters.set_push_all_tasks_toward_start(true);
  parameters.set_use_dynamic_precedence_in_disjunctive(true);

  // Parse the --params flag.
  if (!absl::GetFlag(FLAGS_params).empty()) {
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_params), &parameters))
        << absl::GetFlag(FLAGS_params);
  }

  // A good default set of workers as of Jul 2026.
  if (parameters.subsolvers().empty() && parameters.num_workers() == 16) {
    LOG(INFO) << "Overriding the set of subsolvers for 16 workers.";
    parameters.add_subsolvers("fixed_no_lp");
    parameters.add_subsolvers("max_lp");
    parameters.add_subsolvers("no_lp");
    parameters.add_subsolvers("objective_shaving_no_lp");
    parameters.add_subsolvers("objective_shaving_max_lp");
    parameters.add_subsolvers("probing_no_lp");
    parameters.add_subsolvers("pseudo_costs");
    parameters.add_subsolvers("quick_restart_no_lp");
    parameters.add_subsolvers("reduced_costs");
    parameters.add_subsolvers("shaving_no_lp");
    parameters.add_subsolvers("shaving_max_lp");
  }

  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), parameters);

  // Abort if we don't have any solution.
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.status() != CpSolverStatus::FEASIBLE)
    return;

  const int num_jobs = problem.jobs_size();

  // Check transitions.
  {
    const int num_machines = problem.machines_size();
    for (int m = 0; m < num_machines; ++m) {
      if (!problem.machines(m).has_transition_time_matrix()) continue;

      struct Data {
        int job;
        int64_t fixed_duration;
        int64_t start;
        int64_t end;
      };
      std::vector<Data> schedule;
      for (const MachineTaskData& d : model.machine_to_tasks[m]) {
        if (!SolutionBooleanValue(response, d.interval.PresenceBoolVar())) {
          continue;
        }
        schedule.push_back(
            {d.job, d.fixed_duration,
             SolutionIntegerValue(response, d.interval.StartExpr()),
             SolutionIntegerValue(response, d.interval.EndExpr())});
      }
      std::sort(schedule.begin(), schedule.end(),
                [](const Data& a, const Data& b) { return a.start < b.start; });

      const TransitionTimeMatrix& transitions =
          problem.machines(m).transition_time_matrix();

      int last_job = -1;
      int64_t last_start = kint64min;
      int64_t last_duration;
      for (const Data& d : schedule) {
        const int64_t transition =
            last_job == -1
                ? 0
                : transitions.transition_time(last_job * num_jobs + d.job);
        CHECK_LE(last_start + last_duration + transition, d.start);
        last_job = d.job;
        last_start = d.start;
        last_duration = d.fixed_duration;
      }
    }
  }

  // Check cost, recompute it from scratch.
  int64_t final_cost = 0;
  if (problem.makespan_cost_per_time_unit() != 0) {
    int64_t makespan = 0;
    for (const std::vector<JobTaskData>& tasks : model.job_to_tasks) {
      const LinearExpr job_end = tasks.back().end;
      makespan = std::max(makespan, SolutionIntegerValue(response, job_end));
    }
    final_cost += makespan * problem.makespan_cost_per_time_unit();
  }

  for (int j = 0; j < num_jobs; ++j) {
    const int64_t early_due_date = problem.jobs(j).early_due_date();
    const int64_t late_due_date = problem.jobs(j).late_due_date();
    const int64_t early_penalty =
        problem.jobs(j).earliness_cost_per_time_unit();
    const int64_t late_penalty = problem.jobs(j).lateness_cost_per_time_unit();
    const int64_t end =
        SolutionIntegerValue(response, model.job_to_tasks[j].back().end);
    if (end < early_due_date && early_penalty != 0) {
      final_cost += (early_due_date - end) * early_penalty;
    }
    if (end > late_due_date && late_penalty != 0) {
      final_cost += (end - late_due_date) * late_penalty;
    }
  }

  // Note that this the objective is a variable of the model, there is actually
  // no strong guarantee that in an intermediate solution, it is packed to its
  // minimum possible value. We do observe this from time to time. The DCHECK is
  // mainly to warn when this happen.
  //
  // TODO(user): Support alternative cost in check.
  const double tolerance = 1e-6;
  DCHECK_GE(response.objective_value(), final_cost - tolerance);
  DCHECK_LE(response.objective_value(), final_cost + tolerance);
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  InitGoogle(argv[0], &argc, &argv, true);

  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please supply a data file with --input=";
  }

  operations_research::scheduling::jssp::JsspParser parser;
  CHECK(parser.ParseFile(absl::GetFlag(FLAGS_input)));
  operations_research::sat::Solve(parser.problem());
  return EXIT_SUCCESS;
}
