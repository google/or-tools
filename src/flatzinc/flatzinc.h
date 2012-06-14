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

#ifndef OR_TOOLS_FLATZINC_H_
#define OR_TOOLS_FLATZINC_H_

#include <map>
#include <cassert>

#include "flatzinc/ast.h"
#include "flatzinc/spec.h"

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
class SetVar {};

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
    return &solver_;
  }

  /// Initialize space with given number of variables
  void Init(int num_int_variables,
            int num_bool_variables,
            int num_set_variables);

  void InitOutput(AST::Array* const output);

  /// Create new integer variable from specification
  void NewIntVar(const std::string& name, IntVarSpec* const vs);
  // Skips the creation of the variable.
  void SkipIntVar();
  /// Create new Boolean variable from specification
  int IntVarCount() const { return integer_variables_.size(); }
  void NewBoolVar(const std::string& name, BoolVarSpec* const vs);
  // Skips the creation of the variable.
  void SkipBoolVar();


  IntVar* GetIntVar(AST::Node* const node);

  IntVar* IntegerVariable(int index) const {
    return integer_variables_[index];
  }

  void SetIntegerVariable(int index, IntVar* const var) {
    integer_variables_[index] = var;
  }

  IntVar* BooleanVariable(int index) const {
    return boolean_variables_[index];
  }

  void SetBooleanVariable(int index, IntVar* const var) {
    boolean_variables_[index] = var;
  }

  /// Post a constraint specified by \a ce
  void PostConstraint(CtSpec* const spec);

  /// Post the solve item
  void Satisfy(AST::Array* const annotation);
  /// Post that integer variable \a var should be minimized
  void Minimize(int var, AST::Array* const annotation);
  /// Post that integer variable \a var should be maximized
  void Maximize(int var, AST::Array* const annotation);

  /// Run the search
  void Solve(int log_frequency,
             bool log,
             bool all_solutions,
             bool ignore_annotations,
             int num_solutions,
             int time_limit_in_ms);

  // \brief Parse FlatZinc file \a fileName into \a fzs and return it.
  void Parse(const std::string& fileName);

  // \brief Parse FlatZinc from \a is into \a fzs and return it.
  void Parse(std::istream& is);

 private:
  enum Meth {
    SAT, //< Solve as satisfaction problem
    MIN, //< Solve as minimization problem
    MAX  //< Solve as maximization problem
  };

  void CreateDecisionBuilders(bool ignore_unknown, bool ignore_annotations);
  string DebugString(AST::Node* const ai) const;

  /// Number of integer variables
  int int_var_count;
  /// Number of Boolean variables
  int bool_var_count;
  /// Number of set variables
  int set_var_count;

  Solver solver_;
  std::vector<DecisionBuilder*> builders_;
  OptimizeVar* objective_;

  /// Index of the integer variable to optimize
  int objective_variable_;

  /// Whether to solve as satisfaction or optimization problem
  Meth method_;

  /// Annotations on the solve item
  AST::Array* solve_annotations_;

  AST::Array* output_;
  /// The integer variables
  std::vector<IntVar*> integer_variables_;
  /// The Boolean variables
  std::vector<IntVar*> boolean_variables_;
  /// The set variables
  std::vector<SetVar> sv;
  std::vector<IntVar*> active_variables_;
  std::vector<IntVar*> introduced_variables_;
  bool parsed_ok_;
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
