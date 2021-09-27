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

#include "ortools/sat/cp_model_checker.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/hash.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

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
  // the domain must fall in [kint64min + 2, kint64max - 1].
  const int64_t lb = proto.domain(0);
  const int64_t ub = proto.domain(proto.domain_size() - 1);
  if (lb < std::numeric_limits<int64_t>::min() + 2 ||
      ub > std::numeric_limits<int64_t>::max() - 1) {
    return absl::StrCat(
        "var #", v, " domain do not fall in [kint64min + 2, kint64max - 1]. ",
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

std::string ValidateArgumentReferencesInConstraint(const CpModelProto& model,
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
  for (const int i : UsedIntervals(ct)) {
    if (i < 0 || i >= model.constraints_size()) {
      return absl::StrCat("Out of bound interval ", i, " in constraint #", c,
                          " : ", ProtobufShortDebugString(ct));
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

template <class LinearExpressionProto>
bool PossibleIntegerOverflow(const CpModelProto& model,
                             const LinearExpressionProto& proto,
                             int64_t offset = 0) {
  int64_t sum_min = -std::abs(offset);
  int64_t sum_max = +std::abs(offset);
  for (int i = 0; i < proto.vars_size(); ++i) {
    const int ref = proto.vars(i);
    const auto& var_proto = model.variables(PositiveRef(ref));
    const int64_t min_domain = var_proto.domain(0);
    const int64_t max_domain = var_proto.domain(var_proto.domain_size() - 1);
    if (proto.coeffs(i) == std::numeric_limits<int64_t>::min()) return true;
    const int64_t coeff =
        RefIsPositive(ref) ? proto.coeffs(i) : -proto.coeffs(i);
    const int64_t prod1 = CapProd(min_domain, coeff);
    const int64_t prod2 = CapProd(max_domain, coeff);

    // Note that we use min/max with zero to disallow "alternative" terms and
    // be sure that we cannot have an overflow if we do the computation in a
    // different order.
    sum_min = CapAdd(sum_min, std::min(int64_t{0}, std::min(prod1, prod2)));
    sum_max = CapAdd(sum_max, std::max(int64_t{0}, std::max(prod1, prod2)));
    for (const int64_t v : {prod1, prod2, sum_min, sum_max}) {
      if (v == std::numeric_limits<int64_t>::max() ||
          v == std::numeric_limits<int64_t>::min())
        return true;
    }
  }

  // In addition to computing the min/max possible sum, we also often compare
  // it with the constraint bounds, so we do not want max - min to overflow.
  if (sum_min < 0 && sum_min + std::numeric_limits<int64_t>::max() < sum_max) {
    return true;
  }
  return false;
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

int64_t IntervalSizeMin(const CpModelProto& model, int interval_index) {
  DCHECK_EQ(ConstraintProto::ConstraintCase::kInterval,
            model.constraints(interval_index).constraint_case());
  const IntervalConstraintProto& proto =
      model.constraints(interval_index).interval();
  if (proto.has_size_view()) {
    return MinOfExpression(model, proto.size_view());
  } else {
    return MinOfRef(model, proto.size());
  }
}

int64_t IntervalSizeMax(const CpModelProto& model, int interval_index) {
  DCHECK_EQ(ConstraintProto::ConstraintCase::kInterval,
            model.constraints(interval_index).constraint_case());
  const IntervalConstraintProto& proto =
      model.constraints(interval_index).interval();
  if (proto.has_size_view()) {
    return MaxOfExpression(model, proto.size_view());
  } else {
    return MaxOfRef(model, proto.size());
  }
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
  if (PossibleIntegerOverflow(model, expr, expr.offset())) {
    return absl::StrCat("Possible overflow in linear expression: ",
                        ProtobufShortDebugString(expr));
  }
  return "";
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
  const LinearConstraintProto& arg = ct.linear();
  if (PossibleIntegerOverflow(model, arg)) {
    return "Possible integer overflow in constraint: " +
           ProtobufDebugString(ct);
  }
  return "";
}

std::string ValidateIntModConstraint(const CpModelProto& model,
                                     const ConstraintProto& ct) {
  if (ct.int_mod().vars().size() != 2) {
    return absl::StrCat("An int_mod constraint should have exactly 2 terms: ",
                        ProtobufShortDebugString(ct));
  }
  const int mod_var = ct.int_mod().vars(1);
  const IntegerVariableProto& mod_proto = model.variables(PositiveRef(mod_var));
  if ((RefIsPositive(mod_var) && mod_proto.domain(0) <= 0) ||
      (!RefIsPositive(mod_var) &&
       mod_proto.domain(mod_proto.domain_size() - 1) >= 0)) {
    return absl::StrCat(
        "An int_mod must have a strictly positive modulo argument: ",
        ProtobufShortDebugString(ct));
  }
  return "";
}

std::string ValidateIntProdConstraint(const CpModelProto& model,
                                      const ConstraintProto& ct) {
  if (ct.int_prod().vars().size() != 2) {
    return absl::StrCat("An int_prod constraint should have exactly 2 terms: ",
                        ProtobufShortDebugString(ct));
  }

  // Detect potential overflow if some of the variables span across 0.
  const Domain product_domain =
      ReadDomainFromProto(model.variables(PositiveRef(ct.int_prod().vars(0))))
          .ContinuousMultiplicationBy(ReadDomainFromProto(
              model.variables(PositiveRef(ct.int_prod().vars(1)))));
  if ((product_domain.Max() == std::numeric_limits<int64_t>::max() &&
       product_domain.Min() < 0) ||
      (product_domain.Min() == std::numeric_limits<int64_t>::min() &&
       product_domain.Max() > 0)) {
    return absl::StrCat("Potential integer overflow in constraint: ",
                        ProtobufShortDebugString(ct));
  }
  return "";
}

std::string ValidateIntDivConstraint(const CpModelProto& model,
                                     const ConstraintProto& ct) {
  if (ct.int_div().vars().size() != 2) {
    return absl::StrCat("An int_div constraint should have exactly 2 terms: ",
                        ProtobufShortDebugString(ct));
  }
  const IntegerVariableProto& divisor_proto =
      model.variables(PositiveRef(ct.int_div().vars(1)));
  if (divisor_proto.domain(0) <= 0 &&
      divisor_proto.domain(divisor_proto.domain_size() - 1) >= 0) {
    return absl::StrCat("The divisor cannot span across zero in constraint: ",
                        ProtobufShortDebugString(ct));
  }
  return "";
}

std::string ValidateTableConstraint(const CpModelProto& model,
                                    const ConstraintProto& ct) {
  const TableConstraintProto& arg = ct.table();
  if (arg.vars().empty()) return "";
  if (arg.values().size() % arg.vars().size() != 0) {
    return absl::StrCat(
        "The flat encoding of a table constraint must be a multiple of the "
        "number of variable: ",
        ProtobufDebugString(ct));
  }
  return "";
}

std::string ValidateAutomatonConstraint(const CpModelProto& model,
                                        const ConstraintProto& ct) {
  const int num_transistions = ct.automaton().transition_tail().size();
  if (num_transistions != ct.automaton().transition_head().size() ||
      num_transistions != ct.automaton().transition_label().size()) {
    return absl::StrCat(
        "The transitions repeated fields must have the same size: ",
        ProtobufShortDebugString(ct));
  }
  absl::flat_hash_map<std::pair<int64_t, int64_t>, int64_t> tail_label_to_head;
  for (int i = 0; i < num_transistions; ++i) {
    const int64_t tail = ct.automaton().transition_tail(i);
    const int64_t head = ct.automaton().transition_head(i);
    const int64_t label = ct.automaton().transition_label(i);
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
std::string ValidateGraphInput(bool is_route, const CpModelProto& model,
                               const GraphProto& graph) {
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

  return ValidateGraphInput(/*is_route=*/true, model, ct.routes());
}

std::string ValidateDomainIsPositive(const CpModelProto& model, int ref,
                                     const std::string& ref_name) {
  if (ref < 0) {
    const IntegerVariableProto& var_proto = model.variables(NegatedRef(ref));
    if (var_proto.domain(var_proto.domain_size() - 1) > 0) {
      return absl::StrCat("Negative value in ", ref_name,
                          " domain: negation of ",
                          ProtobufDebugString(var_proto));
    }
  } else {
    const IntegerVariableProto& var_proto = model.variables(ref);
    if (var_proto.domain(0) < 0) {
      return absl::StrCat("Negative value in ", ref_name,
                          " domain: ", ProtobufDebugString(var_proto));
    }
  }
  return "";
}

void AppendToOverflowValidator(const LinearExpressionProto& input,
                               LinearExpressionProto* output) {
  output->mutable_vars()->Add(input.vars().begin(), input.vars().end());
  output->mutable_coeffs()->Add(input.coeffs().begin(), input.coeffs().end());

  // We add the absolute value to be sure that future computation will not
  // overflow depending on the order they are performed in.
  output->set_offset(
      CapAdd(std::abs(output->offset()), std::abs(input.offset())));
}

std::string ValidateIntervalConstraint(const CpModelProto& model,
                                       const ConstraintProto& ct) {
  if (ct.enforcement_literal().size() > 1) {
    return absl::StrCat(
        "Interval with more than one enforcement literals are currently not "
        "supported: ",
        ProtobufShortDebugString(ct));
  }

  LinearExpressionProto for_overflow_validation;

  const IntervalConstraintProto& arg = ct.interval();
  int num_view = 0;
  if (arg.has_start_view()) {
    ++num_view;
    if (arg.start_view().vars_size() > 1) {
      return "Interval with a start expression containing more than one "
             "variable are currently not supported.";
    }
    RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, arg.start_view()));
    AppendToOverflowValidator(arg.start_view(), &for_overflow_validation);
  } else {
    for_overflow_validation.add_vars(arg.start());
    for_overflow_validation.add_coeffs(1);
  }
  if (arg.has_size_view()) {
    ++num_view;
    if (arg.size_view().vars_size() > 1) {
      return "Interval with a size expression containing more than one "
             "variable are currently not supported.";
    }
    RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, arg.size_view()));
    if (ct.enforcement_literal().empty() &&
        MinOfExpression(model, arg.size_view()) < 0) {
      return absl::StrCat(
          "The size of an performed interval must be >= 0 in constraint: ",
          ProtobufDebugString(ct));
    }
    AppendToOverflowValidator(arg.size_view(), &for_overflow_validation);
  } else {
    if (ct.enforcement_literal().empty()) {
      const std::string domain_error =
          ValidateDomainIsPositive(model, arg.size(), "size");
      if (!domain_error.empty()) {
        return absl::StrCat(domain_error,
                            " in constraint: ", ProtobufDebugString(ct));
      }
    }

    for_overflow_validation.add_vars(arg.size());
    for_overflow_validation.add_coeffs(1);
  }
  if (arg.has_end_view()) {
    ++num_view;
    if (arg.end_view().vars_size() > 1) {
      return "Interval with a end expression containing more than one "
             "variable are currently not supported.";
    }
    RETURN_IF_NOT_EMPTY(ValidateLinearExpression(model, arg.end_view()));
    AppendToOverflowValidator(arg.end_view(), &for_overflow_validation);
  } else {
    for_overflow_validation.add_vars(arg.end());
    for_overflow_validation.add_coeffs(1);
  }

  if (PossibleIntegerOverflow(model, for_overflow_validation,
                              for_overflow_validation.offset())) {
    return absl::StrCat("Possible overflow in interval: ",
                        ProtobufShortDebugString(ct.interval()));
  }

  if (num_view != 0 && num_view != 3) {
    return absl::StrCat(
        "Interval must use either the var or the view representation, but not "
        "both: ",
        ProtobufShortDebugString(ct));
  }
  return "";
}

std::string ValidateCumulativeConstraint(const CpModelProto& model,
                                         const ConstraintProto& ct) {
  if (ct.cumulative().intervals_size() != ct.cumulative().demands_size()) {
    return absl::StrCat("intervals_size() != demands_size() in constraint: ",
                        ProtobufShortDebugString(ct));
  }

  if (ct.cumulative().energies_size() != 0 &&
      ct.cumulative().energies_size() != ct.cumulative().intervals_size()) {
    return absl::StrCat("energies_size() != intervals_size() in constraint: ",
                        ProtobufShortDebugString(ct));
  }

  for (const int demand_ref : ct.cumulative().demands()) {
    const std::string domain_error =
        ValidateDomainIsPositive(model, demand_ref, "demand");
    if (!domain_error.empty()) {
      return absl::StrCat(domain_error,
                          " in constraint: ", ProtobufDebugString(ct));
    }
  }

  int64_t sum_max_demands = 0;
  for (const int demand_ref : ct.cumulative().demands()) {
    const int64_t demand_max = MaxOfRef(model, demand_ref);
    DCHECK_GE(demand_max, 0);
    sum_max_demands = CapAdd(sum_max_demands, demand_max);
    if (sum_max_demands == std::numeric_limits<int64_t>::max()) {
      return "The sum of max demands do not fit on an int64_t in constraint: " +
             ProtobufDebugString(ct);
    }
  }

  int64_t sum_max_energies = 0;
  for (int i = 0; i < ct.cumulative().intervals_size(); ++i) {
    const int64_t demand_max = MaxOfRef(model, ct.cumulative().demands(i));
    const int64_t size_max =
        IntervalSizeMax(model, ct.cumulative().intervals(i));
    sum_max_energies = CapAdd(sum_max_energies, CapProd(size_max, demand_max));
    if (sum_max_energies == std::numeric_limits<int64_t>::max()) {
      return "The sum of max energies (size * demand) do not fit on an int64_t "
             "in constraint: " +
             ProtobufDebugString(ct);
    }
  }

  for (int i = 0; i < ct.cumulative().energies_size(); ++i) {
    const LinearExpressionProto& expr = ct.cumulative().energies(i);
    const std::string error = ValidateLinearExpression(model, expr);
    if (!error.empty()) {
      return absl::StrCat(error, "in energy expression of constraint: ",
                          ProtobufShortDebugString(ct));
    }

    // The following check is quite loose, but it will catch gross mistakes like
    // having an empty expression (= 0) with a non zero demand and non null
    // interval.
    const int interval = ct.cumulative().intervals(i);
    const int64_t size_min = IntervalSizeMin(model, interval);
    const int64_t size_max = IntervalSizeMax(model, interval);
    const Domain product =
        DomainOfRef(model, ct.cumulative().demands(i))
            .ContinuousMultiplicationBy({size_min, size_max});
    const int64_t energy_min = MinOfExpression(model, expr);
    const int64_t energy_max = MaxOfExpression(model, expr);
    if (product.IntersectionWith({energy_min, energy_max}).IsEmpty()) {
      return absl::StrCat(
          "The energy expression (with index ", i,
          ") is not compatible with the product of the size of the interval, "
          "and the demand in constraint: ",
          ProtobufShortDebugString(ct));
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
  if (ct.reservoir().times().size() != ct.reservoir().demands().size()) {
    return absl::StrCat("Times and demands fields must be of the same size: ",
                        ProtobufShortDebugString(ct));
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
  for (const int64_t demand : ct.reservoir().demands()) {
    // We test for min int64_t before the abs().
    if (demand == std::numeric_limits<int64_t>::min()) {
      return "Possible integer overflow in constraint: " +
             ProtobufDebugString(ct);
    }
    sum_abs = CapAdd(sum_abs, std::abs(demand));
    if (sum_abs == std::numeric_limits<int64_t>::max()) {
      return "Possible integer overflow in constraint: " +
             ProtobufDebugString(ct);
    }
  }
  if (ct.reservoir().actives_size() > 0 &&
      ct.reservoir().actives_size() != ct.reservoir().times_size()) {
    return "Wrong array length of actives variables";
  }
  if (ct.reservoir().demands_size() > 0 &&
      ct.reservoir().demands_size() != ct.reservoir().times_size()) {
    return "Wrong array length of demands variables";
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
    if (!VariableReferenceIsValid(model, v)) {
      return absl::StrCat("Out of bound integer variable ", v,
                          " in objective: ", ProtobufShortDebugString(obj));
    }
  }
  if (PossibleIntegerOverflow(model, obj)) {
    return "Possible integer overflow in objective: " +
           ProtobufDebugString(obj);
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
        drs != DecisionStrategyProto::SELECT_MEDIAN_VALUE) {
      return absl::StrCat("Unknown or unsupported domain_reduction_strategy: ",
                          drs);
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
    int previous_index = -1;
    for (const auto& transformation : strategy.transformations()) {
      if (transformation.positive_coeff() <= 0) {
        return absl::StrCat("Affine transformation coeff should be positive: ",
                            ProtobufShortDebugString(transformation));
      }
      if (transformation.index() <= previous_index ||
          transformation.index() >= strategy.variables_size()) {
        return absl::StrCat(
            "Invalid indices (must be sorted and valid) in transformation: ",
            ProtobufShortDebugString(transformation));
      }
      previous_index = transformation.index();
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
  for (const int ref : hint.vars()) {
    if (!VariableReferenceIsValid(model, ref)) {
      return absl::StrCat("Invalid variable reference in solution hint: ", ref);
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

std::string ValidateCpModel(const CpModelProto& model) {
  for (int v = 0; v < model.variables_size(); ++v) {
    RETURN_IF_NOT_EMPTY(ValidateIntegerVariable(model, v));
  }

  // Checks all variable references, and validate intervals before scanning the
  // rest of the constraints.
  for (int c = 0; c < model.constraints_size(); ++c) {
    RETURN_IF_NOT_EMPTY(ValidateArgumentReferencesInConstraint(model, c));

    const ConstraintProto& ct = model.constraints(c);
    if (ct.constraint_case() == ConstraintProto::kInterval) {
      RETURN_IF_NOT_EMPTY(ValidateIntervalConstraint(model, ct));
    }
  }

  for (int c = 0; c < model.constraints_size(); ++c) {
    // By default, a constraint does not support enforcement literals except if
    // explicitly stated by setting this to true below.
    bool support_enforcement = false;

    // Other non-generic validations.
    // TODO(user): validate all constraints.
    const ConstraintProto& ct = model.constraints(c);
    const ConstraintProto::ConstraintCase type = ct.constraint_case();
    switch (type) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kLinear:
        support_enforcement = true;
        RETURN_IF_NOT_EMPTY(ValidateLinearConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kLinMax: {
        RETURN_IF_NOT_EMPTY(
            ValidateLinearExpression(model, ct.lin_max().target()));
        for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
          RETURN_IF_NOT_EMPTY(
              ValidateLinearExpression(model, ct.lin_max().exprs(i)));
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kLinMin: {
        RETURN_IF_NOT_EMPTY(
            ValidateLinearExpression(model, ct.lin_min().target()));
        for (int i = 0; i < ct.lin_min().exprs_size(); ++i) {
          RETURN_IF_NOT_EMPTY(
              ValidateLinearExpression(model, ct.lin_min().exprs(i)));
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kIntProd:
        RETURN_IF_NOT_EMPTY(ValidateIntProdConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kIntDiv:
        RETURN_IF_NOT_EMPTY(ValidateIntDivConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kIntMod:
        RETURN_IF_NOT_EMPTY(ValidateIntModConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        if (ct.inverse().f_direct().size() != ct.inverse().f_inverse().size()) {
          return absl::StrCat("Non-matching fields size in inverse: ",
                              ProtobufShortDebugString(ct));
        }
        break;
      case ConstraintProto::ConstraintCase::kTable:
        RETURN_IF_NOT_EMPTY(ValidateTableConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kAutomaton:
        RETURN_IF_NOT_EMPTY(ValidateAutomatonConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kCircuit:
        RETURN_IF_NOT_EMPTY(
            ValidateGraphInput(/*is_route=*/false, model, ct.circuit()));
        break;
      case ConstraintProto::ConstraintCase::kRoutes:
        RETURN_IF_NOT_EMPTY(ValidateRoutesConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kInterval:
        support_enforcement = true;
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        RETURN_IF_NOT_EMPTY(ValidateCumulativeConstraint(model, ct));
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        RETURN_IF_NOT_EMPTY(ValidateNoOverlap2DConstraint(model, ct));
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
  if (model.has_objective()) {
    RETURN_IF_NOT_EMPTY(ValidateObjective(model, model.objective()));
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

#undef RETURN_IF_NOT_EMPTY

// =============================================================================
// Solution Feasibility.
// =============================================================================

namespace {

class ConstraintChecker {
 public:
  explicit ConstraintChecker(const std::vector<int64_t>& variable_values)
      : variable_values_(variable_values) {}

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
    for (int i = 0; i < num_variables; ++i) {
      sum += Value(ct.linear().vars(i)) * ct.linear().coeffs(i);
    }
    return DomainInProtoContains(ct.linear(), sum);
  }

  bool IntMaxConstraintIsFeasible(const ConstraintProto& ct) {
    const int64_t max = Value(ct.int_max().target());
    int64_t actual_max = std::numeric_limits<int64_t>::min();
    for (int i = 0; i < ct.int_max().vars_size(); ++i) {
      actual_max = std::max(actual_max, Value(ct.int_max().vars(i)));
    }
    return max == actual_max;
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
    const int64_t prod = Value(ct.int_prod().target());
    int64_t actual_prod = 1;
    for (int i = 0; i < ct.int_prod().vars_size(); ++i) {
      actual_prod *= Value(ct.int_prod().vars(i));
    }
    return prod == actual_prod;
  }

  bool IntDivConstraintIsFeasible(const ConstraintProto& ct) {
    return Value(ct.int_div().target()) ==
           Value(ct.int_div().vars(0)) / Value(ct.int_div().vars(1));
  }

  bool IntModConstraintIsFeasible(const ConstraintProto& ct) {
    return Value(ct.int_mod().target()) ==
           Value(ct.int_mod().vars(0)) % Value(ct.int_mod().vars(1));
  }

  bool IntMinConstraintIsFeasible(const ConstraintProto& ct) {
    const int64_t min = Value(ct.int_min().target());
    int64_t actual_min = std::numeric_limits<int64_t>::max();
    for (int i = 0; i < ct.int_min().vars_size(); ++i) {
      actual_min = std::min(actual_min, Value(ct.int_min().vars(i)));
    }
    return min == actual_min;
  }

  bool LinMinConstraintIsFeasible(const ConstraintProto& ct) {
    const int64_t min = LinearExpressionValue(ct.lin_min().target());
    int64_t actual_min = std::numeric_limits<int64_t>::max();
    for (int i = 0; i < ct.lin_min().exprs_size(); ++i) {
      const int64_t expr_value = LinearExpressionValue(ct.lin_min().exprs(i));
      actual_min = std::min(actual_min, expr_value);
    }
    return min == actual_min;
  }

  bool AllDiffConstraintIsFeasible(const ConstraintProto& ct) {
    absl::flat_hash_set<int64_t> values;
    for (const int v : ct.all_diff().vars()) {
      if (gtl::ContainsKey(values, Value(v))) return false;
      values.insert(Value(v));
    }
    return true;
  }

  int64_t IntervalStart(const IntervalConstraintProto& interval) const {
    return interval.has_start_view()
               ? LinearExpressionValue(interval.start_view())
               : Value(interval.start());
  }

  int64_t IntervalSize(const IntervalConstraintProto& interval) const {
    return interval.has_size_view()
               ? LinearExpressionValue(interval.size_view())
               : Value(interval.size());
  }

  int64_t IntervalEnd(const IntervalConstraintProto& interval) const {
    return interval.has_end_view() ? LinearExpressionValue(interval.end_view())
                                   : Value(interval.end());
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
    for (const auto pair : start_durations_pairs) {
      if (pair.first < previous_end) return false;
      previous_end = pair.first + pair.second;
    }
    return true;
  }

  bool IntervalsAreDisjoint(const IntervalConstraintProto& interval1,
                            const IntervalConstraintProto& interval2) {
    return IntervalEnd(interval1) <= IntervalStart(interval2) ||
           IntervalEnd(interval2) <= IntervalStart(interval1);
  }

  bool IntervalIsEmpty(const IntervalConstraintProto& interval) {
    return IntervalStart(interval) == IntervalEnd(interval);
  }

  bool NoOverlap2DConstraintIsFeasible(const CpModelProto& model,
                                       const ConstraintProto& ct) {
    const auto& arg = ct.no_overlap_2d();
    // Those intervals from arg.x_intervals and arg.y_intervals where both
    // the x and y intervals are enforced.
    std::vector<std::pair<const IntervalConstraintProto* const,
                          const IntervalConstraintProto* const>>
        enforced_intervals_xy;
    {
      const int num_intervals = arg.x_intervals_size();
      CHECK_EQ(arg.y_intervals_size(), num_intervals);
      for (int i = 0; i < num_intervals; ++i) {
        const ConstraintProto& x = model.constraints(arg.x_intervals(i));
        const ConstraintProto& y = model.constraints(arg.y_intervals(i));
        if (ConstraintIsEnforced(x) && ConstraintIsEnforced(y) &&
            (!arg.boxes_with_null_area_can_overlap() ||
             (!IntervalIsEmpty(x.interval()) &&
              !IntervalIsEmpty(y.interval())))) {
          enforced_intervals_xy.push_back({&x.interval(), &y.interval()});
        }
      }
    }
    const int num_enforced_intervals = enforced_intervals_xy.size();
    for (int i = 0; i < num_enforced_intervals; ++i) {
      for (int j = i + 1; j < num_enforced_intervals; ++j) {
        const auto& xi = *enforced_intervals_xy[i].first;
        const auto& yi = *enforced_intervals_xy[i].second;
        const auto& xj = *enforced_intervals_xy[j].first;
        const auto& yj = *enforced_intervals_xy[j].second;
        if (!IntervalsAreDisjoint(xi, xj) && !IntervalsAreDisjoint(yi, yj) &&
            !IntervalIsEmpty(xi) && !IntervalIsEmpty(xj) &&
            !IntervalIsEmpty(yi) && !IntervalIsEmpty(yj)) {
          VLOG(1) << "Interval " << i << "(x=[" << IntervalStart(xi) << ", "
                  << IntervalEnd(xi) << "], y=[" << IntervalStart(yi) << ", "
                  << IntervalEnd(yi) << "]) and " << j << "(x=["
                  << IntervalStart(xj) << ", " << IntervalEnd(xj) << "], y=["
                  << IntervalStart(yj) << ", " << IntervalEnd(yj)
                  << "]) are not disjoint.";
          return false;
        }
      }
    }
    return true;
  }

  bool CumulativeConstraintIsFeasible(const CpModelProto& model,
                                      const ConstraintProto& ct) {
    // TODO(user): Improve complexity for large durations.
    const int64_t capacity = Value(ct.cumulative().capacity());
    const int num_intervals = ct.cumulative().intervals_size();
    absl::flat_hash_map<int64_t, int64_t> usage;
    for (int i = 0; i < num_intervals; ++i) {
      const ConstraintProto& interval_constraint =
          model.constraints(ct.cumulative().intervals(i));
      if (ConstraintIsEnforced(interval_constraint)) {
        const IntervalConstraintProto& interval =
            interval_constraint.interval();
        const int64_t start = IntervalStart(interval);
        const int64_t duration = IntervalSize(interval);
        const int64_t demand = Value(ct.cumulative().demands(i));
        for (int64_t t = start; t < start + duration; ++t) {
          usage[t] += demand;
          if (usage[t] > capacity) return false;
        }
        if (!ct.cumulative().energies().empty()) {
          const LinearExpressionProto& energy_expr =
              ct.cumulative().energies(i);
          int64_t energy = energy_expr.offset();
          for (int j = 0; j < energy_expr.vars_size(); ++j) {
            energy += Value(energy_expr.vars(j)) * energy_expr.coeffs(j);
          }
          if (duration * demand != energy) {
            return false;
          }
        }
      }
    }
    return true;
  }

  bool ElementConstraintIsFeasible(const ConstraintProto& ct) {
    if (ct.element().vars().empty()) return false;
    const int index = Value(ct.element().index());
    if (index < 0 || index >= ct.element().vars_size()) return false;
    return Value(ct.element().vars(index)) == Value(ct.element().target());
  }

  bool TableConstraintIsFeasible(const ConstraintProto& ct) {
    const int size = ct.table().vars_size();
    if (size == 0) return true;
    for (int row_start = 0; row_start < ct.table().values_size();
         row_start += size) {
      int i = 0;
      while (Value(ct.table().vars(i)) == ct.table().values(row_start + i)) {
        ++i;
        if (i == size) return !ct.table().negated();
      }
    }
    return ct.table().negated();
  }

  bool AutomatonConstraintIsFeasible(const ConstraintProto& ct) {
    // Build the transition table {tail, label} -> head.
    absl::flat_hash_map<std::pair<int64_t, int64_t>, int64_t> transition_map;
    const int num_transitions = ct.automaton().transition_tail().size();
    for (int i = 0; i < num_transitions; ++i) {
      transition_map[{ct.automaton().transition_tail(i),
                      ct.automaton().transition_label(i)}] =
          ct.automaton().transition_head(i);
    }

    // Walk the automaton.
    int64_t current_state = ct.automaton().starting_state();
    const int num_steps = ct.automaton().vars_size();
    for (int i = 0; i < num_steps; ++i) {
      const std::pair<int64_t, int64_t> key = {current_state,
                                               Value(ct.automaton().vars(i))};
      if (!gtl::ContainsKey(transition_map, key)) {
        return false;
      }
      current_state = transition_map[key];
    }

    // Check we are now in a final state.
    for (const int64_t final : ct.automaton().final_states()) {
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
    int num_nodes = 0;
    std::vector<int> tail_to_head;
    std::vector<int> depot_nexts;
    for (int i = 0; i < num_arcs; ++i) {
      const int tail = ct.routes().tails(i);
      const int head = ct.routes().heads(i);
      num_nodes = std::max(num_nodes, 1 + tail);
      num_nodes = std::max(num_nodes, 1 + head);
      tail_to_head.resize(num_nodes, -1);
      if (LiteralIsTrue(ct.routes().literals(i))) {
        if (tail == head) {
          if (tail == 0) return false;
          ++num_self_arcs;
          continue;
        }
        ++num_used_arcs;
        if (tail == 0) {
          depot_nexts.push_back(head);
        } else {
          if (tail_to_head[tail] != -1) return false;
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
    // multiple times the depot. So the number of nodes covered are:
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
    const int num_variables = ct.reservoir().times_size();
    const int64_t min_level = ct.reservoir().min_level();
    const int64_t max_level = ct.reservoir().max_level();
    std::map<int64_t, int64_t> deltas;
    const bool has_active_variables = ct.reservoir().actives_size() > 0;
    for (int i = 0; i < num_variables; i++) {
      const int64_t time = Value(ct.reservoir().times(i));
      if (!has_active_variables || Value(ct.reservoir().actives(i)) == 1) {
        deltas[time] += ct.reservoir().demands(i);
      }
    }
    int64_t current_level = 0;
    for (const auto& delta : deltas) {
      current_level += delta.second;
      if (current_level < min_level || current_level > max_level) {
        VLOG(1) << "Reservoir level " << current_level
                << " is out of bounds at time" << delta.first;
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<int64_t> variable_values_;
};

}  // namespace

bool SolutionIsFeasible(const CpModelProto& model,
                        const std::vector<int64_t>& variable_values,
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

    if (!checker.ConstraintIsEnforced(ct)) continue;

    bool is_feasible = true;
    const ConstraintProto::ConstraintCase type = ct.constraint_case();
    switch (type) {
      case ConstraintProto::ConstraintCase::kBoolOr:
        is_feasible = checker.BoolOrConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kBoolAnd:
        is_feasible = checker.BoolAndConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kAtMostOne:
        is_feasible = checker.AtMostOneConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kExactlyOne:
        is_feasible = checker.ExactlyOneConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kBoolXor:
        is_feasible = checker.BoolXorConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kLinear:
        is_feasible = checker.LinearConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntProd:
        is_feasible = checker.IntProdConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntDiv:
        is_feasible = checker.IntDivConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntMod:
        is_feasible = checker.IntModConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntMin:
        is_feasible = checker.IntMinConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kLinMin:
        is_feasible = checker.LinMinConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kIntMax:
        is_feasible = checker.IntMaxConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kLinMax:
        is_feasible = checker.LinMaxConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kAllDiff:
        is_feasible = checker.AllDiffConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kInterval:
        if (!checker.IntervalConstraintIsFeasible(ct)) {
          if (ct.interval().has_start_view()) {
            // Tricky: For simplified presolve, we require that a separate
            // constraint is added to the model to enforce the "interval".
            // This indicates that such a constraint was not added to the model.
            // It should probably be a validation error, but it is hard to
            // detect beforehand.
            LOG(ERROR) << "Warning, an interval constraint was likely used "
                          "without a corresponding linear constraint linking "
                          "its start, size and end.";
          } else {
            is_feasible = false;
          }
        }
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap:
        is_feasible = checker.NoOverlapConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        is_feasible = checker.NoOverlap2DConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        is_feasible = checker.CumulativeConstraintIsFeasible(model, ct);
        break;
      case ConstraintProto::ConstraintCase::kElement:
        is_feasible = checker.ElementConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kTable:
        is_feasible = checker.TableConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kAutomaton:
        is_feasible = checker.AutomatonConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kCircuit:
        is_feasible = checker.CircuitConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kRoutes:
        is_feasible = checker.RoutesConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        is_feasible = checker.InverseConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::kReservoir:
        is_feasible = checker.ReservoirConstraintIsFeasible(ct);
        break;
      case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
        // Empty constraint is always feasible.
        break;
      default:
        LOG(FATAL) << "Unuspported constraint: " << ConstraintCaseName(type);
    }

    // Display a message to help debugging.
    if (!is_feasible) {
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
        VLOG(1) << "Objective value not in domain!";
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

}  // namespace sat
}  // namespace operations_research
