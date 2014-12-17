// Copyright 2011-2014 Google
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
//
//
// n-Queens Problem
//
//  Use of SymmetryBreakers.

#include <cstdio>
#include <map>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"

#include "nqueens_utilities.h"

DEFINE_int32(nb_loops, 1,
  "Number of solving loops to perform, for performance timing.");
DEFINE_int32(size, 0,
  "Size of the problem. If equal to 0, will test several increasing sizes.");
DECLARE_bool(use_symmetry);

namespace operations_research {

class NQueenSymmetry : public SymmetryBreaker {
 public:
  NQueenSymmetry(Solver* const s, const std::vector<IntVar*>& vars)
      : solver_(s), vars_(vars), size_(vars.size()) {
    for (int i = 0; i < size_; ++i) {
      indices_[vars[i]] = i;
    }
  }
  virtual ~NQueenSymmetry() {}

 protected:
  int Index(IntVar* const var) const {
    return FindWithDefault(indices_, var, -1);
  }
  IntVar* Var(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, size_);
    return vars_[index];
  }
  int size() const { return size_; }
  int symmetric(int index) const { return size_ - 1 - index; }
  Solver* const solver() const { return solver_; }

 private:
  Solver* const solver_;
  const std::vector<IntVar*> vars_;
  std::map<IntVar*, int> indices_;
  const int size_;
};

// Symmetry vertical axis.
class SX : public NQueenSymmetry {
 public:
  SX(Solver* const s, const std::vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SX() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(symmetric(index));
    AddIntegerVariableEqualValueClause(other_var, value);
  }
};

// Symmetry horizontal axis.
class SY : public NQueenSymmetry {
 public:
  SY(Solver* const s, const std::vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SY() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    AddIntegerVariableEqualValueClause(var, symmetric(value));
  }
};

// Symmetry first diagonal axis.
class SD1 : public NQueenSymmetry {
 public:
  SD1(Solver* const s, const std::vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SD1() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(value);
    AddIntegerVariableEqualValueClause(other_var, index);
  }
};

// Symmetry second diagonal axis.
class SD2 : public NQueenSymmetry {
 public:
  SD2(Solver* const s, const std::vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SD2() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(symmetric(value));
    AddIntegerVariableEqualValueClause(other_var, symmetric(index));
  }
};

// Rotate 1/4 turn.
class R90 : public NQueenSymmetry {
 public:
  R90(Solver* const s, const std::vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~R90() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(value);
    AddIntegerVariableEqualValueClause(other_var, symmetric(index));
  }
};

// Rotate 1/2 turn.
class R180 : public NQueenSymmetry {
 public:
  R180(Solver* const s, const std::vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  virtual ~R180() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(symmetric(index));
    AddIntegerVariableEqualValueClause(other_var, symmetric(value));
  }
};

// Rotate 3/4 turn.
class R270 : public NQueenSymmetry {
 public:
  R270(Solver* const s, const std::vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  virtual ~R270() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(symmetric(value));
    AddIntegerVariableEqualValueClause(other_var, index);
  }
};

void NQueens(int size) {
  CHECK_GE(size, 1);
  Solver s("nqueens");

  // model
  std::vector<IntVar*> queens;
  for (int i = 0; i < size; ++i) {
    queens.push_back(s.MakeIntVar(0, size - 1, StringPrintf("queen%04d", i)));
  }
  s.AddConstraint(s.MakeAllDifferent(queens));

  std::vector<IntVar*> vars(size);
  for (int i = 0; i < size; ++i) {
    vars[i] = s.MakeSum(queens[i], i)->Var();
  }
  s.AddConstraint(s.MakeAllDifferent(vars));
  for (int i = 0; i < size; ++i) {
    vars[i] = s.MakeSum(queens[i], -i)->Var();
  }
  s.AddConstraint(s.MakeAllDifferent(vars));

  SolutionCollector* const solution_counter = s.MakeAllSolutionCollector(NULL);
  SolutionCollector* collector = NULL;
  if (FLAGS_print_all) {
    collector = s.MakeAllSolutionCollector();
  } else {
    collector = s.MakeFirstSolutionCollector();
  }
  
  collector->Add(queens);
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(solution_counter);
  monitors.push_back(collector);
  DecisionBuilder* const db = s.MakePhase(queens,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);
  if (FLAGS_use_symmetry) {
    std::vector<SymmetryBreaker*> breakers;
    NQueenSymmetry* const sx = s.RevAlloc(new SX(&s, queens));
    breakers.push_back(sx);
    NQueenSymmetry* const sy = s.RevAlloc(new SY(&s, queens));
    breakers.push_back(sy);
    NQueenSymmetry* const sd1 = s.RevAlloc(new SD1(&s, queens));
    breakers.push_back(sd1);
    NQueenSymmetry* const sd2 = s.RevAlloc(new SD2(&s, queens));
    breakers.push_back(sd2);
    NQueenSymmetry* const r90 = s.RevAlloc(new R90(&s, queens));
    breakers.push_back(r90);
    NQueenSymmetry* const r180 = s.RevAlloc(new R180(&s, queens));
    breakers.push_back(r180);
    NQueenSymmetry* const r270 = s.RevAlloc(new R270(&s, queens));
    breakers.push_back(r270);
    SearchMonitor* const symmetry_manager = s.MakeSymmetryManager(breakers);
    monitors.push_back(symmetry_manager);
  }

  s.Solve(db, monitors);  // go!
  CheckNumberOfSolutions(size, solution_counter->solution_count());

  const int num_solutions = solution_counter->solution_count();
  const int64 time = s.wall_time();

  std::cout << "============================" << std::endl;
  std::cout << "size: " << size << std::endl;
  std::cout << "The Solve method took "
                                  << time/1000.0 << " seconds" << std::endl;
  std::cout << "Number of solutions: " << num_solutions << std::endl;
  std::cout << "Failures: " << s.failures() << std::endl;
  std::cout << "Branches: " << s.branches() << std::endl;
  std::cout << "Backtracks: " << s.fail_stamp() << std::endl;
  std::cout << "Stamps: " << s.stamp() << std::endl;
  PrintFirstSolution(size, queens, collector);
}
}   // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_size != 0) {
    operations_research::NQueens(FLAGS_size);
  } else {
    for (int n = 1; n < 12; ++n) {
      operations_research::NQueens(n);
    }
  }
  return 0;
}
