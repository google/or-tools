// Copyright 2010-2021 Google LLC
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
// Reading and parsing the data of Frequency Assignment Problem
// Format: http://www.inra.fr/mia/T/schiex/Doc/CELAR.shtml#synt
//

#ifndef OR_TOOLS_EXAMPLES_FAP_PARSER_H_
#define OR_TOOLS_EXAMPLES_FAP_PARSER_H_

#include <map>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"

namespace operations_research {

// Takes a filename and a buffer and fills the lines buffer
// with the lines of the file corresponding to the filename.
void ParseFileByLines(const std::string& filename,
                      std::vector<std::string>* lines);

// The FapVariable struct represents a radio link of the
// frequency assignment problem.
struct FapVariable {
  // Fields:
  // the index of a subset of all available frequencies of the instance
  int domain_index = -1;

  // the number of the frequencies available for the link
  int domain_size = 0;

  // the link's domain, i.e. a finite set of frequencies that can be
  // assigned to this link
  std::vector<int> domain;

  // the number of constraints in which the link appears
  int degree = 0;

  // if positive, it means that the link has already been assigned a frequency
  // of that value
  int initial_position = -1;

  // the index of mobility cost
  int mobility_index = -1;

  // the cost of modification of a link's pre-assigned value
  int mobility_cost = -1;

  // if true, it means that the link's pre-assigned position cannot be modified
  bool hard = false;
};

// The FapConstraint struct represents a constraint between two
// radio links of the frequency assignment problem.
struct FapConstraint {
  // Fields:
  // the index of the first variable appearing in the constraint
  int variable1 = -1;

  // the index of the second variable appearing in the constraint
  int variable2 = -1;

  // the importance of a constraint based on the degree of its variables,
  // the operator used in the constraint ("=" or ">") and whether it is a hard
  // or soft constraint and with what weight cost.
  // impact = (max_degree + min_degree + operator_impact + hardness_impact)
  int impact = 0;

  // the constraint type (D (difference), C (viscosity), F (fixed),P (prefix)
  // or L (far fields)) which is not used in practice
  std::string type;

  // the operator used in the constraint ("=" or ">")
  std::string operation;

  // the constraint deviation: it defines the constant k12 mentioned in RLFAP
  // description
  int value = -1;

  // the index of weight cost
  int weight_index = -1;

  // the cost of not satisfaction of the constraint
  int weight_cost = -1;

  // if true, it means that the constraint must be satisfied
  bool hard = false;
};

// The FapComponent struct represents an component of the RLFAP graph.
// It models an independent sub-problem of the initial instance.
struct FapComponent {
  // Fields:
  // the variable set of the sub-problem, i.e. the vertices of the component
  std::map<int, FapVariable> variables;

  // the constraint set of the sub-problem, i.e. the edges of the component
  std::vector<FapConstraint> constraints;
};

// Parser of the var.txt file.
// This file describes all the variables in the instance.
// Each line corresponds to one variable.
class VariableParser {
 public:
  explicit VariableParser(const std::string& data_directory);
  ~VariableParser();

  const std::map<int, FapVariable>& variables() const { return variables_; }

  void Parse();

 private:
  const std::string filename_;
  // A map is used because in the model, the variables have ids which may not
  // be consecutive, may be very sparse and don't have a specific upper-bound.
  // The key of the map, is the link's id.
  std::map<int, FapVariable> variables_;

  DISALLOW_COPY_AND_ASSIGN(VariableParser);
};

// Parser of the dom.txt file.
// This file describes the domains used by the variables of the problem.
// Each line describes one domain.
class DomainParser {
 public:
  explicit DomainParser(const std::string& data_directory);
  ~DomainParser();

  const std::map<int, std::vector<int> >& domains() const { return domains_; }

  void Parse();

 private:
  const std::string filename_;
  // A map is used because in the model, the ids of the different available
  // domains may be random values, since they are used as names. The key of the
  // map is the subset's id.
  std::map<int, std::vector<int> > domains_;

  DISALLOW_COPY_AND_ASSIGN(DomainParser);
};

// Parse ctr.txt file.
// This file describes the constraints of the instance.
// Each line defines a binary constraint.
class ConstraintParser {
 public:
  explicit ConstraintParser(const std::string& data_directory);
  ~ConstraintParser();

  const std::vector<FapConstraint>& constraints() const { return constraints_; }

  void Parse();

 private:
  const std::string filename_;
  std::vector<FapConstraint> constraints_;

  DISALLOW_COPY_AND_ASSIGN(ConstraintParser);
};

// Parse cst.txt file.
// This file defines the criterion on which the solution will be based.
// It may also contain 8 coefficients: 4 for different constraint violation
// costs and 4 for different variable mobility costs.
class ParametersParser {
 public:
  explicit ParametersParser(const std::string& data_directory);
  ~ParametersParser();

  std::string objective() const { return objective_; }
  const std::vector<int>& constraint_weights() const {
    return constraint_weights_;
  }
  const std::vector<int>& variable_weights() const { return variable_weights_; }

  void Parse();

 private:
  const std::string filename_;
  static constexpr int constraint_coefficient_no_ = 4;
  static constexpr int variable_coefficient_no_ = 4;
  static constexpr int coefficient_no_ = 8;
  std::string objective_;
  std::vector<int> constraint_weights_;
  std::vector<int> variable_weights_;
};

namespace {
int strtoint32(const std::string& word) {
  int result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}
}  // namespace

// Function that finds the disjoint sub-graphs of the graph of the instance.
void FindComponents(const std::vector<FapConstraint>& constraints,
                    const std::map<int, FapVariable>& variables,
                    const int maximum_variable_id,
                    absl::flat_hash_map<int, FapComponent>* components);

// Function that computes the impact of a constraint.
int EvaluateConstraintImpact(const std::map<int, FapVariable>& variables,
                             const int max_weight_cost,
                             const FapConstraint constraint);

// Function that parses an instance of frequency assignment problem.
void ParseInstance(const std::string& data_directory, bool find_components,
                   std::map<int, FapVariable>* variables,
                   std::vector<FapConstraint>* constraints,
                   std::string* objective, std::vector<int>* frequencies,
                   absl::flat_hash_map<int, FapComponent>* components);

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

VariableParser::~VariableParser() {}

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
    variable.domain_index = strtoint32(tokens[1].c_str());
    if (tokens.size() > 3) {
      variable.initial_position = strtoint32(tokens[2].c_str());
      variable.mobility_index = strtoint32(tokens[3].c_str());
    }
    gtl::InsertOrUpdate(&variables_, strtoint32(tokens[0].c_str()), variable);
  }
}

// DomainParser Implementation
DomainParser::DomainParser(const std::string& data_directory)
    : filename_(data_directory + "/dom.txt") {}

DomainParser::~DomainParser() {}

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

    const int key = strtoint32(tokens[0].c_str());

    std::vector<int> domain;
    domain.clear();
    for (int i = 2; i < tokens.size(); ++i) {
      domain.push_back(strtoint32(tokens[i].c_str()));
    }

    if (!domain.empty()) {
      gtl::InsertOrUpdate(&domains_, key, domain);
    }
  }
}

// ConstraintParser Implementation
ConstraintParser::ConstraintParser(const std::string& data_directory)
    : filename_(data_directory + "/ctr.txt") {}

ConstraintParser::~ConstraintParser() {}

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
    constraint.variable1 = strtoint32(tokens[0].c_str());
    constraint.variable2 = strtoint32(tokens[1].c_str());
    constraint.type = tokens[2];
    constraint.operation = tokens[3];
    constraint.value = strtoint32(tokens[4].c_str());

    if (tokens.size() > 5) {
      constraint.weight_index = strtoint32(tokens[5].c_str());
    }
    constraints_.push_back(constraint);
  }
}

// ParametersParser Implementation
const int ParametersParser::constraint_coefficient_no_;
const int ParametersParser::variable_coefficient_no_;
const int ParametersParser::coefficient_no_;

ParametersParser::ParametersParser(const std::string& data_directory)
    : filename_(data_directory + "/cst.txt"),
      objective_(""),
      constraint_weights_(constraint_coefficient_no_, 0),
      variable_weights_(variable_coefficient_no_, 0) {}

ParametersParser::~ParametersParser() {}

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
      largest_token =
          largest_token || (line.find("largest") != std::string::npos);
      value_token = value_token || (line.find("value") != std::string::npos);
      number_token = number_token || (line.find("number") != std::string::npos);
      values_token = values_token || (line.find("values") != std::string::npos);
      coefficient =
          coefficient || (line.find("coefficient") != std::string::npos);
    }

    if (coefficient) {
      CHECK_EQ(coefficient_no_,
               constraint_coefficient_no_ + variable_coefficient_no_);
      objective = false;
      if (line.find("=") != std::string::npos) {
        std::vector<std::string> tokens =
            absl::StrSplit(line, ' ', absl::SkipEmpty());
        CHECK_GE(tokens.size(), 3);
        coefficients.push_back(strtoint32(tokens[2].c_str()));
      }
    }
  }

  if (coefficient) {
    CHECK_EQ(coefficient_no_, coefficients.size());
    for (int i = 0; i < coefficient_no_; i++) {
      if (i < constraint_coefficient_no_) {
        constraint_weights_[i] = coefficients[i];
      } else {
        variable_weights_[i - constraint_coefficient_no_] = coefficients[i];
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
                    const std::map<int, FapVariable>& variables,
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
      CHECK(gtl::ContainsKey(*components, component_index));
      gtl::InsertOrUpdate(&((*components)[component_index].variables),
                          variable_id2, variable2);
      in_component[variable_id2] = component_index;
      (*components)[component_index].constraints.push_back(constraint);
    } else if (in_component[variable_id1] < 0 &&
               in_component[variable_id2] >= 0) {
      // If variable2 belongs to an existing component, variable1 should
      // also be included in the same component.
      const int component_index = in_component[variable_id2];
      CHECK(gtl::ContainsKey(*components, component_index));
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
      CHECK(gtl::ContainsKey(*components, min_component_index));
      CHECK(gtl::ContainsKey(*components, max_component_index));
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

int EvaluateConstraintImpact(const std::map<int, FapVariable>& variables,
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
                   std::map<int, FapVariable>* variables,
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
#endif  // OR_TOOLS_EXAMPLES_FAP_PARSER_H_
