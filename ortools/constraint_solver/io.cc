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


#include <algorithm>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/callback.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/model.pb.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/util/tuple_set.h"
#include "ortools/util/vector_map.h"

namespace operations_research {

Constraint* SetIsEqual(IntVar* const var, const std::vector<int64>& values,
                       const std::vector<IntVar*>& vars);

Constraint* SetIsGreaterOrEqual(IntVar* const var,
                                const std::vector<int64>& values,
                                const std::vector<IntVar*>& vars);

namespace {
// ---------- Model Protobuf Writers -----------

// ----- First Pass visitor -----

// This visitor collects all constraints and expressions.  It sorts the
// expressions, such that we can build them in sequence using
// previously created expressions.
class FirstPassVisitor : public ModelVisitor {
 public:
  FirstPassVisitor() {}  // Needed for Visual Studio.
  ~FirstPassVisitor() override {}

  std::string DebugString() const override { return "FirstPassVisitor"; }

  // Begin/End visit element.
  void BeginVisitModel(const std::string& solver_name) override {
    // Reset statistics.
    expression_map_.clear();
    delegate_map_.clear();
    expression_list_.clear();
    constraint_list_.clear();
    interval_list_.clear();
    sequence_list_.clear();
  }

  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override {
    Register(constraint);
  }

  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* const expression) override {
    Register(expression);
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
    if (delegate != nullptr) {
      delegate->Accept(this);
      delegate_map_[variable] = delegate;
    }
    Register(variable);
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    delegate->Accept(this);
    delegate_map_[variable] = delegate;
    Register(variable);
  }

  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override {
    if (delegate != nullptr) {
      delegate->Accept(this);
    }
    Register(variable);
  }

  void VisitSequenceVariable(const SequenceVar* const sequence) override {
    for (int i = 0; i < sequence->size(); ++i) {
      sequence->Interval(i)->Accept(this);
    }
    Register(sequence);
  }

  // Visit integer expression argument.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* const argument) override {
    VisitSubArgument(argument);
  }

  void VisitSequenceArrayArgument(
      const std::string& arg_name,
      const std::vector<SequenceVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Export
  const std::unordered_map<const IntExpr*, int>& expression_map() const {
    return expression_map_;
  }
  const std::unordered_map<const IntervalVar*, int>& interval_map() const {
    return interval_map_;
  }
  const std::unordered_map<const SequenceVar*, int>& sequence_map() const {
    return sequence_map_;
  }
  const std::unordered_map<const IntVar*, IntExpr*>& delegate_map() const {
    return delegate_map_;
  }
  const std::vector<const IntExpr*>& expression_list() const {
    return expression_list_;
  }
  const std::vector<const Constraint*>& constraint_list() const {
    return constraint_list_;
  }
  const std::vector<const IntervalVar*>& interval_list() const {
    return interval_list_;
  }
  const std::vector<const SequenceVar*>& sequence_list() const {
    return sequence_list_;
  }

 private:
  void Register(const IntExpr* const expression) {
    if (!ContainsKey(expression_map_, expression)) {
      const int index = expression_map_.size();
      CHECK_EQ(index, expression_list_.size());
      expression_map_[expression] = index;
      expression_list_.push_back(expression);
    }
  }

  void Register(const Constraint* const constraint) {
    constraint_list_.push_back(constraint);
  }

  void Register(const IntervalVar* const interval) {
    if (!ContainsKey(interval_map_, interval)) {
      const int index = interval_map_.size();
      CHECK_EQ(index, interval_list_.size());
      interval_map_[interval] = index;
      interval_list_.push_back(interval);
    }
  }

  void Register(const SequenceVar* const sequence) {
    if (!ContainsKey(sequence_map_, sequence)) {
      const int index = sequence_map_.size();
      CHECK_EQ(index, sequence_list_.size());
      sequence_map_[sequence] = index;
      sequence_list_.push_back(sequence);
    }
  }

  void VisitSubArgument(IntExpr* const expression) {
    if (!ContainsKey(expression_map_, expression)) {
      expression->Accept(this);
    }
  }

  void VisitSubArgument(IntervalVar* const interval) {
    if (!ContainsKey(interval_map_, interval)) {
      interval->Accept(this);
    }
  }

  void VisitSubArgument(SequenceVar* const sequence) {
    if (!ContainsKey(sequence_map_, sequence)) {
      sequence->Accept(this);
    }
  }

  const std::string filename_;
  std::unordered_map<const IntExpr*, int> expression_map_;
  std::unordered_map<const IntervalVar*, int> interval_map_;
  std::unordered_map<const SequenceVar*, int> sequence_map_;
  std::unordered_map<const IntVar*, IntExpr*> delegate_map_;
  std::vector<const IntExpr*> expression_list_;
  std::vector<const Constraint*> constraint_list_;
  std::vector<const IntervalVar*> interval_list_;
  std::vector<const SequenceVar*> sequence_list_;
};

// ----- Argument Holder -----

class ArgumentHolder {
 public:
  template <class P>
  void ExportToProto(VectorMap<std::string>* const tags, P* const proto) const {
    for (const auto& it : integer_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      arg_proto->set_integer_value(it.second);
      arg_proto->set_type(CpArgument::INTEGER_VALUE);
    }

    for (const auto& it : integer_array_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      for (int64 value : it.second) {
        arg_proto->add_integer_array(value);
      }
      arg_proto->set_type(CpArgument::INTEGER_ARRAY);
    }

    for (const auto& it : integer_matrix_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      CpIntegerMatrix* const matrix_proto = arg_proto->mutable_integer_matrix();
      const int columns = it.second.first;
      CHECK_GT(columns, 0);
      const int rows = it.second.second.size() / columns;
      matrix_proto->set_rows(rows);
      matrix_proto->set_columns(columns);
      for (int64 value : it.second.second) {
        matrix_proto->add_values(value);
      }
      arg_proto->set_type(CpArgument::INTEGER_MATRIX);
    }

    for (const auto& it : integer_expression_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      arg_proto->set_integer_expression_index(it.second);
      arg_proto->set_type(CpArgument::EXPRESSION);
    }

    for (const auto& it : integer_variable_array_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      for (int expr : it.second) {
        arg_proto->add_integer_expression_array(expr);
      }
      arg_proto->set_type(CpArgument::EXPRESSION_ARRAY);
    }

    for (const auto& it : interval_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      arg_proto->set_interval_index(it.second);
      arg_proto->set_type(CpArgument::INTERVAL);
    }

    for (const auto& it : interval_array_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      for (int arg : it.second) {
        arg_proto->add_interval_array(arg);
      }
      arg_proto->set_type(CpArgument::INTERVAL_ARRAY);
    }

    for (const auto& it : sequence_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      arg_proto->set_sequence_index(it.second);
      arg_proto->set_type(CpArgument::SEQUENCE);
    }

    for (const auto& it : sequence_array_argument_) {
      CpArgument* const arg_proto = proto->add_arguments();
      arg_proto->set_argument_index(tags->Add(it.first));
      for (int arg : it.second) {
        arg_proto->add_sequence_array(arg);
      }
      arg_proto->set_type(CpArgument::SEQUENCE_ARRAY);
    }
  }

  const std::string& type_name() const { return type_name_; }

  void set_type_name(const std::string& type_name) { type_name_ = type_name; }

  void set_integer_argument(const std::string& arg_name, int64 value) {
    integer_argument_[arg_name] = value;
  }

  void set_integer_array_argument(const std::string& arg_name,
                                  const std::vector<int64>& values) {
    integer_array_argument_[arg_name] = values;
  }

  void set_integer_matrix_argument(const std::string& arg_name,
                                   const IntTupleSet& values) {
    const int rows = values.NumTuples();
    const int columns = values.Arity();
    std::pair<int, std::vector<int64>> matrix =
        std::make_pair(columns, std::vector<int64>());
    integer_matrix_argument_[arg_name] = matrix;
    std::vector<int64>* const vals = &integer_matrix_argument_[arg_name].second;
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < columns; ++j) {
        vals->push_back(values.Value(i, j));
      }
    }
  }

  void set_integer_expression_argument(const std::string& arg_name, int index) {
    integer_expression_argument_[arg_name] = index;
  }

  void set_integer_variable_array_argument(const std::string& arg_name,
                                           const int* const indices, int size) {
    for (int i = 0; i < size; ++i) {
      integer_variable_array_argument_[arg_name].push_back(indices[i]);
    }
  }

  void set_interval_argument(const std::string& arg_name, int index) {
    interval_argument_[arg_name] = index;
  }

  void set_interval_array_argument(const std::string& arg_name,
                                   const int* const indices, int size) {
    for (int i = 0; i < size; ++i) {
      interval_array_argument_[arg_name].push_back(indices[i]);
    }
  }

  void set_sequence_argument(const std::string& arg_name, int index) {
    sequence_argument_[arg_name] = index;
  }

  void set_sequence_array_argument(const std::string& arg_name,
                                   const int* const indices, int size) {
    for (int i = 0; i < size; ++i) {
      sequence_array_argument_[arg_name].push_back(indices[i]);
    }
  }

  int64 FindIntegerArgumentWithDefault(const std::string& arg_name, int64 def) {
    return FindWithDefault(integer_argument_, arg_name, def);
  }

  int64 FindIntegerArgumentOrDie(const std::string& arg_name) {
    return FindOrDie(integer_argument_, arg_name);
  }

  int64 FindIntegerExpressionArgumentOrDie(const std::string& arg_name) {
    return FindOrDie(integer_expression_argument_, arg_name);
  }

 private:
  std::string type_name_;
  std::unordered_map<std::string, int> integer_expression_argument_;
  std::unordered_map<std::string, int64> integer_argument_;
  std::unordered_map<std::string, int> interval_argument_;
  std::unordered_map<std::string, int> sequence_argument_;
  std::unordered_map<std::string, std::vector<int64>> integer_array_argument_;
  std::unordered_map<std::string, std::pair<int, std::vector<int64>>> integer_matrix_argument_;
  std::unordered_map<std::string, std::vector<int>> integer_variable_array_argument_;
  std::unordered_map<std::string, std::vector<int>> interval_array_argument_;
  std::unordered_map<std::string, std::vector<int>> sequence_array_argument_;
};

// ----- Second Pass Visitor -----

static const int kModelVersion = 1;

// The second pass visitor will visited sorted expressions, interval
// vars and expressions and export them to a CpModel protocol
// buffer.
class SecondPassVisitor : public ModelVisitor {
 public:
  SecondPassVisitor(const FirstPassVisitor& first_pass,
                    CpModel* const model_proto)
      : expression_map_(first_pass.expression_map()),
        interval_map_(first_pass.interval_map()),
        sequence_map_(first_pass.sequence_map()),
        delegate_map_(first_pass.delegate_map()),
        expression_list_(first_pass.expression_list()),
        constraint_list_(first_pass.constraint_list()),
        interval_list_(first_pass.interval_list()),
        sequence_list_(first_pass.sequence_list()),
        model_proto_(model_proto) {}

  ~SecondPassVisitor() override {}

  std::string DebugString() const override { return "SecondPassVisitor"; }

  void BeginVisitModel(const std::string& model_name) override {
    model_proto_->set_model(model_name);
    model_proto_->set_version(kModelVersion);
    PushArgumentHolder();
    for (const IntExpr* const expr : expression_list_) {
      expr->Accept(this);
    }

    for (const IntervalVar* const var : interval_list_) {
      var->Accept(this);
    }

    for (const SequenceVar* const seq : sequence_list_) {
      seq->Accept(this);
    }
  }

  void EndVisitModel(const std::string& model_name) override {
    for (ArgumentHolder* const arg : extensions_) {
      WriteModelExtension(arg);
    }
    PopArgumentHolder();
    // Write tags.
    for (int i = 0; i < tags_.size(); ++i) {
      model_proto_->add_tags(tags_.Element(i));
    }
  }

  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint* const constraint) override {
    PushArgumentHolder();
  }

  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override {
    // We ignore cast constraints, they will be regenerated automatically.
    if (constraint->IsCastConstraint()) {
      PopArgumentHolder();
      return;
    }

    const int index = model_proto_->constraints_size();
    CpConstraint* const constraint_proto = model_proto_->add_constraints();
    ExportToProto(constraint, constraint_proto, type_name, index);
    if (constraint->HasName()) {
      constraint_proto->set_name(constraint->name());
    }
    PopArgumentHolder();
  }

  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* const expression) override {
    PushArgumentHolder();
  }

  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* const expression) override {
    const int index = model_proto_->expressions_size();
    CpIntegerExpression* const expression_proto =
        model_proto_->add_expressions();
    ExportToProto(expression, expression_proto, type_name, index);
    PopArgumentHolder();
  }

  void BeginVisitExtension(const std::string& type_name) override {
    PushExtension(type_name);
  }

  void EndVisitExtension(const std::string& type_name) override {
    PopAndSaveExtension();
  }

  void VisitIntegerArgument(const std::string& arg_name, int64 value) override {
    top()->set_integer_argument(arg_name, value);
  }

  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64>& values) override {
    top()->set_integer_array_argument(arg_name, values);
  }

  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override {
    top()->set_integer_matrix_argument(arg_name, values);
  }

  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override {
    top()->set_integer_expression_argument(arg_name,
                                           FindExpressionIndexOrDie(argument));
  }

  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments) override {
    std::vector<int> indices;
    for (int i = 0; i < arguments.size(); ++i) {
      indices.push_back(FindExpressionIndexOrDie(arguments[i]));
    }
    top()->set_integer_variable_array_argument(arg_name, indices.data(),
                                               indices.size());
  }

  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* argument) override {
    top()->set_interval_argument(arg_name, FindIntervalIndexOrDie(argument));
  }

  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override {
    std::vector<int> indices;
    for (int i = 0; i < arguments.size(); ++i) {
      indices.push_back(FindIntervalIndexOrDie(arguments[i]));
    }
    top()->set_interval_array_argument(arg_name, indices.data(),
                                       indices.size());
  }

  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* argument) override {
    top()->set_sequence_argument(arg_name, FindSequenceIndexOrDie(argument));
  }

  void VisitSequenceArrayArgument(
      const std::string& arg_name,
      const std::vector<SequenceVar*>& arguments) override {
    std::vector<int> indices;
    for (int i = 0; i < arguments.size(); ++i) {
      indices.push_back(FindSequenceIndexOrDie(arguments[i]));
    }
    top()->set_sequence_array_argument(arg_name, indices.data(),
                                       indices.size());
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
    if (delegate != nullptr) {
      const int index = model_proto_->expressions_size();
      CpIntegerExpression* const var_proto = model_proto_->add_expressions();
      var_proto->set_index(index);
      var_proto->set_type_index(TagIndex(ModelVisitor::kIntegerVariable));
      CpArgument* const sub_proto = var_proto->add_arguments();
      sub_proto->set_argument_index(
          TagIndex(ModelVisitor::kExpressionArgument));
      sub_proto->set_integer_expression_index(
          FindExpressionIndexOrDie(delegate));
      sub_proto->set_type(CpArgument::EXPRESSION);
    } else {
      const int index = model_proto_->expressions_size();
      CpIntegerExpression* const var_proto = model_proto_->add_expressions();
      var_proto->set_index(index);
      var_proto->set_type_index(TagIndex(ModelVisitor::kIntegerVariable));
      if (variable->HasName()) {
        var_proto->set_name(variable->name());
      }
      if (variable->Size() == variable->Max() - variable->Min() + 1) {
        // Contiguous
        CpArgument* const min_proto = var_proto->add_arguments();
        min_proto->set_argument_index(TagIndex(ModelVisitor::kMinArgument));
        min_proto->set_integer_value(variable->Min());
        min_proto->set_type(CpArgument::INTEGER_VALUE);
        CpArgument* const max_proto = var_proto->add_arguments();
        max_proto->set_argument_index(TagIndex(ModelVisitor::kMaxArgument));
        max_proto->set_integer_value(variable->Max());
        max_proto->set_type(CpArgument::INTEGER_VALUE);
      } else {
        // Non Contiguous
        CpArgument* const values_proto = var_proto->add_arguments();
        values_proto->set_argument_index(
            TagIndex(ModelVisitor::kValuesArgument));
        std::unique_ptr<IntVarIterator> it(variable->MakeDomainIterator(false));
        for (const int64 value : InitAndGetValues(it.get())) {
          values_proto->add_integer_array(value);
        }
        values_proto->set_type(CpArgument::INTEGER_ARRAY);
      }
    }
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    const int index = model_proto_->expressions_size();
    CpIntegerExpression* const var_proto = model_proto_->add_expressions();
    var_proto->set_index(index);
    var_proto->set_type_index(TagIndex(ModelVisitor::kIntegerVariable));
    CpArgument* const sub_proto = var_proto->add_arguments();
    sub_proto->set_argument_index(TagIndex(ModelVisitor::kVariableArgument));
    sub_proto->set_integer_expression_index(FindExpressionIndexOrDie(delegate));
    sub_proto->set_type(CpArgument::EXPRESSION);
    CpArgument* const value_proto = var_proto->add_arguments();
    value_proto->set_argument_index(TagIndex(operation));
    value_proto->set_integer_value(value);
    value_proto->set_type(CpArgument::INTEGER_VALUE);
  }

  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override {
    if (delegate != nullptr) {
      const int index = model_proto_->intervals_size();
      CpIntervalVariable* const var_proto = model_proto_->add_intervals();
      var_proto->set_index(index);
      var_proto->set_type_index(TagIndex(ModelVisitor::kIntervalVariable));
      CpArgument* const sub_proto = var_proto->add_arguments();
      sub_proto->set_argument_index(TagIndex(operation));
      sub_proto->set_interval_index(FindIntervalIndexOrDie(delegate));
      sub_proto->set_integer_value(value);
      if (operation == ModelVisitor::kStartSyncOnStartOperation ||
          operation == ModelVisitor::kStartSyncOnEndOperation) {
        CHECK_EQ(delegate->DurationMin(), delegate->DurationMax());
        sub_proto->add_integer_array(delegate->DurationMin());
      }
    } else {
      const int index = model_proto_->intervals_size();
      CpIntervalVariable* const var_proto = model_proto_->add_intervals();
      var_proto->set_index(index);
      var_proto->set_type_index(TagIndex(ModelVisitor::kIntervalVariable));
      if (variable->HasName()) {
        var_proto->set_name(variable->name());
      }
      CpArgument* const start_min_proto = var_proto->add_arguments();
      start_min_proto->set_argument_index(
          TagIndex(ModelVisitor::kStartMinArgument));
      start_min_proto->set_integer_value(variable->StartMin());
      CpArgument* const start_max_proto = var_proto->add_arguments();
      start_max_proto->set_argument_index(
          TagIndex(ModelVisitor::kStartMaxArgument));
      start_max_proto->set_integer_value(variable->StartMax());
      CpArgument* const end_min_proto = var_proto->add_arguments();
      end_min_proto->set_argument_index(
          TagIndex(ModelVisitor::kEndMinArgument));
      end_min_proto->set_integer_value(variable->EndMin());
      CpArgument* const end_max_proto = var_proto->add_arguments();
      end_max_proto->set_argument_index(
          TagIndex(ModelVisitor::kEndMaxArgument));
      end_max_proto->set_integer_value(variable->EndMax());
      CpArgument* const duration_min_proto = var_proto->add_arguments();
      duration_min_proto->set_argument_index(
          TagIndex(ModelVisitor::kDurationMinArgument));
      duration_min_proto->set_integer_value(variable->DurationMin());
      CpArgument* const duration_max_proto = var_proto->add_arguments();
      duration_max_proto->set_argument_index(
          TagIndex(ModelVisitor::kDurationMaxArgument));
      duration_max_proto->set_integer_value(variable->DurationMax());
      CpArgument* const optional_proto = var_proto->add_arguments();
      optional_proto->set_argument_index(
          TagIndex(ModelVisitor::kOptionalArgument));
      optional_proto->set_integer_value(!variable->MustBePerformed());
    }
  }

  void VisitSequenceVariable(const SequenceVar* const sequence) override {
    const int index = model_proto_->sequences_size();
    CpSequenceVariable* const var_proto = model_proto_->add_sequences();
    var_proto->set_index(index);
    var_proto->set_type_index(TagIndex(ModelVisitor::kSequenceVariable));
    if (sequence->HasName()) {
      var_proto->set_name(sequence->name());
    }
    CpArgument* const sub_proto = var_proto->add_arguments();
    sub_proto->set_argument_index(TagIndex(ModelVisitor::kIntervalsArgument));
    for (int i = 0; i < sequence->size(); ++i) {
      IntervalVar* const interval = sequence->Interval(i);
      sub_proto->add_interval_array(FindIntervalIndexOrDie(interval));
    }
    sub_proto->set_type(CpArgument::INTERVAL_ARRAY);
  }

  int TagIndex(const std::string& tag) { return tags_.Add(tag); }

 private:
  void WriteModelExtension(ArgumentHolder* const holder) {
    CHECK(holder != nullptr);
    if (holder->type_name().compare(kObjectiveExtension) == 0) {
      WriteObjective(holder);
    } else if (holder->type_name().compare(kSearchLimitExtension) == 0) {
      WriteSearchLimit(holder);
    } else if (holder->type_name().compare(kVariableGroupExtension) == 0) {
      WriteVariableGroup(holder);
    } else {
      LOG(INFO) << "Unknown model extension :" << holder->type_name();
    }
  }

  void WriteObjective(ArgumentHolder* const holder) {
    CHECK(holder != nullptr);
    const bool maximize = holder->FindIntegerArgumentOrDie(kMaximizeArgument);
    const int64 step = holder->FindIntegerArgumentOrDie(kStepArgument);
    const int objective_index =
        holder->FindIntegerExpressionArgumentOrDie(kExpressionArgument);
    CpObjective* const objective_proto = model_proto_->mutable_objective();
    objective_proto->set_maximize(maximize);
    objective_proto->set_step(step);
    objective_proto->set_objective_index(objective_index);
  }

  void WriteSearchLimit(ArgumentHolder* const holder) {
    CHECK(holder != nullptr);
    SearchLimitParameters* const proto = model_proto_->mutable_search_limit();
    proto->set_time(
        holder->FindIntegerArgumentWithDefault(kTimeLimitArgument, kint64max));
    proto->set_branches(holder->FindIntegerArgumentWithDefault(
        kBranchesLimitArgument, kint64max));
    proto->set_failures(holder->FindIntegerArgumentWithDefault(
        kFailuresLimitArgument, kint64max));
    proto->set_solutions(holder->FindIntegerArgumentWithDefault(
        kSolutionLimitArgument, kint64max));
    proto->set_smart_time_check(
        holder->FindIntegerArgumentWithDefault(kSmartTimeCheckArgument, false));
    proto->set_cumulative(
        holder->FindIntegerArgumentWithDefault(kCumulativeArgument, false));
  }

  void WriteVariableGroup(ArgumentHolder* const holder) {
    CpVariableGroup* const group_proto = model_proto_->add_variable_groups();
    holder->ExportToProto(&tags_, group_proto);
  }

  template <class A, class P>
  void ExportToProto(const A* const argument, P* const proto,
                     const std::string& type_name, int index) {
    CHECK(proto != nullptr);
    CHECK(argument != nullptr);
    proto->set_index(index);
    proto->set_type_index(TagIndex(type_name));
    if (argument->HasName()) {
      proto->set_name(argument->name());
    }
    top()->ExportToProto(&tags_, proto);
    for (ArgumentHolder* const arg : extensions_) {
      CpExtension* const extension_proto = proto->add_extensions();
      extension_proto->set_type_index(TagIndex(arg->type_name()));
      arg->ExportToProto(&tags_, extension_proto);
    }
  }

  void PushArgumentHolder() { holders_.push_back(new ArgumentHolder); }

  void PopArgumentHolder() {
    CHECK(!holders_.empty());
    delete holders_.back();
    holders_.pop_back();
    STLDeleteElements(&extensions_);
    extensions_.clear();
  }

  void PushExtension(const std::string& type_name) {
    PushArgumentHolder();
    holders_.back()->set_type_name(type_name);
  }

  void PopAndSaveExtension() {
    CHECK(!holders_.empty());
    extensions_.push_back(holders_.back());
    holders_.pop_back();
  }

  ArgumentHolder* top() const {
    CHECK(!holders_.empty());
    return holders_.back();
  }

  int FindExpressionIndexOrDie(IntExpr* const expression) const {
    return FindOrDie(expression_map_, expression);
  }

  int FindIntervalIndexOrDie(IntervalVar* const interval) const {
    return FindOrDie(interval_map_, interval);
  }

  int FindSequenceIndexOrDie(SequenceVar* const sequence) const {
    return FindOrDie(sequence_map_, sequence);
  }

  std::unordered_map<const IntExpr*, int> expression_map_;
  std::unordered_map<const IntervalVar*, int> interval_map_;
  std::unordered_map<const SequenceVar*, int> sequence_map_;
  std::unordered_map<const IntVar*, IntExpr*> delegate_map_;
  std::vector<const IntExpr*> expression_list_;
  std::vector<const Constraint*> constraint_list_;
  std::vector<const IntervalVar*> interval_list_;
  std::vector<const SequenceVar*> sequence_list_;
  CpModel* const model_proto_;

  std::vector<ArgumentHolder*> holders_;
  std::vector<ArgumentHolder*> extensions_;
  VectorMap<std::string> tags_;
};

// ---------- Model Protocol Reader ----------

#define VERIFY(expr) \
  if (!(expr)) return nullptr
#define VERIFY_BOOL(expr) \
  if (!(expr)) return false
#define VERIFY_EQ(e1, e2) \
  if ((e1) != (e2)) return nullptr

// ----- kAbs -----

IntExpr* BuildAbs(CpModelLoader* const builder,
                  const CpIntegerExpression& proto) {
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  return builder->solver()->MakeAbs(expr);
}

// ----- kAbsEqual -----

Constraint* BuildAbsEqual(CpModelLoader* const builder,
                          const CpConstraint& proto) {
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeAbsEquality(expr->Var(), target->Var());
}

// ----- kAllDifferent -----

Constraint* BuildAllDifferent(CpModelLoader* const builder,
                              const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  int64 escape = 0;
  if (builder->ScanArguments(ModelVisitor::kValueArgument, proto, &escape)) {
    return builder->solver()->MakeAllDifferentExcept(vars, escape);
  } else {
    int64 range = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kRangeArgument, proto, &range));
    return builder->solver()->MakeAllDifferent(vars, range);
  }
}

// ----- kAllowedAssignments -----

Constraint* BuildAllowedAssignments(CpModelLoader* const builder,
                                    const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  IntTupleSet tuples(vars.size());
  VERIFY(builder->ScanArguments(ModelVisitor::kTuplesArgument, proto, &tuples));
  return builder->solver()->MakeAllowedAssignments(vars, tuples);
}

// ----- kBetween -----

Constraint* BuildBetween(CpModelLoader* const builder,
                         const CpConstraint& proto) {
  int64 value_min = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kMinArgument, proto, &value_min));
  int64 value_max = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &value_max));
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  return builder->solver()->MakeBetweenCt(expr->Var(), value_min, value_max);
}

// ----- kConditionalExpr -----

IntExpr* BuildConditionalExpr(CpModelLoader* const builder,
                              const CpIntegerExpression& proto) {
  IntExpr* condition = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kVariableArgument, proto,
                                &condition));
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeConditionalExpression(condition->Var(), expr,
                                                      value);
}

// ----- kCircuit -----

Constraint* BuildCircuit(CpModelLoader* const builder,
                         const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kNextsArgument, proto, &vars));
  int64 v;
  VERIFY(builder->ScanArguments(ModelVisitor::kPartialArgument, proto, &v));
  if (v == 1) {
    return builder->solver()->MakeSubCircuit(vars);
  } else {
    return builder->solver()->MakeCircuit(vars);
  }
}

// ----- kConvexPiecewise -----

IntExpr* BuildConvexPiecewise(CpModelLoader* const builder,
                              const CpIntegerExpression& proto) {
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  int64 early_cost = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kEarlyCostArgument, proto,
                                &early_cost));
  int64 early_date = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kEarlyDateArgument, proto,
                                &early_date));
  int64 late_cost = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kLateCostArgument, proto,
                                &late_cost));
  int64 late_date = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kLateDateArgument, proto,
                                &late_date));
  return builder->solver()->MakeConvexPiecewiseExpr(
      expr->Var(), early_cost, early_date, late_date, late_cost);
}

// ----- kCountEqual -----

Constraint* BuildCountEqual(CpModelLoader* const builder,
                            const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  int64 count = 0;
  if (builder->ScanArguments(ModelVisitor::kCountArgument, proto, &value)) {
    return builder->solver()->MakeCount(vars, value, count);
  } else {
    IntExpr* count_expr = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kCountArgument, proto,
                                  &count_expr));
    return builder->solver()->MakeCount(vars, value, count_expr->Var());
  }
}

// ----- kCover -----

Constraint* BuildCover(CpModelLoader* const builder,
                       const CpConstraint& proto) {
  std::vector<IntervalVar*> vars;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kIntervalsArgument, proto, &vars));
  IntervalVar* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeCover(vars, target);
}

// ----- kCumulative -----

Constraint* BuildCumulative(CpModelLoader* const builder,
                            const CpConstraint& proto) {
  std::vector<IntervalVar*> vars;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kIntervalsArgument, proto, &vars));
  std::vector<int64> demands;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kDemandsArgument, proto, &demands));
  int64 capacity;
  VERIFY(builder->ScanArguments(ModelVisitor::kCapacityArgument, proto,
                                &capacity));
  std::string name;
  if (!proto.name().empty()) {
    name = proto.name();
  }
  return builder->solver()->MakeCumulative(vars, demands, capacity, name);
}

// ----- kDeviation -----

Constraint* BuildDeviation(CpModelLoader* const builder,
                           const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeDeviation(vars, target->Var(), value);
}

// ----- kDifference -----

IntExpr* BuildDifference(CpModelLoader* const builder,
                         const CpIntegerExpression& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeDifference(left, right);
  }
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeDifference(value, expr);
}

// ----- kDisjunctive -----

Constraint* BuildDisjunctive(CpModelLoader* const builder,
                             const CpConstraint& proto) {
  std::vector<IntervalVar*> vars;
  if (builder->ScanArguments(ModelVisitor::kIntervalsArgument, proto, &vars)) {
    return builder->solver()->MakeDisjunctiveConstraint(vars, proto.name());
  } else {
    std::vector<IntVar*> x;
    std::vector<IntVar*> dx;
    std::vector<IntVar*> y;
    std::vector<IntVar*> dy;
    VERIFY(builder->ScanArguments(ModelVisitor::kPositionXArgument, proto, &x));
    VERIFY(builder->ScanArguments(ModelVisitor::kPositionYArgument, proto, &y));
    VERIFY(builder->ScanArguments(ModelVisitor::kSizeXArgument, proto, &dx));
    VERIFY(builder->ScanArguments(ModelVisitor::kSizeYArgument, proto, &dy));
    return builder->solver()->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  }
}

// ----- kDistribute -----

Constraint* BuildDistribute(CpModelLoader* const builder,
                            const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  if (builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars)) {
    std::vector<IntVar*> cards;
    if (builder->ScanArguments(ModelVisitor::kCardsArgument, proto, &cards)) {
      std::vector<int64> values;
      if (builder->ScanArguments(ModelVisitor::kValuesArgument, proto,
                                 &values)) {
        return builder->solver()->MakeDistribute(vars, values, cards);
      } else {
        return builder->solver()->MakeDistribute(vars, cards);
      }
    } else {
      int64 card_min = 0;
      VERIFY(
          builder->ScanArguments(ModelVisitor::kMinArgument, proto, &card_min));
      int64 card_max = 0;
      VERIFY(
          builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &card_max));
      int64 card_size = 0;
      VERIFY(builder->ScanArguments(ModelVisitor::kSizeArgument, proto,
                                    &card_size));
      return builder->solver()->MakeDistribute(vars, card_min, card_max,
                                               card_size);
    }
  } else {
    std::vector<IntVar*> cards;
    VERIFY(builder->ScanArguments(ModelVisitor::kCardsArgument, proto, &cards));
    return builder->solver()->MakeDistribute(vars, cards);
  }
}

// ----- kDivide -----

IntExpr* BuildDivide(CpModelLoader* const builder,
                     const CpIntegerExpression& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeDiv(left, right);
  }
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeDiv(expr, value);
}

// ----- kDurationExpr -----

IntExpr* BuildDurationExpr(CpModelLoader* const builder,
                           const CpIntegerExpression& proto) {
  IntervalVar* var = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIntervalArgument, proto, &var));
  return var->DurationExpr();
}

// ----- kElement -----

IntExpr* BuildElement(CpModelLoader* const builder,
                      const CpIntegerExpression& proto) {
  IntExpr* index = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIndexArgument, proto, &index));
  std::vector<int64> values;
  IntExpr* index2 = nullptr;
  if (builder->ScanArguments(ModelVisitor::kIndex2Argument, proto, &index2)) {
    int64 index_min = 0;
    VERIFY(
        builder->ScanArguments(ModelVisitor::kMinArgument, proto, &index_min));
    int64 index_max = 0;
    VERIFY(
        builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &index_max));
    const int extension_tag_index =
        builder->TagIndex(ModelVisitor::kInt64ToInt64Extension);
    ArrayWithOffset<Solver::IndexEvaluator1>* const array =
        builder->solver()->RevAlloc(
            new ArrayWithOffset<Solver::IndexEvaluator1>(index_min, index_max));
    for (int i = index_min; i <= index_max; ++i) {
      array->SetValue(i, MakeFunctionFromProto<int64>(
                             builder, proto.extensions(i - index_min),
                             extension_tag_index));
    }
    return builder->solver()->MakeElement(
        [array](int64 i, int64 j) { return array->Evaluate(i)(j); },
        index->Var(), index2->Var());
  }
  if (proto.extensions_size() > 0) {
    VERIFY_EQ(1, proto.extensions_size());
    const int extension_tag_index =
        builder->TagIndex(ModelVisitor::kInt64ToInt64Extension);
    Solver::IndexEvaluator1 callback = MakeFunctionFromProto<int64>(
        builder, proto.extensions(0), extension_tag_index);
    return builder->solver()->MakeElement(callback, index->Var());
  }
  if (builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values)) {
    return builder->solver()->MakeElement(values, index->Var());
  }
  std::vector<IntVar*> vars;
  if (builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars)) {
    return builder->solver()->MakeElement(vars, index->Var());
  }
  return nullptr;
}

// ----- kElementEqual -----

Constraint* BuildElementEqual(CpModelLoader* const builder,
                              const CpConstraint& proto) {
  IntExpr* index = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIndexArgument, proto, &index));
  std::vector<int64> values;
  if (builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values)) {
    IntExpr* target = nullptr;
    VERIFY(
        builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
    return builder->solver()->MakeElementEquality(values, index->Var(),
                                                  target->Var());
  } else {
    std::vector<IntVar*> vars;
    if (builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars)) {
      IntExpr* target = nullptr;
      if (builder->ScanArguments(ModelVisitor::kTargetArgument, proto,
                                 &target)) {
        return builder->solver()->MakeElementEquality(vars, index->Var(),
                                                      target->Var());
      } else {
        int64 target_value = 0;
        VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto,
                                      &target_value));
        return builder->solver()->MakeElementEquality(vars, index->Var(),
                                                      target_value);
      }
    }
  }
  return nullptr;
}

// ----- kEndExpr -----

IntExpr* BuildEndExpr(CpModelLoader* const builder,
                      const CpIntegerExpression& proto) {
  IntervalVar* var = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIntervalArgument, proto, &var));
  return var->EndExpr();
}

// ----- kEquality -----

Constraint* BuildEquality(CpModelLoader* const builder,
                          const CpConstraint& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeEquality(left, right);
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeEquality(expr, value);
  }
  IntervalVar* vleft = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &vleft)) {
    IntervalVar* vright = nullptr;
    VERIFY(
        builder->ScanArguments(ModelVisitor::kRightArgument, proto, &vright));
    return builder->solver()->MakeEquality(vleft, vright);
  }
  return nullptr;
}

// ----- kFalseConstraint -----

Constraint* BuildFalseConstraint(CpModelLoader* const builder,
                                 const CpConstraint& proto) {
  return builder->solver()->MakeFalseConstraint();
}

// ----- kGreater -----

Constraint* BuildGreater(CpModelLoader* const builder,
                         const CpConstraint& proto) {
  IntExpr* left = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  IntExpr* right = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  return builder->solver()->MakeGreater(left, right);
}

// ----- kGreaterOrEqual -----

Constraint* BuildGreaterOrEqual(CpModelLoader* const builder,
                                const CpConstraint& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeGreaterOrEqual(left, right);
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeGreaterOrEqual(expr, value);
  }
  return nullptr;
}

// ----- kIndexOf -----

Constraint* BuildIndexOf(CpModelLoader* const builder,
                         const CpConstraint& proto) {
  IntExpr* index = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIndexArgument, proto, &index));
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  int64 target_value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto,
                                &target_value));
  return builder->solver()->MakeIndexOfConstraint(vars, index->Var(),
                                                  target_value);
}

// ----- kIntegerVariable -----

IntExpr* BuildIntegerVariable(CpModelLoader* const builder,
                              const CpIntegerExpression& proto) {
  IntExpr* sub_expression = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto,
                             &sub_expression)) {
    IntVar* const result = sub_expression->Var();
    if (!proto.name().empty()) {
      result->set_name(proto.name());
    }
    return result;
  }
  IntExpr* sub_var = nullptr;
  if (builder->ScanArguments(ModelVisitor::kVariableArgument, proto,
                             &sub_var)) {
    int64 value;
    IntExpr* result = nullptr;
    if (builder->ScanArguments(ModelVisitor::kSumOperation, proto, &value)) {
      result = builder->solver()->MakeSum(sub_var, value);
    } else if (builder->ScanArguments(ModelVisitor::kDifferenceOperation, proto,
                                      &value)) {
      result = builder->solver()->MakeDifference(value, sub_var);
    } else if (builder->ScanArguments(ModelVisitor::kProductOperation, proto,
                                      &value)) {
      result = builder->solver()->MakeProd(sub_var, value);
    }
    if (!proto.name().empty()) {
      result->set_name(proto.name());
    }
    return result;
  }
  int64 var_min = 0;
  if (builder->ScanArguments(ModelVisitor::kMinArgument, proto, &var_min)) {
    int64 var_max = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &var_max));
    return builder->solver()->MakeIntVar(var_min, var_max, proto.name());
  }
  std::vector<int64> values;
  if (builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values)) {
    return builder->solver()->MakeIntVar(values, proto.name());
  }
  return nullptr;
}

// ----- kIntervalBinaryRelation -----

Constraint* BuildIntervalBinaryRelation(CpModelLoader* const builder,
                                        const CpConstraint& proto) {
  IntervalVar* left = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  IntervalVar* right = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  int64 relation = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kRelationArgument, proto,
                                &relation));
  Solver::BinaryIntervalRelation rel =
      static_cast<Solver::BinaryIntervalRelation>(relation);
  return builder->solver()->MakeIntervalVarRelation(left, rel, right);
}

// ----- kIntervalDisjunction -----

Constraint* BuildIntervalDisjunction(CpModelLoader* const builder,
                                     const CpConstraint& proto) {
  IntervalVar* left = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  IntervalVar* right = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeTemporalDisjunction(left, right, target->Var());
}

// ----- kIntervalUnaryRelation -----

Constraint* BuildIntervalUnaryRelation(CpModelLoader* const builder,
                                       const CpConstraint& proto) {
  IntervalVar* interval = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIntervalArgument, proto,
                                &interval));
  int64 date = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &date));
  int64 relation = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kRelationArgument, proto,
                                &relation));
  Solver::UnaryIntervalRelation rel =
      static_cast<Solver::UnaryIntervalRelation>(relation);
  return builder->solver()->MakeIntervalVarRelation(interval, rel, date);
}

// ----- kIntervalVariable -----

IntervalVar* BuildIntervalVariable(CpModelLoader* const builder,
                                   const CpIntervalVariable& proto) {
  Solver* const solver = builder->solver();
  int64 start_min = 0;
  if (builder->ScanArguments(ModelVisitor::kStartMinArgument, proto,
                             &start_min)) {
    int64 start_max = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kStartMaxArgument, proto,
                                  &start_max));
    int64 end_min = 0;
    VERIFY(
        builder->ScanArguments(ModelVisitor::kEndMinArgument, proto, &end_min));
    int64 end_max = 0;
    VERIFY(
        builder->ScanArguments(ModelVisitor::kEndMaxArgument, proto, &end_max));
    int64 duration_min = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kDurationMinArgument, proto,
                                  &duration_min));
    int64 duration_max = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kDurationMaxArgument, proto,
                                  &duration_max));
    int64 optional = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kOptionalArgument, proto,
                                  &optional));
    VERIFY_EQ(duration_max, duration_min);
    VERIFY_EQ(end_max - duration_max, start_max);
    VERIFY_EQ(end_min - duration_min, start_min);
    const std::string name = proto.name();
    if (start_min == start_max) {
      return solver->MakeFixedInterval(start_min, duration_min, name);
    } else {
      return solver->MakeFixedDurationIntervalVar(start_min, start_max,
                                                  duration_min, optional, name);
    }
  } else {
    VERIFY_EQ(1, proto.arguments_size());
    const CpArgument& sub_proto = proto.arguments(0);
    IntervalVar* const derived =
        builder->IntervalVariable(sub_proto.interval_index());
    const int operation_index = sub_proto.argument_index();
    DCHECK_NE(-1, operation_index);
    if (operation_index == builder->TagIndex(ModelVisitor::kMirrorOperation)) {
      return solver->MakeMirrorInterval(derived);
    } else if (operation_index ==
               builder->TagIndex(ModelVisitor::kRelaxedMaxOperation)) {
      return solver->MakeIntervalRelaxedMax(derived);
    } else if (operation_index ==
               builder->TagIndex(ModelVisitor::kRelaxedMinOperation)) {
      return solver->MakeIntervalRelaxedMin(derived);
    } else if (operation_index ==
               builder->TagIndex(ModelVisitor::kStartSyncOnStartOperation)) {
      const int64 delay = sub_proto.integer_value();
      VERIFY_EQ(1, sub_proto.integer_array_size());
      const int64 duration = sub_proto.integer_array(0);
      return solver->MakeFixedDurationStartSyncedOnStartIntervalVar(
          derived, duration, delay);
    } else if (operation_index ==
               builder->TagIndex(ModelVisitor::kStartSyncOnEndOperation)) {
      const int64 delay = sub_proto.integer_value();
      VERIFY_EQ(1, sub_proto.integer_array_size());
      const int64 duration = sub_proto.integer_array(0);
      return solver->MakeFixedDurationStartSyncedOnEndIntervalVar(
          derived, duration, delay);
    }
  }
  return nullptr;
}

// ----- kInversePermutation -----

Constraint* BuildInversePermutation(CpModelLoader* const builder,
                                    const CpConstraint& proto) {
  std::vector<IntVar*> left;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  std::vector<IntVar*> right;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  return builder->solver()->MakeInversePermutationConstraint(left, right);
}

// ----- kIsBetween -----

Constraint* BuildIsBetween(CpModelLoader* const builder,
                           const CpConstraint& proto) {
  int64 value_min = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kMinArgument, proto, &value_min));
  int64 value_max = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &value_max));
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeIsBetweenCt(expr->Var(), value_min, value_max,
                                            target->Var());
}

// ----- kIsDifferent -----

Constraint* BuildIsDifferent(CpModelLoader* const builder,
                             const CpConstraint& proto) {
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeIsDifferentCt(left, right, target->Var());
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeIsDifferentCstCt(expr, value, target->Var());
  }
  return nullptr;
}

// ----- kIsEqual -----

Constraint* BuildIsEqual(CpModelLoader* const builder,
                         const CpConstraint& proto) {
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeIsEqualCt(left, right, target->Var());
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeIsEqualCstCt(expr, value, target->Var());
  }
  return nullptr;
}

// ----- kIsGreater -----

Constraint* BuildIsGreater(CpModelLoader* const builder,
                           const CpConstraint& proto) {
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  IntExpr* left = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  IntExpr* right = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  return builder->solver()->MakeIsGreaterCt(left, right, target->Var());
}

// ----- kIsGreaterOrEqual -----

Constraint* BuildIsGreaterOrEqual(CpModelLoader* const builder,
                                  const CpConstraint& proto) {
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeIsGreaterOrEqualCt(left, right,
                                                     target->Var());
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeIsGreaterOrEqualCstCt(expr, value,
                                                        target->Var());
  }
  return nullptr;
}

// ----- kIsLess -----

Constraint* BuildIsLess(CpModelLoader* const builder,
                        const CpConstraint& proto) {
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  IntExpr* left = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  IntExpr* right = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  return builder->solver()->MakeIsLessCt(left, right, target->Var());
}

// ----- kIsLessOrEqual -----

Constraint* BuildIsLessOrEqual(CpModelLoader* const builder,
                               const CpConstraint& proto) {
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeIsLessOrEqualCt(left, right, target->Var());
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeIsLessOrEqualCstCt(expr, value,
                                                     target->Var());
  }
  return nullptr;
}

// ----- kIsMember -----

Constraint* BuildIsMember(CpModelLoader* const builder,
                          const CpConstraint& proto) {
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values));
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeIsMemberCt(expr->Var(), values, target->Var());
}

// ----- kLess -----

Constraint* BuildLess(CpModelLoader* const builder, const CpConstraint& proto) {
  IntExpr* left = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  IntExpr* right = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  return builder->solver()->MakeLess(left, right);
}

// ----- kLessOrEqual -----

Constraint* BuildLessOrEqual(CpModelLoader* const builder,
                             const CpConstraint& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeLessOrEqual(left, right);
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeLessOrEqual(expr, value);
  }
  return nullptr;
}

// ----- kLexLess -----

Constraint* BuildLexLess(CpModelLoader* const builder,
                         const CpConstraint& proto) {
  std::vector<IntVar*> left;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  std::vector<IntVar*> right;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return value == 1 ? builder->solver()->MakeLexicalLess(left, right)
                    : builder->solver()->MakeLexicalLessOrEqual(left, right);
}

// ----- kMapDomain -----

Constraint* BuildMapDomain(CpModelLoader* const builder,
                           const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeMapDomain(target->Var(), vars);
}

// ----- kMax -----

IntExpr* BuildMax(CpModelLoader* const builder,
                  const CpIntegerExpression& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeMax(left, right);
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeMax(expr, value);
  }
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  return builder->solver()->MakeMax(vars);
}

// ----- kMaxEqual -----

Constraint* BuildMaxEqual(CpModelLoader* const builder,
                          const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeMaxEquality(vars, target->Var());
}

// ----- kMember -----

Constraint* BuildMember(CpModelLoader* const builder,
                        const CpConstraint& proto) {
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values));
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  return builder->solver()->MakeMemberCt(expr->Var(), values);
}

// ----- kMin -----

IntExpr* BuildMin(CpModelLoader* const builder,
                  const CpIntegerExpression& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeMin(left, right);
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeMin(expr, value);
  }
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  return builder->solver()->MakeMin(vars);
}

// ----- kMinEqual -----

Constraint* BuildMinEqual(CpModelLoader* const builder,
                          const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeMinEquality(vars, target->Var());
}

// ----- kNoCycle -----

Constraint* BuildNoCycle(CpModelLoader* const builder,
                         const CpConstraint& proto) {
  std::vector<IntVar*> nexts;
  VERIFY(builder->ScanArguments(ModelVisitor::kNextsArgument, proto, &nexts));
  std::vector<IntVar*> active;
  VERIFY(builder->ScanArguments(ModelVisitor::kActiveArgument, proto, &active));
  int64 assume_paths = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kAssumePathsArgument, proto,
                                &assume_paths));
  Solver::IndexFilter1 sink_handler = nullptr;
  if (proto.extensions_size() > 0) {
    VERIFY_EQ(1, proto.extensions_size());
    const int tag_index =
        builder->TagIndex(ModelVisitor::kInt64ToBoolExtension);
    sink_handler =
        MakeFunctionFromProto<bool>(builder, proto.extensions(0), tag_index);
  }
  return builder->solver()->MakeNoCycle(nexts, active, sink_handler,
                                        assume_paths);
}

// ----- kNonEqual -----

Constraint* BuildNonEqual(CpModelLoader* const builder,
                          const CpConstraint& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeNonEquality(left, right);
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeNonEquality(expr, value);
  }
  return nullptr;
}

// ----- kNotBetween -----

Constraint* BuildNotBetween(CpModelLoader* const builder,
                            const CpConstraint& proto) {
  int64 value_min = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kMinArgument, proto, &value_min));
  int64 value_max = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &value_max));
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  return builder->solver()->MakeNotBetweenCt(expr->Var(), value_min, value_max);
}

// ----- kNotMember -----

Constraint* BuildNotMember(CpModelLoader* const builder,
                           const CpConstraint& proto) {
  std::vector<int64> starts;
  VERIFY(builder->ScanArguments(ModelVisitor::kStartsArgument, proto, &starts));
  std::vector<int64> ends;
  VERIFY(builder->ScanArguments(ModelVisitor::kEndsArgument, proto, &ends));
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  return builder->solver()->MakeNotMemberCt(expr, starts, ends);
}

// ----- kNullIntersect -----

Constraint* BuildNullIntersect(CpModelLoader* const builder,
                               const CpConstraint& proto) {
  std::vector<IntVar*> left;
  VERIFY(builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left));
  std::vector<IntVar*> right;
  VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
  int64 escape = 0;
  if (builder->ScanArguments(ModelVisitor::kValueArgument, proto, &escape)) {
    return builder->solver()->MakeNullIntersectExcept(left, right, escape);
  } else {
    return builder->solver()->MakeNullIntersect(left, right);
  }
}

// ----- kOpposite -----

IntExpr* BuildOpposite(CpModelLoader* const builder,
                       const CpIntegerExpression& proto) {
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  return builder->solver()->MakeOpposite(expr);
}

// ----- kPack -----

bool AddUsageLessConstantDimension(Pack* const pack,
                                   CpModelLoader* const builder,
                                   const CpExtension& proto) {
  std::vector<int64> weights;
  VERIFY_BOOL(builder->ScanArguments(ModelVisitor::kCoefficientsArgument, proto,
                                     &weights));
  std::vector<int64> upper;
  VERIFY_BOOL(
      builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &upper));
  pack->AddWeightedSumLessOrEqualConstantDimension(weights, upper);
  return true;
}

bool AddCountAssignedItemsDimension(Pack* const pack,
                                    CpModelLoader* const builder,
                                    const CpExtension& proto) {
  IntExpr* target = nullptr;
  VERIFY_BOOL(
      builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  pack->AddCountAssignedItemsDimension(target->Var());
  return true;
}

bool AddCountUsedBinDimension(Pack* const pack, CpModelLoader* const builder,
                              const CpExtension& proto) {
  IntExpr* target = nullptr;
  VERIFY_BOOL(
      builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  pack->AddCountUsedBinDimension(target->Var());
  return true;
}

bool AddUsageEqualVariableDimension(Pack* const pack,
                                    CpModelLoader* const builder,
                                    const CpExtension& proto) {
  std::vector<int64> weights;
  VERIFY_BOOL(builder->ScanArguments(ModelVisitor::kCoefficientsArgument, proto,
                                     &weights));
  std::vector<IntVar*> loads;
  VERIFY_BOOL(
      builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &loads));
  pack->AddWeightedSumEqualVarDimension(weights, loads);
  return true;
}

bool AddVariableUsageLessConstantDimension(Pack* const pack,
                                           CpModelLoader* const builder,
                                           const CpExtension& proto) {
  std::vector<int64> uppers;
  VERIFY_BOOL(
      builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &uppers));
  std::vector<IntVar*> usages;
  VERIFY_BOOL(
      builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &usages));
  pack->AddSumVariableWeightsLessOrEqualConstantDimension(usages, uppers);
  return true;
}

bool AddWeightedSumOfAssignedDimension(Pack* const pack,
                                       CpModelLoader* const builder,
                                       const CpExtension& proto) {
  std::vector<int64> weights;
  VERIFY_BOOL(builder->ScanArguments(ModelVisitor::kCoefficientsArgument, proto,
                                     &weights));
  IntExpr* target = nullptr;
  VERIFY_BOOL(
      builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  pack->AddWeightedSumOfAssignedDimension(weights, target->Var());
  return true;
}

#define IS_TYPE(index, builder, tag) \
  index == builder->TagIndex(ModelVisitor::tag)

Constraint* BuildPack(CpModelLoader* const builder, const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  int64 bins = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kSizeArgument, proto, &bins));
  Pack* const pack = builder->solver()->MakePack(vars, bins);
  // Add dimensions. They are stored as extensions in the proto.
  for (int i = 0; i < proto.extensions_size(); ++i) {
    const CpExtension& dimension_proto = proto.extensions(i);
    const int type_index = dimension_proto.type_index();
    if (IS_TYPE(type_index, builder, kUsageLessConstantExtension)) {
      VERIFY(AddUsageLessConstantDimension(pack, builder, dimension_proto));
    } else if (IS_TYPE(type_index, builder, kCountAssignedItemsExtension)) {
      VERIFY(AddCountAssignedItemsDimension(pack, builder, dimension_proto));
    } else if (IS_TYPE(type_index, builder, kCountUsedBinsExtension)) {
      VERIFY(AddCountUsedBinDimension(pack, builder, dimension_proto));
    } else if (IS_TYPE(type_index, builder, kUsageEqualVariableExtension)) {
      VERIFY(AddUsageEqualVariableDimension(pack, builder, dimension_proto));
    } else if (IS_TYPE(type_index, builder,
                       kVariableUsageLessConstantExtension)) {
      VERIFY(AddVariableUsageLessConstantDimension(pack, builder,
                                                   dimension_proto));
    } else if (IS_TYPE(type_index, builder,
                       kWeightedSumOfAssignedEqualVariableExtension)) {
      VERIFY(AddWeightedSumOfAssignedDimension(pack, builder, dimension_proto));
    } else {
      LOG(ERROR) << "Unrecognized extension " << dimension_proto.DebugString();
      return nullptr;
    }
  }
  return pack;
}
#undef IS_TYPE

// ----- kPathCumul -----

Constraint* BuildPathCumul(CpModelLoader* const builder,
                           const CpConstraint& proto) {
  std::vector<IntVar*> nexts;
  VERIFY(builder->ScanArguments(ModelVisitor::kNextsArgument, proto, &nexts));
  std::vector<IntVar*> active;
  VERIFY(builder->ScanArguments(ModelVisitor::kActiveArgument, proto, &active));
  std::vector<IntVar*> cumuls;
  VERIFY(builder->ScanArguments(ModelVisitor::kCumulsArgument, proto, &cumuls));
  std::vector<IntVar*> transits;
  VERIFY(builder->ScanArguments(ModelVisitor::kTransitsArgument, proto,
                                &transits));
  return builder->solver()->MakePathCumul(nexts, active, cumuls, transits);
}

// ----- kDelayedPathCumul -----

Constraint* BuildDelayedPathCumul(CpModelLoader* const builder,
                                  const CpConstraint& proto) {
  std::vector<IntVar*> nexts;
  VERIFY(builder->ScanArguments(ModelVisitor::kNextsArgument, proto, &nexts));
  std::vector<IntVar*> active;
  VERIFY(builder->ScanArguments(ModelVisitor::kActiveArgument, proto, &active));
  std::vector<IntVar*> cumuls;
  VERIFY(builder->ScanArguments(ModelVisitor::kCumulsArgument, proto, &cumuls));
  std::vector<IntVar*> transits;
  VERIFY(builder->ScanArguments(ModelVisitor::kTransitsArgument, proto,
                                &transits));
  return builder->solver()->MakeDelayedPathCumul(nexts, active, cumuls,
                                                 transits);
}

// ----- kPerformedExpr -----

IntExpr* BuildPerformedExpr(CpModelLoader* const builder,
                            const CpIntegerExpression& proto) {
  IntervalVar* var = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIntervalArgument, proto, &var));
  return var->PerformedExpr();
}

// ----- kPower -----

IntExpr* BuildPower(CpModelLoader* const builder,
                    const CpIntegerExpression& proto) {
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakePower(expr, value);
}

// ----- kProduct -----

IntExpr* BuildProduct(CpModelLoader* const builder,
                      const CpIntegerExpression& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeProd(left, right);
  }
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeProd(expr, value);
}

// ----- kScalProd -----

IntExpr* BuildScalProd(CpModelLoader* const builder,
                       const CpIntegerExpression& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kCoefficientsArgument, proto,
                                &values));
  return builder->solver()->MakeScalProd(vars, values);
}

// ----- kScalProdEqual -----

Constraint* BuildScalProdEqual(CpModelLoader* const builder,
                               const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kCoefficientsArgument, proto,
                                &values));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeScalProdEquality(vars, values, value);
}

// ----- kScalProdGreaterOrEqual -----

Constraint* BuildScalProdGreaterOrEqual(CpModelLoader* const builder,
                                        const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kCoefficientsArgument, proto,
                                &values));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeScalProdGreaterOrEqual(vars, values, value);
}

// ----- kScalProdLessOrEqual -----

Constraint* BuildScalProdLessOrEqual(CpModelLoader* const builder,
                                     const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kCoefficientsArgument, proto,
                                &values));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeScalProdLessOrEqual(vars, values, value);
}

// ----- kSemiContinuous -----

IntExpr* BuildSemiContinuous(CpModelLoader* const builder,
                             const CpIntegerExpression& proto) {
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  int64 fixed_charge = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kFixedChargeArgument, proto,
                                &fixed_charge));
  int64 step = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kStepArgument, proto, &step));
  return builder->solver()->MakeSemiContinuousExpr(expr, fixed_charge, step);
}

// ----- kSortingConstraint -----

Constraint* BuildSortingConstraint(CpModelLoader* const builder,
                                   const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  std::vector<IntVar*> targets;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &targets));
  return builder->solver()->MakeSortingConstraint(vars, targets);
}

// ----- kSquare -----

IntExpr* BuildSquare(CpModelLoader* const builder,
                     const CpIntegerExpression& proto) {
  IntExpr* expr = nullptr;
  VERIFY(
      builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr));
  return builder->solver()->MakeSquare(expr);
}

// ----- kStartExpr -----

IntExpr* BuildStartExpr(CpModelLoader* const builder,
                        const CpIntegerExpression& proto) {
  IntervalVar* var = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kIntervalArgument, proto, &var));
  return var->StartExpr();
}

// ----- kSum -----

IntExpr* BuildSum(CpModelLoader* const builder,
                  const CpIntegerExpression& proto) {
  IntExpr* left = nullptr;
  if (builder->ScanArguments(ModelVisitor::kLeftArgument, proto, &left)) {
    IntExpr* right = nullptr;
    VERIFY(builder->ScanArguments(ModelVisitor::kRightArgument, proto, &right));
    return builder->solver()->MakeSum(left, right);
  }
  IntExpr* expr = nullptr;
  if (builder->ScanArguments(ModelVisitor::kExpressionArgument, proto, &expr)) {
    int64 value = 0;
    VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
    return builder->solver()->MakeSum(expr, value);
  }
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  return builder->solver()->MakeSum(vars);
}

// ----- kSumEqual -----

Constraint* BuildSumEqual(CpModelLoader* const builder,
                          const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  int64 value = 0;
  if (builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value)) {
    return builder->solver()->MakeSumEquality(vars, value);
  }
  IntExpr* target = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target));
  return builder->solver()->MakeSumEquality(vars, target->Var());
}

// ----- kSumGreaterOrEqual -----

Constraint* BuildSumGreaterOrEqual(CpModelLoader* const builder,
                                   const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeSumGreaterOrEqual(vars, value);
}

// ----- kSumLessOrEqual -----

Constraint* BuildSumLessOrEqual(CpModelLoader* const builder,
                                const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  int64 value = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kValueArgument, proto, &value));
  return builder->solver()->MakeSumLessOrEqual(vars, value);
}

// ----- kTransition -----

Constraint* BuildTransition(CpModelLoader* const builder,
                            const CpConstraint& proto) {
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  IntTupleSet tuples(3);
  VERIFY(builder->ScanArguments(ModelVisitor::kTuplesArgument, proto, &tuples));
  int64 initial_state = 0;
  VERIFY(builder->ScanArguments(ModelVisitor::kInitialState, proto,
                                &initial_state));
  std::vector<int64> final_states;
  VERIFY(builder->ScanArguments(ModelVisitor::kFinalStatesArgument, proto,
                                &final_states));

  return builder->solver()->MakeTransitionConstraint(
      vars, tuples, initial_state, final_states);
}

// ----- kTrueConstraint -----

Constraint* BuildTrueConstraint(CpModelLoader* const builder,
                                const CpConstraint& proto) {
  return builder->solver()->MakeTrueConstraint();
}

// ----- kVarValueWatcher -----

Constraint* BuildVarValueWatcher(CpModelLoader* const builder,
                                 const CpConstraint& proto) {
  IntExpr* expr = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kVariableArgument, proto, &expr));
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values));
  return SetIsEqual(expr->Var(), values, vars);
}

// ----- kVarBoundWatcher -----

Constraint* BuildVarBoundWatcher(CpModelLoader* const builder,
                                 const CpConstraint& proto) {
  IntExpr* expr = nullptr;
  VERIFY(builder->ScanArguments(ModelVisitor::kVariableArgument, proto, &expr));
  std::vector<IntVar*> vars;
  VERIFY(builder->ScanArguments(ModelVisitor::kVarsArgument, proto, &vars));
  std::vector<int64> values;
  VERIFY(builder->ScanArguments(ModelVisitor::kValuesArgument, proto, &values));
  return SetIsGreaterOrEqual(expr->Var(), values, vars);
}

#undef VERIFY
#undef VERIFY_EQ
}  // namespace

// ----- CpModelLoader -----

bool CpModelLoader::BuildFromProto(const CpIntegerExpression& proto) {
  const int index = proto.index();
  const int tag_index = proto.type_index();
  Solver::IntegerExpressionBuilder builder =
      solver_->GetIntegerExpressionBuilder(tags_.Element(tag_index));
  if (!builder) {
    LOG(WARNING) << "Tag " << tags_.Element(tag_index) << " was not found";
    return false;
  }
  IntExpr* const built = builder(this, proto);
  if (!built) {
    return false;
  }
  expressions_.resize(
      std::max(static_cast<int>(expressions_.size()), index + 1));
  expressions_[index] = built;
  return true;
}

Constraint* CpModelLoader::BuildFromProto(const CpConstraint& proto) {
  const int tag_index = proto.type_index();
  Solver::ConstraintBuilder builder =
      solver_->GetConstraintBuilder(tags_.Element(tag_index));
  if (!builder) {
    LOG(WARNING) << "Tag " << tags_.Element(tag_index) << " was not found";
    return nullptr;
  }
  Constraint* const built = builder(this, proto);
  return built;
}

bool CpModelLoader::BuildFromProto(const CpIntervalVariable& proto) {
  const int index = proto.index();
  const int tag_index = proto.type_index();
  Solver::IntervalVariableBuilder builder =
      solver_->GetIntervalVariableBuilder(tags_.Element(tag_index));
  if (!builder) {
    LOG(WARNING) << "Tag " << tags_.Element(tag_index) << " was not found";
    return false;
  }
  IntervalVar* const built = builder(this, proto);
  if (!built) {
    return false;
  }
  intervals_.resize(std::max(static_cast<int>(intervals_.size()), index + 1));
  intervals_[index] = built;
  return true;
}

bool CpModelLoader::BuildFromProto(const CpSequenceVariable& proto) {
  const int index = proto.index();
  const int tag_index = proto.type_index();
  Solver::SequenceVariableBuilder builder =
      solver_->GetSequenceVariableBuilder(tags_.Element(tag_index));
  if (!builder) {
    LOG(WARNING) << "Tag " << tags_.Element(tag_index) << " was not found";
    return false;
  }
  SequenceVar* const built = builder(this, proto);
  if (!built) {
    return false;
  }
  sequences_.resize(std::max(static_cast<int>(sequences_.size()), index + 1));
  sequences_[index] = built;
  return true;
}

IntExpr* CpModelLoader::IntegerExpression(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, expressions_.size());
  CHECK(expressions_[index] != nullptr);
  return expressions_[index];
}

IntervalVar* CpModelLoader::IntervalVariable(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, intervals_.size());
  CHECK(intervals_[index] != nullptr);
  return intervals_[index];
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    int64* to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::INTEGER_VALUE) {
    *to_fill = arg_proto.integer_value();
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    IntExpr** to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::EXPRESSION) {
    const int expression_index = arg_proto.integer_expression_index();
    CHECK(expressions_[expression_index] != nullptr);
    *to_fill = expressions_[expression_index];
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    std::vector<int64>* to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::INTEGER_ARRAY) {
    const int values_size = arg_proto.integer_array_size();
    for (int j = 0; j < values_size; ++j) {
      to_fill->push_back(arg_proto.integer_array(j));
    }
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    IntTupleSet* to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.has_integer_matrix()) {
    to_fill->Clear();
    const CpIntegerMatrix& matrix = arg_proto.integer_matrix();
    const int rows = matrix.rows();
    const int columns = matrix.columns();
    int counter = 0;
    for (int i = 0; i < rows; ++i) {
      std::vector<int64> tuple;
      for (int j = 0; j < columns; ++j) {
        const int64 value = matrix.values(counter++);
        tuple.push_back(value);
      }
      to_fill->Insert(tuple);
    }
    CHECK_EQ(matrix.values_size(), counter);
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    std::vector<IntVar*>* to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::EXPRESSION_ARRAY) {
    const int vars_size = arg_proto.integer_expression_array_size();
    for (int j = 0; j < vars_size; ++j) {
      const int expression_index = arg_proto.integer_expression_array(j);
      CHECK(expressions_[expression_index] != nullptr);
      to_fill->push_back(expressions_[expression_index]->Var());
    }
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    IntervalVar** to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::INTERVAL) {
    const int interval_index = arg_proto.interval_index();
    CHECK(intervals_[interval_index] != nullptr);
    *to_fill = intervals_[interval_index];
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    std::vector<IntervalVar*>* to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::INTERVAL_ARRAY) {
    const int vars_size = arg_proto.interval_array_size();
    for (int j = 0; j < vars_size; ++j) {
      const int interval_index = arg_proto.interval_array(j);
      CHECK(intervals_[interval_index] != nullptr);
      to_fill->push_back(intervals_[interval_index]);
    }
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    SequenceVar** to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::SEQUENCE) {
    const int sequence_index = arg_proto.sequence_index();
    CHECK(sequences_[sequence_index] != nullptr);
    *to_fill = sequences_[sequence_index];
    return true;
  }
  return false;
}

bool CpModelLoader::ScanOneArgument(int type_index, const CpArgument& arg_proto,
                                    std::vector<SequenceVar*>* to_fill) {
  if (arg_proto.argument_index() == type_index &&
      arg_proto.type() == CpArgument::SEQUENCE_ARRAY) {
    const int vars_size = arg_proto.sequence_array_size();
    for (int j = 0; j < vars_size; ++j) {
      const int sequence_index = arg_proto.sequence_array(j);
      CHECK(sequences_[sequence_index] != nullptr);
      to_fill->push_back(sequences_[sequence_index]);
    }
    return true;
  }
  return false;
}

// ----- Solver API -----

CpModel Solver::ExportModelWithSearchMonitorsAndDecisionBuilder(
    const std::vector<SearchMonitor*>& monitors,
    DecisionBuilder* const db) const {
  CpModel model_proto;
  FirstPassVisitor first_pass;
  Accept(&first_pass, monitors, db);
  SecondPassVisitor second_pass(first_pass, &model_proto);
  Accept(&second_pass, monitors, db);
  return model_proto;
}

CpModel Solver::ExportModelWithSearchMonitors(
    const std::vector<SearchMonitor*>& monitors) const {
  return ExportModelWithSearchMonitorsAndDecisionBuilder(monitors, nullptr);
}

CpModel Solver::ExportModel() const {
  std::vector<SearchMonitor*> monitors;
  return ExportModelWithSearchMonitorsAndDecisionBuilder(monitors, nullptr);
}

bool Solver::LoadModel(const CpModel& model_proto) {
  return LoadModelWithSearchMonitors(model_proto, nullptr);
}

bool Solver::LoadModelWithSearchMonitors(
    const CpModel& model_proto, std::vector<SearchMonitor*>* monitors) {
  if (model_proto.version() > kModelVersion) {
    LOG(ERROR) << "Model protocol buffer version is greater than"
               << " the one compiled in the reader (" << model_proto.version()
               << " vs " << kModelVersion << ")";
    return false;
  }
  CHECK(model_loader_.get() == nullptr) << "You can only load a model once";
  model_loader_.reset(new CpModelLoader(this));
  for (int i = 0; i < model_proto.tags_size(); ++i) {
    model_loader_->AddTag(model_proto.tags(i));
  }
  for (int i = 0; i < model_proto.intervals_size(); ++i) {
    if (!model_loader_->BuildFromProto(model_proto.intervals(i))) {
      LOG(ERROR) << "Interval variable proto "
                 << model_proto.intervals(i).DebugString()
                 << " was not parsed correctly";
      return false;
    }
  }
  for (int i = 0; i < model_proto.sequences_size(); ++i) {
    if (!model_loader_->BuildFromProto(model_proto.sequences(i))) {
      LOG(ERROR) << "Sequence variable proto "
                 << model_proto.sequences(i).DebugString()
                 << " was not parsed correctly";
      return false;
    }
  }
  for (int i = 0; i < model_proto.expressions_size(); ++i) {
    if (!model_loader_->BuildFromProto(model_proto.expressions(i))) {
      LOG(ERROR) << "Integer expression proto "
                 << model_proto.expressions(i).DebugString()
                 << " was not parsed correctly";
      return false;
    }
  }
  for (int i = 0; i < model_proto.constraints_size(); ++i) {
    Constraint* const constraint =
        model_loader_->BuildFromProto(model_proto.constraints(i));
    if (constraint == nullptr) {
      LOG(ERROR) << "Constraint proto "
                 << model_proto.constraints(i).DebugString()
                 << " was not parsed correctly";
      return false;
    }
    AddConstraint(constraint);
  }
  if (monitors != nullptr) {
    if (model_proto.has_search_limit()) {
      monitors->push_back(MakeLimit(model_proto.search_limit()));
    }
    if (model_proto.has_objective()) {
      const CpObjective& objective_proto = model_proto.objective();
      IntVar* const objective_var = model_loader_->IntegerExpression(
          objective_proto.objective_index())->Var();
      const bool maximize = objective_proto.maximize();
      const int64 step = objective_proto.step();
      OptimizeVar* const objective =
          MakeOptimize(maximize, objective_var, step);
      monitors->push_back(objective);
    }
  }
  return true;
}

bool Solver::UpgradeModel(CpModel* const proto) {
  if (proto->version() == kModelVersion) {
    LOG(INFO) << "Model already up to date with version " << kModelVersion;
  }
  return true;
}

void Solver::RegisterBuilder(const std::string& tag, ConstraintBuilder builder) {
  InsertOrDie(&constraint_builders_, tag, builder);
}

void Solver::RegisterBuilder(const std::string& tag,
                             IntegerExpressionBuilder builder) {
  InsertOrDie(&expression_builders_, tag, builder);
}

void Solver::RegisterBuilder(const std::string& tag,
                             IntervalVariableBuilder builder) {
  InsertOrDie(&interval_builders_, tag, builder);
}

void Solver::RegisterBuilder(const std::string& tag,
                             SequenceVariableBuilder builder) {
  InsertOrDie(&sequence_builders_, tag, builder);
}

Solver::ConstraintBuilder Solver::GetConstraintBuilder(
    const std::string& tag) const {
  return FindWithDefault(constraint_builders_, tag, nullptr);
}

Solver::IntegerExpressionBuilder Solver::GetIntegerExpressionBuilder(
    const std::string& tag) const {
  return FindWithDefault(expression_builders_, tag, nullptr);
}

Solver::IntervalVariableBuilder Solver::GetIntervalVariableBuilder(
    const std::string& tag) const {
  IntervalVariableBuilder builder =
      FindWithDefault(interval_builders_, tag, nullptr);
  return builder;
}

Solver::SequenceVariableBuilder Solver::GetSequenceVariableBuilder(
    const std::string& tag) const {
  SequenceVariableBuilder builder =
      FindWithDefault(sequence_builders_, tag, nullptr);
  return builder;
}

// ----- Manage builders -----

#define REGISTER(tag, func) RegisterBuilder(ModelVisitor::tag, func)

void Solver::InitBuilders() {
  // Explicit casting required by MSV compiler.
  REGISTER(kAbs, IntegerExpressionBuilder(BuildAbs));
  REGISTER(kAbsEqual, ConstraintBuilder(BuildAbsEqual));
  REGISTER(kAllDifferent, ConstraintBuilder(BuildAllDifferent));
  REGISTER(kAllowedAssignments, ConstraintBuilder(BuildAllowedAssignments));
  REGISTER(kBetween, ConstraintBuilder(BuildBetween));
  REGISTER(kConditionalExpr, IntegerExpressionBuilder(BuildConditionalExpr));
  REGISTER(kCircuit, ConstraintBuilder(BuildCircuit));
  REGISTER(kConvexPiecewise, IntegerExpressionBuilder(BuildConvexPiecewise));
  REGISTER(kCountEqual, ConstraintBuilder(BuildCountEqual));
  REGISTER(kCover, ConstraintBuilder(BuildCover));
  REGISTER(kCumulative, ConstraintBuilder(BuildCumulative));
  REGISTER(kDeviation, ConstraintBuilder(BuildDeviation));
  REGISTER(kDifference, IntegerExpressionBuilder(BuildDifference));
  REGISTER(kDisjunctive, ConstraintBuilder(BuildDisjunctive));
  REGISTER(kDistribute, ConstraintBuilder(BuildDistribute));
  REGISTER(kDivide, IntegerExpressionBuilder(BuildDivide));
  REGISTER(kDurationExpr, IntegerExpressionBuilder(BuildDurationExpr));
  REGISTER(kElement, IntegerExpressionBuilder(BuildElement));
  REGISTER(kElementEqual, ConstraintBuilder(BuildElementEqual));
  REGISTER(kEndExpr, IntegerExpressionBuilder(BuildEndExpr));
  REGISTER(kEquality, ConstraintBuilder(BuildEquality));
  REGISTER(kFalseConstraint, ConstraintBuilder(BuildFalseConstraint));
  REGISTER(kGreater, ConstraintBuilder(BuildGreater));
  REGISTER(kGreaterOrEqual, ConstraintBuilder(BuildGreaterOrEqual));
  REGISTER(kIndexOf, ConstraintBuilder(BuildIndexOf));
  REGISTER(kIntegerVariable, IntegerExpressionBuilder(BuildIntegerVariable));
  REGISTER(kIntervalBinaryRelation,
           ConstraintBuilder(BuildIntervalBinaryRelation));
  REGISTER(kIntervalDisjunction, ConstraintBuilder(BuildIntervalDisjunction));
  REGISTER(kIntervalUnaryRelation,
           ConstraintBuilder(BuildIntervalUnaryRelation));
  REGISTER(kIntervalVariable, IntervalVariableBuilder(BuildIntervalVariable));
  REGISTER(kInversePermutation, ConstraintBuilder(BuildInversePermutation));
  REGISTER(kIsBetween, ConstraintBuilder(BuildIsBetween));
  REGISTER(kIsDifferent, ConstraintBuilder(BuildIsDifferent));
  REGISTER(kIsEqual, ConstraintBuilder(BuildIsEqual));
  REGISTER(kIsGreater, ConstraintBuilder(BuildIsGreater));
  REGISTER(kIsGreaterOrEqual, ConstraintBuilder(BuildIsGreaterOrEqual));
  REGISTER(kIsLess, ConstraintBuilder(BuildIsLess));
  REGISTER(kIsLessOrEqual, ConstraintBuilder(BuildIsLessOrEqual));
  REGISTER(kIsMember, ConstraintBuilder(BuildIsMember));
  REGISTER(kLess, ConstraintBuilder(BuildLess));
  REGISTER(kLessOrEqual, ConstraintBuilder(BuildLessOrEqual));
  REGISTER(kLexLess, ConstraintBuilder(BuildLexLess));
  REGISTER(kMapDomain, ConstraintBuilder(BuildMapDomain));
  REGISTER(kMax, IntegerExpressionBuilder(BuildMax));
  REGISTER(kMaxEqual, ConstraintBuilder(BuildMaxEqual));
  REGISTER(kMember, ConstraintBuilder(BuildMember));
  REGISTER(kMin, IntegerExpressionBuilder(BuildMin));
  REGISTER(kMinEqual, ConstraintBuilder(BuildMinEqual));
  REGISTER(kNoCycle, ConstraintBuilder(BuildNoCycle));
  REGISTER(kNonEqual, ConstraintBuilder(BuildNonEqual));
  REGISTER(kNotBetween, ConstraintBuilder(BuildNotBetween));
  REGISTER(kNotMember, ConstraintBuilder(BuildNotMember));
  REGISTER(kNullIntersect, ConstraintBuilder(BuildNullIntersect));
  REGISTER(kOpposite, IntegerExpressionBuilder(BuildOpposite));
  REGISTER(kPack, ConstraintBuilder(BuildPack));
  REGISTER(kPathCumul, ConstraintBuilder(BuildPathCumul));
  REGISTER(kDelayedPathCumul, ConstraintBuilder(BuildDelayedPathCumul));
  REGISTER(kPerformedExpr, IntegerExpressionBuilder(BuildPerformedExpr));
  REGISTER(kPower, IntegerExpressionBuilder(BuildPower));
  REGISTER(kProduct, IntegerExpressionBuilder(BuildProduct));
  REGISTER(kScalProd, IntegerExpressionBuilder(BuildScalProd));
  REGISTER(kScalProdEqual, ConstraintBuilder(BuildScalProdEqual));
  REGISTER(kScalProdGreaterOrEqual,
           ConstraintBuilder(BuildScalProdGreaterOrEqual));
  REGISTER(kScalProdLessOrEqual, ConstraintBuilder(BuildScalProdLessOrEqual));
  REGISTER(kSemiContinuous, IntegerExpressionBuilder(BuildSemiContinuous));
  REGISTER(kSortingConstraint, ConstraintBuilder(BuildSortingConstraint));
  REGISTER(kSquare, IntegerExpressionBuilder(BuildSquare));
  REGISTER(kStartExpr, IntegerExpressionBuilder(BuildStartExpr));
  REGISTER(kSum, IntegerExpressionBuilder(BuildSum));
  REGISTER(kSumEqual, ConstraintBuilder(BuildSumEqual));
  REGISTER(kSumGreaterOrEqual, ConstraintBuilder(BuildSumGreaterOrEqual));
  REGISTER(kSumLessOrEqual, ConstraintBuilder(BuildSumLessOrEqual));
  REGISTER(kTransition, ConstraintBuilder(BuildTransition));
  REGISTER(kTrueConstraint, ConstraintBuilder(BuildTrueConstraint));
  REGISTER(kVarBoundWatcher, ConstraintBuilder(BuildVarBoundWatcher));
  REGISTER(kVarValueWatcher, ConstraintBuilder(BuildVarValueWatcher));
}
#undef REGISTER

void Solver::DeleteBuilders() {
  expression_builders_.clear();
  constraint_builders_.clear();
  interval_builders_.clear();
  sequence_builders_.clear();
}
}  // namespace operations_research
