// Copyright 2010-2014 Google
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

#include "flatzinc/search.h"
#include <iostream>  // NOLINT
#include <string>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/map_util.h"
#include "base/hash.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "flatzinc/model.h"
#include "flatzinc/solver.h"

DECLARE_bool(fz_logging);
DECLARE_bool(fz_verbose);

namespace operations_research {
extern std::string DefaultPhaseStatString(DecisionBuilder* db);
namespace fz {
namespace {
// The Flatzinc SearchLog is just like a regular SearchLog, except
// that it uses stdout with a "%% " prefix instead of LOG(INFO).
class Log : public SearchLog {
 public:
  Log(operations_research::Solver* s, OptimizeVar* obj, int period)
      : SearchLog(s, obj, nullptr, nullptr, period) {}
  virtual ~Log() {}

 protected:
  virtual void OutputLine(const std::string& line) {
    std::cout << "%% " << line << std::endl;
  }
};

static bool ControlC = false;

class Interrupt : public SearchLimit {
 public:
  Interrupt(operations_research::Solver* const solver) : SearchLimit(solver) {}
  virtual ~Interrupt() {}

  virtual bool Check() { return ControlC; }

  virtual void Init() {}

  virtual void Copy(const SearchLimit* const limit) {}

  virtual SearchLimit* MakeClone() const {
    return solver()->RevAlloc(new Interrupt(solver()));
  }
};

// Comparison helpers. This one sorts in a decreasing way.
struct VarDegreeIndexSize {
  IntVar* v;
  int d;
  int i;
  uint64 s;

  VarDegreeIndexSize(IntVar* var, int degree, int index, uint64 size)
      : v(var), d(degree), i(index), s(size) {}

  int Bucket(uint64 size) const {
    if (size < 10) {
      return 0;
    } else if (size < 1000) {
      return 1;
    } else if (size < 100000) {
      return 2;
    } else {
      return 3;
    }
  }

  bool operator<(const VarDegreeIndexSize& other) const {
    const int b = Bucket(s);
    const int ob = Bucket(other.s);
    return b < ob ||
           (b == ob && (d > other.d || (d == other.d && i < other.i)));
  }
};

void SortVariableByDegree(const std::vector<int>& occurrences, bool use_size,
                          std::vector<IntVar*>* int_vars) {
  std::vector<VarDegreeIndexSize> to_sort;
  for (int i = 0; i < int_vars->size(); ++i) {
    IntVar* const var = (*int_vars)[i];
    const uint64 size = use_size ? var->Size() : 1;
    to_sort.push_back(VarDegreeIndexSize(var, occurrences[i], i, size));
  }
  std::sort(to_sort.begin(), to_sort.end());
  for (int i = 0; i < int_vars->size(); ++i) {
    (*int_vars)[i] = to_sort[i].v;
  }
}

// Report memory usage in a nice way.
std::string MemoryUsage() {
  static const int64 kDisplayThreshold = 2;
  static const int64 kKiloByte = 1024;
  static const int64 kMegaByte = kKiloByte * kKiloByte;
  static const int64 kGigaByte = kMegaByte * kKiloByte;
  const int64 memory_usage = operations_research::Solver::MemoryUsage();
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

// Flatten Search annotations.
void FlattenAnnotations(const Annotation& ann, std::vector<Annotation>* out) {
  if (ann.type == Annotation::ANNOTATION_LIST ||
      ann.IsFunctionCallWithIdentifier("seq_search")) {
    for (const Annotation& inner : ann.annotations) {
      FlattenAnnotations(inner, out);
    }
  } else {
    out->push_back(ann);
  }
}
}  // namespace

FlatzincParameters::FlatzincParameters()
    : all_solutions(false),
      free_search(false),
      last_conflict(false),
      ignore_annotations(false),
      ignore_unknown(true),
      use_log(false),
      verbose_impact(false),
      restart_log_size(-1.0),
      log_period(1000000),
      luby_restart(0),
      num_solutions(1),
      random_seed(0),
      threads(1),
      worker_id(-1),
      time_limit_in_ms(0),
      search_type(MIN_SIZE) {}

void MarkComputedVariables(Constraint* ct, hash_set<IntegerVariable*>* marked) {
  const std::string& id = ct->type;
  if (id == "global_cardinality") {
    FZVLOG << "  - marking " << ct->DebugString() << FZENDL;
    for (IntegerVariable* const var : ct->arguments[2].variables) {
      marked->insert(var);
    }
  }

  if (id == "array_var_int_element" && ct->target_variable == nullptr) {
    FZVLOG << "  - marking " << ct->DebugString() << FZENDL;
    marked->insert(ct->arguments[2].Var());
  }

  if (id == "maximum_int" && ct->arguments[0].IsVariable() &&
      ct->target_variable == nullptr) {
    marked->insert(ct->arguments[0].Var());
  }

  if (id == "minimum_int" && ct->arguments[0].IsVariable() &&
      ct->target_variable == nullptr) {
    marked->insert(ct->arguments[0].Var());
  }

  if (id == "int_lin_eq" && ct->target_variable == nullptr) {
    const std::vector<int64>& array_coefficients = ct->arguments[0].values;
    const int size = array_coefficients.size();
    const std::vector<IntegerVariable*>& array_variables =
        ct->arguments[1].variables;
    bool todo = true;
    if (size == 0) {
      return;
    }
    if (array_coefficients[0] == -1) {
      for (int i = 1; i < size; ++i) {
        if (array_coefficients[i] < 0) {
          todo = false;
          break;
        }
      }
      // Can mark the first one, this is a hidden sum.
      if (todo) {
        marked->insert(array_variables[0]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[0]->DebugString() << FZENDL;
        return;
      }
    }
    todo = true;
    if (array_coefficients[0] == 1) {
      for (int i = 1; i < size; ++i) {
        if (array_coefficients[i] > 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        marked->insert(array_variables[0]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[0]->DebugString() << FZENDL;
        return;
      }
    }
    todo = true;
    if (array_coefficients[size - 1] == 1) {
      for (int i = 0; i < size - 1; ++i) {
        if (array_coefficients[i] > 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        // Can mark the last one, this is a hidden sum.
        marked->insert(array_variables[size - 1]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[size - 1]->DebugString() << FZENDL;
        return;
      }
    }
    todo = true;
    if (array_coefficients[size - 1] == -1) {
      for (int i = 0; i < size - 1; ++i) {
        if (array_coefficients[i] < 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        // Can mark the last one, this is a hidden sum.
        marked->insert(array_variables[size - 1]);
        FZVLOG << "  - marking " << ct->DebugString() << ": "
               << array_variables[size - 1]->DebugString() << FZENDL;
        return;
      }
    }
  }
}

void Solver::ParseSearchAnnotations(bool ignore_unknown,
                                    std::vector<DecisionBuilder*>* defined,
                                    std::vector<IntVar*>* defined_variables,
                                    std::vector<IntVar*>* active_variables,
                                    std::vector<int>* defined_occurrences,
                                    std::vector<int>* active_occurrences) {
  std::vector<Annotation> flat_annotations;
  for (const Annotation& ann : model_.search_annotations()) {
    FlattenAnnotations(ann, &flat_annotations);
  }

  FZLOG << "  - parsing search annotations" << std::endl;
  hash_set<IntVar*> added;
  for (const Annotation& ann : flat_annotations) {
    FZLOG << "  - parse " << ann.DebugString() << FZENDL;
    if (ann.IsFunctionCallWithIdentifier("int_search")) {
      const std::vector<Annotation>& args = ann.annotations;
      const Annotation& vars = args[0];
      std::vector<IntVar*> int_vars;
      std::vector<int> occurrences;
      std::vector<IntegerVariable*> fz_vars;
      vars.AppendAllIntegerVariables(&fz_vars);
      for (IntegerVariable* const fz_var : fz_vars) {
        IntVar* const to_add = Extract(fz_var)->Var();
        const int occ = statistics_.NumVariableOccurrences(fz_var);
        if (!ContainsKey(added, to_add) && !to_add->Bound()) {
          added.insert(to_add);
          int_vars.push_back(to_add);
          occurrences.push_back(occ);
          defined_variables->push_back(to_add);
          defined_occurrences->push_back(occ);
        }
      }
      const Annotation& choose = args[1];
      operations_research::Solver::IntVarStrategy str =
          operations_research::Solver::CHOOSE_MIN_SIZE_LOWEST_MIN;
      if (choose.id == "input_order") {
        str = operations_research::Solver::CHOOSE_FIRST_UNBOUND;
      }
      if (choose.id == "first_fail") {
        str = operations_research::Solver::CHOOSE_MIN_SIZE;
      }
      if (choose.id == "anti_first_fail") {
        str = operations_research::Solver::CHOOSE_MAX_SIZE;
      }
      if (choose.id == "smallest") {
        str = operations_research::Solver::CHOOSE_LOWEST_MIN;
      }
      if (choose.id == "largest") {
        str = operations_research::Solver::CHOOSE_HIGHEST_MAX;
      }
      if (choose.id == "max_regret") {
        str = operations_research::Solver::CHOOSE_MAX_REGRET_ON_MIN;
      }
      if (choose.id == "occurrence") {
        SortVariableByDegree(occurrences, false, &int_vars);
        str = operations_research::Solver::CHOOSE_FIRST_UNBOUND;
      }
      if (choose.id == "most_constrained") {
        SortVariableByDegree(occurrences, false, &int_vars);
        str = operations_research::Solver::CHOOSE_MIN_SIZE;
      }
      const Annotation& select = args[2];
      operations_research::Solver::IntValueStrategy vstr =
          operations_research::Solver::ASSIGN_MIN_VALUE;
      if (select.id == "indomain_max") {
        vstr = operations_research::Solver::ASSIGN_MAX_VALUE;
      }
      if (select.id == "indomain_median" || select.id == "indomain_middle") {
        vstr = operations_research::Solver::ASSIGN_CENTER_VALUE;
      }
      if (select.id == "indomain_random") {
        vstr = operations_research::Solver::ASSIGN_RANDOM_VALUE;
      }
      if (select.id == "indomain_split") {
        vstr = operations_research::Solver::SPLIT_LOWER_HALF;
      }
      if (select.id == "indomain_reverse_split") {
        vstr = operations_research::Solver::SPLIT_UPPER_HALF;
      }
      DecisionBuilder* const db = solver()->MakePhase(int_vars, str, vstr);
      defined->push_back(db);
    } else if (ann.IsFunctionCallWithIdentifier("bool_search")) {
      const std::vector<Annotation>& args = ann.annotations;
      const Annotation& vars = args[0];
      std::vector<IntVar*> bool_vars;
      std::vector<int> occurrences;
      std::vector<IntegerVariable*> fz_vars;
      vars.AppendAllIntegerVariables(&fz_vars);
      for (IntegerVariable* const fz_var : fz_vars) {
        IntVar* const to_add = Extract(fz_var)->Var();
        const int occ = statistics_.NumVariableOccurrences(fz_var);
        if (!ContainsKey(added, to_add) && !to_add->Bound()) {
          added.insert(to_add);
          bool_vars.push_back(to_add);
          occurrences.push_back(occ);
          defined_variables->push_back(to_add);
          defined_occurrences->push_back(occ);
        }
      }
      const Annotation& choose = args[1];
      operations_research::Solver::IntVarStrategy str =
          operations_research::Solver::CHOOSE_FIRST_UNBOUND;
      if (choose.id == "occurrence") {
        SortVariableByDegree(occurrences, false, &bool_vars);
        str = operations_research::Solver::CHOOSE_FIRST_UNBOUND;
      }
      const Annotation& select = args[2];
      operations_research::Solver::IntValueStrategy vstr =
          operations_research::Solver::ASSIGN_MAX_VALUE;
      if (select.id == "indomain_min") {
        vstr = operations_research::Solver::ASSIGN_MIN_VALUE;
      }
      if (select.id == "indomain_random") {
        vstr = operations_research::Solver::ASSIGN_RANDOM_VALUE;
      }
      if (!bool_vars.empty()) {
        defined->push_back(solver()->MakePhase(bool_vars, str, vstr));
      }
    }
  }

  // Create the active_variables array, push smaller variables first.
  for (IntVar* const var : active_variables_) {
    if (!ContainsKey(added, var) && !var->Bound()) {
      if (var->Size() < 0xFFFF) {
        added.insert(var);
        active_variables->push_back(var);
        active_occurrences->push_back(extracted_occurrences_[var]);
      }
    }
  }
  for (IntVar* const var : active_variables_) {
    if (!ContainsKey(added, var) && !var->Bound()) {
      if (var->Size() >= 0xFFFF) {
        added.insert(var);
        active_variables->push_back(var);
        active_occurrences->push_back(extracted_occurrences_[var]);
      }
    }
  }
  FZVLOG << "Active variables = ["
         << JoinDebugStringPtr(*active_variables, ", ") << "]" << FZENDL;
}

void Solver::CollectOutputVariables(std::vector<IntVar*>* out) {
  for (const OnSolutionOutput& output : model_.output()) {
    if (output.variable != nullptr) {
      if (!ContainsKey(implied_variables_, output.variable)) {
        out->push_back(Extract(output.variable)->Var());
      }
    }
    for (IntegerVariable* const var : output.flat_variables) {
      if (var->defining_constraint == nullptr &&
          !ContainsKey(implied_variables_, var)) {
        out->push_back(Extract(var)->Var());
      }
    }
  }
}

// Add completion goals to be robust to incomplete search specifications.
void Solver::AddCompletionDecisionBuilders(
    const std::vector<IntVar*>& defined_variables,
    const std::vector<IntVar*>& active_variables, SearchLimit* limit,
    std::vector<DecisionBuilder*>* builders) {
  hash_set<IntVar*> defined_set(defined_variables.begin(),
                                defined_variables.end());
  std::vector<IntVar*> output_variables;
  CollectOutputVariables(&output_variables);
  std::vector<IntVar*> secondary_vars;
  for (IntVar* const var : active_variables) {
    if (!ContainsKey(defined_set, var) && !var->Bound()) {
      secondary_vars.push_back(var);
    }
  }
  for (IntVar* const var : output_variables) {
    if (!ContainsKey(defined_set, var) && !var->Bound()) {
      secondary_vars.push_back(var);
    }
  }
  if (!secondary_vars.empty()) {
    builders->push_back(solver()->MakeSolveOnce(
        solver()->MakePhase(secondary_vars,
                            operations_research::Solver::CHOOSE_FIRST_UNBOUND,
                            operations_research::Solver::ASSIGN_MIN_VALUE),
        limit));
  }
}

DecisionBuilder* Solver::CreateDecisionBuilders(const FlatzincParameters& p,
                                                SearchLimit* limit) {
  FZLOG << "Defining search" << (p.free_search ? "  (free)" : "  (fixed)")
        << std::endl;
  // Fill builders_ with predefined search.
  std::vector<DecisionBuilder*> defined;
  std::vector<IntVar*> defined_variables;
  std::vector<int> defined_occurrences;
  std::vector<IntVar*> active_variables;
  std::vector<int> active_occurrences;
  DecisionBuilder* obj_db = nullptr;
  ParseSearchAnnotations(p.ignore_unknown, &defined, &defined_variables,
                         &active_variables, &defined_occurrences,
                         &active_occurrences);

  search_name_ =
      defined.empty() ? "automatic" : (p.free_search ? "free" : "defined");

  // We fill builders with information from search (flags, annotations).
  std::vector<DecisionBuilder*> builders;
  if (!p.free_search && !defined.empty()) {
    builders = defined;
    default_phase_ = nullptr;
  } else {
    if (defined_variables.empty()) {
      CHECK(defined.empty());
      defined_variables.swap(active_variables);
      defined_occurrences.swap(active_occurrences);
    }
    DefaultPhaseParameters parameters;
    DecisionBuilder* inner_builder = nullptr;
    switch (p.search_type) {
      case FlatzincParameters::DEFAULT: {
        if (defined.empty()) {
          SortVariableByDegree(defined_occurrences, true, &defined_variables);
          inner_builder = solver()->MakePhase(
              defined_variables, operations_research::Solver::CHOOSE_MIN_SIZE,
              operations_research::Solver::ASSIGN_MIN_VALUE);
        } else {
          inner_builder = solver()->Compose(defined);
        }
        break;
      }
      case FlatzincParameters::IBS: {
        break;
      }
      case FlatzincParameters::FIRST_UNBOUND: {
        inner_builder = solver()->MakePhase(
            defined_variables,
            operations_research::Solver::CHOOSE_FIRST_UNBOUND,
            operations_research::Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatzincParameters::MIN_SIZE: {
        inner_builder = solver()->MakePhase(
            defined_variables,
            operations_research::Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
            operations_research::Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatzincParameters::RANDOM_MIN: {
        inner_builder = solver()->MakePhase(
            defined_variables, operations_research::Solver::CHOOSE_RANDOM,
            operations_research::Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatzincParameters::RANDOM_MAX: {
        inner_builder = solver()->MakePhase(
            defined_variables, operations_research::Solver::CHOOSE_RANDOM,
            operations_research::Solver::ASSIGN_MAX_VALUE);
      }
    }
    parameters.use_last_conflict = p.last_conflict;
    parameters.run_all_heuristics = p.run_all_heuristics;
    parameters.heuristic_period =
        model_.objective() != nullptr ||
                (!p.all_solutions && p.num_solutions == 1)
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
    if (inner_builder == nullptr) {
      CHECK_EQ(FlatzincParameters::IBS, p.search_type);
      parameters.decision_builder = nullptr;
    } else {
      parameters.decision_builder = inner_builder;
    }
    default_phase_ = solver()->MakeDefaultPhase(defined_variables, parameters);
    builders.push_back(default_phase_);
  }

  // Add the objective decision builder.
  if (obj_db != nullptr) {
    builders.push_back(obj_db);
  } else if (model_.objective() != nullptr) {
    // The model contains an objective, but the obj_db was not built.
    IntVar* const obj_var = Extract(model_.objective())->Var();
    obj_db = solver()->MakePhase(
        obj_var, operations_research::Solver::CHOOSE_FIRST_UNBOUND,
        model_.maximize() ? operations_research::Solver::ASSIGN_MAX_VALUE
                          : operations_research::Solver::ASSIGN_MIN_VALUE);
    builders.push_back(obj_db);
    FZVLOG << "  - adding objective decision builder = "
           << obj_db->DebugString() << FZENDL;
  }
  // Add completion decision builders to be more robust.
  AddCompletionDecisionBuilders(defined_variables, active_variables, limit,
                                &builders);
  // Reporting
  for (DecisionBuilder* const db : builders) {
    FZVLOG << "  - adding decision builder = " << db->DebugString() << FZENDL;
  }
  return solver()->Compose(builders);
}

void Solver::SyncWithModel() {
  for (Constraint* const ct : model_.constraints()) {
    if (ct->active) {
      MarkComputedVariables(ct, &implied_variables_);
    }
  }

  for (IntegerVariable* const fz_var : model_.variables()) {
    if (!fz_var->active || fz_var->defining_constraint != nullptr ||
        ContainsKey(implied_variables_, fz_var)) {
      continue;
    }
    IntExpr* const expr = Extract(fz_var);
    if (!expr->IsVar() || expr->Var()->Bound()) {
      continue;
    }
    IntVar* const var = expr->Var();
    extracted_occurrences_[var] = statistics_.NumVariableOccurrences(fz_var);
    active_variables_.push_back(var);
  }
  if (model_.objective() != nullptr) {
    objective_var_ = Extract(model_.objective())->Var();
  }
}

void Solver::Solve(FlatzincParameters p,
                   ParallelSupportInterface* parallel_support) {
  SyncWithModel();
  SearchLimit* const limit = p.time_limit_in_ms > 0
                                 ? solver()->MakeTimeLimit(p.time_limit_in_ms)
                                 : nullptr;

  SearchLimit* const shadow =
      limit == nullptr
          ? nullptr
          : solver()->MakeCustomLimit([limit]() { return limit->Check(); });
  DecisionBuilder* const db = CreateDecisionBuilders(p, shadow);
  std::vector<SearchMonitor*> monitors;
  if (model_.objective() != nullptr) {
    objective_monitor_ = parallel_support->Objective(
        solver(), model_.maximize(), objective_var_, 1, p.worker_id);
    SearchMonitor* const log =
        p.use_log
            ? solver()->RevAlloc(
                  new Log(solver(), objective_monitor_, p.log_period))
            : nullptr;
    SearchLimit* const ctrl_c = solver()->RevAlloc(new Interrupt(solver()));
    monitors.push_back(log);
    monitors.push_back(objective_monitor_);
    monitors.push_back(ctrl_c);
    parallel_support->StartSearch(
        p.worker_id, model_.maximize() ? ParallelSupportInterface::MAXIMIZE
                                       : ParallelSupportInterface::MINIMIZE);
  } else {
    SearchMonitor* const log =
        p.use_log ? solver()->RevAlloc(new Log(solver(), nullptr, p.log_period))
                  : nullptr;
    monitors.push_back(log);
    parallel_support->StartSearch(p.worker_id,
                                  ParallelSupportInterface::SATISFY);
  }
  // Custom limit in case of parallelism.
  monitors.push_back(parallel_support->Limit(solver(), p.worker_id));

  if (limit != nullptr) {
    FZLOG << "  - adding a time limit of " << p.time_limit_in_ms << " ms"
          << std::endl;
  }
  monitors.push_back(limit);

  if (p.all_solutions && p.num_solutions == kint32max) {
    FZLOG << "  - searching for all solutions" << std::endl;
  } else if (p.all_solutions && p.num_solutions > 1) {
    FZLOG << "  - searching for " << p.num_solutions << " solutions"
          << std::endl;
  } else if (model_.objective() == NULL ||
             (p.all_solutions && p.num_solutions == 1)) {
    FZLOG << "  - searching for the first solution" << std::endl;
  } else {
    FZLOG << "  - search for the best solution" << std::endl;
  }

  if (p.luby_restart > 0) {
    FZLOG << "  - using luby restart with a factor of " << p.luby_restart
          << std::endl;
    monitors.push_back(solver()->MakeLubyRestart(p.luby_restart));
  }
  if (p.last_conflict && p.free_search) {
    FZLOG << "  - using last conflict search hints" << std::endl;
  }

  bool breaked = false;
  std::string solution_string;
  const int64 build_time = solver()->wall_time();
  solver()->NewSearch(db, monitors);
  while (solver()->NextSolution()) {
    if (!parallel_support->ShouldFinish()) {
      solution_string.clear();
      if (!model_.output().empty()) {
        stored_values_.resize(stored_values_.size() + 1);
        for (const OnSolutionOutput& output : model_.output()) {
          solution_string.append(SolutionString(output, p.store_all_solutions));
          solution_string.append("\n");
        }
      }
      solution_string.append("----------");
      if (model_.objective() != nullptr) {
        const int64 best = objective_monitor_->best();
        parallel_support->OptimizeSolution(p.worker_id, best, solution_string);
        if ((p.num_solutions != 1 &&
             parallel_support->NumSolutions() >= p.num_solutions) ||
            (p.all_solutions && p.num_solutions == 1 &&
             parallel_support->NumSolutions() >= 1)) {
          break;
        }
      } else {
        parallel_support->SatSolution(p.worker_id, solution_string);
        if (parallel_support->NumSolutions() >= p.num_solutions) {
          break;
        }
      }
    }
  }
  solver()->EndSearch();
  parallel_support->EndSearch(p.worker_id,
                              limit != nullptr ? limit->crossed() : false);
  const int64 solve_time = solver()->wall_time() - build_time;
  const int num_solutions = parallel_support->NumSolutions();
  bool proven = false;
  bool timeout = false;
  std::string final_output;
  if (p.worker_id <= 0) {
    if (p.worker_id == 0) {
      // Recompute the breaked variable.
      if (model_.objective() == nullptr) {
        breaked = parallel_support->NumSolutions() >= p.num_solutions;
      } else {
        breaked =
            (p.num_solutions != 1 && num_solutions >= p.num_solutions) ||
            (p.all_solutions && p.num_solutions == 1 && num_solutions >= 1);
      }
    }
    if (parallel_support->Interrupted() || ControlC) {
      final_output.append("%% TIMEOUT\n");
      timeout = true;
    } else if (!breaked && num_solutions == 0 &&
               !parallel_support->Interrupted() && !ControlC) {
      final_output.append("=====UNSATISFIABLE=====\n");
    } else if (!breaked && !parallel_support->Interrupted() && !ControlC &&
               (model_.objective() != nullptr || p.all_solutions)) {
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
                                     solver()->constraints()));
    final_output.append(StringPrintf(
        "%%%%  normal propagations:  %" GG_LL_FORMAT "d\n",
        solver()->demon_runs(operations_research::Solver::NORMAL_PRIORITY)));
    final_output.append(StringPrintf(
        "%%%%  delayed propagations: %" GG_LL_FORMAT "d\n",
        solver()->demon_runs(operations_research::Solver::DELAYED_PRIORITY)));
    final_output.append(
        StringPrintf("%%%%  branches:             %" GG_LL_FORMAT "d\n",
                     solver()->branches()));
    final_output.append(
        StringPrintf("%%%%  failures:             %" GG_LL_FORMAT "d\n",
                     solver()->failures()));
    final_output.append(StringPrintf("%%%%  memory:               %s\n",
                                     MemoryUsage().c_str()));
    const int64 best = parallel_support->BestSolution();
    if (model_.objective() != nullptr) {
      if (!model_.maximize() && num_solutions > 0) {
        final_output.append(
            StringPrintf("%%%%  min objective:        %" GG_LL_FORMAT "d%s\n",
                         best, (proven ? " (proven)" : "")));
      } else if (num_solutions > 0) {
        final_output.append(
            StringPrintf("%%%%  max objective:        %" GG_LL_FORMAT "d%s\n",
                         best, (proven ? " (proven)" : "")));
      }
    }

    if (default_phase_ != nullptr) {
      const std::string default_search_stats =
          DefaultPhaseStatString(default_phase_);
      if (!default_search_stats.empty()) {
        final_output.append(StringPrintf("%%%%  free search stats:    %s\n",
                                         default_search_stats.c_str()));
      }
    }


    const bool no_solutions = num_solutions == 0;
    const std::string status_string =
        (no_solutions ? (timeout ? "**timeout**" : "**unsat**")
                      : (model_.objective() == nullptr
                             ? "**sat**"
                             : (timeout ? "**feasible**" : "**proven**")));
    const std::string obj_string = (model_.objective() != nullptr && !no_solutions
                                   ? StringPrintf("%" GG_LL_FORMAT "d", best)
                                   : "");
    final_output.append(
        "%%  name, status, obj, solns, s_time, b_time, br, "
        "fails, cts, demon, delayed, mem, search\n");
    final_output.append(StringPrintf(
        "%%%%  csv: %s, %s, %s, %d, %" GG_LL_FORMAT "d ms, %" GG_LL_FORMAT
        "d ms, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d, %d, %" GG_LL_FORMAT
        "d, %" GG_LL_FORMAT "d, %s, %s",
        model_.name().c_str(), status_string.c_str(), obj_string.c_str(),
        num_solutions, solve_time, build_time, solver()->branches(),
        solver()->failures(), solver()->constraints(),
        solver()->demon_runs(operations_research::Solver::NORMAL_PRIORITY),
        solver()->demon_runs(operations_research::Solver::DELAYED_PRIORITY),
        MemoryUsage().c_str(), search_name_.c_str()));
    parallel_support->FinalOutput(p.worker_id, final_output);
  }
}
}  // namespace fz
}  // namespace operations_research

void interrupt_handler(int s) {
  FZLOG << "Ctrl-C caught" << FZENDL;
  operations_research::fz::ControlC = true;
}
