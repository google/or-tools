// Copyright 2010-2011 Google
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
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {
namespace {

class ArgumentHolder {
 public:
  struct Matrix {
    Matrix() : values(NULL), rows(0), columns(0) {}
    Matrix(const int64 * const * v, int r, int c)
        : values(v), rows(r), columns(c) {}
    const int64* const * values;
    int rows;
    int columns;
  };

  const string& type_name() const {
    return type_name_;
  }

  void set_type_name(const string& type_name) {
    type_name_ = type_name;
  }

  void set_integer_matrix_argument(const string& arg_name,
                                   const int64* const * const values,
                                   int rows,
                                   int columns) {
    matrix_argument_[arg_name] = Matrix(values, rows, columns);
  }

  void set_integer_expression_argument(const string& arg_name,
                                       const IntExpr* const expr) {
    integer_expression_argument_[arg_name] = expr;
  }

  void set_integer_variable_array_argument(const string& arg_name,
                                           const IntVar* const * const vars,
                                           int size) {
    for (int i = 0; i < size; ++i) {
      integer_variable_array_argument_[arg_name].push_back(vars[i]);
    }
  }

  void set_interval_argument(const string& arg_name,
                             const IntervalVar* const var) {
    interval_argument_[arg_name] = var;
  }

  void set_interval_array_argument(const string& arg_name,
                                   const IntervalVar* const * const vars,
                                   int size) {
    for (int i = 0; i < size; ++i) {
      interval_array_argument_[arg_name].push_back(vars[i]);
    }
  }

  void set_sequence_argument(const string& arg_name,
                             const SequenceVar* const var) {
    sequence_argument_[arg_name] = var;
  }

  void set_sequence_array_argument(const string& arg_name,
                                   const SequenceVar* const * const vars,
                                   int size) {
    for (int i = 0; i < size; ++i) {
      sequence_array_argument_[arg_name].push_back(vars[i]);
    }
  }

  const IntExpr* FindIntegerExpressionArgumentOrDie(const string& arg_name) {
    return FindOrDie(integer_expression_argument_, arg_name);
  }

  const std::vector<const IntVar*>& FindIntegerVariableArrayArgumentOrDie(
      const string& arg_name) {
    return FindOrDie(integer_variable_array_argument_, arg_name);
  }

  const Matrix& FindIntegerMatrixArgumentOrDie(const string& arg_name) {
    return FindOrDie(matrix_argument_, arg_name);
  }

 private:
  string type_name_;
  hash_map<string, const IntExpr*> integer_expression_argument_;
  hash_map<string, const IntervalVar*> interval_argument_;
  hash_map<string, const SequenceVar*> sequence_argument_;
  hash_map<string,
           std::vector<const IntVar*> > integer_variable_array_argument_;
  hash_map<string, std::vector<const IntervalVar*> > interval_array_argument_;
  hash_map<string, std::vector<const SequenceVar*> > sequence_array_argument_;
  hash_map<string, Matrix> matrix_argument_;
};

class CollectVariablesVisitor : public ModelVisitor {
 public:
  CollectVariablesVisitor(std::vector<IntVar*>* const primary_integer_variables,
                          std::vector<IntVar*>* const secondary_integer_variables,
                          std::vector<SequenceVar*>* const sequence_variables,
                          std::vector<IntervalVar*>* const interval_variables)
      : primaries_(primary_integer_variables),
        secondaries_(secondary_integer_variables),
        sequences_(sequence_variables),
        intervals_(interval_variables) {}
  virtual ~CollectVariablesVisitor() {}

  // Header/footers.
  virtual void BeginVisitModel(const string& solver_name) {
    PushArgumentHolder();
  }

  virtual void EndVisitModel(const string& solver_name) {
    PopArgumentHolder();
    primaries_->assign(primary_set_.begin(), primary_set_.end());
    secondaries_->assign(secondary_set_.begin(), secondary_set_.end());
    intervals_->assign(interval_set_.begin(), interval_set_.end());
    sequences_->assign(sequence_set_.begin(), sequence_set_.end());
  }

  virtual void BeginVisitConstraint(const string& type_name,
                                    const Constraint* const constraint) {
    PushArgumentHolder();
  }

  virtual void EndVisitConstraint(const string& type_name,
                                  const Constraint* const constraint) {
    if (type_name.compare(ModelVisitor::kLinkExprVar) == 0 ||
        type_name.compare(ModelVisitor::kSumEqual) == 0 ||
        type_name.compare(ModelVisitor::kCountEqual) == 0 ||
        type_name.compare(ModelVisitor::kElementEqual) == 0 ||
        type_name.compare(ModelVisitor::kScalProdEqual) == 0 ||
        type_name.compare(ModelVisitor::kIsEqual) == 0 ||
        type_name.compare(ModelVisitor::kIsDifferent) == 0 ||
        type_name.compare(ModelVisitor::kIsGreaterOrEqual) == 0 ||
        type_name.compare(ModelVisitor::kIsLessOrEqual) == 0) {
      IntExpr* const target_expr =
          const_cast<IntExpr*>(
              top()->FindIntegerExpressionArgumentOrDie(
                  ModelVisitor::kTargetArgument));
      IntVar* const target_var = target_expr->Var();
      IgnoreIntegerVariable(target_var);
    } else if (type_name.compare(ModelVisitor::kAllowedAssignments) == 0) {
      const ArgumentHolder::Matrix& matrix =
          top()->FindIntegerMatrixArgumentOrDie(ModelVisitor::kTuplesArgument);
      vector<hash_set<int> > counters(matrix.columns);
      for (int i = 0; i < matrix.rows; ++i) {
        for (int j = 0; j < matrix.columns; ++j) {
          counters[j].insert(matrix.values[i][j]);
        }
      }
      for (int j = 0; j < matrix.columns; ++j) {
        if (counters[j].size() == matrix.rows) {
          vector<const IntVar*> vars =
              top()->FindIntegerVariableArrayArgumentOrDie(
                  ModelVisitor::kVarsArgument);
          LOG(INFO) << "Found index variable in allowed assignment constraint: "
                    << vars[j]->DebugString();
          for (int k = 0; k < matrix.columns; ++k) {
            if (j != k) {
              IgnoreIntegerVariable(const_cast<IntVar*>(vars[k]));
            }
          }
          break;
        }
      }
    }
    PopArgumentHolder();
  }

  virtual void BeginVisitIntegerExpression(const string& type_name,
                                           const IntExpr* const expr) {
    PushArgumentHolder();
  }

  virtual void EndVisitIntegerExpression(const string& type_name,
                                         const IntExpr* const expr) {
    PopArgumentHolder();
  }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const IntExpr* const delegate) {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (delegate != NULL) {
      delegate->Accept(this);
    } else {
      if (!ContainsKey(primary_set_, var) &&
          !ContainsKey(secondary_set_, var) &&
          !ContainsKey(ignored_set_, var) &&
          !var->Bound()) {
        primary_set_.insert(var);
      }
    }
  }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const string& operation,
                                    int64 value,
                                    const IntVar* const delegate) {

  }

  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const string operation,
                                     const IntervalVar* const delegate) {
    if (delegate != NULL) {
      delegate->Accept(this);
    } else {
      IntervalVar* const var = const_cast<IntervalVar*>(variable);
      if (!ContainsKey(interval_set_, var)) {
        interval_set_.insert(var);
      }
    }
  }

  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const string operation,
                                     const IntervalVar* const * delegates,
                                     int size) {
    for (int i = 0; i < size; ++i) {
      delegates[i]->Accept(this);
    }
  }

  virtual void VisitSequenceVariable(const SequenceVar* const variable) {
    SequenceVar* const var = const_cast<SequenceVar*>(variable);
    if (!ContainsKey(sequence_set_, var)) {
      sequence_set_.insert(var);
    }
    for (int i = 0; i < var->size(); ++i) {
      var->Interval(i)->Accept(this);
    }
  }

  // Integer arguments
  virtual void VisitIntegerMatrixArgument(const string& arg_name,
                                          const int64* const * const values,
                                          int rows,
                                          int columns) {
    top()->set_integer_matrix_argument(arg_name, values, rows, columns);
  }

  // Variables.
  virtual void VisitIntegerExpressionArgument(
      const string& arg_name,
      const IntExpr* const argument) {
    top()->set_integer_expression_argument(arg_name, argument);
    argument->Accept(this);
  }

  virtual void VisitIntegerVariableArrayArgument(
      const string& arg_name,
      const IntVar* const * arguments,
      int size) {
    top()->set_integer_variable_array_argument(arg_name, arguments, size);
    for (int i = 0; i < size; ++i) {
      arguments[i]->Accept(this);
    }
  }

  // Visit interval argument.
  virtual void VisitIntervalArgument(const string& arg_name,
                                     const IntervalVar* const argument) {
    top()->set_interval_argument(arg_name, argument);
    argument->Accept(this);
  }

  virtual void VisitIntervalArgumentArray(const string& arg_name,
                                          const IntervalVar* const * arguments,
                                          int size) {
    top()->set_interval_array_argument(arg_name, arguments, size);
    for (int i = 0; i < size; ++i) {
      arguments[i]->Accept(this);
    }
  }

  // Visit sequence argument.
  virtual void VisitSequenceArgument(const string& arg_name,
                                     const SequenceVar* const argument) {
    top()->set_sequence_argument(arg_name, argument);
    argument->Accept(this);
  }

  virtual void VisitSequenceArgumentArray(const string& arg_name,
                                          const SequenceVar* const * arguments,
                                          int size) {
    top()->set_sequence_array_argument(arg_name, arguments, size);
    for (int i = 0; i < size; ++i) {
      arguments[i]->Accept(this);
    }
  }

 private:
  void PushArgumentHolder() {
    holders_.push_back(new ArgumentHolder);
  }

  void PopArgumentHolder() {
    CHECK(!holders_.empty());
    delete holders_.back();
    holders_.pop_back();
  }

  ArgumentHolder* top() const {
    CHECK(!holders_.empty());
    return holders_.back();
  }

  void IgnoreIntegerVariable(IntVar* const var) {
    primary_set_.erase(var);
    secondary_set_.erase(var);
    ignored_set_.insert(var);
  }

  std::vector<IntVar*>* const primaries_;
  std::vector<IntVar*>* const secondaries_;
  std::vector<SequenceVar*>* const sequences_;
  std::vector<IntervalVar*>* const intervals_;
  hash_set<IntVar*> primary_set_;
  hash_set<IntVar*> secondary_set_;
  hash_set<IntVar*> ignored_set_;
  hash_set<SequenceVar*> sequence_set_;
  hash_set<IntervalVar*> interval_set_;
  std::vector<ArgumentHolder*> holders_;
};
}  // namespace

bool Solver::CollectDecisionVariables(std::vector<IntVar*>* const primary_integer_variables,
                                      std::vector<IntVar*>* const secondary_integer_variables,
                                      std::vector<SequenceVar*>* const sequence_variables,
                                      std::vector<IntervalVar*>* const interval_variables) {
  CollectVariablesVisitor collector(primary_integer_variables,
                                    secondary_integer_variables,
                                    sequence_variables,
                                    interval_variables);
  Accept(&collector);
  return true;
}
}  // namespace operations_research
