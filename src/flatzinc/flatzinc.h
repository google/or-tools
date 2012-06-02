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

#include "flatzinc/conexpr.h"
#include "flatzinc/ast.h"
#include "flatzinc/varspec.h"

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
  enum Meth {
    SAT, //< Solve as satisfaction problem
    MIN, //< Solve as minimization problem
    MAX  //< Solve as maximization problem
  };
 protected:
  /// Number of integer variables
  int int_var_count;
  /// Number of Boolean variables
  int bool_var_count;
  /// Number of set variables
  int set_var_count;

  /// Index of the integer variable to optimize
  int objective_variable_;

  /// Whether to solve as satisfaction or optimization problem
  Meth method_;

  /// Annotations on the solve item
  AST::Array* solve_annotations_;

  Solver solver_;
  std::vector<DecisionBuilder*> builders_;
  OptimizeVar* objective_;

 public:
  /// The integer variables
  std::vector<operations_research::IntVar*> integer_variables_;
  /// Indicates whether an integer variable is introduced by mzn2fzn
  std::vector<bool> integer_variables_introduced;
  /// Indicates whether an integer variable aliases a Boolean variable
  std::vector<int> integer_variables_boolalias;
  /// The Boolean variables
  std::vector<operations_research::IntVar*> boolean_variables_;
  /// Indicates whether a Boolean variable is introduced by mzn2fzn
  std::vector<bool> boolean_variables_introduced;
  /// The set variables
  std::vector<SetVar> sv;
  /// Indicates whether a set variable is introduced by mzn2fzn
  std::vector<bool> sv_introduced;

  Solver* solver() {
    return &solver_;
  }

  /// Construct empty space
  FlatZincModel(void);

  /// Destructor
  ~FlatZincModel(void);

  /// Initialize space with given number of variables
  void Init(int num_int_variables,
            int num_bool_variables,
            int num_set_variables);

  void InitOutput(AST::Array* const output);

  /// Create new integer variable from specification
  void NewIntVar(const std::string& name, IntVarSpec* const vs);
  /// Link integer variable \a iv to Boolean variable \a bv
  void aliasBool2Int(int iv, int bv);
  /// Return linked Boolean variable for integer variable \a iv
  int aliasBool2Int(int iv);
  /// Create new Boolean variable from specification
  void NewBoolVar(const std::string& name, BoolVarSpec* const vs);
  /// Create new set variable from specification
  void newSetVar(SetVarSpec* const vs);

  /// Post a constraint specified by \a ce
  void PostConstraint(const ConExpr& ce, AST::Node* const annotation);

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
             bool ignore_annotations);

  // \brief Parse FlatZinc file \a fileName into \a fzs and return it.
  void Parse(const std::string& fileName);

  // \brief Parse FlatZinc from \a is into \a fzs and return it.
  void Parse(std::istream& is);

 private:
  void CreateDecisionBuilders(bool ignore_unknown, bool ignore_annotations);
  string DebugString(AST::Node* const ai) const;

  AST::Array* output_;
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
