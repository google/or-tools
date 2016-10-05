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

namespace operations_research {

FapModelPrinter::FapModelPrinter(const std::map<int, FapVariable>& variables,
                                 const std::vector<FapConstraint>& constraints,
                                 const std::string& objective,
                                 const std::vector<int>& values)
    : variables_(variables),
      constraints_(constraints),
      objective_(objective),
      values_(values) {}

FapModelPrinter::~FapModelPrinter() {}

void FapModelPrinter::PrintFapVariables() {
  LOG(INFO) << "Variable File:";
  for (const auto& it : variables_) {
<<<<<<< HEAD
    std::string domain = StringPrintf("{");
    for (const int value : it.second.domain) {
      StringAppendF(&domain, "%d ", value);
    }
    StringAppendF(&domain, "}");

    std::string hard = " ";
    if (it.second.hard) {
      hard = " hard";
    }
=======
    LOG(INFO) << "Variable ";

    key = it.first;
    LOG(INFO) << StringPrintf("%3d: ", key);

    domain_index = it.second.domain_index_;
    LOG(INFO) << StringPrintf("%3d", domain_index);

    initial_position = it.second.initial_position_;
    LOG(INFO) << StringPrintf("%3d", initial_position);

    mobility_index = it.second.mobility_index_;
    LOG(INFO) << StringPrintf("%3d", mobility_index);

    mobility_cost = it.second.mobility_cost_;
    LOG(INFO) << StringPrintf("%8d", mobility_cost);

    LOG(INFO) << StringPrintf(" { ");
    for (int i = 0; i < it.second.domain_.size(); ++i)
      LOG(INFO) << StringPrintf("%d ", it.second.domain_[i]);
    LOG(INFO) << "}";
>>>>>>> 12dc2aee2aa9f9e0c0ba3f00d2c47084ed1ec2af

    LOG(INFO) << "Variable " << StringPrintf("%3d: ", it.first)
              << StringPrintf("(degree: %2d) ", it.second.degree)
              << StringPrintf("%3d", it.second.domain_index)
              << StringPrintf("%3d", it.second.initial_position)
              << StringPrintf("%3d", it.second.mobility_index)
              << StringPrintf("%8d", it.second.mobility_cost)
              << StringPrintf(" (%2d) ", it.second.domain_size) << domain
              << hard;
  }
}

void FapModelPrinter::PrintFapConstraints() {
  LOG(INFO) << "Constraint File:";
<<<<<<< HEAD
  for (const FapConstraint& ct : constraints_) {
    std::string hard = " ";
    if (ct.hard) {
      hard = " hard";
=======
  for (const auto& constraint : constraints_) {
    variable1 = constraint.variable1_;
    LOG(INFO) << StringPrintf("%3d ", variable1);

    variable2 = constraint.variable2_;
    LOG(INFO) << StringPrintf("%3d ", variable2);

    type = constraint.type_;
    LOG(INFO) << type;

    operation = constraint.operator_;
    LOG(INFO) << operation;

    value = constraint.value_;
    LOG(INFO) << StringPrintf("%3d", value);

    weight_index = constraint.weight_index_;
    LOG(INFO) << StringPrintf("%3d", weight_index);

    weight_cost = constraint.weight_cost_;
    LOG(INFO) << StringPrintf("%8d", weight_cost);

    if (constraint.hard_) {
      LOG(INFO) << " hard";
>>>>>>> 12dc2aee2aa9f9e0c0ba3f00d2c47084ed1ec2af
    }

    LOG(INFO) << StringPrintf("%3d ", ct.variable1)
              << StringPrintf("%3d ", ct.variable2) << ct.type << " "
              << ct.operation << " " << StringPrintf("%3d", ct.value)
              << StringPrintf("%3d", ct.weight_index)
              << StringPrintf("%8d", ct.weight_cost) << hard;
  }
}

void FapModelPrinter::PrintFapObjective() {
  LOG(INFO) << "Objective: " << objective_;
}

void FapModelPrinter::PrintFapValues() {
<<<<<<< HEAD
  LOG(INFO) << StringPrintf("Values(%d): ", static_cast<int>(values_.size()));
  std::string domain = " ";
  for (const int value : values_) {
    StringAppendF(&domain, "%d ", value);
=======
  LOG(INFO) << StringPrintf("Values(%d):  ", static_cast<int>(values_.size()));
  for (int value : values_) {
    LOG(INFO) << value;
>>>>>>> 12dc2aee2aa9f9e0c0ba3f00d2c47084ed1ec2af
  }
  LOG(INFO) << domain;
}

}  // namespace operations_research
