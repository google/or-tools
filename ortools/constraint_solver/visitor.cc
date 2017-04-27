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


#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

namespace operations_research {
// ---------- ArgumentHolder ----------

const std::string& ArgumentHolder::TypeName() const { return type_name_; }

void ArgumentHolder::SetTypeName(const std::string& type_name) {
  type_name_ = type_name;
}

void ArgumentHolder::SetIntegerArgument(const std::string& arg_name, int64 value) {
  integer_argument_[arg_name] = value;
}

void ArgumentHolder::SetIntegerArrayArgument(const std::string& arg_name,
                                             const std::vector<int64>& values) {
  integer_array_argument_[arg_name] = values;
}

void ArgumentHolder::SetIntegerMatrixArgument(const std::string& arg_name,
                                              const IntTupleSet& values) {
  std::pair<std::string, IntTupleSet> to_insert = std::make_pair(arg_name, values);
  matrix_argument_.insert(to_insert);
}

void ArgumentHolder::SetIntegerExpressionArgument(const std::string& arg_name,
                                                  IntExpr* const expr) {
  integer_expression_argument_[arg_name] = expr;
}

void ArgumentHolder::SetIntegerVariableArrayArgument(
    const std::string& arg_name, const std::vector<IntVar*>& vars) {
  integer_variable_array_argument_[arg_name] = vars;
}

void ArgumentHolder::SetIntervalArgument(const std::string& arg_name,
                                         IntervalVar* const var) {
  interval_argument_[arg_name] = var;
}

void ArgumentHolder::SetIntervalArrayArgument(
    const std::string& arg_name, const std::vector<IntervalVar*>& vars) {
  interval_array_argument_[arg_name] = vars;
}

void ArgumentHolder::SetSequenceArgument(const std::string& arg_name,
                                         SequenceVar* const var) {
  sequence_argument_[arg_name] = var;
}

void ArgumentHolder::SetSequenceArrayArgument(
    const std::string& arg_name, const std::vector<SequenceVar*>& vars) {
  sequence_array_argument_[arg_name] = vars;
}

bool ArgumentHolder::HasIntegerExpressionArgument(
    const std::string& arg_name) const {
  return ContainsKey(integer_expression_argument_, arg_name);
}

bool ArgumentHolder::HasIntegerVariableArrayArgument(
    const std::string& arg_name) const {
  return ContainsKey(integer_variable_array_argument_, arg_name);
}

int64 ArgumentHolder::FindIntegerArgumentWithDefault(const std::string& arg_name,
                                                     int64 def) const {
  return FindWithDefault(integer_argument_, arg_name, def);
}

int64 ArgumentHolder::FindIntegerArgumentOrDie(const std::string& arg_name) const {
  return FindOrDie(integer_argument_, arg_name);
}

const std::vector<int64>& ArgumentHolder::FindIntegerArrayArgumentOrDie(
    const std::string& arg_name) const {
  return FindOrDie(integer_array_argument_, arg_name);
}

IntExpr* ArgumentHolder::FindIntegerExpressionArgumentOrDie(
    const std::string& arg_name) const {
  return FindOrDie(integer_expression_argument_, arg_name);
}

const std::vector<IntVar*>&
ArgumentHolder::FindIntegerVariableArrayArgumentOrDie(
    const std::string& arg_name) const {
  return FindOrDie(integer_variable_array_argument_, arg_name);
}

const IntTupleSet& ArgumentHolder::FindIntegerMatrixArgumentOrDie(
    const std::string& arg_name) const {
  return FindOrDie(matrix_argument_, arg_name);
}

// ---------- ModelParser ---------

ModelParser::ModelParser() {}

ModelParser::~ModelParser() { CHECK(holders_.empty()); }

void ModelParser::BeginVisitModel(const std::string& solver_name) {
  PushArgumentHolder();
}

void ModelParser::EndVisitModel(const std::string& solver_name) {
  PopArgumentHolder();
}

void ModelParser::BeginVisitConstraint(const std::string& type_name,
                                       const Constraint* const constraint) {
  PushArgumentHolder();
}

void ModelParser::EndVisitConstraint(const std::string& type_name,
                                     const Constraint* const constraint) {
  // Constraint parsing is usually done here.
  PopArgumentHolder();
}

void ModelParser::BeginVisitIntegerExpression(const std::string& type_name,
                                              const IntExpr* const expr) {
  PushArgumentHolder();
}

void ModelParser::EndVisitIntegerExpression(const std::string& type_name,
                                            const IntExpr* const expr) {
  // Expression parsing is usually done here.
  PopArgumentHolder();
}

void ModelParser::VisitIntegerVariable(const IntVar* const variable,
                                       IntExpr* const delegate) {
  // Usual place for parsing.
}

void ModelParser::VisitIntegerVariable(const IntVar* const variable,
                                       const std::string& operation, int64 value,
                                       IntVar* const delegate) {
  delegate->Accept(this);
  // Usual place for parsing.
}

void ModelParser::VisitIntervalVariable(const IntervalVar* const variable,
                                        const std::string& operation, int64 value,
                                        IntervalVar* const delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
  // Usual place for parsing.
}

void ModelParser::VisitSequenceVariable(const SequenceVar* const variable) {
  // Usual place for parsing.
}

// Integer arguments
void ModelParser::VisitIntegerArgument(const std::string& arg_name, int64 value) {
  Top()->SetIntegerArgument(arg_name, value);
}

void ModelParser::VisitIntegerArrayArgument(const std::string& arg_name,
                                            const std::vector<int64>& values) {
  Top()->SetIntegerArrayArgument(arg_name, values);
}

void ModelParser::VisitIntegerMatrixArgument(const std::string& arg_name,
                                             const IntTupleSet& values) {
  Top()->SetIntegerMatrixArgument(arg_name, values);
}

// Variables.
void ModelParser::VisitIntegerExpressionArgument(const std::string& arg_name,
                                                 IntExpr* const argument) {
  Top()->SetIntegerExpressionArgument(arg_name, argument);
  argument->Accept(this);
}

void ModelParser::VisitIntegerVariableArrayArgument(
    const std::string& arg_name, const std::vector<IntVar*>& arguments) {
  Top()->SetIntegerVariableArrayArgument(arg_name, arguments);
  for (int i = 0; i < arguments.size(); ++i) {
    arguments[i]->Accept(this);
  }
}

// Visit interval argument.
void ModelParser::VisitIntervalArgument(const std::string& arg_name,
                                        IntervalVar* const argument) {
  Top()->SetIntervalArgument(arg_name, argument);
  argument->Accept(this);
}

void ModelParser::VisitIntervalArrayArgument(
    const std::string& arg_name, const std::vector<IntervalVar*>& arguments) {
  Top()->SetIntervalArrayArgument(arg_name, arguments);
  for (int i = 0; i < arguments.size(); ++i) {
    arguments[i]->Accept(this);
  }
}

// Visit sequence argument.
void ModelParser::VisitSequenceArgument(const std::string& arg_name,
                                        SequenceVar* const argument) {
  Top()->SetSequenceArgument(arg_name, argument);
  argument->Accept(this);
}

void ModelParser::VisitSequenceArrayArgument(
    const std::string& arg_name, const std::vector<SequenceVar*>& arguments) {
  Top()->SetSequenceArrayArgument(arg_name, arguments);
  for (int i = 0; i < arguments.size(); ++i) {
    arguments[i]->Accept(this);
  }
}

void ModelParser::PushArgumentHolder() {
  holders_.push_back(new ArgumentHolder);
}

void ModelParser::PopArgumentHolder() {
  CHECK(!holders_.empty());
  delete holders_.back();
  holders_.pop_back();
}

ArgumentHolder* ModelParser::Top() const {
  CHECK(!holders_.empty());
  return holders_.back();
}
}  // namespace operations_research
