// Copyright 2011-2012 Google
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


#include "ortools/base/hash.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/random.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/string_array.h"

namespace operations_research {
void TestVisitSumEqual() {
  LOG(INFO) << "----- Test Visit Sum Equal -----";
  Solver solver("BinPacking");
  const int total_items = 10;
  const int total_bins = 3;

  // create the variables
  // Index des lignes => bins
  // Index des colonnes => items
  std::vector<operations_research::IntVar*> vars;
  solver.MakeBoolVarArray(total_items * total_bins, "", &vars);

  //Contrainte ct1 : un item appartient qu'à un seul bin
  for (int i = 0; i < total_items; ++i) {
    std::vector<IntVar*> item_column(total_bins);
    for(int j = 0; j < total_bins; ++j) {
      item_column[j] = vars[j + i * total_bins];
    }
    solver.AddConstraint(solver.MakeSumEquality(item_column, 1));
    ////////////////////////////////Constraint
  }

  std::vector<IntVar*> primary_integer_variables;
  std::vector<IntVar*> secondary_integer_variables;
  std::vector<SequenceVar*> sequence_variables;
  std::vector<IntervalVar*> interval_variables;

  solver.CollectDecisionVariables(&primary_integer_variables,
                                  &secondary_integer_variables,
                                  &sequence_variables,
                                  &interval_variables);
}

}  // namespace operations_research


int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::TestVisitSumEqual();
  return 0;
}
