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

#ifndef OR_TOOLS_FLATZINC_FLATZINC_H_
#define OR_TOOLS_FLATZINC_FLATZINC_H_

#include <map>
#include <cassert>
#include "base/unique_ptr.h"

#include "old_flatzinc/parser.h"
#include "constraint_solver/constraint_solver.h"

/*
 * \namespace operations_research
 * \brief Interpreter for the %FlatZinc language
 *
 * This namespace contains all functionality required
 * to parse and solve constraint models written in the %FlatZinc language.
 *
 */

#define FZLOG if (FLAGS_logging) std::cout << "%% "

namespace operations_research {
class SatPropagator;

struct FlatZincSearchParameters {
  FlatZincSearchParameters()
      : all_solutions(false),
        free_search(false),
        ignore_annotations(false),
        ignore_unknown(true),
        use_log(false),
        verbose_impact(false),
        run_all_heuristics(false),
        restart_log_size(-1.0),
        log_period(1000000),
        luby_restart(0),
        num_solutions(1),
        random_seed(0),
        simplex_frequency(0),
        threads(1),
        worker_id(-1),
        time_limit_in_ms(0),
        search_type(MIN_SIZE) {}

  enum SearchType {
    DEFAULT,
    IBS,
    FIRST_UNBOUND,
    MIN_SIZE,
    RANDOM_MIN,
    RANDOM_MAX,
  };

  bool all_solutions;
  bool free_search;
  bool ignore_annotations;
  bool ignore_unknown;
  bool use_log;
  bool verbose_impact;
  bool run_all_heuristics;
  double restart_log_size;
  int heuristic_period;
  int log_period;
  int luby_restart;
  int num_solutions;
  int random_seed;
  int simplex_frequency;
  int threads;
  int worker_id;
  int64 time_limit_in_ms;
  SearchType search_type;
};

class FzParallelSupport {
 public:
  enum Type {
    UNDEF,
    SATISFY,
    MINIMIZE,
    MAXIMIZE,
  };

  FzParallelSupport() : num_solutions_found_(0) {}
  virtual ~FzParallelSupport() {}
  virtual void Init(int worker_id, const std::string& init_string) = 0;
  virtual void StartSearch(int worker_id, Type type) = 0;
  virtual void SatSolution(int worker_id, const std::string& solution_string) = 0;
  virtual void OptimizeSolution(int worker_id, int64 value,
                                const std::string& solution_string) = 0;
  virtual void FinalOutput(int worker_id, const std::string& final_output) = 0;
  virtual bool ShouldFinish() const = 0;
  virtual void EndSearch(int worker_id, bool interrupted) = 0;
  virtual int64 BestSolution() const = 0;
  virtual OptimizeVar* Objective(Solver* const s, bool maximize,
                                 IntVar* const var, int64 step,
                                 int worker_id) = 0;
  virtual SearchLimit* Limit(Solver* const s, int worker_id) = 0;
  virtual void Log(int worker_id, const std::string& message) = 0;
  virtual bool Interrupted() const = 0;

  void IncrementSolutions() { num_solutions_found_++; }

  int NumSolutions() const { return num_solutions_found_; }

 private:
  int num_solutions_found_;
};

FzParallelSupport* MakeSequentialSupport(
    bool print_last, int num_solutions, bool verbose);
FzParallelSupport* MakeMtSupport(
    bool print_last, int num_solutions, bool verbose);

class FlatZincModel {
 public:
  enum Meth {
    SAT,  //< Solve as satisfaction problem
    MIN,  //< Solve as minimization problem
    MAX   //< Solve as maximization problem
  };

  // Construct empty space
  FlatZincModel(void);

  // Destructor
  ~FlatZincModel(void);

  Solver* solver() { return solver_.get(); }

  // Initialize space with given number of variables
  void Init(int num_int_variables, int num_bool_variables,
            int num_set_variables);
  void InitSolver();

  void InitOutput(AstArray* const output);

  // Creates a new integer variable from specification.
  void NewIntVar(const std::string& name, IntVarSpec* const vs, bool active);
  // Skips the creation of the variable.
  void SkipIntVar();
  // Creates a new boolean variable from specification.
  void NewBoolVar(const std::string& name, BoolVarSpec* const vs);
  // Skips the creation of the variable.
  void SkipBoolVar();
  // Creates a new set variable from specification.
  void NewSetVar(const std::string& name, SetVarSpec* const vs);

  // Adds a constraint to the model.
  void AddConstraint(CtSpec* const spec, Constraint* const ct);

  IntExpr* GetIntExpr(AstNode* const node);

  void CheckIntegerVariableIsNull(AstNode* const node) const {
    CHECK_NOTNULL(node);
    if (node->isIntVar()) {
      CHECK(integer_variables_[node->getIntVar()] == nullptr);
    } else if (node->isBoolVar()) {
      CHECK(boolean_variables_[node->getBoolVar()] == nullptr);
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

  void SetIntegerOccurrences(int var_index, int occurrences) {
    integer_occurrences_[var_index] = occurrences;
  }

  void SetBooleanOccurrences(int var_index, int occurrences) {
    boolean_occurrences_[var_index] = occurrences;
  }

  // Post a constraint specified by \a ce
  void PostConstraint(CtSpec* const spec);

  // Post the solve item
  void Satisfy(AstArray* const annotation);
  // Post that integer variable \a var should be minimized
  void Minimize(int var, AstArray* const annotation);
  // Post that integer variable \a var should be maximized
  void Maximize(int var, AstArray* const annotation);

  // Run the search
  void Solve(FlatZincSearchParameters parameters,
             FzParallelSupport* const parallel_support);

  // \brief Parse FlatZinc file \a fileName into \a fzs and return it.
  bool Parse(const std::string& fileName);

  // \brief Parse FlatZinc from \a is into \a fzs and return it.
  bool Parse(std::istream& is);  // NOLINT

  SatPropagator* Sat() const { return sat_; }

  OptimizeVar* ObjectiveMonitor() const { return objective_; }

  bool HasSolveAnnotations() const;

  DecisionBuilder* CreateDecisionBuilders(
      const FlatZincSearchParameters& parameters);

  void ParseSearchAnnotations(bool ignore_unknown,
                              std::vector<DecisionBuilder*>* const defined,
                              std::vector<IntVar*>* const defined_vars,
                              std::vector<IntVar*>* const active_vars,
                              std::vector<int>* const defined_occurrences,
                              std::vector<int>* const active_occurrences,
                              DecisionBuilder** obj_db);
  void AddCompletionDecisionBuilders(
      const std::vector<IntVar*>& defined_variables,
      const std::vector<IntVar*>& active_variables,
      std::vector<DecisionBuilder*>* const builders);

  const std::vector<IntVar*>& PrimaryVariables() const;
  const std::vector<IntVar*>& SecondaryVariables() const;
  const int objective_variable_index() const { return objective_variable_; }
  Meth ProblemType() const { return method_; }

 private:
  std::string DebugString(AstNode* const ai) const;

  void CollectOutputVariables(AstNode* const node);

  // Number of integer variables
  int int_var_count;
  // Number of Boolean variables
  int bool_var_count;
  // Number of set variables
  int set_var_count;

  std::unique_ptr<Solver> solver_;
  OptimizeVar* objective_;

  // Index of the integer variable to optimize
  int objective_variable_;

  // Whether to solve as satisfaction or optimization problem
  Meth method_;

  // Annotations on the solve item
  AstArray* solve_annotations_;

  AstArray* output_;
  // The integer variables
  std::vector<IntExpr*> integer_variables_;
  // The Boolean variables
  std::vector<IntExpr*> boolean_variables_;
  // Useful for search.
  std::vector<IntVar*> active_variables_;
  std::vector<int> active_occurrences_;
  std::vector<IntVar*> introduced_variables_;
  std::vector<IntVar*> output_variables_;
  bool parsed_ok_;
  std::string search_name_;
  std::string filename_;
  SatPropagator* sat_;
  std::vector<Constraint*> postponed_constraints_;
  std::vector<int> integer_occurrences_;
  std::vector<int> boolean_occurrences_;
};

// %Exception class for %FlatZinc errors
class FzError {
 private:
  const std::string msg;

 public:
  FzError(const std::string& where, const std::string& what)
      : msg(where + ": " + what) {}
  const std::string& DebugString(void) const { return msg; }
};

}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_FLATZINC_H_
