/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Mikael Lagerkvist <lagerkvist@gmail.com>
 *
 *  Modified:
 *     Laurent Perron for OR-Tools (laurent.perron@gmail.com)
 *
 *  Copyright:
 *     Guido Tack, 2007
 *     Mikael Lagerkvist, 2009
 *
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "base/commandlineflags.h"
#include "flatzinc/flatzinc.h"
#include "flatzinc/flatzinc_constraints.h"
#include "base/mathutil.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_trace_propagation);
DEFINE_bool(use_sat, true, "Use sat for boolean propagation");

namespace operations_research {
extern bool HasDomainAnnotation(AstNode* const annotations);
namespace {
// Help

bool IsBoolean(IntExpr* const expr) {
  return expr->Min() >= 0 && expr->Max() <= 1;
}

int64 BuildNonNullVarsCoefs(FlatZincModel* const model,
                            const AstArray* const array_coefficients,
                            const AstArray* const array_variables,
                            std::vector<int64>* const coefficients,
                            std::vector<IntVar*>* const variables) {
  coefficients->clear();
  variables->clear();
  int64 constant = 0;
  for (int i = 0; i < array_coefficients->a.size(); ++i) {
    const int64 coef = array_coefficients->a[i]->getInt();
    IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
    if (coef != 0 && (var->Min() != 0 || var->Max() != 0)) {
      if (var->Bound()) {
        constant += var->Min() * coef;
      } else {
        coefficients->push_back(coef);
        variables->push_back(var);
      }
    }
  }
  return constant;
}

// Map from constraint identifier to constraint posting functions
class ModelBuilder {
 public:
  // Type of constraint posting function
  typedef void (*Builder)(FlatZincModel* const model, CtSpec* const spec);
  // Add posting function \a p with identifier \a id
  void Register(const std::string& id, Builder p);
  // Post constraint specified by \a ce
  void Post(FlatZincModel* const model, CtSpec* const spec);

 private:
  // The actual builders.
  std::map<std::string, Builder> r;
};

static ModelBuilder global_model_builder;

void ModelBuilder::Post(FlatZincModel* const model, CtSpec* const spec) {
  if (!spec->Nullified()) {
    const std::string& id = spec->Id();
    std::map<std::string, Builder>::iterator i = r.find(id);
    if (i == r.end()) {
      throw FzError("ModelBuilder",
                    std::string("Constraint ") + spec->Id() + " not found");
    }
    i->second(model, spec);
  }
}

void ModelBuilder::Register(const std::string& id, Builder p) { r[id] = p; }

void p_int_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  if (spec->IsDefined(spec->Arg(0))) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    VLOG(2) << "  - creating " << spec->Arg(0)->DebugString()
            << " := " << right->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(0));
    model->SetIntegerExpression(spec->Arg(0), right);
  } else if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && IsBoolean(left) && IsBoolean(right) &&
        AddBoolEq(model->Sat(), left, right)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeEquality(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_int_ne(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    if (left->IsVar()) {
      left->Var()->RemoveValue(spec->Arg(1)->getInt());
    } else {
      Constraint* const ct =
          solver->MakeNonEquality(left, spec->Arg(1)->getInt());
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && IsBoolean(left) && IsBoolean(right) &&
        AddBoolNot(model->Sat(), left, right)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeNonEquality(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_int_ge(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct =
        solver->MakeGreaterOrEqual(left, spec->Arg(1)->getInt());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && IsBoolean(left) && IsBoolean(right) &&
        AddBoolLe(model->Sat(), right, left)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeGreaterOrEqual(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_int_gt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct = solver->MakeGreater(left, spec->Arg(1)->getInt());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeGreater(left, right);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct =
        solver->MakeLessOrEqual(left, spec->Arg(1)->getInt());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && IsBoolean(left) && IsBoolean(right) &&
        AddBoolLe(model->Sat(), left, right)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeLessOrEqual(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_int_lt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct = solver->MakeLess(left, spec->Arg(1)->getInt());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeLess(left, right);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

/* Comparisons */
void p_int_eq_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt()
            ? solver->MakeIsEqualCstVar(left, node_right->getInt())
            : solver->MakeIsEqualVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(node_right);
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsEqualCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt()
            ? solver->MakeIsDifferentCstVar(left, node_right->getInt())
            : solver->MakeIsDifferentVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsDifferentCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt()
            ? solver->MakeIsGreaterOrEqualCstVar(left, node_right->getInt())
            : solver->MakeIsGreaterOrEqualVar(left,
                                              model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt()
            ? solver->MakeIsGreaterCstVar(left, node_right->getInt())
            : solver->MakeIsGreaterVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsGreaterCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt()
            ? solver->MakeIsLessOrEqualCstVar(left, node_right->getInt())
            : solver->MakeIsLessOrEqualVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsLessOrEqualCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt()
            ? solver->MakeIsLessCstVar(left, node_right->getInt())
            : solver->MakeIsLessVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsLessCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_lin_eq(FlatZincModel* const model, CtSpec* const spec) {
  bool strong_propagation = HasDomainAnnotation(spec->annotations());
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  if (spec->DefinedArg() != nullptr) {
    if (size == 2) {
      IntExpr* other = nullptr;
      int64 other_coef = 0;
      AstNode* defined = nullptr;
      if (spec->IsDefined(array_variables->a[0]) &&
          array_coefficients->a[0]->getInt() == -1) {
        other = model->GetIntExpr(array_variables->a[1]);
        other_coef = array_coefficients->a[1]->getInt();
        defined = array_variables->a[0];
      } else if (spec->IsDefined(array_variables->a[1]) &&
                 array_coefficients->a[1]->getInt() == -1) {
        other = model->GetIntExpr(array_variables->a[0]);
        other_coef = array_coefficients->a[0]->getInt();
        defined = array_variables->a[1];
      } else {
        throw FzError(
            "ModelBuilder",
            std::string("Constraint ") + spec->Id() +
                " cannot define an integer variable with a coefficient"
                " different from -1");
      }

      IntExpr* const target =
          solver->MakeSum(solver->MakeProd(other, other_coef), -rhs);
      VLOG(2) << "  - creating " << defined->DebugString()
              << " := " << target->DebugString();
      model->CheckIntegerVariableIsNull(defined);
      model->SetIntegerExpression(defined, target);
    } else {
      std::vector<int64> coefficients;
      std::vector<IntVar*> variables;
      int64 constant = 0;
      AstNode* defined = nullptr;
      for (int i = 0; i < size; ++i) {
        if (array_variables->a[i]->isInt()) {
          constant += array_coefficients->a[i]->getInt() *
                      array_variables->a[i]->getInt();
        } else if (spec->IsDefined(array_variables->a[i])) {
          CHECK(defined == nullptr);
          defined = array_variables->a[i];
          if (array_coefficients->a[i]->getInt() != -1) {
            throw FzError(
                "ModelBuilder",
                std::string("Constraint ") + spec->Id() +
                    " cannot define an integer variable with a coefficient"
                    " different from -1");
          }
        } else {
          const int64 coef = array_coefficients->a[i]->getInt();
          IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
          if (coef != 0 && (var->Min() != 0 || var->Max() != 0)) {
            coefficients.push_back(coef);
            variables.push_back(var);
          }
        }
      }
      IntExpr* const target = solver->MakeSum(
          solver->MakeScalProd(variables, coefficients), constant - rhs);
      VLOG(2) << "  - creating " << defined->DebugString()
              << " := " << target->DebugString();
      model->CheckIntegerVariableIsNull(defined);
      model->SetIntegerExpression(defined, target);
    }
  } else {
    Constraint* ct = nullptr;
    switch (size) {
      case 0: {
        ct = rhs == 0 ? solver->MakeTrueConstraint()
                      : solver->MakeFalseConstraint();
        break;
      }
      case 1: {
        IntExpr* const e1 = model->GetIntExpr(array_variables->a[0]);
        const int64 c1 = array_coefficients->a[0]->getInt();
        ct = solver->MakeEquality(solver->MakeProd(e1, c1), rhs);
        break;
      }
      case 2: {
        IntExpr* const e1 = model->GetIntExpr(array_variables->a[0]);
        IntExpr* const e2 = model->GetIntExpr(array_variables->a[1]);
        const int64 c1 = array_coefficients->a[0]->getInt();
        const int64 c2 = array_coefficients->a[1]->getInt();
        if (FLAGS_use_sat && IsBoolean(e1) && IsBoolean(e2) && c1 == 1 &&
            c2 == 1 && rhs == 1 && AddBoolNot(model->Sat(), e1, e2)) {
          // Simple case b1 + b2 == 1, or b1 = not(b2).
          VLOG(2) << "  - posted to sat";
          return;
        } else {
          if (c1 > 0) {
            if (c2 > 0) {
              ct = solver->MakeEquality(
                  solver->MakeProd(e1, c1),
                  solver->MakeDifference(rhs, solver->MakeProd(e2, c2)));
            } else {
              ct = solver->MakeEquality(
                  solver->MakeProd(e1, c1),
                  solver->MakeSum(solver->MakeProd(e2, -c2), rhs));
            }
          } else if (c2 > 0) {
            ct = solver->MakeEquality(
                solver->MakeProd(e2, c2),
                solver->MakeSum(solver->MakeProd(e1, -c1), rhs));
          } else {
            ct = solver->MakeEquality(
                solver->MakeProd(e1, -c1),
                solver->MakeDifference(-rhs, solver->MakeProd(e2, -c2)));
          }
          break;
        }
        break;
      }
      case 3: {
        IntExpr* const e1 = model->GetIntExpr(array_variables->a[0]);
        IntExpr* const e2 = model->GetIntExpr(array_variables->a[1]);
        IntExpr* const e3 = model->GetIntExpr(array_variables->a[2]);
        const int64 c1 = array_coefficients->a[0]->getInt();
        const int64 c2 = array_coefficients->a[1]->getInt();
        const int64 c3 = array_coefficients->a[2]->getInt();
        if (strong_propagation) {
          std::vector<int64> coefficients;
          std::vector<IntVar*> variables;
          coefficients.push_back(c1);
          coefficients.push_back(c2);
          coefficients.push_back(c3);
          variables.push_back(e1->Var());
          variables.push_back(e2->Var());
          variables.push_back(e3->Var());
          ct = MakeStrongScalProdEquality(solver, variables, coefficients, rhs);
        } else {
          if (c1 < 0 && c2 > 0 && c3 > 0) {
            ct = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e2, c2),
                                solver->MakeProd(e3, c3)),
                solver->MakeSum(solver->MakeProd(e1, -c1), rhs));
          } else if (c1 > 0 && c2 < 0 && c3 > 0) {
            ct = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, c1),
                                solver->MakeProd(e3, c3)),
                solver->MakeSum(solver->MakeProd(e2, -c2), rhs));
          } else if (c1 > 0 && c2 > 0 && c3 < 0) {
            ct = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, c1),
                                solver->MakeProd(e2, c2)),
                solver->MakeSum(solver->MakeProd(e3, -c3), rhs));
          } else if (c1 < 0 && c2 < 0 && c3 > 0) {
            ct = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, -c1),
                                solver->MakeProd(e2, -c2)),
                solver->MakeSum(solver->MakeProd(e3, c3), -rhs));
          } else {
            ct = solver->MakeEquality(
                solver->MakeSum(solver->MakeProd(e1, c1),
                                solver->MakeProd(e2, c2)),
                solver->MakeDifference(rhs, solver->MakeProd(e3, c3)));
          }
        }
        break;
      }
      default: {
        std::vector<int64> coefficients;
        std::vector<IntVar*> variables;
        for (int i = 0; i < size; ++i) {
          const int64 coef = array_coefficients->a[i]->getInt();
          IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
          if (coef != 0 && (var->Min() != 0 || var->Max() != 0)) {
            if (var->Bound()) {
              rhs -= var->Min() * coef;
            } else {
              coefficients.push_back(coef);
              variables.push_back(var);
            }
          }
        }
        if (AreAllBooleans(variables) && AreAllOnes(coefficients)) {
          PostBooleanSumInRange(model, spec, variables, rhs, rhs);
          return;
        } else {
          ct = strong_propagation
              ? MakeStrongScalProdEquality(solver, variables, coefficients, rhs)
              : solver->MakeScalProdEquality(variables, coefficients, rhs);
        }
      }
    }
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_lin_eq_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);

  int positive = 0;
  int negative = 0;
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    if (coefficients[i] > 0) {
      positive++;
    } else if (coefficients[i] < 0) {
      negative++;
    }
  }
  const bool define = spec->IsDefined(node_boolvar);

  if (positive == 1) {
    IntExpr* pos = nullptr;
    IntExpr* other = nullptr;
    if (negative == 2) {
      std::vector<IntExpr*> neg_exprs;
      for (int i = 0; i < size; ++i) {
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(expr, coeff);
        } else if (coeff < 0) {
          neg_exprs.push_back(solver->MakeProd(expr, -coeff));
        }
      }
      other = solver->MakeSum(neg_exprs[0], neg_exprs[1]);
    } else {
      std::vector<IntVar*> neg_vars;
      std::vector<int64> neg_coeffs;
      for (int i = 0; i < size; ++i) {
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(var, coeff);
        } else if (coeff < 0) {
          neg_vars.push_back(var);
          neg_coeffs.push_back(-coeff);
        }
      }
      other = solver->MakeScalProd(neg_vars, neg_coeffs);
    }
    pos = solver->MakeSum(pos, -rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsEqualVar(pos, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsEqualCt(pos, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else if (negative == 1) {
    IntExpr* neg = nullptr;
    IntExpr* other = nullptr;
    if (positive == 2) {
      std::vector<IntExpr*> pos_exprs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        if (coeff < 0) {
          neg = solver->MakeProd(expr, -coeff);
        } else if (coeff > 0) {
          pos_exprs.push_back(solver->MakeProd(expr, coeff));
        }
      }
      other = solver->MakeSum(pos_exprs[0], pos_exprs[1]);
    } else {
      std::vector<IntVar*> pos_vars;
      std::vector<int64> pos_coeffs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        if (coeff < 0) {
          neg = solver->MakeProd(var, -coeff);
        } else if (coeff > 0) {
          pos_vars.push_back(var);
          pos_coeffs.push_back(coeff);
        }
      }
      other = solver->MakeScalProd(pos_vars, pos_coeffs);
    }
    neg = solver->MakeSum(neg, rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsEqualVar(neg, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsEqualCt(neg, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    std::vector<IntVar*> variables;
    rhs -= BuildNonNullVarsCoefs(model, array_coefficients, array_variables,
                                 &coefficients, &variables);
    if (AreAllOnes(coefficients) && AreAllBooleans(variables)) {
      IntVar* boolvar = nullptr;
      if (define) {
        boolvar = solver->MakeBoolVar();
        VLOG(2) << "  - creating " << node_boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        boolvar = model->GetIntExpr(node_boolvar)->Var();
      }
      PostIsBooleanSumInRange(model, spec, variables, rhs, rhs, boolvar);
    } else {
      IntExpr* const var = solver->MakeScalProd(variables, coefficients);
      if (define) {
        IntVar* const boolvar = solver->MakeIsEqualCstVar(var, rhs);
        VLOG(2) << "  - creating " << node_boolvar->DebugString()
                << " := " << boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
        Constraint* const ct = solver->MakeIsEqualCstCt(var, rhs, boolvar);
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    }
  }
}

void p_int_lin_ne(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  Constraint* const ct = solver->MakeNonEquality(
      solver->MakeScalProd(variables, coefficients), rhs);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_int_lin_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);

  int positive = 0;
  int negative = 0;
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    if (coefficients[i] > 0) {
      positive++;
    } else if (coefficients[i] < 0) {
      negative++;
    }
  }
  const bool define = spec->IsDefined(node_boolvar);

  if (positive == 1) {
    IntExpr* pos = nullptr;
    IntExpr* other = nullptr;
    if (negative == 2) {
      std::vector<IntExpr*> neg_exprs;
      for (int i = 0; i < size; ++i) {
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(expr, coeff);
        } else if (coeff < 0) {
          neg_exprs.push_back(solver->MakeProd(expr, -coeff));
        }
      }
      other = solver->MakeSum(neg_exprs[0], neg_exprs[1]);
    } else {
      std::vector<IntVar*> neg_vars;
      std::vector<int64> neg_coeffs;
      for (int i = 0; i < size; ++i) {
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(var, coeff);
        } else if (coeff < 0) {
          neg_vars.push_back(var);
          neg_coeffs.push_back(-coeff);
        }
      }
      other = solver->MakeScalProd(neg_vars, neg_coeffs);
    }
    pos = solver->MakeSum(pos, -rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsDifferentVar(pos, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsDifferentCt(pos, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else if (negative == 1) {
    IntExpr* neg = nullptr;
    IntExpr* other = nullptr;
    if (positive == 2) {
      std::vector<IntExpr*> pos_exprs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        if (coeff < 0) {
          neg = solver->MakeProd(expr, -coeff);
        } else if (coeff > 0) {
          pos_exprs.push_back(solver->MakeProd(expr, coeff));
        }
      }
      other = solver->MakeSum(pos_exprs[0], pos_exprs[1]);
    } else {
      std::vector<IntVar*> pos_vars;
      std::vector<int64> pos_coeffs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        if (coeff < 0) {
          neg = solver->MakeProd(var, -coeff);
        } else if (coeff > 0) {
          pos_vars.push_back(var);
          pos_coeffs.push_back(coeff);
        }
      }
      other = solver->MakeScalProd(pos_vars, pos_coeffs);
    }
    neg = solver->MakeSum(neg, rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsDifferentVar(neg, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsDifferentCt(neg, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    std::vector<IntVar*> variables;
    rhs -= BuildNonNullVarsCoefs(model, array_coefficients, array_variables,
                                 &coefficients, &variables);
    if (AreAllOnes(coefficients) && AreAllBooleans(variables)) {
      IntVar* boolvar = nullptr;
      if (define) {
        boolvar = solver->MakeBoolVar();
        VLOG(2) << "  - creating " << node_boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        boolvar = model->GetIntExpr(node_boolvar)->Var();
      }
      IntVar* const negated = solver->MakeDifference(1, boolvar)->Var();
      PostIsBooleanSumInRange(model, spec, variables, rhs, rhs, negated);
    } else {
      IntExpr* const var = solver->MakeScalProd(variables, coefficients);
      if (define) {
        IntVar* const boolvar = solver->MakeIsDifferentCstVar(var, rhs);
        VLOG(2) << "  - creating " << node_boolvar->DebugString()
                << " := " << boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
        Constraint* const ct = solver->MakeIsDifferentCstCt(var, rhs, boolvar);
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    }
  }
}

bool PostHiddenOr(FlatZincModel* const model, CtSpec* const spec,
                  const std::vector<IntVar*>& vars,
                  const std::vector<int64>& coefs) {
  if (coefs[0] > 1 - vars.size()) {
    return false;
  }
  for (int i = 1; i < coefs.size(); ++i) {
    if (coefs[i] != 1) {
      return false;
    }
  }
  std::vector<IntVar*> others(vars.size() - 1);
  for (int i = 1; i < vars.size(); ++i) {
    others[i - 1] = vars[i];
  }
  return AddSumBoolArrayLessEqKVar(model->Sat(), others, vars[0]);
}

bool PostSumGreaterVar(FlatZincModel* const model, CtSpec* const spec,
                       const std::vector<IntVar*>& vars,
                       const std::vector<int64>& coefs) {
  if (coefs[0] != 1) {
    return false;
  }
  for (int i = 1; i < coefs.size(); ++i) {
    if (coefs[i] != -1) {
      return false;
    }
  }
  std::vector<IntVar*> others(vars.size() - 1);
  for (int i = 1; i < vars.size(); ++i) {
    others[i - 1] = vars[i];
  }
  return AddSumBoolArrayGreaterEqVar(model->Sat(), others, vars[0]);
}

void p_int_lin_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  Constraint* ct = nullptr;
  switch (size) {
    case 0: {
      ct = rhs >= 0 ? solver->MakeTrueConstraint()
                    : solver->MakeFalseConstraint();
      break;
    }
    case 1: {
      IntExpr* const e1 = model->GetIntExpr(array_variables->a[0]);
      const int64 c1 = array_coefficients->a[0]->getInt();
      ct = solver->MakeLessOrEqual(solver->MakeProd(e1, c1), rhs);
      break;
    }
    case 2: {
      IntExpr* e1 = model->GetIntExpr(array_variables->a[0]);
      int64 cc1 = 1;
      solver->IsProduct(e1, &e1, &cc1);
      IntExpr* e2 = model->GetIntExpr(array_variables->a[1]);
      int64 cc2 = 1;
      solver->IsProduct(e2, &e2, &cc2);
      int64 c1 = array_coefficients->a[0]->getInt() * cc1;
      int64 c2 = array_coefficients->a[1]->getInt() * cc2;
      const int64 gcd = MathUtil::GCD64(std::abs(c1), std::abs(c2));
      c1 /= gcd;
      c2 /= gcd;
      // if (FLAGS_use_sat && IsBoolean(e1) && IsBoolean(e2) && c1 == 1 &&
      //     c2 == 1 && rhs == 1 && AddBoolNot(model->Sat(), e1, e2)) {
      //   // Simple case b1 + b2 == 1, or b1 = not(b2).
      //   VLOG(2) << "  - posted to sat";
      //   return;
      // } else {
        if (c1 > 0) {
          if (c2 > 0) {
            ct = solver->MakeLessOrEqual(
                solver->MakeProd(e1, c1),
                solver->MakeDifference(rhs, solver->MakeProd(e2, c2)));
            } else {
              ct = solver->MakeLessOrEqual(
                  solver->MakeProd(e1, c1),
                  solver->MakeSum(solver->MakeProd(e2, -c2), rhs));
            }
          } else if (c2 > 0) {
            ct = solver->MakeLessOrEqual(
                solver->MakeProd(e2, c2),
                solver->MakeSum(solver->MakeProd(e1, -c1), rhs));
          } else {
            ct = solver->MakeGreaterOrEqual(
                solver->MakeProd(e1, -c1),
                solver->MakeDifference(-rhs, solver->MakeProd(e2, -c2)));
        }
        //    }
      break;
    }
    default: {
      std::vector<int64> coefficients(size);
      std::vector<IntVar*> variables(size);

      for (int i = 0; i < size; ++i) {
        coefficients[i] = array_coefficients->a[i]->getInt();
        variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
      }

      if (FLAGS_use_sat && AreAllBooleans(variables) && rhs == 0 &&
          PostHiddenOr(model, spec, variables, coefficients)) {
        VLOG(2) << "  - posted to sat";
      } else if (FLAGS_use_sat && AreAllBooleans(variables) && rhs == 0 &&
                 PostSumGreaterVar(model, spec, variables, coefficients)) {
        VLOG(2) << "  - posted to sat";
      } else if (FLAGS_use_sat && AreAllBooleans(variables) &&
                 AreAllOnes(coefficients) &&
                 ((rhs == 1 && AddAtMostOne(model->Sat(), variables)) ||
                  (rhs == variables.size() - 1 &&
                   AddAtMostNMinusOne(model->Sat(), variables)))) {
        VLOG(2) << "  - posted to sat";
      } else {
        ct = solver->MakeScalProdLessOrEqual(variables, coefficients, rhs);
      }
    }
  }
  if (ct != nullptr) {
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_lin_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);

  int positive = 0;
  int negative = 0;
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    if (coefficients[i] > 0) {
      positive++;
    } else if (coefficients[i] < 0) {
      negative++;
    }
  }
  const bool define = spec->IsDefined(node_boolvar);

  if (positive == 1) {
    IntExpr* pos = nullptr;
    IntExpr* other = nullptr;
    if (negative == 2) {
      std::vector<IntExpr*> neg_exprs;
      for (int i = 0; i < size; ++i) {
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(expr, coeff);
        } else if (coeff < 0) {
          neg_exprs.push_back(solver->MakeProd(expr, -coeff));
        }
      }
      other = solver->MakeSum(neg_exprs[0], neg_exprs[1]);
    } else {
      std::vector<IntVar*> neg_vars;
      std::vector<int64> neg_coeffs;
      for (int i = 0; i < size; ++i) {
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(var, coeff);
        } else if (coeff < 0) {
          neg_vars.push_back(var);
          neg_coeffs.push_back(-coeff);
        }
      }
      other = solver->MakeScalProd(neg_vars, neg_coeffs);
    }
    pos = solver->MakeSum(pos, -rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsLessOrEqualVar(pos, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsLessOrEqualCt(pos, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else if (negative == 1) {
    IntExpr* neg = nullptr;
    IntExpr* other = nullptr;
    if (positive == 2) {
      std::vector<IntExpr*> pos_exprs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        if (coeff < 0) {
          neg = solver->MakeProd(expr, -coeff);
        } else if (coeff > 0) {
          pos_exprs.push_back(solver->MakeProd(expr, coeff));
        }
      }
      other = solver->MakeSum(pos_exprs[0], pos_exprs[1]);
    } else {
      std::vector<IntVar*> pos_vars;
      std::vector<int64> pos_coeffs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        if (coeff < 0) {
          neg = solver->MakeProd(var, -coeff);
        } else if (coeff > 0) {
          pos_vars.push_back(var);
          pos_coeffs.push_back(coeff);
        }
      }
      other = solver->MakeScalProd(pos_vars, pos_coeffs);
    }
    neg = solver->MakeSum(neg, rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsGreaterOrEqualVar(neg, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct =
          solver->MakeIsGreaterOrEqualCt(neg, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    std::vector<IntVar*> variables;
    rhs -= BuildNonNullVarsCoefs(model, array_coefficients, array_variables,
                                 &coefficients, &variables);
    if (AreAllOnes(coefficients) && AreAllBooleans(variables)) {
      IntVar* boolvar = nullptr;
      if (define) {
        boolvar = solver->MakeBoolVar();
        VLOG(2) << "  - creating " << node_boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        boolvar = model->GetIntExpr(node_boolvar)->Var();
      }
      PostIsBooleanSumInRange(model, spec, variables, 0, rhs, boolvar);
    } else if (AreAllBooleans(variables) && rhs > 0 &&
               AreAllGreaterOrEqual(coefficients, rhs + 1)) {
      // We can simplify to sum smaller than one.
      IntVar* boolvar = nullptr;
      if (define) {
        boolvar = solver->MakeBoolVar();
        VLOG(2) << "  - creating " << node_boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        boolvar = model->GetIntExpr(node_boolvar)->Var();
      }
      PostIsBooleanSumInRange(model, spec, variables, 0, 0, boolvar);
    } else {
      IntExpr* const var = solver->MakeScalProd(variables, coefficients);
      if (define) {
        IntVar* const boolvar = solver->MakeIsLessOrEqualCstVar(var, rhs);
        VLOG(2) << "  - creating " << node_boolvar->DebugString()
                << " := " << boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
        Constraint* const ct =
            solver->MakeIsLessOrEqualCstCt(var, rhs, boolvar);
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    }
  }
}

void p_int_lin_ge(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  if (FLAGS_use_sat && rhs == 1 && AreAllBooleans(variables) &&
      AreAllOnes(coefficients) &&
      AddBoolOrArrayEqualTrue(model->Sat(), variables)) {
    VLOG(2) << "  - posted to sat";
  } else {
    Constraint* const ct =
        solver->MakeScalProdGreaterOrEqual(variables, coefficients, rhs);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_lin_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);

  int positive = 0;
  int negative = 0;
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    if (coefficients[i] > 0) {
      positive++;
    } else if (coefficients[i] < 0) {
      negative++;
    }
  }
  const bool define = spec->IsDefined(node_boolvar);

  if (positive == 1) {
    IntExpr* pos = nullptr;
    IntExpr* other = nullptr;
    if (negative == 2) {
      std::vector<IntExpr*> neg_exprs;
      for (int i = 0; i < size; ++i) {
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(expr, coeff);
        } else if (coeff < 0) {
          neg_exprs.push_back(solver->MakeProd(expr, -coeff));
        }
      }
      other = solver->MakeSum(neg_exprs[0], neg_exprs[1]);
    } else {
      std::vector<IntVar*> neg_vars;
      std::vector<int64> neg_coeffs;
      for (int i = 0; i < size; ++i) {
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        const int64 coeff = coefficients[i];
        if (coeff > 0) {
          pos = solver->MakeProd(var, coeff);
        } else if (coeff < 0) {
          neg_vars.push_back(var);
          neg_coeffs.push_back(-coeff);
        }
      }
      other = solver->MakeScalProd(neg_vars, neg_coeffs);
    }
    pos = solver->MakeSum(pos, -rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsGreaterOrEqualVar(pos, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct =
          solver->MakeIsGreaterOrEqualCt(pos, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else if (negative == 1) {
    IntExpr* neg = nullptr;
    IntExpr* other = nullptr;
    if (positive == 2) {
      std::vector<IntExpr*> pos_exprs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntExpr* const expr = model->GetIntExpr(array_variables->a[i]);
        if (coeff < 0) {
          neg = solver->MakeProd(expr, -coeff);
        } else if (coeff > 0) {
          pos_exprs.push_back(solver->MakeProd(expr, coeff));
        }
      }
      other = solver->MakeSum(pos_exprs[0], pos_exprs[1]);
    } else {
      std::vector<IntVar*> pos_vars;
      std::vector<int64> pos_coeffs;
      for (int i = 0; i < size; ++i) {
        const int64 coeff = coefficients[i];
        IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
        if (coeff < 0) {
          neg = solver->MakeProd(var, -coeff);
        } else if (coeff > 0) {
          pos_vars.push_back(var);
          pos_coeffs.push_back(coeff);
        }
      }
      other = solver->MakeScalProd(pos_vars, pos_coeffs);
    }
    neg = solver->MakeSum(neg, rhs);
    if (define) {
      IntVar* const boolvar = solver->MakeIsLessOrEqualVar(neg, other);
      VLOG(2) << "  - creating " << node_boolvar->DebugString()
              << " := " << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsLessOrEqualCt(neg, other, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    std::vector<IntVar*> variables;
    rhs -= BuildNonNullVarsCoefs(model, array_coefficients, array_variables,
                                 &coefficients, &variables);
    if (AreAllOnes(coefficients) && AreAllBooleans(variables)) {
      IntVar* boolvar = nullptr;
      if (define) {
        boolvar = solver->MakeBoolVar();
        VLOG(2) << "  - creating " << node_boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        boolvar = model->GetIntExpr(node_boolvar)->Var();
      }
      PostIsBooleanSumInRange(model, spec, variables, rhs, size, boolvar);
    } else if (AreAllBooleans(variables) && rhs > 0 &&
               AreAllGreaterOrEqual(coefficients, rhs + 1)) {
      // We can simplify to sum greater or equal than one.
      IntVar* boolvar = nullptr;
      if (define) {
        boolvar = solver->MakeBoolVar();
        VLOG(2) << "  - creating " << node_boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        boolvar = model->GetIntExpr(node_boolvar)->Var();
      }
      PostIsBooleanSumInRange(
          model, spec, variables, 1, variables.size(), boolvar);
    } else {
      IntExpr* const var = solver->MakeScalProd(variables, coefficients);
      if (define) {
        IntVar* const boolvar = solver->MakeIsGreaterOrEqualCstVar(var, rhs);
        VLOG(2) << "  - creating " << node_boolvar->DebugString()
                << " := " << boolvar->DebugString();
        model->CheckIntegerVariableIsNull(node_boolvar);
        model->SetIntegerExpression(node_boolvar, boolvar);
      } else {
        IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
        Constraint* const ct =
            solver->MakeIsGreaterOrEqualCstCt(var, rhs, boolvar);
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    }
  }
}

/* arithmetic constraints */

void p_int_plus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  if (spec->IsDefined(spec->Arg(0))) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    IntExpr* const left = solver->MakeDifference(target, right);
    VLOG(2) << "  - creating " << spec->Arg(0)->DebugString()
            << " := " << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(0));
    model->SetIntegerExpression(spec->Arg(0), left);
  } else if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    IntExpr* const right = solver->MakeDifference(target, left);
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << right->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), right);
  } else if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntExpr* const target = solver->MakeSum(left, right);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeSum(left, right), target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_minus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeDifference(left, right);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeDifference(left, right), target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_times(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeProd(left, right);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    if (FLAGS_use_sat && AddBoolAndEqVar(model->Sat(), left, right, target)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct =
          solver->MakeEquality(solver->MakeProd(left, right), target);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_int_div(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const target = model->GetIntExpr(spec->Arg(2));
  if (spec->Arg(1)->isIntVar()) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeDiv(left, right), target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    Constraint* const ct = solver->MakeEquality(
        solver->MakeDiv(left, spec->Arg(1)->getInt()), target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_mod(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const target = model->GetIntExpr(spec->Arg(2));
  if (spec->IsDefined(spec->Arg(2))) {
    if (spec->Arg(1)->isIntVar()) {
      IntExpr* const mod = model->GetIntExpr(spec->Arg(1));
      IntExpr* const target = solver->MakeModulo(left, mod);
      VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
              << " := " << target->DebugString();
      model->CheckIntegerVariableIsNull(spec->Arg(2));
      model->SetIntegerExpression(spec->Arg(2), target);
    } else {
      const int64 mod = spec->Arg(1)->getInt();
      IntExpr* const target = solver->MakeModulo(left, mod);
      VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
              << " := " << target->DebugString();
      model->CheckIntegerVariableIsNull(spec->Arg(2));
      model->SetIntegerExpression(spec->Arg(2), target);
    }
  } else {
    if (spec->Arg(1)->isIntVar()) {
      IntExpr* const mod = model->GetIntExpr(spec->Arg(1));
      Constraint* const ct =
          solver->MakeEquality(solver->MakeModulo(left, mod), target);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    } else {
      const int64 mod = spec->Arg(1)->getInt();
      Constraint* ct = nullptr;
      if (mod == 2 && target->Bound()) {
        switch (target->Min()) {
          case 0: {
            ct = MakeVariableEven(solver, left->Var());
            break;
          }
          case 1: {
            ct = MakeVariableOdd(solver, left->Var());
            break;
          }
          default: {
            ct = solver->MakeFalseConstraint();
            break;
          }
        }
      } else {
        ct = solver->MakeEquality(solver->MakeModulo(left, mod), target);
      }
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_int_min(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMin(left, right);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeMin(left, right), target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_max(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMax(left, right);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeMax(left, right), target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_negate(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const target = model->GetIntExpr(spec->Arg(2));
  Constraint* const ct =
      solver->MakeEquality(solver->MakeOpposite(left), target);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

// Bools

void p_bool_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  if (spec->IsDefined(spec->Arg(0))) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    VLOG(2) << "  - creating " << spec->Arg(0)->DebugString()
            << " := " << right->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(0));
    model->SetIntegerExpression(spec->Arg(0), right);
  } else if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && AddBoolEq(model->Sat(), left, right)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeEquality(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_ne(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct =
        solver->MakeNonEquality(left, spec->Arg(1)->getBool());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && AddBoolNot(model->Sat(), left, right)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeNonEquality(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_ge(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct =
        solver->MakeGreaterOrEqual(left, spec->Arg(1)->getBool());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && AddBoolLe(model->Sat(), right, left)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeGreaterOrEqual(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_gt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct = solver->MakeGreater(left, spec->Arg(1)->getBool());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeGreater(left, right);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_bool_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct =
        solver->MakeLessOrEqual(left, spec->Arg(1)->getBool());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && AddBoolLe(model->Sat(), left, right)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeLessOrEqual(left, right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_lt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct = solver->MakeLess(left, spec->Arg(1)->getBool());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeLess(left, right);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

/* Comparisons */
void p_bool_eq_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isBool()
            ? solver->MakeIsEqualCstVar(left, node_right->getBool())
            : solver->MakeIsEqualVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(node_right);
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_sat && AddBoolIsEqVar(model->Sat(), left, right, boolvar)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeIsEqualCt(left, right, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isBool()
            ? solver->MakeIsDifferentCstVar(left, node_right->getBool())
            : solver->MakeIsDifferentVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_sat && AddBoolIsNEqVar(model->Sat(), left, right, boolvar)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeIsDifferentCt(left, right, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isBool()
            ? solver->MakeIsGreaterOrEqualCstVar(left, node_right->getBool())
            : solver->MakeIsGreaterOrEqualVar(left,
                                              model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_sat && AddBoolIsLeVar(model->Sat(), right, left, boolvar)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct =
          solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isBool()
            ? solver->MakeIsGreaterCstVar(left, node_right->getBool())
            : solver->MakeIsGreaterVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsGreaterCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_bool_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isBool()
            ? solver->MakeIsLessOrEqualCstVar(left, node_right->getBool())
            : solver->MakeIsLessOrEqualVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_sat && AddBoolIsLeVar(model->Sat(), left, right, boolvar)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct = solver->MakeIsLessOrEqualCt(left, right, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isBool()
            ? solver->MakeIsLessCstVar(left, node_right->getBool())
            : solver->MakeIsLessVar(left, model->GetIntExpr(node_right));
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsLessCt(left, right, boolvar);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_bool_and(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMin(left, right);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    if (FLAGS_use_sat && AddBoolAndEqVar(model->Sat(), left, right, target)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct =
          solver->MakeEquality(solver->MakeMin(left, right), target);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_or(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMax(left, right);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    if (FLAGS_use_sat && AddBoolOrEqVar(model->Sat(), left, right, target)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct =
          solver->MakeEquality(solver->MakeMax(left, right), target);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_array_bool_or(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  AstNode* const node_boolvar = spec->Arg(1);
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables;
  hash_set<IntExpr*> added;
  for (int i = 0; i < size; ++i) {
    AstNode* const a = array_variables->a[i];
    IntVar* const to_add = model->GetIntExpr(a)->Var();
    if (!ContainsKey(added, to_add) && to_add->Max() != 0) {
      variables.push_back(to_add);
      added.insert(to_add);
    }
  }
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar = solver->MakeMax(variables)->Var();
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
  } else if (node_boolvar->isBool() && node_boolvar->getBool() == 0) {
    VLOG(2) << "  - forcing array_bool_or to 0";
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(0);
    }
  } else {
    if (node_boolvar->isBool()) {
      if (node_boolvar->getBool() == 1) {
        if (FLAGS_use_sat && AddBoolOrArrayEqualTrue(model->Sat(), variables)) {
          VLOG(2) << "  - posted to sat";
        } else {
          Constraint* const ct = solver->MakeSumGreaterOrEqual(variables, 1);
          VLOG(2) << "  - posted " << ct->DebugString();
          model->AddConstraint(spec, ct);
        }
      } else {
        Constraint* const ct = solver->MakeSumEquality(variables, Zero());
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      if (FLAGS_use_sat &&
          AddBoolOrArrayEqVar(model->Sat(), variables, boolvar)) {
        VLOG(2) << "  - posted to sat";
      } else {
        Constraint* const ct = solver->MakeMaxEquality(variables, boolvar);
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    }
  }
}

void p_array_bool_and(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  AstNode* const node_boolvar = spec->Arg(1);
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables;
  hash_set<IntExpr*> added;
  for (int i = 0; i < size; ++i) {
    AstNode* const a = array_variables->a[i];
    IntVar* const to_add = model->GetIntExpr(a)->Var();
    if (!ContainsKey(added, to_add)) {
      variables.push_back(to_add);
      added.insert(to_add);
    }
  }
  if (spec->IsDefined(node_boolvar)) {
    IntExpr* const boolvar = solver->MakeMin(variables);
    VLOG(2) << "  - creating " << node_boolvar->DebugString()
            << " := " << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
  } else if (node_boolvar->isBool() && node_boolvar->getBool() == 1) {
    VLOG(2) << "  - forcing array_bool_and to 1";
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(1);
    }
  } else {
    if (node_boolvar->isBool()) {
      if (node_boolvar->getBool() == 0) {
        if (FLAGS_use_sat &&
            AddBoolAndArrayEqualFalse(model->Sat(), variables)) {
          VLOG(2) << "  - posted to sat";
        } else {
          Constraint* const ct =
              solver->MakeSumLessOrEqual(variables, variables.size() - 1);
          VLOG(2) << "  - posted " << ct->DebugString();
          model->AddConstraint(spec, ct);
        }
      } else {
        Constraint* const ct =
            solver->MakeSumEquality(variables, variables.size());
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      if (FLAGS_use_sat &&
          AddBoolAndArrayEqVar(model->Sat(), variables, boolvar)) {
        VLOG(2) << "  - posted to sat";
      } else {
        Constraint* const ct = solver->MakeMinEquality(variables, boolvar);
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    }
  }
}

void p_array_bool_xor(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables;
  for (int i = 0; i < size; ++i) {
    variables.push_back(model->GetIntExpr(array_variables->a[i])->Var());
  }
  if (FLAGS_use_sat && AddArrayXor(model->Sat(), variables)) {
    VLOG(2) << "  - posted to sat";
  } else {
    Constraint* const ct = MakeBooleanSumOdd(solver, variables);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_array_bool_clause(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const a_array_variables = spec->Arg(0)->getArray();
  const int a_size = a_array_variables->a.size();
  AstArray* const b_array_variables = spec->Arg(1)->getArray();
  const int b_size = b_array_variables->a.size();
  std::vector<IntVar*> variables;
  for (int i = 0; i < a_size; ++i) {
    variables.push_back(model->GetIntExpr(a_array_variables->a[i])->Var());
  }
  for (int i = 0; i < b_size; ++i) {
    variables.push_back(solver->MakeDifference(
        1, model->GetIntExpr(b_array_variables->a[i])->Var())->Var());
  }
  if (FLAGS_use_sat && AddBoolOrArrayEqualTrue(model->Sat(), variables)) {
    VLOG(2) << "  - posted to sat";
  } else {
    Constraint* const ct = solver->MakeSumGreaterOrEqual(variables, 1);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_bool_xor(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
  if (FLAGS_use_sat && AddBoolIsNEqVar(model->Sat(), left, right, target)) {
    VLOG(2) << "  - posted to sat";
  } else {
    Constraint* const ct =
        solver->MakeIsEqualCstCt(solver->MakeSum(left, right), 1, target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_bool_not(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const target = solver->MakeDifference(1, left);
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), target);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_sat && AddBoolNot(model->Sat(), left, right)) {
      VLOG(2) << "  - posted to sat";
    } else {
      Constraint* const ct =
          solver->MakeEquality(solver->MakeDifference(1, left), right);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_bool_lin_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);
  int ones = 0;
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    ones += coefficients[i] == 1;
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  if (node_rhs->isInt()) {
    const int64 rhs = node_rhs->getInt();
    Constraint* const ct =
        ones == size
            ? solver->MakeSumEquality(variables, rhs)
            : solver->MakeScalProdEquality(variables, coefficients, rhs);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntVar* const target = model->GetIntExpr(node_rhs)->Var();
    Constraint* const ct =
        ones == size
            ? solver->MakeSumEquality(variables, target)
            : solver->MakeScalProdEquality(variables, coefficients, target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_bool_lin_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);
  int ones = 0;
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    ones += coefficients[i] == 1;
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  if (node_rhs->isInt()) {
    const int64 rhs = node_rhs->getInt();
    Constraint* const ct =
        ones == size
            ? solver->MakeSumLessOrEqual(variables, rhs)
            : solver->MakeScalProdLessOrEqual(variables, coefficients, rhs);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  } else {
    IntVar* const target = model->GetIntExpr(node_rhs)->Var();
    coefficients.push_back(-1);
    variables.push_back(target);
    Constraint* const ct =
        solver->MakeScalProdLessOrEqual(variables, coefficients, Zero());
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

/* element constraints */
void p_array_int_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index = model->GetIntExpr(spec->Arg(0));
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int64 imin = std::max(index->Min(), 1LL);
  const int64 imax = std::min(
      index->Max(), static_cast<int64>(array_coefficients->a.size()) + 1LL);
  IntVar* const shifted_index = solver->MakeSum(index, -imin)->Var();
  const int64 size = imax - imin + 1;
  std::vector<int64> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i + imin - 1]->getInt();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(coefficients, shifted_index);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(coefficients, shifted_index, target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_array_var_int_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index = model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(variables, shifted_index);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(variables, shifted_index, target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_array_var_int_position(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index = model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  const int target = spec->Arg(2)->getInt();
  Constraint* const ct = solver->MakeEquality(
      shifted_index, solver->MakeIndexExpression(variables, target)->Var());
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_array_bool_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index = model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int size = array_coefficients->a.size();
  std::vector<int64> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getBool();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(coefficients, shifted_index);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(coefficients, shifted_index, target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_array_var_bool_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index = model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(variables, shifted_index);
    VLOG(2) << "  - creating " << spec->Arg(2)->DebugString()
            << " := " << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(variables, shifted_index, target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

/* coercion constraints */
void p_bool2int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->IsDefined(spec->Arg(1))) {
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_bool2bool(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  CHECK_NOTNULL(left);
  if (spec->IsDefined(spec->Arg(1))) {
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int2int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else if (spec->IsDefined(spec->Arg(0))) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    VLOG(2) << "  - creating " << spec->Arg(0)->DebugString()
            << " := " << right->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(0));
    model->SetIntegerExpression(spec->Arg(0), right);

  } else {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_int_in(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstNode* const var_node = spec->Arg(0);
  AstNode* const domain_node = spec->Arg(1);
  CHECK(var_node->isIntVar());
  CHECK(domain_node->isSet());
  IntVar* const var = model->GetIntExpr(var_node)->Var();
  AstSetLit* const domain = domain_node->getSet();
  if (domain->interval) {
    if (var->Min() < domain->imin || var->Max() > domain->imax) {
      Constraint* const ct =
          solver->MakeBetweenCt(var, domain->imin, domain->imax);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    Constraint* const ct = solver->MakeMemberCt(var, domain->s);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_abs(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->IsDefined(spec->Arg(1))) {
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    IntExpr* const target = solver->MakeAbs(left);
    VLOG(2) << "  - creating " << spec->Arg(1)->DebugString()
            << " := " << target->DebugString();
    model->SetIntegerExpression(spec->Arg(1), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(solver->MakeAbs(left), target);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_all_different_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  int64 var_size = 0;
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
    var_size += variables[i]->Size();
  }
  Constraint* const ct = solver->MakeAllDifferent(variables, var_size < 1000);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_alldifferent_except_0(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables;
  for (int i = 0; i < size; ++i) {
    IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
    if (!var->Bound() || var->Min() != 0) {
      variables.push_back(var);
    }
  }
  if (variables.empty()) {
    return;
  }
  Constraint* const ct = solver->MakeAllDifferentExcept(variables, 0);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_count_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  if (spec->Arg(1)->isInt()) {
    const int64 value = spec->Arg(1)->getInt();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = model->GetIntExpr(spec->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  if (spec->Arg(2)->isInt()) {
    const int64 count = spec->Arg(2)->getInt();
    PostBooleanSumInRange(model, spec, tmp_sum, count, count);
  } else {
    IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
    if (count->Bound()) {
      const int64 fcount = count->Min();
      PostBooleanSumInRange(model, spec, tmp_sum, fcount, fcount);
    } else {
      Constraint* const ct = solver->MakeSumEquality(tmp_sum, count);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_count_leq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  if (spec->Arg(1)->isInt()) {
    const int64 value = spec->Arg(1)->getInt();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = model->GetIntExpr(spec->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  if (spec->Arg(2)->isInt()) {
    const int64 count = spec->Arg(2)->getInt();
    PostBooleanSumInRange(model, spec, tmp_sum, 0, count);
  } else {
    IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
    if (count->Bound()) {
      const int64 fcount = count->Min();
      PostBooleanSumInRange(model, spec, tmp_sum, 0, fcount);
    } else {
      Constraint* const ct =
          solver->MakeLessOrEqual(solver->MakeSum(tmp_sum), count);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_count_lt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  if (spec->Arg(1)->isInt()) {
    const int64 value = spec->Arg(1)->getInt();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = model->GetIntExpr(spec->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  if (spec->Arg(2)->isInt()) {
    const int64 count = spec->Arg(2)->getInt();
    PostBooleanSumInRange(model, spec, tmp_sum, 0, count - 1);
  } else {
    IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
    if (count->Bound()) {
      const int64 fcount = count->Min();
      PostBooleanSumInRange(model, spec, tmp_sum, 0, fcount - 1);
    } else {
      Constraint* const ct = solver->MakeLess(solver->MakeSum(tmp_sum), count);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_count_geq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  if (spec->Arg(1)->isInt()) {
    const int64 value = spec->Arg(1)->getInt();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = model->GetIntExpr(spec->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  if (spec->Arg(2)->isInt()) {
    const int64 count = spec->Arg(2)->getInt();
    PostBooleanSumInRange(model, spec, tmp_sum, count, tmp_sum.size());
  } else {
    IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
    if (count->Bound()) {
      const int64 fcount = count->Min();
      PostBooleanSumInRange(model, spec, tmp_sum, fcount, tmp_sum.size());
    } else {
      Constraint* const ct =
          solver->MakeGreaterOrEqual(solver->MakeSum(tmp_sum), count);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_count_gt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  if (spec->Arg(1)->isInt()) {
    const int64 value = spec->Arg(1)->getInt();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = model->GetIntExpr(spec->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  if (spec->Arg(2)->isInt()) {
    const int64 count = spec->Arg(2)->getInt();
    PostBooleanSumInRange(model, spec, tmp_sum, count + 1, tmp_sum.size());
  } else {
    IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
    if (count->Bound()) {
      const int64 fcount = count->Min();
      PostBooleanSumInRange(model, spec, tmp_sum, fcount + 1, tmp_sum.size());
    } else {
      Constraint* const ct =
          solver->MakeGreater(solver->MakeSum(tmp_sum), count);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_count_neq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  if (spec->Arg(1)->isInt()) {
    const int64 value = spec->Arg(1)->getInt();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = model->GetIntExpr(spec->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  if (spec->Arg(2)->isInt()) {
    const int64 count = spec->Arg(2)->getInt();
    if (count == 0) {
      PostBooleanSumInRange(model, spec, tmp_sum, 1, tmp_sum.size());
    } else if (count == tmp_sum.size()) {
      PostBooleanSumInRange(model, spec, tmp_sum, 0, tmp_sum.size() - 1);
    } else {
      Constraint* const ct =
          solver->MakeNonEquality(solver->MakeSum(tmp_sum), count);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeNonEquality(solver->MakeSum(tmp_sum), count);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_count_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  if (spec->Arg(1)->isInt()) {
    const int64 value = spec->Arg(1)->getInt();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualCstVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  } else {
    IntVar* const value = model->GetIntExpr(spec->Arg(1))->Var();
    for (int i = 0; i < size; ++i) {
      IntVar* const var = solver->MakeIsEqualVar(
          model->GetIntExpr(array_variables->a[i]), value);
      if (var->Max() == 1) {
        tmp_sum.push_back(var);
      }
    }
  }
  IntVar* const boolvar = model->GetIntExpr(spec->Arg(3))->Var();
  if (spec->Arg(2)->isInt()) {
    const int64 fcount = spec->Arg(2)->getInt();
    PostIsBooleanSumInRange(model, spec, tmp_sum, fcount, fcount, boolvar);
  } else {
    IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
    if (count->Bound()) {
      const int64 fcount = count->Min();
      PostIsBooleanSumInRange(model, spec, tmp_sum, fcount, fcount, boolvar);
    } else {
      Constraint* const ct =
          solver->MakeIsEqualCt(solver->MakeSum(tmp_sum), count, boolvar);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  }
}

void p_among(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  const int64 count = spec->Arg(0)->getInt();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> tmp_sum;
  AstSetLit* const domain = spec->Arg(2)->getSet();
  if (domain->interval) {
    for (int i = 0; i < size; ++i) {
      IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
      IntVar* const is_var =
          solver->MakeIsBetweenVar(var, domain->imin, domain->imax);
      if (is_var->Max() == 1) {
        tmp_sum.push_back(is_var);
      }
    }
  } else {
    for (int i = 0; i < size; ++i) {
      IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
      IntVar* const is_var = solver->MakeIsMemberVar(var, domain->s);
      if (is_var->Max() == 1) {
        tmp_sum.push_back(is_var);
      }
    }
  }
  PostBooleanSumInRange(model, spec, tmp_sum, count, count);
}

void p_global_cardinality(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int vsize = array_coefficients->a.size();
  std::vector<int64> values(vsize);
  for (int i = 0; i < vsize; ++i) {
    values[i] = array_coefficients->a[i]->getInt();
  }

  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables;
  for (int i = 0; i < size; ++i) {
    IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
    for (int v = 0; v < values.size(); ++v) {
      if (var->Contains(values[v])) {
        variables.push_back(var);
        break;
      }
    }
  }
  AstArray* const array_cards = spec->Arg(2)->getArray();
  const int csize = array_cards->a.size();
  std::vector<IntVar*> cards(csize);
  for (int i = 0; i < csize; ++i) {
    cards[i] = model->GetIntExpr(array_cards->a[i])->Var();
  }
  Constraint* const ct = solver->MakeDistribute(variables, values, cards);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
  Constraint* const ct2 =
      solver->MakeSumLessOrEqual(cards, variables.size());
  VLOG(2) << "  - posted " << ct2->DebugString();
  model->AddConstraint(spec, ct2);
}

void p_global_cardinality_closed(FlatZincModel* const model,
                                 CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int vsize = array_coefficients->a.size();
  std::vector<int64> values(vsize);
  for (int i = 0; i < vsize; ++i) {
    values[i] = array_coefficients->a[i]->getInt();
  }
  AstArray* const array_cards = spec->Arg(2)->getArray();
  const int csize = array_cards->a.size();
  std::vector<IntVar*> cards(csize);
  for (int i = 0; i < csize; ++i) {
    cards[i] = model->GetIntExpr(array_cards->a[i])->Var();
  }
  Constraint* const ct = solver->MakeDistribute(variables, values, cards);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
  for (int i = 0; i < size; ++i) {
    Constraint* const ct2 = solver->MakeMemberCt(variables[i], values);
    if (ct2 != solver->MakeTrueConstraint()) {
      VLOG(2) << "    + " << ct2->DebugString();
      model->AddConstraint(spec, ct2);
    }
  }
  Constraint* const ct3 =
      solver->MakeSumEquality(cards, variables.size());
  VLOG(2) << "  - posted " << ct3->DebugString();
  model->AddConstraint(spec, ct3);
}

void p_global_cardinality_low_up(FlatZincModel* const model,
                                 CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int vsize = array_coefficients->a.size();
  std::vector<int64> values(vsize);
  for (int i = 0; i < vsize; ++i) {
    values[i] = array_coefficients->a[i]->getInt();
  }
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables;
  for (int i = 0; i < size; ++i) {
    IntVar* const var = model->GetIntExpr(array_variables->a[i])->Var();
    for (int v = 0; v < values.size(); ++v) {
      if (var->Contains(values[v])) {
        variables.push_back(var);
        break;
      }
    }
  }
  AstArray* const array_low = spec->Arg(2)->getArray();
  const int lsize = array_low->a.size();
  std::vector<int64> low(lsize);
  for (int i = 0; i < lsize; ++i) {
    low[i] = array_low->a[i]->getInt();
  }
  AstArray* const array_up = spec->Arg(3)->getArray();
  const int usize = array_up->a.size();
  std::vector<int64> up(usize);
  for (int i = 0; i < usize; ++i) {
    up[i] = array_up->a[i]->getInt();
  }
  Constraint* const ct = solver->MakeDistribute(variables, values, low, up);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_global_cardinality_low_up_closed(FlatZincModel* const model,
                                        CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int vsize = array_coefficients->a.size();
  std::vector<int64> values(vsize);
  for (int i = 0; i < vsize; ++i) {
    values[i] = array_coefficients->a[i]->getInt();
  }
  AstArray* const array_low = spec->Arg(2)->getArray();
  const int lsize = array_low->a.size();
  std::vector<int64> low(lsize);
  for (int i = 0; i < lsize; ++i) {
    low[i] = array_low->a[i]->getInt();
  }
  AstArray* const array_up = spec->Arg(3)->getArray();
  const int usize = array_up->a.size();
  std::vector<int64> up(usize);
  for (int i = 0; i < usize; ++i) {
    up[i] = array_up->a[i]->getInt();
  }
  Constraint* const ct = solver->MakeDistribute(variables, values, low, up);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
  for (int i = 0; i < size; ++i) {
    Constraint* const ct2 = solver->MakeMemberCt(variables[i], values);
    if (ct2 != solver->MakeTrueConstraint()) {
      VLOG(2) << "    + " << ct2->DebugString();
      model->AddConstraint(spec, ct2);
    }
  }
}

void p_global_cardinality_old(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  AstArray* const array_cards = spec->Arg(1)->getArray();
  const int csize = array_cards->a.size();
  std::vector<IntVar*> cards(csize);
  for (int i = 0; i < csize; ++i) {
    cards[i] = model->GetIntExpr(array_cards->a[i])->Var();
  }
  Constraint* const ct = solver->MakeDistribute(variables, cards);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_table_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntTupleSet tuples(size);
  AstArray* const t = spec->Arg(1)->getArray();
  const int t_size = t->a.size();
  DCHECK_EQ(0, t_size % size);
  const int num_tuples = t_size / size;
  std::vector<int64> one_tuple(size);
  for (int tuple_index = 0; tuple_index < num_tuples; ++tuple_index) {
    for (int var_index = 0; var_index < size; ++var_index) {
      one_tuple[var_index] = t->a[tuple_index * size + var_index]->getInt();
    }
    tuples.Insert(one_tuple);
  }
  Constraint* const ct = solver->MakeAllowedAssignments(variables, tuples);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_table_bool(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntTupleSet tuples(size);
  AstArray* const t = spec->Arg(1)->getArray();
  const int t_size = t->a.size();
  DCHECK_EQ(0, t_size % size);
  const int num_tuples = t_size / size;
  std::vector<int64> one_tuple(size);
  for (int tuple_index = 0; tuple_index < num_tuples; ++tuple_index) {
    for (int var_index = 0; var_index < size; ++var_index) {
      one_tuple[var_index] = t->a[tuple_index * size + var_index]->getBool();
    }
    tuples.Insert(one_tuple);
  }
  Constraint* const ct = solver->MakeAllowedAssignments(variables, tuples);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_maximum_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const target = model->GetIntExpr(spec->Arg(0))->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  Constraint* const ct = solver->MakeMaxEquality(variables, target);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_minimum_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const target = model->GetIntExpr(spec->Arg(0))->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  Constraint* const ct = solver->MakeMinEquality(variables, target);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_sort(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  AstArray* const array_sorted = spec->Arg(1)->getArray();
  const int csize = array_sorted->a.size();
  std::vector<IntVar*> sorted(csize);
  for (int i = 0; i < csize; ++i) {
    sorted[i] = model->GetIntExpr(array_sorted->a[i])->Var();
  }
  Constraint* const ct = solver->MakeSortingConstraint(variables, sorted);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_fixed_cumulative(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> start_variables(size);
  for (int i = 0; i < size; ++i) {
    start_variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  AstArray* const array_durations = spec->Arg(1)->getArray();
  const int dsize = array_durations->a.size();
  std::vector<int64> durations(dsize);
  for (int i = 0; i < dsize; ++i) {
    durations[i] = array_durations->a[i]->getInt();
  }
  AstArray* const array_usages = spec->Arg(2)->getArray();
  const int usize = array_usages->a.size();
  std::vector<int64> usages(usize);
  for (int i = 0; i < usize; ++i) {
    usages[i] = array_usages->a[i]->getInt();
  }
  std::vector<IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(start_variables, durations, "",
                                            &intervals);
  const int64 capacity = spec->Arg(3)->getInt();
  Constraint* const ct =
      solver->MakeCumulative(intervals, usages, capacity, "");
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_var_cumulative(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> start_variables(size);
  for (int i = 0; i < size; ++i) {
    start_variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  AstArray* const array_durations = spec->Arg(1)->getArray();
  const int dsize = array_durations->a.size();
  std::vector<int64> durations(dsize);
  for (int i = 0; i < dsize; ++i) {
    durations[i] = array_durations->a[i]->getInt();
  }
  AstArray* const array_usages = spec->Arg(2)->getArray();
  const int usize = array_usages->a.size();
  std::vector<int64> usages(usize);
  for (int i = 0; i < usize; ++i) {
    usages[i] = array_usages->a[i]->getInt();
  }
  std::vector<IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(start_variables, durations, "",
                                            &intervals);
  IntVar* const capacity = model->GetIntExpr(spec->Arg(3))->Var();
  Constraint* const ct =
      solver->MakeCumulative(intervals, usages, capacity, "");
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_variable_cumulative(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> starts(size);
  for (int i = 0; i < size; ++i) {
    starts[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  AstArray* const array_durations = spec->Arg(1)->getArray();
  const int dsize = array_durations->a.size();
  std::vector<IntVar*> durations(dsize);
  for (int i = 0; i < dsize; ++i) {
    durations[i] = model->GetIntExpr(array_durations->a[i])->Var();
  }
  AstArray* const array_usages = spec->Arg(2)->getArray();
  const int usize = array_usages->a.size();
  std::vector<IntVar*> usages(usize);
  for (int i = 0; i < usize; ++i) {
    usages[i] = model->GetIntExpr(array_usages->a[i])->Var();
  }
  IntVar* const capacity = model->GetIntExpr(spec->Arg(3))->Var();
  if (AreAllBound(durations) && AreAllBound(usages) &&
      capacity->Bound()) {
    std::vector<int64> fdurations;
    FillValues(durations, &fdurations);
    std::vector<int64> fusages;
    FillValues(usages, &fusages);
    const int64 fcapacity = capacity->Min();
    if (AreAllOnes(fdurations) &&
        AreAllGreaterOrEqual(fusages, fcapacity / 2 + 1)) {
      // Hidden alldifferent.
      Constraint* const ct = solver->MakeAllDifferent(starts);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);

    } else {
      std::vector<IntervalVar*> intervals;
      solver->MakeFixedDurationIntervalVarArray(starts, fdurations, "",
                                                &intervals);
      if (AreAllGreaterOrEqual(fusages, fcapacity / 2 + 1)) {
        Constraint* const ct = solver->MakeDisjunctiveConstraint(intervals, "");
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      } else {
        Constraint* const ct =
            solver->MakeCumulative(intervals, fusages, fcapacity, "");
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
    }
  } else {
    Constraint* const ct =
        MakeVariableCumulative(solver, starts, durations, usages, capacity);
    VLOG(2) << "  - posted " << ct->DebugString();
    model->AddConstraint(spec, ct);
  }
}

void p_diffn(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const ax_variables = spec->Arg(0)->getArray();
  const int x_size = ax_variables->a.size();
  std::vector<IntVar*> x_variables(x_size);
  for (int i = 0; i < x_size; ++i) {
    x_variables[i] = model->GetIntExpr(ax_variables->a[i])->Var();
  }
  AstArray* const ay_variables = spec->Arg(1)->getArray();
  const int y_size = ax_variables->a.size();
  std::vector<IntVar*> y_variables(y_size);
  for (int i = 0; i < y_size; ++i) {
    y_variables[i] = model->GetIntExpr(ay_variables->a[i])->Var();
  }
  AstArray* const ax_sizes = spec->Arg(2)->getArray();
  const int xs_size = ax_sizes->a.size();
  std::vector<IntVar*> x_sizes(xs_size);
  for (int i = 0; i < xs_size; ++i) {
    x_sizes[i] = model->GetIntExpr(ax_sizes->a[i])->Var();
  }
  AstArray* const ay_sizes = spec->Arg(3)->getArray();
  const int ys_size = ay_sizes->a.size();
  std::vector<IntVar*> y_sizes(ys_size);
  for (int i = 0; i < ys_size; ++i) {
    y_sizes[i] = model->GetIntExpr(ay_sizes->a[i])->Var();
  }
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(
          x_variables, y_variables, x_sizes, y_sizes);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_true_constraint(FlatZincModel* const model, CtSpec* const) {}

void p_sliding_sum(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  const int low = spec->Arg(0)->getInt();
  const int up = spec->Arg(1)->getInt();
  const int seq = spec->Arg(2)->getInt();
  AstArray* const array_variables = spec->Arg(3)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  for (int i = 0; i < size - seq; ++i) {
    std::vector<IntVar*> tmp(seq);
    for (int k = 0; k < seq; ++k) {
      tmp[k] = variables[i + k];
    }
    IntVar* const sum_var = solver->MakeSum(tmp)->Var();
    sum_var->SetRange(low, up);
  }
}

void p_regular(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();

  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }

  const int64 num_states = spec->Arg(1)->getInt();

  const int64 num_values = spec->Arg(2)->getInt();

  const AstArray* const array_transitions = spec->Arg(3)->getArray();
  IntTupleSet tuples(3);
  int count = 0;
  for (int q = 1; q <= num_states; ++q) {
    for (int s = 1; s <= num_values; ++s) {
      const int64 next = array_transitions->a[count++]->getInt();
      if (next != 0) {
        tuples.Insert3(q, s, next);
      }
    }
  }

  const int64 initial_state = spec->Arg(4)->getInt();

  AstSetLit* const final_set = spec->Arg(5)->getSet();
  std::vector<int64> final_states;
  if (final_set->interval) {
    for (int v = final_set->imin; v <= final_set->imax; ++v) {
      final_states.push_back(v);
    }
  } else {
    final_states.insert(
        final_states.end(), final_set->s.begin(), final_set->s.end());
  }
  Constraint* const ct = solver->MakeTransitionConstraint(
      variables, tuples, initial_state, final_states);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_lex_less_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_left = spec->Arg(0)->getArray();
  const int size = array_left->a.size();
  std::vector<IntVar*> left(size);
  for (int i = 0; i < size; ++i) {
    left[i] = model->GetIntExpr(array_left->a[i])->Var();
  }

  AstArray* const array_right = spec->Arg(1)->getArray();
  std::vector<IntVar*> right(size);
  for (int i = 0; i < size; ++i) {
    right[i] = model->GetIntExpr(array_right->a[i])->Var();
  }
  Constraint* const ct = solver->MakeLexicalLess(left, right);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_lex_lesseq_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_left = spec->Arg(0)->getArray();
  const int size = array_left->a.size();
  std::vector<IntVar*> left(size);
  for (int i = 0; i < size; ++i) {
    left[i] = model->GetIntExpr(array_left->a[i])->Var();
  }

  AstArray* const array_right = spec->Arg(1)->getArray();
  std::vector<IntVar*> right(size);
  for (int i = 0; i < size; ++i) {
    right[i] = model->GetIntExpr(array_right->a[i])->Var();
  }
  Constraint* const ct = solver->MakeLexicalLessOrEqual(left, right);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_inverse(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();

  AstArray* const array_left = spec->Arg(0)->getArray();
  const int size = array_left->a.size();
  std::vector<IntVar*> left(size + 1);
  // Account for 1 based arrays.
  left[0] = solver->MakeIntConst(0);
  for (int i = 0; i < size; ++i) {
    left[i + 1] = model->GetIntExpr(array_left->a[i])->Var();
  }

  AstArray* const array_right = spec->Arg(1)->getArray();
  std::vector<IntVar*> right(size + 1);
  right[0] = solver->MakeIntConst(0);
  for (int i = 0; i < size; ++i) {
    right[i + 1] = model->GetIntExpr(array_right->a[i])->Var();
  }
  Constraint* const ct = solver->MakeInversePermutationConstraint(left, right);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_nvalue(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();

  AstArray* const array_vars = spec->Arg(1)->getArray();
  const int size = array_vars->a.size();
  std::vector<IntVar*> vars(size);
  for (int i = 0; i < size; ++i) {
    vars[i] = model->GetIntExpr(array_vars->a[i])->Var();
  }
  int64 lb = kint64max;
  int64 ub = kint64min;
  for (int i = 0; i < size; ++i) {
    lb = std::min(lb, vars[i]->Min());
    ub = std::max(ub, vars[i]->Max());
  }
  int csize = ub - lb + 1;
  int64 always_true_cards = 0;
  std::vector<IntVar*> cards;
  for (int b = 0; b < csize; ++b) {
    const int value = lb + b;
    std::vector<IntVar*> contributors;
    bool always_true = false;
    for (int i = 0; i < size; ++i) {
      if (vars[i]->Contains(value)) {
        if (vars[i]->Bound()) {
          always_true = true;
          break;
        } else {
          contributors.push_back(vars[i]->IsEqual(value));
        }
      }
    }
    if (always_true) {
      always_true_cards++;
    } else if (contributors.size() == 1) {
      cards.push_back(contributors.back());
    } else if (contributors.size() > 1) {
      IntVar* const contribution = solver->MakeBoolVar();
      if (FLAGS_use_sat &&
          AddBoolOrArrayEqVar(model->Sat(), contributors, contribution)) {
        VLOG(2) << "  - posted to sat";
      } else {
        Constraint* const ct =
            solver->MakeMaxEquality(contributors, contribution);
        VLOG(2) << "  - posted " << ct->DebugString();
        model->AddConstraint(spec, ct);
      }
      cards.push_back(contribution);
    }
  }
  if (spec->Arg(0)->isInt()) {
    const int64 card = spec->Arg(0)->getInt() - always_true_cards;
    PostBooleanSumInRange(model, spec, cards, card, card);
  } else {
    IntVar* const card = model->GetIntExpr(spec->Arg(0))->Var();
    Constraint* const ct = solver->MakeSumEquality(
        cards, solver->MakeSum(card, -always_true_cards)->Var());
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
  }
}

void p_circuit(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    // Create variables. Account for 1-based array indexing.
    variables[i] =
        solver->MakeSum(model->GetIntExpr(array_variables->a[i]), -1)->Var();
  }
  Constraint* const ct = solver->MakeCircuit(variables);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

void p_subcircuit(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    // Create variables. Account for 1-based array indexing.
    variables[i] =
        solver->MakeSum(model->GetIntExpr(array_variables->a[i]), -1)->Var();
  }
  Constraint* const ct = solver->MakeSubCircuit(variables);
  VLOG(2) << "  - posted " << ct->DebugString();
  model->AddConstraint(spec, ct);
}

class IntBuilder {
 public:
  IntBuilder(void) {
    global_model_builder.Register("int_eq", &p_int_eq);
    global_model_builder.Register("int_ne", &p_int_ne);
    global_model_builder.Register("int_ge", &p_int_ge);
    global_model_builder.Register("int_gt", &p_int_gt);
    global_model_builder.Register("int_le", &p_int_le);
    global_model_builder.Register("int_lt", &p_int_lt);
    global_model_builder.Register("int_eq_reif", &p_int_eq_reif);
    global_model_builder.Register("int_ne_reif", &p_int_ne_reif);
    global_model_builder.Register("int_ge_reif", &p_int_ge_reif);
    global_model_builder.Register("int_gt_reif", &p_int_gt_reif);
    global_model_builder.Register("int_le_reif", &p_int_le_reif);
    global_model_builder.Register("int_lt_reif", &p_int_lt_reif);
    global_model_builder.Register("int_lin_eq", &p_int_lin_eq);
    global_model_builder.Register("int_lin_eq_reif", &p_int_lin_eq_reif);
    global_model_builder.Register("int_lin_ne", &p_int_lin_ne);
    global_model_builder.Register("int_lin_ne_reif", &p_int_lin_ne_reif);
    global_model_builder.Register("int_lin_le", &p_int_lin_le);
    global_model_builder.Register("int_lin_le_reif", &p_int_lin_le_reif);
    global_model_builder.Register("int_lin_ge", &p_int_lin_ge);
    global_model_builder.Register("int_lin_ge_reif", &p_int_lin_ge_reif);
    global_model_builder.Register("int_plus", &p_int_plus);
    global_model_builder.Register("int_minus", &p_int_minus);
    global_model_builder.Register("int_times", &p_int_times);
    global_model_builder.Register("int_div", &p_int_div);
    global_model_builder.Register("int_mod", &p_int_mod);
    global_model_builder.Register("int_min", &p_int_min);
    global_model_builder.Register("int_max", &p_int_max);
    global_model_builder.Register("int_abs", &p_abs);
    global_model_builder.Register("int_negate", &p_int_negate);
    global_model_builder.Register("bool_eq", &p_bool_eq);
    global_model_builder.Register("bool_eq_reif", &p_bool_eq_reif);
    global_model_builder.Register("bool_ne", &p_bool_ne);
    global_model_builder.Register("bool_ne_reif", &p_bool_ne_reif);
    global_model_builder.Register("bool_ge", &p_bool_ge);
    global_model_builder.Register("bool_ge_reif", &p_bool_ge_reif);
    global_model_builder.Register("bool_le", &p_bool_le);
    global_model_builder.Register("bool_le_reif", &p_bool_le_reif);
    global_model_builder.Register("bool_gt", &p_bool_gt);
    global_model_builder.Register("bool_gt_reif", &p_bool_gt_reif);
    global_model_builder.Register("bool_lt", &p_bool_lt);
    global_model_builder.Register("bool_lt_reif", &p_bool_lt_reif);
    global_model_builder.Register("bool_or", &p_bool_or);
    global_model_builder.Register("bool_and", &p_bool_and);
    global_model_builder.Register("bool_xor", &p_bool_xor);
    global_model_builder.Register("array_bool_and", &p_array_bool_and);
    global_model_builder.Register("array_bool_or", &p_array_bool_or);
    global_model_builder.Register("array_bool_xor", &p_array_bool_xor);
    global_model_builder.Register("bool_lin_eq", &p_bool_lin_eq);
    global_model_builder.Register("bool_lin_le", &p_bool_lin_le);
    global_model_builder.Register("bool_clause", &p_array_bool_clause);
    global_model_builder.Register("bool_left_imp", &p_bool_le);
    global_model_builder.Register("bool_right_imp", &p_bool_ge);
    global_model_builder.Register("bool_not", &p_bool_not);
    global_model_builder.Register("array_int_element", &p_array_int_element);
    global_model_builder.Register("array_var_int_element",
                                  &p_array_var_int_element);
    global_model_builder.Register("array_var_int_position",
                                  &p_array_var_int_position);
    global_model_builder.Register("array_bool_element", &p_array_bool_element);
    global_model_builder.Register("array_var_bool_element",
                                  &p_array_var_bool_element);
    global_model_builder.Register("bool2bool", &p_bool2bool);
    global_model_builder.Register("bool2int", &p_bool2int);
    global_model_builder.Register("int2int", &p_int2int);
    global_model_builder.Register("int_in", &p_int_in);
    global_model_builder.Register("all_different_int", &p_all_different_int);
    global_model_builder.Register("alldifferent_except_0",
                                  &p_alldifferent_except_0);
    global_model_builder.Register("count_eq", &p_count_eq);
    global_model_builder.Register("count_leq", &p_count_leq);
    global_model_builder.Register("count_lt", &p_count_lt);
    global_model_builder.Register("count_geq", &p_count_geq);
    global_model_builder.Register("count_gt", &p_count_gt);
    global_model_builder.Register("count_neq", &p_count_neq);
    global_model_builder.Register("count_reif", &p_count_reif);
    global_model_builder.Register("global_cardinality", &p_global_cardinality);
    global_model_builder.Register("global_cardinality_closed",
                                  &p_global_cardinality_closed);
    global_model_builder.Register("global_cardinality_low_up",
                                  &p_global_cardinality_low_up);
    global_model_builder.Register("global_cardinality_low_up_closed",
                                  &p_global_cardinality_low_up_closed);
    global_model_builder.Register("global_cardinality_old",
                                  &p_global_cardinality_old);
    global_model_builder.Register("table_int", &p_table_int);
    global_model_builder.Register("table_bool", &p_table_bool);
    global_model_builder.Register("maximum_int", &p_maximum_int);
    global_model_builder.Register("minimum_int", &p_minimum_int);
    global_model_builder.Register("sort", &p_sort);
    global_model_builder.Register("fixed_cumulative", &p_fixed_cumulative);
    global_model_builder.Register("var_cumulative", &p_var_cumulative);
    global_model_builder.Register("variable_cumulative",
                                  &p_variable_cumulative);
    global_model_builder.Register("true_constraint", &p_true_constraint);
    global_model_builder.Register("sliding_sum", &p_sliding_sum);
    global_model_builder.Register("diffn", &p_diffn);
    global_model_builder.Register("regular", &p_regular);
    global_model_builder.Register("lex_less_int", &p_lex_less_int);
    global_model_builder.Register("lex_less_bool", &p_lex_less_int);
    global_model_builder.Register("lex_lesseq_int", &p_lex_less_int);
    global_model_builder.Register("lex_lesseq_bool", &p_lex_less_int);
    global_model_builder.Register("inverse", &p_inverse);
    global_model_builder.Register("nvalue", &p_nvalue);
    global_model_builder.Register("circuit", &p_circuit);
    global_model_builder.Register("subcircuit", &p_subcircuit);
    global_model_builder.Register("among", &p_among);
  }
};
IntBuilder __int_Builder;

void p_set_in(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const var = model->GetIntExpr(spec->Arg(0))->Var();
  if (spec->Arg(1)->isSet()) {
    AstSetLit* const domain = spec->Arg(1)->getSet();
    if (domain->interval) {
      Constraint* const ct =
          solver->MakeBetweenCt(var, domain->imin, domain->imax);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    } else {
      Constraint* const ct = solver->MakeMemberCt(var, domain->s);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    throw FzError("ModelBuilder", std::string("Constraint ") + spec->Id() +
                                      " does not support variable sets");
  }
}
void p_set_in_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const var = model->GetIntExpr(spec->Arg(0))->Var();
  IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
  if (spec->Arg(1)->isSet()) {
    AstSetLit* const domain = spec->Arg(1)->getSet();
    if (domain->interval) {
      Constraint* const ct =
          solver->MakeIsBetweenCt(var, domain->imin, domain->imax, target);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    } else {
      Constraint* const ct = solver->MakeIsMemberCt(var, domain->s, target);
      VLOG(2) << "  - posted " << ct->DebugString();
      model->AddConstraint(spec, ct);
    }
  } else {
    throw FzError("ModelBuilder", std::string("Constraint ") + spec->Id() +
                                      " does not support variable sets");
  }
}

class SetBuilder {
 public:
  SetBuilder(void) {
    global_model_builder.Register("set_in", &p_set_in);
    global_model_builder.Register("set_in_reif", &p_set_in_reif);
  }
};
SetBuilder __set_Builder;
}  // namespace

void FlatZincModel::PostConstraint(CtSpec* const spec) {
  try {
    global_model_builder.Post(this, spec);
  }
  catch (AstTypeError & e) {
    throw FzError("Type error", e.what());
  }
}  // namespace
}  // namespace operations_research
