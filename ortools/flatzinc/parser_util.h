// Copyright 2010-2022 Google LLC
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
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/flatzinc/model.h"

namespace operations_research {
namespace fz {
// This is the context used during parsing.
struct ParserContext {
  absl::flat_hash_map<std::string, int64_t> integer_map;
  absl::flat_hash_map<std::string, std::vector<int64_t>> integer_array_map;
  absl::flat_hash_map<std::string, double> float_map;
  absl::flat_hash_map<std::string, std::vector<double>> float_array_map;
  absl::flat_hash_map<std::string, Variable*> variable_map;
  absl::flat_hash_map<std::string, std::vector<Variable*>> variable_array_map;
  absl::flat_hash_map<std::string, Domain> domain_map;
  absl::flat_hash_map<std::string, std::vector<Domain>> domain_array_map;
};

// An optional reference to a variable, or an integer value, used in
// assignments during the declaration of a variable, or a variable
// array.
struct VarRefOrValue {
  static VarRefOrValue Undefined() { return VarRefOrValue(); }
  static VarRefOrValue VarRef(Variable* var) {
    VarRefOrValue result;
    result.variable = var;
    result.defined = true;
    return result;
  }
  static VarRefOrValue Value(int64_t value) {
    VarRefOrValue result;
    result.value = value;
    result.defined = true;
    return result;
  }
  static VarRefOrValue FloatValue(double value) {
    VarRefOrValue result;
    result.float_value = value;
    result.defined = true;
    result.is_float = true;
    return result;
  }

  Variable* variable = nullptr;
  int64_t value = 0;
  double float_value = 0.0;
  bool defined = false;
  bool is_float = false;
};

// Class needed to pass information from the lexer to the parser.
// TODO(user): Use std::unique_ptr<vector< >> to ease memory management.
struct LexerInfo {
  int64_t integer_value;
  double double_value;
  std::string string_value;
  Domain domain;
  std::vector<Domain>* domains;
  std::vector<int64_t>* integers;
  std::vector<double>* doubles;
  Argument arg;
  std::vector<Argument>* args;
  Annotation annotation;
  std::vector<Annotation>* annotations;
  VarRefOrValue var_or_value;
  std::vector<VarRefOrValue>* var_or_value_array;
};

// If the argument is an integer, return it as int64_t. Otherwise, die.
int64_t ConvertAsIntegerOrDie(double d);
}  // namespace fz
}  // namespace operations_research
#endif  // OR_TOOLS_FLATZINC_PARSER_UTIL_H_
