// Copyright 2010-2025 Google LLC
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

#include "examples/cpp/fap_parser.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "ortools/base/helpers.h"
#include "ortools/base/map_util.h"

namespace operations_research {
namespace {
int strtoint32(const std::string& word) {
  int result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}
}  // namespace

void ParseFileByLines(const std::string& filename,
                      std::vector<std::string>* lines) {
  CHECK(lines != nullptr);
  std::string result;
  CHECK_OK(file::GetContents(filename, &result, file::Defaults()));
  *lines = absl::StrSplit(result, '\n', absl::SkipEmpty());
}

// VariableParser Implementation
VariableParser::VariableParser(const std::string& data_directory)
    : filename_(data_directory + "/var.txt") {}

VariableParser::~VariableParser() = default;

void VariableParser::Parse() {
  std::vector<std::string> lines;
  ParseFileByLines(filename_, &lines);
  for (const std::string& line : lines) {
    std::vector<std::string> tokens =
        absl::StrSplit(line, ' ', absl::SkipEmpty());
    if (tokens.empty()) {
      continue;
    }
    CHECK_GE(tokens.size(), 2);

    FapVariable variable;
    variable.domain_index = strtoint32(tokens[1]);
    if (tokens.size() > 3) {
      variable.initial_position = strtoint32(tokens[2]);
      variable.mobility_index = strtoint32(tokens[3]);
    }
    gtl::InsertOrUpdate(&variables_, strtoint32(tokens[0]), variable);
  }
}

// DomainParser Implementation
DomainParser::DomainParser(const std::string& data_directory)
    : filename_(data_directory + "/dom.txt") {}

DomainParser::~DomainParser() = default;

void DomainParser::Parse() {
  std::vector<std::string> lines;
  ParseFileByLines(filename_, &lines);
  for (const std::string& line : lines) {
    std::vector<std::string> tokens =
        absl::StrSplit(line, ' ', absl::SkipEmpty());
    if (tokens.empty()) {
      continue;
    }
    CHECK_GE(tokens.size(), 2);

    const int key = strtoint32(tokens[0]);

    std::vector<int> domain;
    domain.clear();
    for (int i = 2; i < tokens.size(); ++i) {
      domain.push_back(strtoint32(tokens[i]));
    }

    if (!domain.empty()) {
      gtl::InsertOrUpdate(&domains_, key, domain);
    }
  }
}

// ConstraintParser Implementation
ConstraintParser::ConstraintParser(const std::string& data_directory)
    : filename_(data_directory + "/ctr.txt") {}

ConstraintParser::~ConstraintParser() = default;

void ConstraintParser::Parse() {
  std::vector<std::string> lines;
  ParseFileByLines(filename_, &lines);
  for (const std::string& line : lines) {
    std::vector<std::string> tokens =
        absl::StrSplit(line, ' ', absl::SkipEmpty());
    if (tokens.empty()) {
      continue;
    }
    CHECK_GE(tokens.size(), 5);

    FapConstraint constraint;
    constraint.variable1 = strtoint32(tokens[0]);
    constraint.variable2 = strtoint32(tokens[1]);
    constraint.type = tokens[2];
    constraint.operation = tokens[3];
    constraint.value = strtoint32(tokens[4]);

    if (tokens.size() > 5) {
      constraint.weight_index = strtoint32(tokens[5]);
    }
    constraints_.push_back(constraint);
  }
}

// ParametersParser Implementation
const int ParametersParser::kConstraintCoefficientNo;
const int ParametersParser::kVariableCoefficientNo;
const int ParametersParser::kCoefficientNo;

ParametersParser::ParametersParser(const std::string& data_directory)
    : filename_(data_directory + "/cst.txt"),
      objective_(""),
      constraint_weights_(kConstraintCoefficientNo, 0),
      variable_weights_(kVariableCoefficientNo, 0) {}

ParametersParser::~ParametersParser() = default;

void ParametersParser::Parse() {
  bool objective = true;
  bool largest_token = false;
  bool value_token = false;
  bool number_token = false;
  bool values_token = false;
  bool coefficient = false;
  std::vector<int> coefficients;
  std::vector<std::string> lines;

  ParseFileByLines(filename_, &lines);
  for (const std::string& line : lines) {
    if (objective) {
      largest_token = largest_token || absl::StrContains(line, "largest");
      value_token = value_token || absl::StrContains(line, "value");
      number_token = number_token || absl::StrContains(line, "number");
      values_token = values_token || absl::StrContains(line, "values");
      coefficient = coefficient || absl::StrContains(line, "coefficient");
    }

    if (coefficient) {
      CHECK_EQ(kCoefficientNo,
               kConstraintCoefficientNo + kVariableCoefficientNo);
      objective = false;
      if (absl::StrContains(line, "=")) {
        std::vector<std::string> tokens =
            absl::StrSplit(line, ' ', absl::SkipEmpty());
        CHECK_GE(tokens.size(), 3);
        coefficients.push_back(strtoint32(tokens[2]));
      }
    }
  }

  if (coefficient) {
    CHECK_EQ(kCoefficientNo, coefficients.size());
    for (int i = 0; i < kCoefficientNo; i++) {
      if (i < kConstraintCoefficientNo) {
        constraint_weights_[i] = coefficients[i];
      } else {
        variable_weights_[i - kConstraintCoefficientNo] = coefficients[i];
      }
    }
  }

  if (largest_token && value_token) {
    objective_ = "Minimize the largest assigned value.";
  } else if (number_token && values_token) {
    objective_ = "Minimize the number of assigned values.";
  } else {
    // Should not reach this point.
    LOG(WARNING) << "Cannot read the objective of the instance.";
  }
}

// TODO(user): Make FindComponents linear instead of quadratic.
void FindComponents(const std::vector<FapConstraint>& constraints,
                    const absl::btree_map<int, FapVariable>& variables,
                    const int maximum_variable_id,
                    absl::flat_hash_map<int, FapComponent>* components) {
  std::vector<int> in_component(maximum_variable_id + 1, -1);
  int constraint_index = 0;
  for (const FapConstraint& constraint : constraints) {
    const int variable_id1 = constraint.variable1;
    const int variable_id2 = constraint.variable2;
    const FapVariable& variable1 = gtl::FindOrDie(variables, variable_id1);
    const FapVariable& variable2 = gtl::FindOrDie(variables, variable_id2);
    CHECK_LT(variable_id1, in_component.size());
    CHECK_LT(variable_id2, in_component.size());
    if (in_component[variable_id1] < 0 && in_component[variable_id2] < 0) {
      // None of the variables belong to an existing component.
      // Create a new one.
      FapComponent component;
      const int component_index = constraint_index;
      gtl::InsertOrUpdate(&(component.variables), variable_id1, variable1);
      gtl::InsertOrUpdate(&(component.variables), variable_id2, variable2);
      in_component[variable_id1] = component_index;
      in_component[variable_id2] = component_index;
      component.constraints.push_back(constraint);
      gtl::InsertOrUpdate(components, component_index, component);
    } else if (in_component[variable_id1] >= 0 &&
               in_component[variable_id2] < 0) {
      // If variable1 belongs to an existing component, variable2 should
      // also be included in the same component.
      const int component_index = in_component[variable_id1];
      CHECK(components->contains(component_index));
      gtl::InsertOrUpdate(&((*components)[component_index].variables),
                          variable_id2, variable2);
      in_component[variable_id2] = component_index;
      (*components)[component_index].constraints.push_back(constraint);
    } else if (in_component[variable_id1] < 0 &&
               in_component[variable_id2] >= 0) {
      // If variable2 belongs to an existing component, variable1 should
      // also be included in the same component.
      const int component_index = in_component[variable_id2];
      CHECK(components->contains(component_index));
      gtl::InsertOrUpdate(&((*components)[component_index].variables),
                          variable_id1, variable1);
      in_component[variable_id1] = component_index;
      (*components)[component_index].constraints.push_back(constraint);
    } else {
      // The current constraint connects two different components.
      const int component_index1 = in_component[variable_id1];
      const int component_index2 = in_component[variable_id2];
      const int min_component_index =
          std::min(component_index1, component_index2);
      const int max_component_index =
          std::max(component_index1, component_index2);
      CHECK(components->contains(min_component_index));
      CHECK(components->contains(max_component_index));
      if (min_component_index != max_component_index) {
        // Update the component_index of maximum indexed component's variables.
        for (const auto& variable :
             (*components)[max_component_index].variables) {
          int variable_id = variable.first;
          in_component[variable_id] = min_component_index;
        }
        // Insert all the variables of the maximum indexed component to the
        // variables of the minimum indexed component.
        ((*components)[min_component_index])
            .variables.insert(
                ((*components)[max_component_index]).variables.begin(),
                ((*components)[max_component_index]).variables.end());
        // Insert all the constraints of the maximum indexed component to the
        // constraints of the minimum indexed component.
        ((*components)[min_component_index])
            .constraints.insert(
                ((*components)[min_component_index]).constraints.end(),
                ((*components)[max_component_index]).constraints.begin(),
                ((*components)[max_component_index]).constraints.end());
        (*components)[min_component_index].constraints.push_back(constraint);
        // Delete the maximum indexed component from the components set.
        components->erase(max_component_index);
      } else {
        // Both variables belong to the same component, just add the constraint.
        (*components)[min_component_index].constraints.push_back(constraint);
      }
    }
    constraint_index++;
  }
}

int EvaluateConstraintImpact(const absl::btree_map<int, FapVariable>& variables,
                             const int max_weight_cost,
                             const FapConstraint constraint) {
  const FapVariable& variable1 =
      gtl::FindOrDie(variables, constraint.variable1);
  const FapVariable& variable2 =
      gtl::FindOrDie(variables, constraint.variable2);
  const int degree1 = variable1.degree;
  const int degree2 = variable2.degree;
  const int max_degree = std::max(degree1, degree2);
  const int min_degree = std::min(degree1, degree2);
  const int operator_impact =
      constraint.operation == "=" ? max_degree : min_degree;
  const int kHardnessBias = 10;
  int hardness_impact = 0;
  if (constraint.hard) {
    hardness_impact = max_weight_cost > 0 ? kHardnessBias * max_weight_cost : 0;
  } else {
    hardness_impact = constraint.weight_cost;
  }
  return max_degree + min_degree + operator_impact + hardness_impact;
}

void ParseInstance(const std::string& data_directory, bool find_components,
                   absl::btree_map<int, FapVariable>* variables,
                   std::vector<FapConstraint>* constraints,
                   std::string* objective, std::vector<int>* frequencies,
                   absl::flat_hash_map<int, FapComponent>* components) {
  CHECK(variables != nullptr);
  CHECK(constraints != nullptr);
  CHECK(objective != nullptr);
  CHECK(frequencies != nullptr);

  // Parse the data files.
  VariableParser var(data_directory);
  var.Parse();
  *variables = var.variables();
  const int maximum_variable_id = variables->rbegin()->first;

  ConstraintParser ctr(data_directory);
  ctr.Parse();
  *constraints = ctr.constraints();

  DomainParser dom(data_directory);
  dom.Parse();

  ParametersParser cst(data_directory);
  cst.Parse();
  const int maximum_weight_cost = *std::max_element(
      (cst.constraint_weights()).begin(), (cst.constraint_weights()).end());

  // Make the variables of the instance.
  for (auto& it : *variables) {
    it.second.domain = gtl::FindOrDie(dom.domains(), it.second.domain_index);
    it.second.domain_size = it.second.domain.size();

    if ((it.second.mobility_index == -1) || (it.second.mobility_index == 0)) {
      it.second.mobility_cost = -1;
      if (it.second.initial_position != -1) {
        it.second.hard = true;
      }
    } else {
      it.second.mobility_cost =
          (cst.variable_weights())[it.second.mobility_index - 1];
    }
  }
  // Make the constraints of the instance.
  for (FapConstraint& ct : *constraints) {
    if ((ct.weight_index == -1) || (ct.weight_index == 0)) {
      ct.weight_cost = -1;
      ct.hard = true;
    } else {
      ct.weight_cost = (cst.constraint_weights())[ct.weight_index - 1];
      ct.hard = false;
    }
    ++((*variables)[ct.variable1]).degree;
    ++((*variables)[ct.variable2]).degree;
  }
  // Make the available frequencies of the instance.
  *frequencies = gtl::FindOrDie(dom.domains(), 0);
  // Make the objective of the instance.
  *objective = cst.objective();

  if (find_components) {
    CHECK(components != nullptr);
    FindComponents(*constraints, *variables, maximum_variable_id, components);
    // Evaluate each components's constraints impacts.
    for (auto& component : *components) {
      for (auto& constraint : component.second.constraints) {
        constraint.impact = EvaluateConstraintImpact(
            *variables, maximum_weight_cost, constraint);
      }
    }
  } else {
    for (FapConstraint& constraint : *constraints) {
      constraint.impact =
          EvaluateConstraintImpact(*variables, maximum_weight_cost, constraint);
    }
  }
}
}  // namespace operations_research
