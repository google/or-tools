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

#include "ortools/sat/cp_model_expand.h"

#include <map>

#include <unordered_map>
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {
namespace {

struct ExpansionHelper {
  CpModelProto expanded_proto;
  std::unordered_map<std::pair<int, int>, int> precedence_cache;
  std::map<std::string, int> statistics;

  // a => b.
  void AddImplication(int a, int b) {
    ConstraintProto* const ct = expanded_proto.add_constraints();
    ct->add_enforcement_literal(a);
    ct->mutable_bool_and()->add_literals(b);
  }

  // b => x in [lb, ub].
  void AddImplyInDomain(int b, int x, int64 lb, int64 ub) {
    ConstraintProto* const imply = expanded_proto.add_constraints();
    imply->add_enforcement_literal(b);
    imply->mutable_linear()->add_vars(x);
    imply->mutable_linear()->add_coeffs(1);
    imply->mutable_linear()->add_domain(lb);
    imply->mutable_linear()->add_domain(ub);
  }

  int AddBoolVar() {
    IntegerVariableProto* const var = expanded_proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
    return expanded_proto.variables_size() - 1;
  }

  int VariableIsOptional(int index) const {
    return expanded_proto.variables(index).enforcement_literal_size() > 0;
  }

  int VariableEnforcementLiteral(int index) const {
    DCHECK(VariableIsOptional(index));
    return expanded_proto.variables(index).enforcement_literal(0);
  }

  // lesseq_0 <=> (x <= 0 && x performed).
  void AddReifiedLesssOrEqualThanZero(int lesseq_0, int x) {
    AddImplyInDomain(lesseq_0, x, kint64min, 0);
    AddImplyInDomain(NegatedRef(lesseq_0), x, 1, kint64max);
    if (VariableIsOptional(x)) {
      AddImplication(lesseq_0, VariableEnforcementLiteral(x));
    }
  }

  // x_lesseq_y <=> (x <= y && x enforced && y enforced).
  void AddReifiedPrecedence(int x_lesseq_y, int x, int y) {
    // x_lesseq_y => x enforced && y enforced
    if (VariableIsOptional(x)) {
      AddImplication(x_lesseq_y, VariableEnforcementLiteral(x));
    }
    if (VariableIsOptional(y)) {
      AddImplication(x_lesseq_y, VariableEnforcementLiteral(y));
    }

    // x_lesseq_y => (x <= y)
    ConstraintProto* const lesseq = expanded_proto.add_constraints();
    lesseq->add_enforcement_literal(x_lesseq_y);
    lesseq->mutable_linear()->add_vars(x);
    lesseq->mutable_linear()->add_vars(y);
    lesseq->mutable_linear()->add_coeffs(-1);
    lesseq->mutable_linear()->add_coeffs(1);
    lesseq->mutable_linear()->add_domain(0);
    lesseq->mutable_linear()->add_domain(kint64max);

    // Not(x_lesseq_y) => (x > y)
    ConstraintProto* const greater = expanded_proto.add_constraints();
    greater->add_enforcement_literal(NegatedRef(x_lesseq_y));
    greater->mutable_linear()->add_vars(x);
    greater->mutable_linear()->add_vars(y);
    greater->mutable_linear()->add_coeffs(-1);
    greater->mutable_linear()->add_coeffs(1);
    greater->mutable_linear()->add_domain(kint64min);
    greater->mutable_linear()->add_domain(-1);
  }
};

void ExpandReservoir(ConstraintProto* ct, ExpansionHelper* helper) {
  const ReservoirConstraintProto& reservoir = ct->reservoir();
  const int num_variables = reservoir.times_size();
  CpModelProto& expanded_proto = helper->expanded_proto;

  int num_positives = 0;
  int num_negatives = 0;
  for (const int64 demand : reservoir.demands()) {
    if (demand > 0) {
      num_positives++;
    } else if (demand < 0) {
      num_negatives++;
    }
  }

  if (num_positives > 0 && num_negatives > 0) {
    // Creates Boolean variables equivalent to (start[i] <= start[j]) i != j
    for (int i = 0; i < num_variables - 1; ++i) {
      const int time_i = reservoir.times(i);
      for (int j = i + 1; j < num_variables; ++j) {
        const int time_j = reservoir.times(j);
        const std::pair<int, int> p = std::make_pair(time_i, time_j);
        const std::pair<int, int> rev_p = std::make_pair(time_j, time_i);
        if (ContainsKey(helper->precedence_cache, p)) continue;

        const int i_lesseq_j = helper->AddBoolVar();
        helper->precedence_cache[p] = i_lesseq_j;
        const int j_lesseq_i = helper->AddBoolVar();
        helper->precedence_cache[rev_p] = j_lesseq_i;
        helper->AddReifiedPrecedence(i_lesseq_j, time_i, time_j);
        helper->AddReifiedPrecedence(j_lesseq_i, time_j, time_i);

        // Consistency. This is redundant but should improves performance.
        auto* const bool_or =
            expanded_proto.add_constraints()->mutable_bool_or();
        bool_or->add_literals(i_lesseq_j);
        bool_or->add_literals(j_lesseq_i);
        if (helper->VariableIsOptional(time_i)) {
          bool_or->add_literals(
              NegatedRef(helper->VariableEnforcementLiteral(time_i)));
        }
        if (helper->VariableIsOptional(time_j)) {
          bool_or->add_literals(
              NegatedRef(helper->VariableEnforcementLiteral(time_j)));
        }
      }
    }

    // Constrains the running level to be consistent at all times.
    // For this we only add a constraint at the time a given demand
    // take place. We also have a constraint for time zero if needed
    // (added below).
    for (int i = 0; i < num_variables; ++i) {
      const int time_i = reservoir.times(i);
      // Accumulates demands of all predecessors.
      ConstraintProto* const level = expanded_proto.add_constraints();
      for (int j = 0; j < num_variables; ++j) {
        if (i == j) continue;
        const int time_j = reservoir.times(j);
        level->mutable_linear()->add_vars(FindOrDieNoPrint(
            helper->precedence_cache, std::make_pair(time_j, time_i)));
        level->mutable_linear()->add_coeffs(reservoir.demands(j));
      }
      // Accounts for own demand.
      const int64 demand_i = reservoir.demands(i);
      level->mutable_linear()->add_domain(
          CapSub(reservoir.min_level(), demand_i));
      level->mutable_linear()->add_domain(
          CapSub(reservoir.max_level(), demand_i));
      if (helper->VariableIsOptional(time_i)) {
        level->add_enforcement_literal(
            helper->VariableEnforcementLiteral(time_i));
      }
    }
  } else {
    // If all demands have the same sign, we do not care about the order, just
    // the sum.
    int64 fixed_demand = 0;
    auto* const sum = expanded_proto.add_constraints()->mutable_linear();
    for (int i = 0; i < num_variables; ++i) {
      const int time = reservoir.times(i);
      const int64 demand = reservoir.demands(i);
      if (demand == 0) continue;
      if (helper->VariableIsOptional(time)) {
        sum->add_vars(helper->VariableEnforcementLiteral(time));
        sum->add_coeffs(demand);
      } else {
        fixed_demand += demand;
      }
    }
    sum->add_domain(CapSub(reservoir.min_level(), fixed_demand));
    sum->add_domain(CapSub(reservoir.max_level(), fixed_demand));
  }

  // Constrains the reservoir level to be consistent at time 0.
  // We need to do it only if 0 is not in [min_level..max_level].
  // Otherwise, the regular propagation will already check it.
  if (reservoir.min_level() > 0 || reservoir.max_level() < 0) {
    auto* const initial_ct = expanded_proto.add_constraints()->mutable_linear();
    for (int i = 0; i < num_variables; ++i) {
      const int time_i = reservoir.times(i);
      const int lesseq_0 = helper->AddBoolVar();
      helper->AddReifiedLesssOrEqualThanZero(lesseq_0, time_i);
      initial_ct->add_vars(lesseq_0);
      initial_ct->add_coeffs(reservoir.demands(i));
    }
    initial_ct->add_domain(reservoir.min_level());
    initial_ct->add_domain(reservoir.max_level());
  }

  ct->Clear();
  helper->statistics["kReservoir"]++;
}

}  // namespace

CpModelProto ExpandCpModel(const CpModelProto& initial_model) {
  ExpansionHelper helper;
  helper.expanded_proto = initial_model;
  const int num_constraints = helper.expanded_proto.constraints_size();
  for (int i = 0; i < num_constraints; ++i) {
    ConstraintProto* const ct = helper.expanded_proto.mutable_constraints(i);
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintCase::kReservoir:
        ExpandReservoir(ct, &helper);
        break;
      default:
        break;
    }
  }

  for (const auto& entry : helper.statistics) {
    if (entry.second == 1) {
      VLOG(1) << "Expanded 1 '" << entry.first << "' constraint.";
    } else {
      VLOG(1) << "Expanded " << entry.second << " '" << entry.first
              << "' constraints";
    }
  }

  return helper.expanded_proto;
}
}  // namespace sat
}  // namespace operations_research
