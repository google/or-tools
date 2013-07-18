/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *  Modified by Laurent Perron for OR-Tools (laurent.perron@gmail.com)
 *
 *  Copyright:
 *     Guido Tack, 2007
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

#include <iostream>  // NOLINT
#include <string>

#include "base/stringprintf.h"
#include "flatzinc/flatzinc.h"
#include "base/concise_iterator.h"
#include "base/hash.h"

DECLARE_bool(use_sat);
DEFINE_bool(logging, false,
            "Print logging information form the flatzinc interpreter.");

namespace operations_research {

SatPropagator* MakeSatPropagator(Solver* const solver);

FlatZincModel::FlatZincModel(void)
    : int_var_count(-1),
      bool_var_count(-1),
      objective_(NULL),
      objective_variable_(-1),
      solve_annotations_(NULL),
      output_(NULL),
      parsed_ok_(true),
      sat_(NULL) {}

void FlatZincModel::Init(int intVars, int boolVars, int setVars) {
  int_var_count = 0;
  integer_variables_.resize(intVars);
  bool_var_count = 0;
  boolean_variables_.resize(boolVars);
}

void FlatZincModel::InitSolver() {
  solver_.reset(new Solver("FlatZincSolver"));
  if (FLAGS_use_sat) {
    FZLOG << "  - Use minisat" << std::endl;
    sat_ = MakeSatPropagator(solver_.get());
    solver_->AddConstraint(reinterpret_cast<Constraint*>(sat_));
  } else {
    sat_ = NULL;
  }
}

void FlatZincModel::NewIntVar(const string& name, IntVarSpec* const vs,
                              bool active, bool appears_in_one_constraint) {
  IntVar* var = NULL;
  if (vs->alias) {
    var = integer_variables_[vs->i]->Var();
  } else if (vs->assigned) {
    var = solver_->MakeIntConst(vs->i, name);
  } else {
    if (!vs->HasDomain()) {
      var = solver_->MakeIntVar(kint32min, kint32max, name);
    } else {
      AstSetLit* const domain = vs->Domain();
      if (domain->interval) {
        var = solver_->MakeIntVar(domain->imin, domain->imax, name);
      } else {
        var = solver_->MakeIntVar(domain->s, name);
      }
    }
    VLOG(2) << "  - creates " << var->DebugString();
    if (!var->Bound()) {
      if (active) {
        active_variables_.push_back(var);
        VLOG(2) << "  - add as active";
      } else {
        introduced_variables_.push_back(var);
        VLOG(2) << "  - add as secondary";
      }
    }
  }
  integer_variables_[int_var_count++] = var;
}

void FlatZincModel::SkipIntVar() { integer_variables_[int_var_count++] = NULL; }

void FlatZincModel::NewBoolVar(const string& name, BoolVarSpec* const vs) {
  IntVar* var = NULL;
  if (vs->alias) {
    var = boolean_variables_[vs->i]->Var();
  } else if (vs->assigned) {
    var = solver_->MakeIntConst(vs->i, name);
  } else {
    var = solver_->MakeBoolVar(name);
    VLOG(2) << "  - creates " << var->DebugString();
    if (!var->Bound()) {
      if (!vs->introduced) {
        active_variables_.push_back(var);
      } else {
        introduced_variables_.push_back(var);
      }
    }
  }
  boolean_variables_[bool_var_count++] = var;
}

void FlatZincModel::SkipBoolVar() {
  boolean_variables_[bool_var_count++] = NULL;
}

void FlatZincModel::NewSetVar(const string& name, SetVarSpec* vs) {
  // if (vs->alias) {
  //   set_variables_[set_var_count++] = set_variables_[vs->i];
  // } else {
  //   AstSetLit* const domain = vs->domain_.value();
  //   if (domain->interval) {
  //     set_variables_[set_var_count++] =
  //         solver_->RevAlloc(
  //             new SetVar(solver_.get(), domain->imin, domain->imax));
  //   } else {
  //     set_variables_[set_var_count++] =
  //         solver_->RevAlloc(new SetVar(solver_.get(), domain->s));
  //   }
  // }
}

void FlatZincModel::AddConstraint(CtSpec* const spec, Constraint* const ct) {
  if (spec->Ignored()) {
    VLOG(2) << "Ignore " << spec->DebugString() << " ----> "
              << ct->DebugString();
  } else {
    solver_->AddConstraint(ct);
  }
}

void FlatZincModel::Satisfy(AstArray* const annotations) {
  objective_variable_ = -1;
  method_ = SAT;
  solve_annotations_ = annotations;
}

void FlatZincModel::Minimize(int var, AstArray* const annotations) {
  method_ = MIN;
  objective_variable_ = var;
  solve_annotations_ = annotations;
  // Branch on optimization variable to ensure that it is given a value.
  AstArray* args = new AstArray(4);
  args->a[0] = new AstArray(new AstIntVar(objective_variable_));
  args->a[1] = new AstAtom("input_order");
  args->a[2] = new AstAtom("indomain_min");
  args->a[3] = new AstAtom("complete");
  AstCall* c = new AstCall("int_search", args);
  if (!solve_annotations_) {
    solve_annotations_ = new AstArray(c);
  } else {
    solve_annotations_->a.push_back(c);
  }
}

void FlatZincModel::Maximize(int var, AstArray* const annotations) {
  method_ = MAX;
  objective_variable_ = var;
  solve_annotations_ = annotations;
  // Branch on optimization variable to ensure that it is given a value.
  AstArray* args = new AstArray(4);
  args->a[0] = new AstArray(new AstIntVar(objective_variable_));
  args->a[1] = new AstAtom("input_order");
  args->a[2] = new AstAtom("indomain_min");
  args->a[3] = new AstAtom("complete");
  AstCall* c = new AstCall("int_search", args);
  if (!solve_annotations_) {
    solve_annotations_ = new AstArray(c);
  } else {
    solve_annotations_->a.push_back(c);
  }
}

FlatZincModel::~FlatZincModel(void) {
  delete solve_annotations_;
  delete output_;
}

void FlatZincModel::InitOutput(AstArray* const output) { output_ = output; }

string FlatZincModel::DebugString(AstNode* const ai) const {
  string output;
  int k;
  if (ai->isArray()) {
    AstArray* aia = ai->getArray();
    int size = aia->a.size();
    output += "[";
    for (int j = 0; j < size; j++) {
      output += DebugString(aia->a[j]);
      if (j < size - 1) {
        output += ", ";
      }
    }
    output += "]";
  } else if (ai->isInt(k)) {
    output += StringPrintf("%d", k);
  } else if (ai->isIntVar()) {
    IntVar* const var = integer_variables_[ai->getIntVar()]->Var();
    output += StringPrintf("%" GG_LL_FORMAT "d", var->Value());
  } else if (ai->isBoolVar()) {
    IntVar* const var = boolean_variables_[ai->getBoolVar()]->Var();
    output += var->Value() ? "true" : "false";
  } else if (ai->isSetVar()) {
    // SetVar* const var = set_variables_[ai->getSetVar()];
    // output += var->DebugString();
  } else if (ai->isBool()) {
    output += (ai->getBool() ? "true" : "false");
  } else if (ai->isSet()) {
    AstSetLit* s = ai->getSet();
    if (s->interval) {
      output += StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d", s->imin,
                             s->imax);
    } else {
      output += "{";
      for (unsigned int i = 0; i < s->s.size(); i++) {
        output += StringPrintf("%" GG_LL_FORMAT "d%s", s->s[i],
                               (i < s->s.size() - 1 ? ", " : "}"));
      }
    }
  } else if (ai->isString()) {
    string s = ai->getString();
    for (unsigned int i = 0; i < s.size(); i++) {
      if (s[i] == '\\' && i < s.size() - 1) {
        switch (s[i + 1]) {
          case 'n':
            output += "\n";
            break;
          case '\\':
            output += "\\";
            break;
          case 't':
            output += "\t";
            break;
          default: {
            output += "\\";
            output += s[i + 1];
          }
        }
        i++;
      } else {
        output += s[i];
      }
    }
  }
  return output;
}

IntExpr* FlatZincModel::GetIntExpr(AstNode* const node) {
  if (node->isIntVar()) {
    return integer_variables_[node->getIntVar()];
  } else if (node->isBoolVar()) {
    return boolean_variables_[node->getBoolVar()];
  } else if (node->isInt()) {
    return solver_->MakeIntConst(node->getInt());
  } else if (node->isBool()) {
    return solver_->MakeIntConst(node->getBool());
  } else {
    LOG(FATAL) << "Cannot build an IntVar from " << node->DebugString();
    return NULL;
  }
}
}  // namespace operations_research
