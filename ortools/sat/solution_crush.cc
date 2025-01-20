// Copyright 2010-2025 Google LLC
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

#include "ortools/sat/solution_crush.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void SolutionCrush::SetVarToLinearExpression(
    int new_var, absl::Span<const std::pair<int, int64_t>> linear) {
  // We only fill the hint of the new variable if all the variable involved
  // in its definition have a value.
  if (hint_is_loaded_) {
    int64_t new_value = 0;
    for (const auto [var, coeff] : linear) {
      CHECK_GE(var, 0);
      CHECK_LE(var, hint_.size());
      if (!hint_has_value_[var]) return;
      new_value += coeff * hint_[var];
    }
    hint_has_value_[new_var] = true;
    hint_[new_var] = new_value;
  }
}

void SolutionCrush::SetVarToClause(int new_var, absl::Span<const int> clause) {
  if (hint_is_loaded_) {
    int new_hint = 0;
    bool all_have_hint = true;
    for (const int literal : clause) {
      const int var = PositiveRef(literal);
      if (!hint_has_value_[var]) {
        all_have_hint = false;
        break;
      }
      if (hint_[var] == (RefIsPositive(literal) ? 1 : 0)) {
        new_hint = 1;
        break;
      }
    }
    // Leave the `new_var` hint unassigned if any literal is not hinted.
    if (all_have_hint) {
      hint_has_value_[new_var] = true;
      hint_[new_var] = new_hint;
    }
  }
}

void SolutionCrush::SetVarToConjunction(int new_var,
                                        absl::Span<const int> conjunction) {
  if (hint_is_loaded_) {
    int new_hint = 1;
    bool all_have_hint = true;
    for (const int literal : conjunction) {
      const int var = PositiveRef(literal);
      if (!hint_has_value_[var]) {
        all_have_hint = false;
        break;
      }
      if (hint_[var] == (RefIsPositive(literal) ? 0 : 1)) {
        new_hint = 0;
        break;
      }
    }
    // Leave the `new_var` hint unassigned if any literal is not hinted.
    if (all_have_hint) {
      hint_has_value_[new_var] = true;
      hint_[new_var] = new_hint;
    }
  }
}

void SolutionCrush::UpdateVarToDomain(int var, const Domain& domain) {
  if (VarHasSolutionHint(var)) {
    UpdateVarSolutionHint(var, domain.ClosestValue(SolutionHint(var)));
  }

#ifdef CHECK_HINT
  if (model_has_hint_ && HintIsLoaded() &&
      !domains_[var].Contains(hint_[var])) {
    LOG(FATAL) << "Hint with value " << hint_[var]
               << " infeasible when changing domain of " << var << " to "
               << domains_[var];
  }
#endif
}

void SolutionCrush::PermuteHintValues(const SparsePermutation& perm) {
  CHECK(hint_is_loaded_);
  perm.ApplyToDenseCollection(hint_);
  perm.ApplyToDenseCollection(hint_has_value_);
}

bool SolutionCrush::MaybeSetVarToAffineEquationSolution(int var_x, int var_y,
                                                        int64_t coeff,
                                                        int64_t offset) {
  if (hint_is_loaded_) {
    if (!hint_has_value_[var_y] && hint_has_value_[var_x]) {
      hint_has_value_[var_y] = true;
      hint_[var_y] = (hint_[var_x] - offset) / coeff;
      if (hint_[var_y] * coeff + offset != hint_[var_x]) {
        // TODO(user): Do we implement a rounding to closest instead of
        // routing towards 0.
        return false;
      }
    }
  }

#ifdef CHECK_HINT
  const int64_t vx = hint_[var_x];
  const int64_t vy = hint_[var_y];
  if (model_has_hint_ && HintIsLoaded() && vx != vy * coeff + offset) {
    LOG(FATAL) << "Affine relation incompatible with hint: " << vx
               << " != " << vy << " * " << coeff << " + " << offset;
  }
#endif
  return true;
}

void SolutionCrush::Resize(int new_size) {
  hint_.resize(new_size, 0);
  hint_has_value_.resize(new_size, false);
}

void SolutionCrush::LoadSolution(
    const absl::flat_hash_map<int, int64_t>& solution) {
  CHECK(!hint_is_loaded_);
  model_has_hint_ = !solution.empty();
  hint_is_loaded_ = true;
  for (const auto [var, value] : solution) {
    hint_has_value_[var] = true;
    hint_[var] = value;
  }
}

void SolutionCrush::MaybeSetLiteralToValueEncoding(int literal, int var,
                                                   int64_t value) {
  if (hint_is_loaded_) {
    const int bool_var = PositiveRef(literal);
    DCHECK(RefIsPositive(var));
    if (!hint_has_value_[bool_var] && hint_has_value_[var]) {
      const int64_t bool_value = hint_[var] == value ? 1 : 0;
      hint_has_value_[bool_var] = true;
      hint_[bool_var] = RefIsPositive(literal) ? bool_value : 1 - bool_value;
    }
  }
}

void SolutionCrush::SetVarToReifiedPrecedenceLiteral(
    int var, const LinearExpressionProto& time_i,
    const LinearExpressionProto& time_j, int active_i, int active_j) {
  // Take care of hints.
  if (hint_is_loaded_) {
    std::optional<int64_t> time_i_hint = GetExpressionSolutionHint(time_i);
    std::optional<int64_t> time_j_hint = GetExpressionSolutionHint(time_j);
    std::optional<int64_t> active_i_hint = GetRefSolutionHint(active_i);
    std::optional<int64_t> active_j_hint = GetRefSolutionHint(active_j);
    if (time_i_hint.has_value() && time_j_hint.has_value() &&
        active_i_hint.has_value() && active_j_hint.has_value()) {
      const bool reified_hint = (active_i_hint.value() != 0) &&
                                (active_j_hint.value() != 0) &&
                                (time_i_hint.value() <= time_j_hint.value());
      SetNewVariableHint(var, reified_hint);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
