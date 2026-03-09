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

#include "ortools/constraint_solver/visitor.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/sequence_var.h"
#include "ortools/constraint_solver/utilities.h"
#include "ortools/util/tuple_set.h"

namespace operations_research {

// ---------- ModelVisitor ----------

// Tags for constraints, arguments, extensions.

const char ModelVisitor::kAbs[] = "Abs";
const char ModelVisitor::kAbsEqual[] = "AbsEqual";
const char ModelVisitor::kAllDifferent[] = "AllDifferent";
const char ModelVisitor::kAllowedAssignments[] = "AllowedAssignments";
const char ModelVisitor::kAtMost[] = "AtMost";
const char ModelVisitor::kBetween[] = "Between";
const char ModelVisitor::kConditionalExpr[] = "ConditionalExpr";
const char ModelVisitor::kCircuit[] = "Circuit";
const char ModelVisitor::kConvexPiecewise[] = "ConvexPiecewise";
const char ModelVisitor::kCountEqual[] = "CountEqual";
const char ModelVisitor::kCover[] = "Cover";
const char ModelVisitor::kCumulative[] = "Cumulative";
const char ModelVisitor::kDeviation[] = "Deviation";
const char ModelVisitor::kDifference[] = "Difference";
const char ModelVisitor::kDisjunctive[] = "Disjunctive";
const char ModelVisitor::kDistribute[] = "Distribute";
const char ModelVisitor::kDivide[] = "Divide";
const char ModelVisitor::kDurationExpr[] = "DurationExpression";
const char ModelVisitor::kElement[] = "Element";
const char ModelVisitor::kLightElementEqual[] = "LightElementEqual";
const char ModelVisitor::kElementEqual[] = "ElementEqual";
const char ModelVisitor::kEndExpr[] = "EndExpression";
const char ModelVisitor::kEquality[] = "Equal";
const char ModelVisitor::kFalseConstraint[] = "FalseConstraint";
const char ModelVisitor::kGlobalCardinality[] = "GlobalCardinality";
const char ModelVisitor::kGreater[] = "Greater";
const char ModelVisitor::kGreaterOrEqual[] = "GreaterOrEqual";
const char ModelVisitor::kIndexOf[] = "IndexOf";
const char ModelVisitor::kIntegerVariable[] = "IntegerVariable";
const char ModelVisitor::kIntervalBinaryRelation[] = "IntervalBinaryRelation";
const char ModelVisitor::kIntervalDisjunction[] = "IntervalDisjunction";
const char ModelVisitor::kIntervalUnaryRelation[] = "IntervalUnaryRelation";
const char ModelVisitor::kIntervalVariable[] = "IntervalVariable";
const char ModelVisitor::kInversePermutation[] = "InversePermutation";
const char ModelVisitor::kIsBetween[] = "IsBetween;";
const char ModelVisitor::kIsDifferent[] = "IsDifferent";
const char ModelVisitor::kIsEqual[] = "IsEqual";
const char ModelVisitor::kIsGreater[] = "IsGreater";
const char ModelVisitor::kIsGreaterOrEqual[] = "IsGreaterOrEqual";
const char ModelVisitor::kIsLess[] = "IsLess";
const char ModelVisitor::kIsLessOrEqual[] = "IsLessOrEqual";
const char ModelVisitor::kIsMember[] = "IsMember;";
const char ModelVisitor::kLess[] = "Less";
const char ModelVisitor::kLessOrEqual[] = "LessOrEqual";
const char ModelVisitor::kLexLess[] = "LexLess";
const char ModelVisitor::kLinkExprVar[] = "CastExpressionIntoVariable";
const char ModelVisitor::kMapDomain[] = "MapDomain";
const char ModelVisitor::kMax[] = "Max";
const char ModelVisitor::kMaxEqual[] = "MaxEqual";
const char ModelVisitor::kMember[] = "Member";
const char ModelVisitor::kMin[] = "Min";
const char ModelVisitor::kMinEqual[] = "MinEqual";
const char ModelVisitor::kModulo[] = "Modulo";
const char ModelVisitor::kNoCycle[] = "NoCycle";
const char ModelVisitor::kNonEqual[] = "NonEqual";
const char ModelVisitor::kNotBetween[] = "NotBetween";
const char ModelVisitor::kNotMember[] = "NotMember";
const char ModelVisitor::kNullIntersect[] = "NullIntersect";
const char ModelVisitor::kOpposite[] = "Opposite";
const char ModelVisitor::kPack[] = "Pack";
const char ModelVisitor::kPathCumul[] = "PathCumul";
const char ModelVisitor::kDelayedPathCumul[] = "DelayedPathCumul";
const char ModelVisitor::kPerformedExpr[] = "PerformedExpression";
const char ModelVisitor::kPower[] = "Power";
const char ModelVisitor::kProduct[] = "Product";
const char ModelVisitor::kScalProd[] = "ScalarProduct";
const char ModelVisitor::kScalProdEqual[] = "ScalarProductEqual";
const char ModelVisitor::kScalProdGreaterOrEqual[] =
    "ScalarProductGreaterOrEqual";
const char ModelVisitor::kScalProdLessOrEqual[] = "ScalarProductLessOrEqual";
const char ModelVisitor::kSemiContinuous[] = "SemiContinuous";
const char ModelVisitor::kSequenceVariable[] = "SequenceVariable";
const char ModelVisitor::kSortingConstraint[] = "SortingConstraint";
const char ModelVisitor::kSquare[] = "Square";
const char ModelVisitor::kStartExpr[] = "StartExpression";
const char ModelVisitor::kSum[] = "Sum";
const char ModelVisitor::kSumEqual[] = "SumEqual";
const char ModelVisitor::kSumGreaterOrEqual[] = "SumGreaterOrEqual";
const char ModelVisitor::kSumLessOrEqual[] = "SumLessOrEqual";
const char ModelVisitor::kTransition[] = "Transition";
const char ModelVisitor::kTrace[] = "Trace";
const char ModelVisitor::kTrueConstraint[] = "TrueConstraint";
const char ModelVisitor::kVarBoundWatcher[] = "VarBoundWatcher";
const char ModelVisitor::kVarValueWatcher[] = "VarValueWatcher";

const char ModelVisitor::kCountAssignedItemsExtension[] = "CountAssignedItems";
const char ModelVisitor::kCountUsedBinsExtension[] = "CountUsedBins";
const char ModelVisitor::kInt64ToBoolExtension[] = "Int64ToBoolFunction";
const char ModelVisitor::kInt64ToInt64Extension[] = "Int64ToInt64Function";
const char ModelVisitor::kObjectiveExtension[] = "Objective";
const char ModelVisitor::kSearchLimitExtension[] = "SearchLimit";
const char ModelVisitor::kUsageEqualVariableExtension[] = "UsageEqualVariable";

const char ModelVisitor::kUsageLessConstantExtension[] = "UsageLessConstant";
const char ModelVisitor::kVariableGroupExtension[] = "VariableGroup";
const char ModelVisitor::kVariableUsageLessConstantExtension[] =
    "VariableUsageLessConstant";
const char ModelVisitor::kWeightedSumOfAssignedEqualVariableExtension[] =
    "WeightedSumOfAssignedEqualVariable";

const char ModelVisitor::kActiveArgument[] = "active";
const char ModelVisitor::kAssumePathsArgument[] = "assume_paths";
const char ModelVisitor::kBranchesLimitArgument[] = "branches_limit";
const char ModelVisitor::kCapacityArgument[] = "capacity";
const char ModelVisitor::kCardsArgument[] = "cardinalities";
const char ModelVisitor::kCoefficientsArgument[] = "coefficients";
const char ModelVisitor::kCountArgument[] = "count";
const char ModelVisitor::kCumulativeArgument[] = "cumulative";
const char ModelVisitor::kCumulsArgument[] = "cumuls";
const char ModelVisitor::kDemandsArgument[] = "demands";
const char ModelVisitor::kDurationMinArgument[] = "duration_min";
const char ModelVisitor::kDurationMaxArgument[] = "duration_max";
const char ModelVisitor::kEarlyCostArgument[] = "early_cost";
const char ModelVisitor::kEarlyDateArgument[] = "early_date";
const char ModelVisitor::kEndMinArgument[] = "end_min";
const char ModelVisitor::kEndMaxArgument[] = "end_max";
const char ModelVisitor::kEndsArgument[] = "ends";
const char ModelVisitor::kExpressionArgument[] = "expression";
const char ModelVisitor::kFailuresLimitArgument[] = "failures_limit";
const char ModelVisitor::kFinalStatesArgument[] = "final_states";
const char ModelVisitor::kFixedChargeArgument[] = "fixed_charge";
const char ModelVisitor::kIndex2Argument[] = "index2";
const char ModelVisitor::kIndex3Argument[] = "index3";
const char ModelVisitor::kIndexArgument[] = "index";
const char ModelVisitor::kInitialState[] = "initial_state";
const char ModelVisitor::kIntervalArgument[] = "interval";
const char ModelVisitor::kIntervalsArgument[] = "intervals";
const char ModelVisitor::kLateCostArgument[] = "late_cost";
const char ModelVisitor::kLateDateArgument[] = "late_date";
const char ModelVisitor::kLeftArgument[] = "left";
const char ModelVisitor::kMaxArgument[] = "max_value";
const char ModelVisitor::kMaximizeArgument[] = "maximize";
const char ModelVisitor::kMinArgument[] = "min_value";
const char ModelVisitor::kModuloArgument[] = "modulo";
const char ModelVisitor::kNextsArgument[] = "nexts";
const char ModelVisitor::kOptionalArgument[] = "optional";
const char ModelVisitor::kPartialArgument[] = "partial";
const char ModelVisitor::kPositionXArgument[] = "position_x";
const char ModelVisitor::kPositionYArgument[] = "position_y";
const char ModelVisitor::kRangeArgument[] = "range";
const char ModelVisitor::kRelationArgument[] = "relation";
const char ModelVisitor::kRightArgument[] = "right";
const char ModelVisitor::kSequenceArgument[] = "sequence";
const char ModelVisitor::kSequencesArgument[] = "sequences";
const char ModelVisitor::kSmartTimeCheckArgument[] = "smart_time_check";
const char ModelVisitor::kSizeArgument[] = "size";
const char ModelVisitor::kSizeXArgument[] = "size_x";
const char ModelVisitor::kSizeYArgument[] = "size_y";
const char ModelVisitor::kSolutionLimitArgument[] = "solutions_limit";
const char ModelVisitor::kStartMinArgument[] = "start_min";
const char ModelVisitor::kStartMaxArgument[] = "start_max";
const char ModelVisitor::kStartsArgument[] = "starts";
const char ModelVisitor::kStepArgument[] = "step";
const char ModelVisitor::kTargetArgument[] = "target_variable";
const char ModelVisitor::kTimeLimitArgument[] = "time_limit";
const char ModelVisitor::kTransitsArgument[] = "transits";
const char ModelVisitor::kTuplesArgument[] = "tuples";
const char ModelVisitor::kValueArgument[] = "value";
const char ModelVisitor::kValuesArgument[] = "values";
const char ModelVisitor::kVarsArgument[] = "variables";
const char ModelVisitor::kEvaluatorArgument[] = "evaluator";

const char ModelVisitor::kVariableArgument[] = "variable";

const char ModelVisitor::kMirrorOperation[] = "mirror";
const char ModelVisitor::kRelaxedMaxOperation[] = "relaxed_max";
const char ModelVisitor::kRelaxedMinOperation[] = "relaxed_min";
const char ModelVisitor::kSumOperation[] = "sum";
const char ModelVisitor::kDifferenceOperation[] = "difference";
const char ModelVisitor::kProductOperation[] = "product";
const char ModelVisitor::kStartSyncOnStartOperation[] = "start_synced_on_start";
const char ModelVisitor::kStartSyncOnEndOperation[] = "start_synced_on_end";
const char ModelVisitor::kTraceOperation[] = "trace";

// Methods

ModelVisitor::~ModelVisitor() {}

void ModelVisitor::BeginVisitModel(
    [[maybe_unused]] const std::string& type_name) {}
void ModelVisitor::EndVisitModel(
    [[maybe_unused]] const std::string& type_name) {}

void ModelVisitor::BeginVisitConstraint(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const Constraint* constraint) {}
void ModelVisitor::EndVisitConstraint(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const Constraint* constraint) {}

void ModelVisitor::BeginVisitExtension(
    [[maybe_unused]] const std::string& type) {}
void ModelVisitor::EndVisitExtension([[maybe_unused]] const std::string& type) {
}

void ModelVisitor::BeginVisitIntegerExpression(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const IntExpr* expr) {}
void ModelVisitor::EndVisitIntegerExpression(
    [[maybe_unused]] const std::string& type_name,
    [[maybe_unused]] const IntExpr* expr) {}

void ModelVisitor::VisitIntegerVariable([[maybe_unused]] const IntVar* variable,
                                        IntExpr* delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitIntegerVariable(
    [[maybe_unused]] const IntVar* variable,
    [[maybe_unused]] const std::string& operation,
    [[maybe_unused]] int64_t value, IntVar* delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitIntervalVariable(
    [[maybe_unused]] const IntervalVar* variable,
    [[maybe_unused]] const std::string& operation,
    [[maybe_unused]] int64_t value, IntervalVar* delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
}

void ModelVisitor::VisitSequenceVariable(const SequenceVar* variable) {
  for (int i = 0; i < variable->size(); ++i) {
    variable->Interval(i)->Accept(this);
  }
}

void ModelVisitor::VisitIntegerArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] int64_t value) {}

void ModelVisitor::VisitIntegerArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] const std::vector<int64_t>& values) {}

void ModelVisitor::VisitIntegerMatrixArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] const IntTupleSet& tuples) {}

void ModelVisitor::VisitIntegerExpressionArgument(
    [[maybe_unused]] const std::string& arg_name, IntExpr* argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntegerVariableEvaluatorArgument(
    [[maybe_unused]] const std::string& arg_name,
    [[maybe_unused]] const Solver::Int64ToIntVar& arguments) {}

void ModelVisitor::VisitIntegerVariableArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    const std::vector<IntVar*>& arguments) {
  ForAll(arguments, &IntVar::Accept, this);
}

void ModelVisitor::VisitIntervalArgument(
    [[maybe_unused]] const std::string& arg_name, IntervalVar* argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitIntervalArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    const std::vector<IntervalVar*>& arguments) {
  ForAll(arguments, &IntervalVar::Accept, this);
}

void ModelVisitor::VisitSequenceArgument(
    [[maybe_unused]] const std::string& arg_name, SequenceVar* argument) {
  argument->Accept(this);
}

void ModelVisitor::VisitSequenceArrayArgument(
    [[maybe_unused]] const std::string& arg_name,
    const std::vector<SequenceVar*>& arguments) {
  ForAll(arguments, &SequenceVar::Accept, this);
}

// ----- Helpers -----

void ModelVisitor::VisitInt64ToBoolExtension(Solver::IndexFilter1 filter,
                                             int64_t index_min,
                                             int64_t index_max) {
  if (filter != nullptr) {
    std::vector<int64_t> cached_results;
    for (int i = index_min; i <= index_max; ++i) {
      cached_results.push_back(filter(i));
    }
    BeginVisitExtension(kInt64ToBoolExtension);
    VisitIntegerArgument(kMinArgument, index_min);
    VisitIntegerArgument(kMaxArgument, index_max);
    VisitIntegerArrayArgument(kValuesArgument, cached_results);
    EndVisitExtension(kInt64ToBoolExtension);
  }
}

void ModelVisitor::VisitInt64ToInt64Extension(
    const Solver::IndexEvaluator1& eval, int64_t index_min, int64_t index_max) {
  CHECK(eval != nullptr);
  std::vector<int64_t> cached_results;
  for (int i = index_min; i <= index_max; ++i) {
    cached_results.push_back(eval(i));
  }
  BeginVisitExtension(kInt64ToInt64Extension);
  VisitIntegerArgument(kMinArgument, index_min);
  VisitIntegerArgument(kMaxArgument, index_max);
  VisitIntegerArrayArgument(kValuesArgument, cached_results);
  EndVisitExtension(kInt64ToInt64Extension);
}

void ModelVisitor::VisitInt64ToInt64AsArray(const Solver::IndexEvaluator1& eval,
                                            const std::string& arg_name,
                                            int64_t index_max) {
  CHECK(eval != nullptr);
  std::vector<int64_t> cached_results;
  for (int i = 0; i <= index_max; ++i) {
    cached_results.push_back(eval(i));
  }
  VisitIntegerArrayArgument(arg_name, cached_results);
}

// ---------- ArgumentHolder ----------

const std::string& ArgumentHolder::TypeName() const { return type_name_; }

void ArgumentHolder::SetTypeName(const std::string& type_name) {
  type_name_ = type_name;
}

void ArgumentHolder::SetIntegerArgument(const std::string& arg_name,
                                        int64_t value) {
  integer_argument_[arg_name] = value;
}

void ArgumentHolder::SetIntegerArrayArgument(
    const std::string& arg_name, const std::vector<int64_t>& values) {
  integer_array_argument_[arg_name] = values;
}

void ArgumentHolder::SetIntegerMatrixArgument(const std::string& arg_name,
                                              const IntTupleSet& values) {
  matrix_argument_.insert(std::make_pair(arg_name, values));
}

void ArgumentHolder::SetIntegerExpressionArgument(const std::string& arg_name,
                                                  IntExpr* expr) {
  integer_expression_argument_[arg_name] = expr;
}

void ArgumentHolder::SetIntegerVariableArrayArgument(
    const std::string& arg_name, const std::vector<IntVar*>& vars) {
  integer_variable_array_argument_[arg_name] = vars;
}

void ArgumentHolder::SetIntervalArgument(const std::string& arg_name,
                                         IntervalVar* var) {
  interval_argument_[arg_name] = var;
}

void ArgumentHolder::SetIntervalArrayArgument(
    const std::string& arg_name, const std::vector<IntervalVar*>& vars) {
  interval_array_argument_[arg_name] = vars;
}

void ArgumentHolder::SetSequenceArgument(const std::string& arg_name,
                                         SequenceVar* var) {
  sequence_argument_[arg_name] = var;
}

void ArgumentHolder::SetSequenceArrayArgument(
    const std::string& arg_name, const std::vector<SequenceVar*>& vars) {
  sequence_array_argument_[arg_name] = vars;
}

bool ArgumentHolder::HasIntegerExpressionArgument(
    const std::string& arg_name) const {
  return integer_expression_argument_.contains(arg_name);
}

bool ArgumentHolder::HasIntegerVariableArrayArgument(
    const std::string& arg_name) const {
  return integer_variable_array_argument_.contains(arg_name);
}

int64_t ArgumentHolder::FindIntegerArgumentWithDefault(
    const std::string& arg_name, int64_t def) const {
  return gtl::FindWithDefault(integer_argument_, arg_name, def);
}

int64_t ArgumentHolder::FindIntegerArgumentOrDie(
    const std::string& arg_name) const {
  return gtl::FindOrDie(integer_argument_, arg_name);
}

const std::vector<int64_t>& ArgumentHolder::FindIntegerArrayArgumentOrDie(
    const std::string& arg_name) const {
  return gtl::FindOrDie(integer_array_argument_, arg_name);
}

IntExpr* ArgumentHolder::FindIntegerExpressionArgumentOrDie(
    const std::string& arg_name) const {
  return gtl::FindOrDie(integer_expression_argument_, arg_name);
}

const std::vector<IntVar*>&
ArgumentHolder::FindIntegerVariableArrayArgumentOrDie(
    const std::string& arg_name) const {
  return gtl::FindOrDie(integer_variable_array_argument_, arg_name);
}

const IntTupleSet& ArgumentHolder::FindIntegerMatrixArgumentOrDie(
    const std::string& arg_name) const {
  return gtl::FindOrDie(matrix_argument_, arg_name);
}

// ---------- ModelParser ---------

ModelParser::ModelParser() {}

ModelParser::~ModelParser() { CHECK(holders_.empty()); }

void ModelParser::BeginVisitModel(const std::string&) { PushArgumentHolder(); }

void ModelParser::EndVisitModel(const std::string&) { PopArgumentHolder(); }

void ModelParser::BeginVisitConstraint(const std::string&,
                                       const Constraint* const) {
  PushArgumentHolder();
}

void ModelParser::EndVisitConstraint(const std::string&,
                                     const Constraint* const) {
  // Constraint parsing is usually done here.
  PopArgumentHolder();
}

void ModelParser::BeginVisitIntegerExpression(const std::string&,
                                              const IntExpr* const) {
  PushArgumentHolder();
}

void ModelParser::EndVisitIntegerExpression(const std::string&,
                                            const IntExpr* const) {
  // Expression parsing is usually done here.
  PopArgumentHolder();
}

void ModelParser::VisitIntegerVariable(const IntVar*, IntExpr*) {
  // Usual place for parsing.
}

void ModelParser::VisitIntegerVariable(const IntVar*, const std::string&,
                                       int64_t, IntVar* delegate) {
  delegate->Accept(this);
  // Usual place for parsing.
}

void ModelParser::VisitIntervalVariable(const IntervalVar*, const std::string&,
                                        int64_t, IntervalVar* delegate) {
  if (delegate != nullptr) {
    delegate->Accept(this);
  }
  // Usual place for parsing.
}

void ModelParser::VisitSequenceVariable(const SequenceVar*) {
  // Usual place for parsing.
}

// Integer arguments
void ModelParser::VisitIntegerArgument(const std::string& arg_name,
                                       int64_t value) {
  Top()->SetIntegerArgument(arg_name, value);
}

void ModelParser::VisitIntegerArrayArgument(
    const std::string& arg_name, const std::vector<int64_t>& values) {
  Top()->SetIntegerArrayArgument(arg_name, values);
}

void ModelParser::VisitIntegerMatrixArgument(const std::string& arg_name,
                                             const IntTupleSet& values) {
  Top()->SetIntegerMatrixArgument(arg_name, values);
}

// Variables.
void ModelParser::VisitIntegerExpressionArgument(const std::string& arg_name,
                                                 IntExpr* argument) {
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
                                        IntervalVar* argument) {
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
                                        SequenceVar* argument) {
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

namespace {

// ----- PrintModelVisitor -----

class PrintModelVisitor : public ModelVisitor {
 public:
  PrintModelVisitor() : indent_(0) {}
  ~PrintModelVisitor() override {}

  // Header/footers.
  void BeginVisitModel(const std::string& solver_name) override {
    LOG(INFO) << "Model " << solver_name << " {";
    Increase();
  }

  void EndVisitModel(const std::string&) override {
    LOG(INFO) << "}";
    Decrease();
    CHECK_EQ(0, indent_);
  }

  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint*) override {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  void EndVisitConstraint(const std::string&, const Constraint*) override {
    Decrease();
  }

  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr*) override {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  void EndVisitIntegerExpression(const std::string&, const IntExpr*) override {
    Decrease();
  }

  void BeginVisitExtension(const std::string& type_name) override {
    LOG(INFO) << Spaces() << type_name;
    Increase();
  }

  void EndVisitExtension(const std::string&) override { Decrease(); }

  void VisitIntegerVariable(const IntVar* variable,
                            IntExpr* delegate) override {
    if (delegate != nullptr) {
      delegate->Accept(this);
    } else {
      if (variable->Bound() && variable->name().empty()) {
        LOG(INFO) << Spaces() << variable->Min();
      } else {
        LOG(INFO) << Spaces() << variable->DebugString();
      }
    }
  }

  void VisitIntegerVariable(const IntVar*, const std::string& operation,
                            int64_t value, IntVar* delegate) override {
    LOG(INFO) << Spaces() << "IntVar";
    Increase();
    LOG(INFO) << Spaces() << value;
    LOG(INFO) << Spaces() << operation;
    delegate->Accept(this);
    Decrease();
  }

  void VisitIntervalVariable(const IntervalVar* variable,
                             const std::string& operation, int64_t value,
                             IntervalVar* delegate) override {
    if (delegate != nullptr) {
      LOG(INFO) << Spaces() << operation << " <" << value << ", ";
      Increase();
      delegate->Accept(this);
      Decrease();
      LOG(INFO) << Spaces() << ">";
    } else {
      LOG(INFO) << Spaces() << variable->DebugString();
    }
  }

  void VisitSequenceVariable(const SequenceVar* sequence) override {
    LOG(INFO) << Spaces() << sequence->DebugString();
  }

  // Variables.
  void VisitIntegerArgument(const std::string& arg_name,
                            int64_t value) override {
    LOG(INFO) << Spaces() << arg_name << ": " << value;
  }

  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64_t>& values) override {
    LOG(INFO) << Spaces() << arg_name << ": [" << absl::StrJoin(values, ", ")
              << "]";
  }

  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override {
    const int rows = values.NumTuples();
    const int columns = values.Arity();
    std::string array = "[";
    for (int i = 0; i < rows; ++i) {
      if (i != 0) {
        array.append(", ");
      }
      array.append("[");
      for (int j = 0; j < columns; ++j) {
        if (j != 0) {
          array.append(", ");
        }
        absl::StrAppendFormat(&array, "%d", values.Value(i, j));
      }
      array.append("]");
    }
    array.append("]");
    LOG(INFO) << Spaces() << arg_name << ": " << array;
  }

  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* argument) override {
    set_prefix(absl::StrFormat("%s: ", arg_name));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name,
      const std::vector<IntVar*>& arguments) override {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* argument) override {
    set_prefix(absl::StrFormat("%s: ", arg_name));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  virtual void VisitIntervalArgumentArray(
      const std::string& arg_name, const std::vector<IntervalVar*>& arguments) {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  // Visit sequence argument.
  void VisitSequenceArgument(const std::string& arg_name,
                             SequenceVar* argument) override {
    set_prefix(absl::StrFormat("%s: ", arg_name));
    Increase();
    argument->Accept(this);
    Decrease();
  }

  virtual void VisitSequenceArgumentArray(
      const std::string& arg_name, const std::vector<SequenceVar*>& arguments) {
    LOG(INFO) << Spaces() << arg_name << ": [";
    Increase();
    for (int i = 0; i < arguments.size(); ++i) {
      arguments[i]->Accept(this);
    }
    Decrease();
    LOG(INFO) << Spaces() << "]";
  }

  std::string DebugString() const override { return "PrintModelVisitor"; }

 private:
  void Increase() { indent_ += 2; }

  void Decrease() { indent_ -= 2; }

  std::string Spaces() {
    std::string result;
    for (int i = 0; i < indent_ - 2 * (!prefix_.empty()); ++i) {
      result.append(" ");
    }
    if (!prefix_.empty()) {
      result.append(prefix_);
      prefix_ = "";
    }
    return result;
  }

  void set_prefix(absl::string_view prefix) { prefix_ = prefix; }

  int indent_;
  std::string prefix_;
};

// ---------- ModelStatisticsVisitor -----------

class ModelStatisticsVisitor : public ModelVisitor {
 public:
  ModelStatisticsVisitor()
      : num_constraints_(0),
        num_variables_(0),
        num_expressions_(0),
        num_casts_(0),
        num_intervals_(0),
        num_sequences_(0),
        num_extensions_(0) {}

  ~ModelStatisticsVisitor() override {}

  // Begin/End visit element.
  void BeginVisitModel(const std::string&) override {
    // Reset statistics.
    num_constraints_ = 0;
    num_variables_ = 0;
    num_expressions_ = 0;
    num_casts_ = 0;
    num_intervals_ = 0;
    num_sequences_ = 0;
    num_extensions_ = 0;
    already_visited_.clear();
    constraint_types_.clear();
    expression_types_.clear();
    extension_types_.clear();
  }

  void EndVisitModel(const std::string&) override {
    // Display statistics.
    LOG(INFO) << "Model has:";
    LOG(INFO) << "  - " << num_constraints_ << " constraints.";
    for (const auto& it : constraint_types_) {
      LOG(INFO) << "    * " << it.second << " " << it.first;
    }
    LOG(INFO) << "  - " << num_variables_ << " integer variables.";
    LOG(INFO) << "  - " << num_expressions_ << " integer expressions.";
    for (const auto& it : expression_types_) {
      LOG(INFO) << "    * " << it.second << " " << it.first;
    }
    LOG(INFO) << "  - " << num_casts_ << " expressions casted into variables.";
    LOG(INFO) << "  - " << num_intervals_ << " interval variables.";
    LOG(INFO) << "  - " << num_sequences_ << " sequence variables.";
    LOG(INFO) << "  - " << num_extensions_ << " model extensions.";
    for (const auto& it : extension_types_) {
      LOG(INFO) << "    * " << it.second << " " << it.first;
    }
  }

  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint*) override {
    num_constraints_++;
    AddConstraintType(type_name);
  }

  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr*) override {
    AddExpressionType(type_name);
    num_expressions_++;
  }

  void BeginVisitExtension(const std::string& type_name) override {
    AddExtensionType(type_name);
    num_extensions_++;
  }

  void VisitIntegerVariable(const IntVar* variable,
                            IntExpr* delegate) override {
    num_variables_++;
    Register(variable);
    if (delegate) {
      num_casts_++;
      VisitSubArgument(delegate);
    }
  }

  void VisitIntegerVariable(const IntVar* variable, const std::string&, int64_t,
                            IntVar* delegate) override {
    num_variables_++;
    Register(variable);
    num_casts_++;
    VisitSubArgument(delegate);
  }

  void VisitIntervalVariable(const IntervalVar*, const std::string&, int64_t,
                             IntervalVar* delegate) override {
    num_intervals_++;
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  void VisitSequenceVariable(const SequenceVar* sequence) override {
    num_sequences_++;
    for (int i = 0; i < sequence->size(); ++i) {
      VisitSubArgument(sequence->Interval(i));
    }
  }

  // Visit integer expression argument.
  void VisitIntegerExpressionArgument(const std::string&,
                                      IntExpr* argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntegerVariableArrayArgument(
      const std::string&, const std::vector<IntVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string&,
                             IntervalVar* argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntervalArrayArgument(
      const std::string&, const std::vector<IntervalVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit sequence argument.
  void VisitSequenceArgument(const std::string&,
                             SequenceVar* argument) override {
    VisitSubArgument(argument);
  }

  void VisitSequenceArrayArgument(
      const std::string&, const std::vector<SequenceVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  std::string DebugString() const override { return "ModelStatisticsVisitor"; }

 private:
  void Register(const BaseObject* object) { already_visited_.insert(object); }

  bool AlreadyVisited(const BaseObject* object) {
    return already_visited_.contains(object);
  }

  // T should derive from BaseObject
  template <typename T>
  void VisitSubArgument(T* object) {
    if (!AlreadyVisited(object)) {
      Register(object);
      object->Accept(this);
    }
  }

  void AddConstraintType(absl::string_view constraint_type) {
    constraint_types_[constraint_type]++;
  }

  void AddExpressionType(absl::string_view expression_type) {
    expression_types_[expression_type]++;
  }

  void AddExtensionType(absl::string_view extension_type) {
    extension_types_[extension_type]++;
  }

  absl::flat_hash_map<std::string, int> constraint_types_;
  absl::flat_hash_map<std::string, int> expression_types_;
  absl::flat_hash_map<std::string, int> extension_types_;
  int num_constraints_;
  int num_variables_;
  int num_expressions_;
  int num_casts_;
  int num_intervals_;
  int num_sequences_;
  int num_extensions_;
  absl::flat_hash_set<const BaseObject*> already_visited_;
};

// ---------- Variable Degree Visitor ---------

class VariableDegreeVisitor : public ModelVisitor {
 public:
  explicit VariableDegreeVisitor(absl::flat_hash_map<const IntVar*, int>* map)
      : map_(map) {}

  ~VariableDegreeVisitor() override {}

  // Begin/End visit element.
  void VisitIntegerVariable(const IntVar* variable,
                            IntExpr* delegate) override {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (map_->contains(var)) {
      (*map_)[var]++;
    }
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  void VisitIntegerVariable(const IntVar* variable, const std::string&, int64_t,
                            IntVar* delegate) override {
    IntVar* const var = const_cast<IntVar*>(variable);
    if (map_->contains(var)) {
      (*map_)[var]++;
    }
    VisitSubArgument(delegate);
  }

  void VisitIntervalVariable(const IntervalVar*, const std::string&, int64_t,
                             IntervalVar* delegate) override {
    if (delegate) {
      VisitSubArgument(delegate);
    }
  }

  void VisitSequenceVariable(const SequenceVar* sequence) override {
    for (int i = 0; i < sequence->size(); ++i) {
      VisitSubArgument(sequence->Interval(i));
    }
  }

  // Visit integer expression argument.
  void VisitIntegerExpressionArgument(const std::string&,
                                      IntExpr* argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntegerVariableArrayArgument(
      const std::string&, const std::vector<IntVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string&,
                             IntervalVar* argument) override {
    VisitSubArgument(argument);
  }

  void VisitIntervalArrayArgument(
      const std::string&, const std::vector<IntervalVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  // Visit sequence argument.
  void VisitSequenceArgument(const std::string&,
                             SequenceVar* argument) override {
    VisitSubArgument(argument);
  }

  void VisitSequenceArrayArgument(
      const std::string&, const std::vector<SequenceVar*>& arguments) override {
    for (int i = 0; i < arguments.size(); ++i) {
      VisitSubArgument(arguments[i]);
    }
  }

  std::string DebugString() const override { return "VariableDegreeVisitor"; }

 private:
  // T should derive from BaseObject
  template <typename T>
  void VisitSubArgument(T* object) {
    object->Accept(this);
  }

  absl::flat_hash_map<const IntVar*, int>* const map_;
};
}  // namespace

ModelVisitor* Solver::MakePrintModelVisitor() {
  return RevAlloc(new PrintModelVisitor);
}

ModelVisitor* Solver::MakeStatisticsModelVisitor() {
  return RevAlloc(new ModelStatisticsVisitor);
}

ModelVisitor* Solver::MakeVariableDegreeVisitor(
    absl::flat_hash_map<const IntVar*, int>* map) {
  return RevAlloc(new VariableDegreeVisitor(map));
}

}  // namespace operations_research
