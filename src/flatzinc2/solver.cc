// Copyright 2010-2013 Google
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
#include "flatzinc2/model.h"
#include "flatzinc2/sat_constraint.h"
#include "flatzinc2/solver.h"
#include "constraint_solver/constraint_solver.h"
#include "util/string_array.h"

DECLARE_bool(logging);
DECLARE_bool(verbose_logging);
DEFINE_bool(use_sat, true, "Use a sat solver for propagating on booleans.");

namespace operations_research {
IntExpr* FzSolver::GetExpression(const FzArgument& arg) {
  switch (arg.type) {
    case FzArgument::INT_VALUE: { return solver_.MakeIntConst(arg.Value()); }
    case FzArgument::INT_VAR_REF: { return Extract(arg.variables[0]); }
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
  } else {
    LOG(FATAL) << "Cannot extract " << arg.DebugString()
               << " as a variable array";
  }
  return result;
}

IntExpr* FzSolver::Extract(FzIntegerVariable* var) {
  IntExpr* result = FindPtrOrNull(extrated_map_, var);
  if (result != nullptr) {
    return result;
  }
  if (var->domain.values.size() == 1) {
    result = solver_.MakeIntConst(var->domain.values.back());
  } else if (var->domain.is_interval && var->domain.values.empty()) {
    result = solver_.MakeIntVar(kint32min, kint32max, var->name);
  } else if (var->domain.is_interval) {
    result = solver_.MakeIntVar(var->domain.values[0], var->domain.values[1],
                                var->name);
  } else {
    result = solver_.MakeIntVar(var->domain.values, var->name);
  }
  FZVLOG << "Extract " << var->DebugString() << " into "
         << result->DebugString() << FZENDL;
  extrated_map_[var] = result;
  return result;
}

void FzSolver::SetExtracted(FzIntegerVariable* var, IntExpr* expr) {
  CHECK(!ContainsKey(extrated_map_, var));
  extrated_map_[var] = expr;
}

// The format is fixed in the flatzinc specification.
std::string FzSolver::SolutionString(const FzOnSolutionOutput& output) {
  if (output.variable != nullptr) {
    return StringPrintf("%s = %" GG_LL_FORMAT "d;", output.name.c_str(),
                        Extract(output.variable)->Var()->Value());
  } else {
    const int bound_size = output.bounds.size();
    std::string result =
        StringPrintf("%s = array%dd(", output.name.c_str(), bound_size);
    for (int i = 0; i < bound_size; ++i) {
      result.append(
          StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d, ",
                       output.bounds[i].min_value, output.bounds[i].max_value));
    }
    result.append("[");
    for (int i = 0; i < output.flat_variables.size(); ++i) {
      result.append(
          StringPrintf("%" GG_LL_FORMAT "d",
                       Extract(output.flat_variables[i])->Var()->Value()));
      if (i != output.flat_variables.size() - 1) {
        result.append(", ");
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
};

int ComputeWeight(const ConstraintWithIo& ctio) {
  if (ctio.required.empty()) return 0;
  if (ctio.ct->target_variable != nullptr) return 1;
  return 2;
}

// Comparator to sort constraints based on numbers of required
// elements and index. Reverse sorting to put elements to remove at the end.
struct ConstraintWithIoComparator {
  bool operator()(ConstraintWithIo* a, ConstraintWithIo* b) const {
    const int a_weight = ComputeWeight(*a);
    const int b_weight = ComputeWeight(*b);
    return a_weight > b_weight ||
           (a_weight == b_weight && a_weight != 1 && a->index > b->index) ||
           (a_weight == b_weight && a_weight == 1 &&
            ContainsKey(a->required, b->ct->target_variable));
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
  hash_set<FzIntegerVariable*> defined_variables;
  for (FzIntegerVariable* const var : model_.variables()) {
    if (var->defining_constraint == nullptr && var->active) {
      Extract(var);
    } else {
      FZVLOG << "Skip " << var->DebugString() << FZENDL;
      if (var->defining_constraint != nullptr) {
        FZVLOG << "  - defined by " << var->defining_constraint->DebugString()
               << FZENDL;
      }
      defined_variables.insert(var);
    }
  }
  // Parse model to store info.
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
  hash_map<const FzIntegerVariable*, std::vector<ConstraintWithIo*>>
      dependencies;
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
  // Topological sort.
  while (!to_sort.empty()) {
    if (!to_sort.back()->required.empty()) {
      // Sort again.
      std::sort(to_sort.begin(), to_sort.end(), ConstraintWithIoComparator());
    }
    ConstraintWithIo* const ctio = to_sort.back();
    to_sort.pop_back();
    FZVLOG << "Pop " << ctio->ct->DebugString() << FZENDL;
    CHECK(ctio->required.empty());
    // TODO(user): Implement recovery mode.
    sorted.push_back(ctio->ct);
    FzIntegerVariable* const var = ctio->ct->target_variable;
    if (var != nullptr && ContainsKey(dependencies, var)) {
      for (ConstraintWithIo* const to_clean : dependencies[var]) {
        to_clean->required.erase(var);
      }
    }
    delete ctio;
  }
  for (FzConstraint* const ct : sorted) {
    ExtractConstraint(ct);
  }
  // Add domain constraints to created extressions.
  for (FzIntegerVariable* const var : model_.variables()) {
    if (var->defining_constraint != nullptr && var->active) {
      const FzDomain& domain = var->domain;
      IntExpr* const expr = Extract(var);
      if (expr->IsVar() && domain.is_interval && !domain.values.empty() &&
          (expr->Min() < domain.values[0] || expr->Max() > domain.values[1])) {
        FZVLOG << "Reduce variable domain of " << expr->DebugString()
               << " from " << domain.DebugString() << FZENDL;
        expr->Var()->SetRange(domain.values[0], domain.values[1]);
      } else if (expr->IsVar() && !domain.is_interval) {
        FZVLOG << "Reduce variable domain of " << expr->DebugString()
               << " from " << domain.DebugString() << FZENDL;
        expr->Var()->SetValues(domain.values);
      } else if (domain.is_interval && !domain.values.empty() &&
                 (expr->Min() < domain.values[0] ||
                  expr->Max() > domain.values[1])) {
        FZVLOG << "Add domain constraint " << domain.DebugString() << " onto "
               << expr->DebugString() << FZENDL;
        solver_.AddConstraint(solver_.MakeBetweenCt(
            expr->Var(), domain.values[0], domain.values[1]));
      } else if (!domain.is_interval) {
        FZVLOG << "Add domain constraint " << domain.DebugString() << " onto "
               << expr->DebugString() << FZENDL;
        solver_.AddConstraint(solver_.MakeMemberCt(expr->Var(), domain.values));
      }
    }
  }

  return true;
}

// ----- Alldiff info support -----

void FzSolver::StoreAllDifferent(const std::vector<FzIntegerVariable*>& diffs) {
  std::vector<FzIntegerVariable*> local(diffs);
  std::sort(local.begin(), local.end());
  FZVLOG << "Store AllDifferent info for [" << JoinDebugStringPtr(diffs, ", ")
         << "]" << FZENDL;
  alldiffs_[local.front()].push_back(local);
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

bool FzSolver::IsAllDifferent(
    const std::vector<FzIntegerVariable*>& diffs) const {
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
