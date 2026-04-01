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
#include <functional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/base_export.h"
#include "ortools/constraint_solver/reversible_engine.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {

class BaseObject;
class Constraint;
class IntervalVar;
class IntExpr;
class IntVar;
class SequenceVar;

/// Model visitor.
class OR_DLL ModelVisitor : public BaseObject {
 public:
  /// Constraint and Expression types.
  static const char kAbs[];
  static const char kAbsEqual[];
  static const char kAllDifferent[];
  static const char kAllowedAssignments[];
  static const char kAtMost[];
  static const char kIndexOf[];
  static const char kBetween[];
  static const char kConditionalExpr[];
  static const char kCircuit[];
  static const char kConvexPiecewise[];
  static const char kCountEqual[];
  static const char kCover[];
  static const char kCumulative[];
  static const char kDeviation[];
  static const char kDifference[];
  static const char kDisjunctive[];
  static const char kDistribute[];
  static const char kDivide[];
  static const char kDurationExpr[];
  static const char kElement[];
  static const char kLightElementEqual[];
  static const char kElementEqual[];
  static const char kEndExpr[];
  static const char kEquality[];
  static const char kFalseConstraint[];
  static const char kGlobalCardinality[];
  static const char kGreater[];
  static const char kGreaterOrEqual[];
  static const char kIntegerVariable[];
  static const char kIntervalBinaryRelation[];
  static const char kIntervalDisjunction[];
  static const char kIntervalUnaryRelation[];
  static const char kIntervalVariable[];
  static const char kInversePermutation[];
  static const char kIsBetween[];
  static const char kIsDifferent[];
  static const char kIsEqual[];
  static const char kIsGreater[];
  static const char kIsGreaterOrEqual[];
  static const char kIsLess[];
  static const char kIsLessOrEqual[];
  static const char kIsMember[];
  static const char kLess[];
  static const char kLessOrEqual[];
  static const char kLexLess[];
  static const char kLinkExprVar[];
  static const char kMapDomain[];
  static const char kMax[];
  static const char kMaxEqual[];
  static const char kMember[];
  static const char kMin[];
  static const char kMinEqual[];
  static const char kModulo[];
  static const char kNoCycle[];
  static const char kNonEqual[];
  static const char kNotBetween[];
  static const char kNotMember[];
  static const char kNullIntersect[];
  static const char kOpposite[];
  static const char kPack[];
  static const char kPathCumul[];
  static const char kDelayedPathCumul[];
  static const char kPerformedExpr[];
  static const char kPower[];
  static const char kProduct[];
  static const char kScalProd[];
  static const char kScalProdEqual[];
  static const char kScalProdGreaterOrEqual[];
  static const char kScalProdLessOrEqual[];
  static const char kSemiContinuous[];
  static const char kSequenceVariable[];
  static const char kSortingConstraint[];
  static const char kSquare[];
  static const char kStartExpr[];
  static const char kSum[];
  static const char kSumEqual[];
  static const char kSumGreaterOrEqual[];
  static const char kSumLessOrEqual[];
  static const char kTrace[];
  static const char kTransition[];
  static const char kTrueConstraint[];
  static const char kVarBoundWatcher[];
  static const char kVarValueWatcher[];

  /// Extension names:
  static const char kCountAssignedItemsExtension[];
  static const char kCountUsedBinsExtension[];
  static const char kInt64ToBoolExtension[];
  static const char kInt64ToInt64Extension[];
  static const char kObjectiveExtension[];
  static const char kSearchLimitExtension[];
  static const char kUsageEqualVariableExtension[];

  static const char kUsageLessConstantExtension[];
  static const char kVariableGroupExtension[];
  static const char kVariableUsageLessConstantExtension[];
  static const char kWeightedSumOfAssignedEqualVariableExtension[];

  /// argument names:
  static const char kActiveArgument[];
  static const char kAssumePathsArgument[];
  static const char kBranchesLimitArgument[];
  static const char kCapacityArgument[];
  static const char kCardsArgument[];
  static const char kCoefficientsArgument[];
  static const char kCountArgument[];
  static const char kCumulativeArgument[];
  static const char kCumulsArgument[];
  static const char kDemandsArgument[];
  static const char kDurationMaxArgument[];
  static const char kDurationMinArgument[];
  static const char kEarlyCostArgument[];
  static const char kEarlyDateArgument[];
  static const char kEndMaxArgument[];
  static const char kEndMinArgument[];
  static const char kEndsArgument[];
  static const char kExpressionArgument[];
  static const char kFailuresLimitArgument[];
  static const char kFinalStatesArgument[];
  static const char kFixedChargeArgument[];
  static const char kIndex2Argument[];
  static const char kIndex3Argument[];
  static const char kIndexArgument[];
  static const char kInitialState[];
  static const char kIntervalArgument[];
  static const char kIntervalsArgument[];
  static const char kLateCostArgument[];
  static const char kLateDateArgument[];
  static const char kLeftArgument[];
  static const char kMaxArgument[];
  static const char kMaximizeArgument[];
  static const char kMinArgument[];
  static const char kModuloArgument[];
  static const char kNextsArgument[];
  static const char kOptionalArgument[];
  static const char kPartialArgument[];
  static const char kPositionXArgument[];
  static const char kPositionYArgument[];
  static const char kRangeArgument[];
  static const char kRelationArgument[];
  static const char kRightArgument[];
  static const char kSequenceArgument[];
  static const char kSequencesArgument[];
  static const char kSizeArgument[];
  static const char kSizeXArgument[];
  static const char kSizeYArgument[];
  static const char kSmartTimeCheckArgument[];
  static const char kSolutionLimitArgument[];
  static const char kStartMaxArgument[];
  static const char kStartMinArgument[];
  static const char kStartsArgument[];
  static const char kStepArgument[];
  static const char kTargetArgument[];
  static const char kTimeLimitArgument[];
  static const char kTransitsArgument[];
  static const char kTuplesArgument[];
  static const char kValueArgument[];
  static const char kValuesArgument[];
  static const char kVariableArgument[];
  static const char kVarsArgument[];
  static const char kEvaluatorArgument[];

  /// Operations.
  static const char kMirrorOperation[];
  static const char kRelaxedMaxOperation[];
  static const char kRelaxedMinOperation[];
  static const char kSumOperation[];
  static const char kDifferenceOperation[];
  static const char kProductOperation[];
  static const char kStartSyncOnStartOperation[];
  static const char kStartSyncOnEndOperation[];
  static const char kTraceOperation[];

  ~ModelVisitor() override;

  /// ----- Virtual methods for visitors -----

  /// Begin/End visit element.
  virtual void BeginVisitModel(const std::string& type_name);
  virtual void EndVisitModel(const std::string& type_name);
  virtual void BeginVisitConstraint(const std::string& type_name,
                                    const Constraint* constraint);
  virtual void EndVisitConstraint(const std::string& type_name,
                                  const Constraint* constraint);
  virtual void BeginVisitExtension(const std::string& type);
  virtual void EndVisitExtension(const std::string& type);
  virtual void BeginVisitIntegerExpression(const std::string& type_name,
                                           const IntExpr* expr);
  virtual void EndVisitIntegerExpression(const std::string& type_name,
                                         const IntExpr* expr);
  virtual void VisitIntegerVariable(const IntVar* variable, IntExpr* delegate);
  virtual void VisitIntegerVariable(const IntVar* variable,
                                    const std::string& operation, int64_t value,
                                    IntVar* delegate);
  virtual void VisitIntervalVariable(const IntervalVar* variable,
                                     const std::string& operation,
                                     int64_t value, IntervalVar* delegate);
  virtual void VisitSequenceVariable(const SequenceVar* variable);

  /// Visit integer arguments.
  virtual void VisitIntegerArgument(const std::string& arg_name, int64_t value);
  virtual void VisitIntegerArrayArgument(const std::string& arg_name,
                                         const std::vector<int64_t>& values);
  virtual void VisitIntegerMatrixArgument(const std::string& arg_name,
                                          const IntTupleSet& tuples);

  /// Visit integer expression argument.
  virtual void VisitIntegerExpressionArgument(const std::string& arg_name,
                                              IntExpr* argument);

  virtual void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments);

  /// Visit interval argument.
  virtual void VisitIntervalArgument(const std::string& arg_name,
                                     IntervalVar* argument);

  virtual void VisitIntervalArrayArgument(
      const std::string& arg_name, const std::vector<IntervalVar*>& arguments);
  /// Visit sequence argument.
  virtual void VisitSequenceArgument(const std::string& arg_name,
                                     SequenceVar* argument);

  virtual void VisitSequenceArrayArgument(
      const std::string& arg_name, const std::vector<SequenceVar*>& arguments);
#if !defined(SWIG)
  /// Helpers.
  virtual void VisitIntegerVariableEvaluatorArgument(
      const std::string& arg_name,
      const std::function<IntVar*(int64_t)>& arguments);

  /// Using SWIG on callbacks is troublesome, so we hide these methods during
  /// the wrapping.
  void VisitInt64ToBoolExtension(std::function<bool(int64_t)> filter,
                                 int64_t index_min, int64_t index_max);
  void VisitInt64ToInt64Extension(const std::function<int64_t(int64_t)>& eval,
                                  int64_t index_min, int64_t index_max);
  /// Expands function as array when index min is 0.
  void VisitInt64ToInt64AsArray(const std::function<int64_t(int64_t)>& eval,
                                const std::string& arg_name, int64_t index_max);
#endif  // #if !defined(SWIG)
};

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
