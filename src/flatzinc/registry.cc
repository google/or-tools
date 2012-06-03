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

#include "flatzinc/registry.h"
#include "flatzinc/flatzinc.h"

namespace operations_research {

Registry& registry(void) {
  static Registry r;
  return r;
}

void Registry::Post(FlatZincModel& s, CtSpec* const spec) {
  std::map<std::string, poster>::iterator i = r.find(spec->id);
  if (i == r.end()) {
    throw Error("Registry",
                std::string("Constraint ") + spec->id + " not found");
  }
  i->second(s, spec);
}

void Registry::Add(const std::string& id, poster p) {
  r[id] = p;
}

namespace {

void p_int_eq(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();

    } else {
      IntVar* const right_var = s.integer_variables_[right->getIntVar()];
      ct = solver->MakeEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.integer_variables_[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeEquality(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ne(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeNonEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.integer_variables_[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeNonEquality(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeNonEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ge(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeLessOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.integer_variables_[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeGreaterOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeGreaterOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_gt(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeLess(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.integer_variables_[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeGreater(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeGreater(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_le(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeGreaterOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.integer_variables_[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeLessOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeLessOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lt(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();

    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeGreater(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.integer_variables_[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeLess(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.integer_variables_[right->getIntVar()];
      ct = solver->MakeLess(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* Comparisons */
void p_int_eq_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      s.boolean_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ne_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      s.boolean_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsDifferentCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_ge_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      s.boolean_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsGreaterOrEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_gt_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      s.boolean_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsGreaterCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_le_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      s.boolean_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsLessOrEqualCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lt_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const boolvar = spec->Arg(2)->isBoolVar() ?
      s.boolean_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct = solver->MakeIsLessCt(left, right, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_eq(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct =
      solver->MakeScalProdEquality(variables, coefficients, rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_eq_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      s.boolean_variables_[node_boolvar->getBoolVar()];
  Constraint* const ct =
      solver->MakeIsEqualCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ne(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct =
      solver->MakeNonEquality(
          solver->MakeScalProd(variables, coefficients)->Var(),
          rhs);;
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ne_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      s.boolean_variables_[node_boolvar->getBoolVar()];
  Constraint* const ct =
      solver->MakeIsDifferentCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_le(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct =
      solver->MakeScalProdLessOrEqual(variables, coefficients, rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_le_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      s.boolean_variables_[node_boolvar->getBoolVar()];
  Constraint* const ct =
      solver->MakeIsLessOrEqualCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_lt(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct =
      solver->MakeScalProdLessOrEqual(variables, coefficients, rhs - 1);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_lt_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      s.boolean_variables_[node_boolvar->getBoolVar()];
  Constraint* const ct =
      solver->MakeIsLessOrEqualCstCt(var, rhs - 1, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ge(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct =
      solver->MakeScalProdGreaterOrEqual(variables, coefficients, rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_ge_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      s.boolean_variables_[node_boolvar->getBoolVar()];
  Constraint* const ct =
      solver->MakeIsGreaterOrEqualCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_gt(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct =
      solver->MakeScalProdGreaterOrEqual(variables, coefficients, rhs + 1);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_lin_gt_reif(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
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
    variables[i] = s.integer_variables_[array_variables->a[i]->getIntVar()];
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      s.boolean_variables_[node_boolvar->getBoolVar()];
  Constraint* const ct =
      solver->MakeIsGreaterOrEqualCstCt(var, rhs + 1, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* arithmetic constraints */

void p_int_plus(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeSum(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_minus(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeDifference(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_times(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeProd(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_div(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  if (spec->Arg(1)->isIntVar()) {
    IntVar* const right = s.integer_variables_[spec->Arg(1)->getIntVar()];
    Constraint* const ct =
        solver->MakeEquality(solver->MakeDiv(left, right)->Var(), target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    Constraint* const ct =
        solver->MakeEquality(solver->MakeDiv(left, spec->Arg(1)->getInt())->Var(),
                             target);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  }
}

void p_int_mod(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "int_mod(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_int_min(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMin(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_max(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMax(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_negate(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const target = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeOpposite(left)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_eq(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var = s.boolean_variables_[left->getBoolVar()];
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeEquality(left_var, right_value);
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_eq_reif(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_eq_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_ne(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeNonEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var = s.boolean_variables_[left->getBoolVar()];
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeNonEquality(left_var, right_value);
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeNonEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_ne_reif(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_ne_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_ge(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeLessOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var = s.boolean_variables_[left->getBoolVar()];
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeGreaterOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeGreaterOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_ge_reif(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_ge_reif(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_le(FlatZincModel& s, CtSpec* const spec) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = spec->Arg(0);
  AST::Node* const right = spec->Arg(1);
  if (left->isBool()) {
    const int left_value = left->getBool();
    if (right->isBool()) {
      const int right_value = right->getBool();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeGreaterOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var = s.boolean_variables_[left->getBoolVar()];
    if (right->isBool()) {
      const int right_value = right->getBool();
      ct = solver->MakeLessOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var = s.boolean_variables_[right->getBoolVar()];
      ct = solver->MakeLessOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_le_reif(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_le_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_gt(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_gt(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_gt_reif(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_gt_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_lt(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_lt(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_lt_reif(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_lt_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_or(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      s.integer_variables_[spec->Arg(0)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const right = spec->Arg(1)->isBoolVar() ?
      s.integer_variables_[spec->Arg(1)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(1)->getBool());
  IntVar* const target = spec->Arg(2)->isBoolVar() ?
      s.integer_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMax(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_and(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      s.integer_variables_[spec->Arg(0)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const right = spec->Arg(1)->isBoolVar() ?
      s.integer_variables_[spec->Arg(1)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(1)->getBool());
  IntVar* const target = spec->Arg(2)->isBoolVar() ?
      s.integer_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeMin(left, right)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_and(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  AST::Node* const node_boolvar = spec->Arg(1);
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = s.boolean_variables_[array_variables->a[i]->getBoolVar()];
  }
  IntVar* const boolvar =
      node_boolvar->isBoolVar() ?
      s.boolean_variables_[node_boolvar->getBoolVar()] :
      solver->MakeIntConst(node_boolvar->getBool());
  Constraint* const ct =
      solver->MakeMinEquality(variables, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_or(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  AST::Node* const node_boolvar = spec->Arg(1);
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = s.boolean_variables_[array_variables->a[i]->getBoolVar()];
  }
  IntVar* const boolvar =
      node_boolvar->isBoolVar() ?
      s.boolean_variables_[node_boolvar->getBoolVar()] :
      solver->MakeIntConst(node_boolvar->getBool());
  Constraint* const ct =
      solver->MakeMaxEquality(variables, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_clause(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "array_bool_clause(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_array_bool_clause_reif(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "array_bool_clause_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_xor(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      s.integer_variables_[spec->Arg(0)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const right = spec->Arg(1)->isBoolVar() ?
      s.integer_variables_[spec->Arg(1)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(1)->getBool());
  IntVar* const target = spec->Arg(2)->isBoolVar() ?
      s.integer_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct =
      solver->MakeIsEqualCstCt(solver->MakeSum(left, right)->Var(),
                               1,
                               target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_bool_l_imp(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_l_imp(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_r_imp(FlatZincModel& s, CtSpec* const spec) {
  LOG(FATAL) << "bool_r_imp(" << (spec->Arg(0)->DebugString()) << ","
             << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
             << ")::" << spec->annotations->DebugString();
}

void p_bool_not(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      s.integer_variables_[spec->Arg(0)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const target = spec->Arg(1)->isBoolVar() ?
      s.integer_variables_[spec->Arg(1)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(1)->getBool());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeDifference(1, left)->Var(), target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* element constraints */
void p_array_int_element(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const index = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  AST::Array* const array_coefficents = spec->Arg(1)->getArray();
  const int size = array_coefficents->a.size();
  std::vector<int> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
  }
  IntVar* const target = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeElement(coefficients, index)->Var(),
                           target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_array_bool_element(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const index = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  AST::Array* const array_coefficents = spec->Arg(1)->getArray();
  const int size = array_coefficents->a.size();
  std::vector<int> coefficients(size);
  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getBool();
  }
  IntVar* const target = spec->Arg(2)->isBoolVar() ?
      s.integer_variables_[spec->Arg(2)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(2)->getBool());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeElement(coefficients, index)->Var(),
                           target);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* coercion constraints */
void p_bool2int(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isBoolVar() ?
      s.boolean_variables_[spec->Arg(0)->getBoolVar()] :
      solver->MakeIntConst(spec->Arg(0)->getBool());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  Constraint* const ct = solver->MakeEquality(left, right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_in(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  AST::Node* const var_node = spec->Arg(0);
  AST::Node* const domain_node = spec->Arg(1);
  CHECK(var_node->isIntVar());
  CHECK(domain_node->isSet());
  IntVar* const var = s.integer_variables_[var_node->getIntVar()];
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

void p_abs(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  IntVar* const left = spec->Arg(0)->isIntVar() ?
      s.integer_variables_[spec->Arg(0)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(0)->getInt());
  IntVar* const right = spec->Arg(1)->isIntVar() ?
      s.integer_variables_[spec->Arg(1)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(1)->getInt());
  Constraint* const ct =
      solver->MakeEquality(solver->MakeAbs(left)->Var(), right);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_all_different_int(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = array_variables->a[i]->isIntVar() ?
        s.integer_variables_[array_variables->a[i]->getIntVar()] :
        solver->MakeIntConst(array_variables->a[i]->getInt());
  }
  Constraint* const ct = solver->MakeAllDifferent(variables);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_count(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = array_variables->a[i]->isIntVar() ?
        s.integer_variables_[array_variables->a[i]->getIntVar()] :
        solver->MakeIntConst(array_variables->a[i]->getInt());
  }
  IntVar* const count = spec->Arg(2)->isIntVar() ?
      s.integer_variables_[spec->Arg(2)->getIntVar()] :
      solver->MakeIntConst(spec->Arg(2)->getInt());
  if (spec->Arg(1)->isInt()) {
    Constraint* const ct = solver->MakeCount(variables, spec->Arg(1)->getInt(), count);
    VLOG(1) << "Posted " << ct->DebugString();
    solver->AddConstraint(ct);
  } else {
    LOG(FATAL) << "Not implemented";
  }
}

void p_global_cardinality(FlatZincModel& s, CtSpec* const spec) {
  Solver* const solver = s.solver();
  AST::Array* const array_variables = spec->Arg(0)->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = array_variables->a[i]->isIntVar() ?
        s.integer_variables_[array_variables->a[i]->getIntVar()] :
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
        s.integer_variables_[array_cards->a[i]->getIntVar()] :
        solver->MakeIntConst(array_cards->a[i]->getInt());
  }
  Constraint* const ct = solver->MakeDistribute(variables, values, cards);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

class IntPoster {
 public:
  IntPoster(void) {
    registry().Add("int_eq", &p_int_eq);
    registry().Add("int_ne", &p_int_ne);
    registry().Add("int_ge", &p_int_ge);
    registry().Add("int_gt", &p_int_gt);
    registry().Add("int_le", &p_int_le);
    registry().Add("int_lt", &p_int_lt);
    registry().Add("int_eq_reif", &p_int_eq_reif);
    registry().Add("int_ne_reif", &p_int_ne_reif);
    registry().Add("int_ge_reif", &p_int_ge_reif);
    registry().Add("int_gt_reif", &p_int_gt_reif);
    registry().Add("int_le_reif", &p_int_le_reif);
    registry().Add("int_lt_reif", &p_int_lt_reif);
    registry().Add("int_lin_eq", &p_int_lin_eq);
    registry().Add("int_lin_eq_reif", &p_int_lin_eq_reif);
    registry().Add("int_lin_ne", &p_int_lin_ne);
    registry().Add("int_lin_ne_reif", &p_int_lin_ne_reif);
    registry().Add("int_lin_le", &p_int_lin_le);
    registry().Add("int_lin_le_reif", &p_int_lin_le_reif);
    registry().Add("int_lin_lt", &p_int_lin_lt);
    registry().Add("int_lin_lt_reif", &p_int_lin_lt_reif);
    registry().Add("int_lin_ge", &p_int_lin_ge);
    registry().Add("int_lin_ge_reif", &p_int_lin_ge_reif);
    registry().Add("int_lin_gt", &p_int_lin_gt);
    registry().Add("int_lin_gt_reif", &p_int_lin_gt_reif);
    registry().Add("int_plus", &p_int_plus);
    registry().Add("int_minus", &p_int_minus);
    registry().Add("int_times", &p_int_times);
    registry().Add("int_div", &p_int_div);
    registry().Add("int_mod", &p_int_mod);
    registry().Add("int_min", &p_int_min);
    registry().Add("int_max", &p_int_max);
    registry().Add("int_abs", &p_abs);
    registry().Add("int_negate", &p_int_negate);
    registry().Add("bool_eq", &p_bool_eq);
    registry().Add("bool_eq_reif", &p_bool_eq_reif);
    registry().Add("bool_ne", &p_bool_ne);
    registry().Add("bool_ne_reif", &p_bool_ne_reif);
    registry().Add("bool_ge", &p_bool_ge);
    registry().Add("bool_ge_reif", &p_bool_ge_reif);
    registry().Add("bool_le", &p_bool_le);
    registry().Add("bool_le_reif", &p_bool_le_reif);
    registry().Add("bool_gt", &p_bool_gt);
    registry().Add("bool_gt_reif", &p_bool_gt_reif);
    registry().Add("bool_lt", &p_bool_lt);
    registry().Add("bool_lt_reif", &p_bool_lt_reif);
    registry().Add("bool_or", &p_bool_or);
    registry().Add("bool_and", &p_bool_and);
    registry().Add("bool_xor", &p_bool_xor);
    registry().Add("array_bool_and", &p_array_bool_and);
    registry().Add("array_bool_or", &p_array_bool_or);
    registry().Add("bool_clause", &p_array_bool_clause);
    registry().Add("bool_clause_reif", &p_array_bool_clause_reif);
    registry().Add("bool_left_imp", &p_bool_l_imp);
    registry().Add("bool_right_imp", &p_bool_r_imp);
    registry().Add("bool_not", &p_bool_not);
    registry().Add("array_int_element", &p_array_int_element);
    registry().Add("array_var_int_element", &p_array_int_element);
    registry().Add("array_bool_element", &p_array_bool_element);
    registry().Add("array_var_bool_element", &p_array_bool_element);
    registry().Add("bool2int", &p_bool2int);
    registry().Add("int_in", &p_int_in);
    registry().Add("all_different_int", &p_all_different_int);
    registry().Add("count", &p_count);
    registry().Add("global_cardinality", &p_global_cardinality);
  }
};
IntPoster __int_poster;

// void p_set_union(FlatZincModel& s, const ConExpr& ce, AST::Node *spec->annotations) {
//   LOG(FATAL) << "set_union(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_intersect(FlatZincModel& s, const ConExpr& ce, AST::Node *spec->annotations) {
//   LOG(FATAL) << "set_intersect(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_diff(FlatZincModel& s, const ConExpr& ce, AST::Node *spec->annotations) {
//   LOG(FATAL) << "set_diff(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }

// void p_set_symdiff(FlatZincModel& s, CtSpec* const spec) {
//   LOG(FATAL) << "set_symdiff(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }

// void p_set_eq(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_eq(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_ne(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_ne(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_subset(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_subset(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_superset(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_superset(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_card(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_card(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_in(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_in(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_eq_reif(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_eq_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_ne_reif(FlatZincModel& s, const ConExpr& ce, AST::Node * spec->annotations) {
//   LOG(FATAL) << "set_ne_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_subset_reif(FlatZincModel& s, const ConExpr& ce,
//                        AST::Node *spec->annotations) {
//   LOG(FATAL) << "set_subset_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_superset_reif(FlatZincModel& s, const ConExpr& ce,
//                          AST::Node *spec->annotations) {
//   LOG(FATAL) << "set_superset_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_in_reif(FlatZincModel& s, const ConExpr& ce, AST::Node *spec->annotations) {
//   LOG(FATAL) << "set_in_reif(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }
// void p_set_disjoint(FlatZincModel& s, const ConExpr& ce, AST::Node *spec->annotations) {
//   LOG(FATAL) << "set_disjoint(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }

// void p_array_set_element(FlatZincModel& s, const ConExpr& ce,
//                          AST::Node* spec->annotations) {
//   LOG(FATAL) << "array_set_element(" << (spec->Arg(0)->DebugString()) << "," << (spec->Arg(1)->DebugString()) << "," << (spec->Arg(2)->DebugString())
//              << ")::" << spec->annotations->DebugString();
// }


// class SetPoster {
// public:
//   SetPoster(void) {
//     registry().Add("set_eq", &p_set_eq);
//     registry().Add("set_ne", &p_set_ne);
//     registry().Add("set_union", &p_set_union);
//     registry().Add("array_set_element", &p_array_set_element);
//     registry().Add("array_var_set_element", &p_array_set_element);
//     registry().Add("set_intersect", &p_set_intersect);
//     registry().Add("set_diff", &p_set_diff);
//     registry().Add("set_symdiff", &p_set_symdiff);
//     registry().Add("set_subset", &p_set_subset);
//     registry().Add("set_superset", &p_set_superset);
//     registry().Add("set_card", &p_set_card);
//     registry().Add("set_in", &p_set_in);
//     registry().Add("set_eq_reif", &p_set_eq_reif);
//     registry().Add("equal_reif", &p_set_eq_reif);
//     registry().Add("set_ne_reif", &p_set_ne_reif);
//     registry().Add("set_subset_reif", &p_set_subset_reif);
//     registry().Add("set_superset_reif", &p_set_superset_reif);
//     registry().Add("set_in_reif", &p_set_in_reif);
//     registry().Add("disjoint", &p_set_disjoint);
//   }
// };
// SetPoster __set_poster;

}
}

// STATISTICS: flatzinc-any
