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

#include <stddef.h>
#include <string.h>
#include "base/hash.h"
#include "base/hash.h"
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {
// ---------- ArgumentHolder ----------

const string& ArgumentHolder::TypeName() const {
  return type_name_;
}

void ArgumentHolder::SetTypeName(const string& type_name) {
  type_name_ = type_name;
}

void ArgumentHolder::SetIntegerArgument(const string& arg_name, int64 value) {
  integer_argument_[arg_name] = value;
}

void ArgumentHolder::SetIntegerArrayArgument(
    const string& arg_name,
    const int64* const values,
    int size) {
  for (int i = 0; i < size; ++i) {
    integer_array_argument_[arg_name].push_back(values[i]);
  }
}

void ArgumentHolder::SetIntegerMatrixArgument(
    const string& arg_name,
    const IntTupleSet& values) {
  std::pair<string, IntTupleSet> to_insert= std::make_pair(arg_name, values);
  matrix_argument_.insert(to_insert);
}

void ArgumentHolder::SetIntegerExpressionArgument(
    const string& arg_name,
    const IntExpr* const expr) {
  integer_expression_argument_[arg_name] = expr;
}

void ArgumentHolder::SetIntegerVariableArrayArgument(
    const string& arg_name,
    const IntVar* const * const vars,
    int size) {
  for (int i = 0; i < size; ++i) {
    integer_variable_array_argument_[arg_name].push_back(vars[i]);
  }
}

void ArgumentHolder::SetIntervalArgument(
    const string& arg_name,
    const IntervalVar* const var) {
  interval_argument_[arg_name] = var;
}

void ArgumentHolder::SetIntervalArrayArgument(
    const string& arg_name,
    const IntervalVar* const * const vars,
    int size) {
  for (int i = 0; i < size; ++i) {
    interval_array_argument_[arg_name].push_back(vars[i]);
  }
}

void ArgumentHolder::SetSequenceArgument(
    const string& arg_name,
    const SequenceVar* const var) {
  sequence_argument_[arg_name] = var;
}

void ArgumentHolder::SetSequenceArrayArgument(
    const string& arg_name,
    const SequenceVar* const * const vars,
    int size) {
  for (int i = 0; i < size; ++i) {
    sequence_array_argument_[arg_name].push_back(vars[i]);
  }
}

bool ArgumentHolder::HasIntegerExpressionArgument(
    const string& arg_name) const {
  return ContainsKey(integer_expression_argument_, arg_name);
}

bool ArgumentHolder::HasIntegerVariableArrayArgument(
    const string& arg_name) const {
  return ContainsKey(integer_variable_array_argument_, arg_name);
}


int64 ArgumentHolder::FindIntegerArgumentWithDefault(
    const string& arg_name,
    int64 def) const {
  return FindWithDefault(integer_argument_, arg_name, def);
}

int64 ArgumentHolder::FindIntegerArgumentOrDie(const string& arg_name) const {
  return FindOrDie(integer_argument_, arg_name);
}

const std::vector<int64>& ArgumentHolder::FindIntegerArrayArgumentOrDie(
    const string& arg_name) const {
  return FindOrDie(integer_array_argument_, arg_name);
}


const IntExpr* ArgumentHolder::FindIntegerExpressionArgumentOrDie(
    const string& arg_name) const {
  return FindOrDie(integer_expression_argument_, arg_name);
}

const std::vector<const IntVar*>&
ArgumentHolder::FindIntegerVariableArrayArgumentOrDie(
    const string& arg_name) const {
  return FindOrDie(integer_variable_array_argument_, arg_name);
}

const IntTupleSet& ArgumentHolder::FindIntegerMatrixArgumentOrDie(
    const string& arg_name) const {
  return FindOrDie(matrix_argument_, arg_name);
}

// ---------- ModelParser ---------

ModelParser::ModelParser() {}

ModelParser::~ModelParser() {
  CHECK(holders_.empty());
}

void ModelParser::BeginVisitModel(const string& solver_name) {
  PushArgumentHolder();
}

void ModelParser::EndVisitModel(const string& solver_name) {
  PopArgumentHolder();
}

void ModelParser::BeginVisitConstraint(const string& type_name,
                                       const Constraint* const constraint) {
  PushArgumentHolder();
}

void ModelParser::EndVisitConstraint(const string& type_name,
                                     const Constraint* const constraint) {
  // Constraint parsing is usually done here.
  PopArgumentHolder();
}

void ModelParser::BeginVisitIntegerExpression(const string& type_name,
                                              const IntExpr* const expr) {
  PushArgumentHolder();
}

void ModelParser::EndVisitIntegerExpression(const string& type_name,
                                            const IntExpr* const expr) {
  // Expression parsing is usually done here.
  PopArgumentHolder();
}

void ModelParser::VisitIntegerVariable(const IntVar* const variable,
                                       const IntExpr* const delegate) {
  // Usual place for parsing.
}

void ModelParser::VisitIntegerVariable(const IntVar* const variable,
                                       const string& operation,
                                       int64 value,
                                       const IntVar* const delegate) {
  delegate->Accept(this);
  // Usual place for parsing.
}

void ModelParser::VisitIntervalVariable(const IntervalVar* const variable,
                                        const string& operation,
                                        int64 value,
                                        const IntervalVar* const delegate) {
  if (delegate != NULL) {
    delegate->Accept(this);
  }
  // Usual place for parsing.
}

void ModelParser::VisitIntervalVariable(const IntervalVar* const variable,
                                        const string& operation,
                                        const IntervalVar* const * delegates,
                                        int size) {
  for (int i = 0; i < size; ++i) {
    delegates[i]->Accept(this);
  }
  // Usual place for parsing.
}

void ModelParser::VisitSequenceVariable(const SequenceVar* const variable) {
  // Usual place for parsing.
}

// Integer arguments
void ModelParser::VisitIntegerArgument(const string& arg_name, int64 value) {
  Top()->SetIntegerArgument(arg_name, value);
}

void ModelParser::VisitIntegerArrayArgument(const string& arg_name,
                                            const int64* const values,
                                            int size) {
  Top()->SetIntegerArrayArgument(arg_name, values, size);
}

void ModelParser::VisitIntegerMatrixArgument(const string& arg_name,
                                             const IntTupleSet& values) {
  Top()->SetIntegerMatrixArgument(arg_name, values);
}

// Variables.
void ModelParser::VisitIntegerExpressionArgument(
    const string& arg_name,
    const IntExpr* const argument) {
  Top()->SetIntegerExpressionArgument(arg_name, argument);
  argument->Accept(this);
}

void ModelParser::VisitIntegerVariableArrayArgument(
    const string& arg_name,
    const IntVar* const * arguments,
    int size) {
  Top()->SetIntegerVariableArrayArgument(arg_name, arguments, size);
  for (int i = 0; i < size; ++i) {
    arguments[i]->Accept(this);
  }
}

// Visit interval argument.
void ModelParser::VisitIntervalArgument(
    const string& arg_name,
    const IntervalVar* const argument) {
  Top()->SetIntervalArgument(arg_name, argument);
  argument->Accept(this);
}

void ModelParser::VisitIntervalArrayArgument(
    const string& arg_name,
    const IntervalVar* const * arguments,
    int size) {
  Top()->SetIntervalArrayArgument(arg_name, arguments, size);
  for (int i = 0; i < size; ++i) {
    arguments[i]->Accept(this);
  }
}

// Visit sequence argument.
void ModelParser::VisitSequenceArgument(
    const string& arg_name,
    const SequenceVar* const argument) {
  Top()->SetSequenceArgument(arg_name, argument);
  argument->Accept(this);
}

void ModelParser::VisitSequenceArrayArgument(
    const string& arg_name,
    const SequenceVar* const * arguments,
    int size) {
  Top()->SetSequenceArrayArgument(arg_name, arguments, size);
  for (int i = 0; i < size; ++i) {
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
