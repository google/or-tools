/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
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

#include "base/stringprintf.h"
#include "flatzinc/flatzinc.h"
#include "flatzinc/registry.h"

#include <vector>
#include <string>
using namespace std;

namespace operations_research {

FlatZincModel::FlatZincModel(void)
    : intVarCount(-1), boolVarCount(-1), setVarCount(-1), _optVar(-1),
      _solveAnnotations(NULL),
      solver_("FlatZincSolver"),
      collector_(NULL),
      objective_(NULL) {}

void FlatZincModel::init(int intVars, int boolVars, int setVars) {
  intVarCount = 0;
  iv = std::vector<IntVar*>(intVars);
  iv_introduced = std::vector<bool>(intVars);
  iv_boolalias = std::vector<int>(intVars);
  boolVarCount = 0;
  bv = std::vector<IntVar*>(boolVars);
  bv_introduced = std::vector<bool>(boolVars);
  setVarCount = 0;
  sv = std::vector<SetVar>(setVars);
  sv_introduced = std::vector<bool>(setVars);
}

void FlatZincModel::newIntVar(const std::string& name, IntVarSpec* vs) {
  if (vs->alias) {
    iv[intVarCount++] = iv[vs->i];
  } else {
    AST::SetLit* domain = vs->domain.some();
    if (domain == NULL) {
      iv[intVarCount++] = solver_.MakeIntVar(kint32min, kint32max, name);
    } else if (domain->interval) {
      iv[intVarCount++] = solver_.MakeIntVar(domain->min, domain->max, name);
    } else {
      iv[intVarCount++] = solver_.MakeIntVar(domain->s, name);
    }
    VLOG(1) << "Create IntVar: " << iv[intVarCount - 1]->DebugString();
  }
  iv_introduced[intVarCount-1] = vs->introduced;
  iv_boolalias[intVarCount-1] = -1;
}

void FlatZincModel::aliasBool2Int(int iv, int bv) {
  iv_boolalias[iv] = bv;
}
int FlatZincModel::aliasBool2Int(int iv) {
  return iv_boolalias[iv];
}

void FlatZincModel::newBoolVar(const std::string& name, BoolVarSpec* vs) {
  if (vs->alias) {
    bv[boolVarCount++] = bv[vs->i];
  } else {
    bv[boolVarCount++] = solver_.MakeBoolVar(name);
    VLOG(1) << "Create BoolVar: " << bv[boolVarCount - 1]->DebugString();
  }
  bv_introduced[boolVarCount-1] = vs->introduced;
}

void
FlatZincModel::newSetVar(SetVarSpec* vs) {
  if (vs->alias) {
    sv[setVarCount++] = sv[vs->i];
  } else {
    LOG(FATAL) << "SetVar not supported";
    sv[setVarCount++] = SetVar();
  }
  sv_introduced[setVarCount-1] = vs->introduced;
}

void FlatZincModel::postConstraint(const ConExpr& ce, AST::Node* ann) {
  try {
    registry().post(*this, ce, ann);
  } catch (AST::TypeError& e) {
    throw Error("Type error", e.what());
  }
}

void flattenAnnotations(AST::Array* ann, std::vector<AST::Node*>& out) {
  for (unsigned int i=0; i<ann->a.size(); i++) {
    if (ann->a[i]->isCall("seq_search")) {
      AST::Call* c = ann->a[i]->getCall();
      if (c->args->isArray())
        flattenAnnotations(c->args->getArray(), out);
      else
        out.push_back(c->args);
    } else {
      out.push_back(ann->a[i]);
    }
  }
}

void FlatZincModel::createBranchers(AST::Node* ann,
                                    bool ignoreUnknown,
                                    std::ostream& err) {
  if (ann) {
    std::vector<AST::Node*> flatAnn;
    if (ann->isArray()) {
      flattenAnnotations(ann->getArray()  , flatAnn);
    } else {
      flatAnn.push_back(ann);
    }

    for (unsigned int i=0; i<flatAnn.size(); i++) {
      try {
        AST::Call *call = flatAnn[i]->getCall("int_search");
        AST::Array *args = call->getArgs(4);
        AST::Array *vars = args->a[0]->getArray();
        std::vector<IntVar*> int_vars;
        for (int i = 0; i < vars->a.size(); ++i) {
          int_vars.push_back(iv[vars->a[i]->getIntVar()]);
        }
        builders_.push_back(solver_.MakePhase(int_vars,
                                              Solver::CHOOSE_FIRST_UNBOUND,
                                              Solver::ASSIGN_MIN_VALUE));
      } catch (AST::TypeError& e) {
        (void) e;
        try {
          AST::Call *call = flatAnn[i]->getCall("bool_search");
          AST::Array *args = call->getArgs(4);
          AST::Array *vars = args->a[0]->getArray();
          std::cerr << "bool_search\n";
          // TODO: install search
        } catch (AST::TypeError& e) {
          (void) e;
          try {
            AST::Call *call = flatAnn[i]->getCall("set_search");
            AST::Array *args = call->getArgs(4);
            AST::Array *vars = args->a[0]->getArray();
            std::cerr << "set_search\n";
            // TODO: install search
          } catch (AST::TypeError& e) {
            (void) e;
            if (!ignoreUnknown) {
              err << "Warning, ignored search annotation: ";
              flatAnn[i]->print(err);
              err << std::endl;
            }
          }
        }
      }
      VLOG(1) << "Adding decision builder = "
              << builders_.back()->DebugString();
    }
  } else {
    std::vector<IntVar*> primary_integer_variables;
    std::vector<IntVar*> secondary_integer_variables;
    std::vector<SequenceVar*> sequence_variables;
    std::vector<IntervalVar*> interval_variables;
    solver_.CollectDecisionVariables(&primary_integer_variables,
                                     &secondary_integer_variables,
                                     &sequence_variables,
                                     &interval_variables);
    builders_.push_back(solver_.MakePhase(primary_integer_variables,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE));
    VLOG(1) << "Decision builder = " << builders_.back()->DebugString();
  }
}

AST::Array* FlatZincModel::solveAnnotations(void) const {
  return _solveAnnotations;
}

void FlatZincModel::solve(AST::Array* ann) {
  _method = SAT;
  _solveAnnotations = ann;
}

void FlatZincModel::minimize(int var, AST::Array* ann) {
  _method = MIN;
  _optVar = var;
  _solveAnnotations = ann;
  // Branch on optimization variable to ensure that it is given a value.
  AST::Array* args = new AST::Array(4);
  args->a[0] = new AST::Array(new AST::IntVar(_optVar));
  args->a[1] = new AST::Atom("input_order");
  args->a[2] = new AST::Atom("indomain_min");
  args->a[3] = new AST::Atom("complete");
  AST::Call* c = new AST::Call("int_search", args);
  if (!ann)
    ann = new AST::Array(c);
  else
    ann->a.push_back(c);
  objective_ = solver_.MakeMinimize(iv[_optVar], 1);
}

void FlatZincModel::maximize(int var, AST::Array* ann) {
  _method = MAX;
  _optVar = var;
  _solveAnnotations = ann;
  // Branch on optimization variable to ensure that it is given a value.
  AST::Array* args = new AST::Array(4);
  args->a[0] = new AST::Array(new AST::IntVar(_optVar));
  args->a[1] = new AST::Atom("input_order");
  args->a[2] = new AST::Atom("indomain_min");
  args->a[3] = new AST::Atom("complete");
  AST::Call* c = new AST::Call("int_search", args);
  if (!ann)
    ann = new AST::Array(c);
  else
    ann->a.push_back(c);
  objective_ = solver_.MakeMaximize(iv[_optVar], 1);
}

FlatZincModel::~FlatZincModel(void) {
  delete _solveAnnotations;
}

void FlatZincModel::run(std::ostream& out, const FzPrinter& p) {
  switch (_method) {
    case MIN:
    case MAX: {
      std::cerr << "start optimization search\n";
      SearchMonitor* const log = solver_.MakeSearchLog(100000);
      collector_ = solver_.MakeLastSolutionCollector();
      collector_->Add(iv);
      collector_->Add(bv);
      collector_->AddObjective(iv[_optVar]);
      solver_.Solve(solver_.Compose(builders_), log, collector_, objective_);
      break;
    }
    case SAT: {
      SearchMonitor* const log = solver_.MakeSearchLog(100000);
      collector_ = solver_.MakeFirstSolutionCollector();
      collector_->Add(iv);
      collector_->Add(bv);
      solver_.Solve(solver_.Compose(builders_), log, collector_);
      break;
    }
  }
}

FlatZincModel::Meth FlatZincModel::method(void) const {
  return _method;
}

int FlatZincModel::optVar(void) const {
  return _optVar;
}

void FlatZincModel::print(std::ostream& out, const FzPrinter& p) const {
  p.print(out, *this);
}

void FzPrinter::init(AST::Array* output) {
  _output = output;
}

void FzPrinter::printElem(std::ostream& out, AST::Node* ai,
                          const FlatZincModel& m) const {
  int k;
  if (ai->isInt(k)) {
    out << k;
  } else if (ai->isIntVar()) {
    IntVar* const var = m.iv[ai->getIntVar()];
    if (m.collector() != NULL && m.collector()->solution_count() > 0) {
      out << m.collector()->Value(0, var);
    } else {
      out << var->DebugString();
    }
  } else if (ai->isBoolVar()) {
    IntVar* const var = m.bv[ai->getIntVar()];
    if (m.collector() != NULL && m.collector()->solution_count() > 0) {
      out << m.collector()->Value(0, var);
    } else {
      out << var->DebugString();
    }
  } else if (ai->isSetVar()) {
    // TODO: output actual variable
    out << ai->getSetVar();
  } else if (ai->isBool()) {
    out << (ai->getBool() ? "true" : "false");
  } else if (ai->isSet()) {
    AST::SetLit* s = ai->getSet();
    if (s->interval) {
      out << s->min << ".." << s->max;
    } else {
      out << "{";
      for (unsigned int i=0; i<s->s.size(); i++) {
        out << s->s[i] << (i < s->s.size()-1 ? ", " : "}");
      }
    }
  } else if (ai->isString()) {
    std::string s = ai->getString();
    for (unsigned int i=0; i<s.size(); i++) {
      if (s[i] == '\\' && i<s.size()-1) {
        switch (s[i+1]) {
          case 'n': out << "\n"; break;
          case '\\': out << "\\"; break;
          case 't': out << "\t"; break;
          default: out << "\\" << s[i+1];
        }
        i++;
      } else {
        out << s[i];
      }
    }
  }
}

void FzPrinter::print(std::ostream& out, const FlatZincModel& m) const {
  if (_output == NULL)
    return;
  for (unsigned int i=0; i< _output->a.size(); i++) {
    AST::Node* ai = _output->a[i];
    if (ai->isArray()) {
      AST::Array* aia = ai->getArray();
      int size = aia->a.size();
      out << "[";
      for (int j=0; j<size; j++) {
        printElem(out,aia->a[j],m);
        if (j<size-1)
          out << ", ";
      }
      out << "]";
    } else {
      printElem(out,ai,m);
    }
  }
}

FzPrinter::~FzPrinter(void) {
  delete _output;
}
}

// STATISTICS: flatzinc-any
