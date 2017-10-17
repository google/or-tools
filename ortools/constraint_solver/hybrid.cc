// Copyright 2010-2017 Google
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


#include "ortools/constraint_solver/hybrid.h"

#include <math.h>
#include <functional>
#include <unordered_map>
#include <memory>
#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/base/stl_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/util/string_array.h"

DEFINE_int32(simplex_cleanup_frequency, 0,
             "frequency to cleanup the simplex after each call, 0: no cleanup");
DEFINE_bool(verbose_simplex_call, false,
            "Do not suppress output of the simplex");

namespace operations_research {
namespace {
inline MPSolver::OptimizationProblemType LpSolverForCp() {
  #if defined(USE_GUROBI)
  return MPSolver::GUROBI_LINEAR_PROGRAMMING;
  #elif defined(USE_CLP)
  return MPSolver::CLP_LINEAR_PROGRAMMING;
  #elif defined(USE_GLPK)
  return MPSolver::CLP_LINEAR_PROGRAMMING;
  #elif defined(USE_SULUM)
  return MPSolver::SULUM_LINEAR_PROGRAMMING;
  #else
  LOG(FATAL) << "No suitable linear programming solver available.";
  #endif
}

class SimplexConnection : public SearchMonitor {
 public:
  SimplexConnection(Solver* const solver,
                    std::function<void(MPSolver*)> builder,
                    std::function<void(MPSolver*)> modifier,
                    std::function<void(MPSolver*)> runner,
                    int simplex_frequency)
      : SearchMonitor(solver),
        builder_(std::move(builder)),
        modifier_(std::move(modifier)),
        runner_(std::move(runner)),
        mp_solver_("InSearchSimplex", LpSolverForCp()),
        counter_(0LL),
        simplex_frequency_(simplex_frequency) {
    if (!FLAGS_verbose_simplex_call) {
      mp_solver_.SuppressOutput();
    }
  }

  void EndInitialPropagation() override {
    mp_solver_.Clear();
    if (builder_) {
      builder_(&mp_solver_);
    }
    RunOptim();
  }

  void BeginNextDecision(DecisionBuilder* const b) override {
    if (++counter_ % simplex_frequency_ == 0) {
      const int cleanup = FLAGS_simplex_cleanup_frequency * simplex_frequency_;
      if (FLAGS_simplex_cleanup_frequency != 0 && counter_ % cleanup == 0) {
        mp_solver_.Clear();
        if (builder_) {
          builder_(&mp_solver_);
        }
      }
      RunOptim();
    }
  }

  void RunOptim() {
    if (modifier_) {
      modifier_(&mp_solver_);
    }
    if (runner_) {
      runner_(&mp_solver_);
    }
  }

  std::string DebugString() const override { return "SimplexConnection"; }

 private:
  std::function<void(MPSolver*)> builder_;
  std::function<void(MPSolver*)> modifier_;
  std::function<void(MPSolver*)> runner_;
  MPSolver mp_solver_;
  int64 counter_;
  const int simplex_frequency_;
  DISALLOW_COPY_AND_ASSIGN(SimplexConnection);
};

// ---------- Automatic Linearization ----------

// ----- Useful typedefs -----

typedef std::unordered_map<const IntExpr*, MPVariable*> ExprTranslation;

#define IS_TYPE(type, tag) type.compare(ModelVisitor::tag) == 0

// ----- Actual Linearization -----

class Linearizer : public ModelParser {
 public:
  Linearizer(MPSolver* const mp_solver, ExprTranslation* tr, IntVar** objective,
             bool* maximize)
      : mp_solver_(mp_solver),
        translation_(tr),
        objective_(objective),
        maximize_(maximize) {}

  ~Linearizer() override {}

  // Begin/End visit element.
  void BeginVisitModel(const std::string& solver_name) override { BeginVisit(true); }

  void EndVisitModel(const std::string& solver_name) override { EndVisit(); }

  void BeginVisitConstraint(const std::string& type_name,
                            const Constraint* const constraint) override {
    if (!constraint->IsCastConstraint() &&
        (IS_TYPE(type_name, kEquality) || IS_TYPE(type_name, kLessOrEqual) ||
         IS_TYPE(type_name, kGreaterOrEqual) ||
         IS_TYPE(type_name, kScalProdLessOrEqual))) {
      BeginVisit(true);
    } else {
      BeginVisit(false);
    }
  }

  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override {
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
  void BeginVisitExtension(const std::string& type) override { BeginVisit(true); }
  void EndVisitExtension(const std::string& type) override {
    if (IS_TYPE(type, kObjectiveExtension)) {
      VisitObjective();
    }
    EndVisit();
  }
  void BeginVisitIntegerExpression(const std::string& type_name,
                                   const IntExpr* const expr) override {
    BeginVisit(true);
  }
  void EndVisitIntegerExpression(const std::string& type_name,
                                 const IntExpr* const expr) override {
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

  void VisitIntegerVariable(const IntVar* const variable,
                            const std::string& operation, int64 value,
                            IntVar* const delegate) override {
    RegisterExpression(variable);
    RegisterExpression(delegate);
    if (operation == ModelVisitor::kSumOperation) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(value, value);
      ct->SetCoefficient(Translated(variable), 1.0);
      ct->SetCoefficient(Translated(delegate), -1.0);
    } else if (operation == ModelVisitor::kDifferenceOperation) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(value, value);
      ct->SetCoefficient(Translated(variable), 1.0);
      ct->SetCoefficient(Translated(delegate), 1.0);
    } else if (operation == ModelVisitor::kProductOperation) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
      ct->SetCoefficient(Translated(variable), 1.0);
      ct->SetCoefficient(Translated(delegate), -value);
    }
  }

  void VisitIntegerVariable(const IntVar* const variable,
                            IntExpr* const delegate) override {
    RegisterExpression(variable);
    if (delegate != nullptr) {
      VisitSubExpression(delegate);
      AddMpEquality(variable, delegate);
    }
  }

  void VisitIntervalVariable(const IntervalVar* const variable,
                             const std::string& operation, int64 value,
                             IntervalVar* const delegate) override {}

  // Visit integer arguments.
  void VisitIntegerArgument(const std::string& arg_name, int64 value) override {
    if (DoVisit()) {
      Top()->SetIntegerArgument(arg_name, value);
    }
  }

  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64>& values) override {
    if (DoVisit()) {
      Top()->SetIntegerArrayArgument(arg_name, values);
    }
  }

  void VisitIntegerMatrixArgument(const std::string& arg_name,
                                  const IntTupleSet& values) override {
    if (DoVisit()) {
      Top()->SetIntegerMatrixArgument(arg_name, values);
    }
  }

  // Visit integer expression argument.
  void VisitIntegerExpressionArgument(const std::string& arg_name,
                                      IntExpr* const argument) override {
    if (DoVisit()) {
      Top()->SetIntegerExpressionArgument(arg_name, argument);
      VisitSubExpression(argument);
    }
  }

  void VisitIntegerVariableArrayArgument(
      const std::string& arg_name, const std::vector<IntVar*>& arguments) override {
    if (DoVisit()) {
      Top()->SetIntegerVariableArrayArgument(arg_name, arguments);
      for (int i = 0; i < arguments.size(); ++i) {
        VisitSubExpression(arguments[i]);
      }
    }
  }

  // Visit interval argument.
  void VisitIntervalArgument(const std::string& arg_name,
                             IntervalVar* const argument) override {}

  void VisitIntervalArrayArgument(
      const std::string& arg_name,
      const std::vector<IntervalVar*>& argument) override {}

  std::string DebugString() const override { return "Linearizer"; }

 private:
  void BeginVisit(bool active) {
    PushActive(active);
    PushArgumentHolder();
  }

  void EndVisit() {
    PopArgumentHolder();
    PopActive();
  }

  bool DoVisit() const { return actives_.back(); }

  void PushActive(bool active) { actives_.push_back(active); }

  void PopActive() { actives_.pop_back(); }

  void RegisterExpression(const IntExpr* const cp_expr) {
    if (!ContainsKey(*translation_, cp_expr)) {
      MPVariable* const mp_var =
          mp_solver_->MakeIntVar(cp_expr->Min(), cp_expr->Max(), "");
      (*translation_)[cp_expr] = mp_var;
    }
  }

  void VisitSubExpression(IntExpr* const cp_expr) {
    if (!ContainsKey(*translation_, cp_expr)) {
      cp_expr->Accept(this);
    }
  }

  void AddMpEquality(const IntExpr* const left, const IntExpr* const right) {
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
    ct->SetCoefficient(Translated(left), 1.0);
    ct->SetCoefficient(Translated(right), -1.0);
  }

  MPVariable* Translated(const IntExpr* const cp_expr) {
    return FindOrDie((*translation_), cp_expr);
  }

  void VisitBinaryRowConstraint(double lhs, double rhs) {
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(lhs, rhs);
    const IntExpr* const left =
        Top()->FindIntegerExpressionArgumentOrDie(ModelVisitor::kLeftArgument);
    const IntExpr* const right =
        Top()->FindIntegerExpressionArgumentOrDie(ModelVisitor::kRightArgument);
    ct->SetCoefficient(Translated(left), 1.0);
    ct->SetCoefficient(Translated(right), -1.0);
  }

  void VisitUnaryRowConstraint(double lhs, double rhs) {
    const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
        ModelVisitor::kExpressionArgument);
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(lhs, rhs);
    ct->SetCoefficient(Translated(expr), 1.0);
  }

  void VisitEquality() {
    if (Top()->HasIntegerExpressionArgument(ModelVisitor::kLeftArgument)) {
      VisitBinaryRowConstraint(0.0, 0.0);
    } else {
      const int64 value =
          Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      VisitUnaryRowConstraint(value, value);
    }
  }

  void VisitLessOrEqual() {
    if (Top()->HasIntegerExpressionArgument(ModelVisitor::kLeftArgument)) {
      VisitBinaryRowConstraint(-mp_solver_->infinity(), 0.0);
    } else {
      const int64 value =
          Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      VisitUnaryRowConstraint(-mp_solver_->infinity(), value);
    }
  }

  void VisitGreaterOrEqual() {
    if (Top()->HasIntegerExpressionArgument(ModelVisitor::kLeftArgument)) {
      VisitBinaryRowConstraint(0.0, mp_solver_->infinity());
    } else {
      const int64 value =
          Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      VisitUnaryRowConstraint(value, mp_solver_->infinity());
    }
  }

  void VisitScalProdLessOrEqual() {
    const std::vector<IntVar*>& cp_vars =
        Top()->FindIntegerVariableArrayArgumentOrDie(
            ModelVisitor::kVarsArgument);
    const std::vector<int64>& cp_coefficients =
        Top()->FindIntegerArrayArgumentOrDie(
            ModelVisitor::kCoefficientsArgument);
    const int64 constant =
        Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
    MPConstraint* const ct =
        mp_solver_->MakeRowConstraint(-mp_solver_->infinity(), constant);
    CHECK_EQ(cp_vars.size(), cp_coefficients.size());
    for (int i = 0; i < cp_coefficients.size(); ++i) {
      MPVariable* const mp_var = Translated(cp_vars[i]);
      ct->SetCoefficient(mp_var,
                         cp_coefficients[i] + ct->GetCoefficient(mp_var));
    }
  }

  void VisitSum(const IntExpr* const cp_expr) {
    if (Top()->HasIntegerVariableArrayArgument(ModelVisitor::kVarsArgument)) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
      const std::vector<IntVar*>& cp_vars =
          Top()->FindIntegerVariableArrayArgumentOrDie(
              ModelVisitor::kVarsArgument);
      for (int i = 0; i < cp_vars.size(); ++i) {
        MPVariable* const mp_var = Translated(cp_vars[i]);
        ct->SetCoefficient(mp_var, 1.0 + ct->GetCoefficient(mp_var));
      }
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1.0);
    } else if (Top()->HasIntegerExpressionArgument(
                   ModelVisitor::kLeftArgument)) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
      const IntExpr* const left = Top()->FindIntegerExpressionArgumentOrDie(
          ModelVisitor::kLeftArgument);
      const IntExpr* const right = Top()->FindIntegerExpressionArgumentOrDie(
          ModelVisitor::kRightArgument);
      if (left != right) {
        ct->SetCoefficient(Translated(left), 1.0);
        ct->SetCoefficient(Translated(right), 1.0);
      } else {
        ct->SetCoefficient(Translated(left), 2.0);
      }
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1.0);
    } else {
      const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
          ModelVisitor::kExpressionArgument);
      const int64 value =
          Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(-value, -value);
      ct->SetCoefficient(Translated(expr), 1.0);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1.0);
    }
  }

  void VisitScalProd(const IntExpr* const cp_expr) {
    const std::vector<IntVar*>& cp_vars =
        Top()->FindIntegerVariableArrayArgumentOrDie(
            ModelVisitor::kVarsArgument);
    const std::vector<int64>& cp_coefficients =
        Top()->FindIntegerArrayArgumentOrDie(
            ModelVisitor::kCoefficientsArgument);
    CHECK_EQ(cp_vars.size(), cp_coefficients.size());
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
    for (int i = 0; i < cp_vars.size(); ++i) {
      MPVariable* const mp_var = Translated(cp_vars[i]);
      const int64 coefficient = cp_coefficients[i];
      ct->SetCoefficient(mp_var, coefficient + ct->GetCoefficient(mp_var));
    }
    RegisterExpression(cp_expr);
    ct->SetCoefficient(Translated(cp_expr), -1.0);
  }

  void VisitDifference(const IntExpr* const cp_expr) {
    if (Top()->HasIntegerExpressionArgument(ModelVisitor::kLeftArgument)) {
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
      const IntExpr* const left = Top()->FindIntegerExpressionArgumentOrDie(
          ModelVisitor::kLeftArgument);
      const IntExpr* const right = Top()->FindIntegerExpressionArgumentOrDie(
          ModelVisitor::kRightArgument);
      ct->SetCoefficient(Translated(left), 1.0);
      ct->SetCoefficient(Translated(right), -1.0);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1.0);
    } else {
      const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
          ModelVisitor::kExpressionArgument);
      const int64 value =
          Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(value, value);
      ct->SetCoefficient(Translated(expr), 1.0);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), 1.0);
    }
  }

  void VisitOpposite(const IntExpr* const cp_expr) {
    const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
        ModelVisitor::kExpressionArgument);
    MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
    ct->SetCoefficient(Translated(expr), 1.0);
    RegisterExpression(cp_expr);
    ct->SetCoefficient(Translated(cp_expr), -1.0);
  }

  void VisitProduct(const IntExpr* const cp_expr) {
    if (Top()->HasIntegerExpressionArgument(
            ModelVisitor::kExpressionArgument)) {
      const IntExpr* const expr = Top()->FindIntegerExpressionArgumentOrDie(
          ModelVisitor::kExpressionArgument);
      const int64 value =
          Top()->FindIntegerArgumentOrDie(ModelVisitor::kValueArgument);
      MPConstraint* const ct = mp_solver_->MakeRowConstraint(0.0, 0.0);
      ct->SetCoefficient(Translated(expr), value);
      RegisterExpression(cp_expr);
      ct->SetCoefficient(Translated(cp_expr), -1.0);
    } else {
      RegisterExpression(cp_expr);
    }
  }

  void VisitIntegerExpression(const IntExpr* const cp_expr) {
    RegisterExpression(cp_expr);
  }

  void VisitObjective() {
    *maximize_ =
        Top()->FindIntegerArgumentOrDie(ModelVisitor::kMaximizeArgument);
    *objective_ =
        const_cast<IntExpr*>(Top()->FindIntegerExpressionArgumentOrDie(
                                 ModelVisitor::kExpressionArgument))->Var();
    MPObjective* const objective = mp_solver_->MutableObjective();
    objective->SetCoefficient(Translated(*objective_), 1.0);
    objective->SetOptimizationDirection(*maximize_);
  }

  MPSolver* const mp_solver_;
  ExprTranslation* const translation_;
  IntVar** objective_;
  bool* maximize_;
  std::vector<bool> actives_;
};

#undef IS_TYPE

// ----- Search Monitor -----

class AutomaticLinearization : public SearchMonitor {
 public:
  AutomaticLinearization(Solver* const solver, int frequency)
      : SearchMonitor(solver),
        mp_solver_("InSearchSimplex", LpSolverForCp()),
        counter_(0),
        simplex_frequency_(frequency),
        objective_(nullptr),
        maximize_(false) {}

  ~AutomaticLinearization() override {}

  void BeginInitialPropagation() override {
    mp_solver_.Clear();
    translation_.clear();
    BuildModel();
  }

  void EndInitialPropagation() override { RunOptim(); }

  void BeginNextDecision(DecisionBuilder* const b) override {
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
    for (const auto& it : translation_) {
      it.second->SetBounds(it.first->Min(), it.first->Max());
    }
  }

  void SolveProblem() {
    if (objective_ != nullptr) {
      switch (mp_solver_.Solve()) {
        case MPSolver::OPTIMAL: {
          const double obj_value = mp_solver_.Objective().Value();
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

  std::string DebugString() const override { return "AutomaticLinearization"; }

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

SearchMonitor* MakeSimplexConnection(Solver* const solver,
                                     std::function<void(MPSolver*)> builder,
                                     std::function<void(MPSolver*)> modifier,
                                     std::function<void(MPSolver*)> runner,
                                     int frequency) {
  return solver->RevAlloc(new SimplexConnection(solver, std::move(builder),
                                                std::move(modifier),
                                                std::move(runner), frequency));
}

SearchMonitor* MakeSimplexConstraint(Solver* const solver,
                                     int simplex_frequency) {
  return solver->RevAlloc(
      new AutomaticLinearization(solver, simplex_frequency));
}
}  // namespace operations_research
