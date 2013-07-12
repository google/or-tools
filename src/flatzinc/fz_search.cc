// Copyright 2010-2012 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-

#include <iostream>  // NOLINT
#include <string>
#include <vector>

#include "base/concise_iterator.h"
#include "base/hash.h"
#include "base/mutex.h"
#include "base/stringprintf.h"
#include "base/synchronization.h"
#include "flatzinc/flatzinc.h"

using std::string;
namespace operations_research {
namespace {
class MtOptimizeVar : public OptimizeVar {
 public:
  MtOptimizeVar(Solver* const s, bool maximize, IntVar* const v, int64 step,
                FzParallelSupport* const support, int worker_id)
      : OptimizeVar(s, maximize, v, step),
        support_(support),
        worker_id_(worker_id) {}

  virtual ~MtOptimizeVar() {}

  virtual void RefuteDecision(Decision* const d) {
    const int64 polled_best = support_->BestSolution();
    if ((maximize_ && polled_best > best_) ||
        (!maximize_ && polled_best < best_)) {
      support_->Log(
          worker_id_,
          StringPrintf("Polling improved objective %" GG_LL_FORMAT "d",
                       polled_best));
      best_ = polled_best;
    }
    OptimizeVar::RefuteDecision(d);
  }

 private:
  FzParallelSupport* const support_;
  const int worker_id_;
};

class MtCustomLimit : public SearchLimit {
 public:
  MtCustomLimit(Solver* const s, FzParallelSupport* const support,
                int worker_id)
      : SearchLimit(s), support_(support), worker_id_(worker_id) {}

  virtual ~MtCustomLimit() {}

  virtual void Init() {}

  virtual bool Check() {
    const bool result = support_->ShouldFinish();
    if (result) {
      support_->Log(worker_id_, "terminating");
    }
    return result;
  }

  virtual void Copy(const SearchLimit* const limit) {}

  virtual SearchLimit* MakeClone() const { return NULL; }

 private:
  FzParallelSupport* const support_;
  const int worker_id_;
};

class SequentialSupport : public FzParallelSupport {
 public:
  SequentialSupport(bool print_all, int num_solutions, bool verbose)
      : print_all_(print_all),
        verbose_(verbose),
        num_solutions_(num_solutions),
        type_(UNDEF),
        best_solution_(0),
        interrupted_(false) {}

  virtual ~SequentialSupport() {}

  virtual void Init(int worker_id, const string& init_string) {
    std::cout << init_string << std::endl;
  }

  virtual void StartSearch(int worker_id, Type type) {
    type_ = type;
    if (type == MAXIMIZE) {
      best_solution_ = kint64min;
    } else if (type_ == MINIMIZE) {
      best_solution_ = kint64max;
    }
  }

  virtual void SatSolution(int worker_id, const string& solution_string) {
    if (NumSolutions() == 0) {
      IncrementSolutions();
      std::cout << solution_string << std::endl;
    }
  }

  virtual void OptimizeSolution(int worker_id, int64 value,
                                const string& solution_string) {
    best_solution_ = value;
    if (print_all_) {
      std::cout << solution_string << std::endl;
    } else {
      last_solution_ = solution_string + "\n";
    }
    IncrementSolutions();
  }

  virtual void FinalOutput(int worker_id, const string& final_output) {
    std::cout << final_output << std::endl;
  }

  virtual bool ShouldFinish() const { return false; }

  virtual void EndSearch(int worker_id, bool interrupted) {
    if (!last_solution_.empty()) {
      std::cout << last_solution_;
    }
    interrupted_ = interrupted;
  }

  virtual int64 BestSolution() const { return best_solution_; }

  virtual OptimizeVar* Objective(Solver* const s, bool maximize,
                                 IntVar* const var, int64 step, int worker_id) {
    return s->MakeOptimize(maximize, var, step);
  }

  virtual SearchLimit* Limit(Solver* const s, int worker_id) { return NULL; }

  virtual void Log(int worker_id, const string& message) {
    std::cout << "%%  worker " << worker_id << ": " << message << std::endl;
  }

  virtual bool Interrupted() const { return interrupted_; }

 private:
  const bool print_all_;
  const bool verbose_;
  const int num_solutions_;
  Type type_;
  string last_solution_;
  int64 best_solution_;
  bool interrupted_;
};

class MtSupport : public FzParallelSupport {
 public:
  MtSupport(bool print_all, bool verbose)
      : print_all_(print_all),
        verbose_(verbose),
        type_(UNDEF),
        last_worker_(-1),
        best_solution_(0),
        should_finish_(false),
        interrupted_(false) {}

  virtual ~MtSupport() {}

  virtual void Init(int worker_id, const string& init_string) {
    MutexLock lock(&mutex_);
    if (worker_id == 0) {
      std::cout << init_string;
    }
    LogNoLock(worker_id, "starting");
  }

  virtual void StartSearch(int worker_id, Type type) {
    MutexLock lock(&mutex_);
    if (type_ == UNDEF) {
      type_ = type;
      if (type == MAXIMIZE) {
        best_solution_ = kint64min;
      } else if (type_ == MINIMIZE) {
        best_solution_ = kint64max;
      }
    }
  }

  virtual void SatSolution(int worker_id, const string& solution_string) {
    MutexLock lock(&mutex_);
    if (NumSolutions() == 0) {
      IncrementSolutions();
      LogNoLock(worker_id, "solution found");
      std::cout << solution_string << std::endl;
      should_finish_ = true;
    }
  }

  virtual void OptimizeSolution(int worker_id, int64 value,
                                const string& solution_string) {
    MutexLock lock(&mutex_);
    if (!should_finish_) {
      switch (type_) {
        case MINIMIZE: {
          if (value < best_solution_) {
            best_solution_ = value;
            IncrementSolutions();
            if (print_all_) {
              LogNoLock(
                  worker_id,
                  StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                               value));
              std::cout << solution_string << std::endl;
            } else {
              last_solution_ = solution_string + "\n";
              last_worker_ = worker_id;
            }
          }
          break;
        }
        case MAXIMIZE: {
          if (value > best_solution_) {
            best_solution_ = value;
            IncrementSolutions();
            if (print_all_) {
              LogNoLock(
                  worker_id,
                  StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                               value));
              std::cout << solution_string << std::endl;
            } else {
              last_solution_ = solution_string + "\n";
              last_worker_ = worker_id;
            }
          }
          break;
        }
        default:
          LOG(ERROR) << "Should not be here";
      }
    }
  }

  virtual void FinalOutput(int worker_id, const string& final_output) {
    MutexLock lock(&mutex_);
    std::cout << final_output << std::endl;
  }

  virtual bool ShouldFinish() const { return should_finish_; }

  virtual void EndSearch(int worker_id, bool interrupted) {
    MutexLock lock(&mutex_);
    LogNoLock(worker_id, "exiting");
    if (!last_solution_.empty()) {
      LogNoLock(last_worker_,
                StringPrintf("solution found with value %" GG_LL_FORMAT "d",
                             best_solution_));
      std::cout << last_solution_;
      last_solution_.clear();
    }
    should_finish_ = true;
    if (interrupted) {
      interrupted_ = true;
    }
  }

  virtual int64 BestSolution() const { return best_solution_; }

  virtual OptimizeVar* Objective(Solver* const s, bool maximize,
                                 IntVar* const var, int64 step, int w) {
    return s->RevAlloc(new MtOptimizeVar(s, maximize, var, step, this, w));
  }

  virtual SearchLimit* Limit(Solver* const s, int worker_id) {
    return s->RevAlloc(new MtCustomLimit(s, this, worker_id));
  }

  virtual void Log(int worker_id, const string& message) {
    if (verbose_) {
      MutexLock lock(&mutex_);
      std::cout << "%%  worker " << worker_id << ": " << message << std::endl;
    }
  }

  virtual bool Interrupted() const { return interrupted_; }

  void LogNoLock(int worker_id, const string& message) {
    if (verbose_) {
      std::cout << "%%  worker " << worker_id << ": " << message << std::endl;
    }
  }

 private:
  const bool print_all_;
  const bool verbose_;
  Mutex mutex_;
  Type type_;
  string last_solution_;
  int last_worker_;
  int64 best_solution_;
  bool should_finish_;
  bool interrupted_;
};

// Flatten Search annotations.
void FlattenAnnotations(AstArray* const annotations, std::vector<AstNode*>* out) {
  for (unsigned int i = 0; i < annotations->a.size(); i++) {
    if (annotations->a[i]->isCall("seq_search")) {
      AstCall* c = annotations->a[i]->getCall();
      if (c->args->isArray()) {
        FlattenAnnotations(c->args->getArray(), out);
      } else {
        out->push_back(c->args);
      }
    } else {
      out->push_back(annotations->a[i]);
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
    degree_map[ (*int_vars)[i]] = 0;
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

// Report memory usage in a nice way.
string FlatZincMemoryUsage() {
  static const int64 kDisplayThreshold = 2;
  static const int64 kKiloByte = 1024;
  static const int64 kMegaByte = kKiloByte * kKiloByte;
  static const int64 kGigaByte = kMegaByte * kKiloByte;
  const int64 memory_usage = Solver::MemoryUsage();
  if (memory_usage > kDisplayThreshold * kGigaByte) {
    return StringPrintf("%.2lf GB", memory_usage * 1.0 / kGigaByte);
  } else if (memory_usage > kDisplayThreshold * kMegaByte) {
    return StringPrintf("%.2lf MB", memory_usage * 1.0 / kMegaByte);
  } else if (memory_usage > kDisplayThreshold * kKiloByte) {
    return StringPrintf("%2lf KB", memory_usage * 1.0 / kKiloByte);
  } else {
    return StringPrintf("%" GG_LL_FORMAT "d", memory_usage);
  }
}

// Assign to bound decision builder
class AssignToBounds : public DecisionBuilder {
 public:
  explicit AssignToBounds(const std::vector<IntVar*>& vars)
      : vars_(vars), mins_(vars.size()), max_(vars.size()), init_(false) {}
  virtual ~AssignToBounds() {}

  virtual Decision* Next(Solver* const solver) {
    if (!init_) {
      solver->SaveAndSetValue(&init_, true);
      for (int i = 0; i < vars_.size(); ++i) {
        mins_[i] = vars_[i]->Min();
        max_[i] = vars_[i]->Max();
      }
    }
    for (int i = 0; i < vars_.size(); ++i) {
      if (vars_[i]->Bound()) {
        continue;
      }
      if (vars_[i]->Min() == mins_[i]) {
        return solver->MakeAssignVariableValue(vars_[i], mins_[i]);
      } else if (vars_[i]->Max() == max_[i]) {
        return solver->MakeAssignVariableValue(vars_[i], max_[i]);
      }
    }
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        return solver->MakeAssignVariableValue(vars_[i], vars_[i]->Min());
      }
    }
    return NULL;
  }

  virtual string DebugString() const { return "AssignToBounds"; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_.data(), vars_.size());
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  std::vector<IntVar*> vars_;
  std::vector<int64> mins_;
  std::vector<int64> max_;
  bool init_;
};
}  // namespace

bool FlatZincModel::HasSolveAnnotations() const {
  bool has_annotations = false;
  if (solve_annotations_) {
    std::vector<AstNode*> flat_annotations;
    if (solve_annotations_->isArray()) {
      FlattenAnnotations(solve_annotations_->getArray(), &flat_annotations);
    } else {
      flat_annotations.push_back(solve_annotations_);
    }
    for (unsigned int i = 0; i < flat_annotations.size(); i++) {
      try {
        flat_annotations[i]->getCall("int_search");
        has_annotations = true;
      }
      catch (AstTypeError & e) {
        (void) e;
        try {
          flat_annotations[i]->getCall("bool_search");
          has_annotations = true;
        }
        catch (AstTypeError & e) {
          (void) e;
        }
      }
    }
  }
  return has_annotations;
}

void FlatZincModel::ParseSearchAnnotations(
    bool ignore_unknown, std::vector<DecisionBuilder*>* const defined,
    std::vector<IntVar*>* const defined_variables,
    std::vector<IntVar*>* const active_variables, DecisionBuilder** obj_db) {
  const bool has_solve_annotations = HasSolveAnnotations();
  const bool satisfy = method_ == SAT;
  std::vector<AstNode*> flat_annotations;
  if (has_solve_annotations) {
    CHECK_NOTNULL(solve_annotations_);
    if (solve_annotations_->isArray()) {
      FlattenAnnotations(solve_annotations_->getArray(), &flat_annotations);
    } else {
      flat_annotations.push_back(solve_annotations_);
    }
  }

  VLOG(1) << "Create decision builders from search annotations";
  hash_set<IntVar*> added;
  for (unsigned int i = 0; i < flat_annotations.size(); i++) {
    try {
      AstCall* call = flat_annotations[i]->getCall("int_search");
      AstArray* args = call->getArgs(4);
      AstArray* vars = args->a[0]->getArray();
      std::vector<IntVar*> int_vars;
      for (int j = 0; j < vars->a.size(); ++j) {
        if (vars->a[j]->isIntVar()) {
          IntVar* const to_add =
              integer_variables_[vars->a[j]->getIntVar()]->Var();
          if (!ContainsKey(added, to_add)) {
            added.insert(to_add);
            int_vars.push_back(to_add);
            // Ignore the variable defined in the objective.
            if (satisfy || i != flat_annotations.size() - 1) {
              defined_variables->push_back(to_add);
            }
          }
        }
      }
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
        str = Solver::CHOOSE_MAX_REGRET_ON_MIN;
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
      DecisionBuilder* const db = solver_->MakePhase(int_vars, str, vstr);
      if (satisfy || i != flat_annotations.size() - 1) {
        defined->push_back(db);
      } else {
        *obj_db = db;
      }
    }
    catch (AstTypeError & e) {
      (void) e;
      try {
        AstCall* call = flat_annotations[i]->getCall("bool_search");
        AstArray* args = call->getArgs(4);
        AstArray* vars = args->a[0]->getArray();
        std::vector<IntVar*> bool_vars;
        for (int j = 0; j < vars->a.size(); ++j) {
          if (vars->a[j]->isBoolVar()) {
            IntVar* const to_add =
                boolean_variables_[vars->a[j]->getBoolVar()]->Var();
            if (!ContainsKey(added, to_add)) {
              added.insert(to_add);
              bool_vars.push_back(to_add);
              defined_variables->push_back(to_add);
            }
          }
        }
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
        defined->push_back(solver_->MakePhase(bool_vars, str, vstr));
      }
      catch (AstTypeError & e) {
        (void) e;
        try {
          AstCall* call = flat_annotations[i]->getCall("set_search");
          AstArray* args = call->getArgs(4);
          args->a[0]->getArray();
          LOG(FATAL) << "Search on set variables not supported";
        }
        catch (AstTypeError & e) {
          (void) e;
          if (!ignore_unknown) {
            LOG(WARNING) << "Warning, ignored search annotation: "
                         << flat_annotations[i]->DebugString();
          }
        }
      }
    }
  }

  // Create the active_variables array, push smaller variables first.
  for (int i = 0; i < active_variables_.size(); ++i) {
    IntVar* const var = active_variables_[i];
    if (!ContainsKey(added, var)) {
      if (var->Size() < 0xFFFF) {
        added.insert(var);
        active_variables->push_back(var);
      }
    }
  }
  for (int i = 0; i < active_variables_.size(); ++i) {
    IntVar* const var = active_variables_[i];
    if (!ContainsKey(added, var)) {
      if (var->Size() >= 0xFFFF) {
        added.insert(var);
        active_variables->push_back(var);
      }
    }
  }
}

// Add completion goals to be robust to incomplete search specifications.
void FlatZincModel::AddCompletionDecisionBuilders(
    const std::vector<IntVar*>& active_variables,
    std::vector<DecisionBuilder*>* const builders) {
  const bool has_solve_annotations = HasSolveAnnotations();
  std::vector<IntVar*> secondary_vars(active_variables);
  if (!has_solve_annotations) {
    secondary_vars.insert(secondary_vars.end(),
                          introduced_variables_.begin(),
                          introduced_variables_.end());
  }
  builders->push_back(solver_->MakeSolveOnce(solver_->MakePhase(
      secondary_vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE)));
}

DecisionBuilder* FlatZincModel::CreateDecisionBuilders(
    const FlatZincSearchParameters& p) {
  VLOG(1) << "Create decision builders";
  // Fill builders_ with predefined search.
  std::vector<DecisionBuilder*> defined;
  std::vector<IntVar*> defined_variables;
  std::vector<IntVar*> active_variables;
  DecisionBuilder* obj_db = NULL;
  ParseSearchAnnotations(p.ignore_unknown, &defined, &defined_variables,
                         &active_variables, &obj_db);

  search_name_ =
      defined.empty() ? "automatic" : (p.free_search ? "free" : "defined");

  // We fill builders with information from search (flags, annotations).
  std::vector<DecisionBuilder*> builders;
  if (!p.free_search && !defined.empty()) {
    builders = defined;
  } else {
    if (defined_variables.empty()) {
      CHECK(defined.empty());
      defined_variables.swap(active_variables);
    }
    DefaultPhaseParameters parameters;
    DecisionBuilder* inner_builder = NULL;
    switch (p.search_type) {
      case FlatZincSearchParameters::DEFAULT: {
        if (defined.empty()) {
          inner_builder =
              solver_->MakePhase(defined_variables, Solver::CHOOSE_MIN_SIZE,
                                 Solver::ASSIGN_MIN_VALUE);
        } else {
          inner_builder = solver_->Compose(defined);
        }
        break;
      }
      case FlatZincSearchParameters::IBS: { break; }
      case FlatZincSearchParameters::FIRST_UNBOUND: {
        inner_builder =
            solver_->MakePhase(defined_variables, Solver::CHOOSE_FIRST_UNBOUND,
                               Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatZincSearchParameters::MIN_SIZE: {
        inner_builder = solver_->MakePhase(defined_variables,
                                           Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                                           Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatZincSearchParameters::RANDOM_MIN: {
        inner_builder = solver_->MakePhase(
            defined_variables, Solver::CHOOSE_RANDOM, Solver::ASSIGN_MIN_VALUE);
      }
      case FlatZincSearchParameters::RANDOM_MAX: {
        inner_builder = solver_->MakePhase(
            defined_variables, Solver::CHOOSE_RANDOM, Solver::ASSIGN_MAX_VALUE);
      }
    }
    parameters.run_all_heuristics = false;
    parameters.heuristic_period =
        method_ != SAT || (!p.all_solutions && p.num_solutions == 1)
            ? p.heuristic_period
            : -1;
    parameters.restart_log_size = p.restart_log_size;
    parameters.display_level =
        p.use_log ? (p.verbose_impact ? DefaultPhaseParameters::VERBOSE
                                      : DefaultPhaseParameters::NORMAL)
                  : DefaultPhaseParameters::NONE;
    parameters.use_no_goods = (p.restart_log_size > 0);
    parameters.var_selection_schema =
        DefaultPhaseParameters::CHOOSE_MAX_SUM_IMPACT;
    parameters.value_selection_schema =
        DefaultPhaseParameters::SELECT_MIN_IMPACT;
    parameters.random_seed = p.random_seed;
    if (inner_builder == NULL) {
      CHECK_EQ(FlatZincSearchParameters::IBS, p.search_type);
      parameters.decision_builder = NULL;
    } else {
      parameters.decision_builder = inner_builder;
    }
    builders.push_back(
        solver_->MakeDefaultPhase(defined_variables, parameters));
  }
  // Add the objective decision builder.
  if (obj_db != NULL) {
    builders.push_back(obj_db);
  }
  // Add completion decision builders to be more robust.
  AddCompletionDecisionBuilders(active_variables, &builders);
  // Reporting
  for (int i = 0; i < builders.size(); ++i) {
    VLOG(1) << "  - adding decision builder = " << builders[i]->DebugString();
  }
  return solver_->Compose(builders);
}

const std::vector<IntVar*>& FlatZincModel::PrimaryVariables() const {
  return active_variables_;
}

const std::vector<IntVar*>& FlatZincModel::SecondaryVariables() const {
  return introduced_variables_;
}

void FlatZincModel::Solve(FlatZincSearchParameters p,
                          FzParallelSupport* const parallel_support) {
  if (!parsed_ok_) {
    return;
  }

  DecisionBuilder* const db = CreateDecisionBuilders(p);
  bool print_last = false;
  if (p.all_solutions && p.num_solutions == 0) {
    p.num_solutions = kint32max;
  } else if (method_ == SAT && p.num_solutions == 0) {
    p.num_solutions = 1;
  } else if (method_ != SAT && !p.all_solutions && p.num_solutions > 0) {
    p.num_solutions = kint32max;
    print_last = true;
  }

  std::vector<SearchMonitor*> monitors;
  switch (method_) {
    case MIN:
    case MAX: {
      objective_ = parallel_support->Objective(
          solver_.get(), method_ == MAX,
          integer_variables_[objective_variable_]->Var(), 1, p.worker_id);
      SearchMonitor* const log =
          p.use_log ? solver_->MakeSearchLog(p.log_period, objective_) : NULL;
      monitors.push_back(log);
      monitors.push_back(objective_);
      parallel_support->StartSearch(
          p.worker_id, method_ == MAX ? FzParallelSupport::MAXIMIZE
                                      : FzParallelSupport::MINIMIZE);
      break;
    }
    case SAT: {
      SearchMonitor* const log =
          p.use_log ? solver_->MakeSearchLog(p.log_period) : NULL;
      monitors.push_back(log);
      parallel_support->StartSearch(p.worker_id, FzParallelSupport::SATISFY);
      break;
    }
  }
  // Custom limit in case of parallelism.
  monitors.push_back(parallel_support->Limit(solver_.get(), p.worker_id));

  SearchLimit* const limit =
      p.time_limit_in_ms > 0 ? solver_->MakeTimeLimit(p.time_limit_in_ms)
                             : NULL;
  monitors.push_back(limit);

  if (p.simplex_frequency > 0) {
    monitors.push_back(solver_->MakeSimplexConstraint(p.simplex_frequency));
  }

  if (p.luby_restart > 0) {
    monitors.push_back(solver_->MakeLubyRestart(p.luby_restart));
  }

  bool breaked = false;
  string solution_string;
  const int64 build_time = solver_->wall_time();
  solver_->NewSearch(db, monitors);
  while (solver_->NextSolution()) {
    if (output_ != NULL && !parallel_support->ShouldFinish()) {
      solution_string.clear();
      for (unsigned int i = 0; i < output_->a.size(); i++) {
        solution_string.append(DebugString(output_->a[i]));
      }
      solution_string.append("----------");
      switch (method_) {
        case MIN:
        case MAX: {
          const int64 best = objective_ != NULL ? objective_->best() : 0;
          parallel_support->OptimizeSolution(p.worker_id, best,
                                             solution_string);
          break;
        }
        case SAT: {
          parallel_support->SatSolution(p.worker_id, solution_string);
        }
      }
    }
    if (p.num_solutions > 0 &&
        parallel_support->NumSolutions() >= p.num_solutions) {
      breaked = true;
      break;
    }
  }
  solver_->EndSearch();
  parallel_support->EndSearch(p.worker_id,
                              limit != NULL ? limit->crossed() : false);
  const int64 solve_time = solver_->wall_time() - build_time;
  const int num_solutions = parallel_support->NumSolutions();
  bool proven = false;
  bool timeout = false;
  string final_output;
  if (p.worker_id <= 0) {
    if (parallel_support->Interrupted()) {
      final_output.append("%% TIMEOUT\n");
      timeout = true;
    } else if (!breaked && num_solutions == 0 &&
               !parallel_support->Interrupted()) {
      final_output.append("=====UNSATISFIABLE=====\n");
    } else if (!breaked && !parallel_support->Interrupted()) {
      final_output.append("==========\n");
      proven = true;
    }
    final_output.append(
        StringPrintf("%%%%  total runtime:        %" GG_LL_FORMAT "d ms\n",
                     solve_time + build_time));
    final_output.append(StringPrintf(
        "%%%%  build time:           %" GG_LL_FORMAT "d ms\n", build_time));
    final_output.append(StringPrintf(
        "%%%%  solve time:           %" GG_LL_FORMAT "d ms\n", solve_time));
    final_output.append(
        StringPrintf("%%%%  solutions:            %d\n", num_solutions));
    final_output.append(StringPrintf("%%%%  constraints:          %d\n",
                                     solver_->constraints()));
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
    final_output.append(StringPrintf("%%%%  memory:               %s\n",
                                     FlatZincMemoryUsage().c_str()));
    const int64 best = objective_ != NULL ? objective_->best() : 0;
    if (objective_ != NULL) {
      if (method_ == MIN && solver_->solutions() > 0) {
        final_output.append(
            StringPrintf("%%%%  min objective:        %" GG_LL_FORMAT "d%s\n",
                         best, (proven ? " (proven)" : "")));
      } else if (solver_->solutions() > 0) {
        final_output.append(
            StringPrintf("%%%%  max objective:        %" GG_LL_FORMAT "d%s\n",
                         best, (proven ? " (proven)" : "")));
      }
    }
    const int num_solutions_found = solver_->solutions();
    const bool no_solutions = num_solutions_found == 0;
    const string status_string =
        (no_solutions
             ? (timeout ? "**timeout**" : "**unsat**")
             : (objective_ == NULL ? "**sat**" : (timeout ? "**feasible**"
                                                          : "**proven**")));
    const string obj_string = (objective_ != NULL && !no_solutions
                                   ? StringPrintf("%" GG_LL_FORMAT "d", best)
                                   : "");
    final_output.append("%%  name, status, obj, solns, s_time, b_time, br, "
                        "fails, cts, demon, delayed, mem, search\n");
    final_output.append(StringPrintf(
        "%%%%  csv: %s, %s, %s, %d, %" GG_LL_FORMAT "d ms, %" GG_LL_FORMAT
        "d ms, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d, %d, %" GG_LL_FORMAT
        "d, %" GG_LL_FORMAT "d, %s, %s",
        filename_.c_str(), status_string.c_str(), obj_string.c_str(),
        num_solutions_found, solve_time, build_time, solver_->branches(),
        solver_->failures(), solver_->constraints(),
        solver_->demon_runs(Solver::NORMAL_PRIORITY),
        solver_->demon_runs(Solver::DELAYED_PRIORITY),
        FlatZincMemoryUsage().c_str(), search_name_.c_str()));
    parallel_support->FinalOutput(p.worker_id, final_output);
  }
}

FzParallelSupport* MakeSequentialSupport(bool print_all, int num_solutions,
                                         bool verbose) {
  return new SequentialSupport(print_all, num_solutions, verbose);
}

FzParallelSupport* MakeMtSupport(bool print_all, bool verbose) {
  return new MtSupport(print_all, verbose);
}
}  // namespace operations_research
