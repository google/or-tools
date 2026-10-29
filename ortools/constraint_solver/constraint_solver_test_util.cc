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

#include "ortools/constraint_solver/constraint_solver_test_util.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <numeric>
#include <ostream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

void BaseOperatorTest::MakeVars(Solver* const solver,
                                std::vector<IntVar*>* primary_vars,
                                std::vector<IntVar*>* secondary_vars) const {
  auto make_vars = [solver](const std::vector<int>& first_solution,
                            std::string prefix, std::vector<IntVar*>* vars) {
    if (vars == nullptr || first_solution.empty()) return;
    const int size = first_solution.size();
    const int max =
        *std::max_element(first_solution.begin(), first_solution.end());
    solver->MakeIntVarArray(size, 0, max, prefix, vars);
  };
  make_vars(first_solution_primary_vars_, "primary_vars", primary_vars);
  make_vars(first_solution_secondary_vars_, "secondary_vars", secondary_vars);
}

void BaseOperatorTest::BaseSolveAndTest(
    bool match_all, const std::function<bool()>& on_completion) const {
  Solver solver("Test");
  std::vector<IntVar*> primary_vars;
  std::vector<IntVar*> secondary_vars;
  MakeVars(&solver, &primary_vars, &secondary_vars);
  Assignment assignment(&solver);
  auto set_first_solution = [&assignment](
                                const std::vector<IntVar*>& vars,
                                const std::vector<int>& first_solution) {
    if (first_solution.empty()) return;
    assignment.Add(vars);
    const int size = first_solution.size();
    for (int i = 0; i < size; ++i) {
      assignment.SetValue(vars[i], first_solution[i]);
    }
  };
  LocalSearchOperator* localsearch_operator = nullptr;
  for (int i = 0;
       i < secondary_vars.size() && i < allowed_secondary_values_.size(); ++i) {
    if (!allowed_secondary_values_[i].empty()) {
      secondary_vars[i]->SetValues(allowed_secondary_values_[i]);
    }
  }
  localsearch_operator =
      BuildOperatorWithSecondaryVars(primary_vars, secondary_vars, &solver);
  do {
    assignment.Clear();
    set_first_solution(primary_vars, first_solution_primary_vars_);
    set_first_solution(secondary_vars, first_solution_secondary_vars_);
    localsearch_operator->Start(&assignment);
    if (match_all) {
      OrderedMatch(primary_vars, &solver, localsearch_operator);
    } else {
      UnorderedMatch(primary_vars, &solver, localsearch_operator);
    }
  } while (on_completion != nullptr && on_completion());
}

void BaseOperatorTest::SetFirstSolution(
    const std::vector<int>& first_solution_primary_vars,
    const std::vector<int>& first_solution_secondary_vars) {
  first_solution_primary_vars_ = first_solution_primary_vars;
  first_solution_secondary_vars_ = first_solution_secondary_vars;
}

void BaseOperatorTest::SetAllowedSecondaryValues(
    const std::vector<std::vector<int64_t>>& allowed_secondary_values) {
  allowed_secondary_values_ = allowed_secondary_values;
}

void BaseOperatorTest::AddExpectedDelta(
    absl::Span<const int> expected_delta,
    absl::Span<const int> expected_delta_delta) {
  expected_deltas_.push_back(
      std::vector<int>(expected_delta.begin(), expected_delta.end()));
  expected_delta_deltas_.push_back(std::vector<int>(
      expected_delta_delta.begin(), expected_delta_delta.end()));
}

void BaseOperatorTest::AddExpectedSolution(
    absl::Span<const int> expected_solution) {
  CHECK_EQ(first_solution_primary_vars_.size(), expected_solution.size());
  std::vector<int> expected_delta(first_solution_primary_vars_.size(), -1);
  for (int i = 0; i < first_solution_primary_vars_.size(); ++i) {
    if (first_solution_primary_vars_[i] != expected_solution[i]) {
      expected_delta[i] = expected_solution[i];
    }
  }
  AddExpectedDelta(expected_delta);
}

void BaseOperatorTest::FillSolutionWithPaths(
    std::vector<int>* solution, std::vector<int>* secondary_solution,
    int num_nodes, absl::Span<const std::vector<int>> paths) {
  solution->resize(num_nodes);
  std::iota(solution->begin(), solution->end(), 0);
  if (secondary_solution != nullptr) {
    secondary_solution->assign(num_nodes + paths.size(), -1);
  }
  int path_id = 0;
  for (const auto& path : paths) {
    for (int i = 1; i < path.size(); ++i) {
      CHECK_LE(0, path[i - 1]);
      CHECK_LE(path[i - 1], num_nodes);
      (*solution)[path[i - 1]] = path[i];
      if (secondary_solution != nullptr) {
        (*secondary_solution)[path[i - 1]] = path_id;
      }
    }
    if (secondary_solution != nullptr) {
      (*secondary_solution)[path.back()] = path_id;
      ++path_id;
    }
  }
}

void BaseOperatorTest::SetFirstPaths(int num_nodes,
                                     absl::Span<const std::vector<int>> paths,
                                     bool fill_secondary_vars) {
  FillSolutionWithPaths(
      &first_solution_primary_vars_,
      fill_secondary_vars ? &first_solution_secondary_vars_ : nullptr,
      num_nodes, paths);
}

void BaseOperatorTest::AddExpectedPaths(
    absl::Span<const std::vector<int>> paths) {
  std::vector<int> expected_solution;
  FillSolutionWithPaths(&expected_solution, nullptr,
                        first_solution_primary_vars_.size(), paths);
  AddExpectedSolution(expected_solution);
}

void BaseOperatorTest::SolveAndLogNeighbors(bool log_delta_only) const {
  Solver solver("Test");
  std::vector<IntVar*> primary_vars;
  MakeVars(&solver, &primary_vars, nullptr);
  Assignment assignment(&solver);
  assignment.Add(primary_vars);
  const int size = first_solution_primary_vars_.size();
  for (int i = 0; i < size; ++i) {
    assignment.SetValue(primary_vars[i], first_solution_primary_vars_[i]);
  }
  LocalSearchOperator* localsearch_operator =
      BuildOperator(primary_vars, &solver);
  localsearch_operator->Start(&assignment);
  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  int num_neighbors = 0;
  while (localsearch_operator->MakeNextNeighbor(&delta, &deltadelta)) {
    std::string out = absl::StrCat("Neighbor #", num_neighbors, ":");
    for (int var_index = 0; var_index < size; ++var_index) {
      IntVar* const var = primary_vars[var_index];
      if (delta.Contains(var)) {
        absl::StrAppend(&out, " [", var_index, "]=", delta.Value(var));
      } else if (!log_delta_only) {
        absl::StrAppend(&out, " [", var_index, "]=", assignment.Value(var));
      }
    }
    LOG(INFO) << out;
    delta.Clear();
    deltadelta.Clear();
    ++num_neighbors;
  }
  LOG(INFO) << "Found a total of " << num_neighbors << " neighbors.";
}

void BaseOperatorTest::OrderedMatch(
    const std::vector<IntVar*>& vars, Solver* solver,
    LocalSearchOperator* localsearch_operator) const {
  Assignment delta(solver);
  Assignment deltadelta(solver);
  for (int i = 0; i < expected_deltas_.size(); ++i) {
    const std::vector<int>& expected_delta = expected_deltas_[i];
    const std::vector<int>& expected_delta_delta = expected_delta_deltas_[i];
    if (expected_delta_delta.empty()) delta.Clear();
    ASSERT_TRUE(localsearch_operator->MakeNextNeighbor(&delta, &deltadelta))
        << "Expected #neighbors: " << expected_deltas_.size()
        << " actual #neighbors: " << i;
    if (expected_delta.empty()) {
      EXPECT_TRUE(delta.Empty());
      EXPECT_TRUE(deltadelta.Empty());
    } else {
      for (int j = 0; j < expected_delta.size(); ++j) {
        if (expected_delta[j] >= 0) {
          EXPECT_TRUE(delta.Contains(vars[j]))
              << "Delta number: " << i << " var: " << j << " "
              << delta.DebugString();
          EXPECT_EQ(delta.Value(vars[j]), expected_delta[j])
              << "Delta number: " << i << " var: " << j;
        } else {
          EXPECT_FALSE(delta.Contains(vars[j]))
              << "Delta number: " << i << " var: " << j << " "
              << delta.DebugString();
        }
      }
      for (int j = 0; j < expected_delta_delta.size(); ++j) {
        if (expected_delta_delta[j] >= 0) {
          EXPECT_TRUE(deltadelta.Contains(vars[j]))
              << "Deltadelta number: " << i << " var: " << j << " "
              << deltadelta.DebugString();
          EXPECT_EQ(deltadelta.Value(vars[j]), expected_delta_delta[j])
              << "Deltadelta number: " << i << " var: " << j;
        } else {
          EXPECT_FALSE(deltadelta.Contains(vars[j]))
              << "Deltadelta number: " << i << " var: " << j << " "
              << deltadelta.DebugString();
        }
      }
    }
    deltadelta.Clear();
  }
  EXPECT_FALSE(localsearch_operator->MakeNextNeighbor(&delta, &deltadelta))
      << delta.DebugString();
}

void BaseOperatorTest::UnorderedMatch(
    const std::vector<IntVar*>& vars, Solver* solver,
    LocalSearchOperator* localsearch_operator) const {
  Assignment delta(solver);
  Assignment deltadelta(solver);
  for (int i = 0; i < expected_deltas_.size(); ++i) {
    EXPECT_TRUE(localsearch_operator->MakeNextNeighbor(&delta, &deltadelta))
        << "Delta number: " << i;
    bool found = false;
    for (int j = 0; j < expected_deltas_.size(); ++j) {
      const std::vector<int>& expected_delta = expected_deltas_[j];
      bool match = true;
      for (int k = 0; k < expected_delta.size(); ++k) {
        if (expected_delta[j] >= 0) {
          if (!delta.Contains(vars[j]) ||
              delta.Value(vars[j]) != expected_delta[j]) {
            match = false;
            break;
          }
        } else {
          if (delta.Contains(vars[j])) {
            match = false;
            break;
          }
        }
      }
      if (match) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << i << " " << delta.DebugString();
    delta.Clear();
  }
}

ExpectedSolutionsSearchMonitor::ExpectedSolutionsSearchMonitor(
    Solver* const s, const std::vector<const Assignment*>& expected_solutions)
    : SearchMonitor(s),
      expected_solutions_(expected_solutions),
      next_solution_index_(0) {
  CHECK(!expected_solutions_.empty()) << "Expected solutions can not be empty.";
}

ExpectedSolutionsSearchMonitor::ExpectedSolutionsSearchMonitor(Solver* const s)
    : SearchMonitor(s), next_solution_index_(0) {}

void ExpectedSolutionsSearchMonitor::AddExpectedSolution(
    const Assignment* solution) {
  CHECK_EQ(0, next_solution_index_)
      << "All calls to AddExpectedSolution need to be made "
      << "before the call to Solve()";
  expected_solutions_.push_back(solution);
}

bool ExpectedSolutionsSearchMonitor::AtSolution() {
  if (next_solution_index_ < expected_solutions_.size() &&
      IsExpectedAssignment(expected_solutions_[next_solution_index_])) {
    next_solution_index_++;
  }
  return next_solution_index_ == expected_solutions_.size();
}

bool ExpectedSolutionsSearchMonitor::AllExpectedSolutionsReceived() {
  LOG(INFO) << "Current solution index" << next_solution_index_
            << " expected solutions size " << expected_solutions_.size();
  return next_solution_index_ == expected_solutions_.size();
}

bool ExpectedSolutionsSearchMonitor::IsExpectedAssignment(
    const Assignment* assignment) {
  // TODO(user): Do the same thing on IntervalVarContainer.
  const Assignment::IntContainer& container = assignment->IntVarContainer();
  for (int i = 0; i < container.Size(); ++i) {
    const IntVarElement& element = container.Element(i);
    IntVar* var = container.Element(i).Var();
    EXPECT_TRUE(var->Bound()) << "Variable " << var->name() << " is not bound";
    if (var->Min() != element.Value()) {
      VLOG(1) << "At expected solution " << next_solution_index_ << " variable "
              << var->DebugString() << " has " << std::endl
              << "value: " << var->Min() << std::endl
              << "expected value:" << element.Value();
      return false;
    }
  }
  return true;
}

}  // namespace operations_research
