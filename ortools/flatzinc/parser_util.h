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

// Struct and utility functions used by the code in parser.yy
// Included in parser.tab.hh.

#ifndef OR_TOOLS_FLATZINC_PARSER_UTIL_H_
#define OR_TOOLS_FLATZINC_PARSER_UTIL_H_

#include <cmath>
#include <unordered_map>

#include "ortools/base/map_util.h"
#include "ortools/flatzinc/model.h"

using operations_research::HasPrefixString;
using operations_research::HasSuffixString;
using operations_research::StringPrintf;

namespace operations_research {
namespace fz {
// This is the context used during parsing.
struct ParserContext {
  std::unordered_map<std::string, int64> integer_map;
  std::unordered_map<std::string, std::vector<int64>> integer_array_map;
  std::unordered_map<std::string, double> float_map;
  std::unordered_map<std::string, std::vector<double>> float_array_map;
  std::unordered_map<std::string, IntegerVariable*> variable_map;
  std::unordered_map<std::string, std::vector<IntegerVariable*>> variable_array_map;
  std::unordered_map<std::string, Domain> domain_map;
  std::unordered_map<std::string, std::vector<Domain>> domain_array_map;
};

// An optional reference to a variable, or an integer value, used in
// assignments during the declaration of a variable, or a variable
// array.
struct VariableRefOrValue {
  static VariableRefOrValue Undefined() {
    VariableRefOrValue result;
    result.variable = nullptr;
    result.value = 0;
    result.defined = false;
    return result;
  }
  static VariableRefOrValue VariableRef(IntegerVariable* var) {
    VariableRefOrValue result;
    result.variable = var;
    result.value = 0;
    result.defined = true;
    return result;
  }
  static VariableRefOrValue Value(int64 value) {
    VariableRefOrValue result;
    result.variable = nullptr;
    result.value = value;
    result.defined = true;
    return result;
  }

  IntegerVariable* variable;
  int64 value;
  bool defined;
};

struct VariableRefOrValueArray {
  std::vector<IntegerVariable*> variables;
  std::vector<int64> values;

  void PushBack(const VariableRefOrValue& v) {
    CHECK(v.defined);
    variables.push_back(v.variable);
    values.push_back(v.value);
  }

  int Size() const { return values.size(); }
};

// Class needed to pass information from the lexer to the parser.
// TODO(user): Use std::unique_ptr<std::vector< >> to ease memory management.
struct LexerInfo {
  int64 integer_value;
  double double_value;
  std::string string_value;
  Domain domain;
  std::vector<Domain>* domains;
  std::vector<int64>* integers;
  std::vector<double>* doubles;
  Argument arg;
  std::vector<Argument>* args;
  Annotation annotation;
  std::vector<Annotation>* annotations;
  VariableRefOrValue var_or_value;
  VariableRefOrValueArray* var_or_value_array;
};

// If the argument is an integer, return it as int64. Otherwise, die.
int64 ConvertAsIntegerOrDie(double d);
}  // namespace fz
}  // namespace operations_research
#endif  // OR_TOOLS_FLATZINC_PARSER_UTIL_H_
