// Copyright 2010-2013 Google
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

#include <map>
#include <string>
#include <vector>
#include "base/file.h"
#include "base/split.h"
#include "base/concise_iterator.h"
#include "base/map_util.h"

#include "cpp/fap_parser.h"

namespace operations_research {

void ParseFileByLines(const string& filename, std::vector<string>* lines) {
  CHECK_NOTNULL(lines);
  File::Init();
  File* file = File::OpenOrDie(filename.c_str(), "r");

  string result;
  const int64 kMaxInputFileSize = 1 << 30;  // 1GB
  file->ReadToString(&result, kMaxInputFileSize);
  file->Close();

  SplitStringUsing(result, "\n", lines);
}

// VariableParser Implementation
VariableParser::VariableParser(const string& data_directory)
    : filename_(data_directory + "/var.txt") { }

VariableParser::~VariableParser() { }

void VariableParser::Parse() {
  std::vector<string> lines;
  ParseFileByLines(filename_, &lines);
  for (ConstIter<std::vector<string> > it(lines); !it.at_end(); ++it) {
    std::vector<string> tokens;
    SplitStringUsing(*it, " ", &tokens);
    if (tokens.empty()) {
        continue;
    }
    CHECK_GE(tokens.size(), 2);

    FapVariable variable;
    variable.domain_index_ = atoi32(tokens[1].c_str());
    if (tokens.size() > 3) {
      variable.initial_position_ = atoi32(tokens[2].c_str());
      variable.mobility_index_ = atoi32(tokens[3].c_str());
    }

    InsertOrUpdate(&variables_, atoi32(tokens[0].c_str()), variable);
  }
}

// DomainParser Implementation
DomainParser::DomainParser(const string& data_directory)
    : filename_(data_directory + "/dom.txt") { }

DomainParser::~DomainParser() { }

void DomainParser::Parse() {
  std::vector<string> lines;
  ParseFileByLines(filename_, &lines);
  for (ConstIter<std::vector<string> > it(lines); !it.at_end(); ++it) {
    std::vector<string> tokens;
    SplitStringUsing(*it, " ", &tokens);
    if (tokens.empty()) {
        continue;
    }
    CHECK_GE(tokens.size(), 2);

    const int key = atoi32(tokens[0].c_str());
    domain_cardinality_ = atoi32(tokens[1].c_str());

    std::vector<int> domain;
    domain.clear();
    for (int i = 2; i < tokens.size(); ++i) {
      domain.push_back(atoi32(tokens[i].c_str()));
    }

    if (!domain.empty()) {
      InsertOrUpdate(&domains_, key, domain);
    }
  }
}

// ConstraintParser Implementation
ConstraintParser::ConstraintParser(const string& data_directory)
    : filename_(data_directory + "/ctr.txt") { }

ConstraintParser::~ConstraintParser() { }

void ConstraintParser::Parse() {
  std::vector<string> lines;
  ParseFileByLines(filename_, &lines);
  for (ConstIter<std::vector<string> > it(lines); !it.at_end(); ++it) {
    std::vector<string> tokens;
    SplitStringUsing(*it, " ", &tokens);
    if (tokens.empty()) {
        continue;
    }
    CHECK_GE(tokens.size(), 5);

    FapConstraint constraint;
    constraint.variable1_ = atoi32(tokens[0].c_str());
    constraint.variable2_ = atoi32(tokens[1].c_str());
    constraint.type_ = tokens[2];
    constraint.operator_ = tokens[3];
    constraint.value_ = atoi32(tokens[4].c_str());

    if (tokens.size() > 5) {
      constraint.weight_index_ = atoi32(tokens[5].c_str());
    }

    constraints_.push_back(constraint);
  }
}

// ParametersParser Implementation
ParametersParser::ParametersParser(const string& data_directory)
    : filename_(data_directory + "/cst.txt") ,
      objective_(""),
      constraint_weights_(constraint_coefficient_no_, 0),
      variable_weights_(variable_coefficient_no_, 0) { }

ParametersParser::~ParametersParser() { }

void ParametersParser::Parse() {
  bool objective = true;
  bool largest_token = false;
  bool value_token = false;
  bool number_token = false;
  bool values_token = false;
  bool coefficient = false;
  std::vector<int> coefficients;
  std::vector<string> lines;

  ParseFileByLines(filename_, &lines);
  for (ConstIter<std::vector<string> > it(lines); !it.at_end(); ++it) {
    if (objective) {
      largest_token = largest_token || (it->find("largest") != string::npos);
      value_token = value_token || (it->find("value") != string::npos);
      number_token = number_token || (it->find("number") != string::npos);
      values_token = values_token || (it->find("values") != string::npos);
      coefficient = coefficient || (it->find("coefficient") != string::npos);
    }

    if (coefficient) {
      CHECK_EQ(coefficient_no_,
               constraint_coefficient_no_ + variable_coefficient_no_);
      objective = false;
      if (it->find("=") != string::npos) {
        std::vector<string> tokens;
        SplitStringUsing(*it, " ", &tokens);
        CHECK_GE(tokens.size(), 3);
        coefficients.push_back(atoi32(tokens[2].c_str()));
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


void ParseInstance(const string& data_directory,
                   std::map<int, FapVariable>* variables,
                   std::vector<FapConstraint>* constraints, string* objective,
                   std::vector<int>* frequencies) {
  CHECK_NOTNULL(variables);
  CHECK_NOTNULL(constraints);
  CHECK_NOTNULL(objective);

  VariableParser var(data_directory);
  var.Parse();
  *variables = var.variables();

  ConstraintParser ctr(data_directory);
  ctr.Parse();
  *constraints = ctr.constraints();

  DomainParser dom(data_directory);
  dom.Parse();

  ParametersParser cst(data_directory);
  cst.Parse();

  for (MutableIter<std::map<int, FapVariable> > it(*variables); !it.at_end(); ++it) {
    it->second.domain_ = FindOrDie(dom.domains(), it->second.domain_index_);
    it->second.domain_size_ = dom.domain_cardinality();
    if ((it->second.mobility_index_ == -1) ||
        (it->second.mobility_index_ == 0)) {
      it->second.mobility_cost_ = -1;
      if (it->second.initial_position_ != -1) {
        it->second.hard_ = true;
      }
    } else {
      it->second.mobility_cost_ =
        (cst.variable_weights())[it->second.mobility_index_-1];
    }
  }
  *frequencies = FindOrDie(dom.domains(), 0);
  *objective = cst.objective();

  for (MutableIter<std::vector<FapConstraint> > it(*constraints);
                                           !it.at_end(); ++it) {
    if ((it->weight_index_ == -1) || (it->weight_index_ == 0)) {
      it->weight_cost_ = -1;
      it->hard_ = true;
    } else {
      it->weight_cost_ = (cst.constraint_weights())[it->weight_index_-1];
    }
  }
}
}  // namespace operations_research
