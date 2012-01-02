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


#include <math.h>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/hash.h"
#include "base/integral_types.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solver.h"
#include "linear_solver/linear_solver.h"
#include "util/string_array.h"

DEFINE_int32(simplex_cleanup_frequency, 0,
             "frequency to cleanup the simplex after each call, 0: no cleanup");
DEFINE_bool(verbose_simplex_call, false,
            "Do not suppress output of the simplex");
DEFINE_bool(use_clp, true, "use Clp instead of glpk");

namespace operations_research {
namespace {
MPSolver::OptimizationProblemType GetType(bool use_clp) {
  if (use_clp) {
#if defined(USE_CLP)
    return MPSolver::CLP_LINEAR_PROGRAMMING;
#else
    LOG(FATAL) << "CLP not defined";
#endif  // USE_CLP
  } else {
#if defined(USE_GLPK)
    return MPSolver::GLPK_LINEAR_PROGRAMMING;
#else
    LOG(FATAL) << "GLPK not defined";
#endif  // USE_GLPK
  }
}

class SimplexConnection : public SearchMonitor {
 public:
  SimplexConnection(Solver* const solver,
                    Callback1<MPSolver*>* const builder,
                    Callback1<MPSolver*>* const modifier,
                    Callback1<MPSolver*>* const runner,
                    int simplex_frequency)
      : SearchMonitor(solver),
        builder_(builder),
        modifier_(modifier),
        runner_(runner),
        mp_solver_("InSearchSimplex", GetType(FLAGS_use_clp)),
        counter_(0LL),
        simplex_frequency_(simplex_frequency) {
    if (builder != NULL) {
      builder->CheckIsRepeatable();
    }
    if (modifier != NULL) {
      modifier->CheckIsRepeatable();
    }
    if (runner != NULL) {
      runner->CheckIsRepeatable();
    }
    if (!FLAGS_verbose_simplex_call) {
      mp_solver_.SuppressOutput();
    }
  }

  virtual void EndInitialPropagation() {
    mp_solver_.Clear();
    if (builder_.get() != NULL) {
      builder_->Run(&mp_solver_);
    }
    RunOptim();
  }

  virtual void BeginNextDecision(DecisionBuilder* const b) {
    if (++counter_ % simplex_frequency_ == 0) {
      const int cleanup = FLAGS_simplex_cleanup_frequency * simplex_frequency_;
      if (FLAGS_simplex_cleanup_frequency != 0 && counter_ %  cleanup == 0) {
        mp_solver_.Clear();
        if (builder_.get() != NULL) {
          builder_->Run(&mp_solver_);
        }
      }
      RunOptim();
    }
  }

  void RunOptim() {
    if (modifier_.get() != NULL) {
      modifier_->Run(&mp_solver_);
    }
    if (runner_.get() != NULL) {
      runner_->Run(&mp_solver_);
    }
  }

 private:
  scoped_ptr<Callback1<MPSolver*> > builder_;
  scoped_ptr<Callback1<MPSolver*> > modifier_;
  scoped_ptr<Callback1<MPSolver*> > runner_;
  MPSolver mp_solver_;
  int64 counter_;
  const int simplex_frequency_;
  DISALLOW_COPY_AND_ASSIGN(SimplexConnection);
};

// ---------- Automatic Linearization ----------

// ----- Useful typedefs -----

typedef hash_map<const IntExpr*, MPVariable*> ExprTranslation;

// ----- Context to represent a linear sum -----

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

  void set_integer_argument(const string& arg_name, int64 value) {
    integer_argument_[arg_name] = value;
  }

  void set_integer_array_argument(const string& arg_name,
                                  const int64* const values,
                                  int size) {
    for (int i = 0; i < size; ++i) {
      integer_array_argument_[arg_name].push_back(values[i]);
    }
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

  int64 FindIntegerArgumentOrDie(const string& arg_name) {
    return FindOrDie(integer_argument_, arg_name);
  }

  const std::vector<int64>& FindIntegerArrayArgumentOrDie(
      const string& arg_name) {
    return FindOrDie(integer_array_argument_, arg_name);
  }

  const Matrix& FindIntegerMatrixArgumentOrDie(const string& arg_name) {
    return FindOrDie(matrix_argument_, arg_name);
  }

  bool HasIntegerExpression(const string& arg_name) {
    return ContainsKey(integer_expression_argument_, arg_name);
  }

  bool HasIntegerVariableArray(const string& arg_name) {
    return ContainsKey(integer_variable_array_argument_, arg_name);
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
  hash_map<string, int64> integer_argument_;
  hash_map<string, std::vector<int64> > integer_array_argument_;
  hash_map<string, Matrix> matrix_argument_;
};

#define IS_TYPE(type, tag) type.compare(ModelVisitor::tag) == 0

// ----- Actual Linearization -----

class Linearizer : public ModelVisitor {
 public:
  Linearizer(MPSolver* const mp_solver,
             ExprTranslation* tr,
             IntVar** objective,
             bool* maximize)
      : mp_solver_(mp_solver),
        translation_(tr),
        objective_(objective),
        maximize_(maximize) {}

  virtual ~Linearizer() {}

  // Begin/End visit element.
  virtual void BeginVisitModel(const string& solver_name) {
    BeginVisit(true);
  }

  virtual void EndVisitModel(const string& solver_name) {
    EndVisit();
  }

  virtual void BeginVisitConstraint(const string& type_name,
                                    const Constraint* const constraint) {
    if (!constraint->IsCastConstraint() &&
        (IS_TYPE(type_name, kEquality) ||
         IS_TYPE(type_name, kLessOrEqual) ||
         IS_TYPE(type_name, kGreaterOrEqual) ||
         IS_TYPE(type_name, kScalProdLessOrEqual))) {
      BeginVisit(true);
    } else {
      BeginVisit(false);
    }
  }

  virtual void EndVisitConstraint(const string& type_name,
                                  const Constraint* const constraint) {
    if (!constraint->IsCastConstraint()) {
      if (IS_TYPE(type_name, kEquality)) {
        VisitEquality();
      } else if (IS_TYPE(type_name, kLessOrEqual)) {
        VisitLessOrEqual();
      } else if (IS_TYPE(type_name, kGreaterOrEqual)) {
        VisitGreaterOrEqual();
      } else if (IS_TYPE(type_name, kScalProdLessOrEqual)) {
        VisitScalProdLessOrEqual();
      }
    }
    EndVisit();
  }
  virtual void BeginVisitExtension(const string& type) {
    BeginVisit(true);
  }
  virtual void EndVisitExtension(const string& type) {
    if (IS_TYPE(type, kObjectiveExtension)) {
      VisitObjective();
    }
    EndVisit();
  }
  virtual void BeginVisitIntegerExpression(const string& type_name,
                                           const IntExpr* const expr) {
    BeginVisit(true);
  }
  virtual void EndVisitIntegerExpression(const string& type_name,
                                         const IntExpr* const expr) {
    if (IS_TYPE(type_name, kSum)) {
      VisitSum(expr);
    } else if (IS_TYPE(type_name, kScalProd)) {
      VisitScalProd(expr);
    } else if (IS_TYPE(type_name, kDifference)) {
      VisitDifference(expr);
    } else if (IS_TYPE(type_name, kOpposite)) {
      VisitOpposite(expr);
    } else if (IS_TYPE(type_name, kProduct)) {
      VisitProduct(expr);
    } else {
      VisitIntegerExpression(expr);
    }
    EndVisit();
  }

  virtual void VisitIntegerVariable(const IntVar* const variable,
                                    const IntExpr* const delegate) {
    RegisterExpression(const_cast<IntVar*>(variable));
    if (delegate != NULL) {
      VisitSubExpression(delegate);
      AddMpEquality(const_cast<IntVar*>(variable),
                    const_cast<IntExpr*>(delegate));
    }
  }

  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const string& operation,
                                     const IntervalVar* const delegate) {}

  virtual void VisitIntervalVariable(const IntervalVar* const variable,
                                     const string& operation,
                                     const IntervalVar* const * delegate,
                                     int size) {}

  // Visit integer arguments.
  virtual void VisitIntegerArgument(const string& arg_name, int64 value) {
    if (DoVisit()) {
      top()->set_integer_argument(arg_name, value);
    }
  }

  virtual void VisitIntegerArrayArgument(const string& arg_name,
                                         const int64* const values,
                                         int size) {
    if (DoVisit()) {
      top()->set_integer_array_argument(arg_name, values, size);
    }
  }

  virtual void VisitIntegerMatrixArgument(const string& arg_name,
                                          const int64* const * const values,
                                          int rows,
                                          int columns) {
    if (DoVisit()) {
      top()->set_integer_matrix_argument(arg_name, values, rows, columns);
    }
  }

  // Visit integer expression argument.
  virtual void VisitIntegerExpressionArgument(
      const string& arg_name,
      const IntExpr* const argument) {
    if (DoVisit()) {
      top()->set_integer_expression_argument(arg_name, argument);
      VisitSubExpression(argument);
    }
  }

  virtual void VisitIntegerVariableArrayArgument(
      const string& arg_name,
      const IntVar* const * arguments,
      int size) {
    if (DoVisit()) {
      top()->set_integer_variable_array_argument(arg_name, arguments, size);
      for (int i = 0; i < size; ++i) {
        VisitSubExpression(arguments[i]);
      }
    }
  }

  // Visit interval argument.
  virtual void VisitIntervalArgument(const string& arg_name,
                                     const IntervalVar* const argument) {}

  virtual void VisitIntervalArrayArgument(const string& arg_name,
                                          const IntervalVar* const * argument,
                                          int size) {}

 private:
  void BeginVisit(bool active) {
    PushActive(active);
    PushArgumentHolder();
  }

  void EndVisit() {
    PopArgumentHolder();
    PopActive();
  }

  bool DoVisit() const {
    return actives_.back();
  }

  void PushActive(bool active) {
    actives_.push_back(active);
  }

  void PopActive() {
    actives_.pop_back();
  }

  void PushArgumentHolder() {
    holders_.push_back(new ArgumentHolder);
  }

  void PopArgumentHolder() {
    CHECK(!holders_.empty());
    delete holders_.back();
    holders_.pop_back();
    STLDeleteElements(&extensions_);
    extensions_.clear();
  }

  void PushExtension(const string& type_name) {
    PushActive(true);
    PushArgumentHolder();
    holders_.back()->set_type_name(type_name);
  }

  void PopAndSaveExtension() {
    CHECK(!holders_.empty());
    extensions_.push_back(holders_.back());
    holders_.pop_back();
    PopActive();
  }

  ArgumentHolder* top() const {
    CHECK(!holders_.empty());
    return holders_.back();
  }

  void RegisterExpression(const IntExpr* const cp_expr) {
    if (!ContainsKey(*translation_, cp_expr)) {
      MPVariable* const mp_var = mp_solver_->MakeIntVar(cp_expr->Min(),
                                                        cp_expr->Max(),
                                                        "");
      CHECK(cp_expr->Min() != cp_expr->Max());
      (*translation_)[cp_expr] = mp_var;
    }
  }

  void VisitSubExpression(const IntExpr* const cp_expr) {
    if (!ContainsKey(*translation_, cp_expr)) {
      cp_expr->Accept(this);
    }
  }

  void AddMpEquality(IntExpr* const left, IntExpr* const right) {
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(0, 0);
    ct->SetCoefficient(Translated(left), 1);
    ct->SetCoefficient(Translated(right), -1);
  }

  MPVariable* Translated(const IntExpr* const cp_var) {
    return FindOrDie((*translation_), cp_var);
  }

  void VisitEquality() {
    if (top()->HasIntegerExpression(ModelVisitor::kLeftArgument)) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
      const IntExpr* const left =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kLeftArgument);
      const IntExpr* const right =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kRightArgument);
      ct->SetCoefficient(Translated(left), 1.0);
      ct->SetCoefficient(Translated(right), -1.0);
    } else {
      const IntExpr* const expr =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kExpressionArgument);
      const int64 value =
          top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(value, value);
      ct->SetCoefficient(Translated(expr), 1.0);
    }
  }

  void VisitLessOrEqual() {
    if (top()->HasIntegerExpression(ModelVisitor::kLeftArgument)) {
      MPConstraint* const ct =
          mp_solver_->MakeRowConstraint(-mp_solver_->infinity(), 0.0);
      const IntExpr* const left =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kLeftArgument);
      const IntExpr* const right =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kRightArgument);
      ct->SetCoefficient(Translated(left), 1.0);
      ct->SetCoefficient(Translated(right), -1.0);
    } else {
      const IntExpr* const expr =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kExpressionArgument);
      const int64 value =
          top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct =
          mp_solver_->MakeRowConstraint(-mp_solver_->infinity(), value);
      ct->SetCoefficient(Translated(expr), 1.0);
    }
  }

  void VisitGreaterOrEqual() {
    if (top()->HasIntegerExpression(ModelVisitor::kLeftArgument)) {
      MPConstraint* const ct =
          mp_solver_->MakeRowConstraint(0.0, mp_solver_->infinity());
      const IntExpr* const left =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kLeftArgument);
      const IntExpr* const right =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kRightArgument);
      ct->SetCoefficient(Translated(left), 1.0);
      ct->SetCoefficient(Translated(right), -1.0);
    } else {
      const IntExpr* const expr =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kExpressionArgument);
      const int64 value =
          top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct =
          mp_solver_->MakeRowConstraint(value, mp_solver_->infinity());
      ct->SetCoefficient(Translated(expr), 1.0);
    }
  }

  void VisitScalProdLessOrEqual() {
    const std::vector<const IntVar*>& cp_vars =
        top()->FindIntegerVariableArrayArgumentOrDie(
            ModelVisitor::kVarsArgument);
    const std::vector<int64>& cp_coefficients =
        top()->FindIntegerArrayArgumentOrDie(
            ModelVisitor::kCoefficientsArgument);
    const int64 constant =
        top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
    MPConstraint* const ct =
        mp_solver_->MakeRowConstraint(-mp_solver_->infinity(),
                                      constant);
    CHECK_EQ(cp_vars.size(), cp_coefficients.size());
    for (int i = 0; i < cp_coefficients.size(); ++i) {
      MPVariable* const mp_var = Translated(cp_vars[i]);
      ct->SetCoefficient(mp_var,
                         cp_coefficients[i] + ct->GetCoefficient(mp_var));
    }
  }

  void VisitSum(const IntExpr* const cp_expr) {
    if (top()->HasIntegerVariableArray(ModelVisitor::kVarsArgument)) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0, 0);
      const std::vector<const IntVar*>& cp_vars =
          top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kVarsArgument);
      for (int i = 0; i < cp_vars.size(); ++i) {
        MPVariable* const mp_var = Translated(cp_vars[i]);
        ct->SetCoefficient(mp_var, 1.0 + ct->GetCoefficient(mp_var));
      }
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1);
    } else if (top()->HasIntegerExpression(ModelVisitor::kLeftArgument)) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0, 0);
      const IntExpr* const left =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kLeftArgument);
      const IntExpr* const right =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kRightArgument);
      if (left != right) {
        ct->SetCoefficient(Translated(left), 1.0);
        ct->SetCoefficient(Translated(right), 1.0);
      } else {
        ct->SetCoefficient(Translated(left), 2.0);
      }
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1);
    } else {
      const IntExpr* const expr =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kExpressionArgument);
      const int64 value =
          top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(-value, -value);
      ct->SetCoefficient(Translated(expr), 1.0);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1);
    }
  }

  void VisitScalProd(const IntExpr* const cp_expr) {
    const std::vector<const IntVar*>& cp_vars =
        top()->FindIntegerVariableArrayArgumentOrDie(
            ModelVisitor::kVarsArgument);
    const std::vector<int64>& cp_coefficients =
        top()->FindIntegerArrayArgumentOrDie(
            ModelVisitor::kCoefficientsArgument);
    CHECK_EQ(cp_vars.size(), cp_coefficients.size());
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(0, 0);
    for (int i = 0; i < cp_vars.size(); ++i) {
      MPVariable* const mp_var = Translated(cp_vars[i]);
      const int64 coefficient = cp_coefficients[i];
      ct->SetCoefficient(mp_var, coefficient + ct->GetCoefficient(mp_var));
    }
    RegisterExpression(cp_expr);
    ct->SetCoefficient(Translated(cp_expr), -1);
  }

  void VisitDifference(const IntExpr* const cp_expr) {
    if (top()->HasIntegerExpression(ModelVisitor::kLeftArgument)) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0, 0);
      const IntExpr* const left =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kLeftArgument);
      const IntExpr* const right =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kRightArgument);
      ct->SetCoefficient(Translated(left), 1.0);
      ct->SetCoefficient(Translated(right), -1.0);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1);
    } else {
      const IntExpr* const expr =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kExpressionArgument);
      const int64 value =
          top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(value, value);
      ct->SetCoefficient(Translated(expr), 1.0);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), 1);
    }
  }

  void VisitOpposite(const IntExpr* const cp_expr) {
    const IntExpr* const expr =
        top()->FindIntegerExpressionArgumentOrDie(
            ModelVisitor::kExpressionArgument);
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
    ct->SetCoefficient(Translated(expr), 1.0);
    RegisterExpression(cp_expr);
    ct->SetCoefficient(Translated(cp_expr), -1);
  }

  void VisitProduct(const IntExpr* const cp_expr) {
    if (top()->HasIntegerExpression(ModelVisitor::kExpressionArgument)) {
      const IntExpr* const expr =
          top()->FindIntegerExpressionArgumentOrDie(
              ModelVisitor::kExpressionArgument);
      const int64 value =
          top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0, 0);
      ct->SetCoefficient(Translated(expr), value);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1);
    } else {
      RegisterExpression(cp_expr);
    }
  }

  void VisitIntegerExpression(const IntExpr* const cp_expr) {
    RegisterExpression(cp_expr);
  }

  void VisitObjective() {
    *maximize_ =
        top()->FindIntegerArgumentOrDie(ModelVisitor::kMaximizeArgument);
    *objective_ =
        const_cast<IntExpr*>(top()->FindIntegerExpressionArgumentOrDie(
            ModelVisitor::kExpressionArgument))->Var();
    mp_solver_->SetObjectiveCoefficient(Translated(*objective_), 1.0);
    mp_solver_->SetOptimizationDirection(*maximize_);
  }

  MPSolver* const mp_solver_;
  ExprTranslation* const translation_;
  IntVar** objective_;
  bool* maximize_;
  std::vector<ArgumentHolder*> holders_;
  std::vector<ArgumentHolder*> extensions_;
  std::vector<bool> actives_;
};

// ----- Search Monitor -----

class AutomaticLinearization : public SearchMonitor {
 public:
  AutomaticLinearization(Solver* const solver, int frequency)
    : SearchMonitor(solver),
      mp_solver_("InSearchSimplex", MPSolver::CLP_LINEAR_PROGRAMMING),
      counter_(0),
      simplex_frequency_(frequency),
      objective_(NULL),
      maximize_(false) {}

  virtual ~AutomaticLinearization() {}

  virtual void BeginInitialPropagation() {
    mp_solver_.Clear();
    translation_.clear();
    BuildModel();
  }

  virtual void EndInitialPropagation() {
    RunOptim();
  }

  virtual void BeginNextDecision(DecisionBuilder* const b) {
    if (++counter_ % simplex_frequency_ == 0) {
      RunOptim();
    }
  }

  void RunOptim() {
    AssignVariables();
    SolveProblem();
  }

  void BuildModel() {
    Linearizer linearizer(&mp_solver_, &translation_, &objective_, &maximize_);
    solver()->Accept(&linearizer);
  }

  void AssignVariables() {
    for (ConstIter<ExprTranslation> it(translation_); !it.at_end(); ++it) {
      it->second->SetBounds(it->first->Min(), it->first->Max());
    }
  }

  void SolveProblem() {
    if (objective_ != NULL) {
      switch (mp_solver_.Solve()) {
        case MPSolver::OPTIMAL: {
          const double obj_value = mp_solver_.objective_value();
          if (maximize_) {
            const int64 int_obj_value = static_cast<int64>(ceil(obj_value));
            objective_->SetMax(int_obj_value);
          } else {
            const int64 int_obj_value = static_cast<int64>(floor(obj_value));
            objective_->SetMin(int_obj_value);
          }
          break;
        }
        case MPSolver::FEASIBLE:
          break;
        case MPSolver::INFEASIBLE:
          solver()->Fail();
          break;
        case MPSolver::UNBOUNDED:
          LOG(INFO) << "Error: unbounded LP status.";
          break;
        case MPSolver::ABNORMAL:
          LOG(INFO) << "Error: abnormal LP status.";
          break;
        default:
          LOG(FATAL) << "Error: Unknown LP status.";
          break;
      }
    }
  }

 private:
  MPSolver mp_solver_;
  int64 counter_;
  const int simplex_frequency_;
  ExprTranslation translation_;
  IntVar* objective_;
  bool maximize_;
};


}  // namespace

// ---------- API ----------

SearchMonitor* Solver::MakeSimplexConnection(
    Callback1<MPSolver*>* const builder,
    Callback1<MPSolver*>* const modifier,
    Callback1<MPSolver*>* const runner,
    int frequency) {
  return RevAlloc(new SimplexConnection(this,
                                        builder,
                                        modifier,
                                        runner,
                                        frequency));
}

SearchMonitor* Solver::MakeSimplexConstraint(int frequency) {
  return RevAlloc(new AutomaticLinearization(this, frequency));
}
}  // namespace operations_research
