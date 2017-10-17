// Copyright 2010-2017 Google
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

#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include "ortools/base/logging.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/split.h"
#include "ortools/base/map_util.h"

namespace operations_research {

// Takes a filename and a buffer and fills the lines buffer
// with the lines of the file corresponding to the filename.
void ParseFileByLines(const std::string& filename, std::vector<std::string>* lines);

// The FapVariable struct represents a radio link of the
// frequency assignment problem.
struct FapVariable {
  FapVariable()
      : domain_index(-1),
        domain_size(0),
        domain(),
        degree(0),
        initial_position(-1),
        mobility_index(-1),
        mobility_cost(-1),
        hard(false) {}

  ~FapVariable() {}

  // Fields:
  // the index of a subset of all available frequencies of the instance
  int domain_index;

  // the number of the frequencies available for the link
  int domain_size;

  // the link's domain, i.e. a finite set of frequencies that can be
  // assigned to this link
  std::vector<int> domain;

  // the number of constraints in which the link appears
  int degree;

  // if positive, it means that the link has already been assigned a frequency
  // of that value
  int initial_position;

  // the index of mobility cost
  int mobility_index;

  // the cost of modification of a link's pre-assigned value
  int mobility_cost;

  // if true, it means that the link's pre-assigned position cannot be modified
  bool hard;
};

// The FapConstraint struct represents a constraint between two
// radio links of the frequency assignment problem.
struct FapConstraint {
  FapConstraint()
      : variable1(-1),
        variable2(-1),
        impact(0),
        type(""),
        operation(""),
        value(-1),
        weight_index(-1),
        weight_cost(-1),
        hard(false) {}
  ~FapConstraint() {}

  // Fields:
  // the index of the first variable appearing in the constraint
  int variable1;

  // the index of the second variable appearing in the constraint
  int variable2;

  // the importance of a constraint based on the degree of its variables,
  // the operator used in the constraint ("=" or ">") and whether it is a hard
  // or soft constraint and with what weight cost.
  // impact = (max_degree + min_degree + operator_impact + hardness_impact)
  int impact;

  // the constraint type (D (difference), C (viscosity), F (fixed),P (prefix)
  // or L (far fields)) which is not used in practice
  std::string type;

  // the operator used in the constraint ("=" or ">")
  std::string operation;

  // the constraint deviation: it defines the constant k12 mentioned in RLFAP
  // description
  int value;

  // the index of weight cost
  int weight_index;

  // the cost of not satisfaction of the constraint
  int weight_cost;

  // if true, it means that the constraint must be satisfied
  bool hard;
};

// The FapComponent struct represents an component of the RLFAP graph.
// It models an independent sub-problem of the initial instance.
struct FapComponent {
  FapComponent() : variables(), constraints() {}
  ~FapComponent() {}

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
  static const int constraint_coefficient_no_ = 4;
  static const int variable_coefficient_no_ = 4;
  static const int coefficient_no_ = 8;
  std::string objective_;
  std::vector<int> constraint_weights_;
  std::vector<int> variable_weights_;
};

// Function that finds the disjoint sub-graphs of the graph of the instance.
void FindComponents(const std::vector<FapConstraint>& constraints,
                    const std::map<int, FapVariable>& variables,
                    const int maximum_variable_id,
                    std::unordered_map<int, FapComponent>* components);

// Function that computes the impact of a constraint.
int EvaluateConstraintImpact(const std::map<int, FapVariable>& variables,
                             const int max_weight_cost,
                             const FapConstraint constraint);

// Function that parses an instance of frequency assignment problem.
void ParseInstance(const std::string& data_directory, bool find_components,
                   std::map<int, FapVariable>* variables,
                   std::vector<FapConstraint>* constraints, std::string* objective,
                   std::vector<int>* frequencies,
                   std::unordered_map<int, FapComponent>* components);
}  // namespace operations_research
#endif  // OR_TOOLS_EXAMPLES_FAP_PARSER_H_
