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

#include "ortools/sat/cp_model_utils.h"

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

ABSL_FLAG(bool, cp_model_dump_models, false,
          "DEBUG ONLY. When set to true, SolveCpModel() will dump its model "
          "protos (original model, presolved model, mapping model) in text "
          "format to 'FLAGS_cp_model_dump_prefix'{model|presolved_model|"
          "mapping_model}.pb.txt.");

#if defined(_MSC_VER)
ABSL_FLAG(std::string, cp_model_dump_prefix, ".\\",
          "Prefix filename for all dumped files");
#else
ABSL_FLAG(std::string, cp_model_dump_prefix, "/tmp/",
          "Prefix filename for all dumped files");
#endif

ABSL_FLAG(bool, cp_model_dump_submodels, false,
          "DEBUG ONLY. When set to true, solve will dump all "
          "lns or objective_shaving submodels proto in text format to "
          "'FLAGS_cp_model_dump_prefix'xxx.pb.txt.");

namespace operations_research {
namespace sat {

namespace {

template <typename IntList>
void AddIndices(const IntList& indices, std::vector<int>* output) {
  output->insert(output->end(), indices.begin(), indices.end());
}

}  // namespace

int64_t LinearExpressionGcd(const LinearExpressionProto& expr, int64_t gcd) {
  gcd = std::gcd(gcd, std::abs(expr.offset()));
  for (const int64_t coeff : expr.coeffs()) {
    gcd = std::gcd(gcd, std::abs(coeff));
  }
  return gcd;
}

void DivideLinearExpression(int64_t divisor, LinearExpressionProto* expr) {
  CHECK_NE(divisor, 0);
  if (divisor == 1) return;

  DCHECK_EQ(expr->offset() % divisor, 0);
  expr->set_offset(expr->offset() / divisor);
  for (int i = 0; i < expr->vars_size(); ++i) {
    DCHECK_EQ(expr->coeffs(i) % divisor, 0);
    expr->set_coeffs(i, expr->coeffs(i) / divisor);
  }
}

void SetToNegatedLinearExpression(const LinearExpressionProto& input_expr,
                                  LinearExpressionProto* output_negated_expr) {
  output_negated_expr->Clear();
  for (int i = 0; i < input_expr.vars_size(); ++i) {
    output_negated_expr->add_vars(NegatedRef(input_expr.vars(i)));
    output_negated_expr->add_coeffs(input_expr.coeffs(i));
  }
  output_negated_expr->set_offset(-input_expr.offset());
}

IndexReferences GetReferencesUsedByConstraint(const ConstraintProto& ct) {
  IndexReferences output;
  GetReferencesUsedByConstraint(ct, &output.variables, &output.literals);
  return output;
}

void GetReferencesUsedByConstraint(const ConstraintProto& ct,
                                   std::vector<int>* variables,
                                   std::vector<int>* literals) {
  variables->clear();
  literals->clear();
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      AddIndices(ct.bool_or().literals(), literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      AddIndices(ct.bool_and().literals(), literals);
      break;
    case ConstraintProto::ConstraintCase::kAtMostOne:
      AddIndices(ct.at_most_one().literals(), literals);
      break;
    case ConstraintProto::ConstraintCase::kExactlyOne:
      AddIndices(ct.exactly_one().literals(), literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolXor:
      AddIndices(ct.bool_xor().literals(), literals);
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      AddIndices(ct.int_div().target().vars(), variables);
      for (const LinearExpressionProto& expr : ct.int_div().exprs()) {
        AddIndices(expr.vars(), variables);
      }
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      AddIndices(ct.int_mod().target().vars(), variables);
      for (const LinearExpressionProto& expr : ct.int_mod().exprs()) {
        AddIndices(expr.vars(), variables);
      }
      break;
    case ConstraintProto::ConstraintCase::kLinMax: {
      AddIndices(ct.lin_max().target().vars(), variables);
      for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
        AddIndices(expr.vars(), variables);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kIntProd:
      AddIndices(ct.int_prod().target().vars(), variables);
      for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
        AddIndices(expr.vars(), variables);
      }
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      AddIndices(ct.linear().vars(), variables);
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      for (const LinearExpressionProto& expr : ct.all_diff().exprs()) {
        AddIndices(expr.vars(), variables);
      }
      break;
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      AddIndices(ct.dummy_constraint().vars(), variables);
      break;
    case ConstraintProto::ConstraintCase::kElement:
      if (ct.element().index() != 0 || ct.element().target() != 0 ||
          !ct.element().vars().empty()) {
        variables->push_back(ct.element().index());
        variables->push_back(ct.element().target());
        AddIndices(ct.element().vars(), variables);
      } else if (ct.element().has_linear_index() ||
                 ct.element().has_linear_target() ||
                 !ct.element().exprs().empty()) {
        AddIndices(ct.element().linear_index().vars(), variables);
        AddIndices(ct.element().linear_target().vars(), variables);
        for (const LinearExpressionProto& expr : ct.element().exprs()) {
          AddIndices(expr.vars(), variables);
        }
      }
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      AddIndices(ct.circuit().literals(), literals);
      break;
    case ConstraintProto::ConstraintCase::kRoutes:
      AddIndices(ct.routes().literals(), literals);
      // The node expressions are not used by the constraint itself.
      break;
    case ConstraintProto::ConstraintCase::kInverse:
      AddIndices(ct.inverse().f_direct(), variables);
      AddIndices(ct.inverse().f_inverse(), variables);
      break;
    case ConstraintProto::ConstraintCase::kReservoir:
      for (const LinearExpressionProto& time : ct.reservoir().time_exprs()) {
        AddIndices(time.vars(), variables);
      }
      for (const LinearExpressionProto& level :
           ct.reservoir().level_changes()) {
        AddIndices(level.vars(), variables);
      }
      AddIndices(ct.reservoir().active_literals(), literals);
      break;
    case ConstraintProto::ConstraintCase::kTable:
      if (!ct.table().vars().empty()) {
        AddIndices(ct.table().vars(), variables);
      } else {
        for (const LinearExpressionProto& expr : ct.table().exprs()) {
          AddIndices(expr.vars(), variables);
        }
      }
      break;
    case ConstraintProto::ConstraintCase::kAutomaton:
      if (!ct.automaton().vars().empty()) {
        AddIndices(ct.automaton().vars(), variables);
      } else {
        for (const LinearExpressionProto& expr : ct.automaton().exprs()) {
          AddIndices(expr.vars(), variables);
        }
      }
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      AddIndices(ct.interval().start().vars(), variables);
      AddIndices(ct.interval().size().vars(), variables);
      AddIndices(ct.interval().end().vars(), variables);
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      AddIndices(ct.cumulative().capacity().vars(), variables);
      for (const LinearExpressionProto& demand : ct.cumulative().demands()) {
        AddIndices(demand.vars(), variables);
      }
      break;
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      break;
  }
}

#define APPLY_TO_SINGULAR_FIELD(ct_name, field_name)  \
  {                                                   \
    int temp = ct->mutable_##ct_name()->field_name(); \
    f(&temp);                                         \
    ct->mutable_##ct_name()->set_##field_name(temp);  \
  }

#define APPLY_TO_REPEATED_FIELD(ct_name, field_name)                       \
  {                                                                        \
    for (int& r : *ct->mutable_##ct_name()->mutable_##field_name()) f(&r); \
  }

void ApplyToAllLiteralIndices(const std::function<void(int*)>& f,
                              ConstraintProto* ct) {
  for (int& r : *ct->mutable_enforcement_literal()) f(&r);
  switch (ct->constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      APPLY_TO_REPEATED_FIELD(bool_or, literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      APPLY_TO_REPEATED_FIELD(bool_and, literals);
      break;
    case ConstraintProto::ConstraintCase::kAtMostOne:
      APPLY_TO_REPEATED_FIELD(at_most_one, literals);
      break;
    case ConstraintProto::ConstraintCase::kExactlyOne:
      APPLY_TO_REPEATED_FIELD(exactly_one, literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolXor:
      APPLY_TO_REPEATED_FIELD(bool_xor, literals);
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      break;
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      break;
    case ConstraintProto::ConstraintCase::kElement:
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      APPLY_TO_REPEATED_FIELD(circuit, literals);
      break;
    case ConstraintProto::ConstraintCase::kRoutes:
      APPLY_TO_REPEATED_FIELD(routes, literals);
      break;
    case ConstraintProto::ConstraintCase::kInverse:
      break;
    case ConstraintProto::ConstraintCase::kReservoir:
      APPLY_TO_REPEATED_FIELD(reservoir, active_literals);
      break;
    case ConstraintProto::ConstraintCase::kTable:
      break;
    case ConstraintProto::ConstraintCase::kAutomaton:
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      break;
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      break;
  }
}

void ApplyToAllVariableIndices(const std::function<void(int*)>& f,
                               ConstraintProto* ct) {
  switch (ct->constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      break;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      break;
    case ConstraintProto::ConstraintCase::kAtMostOne:
      break;
    case ConstraintProto::ConstraintCase::kExactlyOne:
      break;
    case ConstraintProto::ConstraintCase::kBoolXor:
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      APPLY_TO_REPEATED_FIELD(int_div, target()->mutable_vars);
      for (int i = 0; i < ct->int_div().exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(int_div, exprs(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      APPLY_TO_REPEATED_FIELD(int_mod, target()->mutable_vars);
      for (int i = 0; i < ct->int_mod().exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(int_mod, exprs(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      APPLY_TO_REPEATED_FIELD(lin_max, target()->mutable_vars);
      for (int i = 0; i < ct->lin_max().exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(lin_max, exprs(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      APPLY_TO_REPEATED_FIELD(int_prod, target()->mutable_vars);
      for (int i = 0; i < ct->int_prod().exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(int_prod, exprs(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      APPLY_TO_REPEATED_FIELD(linear, vars);
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      for (int i = 0; i < ct->all_diff().exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(all_diff, exprs(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      APPLY_TO_REPEATED_FIELD(dummy_constraint, vars);
      break;
    case ConstraintProto::ConstraintCase::kElement:
      if (ct->element().index() != 0 || ct->element().target() != 0 ||
          !ct->element().vars().empty()) {
        APPLY_TO_SINGULAR_FIELD(element, index);
        APPLY_TO_SINGULAR_FIELD(element, target);
        APPLY_TO_REPEATED_FIELD(element, vars);
      } else if (ct->element().has_linear_index() ||
                 ct->element().has_linear_target() ||
                 !ct->element().exprs().empty()) {
        APPLY_TO_REPEATED_FIELD(element, linear_index()->mutable_vars);
        APPLY_TO_REPEATED_FIELD(element, linear_target()->mutable_vars);
        for (int i = 0; i < ct->element().exprs_size(); ++i) {
          APPLY_TO_REPEATED_FIELD(element, exprs(i)->mutable_vars);
        }
      }
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      break;
    case ConstraintProto::ConstraintCase::kRoutes:
      break;
    case ConstraintProto::ConstraintCase::kInverse:
      APPLY_TO_REPEATED_FIELD(inverse, f_direct);
      APPLY_TO_REPEATED_FIELD(inverse, f_inverse);
      break;
    case ConstraintProto::ConstraintCase::kReservoir:
      for (int i = 0; i < ct->reservoir().time_exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(reservoir, time_exprs(i)->mutable_vars);
      }
      for (int i = 0; i < ct->reservoir().level_changes_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(reservoir, level_changes(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kTable:
      if (!ct->table().vars().empty()) {
        APPLY_TO_REPEATED_FIELD(table, vars);
      } else {
        for (int i = 0; i < ct->table().exprs_size(); ++i) {
          APPLY_TO_REPEATED_FIELD(table, exprs(i)->mutable_vars);
        }
      }
      break;
    case ConstraintProto::ConstraintCase::kAutomaton:
      if (!ct->automaton().vars().empty()) {
        APPLY_TO_REPEATED_FIELD(automaton, vars);
      } else {
        for (int i = 0; i < ct->automaton().exprs_size(); ++i) {
          APPLY_TO_REPEATED_FIELD(automaton, exprs(i)->mutable_vars);
        }
      }
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      APPLY_TO_REPEATED_FIELD(interval, start()->mutable_vars);
      APPLY_TO_REPEATED_FIELD(interval, size()->mutable_vars);
      APPLY_TO_REPEATED_FIELD(interval, end()->mutable_vars);
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      APPLY_TO_REPEATED_FIELD(cumulative, capacity()->mutable_vars);
      for (int i = 0; i < ct->cumulative().demands_size(); ++i) {
        for (int& r :
             *ct->mutable_cumulative()->mutable_demands(i)->mutable_vars()) {
          f(&r);
        }
      }
      break;
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      break;
  }
}

void ApplyToAllIntervalIndices(const std::function<void(int*)>& f,
                               ConstraintProto* ct) {
  switch (ct->constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      break;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      break;
    case ConstraintProto::ConstraintCase::kAtMostOne:
      break;
    case ConstraintProto::ConstraintCase::kExactlyOne:
      break;
    case ConstraintProto::ConstraintCase::kBoolXor:
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      break;
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      break;
    case ConstraintProto::ConstraintCase::kElement:
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      break;
    case ConstraintProto::ConstraintCase::kRoutes:
      break;
    case ConstraintProto::ConstraintCase::kInverse:
      break;
    case ConstraintProto::ConstraintCase::kReservoir:
      break;
    case ConstraintProto::ConstraintCase::kTable:
      break;
    case ConstraintProto::ConstraintCase::kAutomaton:
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      APPLY_TO_REPEATED_FIELD(no_overlap, intervals);
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      APPLY_TO_REPEATED_FIELD(no_overlap_2d, x_intervals);
      APPLY_TO_REPEATED_FIELD(no_overlap_2d, y_intervals);
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      APPLY_TO_REPEATED_FIELD(cumulative, intervals);
      break;
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      break;
  }
}

#undef APPLY_TO_SINGULAR_FIELD
#undef APPLY_TO_REPEATED_FIELD

absl::string_view ConstraintCaseName(
    ConstraintProto::ConstraintCase constraint_case) {
  switch (constraint_case) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      return "kBoolOr";
    case ConstraintProto::ConstraintCase::kBoolAnd:
      return "kBoolAnd";
    case ConstraintProto::ConstraintCase::kAtMostOne:
      return "kAtMostOne";
    case ConstraintProto::ConstraintCase::kExactlyOne:
      return "kExactlyOne";
    case ConstraintProto::ConstraintCase::kBoolXor:
      return "kBoolXor";
    case ConstraintProto::ConstraintCase::kIntDiv:
      return "kIntDiv";
    case ConstraintProto::ConstraintCase::kIntMod:
      return "kIntMod";
    case ConstraintProto::ConstraintCase::kLinMax:
      return "kLinMax";
    case ConstraintProto::ConstraintCase::kIntProd:
      return "kIntProd";
    case ConstraintProto::ConstraintCase::kLinear:
      return "kLinear";
    case ConstraintProto::ConstraintCase::kAllDiff:
      return "kAllDiff";
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      return "kDummyConstraint";
    case ConstraintProto::ConstraintCase::kElement:
      return "kElement";
    case ConstraintProto::ConstraintCase::kCircuit:
      return "kCircuit";
    case ConstraintProto::ConstraintCase::kRoutes:
      return "kRoutes";
    case ConstraintProto::ConstraintCase::kInverse:
      return "kInverse";
    case ConstraintProto::ConstraintCase::kReservoir:
      return "kReservoir";
    case ConstraintProto::ConstraintCase::kTable:
      return "kTable";
    case ConstraintProto::ConstraintCase::kAutomaton:
      return "kAutomaton";
    case ConstraintProto::ConstraintCase::kInterval:
      return "kInterval";
    case ConstraintProto::ConstraintCase::kNoOverlap:
      return "kNoOverlap";
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      return "kNoOverlap2D";
    case ConstraintProto::ConstraintCase::kCumulative:
      return "kCumulative";
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      return "kEmpty";
  }
}

std::vector<int> UsedVariables(const ConstraintProto& ct) {
  std::vector<int> result;
  GetReferencesUsedByConstraint(ct, &result, &result);
  for (int& ref : result) {
    ref = PositiveRef(ref);
  }
  for (const int lit : ct.enforcement_literal()) {
    result.push_back(PositiveRef(lit));
  }
  gtl::STLSortAndRemoveDuplicates(&result);
  return result;
}

std::vector<int> UsedIntervals(const ConstraintProto& ct) {
  std::vector<int> used_intervals;
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      break;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      break;
    case ConstraintProto::ConstraintCase::kAtMostOne:
      break;
    case ConstraintProto::ConstraintCase::kExactlyOne:
      break;
    case ConstraintProto::ConstraintCase::kBoolXor:
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      break;
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      break;
    case ConstraintProto::ConstraintCase::kElement:
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      break;
    case ConstraintProto::ConstraintCase::kRoutes:
      break;
    case ConstraintProto::ConstraintCase::kInverse:
      break;
    case ConstraintProto::ConstraintCase::kReservoir:
      break;
    case ConstraintProto::ConstraintCase::kTable:
      break;
    case ConstraintProto::ConstraintCase::kAutomaton:
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      AddIndices(ct.no_overlap().intervals(), &used_intervals);
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      AddIndices(ct.no_overlap_2d().x_intervals(), &used_intervals);
      AddIndices(ct.no_overlap_2d().y_intervals(), &used_intervals);
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      AddIndices(ct.cumulative().intervals(), &used_intervals);
      break;
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      break;
  }
  gtl::STLSortAndRemoveDuplicates(&used_intervals);
  return used_intervals;
}

int64_t ComputeInnerObjective(const CpObjectiveProto& objective,
                              absl::Span<const int64_t> solution) {
  int64_t objective_value = 0;
  for (int i = 0; i < objective.vars_size(); ++i) {
    int64_t coeff = objective.coeffs(i);
    const int ref = objective.vars(i);
    const int var = PositiveRef(ref);
    if (!RefIsPositive(ref)) coeff = -coeff;
    objective_value += coeff * solution[var];
  }
  return objective_value;
}

bool ExpressionContainsSingleRef(const LinearExpressionProto& expr) {
  return expr.offset() == 0 && expr.vars_size() == 1 &&
         std::abs(expr.coeffs(0)) == 1;
}

bool ExpressionIsAffine(const LinearExpressionProto& expr) {
  return expr.vars_size() <= 1;
}

// Returns the reference the expression can be reduced to. It will DCHECK that
// ExpressionContainsSingleRef(expr) is true.
int GetSingleRefFromExpression(const LinearExpressionProto& expr) {
  DCHECK(ExpressionContainsSingleRef(expr));
  return expr.coeffs(0) == 1 ? expr.vars(0) : NegatedRef(expr.vars(0));
}

void AddLinearExpressionToLinearConstraint(const LinearExpressionProto& expr,
                                           int64_t coefficient,
                                           LinearConstraintProto* linear) {
  for (int i = 0; i < expr.vars_size(); ++i) {
    linear->add_vars(expr.vars(i));
    linear->add_coeffs(expr.coeffs(i) * coefficient);
  }
  DCHECK(!linear->domain().empty());
  const int64_t shift = coefficient * expr.offset();
  if (shift != 0) {
    FillDomainInProto(ReadDomainFromProto(*linear).AdditionWith(Domain(-shift)),
                      linear);
  }
}

void AddWeightedLiteralToLinearConstraint(int lit, int64_t coeff,
                                          LinearConstraintProto* linear,
                                          int64_t* offset) {
  if (coeff == 0) return;
  if (RefIsPositive(lit)) {
    linear->add_vars(lit);
    linear->add_coeffs(coeff);
  } else {
    linear->add_vars(NegatedRef(lit));
    linear->add_coeffs(-coeff);
    *offset += coeff;
  }
}

void LiteralsToLinear(absl::Span<const int> literals, int64_t lb, int64_t ub,
                      LinearConstraintProto* linear) {
  linear->Clear();
  for (const int lit : literals) {
    if (RefIsPositive(lit)) {
      linear->add_vars(lit);
      linear->add_coeffs(1);
    } else {
      linear->add_vars(NegatedRef(lit));
      linear->add_coeffs(-1);
      lb -= 1;
      ub -= 1;
    }
  }
  linear->add_domain(lb);
  linear->add_domain(ub);
}

bool SafeAddLinearExpressionToLinearConstraint(
    const LinearExpressionProto& expr, int64_t coefficient,
    LinearConstraintProto* linear) {
  for (int i = 0; i < expr.vars_size(); ++i) {
    linear->add_vars(expr.vars(i));
    const int64_t prod = CapProd(expr.coeffs(i), coefficient);
    if (AtMinOrMaxInt64(prod)) return false;
    linear->add_coeffs(prod);
  }
  DCHECK(!linear->domain().empty());

  const int64_t shift = CapProd(coefficient, expr.offset());
  if (AtMinOrMaxInt64(shift)) return false;
  Domain d = ReadDomainFromProto(*linear).AdditionWith(Domain(-shift));
  if (AtMinOrMaxInt64(d.Min()) || AtMinOrMaxInt64(d.Max())) return false;
  FillDomainInProto(d, linear);
  return true;
}

bool LinearExpressionProtosAreEqual(const LinearExpressionProto& a,
                                    const LinearExpressionProto& b,
                                    int64_t b_scaling) {
  if (a.vars_size() != b.vars_size()) return false;
  if (a.offset() != b.offset() * b_scaling) return false;
  absl::flat_hash_map<int, int64_t> coeffs;
  for (int i = 0; i < a.vars_size(); ++i) {
    coeffs[a.vars(i)] += a.coeffs(i);
    coeffs[b.vars(i)] += -b.coeffs(i) * b_scaling;
  }

  for (const auto [var, coeff] : coeffs) {
    if (coeff != 0) return false;
  }
  return true;
}

uint64_t FingerprintExpression(const LinearExpressionProto& lin,
                               uint64_t seed) {
  uint64_t fp = seed;
  if (!lin.vars().empty()) {
    fp = FingerprintRepeatedField(lin.vars(), fp);
    fp = FingerprintRepeatedField(lin.coeffs(), fp);
  }
  fp = FingerprintSingleField(lin.offset(), fp);
  return fp;
}

uint64_t FingerprintModel(const CpModelProto& model, uint64_t seed) {
  uint64_t fp = seed;
  for (const IntegerVariableProto& var_proto : model.variables()) {
    fp = FingerprintRepeatedField(var_proto.domain(), fp);
  }
  for (const ConstraintProto& ct : model.constraints()) {
    if (!ct.enforcement_literal().empty()) {
      fp = FingerprintRepeatedField(ct.enforcement_literal(), fp);
    }
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        fp = FingerprintRepeatedField(ct.bool_or().literals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        fp = FingerprintRepeatedField(ct.bool_and().literals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kAtMostOne:
        fp = FingerprintRepeatedField(ct.at_most_one().literals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kExactlyOne:
        fp = FingerprintRepeatedField(ct.exactly_one().literals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kBoolXor:
        fp = FingerprintRepeatedField(ct.bool_xor().literals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kIntDiv:
        fp = FingerprintExpression(ct.int_div().target(), fp);
        for (const LinearExpressionProto& expr : ct.int_div().exprs()) {
          fp = FingerprintExpression(expr, fp);
        }
        break;
      case ConstraintProto::ConstraintCase::kIntMod:
        fp = FingerprintExpression(ct.int_mod().target(), fp);
        for (const LinearExpressionProto& expr : ct.int_mod().exprs()) {
          fp = FingerprintExpression(expr, fp);
        }
        break;
      case ConstraintProto::ConstraintCase::kLinMax: {
        fp = FingerprintExpression(ct.lin_max().target(), fp);
        for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
          fp = FingerprintExpression(expr, fp);
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kIntProd:
        fp = FingerprintExpression(ct.int_prod().target(), fp);
        for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
          fp = FingerprintExpression(expr, fp);
        }
        break;
      case ConstraintProto::ConstraintCase::kLinear:
        fp = FingerprintRepeatedField(ct.linear().vars(), fp);
        fp = FingerprintRepeatedField(ct.linear().coeffs(), fp);
        fp = FingerprintRepeatedField(ct.linear().domain(), fp);
        break;
      case ConstraintProto::ConstraintCase::kAllDiff:
        for (const LinearExpressionProto& expr : ct.all_diff().exprs()) {
          fp = FingerprintExpression(expr, fp);
        }
        break;
      case ConstraintProto::ConstraintCase::kDummyConstraint:
        break;
      case ConstraintProto::ConstraintCase::kElement:
        fp = FingerprintSingleField(ct.element().index(), fp);
        fp = FingerprintSingleField(ct.element().target(), fp);
        fp = FingerprintRepeatedField(ct.element().vars(), fp);
        fp = FingerprintExpression(ct.element().linear_index(), fp);
        fp = FingerprintExpression(ct.element().linear_target(), fp);
        for (const LinearExpressionProto& expr : ct.element().exprs()) {
          fp = FingerprintExpression(expr, fp);
        }
        break;
      case ConstraintProto::ConstraintCase::kCircuit:
        fp = FingerprintRepeatedField(ct.circuit().heads(), fp);
        fp = FingerprintRepeatedField(ct.circuit().tails(), fp);
        fp = FingerprintRepeatedField(ct.circuit().literals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kRoutes:
        fp = FingerprintRepeatedField(ct.routes().heads(), fp);
        fp = FingerprintRepeatedField(ct.routes().tails(), fp);
        fp = FingerprintRepeatedField(ct.routes().literals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        fp = FingerprintRepeatedField(ct.inverse().f_direct(), fp);
        fp = FingerprintRepeatedField(ct.inverse().f_inverse(), fp);
        break;
      case ConstraintProto::ConstraintCase::kReservoir:
        fp = FingerprintSingleField(ct.reservoir().min_level(), fp);
        fp = FingerprintSingleField(ct.reservoir().max_level(), fp);
        for (const LinearExpressionProto& expr : ct.reservoir().time_exprs()) {
          fp = FingerprintExpression(expr, fp);
        }
        for (const LinearExpressionProto& expr :
             ct.reservoir().level_changes()) {
          fp = FingerprintExpression(expr, fp);
        }
        break;
      case ConstraintProto::ConstraintCase::kTable:
        if (!ct.table().vars().empty()) {
          fp = FingerprintRepeatedField(ct.table().vars(), fp);
        } else {
          for (const LinearExpressionProto& expr : ct.table().exprs()) {
            fp = FingerprintExpression(expr, fp);
          }
        }
        fp = FingerprintRepeatedField(ct.table().values(), fp);
        fp = FingerprintSingleField(ct.table().negated(), fp);
        break;
      case ConstraintProto::ConstraintCase::kAutomaton:
        fp = FingerprintSingleField(ct.automaton().starting_state(), fp);
        fp = FingerprintRepeatedField(ct.automaton().final_states(), fp);
        fp = FingerprintRepeatedField(ct.automaton().transition_tail(), fp);
        fp = FingerprintRepeatedField(ct.automaton().transition_head(), fp);
        fp = FingerprintRepeatedField(ct.automaton().transition_label(), fp);
        if (!ct.automaton().vars().empty()) {
          fp = FingerprintRepeatedField(ct.automaton().vars(), fp);
        } else {
          for (const LinearExpressionProto& expr : ct.automaton().exprs()) {
            fp = FingerprintExpression(expr, fp);
          }
        }
        break;
      case ConstraintProto::ConstraintCase::kInterval:
        fp = FingerprintExpression(ct.interval().start(), fp);
        fp = FingerprintExpression(ct.interval().size(), fp);
        fp = FingerprintExpression(ct.interval().end(), fp);
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap:
        fp = FingerprintRepeatedField(ct.no_overlap().intervals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        fp = FingerprintRepeatedField(ct.no_overlap_2d().x_intervals(), fp);
        fp = FingerprintRepeatedField(ct.no_overlap_2d().y_intervals(), fp);
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        fp = FingerprintRepeatedField(ct.cumulative().intervals(), fp);
        fp = FingerprintExpression(ct.cumulative().capacity(), fp);
        for (const LinearExpressionProto& demand : ct.cumulative().demands()) {
          fp = FingerprintExpression(demand, fp);
        }
        break;
      case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
        break;
    }
  }

  // Fingerprint the objective.
  if (model.has_objective()) {
    fp = FingerprintRepeatedField(model.objective().vars(), fp);
    fp = FingerprintRepeatedField(model.objective().coeffs(), fp);
    fp = FingerprintSingleField(model.objective().offset(), fp);
    fp = FingerprintSingleField(model.objective().scaling_factor(), fp);
    fp = FingerprintRepeatedField(model.objective().domain(), fp);
  } else if (model.has_floating_point_objective()) {
    fp = FingerprintRepeatedField(model.floating_point_objective().vars(), fp);
    fp =
        FingerprintRepeatedField(model.floating_point_objective().coeffs(), fp);
    fp = FingerprintSingleField(model.floating_point_objective().offset(), fp);
    fp =
        FingerprintSingleField(model.floating_point_objective().maximize(), fp);
  }

  if (model.has_solution_hint()) {
    fp = FingerprintRepeatedField(model.solution_hint().vars(), fp);
    fp = FingerprintRepeatedField(model.solution_hint().values(), fp);
  }

  // TODO(user): Should we fingerprint decision strategies?

  return fp;
}

#if !defined(__PORTABLE_PLATFORM__)
namespace {

// We need to print " { " instead of " {\n" to inline our variables like:
//
// variables { domain: [0, 1] }
//
// instead of
//
// variables {
//   domain: [0, 1] }
class InlineFieldPrinter
    : public google::protobuf::TextFormat::FastFieldValuePrinter {
  void PrintMessageStart(const google::protobuf::Message& /*message*/,
                         int /*field_index*/, int /*field_count*/,
                         bool /*single_line_mode*/,
                         google::protobuf::TextFormat::BaseTextGenerator*
                             generator) const override {
    generator->PrintLiteral(" { ");
  }
};

class InlineMessagePrinter
    : public google::protobuf::TextFormat::MessagePrinter {
 public:
  InlineMessagePrinter() {
    printer_.SetSingleLineMode(true);
    printer_.SetUseShortRepeatedPrimitives(true);
  }

  void Print(const google::protobuf::Message& message,
             bool /*single_line_mode*/,
             google::protobuf::TextFormat::BaseTextGenerator* generator)
      const override {
    buffer_.clear();
    printer_.PrintToString(message, &buffer_);
    generator->Print(buffer_.data(), buffer_.size());
  }

 private:
  google::protobuf::TextFormat::Printer printer_;
  mutable std::string buffer_;
};

// Register a InlineFieldPrinter() for all the fields containing the message we
// want to print in one line.
void RegisterFieldPrinters(
    const google::protobuf::Descriptor* descriptor,
    absl::flat_hash_set<const google::protobuf::Descriptor*>* descriptors,
    google::protobuf::TextFormat::Printer* printer) {
  // Recursion stopper.
  if (!descriptors->insert(descriptor).second) return;

  for (int i = 0; i < descriptor->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* field = descriptor->field(i);
    if (field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE) {
      if (field->message_type() == IntegerVariableProto::descriptor() ||
          field->message_type() == LinearExpressionProto::descriptor()) {
        printer->RegisterFieldValuePrinter(field, new InlineFieldPrinter());
      } else {
        RegisterFieldPrinters(field->message_type(), descriptors, printer);
      }
    }
  }
}

}  // namespace

void SetupTextFormatPrinter(google::protobuf::TextFormat::Printer* printer) {
  printer->SetUseShortRepeatedPrimitives(true);
  absl::flat_hash_set<const google::protobuf::Descriptor*> descriptors;
  RegisterFieldPrinters(CpModelProto::descriptor(), &descriptors, printer);
  printer->RegisterMessagePrinter(IntegerVariableProto::descriptor(),
                                  new InlineMessagePrinter());
  printer->RegisterMessagePrinter(LinearExpressionProto::descriptor(),
                                  new InlineMessagePrinter());
}
#endif  // !defined(__PORTABLE_PLATFORM__)

bool ConvertCpModelProtoToCnf(const CpModelProto& cp_model, std::string* out) {
  out->clear();

  // We should have no objective, only unassigned Boolean, and only bool_or and
  // bool_and.
  if (cp_model.has_objective()) return false;
  const int num_vars = cp_model.variables().size();
  for (int v = 0; v < num_vars; ++v) {
    if (cp_model.variables(v).domain().size() != 2) return false;
    if (cp_model.variables(v).domain(0) != 0) return false;
    if (cp_model.variables(v).domain(1) != 1) return false;
  }
  int num_clauses = 0;
  for (const ConstraintProto& ct : cp_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kBoolOr) {
      ++num_clauses;
    } else if (ct.constraint_case() == ConstraintProto::kBoolAnd) {
      num_clauses += ct.bool_and().literals().size();
    } else {
      return false;
    }
  }

  // We can convert.
  std::string start;
  absl::StrAppend(out, "p cnf ", num_vars, " ", num_clauses, "\n");
  for (const ConstraintProto& ct : cp_model.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kBoolOr) {
      CHECK(ct.enforcement_literal().empty());
      for (const int lit : ct.bool_or().literals()) {
        const int value =
            Literal(BooleanVariable(PositiveRef(lit)), RefIsPositive(lit))
                .SignedValue();
        absl::StrAppend(out, value, " ");
      }
      absl::StrAppend(out, "0\n");
    } else if (ct.constraint_case() == ConstraintProto::kBoolAnd) {
      CHECK(!ct.enforcement_literal().empty());
      start.clear();
      for (const int lit : ct.enforcement_literal()) {
        const int value =
            Literal(BooleanVariable(PositiveRef(lit)), RefIsPositive(lit))
                .SignedValue();
        absl::StrAppend(&start, -value, " ");
      }
      for (const int lit : ct.bool_and().literals()) {
        const int value =
            Literal(BooleanVariable(PositiveRef(lit)), RefIsPositive(lit))
                .SignedValue();
        absl::StrAppend(out, start, value, " 0\n");
      }
    }
  }

  return true;
}

int CombineSeed(int base_seed, int64_t delta) {
  CHECK_GE(delta, 0);
  const uint64_t fp = FingerprintSingleField(delta, kDefaultFingerprintSeed);
  return static_cast<int>(FingerprintSingleField(base_seed, fp) & (0x7FFFFFFF));
}

}  // namespace sat
}  // namespace operations_research
