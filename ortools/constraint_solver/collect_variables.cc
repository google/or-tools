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
#include <unordered_set>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

namespace operations_research {
namespace {
class CollectVariablesVisitor : public ModelParser {
 public:
  CollectVariablesVisitor(
      std::vector<IntVar*>* const primary_integer_variables,
      std::vector<IntVar*>* const secondary_integer_variables,
      std::vector<SequenceVar*>* const sequence_variables,
      std::vector<IntervalVar*>* const interval_variables)
      : primaries_(primary_integer_variables),
        secondaries_(secondary_integer_variables),
        sequences_(sequence_variables),
        intervals_(interval_variables) {}

  ~CollectVariablesVisitor() override {}

  void EndVisitModel(const std::string& solver_name) override {
    PopArgumentHolder();
    primaries_->assign(primary_set_.begin(), primary_set_.end());
    std::sort(primaries_->begin(), primaries_->end());
    secondaries_->assign(secondary_set_.begin(), secondary_set_.end());
    std::sort(secondaries_->begin(), secondaries_->end());
    intervals_->assign(interval_set_.begin(), interval_set_.end());
    std::sort(intervals_->begin(), intervals_->end());
    sequences_->assign(sequence_set_.begin(), sequence_set_.end());
    std::sort(sequences_->begin(), sequences_->end());
  }

  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override {
    if (type_name.compare(ModelVisitor::kLinkExprVar) == 0 ||
        (type_name.compare(ModelVisitor::kSumEqual) == 0 &&
         Top()->HasIntegerExpressionArgument(ModelVisitor::kTargetArgument)) ||
        type_name.compare(ModelVisitor::kElementEqual) == 0 ||
        (type_name.compare(ModelVisitor::kScalProdEqual) == 0 &&
         Top()->HasIntegerExpressionArgument(ModelVisitor::kTargetArgument)) ||
        type_name.compare(ModelVisitor::kIsEqual) == 0 ||
        type_name.compare(ModelVisitor::kIsDifferent) == 0 ||
        type_name.compare(ModelVisitor::kIsGreaterOrEqual) == 0 ||
        type_name.compare(ModelVisitor::kIsLessOrEqual) == 0 ||
        type_name.compare(ModelVisitor::kMinEqual) == 0 ||
        type_name.compare(ModelVisitor::kMaxEqual) == 0) {
      IntExpr* const target_expr =
          const_cast<IntExpr*>(Top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kTargetArgument));
      IntVar* const target_var = target_expr->Var();
      IgnoreIntegerVariable(target_var);
    } else if (type_name.compare(ModelVisitor::kCountEqual) == 0 &&
               Top()->HasIntegerExpressionArgument(
                   ModelVisitor::kCountArgument)) {
      IntExpr* const count_expr =
          const_cast<IntExpr*>(Top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kCountArgument));
      IntVar* const count_var = count_expr->Var();
      IgnoreIntegerVariable(count_var);
    } else if (type_name.compare(ModelVisitor::kAllowedAssignments) == 0) {
      const IntTupleSet& matrix =
          Top()->FindIntegerMatrixArgumentOrDie(ModelVisitor::kTuplesArgument);
      std::vector<std::unordered_set<int> > counters(matrix.Arity());
      for (int i = 0; i < matrix.NumTuples(); ++i) {
        for (int j = 0; j < matrix.Arity(); ++j) {
          counters[j].insert(matrix.Value(i, j));
        }
      }
      for (int j = 0; j < matrix.Arity(); ++j) {
        if (counters[j].size() == matrix.NumTuples()) {
          const std::vector<IntVar*>& vars =
              Top()->FindIntegerVariableArrayArgumentOrDie(
                  ModelVisitor::kVarsArgument);
          for (int k = 0; k < matrix.Arity(); ++k) {
            if (j != k) {
              IgnoreIntegerVariable(const_cast<IntVar*>(vars[k]));
            }
          }
          break;
        }
      }
    } else if (type_name.compare(ModelVisitor::kNoCycle) == 0) {
      const std::vector<IntVar*>& vars =
          Top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kActiveArgument);
      for (int i = 0; i < vars.size(); ++i) {
        IgnoreIntegerVariable(const_cast<IntVar*>(vars[i]));
      }
    } else if (type_name.compare(ModelVisitor::kPathCumul) == 0) {
      const std::vector<IntVar*>& vars =
          Top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kActiveArgument);
      for (int i = 0; i < vars.size(); ++i) {
        IgnoreIntegerVariable(const_cast<IntVar*>(vars[i]));
      }
    } else if (type_name.compare(ModelVisitor::kDistribute) == 0) {
      const std::vector<IntVar*>& vars =
          Top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kCardsArgument);
      for (int i = 0; i < vars.size(); ++i) {
        IgnoreIntegerVariable(const_cast<IntVar*>(vars[i]));
      }
    } else if (type_name.compare(ModelVisitor::kSortingConstraint) == 0) {
      const std::vector<IntVar*>& vars =
          Top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kTargetArgument);
      for (int i = 0; i < vars.size(); ++i) {
        IgnoreIntegerVariable(const_cast<IntVar*>(vars[i]));
      }
    } else if (type_name.compare(ModelVisitor::kVarValueWatcher) == 0) {
      const std::vector<IntVar*>& vars =
          Top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kVarsArgument);
      for (int i = 0; i < vars.size(); ++i) {
        IgnoreIntegerVariable(const_cast<IntVar*>(vars[i]));
      }
    } else if (type_name.compare(ModelVisitor::kVarBoundWatcher) == 0) {
      const std::vector<IntVar*>& vars =
          Top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kVarsArgument);
      for (int i = 0; i < vars.size(); ++i) {
        IgnoreIntegerVariable(const_cast<IntVar*>(vars[i]));
      }
    }

    PopArgumentHolder();
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (delegate != nullptr) {
      delegate->Accept(this);
      IgnoreIntegerVariable(const_cast<IntVar*>(variable));
    } else {
      if (!ContainsKey(primary_set_, var) &&
          !ContainsKey(secondary_set_, var) &&
          !ContainsKey(ignored_set_, var) && !var->Bound()) {
        primary_set_.insert(var);
      }
    }
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    IgnoreIntegerVariable(const_cast<IntVar*>(variable));
    delegate->Accept(this);
  }

  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override {
    if (delegate != nullptr) {
      delegate->Accept(this);
    } else {
      DeclareInterval(const_cast<IntervalVar*>(variable));
    }
  }

  void VisitSequenceVariable(const SequenceVar* const variable) override {
    SequenceVar* const var = const_cast<SequenceVar*>(variable);
    sequence_set_.insert(var);
    for (int i = 0; i < var->size(); ++i) {
      var->Interval(i)->Accept(this);
      DeclareInterval(var->Interval(i));
    }
  }

  std::string DebugString() const override { return "CollectVariablesVisitor"; }

 private:
  void IgnoreIntegerVariable(IntVar* const var) {
    primary_set_.erase(var);
    secondary_set_.erase(var);
    ignored_set_.insert(var);
  }

  void DeclareInterval(IntervalVar* const var) { interval_set_.insert(var); }

  std::vector<IntVar*>* const primaries_;
  std::vector<IntVar*>* const secondaries_;
  std::vector<SequenceVar*>* const sequences_;
  std::vector<IntervalVar*>* const intervals_;
  // These hash_set can't easily hold const IntVar*, because they
  // ultimately serve as containers of mutable IntVar.
  std::unordered_set<IntVar*> primary_set_;
  std::unordered_set<IntVar*> secondary_set_;
  std::unordered_set<IntVar*> ignored_set_;
  std::unordered_set<SequenceVar*> sequence_set_;
  std::unordered_set<IntervalVar*> interval_set_;
};
}  // namespace

bool Solver::CollectDecisionVariables(
    std::vector<IntVar*>* const primary_integer_variables,
    std::vector<IntVar*>* const secondary_integer_variables,
    std::vector<SequenceVar*>* const sequence_variables,
    std::vector<IntervalVar*>* const interval_variables) {
  CollectVariablesVisitor collector(primary_integer_variables,
                                    secondary_integer_variables,
                                    sequence_variables, interval_variables);
  Accept(&collector);
  return true;
}
}  // namespace operations_research
