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
//  Several basic utilities to deal with the n-Queens Problem:
//
//    - Test number of solutions;
//    - Print solutions;

#ifndef _OR_TOOLS_EXAMPLES_CPLUSPLUS_EXAMPLES_CHAP5_NQUEENS_UTILITIES_H
#define _OR_TOOLS_EXAMPLES_CPLUSPLUS_EXAMPLES_CHAP5_NQUEENS_UTILITIES_H

#include <iostream>
#include <iomanip>
#include <vector>

#include "base/commandlineflags.h"
#include "constraint_solver/constraint_solver.h"

DEFINE_bool(print, false, "If true, print first solution.");
DEFINE_bool(print_all, false, "If true, print all solutions");
DEFINE_bool(use_symmetry, false, "Use Symmetry Breaking methods");
DECLARE_bool(cp_no_solve);

static const int kNumSolutions[] = {
  1, 0, 0, 2, 10, 4, 40, 92, 352, 724,
  2680, 14200, 73712, 365596, 2279184, 14772512, 95815104,
  666090624
};
static const int kKnownSolutions = 18;

static const int kNumUniqueSolutions[] = {
  1, 0, 0, 1, 2, 1, 6, 12, 46, 92, 341, 1787, 9233, 45752,
  285053, 1846955, 11977939, 83263591, 621012754
};
static const int kKnownUniqueSolutions = 19;

namespace operations_research {

void CheckNumberOfSolutions(int size, int num_solutions) {
  if (FLAGS_use_symmetry) {
    if (size - 1 < kKnownUniqueSolutions) {
      CHECK_EQ(num_solutions, kNumUniqueSolutions[size - 1]);
    } else if (!FLAGS_cp_no_solve) {
      CHECK_GT(num_solutions, 0);
    }
  } else {
    if (size - 1 < kKnownSolutions) {
      CHECK_EQ(num_solutions, kNumSolutions[size - 1]);
    } else if (!FLAGS_cp_no_solve) {
      CHECK_GT(num_solutions, 0);
    }
  }

  return;
}

//  size < 100 is more than enough!
void PrintSolution(const int size,
                   const std::vector<IntVar*>& queens,
                   SolutionCollector* const collector,
                   const int solution_number) {
  if (collector->solution_count() > solution_number && size < 100) {
    //  go through lines
    for (int j = 0; j < size; ++j) {
      //  go through queens
      for (int i = 0; i < size; ++i) {
        const int pos = static_cast<int>(collector->Value(solution_number, queens[i]));
        std::cout << std::setw(2);
        if (pos == j) {
          std::cout << i;
        } else {
          std::cout << ".";
        }
        std::cout << " ";
      }
      std::cout << std::endl;
    }
  }

  return;
}

void PrintFirstSolution(const int size,
                        const std::vector<IntVar*>& queens,
                        SolutionCollector* const collector) {
  if (FLAGS_print) {
    PrintSolution(size, queens, collector, 0);
  }
  return;
}

void PrintAllSolutions(const int size,
                       const std::vector<IntVar*>& queens,
                       SolutionCollector* const collector) {
  if (FLAGS_print_all) {
    for (int i = 0; i < collector->solution_count(); ++i) {
      PrintSolution(size, queens, collector, i);
      std::cout << std::endl;
    }
  }

  return;
}

}  //  namespace operations_research

#endif  //  _OR_TOOLS_EXAMPLES_CPLUSPLUS_EXAMPLES_CHAP5_NQUEENS_UTILITIES_H

