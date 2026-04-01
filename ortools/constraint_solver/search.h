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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_SEARCH_H_
#define ORTOOLS_CONSTRAINT_SOLVER_SEARCH_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/time/time.h"
#include "ortools/base/timer.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

class SymmetryManager;

/// A symmetry breaker is an object that will visit a decision and
/// create the 'symmetrical' decision in return.
/// Each symmetry breaker represents one class of symmetry.
class SymmetryBreaker : public DecisionVisitor {
 public:
  SymmetryBreaker()
      : symmetry_manager_(nullptr), index_in_symmetry_manager_(-1) {}
  ~SymmetryBreaker() override {}

  void AddIntegerVariableEqualValueClause(IntVar* var, int64_t value);
  void AddIntegerVariableGreaterOrEqualValueClause(IntVar* var, int64_t value);
  void AddIntegerVariableLessOrEqualValueClause(IntVar* var, int64_t value);

 private:
  friend class SymmetryManager;
  void set_symmetry_manager_and_index(SymmetryManager* manager, int index) {
    CHECK(symmetry_manager_ == nullptr);
    CHECK_EQ(-1, index_in_symmetry_manager_);
    symmetry_manager_ = manager;
    index_in_symmetry_manager_ = index;
  }
  SymmetryManager* symmetry_manager() const { return symmetry_manager_; }
  int index_in_symmetry_manager() const { return index_in_symmetry_manager_; }

  SymmetryManager* symmetry_manager_;
  /// Index of the symmetry breaker when used inside the symmetry manager.
  int index_in_symmetry_manager_;
};

/// The base class of all search logs that periodically outputs information when
/// the search is running.
class SearchLog : public SearchMonitor {
 public:
  SearchLog(Solver* solver, std::vector<IntVar*> vars, std::string vars_name,
            std::vector<double> scaling_factors, std::vector<double> offsets,
            std::function<std::string()> display_callback,
            bool display_on_new_solutions_only, int period);
  ~SearchLog() override;
  void EnterSearch() override;
  void ExitSearch() override;
  bool AtSolution() override;
  void BeginFail() override;
  void NoMoreSolutions() override;
  void AcceptUncheckedNeighbor() override;
  void ApplyDecision(Decision* decision) override;
  void RefuteDecision(Decision* decision) override;
  void OutputDecision();
  void Maintain();
  void BeginInitialPropagation() override;
  void EndInitialPropagation() override;
  std::string DebugString() const override;

 protected:
  /* Bottleneck function used for all UI related output. */
  virtual void OutputLine(const std::string& line);

 private:
  static std::string MemoryUsage();

  const int period_;
  std::unique_ptr<WallTimer> timer_;
  const std::vector<IntVar*> vars_;
  const std::string vars_name_;
  const std::vector<double> scaling_factors_;
  const std::vector<double> offsets_;
  std::function<std::string()> display_callback_;
  const bool display_on_new_solutions_only_;
  int nsol_;
  absl::Duration tick_;
  std::vector<int64_t> objective_min_;
  std::vector<int64_t> objective_max_;
  std::vector<int64_t> last_objective_value_;
  absl::Duration last_objective_timestamp_;
  int min_right_depth_;
  int max_depth_;
  int sliding_min_depth_;
  int sliding_max_depth_;
  int neighbors_offset_ = 0;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_SEARCH_H_
