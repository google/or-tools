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

#include <unordered_map>
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {
namespace {

int Not(int a) { return -a - 1; }

void AddImplication(int a, int b, CpModelProto* model_proto) {
  ConstraintProto* const ct = model_proto->add_constraints();
  ct->add_enforcement_literal(a);
  ct->mutable_bool_or()->add_literals(b);
}

void AddImplyInDomain(int b, int x, int64 lb, int64 ub,
                      CpModelProto* model_proto) {
  ConstraintProto* const imply = model_proto->add_constraints();
  imply->add_enforcement_literal(b);
  imply->mutable_linear()->add_vars(x);
  imply->mutable_linear()->add_coeffs(1);
  imply->mutable_linear()->add_domain(lb);
  imply->mutable_linear()->add_domain(ub);
}

int AddBoolVar(CpModelProto* model_proto) {
  IntegerVariableProto* const var = model_proto->add_variables();
  var->add_domain(0);
  var->add_domain(1);
  return model_proto->variables_size() - 1;
}

bool IsOptional(const IntegerVariableProto& v) {
  return v.enforcement_literal_size() > 0;
}

// b <=> (x <= 0).
void AddVarEqualLessOrEqualZero(int b, int x, CpModelProto* model_proto) {
  if (model_proto->variables(x).enforcement_literal_size() == 0) {
    AddImplyInDomain(b, x, kint64min, 0, model_proto);
    AddImplyInDomain(Not(b), x, 1, kint64max, model_proto);
  } else {
    const int opt_x = model_proto->variables(x).enforcement_literal(0);
    AddImplyInDomain(b, x, kint64min, 0, model_proto);
    AddImplication(b, opt_x, model_proto);
    const int g = AddBoolVar(model_proto);
    AddImplyInDomain(g, x, 1, kint64max, model_proto);
    AddImplication(g, opt_x, model_proto);
    ConstraintProto* const bool_or = model_proto->add_constraints();
    bool_or->mutable_bool_or()->add_literals(b);
    bool_or->mutable_bool_or()->add_literals(g);
    bool_or->mutable_bool_or()->add_literals(Not(opt_x));
  }
}

// x_lesseq_y <=> (x <= y) && x enforced && y enforced.
void AddVarEqualPrecedence(int x_lesseq_y, int x, int y,
                           CpModelProto* model_proto) {
  const IntegerVariableProto& var_x = model_proto->variables(x);
  const IntegerVariableProto& var_y = model_proto->variables(y);
  const bool x_is_optional = IsOptional(var_x);
  const bool y_is_optional = IsOptional(var_y);
  const int x_lit = x_is_optional ? var_x.enforcement_literal(0) : -1;
  const int y_lit = y_is_optional ? var_y.enforcement_literal(0) : -1;

  // x_lesseq_y => (x <= y) && x => enforced && y enforced.
  ConstraintProto* const lesseq = model_proto->add_constraints();
  lesseq->add_enforcement_literal(x_lesseq_y);
  lesseq->mutable_linear()->add_vars(x);
  lesseq->mutable_linear()->add_vars(y);
  lesseq->mutable_linear()->add_coeffs(-1);
  lesseq->mutable_linear()->add_coeffs(1);
  lesseq->mutable_linear()->add_domain(0);
  lesseq->mutable_linear()->add_domain(kint64max);

  if (IsOptional(var_x)) {
    AddImplication(x_lesseq_y, x_lit, model_proto);
  }
  if (IsOptional(var_y)) {
    AddImplication(x_lesseq_y, y_lit, model_proto);
  }

  // x_greater_y => (x > y) && x enforced && y enforced.
  const int x_greater_y = x_is_optional || y_is_optional
                              ? AddBoolVar(model_proto)
                              : Not(x_lesseq_y);

  ConstraintProto* const greater = model_proto->add_constraints();
  greater->add_enforcement_literal(x_greater_y);
  greater->mutable_linear()->add_vars(x);
  greater->mutable_linear()->add_vars(y);
  greater->mutable_linear()->add_coeffs(-1);
  greater->mutable_linear()->add_coeffs(1);
  greater->mutable_linear()->add_domain(kint64min);
  greater->mutable_linear()->add_domain(-1);

  if (IsOptional(var_x)) {
    AddImplication(x_greater_y, x_lit, model_proto);
  }
  if (IsOptional(var_y)) {
    AddImplication(x_greater_y, y_lit, model_proto);
  }

  // Consistency between x_lesseq_y, x_greater_y, x_lit, y_lit.
  ConstraintProto* const bool_or = model_proto->add_constraints();
  bool_or->mutable_bool_or()->add_literals(x_lesseq_y);
  bool_or->mutable_bool_or()->add_literals(x_greater_y);
  if (x_is_optional) {
    AddImplication(Not(x_lit), Not(x_lesseq_y), model_proto);
    AddImplication(Not(x_lit), Not(x_greater_y), model_proto);
    bool_or->mutable_bool_or()->add_literals(Not(x_lit));
  }
  if (y_is_optional) {
    AddImplication(Not(y_lit), Not(x_lesseq_y), model_proto);
    AddImplication(Not(y_lit), Not(x_greater_y), model_proto);
    bool_or->mutable_bool_or()->add_literals(Not(y_lit));
  }

  // TODO(user): Do we add x_lesseq_y => Not(x_greater_y)
  // and x_greater_y => Not(x_lesseq_y)?
}

struct RewriteContext {
  CpModelProto expanded_proto;
  std::unordered_map<std::pair<int, int>, int> precedence_cache;
  std::unordered_map<std::string, int> statistics;
};

void ExpandReservoir(ConstraintProto* ct, RewriteContext* context) {
  const ReservoirConstraintProto& reservoir = ct->reservoir();
  const int num_variables = reservoir.times_size();
  CpModelProto& expanded = context->expanded_proto;

  int num_positives = 0;
  int num_negatives = 0;
  int num_zeros = 0;

  for (const int demand : reservoir.demands()) {
    if (demand > 0) {
      num_positives++;
    } else if (demand < 0) {
      num_negatives++;
    } else {
      num_zeros++;
    }
  }

  if (num_positives > 0 && num_negatives > 0) {
    // Creates boolean variables equivalent to (start[i] <= start[j]) i != j,
    for (int i = 0; i < num_variables - 1; ++i) {
      const int ti = reservoir.times(i);
      for (int j = i + 1; j < num_variables; ++j) {
        const int tj = reservoir.times(j);
        const std::pair<int, int> p = std::make_pair(ti, tj);
        const std::pair<int, int> rev_p = std::make_pair(tj, ti);
        if (ContainsKey(context->precedence_cache, p)) continue;

        const int i_lesseq_j = AddBoolVar(&expanded);
        context->precedence_cache[p] = i_lesseq_j;
        const int j_lesseq_i = AddBoolVar(&expanded);
        context->precedence_cache[rev_p] = j_lesseq_i;
        AddVarEqualPrecedence(i_lesseq_j, ti, tj, &expanded);
        AddVarEqualPrecedence(j_lesseq_i, tj, ti, &expanded);
        // Consistency.
        ConstraintProto* const bool_or = expanded.add_constraints();
        bool_or->mutable_bool_or()->add_literals(i_lesseq_j);
        bool_or->mutable_bool_or()->add_literals(j_lesseq_i);
        const IntegerVariableProto& var_i = expanded.variables(ti);
        if (IsOptional(var_i)) {
          bool_or->mutable_bool_or()->add_literals(
              Not(var_i.enforcement_literal(0)));
        }
        const IntegerVariableProto& var_j = expanded.variables(tj);
        if (IsOptional(var_j)) {
          bool_or->mutable_bool_or()->add_literals(
              Not(var_j.enforcement_literal(0)));
        }
      }
    }

    // Constrains the running level to be consistent at all times.
    for (int i = 0; i < num_variables; ++i) {
      const int ti = reservoir.times(i);
      // Accumulates demands of all predecessors.
      ConstraintProto* const level = expanded.add_constraints();
      for (int j = 0; j < num_variables; ++j) {
        if (i == j) continue;
        const int tj = reservoir.times(j);
        const std::pair<int, int> p = std::make_pair(tj, ti);
        level->mutable_linear()->add_vars(
            FindOrDieNoPrint(context->precedence_cache, p));
        level->mutable_linear()->add_coeffs(reservoir.demands(j));
      }
      // Accounts for own demand.
      const int64 demand_i = reservoir.demands(i);
      level->mutable_linear()->add_domain(reservoir.min_level() - demand_i);
      level->mutable_linear()->add_domain(reservoir.max_level() - demand_i);
      const IntegerVariableProto& var_i = expanded.variables(ti);
      if (IsOptional(var_i)) {
        level->add_enforcement_literal(var_i.enforcement_literal(0));
      }
    }
  } else {
    // If all demands have the same sign, we do not care about the order, just
    // the sum.
    int64 fixed = 0;
    ConstraintProto* const sum = expanded.add_constraints();
    for (int i = 0; i < num_variables; ++i) {
      const int64 demand = reservoir.demands(i);
      if (demand == 0) continue;
      const IntegerVariableProto& var = expanded.variables(reservoir.times(i));
      if (IsOptional(var)) {
        sum->mutable_linear()->add_vars(var.enforcement_literal(0));
        sum->mutable_linear()->add_coeffs(demand);
      } else {
        fixed += demand;
      }
      // TODO(user): overflow?
    }
    sum->mutable_linear()->add_domain(reservoir.min_level() - fixed);
    sum->mutable_linear()->add_domain(reservoir.max_level() - fixed);
  }

  // Constrains the reservoir level to be consistent at time 0.
  // We need to do it only if 0 is not in [min_level..max_level].
  // Otherwise, the regular propagation will already check it.
  if (reservoir.min_level() > 0 || reservoir.max_level() < 0) {
    ConstraintProto* const initial = expanded.add_constraints();
    for (int i = 0; i < num_variables; ++i) {
      const int ti = reservoir.times(i);
      const int b = AddBoolVar(&expanded);
      initial->mutable_linear()->add_vars(b);
      AddVarEqualLessOrEqualZero(b, ti, &expanded);
      initial->mutable_linear()->add_coeffs(reservoir.demands(i));
    }
    initial->mutable_linear()->add_domain(reservoir.min_level());
    initial->mutable_linear()->add_domain(reservoir.max_level());
  }

  // Constrains all times to be >= 0.
  for (int i = 0; i < num_variables; ++i) {
    const int ti = reservoir.times(i);
    ConstraintProto* const positive = expanded.add_constraints();
    positive->mutable_linear()->add_vars(ti);
    positive->mutable_linear()->add_coeffs(1);
    positive->mutable_linear()->add_domain(0);
    positive->mutable_linear()->add_domain(kint64max);
  }

  ct->Clear();
  context->statistics["kReservoir"]++;
}

}  // namespace

CpModelProto ExpandCpModel(const CpModelProto& initial_model) {
  RewriteContext context;
  context.expanded_proto = initial_model;
  for (int i = 0; i < initial_model.constraints_size(); ++i) {
    ConstraintProto* const ct = context.expanded_proto.mutable_constraints(i);
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintCase::kReservoir:
        ExpandReservoir(ct, &context);
        break;
      default:
        break;
    }
  }

  return context.expanded_proto;
}
}  // namespace sat
}  // namespace operations_research
