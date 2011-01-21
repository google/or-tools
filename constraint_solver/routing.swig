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

%include "constraint_solver/constraint_solver.swig"

// Include the file we want to wrap a first time.
%{
#include "constraint_solver/routing.h"
%}

namespace operations_research {

%ignore RoutingModel::AddVectorDimension(const int64* values,
                                         int64 capacity,
                                         const string& name);

%ignore RoutingModel::AddMatrixDimension(const int64* const* values,
                                         int64 capacity,
                                         const string& name);
%ignore RoutingModel::AddNoCycleConstraint();

%extend RoutingModel {
  void AddVectorDimension(const vector<int64>& values,
                          int64 capacity,
                          const string& name) {
    DCHECK_EQ(values.size(), self->nodes());
    self->AddVectorDimension(values.data(), capacity, name);
  }
}

%ignore RoutingModel::RoutingModel(int nodes, int vehicles,
                                   const vector<pair<int, int> >& start_end);
}  // namespace operations_research

#ifdef SWIGJAVA
%rename (solve) Solve;
%rename (addDimension) AddDimension;
%rename (addConstantDimension) AddConstantDimension;
%rename (addVectorDimension) AddVectorDimension;
%rename (addAllActive) AddAllActive;
%rename (addDisjunction) AddDisjunction;
%rename (addLocalSearchOperator) AddLocalSearchOperator;
%rename (setCost) SetCost;
%rename (setVehicleCost) SetVehicleCost;
%rename (setDepot) SetDepot;
%rename (addSearchMonitor) AddSearchMonitor;
%rename (applyLocks) ApplyLocks;
%rename (writeAssignment) WriteAssignment;
%rename (readAssignment) ReadAssignment;
%rename (start) Start;
%rename (end) End;
%rename (isStart) IsStart;
%rename (isEnd) IsEnd;
%rename (getCost) GetCost;
%rename (getFirstSolutionCost) GetFirstSolutionCost;
%rename (nexts) Nexts;
%rename (nextVar) NextVar;
%rename (activeVar) ActiveVar;
%rename (cumulVar) CumulVar;
%rename (transitVar) TransitVar;
%rename (addToAssignment) AddToAssignment;
%rename (size) Size;
%rename (costVar) CostVar;
%rename (updateTimeLimit) UpdateTimeLimit;
%rename (preAssignment) PreAssignment;
%rename (getFirstSolutionStrategy) first_solution_strategy;
%rename (setFirstSolutionStrategy) set_first_solution_strategy;
%rename (setFirstSolutionEvaluator) SetFirstSolutionEvaluator;
%rename (getMetaheuristic) metaheuristic;
%rename (setMetaheuristic) set_metaheuristic;
%rename (setCommandLineOption) SetCommandLineOption;
#endif // SWIGJAVA

// Wrap cp includes
%include constraint_solver/routing.h
