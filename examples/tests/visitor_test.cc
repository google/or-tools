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
#include "ortools/constraint_solver/model.pb.h"
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


void RunExport(CpModel* const model) {
  const int total_items = 3;
  const int total_bins = 2;

  Solver solver("BinPacking");

  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(total_items * total_bins, 0, 1, "vars_", &vars);

  //Contrainte ct1 : un item appartient qu'à un seul bin
  for (int i = 0; i < total_items; ++i) {
    std::vector<IntVar*> item_column(total_bins);
    for(int j = 0; j < total_bins; ++j) {
      item_column[j] = vars[j + i * total_bins];
    }
    solver.AddConstraint(solver.MakeSumEquality(item_column, 1));
    ////////////////////////////////Constraint
  }

  //binNo[i(item)] = b <=> xib = 1
  std::vector<IntVar*> tabItemInNoBin;
  for (int i = 0; i < total_items; ++i) {
    std::vector<IntVar*> item_column(total_bins);
    for(int j = 0; j < total_bins; ++j) {
      item_column[j] = vars[j + i * total_bins];
    }
    tabItemInNoBin.push_back(
        solver.MakeIntVar(0, total_bins - 1));

    solver.AddConstraint(solver.MakeMapDomain(tabItemInNoBin[i], item_column));
  }

  std::vector<IntVar*> items_per_bin;
  for(int j = 0; j < total_bins; ++j) {
    std::vector<IntVar*> bin_column(total_items);
    std::vector<int64> weights(total_items);
    for(int i = 0; i < total_items; ++i) {
      bin_column[i] = vars[i + j * total_items];
      weights[i] = i + 1;
    }
    items_per_bin.push_back(solver.MakeScalProd(bin_column, weights)->Var());
  }

  ////////////////////////// Optimization
  std::vector<IntVar*> bin_used;
  for(int j = 0; j < total_bins; ++j) {
    bin_used.push_back(solver.MakeIsGreaterCstVar(items_per_bin[j], 0)->Var());
  }
  IntVar* const numNotEmptyBins =
      solver.MakeSum(bin_used)->VarWithName("objective");

	OptimizeVar* const minimizeNumBins = solver.MakeMinimize(numNotEmptyBins, 1);
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(minimizeNumBins);

  //Export the model
  *model = solver.ExportModelWithSearchMonitors(monitors);
}

void TestExport() {
  LOG(INFO) << "----- Test Export -----";
  CpModel model;
  RunExport(&model);
  CHECK(model.has_objective());
}

// Test 3

bool sortIntVar(const IntVar* x, const IntVar* y) {
  return x->name() < y->name();
}

void TestImport() {
  LOG(INFO) << "----- Test Import -----";
  CpModel model;
  RunExport(&model);
  Solver solver("BinPacking");
  std::vector<SearchMonitor*> monitors;
  solver.LoadModelWithSearchMonitors(model, &monitors);

  std::vector<IntVar*> primary_integer_variables;
  std::vector<IntVar*> secondary_integer_variables;
  std::vector<SequenceVar*> sequence_variables;
  std::vector<IntervalVar*> interval_variables;
  solver.CollectDecisionVariables(&primary_integer_variables,
                                   &secondary_integer_variables,
                                   &sequence_variables,
                                   &interval_variables);
  std::sort(primary_integer_variables.begin(),
            primary_integer_variables.end(),
            sortIntVar);
  std::vector<IntVar*> new_vars;
  for (int i = 0; i < primary_integer_variables.size(); ++i) {
    if (primary_integer_variables[i]->HasName()) {
      new_vars.push_back(primary_integer_variables[i]);
    }
  }
  DecisionBuilder* const db = solver.MakePhase(new_vars,
                                               Solver::CHOOSE_FIRST_UNBOUND,
                                               Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db, monitors);

  bool result = solver.NextSolution();
}
}  // namespace operations_research


int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::TestVisitSumEqual();
  operations_research::TestExport();
  operations_research::TestImport();
  return 0;
}
