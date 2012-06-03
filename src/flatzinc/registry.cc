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
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();

    } else {
      IntVar* const right_var = model->IntegerVariable(right->getIntVar());
      ct = solver->MakeEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        model->IntegerVariable(left->getIntVar());
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeEquality(left_var, right_value);
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ne(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeNonEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        model->IntegerVariable(left->getIntVar());
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeNonEquality(left_var, right_value);
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeNonEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ge(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeLessOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        model->IntegerVariable(left->getIntVar());
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeGreaterOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeGreaterOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_gt(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeLess(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        model->IntegerVariable(left->getIntVar());
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeGreater(left_var, right_value);
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeGreater(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_le(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeGreaterOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        model->IntegerVariable(left->getIntVar());
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeLessOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var =
          model->IntegerVariable(right->getIntVar());
      ct = solver->MakeLessOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lt(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();

    } else {
      IntVar* const right_var = model->IntegerVariable(right->getIntVar());
      ct = solver->MakeGreater(right_var, left_value);
    }
  } else {
    IntVar* const left_var = model->IntegerVariable(left->getIntVar());
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeLess(left_var, right_value);
    } else {
      IntVar* const right_var = model->IntegerVariable(right->getIntVar());
      ct = solver->MakeLess(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* Comparisons */
void p_int_eq_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsDifferentCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsGreaterCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsLessOrEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      model->BooleanVariable(node_boolvar->getBoolVar());
  Constraint* const ct =
      solver->MakeIsEqualCstCt(var, rhs, boolvar);
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
  }
  Constraint* const ct =
      solver->MakeNonEquality(
          solver->MakeScalProd(variables, coefficients)->Var(),
          rhs);;
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      model->BooleanVariable(node_boolvar->getBoolVar());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      model->BooleanVariable(node_boolvar->getBoolVar());
  Constraint* const ct =
      solver->MakeIsLessOrEqualCstCt(var, rhs, boolvar);
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      model->BooleanVariable(node_boolvar->getBoolVar());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      model->BooleanVariable(node_boolvar->getBoolVar());
  Constraint* const ct =
      solver->MakeIsGreaterOrEqualCstCt(var, rhs, boolvar);
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
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
    variables[i] = model->IntegerVariable(array_variables->a[i]->getIntVar());
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      model->BooleanVariable(node_boolvar->getBoolVar());
  Constraint* const ct =
      solver->MakeIsGreaterOrEqualCstCt(var, rhs + 1, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* arithmetic constraints */

void p_int_plus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeSum(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_minus(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeDifference(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_times(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeProd(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_div(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  if (spec->Arg(1)->isIntVar()) {
    IntVar* const right = model->IntegerVariable(spec->Arg(1)->getIntVar());
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
  LOG(FATAL) << "int_mod(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_int_min(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMin(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_max(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMax(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_negate(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const target = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeOpposite(left)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_eq(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var = model->BooleanVariable(left->getBoolVar());
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeEquality(left_var, right_value);
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_eq_reif(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_eq_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_ne(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeNonEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var = model->BooleanVariable(left->getBoolVar());
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeNonEquality(left_var, right_value);
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeNonEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_ne_reif(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_ne_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_ge(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeLessOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var = model->BooleanVariable(left->getBoolVar());
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeGreaterOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeGreaterOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_ge_reif(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_ge_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_le(FlatZincModel* const model, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = model->solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeGreaterOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var = model->BooleanVariable(left->getBoolVar());
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeLessOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var = model->BooleanVariable(right->getBoolVar());
      ct = solver->MakeLessOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_le_reif(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_le_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_gt(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_gt(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_gt_reif(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_gt_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_lt(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_lt(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_lt_reif(FlatZincModel* const model, CtSpec* const spec) {
  LOG(FATAL) << "bool_lt_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << ","
             << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations()->DebugString();
}

void p_bool_or(FlatZincModel* const model, CtSpec* const spec) {
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
      solver->MakeEquality(solver->MakeMax(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_and(FlatZincModel* const model, CtSpec* const spec) {
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
      solver->MakeEquality(solver->MakeMin(left, right)->Var(), target);
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
    variables[i] = model->BooleanVariable(array_variables->a[i]->getBoolVar());
  }
  IntVar* const boolvar =
      node_boolvar->isBoolVar() ?
      model->BooleanVariable(node_boolvar->getBoolVar()) :
      solver->MakeIntConst(node_boolvar->getBool());
  Constraint* const ct =
      solver->MakeMinEquality(variables, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_or(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  AST::Node* const node_boolvar = spec->Arg(1);
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = model->BooleanVariable(array_variables->a[i]->getBoolVar());
  }
  IntVar* const boolvar =
      node_boolvar->isBoolVar() ?
      model->BooleanVariable(node_boolvar->getBoolVar()) :
      solver->MakeIntConst(node_boolvar->getBool());
  Constraint* const ct =
      solver->MakeMaxEquality(variables, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
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
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(0)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const target = spec->Arg(1)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(1)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(1)->getBool());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeDifference(1, left)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* element constraints */
void p_array_int_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const index = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  AST::Array* const array_coefficents = spec->Arg(1)->getArray();
  const int size = array_coefficents->a.size();
  std::vector<int> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
  }
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeElement(coefficients, index)->Var(),
                           target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_element(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const index = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  AST::Array* const array_coefficents = spec->Arg(1)->getArray();
  const int size = array_coefficents->a.size();
  std::vector<int> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getBool();
  }
  IntVar* const target = spec->Arg(2)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(2)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeElement(coefficients, index)->Var(),
                           target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* coercion constraints */
void p_bool2int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      model->BooleanVariable(spec->Arg(0)->getBoolVar()) :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  Constraint* const ct = solver->MakeEquality(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_in(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Node* const var_node = spec->Arg(0);
  AST::Node* const domain_node = spec->Arg(1);
  CHECK(var_node->isIntVar());
  CHECK(domain_node->isSet());
  IntVar* const var = model->IntegerVariable(var_node->getIntVar());
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
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      model->IntegerVariable(spec->Arg(0)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      model->IntegerVariable(spec->Arg(1)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeAbs(left)->Var(), right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_all_different_int(FlatZincModel* const model, CtSpec* const spec) {
  Solver* const solver = model->solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = array_variables->a[i]->isIntVar() ?
        model->IntegerVariable(array_variables->a[i]->getIntVar()) :
        solver->MakeIntConst(array_variables->a[i]->getInt());
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
    variables[i] = array_variables->a[i]->isIntVar() ?
        model->IntegerVariable(array_variables->a[i]->getIntVar()) :
        solver->MakeIntConst(array_variables->a[i]->getInt());
  }
  IntVar* const count = spec->Arg(2)->isIntVar() ?
      model->IntegerVariable(spec->Arg(2)->getIntVar()) :
      solver->MakeIntConst(spec->Arg(2)->getInt());
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
    variables[i] = array_variables->a[i]->isIntVar() ?
        model->IntegerVariable(array_variables->a[i]->getIntVar()) :
        solver->MakeIntConst(array_variables->a[i]->getInt());
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
    cards[i] = array_cards->a[i]->isIntVar() ?
        model->IntegerVariable(array_cards->a[i]->getIntVar()) :
        solver->MakeIntConst(array_cards->a[i]->getInt());
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
    global_model_builder.Register("bool_clause", &p_array_bool_clause);
    global_model_builder.Register("bool_clause_reif", &p_array_bool_clause_reif);
    global_model_builder.Register("bool_left_imp", &p_bool_l_imp);
    global_model_builder.Register("bool_right_imp", &p_bool_r_imp);
    global_model_builder.Register("bool_not", &p_bool_not);
    global_model_builder.Register("array_int_element", &p_array_int_element);
    global_model_builder.Register("array_var_int_element", &p_array_int_element);
    global_model_builder.Register("array_bool_element", &p_array_bool_element);
    global_model_builder.Register("array_var_bool_element", &p_array_bool_element);
    global_model_builder.Register("bool2int", &p_bool2int);
    global_model_builder.Register("int_in", &p_int_in);
    global_model_builder.Register("all_different_int", &p_all_different_int);
    global_model_builder.Register("count", &p_count);
    global_model_builder.Register("global_cardinality", &p_global_cardinality);
  }
};
IntBuilder __int_Builder;

// void p_set_union(FlatZincModel* const model, const ConExpr& ce, AST::Node *spec->annotations()) {
//   LOG(FATAL) << "set_union(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_intersect(FlatZincModel* const model, const ConExpr& ce, AST::Node *spec->annotations()) {
//   LOG(FATAL) << "set_intersect(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_diff(FlatZincModel* const model, const ConExpr& ce, AST::Node *spec->annotations()) {
//   LOG(FATAL) << "set_diff(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }

// void p_set_symdiff(FlatZincModel* const model, CtSpec* const spec) {
//   LOG(FATAL) << "set_symdiff(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }

// void p_set_eq(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_eq(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_ne(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_ne(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_subset(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_subset(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_superset(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_superset(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_card(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_card(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_in(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_in(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_eq_reif(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_eq_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_ne_reif(FlatZincModel* const model, const ConExpr& ce, AST::Node * spec->annotations()) {
//   LOG(FATAL) << "set_ne_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_subset_reif(FlatZincModel* const model, const ConExpr& ce,
//                        AST::Node *spec->annotations()) {
//   LOG(FATAL) << "set_subset_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_superset_reif(FlatZincModel* const model, const ConExpr& ce,
//                          AST::Node *spec->annotations()) {
//   LOG(FATAL) << "set_superset_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_in_reif(FlatZincModel* const model, const ConExpr& ce, AST::Node *spec->annotations()) {
//   LOG(FATAL) << "set_in_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }
// void p_set_disjoint(FlatZincModel* const model, const ConExpr& ce, AST::Node *spec->annotations()) {
//   LOG(FATAL) << "set_disjoint(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }

// void p_array_set_element(FlatZincModel* const model, const ConExpr& ce,
//                          AST::Node* spec->annotations()) {
//   LOG(FATAL) << "array_set_element(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations()->DebugString();
// }


// class SetBuilder {
// public:
//   SetBuilder(void) {
//     global_model_builder.Register("set_eq", &p_set_eq);
//     global_model_builder.Register("set_ne", &p_set_ne);
//     global_model_builder.Register("set_union", &p_set_union);
//     global_model_builder.Register("array_set_element", &p_array_set_element);
//     global_model_builder.Register("array_var_set_element", &p_array_set_element);
//     global_model_builder.Register("set_intersect", &p_set_intersect);
//     global_model_builder.Register("set_diff", &p_set_diff);
//     global_model_builder.Register("set_symdiff", &p_set_symdiff);
//     global_model_builder.Register("set_subset", &p_set_subset);
//     global_model_builder.Register("set_superset", &p_set_superset);
//     global_model_builder.Register("set_card", &p_set_card);
//     global_model_builder.Register("set_in", &p_set_in);
//     global_model_builder.Register("set_eq_reif", &p_set_eq_reif);
//     global_model_builder.Register("equal_reif", &p_set_eq_reif);
//     global_model_builder.Register("set_ne_reif", &p_set_ne_reif);
//     global_model_builder.Register("set_subset_reif", &p_set_subset_reif);
//     global_model_builder.Register("set_superset_reif", &p_set_superset_reif);
//     global_model_builder.Register("set_in_reif", &p_set_in_reif);
//     global_model_builder.Register("disjoint", &p_set_disjoint);
//   }
// };
// SetBuilder __set_Builder;
}  // namespace

void FlatZincModel::PostConstraint(CtSpec* const spec) {
  try {
    global_model_builder.Post(this, spec);
  } catch (AST::TypeError& e) {
    throw Error("Type error", e.what());
  }
}
}  // namespace operations_research

