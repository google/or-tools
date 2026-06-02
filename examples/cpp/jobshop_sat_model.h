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

#ifndef THIRD_PARTY_ORTOOLS_EXAMPLES_CPP_JOBSHOP_SAT_MODEL_H_
#define THIRD_PARTY_ORTOOLS_EXAMPLES_CPP_JOBSHOP_SAT_MODEL_H_

#include <cstdint>
#include <vector>

#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/scheduling/jobshop_scheduling.pb.h"

namespace operations_research {
namespace sat {

// Tasks or alternative tasks are added to machines one by one.
// This structure records the characteristics of each task added on a machine.
// This information is indexed on each vector by the order of addition.
struct MachineTaskData {
  int job;
  IntervalVar interval;
  int64_t fixed_duration;
};

// A job is a sequence of tasks. For each task, we store the main interval, as
// well as its start, size, and end expressions.
struct JobTaskData {
  IntervalVar interval;
  LinearExpr start;
  LinearExpr duration;
  LinearExpr end;
};

struct JobshopModelMapping {
  std::vector<std::vector<JobTaskData>> job_to_tasks;
  std::vector<std::vector<MachineTaskData>> machine_to_tasks;
};

// Build a CP-SAT model for the given job-shop problem. Returns the model
// alongside a mapping that links the model variables and expressions to the
// problem data.
struct JobshopModelOptions {
  bool use_optional_variables = false;
  bool use_interval_makespan = false;
  bool use_variable_duration_to_encode_transition = false;
  bool use_cumulative_relaxation = true;
  int64_t horizon = -1;  // If -1, compute the horizon from the problem.
};
JobshopModelMapping CreateJobshopModel(
    const operations_research::scheduling::jssp::JsspInputProblem& problem,
    const JobshopModelOptions& options, CpModelBuilder* builder);

}  // namespace sat
}  // namespace operations_research

#endif  // THIRD_PARTY_ORTOOLS_EXAMPLES_CPP_JOBSHOP_SAT_MODEL_H_
