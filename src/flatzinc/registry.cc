/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Contributing authors:
 *     Mikael Lagerkvist <lagerkvist@gmail.com>
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

#include "flatzinc/flatzinc.h"

namespace operations_research {
namespace {

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
  std::map<std::string, Builder>::iterator i = r.find(spec->Id());
  if (i == r.end()) {
    throw Error("ModelBuilder",
                std::string("Constraint ") + spec->Id() + " not found");
  }
  i->second(model, spec);
}

void ModelBuilder::Register(const std::string& id, Builder p) {
  r[id] = p;
}

void p_int_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  Constraint* const ct = solver->MakeEquality(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ne(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  Constraint* const ct = solver->MakeNonEquality(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ge(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  Constraint* const ct = solver->MakeGreaterOrEqual(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_gt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  Constraint* const ct = solver->MakeGreater(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  Constraint* const ct = solver->MakeLessOrEqual(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  Constraint* const ct = solver->MakeLess(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* Comparisons */
void p_int_eq_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const boolvar = model->GetIntVar(spec->Arg(2));
  Constraint* const ct = solver->MakeIsEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const boolvar = model->GetIntVar(spec->Arg(2));
  Constraint* const ct = solver->MakeIsDifferentCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const boolvar = model->GetIntVar(spec->Arg(2));
  Constraint* const ct = solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const boolvar = model->GetIntVar(spec->Arg(2));
  Constraint* const ct = solver->MakeIsGreaterCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const boolvar = model->GetIntVar(spec->Arg(2));
  Constraint* const ct = solver->MakeIsLessOrEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const boolvar = model->GetIntVar(spec->Arg(2));
  Constraint* const ct = solver->MakeIsLessCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_eq(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  Constraint* const ct =
      solver->MakeScalProdEquality(variables, coefficients, rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_eq_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  AST::Node* const node_boolvar = spec->Arg(3);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  IntVar* const var = solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar = model->GetIntVar(node_boolvar);
  Constraint* const ct = solver->MakeIsEqualCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ne(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  Constraint* const ct =
      solver->MakeNonEquality(
          solver->MakeScalProd(variables, coefficients)->Var(),
          rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  AST::Node* const node_boolvar = spec->Arg(3);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar = model->GetIntVar(node_boolvar);
  Constraint* const ct =
      solver->MakeIsDifferentCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_le(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  Constraint* const ct =
      solver->MakeScalProdLessOrEqual(variables, coefficients, rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  AST::Node* const node_boolvar = spec->Arg(3);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  IntVar* const var = solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar = model->GetIntVar(node_boolvar);
  Constraint* const ct = solver->MakeIsLessOrEqualCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_lt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  Constraint* const ct =
      solver->MakeScalProdLessOrEqual(variables, coefficients, rhs - 1);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  AST::Node* const node_boolvar = spec->Arg(3);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar = model->GetIntVar(node_boolvar);
  Constraint* const ct =
      solver->MakeIsLessOrEqualCstCt(var, rhs - 1, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ge(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  Constraint* const ct =
      solver->MakeScalProdGreaterOrEqual(variables, coefficients, rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  AST::Node* const node_boolvar = spec->Arg(3);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  IntVar* const var = solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar = model->GetIntVar(node_boolvar);
  Constraint* const ct = solver->MakeIsGreaterOrEqualCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_gt(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  Constraint* const ct =
      solver->MakeScalProdGreaterOrEqual(variables, coefficients, rhs + 1);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_coefficents = spec->Arg(0)->getArray();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  AST::Node* const node_rhs = spec->Arg(2);
  AST::Node* const node_boolvar = spec->Arg(3);
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  IntVar* const var = solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar = model->GetIntVar(node_boolvar);
  Constraint* const ct =
      solver->MakeIsGreaterOrEqualCstCt(var, rhs + 1, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* arithmetic constraints */

void p_int_plus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  if (spec->Arg(2)->isIntVar() &&
      spec->defines() == spec->Arg(2)->getIntVar()) {
    VLOG(1) << "Aliasing int_plus";
    IntVar* const target = solver->MakeSum(left, right)->Var();
    CHECK(model->IntegerVariable(spec->Arg(2)->getIntVar()) == NULL);
    model->SetIntegerVariable(spec->Arg(2)->getIntVar(), target);
  } else {
    IntVar* const target = model->GetIntVar(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeSum(left, right)->Var(), target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_minus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  if (spec->Arg(2)->isIntVar() &&
      spec->defines() == spec->Arg(2)->getIntVar()) {
    VLOG(1) << "Aliasing int_plus";
    IntVar* const target = solver->MakeDifference(left, right)->Var();
    CHECK(model->IntegerVariable(spec->Arg(2)->getIntVar()) == NULL);
    model->SetIntegerVariable(spec->Arg(2)->getIntVar(), target);
  } else {
    IntVar* const target = model->GetIntVar(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeDifference(left, right)->Var(),
                             target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_times(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  if (spec->Arg(2)->isIntVar() &&
      spec->defines() == spec->Arg(2)->getIntVar()) {
    VLOG(1) << "Aliasing int_plus";
    IntVar* const target = solver->MakeProd(left, right)->Var();
    CHECK(model->IntegerVariable(spec->Arg(2)->getIntVar()) == NULL);
    model->SetIntegerVariable(spec->Arg(2)->getIntVar(), target);
  } else {
    IntVar* const target = model->GetIntVar(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeProd(left, right)->Var(), target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_div(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const target = model->GetIntVar(spec->Arg(2));
  if (spec->Arg(1)->isIntVar()) {
    IntVar* const right = model->GetIntVar(spec->Arg(1));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeDiv(left, right)->Var(), target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    Constraint* const ct =
        solver->MakeEquality(
            solver->MakeDiv(left, spec->Arg(1)->getInt())->Var(),
            target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_mod(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  const int mod = spec->Arg(1)->getInt();
  IntVar* const target = model->GetIntVar(spec->Arg(2));
  Constraint* const ct = solver->MakeModuloConstraint(left, mod, target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_min(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const target = model->GetIntVar(spec->Arg(2));
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMin(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_max(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const target = model->GetIntVar(spec->Arg(2));
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMax(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_negate(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const target = model->GetIntVar(spec->Arg(2));
  Constraint* const ct =
      solver->MakeEquality(solver->MakeOpposite(left)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_and(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  AST::Node* const node_boolvar = spec->Arg(1);
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    AST::Node* const a = array_variables->a[i];
    variables[i] = model->GetIntVar(a);
  }
  if (node_boolvar->isBoolVar() &&
      node_boolvar->getBoolVar() + model->IntVarCount() == spec->defines()) {
    VLOG(1) << "Aliasing array_bool_and";
    IntVar* const boolvar = solver->MakeMin(variables)->Var();
    CHECK(model->BooleanVariable(node_boolvar->getBoolVar()) == NULL);
    model->SetBooleanVariable(node_boolvar->getBoolVar(), boolvar);
  } else {
    IntVar* const boolvar = model->GetIntVar(node_boolvar);
    Constraint* const ct =
        solver->MakeMinEquality(variables, boolvar);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_array_bool_or(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  AST::Node* const node_boolvar = spec->Arg(1);
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    AST::Node* const a = array_variables->a[i];
    variables[i] = model->GetIntVar(a);
  }
  if (node_boolvar->isBoolVar() &&
      node_boolvar->getBoolVar() + model->IntVarCount() == spec->defines()) {
    VLOG(1) << "Aliasing array_bool_or";
    IntVar* const boolvar = solver->MakeMax(variables)->Var();
    CHECK(model->BooleanVariable(node_boolvar->getBoolVar()) == NULL);
    model->SetBooleanVariable(node_boolvar->getBoolVar(), boolvar);
  } else {
    IntVar* const boolvar = model->GetIntVar(node_boolvar);
    Constraint* const ct =
        solver->MakeMaxEquality(variables, boolvar);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
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
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(0)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const right = spec->Arg(1)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(1)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(1)->getBool());
  IntVar* const target = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct =
      solver->MakeIsEqualCstCt(solver->MakeSum(left, right)->Var(),
                               1,
                               target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_l_imp(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_l_imp(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_r_imp(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_r_imp(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_not(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const right = model->GetIntVar(spec->Arg(1));
  IntVar* const target = model->GetIntVar(spec->Arg(2));
  Constraint* const ct =
      solver->MakeEquality(solver->MakeDifference(1, left)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* element constraints */
void p_array_int_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const index =  model->GetIntVar(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AST::Array* const array_coefficents = spec->Arg(1)->getArray();
  const int size = array_coefficents->a.size();
  std::vector<int> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
  }
  if (spec->Arg(2)->isIntVar() &&
      spec->defines() == spec->Arg(2)->getIntVar()) {
    VLOG(1) << "Aliasing array_int_element";
    IntVar* const target =
        solver->MakeElement(coefficients, shifted_index)->Var();
    CHECK(model->IntegerVariable(spec->Arg(2)->getIntVar()) == NULL);
    model->SetIntegerVariable(spec->Arg(2)->getIntVar(), target);
  } else {
    IntVar* const target = model->GetIntVar(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(
            solver->MakeElement(coefficients, shifted_index)->Var(),
            target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_array_var_int_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const index =  model->GetIntVar(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AST::Array* const array_variables = spec->Arg(1)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  if (spec->Arg(2)->isIntVar() &&
      spec->defines() == spec->Arg(2)->getIntVar()) {
    VLOG(1) << "Aliasing array_var_int_element";
    IntVar* const target = solver->MakeElement(variables, shifted_index)->Var();
    CHECK(model->IntegerVariable(spec->Arg(2)->getIntVar()) == NULL);
    model->SetIntegerVariable(spec->Arg(2)->getIntVar(), target);
  } else {
    IntVar* const target = model->GetIntVar(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(solver->MakeElement(variables,
                                                 shifted_index)->Var(),
                             target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_array_bool_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const index = model->GetIntVar(spec->Arg(0));
  IntVar* const shifted_index = solver->MakeSum(index, -1)->Var();
  AST::Array* const array_coefficents = spec->Arg(1)->getArray();
  const int size = array_coefficents->a.size();
  std::vector<int> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getBool();
  }
  if (spec->Arg(2)->isIntVar() &&
      spec->defines() == spec->Arg(2)->getIntVar()) {
    VLOG(1) << "Aliasing array_bool_element";
    IntVar* const target =
        solver->MakeElement(coefficients, shifted_index)->Var();
    CHECK(model->IntegerVariable(spec->Arg(2)->getIntVar()) == NULL);
    model->SetIntegerVariable(spec->Arg(2)->getIntVar(), target);
  } else {
    IntVar* const target = model->GetIntVar(spec->Arg(2));
    Constraint* const ct =
        solver->MakeEquality(
            solver->MakeElement(coefficients, shifted_index)->Var(),
            target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

/* coercion constraints */
void p_bool2int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  if (spec->Arg(1)->isIntVar() &&
      spec->defines() == spec->Arg(1)->getIntVar()) {
    VLOG(1) << "Aliasing bool2int";
    CHECK(model->IntegerVariable(spec->defines()) == NULL);
    model->SetIntegerVariable(spec->defines(), left);
  } else {
    IntVar* const right = model->GetIntVar(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_bool2bool(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  if (spec->Arg(1)->isBoolVar() &&
      spec->defines() == spec->Arg(1)->getBoolVar() + model->IntVarCount()) {
    VLOG(1) << "Aliasing bool2bool";
    CHECK(model->BooleanVariable(spec->Arg(1)->getBoolVar()) == NULL);
    model->SetBooleanVariable(spec->Arg(1)->getBoolVar(), left);
  } else {
    IntVar* const right = model->GetIntVar(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int2int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  if (spec->Arg(1)->isIntVar() &&
      spec->defines() == spec->Arg(1)->getIntVar()) {
    VLOG(1) << "Aliasing int2int";
    CHECK(model->IntegerVariable(spec->defines()) == NULL);
    model->SetIntegerVariable(spec->defines(), left);
  } else {
    IntVar* const right = model->GetIntVar(spec->Arg(1));
    Constraint* const ct = solver->MakeEquality(left, right);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_in(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Node* const var_node = spec->Arg(0);
  AST::Node* const domain_node = spec->Arg(1);
  CHECK(var_node->isIntVar());
  CHECK(domain_node->isSet());
  IntVar* const var = model->GetIntVar(var_node);
  AST::SetLit* const domain = domain_node->getSet();
  if (domain->interval) {
    if (var->Min() < domain->min || var->Max() > domain->max) {
      Constraint* const ct =
          solver->MakeBetweenCt(var, domain->min, domain->max);
      VLOG(1) << "Posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else {
    Constraint* const ct = solver->MakeMemberCt(var, domain->s);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_abs(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = model->GetIntVar(spec->Arg(0));
  IntVar* const target = model->GetIntVar(spec->Arg(1));
  Constraint* const ct =
      solver->MakeEquality(solver->MakeAbs(left)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_all_different_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  Constraint* const ct = solver->MakeAllDifferent(variables);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_count(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  IntVar* const count = model->GetIntVar(spec->Arg(2));
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct = solver->MakeCount(variables,
                                             spec->Arg(1)->getInt(),
                                             count);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    LOG(FATAL) << "Not implemented";
  }
}

void p_global_cardinality(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->GetIntVar(array_variables->a[i]);
  }
  AST::Array* const array_coefficents = spec->Arg(1)->getArray();
  const int vsize = array_coefficents->a.size();
  std::vector<int> values(vsize);
  for (int i = 0; i < vsize; ++i) {
    values[i] = array_coefficents->a[i]->getInt();
  }
  AST::Array* const array_cards = spec->Arg(2)->getArray();
  const int csize = array_cards->a.size();
  std::vector<IntVar*> cards(csize);
  for (int i = 0; i < csize; ++i) {
    cards[i] = model->GetIntVar(array_cards->a[i]);
  }
  Constraint* const ct = solver->MakeDistribute(variables, values, cards);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
    global_model_builder.Register("bool_eq", &p_int_eq);
    global_model_builder.Register("bool_eq_reif", &p_int_eq_reif);
    global_model_builder.Register("bool_ne", &p_int_ne);
    global_model_builder.Register("bool_ne_reif", &p_int_ne_reif);
    global_model_builder.Register("bool_ge", &p_int_ge);
    global_model_builder.Register("bool_ge_reif", &p_int_ge_reif);
    global_model_builder.Register("bool_le", &p_int_le);
    global_model_builder.Register("bool_le_reif", &p_int_le_reif);
    global_model_builder.Register("bool_gt", &p_int_gt);
    global_model_builder.Register("bool_gt_reif", &p_int_gt_reif);
    global_model_builder.Register("bool_lt", &p_int_lt);
    global_model_builder.Register("bool_lt_reif", &p_int_lt_reif);
    global_model_builder.Register("bool_or", &p_int_max);
    global_model_builder.Register("bool_and", &p_int_min);
    global_model_builder.Register("bool_xor", &p_bool_xor);
    global_model_builder.Register("array_bool_and", &p_array_bool_and);
    global_model_builder.Register("array_bool_or", &p_array_bool_or);
    global_model_builder.Register("bool_clause", &p_array_bool_clause);
    global_model_builder.Register("bool_clause_reif", &p_array_bool_clause_reif);
    global_model_builder.Register("bool_left_imp", &p_bool_l_imp);
    global_model_builder.Register("bool_right_imp", &p_bool_r_imp);
    global_model_builder.Register("bool_not", &p_bool_not);
    global_model_builder.Register("array_int_element", &p_array_int_element);
    global_model_builder.Register("array_var_int_element", &p_array_var_int_element);
    global_model_builder.Register("array_bool_element", &p_array_bool_element);
    global_model_builder.Register("array_var_bool_element", &p_array_bool_element);
    global_model_builder.Register("bool2bool", &p_bool2bool);
    global_model_builder.Register("bool2int", &p_bool2int);
    global_model_builder.Register("int2int", &p_int2int);
    global_model_builder.Register("int_in", &p_int_in);
    global_model_builder.Register("all_different_int", &p_all_different_int);
    global_model_builder.Register("count", &p_count);
    global_model_builder.Register("global_cardinality", &p_global_cardinality);
  }
};
IntBuilder __int_Builder;

void p_set_in(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const var = model->GetIntVar(spec->Arg(0));
  if (spec->Arg(1)->isSet()) {
    AST::SetLit* const domain = spec->Arg(1)->getSet();
    if (domain->interval) {
      Constraint* const ct =
          solver->MakeBetweenCt(var, domain->min, domain->max);
      VLOG(1) << "Posted " << ct->DebugString();
      solver->AddConstraint(ct);
    } else {
      Constraint* const ct = solver->MakeMemberCt(var, domain->s);
      VLOG(1) << "Posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else {
    LOG(FATAL) << "set_in(" << (spec->Arg(0)->DebugString())
               << "," << (spec->Arg(1)->DebugString())
               << ")::" << spec->annotations()->DebugString();
  }
}
void p_set_in_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const var = model->GetIntVar(spec->Arg(0));
  IntVar* const target = model->GetIntVar(spec->Arg(2));
  if (spec->Arg(1)->isSet()) {
    AST::SetLit* const domain = spec->Arg(1)->getSet();
    if (domain->interval) {
      Constraint* const ct =
          solver->MakeIsBetweenCt(var, domain->min, domain->max, target);
      VLOG(1) << "Posted " << ct->DebugString();
      solver->AddConstraint(ct);
    } else {
      Constraint* const ct = solver->MakeIsMemberCt(var, domain->s, target);
      VLOG(1) << "Posted " << ct->DebugString();
      solver->AddConstraint(ct);
    }
  } else {
    LOG(FATAL) << "set_in_reif(" << (spec->Arg(0)->DebugString())
               << "," << (spec->Arg(1)->DebugString()) << ","
               << (spec->Arg(2)->DebugString())
               << ")::" << spec->annotations()->DebugString();
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
  } catch (AST::TypeError& e) {
    throw Error("Type error", e.what());
  }
}
}  // namespace operations_research
