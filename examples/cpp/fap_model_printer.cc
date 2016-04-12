// Copyright 2010-2014 Google
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

#include "cpp/fap_model_printer.h"

#include <map>
#include <string>
#include <vector>
#include "base/stringprintf.h"
#include "base/concise_iterator.h"

namespace operations_research {

FapModelPrinter::FapModelPrinter(const std::map<int, FapVariable>& variables,
                                 const std::vector<FapConstraint>& constraints,
                                 const string& objective,
                                 const std::vector<int>& values)
    : variables_(variables),
      constraints_(constraints),
      objective_(objective),
      values_(values) { }

FapModelPrinter::~FapModelPrinter() { }

void FapModelPrinter::PrintFapVariables() {
  int key;
  int domain_index;
  int initial_position;
  int mobility_index;
  int mobility_cost;

  LOG(INFO) << "Variable File:";
  for (ConstIter<std::map<int, FapVariable> > it(variables_); !it.at_end(); ++it) {
    LOG(INFO) << "Variable ";

    key = it->first;
    LOG(INFO) << StringPrintf("%3d: ", key);

    domain_index = it->second.domain_index_;
    LOG(INFO) << StringPrintf("%3d", domain_index);

    initial_position = it->second.initial_position_;
    LOG(INFO) << StringPrintf("%3d", initial_position);

    mobility_index = it->second.mobility_index_;
    LOG(INFO) << StringPrintf("%3d", mobility_index);

    mobility_cost = it->second.mobility_cost_;
    LOG(INFO) << StringPrintf("%8d", mobility_cost);

    LOG(INFO) << StringPrintf(" { ");
    for (int i = 0; i < it->second.domain_.size(); ++i)
      LOG(INFO) << StringPrintf("%d ", it->second.domain_[i]);
    LOG(INFO) << "}";

    LOG(INFO) << "\n";
  }
}


void FapModelPrinter::PrintFapConstraints() {
  int variable1;
  int variable2;
  string type;
  string operation;
  int value;
  int weight_index;
  int weight_cost;

  LOG(INFO) << "Constraint File:";
  for (ConstIter<std::vector<FapConstraint> > it(constraints_); !it.at_end(); ++it) {
    variable1 = it->variable1_;
    LOG(INFO) << StringPrintf("%3d ", variable1);

    variable2 = it->variable2_;
    LOG(INFO) << StringPrintf("%3d ", variable2);

    type = it->type_;
    LOG(INFO) << type;

    operation = it->operator_;
    LOG(INFO) << operation;

    value = it->value_;
    LOG(INFO) << StringPrintf("%3d", value);

    weight_index = it->weight_index_;
    LOG(INFO) << StringPrintf("%3d", weight_index);

    weight_cost = it->weight_cost_;
    LOG(INFO) << StringPrintf("%8d", weight_cost);

    if (it->hard_) {
      LOG(INFO) << " hard";
    }

    LOG(INFO) << "\n";
  }
}

void FapModelPrinter::PrintFapObjective() {
  LOG(INFO) << "Objective: " << objective_;
}

void FapModelPrinter::PrintFapValues() {
  LOG(INFO) << StringPrintf("Values(%d):  ", static_cast<int>(values_.size()));
  for (ConstIter<std::vector<int> > it(values_); !it.at_end(); ++it) {
    LOG(INFO) << *it;
  }
}

}  // namespace operations_research
