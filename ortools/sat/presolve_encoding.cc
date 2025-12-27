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

#include "ortools/sat/presolve_encoding.h"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

bool MaybeTransferLinear1ToAnotherVariable(
    VariableEncodingLocalModel& local_model, PresolveContext* context) {
  if (local_model.var == -1) return true;
  if (local_model.variable_coeff_in_objective != 0) return true;
  if (local_model.single_constraint_using_the_var_outside_the_local_model ==
      -1) {
    return true;
  }
  const int other_c =
      local_model.single_constraint_using_the_var_outside_the_local_model;

  const std::vector<int>& to_rewrite = local_model.linear1_constraints;

  // In general constraint with more than two variable can't be removed.
  // Similarly for linear2 with non-fixed rhs as we would need to check the form
  // of all implied domain.
  const auto& other_ct = context->working_model->constraints(other_c);
  if (context->ConstraintToVars(other_c).size() != 2 ||
      !other_ct.enforcement_literal().empty() ||
      other_ct.constraint_case() == ConstraintProto::kLinear) {
    return true;
  }

  // This will be the rewriting function. It takes the implied domain of var
  // from linear1, and return a pair {new_var, new_var_implied_domain}.
  std::function<std::pair<int, Domain>(const Domain& implied)> transfer_f =
      nullptr;

  const int var = local_model.var;
  // We only support a few cases.
  //
  // TODO(user): implement more! Note that the linear2 case was tempting, but if
  // we don't have an equality, we can't transfer, and if we do, we actually
  // have affine equivalence already.
  if (other_ct.constraint_case() == ConstraintProto::kLinMax &&
      other_ct.lin_max().target().vars().size() == 1 &&
      other_ct.lin_max().target().vars(0) == var &&
      std::abs(other_ct.lin_max().target().coeffs(0)) == 1 &&
      IsAffineIntAbs(other_ct)) {
    context->UpdateRuleStats("linear1: transferred from abs(X) to X");
    const LinearExpressionProto& target = other_ct.lin_max().target();
    const LinearExpressionProto& expr = other_ct.lin_max().exprs(0);
    transfer_f = [target = target, expr = expr](const Domain& implied) {
      Domain target_domain =
          implied.ContinuousMultiplicationBy(target.coeffs(0))
              .AdditionWith(Domain(target.offset()));
      target_domain = target_domain.IntersectionWith(
          Domain(0, std::numeric_limits<int64_t>::max()));

      // We have target = abs(expr).
      const Domain expr_domain =
          target_domain.UnionWith(target_domain.Negation());
      const Domain new_domain = expr_domain.AdditionWith(Domain(-expr.offset()))
                                    .InverseMultiplicationBy(expr.coeffs(0));
      return std::make_pair(expr.vars(0), new_domain);
    };
  }

  if (transfer_f == nullptr) {
    context->UpdateRuleStats(
        "TODO linear1: appear in only one extra 2-var constraint");
    return true;
  }

  // Applies transfer_f to all linear1.
  const Domain var_domain = context->DomainOf(var);
  for (const int c : to_rewrite) {
    ConstraintProto* ct = context->working_model->mutable_constraints(c);
    if (ct->linear().vars(0) != var || ct->linear().coeffs(0) != 1) {
      // This shouldn't happen.
      LOG(INFO) << "Aborted in MaybeTransferLinear1ToAnotherVariable()";
      return true;
    }

    const Domain implied =
        var_domain.IntersectionWith(ReadDomainFromProto(ct->linear()));
    auto [new_var, new_domain] = transfer_f(implied);
    const Domain current = context->DomainOf(new_var);
    new_domain = new_domain.IntersectionWith(current);
    if (new_domain.IsEmpty()) {
      if (!context->MarkConstraintAsFalse(ct, "linear1: unsat transfer")) {
        return false;
      }
    } else if (new_domain == current) {
      // Note that we don't need to remove this constraint from
      // local_model.linear1_constraints since we will set
      // local_model.var = -1 below.
      ct->Clear();
    } else {
      ct->mutable_linear()->set_vars(0, new_var);
      FillDomainInProto(new_domain, ct->mutable_linear());
    }
    context->UpdateConstraintVariableUsage(c);
  }

  // Copy other_ct to the mapping model and delete var!
  context->NewMappingConstraint(other_ct, __FILE__, __LINE__);
  context->working_model->mutable_constraints(other_c)->Clear();
  context->UpdateConstraintVariableUsage(other_c);
  context->MarkVariableAsRemoved(var);
  local_model.var = -1;
  return true;
}

}  // namespace sat
}  // namespace operations_research
