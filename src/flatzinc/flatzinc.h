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

#ifndef OR_TOOLS_FLATZINC_H_
#define OR_TOOLS_FLATZINC_H_

#include <map>
#include <cassert>

#include "flatzinc/parser.h"
#include "constraint_solver/constraint_solver.h"

/**
 * \namespace operations_research
 * \brief Interpreter for the %FlatZinc language
 *
 * This namespace contains all functionality required
 * to parse and solve constraint models written in the %FlatZinc language.
 *
 */

namespace operations_research {
class SatPropagator;

struct FlatZincSearchParameters {
  enum SearchType {
    IBS,
    FIRST_UNBOUND,
    MIN_SIZE,
  };

  bool all_solutions;
  bool free_search;
  bool ignore_annotations;
  bool ignore_unknown;
  bool use_log;
  bool verbose_impact;
  double restart_log_size;
  int heuristic_period;
  int log_period;
  int luby_restart;
  int num_solutions;
  int simplex_frequency;
  int threads;
  int worker_id;
  int64 time_limit_in_ms;
  SearchType search_type;
};

/**
 * \brief A space that can be initialized with a %FlatZinc model
 *
 */
class FlatZincModel {
 public:
  /// Construct empty space
  FlatZincModel(void);

  /// Destructor
  ~FlatZincModel(void);

  Solver* solver() {
    return solver_.get();
  }

  /// Initialize space with given number of variables
  void Init(int num_int_variables,
            int num_bool_variables,
            int num_set_variables);
  void InitSolver();

  void InitOutput(AstArray* const output);

  /// Creates a new integer variable from specification.
  void NewIntVar(const std::string& name, IntVarSpec* const vs, bool active);
  // Skips the creation of the variable.
  void SkipIntVar();
  /// Creates a new boolean variable from specification.
  void NewBoolVar(const std::string& name, BoolVarSpec* const vs);
  // Skips the creation of the variable.
  void SkipBoolVar();
  // Creates a new set variable from specification.
  void NewSetVar(const std::string& name, SetVarSpec* const vs);

  IntExpr* GetIntExpr(AstNode* const node);

  void CheckIntegerVariableIsNull(AstNode* const node) const {
    CHECK_NOTNULL(node);
    if (node->isIntVar()) {
      CHECK(integer_variables_[node->getIntVar()] == NULL);
    } else if (node->isBoolVar()) {
      CHECK(boolean_variables_[node->getBoolVar()] == NULL);
    } else {
      LOG(FATAL) << "Wrong CheckIntegerVariableIsNull with "
                 << node->DebugString();
    }
  }

  void SetIntegerExpression(AstNode* const node, IntExpr* const expr) {
    CHECK_NOTNULL(node);
    CHECK_NOTNULL(expr);
    if (node->isIntVar()) {
      integer_variables_[node->getIntVar()] = expr;
    } else if (node->isBoolVar()) {
      boolean_variables_[node->getBoolVar()] = expr;
    } else {
      LOG(FATAL) << "Wrong SetIntegerVariable with " << node->DebugString();
    }
  }

  SetVar* SetVariable(int index) const {
    return set_variables_[index];
  }

  void SetSetVariable(int index, SetVar* const var) {
    set_variables_[index] = var;
  }

  /// Post a constraint specified by \a ce
  void PostConstraint(CtSpec* const spec);

  /// Post the solve item
  void Satisfy(AstArray* const annotation);
  /// Post that integer variable \a var should be minimized
  void Minimize(int var, AstArray* const annotation);
  /// Post that integer variable \a var should be maximized
  void Maximize(int var, AstArray* const annotation);

  /// Run the search
  void Solve(FlatZincSearchParameters parameters);

  // \brief Parse FlatZinc file \a fileName into \a fzs and return it.
  bool Parse(const std::string& fileName);

  // \brief Parse FlatZinc from \a is into \a fzs and return it.
  bool Parse(std::istream& is);

  SatPropagator* Sat() const { return sat_; }

  OptimizeVar* ObjectiveMonitor() const { return objective_; }

  bool HasSolveAnnotations() const;

 private:
  enum Meth {
    SAT, //< Solve as satisfaction problem
    MIN, //< Solve as minimization problem
    MAX  //< Solve as maximization problem
  };

  void CreateDecisionBuilders(const FlatZincSearchParameters& parameters);
  string DebugString(AstNode* const ai) const;

  /// Number of integer variables
  int int_var_count;
  /// Number of Boolean variables
  int bool_var_count;
  /// Number of set variables
  int set_var_count;

  scoped_ptr<Solver> solver_;
  std::vector<DecisionBuilder*> builders_;
  OptimizeVar* objective_;

  /// Index of the integer variable to optimize
  int objective_variable_;

  /// Whether to solve as satisfaction or optimization problem
  Meth method_;

  /// Annotations on the solve item
  AstArray* solve_annotations_;

  AstArray* output_;
  /// The integer variables
  std::vector<IntExpr*> integer_variables_;
  /// The Boolean variables
  std::vector<IntExpr*> boolean_variables_;
  /// The set variables
  std::vector<SetVar*> set_variables_;
  // Useful for search.
  std::vector<IntVar*> active_variables_;
  std::vector<IntVar*> introduced_variables_;
  bool parsed_ok_;
  string search_name_;
  string filename_;
  SatPropagator* sat_;
};

/// %Exception class for %FlatZinc errors
class Error {
 private:
  const std::string msg;
 public:
  Error(const std::string& where, const std::string& what)
      : msg(where+": "+what) {}
  const std::string& DebugString(void) const { return msg; }
};

}

#endif
