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

//
// N-queens problem
//
//  unique solutions: http://www.research.att.com/~njas/sequences/A000170
//  distinct solutions: http://www.research.att.com/~njas/sequences/A002562

#include <cstdio>
#include <map>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solveri.h"

DEFINE_bool(print, false, "If true, print one of the solution.");
DEFINE_bool(print_all, false, "If true, print all the solutions.");
DEFINE_int32(nb_loops, 1,
             "Number of solving loops to perform, for performance timing.");
DEFINE_int32(
    size, 0,
    "Size of the problem. If equal to 0, will test several increasing sizes.");
DEFINE_bool(use_symmetry, false, "Use Symmetry Breaking methods");
DECLARE_bool(cp_disable_solve);

static const int kNumSolutions[] = {
    1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596, 2279184};
static const int kKnownSolutions = 15;

static const int kNumUniqueSolutions[] = {
    1,   0,    0,    1,     2,      1,       6,        12,       46,       92,
    341, 1787, 9233, 45752, 285053, 1846955, 11977939, 83263591, 621012754};
static const int kKnownUniqueSolutions = 19;

namespace operations_research {

class NQueenSymmetry : public SymmetryBreaker {
 public:
  NQueenSymmetry(Solver* const s, const std::vector<IntVar*>& vars)
      : solver_(s), vars_(vars), size_(vars.size()) {
    for (int i = 0; i < size_; ++i) {
      indices_[vars[i]] = i;
    }
  }
  ~NQueenSymmetry() override {}

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
  std::map<const IntVar*, int> indices_;
  const int size_;
};

// Symmetry vertical axis.
class SX : public NQueenSymmetry {
 public:
  SX(Solver* const s, const std::vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  ~SX() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
    const int index = Index(var);
    IntVar* const other_var = Var(symmetric(index));
    AddIntegerVariableEqualValueClause(other_var, value);
  }
};

// Symmetry horizontal axis.
class SY : public NQueenSymmetry {
 public:
  SY(Solver* const s, const std::vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  ~SY() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
    AddIntegerVariableEqualValueClause(var, symmetric(value));
  }
};

// Symmetry first diagonal axis.
class SD1 : public NQueenSymmetry {
 public:
  SD1(Solver* const s, const std::vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  ~SD1() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
    const int index = Index(var);
    IntVar* const other_var = Var(value);
    AddIntegerVariableEqualValueClause(other_var, index);
  }
};

// Symmetry second diagonal axis.
class SD2 : public NQueenSymmetry {
 public:
  SD2(Solver* const s, const std::vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  ~SD2() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
    const int index = Index(var);
    IntVar* const other_var = Var(symmetric(value));
    AddIntegerVariableEqualValueClause(other_var, symmetric(index));
  }
};

// Rotate 1/4 turn.
class R90 : public NQueenSymmetry {
 public:
  R90(Solver* const s, const std::vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  ~R90() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
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
  ~R180() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
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
  ~R270() override {}

  void VisitSetVariableValue(IntVar* const var, int64 value) override {
    const int index = Index(var);
    IntVar* const other_var = Var(symmetric(value));
    AddIntegerVariableEqualValueClause(other_var, index);
  }
};

void CheckNumberOfSolutions(int size, int num_solutions) {
  if (FLAGS_use_symmetry) {
    if (size - 1 < kKnownUniqueSolutions) {
      CHECK_EQ(num_solutions, kNumUniqueSolutions[size - 1]);
    } else if (!FLAGS_cp_disable_solve) {
      CHECK_GT(num_solutions, 0);
    }
  } else {
    if (size - 1 < kKnownSolutions) {
      CHECK_EQ(num_solutions, kNumSolutions[size - 1]);
    } else if (!FLAGS_cp_disable_solve) {
      CHECK_GT(num_solutions, 0);
    }
  }
}

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

  SolutionCollector* const solution_counter =
      s.MakeAllSolutionCollector(nullptr);
  SolutionCollector* const collector = s.MakeAllSolutionCollector();
  collector->Add(queens);
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(solution_counter);
  monitors.push_back(collector);
  DecisionBuilder* const db = s.MakePhase(queens, Solver::CHOOSE_FIRST_UNBOUND,
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

  for (int loop = 0; loop < FLAGS_nb_loops; ++loop) {
    s.Solve(db, monitors);  // go!
    CheckNumberOfSolutions(size, solution_counter->solution_count());
  }

  const int num_solutions = solution_counter->solution_count();
  if (num_solutions > 0 && size < kKnownSolutions) {
    int print_max = FLAGS_print_all ? num_solutions : FLAGS_print ? 1 : 0;
    for (int n = 0; n < print_max; ++n) {
      printf("--- solution #%d\n", n);
      for (int i = 0; i < size; ++i) {
        const int pos = static_cast<int>(collector->Value(n, queens[i]));
        for (int k = 0; k < pos; ++k) printf(" . ");
        printf("%2d ", i);
        for (int k = pos + 1; k < size; ++k) printf(" . ");
        printf("\n");
      }
    }
  }
  printf("========= number of solutions:%d\n", num_solutions);
  printf("          number of failures: %lld\n", s.failures());
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_size != 0) {
    operations_research::NQueens(FLAGS_size);
  } else {
    for (int n = 1; n < 12; ++n) {
      operations_research::NQueens(n);
    }
  }
  return 0;
}
