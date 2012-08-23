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


#include <vector>
#include <string>

#include "base/concise_iterator.h"
#include "base/hash.h"
#include "base/mutex.h"
#include "base/stringprintf.h"
#include "base/synchronization.h"
#include "flatzinc/flatzinc.h"

using namespace std;
namespace operations_research {
namespace {
class MtOptimizeVar : public OptimizeVar {
 public:
  MtOptimizeVar(Solver* const s,
                bool maximize,
                IntVar* const v,
                int64 step,
                FzParallelSupport* const support,
                int worker_id)
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
          StringPrintf(
              "Polling improved objective %" GG_LL_FORMAT "d",
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
  MtCustomLimit(Solver* const s,
                FzParallelSupport* const support,
                int worker_id)
      : SearchLimit(s),
        support_(support),
        worker_id_(worker_id) {}

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
  SequentialSupport(bool print_all, bool verbose)
  : print_all_(print_all),
    verbose_(verbose),
    type_(UNDEF),
    best_solution_(0) {}
  virtual ~SequentialSupport() {}

  virtual void Init(int worker_id, const string& init_string) {
    std::cout << init_string;
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
    std::cout << solution_string;
  }

  virtual void OptimizeSolution(int worker_id,
                                int64 value,
                                const string& solution_string) {
    best_solution_ = value;
    if (print_all_) {
      std::cout << solution_string;
    } else {
      last_solution_ = solution_string;
    }
  }

  virtual void FinalOutput(int worker_id, const string& final_output) {
    std::cout << final_output;
  }

  virtual bool ShouldFinish() const {
    return false;
  }

  virtual void EndSearch(int worker_id) {
    if (!last_solution_.empty()) {
      std::cout << last_solution_;
    }
  }

  virtual int64 BestSolution() const {
    return best_solution_;
  }

  virtual OptimizeVar* Objective(Solver* const s,
                                 bool maximize,
                                 IntVar* const var,
                                 int64 step,
                                 int worker_id) {
    return s->MakeOptimize(maximize, var, step);
  }

  virtual SearchLimit* Limit(Solver* const s, int worker_id) {
    return NULL;
  }

  virtual void Log(int worker_id, const string& message) {
    std::cout << "%%  worker " << worker_id << ": " << message << std::endl;
  }

 private:
  const bool print_all_;
  const bool verbose_;
  Type type_;
  string last_solution_;
  int64 best_solution_;
};

class MtSupport : public FzParallelSupport {
 public:
  MtSupport(bool print_all, bool verbose)
  : print_all_(print_all),
    verbose_(verbose),
    type_(UNDEF),
    best_solution_(0),
    last_worker_(-1),
    should_finish_(false) {}

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
    LogNoLock(worker_id, "solution found");
    std::cout << solution_string;
  }

  virtual void OptimizeSolution(int worker_id,
                                int64 value,
                                const string& solution_string) {
    MutexLock lock(&mutex_);
    switch (type_) {
      case MINIMIZE: {
        if (value < best_solution_) {
          best_solution_ = value;
          if (print_all_) {
            LogNoLock(
                worker_id,
                StringPrintf(
                    "solution found with value %" GG_LL_FORMAT "d",
                    value));
            std::cout << solution_string;
          } else {
            last_solution_ = solution_string;
            last_worker_ = worker_id;
          }
        }
        break;
      }
      case MAXIMIZE: {
        if (value > best_solution_) {
          best_solution_ = value;
          if (print_all_) {
            LogNoLock(
                worker_id,
                StringPrintf(
                    "solution found with value %" GG_LL_FORMAT "d",
                    value));
            std::cout << solution_string;
          } else {
            last_solution_ = solution_string;
            last_worker_ = worker_id;
          }
        }
        break;
      }
      default:
        LOG(ERROR) << "Should not be here";
    }
  }

  virtual void FinalOutput(int worker_id, const string& final_output) {
    MutexLock lock(&mutex_);
    std::cout << final_output;
  }

  virtual bool ShouldFinish() const {
    return should_finish_;
  }

  virtual void EndSearch(int worker_id) {
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
  }

  virtual int64 BestSolution() const {
    return best_solution_;
  }

  virtual OptimizeVar* Objective(Solver* const s,
                                 bool maximize,
                                 IntVar* const var,
                                 int64 step,
                                 int w) {
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
};

// Flatten Search annotations.
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

// Report memory usage in a nice way.
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
}  // namespace

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
  switch (p.search_type) {
    case FlatZincSearchParameters::IBS:
      parameters.search_strategy = DefaultPhaseParameters::IMPACT_BASED_SEARCH;
      break;
    case FlatZincSearchParameters::FIRST_UNBOUND:
      parameters.search_strategy =
          DefaultPhaseParameters::CHOOSE_FIRST_UNBOUND_ASSIGN_MIN;
      break;
    case FlatZincSearchParameters::MIN_SIZE:
      parameters.search_strategy =
          DefaultPhaseParameters::CHOOSE_MIN_SIZE_ASSIGN_MIN;
      break;
    case FlatZincSearchParameters::RANDOM_MIN:
      parameters.search_strategy =
          DefaultPhaseParameters::CHOOSE_RANDOM_ASSIGN_MIN;
      break;
    case FlatZincSearchParameters::RANDOM_MAX:
      parameters.search_strategy =
          DefaultPhaseParameters::CHOOSE_RANDOM_ASSIGN_MAX;
      break;
  }
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
  parameters.random_seed = p.random_seed;

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
    if (method_ != SAT && flat_annotations.size() == 1) {
      search_name_ = "automatic";
      builders_.push_back(
          solver_->MakeDefaultPhase(active_variables_, parameters));
      VLOG(1) << "  - adding decision builder = "
              << builders_.back()->DebugString();
    }

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

void FlatZincModel::Solve(FlatZincSearchParameters p,
                          FzParallelSupport* const parallel_support) {
  if (!parsed_ok_) {
    return;
  }

  CreateDecisionBuilders(p);
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
      objective_ = parallel_support->Objective(solver_.get(),
                                               method_ == MAX,
                                               integer_variables_[objective_variable_]->Var(),
                                               1,
                                               p.worker_id);
      SearchMonitor* const log = p.use_log ?
          solver_->MakeSearchLog(p.log_period, objective_) :
          NULL;
      monitors.push_back(log);
      monitors.push_back(objective_);
      parallel_support->StartSearch(p.worker_id,
                                    method_ == MAX ?
                                    FzParallelSupport::MAXIMIZE :
                                    FzParallelSupport::MINIMIZE);
      break;
    }
    case SAT: {
      SearchMonitor* const log = p.use_log ?
          solver_->MakeSearchLog(p.log_period) :
          NULL;
      monitors.push_back(log);
      parallel_support->StartSearch(p.worker_id, FzParallelSupport::SATISFY);

      break;
    }
  }
  // Custom limit in case of parallelism.
  monitors.push_back(parallel_support->Limit(solver_.get(), p.worker_id));

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
  string solution_string;
  const int64 build_time = solver_->wall_time();
  solver_->NewSearch(solver_->Compose(builders_), monitors);
  while (solver_->NextSolution()) {
    if (output_ != NULL) {
      solution_string.clear();
      for (unsigned int i = 0; i < output_->a.size(); i++) {
        solution_string.append(DebugString(output_->a[i]));
      }
      solution_string.append("----------\n");
      switch (method_) {
        case MIN:
        case MAX: {
          const int64 best = objective_ != NULL ? objective_->best() : 0;
          parallel_support->OptimizeSolution(p.worker_id,
                                             best,
                                             solution_string);
          break;
        }
        case SAT: {
          parallel_support->SatSolution(p.worker_id, solution_string);
        }
      }
    }
    count++;
    if (p.num_solutions > 0 && count >= p.num_solutions) {
      breaked = true;
      break;
    }
  }
  solver_->EndSearch();
  parallel_support->EndSearch(p.worker_id);
  const int64 solve_time = solver_->wall_time() - build_time;
  bool proven = false;
  bool timeout = false;
  string final_output;
  if (p.worker_id <= 0) {
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
        StringPrintf("%%%%  constraints:          %d\n",
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
    parallel_support->FinalOutput(p.worker_id, final_output);
  }
}

FzParallelSupport* MakeSequentialSupport(bool print_all, bool verbose) {
  return new SequentialSupport(print_all, verbose);
}

FzParallelSupport* MakeMtSupport(bool print_all, bool verbose) {
  return new MtSupport(print_all, verbose);
}
}  // namespace operations_research
