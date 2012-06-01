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

void
Registry::post(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::map<std::string,poster>::iterator i = r.find(ce.id);
  if (i == r.end()) {
    throw Error("Registry",
                std::string("Constraint ")+ce.id+" not found");
  }
  i->second(s, ce, ann);
}

void
Registry::add(const std::string& id, poster p) {
  r[id] = p;
}

namespace {

void p_int_eq(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = ce[0];
  AST::Node* const right = ce[1];
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();

    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.iv[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeEquality(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}
void p_int_ne(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = ce[0];
  AST::Node* const right = ce[1];
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeNonEquality(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.iv[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeNonEquality(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeNonEquality(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}
void p_int_ge(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = ce[0];
  AST::Node* const right = ce[1];
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeLessOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.iv[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeGreaterOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeGreaterOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_gt(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = ce[0];
  AST::Node* const right = ce[1];
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeLess(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.iv[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeGreater(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeGreater(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

void p_int_le(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = ce[0];
  AST::Node* const right = ce[1];
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();
      LOG(FATAL) << "Not implemented";
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeGreaterOrEqual(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.iv[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeLessOrEqual(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeLessOrEqual(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}
void p_int_lt(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Constraint* ct = NULL;
  Solver* const solver = s.solver();
  AST::Node* const left = ce[0];
  AST::Node* const right = ce[1];
  if (left->isInt()) {
    const int left_value = left->getInt();
    if (right->isInt()) {
      const int right_value = right->getInt();

    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeGreater(right_var, left_value);
    }
  } else {
    IntVar* const left_var =
        s.iv[left->getIntVar()];
    if (right->isInt()) {
      const int right_value = right->getInt();
      ct = solver->MakeLess(left_var, right_value);
    } else {
      IntVar* const right_var =
          s.iv[right->getIntVar()];
      ct = solver->MakeLess(left_var, right_var);
    }
  }
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

/* Comparisons */
void p_int_eq_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_eq_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_ne_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_ne_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_ge_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_ge_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_gt_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_gt_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_le_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_le_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_lt_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lt_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}

void p_int_lin_eq(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Solver* const solver = s.solver();
  AST::Array* const array_coefficents = ce[0]->getArray();
  AST::Array* const array_variables = ce[1]->getArray();
  AST::Node* const node_rhs = ce[2];
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = s.iv[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct =
      solver->MakeScalProdEquality(variables, coefficients, rhs);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}
void p_int_lin_eq_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Solver* const solver = s.solver();
  AST::Array* const array_coefficents = ce[0]->getArray();
  AST::Array* const array_variables = ce[1]->getArray();
  AST::Node* const node_rhs = ce[2];
  AST::Node* const node_boolvar = ce[3];
  const int rhs = node_rhs->getInt();
  const int size = array_coefficents->a.size();
  CHECK_EQ(size, array_variables->a.size());
  std::vector<int> coefficients(size);
  std::vector<IntVar*> variables(size);

  for (int i = 0; i < size; ++i) {
    coefficients[i] = array_coefficents->a[i]->getInt();
    variables[i] = s.iv[array_variables->a[i]->getIntVar()];
  }
  IntVar* const var =
      solver->MakeScalProd(variables, coefficients)->Var();
  IntVar* const boolvar =
      s.bv[node_boolvar->getBoolVar()];
  Constraint* const ct =
      solver->MakeIsEqualCstCt(var, rhs, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}
void p_int_lin_ne(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_ne("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_lin_ne_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_ne_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<","<<(*ce[3])<<")::"<<(*ann)<<"\n";
}
void p_int_lin_le(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_le("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_lin_le_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_le_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<","<<(*ce[3])<<")::"<<(*ann)<<"\n";
}
void p_int_lin_lt(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_lt("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_lin_lt_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_lt_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<","<<(*ce[3])<<")::"<<(*ann)<<"\n";
}
void p_int_lin_ge(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_ge("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_lin_ge_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_ge_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<","<<(*ce[3])<<")::"<<(*ann)<<"\n";
}
void p_int_lin_gt(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_gt("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_lin_gt_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_lin_gt_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<","<<(*ce[3])<<")::"<<(*ann)<<"\n";
}

/* arithmetic constraints */

void p_int_plus(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_plus("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}

void p_int_minus(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_minus("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}

void p_int_times(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_times("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_div(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_div("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_mod(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_mod("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}

void p_int_min(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_min("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_max(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_max("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_int_negate(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "int_negate("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}

void p_bool_eq(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_eq("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_eq_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_eq_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_ne(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_ne("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_ne_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_ne_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_ge(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_ge("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_ge_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_ge_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_le(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_le("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_le_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_le_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_gt(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_gt("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_gt_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_gt_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_lt(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_lt("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_lt_reif(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_lt_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}

void p_bool_or(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_or("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_and(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_and("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_array_bool_and(FlatZincModel& s, const ConExpr& ce, AST::Node* ann)
{
  Solver* const solver = s.solver();
  AST::Array* const array_variables = ce[0]->getArray();
  AST::Node* const node_boolvar = ce[1];
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = s.bv[array_variables->a[i]->getBoolVar()];
  }
  IntVar* const boolvar =
      node_boolvar->isBoolVar() ?
      s.bv[node_boolvar->getBoolVar()] :
      solver->MakeIntConst(node_boolvar->getBool());
  Constraint* const ct =
      solver->MakeMinEquality(variables, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}
void p_array_bool_or(FlatZincModel& s, const ConExpr& ce, AST::Node* ann)
{
  Solver* const solver = s.solver();
  AST::Array* const array_variables = ce[0]->getArray();
  AST::Node* const node_boolvar = ce[1];
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = s.bv[array_variables->a[i]->getBoolVar()];
  }
  IntVar* const boolvar =
      node_boolvar->isBoolVar() ?
      s.bv[node_boolvar->getBoolVar()] :
      solver->MakeIntConst(node_boolvar->getBool());
  Constraint* const ct =
      solver->MakeMaxEquality(variables, boolvar);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}
void p_array_bool_clause(FlatZincModel& s, const ConExpr& ce,
                         AST::Node* ann) {
  std::cerr << "array_bool_clause("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}
void p_array_bool_clause_reif(FlatZincModel& s, const ConExpr& ce,
                              AST::Node* ann) {
  std::cerr << "array_bool_clause_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_xor(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_xor("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_l_imp(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_l_imp("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_r_imp(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_r_imp("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_bool_not(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool_not("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}

/* element constraints */
void p_array_int_element(FlatZincModel& s, const ConExpr& ce,
                         AST::Node* ann) {
  std::cerr << "array_int_element("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}
void p_array_bool_element(FlatZincModel& s, const ConExpr& ce,
                          AST::Node* ann) {
  std::cerr << "array_bool_element("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
            <<")::"<<(*ann)<<"\n";
}

/* coercion constraints */
void p_bool2int(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "bool2int("<<(*ce[0])<<","<<(*ce[1])
            <<")::"<<(*ann)<<"\n";
}

void p_int_in(FlatZincModel& s, const ConExpr& ce, AST::Node *ann) {
  Solver* const solver = s.solver();
  AST::Node* const var_node = ce[0];
  AST::Node* const domain_node = ce[1];
  CHECK(var_node->isIntVar());
  CHECK(domain_node->isSet());
  IntVar* const var = s.iv[var_node->getIntVar()];
  AST::SetLit* const domain = domain_node->getSet();
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
}

void p_abs(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  std::cerr << "abs("<<(*ce[0])<<","<<(*ce[1])<<")::"<<(*ann)<<"\n";
}

void p_all_different_int(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
  Solver* const solver = s.solver();
  AST::Array* const array_variables = ce[0]->getArray();
  const int size = array_variables->a.size();
  std::vector<IntVar*> variables(size);
  for (int i = 0; i < size; ++i) {
    variables[i] = s.iv[array_variables->a[i]->getIntVar()];
  }
  Constraint* const ct = solver->MakeAllDifferent(variables);
  VLOG(1) << "Posted " << ct->DebugString();
  solver->AddConstraint(ct);
}

class IntPoster {
 public:
  IntPoster(void) {
    registry().add("int_eq", &p_int_eq);
    registry().add("int_ne", &p_int_ne);
    registry().add("int_ge", &p_int_ge);
    registry().add("int_gt", &p_int_gt);
    registry().add("int_le", &p_int_le);
    registry().add("int_lt", &p_int_lt);
    registry().add("int_eq_reif", &p_int_eq_reif);
    registry().add("int_ne_reif", &p_int_ne_reif);
    registry().add("int_ge_reif", &p_int_ge_reif);
    registry().add("int_gt_reif", &p_int_gt_reif);
    registry().add("int_le_reif", &p_int_le_reif);
    registry().add("int_lt_reif", &p_int_lt_reif);
    registry().add("int_lin_eq", &p_int_lin_eq);
    registry().add("int_lin_eq_reif", &p_int_lin_eq_reif);
    registry().add("int_lin_ne", &p_int_lin_ne);
    registry().add("int_lin_ne_reif", &p_int_lin_ne_reif);
    registry().add("int_lin_le", &p_int_lin_le);
    registry().add("int_lin_le_reif", &p_int_lin_le_reif);
    registry().add("int_lin_lt", &p_int_lin_lt);
    registry().add("int_lin_lt_reif", &p_int_lin_lt_reif);
    registry().add("int_lin_ge", &p_int_lin_ge);
    registry().add("int_lin_ge_reif", &p_int_lin_ge_reif);
    registry().add("int_lin_gt", &p_int_lin_gt);
    registry().add("int_lin_gt_reif", &p_int_lin_gt_reif);
    registry().add("int_plus", &p_int_plus);
    registry().add("int_minus", &p_int_minus);
    registry().add("int_times", &p_int_times);
    registry().add("int_div", &p_int_div);
    registry().add("int_mod", &p_int_mod);
    registry().add("int_min", &p_int_min);
    registry().add("int_max", &p_int_max);
    registry().add("int_abs", &p_abs);
    registry().add("int_negate", &p_int_negate);
    registry().add("bool_eq", &p_bool_eq);
    registry().add("bool_eq_reif", &p_bool_eq_reif);
    registry().add("bool_ne", &p_bool_ne);
    registry().add("bool_ne_reif", &p_bool_ne_reif);
    registry().add("bool_ge", &p_bool_ge);
    registry().add("bool_ge_reif", &p_bool_ge_reif);
    registry().add("bool_le", &p_bool_le);
    registry().add("bool_le_reif", &p_bool_le_reif);
    registry().add("bool_gt", &p_bool_gt);
    registry().add("bool_gt_reif", &p_bool_gt_reif);
    registry().add("bool_lt", &p_bool_lt);
    registry().add("bool_lt_reif", &p_bool_lt_reif);
    registry().add("bool_or", &p_bool_or);
    registry().add("bool_and", &p_bool_and);
    registry().add("bool_xor", &p_bool_xor);
    registry().add("array_bool_and", &p_array_bool_and);
    registry().add("array_bool_or", &p_array_bool_or);
    registry().add("bool_clause", &p_array_bool_clause);
    registry().add("bool_clause_reif", &p_array_bool_clause_reif);
    registry().add("bool_left_imp", &p_bool_l_imp);
    registry().add("bool_right_imp", &p_bool_r_imp);
    registry().add("bool_not", &p_bool_not);
    registry().add("array_int_element", &p_array_int_element);
    registry().add("array_var_int_element", &p_array_int_element);
    registry().add("array_bool_element", &p_array_bool_element);
    registry().add("array_var_bool_element", &p_array_bool_element);
    registry().add("bool2int", &p_bool2int);
    registry().add("int_in", &p_int_in);
    registry().add("all_different_int", &p_all_different_int);

  }
};
IntPoster __int_poster;

// void p_set_union(FlatZincModel& s, const ConExpr& ce, AST::Node *ann) {
//   std::cerr << "set_union("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_intersect(FlatZincModel& s, const ConExpr& ce, AST::Node *ann) {
//   std::cerr << "set_intersect("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_diff(FlatZincModel& s, const ConExpr& ce, AST::Node *ann) {
//   std::cerr << "set_diff("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }

// void p_set_symdiff(FlatZincModel& s, const ConExpr& ce, AST::Node* ann) {
//   std::cerr << "set_symdiff("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }

// void p_set_eq(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_eq("<<(*ce[0])<<","<<(*ce[1])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_ne(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_ne("<<(*ce[0])<<","<<(*ce[1])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_subset(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_subset("<<(*ce[0])<<","<<(*ce[1])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_superset(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_superset("<<(*ce[0])<<","<<(*ce[1])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_card(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_card("<<(*ce[0])<<","<<(*ce[1])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_in(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_in("<<(*ce[0])<<","<<(*ce[1])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_eq_reif(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_eq_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_ne_reif(FlatZincModel& s, const ConExpr& ce, AST::Node * ann) {
//   std::cerr << "set_ne_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_subset_reif(FlatZincModel& s, const ConExpr& ce,
//                        AST::Node *ann) {
//   std::cerr << "set_subset_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_superset_reif(FlatZincModel& s, const ConExpr& ce,
//                          AST::Node *ann) {
//   std::cerr << "set_superset_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_in_reif(FlatZincModel& s, const ConExpr& ce, AST::Node *ann) {
//   std::cerr << "set_in_reif("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }
// void p_set_disjoint(FlatZincModel& s, const ConExpr& ce, AST::Node *ann) {
//   std::cerr << "set_disjoint("<<(*ce[0])<<","<<(*ce[1])
//             <<")::"<<(*ann)<<"\n";
// }

// void p_array_set_element(FlatZincModel& s, const ConExpr& ce,
//                          AST::Node* ann) {
//   std::cerr << "array_set_element("<<(*ce[0])<<","<<(*ce[1])<<","<<(*ce[2])
//             <<")::"<<(*ann)<<"\n";
// }


// class SetPoster {
// public:
//   SetPoster(void) {
//     registry().add("set_eq", &p_set_eq);
//     registry().add("set_ne", &p_set_ne);
//     registry().add("set_union", &p_set_union);
//     registry().add("array_set_element", &p_array_set_element);
//     registry().add("array_var_set_element", &p_array_set_element);
//     registry().add("set_intersect", &p_set_intersect);
//     registry().add("set_diff", &p_set_diff);
//     registry().add("set_symdiff", &p_set_symdiff);
//     registry().add("set_subset", &p_set_subset);
//     registry().add("set_superset", &p_set_superset);
//     registry().add("set_card", &p_set_card);
//     registry().add("set_in", &p_set_in);
//     registry().add("set_eq_reif", &p_set_eq_reif);
//     registry().add("equal_reif", &p_set_eq_reif);
//     registry().add("set_ne_reif", &p_set_ne_reif);
//     registry().add("set_subset_reif", &p_set_subset_reif);
//     registry().add("set_superset_reif", &p_set_superset_reif);
//     registry().add("set_in_reif", &p_set_in_reif);
//     registry().add("disjoint", &p_set_disjoint);
//   }
// };
// SetPoster __set_poster;

}
}

// STATISTICS: flatzinc-any
