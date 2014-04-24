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
#include "flatzinc2/solver.h"
#include "constraint_solver/constraint_solver.h"
#include "util/string_array.h"

namespace operations_research {
IntExpr* FzSolver::GetExpression(const FzArgument& arg) {
  switch (arg.type) {
    case FzArgument::INT_VALUE: {
      return solver_.MakeIntConst(arg.integer_value);
    }
    case FzArgument::INT_VAR_REF: { return Extract(arg.variable); }
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
  } else {
    LOG(FATAL) << "Cannot extract " << arg.DebugString()
               << " as a variable array";
  }
  return result;
}

IntExpr* FzSolver::Extract(FzIntegerVariable* const var) {
  IntExpr* result = FindPtrOrNull(extrated_map_, var);
  if (result != nullptr) {
    return result;
  }
  if (var->domain.values.size() == 1) {
    result = solver_.MakeIntConst(var->domain.values.back());
  } else if (var->domain.is_interval) {
    result = solver_.MakeIntVar(var->domain.values[0], var->domain.values[1],
                                var->name);
  } else {
    result = solver_.MakeIntVar(var->domain.values, var->name);
  }
  extrated_map_[var] = result;
  return nullptr;
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
                   const hash_set<FzIntegerVariable*> & defined)
      : ct(cte), index(i) {
    hash_set<FzIntegerVariable*> marked;
    for (const FzArgument& arg : ct->arguments) {
      if (arg.variable != nullptr && ContainsKey(defined, arg.variable)) {
        required.insert(arg.variable);
      }
      for (FzIntegerVariable* const var : arg.variables) {
        if (ContainsKey(defined, var)) {
          required.insert(var);
        }
      }
    }
    if (ct->target_variable != nullptr) {
      required.erase(ct->target_variable);
    }
  }
};

struct ConstraintWithIoComparator {
  bool operator()(const ConstraintWithIo& a, const ConstraintWithIo& b) const {
    if (a.ct->target_variable != nullptr &&
        ContainsKey(b.required, a.ct->target_variable)) {
      return true;
    }
    if (b.ct->target_variable != nullptr &&
        ContainsKey(a.required, b.ct->target_variable)) {
      return false;
    }
    return a.index < b.index;
  }
};
}  // namespace

bool FzSolver::Extract() {
  statistics_.BuildStatistics();
  hash_set<FzIntegerVariable*> defined_variables;
  for (FzIntegerVariable* const var : model_.variables()) {
    if (var->defining_constraint == nullptr) {
      Extract(var);
    } else {
      defined_variables.insert(var);
    }
  }
  int index = 0;
  std::vector<ConstraintWithIo> to_sort;
  for (FzConstraint* ct : model_.constraints()) {
    if (ct != nullptr && !ct->is_trivially_true) {
      to_sort.push_back(ConstraintWithIo(ct, index++, defined_variables));
    }
  }
  std::sort(to_sort.begin(), to_sort.end(), ConstraintWithIoComparator());
  for (const ConstraintWithIo& ctio : to_sort) {
    ExtractConstraint(ctio.ct);
  }
  return true;
}
}  // namespace operations_research
