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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_VISITOR_H_
#define ORTOOLS_CONSTRAINT_SOLVER_VISITOR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {

/// Argument Holder: useful when visiting a model.
#ifndef SWIG
class ArgumentHolder {
 public:
  /// Type of the argument.
  const std::string& TypeName() const;
  void SetTypeName(const std::string& type_name);

  /// Setters.
  void SetIntegerArgument(const std::string& arg_name, int64_t value);
  void SetIntegerArrayArgument(const std::string& arg_name,
                               const std::vector<int64_t>& values);
  void SetIntegerMatrixArgument(const std::string& arg_name,
                                const IntTupleSet& values);
  void SetIntegerExpressionArgument(const std::string& arg_name, IntExpr* expr);
  void SetIntegerVariableArrayArgument(const std::string& arg_name,
                                       const std::vector<IntVar*>& vars);
  void SetIntervalArgument(const std::string& arg_name, IntervalVar* var);
  void SetIntervalArrayArgument(const std::string& arg_name,
                                const std::vector<IntervalVar*>& vars);
  void SetSequenceArgument(const std::string& arg_name, SequenceVar* var);
  void SetSequenceArrayArgument(const std::string& arg_name,
                                const std::vector<SequenceVar*>& vars);

  /// Checks if arguments exist.
  bool HasIntegerExpressionArgument(const std::string& arg_name) const;
  bool HasIntegerVariableArrayArgument(const std::string& arg_name) const;

  /// Getters.
  int64_t FindIntegerArgumentWithDefault(const std::string& arg_name,
                                         int64_t def) const;
  int64_t FindIntegerArgumentOrDie(const std::string& arg_name) const;
  const std::vector<int64_t>& FindIntegerArrayArgumentOrDie(
      const std::string& arg_name) const;
  const IntTupleSet& FindIntegerMatrixArgumentOrDie(
      const std::string& arg_name) const;

  IntExpr* FindIntegerExpressionArgumentOrDie(
      const std::string& arg_name) const;
  const std::vector<IntVar*>& FindIntegerVariableArrayArgumentOrDie(
      const std::string& arg_name) const;

 private:
  std::string type_name_;
  absl::flat_hash_map<std::string, int64_t> integer_argument_;
  absl::flat_hash_map<std::string, std::vector<int64_t>>
      integer_array_argument_;
  absl::flat_hash_map<std::string, IntTupleSet> matrix_argument_;
  absl::flat_hash_map<std::string, IntExpr*> integer_expression_argument_;
  absl::flat_hash_map<std::string, IntervalVar*> interval_argument_;
  absl::flat_hash_map<std::string, SequenceVar*> sequence_argument_;
  absl::flat_hash_map<std::string, std::vector<IntVar*>>
      integer_variable_array_argument_;
  absl::flat_hash_map<std::string, std::vector<IntervalVar*>>
      interval_array_argument_;
  absl::flat_hash_map<std::string, std::vector<SequenceVar*>>
      sequence_array_argument_;
};

/// Model Parser
class ModelParser : public ModelVisitor {
 public:
  ModelParser();

  ~ModelParser() override;

  /// Header/footers.
  void BeginVisitModel(const std::string& solver_name) override;
  void EndVisitModel(const std::string& solver_name) override;
  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint* constraint) override;
  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* constraint) override;
  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* expr) override;
  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* expr) override;
  void VisitIntegerVariable(const IntVar* variable, IntExpr* delegate) override;
  void VisitIntegerVariable(const IntVar* variable,
                            const std::string& operation, int64_t value,
                            IntVar* delegate) override;
  void VisitIntervalVariable(const IntervalVar* variable,
                             const std::string& operation, int64_t value,
                             IntervalVar* delegate) override;
  void VisitSequenceVariable(const SequenceVar* variable) override;
  /// Integer arguments
  void VisitIntegerArgument(const std::string& arg_name,
                            int64_t value) override;
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64_t>& values) override;
  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override;
  /// Variables.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* argument) override;
  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name,
      const std::vector<IntVar*>& arguments) override;
  /// Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* argument) override;
  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& arguments) override;
  /// Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* argument) override;
  void VisitSequenceArrayArgument(
      const std::string& arg_name,
      const std::vector<SequenceVar*>& arguments) override;

 protected:
  void PushArgumentHolder();
  void PopArgumentHolder();
  ArgumentHolder* Top() const;

 private:
  std::vector<ArgumentHolder*> holders_;
};
#endif  // SWIG

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_VISITOR_H_
