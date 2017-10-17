// Copyright 2010-2017 Google
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

#include "ortools/flatzinc/solver_data.h"

#include "ortools/flatzinc/logging.h"

namespace operations_research {
namespace fz {

IntExpr* SolverData::GetOrCreateExpression(const Argument& arg) {
  switch (arg.type) {
    case Argument::INT_VALUE: {
      return solver_.MakeIntConst(arg.Value());
    }
    case Argument::INT_VAR_REF: {
      return Extract(arg.variables[0]);
    }
    default: {
      LOG(FATAL) << "Cannot extract " << arg.DebugString() << " as a variable";
      return nullptr;
    }
  }
}

std::vector<IntVar*> SolverData::GetOrCreateVariableArray(const Argument& arg) {
  std::vector<IntVar*> result;
  if (arg.type == Argument::INT_VAR_REF_ARRAY) {
    result.resize(arg.variables.size());
    for (int i = 0; i < arg.variables.size(); ++i) {
      result[i] = Extract(arg.variables[i])->Var();
    }
  } else if (arg.type == Argument::INT_LIST) {
    result.resize(arg.values.size());
    for (int i = 0; i < arg.values.size(); ++i) {
      result[i] = solver_.MakeIntConst(arg.values[i]);
    }
  } else if (arg.type == Argument::VOID_ARGUMENT) {
    // Nothing to do.
  } else {
    LOG(FATAL) << "Cannot extract " << arg.DebugString()
               << " as a variable array";
  }
  return result;
}

IntExpr* SolverData::Extract(IntegerVariable* var) {
  IntExpr* result = FindPtrOrNull(extracted_map_, var);
  if (result != nullptr) {
    return result;
  }
  if (var->domain.HasOneValue()) {
    result = solver_.MakeIntConst(var->domain.values.back());
  } else if (var->domain.IsAllInt64()) {
    result = solver_.MakeIntVar(kint32min, kint32max, var->name);
  } else if (var->domain.is_interval) {
    result = solver_.MakeIntVar(std::max<int64>(var->domain.Min(), kint32min),
                                std::min<int64>(var->domain.Max(), kint32max),
                                var->name);
  } else {
    result = solver_.MakeIntVar(var->domain.values, var->name);
  }
  FZVLOG << "Extract " << var->DebugString() << FZENDL;
  FZVLOG << "  - created " << result->DebugString() << FZENDL;
  extracted_map_[var] = result;
  return result;
}

void SolverData::SetExtracted(IntegerVariable* fz_var, IntExpr* expr) {
  CHECK(!ContainsKey(extracted_map_, fz_var));
  if (!expr->IsVar() && !fz_var->domain.is_interval) {
    FZVLOG << "  - lift to var" << FZENDL;
    expr = expr->Var();
  }
  extracted_map_[fz_var] = expr;
}

void SolverData::StoreAllDifferent(std::vector<IntegerVariable*> diffs) {
  if (!diffs.empty()) {
    std::sort(diffs.begin(), diffs.end());
    FZVLOG << "Store AllDifferent info for [" << JoinDebugStringPtr(diffs, ", ")
           << "]" << FZENDL;
    alldiffs_.insert(diffs);
  }
}

bool SolverData::IsAllDifferent(std::vector<IntegerVariable*> diffs) const {
  std::sort(diffs.begin(), diffs.end());
  return ContainsKey(alldiffs_, diffs);
}

void SolverData::CreateSatPropagatorAndAddToSolver() {
  CHECK(sat_ == nullptr);
  sat_ = MakeSatPropagator(&solver_);
  solver_.AddConstraint(
      reinterpret_cast<operations_research::Constraint*>(sat_));
}
}  // namespace fz
}  // namespace operations_research
