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

#ifndef ORTOOLS_SAT_SHAVING_SOLVER_H_
#define ORTOOLS_SAT_SHAVING_SOLVER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/arena.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_lns.h"
#include "ortools/sat/cp_model_solver_helpers.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/subsolver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

class ObjectiveShavingSolver : public SubSolver {
 public:
  ObjectiveShavingSolver(const SatParameters& local_parameters,
                         NeighborhoodGeneratorHelper* helper,
                         SharedClasses* shared);

  ~ObjectiveShavingSolver() override;

  bool TaskIsAvailable() override;

  std::function<void()> GenerateTask(int64_t task_id) override;
  void Synchronize() override;

 private:
  std::string Info();

  void ResetModel();
  bool ResetAndSolveModel(int64_t task_id);

  // This is fixed at construction.
  SatParameters local_params_;
  NeighborhoodGeneratorHelper* helper_;
  SharedClasses* shared_;

  // Allow to control the local time limit in addition to a potential user
  // defined external Boolean.
  std::atomic<bool> stop_current_chunk_;

  // Local singleton repository and presolved local model.
  std::unique_ptr<Model> local_sat_model_;
  std::unique_ptr<google::protobuf::Arena> arena_;
  CpModelProto* local_proto_;

  // For postsolving a feasible solution or improving objective lb.
  std::vector<int> postsolve_mapping_;
  CpModelProto mapping_proto_;

  absl::Mutex mutex_;
  IntegerValue objective_lb_ ABSL_GUARDED_BY(mutex_);
  IntegerValue objective_ub_ ABSL_GUARDED_BY(mutex_);
  IntegerValue current_objective_target_ub_ ABSL_GUARDED_BY(mutex_);
  bool task_in_flight_ ABSL_GUARDED_BY(mutex_) = false;
};

class VariablesShavingSolver : public SubSolver {
 public:
  struct State {
    int var_index;
    bool minimize;

    // We have two modes:
    // - When "shave_using_objective" is true, we shave by minimizing the value
    //   of a variable.
    // - When false, we restrict its domain and detect feasible/infeasible.
    Domain reduced_domain;
    bool shave_using_objective = false;
  };

  VariablesShavingSolver(const SatParameters& local_parameters,
                         NeighborhoodGeneratorHelper* helper,
                         SharedClasses* shared);

  ~VariablesShavingSolver() override;

  bool TaskIsAvailable() override;

  void ProcessLocalResponse(const CpSolverResponse& local_response,
                            const State& state);

  std::function<void()> GenerateTask(int64_t task_id) override;

  void Synchronize() override;

 private:
  std::string Info();

  int64_t DomainSize(int var) const ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool VarIsFixed(int int_var) const ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool ConstraintIsInactive(int c) const ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool FindNextVar(State* state) ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  void CopyModelConnectedToVar(State* state, Model* local_model,
                               CpModelProto* shaving_proto,
                               bool* has_no_overlap_2d)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool ResetAndSolveModel(int64_t task_id, State* state, Model* local_model,
                          CpModelProto* shaving_proto);

  // This is fixed at construction.
  SatParameters local_params_;
  SharedClasses* shared_;
  int shared_bounds_id_ = -1;

  // Allow to control the local time limit in addition to a potential user
  // defined external Boolean.
  std::atomic<bool> stop_current_chunk_;

  const CpModelProto& model_proto_;

  absl::Mutex mutex_;
  int64_t current_index_ = -1;
  std::vector<Domain> var_domains_ ABSL_GUARDED_BY(mutex_);

  // Stats.
  int num_vars_tried_ ABSL_GUARDED_BY(mutex_) = 0;
  int num_vars_shaved_ ABSL_GUARDED_BY(mutex_) = 0;
  int num_infeasible_found_ ABSL_GUARDED_BY(mutex_) = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SHAVING_SOLVER_H_
