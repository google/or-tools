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

#include "ortools/flatzinc/solver.h"

#include <string>
#include <unordered_set>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/flatzinc/checker.h"
#include "ortools/flatzinc/constraints.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/flatzinc/model.h"
#include "ortools/flatzinc/sat_constraint.h"
#include "ortools/util/string_array.h"

DEFINE_bool(fz_use_sat, true, "Use a sat solver for propagating on Booleans.");
DEFINE_bool(fz_check_solutions, true, "Check solutions");

namespace operations_research {

// TODO(user): Proper API.
extern std::string DefaultPhaseStatString(DecisionBuilder* db);

namespace fz {

// ----- Parameters -----
FlatzincParameters::FlatzincParameters()
    : all_solutions(false),
      free_search(false),
      last_conflict(false),
      ignore_annotations(false),
      ignore_unknown(true),
      logging(false),
      statistics(false),
      verbose_impact(false),
      restart_log_size(-1.0),
      run_all_heuristics(false),
      heuristic_period(100),
      log_period(1000000),
      luby_restart(0),
      num_solutions(1),
      random_seed(0),
      threads(0),
      thread_id(-1),
      time_limit_in_ms(0),
      search_type(MIN_SIZE),
      store_all_solutions(false) {}

// TODO(user): Investigate having the constants directly in the
// definition of the struct. This has to be tested with visual studio
// 2013 and 2015.

// ----- Solver -----

int64 Solver::SolutionValue(IntegerVariable* var) const {
  IntExpr* const result = FindPtrOrNull(data_.extracted_map(), var);
  if (result != nullptr) {
    if (result->IsVar()) {
      return result->Var()->Value();
    } else {
      int64 emin = 0;
      int64 emax = 0;
      result->Range(&emin, &emax);
      CHECK_EQ(emin, emax) << "Expression " << result->DebugString()
                           << " is not fixed to a single value at a solution";
      return emin;
    }
  } else {
    CHECK(var->domain.HasOneValue());
    return var->domain.values[0];
  }
}

// The format is fixed in the flatzinc specification.
std::string Solver::SolutionString(const SolutionOutputSpecs& output) const {
  if (output.variable != nullptr) {
    const int64 value = SolutionValue(output.variable);
    if (output.display_as_boolean) {
      return StringPrintf("%s = %s;", output.name.c_str(),
                          value == 1 ? "true" : "false");
    } else {
      return StringPrintf("%s = %" GG_LL_FORMAT "d;", output.name.c_str(),
                          value);
    }
  } else {
    const int bound_size = output.bounds.size();
    std::string result =
        StringPrintf("%s = array%dd(", output.name.c_str(), bound_size);
    for (int i = 0; i < bound_size; ++i) {
      if (output.bounds[i].max_value != 0) {
        result.append(StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d, ",
                                   output.bounds[i].min_value,
                                   output.bounds[i].max_value));
      } else {
        result.append("{},");
      }
    }
    result.append("[");
    for (int i = 0; i < output.flat_variables.size(); ++i) {
      const int64 value = SolutionValue(output.flat_variables[i]);
      if (output.display_as_boolean) {
        result.append(StringPrintf(value ? "true" : "false"));
      } else {
        StrAppend(&result, value);
      }
      if (i != output.flat_variables.size() - 1) {
        result.append(", ");
      }
    }
    result.append("]);");
    return result;
  }
  return "";
}

void Solver::StoreSolution() {
  stored_values_.resize(stored_values_.size() + 1);
  for (const SolutionOutputSpecs& output : model_.output()) {
    if (output.variable != nullptr) {
      const int64 value = SolutionValue(output.variable);
      stored_values_.back()[output.variable] = value;
    } else {
      for (IntegerVariable* const var : output.flat_variables) {
        const int64 value = SolutionValue(var);
        stored_values_.back()[var] = value;
      }
    }
  }
}

namespace {
struct ConstraintsWithRequiredVariables {
  Constraint* ct;
  int index;
  std::unordered_set<IntegerVariable*> required;

  ConstraintsWithRequiredVariables(
      Constraint* cte, int i,
      const std::unordered_set<IntegerVariable*>& defined)
      : ct(cte), index(i) {
    // Collect required variables.
    for (const Argument& arg : ct->arguments) {
      for (IntegerVariable* const var : arg.variables) {
        if (var != cte->target_variable && ContainsKey(defined, var)) {
          required.insert(var);
        }
      }
    }
  }

  std::string DebugString() const {
    return StringPrintf("Ctio(%s, %d, deps_size = %lu)", ct->type.c_str(),
                        index, required.size());
  }
};

int ComputeWeight(const ConstraintsWithRequiredVariables& c) {
  return c.required.size() * 2 + (c.ct->target_variable == nullptr);
}

// Comparator to sort constraints based on numbers of required
// elements and index. Reverse sorting to put elements to remove at the end.
struct ConstraintsWithRequiredVariablesComparator {
  bool operator()(ConstraintsWithRequiredVariables* a,
                  ConstraintsWithRequiredVariables* b) const {
    const int a_weight = ComputeWeight(*a);
    const int b_weight = ComputeWeight(*b);
    return a_weight > b_weight || (a_weight == b_weight && a->index > b->index);
  }
};
}  // namespace

bool Solver::Extract() {
  // Create the sat solver.
  if (FLAGS_fz_use_sat) {
    FZLOG << "  - Use sat" << FZENDL;
    data_.CreateSatPropagatorAndAddToSolver();
  }

  statistics_.BuildStatistics();

  // Extract the variable without defining constraint, and stores the other in
  // defined_variables.
  FZLOG << "Extract variables" << FZENDL;
  int extracted_variables = 0;
  int extracted_constants = 0;
  int skipped_variables = 0;
  std::unordered_set<IntegerVariable*> defined_variables;
  for (IntegerVariable* const var : model_.variables()) {
    if (var->defining_constraint == nullptr && var->active) {
      data_.Extract(var);
      if (var->domain.HasOneValue()) {
        extracted_constants++;
      } else {
        extracted_variables++;
      }
    } else {
      FZVLOG << "Skip " << var->DebugString() << FZENDL;
      if (var->defining_constraint != nullptr) {
        FZVLOG << "  - defined by " << var->defining_constraint->DebugString()
               << FZENDL;
      }
      defined_variables.insert(var);
      skipped_variables++;
    }
  }
  FZLOG << "  - " << extracted_variables << " variables created" << FZENDL;
  FZLOG << "  - " << extracted_constants << " constants created" << FZENDL;
  FZLOG << "  - " << skipped_variables << " variables skipped" << FZENDL;

  FZLOG << "Extract constraints" << FZENDL;

  // Sort constraints such that defined variables are created before the
  // extraction of the constraints that use them.
  // TODO(user): Look at our graph algorithms.
  int index = 0;
  std::vector<ConstraintsWithRequiredVariables*> to_sort;
  std::vector<Constraint*> sorted;
  std::unordered_map<const IntegerVariable*,
                     std::vector<ConstraintsWithRequiredVariables*>>
      dependencies;
  for (Constraint* ct : model_.constraints()) {
    if (ct != nullptr && ct->active) {
      ConstraintsWithRequiredVariables* const ctio =
          new ConstraintsWithRequiredVariables(ct, index++, defined_variables);
      to_sort.push_back(ctio);
      for (IntegerVariable* const var : ctio->required) {
        dependencies[var].push_back(ctio);
      }
    }
  }

  // Sort a first time.
  std::sort(to_sort.begin(), to_sort.end(),
            ConstraintsWithRequiredVariablesComparator());

  // Topological sort.
  while (!to_sort.empty()) {
    if (!to_sort.back()->required.empty()) {
      // Sort again.
      std::sort(to_sort.begin(), to_sort.end(),
                ConstraintsWithRequiredVariablesComparator());
    }
    ConstraintsWithRequiredVariables* const ctio = to_sort.back();
    if (!ctio->required.empty()) {
      // Recovery. We pick the last constraint (min number of required
      // variable)
      // And we clean all of them (mark as non target).
      std::vector<IntegerVariable*> required_vars(ctio->required.begin(),
                                             ctio->required.end());
      for (IntegerVariable* const fz_var : required_vars) {
        FZDLOG << "  - clean " << fz_var->DebugString() << FZENDL;
        if (fz_var->defining_constraint != nullptr) {
          fz_var->defining_constraint->target_variable = nullptr;
          fz_var->defining_constraint = nullptr;
        }
        for (ConstraintsWithRequiredVariables* const to_clean :
             dependencies[fz_var]) {
          to_clean->required.erase(fz_var);
        }
      }
      continue;
    }
    to_sort.pop_back();
    FZDLOG << "Pop " << ctio->ct->DebugString() << FZENDL;
    CHECK(ctio->required.empty());
    // TODO(user): Implement recovery mode.
    sorted.push_back(ctio->ct);
    IntegerVariable* const var = ctio->ct->target_variable;
    if (var != nullptr && ContainsKey(dependencies, var)) {
      FZDLOG << "  - clean " << var->DebugString() << FZENDL;
      for (ConstraintsWithRequiredVariables* const to_clean :
           dependencies[var]) {
        to_clean->required.erase(var);
      }
    }
    delete ctio;
  }

  // Start by identifying the all differents constraints. This does not process
  // them yet.
  for (Constraint* const ct : model_.constraints()) {
    if (ct->type == "all_different_int") {
      data_.StoreAllDifferent(ct->arguments[0].variables);
    }
  }

  // Then extract all constraints one by one.
  for (Constraint* const ct : sorted) {
    ExtractConstraint(&data_, ct);
  }

  // Display some nice statistics.
  FZLOG << "  - " << sorted.size() << " constraints parsed" << FZENDL;
  const int num_cp_constraints = solver_->constraints();
  if (num_cp_constraints <= 1) {
    FZLOG << "  - " << num_cp_constraints
          << " constraint added to the CP solver" << FZENDL;
  } else {
    FZLOG << "  - " << num_cp_constraints
          << " constraints added to the CP solver" << FZENDL;
  }
  // TODO(user): Print the number of sat clauses/constraints.

  // Add domain constraints to created expressions.
  int domain_constraints = 0;
  for (IntegerVariable* const var : model_.variables()) {
    if (var->defining_constraint != nullptr && var->active) {
      const Domain& domain = var->domain;
      if (!domain.is_interval && domain.values.size() == 2 &&
          domain.values[0] == 0 && domain.values[1] == 1) {
        // Canonicalize domains: {0, 1} -> [0 ,, 1]
        var->domain.is_interval = true;
      }
      IntExpr* const expr = data_.Extract(var);
      if (expr->IsVar() && domain.is_interval && !domain.values.empty() &&
          (expr->Min() < domain.values[0] || expr->Max() > domain.values[1])) {
        FZVLOG << "Intersect variable domain of " << expr->DebugString()
               << " with" << domain.DebugString() << FZENDL;
        expr->Var()->SetRange(domain.values[0], domain.values[1]);
      } else if (expr->IsVar() && !domain.is_interval) {
        FZVLOG << "Intersect variable domain of " << expr->DebugString()
               << " with " << domain.DebugString() << FZENDL;
        expr->Var()->SetValues(domain.values);
      } else if (domain.is_interval && !domain.values.empty() &&
                 (expr->Min() < domain.values[0] ||
                  expr->Max() > domain.values[1])) {
        FZVLOG << "Add domain constraint " << domain.DebugString() << " onto "
               << expr->DebugString() << FZENDL;
        solver_->AddConstraint(solver_->MakeBetweenCt(
            expr->Var(), domain.values[0], domain.values[1]));
        domain_constraints++;
      } else if (!domain.is_interval) {
        FZVLOG << "Add domain constraint " << domain.DebugString() << " onto "
               << expr->DebugString() << FZENDL;
        solver_->AddConstraint(
            solver_->MakeMemberCt(expr->Var(), domain.values));
        domain_constraints++;
      }
    }
  }
  if (domain_constraints == 1) {
    FZLOG << "  - 1 domain constraint added" << FZENDL;
  } else if (domain_constraints > 1) {
    FZLOG << "  - " << domain_constraints << " domain constraints added"
          << FZENDL;
  }

  return true;
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

  FZLOG << "  - parsing search annotations" << FZENDL;
  std::unordered_set<IntVar*> added;
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
        IntVar* const to_add = data_.Extract(fz_var)->Var();
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
      DecisionBuilder* const db = solver_->MakePhase(int_vars, str, vstr);
      defined->push_back(db);
    } else if (ann.IsFunctionCallWithIdentifier("bool_search")) {
      const std::vector<Annotation>& args = ann.annotations;
      const Annotation& vars = args[0];
      std::vector<IntVar*> bool_vars;
      std::vector<int> occurrences;
      std::vector<IntegerVariable*> fz_vars;
      vars.AppendAllIntegerVariables(&fz_vars);
      for (IntegerVariable* const fz_var : fz_vars) {
        IntVar* const to_add = data_.Extract(fz_var)->Var();
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
        defined->push_back(solver_->MakePhase(bool_vars, str, vstr));
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
  for (const SolutionOutputSpecs& output : model_.output()) {
    if (output.variable != nullptr) {
      if (!ContainsKey(implied_variables_, output.variable)) {
        out->push_back(data_.Extract(output.variable)->Var());
      }
    }
    for (IntegerVariable* const var : output.flat_variables) {
      if (var->defining_constraint == nullptr &&
          !ContainsKey(implied_variables_, var)) {
        out->push_back(data_.Extract(var)->Var());
      }
    }
  }
}

// Add completion goals to be robust to incomplete search specifications.
void Solver::AddCompletionDecisionBuilders(
    const std::vector<IntVar*>& defined_variables,
    const std::vector<IntVar*>& active_variables, SearchLimit* limit,
    std::vector<DecisionBuilder*>* builders) {
  std::unordered_set<IntVar*> defined_set(defined_variables.begin(),
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
    builders->push_back(solver_->MakeSolveOnce(
        solver_->MakePhase(secondary_vars,
                           operations_research::Solver::CHOOSE_FIRST_UNBOUND,
                           operations_research::Solver::ASSIGN_MIN_VALUE),
        limit));
  }
}

DecisionBuilder* Solver::CreateDecisionBuilders(const FlatzincParameters& p,
                                                SearchLimit* limit) {
  FZLOG << "Defining search" << (p.free_search ? "  (free)" : "  (fixed)")
        << FZENDL;
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
          inner_builder = solver_->MakePhase(
              defined_variables, operations_research::Solver::CHOOSE_MIN_SIZE,
              operations_research::Solver::ASSIGN_MIN_VALUE);
        } else {
          inner_builder = solver_->Compose(defined);
        }
        break;
      }
      case FlatzincParameters::IBS: {
        break;
      }
      case FlatzincParameters::FIRST_UNBOUND: {
        inner_builder = solver_->MakePhase(
            defined_variables,
            operations_research::Solver::CHOOSE_FIRST_UNBOUND,
            operations_research::Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatzincParameters::MIN_SIZE: {
        inner_builder = solver_->MakePhase(
            defined_variables,
            operations_research::Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
            operations_research::Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatzincParameters::RANDOM_MIN: {
        inner_builder = solver_->MakePhase(
            defined_variables, operations_research::Solver::CHOOSE_RANDOM,
            operations_research::Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case FlatzincParameters::RANDOM_MAX: {
        inner_builder = solver_->MakePhase(
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
        p.logging ? (p.verbose_impact ? DefaultPhaseParameters::VERBOSE
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
    default_phase_ = solver_->MakeDefaultPhase(defined_variables, parameters);
    builders.push_back(default_phase_);
  }

  // Add the objective decision builder.
  if (obj_db != nullptr) {
    builders.push_back(obj_db);
  } else if (model_.objective() != nullptr) {
    // The model contains an objective, but the obj_db was not built.
    IntVar* const obj_var = data_.Extract(model_.objective())->Var();
    obj_db = solver_->MakePhase(
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
  return solver_->Compose(builders);
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
    IntExpr* const expr = data_.Extract(fz_var);
    if (!expr->IsVar() || expr->Var()->Bound()) {
      continue;
    }
    IntVar* const var = expr->Var();
    extracted_occurrences_[var] = statistics_.NumVariableOccurrences(fz_var);
    active_variables_.push_back(var);
  }
  if (model_.objective() != nullptr) {
    objective_var_ = data_.Extract(model_.objective())->Var();
  }
}

void Solver::ReportInconsistentModel(const Model& model, FlatzincParameters p,
                                     SearchReportingInterface* report) {
  // Special mode. Print out failure status.
  const std::string search_status = "=====UNSATISFIABLE=====";
  report->Print(p.thread_id, search_status);
  if (p.statistics) {
    std::string solver_status =
        "%%  name, status, obj, solns, s_time, b_time, br, "
        "fails, cts, demon, delayed, mem, search\n";
    StringAppendF(
        &solver_status,
        "%%%%  csv: %s, **unsat**, , 0, 0 ms, 0 ms, 0, 0, 0, 0, 0, %s, free",
        model.name().c_str(), MemoryUsage().c_str());
    report->Print(p.thread_id, solver_status);
  }
}

void Solver::Solve(FlatzincParameters p, SearchReportingInterface* report) {
  SyncWithModel();
  SearchLimit* const limit = p.time_limit_in_ms > 0
                                 ? solver_->MakeTimeLimit(p.time_limit_in_ms)
                                 : nullptr;

  SearchLimit* const shadow =
      limit == nullptr
          ? nullptr
          : solver_->MakeCustomLimit([limit]() { return limit->Check(); });
  DecisionBuilder* const db = CreateDecisionBuilders(p, shadow);
  std::vector<SearchMonitor*> monitors;
  if (model_.objective() != nullptr) {
    objective_monitor_ = report->CreateObjective(
        solver_, model_.maximize(), objective_var_, 1, p.thread_id);
    SearchMonitor* const log =
        p.logging
            ? solver_->RevAlloc(
                  new Log(solver_, objective_monitor_, p.log_period))
            : nullptr;
    SearchLimit* const ctrl_c = solver_->RevAlloc(new Interrupt(solver_));
    monitors.push_back(log);
    monitors.push_back(objective_monitor_);
    monitors.push_back(ctrl_c);
    report->OnSearchStart(
        p.thread_id, model_.maximize() ? SearchReportingInterface::MAXIMIZE
                                       : SearchReportingInterface::MINIMIZE);
  } else {
    SearchMonitor* const log =
        p.logging ? solver_->RevAlloc(new Log(solver_, nullptr, p.log_period))
                  : nullptr;
    monitors.push_back(log);
    report->OnSearchStart(p.thread_id, SearchReportingInterface::SATISFY);
  }
  // Custom limit in case of parallelism.
  monitors.push_back(report->CreateLimit(solver_, p.thread_id));

  if (limit != nullptr) {
    FZLOG << "  - adding a time limit of " << p.time_limit_in_ms << " ms"
          << FZENDL;
  }
  monitors.push_back(limit);

  if (p.all_solutions && p.num_solutions == kint32max) {
    FZLOG << "  - searching for all solutions" << FZENDL;
  } else if (p.all_solutions && p.num_solutions > 1) {
    FZLOG << "  - searching for " << p.num_solutions << " solutions" << FZENDL;
  } else if (model_.objective() == nullptr ||
             (p.all_solutions && p.num_solutions == 1)) {
    FZLOG << "  - searching for the first solution" << FZENDL;
  } else {
    FZLOG << "  - search for the best solution" << FZENDL;
  }

  if (p.luby_restart > 0) {
    FZLOG << "  - using luby restart with a factor of " << p.luby_restart
          << FZENDL;
    monitors.push_back(solver_->MakeLubyRestart(p.luby_restart));
  }
  if (p.last_conflict && p.free_search) {
    FZLOG << "  - using last conflict search hints" << FZENDL;
  }
  if (FLAGS_fz_check_solutions) {
    FZLOG << "  - using solution checker" << FZENDL;
  }

  bool breaked = false;
  std::string solution_string;
  const int64 build_time = solver_->wall_time();
  solver_->NewSearch(db, monitors);
  while (solver_->NextSolution()) {
    if (FLAGS_fz_check_solutions) {
      CHECK(CheckSolution(
          model_, [this](IntegerVariable* v) { return SolutionValue(v); }));
    }
    if (!report->ShouldFinish()) {
      solution_string.clear();
      if (!model_.output().empty()) {
        for (const SolutionOutputSpecs& output : model_.output()) {
          solution_string.append(SolutionString(output));
          solution_string.append("\n");
        }
        if (p.store_all_solutions) {
          StoreSolution();
        }
      }
      solution_string.append("----------");
      if (model_.objective() != nullptr) {
        const int64 best = objective_monitor_->best();
        report->OnOptimizeSolution(p.thread_id, best, solution_string);
        if ((p.num_solutions != 1 &&
             report->NumSolutions() >= p.num_solutions) ||
            (p.all_solutions && p.num_solutions == 1 &&
             report->NumSolutions() >= 1)) {
          break;
        }
      } else {
        report->OnSatSolution(p.thread_id, solution_string);
        if (report->NumSolutions() >= p.num_solutions) {
          break;
        }
      }
    }
  }
  solver_->EndSearch();
  report->OnSearchEnd(p.thread_id, limit != nullptr ? limit->crossed() : false);
  const int64 solve_time = solver_->wall_time() - build_time;
  const int num_solutions = report->NumSolutions();
  bool proven = false;
  bool timeout = false;
  std::string search_status;
  std::string solver_status;
  if (p.thread_id <= 0) {
    if (p.thread_id == 0) {
      // Recompute the breaked variable.
      if (model_.objective() == nullptr) {
        breaked = report->NumSolutions() >= p.num_solutions;
      } else {
        breaked =
            (p.num_solutions != 1 && num_solutions >= p.num_solutions) ||
            (p.all_solutions && p.num_solutions == 1 && num_solutions >= 1);
      }
    }

    if (report->Interrupted() || Interrupt::Interrupted()) {
      search_status = "%% TIMEOUT";
      timeout = true;
    } else if (!breaked && num_solutions == 0 && !report->Interrupted() &&
               !Interrupt::Interrupted()) {
      search_status = "=====UNSATISFIABLE=====";
    } else if (!breaked && !report->Interrupted() &&
               !Interrupt::Interrupted() &&
               (model_.objective() != nullptr || p.all_solutions)) {
      search_status = "==========";
      proven = true;
    }
    solver_status.append(
        StringPrintf("%%%%  total runtime:        %" GG_LL_FORMAT "d ms\n",
                     solve_time + build_time));
    solver_status.append(StringPrintf(
        "%%%%  build time:           %" GG_LL_FORMAT "d ms\n", build_time));
    solver_status.append(StringPrintf(
        "%%%%  solve time:           %" GG_LL_FORMAT "d ms\n", solve_time));
    solver_status.append(
        StringPrintf("%%%%  solutions:            %d\n", num_solutions));
    solver_status.append(StringPrintf("%%%%  constraints:          %d\n",
                                      solver_->constraints()));
    solver_status.append(StringPrintf(
        "%%%%  normal propagations:  %" GG_LL_FORMAT "d\n",
        solver_->demon_runs(operations_research::Solver::NORMAL_PRIORITY)));
    solver_status.append(StringPrintf(
        "%%%%  delayed propagations: %" GG_LL_FORMAT "d\n",
        solver_->demon_runs(operations_research::Solver::DELAYED_PRIORITY)));
    solver_status.append(
        StringPrintf("%%%%  branches:             %" GG_LL_FORMAT "d\n",
                     solver_->branches()));
    solver_status.append(
        StringPrintf("%%%%  failures:             %" GG_LL_FORMAT "d\n",
                     solver_->failures()));
    solver_status.append(StringPrintf("%%%%  memory:               %s\n",
                                      MemoryUsage().c_str()));
    const int64 best = report->BestSolution();
    if (model_.objective() != nullptr) {
      if (!model_.maximize() && num_solutions > 0) {
        solver_status.append(
            StringPrintf("%%%%  min objective:        %" GG_LL_FORMAT "d%s\n",
                         best, (proven ? " (proven)" : "")));
      } else if (num_solutions > 0) {
        solver_status.append(
            StringPrintf("%%%%  max objective:        %" GG_LL_FORMAT "d%s\n",
                         best, (proven ? " (proven)" : "")));
      }
    }

    if (default_phase_ != nullptr) {
      const std::string default_search_stats =
          DefaultPhaseStatString(default_phase_);
      if (!default_search_stats.empty()) {
        solver_status.append(StringPrintf("%%%%  free search stats:    %s\n",
                                          default_search_stats.c_str()));
      }
    }

    const bool no_solutions = num_solutions == 0;
    const std::string status_string =
        (no_solutions ? (timeout ? "**timeout**" : "**unsat**")
                      : (model_.objective() == nullptr
                             ? "**sat**"
                             : (timeout ? "**feasible**" : "**proven**")));
    const std::string obj_string =
        (model_.objective() != nullptr && !no_solutions ? StrCat(best) : "");
    solver_status.append(
        "%%  name, status, obj, solns, s_time, b_time, br, "
        "fails, cts, demon, delayed, mem, search\n");
    solver_status.append(StringPrintf(
        "%%%%  csv: %s, %s, %s, %d, %" GG_LL_FORMAT "d ms, %" GG_LL_FORMAT
        "d ms, %" GG_LL_FORMAT "d, %" GG_LL_FORMAT "d, %d, %" GG_LL_FORMAT
        "d, %" GG_LL_FORMAT "d, %s, %s",
        model_.name().c_str(), status_string.c_str(), obj_string.c_str(),
        num_solutions, solve_time, build_time, solver_->branches(),
        solver_->failures(), solver_->constraints(),
        solver_->demon_runs(operations_research::Solver::NORMAL_PRIORITY),
        solver_->demon_runs(operations_research::Solver::DELAYED_PRIORITY),
        MemoryUsage().c_str(), search_name_.c_str()));
    report->Print(p.thread_id, search_status);
    if (p.statistics) {
      report->Print(p.thread_id, solver_status);
    }
  }
}
}  // namespace fz
}  // namespace operations_research
