// Copyright 2010-2024 Google LLC
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

#include "examples/cpp/fap_model_printer.h"

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"

namespace operations_research {

FapModelPrinter::FapModelPrinter(
    const absl::btree_map<int, FapVariable>& variables,
    const std::vector<FapConstraint>& constraints, absl::string_view objective,
    const std::vector<int>& values)
    : variables_(variables),
      constraints_(constraints),
      objective_(objective),
      values_(values) {}

FapModelPrinter::~FapModelPrinter() = default;

void FapModelPrinter::PrintFapVariables() {
  LOG(INFO) << "Variable File:";
  for (const auto& it : variables_) {
    std::string domain = "{";
    for (const int value : it.second.domain) {
      absl::StrAppendFormat(&domain, "%d ", value);
    }
    domain.append("}");

    std::string hard = " ";
    if (it.second.hard) {
      hard = " hard";
    }

    LOG(INFO) << "Variable " << absl::StrFormat("%3d: ", it.first)
              << absl::StrFormat("(degree: %2d) ", it.second.degree)
              << absl::StrFormat("%3d", it.second.domain_index)
              << absl::StrFormat("%3d", it.second.initial_position)
              << absl::StrFormat("%3d", it.second.mobility_index)
              << absl::StrFormat("%8d", it.second.mobility_cost)
              << absl::StrFormat(" (%2d) ", it.second.domain_size) << domain
              << hard;
  }
}

void FapModelPrinter::PrintFapConstraints() {
  LOG(INFO) << "Constraint File:";
  for (const FapConstraint& ct : constraints_) {
    std::string hard = " ";
    if (ct.hard) {
      hard = " hard";
    }

    LOG(INFO) << absl::StrFormat("%3d ", ct.variable1)
              << absl::StrFormat("%3d ", ct.variable2) << ct.type << " "
              << ct.operation << " " << absl::StrFormat("%3d", ct.value)
              << absl::StrFormat("%3d", ct.weight_index)
              << absl::StrFormat("%8d", ct.weight_cost) << hard;
  }
}

void FapModelPrinter::PrintFapObjective() {
  LOG(INFO) << "Objective: " << objective_;
}

void FapModelPrinter::PrintFapValues() {
  LOG(INFO) << absl::StrFormat("Values(%d): ",
                               static_cast<int>(values_.size()));
  std::string domain = " ";
  for (const int value : values_) {
    absl::StrAppendFormat(&domain, "%d ", value);
  }
  LOG(INFO) << domain;
}

}  // namespace operations_research
