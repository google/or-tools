/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *  Modified by Laurent Perron for Google (lperron@google.com)
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

#include <vector>
#include <string>

#include "base/hash.h"
#include "base/concise_iterator.h"
#include "base/stringprintf.h"
#include "flatzinc/flatzinc.h"

using namespace std;

DEFINE_bool(backjump, false, "Use backjump is the minisat connection");
DECLARE_bool(use_minisat);

namespace operations_research {
SatPropagator* MakeSatPropagator(Solver* const solver, bool backjump);

FlatZincModel::FlatZincModel(void)
    : int_var_count(-1),
      bool_var_count(-1),
      set_var_count(-1),
      objective_variable_(-1),
      solve_annotations_(NULL),
      solver_(NULL),
      objective_(NULL),
      output_(NULL),
      parsed_ok_(true),
      sat_(NULL) {}

void FlatZincModel::Init(int intVars, int boolVars, int setVars) {
  int_var_count = 0;
  integer_variables_.resize(intVars);
  bool_var_count = 0;
  boolean_variables_.resize(boolVars);
  set_var_count = 0;
  set_variables_.resize(setVars);
}

void FlatZincModel::InitSolver() {
  solver_.reset(new Solver("FlatZincSolver"));
  if (FLAGS_use_minisat) {
    sat_ = MakeSatPropagator(solver_.get(), FLAGS_backjump);
    solver_->AddConstraint(reinterpret_cast<Constraint*>(sat_));
  } else {
    sat_ = NULL;
  }
}

void FlatZincModel::NewIntVar(const std::string& name,
                              IntVarSpec* const vs,
                              bool active) {
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
    VLOG(1) << "  - creates " << var->DebugString();
    if (!var->Bound()) {
      if (active) {
        active_variables_.push_back(var);
        VLOG(1) << "  - add as active";
      } else {
        introduced_variables_.push_back(var);
        VLOG(1) << "  - add as secondary";
      }
    }
  }
  integer_variables_[int_var_count++] = var;
}

void FlatZincModel::SkipIntVar() {
  integer_variables_[int_var_count++] = NULL;
}

void FlatZincModel::NewBoolVar(const std::string& name, BoolVarSpec* const vs) {
  IntVar* var = NULL;
  if (vs->alias) {
    var = boolean_variables_[vs->i]->Var();;
  } else if (vs->assigned) {
     var = solver_->MakeIntConst(vs->i, name);
  } else {
    var = solver_->MakeBoolVar(name);
    VLOG(1) << "  - creates " << var->DebugString();
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

void FlatZincModel::NewSetVar(const std::string& name, SetVarSpec* vs) {
  if (vs->alias) {
    set_variables_[set_var_count++] = set_variables_[vs->i];
  } else {
    AstSetLit* const domain = vs->domain_.value();
    if (domain->interval) {
      set_variables_[set_var_count++] =
          solver_->RevAlloc(
              new SetVar(solver_.get(), domain->imin, domain->imax));
    } else {
      set_variables_[set_var_count++] =
          solver_->RevAlloc(new SetVar(solver_.get(), domain->s));
    }
  }
}

void FlattenAnnotations(AstArray* const annotations,
                        std::vector<AstNode*>& out) {
  for (unsigned int i=0; i < annotations->a.size(); i++) {
    if (annotations->a[i]->isCall("seq_search")) {
      AstCall* c = annotations->a[i]->getCall();
      if (c->args->isArray()) {
        FlattenAnnotations(c->args->getArray(), out);
      } else {
        out.push_back(c->args);
      }
    } else {
      out.push_back(annotations->a[i]);
    }
  }
}

  // Comparison helpers. This one sorts in a decreasing way.
struct VarDegreeIndex {
  IntVar* v;
  int d;
  int i;

  VarDegreeIndex(IntVar* const var, int degree, int index)
      : v(var), d(degree), i(index) {}

  bool operator<(const VarDegreeIndex& other) const {
    return d > other.d || (d == other.d && i < other.i);
  }
};

void SortVariableByDegree(Solver* const solver,
                          std::vector<IntVar*>* const int_vars) {
  hash_map<IntVar*, int> degree_map;
  for (int i = 0; i < int_vars->size(); ++i) {
    degree_map[(*int_vars)[i]] = 0;
  }
  ModelVisitor* const degree_visitor =
      solver->MakeVariableDegreeVisitor(&degree_map);
  solver->Accept(degree_visitor);
  std::vector<VarDegreeIndex> to_sort;
  for (int i = 0; i < int_vars->size(); ++i) {
    IntVar* const var = (*int_vars)[i];
    to_sort.push_back(VarDegreeIndex(var, degree_map[var], i));
  }
  std::sort(to_sort.begin(), to_sort.end());
  for (int i = 0; i < int_vars->size(); ++i) {
    (*int_vars)[i] = to_sort[i].v;
  }
}

bool FlatZincModel::HasSolveAnnotations() const {
  bool has_annotations = false;
  if (solve_annotations_) {
    std::vector<AstNode*> flat_annotations;
    if (solve_annotations_->isArray()) {
      FlattenAnnotations(solve_annotations_->getArray(), flat_annotations);
    } else {
      flat_annotations.push_back(solve_annotations_);
    }
    for (unsigned int i = 0; i < flat_annotations.size(); i++) {
      try {
        AstCall *call = flat_annotations[i]->getCall("int_search");
        has_annotations = true;
      } catch (AstTypeError& e) {
        (void) e;
        try {
          AstCall *call = flat_annotations[i]->getCall("bool_search");
          has_annotations = true;
        } catch (AstTypeError& e) {
          (void) e;
        }
      }
    }
  }
  return has_annotations;
}

void FlatZincModel::CreateDecisionBuilders(const FlatZincSearchParameters& p) {
  const bool has_solve_annotations = HasSolveAnnotations();
  DefaultPhaseParameters parameters;
  parameters.search_strategy =
      p.search_type == FlatZincSearchParameters::IBS ?
      DefaultPhaseParameters::IMPACT_BASED_SEARCH :
      DefaultPhaseParameters::CHOOSE_FIRST_UNBOUND_ASSIGN_MIN;
  parameters.run_all_heuristics = true;
  parameters.heuristic_period =
      method_ != SAT || !p.all_solutions ? p.heuristic_period : -1;
  parameters.restart_log_size = p.restart_log_size;
  parameters.display_level = p.use_log ?
      (p.verbose_impact ?
       DefaultPhaseParameters::VERBOSE :
       DefaultPhaseParameters::NORMAL) :
      DefaultPhaseParameters::NONE;
  parameters.use_no_goods = true;
  parameters.var_selection_schema =
      DefaultPhaseParameters::CHOOSE_MAX_SUM_IMPACT;
  parameters.value_selection_schema =
      DefaultPhaseParameters::SELECT_MIN_IMPACT;

  VLOG(1) << "Create decision builders";
  if (has_solve_annotations) {
    CHECK_NOTNULL(solve_annotations_);
    std::vector<AstNode*> flat_annotations;
    if (solve_annotations_->isArray()) {
      FlattenAnnotations(solve_annotations_->getArray(), flat_annotations);
    } else {
      flat_annotations.push_back(solve_annotations_);
    }

    search_name_ = p.free_search ? "free" : "defined";
    for (unsigned int i = 0; i < flat_annotations.size(); i++) {
      try {
        AstCall *call = flat_annotations[i]->getCall("int_search");
        AstArray *args = call->getArgs(4);
        AstArray *vars = args->a[0]->getArray();
        std::vector<IntVar*> int_vars;
        for (int i = 0; i < vars->a.size(); ++i) {
          if (vars->a[i]->isIntVar()) {
            int_vars.push_back(
                integer_variables_[vars->a[i]->getIntVar()]->Var());
          }
        }
        if (p.free_search) {
          builders_.push_back(
              solver_->MakeDefaultPhase(int_vars, parameters));
        } else {
          Solver::IntVarStrategy str = Solver::CHOOSE_MIN_SIZE_LOWEST_MIN;
          if (args->hasAtom("input_order")) {
            str = Solver::CHOOSE_FIRST_UNBOUND;
          }
          if (args->hasAtom("first_fail")) {
            str = Solver::CHOOSE_MIN_SIZE;
          }
          if (args->hasAtom("anti_first_fail")) {
            str = Solver::CHOOSE_MAX_SIZE;
          }
          if (args->hasAtom("smallest")) {
            str = Solver::CHOOSE_LOWEST_MIN;
          }
          if (args->hasAtom("largest")) {
            str = Solver::CHOOSE_HIGHEST_MAX;
          }
          if (args->hasAtom("max_regret")) {
            str = Solver::CHOOSE_MAX_REGRET;
          }
          if (args->hasAtom("occurrence")) {
            SortVariableByDegree(solver_.get(), &int_vars);
            str = Solver::CHOOSE_FIRST_UNBOUND;
          }
          Solver::IntValueStrategy vstr = Solver::ASSIGN_MIN_VALUE;
          if (args->hasAtom("indomain_max")) {
            vstr = Solver::ASSIGN_MAX_VALUE;
          }
          if (args->hasAtom("indomain_median") ||
              args->hasAtom("indomain_middle")) {
            vstr = Solver::ASSIGN_CENTER_VALUE;
          }
          if (args->hasAtom("indomain_random")) {
            vstr = Solver::ASSIGN_RANDOM_VALUE;
          }
          if (args->hasAtom("indomain_split")) {
            vstr = Solver::SPLIT_LOWER_HALF;
          }
          if (args->hasAtom("indomain_reverse_split")) {
            vstr = Solver::SPLIT_UPPER_HALF;
          }
          builders_.push_back(solver_->MakePhase(int_vars, str, vstr));
        }
      } catch (AstTypeError& e) {
        (void) e;
        try {
          AstCall *call = flat_annotations[i]->getCall("bool_search");
          AstArray *args = call->getArgs(4);
          AstArray *vars = args->a[0]->getArray();
          std::vector<IntVar*> bool_vars;
          for (int i = 0; i < vars->a.size(); ++i) {
            if (vars->a[i]->isBoolVar()) {
              bool_vars.push_back(
                  boolean_variables_[vars->a[i]->getBoolVar()]->Var());
            }
          }
          if (p.free_search) {
            builders_.push_back(
                solver_->MakeDefaultPhase(bool_vars, parameters));
          } else {
            Solver::IntVarStrategy str = Solver::CHOOSE_MIN_SIZE_LOWEST_MIN;
            if (args->hasAtom("input_order")) {
              str = Solver::CHOOSE_FIRST_UNBOUND;
            }
            if (args->hasAtom("occurrence")) {
              SortVariableByDegree(solver_.get(), &bool_vars);
              str = Solver::CHOOSE_FIRST_UNBOUND;
            }
            Solver::IntValueStrategy vstr = Solver::ASSIGN_MAX_VALUE;
            if (args->hasAtom("indomain_min")) {
              vstr = Solver::ASSIGN_MIN_VALUE;
            }
            if (args->hasAtom("indomain_random")) {
              vstr = Solver::ASSIGN_RANDOM_VALUE;
            }
            builders_.push_back(solver_->MakePhase(bool_vars, str, vstr));
          }
        } catch (AstTypeError& e) {
          (void) e;
          try {
            AstCall *call = flat_annotations[i]->getCall("set_search");
            AstArray *args = call->getArgs(4);
            AstArray *vars = args->a[0]->getArray();
            LOG(FATAL) << "Search on set variables not supported";
          } catch (AstTypeError& e) {
            (void) e;
            if (!p.ignore_unknown) {
              LOG(WARNING) << "Warning, ignored search annotation: "
                           << flat_annotations[i]->DebugString();
            }
          }
        }
      }
      VLOG(1) << "  - adding decision builder = "
              << builders_.back()->DebugString();
    }
  } else {
    search_name_ = "automatic";
    builders_.push_back(
        solver_->MakeDefaultPhase(active_variables_, parameters));
    VLOG(1) << "  - adding decision builder = "
            << builders_.back()->DebugString();
    if (!introduced_variables_.empty() && !has_solve_annotations) {
      // Better safe than sorry.
      builders_.push_back(solver_->MakePhase(introduced_variables_,
                                             Solver::CHOOSE_FIRST_UNBOUND,
                                             Solver::ASSIGN_MIN_VALUE));
      VLOG(1) << "  - adding decision builder = "
              << builders_.back()->DebugString();
    }
  }
}

void FlatZincModel::Satisfy(AstArray* const annotations) {
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
  objective_ =
      solver_->MakeMinimize(integer_variables_[objective_variable_]->Var(), 1);
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
  objective_ =
      solver_->MakeMaximize(integer_variables_[objective_variable_]->Var(), 1);
}

FlatZincModel::~FlatZincModel(void) {
  delete solve_annotations_;
  delete output_;
}

string FlatZincMemoryUsage() {
  static const int64 kDisplayThreshold = 2;
  static const int64 kKiloByte = 1024;
  static const int64 kMegaByte = kKiloByte * kKiloByte;
  static const int64 kGigaByte = kMegaByte * kKiloByte;
  const int64 memory_usage = Solver::MemoryUsage();
  if (memory_usage > kDisplayThreshold * kGigaByte) {
    return StringPrintf("%.2lf GB",
                        memory_usage  * 1.0 / kGigaByte);
  } else if (memory_usage > kDisplayThreshold * kMegaByte) {
    return StringPrintf("%.2lf MB",
                        memory_usage  * 1.0 / kMegaByte);
  } else if (memory_usage > kDisplayThreshold * kKiloByte) {
    return StringPrintf("%2lf KB",
                        memory_usage * 1.0 / kKiloByte);
  } else {
    return StringPrintf("%" GG_LL_FORMAT "d", memory_usage);
  }
}

void FlatZincModel::Solve(FlatZincSearchParameters p) {
  if (!parsed_ok_) {
    return;
  }

  CreateDecisionBuilders(p);
  bool print_last = false;
  if (p.all_solutions && p.num_solutions == 0) {
    p.num_solutions = kint32max;
  } else if (objective_ == NULL && p.num_solutions == 0) {
    p.num_solutions = 1;
  } else if (objective_ != NULL && !p.all_solutions && p.num_solutions > 0) {
    p.num_solutions = kint32max;
    print_last = true;
  }

  std::vector<SearchMonitor*> monitors;
  switch (method_) {
    case MIN:
    case MAX: {
      SearchMonitor* const log = p.use_log ?
          solver_->MakeSearchLog(p.log_period, objective_) :
          NULL;
      monitors.push_back(log);
      monitors.push_back(objective_);
      break;
    }
    case SAT: {
      SearchMonitor* const log = p.use_log ?
          solver_->MakeSearchLog(p.log_period) :
          NULL;
      monitors.push_back(log);
      break;
    }
  }

  SearchLimit* const limit = (p.time_limit_in_ms > 0 ?
                              solver_->MakeLimit(p.time_limit_in_ms,
                                                 kint64max,
                                                 kint64max,
                                                 kint64max) :
                              NULL);
  monitors.push_back(limit);

  if (p.simplex_frequency > 0) {
    monitors.push_back(solver_->MakeSimplexConstraint(p.simplex_frequency));
  }

  if (p.luby_restart > 0) {
    monitors.push_back(solver_->MakeLubyRestart(p.luby_restart));
  }

  int count = 0;
  bool breaked = false;
  string last_solution;
  const int64 build_time = solver_->wall_time();
  solver_->NewSearch(solver_->Compose(builders_), monitors);
  while (solver_->NextSolution()) {
    if (output_ != NULL) {
      last_solution.clear();
      for (unsigned int i = 0; i < output_->a.size(); i++) {
        last_solution.append(DebugString(output_->a[i]));
      }
      last_solution.append("----------\n");
      if (!print_last) {
        std::cout << last_solution;
      }
    }
    count++;
    if (p.num_solutions > 0 && count >= p.num_solutions) {
      breaked = true;
      break;
    }
  }
  solver_->EndSearch();
  const int64 solve_time = solver_->wall_time() - build_time;
  if (print_last) {
    std::cout << last_solution;
  }
  bool proven = false;
  bool timeout = false;
  string final_output;
  if (limit != NULL && limit->crossed()) {
    final_output.append("%% TIMEOUT\n");
    timeout = true;
  } else if (!breaked && count == 0 && (limit == NULL || !limit->crossed())) {
    final_output.append("=====UNSATISFIABLE=====\n");
  } else if (!breaked && (limit == NULL || !limit->crossed())) {
    final_output.append("==========\n");
    proven = true;
  }
  final_output.append(StringPrintf(
      "%%%%  total runtime:        %" GG_LL_FORMAT "d ms\n",
      solve_time + build_time));
  final_output.append(
      StringPrintf("%%%%  build time:           %" GG_LL_FORMAT "d ms\n",
                   build_time));
  final_output.append(
      StringPrintf("%%%%  solve time:           %" GG_LL_FORMAT "d ms\n",
                   solve_time));
  final_output.append(
      StringPrintf("%%%%  solutions:            %" GG_LL_FORMAT "d\n",
                   solver_->solutions()));
  final_output.append(
      StringPrintf("%%%%  constraints:          %d\n", solver_->constraints()));
  final_output.append(
      StringPrintf("%%%%  normal propagations:  %" GG_LL_FORMAT "d\n",
                   solver_->demon_runs(Solver::NORMAL_PRIORITY)));
  final_output.append(
      StringPrintf("%%%%  delayed propagations: %" GG_LL_FORMAT "d\n",
                   solver_->demon_runs(Solver::DELAYED_PRIORITY)));
  final_output.append(
      StringPrintf("%%%%  branches:             %" GG_LL_FORMAT "d\n",
                   solver_->branches()));
  final_output.append(
      StringPrintf("%%%%  failures:             %" GG_LL_FORMAT "d\n",
                   solver_->failures()));
  final_output.append(StringPrintf("%%  memory:               %s\n",
                                   FlatZincMemoryUsage().c_str()));
  const int64 best = objective_ != NULL ? objective_->best() : 0;
  if (objective_ != NULL) {
    if (method_ == MIN && solver_->solutions() > 0) {
      final_output.append(
          StringPrintf("%%%%  min objective:        %" GG_LL_FORMAT "d%s\n",
                       best,
                       (proven ? " (proven)" : "")));
    } else if (solver_->solutions() > 0) {
      final_output.append(
          StringPrintf("%%%%  max objective:        %" GG_LL_FORMAT "d%s\n",
                       best,
                       (proven ? " (proven)" : "")));
    }
  }
  const int num_solutions_found = solver_->solutions();
  const bool no_solutions = num_solutions_found == 0;
  const string status_string =
      (no_solutions ?
       (timeout ? "**timeout**" : "**unsat**") :
       (objective_ == NULL ?
        "**sat**" :
        (timeout ? "**feasible**" :"**proven**")));
  const string obj_string = (objective_ != NULL && !no_solutions ?
                             StringPrintf("%" GG_LL_FORMAT "d", best) :
                             "");
  final_output.append("%%  name, status, obj, solns, s_time, b_time, br, "
                      "fails, cts, demon, delayed, mem, search\n");
  final_output.append(
      StringPrintf("%%%%  csv: %s, %s, %s, %d, %" GG_LL_FORMAT
                   "d ms, %" GG_LL_FORMAT "d ms, %" GG_LL_FORMAT "d, %"
                   GG_LL_FORMAT "d, %d, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT
                   "d, %s, %s\n",
                   filename_.c_str(),
                   status_string.c_str(),
                   obj_string.c_str(),
                   num_solutions_found,
                   solve_time,
                   build_time,
                   solver_->branches(),
                   solver_->failures(),
                   solver_->constraints(),
                   solver_->demon_runs(Solver::NORMAL_PRIORITY),
                   solver_->demon_runs(Solver::DELAYED_PRIORITY),
                   FlatZincMemoryUsage().c_str(),
                   search_name_.c_str()));
  std::cout << final_output;
}

void FlatZincModel::InitOutput(AstArray* const output) {
  output_ = output;
}

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
    output += StringPrintf("%d", var->Value());
  } else if (ai->isBoolVar()) {
    IntVar* const var = boolean_variables_[ai->getBoolVar()]->Var();
    output += var->Value() ? "true" : "false";
  } else if (ai->isSetVar()) {
    SetVar* const var = set_variables_[ai->getSetVar()];
    output += var->DebugString();
  } else if (ai->isBool()) {
    output += (ai->getBool() ? "true" : "false");
  } else if (ai->isSet()) {
    AstSetLit* s = ai->getSet();
    if (s->interval) {
      output += StringPrintf("%d..%d", s->imin, s->imax);
    } else {
      output += "{";
      for (unsigned int i=0; i<s->s.size(); i++) {
        output += StringPrintf("%d%s",
                               s->s[i],
                               (i < s->s.size()-1 ? ", " : "}"));
      }
    }
  } else if (ai->isString()) {
    string s = ai->getString();
    for (unsigned int i = 0; i < s.size(); i++) {
      if (s[i] == '\\' && i < s.size() - 1) {
        switch (s[i+1]) {
          case 'n': output += "\n"; break;
          case '\\': output += "\\"; break;
          case 't': output += "\t"; break;
          default: {
            output += "\\";
            output += s[i+1];
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
