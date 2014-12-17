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
// Use of customized search strategies with the help of customized DecisionBuilder.

#include "base/join.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

#include "nqueens_utilities.h"

DEFINE_int32(size, 4, "Size of the problem.");
DEFINE_int64(time_limit, 0, "Time limit on the solving"
			    " process. 0 means no time limit.");
DECLARE_bool(use_symmetry);

namespace operations_research {

namespace {

class NQueensDecisionBuilder : public DecisionBuilder {
public:
  NQueensDecisionBuilder(const int size, const std::vector<IntVar*>& vars): size_(size), vars_(vars), middle_var_index_((size-1)/2) {
    CHECK_EQ(vars_.size(), size_);
  }
  ~NQueensDecisionBuilder() {}

  // Select a variable with the smallest domain starting from the center
  IntVar* SelectVar(Solver* const s) {
    IntVar* selected_var = nullptr;
    int64 id = -1; 
    int64 min_domain_size = kint64max;

    // go left on the chessboard
    for (int64 i = middle_var_index_; i >= 0; --i) {
      IntVar* const var = vars_[i];
      if (!var->Bound() && var->Size() < min_domain_size) {
	selected_var = var;
	id = i;
	min_domain_size = var->Size();
      }
    }

    // go right on the chessboard
    for (int64 i = middle_var_index_ + 1; i < size_; ++i) {
      IntVar* const var = vars_[i];
      if (!var->Bound() && var->Size() < min_domain_size) {
	selected_var = var;
	id = i;
	min_domain_size = var->Size();
      }
    }

    if (id == -1) {
      return nullptr;
    } else {
      return selected_var;
    }
  }

  int64 count_number_of_row_incompatibilities(int64 row) {
    int64 count = 0;
    for (int64 i = 0; i < size_; ++i) {
      if (!vars_[i]->Contains(row)) {
	++count;
      }
    }
    return count;
  }

  //  For a given variable, take the row with the least compatible columns, starting from the center
  int64 SelectValue(const IntVar* const v) { 
    CHECK_GE(v->Size(), 2);

    int64 max_incompatible_columns = -1;
    int64 selected_value = -1;

    const int64 vmin = v->Min();
    const int64 vmax = v->Max();
    const int64 v_middle = (vmin + vmax) / 2;

    for (int64 i = v_middle; i >= vmin; --i) {
      if (v->Contains(i)) {
	const int64 nbr_of_incompatibilities = count_number_of_row_incompatibilities(i);
	if (nbr_of_incompatibilities > max_incompatible_columns) {
	  max_incompatible_columns = nbr_of_incompatibilities;
	  selected_value = i;
	  
	}
      }
    }

    for (int64 i = v_middle; i <= vmin; ++i) {
      if (v->Contains(i)) {
	const int64 nbr_of_incompatibilities = count_number_of_row_incompatibilities(i);
	if (nbr_of_incompatibilities > max_incompatible_columns) {
	  max_incompatible_columns = nbr_of_incompatibilities;
	  selected_value = i;
	  
	}
      }
    }

    CHECK_NE(selected_value, -1);

    return selected_value;
  }

  Decision* Next(Solver* const s) {
    IntVar* const var = SelectVar(s);
    if (nullptr != var) {
      const int64 value = SelectValue(var);
      return s->MakeAssignVariableValue(var, value);
      } 

    return nullptr;
  }

private:
  const int size_;
  const std::vector<IntVar*> vars_;
  const int middle_var_index_;
}; //  class NQueensDecisionBuilder



}  // namespace

DecisionBuilder* MakeNQueensDecisionBuilder(Solver* const s,
                                            const int size,
                                            const std::vector<IntVar*>& vars) {
  return s->RevAlloc(new NQueensDecisionBuilder(size, vars));
}


void NQueens(int size) {
  CHECK_GE(size, 1);
  Solver s("nqueens");

  // model
  std::vector<IntVar*> queens;
  for (int i = 0; i < size; ++i) {
    queens.push_back(s.MakeIntVar(0, size - 1, StringPrintf("x%04d", i)));
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

  vector<SearchMonitor*> monitors;

  SolutionCollector* const solution_counter = s.MakeAllSolutionCollector(NULL);
  monitors.push_back(solution_counter);
  SolutionCollector* const collector = s.MakeFirstSolutionCollector();
  collector->Add(queens);
  monitors.push_back(collector);

  DecisionBuilder* const db =  MakeNQueensDecisionBuilder(&s, size, queens);

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
  if (FLAGS_use_symmetry) {
    LOG(FATAL) << "Symmetries not yet implemented!";
  }

  operations_research::NQueens(FLAGS_size);

  return 0;
}
