// Copyright 2010-2014 Google
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

// This file illustrate the API for Large Neighborhood Search and
// Local Search. It solves the same trivial problem with a Large
// Neighborhood Search approach, a Local Search approach, and a Local
// Search with Filter approach.

#include "ortools/base/commandlineflags.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/base/random.h"

namespace operations_research {
class OneVarLns : public BaseLns {
 public:
  explicit OneVarLns(const std::vector<IntVar*>& vars)
      : BaseLns(vars), index_(0) {}

  ~OneVarLns() override {}

  void InitFragments() override { index_ = 0; }

  bool NextFragment() override {
    const int size = Size();
    if (index_ < size) {
      AppendToFragment(index_);
      ++index_;
      return true;
    } else {
      return false;
    }
  }

 private:
  int index_;
};

class MoveOneVar : public IntVarLocalSearchOperator {
 public:
  explicit MoveOneVar(const std::vector<IntVar*>& variables)
      : IntVarLocalSearchOperator(variables),
        variable_index_(0),
        move_up_(false) {}

  ~MoveOneVar() override {}

 protected:
  // Make a neighbor assigning one variable to its target value.
  bool MakeOneNeighbor() override {
    const int64 current_value = OldValue(variable_index_);
    if (move_up_) {
      SetValue(variable_index_, current_value + 1);
      variable_index_ = (variable_index_ + 1) % Size();
    } else {
      SetValue(variable_index_, current_value - 1);
    }
    move_up_ = !move_up_;
    return true;
  }

 private:
  void OnStart() override {
    CHECK_GE(variable_index_, 0);
    CHECK_LT(variable_index_, Size());
  }

  // Index of the next variable to try to restore
  int64 variable_index_;
  // Direction of the modification.
  bool move_up_;
};

class SumFilter : public IntVarLocalSearchFilter {
 public:
  explicit SumFilter(const std::vector<IntVar*>& vars)
      : IntVarLocalSearchFilter(vars), sum_(0) {}

  ~SumFilter() override {}

  void OnSynchronize(const Assignment* delta) override {
    sum_ = 0;
    for (int index = 0; index < Size(); ++index) {
      sum_ += Value(index);
    }
  }

  bool Accept(const Assignment* delta,
              const Assignment* unused_deltadelta) override {
    const Assignment::IntContainer& solution_delta = delta->IntVarContainer();
    const int solution_delta_size = solution_delta.Size();

    // The input const Assignment* delta given to Accept() may
    // actually contain "Deactivated" elements, which represent
    // variables that have been freed -- they are not bound to a
    // single value anymore. This happens with LNS-type (Large
    // Neighborhood Search) LocalSearchOperator, which are not used in
    // this example as of 2012-01; and we refer the reader to
    // ./routing.cc for an example of such LNS-type operators.
    //
    // For didactical purposes, we will assume for a moment that a
    // LNS-type operator might be applied. The Filter will still be
    // called, but our filter here won't be able to work, since
    // it needs every variable to be bound (i.e. have a fixed value),
    // in the assignment that it considers. Therefore, we include here
    // a snippet of code that will detect if the input assignment is
    // not fully bound. For further details, read ./routing.cc -- but
    // we strongly advise the reader to first try and understand all
    // of this file.
    for (int i = 0; i < solution_delta_size; ++i) {
      if (!solution_delta.Element(i).Activated()) {
        VLOG(1) << "Element #" << i << " of the delta assignment given to"
                << " SumFilter::Accept() is not activated (i.e. its variable"
                << " is not bound to a single value anymore). This means that"
                << " we are in a LNS phase, and the DobbleFilter won't be able"
                << " to filter anything. Returning true.";
        return true;
      }
    }
    int64 new_sum = sum_;
    VLOG(1) << "No LNS, size = " << solution_delta_size;
    for (int index = 0; index < solution_delta_size; ++index) {
      int64 touched_var = -1;
      FindIndex(solution_delta.Element(index).Var(), &touched_var);
      const int64 old_value = Value(touched_var);
      const int64 new_value = solution_delta.Element(index).Value();
      new_sum += new_value - old_value;
    }
    VLOG(1) << "new sum = " << new_sum << ", old sum = " << sum_;
    return new_sum < sum_;
  }

 private:
  int64 sum_;
};

enum SolveType {
  LNS,
  LS,
  LS_WITH_FILTER
};

void SolveProblem(SolveType solve_type) {
  Solver s("Sample");
  std::vector<IntVar*> vars;
  s.MakeIntVarArray(4, 0, 4, &vars);
  IntVar* const sum_var = s.MakeSum(vars)->Var();
  OptimizeVar* const obj = s.MakeMinimize(sum_var, 1);
  DecisionBuilder* const db =
      s.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MAX_VALUE);
  DecisionBuilder* ls = nullptr;
  switch (solve_type) {
    case LNS: {
      LOG(INFO) << "Large Neighborhood Search";
      OneVarLns* const one_var_lns = s.RevAlloc(new OneVarLns(vars));
      LocalSearchPhaseParameters* const ls_params =
          s.MakeLocalSearchPhaseParameters(one_var_lns, db);
      ls = s.MakeLocalSearchPhase(vars, db, ls_params);
      break;
    }
    case LS: {
      LOG(INFO) << "Local Search";
      MoveOneVar* const one_var_ls = s.RevAlloc(new MoveOneVar(vars));
      LocalSearchPhaseParameters* const ls_params =
          s.MakeLocalSearchPhaseParameters(one_var_ls, db);
      ls = s.MakeLocalSearchPhase(vars, db, ls_params);
      break;
    }
    case LS_WITH_FILTER: {
      LOG(INFO) << "Local Search with Filter";
      MoveOneVar* const one_var_ls = s.RevAlloc(new MoveOneVar(vars));
      std::vector<LocalSearchFilter*> filters;
      filters.push_back(s.RevAlloc(new SumFilter(vars)));

      LocalSearchPhaseParameters* const ls_params =
          s.MakeLocalSearchPhaseParameters(one_var_ls, db, nullptr, filters);
      ls = s.MakeLocalSearchPhase(vars, db, ls_params);
      break;
    }
  }
  SolutionCollector* const collector = s.MakeLastSolutionCollector();
  collector->Add(vars);
  collector->AddObjective(sum_var);
  SearchMonitor* const log = s.MakeSearchLog(1000, obj);
  s.Solve(ls, collector, obj, log);
  LOG(INFO) << "Objective value = " << collector->objective_value(0);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::SolveProblem(operations_research::LNS);
  operations_research::SolveProblem(operations_research::LS);
  operations_research::SolveProblem(operations_research::LS_WITH_FILTER);
  return 0;
}
