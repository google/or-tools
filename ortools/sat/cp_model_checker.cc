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

#include "ortools/sat/cp_model_checker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/primary_variables.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

ABSL_FLAG(bool, cp_model_check_dependent_variables, false,
          "When true, check that solutions can be computed only from their "
          "free variables.");

namespace operations_research {
namespace sat {
namespace {

// =============================================================================
// CpModelProto validation.
// =============================================================================

// If the string returned by "statement" is not empty, returns it.
#define RETURN_IF_NOT_EMPTY(statement)                \
  do {                                                \
    const std::string error_message = statement;      \
    if (!error_message.empty()) return error_message; \
  } while (false)

template <typename ProtoWithDomain>
bool DomainInProtoIsValid(const ProtoWithDomain& proto) {
  if (proto.domain().size() % 2) return false;
  std::vector<ClosedInterval> domain;
  for (int i = 0; i < proto.domain_size(); i += 2) {
    if (proto.domain(i) > proto.domain(i + 1)) return false;
    domain.push_back({proto.domain(i), proto.domain(i + 1)});
  }
  return IntervalsAreSortedAndNonAdjacent(domain);
}

bool VariableReferenceIsValid(const CpModelProto& model, int reference) {
  // We do it this way to avoid overflow if reference is kint64min for instance.
  if (reference >= model.variables_size()) return false;
  return reference >= -static_cast<int>(model.variables_size());
}

// Note(user): Historically we always accepted positive or negative variable
// reference everywhere, but now that we can always substitute affine relation,
// we starts to transition to positive reference only, which are clearer. Note
// that this doesn't concern literal reference though.
bool VariableIndexIsValid(const CpModelProto& model, int var) {
  return var >= 0 && var < model.variables_size();
}

bool LiteralReferenceIsValid(const CpModelProto& model, int reference) {
  if (!VariableReferenceIsValid(model, reference)) return false;
  const auto& var_proto = model.variables(PositiveRef(reference));
  const int64_t min_domain = var_proto.domain(0);
  const int64_t max_domain = var_proto.domain(var_proto.domain_size() - 1);
  return min_domain >= 0 && max_domain <= 1;
}

std::string ValidateIntegerVariable(const CpModelProto& model, int v) {
  const IntegerVariableProto& proto = model.variables(v);
  if (proto.domain_size() == 0) {
    return absl::StrCat("var #", v,
                        " has no domain(): ", ProtobufShortDebugString(proto));
  }
  if (proto.domain_size() % 2 != 0) {
    return absl::StrCat("var #", v, " has an odd domain() size: ",
                        ProtobufShortDebugString(proto));
  }
  if (!DomainInProtoIsValid(proto)) {
    return absl::StrCat("var #", v, " has and invalid domain() format: ",
                        ProtobufShortDebugString(proto));
  }

  // Internally, we often take the negation of a domain, and we also want to
  // have sentinel values greater than the min/max of a variable domain, so
  // the domain must fall in [-kint64max / 2, kint64max / 2].
  const int64_t lb = proto.domain(0);
  const int64_t ub = proto.domain(proto.domain_size() - 1);
  if (lb < -std::numeric_limits<int64_t>::max() / 2 ||
      ub > std::numeric_limits<int64_t>::max() / 2) {
    return absl::StrCat(
        "var #", v, " domain do not fall in [-kint64max / 2, kint64max / 2]. ",
        ProtobufShortDebugString(proto));
  }

  // We do compute ub - lb in some place in the code and do not want to deal
  // with overflow everywhere. This seems like a reasonable precondition anyway.
  if (lb < 0 && lb + std::numeric_limits<int64_t>::max() < ub) {
    return absl::StrCat(
        "var #", v,
        " has a domain that is too large, i.e. |UB - LB| overflow an int64_t: ",
        ProtobufShortDebugString(proto));
  }

  return "";
}

std::string ValidateVariablesUsedInConstraint(const CpModelProto& model,
                                              int c) {
  const ConstraintProto& ct = model.constraints(c);
  IndexReferences references = GetReferencesUsedByConstraint(ct);
  for (const int v : references.variables) {
    if (!VariableReferenceIsValid(model, v)) {
      return absl::StrCat("Out of bound integer variable ", v,
                          " in constraint #", c, " : ",
                          ProtobufShortDebugString(ct));
    }
  }
  for (const int lit : ct.enforcement_literal()) {
    if (!LiteralReferenceIsValid(model, lit)) {
      return absl::StrCat("Invalid enforcement literal ", lit,
                          " in constraint #", c, " : ",
                          ProtobufShortDebugString(ct));
    }
  }
  for (const int lit : references.literals) {
    if (!LiteralReferenceIsValid(model, lit)) {
      return absl::StrCat("Invalid literal ", lit, " in constraint #", c, " : ",
                          ProtobufShortDebugString(ct));
    }
  }
  return "";
}

std::string ValidateIntervalsUsedInConstraint(bool after_presolve,
                                              const CpModelProto& model,
                                              int c) {
  const ConstraintProto& ct = model.constraints(c);
  for (const int i : UsedIntervals(ct)) {
    if (i < 0 || i >= model.constraints_size()) {
      return absl::StrCat("Out of bound interval ", i, " in constraint #", c,
                          " : ", ProtobufShortDebugString(ct));
    }
    if (after_presolve && i >= c) {
      return absl::StrCat("Interval ", i, " in constraint #", c,
                          " must appear before in the list of constraints :",
                          ProtobufShortDebugString(ct));
    }
    if (model.constraints(i).constraint_case() !=
        ConstraintProto::ConstraintCase::kInterval) {
      return absl::StrCat(
          "Interval ", i,
          " does not refer to an interval constraint. Problematic constraint #",
          c, " : ", ProtobufShortDebugString(ct));
    }
  }
  return "";
}

int64_t MinOfRef(const CpModelProto& model, int ref) {
  const IntegerVariableProto& var_proto = model.variables(PositiveRef(ref));
  if (RefIsPositive(ref)) {
    return var_proto.domain(0);
  } else {
    return -var_proto.domain(var_proto.domain_size() - 1);
  }
}

int64_t MaxOfRef(const CpModelProto& model, int ref) {
  const IntegerVariableProto& var_proto = model.variables(PositiveRef(ref));
  if (RefIsPositive(ref)) {
    return var_proto.domain(var_proto.domain_size() - 1);
  } else {
    return -var_proto.domain(0);
  }
}

template <class LinearExpressionProto>
int64_t MinOfExpression(const CpModelProto& model,
                        const LinearExpressionProto& proto) {
  int64_t sum_min = proto.offset();
  for (int i = 0; i < proto.vars_size(); ++i) {
    const int ref = proto.vars(i);
    const int64_t coeff = proto.coeffs(i);
    sum_min =
        CapAdd(sum_min, coeff >= 0 ? CapProd(MinOfRef(model, ref), coeff)
                                   : CapProd(MaxOfRef(model, ref), coeff));
  }

  return sum_min;
}

template <class LinearExpressionProto>
int64_t MaxOfExpression(const CpModelProto& model,
                        const LinearExpressionProto& proto) {
  int64_t sum_max = proto.offset();
  for (int i = 0; i < proto.vars_size(); ++i) {
    const int ref = proto.vars(i);
    const int64_t coeff = proto.coeffs(i);
    sum_max =
        CapAdd(sum_max, coeff >= 0 ? CapProd(MaxOfRef(model, ref), coeff)
                                   : CapProd(MinOfRef(model, ref), coeff));
  }

  return sum_max;
}

bool ExpressionIsFixed(const CpModelProto& model,
                       const LinearExpressionProto& expr) {
  for (int i = 0; i < expr.vars_size(); ++i) {
    if (expr.coeffs(i) == 0) continue;
    const IntegerVariableProto& var_proto = model.variables(expr.vars(i));
    if (var_proto.domain_size() != 2 ||
        var_proto.domain(0) != var_proto.domain(1)) {
      return false;
    }
  }
  return true;
}

int64_t ExpressionFixedValue(const CpModelProto& model,
                             const LinearExpressionProto& expr) {
  DCHECK(ExpressionIsFixed(model, expr));
  return MinOfExpression(model, expr);
}

int64_t IntervalSizeMax(const CpModelProto& model, int interval_index) {
  DCHECK_EQ(ConstraintProto::ConstraintCase::kInterval,
            model.constraints(interval_index).constraint_case());
  const IntervalConstraintProto& proto =
      model.constraints(interval_index).interval();
  return MaxOfExpression(model, proto.size());
}

Domain DomainOfRef(const CpModelProto& model, int ref) {
  const Domain domain = ReadDomainFromProto(model.variables(PositiveRef(ref)));
  return RefIsPositive(ref) ? domain : domain.Negation();
}

std::string ValidateLinearExpression(const CpModelProto& model,
                                     const LinearExpressionProto& expr) {
  if (expr.coeffs_size() != expr.vars_size()) {
    return absl::StrCat("coeffs_size() != vars_size() in linear expression: ",
                        ProtobufShortDebugString(expr));
  }
  if (PossibleIntegerOverflow(model, expr.vars(), expr.coeffs(),
                              expr.offset())) {
    return absl::StrCat("Possible overflow in linear expression: ",
                        ProtobufShortDebugString(expr));
  }
  for (const int var : expr.vars()) {
    if (!RefIsPositive(var)) {
      return absl::StrCat("Invalid negated variable in linear expression: ",
                          ProtobufShortDebugString(expr));
    }
  }
  return "";
}

std::string ValidateAffineExpression(const CpModelProto& model,
                                     const LinearExpressionProto& expr) {
  if (expr.vars_size() > 1) {
    return absl::StrCat("expression must be affine: ",
                        ProtobufShortDebugString(expr));
  }
  return ValidateLinearExpression(model, expr);
}

std::string ValidateConstantAffineExpression(
    const CpModelProto& model, const LinearExpressionProto& expr) {
  if (!expr.vars().empty()) {
    return absl::StrCat("expression must be constant: ",
                        ProtobufShortDebugString(expr));
  }
  return ValidateLinearExpression(model, expr);
}

std::string ValidateLinearConstraint(const CpModelProto& model,
                                     const ConstraintProto& ct) {
  if (!DomainInProtoIsValid(ct.linear())) {
    return absl::StrCat("Invalid domain in constraint : ",
                        ProtobufShortDebugString(ct));
  }
  if (ct.linear().coeffs_size() != ct.linear().vars_size()) {
    return absl::StrCat("coeffs_size() != vars_size() in constraint: ",
                        ProtobufShortDebugString(ct));
  }
  for (const int var : ct.linear().vars()) {
    if (!RefIsPositive(var)) {
      return absl::StrCat("Invalid negated variable in linear constraint: ",
                          ProtobufShortDebugString(ct));
    }
  }
  const LinearConstraintProto& arg = ct.linear();
  if (PossibleIntegerOverflow(model, arg.vars(), arg.coeffs())) {
    return "Possible integer overflow in constraint: " +
           ProtobufDebugString(ct);
  }
  return "";
}

std::string ValidateIntModConstraint(const CpModelProto& model,
                                     const ConstraintProto& ct) {
  if (ct.int_mod().exprs().size() != 2) {
    return absl::StrCat("An int_mod constraint should have exactly 2 terms: ",
                        ProtobufShortDebugString(ct));
  }
  if (!ct.int_mod().has_target()) {
    return absl::StrCat("An int_mod constraint should have a target: ",
                        ProtobufShortDebugString(ct));
  }

  RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, ct.int_mod().exprs(0)));
  RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, ct.int_mod().exprs(1)));
  RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, ct.int_mod().target()));

  const LinearExpressionProto mod_expr = ct.int_mod().exprs(1);
  if (MinOfExpression(model, mod_expr) <= 0) {
    return absl::StrCat(
        "An int_mod must have a strictly positive modulo argument: ",
        ProtobufShortDebugString(ct));
  }

  return "";
}

std::string ValidateIntProdConstraint(const CpModelProto& model,
                                      const ConstraintProto& ct) {
  if (!ct.int_prod().has_target()) {
    return absl::StrCat("An int_prod constraint should have a target: ",
                        ProtobufShortDebugString(ct));
  }

  for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
    RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, expr));
  }
  RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, ct.int_prod().target()));

  // Detect potential overflow.
  Domain product_domain(1);
  for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
    const int64_t min_expr = MinOfExpression(model, expr);
    const int64_t max_expr = MaxOfExpression(model, expr);
    if (min_expr == 0 && max_expr == 0) {
      // An overflow multiplied by zero is still invalid.
      continue;
    }
    product_domain =
        product_domain.ContinuousMultiplicationBy({min_expr, max_expr});
  }

  if (product_domain.Max() <= -std ::numeric_limits<int64_t>::max() ||
      product_domain.Min() >= std::numeric_limits<int64_t>::max()) {
    return absl::StrCat("integer overflow in constraint: ",
                        ProtobufShortDebugString(ct));
  }

  // We need to expand the product when its arity is > 2. In that case, we must
  // be strict with overflows.
  if (ct.int_prod().exprs_size() > 2 &&
      (product_domain.Max() >= std ::numeric_limits<int64_t>::max() ||
       product_domain.Min() <= -std::numeric_limits<int64_t>::max())) {
    return absl::StrCat("Potential integer overflow in constraint: ",
                        ProtobufShortDebugString(ct));
  }

  return "";
}

std::string ValidateIntDivConstraint(const CpModelProto& model,
                                     const ConstraintProto& ct) {
  if (ct.int_div().exprs().size() != 2) {
    return absl::StrCat("An int_div constraint should have exactly 2 terms: ",
                        ProtobufShortDebugString(ct));
  }
  if (!ct.int_div().has_target()) {
    return absl::StrCat("An int_div constraint should have a target: ",
                        ProtobufShortDebugString(ct));
  }

  RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, ct.int_div().exprs(0)));
  RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, ct.int_div().exprs(1)));
  RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, ct.int_div().target()));

  const LinearExpressionProto& denom = ct.int_div().exprs(1);
  const int64_t offset = denom.offset();
  if (ExpressionIsFixed(model, denom)) {
    if (ExpressionFixedValue(model, denom) == 0) {
      return absl::StrCat("Division by 0: ", ProtobufShortDebugString(ct));
    }
  } else {
    const int64_t coeff = denom.coeffs(0);
    CHECK_NE(coeff, 0);
    const int64_t inverse_of_zero = -offset / coeff;
    if (inverse_of_zero * coeff + offset == 0 &&
        DomainOfRef(model, denom.vars(0)).Contains(inverse_of_zero)) {
      return absl::StrCat("The domain of the divisor cannot contain 0: ",
                          ProtobufShortDebugString(ct));
    }
  }
  return "";
}

void AppendToOverflowValidator(const LinearExpressionProto& input,
                               LinearExpressionProto* output,
                               int64_t prod = 1) {
  output->mutable_vars()->Add(input.vars().begin(), input.vars().end());
  for (const int64_t coeff : input.coeffs()) {
    output->add_coeffs(coeff * prod);
  }

  // We add the absolute value to be sure that future computation will not
  // overflow depending on the order they are performed in.
  output->set_offset(
      CapAdd(std::abs(output->offset()), std::abs(input.offset())));
}

std::string ValidateElementConstraint(const CpModelProto& model,
                                      const ConstraintProto& ct) {
  const ElementConstraintProto& element = ct.element();

  const bool in_linear_format = element.has_linear_index() ||
                                element.has_linear_target() ||
                                !element.exprs().empty();
  const bool in_legacy_format =
      !element.vars().empty() || element.index() != 0 || element.target() != 0;
  if (in_linear_format && in_legacy_format) {
    return absl::StrCat(
        "Inconsistent element with both legacy and new format defined",
        ProtobufShortDebugString(ct));
  }

  if (element.vars().empty() && element.exprs().empty()) {
    return "Empty element constraint is interpreted as vars[], thus invalid "
           "since the index will be out of bounds.";
  }

  // We need to be able to manipulate expression like "target - var" without
  // integer overflow.
  if (!element.vars().empty()) {
    LinearExpressionProto overflow_detection;
    overflow_detection.add_vars(element.target());
    overflow_detection.add_coeffs(1);
    overflow_detection.add_vars(/*dummy*/ 0);
    overflow_detection.add_coeffs(-1);
    for (const int ref : element.vars()) {
      if (!VariableIndexIsValid(model, ref)) {
        return absl::StrCat("Element vars must be valid variables: ",
                            ProtobufShortDebugString(ct));
      }
      overflow_detection.set_vars(1, ref);
      if (PossibleIntegerOverflow(model, overflow_detection.vars(),
                                  overflow_detection.coeffs())) {
        return absl::StrCat(
            "Domain of the variables involved in element constraint may cause "
            "overflow",
            ProtobufShortDebugString(ct));
      }
    }
  }

  if (in_legacy_format) {
    if (!VariableIndexIsValid(model, element.index()) ||
        !VariableIndexIsValid(model, element.target())) {
      return absl::StrCat(
          "Element constraint index and target must valid variables: ",
          ProtobufShortDebugString(ct));
    }
  }

  if (in_linear_format) {
    RETURN_IF_NOT_EMPTY(
        ValidateAffineExpression(model, element.linear_index()));
    RETURN_IF_NOT_EMPTY(
        ValidateAffineExpression(model, element.linear_target()));
    for (const LinearExpressionProto& expr : element.exprs()) {
      RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, expr));
      LinearExpressionProto overflow_detection = ct.element().linear_target();
      AppendToOverflowValidator(expr, &overflow_detection, -1);
      const int64_t offset = CapSub(overflow_detection.offset(), expr.offset());
      overflow_detection.set_offset(offset);
      if (PossibleIntegerOverflow(model, overflow_detection.vars(),
                                  overflow_detection.coeffs(),
                                  overflow_detection.offset())) {
        return absl::StrCat(
            "Domain of the variables involved in element constraint may cause "
            "overflow");
      }
    }
  }

  return "";
}

std::string ValidateTableConstraint(const CpModelProto& model,
                                    const ConstraintProto& ct) {
  const TableConstraintProto& arg = ct.table();
  if (!arg.vars().empty() && !arg.exprs().empty()) {
    return absl::StrCat(
        "Inconsistent table with both legacy and new format defined: ",
        ProtobufShortDebugString(ct));
  }
  if (arg.vars().empty() && arg.exprs().empty() && !arg.values().empty()) {
    return absl::StrCat(
        "Inconsistent table empty expressions and non-empty tuples: ",
        ProtobufShortDebugString(ct));
  }
  if (arg.vars().empty() && arg.exprs().empty() && arg.values().empty()) {
    return "";
  }
  const int arity = arg.vars().empty() ? arg.exprs().size() : arg.vars().size();
  if (arg.values().size() % arity != 0) {
    return absl::StrCat(
        "The flat encoding of a table constraint tuples must be a multiple of "
        "the number of expressions: ",
        ProtobufDebugString(ct));
  }
  for (const int var : arg.vars()) {
    if (!VariableIndexIsValid(model, var)) {
      return absl::StrCat("Invalid variable index in table constraint: ", var);
    }
  }
  for (const LinearExpressionProto& expr : arg.exprs()) {
    RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, expr));
  }
  return "";
}

std::string ValidateAutomatonConstraint(const CpModelProto& model,
                                        const ConstraintProto& ct) {
  const AutomatonConstraintProto& automaton = ct.automaton();
  if (!automaton.vars().empty() && !automaton.exprs().empty()) {
    return absl::StrCat(
        "Inconsistent automaton with both legacy and new format defined: ",
        ProtobufShortDebugString(ct));
  }
  const int num_transistions = automaton.transition_tail().size();
  if (num_transistions != automaton.transition_head().size() ||
      num_transistions != automaton.transition_label().size()) {
    return absl::StrCat(
        "The transitions repeated fields must have the same size: ",
        ProtobufShortDebugString(ct));
  }
  for (const int var : automaton.vars()) {
    if (!VariableIndexIsValid(model, var)) {
      return absl::StrCat("Invalid variable index in automaton constraint: ",
                          var);
    }
  }
  for (const LinearExpressionProto& expr : automaton.exprs()) {
    RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, expr));
  }
  absl::flat_hash_map<std::pair<int64_t, int64_t>, int64_t> tail_label_to_head;
  for (int i = 0; i < num_transistions; ++i) {
    const int64_t tail = automaton.transition_tail(i);
    const int64_t head = automaton.transition_head(i);
    const int64_t label = automaton.transition_label(i);
    if (label <= std::numeric_limits<int64_t>::min() + 1 ||
        label == std::numeric_limits<int64_t>::max()) {
      return absl::StrCat("labels in the automaton constraint are too big: ",
                          label);
    }
    const auto [it, inserted] =
        tail_label_to_head.insert({{tail, label}, head});
    if (!inserted) {
      if (it->second == head) {
        return absl::StrCat("automaton: duplicate transition ", tail, " --(",
                            label, ")--> ", head);
      } else {
        return absl::StrCat("automaton: incompatible transitions ", tail,
                            " --(", label, ")--> ", head, " and ", tail, " --(",
                            label, ")--> ", it->second);
      }
    }
  }
  return "";
}

template <typename GraphProto>
std::string ValidateGraphInput(bool is_route, const GraphProto& graph) {
  const int size = graph.tails().size();
  if (graph.heads().size() != size || graph.literals().size() != size) {
    return absl::StrCat("Wrong field sizes in graph: ",
                        ProtobufShortDebugString(graph));
  }

  // We currently disallow multiple self-loop on the same node.
  absl::flat_hash_set<int> self_loops;
  for (int i = 0; i < size; ++i) {
    if (graph.heads(i) != graph.tails(i)) continue;
    if (!self_loops.insert(graph.heads(i)).second) {
      return absl::StrCat(
          "Circuit/Route constraint contains multiple self-loop involving "
          "node ",
          graph.heads(i));
    }
    if (is_route && graph.tails(i) == 0) {
      return absl::StrCat(
          "A route constraint cannot have a self-loop on the depot (node 0)");
    }
  }

  return "";
}

std::string ValidateRoutesConstraint(const CpModelProto& model,
                                     const ConstraintProto& ct) {
  int max_node = 0;
  absl::flat_hash_set<int> nodes;
  for (const int node : ct.routes().tails()) {
    if (node < 0) {
      return "All node in a route constraint must be in [0, num_nodes)";
    }
    nodes.insert(node);
    max_node = std::max(max_node, node);
  }
  for (const int node : ct.routes().heads()) {
    if (node < 0) {
      return "All node in a route constraint must be in [0, num_nodes)";
    }
    nodes.insert(node);
    max_node = std::max(max_node, node);
  }
  if (!nodes.empty() && max_node != nodes.size() - 1) {
    return absl::StrCat(
        "All nodes in a route constraint must have incident arcs");
  }

  for (const RoutesConstraintProto::NodeExpressions& dimension :
       ct.routes().dimensions()) {
    if (dimension.exprs().size() != nodes.size()) {
      return absl::StrCat(
          "If the dimensions field in a route constraint is set, its elements "
          "must be of size num_nodes:",
          nodes.size());
    }
    for (const LinearExpressionProto& expr : dimension.exprs()) {
      for (const int v : expr.vars()) {
        if (!VariableReferenceIsValid(model, v)) {
          return absl::StrCat("Out of bound integer variable ", v,
                              " in route constraint ",
                              ProtobufShortDebugString(ct));
        }
      }
      RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, expr));
    }
  }

  return ValidateGraphInput(/*is_route=*/true, ct.routes());
}

std::string ValidateIntervalConstraint(const CpModelProto& model,
                                       const ConstraintProto& ct) {
  if (ct.enforcement_literal().size() > 1) {
    return absl::StrCat(
        "Interval with more than one enforcement literals are currently not "
        "supported: ",
        ProtobufShortDebugString(ct));
  }
  const IntervalConstraintProto& arg = ct.interval();

  if (!arg.has_start()) {
    return absl::StrCat("Interval must have a start expression: ",
                        ProtobufShortDebugString(ct));
  }
  if (!arg.has_size()) {
    return absl::StrCat("Interval must have a size expression: ",
                        ProtobufShortDebugString(ct));
  }
  if (!arg.has_end()) {
    return absl::StrCat("Interval must have a end expression: ",
                        ProtobufShortDebugString(ct));
  }

  LinearExpressionProto for_overflow_validation;
  if (arg.start().vars_size() > 1) {
    return "Interval with a start expression containing more than one "
           "variable are currently not supported.";
  }
  RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, arg.start()));
  AppendToOverflowValidator(arg.start(), &for_overflow_validation);
  if (arg.size().vars_size() > 1) {
    return "Interval with a size expression containing more than one "
           "variable are currently not supported.";
  }
  RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, arg.size()));
  if (ct.enforcement_literal().empty() &&
      MinOfExpression(model, arg.size()) < 0) {
    return absl::StrCat(
        "The size of a performed interval must be >= 0 in constraint: ",
        ProtobufDebugString(ct));
  }
  AppendToOverflowValidator(arg.size(), &for_overflow_validation);
  if (arg.end().vars_size() > 1) {
    return "Interval with a end expression containing more than one "
           "variable are currently not supported.";
  }
  RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, arg.end()));
  AppendToOverflowValidator(arg.end(), &for_overflow_validation, -1);

  if (PossibleIntegerOverflow(model, for_overflow_validation.vars(),
                              for_overflow_validation.coeffs(),
                              for_overflow_validation.offset())) {
    return absl::StrCat("Possible overflow in interval: ",
                        ProtobufShortDebugString(ct.interval()));
  }

  return "";
}

std::string ValidateCumulativeConstraint(const CpModelProto& model,
                                         const ConstraintProto& ct) {
  if (ct.cumulative().intervals_size() != ct.cumulative().demands_size()) {
    return absl::StrCat("intervals_size() != demands_size() in constraint: ",
                        ProtobufShortDebugString(ct));
  }

  RETURN_IF_NOT_EMPTY(
      ValidateLinearExpression(model, ct.cumulative().capacity()));
  for (const LinearExpressionProto& demand : ct.cumulative().demands()) {
    RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, demand));
  }

  for (const LinearExpressionProto& demand_expr : ct.cumulative().demands()) {
    if (MinOfExpression(model, demand_expr) < 0) {
      return absl::StrCat(
          "Demand ", ProtobufDebugString(demand_expr),
          " must be positive in constraint: ", ProtobufDebugString(ct));
    }
    if (demand_expr.vars_size() > 1) {
      return absl::StrCat("Demand ", ProtobufDebugString(demand_expr),
                          " must be affine or constant in constraint: ",
                          ProtobufDebugString(ct));
    }
  }
  if (ct.cumulative().capacity().vars_size() > 1) {
    return absl::StrCat(
        "capacity ", ProtobufDebugString(ct.cumulative().capacity()),
        " must be affine or constant in constraint: ", ProtobufDebugString(ct));
  }

  int64_t sum_max_demands = 0;
  for (const LinearExpressionProto& demand_expr : ct.cumulative().demands()) {
    const int64_t demand_max = MaxOfExpression(model, demand_expr);
    DCHECK_GE(demand_max, 0);
    sum_max_demands = CapAdd(sum_max_demands, demand_max);
    if (sum_max_demands == std::numeric_limits<int64_t>::max()) {
      return "The sum of max demands do not fit on an int64_t in constraint: " +
             ProtobufDebugString(ct);
    }
  }

  return "";
}

std::string ValidateNoOverlap2DConstraint(const CpModelProto& model,
                                          const ConstraintProto& ct) {
  const int size_x = ct.no_overlap_2d().x_intervals().size();
  const int size_y = ct.no_overlap_2d().y_intervals().size();
  if (size_x != size_y) {
    return absl::StrCat("The two lists of intervals must have the same size: ",
                        ProtobufShortDebugString(ct));
  }

  // Checks if the sum of max areas of each rectangle can overflow.
  int64_t sum_max_areas = 0;
  for (int i = 0; i < ct.no_overlap_2d().x_intervals().size(); ++i) {
    const int64_t max_size_x =
        IntervalSizeMax(model, ct.no_overlap_2d().x_intervals(i));
    const int64_t max_size_y =
        IntervalSizeMax(model, ct.no_overlap_2d().y_intervals(i));
    sum_max_areas = CapAdd(sum_max_areas, CapProd(max_size_x, max_size_y));
    if (sum_max_areas == std::numeric_limits<int64_t>::max()) {
      return "Integer overflow when summing all areas in "
             "constraint: " +
             ProtobufDebugString(ct);
    }
  }
  return "";
}

std::string ValidateReservoirConstraint(const CpModelProto& model,
                                        const ConstraintProto& ct) {
  if (ct.enforcement_literal_size() > 0) {
    return "Reservoir does not support enforcement literals.";
  }
  if (ct.reservoir().time_exprs().size() !=
      ct.reservoir().level_changes().size()) {
    return absl::StrCat(
        "time_exprs and level_changes fields must be of the same size: ",
        ProtobufShortDebugString(ct));
  }
  for (const LinearExpressionProto& expr : ct.reservoir().time_exprs()) {
    RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, expr));
    // We want to be able to safely put time_exprs[i]-time_exprs[j] in a linear.
    if (MinOfExpression(model, expr) <=
            -std::numeric_limits<int64_t>::max() / 4 ||
        MaxOfExpression(model, expr) >=
            std::numeric_limits<int64_t>::max() / 4) {
      return absl::StrCat(
          "Potential integer overflow on time_expr of a reservoir: ",
          ProtobufShortDebugString(ct));
    }
  }
  for (const LinearExpressionProto& expr : ct.reservoir().level_changes()) {
    RETURN_IF_NOT_EMPTY(ValidateConstantAffineExpression(model, expr));
  }
  if (ct.reservoir().min_level() > 0) {
    return absl::StrCat(
        "The min level of a reservoir must be <= 0. Please use fixed events to "
        "setup initial state: ",
        ProtobufShortDebugString(ct));
  }
  if (ct.reservoir().max_level() < 0) {
    return absl::StrCat(
        "The max level of a reservoir must be >= 0. Please use fixed events to "
        "setup initial state: ",
        ProtobufShortDebugString(ct));
  }

  int64_t sum_abs = 0;
  for (const LinearExpressionProto& demand : ct.reservoir().level_changes()) {
    // We test for min int64_t before the abs().
    const int64_t demand_min = MinOfExpression(model, demand);
    const int64_t demand_max = MaxOfExpression(model, demand);
    sum_abs = CapAdd(sum_abs, std::max(CapAbs(demand_min), CapAbs(demand_max)));
    if (sum_abs == std::numeric_limits<int64_t>::max()) {
      return "Possible integer overflow in constraint: " +
             ProtobufDebugString(ct);
    }
  }
  if (ct.reservoir().active_literals_size() > 0 &&
      ct.reservoir().active_literals_size() !=
          ct.reservoir().time_exprs_size()) {
    return "Wrong array length of active_literals variables";
  }
  if (ct.reservoir().level_changes_size() > 0 &&
      ct.reservoir().level_changes_size() != ct.reservoir().time_exprs_size()) {
    return "Wrong array length of level_changes variables";
  }
  return "";
}

std::string ValidateObjective(const CpModelProto& model,
                              const CpObjectiveProto& obj) {
  if (!DomainInProtoIsValid(obj)) {
    return absl::StrCat("The objective has and invalid domain() format: ",
                        ProtobufShortDebugString(obj));
  }
  if (obj.vars().size() != obj.coeffs().size()) {
    return absl::StrCat("vars and coeffs size do not match in objective: ",
                        ProtobufShortDebugString(obj));
  }
  for (const int v : obj.vars()) {
    if (!VariableIndexIsValid(model, v)) {
      return absl::StrCat("Out of bound integer variable ", v,
                          " in objective: ", ProtobufShortDebugString(obj));
    }
  }
  std::pair<int64_t, int64_t> bounds;
  if (PossibleIntegerOverflow(model, obj.vars(), obj.coeffs(), 0, &bounds)) {
    return "Possible integer overflow in objective: " +
           ProtobufDebugString(obj);
  }
  if (!std::isfinite(model.objective().offset())) {
    return "Objective offset must be finite: " + ProtobufDebugString(obj);
  }
  if (model.objective().scaling_factor() != 0 &&
      model.objective().scaling_factor() != 1 &&
      model.objective().scaling_factor() != -1) {
    if (!std::isfinite(
            std::abs(model.objective().scaling_factor() * bounds.first) +
            std::abs(model.objective().offset())) ||
        !std::isfinite(
            std::abs(model.objective().scaling_factor() * bounds.second) +
            std::abs(model.objective().offset()))) {
      return "Possible floating point overflow in objective when multiplied by "
             "the scaling factor: " +
             ProtobufDebugString(obj);
    }
  }
  return "";
}

std::string ValidateFloatingPointObjective(double max_valid_magnitude,
                                           const CpModelProto& model,
                                           const FloatObjectiveProto& obj) {
  if (obj.vars().size() != obj.coeffs().size()) {
    return absl::StrCat("vars and coeffs size do not match in objective: ",
                        ProtobufShortDebugString(obj));
  }
  for (const int v : obj.vars()) {
    if (!VariableIndexIsValid(model, v)) {
      return absl::StrCat("Out of bound integer variable ", v,
                          " in objective: ", ProtobufShortDebugString(obj));
    }
  }
  for (const double coeff : obj.coeffs()) {
    if (!std::isfinite(coeff)) {
      return absl::StrCat("Coefficients must be finite in objective: ",
                          ProtobufShortDebugString(obj));
    }
    if (std::abs(coeff) > max_valid_magnitude) {
      return absl::StrCat(
          "Coefficients larger than params.mip_max_valid_magnitude() [value = ",
          max_valid_magnitude,
          "] in objective: ", ProtobufShortDebugString(obj));
    }
  }
  if (!std::isfinite(obj.offset())) {
    return absl::StrCat("Offset must be finite in objective: ",
                        ProtobufShortDebugString(obj));
  }
  double sum_min = obj.offset();
  double sum_max = obj.offset();
  for (int i = 0; i < obj.vars().size(); ++i) {
    const int ref = obj.vars(i);
    const auto& var_proto = model.variables(PositiveRef(ref));
    const int64_t min_domain = var_proto.domain(0);
    const int64_t max_domain = var_proto.domain(var_proto.domain_size() - 1);
    const double coeff = RefIsPositive(ref) ? obj.coeffs(i) : -obj.coeffs(i);
    const double prod1 = min_domain * coeff;
    const double prod2 = max_domain * coeff;

    // Note that we use min/max with zero to disallow "alternative" terms and
    // be sure that we cannot have an overflow if we do the computation in a
    // different order.
    sum_min += std::min(0.0, std::min(prod1, prod2));
    sum_max += std::max(0.0, std::max(prod1, prod2));
  }
  if (!std::isfinite(2.0 * sum_min) || !std::isfinite(2.0 * sum_max)) {
    return absl::StrCat("Possible floating point overflow in objective: ",
                        ProtobufShortDebugString(obj));
  }
  return "";
}

std::string ValidateSearchStrategies(const CpModelProto& model) {
  for (const DecisionStrategyProto& strategy : model.search_strategy()) {
    const int vss = strategy.variable_selection_strategy();
    if (vss != DecisionStrategyProto::CHOOSE_FIRST &&
        vss != DecisionStrategyProto::CHOOSE_LOWEST_MIN &&
        vss != DecisionStrategyProto::CHOOSE_HIGHEST_MAX &&
        vss != DecisionStrategyProto::CHOOSE_MIN_DOMAIN_SIZE &&
        vss != DecisionStrategyProto::CHOOSE_MAX_DOMAIN_SIZE) {
      return absl::StrCat(
          "Unknown or unsupported variable_selection_strategy: ", vss);
    }
    const int drs = strategy.domain_reduction_strategy();
    if (drs != DecisionStrategyProto::SELECT_MIN_VALUE &&
        drs != DecisionStrategyProto::SELECT_MAX_VALUE &&
        drs != DecisionStrategyProto::SELECT_LOWER_HALF &&
        drs != DecisionStrategyProto::SELECT_UPPER_HALF &&
        drs != DecisionStrategyProto::SELECT_MEDIAN_VALUE &&
        drs != DecisionStrategyProto::SELECT_RANDOM_HALF) {
      return absl::StrCat("Unknown or unsupported domain_reduction_strategy: ",
                          drs);
    }
    if (!strategy.variables().empty() && !strategy.exprs().empty()) {
      return absl::StrCat("Strategy can't have both variables and exprs: ",
                          ProtobufShortDebugString(strategy));
    }
    for (const int ref : strategy.variables()) {
      if (!VariableReferenceIsValid(model, ref)) {
        return absl::StrCat("Invalid variable reference in strategy: ",
                            ProtobufShortDebugString(strategy));
      }
      if (drs == DecisionStrategyProto::SELECT_MEDIAN_VALUE &&
          ReadDomainFromProto(model.variables(PositiveRef(ref))).Size() >
              100000) {
        return absl::StrCat("Variable #", PositiveRef(ref),
                            " has a domain too large to be used in a"
                            " SELECT_MEDIAN_VALUE value selection strategy");
      }
    }
    for (const LinearExpressionProto& expr : strategy.exprs()) {
      for (const int var : expr.vars()) {
        if (!VariableReferenceIsValid(model, var)) {
          return absl::StrCat("Invalid variable reference in strategy: ",
                              ProtobufShortDebugString(strategy));
        }
      }
      if (!ValidateAffineExpression(model, expr).empty()) {
        return absl::StrCat("Invalid affine expr in strategy: ",
                            ProtobufShortDebugString(strategy));
      }
      if (drs == DecisionStrategyProto::SELECT_MEDIAN_VALUE) {
        for (const int var : expr.vars()) {
          if (ReadDomainFromProto(model.variables(var)).Size() > 100000) {
            return absl::StrCat(
                "Variable #", var,
                " has a domain too large to be used in a"
                " SELECT_MEDIAN_VALUE value selection strategy");
          }
        }
      }
    }
  }
  return "";
}

std::string ValidateSolutionHint(const CpModelProto& model) {
  if (!model.has_solution_hint()) return "";
  const auto& hint = model.solution_hint();
  if (hint.vars().size() != hint.values().size()) {
    return "Invalid solution hint: vars and values do not have the same size.";
  }
  for (const int var : hint.vars()) {
    if (!VariableIndexIsValid(model, var)) {
      return absl::StrCat("Invalid variable in solution hint: ", var);
    }
  }

  // Reject hints with duplicate variables as this is likely a user error.
  absl::flat_hash_set<int> indices;
  for (const int var : hint.vars()) {
    const auto insert = indices.insert(PositiveRef(var));
    if (!insert.second) {
      return absl::StrCat(
          "The solution hint contains duplicate variables like the variable "
          "with index #",
          PositiveRef(var));
    }
  }

  // Reject hints equals to INT_MIN or INT_MAX.
  for (const int64_t value : hint.values()) {
    if (value == std::numeric_limits<int64_t>::min() ||
        value == std::numeric_limits<int64_t>::max()) {
      return "The solution hint cannot contains the INT_MIN or INT_MAX values.";
    }
  }

  return "";
}

}  // namespace

bool PossibleIntegerOverflow(const CpModelProto& model,
                             absl::Span<const int> vars,
                             absl::Span<const int64_t> coeffs, int64_t offset,
                             std::pair<int64_t, int64_t>* implied_domain) {
  if (offset == std::numeric_limits<int64_t>::min()) return true;
  int64_t sum_min = -std::abs(offset);
  int64_t sum_max = +std::abs(offset);
  for (int i = 0; i < vars.size(); ++i) {
    const int ref = vars[i];
    const auto& var_proto = model.variables(PositiveRef(ref));
    const int64_t min_domain = var_proto.domain(0);
    const int64_t max_domain = var_proto.domain(var_proto.domain_size() - 1);
    if (coeffs[i] == std::numeric_limits<int64_t>::min()) return true;
    const int64_t coeff = RefIsPositive(ref) ? coeffs[i] : -coeffs[i];
    const int64_t prod1 = CapProd(min_domain, coeff);
    const int64_t prod2 = CapProd(max_domain, coeff);

    // Note that we use min/max with zero to disallow "alternative" terms and
    // be sure that we cannot have an overflow if we do the computation in a
    // different order.
    sum_min = CapAdd(sum_min, std::min(int64_t{0}, std::min(prod1, prod2)));
    sum_max = CapAdd(sum_max, std::max(int64_t{0}, std::max(prod1, prod2)));
    for (const int64_t v : {prod1, prod2, sum_min, sum_max}) {
      if (AtMinOrMaxInt64(v)) return true;
    }
  }

  // In addition to computing the min/max possible sum, we also often compare
  // it with the constraint bounds, so we do not want max - min to overflow.
  // We might also create an intermediate variable to represent the sum.
  //
  // Note that it is important to be symmetric here, as we do not want expr to
  // pass but not -expr!
  if (sum_min < -std::numeric_limits<int64_t>::max() / 2) return true;
  if (sum_max > std::numeric_limits<int64_t>::max() / 2) return true;
  if (implied_domain) {
    *implied_domain = {sum_min, sum_max};
  }
  return false;
}

std::string ValidateCpModel(const CpModelProto& model, bool after_presolve) {
  int64_t int128_overflow = 0;
  for (int v = 0; v < model.variables_size(); ++v) {
    RETURN_IF_NOT_EMPTY(ValidateIntegerVariable(model, v));

    const auto& domain = model.variables(v).domain();
    const int64_t min = domain[0];
    const int64_t max = domain[domain.size() - 1];
    int128_overflow = CapAdd(
        int128_overflow, std::max({std::abs(min), std::abs(max), max - min}));
  }

  // We require this precondition so that we can take any linear combination of
  // variable with coefficient in int64_t and compute the activity on an int128
  // with no overflow. This is useful during cut computation.
  if (int128_overflow == std::numeric_limits<int64_t>::max()) {
    return "The sum of all variable domains do not fit on an int64_t. This is "
           "needed to prevent overflows.";
  }

  // We need to validate the intervals used first, so we add these constraints
  // here so that we can validate them in a second pass.
  std::vector<int> constraints_using_intervals;

  for (int c = 0; c < model.constraints_size(); ++c) {
    RETURN_IF_NOT_EMPTY(ValidateVariablesUsedInConstraint(model, c));

    // By default, a constraint does not support enforcement literals except if
    // explicitly stated by setting this to true below.
    bool support_enforcement = false;

    // Other non-generic validations.
    const ConstraintProto& ct = model.constraints(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kAtMostOne:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kExactlyOne:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kBoolXor:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kLinear:
        support_enforcement = true;
        RETURN_IF_NOT_EMPTY(ValidateLinearConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kLinMax: {
        RETURN_IF_NOT_EMPTY(
            ValidateLinearExpression(model, ct.lin_max().target()));
        for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
          RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, expr));
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kIntProd:
        support_enforcement = true;
        RETURN_IF_NOT_EMPTY(ValidateIntProdConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kIntDiv:
        support_enforcement = true;
        RETURN_IF_NOT_EMPTY(ValidateIntDivConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kIntMod:
        support_enforcement = true;
        RETURN_IF_NOT_EMPTY(ValidateIntModConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        if (ct.inverse().f_direct().size() != ct.inverse().f_inverse().size()) {
          return absl::StrCat("Non-matching fields size in inverse: ",
                              ProtobufShortDebugString(ct));
        }
        break;
      case ConstraintProto::ConstraintCase::kAllDiff:
        for (const LinearExpressionProto& expr : ct.all_diff().exprs()) {
          RETURN_IF_NOT_EMPTY(ValidateAffineExpression(model, expr));
        }
        break;
      case ConstraintProto::ConstraintCase::kElement:
        RETURN_IF_NOT_EMPTY(ValidateElementConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kTable:
        RETURN_IF_NOT_EMPTY(ValidateTableConstraint(model, ct));
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kAutomaton:
        RETURN_IF_NOT_EMPTY(ValidateAutomatonConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kCircuit:
        RETURN_IF_NOT_EMPTY(
            ValidateGraphInput(/*is_route=*/false, ct.circuit()));
        break;
      case ConstraintProto::ConstraintCase::kRoutes:
        RETURN_IF_NOT_EMPTY(ValidateRoutesConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kInterval:
        RETURN_IF_NOT_EMPTY(ValidateIntervalConstraint(model, ct));
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        constraints_using_intervals.push_back(c);
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap:
        constraints_using_intervals.push_back(c);
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        constraints_using_intervals.push_back(c);
        break;
      case ConstraintProto::ConstraintCase::kReservoir:
        RETURN_IF_NOT_EMPTY(ValidateReservoirConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kDummyConstraint:
        return "The dummy constraint should never appear in a model.";
      default:
        break;
    }

    // Because some client set fixed enforcement literal which are supported
    // in the presolve for all constraints, we just check that there is no
    // non-fixed enforcement.
    if (!support_enforcement && !ct.enforcement_literal().empty()) {
      for (const int ref : ct.enforcement_literal()) {
        const int var = PositiveRef(ref);
        const Domain domain = ReadDomainFromProto(model.variables(var));
        if (domain.Size() != 1) {
          return absl::StrCat(
              "Enforcement literal not supported in constraint: ",
              ProtobufShortDebugString(ct));
        }
      }
    }
  }

  // Extra validation for constraint using intervals.
  for (const int c : constraints_using_intervals) {
    RETURN_IF_NOT_EMPTY(
        ValidateIntervalsUsedInConstraint(after_presolve, model, c));

    const ConstraintProto& ct = model.constraints(c);
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kCumulative:
        RETURN_IF_NOT_EMPTY(ValidateCumulativeConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap:
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        RETURN_IF_NOT_EMPTY(ValidateNoOverlap2DConstraint(model, ct));
        break;
      default:
        LOG(DFATAL) << "Shouldn't be here";
    }
  }

  if (model.has_objective() && model.has_floating_point_objective()) {
    return "A model cannot have both an objective and a floating point "
           "objective.";
  }
  if (model.has_objective()) {
    if (model.objective().scaling_factor() != 0 &&
        !std::isnormal(model.objective().scaling_factor())) {
      return "A model cannot have an objective with a nan, inf or subnormal "
             "scaling factor";
    }

    RETURN_IF_NOT_EMPTY(ValidateObjective(model, model.objective()));

    if (model.objective().integer_scaling_factor() != 0 ||
        model.objective().integer_before_offset() != 0 ||
        model.objective().integer_after_offset() != 0) {
      // If any of these fields are set, the domain must be set.
      if (model.objective().domain().empty()) {
        return absl::StrCat(
            "Objective integer scaling or offset is set without an objective "
            "domain.");
      }

      // Check that we can transform any value in the objective domain without
      // overflow. We only check the bounds which is enough.
      bool overflow = false;
      for (const int64_t v : model.objective().domain()) {
        int64_t t = CapAdd(v, model.objective().integer_before_offset());
        if (AtMinOrMaxInt64(t)) {
          overflow = true;
          break;
        }
        t = CapProd(t, model.objective().integer_scaling_factor());
        if (AtMinOrMaxInt64(t)) {
          overflow = true;
          break;
        }
        t = CapAdd(t, model.objective().integer_after_offset());
        if (AtMinOrMaxInt64(t)) {
          overflow = true;
          break;
        }
      }
      if (overflow) {
        return absl::StrCat(
            "Internal fields related to the postsolve of the integer objective "
            "are causing a potential integer overflow: ",
            ProtobufShortDebugString(model.objective()));
      }
    }
  }
  RETURN_IF_NOT_EMPTY(ValidateSearchStrategies(model));
  RETURN_IF_NOT_EMPTY(ValidateSolutionHint(model));
  for (const int ref : model.assumptions()) {
    if (!LiteralReferenceIsValid(model, ref)) {
      return absl::StrCat("Invalid literal reference ", ref,
                          " in the 'assumptions' field.");
    }
  }
  return "";
}

std::string ValidateInputCpModel(const SatParameters& params,
                                 const CpModelProto& model) {
  RETURN_IF_NOT_EMPTY(ValidateCpModel(model));
  if (model.has_floating_point_objective()) {
    RETURN_IF_NOT_EMPTY(
        ValidateFloatingPointObjective(params.mip_max_valid_magnitude(), model,
                                       model.floating_point_objective()));
  }
  return "";
}

#undef RETURN_IF_NOT_EMPTY

// =============================================================================
// Solution Feasibility.
// =============================================================================

namespace {

class ConstraintChecker {
 public:
  explicit ConstraintChecker(absl::Span<const int64_t> variable_values)
      : variable_values_(variable_values.begin(), variable_values.end()) {}

  bool LiteralIsTrue(int l) const {
    if (l >= 0) return variable_values_[l] != 0;
    return variable_values_[-l - 1] == 0;
  }

  bool LiteralIsFalse(int l) const { return !LiteralIsTrue(l); }

  int64_t Value(int var) const {
    if (var >= 0) return variable_values_[var];
    return -variable_values_[-var - 1];
  }

  bool ConstraintIsEnforced(const ConstraintProto& ct) {
    for (const int lit : ct.enforcement_literal()) {
      if (LiteralIsFalse(lit)) return false;
    }
    return true;
  }

  bool BoolOrConstraintIsFeasible(const ConstraintProto& ct) {
    for (const int lit : ct.bool_or().literals()) {
      if (LiteralIsTrue(lit)) return true;
    }
    return false;
  }

  bool BoolAndConstraintIsFeasible(const ConstraintProto& ct) {
    for (const int lit : ct.bool_and().literals()) {
      if (LiteralIsFalse(lit)) return false;
    }
    return true;
  }

  bool AtMostOneConstraintIsFeasible(const ConstraintProto& ct) {
    int num_true_literals = 0;
    for (const int lit : ct.at_most_one().literals()) {
      if (LiteralIsTrue(lit)) ++num_true_literals;
    }
    return num_true_literals <= 1;
  }

  bool ExactlyOneConstraintIsFeasible(const ConstraintProto& ct) {
    int num_true_literals = 0;
    for (const int lit : ct.exactly_one().literals()) {
      if (LiteralIsTrue(lit)) ++num_true_literals;
    }
    return num_true_literals == 1;
  }

  bool BoolXorConstraintIsFeasible(const ConstraintProto& ct) {
    int sum = 0;
    for (const int lit : ct.bool_xor().literals()) {
      sum ^= LiteralIsTrue(lit) ? 1 : 0;
    }
    return sum == 1;
  }

  bool LinearConstraintIsFeasible(const ConstraintProto& ct) {
    int64_t sum = 0;
    const int num_variables = ct.linear().coeffs_size();
    const int* const vars = ct.linear().vars().data();
    const int64_t* const coeffs = ct.linear().coeffs().data();
    for (int i = 0; i < num_variables; ++i) {
      // We know we only have positive reference now.
      DCHECK(RefIsPositive(vars[i]));
      sum += variable_values_[vars[i]] * coeffs[i];
    }
    const bool result = DomainInProtoContains(ct.linear(), sum);
    if (!result) {
      VLOG(1) << "Activity: " << sum;
    }
    return result;
  }

  int64_t LinearExpressionValue(const LinearExpressionProto& expr) const {
    int64_t sum = expr.offset();
    const int num_variables = expr.vars_size();
    for (int i = 0; i < num_variables; ++i) {
      sum += Value(expr.vars(i)) * expr.coeffs(i);
    }
    return sum;
  }

  bool LinMaxConstraintIsFeasible(const ConstraintProto& ct) {
    const int64_t max = LinearExpressionValue(ct.lin_max().target());
    int64_t actual_max = std::numeric_limits<int64_t>::min();
    for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
      const int64_t expr_value = LinearExpressionValue(ct.lin_max().exprs(i));
      actual_max = std::max(actual_max, expr_value);
    }
    return max == actual_max;
  }

  bool IntProdConstraintIsFeasible(const ConstraintProto& ct) {
    const int64_t prod = LinearExpressionValue(ct.int_prod().target());
    int64_t actual_prod = 1;
    for (const LinearExpressionProto& expr : ct.int_prod().exprs()) {
      actual_prod = CapProd(actual_prod, LinearExpressionValue(expr));
    }
    return prod == actual_prod;
  }

  bool IntDivConstraintIsFeasible(const ConstraintProto& ct) {
    return LinearExpressionValue(ct.int_div().target()) ==
           LinearExpressionValue(ct.int_div().exprs(0)) /
               LinearExpressionValue(ct.int_div().exprs(1));
  }

  bool IntModConstraintIsFeasible(const ConstraintProto& ct) {
    return LinearExpressionValue(ct.int_mod().target()) ==
           LinearExpressionValue(ct.int_mod().exprs(0)) %
               LinearExpressionValue(ct.int_mod().exprs(1));
  }

  bool AllDiffConstraintIsFeasible(const ConstraintProto& ct) {
    absl::flat_hash_set<int64_t> values;
    for (const LinearExpressionProto& expr : ct.all_diff().exprs()) {
      const int64_t value = LinearExpressionValue(expr);
      const auto [it, inserted] = values.insert(value);
      if (!inserted) return false;
    }
    return true;
  }

  int64_t IntervalStart(const IntervalConstraintProto& interval) const {
    return LinearExpressionValue(interval.start());
  }

  int64_t IntervalSize(const IntervalConstraintProto& interval) const {
    return LinearExpressionValue(interval.size());
  }

  int64_t IntervalEnd(const IntervalConstraintProto& interval) const {
    return LinearExpressionValue(interval.end());
  }

  bool IntervalConstraintIsFeasible(const ConstraintProto& ct) {
    const int64_t size = IntervalSize(ct.interval());
    if (size < 0) return false;
    return IntervalStart(ct.interval()) + size == IntervalEnd(ct.interval());
  }

  bool NoOverlapConstraintIsFeasible(const CpModelProto& model,
                                     const ConstraintProto& ct) {
    std::vector<std::pair<int64_t, int64_t>> start_durations_pairs;
    for (const int i : ct.no_overlap().intervals()) {
      const ConstraintProto& interval_constraint = model.constraints(i);
      if (ConstraintIsEnforced(interval_constraint)) {
        const IntervalConstraintProto& interval =
            interval_constraint.interval();
        start_durations_pairs.push_back(
            {IntervalStart(interval), IntervalSize(interval)});
      }
    }
    std::sort(start_durations_pairs.begin(), start_durations_pairs.end());
    int64_t previous_end = std::numeric_limits<int64_t>::min();
    for (const auto& pair : start_durations_pairs) {
      if (pair.first < previous_end) return false;
      previous_end = pair.first + pair.second;
    }
    return true;
  }

  bool NoOverlap2DConstraintIsFeasible(const CpModelProto& model,
                                       const ConstraintProto& ct) {
    const auto& arg = ct.no_overlap_2d();
    // Those intervals from arg.x_intervals and arg.y_intervals where both
    // the x and y intervals are enforced.
    bool has_zero_sizes = false;
    std::vector<Rectangle> enforced_rectangles;
    {
      const int num_intervals = arg.x_intervals_size();
      CHECK_EQ(arg.y_intervals_size(), num_intervals);
      for (int i = 0; i < num_intervals; ++i) {
        const ConstraintProto& x = model.constraints(arg.x_intervals(i));
        const ConstraintProto& y = model.constraints(arg.y_intervals(i));
        if (ConstraintIsEnforced(x) && ConstraintIsEnforced(y)) {
          enforced_rectangles.push_back({.x_min = IntervalStart(x.interval()),
                                         .x_max = IntervalEnd(x.interval()),
                                         .y_min = IntervalStart(y.interval()),
                                         .y_max = IntervalEnd(y.interval())});
          const auto& rect = enforced_rectangles.back();
          if (rect.x_min == rect.x_max || rect.y_min == rect.y_max) {
            has_zero_sizes = true;
          }
        }
      }
    }

    std::optional<std::pair<int, int>> one_intersection;
    absl::c_stable_sort(enforced_rectangles,
                        [](const Rectangle& a, const Rectangle& b) {
                          return a.x_min < b.x_min;
                        });
    if (has_zero_sizes) {
      one_intersection =
          FindOneIntersectionIfPresentWithZeroArea(enforced_rectangles);
    } else {
      one_intersection = FindOneIntersectionIfPresent(enforced_rectangles);
    }

    if (one_intersection != std::nullopt) {
      VLOG(1) << "Rectangles " << one_intersection->first << "("
              << enforced_rectangles[one_intersection->first] << ") and "
              << one_intersection->second << "("
              << enforced_rectangles[one_intersection->second]
              << ") are not disjoint.";
      return false;
    }
    return true;
  }

  bool CumulativeConstraintIsFeasible(const CpModelProto& model,
                                      const ConstraintProto& ct) {
    const int64_t capacity = LinearExpressionValue(ct.cumulative().capacity());
    if (capacity < 0) return false;
    const int num_intervals = ct.cumulative().intervals_size();
    std::vector<std::pair<int64_t, int64_t>> events;
    for (int i = 0; i < num_intervals; ++i) {
      const ConstraintProto& interval_constraint =
          model.constraints(ct.cumulative().intervals(i));
      if (!ConstraintIsEnforced(interval_constraint)) continue;
      const int64_t start = IntervalStart(interval_constraint.interval());
      const int64_t duration = IntervalSize(interval_constraint.interval());
      const int64_t demand = LinearExpressionValue(ct.cumulative().demands(i));
      if (duration == 0 || demand == 0) continue;
      events.emplace_back(start, demand);
      events.emplace_back(start + duration, -demand);
    }
    if (events.empty()) return true;

    std::sort(events.begin(), events.end());

    // This works because we will process negative demands first.
    int64_t current_load = 0;
    for (const auto& [time, delta] : events) {
      current_load += delta;
      if (current_load > capacity) {
        VLOG(1) << "Cumulative constraint: load: " << current_load
                << " capacity: " << capacity << " time: " << time;
        return false;
      }
    }
    DCHECK_EQ(current_load, 0);
    return true;
  }

  bool ElementConstraintIsFeasible(const ConstraintProto& ct) {
    if (!ct.element().vars().empty()) {
      const int index = Value(ct.element().index());
      if (index < 0 || index >= ct.element().vars_size()) return false;
      return Value(ct.element().vars(index)) == Value(ct.element().target());
    }

    if (!ct.element().exprs().empty()) {
      const int index = LinearExpressionValue(ct.element().linear_index());
      if (index < 0 || index >= ct.element().exprs_size()) return false;
      return LinearExpressionValue(ct.element().exprs(index)) ==
             LinearExpressionValue(ct.element().linear_target());
    }

    return false;
  }

  bool TableConstraintIsFeasible(const ConstraintProto& ct) {
    std::vector<int64_t> solution;
    if (ct.table().exprs().empty()) {
      for (int i = 0; i < ct.table().vars_size(); ++i) {
        solution.push_back(Value(ct.table().vars(i)));
      }
    } else {
      for (int i = 0; i < ct.table().exprs_size(); ++i) {
        solution.push_back(LinearExpressionValue(ct.table().exprs(i)));
      }
    }

    // No expression -> always feasible.
    if (solution.empty()) return true;
    const int size = solution.size();

    for (int row_start = 0; row_start < ct.table().values_size();
         row_start += size) {
      int i = 0;
      while (solution[i] == ct.table().values(row_start + i)) {
        ++i;
        if (i == size) return !ct.table().negated();
      }
    }
    return ct.table().negated();
  }

  bool AutomatonConstraintIsFeasible(const ConstraintProto& ct) {
    // Build the transition table {tail, label} -> head.
    const AutomatonConstraintProto& automaton = ct.automaton();
    absl::flat_hash_map<std::pair<int64_t, int64_t>, int64_t> transition_map;
    const int num_transitions = automaton.transition_tail().size();
    for (int i = 0; i < num_transitions; ++i) {
      transition_map[{automaton.transition_tail(i),
                      automaton.transition_label(i)}] =
          automaton.transition_head(i);
    }

    // Walk the automaton.
    int64_t current_state = automaton.starting_state();
    const int num_steps =
        std::max(automaton.vars_size(), automaton.exprs_size());
    for (int i = 0; i < num_steps; ++i) {
      const std::pair<int64_t, int64_t> key = {
          current_state, automaton.vars().empty()
                             ? LinearExpressionValue(automaton.exprs(i))
                             : Value(automaton.vars(i))};
      if (!transition_map.contains(key)) {
        return false;
      }
      current_state = transition_map[key];
    }

    // Check we are now in a final state.
    for (const int64_t final : automaton.final_states()) {
      if (current_state == final) return true;
    }
    return false;
  }

  bool CircuitConstraintIsFeasible(const ConstraintProto& ct) {
    // Compute the set of relevant nodes for the constraint and set the next of
    // each of them. This also detects duplicate nexts.
    const int num_arcs = ct.circuit().tails_size();
    absl::flat_hash_set<int> nodes;
    absl::flat_hash_map<int, int> nexts;
    for (int i = 0; i < num_arcs; ++i) {
      const int tail = ct.circuit().tails(i);
      const int head = ct.circuit().heads(i);
      nodes.insert(tail);
      nodes.insert(head);
      if (LiteralIsFalse(ct.circuit().literals(i))) continue;
      if (nexts.contains(tail)) {
        VLOG(1) << "Node with two outgoing arcs";
        return false;  // Duplicate.
      }
      nexts[tail] = head;
    }

    // All node must have a next.
    int in_cycle;
    int cycle_size = 0;
    for (const int node : nodes) {
      if (!nexts.contains(node)) {
        VLOG(1) << "Node with no next: " << node;
        return false;  // No next.
      }
      if (nexts[node] == node) continue;  // skip self-loop.
      in_cycle = node;
      ++cycle_size;
    }
    if (cycle_size == 0) return true;

    // Check that we have only one cycle. visited is used to not loop forever if
    // we have a "rho" shape instead of a cycle.
    absl::flat_hash_set<int> visited;
    int current = in_cycle;
    int num_visited = 0;
    while (!visited.contains(current)) {
      ++num_visited;
      visited.insert(current);
      current = nexts[current];
    }
    if (current != in_cycle) {
      VLOG(1) << "Rho shape";
      return false;  // Rho shape.
    }
    if (num_visited != cycle_size) {
      VLOG(1) << "More than one cycle";
    }
    return num_visited == cycle_size;  // Another cycle somewhere if false.
  }

  bool RoutesConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_arcs = ct.routes().tails_size();
    int num_used_arcs = 0;
    int num_self_arcs = 0;

    // Compute the number of nodes.
    int num_nodes = 0;
    for (int i = 0; i < num_arcs; ++i) {
      num_nodes = std::max(num_nodes, 1 + ct.routes().tails(i));
      num_nodes = std::max(num_nodes, 1 + ct.routes().heads(i));
    }

    std::vector<int> tail_to_head(num_nodes, -1);
    std::vector<bool> has_incoming_arc(num_nodes, false);
    std::vector<int> has_outgoing_arc(num_nodes, false);
    std::vector<int> depot_nexts;
    for (int i = 0; i < num_arcs; ++i) {
      const int tail = ct.routes().tails(i);
      const int head = ct.routes().heads(i);
      if (LiteralIsTrue(ct.routes().literals(i))) {
        // Check for loops.
        if (tail != 0) {
          if (has_outgoing_arc[tail]) {
            VLOG(1) << "routes: node " << tail << "has two outgoing arcs";
            return false;
          }
          has_outgoing_arc[tail] = true;
        }
        if (head != 0) {
          if (has_incoming_arc[head]) {
            VLOG(1) << "routes: node " << head << "has two incoming arcs";
            return false;
          }
          has_incoming_arc[head] = true;
        }

        if (tail == head) {
          if (tail == 0) {
            VLOG(1) << "Self loop on node 0 are forbidden.";
            return false;
          }
          ++num_self_arcs;
          continue;
        }
        ++num_used_arcs;
        if (tail == 0) {
          depot_nexts.push_back(head);
        } else {
          DCHECK_EQ(tail_to_head[tail], -1);
          tail_to_head[tail] = head;
        }
      }
    }

    // An empty constraint with no node to visit should be feasible.
    if (num_nodes == 0) return true;

    // Make sure each routes from the depot go back to it, and count such arcs.
    int count = 0;
    for (int start : depot_nexts) {
      ++count;
      while (start != 0) {
        if (tail_to_head[start] == -1) return false;
        start = tail_to_head[start];
        ++count;
      }
    }

    if (count != num_used_arcs) {
      VLOG(1) << "count: " << count << " != num_used_arcs:" << num_used_arcs;
      return false;
    }

    // Each routes cover as many node as there is arcs, but this way we count
    // multiple time_exprs the depot. So the number of nodes covered are:
    //     count - depot_nexts.size() + 1.
    // And this number + the self arcs should be num_nodes.
    if (count - depot_nexts.size() + 1 + num_self_arcs != num_nodes) {
      VLOG(1) << "Not all nodes are covered!";
      return false;
    }

    return true;
  }

  bool InverseConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_variables = ct.inverse().f_direct_size();
    if (num_variables != ct.inverse().f_inverse_size()) return false;
    // Check that f_inverse(f_direct(i)) == i; this is sufficient.
    for (int i = 0; i < num_variables; i++) {
      const int fi = Value(ct.inverse().f_direct(i));
      if (fi < 0 || num_variables <= fi) return false;
      if (i != Value(ct.inverse().f_inverse(fi))) return false;
    }
    return true;
  }

  bool ReservoirConstraintIsFeasible(const ConstraintProto& ct) {
    const int num_variables = ct.reservoir().time_exprs_size();
    const int64_t min_level = ct.reservoir().min_level();
    const int64_t max_level = ct.reservoir().max_level();
    absl::btree_map<int64_t, int64_t> deltas;
    const bool has_active_variables = ct.reservoir().active_literals_size() > 0;
    for (int i = 0; i < num_variables; i++) {
      const int64_t time = LinearExpressionValue(ct.reservoir().time_exprs(i));
      if (!has_active_variables ||
          Value(ct.reservoir().active_literals(i)) == 1) {
        const int64_t level =
            LinearExpressionValue(ct.reservoir().level_changes(i));
        deltas[time] += level;
      }
    }
    int64_t current_level = 0;
    for (const auto& delta : deltas) {
      current_level += delta.second;
      if (current_level < min_level || current_level > max_level) {
        VLOG(1) << "Reservoir level " << current_level
                << " is out of bounds at time: " << delta.first;
        return false;
      }
    }
    return true;
  }

  bool ConstraintIsFeasible(const CpModelProto& model,
                            const ConstraintProto& ct) {
    // A non-enforced constraint is always feasible.
    if (!ConstraintIsEnforced(ct)) return true;

    const ConstraintProto::ConstraintCase type = ct.constraint_case();
    switch (type) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        return BoolOrConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kBoolAnd:
        return BoolAndConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kAtMostOne:
        return AtMostOneConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kExactlyOne:
        return ExactlyOneConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kBoolXor:
        return BoolXorConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kLinear:
        return LinearConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kIntProd:
        return IntProdConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kIntDiv:
        return IntDivConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kIntMod:
        return IntModConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kLinMax:
        return LinMaxConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kAllDiff:
        return AllDiffConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kInterval:
        if (!IntervalConstraintIsFeasible(ct)) {
          if (ct.interval().has_start()) {
            // Tricky: For simplified presolve, we require that a separate
            // constraint is added to the model to enforce the "interval".
            // This indicates that such a constraint was not added to the model.
            // It should probably be a validation error, but it is hard to
            // detect beforehand.
            VLOG(1) << "Warning, an interval constraint was likely used "
                       "without a corresponding linear constraint linking "
                       "its start, size and end.";
          }
          return false;
        }
        return true;
      case ConstraintProto::ConstraintCase::kNoOverlap:
        return NoOverlapConstraintIsFeasible(model, ct);
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        return NoOverlap2DConstraintIsFeasible(model, ct);
      case ConstraintProto::ConstraintCase::kCumulative:
        return CumulativeConstraintIsFeasible(model, ct);
      case ConstraintProto::ConstraintCase::kElement:
        return ElementConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kTable:
        return TableConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kAutomaton:
        return AutomatonConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kCircuit:
        return CircuitConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kRoutes:
        return RoutesConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kInverse:
        return InverseConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::kReservoir:
        return ReservoirConstraintIsFeasible(ct);
      case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
        // Empty constraint is always feasible.
        return true;
      default:
        LOG(FATAL) << "Unuspported constraint: " << ConstraintCaseName(type);
        return false;
    }
  }

 private:
  const std::vector<int64_t> variable_values_;
};

}  // namespace

bool ConstraintIsFeasible(const CpModelProto& model,
                          const ConstraintProto& constraint,
                          absl::Span<const int64_t> variable_values) {
  ConstraintChecker checker(variable_values);
  return checker.ConstraintIsFeasible(model, constraint);
}

bool SolutionIsFeasible(const CpModelProto& model,
                        absl::Span<const int64_t> variable_values,
                        const CpModelProto* mapping_proto,
                        const std::vector<int>* postsolve_mapping) {
  if (variable_values.size() != model.variables_size()) {
    VLOG(1) << "Wrong number of variables (" << variable_values.size()
            << ") in the solution vector. It should be "
            << model.variables_size() << ".";
    return false;
  }

  // Check that all values fall in the variable domains.
  for (int i = 0; i < model.variables_size(); ++i) {
    if (!DomainInProtoContains(model.variables(i), variable_values[i])) {
      VLOG(1) << "Variable #" << i << " has value " << variable_values[i]
              << " which do not fall in its domain: "
              << ProtobufShortDebugString(model.variables(i));
      return false;
    }
  }

  CHECK_EQ(variable_values.size(), model.variables_size());
  ConstraintChecker checker(variable_values);

  for (int c = 0; c < model.constraints_size(); ++c) {
    const ConstraintProto& ct = model.constraints(c);
    if (checker.ConstraintIsFeasible(model, ct)) continue;

    // Display a message to help debugging.
    VLOG(1) << "Failing constraint #" << c << " : "
            << ProtobufShortDebugString(model.constraints(c));
    if (mapping_proto != nullptr && postsolve_mapping != nullptr) {
      std::vector<int> reverse_map(mapping_proto->variables().size(), -1);
      for (int var = 0; var < postsolve_mapping->size(); ++var) {
        reverse_map[(*postsolve_mapping)[var]] = var;
      }
      for (const int var : UsedVariables(model.constraints(c))) {
        VLOG(1) << "var: " << var << " mapped_to: " << reverse_map[var]
                << " value: " << variable_values[var] << " initial_domain: "
                << ReadDomainFromProto(model.variables(var))
                << " postsolved_domain: "
                << ReadDomainFromProto(mapping_proto->variables(var));
      }
    } else {
      for (const int var : UsedVariables(model.constraints(c))) {
        VLOG(1) << "var: " << var << " value: " << variable_values[var];
      }
    }
    return false;
  }

  // Check that the objective is within its domain.
  //
  // TODO(user): This is not really a "feasibility" question, but we should
  // probably check that the response objective matches with the one we can
  // compute here. This might better be done in another function though.
  if (model.has_objective()) {
    int64_t inner_objective = 0;
    const int num_variables = model.objective().coeffs_size();
    for (int i = 0; i < num_variables; ++i) {
      inner_objective += checker.Value(model.objective().vars(i)) *
                         model.objective().coeffs(i);
    }
    if (!model.objective().domain().empty()) {
      if (!DomainInProtoContains(model.objective(), inner_objective)) {
        VLOG(1) << "Objective value " << inner_objective << " not in domain! "
                << ReadDomainFromProto(model.objective());
        return false;
      }
    }
    double factor = model.objective().scaling_factor();
    if (factor == 0.0) factor = 1.0;
    const double scaled_objective =
        factor *
        (static_cast<double>(inner_objective) + model.objective().offset());
    VLOG(2) << "Checker inner objective = " << inner_objective;
    VLOG(2) << "Checker scaled objective = " << scaled_objective;
  }

  return true;
}

bool SolutionCanBeOptimal(const CpModelProto& model,
                          absl::Span<const int64_t> variable_values) {
  if (absl::GetFlag(FLAGS_cp_model_check_dependent_variables)) {
    const VariableRelationships relationships =
        ComputeVariableRelationships(model);
    std::vector<int64_t> all_variables(variable_values.begin(),
                                       variable_values.end());
    for (const int var : relationships.secondary_variables) {
      all_variables[var] = -999999;  // Those values should be overwritten.
    }
    CHECK(ComputeAllVariablesFromPrimaryVariables(model, relationships,
                                                  &all_variables));
    CHECK(absl::MakeSpan(all_variables) == variable_values);
  }
  return true;
}

}  // namespace sat
}  // namespace operations_research
