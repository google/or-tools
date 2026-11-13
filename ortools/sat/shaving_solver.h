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

#include "absl/base/attributes.h"
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
#include "ortools/sat/util.h"
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

  // Allows controlling the local time limit in addition to a potential user-
  // defined external Boolean.
  std::atomic<bool> stop_current_chunk_;

  // Local singleton repository and presolved local model.
  std::unique_ptr<Model> local_model_;
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
                         SharedClasses* shared);

  ~VariablesShavingSolver() override;

  bool TaskIsAvailable() override;

  void ProcessLocalResponse(const CpSolverResponse& local_response,
                            const State& state);

  std::function<void()> GenerateTask(int64_t task_id) override;

  void Synchronize() override;

 private:
  friend class VariablesShavingSolverTestPeer;

  class ShavingGraph {
   public:
    struct NoOverlap2DPair {
      int constraint;
      int x_interval;
      int y_interval;
    };

    struct Neighborhood {
      uint32_t epoch = 0;
      int num_active_variables = 0;
      int num_active_constraints = 0;
    };

    explicit ShavingGraph(const CpModelProto& model_proto);

    // The returned neighborhood is valid until the next call to Extract().
    Neighborhood Extract(int root_var, const std::vector<Domain>& domains,
                         int max_variables, int max_constraints);

    bool IsConstraintActive(int c, const Neighborhood& neighborhood) const;
    bool IsNoOverlap2DConstraintActive(int c,
                                       const Neighborhood& neighborhood) const;
    bool IsNoOverlap2DPairActive(int pair_id,
                                 const Neighborhood& neighborhood) const;

    const std::vector<int>& active_no_overlap_2d_constraints() const {
      return active_no_overlap_2d_constraints_;
    }

    const std::vector<NoOverlap2DPair>& no_overlap_2d_pairs() const {
      return no_overlap_2d_pairs_;
    }

    const CompactVectorVector<int, int>& constraint_to_no_overlap_2d_pairs()
        const {
      return constraint_to_no_overlap_2d_pairs_;
    }

   private:
    bool ConstraintIsInactive(int c, const std::vector<Domain>& domains) const;

    const CpModelProto& model_proto_;
    CompactVectorVector<int, int> var_to_constraints_;
    CompactVectorVector<int, int> constraint_to_variables_;
    CompactVectorVector<int, int> interval_to_constraints_;
    CompactVectorVector<int, int> constraint_to_intervals_;
    CompactVectorVector<int, int> interval_to_no_overlap_2d_pairs_;
    CompactVectorVector<int, int> no_overlap_2d_pair_to_variables_;
    CompactVectorVector<int, int> constraint_to_no_overlap_2d_pairs_;
    std::vector<NoOverlap2DPair> no_overlap_2d_pairs_;

    // Reused stamps avoid initializing model-sized visited arrays per shave.
    std::vector<uint32_t> selected_variables_epoch_;
    std::vector<uint32_t> queued_variables_epoch_;
    std::vector<uint32_t> queued_intervals_epoch_;
    std::vector<uint32_t> active_constraints_epoch_;
    std::vector<uint32_t> active_no_overlap_2d_constraints_epoch_;
    std::vector<uint32_t> active_no_overlap_2d_pairs_epoch_;
    std::vector<int> active_no_overlap_2d_constraints_;
    uint32_t current_epoch_ = 0;
  };

  std::string Info();

  int64_t DomainSize(int var) const ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool VarIsFixed(int int_var) const ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool FindNextVar(State* state) ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  // Returns false if the model is infeasible.
  ABSL_MUST_USE_RESULT bool CopyModelNeighborhood(State* state,
                                                  Model* local_model,
                                                  CpModelProto* shaving_proto,
                                                  bool* has_no_overlap_2d)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  bool ResetAndSolveModel(int64_t task_id, State* state, Model* local_model,
                          CpModelProto* shaving_proto);

  // This is fixed at construction.
  SatParameters local_params_;
  SharedClasses* shared_;
  int shared_bounds_id_ = -1;

  // Allows controlling the local time limit in addition to a potential user-
  // defined external Boolean.
  std::atomic<bool> stop_current_chunk_;

  const CpModelProto& model_proto_;
  ShavingGraph graph_;

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
