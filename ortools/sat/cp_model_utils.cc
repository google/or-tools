// Copyright 2010-2021 Google LLC
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
#include <functional>

#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {

namespace {

template <typename IntList>
void AddIndices(const IntList& indices, std::vector<int>* output) {
  output->insert(output->end(), indices.begin(), indices.end());
}

}  // namespace

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
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      AddIndices(ct.bool_or().literals(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      AddIndices(ct.bool_and().literals(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kAtMostOne:
      AddIndices(ct.at_most_one().literals(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kExactlyOne:
      AddIndices(ct.exactly_one().literals(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolXor:
      AddIndices(ct.bool_xor().literals(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      output.variables.push_back(ct.int_div().target());
      AddIndices(ct.int_div().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      output.variables.push_back(ct.int_mod().target());
      AddIndices(ct.int_mod().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kIntMax:
      output.variables.push_back(ct.int_max().target());
      AddIndices(ct.int_max().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kLinMax: {
      AddIndices(ct.lin_max().target().vars(), &output.variables);
      for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
        AddIndices(ct.lin_max().exprs(i).vars(), &output.variables);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kIntMin:
      output.variables.push_back(ct.int_min().target());
      AddIndices(ct.int_min().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kLinMin: {
      AddIndices(ct.lin_min().target().vars(), &output.variables);
      for (int i = 0; i < ct.lin_min().exprs_size(); ++i) {
        AddIndices(ct.lin_min().exprs(i).vars(), &output.variables);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kIntProd:
      output.variables.push_back(ct.int_prod().target());
      AddIndices(ct.int_prod().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      AddIndices(ct.linear().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      AddIndices(ct.all_diff().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      AddIndices(ct.dummy_constraint().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kElement:
      output.variables.push_back(ct.element().index());
      output.variables.push_back(ct.element().target());
      AddIndices(ct.element().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      AddIndices(ct.circuit().literals(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kRoutes:
      AddIndices(ct.routes().literals(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kInverse:
      AddIndices(ct.inverse().f_direct(), &output.variables);
      AddIndices(ct.inverse().f_inverse(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kReservoir:
      AddIndices(ct.reservoir().times(), &output.variables);
      AddIndices(ct.reservoir().actives(), &output.literals);
      break;
    case ConstraintProto::ConstraintCase::kTable:
      AddIndices(ct.table().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kAutomaton:
      AddIndices(ct.automaton().vars(), &output.variables);
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      if (ct.interval().has_start_view()) {
        AddIndices(ct.interval().start_view().vars(), &output.variables);
      } else {
        output.variables.push_back(ct.interval().start());
      }
      if (ct.interval().has_size_view()) {
        AddIndices(ct.interval().size_view().vars(), &output.variables);
      } else {
        output.variables.push_back(ct.interval().size());
      }
      if (ct.interval().has_end_view()) {
        AddIndices(ct.interval().end_view().vars(), &output.variables);
      } else {
        output.variables.push_back(ct.interval().end());
      }
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      output.variables.push_back(ct.cumulative().capacity());
      AddIndices(ct.cumulative().demands(), &output.variables);
      for (const LinearExpressionProto& lin : ct.cumulative().energies()) {
        AddIndices(lin.vars(), &output.variables);
      }
      break;
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      break;
  }
  return output;
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
    case ConstraintProto::ConstraintCase::kIntMax:
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      break;
    case ConstraintProto::ConstraintCase::kIntMin:
      break;
    case ConstraintProto::ConstraintCase::kLinMin:
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
      APPLY_TO_REPEATED_FIELD(reservoir, actives);
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
      APPLY_TO_SINGULAR_FIELD(int_div, target);
      APPLY_TO_REPEATED_FIELD(int_div, vars);
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      APPLY_TO_SINGULAR_FIELD(int_mod, target);
      APPLY_TO_REPEATED_FIELD(int_mod, vars);
      break;
    case ConstraintProto::ConstraintCase::kIntMax:
      APPLY_TO_SINGULAR_FIELD(int_max, target);
      APPLY_TO_REPEATED_FIELD(int_max, vars);
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      APPLY_TO_REPEATED_FIELD(lin_max, target()->mutable_vars);
      for (int i = 0; i < ct->lin_max().exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(lin_max, exprs(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kIntMin:
      APPLY_TO_SINGULAR_FIELD(int_min, target);
      APPLY_TO_REPEATED_FIELD(int_min, vars);
      break;
    case ConstraintProto::ConstraintCase::kLinMin:
      APPLY_TO_REPEATED_FIELD(lin_min, target()->mutable_vars);
      for (int i = 0; i < ct->lin_min().exprs_size(); ++i) {
        APPLY_TO_REPEATED_FIELD(lin_min, exprs(i)->mutable_vars);
      }
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      APPLY_TO_SINGULAR_FIELD(int_prod, target);
      APPLY_TO_REPEATED_FIELD(int_prod, vars);
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      APPLY_TO_REPEATED_FIELD(linear, vars);
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      APPLY_TO_REPEATED_FIELD(all_diff, vars);
      break;
    case ConstraintProto::ConstraintCase::kDummyConstraint:
      APPLY_TO_REPEATED_FIELD(dummy_constraint, vars);
      break;
    case ConstraintProto::ConstraintCase::kElement:
      APPLY_TO_SINGULAR_FIELD(element, index);
      APPLY_TO_SINGULAR_FIELD(element, target);
      APPLY_TO_REPEATED_FIELD(element, vars);
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
      APPLY_TO_REPEATED_FIELD(reservoir, times);
      break;
    case ConstraintProto::ConstraintCase::kTable:
      APPLY_TO_REPEATED_FIELD(table, vars);
      break;
    case ConstraintProto::ConstraintCase::kAutomaton:
      APPLY_TO_REPEATED_FIELD(automaton, vars);
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      if (ct->interval().has_start_view()) {
        APPLY_TO_REPEATED_FIELD(interval, start_view()->mutable_vars);
      } else {
        APPLY_TO_SINGULAR_FIELD(interval, start);
      }
      if (ct->interval().has_size_view()) {
        APPLY_TO_REPEATED_FIELD(interval, size_view()->mutable_vars);
      } else {
        APPLY_TO_SINGULAR_FIELD(interval, size);
      }
      if (ct->interval().has_end_view()) {
        APPLY_TO_REPEATED_FIELD(interval, end_view()->mutable_vars);
      } else {
        APPLY_TO_SINGULAR_FIELD(interval, end);
      }
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      APPLY_TO_SINGULAR_FIELD(cumulative, capacity);
      APPLY_TO_REPEATED_FIELD(cumulative, demands);
      for (int i = 0; i < ct->cumulative().energies_size(); ++i) {
        for (int& r :
             *ct->mutable_cumulative()->mutable_energies(i)->mutable_vars()) {
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
    case ConstraintProto::ConstraintCase::kIntMax:
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      break;
    case ConstraintProto::ConstraintCase::kIntMin:
      break;
    case ConstraintProto::ConstraintCase::kLinMin:
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

std::string ConstraintCaseName(
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
    case ConstraintProto::ConstraintCase::kIntMax:
      return "kIntMax";
    case ConstraintProto::ConstraintCase::kLinMax:
      return "kLinMax";
    case ConstraintProto::ConstraintCase::kIntMin:
      return "kIntMin";
    case ConstraintProto::ConstraintCase::kLinMin:
      return "kLinMin";
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
  IndexReferences references = GetReferencesUsedByConstraint(ct);
  for (int& ref : references.variables) {
    ref = PositiveRef(ref);
  }
  for (const int lit : references.literals) {
    references.variables.push_back(PositiveRef(lit));
  }
  for (const int lit : ct.enforcement_literal()) {
    references.variables.push_back(PositiveRef(lit));
  }
  gtl::STLSortAndRemoveDuplicates(&references.variables);
  return references.variables;
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
    case ConstraintProto::ConstraintCase::kIntMax:
      break;
    case ConstraintProto::ConstraintCase::kLinMax:
      break;
    case ConstraintProto::ConstraintCase::kIntMin:
      break;
    case ConstraintProto::ConstraintCase::kLinMin:
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
                              const CpSolverResponse& response) {
  int64_t objective_value = 0;
  auto& repeated_field_values = response.solution().empty()
                                    ? response.solution_lower_bounds()
                                    : response.solution();
  for (int i = 0; i < objective.vars_size(); ++i) {
    int64_t coeff = objective.coeffs(i);
    const int ref = objective.vars(i);
    const int var = PositiveRef(ref);
    if (!RefIsPositive(ref)) coeff = -coeff;
    objective_value += coeff * repeated_field_values[var];
  }
  return objective_value;
}

}  // namespace sat
}  // namespace operations_research
