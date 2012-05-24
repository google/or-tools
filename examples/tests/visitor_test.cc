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


#include "base/hash.h"
#include "base/map-util.h"
#include "base/stl_util.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/model.pb.h"

namespace operations_research {
void TestVisitSumEqual() {
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
    vector<IntVar*> item_column(total_bins);
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


void TestExport() {
  std::vector<int> items;
  items.push_back(5);
  items.push_back(4);
  items.push_back(6);
  items.push_back(8);
  items.push_back(7);

  std::vector<int> bins;
  bins.push_back(10);
  bins.push_back(10);
  bins.push_back(10);

  size_t total_items = items.size();
  size_t total_bins = bins.size();

  string nameModel("BinPacking");
  Solver solver(nameModel);

  int sumCapacityItems = 0;
  for (int i = 0; i < total_items; ++i) {
    sumCapacityItems += items[i];
  }

  int sumCapacityBins = 0;
  for (int j = 0; j < total_bins; ++j) {
    sumCapacityBins += bins[j];
  }


  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(total_items * total_bins, 0, 1, "", &vars);

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
    tabItemInNoBin.push_back(solver.MakeIntVar(0, total_bins-1));

    solver.AddConstraint(solver.MakeMapDomain(tabItemInNoBin[i], item_column));
  }

  //Array of tabCapaPrises
  std::vector<IntVar*> tabCapaPrises;

  //Array of tabCapaRestantes
  std::vector<IntVar*> tabCapaRestantes;

  //Array of CapaInstancied
  std::vector<IntVar*> tabCapaInstancied;

  for(int j = 0; j < total_bins; ++j) {
    //ct1 : Sum(ci*xij) = capaPrise[j]
    std::vector<IntVar*> bin_column(total_items);
    for(int i = 0; i < total_items; ++i) {
      bin_column[i] = vars[i + j * total_items];
    }
    tabCapaPrises.push_back(solver.MakeScalProd(bin_column, items)->Var());

    //Sum(ci*xij) = capaInstancied[j]
    tabCapaInstancied.push_back(solver.MakeScalProd(bin_column, items)->Var());

    //Add the remaining capacity on each bin
    tabCapaRestantes.push_back(solver.MakeIntVar(0, bins[j]));
  }

  ////////////////////////// Optimization
  //Minimize le nb de bins
  std::vector<IntVar*> tabStatutBins;
  for(int j = 0; j < total_bins; ++j) {
    IntExpr* binJUsed = solver.MakeIsGreaterCstVar(tabCapaPrises[j], 0);
    tabStatutBins.push_back(binJUsed->Var());
  }
  IntVar* const numNotEmptyBins = solver.MakeSum(tabStatutBins)->Var();

  OptimizeVar* const minimizeNumBins = solver.MakeMinimize(numNotEmptyBins, 1);

  std::vector<SearchMonitor*> monitors;
  monitors.push_back(minimizeNumBins);

  ////////////////////////// Optimization

  //Export the model
  CPModelProto model;
  solver.ExportModel(monitors, &model);
  CHECK(model.has_objective());
}
}  // namespace operations_research


int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::TestVisitSumEqual();
  operations_research::TestExport();
  return 0;
}
