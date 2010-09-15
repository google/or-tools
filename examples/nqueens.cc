// Copyright 2010 Google
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

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/map-util.h"
#include "constraint_solver/constraint_solveri.h"

DEFINE_bool(use_range, false, "If true, use AllDifferenceRange.");
DEFINE_bool(print, false, "If true, print one of the solution.");
DEFINE_bool(print_all, false, "If true, print all the solutions.");
DEFINE_int32(nb_loops, 1,
  "Number of solving loops to perform, for performance timing.");
DEFINE_int32(size, 0,
  "Size of the problem. If equal to 0, will test several increasing sizes.");
DEFINE_bool(use_symmetry, false, "Use Symmetry Breaking methods");

static const int kNumSolutions[] = {
  1, 0, 0, 2, 10, 4, 40, 92, 352, 724,
  2680, 14200, 73712, 365596, 2279184
};
static const int kKnownSolutions = 15;

static const int kNumUniqueSolutions[] = {
  1, 0, 0, 1, 2, 1, 6, 12, 46, 92, 341, 1787, 9233, 45752,
  285053, 1846955, 11977939, 83263591, 621012754
};
static const int kKnownUniqueSolutions = 19;

namespace operations_research {

class MyFirstSolutionCollector : public SolutionCollector {
 public:
  MyFirstSolutionCollector(Solver* const s, const Assignment* a);
  virtual ~MyFirstSolutionCollector();
  virtual void EnterSearch();
  virtual bool RejectSolution();
  virtual string DebugString() const;
 private:
  bool done_;
};

MyFirstSolutionCollector::MyFirstSolutionCollector(Solver* const s,
                                               const Assignment* a)
    : SolutionCollector(s, a), done_(false) {}


MyFirstSolutionCollector::~MyFirstSolutionCollector() {}

void MyFirstSolutionCollector::EnterSearch() {
  SolutionCollector::EnterSearch();
  done_ = false;
}

bool MyFirstSolutionCollector::RejectSolution() {
  if (!done_) {
    PushSolution();
    done_ = true;
    return false;
  }
  return true;
}

string MyFirstSolutionCollector::DebugString() const {
  if (prototype_.get() == NULL) {
    return "MyFirstSolutionCollector()";
  } else {
    return "MyFirstSolutionCollector(" + prototype_->DebugString() + ")";
  }
}

class NQueenSymmetry : public SymmetryBreaker {
 public:
  NQueenSymmetry(Solver* const s, const vector<IntVar*>& vars)
      : solver_(s), vars_(vars), size_(vars.size()) {
    for (int i = 0; i < size_; ++i) {
      indices_[vars[i]] = i;
    }
  }
  virtual ~NQueenSymmetry() {}
  int Index(IntVar* const var) const {
    return FindWithDefault(indices_, var, -1);
  }
  IntVar* Var(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, size_);
    return vars_[index];
  }
  int size() const { return size_; }
  Solver* const solver() const { return solver_; }
 private:
  Solver* const solver_;
  const vector<IntVar*> vars_;
  map<IntVar*, int> indices_;
  const int size_;
};

// Symmetry vertical axis.
class SX : public NQueenSymmetry {
 public:
  SX(Solver* const s, const vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SX() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(size() - 1 - index);
    AddToClause(solver()->MakeIsEqualCstVar(other_var, value));
  }
};

// Symmetry horizontal axis.
class SY : public NQueenSymmetry {
 public:
  SY(Solver* const s, const vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SY() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    AddToClause(solver()->MakeIsEqualCstVar(var, size() - 1 - value));
  }
};

// Symmetry 1 diagonal axis.
class SD1 : public NQueenSymmetry {
 public:
  SD1(Solver* const s, const vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SD1() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(value);
    AddToClause(solver()->MakeIsEqualCstVar(other_var, index));
  }
};

// Symmetry second diagonal axis.
class SD2 : public NQueenSymmetry {
 public:
  SD2(Solver* const s, const vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~SD2() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(size() - 1 - value);
    AddToClause(solver()->MakeIsEqualCstVar(other_var, size() - 1 - index));
  }
};

// Rotate 1/4 turn.
class R90 : public NQueenSymmetry {
 public:
  R90(Solver* const s, const vector<IntVar*>& vars) : NQueenSymmetry(s, vars) {}
  virtual ~R90() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(value);
    AddToClause(solver()->MakeIsEqualCstVar(other_var, size() - 1 - index));
  }
};

// Rotate 1/2 turn.
class R180 : public NQueenSymmetry {
 public:
  R180(Solver* const s, const vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  virtual ~R180() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(size() - 1 - index);
    AddToClause(solver()->MakeIsEqualCstVar(other_var, size() - 1 - value));
  }
};

// Rotate 3/4 turn.
class R270 : public NQueenSymmetry {
 public:
  R270(Solver* const s, const vector<IntVar*>& vars)
      : NQueenSymmetry(s, vars) {}
  virtual ~R270() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    const int index = Index(var);
    IntVar* const other_var = Var(size() - 1 - value);
    AddToClause(solver()->MakeIsEqualCstVar(other_var, index));
  }
};

void NQueens(int size) {
  CHECK_GE(size, 1);
  scoped_ptr<Solver> s(new Solver("nqueens"));

  // model
  vector<IntVar*> queens;
  for (int i = 0; i < size; ++i) {
    queens.push_back(
      s->MakeIntVar(0, size - 1, StringPrintf("queen%04d", i)));
  }
  s->AddConstraint(s->MakeAllDifferent(queens, FLAGS_use_range));

  vector<IntVar*> vars;
  vars.resize(size);
  for (int i = 0; i < size; ++i) {
    vars[i] = s->MakeSum(queens[i], i)->Var();
  }
  s->AddConstraint(s->MakeAllDifferent(vars, FLAGS_use_range));
  for (int i = 0; i < size; ++i) {
    vars[i] = s->MakeSum(queens[i], -i)->Var();
  }
  s->AddConstraint(s->MakeAllDifferent(vars, FLAGS_use_range));

  SolutionCollector * const c1 = s->MakeAllSolutionCollector(NULL);
  Assignment * a = new Assignment(s.get());   // store first solution
  a->Add(queens);
  SolutionCollector * const c2 =
      s->RevAlloc(new MyFirstSolutionCollector(s.get(), a));
  delete a;
  vector<SearchMonitor*> monitors;
  monitors.push_back(c1);
  monitors.push_back(c2);
  DecisionBuilder* const db = s->MakePhase(queens,
                                           Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  if (FLAGS_use_symmetry) {
    vector<SymmetryBreaker*> breakers;
    NQueenSymmetry* sx = s->RevAlloc(new SX(s.get(), queens));
    breakers.push_back(sx);
    NQueenSymmetry* sy = s->RevAlloc(new SY(s.get(), queens));
    breakers.push_back(sy);
    NQueenSymmetry* sd1 = s->RevAlloc(new SD1(s.get(), queens));
    breakers.push_back(sd1);
    NQueenSymmetry* sd2 = s->RevAlloc(new SD2(s.get(), queens));
    breakers.push_back(sd2);
    NQueenSymmetry* r90 = s->RevAlloc(new R90(s.get(), queens));
    breakers.push_back(r90);
    NQueenSymmetry* r180 = s->RevAlloc(new R180(s.get(), queens));
    breakers.push_back(r180);
    NQueenSymmetry* r270 = s->RevAlloc(new R270(s.get(), queens));
    breakers.push_back(r270);
    SearchMonitor* symmetry_manager = s->MakeSymmetryManager(breakers);
    monitors.push_back(symmetry_manager);
  }

  for (int loop = 0; loop < FLAGS_nb_loops; ++loop) {
    s->Solve(db, monitors);  // go!
  }

  const int num_solutions = c1->solution_count();
  if (num_solutions > 0 && size < kKnownSolutions) {
    int print_max = FLAGS_print_all ? num_solutions : FLAGS_print ? 1 : 0;
    for (int n = 0; n < print_max; ++n) {
      printf("--- solution #%d\n", n);
      const Assignment * const b = c2->solution(n);
      for (int i = 0; i < size; ++i) {
        const int pos = static_cast<int>(b->Value(queens[i]));
        for (int k = 0; k < pos; ++k) printf(" . ");
        printf("%2d ", i);
        for (int k = pos + 1; k < size; ++k) printf(" . ");
        printf("\n");
      }
    }
  }
  printf("========= number of solutions:%d\n", num_solutions);
  printf("          number of failures: %lld\n", s->failures());
  if (FLAGS_use_symmetry) {
    if (size - 1 < kKnownUniqueSolutions) {
      CHECK_EQ(num_solutions, kNumUniqueSolutions[size - 1] * FLAGS_nb_loops);
    } else {
      CHECK_GT(num_solutions, 0);
    }
  } else {
    if (size - 1 < kKnownSolutions) {
      CHECK_EQ(num_solutions, kNumSolutions[size - 1] * FLAGS_nb_loops);
    } else {
      CHECK_GT(num_solutions, 0);
    }
  }
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
