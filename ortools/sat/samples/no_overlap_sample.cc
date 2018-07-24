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

void NoOverlapSample() {
  CpModelProto cp_model;
  const int kHorizon = 21;  // 3 weeks.

  auto new_variable = [&cp_model](int64 lb, int64 ub) {
    CHECK_LE(lb, ub);
    const int index = cp_model.variables_size();
    IntegerVariableProto* const var = cp_model.add_variables();
    var->add_domain(lb);
    var->add_domain(ub);
    return index;
  };

  auto new_constant = [&new_variable](int64 v) { return new_variable(v, v); };

  auto new_interval = [&cp_model](int start, int duration, int end) {
    const int index = cp_model.constraints_size();
    IntervalConstraintProto* const interval =
        cp_model.add_constraints()->mutable_interval();
    interval->set_start(start);
    interval->set_size(duration);
    interval->set_end(end);
    return index;
  };

  auto new_fixed_interval = [&cp_model, &new_constant](int64 start,
                                                       int64 duration) {
    const int index = cp_model.constraints_size();
    IntervalConstraintProto* const interval =
        cp_model.add_constraints()->mutable_interval();
    interval->set_start(new_constant(start));
    interval->set_size(new_constant(duration));
    interval->set_end(new_constant(start + duration));
    return index;
  };

  auto add_no_overlap = [&cp_model](const std::vector<int>& intervals) {
    NoOverlapConstraintProto* const no_overlap =
        cp_model.add_constraints()->mutable_no_overlap();
    for (const int i : intervals) {
      no_overlap->add_intervals(i);
    }
  };

  auto add_precedence = [&cp_model](int before, int after) {
    LinearConstraintProto* const lin =
        cp_model.add_constraints()->mutable_linear();
    lin->add_vars(after);
    lin->add_coeffs(1L);
    lin->add_vars(before);
    lin->add_coeffs(-1L);
    lin->add_domain(0);
    lin->add_domain(kint64max);
  };

  // Task 0, duration 2.
  const int start_0 = new_variable(0, kHorizon);
  const int duration_0 = new_constant(2);
  const int end_0 = new_variable(0, kHorizon);
  const int task_0 = new_interval(start_0, duration_0, end_0);

  // Task 1, duration 4.
  const int start_1 = new_variable(0, kHorizon);
  const int duration_1 = new_constant(4);
  const int end_1 = new_variable(0, kHorizon);
  const int task_1 = new_interval(start_1, duration_1, end_1);

  // Task 2, duration 3.
  const int start_2 = new_variable(0, kHorizon);
  const int duration_2 = new_constant(3);
  const int end_2 = new_variable(0, kHorizon);
  const int task_2 = new_interval(start_2, duration_2, end_2);

  // Week ends.
  const int weekend_0 = new_fixed_interval(5, 2);
  const int weekend_1 = new_fixed_interval(12, 2);
  const int weekend_2 = new_fixed_interval(19, 2);

  // No Overlap constraint.
  add_no_overlap({task_0, task_1, task_2, weekend_0, weekend_1, weekend_2});

  // Makespan.
  const int makespan = new_variable(0, kHorizon);
  add_precedence(end_0, makespan);
  add_precedence(end_1, makespan);
  add_precedence(end_2, makespan);
  CpObjectiveProto* const obj = cp_model.mutable_objective();
  obj->add_vars(makespan);
  obj->add_coeffs(1);  // Minimization.

  // Solving part.
  Model model;
  LOG(INFO) << CpModelStats(cp_model);
  const CpSolverResponse response = SolveCpModel(cp_model, &model);
  LOG(INFO) << CpSolverResponseStats(response);

  if (response.status() == CpSolverStatus::OPTIMAL) {
    LOG(INFO) << "Optimal Schedule Length: " << response.objective_value();
    LOG(INFO) << "Task 0 starts at " << response.solution(start_0);
    LOG(INFO) << "Task 1 starts at " << response.solution(start_1);
    LOG(INFO) << "Task 2 starts at " << response.solution(start_2);
  }
}


}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::NoOverlapSample();

  return EXIT_SUCCESS;
}
