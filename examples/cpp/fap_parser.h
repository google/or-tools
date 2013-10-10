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
// Reading and parsing the data of Frequency Assignment Problem
// Format: http://www.inra.fr/mia/T/schiex/Doc/CELAR.shtml#synt
//

#ifndef OR_TOOLS_EXAMPLES_FAP_PARSER_H_
#define OR_TOOLS_EXAMPLES_FAP_PARSER_H_

#include <map>
#include <string>
#include <vector>
#include "base/logging.h"
#include "base/strtoint.h"
#include "base/split.h"
#include "base/concise_iterator.h"
#include "base/map_util.h"

using std::string;

namespace operations_research {

// Takes a filename and a buffer and fills the lines buffer
// with the lines of the file corresponding to the filename.
void ParseFileByLines(const string& filename, std::vector<string>* lines);

// The FapVariable struct represents a radio link of the
// frequency assignment problem.
// Each variable has the following fields:
// - domain_: a finite set of frequencies that can be assigned to
//           this link
// - domain_index_: the domain index
// - domain_size_: the domain cardinality
// - initial_position_: if positive, it means that the link has already
//                     been assigned a frequency of that value
// - mobility_cost_: cost of modification of a link's assigned value
// - mobility_index_: the index of mobility cost
// - hard_: if true, it means that the link's value cannot be modified
struct FapVariable {
  FapVariable() : domain_index_(-1),
                  initial_position_(-1),
                  mobility_index_(-1),
                  mobility_cost_(-1),
                  hard_(false) { }
  ~FapVariable() { }

  int domain_index_;
  int domain_size_;
  std::vector<int> domain_;
  int initial_position_;
  int mobility_index_;
  int mobility_cost_;
  bool hard_;
};

// The FapConstraint struct represents a constraint between two
// radio links of the frequency assignment problem.
// Each constraint has the following fields:
// - variable1_: the index of the first variable involved in the constraint
// - variable2_: the index of the second variable involved in the constraint
// - type_: the constraint type (D (difference), C (viscosity), F (fixed),
//          P (prefix) or L (far fields)) which is not used in practice
// - operator_: the operator used in the constraint ("=" or ">")
// - value_: the constraint deviation
//           it defines the constant k12 mentioned in FAP description
// - weight_cost_: cost of not satisfaction of the constraint
// - weight_index_: the index of weight cost
// - hard_: if true, it means that the constraint must be satisfied
struct FapConstraint {
  FapConstraint() : variable1_(-1),
                    variable2_(-1),
                    type_(""),
                    operator_(""),
                    value_(-1),
                    weight_index_(-1),
                    weight_cost_(-1),
                    hard_(false) { }
  ~FapConstraint() { }

  int variable1_;
  int variable2_;
  string type_;
  string operator_;
  int value_;
  int weight_index_;
  int weight_cost_;
  bool hard_;
};

// Parser of the var.txt file.
// This file describes all the variables in the instance.
// Each line corresponds to one variable.
class VariableParser {
 public:
  explicit VariableParser(const string& data_directory);
  ~VariableParser();

  const std::map<int, FapVariable>& variables() const { return variables_; }

  void Parse();

 private:
  const string filename_;
  std::map<int, FapVariable> variables_;

  DISALLOW_COPY_AND_ASSIGN(VariableParser);
};

// Parser of the dom.txt file.
// This file describes the domains used by the variables of the problem.
// Each line describes one domain.
class DomainParser {
 public:
  explicit DomainParser(const string& data_directory);
  ~DomainParser();

  int domain_cardinality() const { return domain_cardinality_; }
  const std::map<int, std::vector<int> >& domains() const { return domains_; }

  void Parse();

 private:
  const string filename_;
  int domain_cardinality_;
  std::map<int, std::vector<int> > domains_;

  DISALLOW_COPY_AND_ASSIGN(DomainParser);
};

// Parse ctr.txt file.
// This file describes the constraints of the instance.
// Each line defines a binary constraint.
class ConstraintParser {
 public:
  explicit ConstraintParser(const string& data_directory);
  ~ConstraintParser();

  const std::vector<FapConstraint>& constraints() const { return constraints_; }

  void Parse();

 private:
  const string filename_;
  std::vector<FapConstraint> constraints_;

  DISALLOW_COPY_AND_ASSIGN(ConstraintParser);
};

// Parse cst.txt file.
// This file defines the criterion on which the solution will be based.
// It may also contain 8 coefficients: 4 for different constraint violation
// costs and 4 for different variable mobility costs.
class ParametersParser {
 public:
  explicit ParametersParser(const string& data_directory);
  ~ParametersParser();

  string objective() const { return objective_; }
  const std::vector<int>& constraint_weights() const { return constraint_weights_; }
  const std::vector<int>& variable_weights() const { return variable_weights_; }

  void Parse();

 private:
  const string filename_;
  static const int constraint_coefficient_no_ = 4;
  static const int variable_coefficient_no_ = 4;
  static const int coefficient_no_ = 8;
  string objective_;
  std::vector<int> constraint_weights_;
  std::vector<int> variable_weights_;
};

// Function that parses an instance of frequency assignment problem.
void ParseInstance(const string& data_directory,
                   std::map<int, FapVariable>* variables,
                   std::vector<FapConstraint>* constraints,
                   string* objective,
                   std::vector<int>* frequencies);

}  // namespace operations_research
#endif  // OR_TOOLS_EXAMPLES_FAP_PARSER_H_
