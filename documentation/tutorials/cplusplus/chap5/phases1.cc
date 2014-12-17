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
// Benchmark of the different available strategies to select a variable
// and assign a value to it.

#include "base/join.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

#include "nqueens_utilities.h"
#include "solver_benchmark.h"

DEFINE_int32(size, 5, "Size of the problem.");
DEFINE_int64(time_limit, 0, "Time limit on the solving"
                            " process. 0 means no time limit.");
DECLARE_bool(use_symmetry);

namespace operations_research {


static const int kIntVarStrategyStart = 2;
static const int kIntVarStrategyStop = 7;
static const int kIntValueStrategyStart = 2;
static const int kIntValueStrategyStop = 7;

class NQueensBasicModel {
  public:
    NQueensBasicModel(Solver * s,
                      std::vector<IntVar*>& queens,
                      int size): s_(s), queens_(queens), size_(size) {}
    void Construct();
  private:
    Solver * s_;
    std::vector<IntVar*> & queens_;
    const int size_;
};

void NQueensBasicModel::Construct() {
  // model
  s_->AddConstraint(s_->MakeAllDifferent(queens_));

  std::vector<IntVar*> vars(size_);
  for (int i = 0; i < size_; ++i) {
    vars[i] = s_->MakeSum(queens_[i], i)->Var();
  }
  s_->AddConstraint(s_->MakeAllDifferent(vars));
  for (int i = 0; i < size_; ++i) {
    vars[i] = s_->MakeSum(queens_[i], -i)->Var();
  }
  s_->AddConstraint(s_->MakeAllDifferent(vars));
}


class NQueensBenchmark : public SolverBenchmark {
  public:
    explicit NQueensBenchmark(int size): SolverBenchmark(), size_(size) {}
    virtual ~NQueensBenchmark() {}
    bool Test();
  private:
    int size_;
};

bool NQueensBenchmark::Test() {
  for (int i = kIntVarStrategyStart; i <= kIntVarStrategyStop; ++i) {
    for (int j = kIntValueStrategyStart; j <= kIntValueStrategyStop; ++j) {
      Solver s("nqueens");

      // model
      std::vector<IntVar*> queens;
      for (int index = 0; index < size_; ++index) {
        queens.push_back(s.MakeIntVar(0, size_ - 1,
                                      StringPrintf("queen%04d", index)));
      }

      NQueensBasicModel model(&s, queens, size_);
      model.Construct();

      //  monitors
      SolutionCollector* const collector = s.MakeAllSolutionCollector();
      collector->Add(queens);

      SearchMonitor* time_limit = NULL;
      if (FLAGS_time_limit != 0) {
        time_limit = s.MakeTimeLimit(FLAGS_time_limit);
      }

      std::vector<SearchMonitor*> monitors;
      monitors.push_back(collector);
      monitors.push_back(time_limit);

      Solver::IntVarStrategy var_str = static_cast<Solver::IntVarStrategy>(i);
      Solver::IntValueStrategy val_str =
                                     static_cast<Solver::IntValueStrategy>(j);

      DecisionBuilder* const db = s.MakePhase(queens,
                                              var_str,
                                              val_str);

      SolverBenchmarkStats stats;

      //  horrible hack for the description but it works...
      std::string description = db->DebugString();
      Run(&s, db, monitors,
          description.substr(0, description.find('(')), stats);
    }
  }
}


void NQueens(int size) {
  CHECK_GE(size, 1);
  NQueensBenchmark benchmark(size);
  benchmark.Test();
  benchmark.Report(StrCat("report_", size, ".txt"));
}
}   // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_use_symmetry) {
    LOG(FATAL) << "Symmetries not yet implemented!";
  }

  operations_research::NQueens(FLAGS_size);

  return 0;
}
