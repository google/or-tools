// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_SAT_FEASIBILITY_JUMP_H_
#define OR_TOOLS_SAT_FEASIBILITY_JUMP_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/sat/constraint_violation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research::sat {

// Implements and heuristic similar to the one described in the paper:
// "Feasibility Jump: an LP-free Lagrangian MIP heuristic", Bjørnar
// Luteberget, Giorgio Sartor, 2023, Mathematical Programming Computation.
//
// TODO(user): If we have more than one of these solver, we might want to share
// the evaluator memory between them. Right now we basically keep a copy of the
// model and its transpose for each FeasibilityJumpSolver.
class FeasibilityJumpSolver : public SubSolver {
 public:
  FeasibilityJumpSolver(const std::string name, SubSolver::SubsolverType type,
                        const CpModelProto& cp_model_proto,
                        SatParameters params,
                        ModelSharedTimeLimit* shared_time_limit,
                        SharedResponseManager* shared_response,
                        SharedBoundsManager* shared_bounds,
                        SharedStatistics* shared_stats)
      : SubSolver(name, type),
        cp_model_(cp_model_proto),
        params_(params),
        shared_time_limit_(shared_time_limit),
        shared_response_(shared_response),
        shared_bounds_(shared_bounds),
        shared_stats_(shared_stats),
        random_(params_) {}

  ~FeasibilityJumpSolver() override {
    if (!VLOG_IS_ON(1)) return;
    std::vector<std::pair<std::string, int64_t>> stats;
    stats.push_back({"fs_jump/num_jumps", num_jumps_});
    stats.push_back({"fs_jump/num_computed_jumps", num_computed_jumps_});
    stats.push_back({"fs_jump/num_updates", num_weight_updates_});
    shared_stats_->AddStats(stats);
  }

  // No synchronization needed for TaskIsAvailable().
  void Synchronize() final {}

  bool IsDone() final {
    // Tricky: we cannot delete something if there is a task in flight, we will
    // have to wait.
    if (task_generated_.load()) return false;

    if (!model_is_supported_.load()) return true;
    if (shared_response_->ProblemIsSolved()) return true;
    if (shared_time_limit_->LimitReached()) return true;

    // We are done after the first task is done in the FIRST_SOLUTION mode.
    return type() == SubSolver::FIRST_SOLUTION &&
           shared_response_->first_solution_solvers_should_stop()->load();
  }

  bool TaskIsAvailable() final {
    if (task_generated_.load()) return false;

    return (shared_response_->SolutionsRepository().NumSolutions() > 0) ==
           (type() == SubSolver::INCOMPLETE);
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) final;

 private:
  void Initialize(const CpModelProto& cp_model_proto);

  // Linear only.
  void RecomputeJump(int var);
  void MarkJumpForRecomputation(int var);
  void RecomputeAllJumps();

  // Moves.
  bool DoSomeLinearIterations();
  bool DoSomeGeneralIterations();

  // Return false if we have no infeasibility and the solution is feasible.
  bool UpdateLinearConstraintWeights();
  bool UpdateAllConstraintWeights();

  const CpModelProto& cp_model_;
  SatParameters params_;
  ModelSharedTimeLimit* shared_time_limit_;
  SharedResponseManager* shared_response_;
  SharedBoundsManager* shared_bounds_ = nullptr;
  SharedStatistics* shared_stats_;
  ModelRandomGenerator random_;

  // We don't support all type of model.
  // This will be set to false in Initialize() or later if we don't.
  bool is_initialized_ = false;
  std::atomic<bool> model_is_supported_ = true;
  std::atomic<bool> task_generated_ = false;

  std::unique_ptr<LsEvaluator> evaluator_;
  std::vector<Domain> var_domains_;

  // The current base solution and its violation (lower is better).
  double solution_score_;
  std::vector<int64_t> solution_;

  // For each variable a value different from the current one in the solution
  // (except if the variable is fixed), and the associated weighted feasibility
  // violation change.
  std::vector<bool> jump_need_recomputation_;
  std::vector<int64_t> jump_values_;
  std::vector<double> jump_deltas_;

  // The score of a solution is just the sum of infeasibility of each
  // constraints weighted by these scores.
  std::vector<double> weights_;
  std::vector<int> last_infeasible_constraints_;

  // We use an exponentially decaying constraint weight like for SAT activities.
  double bump_value_ = 1.0;

  // Sparse list of jump with a delta <= 0.0.
  std::vector<bool> in_good_jumps_;
  std::vector<int> good_jumps_;

  std::vector<int64_t> tmp_breakpoints_;

  // Statistics
  absl::Time last_logging_time_;
  int64_t num_batches_ = 0;
  int64_t num_jumps_ = 0;
  int64_t num_computed_jumps_ = 0;
  int64_t num_weight_updates_ = 0;
};

}  // namespace operations_research::sat

#endif  // OR_TOOLS_SAT_FEASIBILITY_JUMP_H_
