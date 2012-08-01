/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Mikael Lagerkvist <lagerkvist@gmail.com>
 *
 *  Modified:
 *     Laurent Perron <lperron@google.com>
 *
 *
 *  Copyright:
 *     Guido Tack, 2007
 *     Mikael Lagerkvist, 2009
 *
 *  Last modified:
 *     $Date: 2010-07-02 19:18:43 +1000 (Fri, 02 Jul 2010) $ by $Author: tack $
 *     $Revision: 11149 $
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

DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_trace_propagation);
DEFINE_bool(use_minisat, true, "Use minisat in boolean formulas");

namespace operations_research {
class SatPropagator;
bool AddBoolEq(SatPropagator* const sat,
               IntExpr* const left,
               IntExpr* const right);

bool AddBoolLe(SatPropagator* const sat,
               IntExpr* const left,
               IntExpr* const right);

bool AddBoolNot(SatPropagator* const sat,
                IntExpr* const left,
                IntExpr* const right);

bool AddBoolAndArrayEqVar(SatPropagator* const sat,
                          const std::vector<IntVar*>& vars,
                          IntExpr* const target);

bool AddBoolOrArrayEqVar(SatPropagator* const sat,
                         const std::vector<IntVar*>& vars,
                         IntExpr* const target);

bool AddBoolAndEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target);

bool AddBoolIsNEqVar(SatPropagator* const sat,
                     IntExpr* const left,
                     IntExpr* const right,
                     IntExpr* const target);

bool AddBoolIsLeVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target);

bool AddBoolOrEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target);

bool AddBoolIsEqVar(SatPropagator* const sat,
                    IntExpr* const left,
                    IntExpr* const right,
                    IntExpr* const target);


bool AddBoolOrArrayEqualTrue(SatPropagator* const sat,
                             const std::vector<IntVar*>& vars);

bool AddBoolAndArrayEqualFalse(SatPropagator* const sat,
                               const std::vector<IntVar*>& vars);

extern bool HasDomainAnnotation(AstNode* const annotations);
namespace {
// Help

Constraint* MakeStrongScalProdEquality(Solver* const solver,
                                       std::vector<IntVar*>& variables,
                                       std::vector<int64>& coefficients,
                                       int64 rhs) {
  const bool trace = FLAGS_cp_trace_search;
  const bool propag = FLAGS_cp_trace_propagation;
  FLAGS_cp_trace_search = false;
  FLAGS_cp_trace_propagation = false;
  const int size = variables.size();
  IntTupleSet tuples(size);
  Solver s("build");
  std::vector<IntVar*> copy_vars(size);
  for (int i = 0; i < size; ++i) {
    copy_vars[i] = s.MakeIntVar(variables[i]->Min(), variables[i]->Max());
  }
  s.AddConstraint(s.MakeScalProdEquality(copy_vars, coefficients, rhs));
  s.NewSearch(s.MakePhase(copy_vars,
                          Solver::CHOOSE_FIRST_UNBOUND,
                          Solver::ASSIGN_MIN_VALUE));
  while (s.NextSolution()) {
    std::vector<int> one_tuple(size);
    for (int i = 0; i < size; ++i) {
      one_tuple[i] = copy_vars[i]->Value();
    }
    tuples.Insert(one_tuple);
  }
  s.EndSearch();
  FLAGS_cp_trace_search = trace;
  FLAGS_cp_trace_propagation = propag;
  return solver->MakeAllowedAssignments(variables, tuples);
}

/// Map from constraint identifier to constraint posting functions
class ModelBuilder {
 public:
  /// Type of constraint posting function
  typedef void (*Builder) (FlatZincModel* const model, CtSpec* const spec);
  /// Add posting function \a p with identifier \a id
  void Register(const std::string& id, Builder p);
  /// Post constraint specified by \a ce
  void Post(FlatZincModel* const model, CtSpec* const spec);

 private:
  /// The actual builders.
  std::map<std::string, Builder> r;
};

static ModelBuilder global_model_builder;

void ModelBuilder::Post(FlatZincModel* const model, CtSpec* const spec) {
  if (!spec->Nullified()) {
    const string& id = spec->Id();
    std::map<std::string, Builder>::iterator i = r.find(id);
    if (i == r.end()) {
      throw Error("ModelBuilder",
                  std::string("Constraint ") + spec->Id() + " not found");
    }
    i->second(model, spec);
  }
}

void ModelBuilder::Register(const std::string& id, Builder p) {
  r[id] = p;
}

void p_int_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  if (spec->IsDefined(spec->Arg(0))) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    VLOG(1) << "  - creating " << spec->Arg(0)->DebugString() << " := "
            << right->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(0));
    model->SetIntegerExpression(spec->Arg(0), right);
  } else if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_ne(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct =
        solver->MakeNonEquality(left, spec->Arg(1)->getInt());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeNonEquality(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_ge(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct =
        solver->MakeGreaterOrEqual(left, spec->Arg(1)->getInt());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeGreaterOrEqual(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_gt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct = solver->MakeGreater(left, spec->Arg(1)->getInt());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeGreater(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct =
        solver->MakeLessOrEqual(left, spec->Arg(1)->getInt());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeLessOrEqual(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    LOG(INFO) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_lt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct = solver->MakeLess(left, spec->Arg(1)->getInt());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeLess(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
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
        node_right->isInt() ?
        solver->MakeIsEqualCstVar(left, node_right->getInt()) :
        solver->MakeIsEqualVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(node_right);
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsEqualCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt() ?
        solver->MakeIsDifferentCstVar(left, node_right->getInt()) :
        solver->MakeIsDifferentVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsDifferentCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt() ?
        solver->MakeIsGreaterOrEqualCstVar(left, node_right->getInt()) :
        solver->MakeIsGreaterOrEqualVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt() ?
        solver->MakeIsGreaterCstVar(left, node_right->getInt()) :
        solver->MakeIsGreaterVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsGreaterCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt() ?
        solver->MakeIsLessOrEqualCstVar(left, node_right->getInt()) :
        solver->MakeIsLessOrEqualVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsLessOrEqualCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isInt() ?
        solver->MakeIsLessCstVar(left, node_right->getInt()) :
        solver->MakeIsLessVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsLessCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_lin_eq(FlatZincModel* const model, CtSpec* const spec) {
  bool strong_propagation = HasDomainAnnotation(spec->annotations());
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  if (spec->DefinedArg() != NULL) {
    std::vector<int64> coefficients;
    std::vector<IntVar*> variables;
    int64 constant = 0;
    AstNode* defined = NULL;
    for (int i = 0; i < size; ++i) {
      if (array_variables->a[i]->isInt()) {
        constant += array_coefficients->a[i]->getInt() *
                    array_variables->a[i]->getInt();
      } else if (spec->IsDefined(array_variables->a[i])) {
        CHECK(defined == NULL);
        defined = array_variables->a[i];
        if (array_coefficients->a[i]->getInt() != -1) {
          throw Error("ModelBuilder",
                      std::string("Constraint ") + spec->Id() +
                      " cannot define an integer variable with a coefficient"
                      " different from -1");
        }
      } else {
        coefficients.push_back(array_coefficients->a[i]->getInt());
        variables.push_back(model->GetIntExpr(array_variables->a[i])->Var());
      }
    }
    if (constant - rhs != 0) {
      coefficients.push_back(constant - rhs);
      variables.push_back(solver->MakeIntConst(1));
    }
    IntExpr* const target =
        solver->MakeScalProd(variables, coefficients);
    VLOG(1) << "  - creating " << defined->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(defined);
    model->SetIntegerExpression(defined, target);
  } else {
    std::vector<int64> coefficients(size);
    std::vector<IntVar*> variables(size);
    int ones = 0;
    for (int i = 0; i < size; ++i) {
      coefficients[i] = array_coefficients->a[i]->getInt();
      ones += coefficients[i] == 1;
      variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
    }
    Constraint* const ct =
        strong_propagation ?
        MakeStrongScalProdEquality(solver, variables, coefficients, rhs) :
        (ones == size ?
         solver->MakeSumEquality(variables, rhs) :
         solver->MakeScalProdEquality(variables, coefficients, rhs));
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
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
  std::vector<int> coefficients(size);

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
    IntExpr* pos = NULL;
    IntExpr* other = NULL;
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
      VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
              << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsEqualCt(pos, other, boolvar);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else if (negative == 1) {
    IntExpr* neg = NULL;
    IntExpr* other = NULL;
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
      VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
              << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsEqualCt(neg, other, boolvar);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else {
    std::vector<IntVar*> variables(size);
    for (int i = 0; i < size; ++i) {
      coefficients[i] = array_coefficients->a[i]->getInt();
      variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
    }
    IntExpr* const var = solver->MakeScalProd(variables, coefficients);
    if (define) {
      IntVar* const boolvar = solver->MakeIsEqualCstVar(var, rhs);
      VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
              << boolvar->DebugString();
      model->CheckIntegerVariableIsNull(node_boolvar);
      model->SetIntegerExpression(node_boolvar, boolvar);
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      Constraint* const ct = solver->MakeIsEqualCstCt(var, rhs, boolvar);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
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
  Constraint* const ct =
      solver->MakeNonEquality(
          solver->MakeScalProd(variables, coefficients),
          rhs);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntExpr* const var =
      solver->MakeScalProd(variables, coefficients);
  IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
  Constraint* const ct = solver->MakeIsDifferentCstCt(var, rhs, boolvar);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_le(FlatZincModel* const model, CtSpec* const spec) {
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
  Constraint* const ct =
      solver->MakeScalProdLessOrEqual(variables, coefficients, rhs);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntExpr* const expr = solver->MakeScalProd(variables, coefficients);
  IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
  Constraint* const ct = solver->MakeIsLessOrEqualCstCt(expr, rhs, boolvar);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_lt(FlatZincModel* const model, CtSpec* const spec) {
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
  Constraint* const ct =
      solver->MakeScalProdLessOrEqual(variables, coefficients, rhs - 1);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntExpr* const expr =
      solver->MakeScalProd(variables, coefficients);
  IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
  Constraint* const ct = solver->MakeIsLessCstCt(expr, rhs, boolvar);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  Constraint* const ct =
      solver->MakeScalProdGreaterOrEqual(variables, coefficients, rhs);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntExpr* const expr = solver->MakeScalProd(variables, coefficients);
  IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
  Constraint* const ct = solver->MakeIsGreaterOrEqualCstCt(expr, rhs, boolvar);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_gt(FlatZincModel* const model, CtSpec* const spec) {
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
  Constraint* const ct =
      solver->MakeScalProdGreaterOrEqual(variables, coefficients, rhs + 1);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_coefficients = spec->Arg(0)->getArray();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  AstNode* const node_rhs = spec->Arg(2);
  AstNode* const node_boolvar = spec->Arg(3);
  const int64 rhs = node_rhs->getInt();
  const int size = array_coefficients->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int64> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntExpr* const expr = solver->MakeScalProd(variables, coefficients);
  IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
  Constraint* const ct = solver->MakeIsGreaterCstCt(expr, rhs, boolvar);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* arithmetic constraints */

void p_int_plus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  if (spec->IsDefined(spec->Arg(0))) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    IntExpr* const left = solver->MakeDifference(target, right);
    VLOG(1) << "  - creating " << spec->Arg(0)->DebugString() << " := "
            << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(0));
    model->SetIntegerExpression(spec->Arg(0), left);
  } else if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    IntExpr* const right = solver->MakeDifference(target, left);
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << right->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), right);
  } else if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntExpr* const target = solver->MakeSum(left, right);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeSum(left, right), target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_minus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeDifference(left, right);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeDifference(left, right), target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_times(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeProd(left, right);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeProd(left, right), target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
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
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    Constraint* const ct =
        solver->MakeEquality(
            solver->MakeDiv(left, spec->Arg(1)->getInt()), target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_mod(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntExpr(spec->Arg(0))->Var();
  IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
  if (spec->Arg(1)->isIntVar()) {
    IntVar* const mod = model->GetIntExpr(spec->Arg(1))->Var();
    Constraint* const ct = solver->MakeModuloConstraint(left, mod, target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    const int64 mod = spec->Arg(1)->getInt();
    Constraint* const ct = solver->MakeModuloConstraint(left, mod, target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_min(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMin(left, right);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeMin(left, right), target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_max(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMax(left, right);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeMax(left, right), target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_negate(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const target = model->GetIntExpr(spec->Arg(2));
  Constraint* const ct =
      solver->MakeEquality(solver->MakeOpposite(left), target);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

// Bools

void p_bool_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  if (spec->IsDefined(spec->Arg(0))) {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    VLOG(1) << "  - creating " << spec->Arg(0)->DebugString() << " := "
            << right->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(0));
    model->SetIntegerExpression(spec->Arg(0), right);
  } else if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const left = model->GetIntExpr(spec->Arg(0));
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_minisat && AddBoolEq(model->Sat(), left, right)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct = solver->MakeEquality(left, right);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  }
}

void p_bool_ne(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct =
        solver->MakeNonEquality(left, spec->Arg(1)->getBool());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_minisat && AddBoolNot(model->Sat(), left, right)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct = solver->MakeNonEquality(left, right);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  }
}

void p_bool_ge(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct =
        solver->MakeGreaterOrEqual(left, spec->Arg(1)->getBool());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_minisat && AddBoolLe(model->Sat(), right, left)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct = solver->MakeGreaterOrEqual(left, right);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  }
}

void p_bool_gt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct = solver->MakeGreater(left, spec->Arg(1)->getBool());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeGreater(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_bool_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct =
        solver->MakeLessOrEqual(left, spec->Arg(1)->getBool());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_minisat && AddBoolLe(model->Sat(), left, right)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct = solver->MakeLessOrEqual(left, right);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  }
}

void p_bool_lt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->Arg(1)->isBool()) {
    Constraint* const ct = solver->MakeLess(left, spec->Arg(1)->getBool());
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeLess(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
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
        node_right->isBool() ?
        solver->MakeIsEqualCstVar(left, node_right->getBool()) :
        solver->MakeIsEqualVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(node_right);
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_minisat &&
        AddBoolIsEqVar(model->Sat(), left, right, boolvar)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct = solver->MakeIsEqualCt(left, right, boolvar);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
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
        node_right->isBool() ?
        solver->MakeIsDifferentCstVar(left, node_right->getBool()) :
        solver->MakeIsDifferentVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_minisat &&
        AddBoolIsNEqVar(model->Sat(), left, right, boolvar)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct = solver->MakeIsDifferentCt(left, right, boolvar);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
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
        node_right->isBool() ?
        solver->MakeIsGreaterOrEqualCstVar(left, node_right->getBool()) :
        solver->MakeIsGreaterOrEqualVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_minisat &&
        AddBoolIsLeVar(model->Sat(), right, left, boolvar)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct =
          solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
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
        node_right->isBool() ?
        solver->MakeIsGreaterCstVar(left, node_right->getBool()) :
        solver->MakeIsGreaterVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsGreaterCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_bool_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  AstNode* const node_right = spec->Arg(1);
  AstNode* const node_boolvar = spec->Arg(2);
  if (spec->IsDefined(node_boolvar)) {
    IntVar* const boolvar =
        node_right->isBool() ?
        solver->MakeIsLessOrEqualCstVar(left, node_right->getBool()) :
        solver->MakeIsLessOrEqualVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    if (FLAGS_use_minisat &&
        AddBoolIsLeVar(model->Sat(), left, right, boolvar)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct = solver->MakeIsLessOrEqualCt(left, right, boolvar);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
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
        node_right->isBool() ?
        solver->MakeIsLessCstVar(left, node_right->getBool()) :
        solver->MakeIsLessVar(left, model->GetIntExpr(node_right));
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
    CHECK_NOTNULL(boolvar);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
    Constraint* const ct = solver->MakeIsLessCt(left, right, boolvar);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_bool_and(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMin(left, right);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    if (FLAGS_use_minisat &&
        AddBoolAndEqVar(model->Sat(), left, right, target)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct =
          solver->MakeEquality(solver->MakeMin(left, right), target);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  }
}

void p_bool_or(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeMax(left, right);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(2));
    if (FLAGS_use_minisat &&
        AddBoolOrEqVar(model->Sat(), left, right, target)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct =
          solver->MakeEquality(solver->MakeMax(left, right), target);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
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
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
  } else if (node_boolvar->isBool() && node_boolvar->getBool() == 0) {
    VLOG(1) << "  - forcing array_bool_or to 0";
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(0);
    }
  } else {
    if (node_boolvar->isBool()) {
      if (node_boolvar->getBool() == 1) {
        if (FLAGS_use_minisat &&
            AddBoolOrArrayEqualTrue(model->Sat(), variables)) {
          VLOG(1) << "  - posted to minisat";
        } else {
          Constraint* const ct = solver->MakeSumGreaterOrEqual(variables, 1);
          VLOG(1) << "  - posted " << ct->DebugString();
          solver->AddConstraint(ct);
        }
      } else {
        Constraint* const ct = solver->MakeSumEquality(variables, Zero());
        VLOG(1) << "  - posted " << ct->DebugString();
        solver->AddConstraint(ct);
      }
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      if (FLAGS_use_minisat &&
          AddBoolOrArrayEqVar(model->Sat(), variables, boolvar)) {
        VLOG(1) << "  - posted to minisat";
      } else {
        Constraint* const ct = solver->MakeMaxEquality(variables, boolvar);
        VLOG(1) << "  - posted " << ct->DebugString();
        solver->AddConstraint(ct);
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
    IntExpr* const boolvar = solver->MakeMin(variables)->Var();
    VLOG(1) << "  - creating " << node_boolvar->DebugString() << " := "
            << boolvar->DebugString();
    model->CheckIntegerVariableIsNull(node_boolvar);
    model->SetIntegerExpression(node_boolvar, boolvar);
  } else if (node_boolvar->isBool() && node_boolvar->getBool() == 1) {
    VLOG(1) << "  - forcing array_bool_and to 1";
    for (int i = 0; i < variables.size(); ++i) {
      variables[i]->SetValue(1);
    }
  } else {
    if (node_boolvar->isBool()) {
      if (node_boolvar->getBool() == 0) {
        if (FLAGS_use_minisat &&
            AddBoolAndArrayEqualFalse(model->Sat(), variables)) {
          VLOG(1) << "  - posted to minisat";
        } else {
          Constraint* const ct =
              solver->MakeSumLessOrEqual(variables, variables.size() - 1);
          VLOG(1) << "  - posted " << ct->DebugString();
          solver->AddConstraint(ct);
        }
      } else {
        Constraint* const ct =
            solver->MakeSumEquality(variables, variables.size());
        VLOG(1) << "  - posted " << ct->DebugString();
        solver->AddConstraint(ct);
      }
    } else {
      IntVar* const boolvar = model->GetIntExpr(node_boolvar)->Var();
      if (FLAGS_use_minisat &&
          AddBoolAndArrayEqVar(model->Sat(), variables, boolvar)) {
        VLOG(1) << "  - posted to minisat";
      } else {
        Constraint* const ct = solver->MakeMinEquality(variables, boolvar);
        VLOG(1) << "  - posted " << ct->DebugString();
        solver->AddConstraint(ct);
      }
    }
  }
}

void p_array_bool_clause(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "array_bool_clause(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_array_bool_clause_reif(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "array_bool_clause_reif(" << (spec->Arg(0)->DebugString())
             << "," << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_xor(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  IntExpr* const right = model->GetIntExpr(spec->Arg(1));
  IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
  if (FLAGS_use_minisat && AddBoolIsNEqVar(model->Sat(), left, right, target)) {
    VLOG(1) << "  - posted to minisat";
  } else {
    Constraint* const ct =
        solver->MakeIsEqualCstCt(solver->MakeSum(left, right), 1, target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_bool_not(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->IsDefined(spec->Arg(1))) {
    IntExpr* const target = solver->MakeDifference(1, left);
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), target);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    if (FLAGS_use_minisat && AddBoolNot(model->Sat(), left, right)) {
      VLOG(1) << "  - posted to minisat";
    } else {
      Constraint* const ct =
          solver->MakeEquality(solver->MakeDifference(1, left), right);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  }
}

/* element constraints */
void p_array_int_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index =  model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int size = array_coefficients->a.size();
  std::vector<int64> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getInt();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(coefficients, shifted_index);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(coefficients, shifted_index, target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_array_var_int_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index =  model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(variables, shifted_index);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(variables, shifted_index, target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_array_var_int_position(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index =  model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  const int target = spec->Arg(2)->getInt();
  Constraint* const ct =
      solver->MakeEquality(
          shifted_index, solver->MakeIndexExpression(variables, target)->Var());
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index = model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_coefficients = spec->Arg(1)->getArray();
  const int size = array_coefficients->a.size();
  std::vector<int> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficients->a[i]->getBool();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(coefficients, shifted_index);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(coefficients, shifted_index, target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_array_var_bool_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const index =  model->GetIntExpr(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AstArray* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  if (spec->IsDefined(spec->Arg(2))) {
    IntExpr* const target = solver->MakeElement(variables, shifted_index);
    VLOG(1) << "  - creating " << spec->Arg(2)->DebugString() << " := "
            << target->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(2));
    model->SetIntegerExpression(spec->Arg(2), target);
  } else {
    IntVar* const target = model->GetIntExpr(spec->Arg(2))->Var();
    Constraint* const ct =
        solver->MakeElementEquality(variables, shifted_index, target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}



/* coercion constraints */
void p_bool2int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->IsDefined(spec->Arg(1))) {
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_bool2bool(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  CHECK_NOTNULL(left);
  if (spec->IsDefined(spec->Arg(1))) {
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int2int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->IsDefined(spec->Arg(1))) {
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << left->DebugString();
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    model->SetIntegerExpression(spec->Arg(1), left);
  } else {
    IntExpr* const right = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
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
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else {
    Constraint* const ct = solver->MakeMemberCt(var, domain->s);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_abs(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntExpr* const left = model->GetIntExpr(spec->Arg(0));
  if (spec->IsDefined(spec->Arg(1))) {
    model->CheckIntegerVariableIsNull(spec->Arg(1));
    IntExpr* const target = solver->MakeAbs(left);
    VLOG(1) << "  - creating " << spec->Arg(1)->DebugString() << " := "
            << target->DebugString();
    model->SetIntegerExpression(spec->Arg(1), target);
  } else {
    IntExpr* const target = model->GetIntExpr(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(solver->MakeAbs(left), target);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
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
  Constraint* const ct = solver->MakeAllDifferent(variables, var_size < 10000);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_count(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AstArray* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntExpr(array_variables->a[i])->Var();
  }
  IntVar* const count = model->GetIntExpr(spec->Arg(2))->Var();
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct = solver->MakeCount(variables,
                                             spec->Arg(1)->getInt(),
                                             count);
    VLOG(1) << "  - posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    throw Error("ModelBuilder",
                std::string("Constraint ") + spec->Id() +
                " does not support variable values");
  }
}

void p_global_cardinality(FlatZincModel* const model, CtSpec* const spec) {
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
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  DCHECK(t_size % size == 0);
  const int num_tuples = t_size / size;
  std::vector<int64> one_tuple(size);
  for (int tuple_index = 0; tuple_index < num_tuples; ++tuple_index) {
    for (int var_index = 0; var_index < size; ++var_index) {
      one_tuple[var_index] = t->a[tuple_index * size + var_index]->getInt();
    }
    tuples.Insert(one_tuple);
  }
  Constraint* const ct = solver->MakeAllowedAssignments(variables, tuples);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  DCHECK(t_size % size == 0);
  const int num_tuples = t_size / size;
  std::vector<int> one_tuple(size);
  for (int tuple_index = 0; tuple_index < num_tuples; ++tuple_index) {
    for (int var_index = 0; var_index < size; ++var_index) {
      one_tuple[var_index] = t->a[tuple_index * size + var_index]->getBool();
    }
    tuples.Insert(one_tuple);
  }
  Constraint* const ct = solver->MakeAllowedAssignments(variables, tuples);
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  solver->MakeFixedDurationIntervalVarArray(start_variables,
                                            durations,
                                            "",
                                            &intervals);
  const int64 capacity = spec->Arg(3)->getInt();
  Constraint* const ct = solver->MakeCumulative(intervals,
                                                usages,
                                                capacity,
                                                "");
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  solver->MakeFixedDurationIntervalVarArray(start_variables,
                                            durations,
                                            "",
                                            &intervals);
  IntVar* const capacity = model->GetIntExpr(spec->Arg(3))->Var();
  Constraint* const ct = solver->MakeCumulative(intervals,
                                                usages,
                                                capacity,
                                                "");
  VLOG(1) << "  - posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_true_constraint(FlatZincModel* const model, CtSpec* const spec) {}

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
    global_model_builder.Register("int_lin_lt", &p_int_lin_lt);
    global_model_builder.Register("int_lin_lt_reif", &p_int_lin_lt_reif);
    global_model_builder.Register("int_lin_ge", &p_int_lin_ge);
    global_model_builder.Register("int_lin_ge_reif", &p_int_lin_ge_reif);
    global_model_builder.Register("int_lin_gt", &p_int_lin_gt);
    global_model_builder.Register("int_lin_gt_reif", &p_int_lin_gt_reif);
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
    global_model_builder.Register("bool_and", &p_bool_or);
    global_model_builder.Register("bool_xor", &p_bool_xor);
    global_model_builder.Register("array_bool_and", &p_array_bool_and);
    global_model_builder.Register("array_bool_or", &p_array_bool_or);
    global_model_builder.Register("bool_clause", &p_array_bool_clause);
    global_model_builder.Register("bool_clause_reif",
                                  &p_array_bool_clause_reif);
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
    global_model_builder.Register("count", &p_count);
    global_model_builder.Register("global_cardinality", &p_global_cardinality);
    global_model_builder.Register("global_cardinality_old",
                                  &p_global_cardinality_old);
    global_model_builder.Register("table_int", &p_table_int);
    global_model_builder.Register("table_bool", &p_table_bool);
    global_model_builder.Register("maximum_int", &p_maximum_int);
    global_model_builder.Register("minimum_int", &p_minimum_int);
    global_model_builder.Register("sort", &p_sort);
    global_model_builder.Register("fixed_cumulative", &p_fixed_cumulative);
    global_model_builder.Register("var_cumulative", &p_var_cumulative);
    global_model_builder.Register("true_constraint", &p_true_constraint);
    global_model_builder.Register("sliding_sum", &p_sliding_sum);
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
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    } else {
      Constraint* const ct = solver->MakeMemberCt(var, domain->s);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else {
    throw Error("ModelBuilder",
                std::string("Constraint ") + spec->Id() +
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
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    } else {
      Constraint* const ct = solver->MakeIsMemberCt(var, domain->s, target);
      VLOG(1) << "  - posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else {
    throw Error("ModelBuilder",
                std::string("Constraint ") + spec->Id() +
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
  } catch (AstTypeError& e) {
    throw Error("Type error", e.what());
  }
}
}  // namespace operations_research
