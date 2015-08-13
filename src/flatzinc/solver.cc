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

#include <string>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/hash.h"
#include "base/map_util.h"
#include "flatzinc/model.h"
#include "flatzinc/sat_constraint.h"
#include "flatzinc/solver.h"
#include "constraint_solver/constraint_solver.h"
#include "util/string_array.h"

DECLARE_bool(fz_logging);
DECLARE_bool(fz_verbose);
DECLARE_bool(fz_debug);
DEFINE_bool(use_sat, true, "Use a sat solver for propagating on booleans.");

namespace operations_research {
IntExpr* FzSolver::GetExpression(const FzArgument& arg) {
  switch (arg.type) {
    case FzArgument::INT_VALUE: {
      return solver_.MakeIntConst(arg.Value());
    }
    case FzArgument::INT_VAR_REF: {
      return Extract(arg.variables[0]);
    }
    default: {
      LOG(FATAL) << "Cannot extract " << arg.DebugString() << " as a variable";
      return nullptr;
    }
  }
}

std::vector<IntVar*> FzSolver::GetVariableArray(const FzArgument& arg) {
  std::vector<IntVar*> result;
  if (arg.type == FzArgument::INT_VAR_REF_ARRAY) {
    result.resize(arg.variables.size());
    for (int i = 0; i < arg.variables.size(); ++i) {
      result[i] = Extract(arg.variables[i])->Var();
    }
  } else if (arg.type == FzArgument::INT_LIST) {
    result.resize(arg.values.size());
    for (int i = 0; i < arg.values.size(); ++i) {
      result[i] = solver_.MakeIntConst(arg.values[i]);
    }
  } else if (arg.type == FzArgument::VOID_ARGUMENT) {
    // Nothing to do.
  } else {
    LOG(FATAL) << "Cannot extract " << arg.DebugString()
               << " as a variable array";
  }
  return result;
}

IntExpr* FzSolver::Extract(FzIntegerVariable* var) {
  IntExpr* result = FindPtrOrNull(extracted_map_, var);
  if (result != nullptr) {
    return result;
  }
  if (var->domain.IsSingleton()) {
    result = solver_.MakeIntConst(var->domain.values.back());
  } else if (var->IsAllInt64()) {
    result = solver_.MakeIntVar(kint32min, kint32max, var->name);
  } else if (var->domain.is_interval) {
    result =
        solver_.MakeIntVar(std::max<int64>(var->Min(), kint32min),
                           std::min<int64>(var->Max(), kint32max), var->name);
  } else {
    result = solver_.MakeIntVar(var->domain.values, var->name);
  }
  FZVLOG << "Extract " << var->DebugString() << FZENDL;
  FZVLOG << "  - created " << result->DebugString() << FZENDL;
  extracted_map_[var] = result;
  return result;
}

void FzSolver::SetExtracted(FzIntegerVariable* fz_var, IntExpr* expr) {
  CHECK(!ContainsKey(extracted_map_, fz_var));
  if (!expr->IsVar() && !fz_var->domain.is_interval) {
    FZVLOG << "  - lift to var" << FZENDL;
    expr = expr->Var();
  }
  extracted_map_[fz_var] = expr;
}

int64 FzSolver::SolutionValue(FzIntegerVariable* var) {
  IntExpr* const result = FindPtrOrNull(extracted_map_, var);
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
    CHECK(var->domain.IsSingleton());
    return var->domain.values[0];
  }
}

// The format is fixed in the flatzinc specification.
std::string FzSolver::SolutionString(const FzOnSolutionOutput& output, bool store) {
  if (output.variable != nullptr) {
    const int64 value = SolutionValue(output.variable);
    if (store) {
      stored_values_.back()[output.variable] = value;
    }
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
      result.append(StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d, ",
                                 output.bounds[i].min_value,
                                 output.bounds[i].max_value));
    }
    result.append("[");
    for (int i = 0; i < output.flat_variables.size(); ++i) {
      const int64 value = SolutionValue(output.flat_variables[i]);
      FzIntegerVariable* const var = output.flat_variables[i];
      if (output.display_as_boolean) {
        result.append(StringPrintf(value ? "true" : "false"));
      } else {
        result.append(StringPrintf("%" GG_LL_FORMAT "d", value));
      }
      if (i != output.flat_variables.size() - 1) {
        result.append(", ");
      }
      if (store) {
        stored_values_.back()[var] = value;
      }
    }
    result.append("]);");
    return result;
  }
  return "";
}

namespace {
struct ConstraintWithIo {
  FzConstraint* ct;
  int index;
  hash_set<FzIntegerVariable*> required;

  ConstraintWithIo(FzConstraint* cte, int i,
                   const hash_set<FzIntegerVariable*>& defined)
      : ct(cte), index(i) {
    // Collect required variables.
    for (const FzArgument& arg : ct->arguments) {
      for (FzIntegerVariable* const var : arg.variables) {
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

int ComputeWeight(const ConstraintWithIo& ctio) {
  return ctio.required.size() * 2 + (ctio.ct->target_variable == nullptr);
}

// Comparator to sort constraints based on numbers of required
// elements and index. Reverse sorting to put elements to remove at the end.
struct ConstraintWithIoComparator {
  bool operator()(ConstraintWithIo* a, ConstraintWithIo* b) const {
    const int a_weight = ComputeWeight(*a);
    const int b_weight = ComputeWeight(*b);
    return a_weight > b_weight || (a_weight == b_weight && a->index > b->index);
  }
};
}  // namespace

bool FzSolver::Extract() {
  // Create the sat solver.
  if (FLAGS_use_sat) {
    FZLOG << "  - Use sat" << FZENDL;
    sat_ = MakeSatPropagator(&solver_);
    solver_.AddConstraint(reinterpret_cast<Constraint*>(sat_));
  } else {
    sat_ = nullptr;
  }
  // Build statistics.
  statistics_.BuildStatistics();
  // Extract variables.
  FZLOG << "Extract variables" << FZENDL;
  int extracted_variables = 0;
  int extracted_constants = 0;
  int skipped_variables = 0;
  hash_set<FzIntegerVariable*> defined_variables;
  for (FzIntegerVariable* const var : model_.variables()) {
    if (var->defining_constraint == nullptr && var->active) {
      Extract(var);
      if (var->domain.IsSingleton()) {
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
  // Parse model to store info.
  FZLOG << "Extract constraints" << FZENDL;
  for (FzConstraint* const ct : model_.constraints()) {
    if (ct->type == "all_different_int") {
      StoreAllDifferent(ct->Arg(0).variables);
    }
  }
  // Sort constraints such that defined variables are created before the
  // extraction of the constraints that use them.
  int index = 0;
  std::vector<ConstraintWithIo*> to_sort;
  std::vector<FzConstraint*> sorted;
  hash_map<const FzIntegerVariable*, std::vector<ConstraintWithIo*>> dependencies;
  for (FzConstraint* ct : model_.constraints()) {
    if (ct != nullptr && ct->active) {
      ConstraintWithIo* const ctio =
          new ConstraintWithIo(ct, index++, defined_variables);
      to_sort.push_back(ctio);
      for (FzIntegerVariable* const var : ctio->required) {
        dependencies[var].push_back(ctio);
      }
    }
  }
  // Sort a first time.
  std::sort(to_sort.begin(), to_sort.end(), ConstraintWithIoComparator());
  for (ConstraintWithIo* const ctio : to_sort) {
    CHECK(ctio != nullptr);
  }
  // Topological sort.
  while (!to_sort.empty()) {
    if (!to_sort.back()->required.empty()) {
      // Sort again.
      std::sort(to_sort.begin(), to_sort.end(), ConstraintWithIoComparator());
    }
    ConstraintWithIo* const ctio = to_sort.back();
    if (!ctio->required.empty()) {
      // Recovery. We pick the last constraint (min number of required variable)
      // And we clean all of them (mark as non target).
      std::vector<FzIntegerVariable*> required_vars(ctio->required.begin(),
                                               ctio->required.end());
      for (FzIntegerVariable* const fz_var : required_vars) {
        FZDLOG << "  - clean " << fz_var->DebugString() << FZENDL;
        if (fz_var->defining_constraint != nullptr) {
          fz_var->defining_constraint->target_variable = nullptr;
          fz_var->defining_constraint = nullptr;
        }
        for (ConstraintWithIo* const to_clean : dependencies[fz_var]) {
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
    FzIntegerVariable* const var = ctio->ct->target_variable;
    if (var != nullptr && ContainsKey(dependencies, var)) {
      FZDLOG << "  - clean " << var->DebugString() << FZENDL;
      for (ConstraintWithIo* const to_clean : dependencies[var]) {
        to_clean->required.erase(var);
      }
    }
    delete ctio;
  }
  for (FzConstraint* const ct : sorted) {
    ExtractConstraint(ct);
  }
  FZLOG << "  - " << sorted.size() << " constraints parsed" << FZENDL;
  const int num_cp_constraints = solver_.constraints();
  if (num_cp_constraints <= 1) {
    FZLOG << "  - " << num_cp_constraints
          << " constraint added to the CP solver" << FZENDL;
  } else {
    FZLOG << "  - " << num_cp_constraints
          << " constraints added to the CP solver" << FZENDL;
  }
  const int num_sat_constraints = FLAGS_use_sat ? NumSatConstraints(sat_) : 0;
  if (num_sat_constraints > 0) {
    FZLOG << "  - " << num_sat_constraints << " constraints added to SAT solver"
          << FZENDL;
  }

  // Add domain constraints to created expressions.
  int domain_constraints = 0;
  for (FzIntegerVariable* const var : model_.variables()) {
    if (var->defining_constraint != nullptr && var->active) {
      const FzDomain& domain = var->domain;
      if (!domain.is_interval && domain.values.size() == 2 &&
          domain.values[0] == 0 && domain.values[1] == 1) {
        // Canonicalize domains: {0, 1} -> [0 ,, 1]
        var->domain.is_interval = true;
      }
      IntExpr* const expr = Extract(var);
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
        solver_.AddConstraint(solver_.MakeBetweenCt(
            expr->Var(), domain.values[0], domain.values[1]));
        domain_constraints++;
      } else if (!domain.is_interval) {
        FZVLOG << "Add domain constraint " << domain.DebugString() << " onto "
               << expr->DebugString() << FZENDL;
        solver_.AddConstraint(solver_.MakeMemberCt(expr->Var(), domain.values));
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

// ----- Alldiff info support -----

void FzSolver::StoreAllDifferent(const std::vector<FzIntegerVariable*>& diffs) {
  if (!diffs.empty()) {
    std::vector<FzIntegerVariable*> local(diffs);
    std::sort(local.begin(), local.end());
    FZVLOG << "Store AllDifferent info for [" << JoinDebugStringPtr(diffs, ", ")
           << "]" << FZENDL;
    alldiffs_[local.front()].push_back(local);
  }
}

namespace {
template <class T>
bool EqualVector(const std::vector<T>& v1, const std::vector<T>& v2) {
  if (v1.size() != v2.size()) return false;
  for (int i = 0; i < v1.size(); ++i) {
    if (v1[i] != v2[i]) return false;
  }
  return true;
}
}  // namespace

bool FzSolver::IsAllDifferent(const std::vector<FzIntegerVariable*>& diffs) const {
  std::vector<FzIntegerVariable*> local(diffs);
  std::sort(local.begin(), local.end());
  const FzIntegerVariable* const start = local.front();
  if (!ContainsKey(alldiffs_, start)) return false;
  const std::vector<std::vector<FzIntegerVariable*>>& stored =
      FindOrDie(alldiffs_, start);
  for (const std::vector<FzIntegerVariable*>& one_diff : stored) {
    if (EqualVector(local, one_diff)) {
      return true;
    }
  }
  return false;
}
}  // namespace operations_research
