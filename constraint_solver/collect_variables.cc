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
    LOG(INFO) << "Starts collecting variables on model " << solver_name;
  }

  virtual void EndVisitModel(const string& solver_name) {
    LOG(INFO) << "Finishes collecting variables.";
  }

  virtual void BeginVisitConstraint(const string& type_name,
                                    const Constraint* const constraint) {
    if (constraint->IsCastConstraint()) {
      const CastConstraint* const cast_constraint =
          reinterpret_cast<const CastConstraint* const>(constraint);
      ignored_set_.insert(cast_constraint->target_var());
    }
  }

  virtual void EndVisitConstraint(const string& type_name,
                                  const Constraint* const constraint) {
  }

  virtual void BeginVisitIntegerExpression(const string& type_name,
                                           const IntExpr* const expr) {
  }

  virtual void EndVisitIntegerExpression(const string& type_name,
                                         const IntExpr* const expr) {
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
        primaries_->push_back(const_cast<IntVar*>(var));
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
        intervals_->push_back(var);
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
      sequences_->push_back(var);
    }
    for (int i = 0; i < var->size(); ++i) {
      var->Interval(i)->Accept(this);
    }
  }

  // Variables.
  virtual void VisitIntegerExpressionArgument(
      const string& arg_name,
      const IntExpr* const argument) {
    argument->Accept(this);
  }

  virtual void VisitIntegerVariableArrayArgument(
      const string& arg_name,
      const IntVar* const * arguments,
      int size) {
    for (int i = 0; i < size; ++i) {
      arguments[i]->Accept(this);
    }
  }

  // Visit interval argument.
  virtual void VisitIntervalArgument(const string& arg_name,
                                     const IntervalVar* const argument) {
    argument->Accept(this);
  }

  virtual void VisitIntervalArgumentArray(const string& arg_name,
                                          const IntervalVar* const * arguments,
                                          int size) {
    for (int i = 0; i < size; ++i) {
      arguments[i]->Accept(this);
    }
  }

  // Visit sequence argument.
  virtual void VisitSequenceArgument(const string& arg_name,
                                     const SequenceVar* const argument) {
    argument->Accept(this);
  }

  virtual void VisitSequenceArgumentArray(const string& arg_name,
                                          const SequenceVar* const * arguments,
                                          int size) {
    for (int i = 0; i < size; ++i) {
      arguments[i]->Accept(this);
    }
  }

 private:
  std::vector<IntVar*>* const primaries_;
  std::vector<IntVar*>* const secondaries_;
  std::vector<SequenceVar*>* const sequences_;
  std::vector<IntervalVar*>* const intervals_;
  hash_set<IntVar*> primary_set_;
  hash_set<IntVar*> secondary_set_;
  hash_set<IntVar*> ignored_set_;
  hash_set<SequenceVar*> sequence_set_;
  hash_set<IntervalVar*> interval_set_;
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
