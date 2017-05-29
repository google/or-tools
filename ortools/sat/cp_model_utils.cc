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

#include "ortools/sat/cp_model_utils.h"

#include "ortools/base/stl_util.h"

namespace operations_research {
namespace sat {

namespace {

template <typename IntList>
void AddIndices(const IntList& indices, std::unordered_set<int>* output) {
  for (const int index : indices) output->insert(index);
}

}  // namespace

void AddReferencesUsedByConstraint(const ConstraintProto& ct,
                                   IndexReferences* output) {
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      AddIndices(ct.bool_or().literals(), &output->literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolAnd:
      AddIndices(ct.bool_and().literals(), &output->literals);
      break;
    case ConstraintProto::ConstraintCase::kBoolXor:
      AddIndices(ct.bool_xor().literals(), &output->literals);
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      output->variables.insert(ct.int_div().target());
      AddIndices(ct.int_div().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      output->variables.insert(ct.int_mod().target());
      AddIndices(ct.int_mod().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kIntMax:
      output->variables.insert(ct.int_max().target());
      AddIndices(ct.int_max().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kIntMin:
      output->variables.insert(ct.int_min().target());
      AddIndices(ct.int_min().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      output->variables.insert(ct.int_prod().target());
      AddIndices(ct.int_prod().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      AddIndices(ct.linear().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      AddIndices(ct.all_diff().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kElement:
      output->variables.insert(ct.element().index());
      output->variables.insert(ct.element().target());
      AddIndices(ct.element().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      AddIndices(ct.circuit().nexts(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kTable:
      AddIndices(ct.table().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kAutomata:
      AddIndices(ct.automata().vars(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      output->variables.insert(ct.interval().start());
      output->variables.insert(ct.interval().end());
      output->variables.insert(ct.interval().size());
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      AddIndices(ct.no_overlap().intervals(), &output->intervals);
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      AddIndices(ct.no_overlap_2d().x_intervals(), &output->intervals);
      AddIndices(ct.no_overlap_2d().y_intervals(), &output->intervals);
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      output->variables.insert(ct.cumulative().capacity());
      AddIndices(ct.cumulative().intervals(), &output->intervals);
      AddIndices(ct.cumulative().demands(), &output->variables);
      break;
    case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
      // Empty constraint.
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
    case ConstraintProto::ConstraintCase::kBoolXor:
      APPLY_TO_REPEATED_FIELD(bool_xor, literals);
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      break;
    case ConstraintProto::ConstraintCase::kIntMax:
      break;
    case ConstraintProto::ConstraintCase::kIntMin:
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      break;
    case ConstraintProto::ConstraintCase::kElement:
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      break;
    case ConstraintProto::ConstraintCase::kTable:
      break;
    case ConstraintProto::ConstraintCase::kAutomata:
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
    case ConstraintProto::ConstraintCase::kIntMin:
      APPLY_TO_SINGULAR_FIELD(int_min, target);
      APPLY_TO_REPEATED_FIELD(int_min, vars);
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
    case ConstraintProto::ConstraintCase::kElement:
      APPLY_TO_SINGULAR_FIELD(element, index);
      APPLY_TO_SINGULAR_FIELD(element, target);
      APPLY_TO_REPEATED_FIELD(element, vars);
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      APPLY_TO_REPEATED_FIELD(circuit, nexts);
      break;
    case ConstraintProto::ConstraintCase::kTable:
      APPLY_TO_REPEATED_FIELD(table, vars);
      break;
    case ConstraintProto::ConstraintCase::kAutomata:
      APPLY_TO_REPEATED_FIELD(automata, vars);
      break;
    case ConstraintProto::ConstraintCase::kInterval:
      APPLY_TO_SINGULAR_FIELD(interval, start);
      APPLY_TO_SINGULAR_FIELD(interval, end);
      APPLY_TO_SINGULAR_FIELD(interval, size);
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap:
      break;
    case ConstraintProto::ConstraintCase::kNoOverlap2D:
      break;
    case ConstraintProto::ConstraintCase::kCumulative:
      APPLY_TO_SINGULAR_FIELD(cumulative, capacity);
      APPLY_TO_REPEATED_FIELD(cumulative, demands);
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
    case ConstraintProto::ConstraintCase::kBoolXor:
      break;
    case ConstraintProto::ConstraintCase::kIntDiv:
      break;
    case ConstraintProto::ConstraintCase::kIntMod:
      break;
    case ConstraintProto::ConstraintCase::kIntMax:
      break;
    case ConstraintProto::ConstraintCase::kIntMin:
      break;
    case ConstraintProto::ConstraintCase::kIntProd:
      break;
    case ConstraintProto::ConstraintCase::kLinear:
      break;
    case ConstraintProto::ConstraintCase::kAllDiff:
      break;
    case ConstraintProto::ConstraintCase::kElement:
      break;
    case ConstraintProto::ConstraintCase::kCircuit:
      break;
    case ConstraintProto::ConstraintCase::kTable:
      break;
    case ConstraintProto::ConstraintCase::kAutomata:
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

std::string ConstraintCaseName(ConstraintProto::ConstraintCase constraint_case) {
  switch (constraint_case) {
    case ConstraintProto::ConstraintCase::kBoolOr:
      return "kBoolOr";
    case ConstraintProto::ConstraintCase::kBoolAnd:
      return "kBoolAnd";
    case ConstraintProto::ConstraintCase::kBoolXor:
      return "kBoolXor";
    case ConstraintProto::ConstraintCase::kIntDiv:
      return "kIntDiv";
    case ConstraintProto::ConstraintCase::kIntMod:
      return "kIntMod";
    case ConstraintProto::ConstraintCase::kIntMax:
      return "kIntMax";
    case ConstraintProto::ConstraintCase::kIntMin:
      return "kIntMin";
    case ConstraintProto::ConstraintCase::kIntProd:
      return "kIntProd";
    case ConstraintProto::ConstraintCase::kLinear:
      return "kLinear";
    case ConstraintProto::ConstraintCase::kAllDiff:
      return "kAllDiff";
    case ConstraintProto::ConstraintCase::kElement:
      return "kElement";
    case ConstraintProto::ConstraintCase::kCircuit:
      return "kCircuit";
    case ConstraintProto::ConstraintCase::kTable:
      return "kTable";
    case ConstraintProto::ConstraintCase::kAutomata:
      return "kAutomata";
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

}  // namespace sat
}  // namespace operations_research
