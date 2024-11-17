// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/linear_relaxation.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/types/span.h"
#include "google/protobuf/message.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/circuit.h"  // for ReindexArcs.
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/diffn_cuts.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/presolve_util.h"
#include "ortools/sat/routing_cuts.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_cuts.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

namespace {

std::pair<IntegerValue, IntegerValue> GetMinAndMaxNotEncoded(
    IntegerVariable var,
    const absl::flat_hash_set<IntegerValue>& encoded_values,
    const Model& model) {
  CHECK(VariableIsPositive(var));
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);

  const auto* domains = model.Get<IntegerDomains>();
  if (domains == nullptr || index >= domains->size()) {
    return {kMaxIntegerValue, kMinIntegerValue};
  }

  // The domain can be large, but the list of values shouldn't, so this
  // runs in O(encoded_values.size());
  IntegerValue min = kMaxIntegerValue;
  for (const int64_t v : (*domains)[index].Values()) {
    if (!encoded_values.contains(IntegerValue(v))) {
      min = IntegerValue(v);
      break;
    }
  }

  IntegerValue max = kMinIntegerValue;
  const Domain negated_domain = (*domains)[index].Negation();
  for (const int64_t v : negated_domain.Values()) {
    if (!encoded_values.contains(IntegerValue(-v))) {
      max = IntegerValue(-v);
      break;
    }
  }

  return {min, max};
}

// Collect all the affines expressions in a LinMax constraint.
// It checks that these are indeed affine expressions, and that they all share
// the same variable.
// It returns the shared variable, as well as a vector of pairs
// (coefficient, offset) when each affine is coefficient * shared_var + offset.
void CollectAffineExpressionWithSingleVariable(
    const ConstraintProto& ct, CpModelMapping* mapping, IntegerVariable* var,
    std::vector<std::pair<IntegerValue, IntegerValue>>* affines) {
  DCHECK(ExpressionsContainsOnlyOneVar(ct.lin_max().exprs()));
  CHECK_EQ(ct.constraint_case(), ConstraintProto::ConstraintCase::kLinMax);
  *var = kNoIntegerVariable;
  affines->clear();
  for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
    if (expr.vars().empty()) {
      affines->push_back({IntegerValue(0), IntegerValue(expr.offset())});
    } else {
      CHECK_EQ(expr.vars().size(), 1);
      const IntegerVariable affine_var = mapping->Integer(expr.vars(0));
      if (*var == kNoIntegerVariable) {
        *var = PositiveVariable(affine_var);
      }
      if (VariableIsPositive(affine_var)) {
        CHECK_EQ(affine_var, *var);
        affines->push_back(
            {IntegerValue(expr.coeffs(0)), IntegerValue(expr.offset())});
      } else {
        CHECK_EQ(NegationOf(affine_var), *var);
        affines->push_back(
            {IntegerValue(-expr.coeffs(0)), IntegerValue(expr.offset())});
      }
    }
  }
}

}  // namespace

void AppendRelaxationForEqualityEncoding(IntegerVariable var,
                                         const Model& model,
                                         LinearRelaxation* relaxation,
                                         int* num_tight, int* num_loose) {
  const auto* encoder = model.Get<IntegerEncoder>();
  const auto* integer_trail = model.Get<IntegerTrail>();
  if (encoder == nullptr || integer_trail == nullptr) return;

  std::vector<Literal> at_most_one_ct;
  absl::flat_hash_set<IntegerValue> encoded_values;
  std::vector<ValueLiteralPair> encoding;
  {
    const std::vector<ValueLiteralPair>& initial_encoding =
        encoder->PartialDomainEncoding(var);
    if (initial_encoding.empty()) return;
    for (const auto value_literal : initial_encoding) {
      const Literal literal = value_literal.literal;

      // Note that we skip pairs that do not have an Integer view.
      if (encoder->GetLiteralView(literal) == kNoIntegerVariable &&
          encoder->GetLiteralView(literal.Negated()) == kNoIntegerVariable) {
        continue;
      }

      encoding.push_back(value_literal);
      at_most_one_ct.push_back(literal);
      encoded_values.insert(value_literal.value);
    }
  }
  if (encoded_values.empty()) return;

  // TODO(user): PartialDomainEncoding() filter pair corresponding to literal
  // set to false, however the initial variable Domain is not always updated. As
  // a result, these min/max can be larger than in reality. Try to fix this even
  // if in practice this is a rare occurrence, as the presolve should have
  // propagated most of what we can.
  const auto [min_not_encoded, max_not_encoded] =
      GetMinAndMaxNotEncoded(var, encoded_values, model);

  // This means that there are no non-encoded value and we have a full encoding.
  // We substract the minimum value to reduce its size.
  if (min_not_encoded == kMaxIntegerValue) {
    const IntegerValue rhs = encoding[0].value;
    LinearConstraintBuilder at_least_one(&model, IntegerValue(1),
                                         kMaxIntegerValue);
    LinearConstraintBuilder encoding_ct(&model, rhs, rhs);
    encoding_ct.AddTerm(var, IntegerValue(1));
    for (const auto value_literal : encoding) {
      const Literal lit = value_literal.literal;
      CHECK(at_least_one.AddLiteralTerm(lit, IntegerValue(1)));

      const IntegerValue delta = value_literal.value - rhs;
      if (delta != IntegerValue(0)) {
        CHECK_GE(delta, IntegerValue(0));
        CHECK(encoding_ct.AddLiteralTerm(lit, -delta));
      }
    }

    // It is possible that the linear1 encoding respect our overflow
    // precondition but not the Var = sum bool * value one. In this case, we
    // just don't encode it this way. Hopefully, most normal model will not run
    // into this.
    LinearConstraint lc = encoding_ct.Build();
    if (!PossibleOverflow(*integer_trail, lc)) {
      relaxation->linear_constraints.push_back(at_least_one.Build());
      relaxation->linear_constraints.push_back(std::move(lc));
      relaxation->at_most_ones.push_back(at_most_one_ct);
      ++*num_tight;
    }
    return;
  }

  // In this special case, the two constraints below can be merged into an
  // equality: var = rhs + sum l_i * (value_i - rhs).
  if (min_not_encoded == max_not_encoded) {
    const IntegerValue rhs = min_not_encoded;
    LinearConstraintBuilder encoding_ct(&model, rhs, rhs);
    encoding_ct.AddTerm(var, IntegerValue(1));
    for (const auto value_literal : encoding) {
      CHECK(encoding_ct.AddLiteralTerm(value_literal.literal,
                                       rhs - value_literal.value));
    }

    LinearConstraint lc = encoding_ct.Build();
    if (!PossibleOverflow(*integer_trail, lc)) {
      relaxation->at_most_ones.push_back(at_most_one_ct);
      relaxation->linear_constraints.push_back(std::move(lc));
      ++*num_tight;
    }
    return;
  }

  // min + sum l_i * (value_i - min) <= var.
  // Note that this might overflow in corner cases, so we need to prevent that.
  const IntegerValue d_min = min_not_encoded;
  LinearConstraintBuilder lower_bound_ct(&model, d_min, kMaxIntegerValue);
  lower_bound_ct.AddTerm(var, IntegerValue(1));
  for (const auto value_literal : encoding) {
    CHECK(lower_bound_ct.AddLiteralTerm(value_literal.literal,
                                        d_min - value_literal.value));
  }
  LinearConstraint built_lb_ct = lower_bound_ct.Build();
  if (!PossibleOverflow(*integer_trail, built_lb_ct)) {
    relaxation->linear_constraints.push_back(std::move(built_lb_ct));
    ++*num_loose;
  }

  // var <= max + sum l_i * (value_i - max).
  // Note that this might overflow in corner cases, so we need to prevent that.
  const IntegerValue d_max = max_not_encoded;
  LinearConstraintBuilder upper_bound_ct(&model, kMinIntegerValue, d_max);
  upper_bound_ct.AddTerm(var, IntegerValue(1));
  for (const auto value_literal : encoding) {
    CHECK(upper_bound_ct.AddLiteralTerm(value_literal.literal,
                                        d_max - value_literal.value));
  }
  LinearConstraint built_ub_ct = upper_bound_ct.Build();
  if (!PossibleOverflow(*integer_trail, built_ub_ct)) {
    relaxation->linear_constraints.push_back(std::move(built_ub_ct));
    ++*num_loose;
  }

  // Note that empty/trivial constraints will be filtered later.
  relaxation->at_most_ones.push_back(at_most_one_ct);
}

void AppendPartialGreaterThanEncodingRelaxation(IntegerVariable var,
                                                const Model& model,
                                                LinearRelaxation* relaxation) {
  const auto* integer_trail = model.Get<IntegerTrail>();
  const auto* encoder = model.Get<IntegerEncoder>();
  if (integer_trail == nullptr || encoder == nullptr) return;

  const auto& greater_than_encoding = encoder->PartialGreaterThanEncoding(var);
  if (greater_than_encoding.empty()) return;

  // Start by the var >= side.
  // And also add the implications between used literals.
  {
    IntegerValue prev_used_bound = integer_trail->LowerBound(var);
    LinearConstraintBuilder builder(&model, prev_used_bound, kMaxIntegerValue);
    builder.AddTerm(var, IntegerValue(1));
    LiteralIndex prev_literal_index = kNoLiteralIndex;
    for (const auto entry : greater_than_encoding) {
      if (entry.value <= prev_used_bound) continue;

      const LiteralIndex literal_index = entry.literal.Index();
      const IntegerValue diff = prev_used_bound - entry.value;

      // Skip the entry if the literal doesn't have a view.
      if (!builder.AddLiteralTerm(entry.literal, diff)) continue;
      if (prev_literal_index != kNoLiteralIndex) {
        // Add var <= prev_var, which is the same as var + not(prev_var) <= 1
        relaxation->at_most_ones.push_back(
            {Literal(literal_index), Literal(prev_literal_index).Negated()});
      }
      prev_used_bound = entry.value;
      prev_literal_index = literal_index;
    }

    // Note that by construction, this shouldn't be able to overflow.
    relaxation->linear_constraints.push_back(builder.Build());
  }

  // Do the same for the var <= side by using NegationOfVar().
  // Note that we do not need to add the implications between literals again.
  {
    IntegerValue prev_used_bound = integer_trail->LowerBound(NegationOf(var));
    LinearConstraintBuilder builder(&model, prev_used_bound, kMaxIntegerValue);
    builder.AddTerm(var, IntegerValue(-1));
    for (const auto entry :
         encoder->PartialGreaterThanEncoding(NegationOf(var))) {
      if (entry.value <= prev_used_bound) continue;
      const IntegerValue diff = prev_used_bound - entry.value;

      // Skip the entry if the literal doesn't have a view.
      if (!builder.AddLiteralTerm(entry.literal, diff)) continue;
      prev_used_bound = entry.value;
    }

    // Note that by construction, this shouldn't be able to overflow.
    relaxation->linear_constraints.push_back(builder.Build());
  }
}

namespace {

bool AllLiteralsHaveViews(const IntegerEncoder& encoder,
                          absl::Span<const Literal> literals) {
  for (const Literal lit : literals) {
    if (!encoder.LiteralOrNegationHasView(lit)) return false;
  }
  return true;
}

}  // namespace

void AppendBoolOrRelaxation(const ConstraintProto& ct, Model* model,
                            LinearRelaxation* relaxation) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  LinearConstraintBuilder lc(model, IntegerValue(1), kMaxIntegerValue);
  for (const int enforcement_ref : ct.enforcement_literal()) {
    CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                            IntegerValue(1)));
  }
  for (const int ref : ct.bool_or().literals()) {
    CHECK(lc.AddLiteralTerm(mapping->Literal(ref), IntegerValue(1)));
  }
  relaxation->linear_constraints.push_back(lc.Build());
}

void AppendBoolAndRelaxation(const ConstraintProto& ct, Model* model,
                             LinearRelaxation* relaxation,
                             ActivityBoundHelper* activity_helper) {
  if (!HasEnforcementLiteral(ct)) return;

  // TODO(user): These constraints can be many, and if they are not regrouped
  // in big at most ones, then they should probably only added lazily as cuts.
  // Regroup this with future clique-cut separation logic.
  //
  // Note that for the case with only one enforcement, what we do below is
  // already done by the clique merging code.
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  if (ct.enforcement_literal().size() == 1) {
    const Literal enforcement = mapping->Literal(ct.enforcement_literal(0));
    for (const int ref : ct.bool_and().literals()) {
      relaxation->at_most_ones.push_back(
          {enforcement, mapping->Literal(ref).Negated()});
    }
    return;
  }

  // If we have many_literals => many_fixed literal, it is important to
  // try to use a tight big-M if we can. This is important on neos-957323.pb.gz
  // for instance.
  //
  // We split the literal into disjoint AMO and we encode each with
  //     sum Not(literals) <= sum Not(enforcement)
  //
  // Note that what we actually do is use the decomposition into at most one
  // and add a constraint for each part rather than just adding the sum of them.
  //
  // TODO(user): More generally, do not miss the same structure if the bool_and
  // was expanded into many clauses!
  //
  // TODO(user): It is not 100% clear that just not adding one constraint is
  // worse. Relaxation is worse, but then we have less constraint.
  LinearConstraintBuilder builder(model);
  if (activity_helper != nullptr) {
    std::vector<int> negated_lits;
    for (const int ref : ct.bool_and().literals()) {
      negated_lits.push_back(NegatedRef(ref));
    }
    for (absl::Span<const int> part :
         activity_helper->PartitionLiteralsIntoAmo(negated_lits)) {
      builder.Clear();
      for (const int negated_ref : part) {
        CHECK(builder.AddLiteralTerm(mapping->Literal(negated_ref)));
      }
      for (const int enforcement_ref : ct.enforcement_literal()) {
        CHECK(builder.AddLiteralTerm(
            mapping->Literal(NegatedRef(enforcement_ref)), IntegerValue(-1)));
      }
      relaxation->linear_constraints.push_back(
          builder.BuildConstraint(kMinIntegerValue, IntegerValue(0)));
    }
  } else {
    for (const int ref : ct.bool_and().literals()) {
      builder.Clear();
      CHECK(builder.AddLiteralTerm(mapping->Literal(NegatedRef(ref))));
      for (const int enforcement_ref : ct.enforcement_literal()) {
        CHECK(builder.AddLiteralTerm(
            mapping->Literal(NegatedRef(enforcement_ref)), IntegerValue(-1)));
      }
      relaxation->linear_constraints.push_back(
          builder.BuildConstraint(kMinIntegerValue, IntegerValue(0)));
    }
  }
}

void AppendAtMostOneRelaxation(const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  relaxation->at_most_ones.push_back(
      mapping->Literals(ct.at_most_one().literals()));
}

void AppendExactlyOneRelaxation(const ConstraintProto& ct, Model* model,
                                LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  auto* encoder = model->GetOrCreate<IntegerEncoder>();

  const std::vector<Literal> literals =
      mapping->Literals(ct.exactly_one().literals());
  if (AllLiteralsHaveViews(*encoder, literals)) {
    LinearConstraintBuilder lc(model, IntegerValue(1), IntegerValue(1));
    for (const Literal lit : literals) {
      CHECK(lc.AddLiteralTerm(lit, IntegerValue(1)));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  } else {
    // We just encode the at most one part that might be partially linearized
    // later.
    relaxation->at_most_ones.push_back(literals);
  }
}

std::vector<Literal> CreateAlternativeLiteralsWithView(
    int num_literals, Model* model, LinearRelaxation* relaxation) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();

  if (num_literals == 1) {
    // This is not supposed to happen, but it is easy enough to cover, just
    // in case. We might however want to use encoder->GetTrueLiteral().
    const IntegerVariable var = model->Add(NewIntegerVariable(1, 1));
    const Literal lit =
        encoder->GetOrCreateLiteralAssociatedToEquality(var, IntegerValue(1));
    return {lit};
  }

  if (num_literals == 2) {
    const IntegerVariable var = model->Add(NewIntegerVariable(0, 1));
    const Literal lit =
        encoder->GetOrCreateLiteralAssociatedToEquality(var, IntegerValue(1));

    // TODO(user): We shouldn't need to create this view ideally. Even better,
    // we should be able to handle Literal natively in the linear relaxation,
    // but that is a lot of work.
    const IntegerVariable var2 = model->Add(NewIntegerVariable(0, 1));
    encoder->AssociateToIntegerEqualValue(lit.Negated(), var2, IntegerValue(1));

    return {lit, lit.Negated()};
  }

  std::vector<Literal> literals;
  LinearConstraintBuilder lc_builder(model, IntegerValue(1), IntegerValue(1));
  for (int i = 0; i < num_literals; ++i) {
    const IntegerVariable var = model->Add(NewIntegerVariable(0, 1));
    const Literal lit =
        encoder->GetOrCreateLiteralAssociatedToEquality(var, IntegerValue(1));
    literals.push_back(lit);
    CHECK(lc_builder.AddLiteralTerm(lit, IntegerValue(1)));
  }
  model->Add(ExactlyOneConstraint(literals));
  relaxation->linear_constraints.push_back(lc_builder.Build());
  return literals;
}

void AppendCircuitRelaxation(const ConstraintProto& ct, Model* model,
                             LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const int num_arcs = ct.circuit().literals_size();
  CHECK_EQ(num_arcs, ct.circuit().tails_size());
  CHECK_EQ(num_arcs, ct.circuit().heads_size());

  // Each node must have exactly one incoming and one outgoing arc (note
  // that it can be the unique self-arc of this node too).
  absl::btree_map<int, std::vector<Literal>> incoming_arc_constraints;
  absl::btree_map<int, std::vector<Literal>> outgoing_arc_constraints;
  for (int i = 0; i < num_arcs; i++) {
    const Literal arc = mapping->Literal(ct.circuit().literals(i));
    const int tail = ct.circuit().tails(i);
    const int head = ct.circuit().heads(i);
    outgoing_arc_constraints[tail].push_back(arc);
    incoming_arc_constraints[head].push_back(arc);
  }
  for (const auto* node_map :
       {&outgoing_arc_constraints, &incoming_arc_constraints}) {
    for (const auto& entry : *node_map) {
      const std::vector<Literal>& exactly_one = entry.second;
      if (exactly_one.size() > 1) {
        LinearConstraintBuilder at_least_one_lc(model, IntegerValue(1),
                                                kMaxIntegerValue);
        for (const Literal l : exactly_one) {
          CHECK(at_least_one_lc.AddLiteralTerm(l, IntegerValue(1)));
        }

        // We separate the two constraints.
        relaxation->at_most_ones.push_back(exactly_one);
        relaxation->linear_constraints.push_back(at_least_one_lc.Build());
      }
    }
  }
}

void AppendRoutesRelaxation(const ConstraintProto& ct, Model* model,
                            LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const int num_arcs = ct.routes().literals_size();
  CHECK_EQ(num_arcs, ct.routes().tails_size());
  CHECK_EQ(num_arcs, ct.routes().heads_size());

  // Each node except node zero must have exactly one incoming and one outgoing
  // arc (note that it can be the unique self-arc of this node too). For node
  // zero, the number of incoming arcs should be the same as the number of
  // outgoing arcs.
  absl::btree_map<int, std::vector<Literal>> incoming_arc_constraints;
  absl::btree_map<int, std::vector<Literal>> outgoing_arc_constraints;
  for (int i = 0; i < num_arcs; i++) {
    const Literal arc = mapping->Literal(ct.routes().literals(i));
    const int tail = ct.routes().tails(i);
    const int head = ct.routes().heads(i);
    outgoing_arc_constraints[tail].push_back(arc);
    incoming_arc_constraints[head].push_back(arc);
  }
  for (const auto* node_map :
       {&outgoing_arc_constraints, &incoming_arc_constraints}) {
    for (const auto& entry : *node_map) {
      if (entry.first == 0) continue;
      const std::vector<Literal>& exactly_one = entry.second;
      if (exactly_one.size() > 1) {
        LinearConstraintBuilder at_least_one_lc(model, IntegerValue(1),
                                                kMaxIntegerValue);
        for (const Literal l : exactly_one) {
          CHECK(at_least_one_lc.AddLiteralTerm(l, IntegerValue(1)));
        }

        // We separate the two constraints.
        relaxation->at_most_ones.push_back(exactly_one);
        relaxation->linear_constraints.push_back(at_least_one_lc.Build());
      }
    }
  }
  LinearConstraintBuilder zero_node_balance_lc(model, IntegerValue(0),
                                               IntegerValue(0));
  for (const Literal& incoming_arc : incoming_arc_constraints[0]) {
    CHECK(zero_node_balance_lc.AddLiteralTerm(incoming_arc, IntegerValue(1)));
  }
  for (const Literal& outgoing_arc : outgoing_arc_constraints[0]) {
    CHECK(zero_node_balance_lc.AddLiteralTerm(outgoing_arc, IntegerValue(-1)));
  }
  relaxation->linear_constraints.push_back(zero_node_balance_lc.Build());
}

void AddCircuitCutGenerator(const ConstraintProto& ct, Model* m,
                            LinearRelaxation* relaxation) {
  std::vector<int> tails(ct.circuit().tails().begin(),
                         ct.circuit().tails().end());
  std::vector<int> heads(ct.circuit().heads().begin(),
                         ct.circuit().heads().end());
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<Literal> literals = mapping->Literals(ct.circuit().literals());
  const int num_nodes = ReindexArcs(&tails, &heads);

  relaxation->cut_generators.push_back(CreateStronglyConnectedGraphCutGenerator(
      num_nodes, tails, heads, literals, m));
}

void AddRoutesCutGenerator(const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation) {
  std::vector<int> tails(ct.routes().tails().begin(),
                         ct.routes().tails().end());
  std::vector<int> heads(ct.routes().heads().begin(),
                         ct.routes().heads().end());
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<Literal> literals = mapping->Literals(ct.routes().literals());

  int num_nodes = 0;
  for (int i = 0; i < ct.routes().tails_size(); ++i) {
    num_nodes = std::max(num_nodes, 1 + ct.routes().tails(i));
    num_nodes = std::max(num_nodes, 1 + ct.routes().heads(i));
  }
  if (ct.routes().demands().empty() || ct.routes().capacity() == 0) {
    relaxation->cut_generators.push_back(
        CreateStronglyConnectedGraphCutGenerator(num_nodes, tails, heads,
                                                 literals, m));
  } else {
    const std::vector<int64_t> demands(ct.routes().demands().begin(),
                                       ct.routes().demands().end());
    relaxation->cut_generators.push_back(CreateCVRPCutGenerator(
        num_nodes, tails, heads, literals, demands, ct.routes().capacity(), m));
  }
}

// Scan the intervals of a cumulative/no_overlap constraint, and its capacity (1
// for the no_overlap). It returns the index of the makespan interval if found,
// or std::nullopt otherwise.
//
// Currently, this requires the capacity to be fixed in order to scan for a
// makespan interval.
//
// The makespan interval has the following property:
//   - its end is fixed at the horizon
//   - it is always present
//   - its demand is the capacity of the cumulative/no_overlap.
//   - its size is > 0.
//
// These property ensures that all other intervals ends before the start of
// the makespan interval.
std::optional<int> DetectMakespan(
    const std::vector<IntervalVariable>& intervals,
    const std::vector<AffineExpression>& demands,
    const AffineExpression& capacity, Model* model) {
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();

  // TODO(user): Supports variable capacity.
  if (!integer_trail->IsFixed(capacity)) {
    return std::nullopt;
  }

  // Detect the horizon (max of all end max of all intervals).
  IntegerValue horizon = kMinIntegerValue;
  for (int i = 0; i < intervals.size(); ++i) {
    if (repository->IsAbsent(intervals[i])) continue;
    horizon = std::max(
        horizon, integer_trail->UpperBound(repository->End(intervals[i])));
  }

  const IntegerValue capacity_value = integer_trail->FixedValue(capacity);
  for (int i = 0; i < intervals.size(); ++i) {
    if (!repository->IsPresent(intervals[i])) continue;
    const AffineExpression& end = repository->End(intervals[i]);
    if (integer_trail->IsFixed(demands[i]) &&
        integer_trail->FixedValue(demands[i]) == capacity_value &&
        integer_trail->IsFixed(end) &&
        integer_trail->FixedValue(end) == horizon &&
        integer_trail->LowerBound(repository->Size(intervals[i])) > 0) {
      return i;
    }
  }
  return std::nullopt;
}

namespace {

std::optional<AffineExpression> DetectMakespanFromPrecedences(
    const SchedulingConstraintHelper& helper, Model* model) {
  if (helper.NumTasks() == 0) return {};

  const absl::Span<const AffineExpression> ends = helper.Ends();
  std::vector<IntegerVariable> end_vars;
  for (const AffineExpression& end : ends) {
    // TODO(user): Deal with constant end.
    if (end.var == kNoIntegerVariable) return {};
    if (end.coeff != 1) return {};
    end_vars.push_back(end.var);
  }

  std::vector<FullIntegerPrecedence> output;
  auto* precedences = model->GetOrCreate<PrecedenceRelations>();
  precedences->ComputeFullPrecedences(end_vars, &output);
  for (const auto& p : output) {
    // TODO(user): What if we have more than one candidate makespan ?
    if (p.indices.size() != ends.size()) continue;

    // We have a Makespan!
    // We want p.var - min_delta >= max(end).
    IntegerValue min_delta = kMaxIntegerValue;
    for (int i = 0; i < p.indices.size(); ++i) {
      // end_vars[indices[i]] + p.offset[i] <= p.var
      // [interval_end - end_offset][indices[i]] + p.offset[i] <= p.var
      //
      // TODO(user): It is possible this min_delta becomes positive but for a
      // real makespan, we could make sure it is <= 0. This can happen if we
      // have an optional interval because we didn't compute the offset assuming
      // this interval was present. We could potentially fix it if we know that
      // presence_literal => p.var >= end.
      min_delta =
          std::min(min_delta, p.offsets[i] - ends[p.indices[i]].constant);
    }
    VLOG(2) << "Makespan detected >= ends + " << min_delta;
    return AffineExpression(p.var, IntegerValue(1), -min_delta);
  }

  return {};
}

}  // namespace

void AppendNoOverlapRelaxationAndCutGenerator(const ConstraintProto& ct,
                                              Model* model,
                                              LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.no_overlap().intervals());
  const IntegerValue one(1);
  std::vector<AffineExpression> demands(intervals.size(), one);
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();

  std::optional<AffineExpression> makespan;
  const std::optional<int> makespan_index =
      DetectMakespan(intervals, demands, /*capacity=*/one, model);
  if (makespan_index.has_value()) {
    makespan = repository->Start(intervals[makespan_index.value()]);
    demands.pop_back();  // the vector is filled with ones.
    intervals.erase(intervals.begin() + makespan_index.value());
  }

  SchedulingConstraintHelper* helper = repository->GetOrCreateHelper(intervals);
  if (!helper->SynchronizeAndSetTimeDirection(true)) return;
  SchedulingDemandHelper* demands_helper =
      repository->GetOrCreateDemandHelper(helper, demands);

  if (!makespan.has_value()) {
    makespan = DetectMakespanFromPrecedences(*helper, model);
  }

  AddCumulativeRelaxation(/*capacity=*/one, helper, demands_helper, makespan,
                          model, relaxation);
  if (model->GetOrCreate<SatParameters>()->linearization_level() > 1) {
    AddNoOverlapCutGenerator(helper, makespan, model, relaxation);
  }
}

void AppendCumulativeRelaxationAndCutGenerator(const ConstraintProto& ct,
                                               Model* model,
                                               LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> intervals =
      mapping->Intervals(ct.cumulative().intervals());
  std::vector<AffineExpression> demands =
      mapping->Affines(ct.cumulative().demands());
  const AffineExpression capacity = mapping->Affine(ct.cumulative().capacity());
  const std::optional<int> makespan_index =
      DetectMakespan(intervals, demands, capacity, model);
  std::optional<AffineExpression> makespan;
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();
  if (makespan_index.has_value()) {
    // We remove the makespan data from the intervals the demands vector.
    makespan = repository->Start(intervals[makespan_index.value()]);
    demands.erase(demands.begin() + makespan_index.value());
    intervals.erase(intervals.begin() + makespan_index.value());
  }

  // We try to linearize the energy of each task (size * demand).
  SchedulingConstraintHelper* helper = repository->GetOrCreateHelper(intervals);
  if (!helper->SynchronizeAndSetTimeDirection(true)) return;
  SchedulingDemandHelper* demands_helper =
      repository->GetOrCreateDemandHelper(helper, demands);

  if (!makespan.has_value()) {
    makespan = DetectMakespanFromPrecedences(*helper, model);
  }

  // We can now add the relaxation and the cut generators.
  AddCumulativeRelaxation(capacity, helper, demands_helper, makespan, model,
                          relaxation);
  if (model->GetOrCreate<SatParameters>()->linearization_level() > 1) {
    AddCumulativeCutGenerator(capacity, helper, demands_helper, makespan, model,
                              relaxation);
  }
}

// This relaxation will compute the bounding box of all tasks in the cumulative,
// and add the constraint that the sum of energies of each task must fit in the
// capacity * span area.
void AddCumulativeRelaxation(const AffineExpression& capacity,
                             SchedulingConstraintHelper* helper,
                             SchedulingDemandHelper* demands_helper,
                             const std::optional<AffineExpression>& makespan,
                             Model* model, LinearRelaxation* relaxation) {
  const int num_intervals = helper->NumTasks();
  if (!demands_helper->CacheAllEnergyValues()) return;

  IntegerValue min_of_starts = kMaxIntegerValue;
  IntegerValue max_of_ends = kMinIntegerValue;
  int num_variable_energies = 0;
  int num_optionals = 0;
  int64_t sizes_gcd = 0;
  int64_t demands_gcd = 0;
  int64_t num_active_intervals = 0;
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (int index = 0; index < num_intervals; ++index) {
    if (helper->IsAbsent(index)) continue;
    if (helper->SizeMax(index) == 0 || demands_helper->DemandMax(index) == 0) {
      continue;
    }

    if (helper->IsOptional(index)) {
      if (demands_helper->EnergyMin(index) > 0) {
        num_optionals++;
      } else {
        continue;
      }
    }

    if (!helper->SizeIsFixed(index) || !demands_helper->DemandIsFixed(index)) {
      num_variable_energies++;
    }
    if (sizes_gcd != 1) {
      if (helper->SizeIsFixed(index)) {
        sizes_gcd = std::gcd(helper->SizeMin(index).value(), sizes_gcd);
      } else {
        sizes_gcd = 1;
      }
    }
    if (demands_gcd != 1) {
      if (demands_helper->DemandIsFixed(index)) {
        demands_gcd =
            std::gcd(demands_helper->DemandMin(index).value(), demands_gcd);
      } else {
        demands_gcd = 1;
      }
    }
    num_active_intervals++;
    min_of_starts = std::min(min_of_starts, helper->StartMin(index));
    max_of_ends = std::max(max_of_ends, helper->EndMax(index));
  }

  VLOG(2) << "Span [" << min_of_starts << ".." << max_of_ends << "] with "
          << num_optionals << " optional intervals, and "
          << num_variable_energies << " variable energy tasks out of "
          << num_active_intervals << "/" << num_intervals << " intervals"
          << (makespan.has_value() ? ", and 1 makespan" : "")
          << " sizes_gcd: " << sizes_gcd << " demands_gcd: " << demands_gcd;

  // There are no active intervals, no need to add the relaxation.
  if (num_active_intervals == 0) return;

  // If nothing is variable, and the coefficients cannot be reduced, the linear
  // relaxation will already be enforced by the scheduling propagators.
  if (num_variable_energies + num_optionals == 0 && sizes_gcd == 1 &&
      demands_gcd == 1) {
    return;
  }

  // Specialized case 1: sizes are fixed with a non 1 gcd and no makespan.
  if (sizes_gcd != 1 && !makespan.has_value()) {
    VLOG(2) << "Cumulative relaxation: sizes_gcd = " << sizes_gcd
            << ", demands_gcd = " << demands_gcd
            << ", no makespan, capacity is " << capacity.DebugString();
    // We can simplify the capacity only if it is fixed.
    // TODO(user): We could use (capacity / demands_gcd) * demands_gcd.
    if (!integer_trail->IsFixed(capacity)) demands_gcd = 1;
    LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
    for (int index = 0; index < num_intervals; ++index) {
      if (helper->IsAbsent(index)) continue;
      if (helper->SizeMax(index) == 0 ||
          demands_helper->DemandMax(index) == 0) {
        continue;
      }

      if (helper->IsOptional(index)) {
        const IntegerValue energy_min = demands_helper->EnergyMin(index);
        if (energy_min == 0) continue;
        DCHECK_EQ(energy_min % (sizes_gcd * demands_gcd), 0);
        if (!lc.AddLiteralTerm(helper->PresenceLiteral(index),
                               energy_min / sizes_gcd / demands_gcd)) {
          return;
        }
      } else {
        // Copy the decomposed energy.
        std::vector<LiteralValueValue> product =
            demands_helper->DecomposedEnergies()[index];
        if (!product.empty()) {
          // The energy is defined if the vector is not empty.
          // Let's reduce the coefficients.
          for (LiteralValueValue& entry : product) {
            DCHECK_EQ(entry.left_value % sizes_gcd, 0);
            entry.left_value /= sizes_gcd;
            DCHECK_EQ(entry.right_value % demands_gcd, 0);
            entry.right_value /= demands_gcd;
          }
          if (!lc.AddDecomposedProduct(product)) return;
        } else {
          // We know the size is fixed.
          const IntegerValue local_size =
              integer_trail->FixedValue(helper->Sizes()[index]);
          DCHECK_EQ(local_size % sizes_gcd, 0);
          if (demands_gcd == 1) {
            lc.AddTerm(demands_helper->Demands()[index],
                       local_size / sizes_gcd);
          } else {
            const IntegerValue local_demand =
                integer_trail->FixedValue(demands_helper->Demands()[index]);
            DCHECK_EQ(local_demand % demands_gcd, 0);
            lc.AddConstant(local_size * local_demand / sizes_gcd / demands_gcd);
          }
        }
      }
    }

    // Add the available energy of the cumulative.
    if (integer_trail->IsFixed(capacity)) {
      const IntegerValue span = max_of_ends - min_of_starts;
      const IntegerValue fixed_capacity = integer_trail->FixedValue(capacity);
      lc.AddConstant(
          -MathUtil::FloorOfRatio(fixed_capacity.value(), demands_gcd) *
          MathUtil::FloorOfRatio(span.value(), sizes_gcd));
    } else {
      DCHECK_EQ(demands_gcd, 1);
      lc.AddTerm(capacity, -(max_of_ends - min_of_starts) / sizes_gcd);
    }
    relaxation->linear_constraints.push_back(lc.Build());
    return;
  }

  // TODO(user): Implement demands_gcd != 1 && capacity is fixed.

  LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
  for (int index = 0; index < num_intervals; ++index) {
    if (helper->IsAbsent(index)) continue;
    if (helper->IsOptional(index)) {
      const IntegerValue energy_min = demands_helper->EnergyMin(index);
      if (energy_min == 0) continue;
      if (!lc.AddLiteralTerm(helper->PresenceLiteral(index), energy_min)) {
        return;
      }
    } else {
      const std::vector<LiteralValueValue>& product =
          demands_helper->DecomposedEnergies()[index];
      if (!product.empty()) {
        // The energy is defined if the vector is not empty.
        if (!lc.AddDecomposedProduct(product)) return;
      } else {
        // The energy is not a decomposed product, but it could still be
        // constant or linear. If not, a McCormick relaxation will be
        // introduced. AddQuadraticLowerBound() supports all cases.
        lc.AddQuadraticLowerBound(helper->Sizes()[index],
                                  demands_helper->Demands()[index],
                                  integer_trail);
      }
    }
  }

  // Create and link span_start and span_end to the starts and ends of the
  // tasks.
  //
  // TODO(user): In some cases, we could have only one task that can be
  // first.
  const AffineExpression span_start = min_of_starts;
  const AffineExpression span_end =
      makespan.has_value() ? makespan.value() : max_of_ends;
  lc.AddTerm(span_end, -integer_trail->UpperBound(capacity));
  lc.AddTerm(span_start, integer_trail->UpperBound(capacity));
  relaxation->linear_constraints.push_back(lc.Build());
}

// Adds the energetic relaxation sum(areas) <= bounding box area.
void AppendNoOverlap2dRelaxation(const ConstraintProto& ct, Model* model,
                                 LinearRelaxation* relaxation) {
  CHECK(ct.has_no_overlap_2d());
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> x_intervals =
      mapping->Intervals(ct.no_overlap_2d().x_intervals());
  std::vector<IntervalVariable> y_intervals =
      mapping->Intervals(ct.no_overlap_2d().y_intervals());

  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* intervals_repository = model->GetOrCreate<IntervalsRepository>();

  IntegerValue x_min = kMaxIntegerValue;
  IntegerValue x_max = kMinIntegerValue;
  IntegerValue y_min = kMaxIntegerValue;
  IntegerValue y_max = kMinIntegerValue;
  std::vector<AffineExpression> x_sizes;
  std::vector<AffineExpression> y_sizes;
  for (int i = 0; i < ct.no_overlap_2d().x_intervals_size(); ++i) {
    x_sizes.push_back(intervals_repository->Size(x_intervals[i]));
    y_sizes.push_back(intervals_repository->Size(y_intervals[i]));
    x_min = std::min(x_min, integer_trail->LevelZeroLowerBound(
                                intervals_repository->Start(x_intervals[i])));
    x_max = std::max(x_max, integer_trail->LevelZeroUpperBound(
                                intervals_repository->End(x_intervals[i])));
    y_min = std::min(y_min, integer_trail->LevelZeroLowerBound(
                                intervals_repository->Start(y_intervals[i])));
    y_max = std::max(y_max, integer_trail->LevelZeroUpperBound(
                                intervals_repository->End(y_intervals[i])));
  }

  const IntegerValue max_area =
      IntegerValue(CapProd(CapSub(x_max.value(), x_min.value()),
                           CapSub(y_max.value(), y_min.value())));
  if (max_area == kMaxIntegerValue) return;

  auto* product_decomposer = model->GetOrCreate<ProductDecomposer>();
  LinearConstraintBuilder lc(model, IntegerValue(0), max_area);
  for (int i = 0; i < ct.no_overlap_2d().x_intervals_size(); ++i) {
    if (intervals_repository->IsPresent(x_intervals[i]) &&
        intervals_repository->IsPresent(y_intervals[i])) {
      const std::vector<LiteralValueValue> energy =
          product_decomposer->TryToDecompose(x_sizes[i], y_sizes[i]);
      if (!energy.empty()) {
        if (!lc.AddDecomposedProduct(energy)) return;
      } else {
        lc.AddQuadraticLowerBound(x_sizes[i], y_sizes[i], integer_trail);
      }
    } else if (intervals_repository->IsPresent(x_intervals[i]) ||
               intervals_repository->IsPresent(y_intervals[i]) ||
               (intervals_repository->PresenceLiteral(x_intervals[i]) ==
                intervals_repository->PresenceLiteral(y_intervals[i]))) {
      // We have only one active literal.
      const Literal presence_literal =
          intervals_repository->IsPresent(x_intervals[i])
              ? intervals_repository->PresenceLiteral(y_intervals[i])
              : intervals_repository->PresenceLiteral(x_intervals[i]);
      const IntegerValue area_min =
          integer_trail->LevelZeroLowerBound(x_sizes[i]) *
          integer_trail->LevelZeroLowerBound(y_sizes[i]);
      if (area_min != 0) {
        // Not including the term if we don't have a view is ok.
        (void)lc.AddLiteralTerm(presence_literal, area_min);
      }
    }
  }
  relaxation->linear_constraints.push_back(lc.Build());
}

void AppendLinMaxRelaxationPart1(const ConstraintProto& ct, Model* model,
                                 LinearRelaxation* relaxation) {
  auto* mapping = model->GetOrCreate<CpModelMapping>();

  // We want to linearize target = max(exprs[1], exprs[2], ..., exprs[d]).
  // Part 1: Encode target >= max(exprs[1], exprs[2], ..., exprs[d])
  const LinearExpression negated_target =
      NegationOf(mapping->GetExprFromProto(ct.lin_max().target()));
  for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
    const LinearExpression expr =
        mapping->GetExprFromProto(ct.lin_max().exprs(i));
    LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
    lc.AddLinearExpression(negated_target);
    lc.AddLinearExpression(expr);
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

// TODO(user): experiment with:
//   1) remove this code
//   2) keep this code
//   3) remove this code and create the cut generator at level 1.
void AppendMaxAffineRelaxation(const ConstraintProto& ct, Model* model,
                               LinearRelaxation* relaxation) {
  IntegerVariable var;
  std::vector<std::pair<IntegerValue, IntegerValue>> affines;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  CollectAffineExpressionWithSingleVariable(ct, mapping, &var, &affines);
  if (var == kNoIntegerVariable ||
      model->GetOrCreate<IntegerTrail>()->IsFixed(var)) {
    return;
  }

  CHECK(VariableIsPositive(var));
  const LinearExpression target_expr =
      PositiveVarExpr(mapping->GetExprFromProto(ct.lin_max().target()));
  LinearConstraintBuilder builder(model);
  if (BuildMaxAffineUpConstraint(target_expr, var, affines, model, &builder)) {
    relaxation->linear_constraints.push_back(builder.Build());
  }
}

void AddMaxAffineCutGenerator(const ConstraintProto& ct, Model* model,
                              LinearRelaxation* relaxation) {
  IntegerVariable var;
  std::vector<std::pair<IntegerValue, IntegerValue>> affines;
  auto* mapping = model->GetOrCreate<CpModelMapping>();
  CollectAffineExpressionWithSingleVariable(ct, mapping, &var, &affines);
  if (var == kNoIntegerVariable ||
      model->GetOrCreate<IntegerTrail>()->IsFixed(var)) {
    return;
  }

  // If the target is constant, propagation is enough.
  if (ct.lin_max().target().vars().empty()) return;

  const LinearExpression target_expr =
      PositiveVarExpr(mapping->GetExprFromProto(ct.lin_max().target()));
  relaxation->cut_generators.push_back(CreateMaxAffineCutGenerator(
      target_expr, var, affines, "AffineMax", model));
}

// Part 2: Encode upper bound on X.
//
// Add linking constraint to the CP solver
// sum zi = 1 and for all i, zi => max = expr_i.
void AppendLinMaxRelaxationPart2(
    IntegerVariable target, const std::vector<Literal>& alternative_literals,
    const std::vector<LinearExpression>& exprs, Model* model,
    LinearRelaxation* relaxation) {
  const int num_exprs = exprs.size();
  GenericLiteralWatcher* watcher = model->GetOrCreate<GenericLiteralWatcher>();

  // First add the CP constraints.
  for (int i = 0; i < num_exprs; ++i) {
    LinearExpression local_expr;
    local_expr.vars = NegationOf(exprs[i].vars);
    local_expr.vars.push_back(target);
    local_expr.coeffs = exprs[i].coeffs;
    local_expr.coeffs.push_back(IntegerValue(1));
    IntegerSumLE* upper_bound =
        new IntegerSumLE({alternative_literals[i]}, local_expr.vars,
                         local_expr.coeffs, exprs[i].offset, model);
    upper_bound->RegisterWith(watcher);
    model->TakeOwnership(upper_bound);
  }

  // For the relaxation, we use different constraints with a stronger linear
  // relaxation as explained in the .h
  std::vector<std::vector<IntegerValue>> sum_of_max_corner_diff(
      num_exprs, std::vector<IntegerValue>(num_exprs, IntegerValue(0)));

  // Cache coefficients.
  // TODO(user): Remove hash_map ?
  absl::flat_hash_map<std::pair<int, IntegerVariable>, IntegerValue> cache;
  for (int i = 0; i < num_exprs; ++i) {
    for (int j = 0; j < exprs[i].vars.size(); ++j) {
      cache[std::make_pair(i, exprs[i].vars[j])] = exprs[i].coeffs[j];
    }
  }
  const auto get_coeff = [&cache](IntegerVariable var, int index) {
    const auto it = cache.find(std::make_pair(index, var));
    if (it == cache.end()) return IntegerValue(0);
    return it->second;
  };

  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  std::vector<IntegerVariable> active_vars;
  for (int i = 0; i + 1 < num_exprs; ++i) {
    for (int j = i + 1; j < num_exprs; ++j) {
      active_vars = exprs[i].vars;
      active_vars.insert(active_vars.end(), exprs[j].vars.begin(),
                         exprs[j].vars.end());
      gtl::STLSortAndRemoveDuplicates(&active_vars);
      for (const IntegerVariable x_var : active_vars) {
        const IntegerValue diff = get_coeff(x_var, j) - get_coeff(x_var, i);
        if (diff == 0) continue;

        const IntegerValue lb = integer_trail->LevelZeroLowerBound(x_var);
        const IntegerValue ub = integer_trail->LevelZeroUpperBound(x_var);
        sum_of_max_corner_diff[i][j] += std::max(diff * lb, diff * ub);
        sum_of_max_corner_diff[j][i] += std::max(-diff * lb, -diff * ub);
      }
    }
  }

  for (int i = 0; i < num_exprs; ++i) {
    LinearConstraintBuilder lc(model, kMinIntegerValue, IntegerValue(0));
    lc.AddTerm(target, IntegerValue(1));
    for (int j = 0; j < exprs[i].vars.size(); ++j) {
      lc.AddTerm(exprs[i].vars[j], -exprs[i].coeffs[j]);
    }
    for (int j = 0; j < num_exprs; ++j) {
      CHECK(lc.AddLiteralTerm(alternative_literals[j],
                              -exprs[j].offset - sum_of_max_corner_diff[i][j]));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

void AppendLinearConstraintRelaxation(const ConstraintProto& ct,
                                      bool linearize_enforced_constraints,
                                      Model* model,
                                      LinearRelaxation* relaxation,
                                      ActivityBoundHelper* activity_helper) {
  auto* mapping = model->Get<CpModelMapping>();

  // Note that we ignore the holes in the domain.
  //
  // TODO(user): In LoadLinearConstraint() we already created intermediate
  // Booleans for each disjoint interval, we should reuse them here if
  // possible.
  //
  // TODO(user): process the "at most one" part of a == 1 separately?
  const IntegerValue rhs_domain_min = IntegerValue(ct.linear().domain(0));
  const IntegerValue rhs_domain_max =
      IntegerValue(ct.linear().domain(ct.linear().domain_size() - 1));
  if (rhs_domain_min == std::numeric_limits<int64_t>::min() &&
      rhs_domain_max == std::numeric_limits<int64_t>::max())
    return;

  if (!HasEnforcementLiteral(ct)) {
    LinearConstraintBuilder lc(model, rhs_domain_min, rhs_domain_max);
    for (int i = 0; i < ct.linear().vars_size(); i++) {
      const int ref = ct.linear().vars(i);
      const int64_t coeff = ct.linear().coeffs(i);
      lc.AddTerm(mapping->Integer(ref), IntegerValue(coeff));
    }
    relaxation->linear_constraints.push_back(lc.Build());
    return;
  }

  // Reified version.
  if (!linearize_enforced_constraints) return;

  // We linearize fully reified constraints of size 1 all together for a given
  // variable. But we need to process half-reified ones or constraint with
  // more than one enforcement.
  //
  // TODO(user): Use cleaner "already loaded" logic, and mark as such constraint
  // already encoded by code like AppendRelaxationForEqualityEncoding().
  if (!mapping->IsHalfEncodingConstraint(&ct) && ct.linear().vars_size() <= 1 &&
      ct.enforcement_literal().size() <= 1) {
    return;
  }

  std::vector<Literal> enforcing_literals;
  enforcing_literals.reserve(ct.enforcement_literal_size());
  for (const int enforcement_ref : ct.enforcement_literal()) {
    enforcing_literals.push_back(mapping->Literal(enforcement_ref));
  }

  // Compute min/max activity.
  std::vector<std::pair<int, int64_t>> bool_terms;
  IntegerValue min_activity(0);
  IntegerValue max_activity(0);
  const auto integer_trail = model->GetOrCreate<IntegerTrail>();
  for (int i = 0; i < ct.linear().vars_size(); i++) {
    const int ref = ct.linear().vars(i);
    const IntegerValue coeff(ct.linear().coeffs(i));
    const IntegerVariable int_var = mapping->Integer(ref);

    // Everything here should have a view.
    CHECK_NE(int_var, kNoIntegerVariable);

    const IntegerValue lb = integer_trail->LowerBound(int_var);
    const IntegerValue ub = integer_trail->UpperBound(int_var);
    if (lb == 0 && ub == 1 && activity_helper != nullptr) {
      bool_terms.push_back({ref, coeff.value()});
    } else {
      if (coeff > 0) {
        min_activity += coeff * lb;
        max_activity += coeff * ub;
      } else {
        min_activity += coeff * ub;
        max_activity += coeff * lb;
      }
    }
  }
  if (activity_helper != nullptr) {
    min_activity +=
        IntegerValue(activity_helper->ComputeMinActivity(bool_terms));
    max_activity +=
        IntegerValue(activity_helper->ComputeMaxActivity(bool_terms));
  }

  if (rhs_domain_min > min_activity) {
    // And(ei) => terms >= rhs_domain_min
    // <=> Sum_i (~ei * (rhs_domain_min - min_activity)) + terms >=
    // rhs_domain_min
    LinearConstraintBuilder lc(model, rhs_domain_min, kMaxIntegerValue);
    for (const Literal& literal : enforcing_literals) {
      CHECK(
          lc.AddLiteralTerm(literal.Negated(), rhs_domain_min - min_activity));
    }
    for (int i = 0; i < ct.linear().vars_size(); i++) {
      const int ref = ct.linear().vars(i);
      const IntegerValue coeff(ct.linear().coeffs(i));
      const IntegerVariable int_var = mapping->Integer(ref);
      lc.AddTerm(int_var, coeff);
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
  if (rhs_domain_max < max_activity) {
    // And(ei) => terms <= rhs_domain_max
    // <=> Sum_i (~ei * (rhs_domain_max - max_activity)) + terms <=
    // rhs_domain_max
    LinearConstraintBuilder lc(model, kMinIntegerValue, rhs_domain_max);
    for (const Literal& literal : enforcing_literals) {
      CHECK(
          lc.AddLiteralTerm(literal.Negated(), rhs_domain_max - max_activity));
    }
    for (int i = 0; i < ct.linear().vars_size(); i++) {
      const int ref = ct.linear().vars(i);
      const IntegerValue coeff(ct.linear().coeffs(i));
      const IntegerVariable int_var = mapping->Integer(ref);
      lc.AddTerm(int_var, coeff);
    }
    relaxation->linear_constraints.push_back(lc.Build());
  }
}

// Add a static and a dynamic linear relaxation of the CP constraint to the set
// of linear constraints. The highest linearization_level is, the more types of
// constraint we encode. This method should be called only for
// linearization_level > 0. The static part is just called a relaxation and is
// called at the root node of the search. The dynamic part is implemented
// through a set of linear cut generators that will be called throughout the
// search.
//
// TODO(user): In full generality, we could encode all the constraint as an LP.
// TODO(user): Add unit tests for this method.
// TODO(user): Remove and merge with model loading.
void TryToLinearizeConstraint(const CpModelProto& /*model_proto*/,
                              const ConstraintProto& ct,
                              int linearization_level, Model* model,
                              LinearRelaxation* relaxation,
                              ActivityBoundHelper* activity_helper) {
  CHECK_EQ(model->GetOrCreate<SatSolver>()->CurrentDecisionLevel(), 0);
  DCHECK_GT(linearization_level, 0);

  const int old_index = relaxation->linear_constraints.size();
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr: {
      if (linearization_level > 1) {
        AppendBoolOrRelaxation(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kBoolAnd: {
      if (linearization_level > 1) {
        AppendBoolAndRelaxation(ct, model, relaxation, activity_helper);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kAtMostOne: {
      AppendAtMostOneRelaxation(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kExactlyOne: {
      AppendExactlyOneRelaxation(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kIntProd: {
      const LinearArgumentProto& int_prod = ct.int_prod();
      if (int_prod.exprs_size() == 2 &&
          LinearExpressionProtosAreEqual(int_prod.exprs(0),
                                         int_prod.exprs(1))) {
        AppendSquareRelaxation(ct, model, relaxation);
        AddSquareCutGenerator(ct, linearization_level, model, relaxation);
      } else {
        // No relaxation, just a cut generator .
        AddIntProdCutGenerator(ct, linearization_level, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kLinMax: {
      AppendLinMaxRelaxationPart1(ct, model, relaxation);
      const bool is_affine_max =
          ExpressionsContainsOnlyOneVar(ct.lin_max().exprs());
      if (is_affine_max) {
        AppendMaxAffineRelaxation(ct, model, relaxation);
      }

      // Add cut generators.
      if (linearization_level > 1) {
        if (is_affine_max) {
          AddMaxAffineCutGenerator(ct, model, relaxation);
        } else if (ct.lin_max().exprs().size() < 100) {
          AddLinMaxCutGenerator(ct, model, relaxation);
        }
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kAllDiff: {
      AddAllDiffRelaxationAndCutGenerator(ct, linearization_level, model,
                                          relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kLinear: {
      AppendLinearConstraintRelaxation(
          ct, /*linearize_enforced_constraints=*/linearization_level > 1, model,
          relaxation, activity_helper);
      break;
    }
    case ConstraintProto::ConstraintCase::kCircuit: {
      AppendCircuitRelaxation(ct, model, relaxation);
      if (linearization_level > 1) {
        AddCircuitCutGenerator(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kRoutes: {
      AppendRoutesRelaxation(ct, model, relaxation);
      if (linearization_level > 1) {
        AddRoutesCutGenerator(ct, model, relaxation);
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap: {
      AppendNoOverlapRelaxationAndCutGenerator(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kCumulative: {
      AppendCumulativeRelaxationAndCutGenerator(ct, model, relaxation);
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap2D: {
      // TODO(user): Use the same pattern as the other 2 scheduling methods:
      //   - single function
      //   - generate helpers once
      //
      // Adds an energetic relaxation (sum of areas fits in bounding box).
      AppendNoOverlap2dRelaxation(ct, model, relaxation);
      if (linearization_level > 1) {
        // Adds a completion time cut generator and an energetic cut generator.
        AddNoOverlap2dCutGenerator(ct, model, relaxation);
      }
      break;
    }
    default: {
    }
  }

  if (DEBUG_MODE) {
    const auto& integer_trail = *model->GetOrCreate<IntegerTrail>();
    for (int i = old_index; i < relaxation->linear_constraints.size(); ++i) {
      const bool issue =
          PossibleOverflow(integer_trail, relaxation->linear_constraints[i]);
      if (issue) {
        LOG(INFO) << "Possible overflow in linearization of: "
                  << google::protobuf::ShortFormat(ct);
      }
    }
  }
}

// Cut generators.

void AddIntProdCutGenerator(const ConstraintProto& ct, int linearization_level,
                            Model* m, LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  if (ct.int_prod().exprs_size() != 2) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();

  // Constraint is z == x * y.
  AffineExpression z = mapping->Affine(ct.int_prod().target());
  AffineExpression x = mapping->Affine(ct.int_prod().exprs(0));
  AffineExpression y = mapping->Affine(ct.int_prod().exprs(1));

  IntegerTrail* const integer_trail = m->GetOrCreate<IntegerTrail>();
  IntegerValue x_lb = integer_trail->LowerBound(x);
  IntegerValue x_ub = integer_trail->UpperBound(x);
  IntegerValue y_lb = integer_trail->LowerBound(y);
  IntegerValue y_ub = integer_trail->UpperBound(y);

  // We currently only support variables with non-negative domains.
  if (x_lb < 0 && x_ub > 0) return;
  if (y_lb < 0 && y_ub > 0) return;

  // Change signs to return to the case where all variables are a domain
  // with non negative values only.
  if (x_ub <= 0) {
    x = x.Negated();
    z = z.Negated();
  }
  if (y_ub <= 0) {
    y = y.Negated();
    z = z.Negated();
  }

  relaxation->cut_generators.push_back(CreatePositiveMultiplicationCutGenerator(
      z, x, y, linearization_level, m));
}

void AppendSquareRelaxation(const ConstraintProto& ct, Model* m,
                            LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  IntegerTrail* const integer_trail = m->GetOrCreate<IntegerTrail>();

  // Constraint is square == x * x.
  AffineExpression square = mapping->Affine(ct.int_prod().target());
  AffineExpression x = mapping->Affine(ct.int_prod().exprs(0));
  IntegerValue x_lb = integer_trail->LowerBound(x);
  IntegerValue x_ub = integer_trail->UpperBound(x);

  if (x_lb == x_ub) return;

  // We currently only support variables with non-negative domains.
  if (x_lb < 0 && x_ub > 0) return;

  // Change the sigh of x if its domain is non-positive.
  if (x_ub <= 0) {
    x = x.Negated();
    const IntegerValue tmp = x_ub;
    x_ub = -x_lb;
    x_lb = -tmp;
  }

  // Check for potential overflows.
  if (x_ub > (int64_t{1} << 31)) return;
  DCHECK_GE(x_lb, 0);

  relaxation->linear_constraints.push_back(
      ComputeHyperplanAboveSquare(x, square, x_lb, x_ub, m));

  relaxation->linear_constraints.push_back(
      ComputeHyperplanBelowSquare(x, square, x_lb, m));
  // TODO(user): We could add all or some below_hyperplans.
  if (x_lb + 1 < x_ub) {
    // The hyperplan will use x_ub - 1 and x_ub.
    relaxation->linear_constraints.push_back(
        ComputeHyperplanBelowSquare(x, square, x_ub - 1, m));
  }
}

void AddSquareCutGenerator(const ConstraintProto& ct, int linearization_level,
                           Model* m, LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  IntegerTrail* const integer_trail = m->GetOrCreate<IntegerTrail>();

  // Constraint is square == x * x.
  const AffineExpression square = mapping->Affine(ct.int_prod().target());
  AffineExpression x = mapping->Affine(ct.int_prod().exprs(0));
  const IntegerValue x_lb = integer_trail->LowerBound(x);
  const IntegerValue x_ub = integer_trail->UpperBound(x);

  // We currently only support variables with non-negative domains.
  if (x_lb < 0 && x_ub > 0) return;

  // Change the sigh of x if its domain is non-positive.
  if (x_ub <= 0) {
    x = x.Negated();
  }

  relaxation->cut_generators.push_back(
      CreateSquareCutGenerator(square, x, linearization_level, m));
}

void AddAllDiffRelaxationAndCutGenerator(const ConstraintProto& ct,
                                         int linearization_level, Model* m,
                                         LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  const int num_exprs = ct.all_diff().exprs_size();

  const std::vector<AffineExpression> exprs =
      mapping->Affines(ct.all_diff().exprs());

  // Build union of affine expressions domains to check if this is a
  // permutation.
  Domain union_of_domains;
  for (const AffineExpression& expr : exprs) {
    if (integer_trail->IsFixed(expr)) {
      union_of_domains = union_of_domains.UnionWith(
          Domain(integer_trail->FixedValue(expr).value()));
    } else {
      union_of_domains = union_of_domains.UnionWith(
          integer_trail->InitialVariableDomain(expr.var)
              .MultiplicationBy(expr.coeff.value())
              .AdditionWith(Domain(expr.constant.value())));
    }
  }

  if (union_of_domains.Size() == num_exprs) {
    // In case of a permutation, the linear constraint is tight.
    int64_t sum_of_values = 0;
    for (const int64_t v : union_of_domains.Values()) {
      sum_of_values += v;
    }
    LinearConstraintBuilder relax(m, sum_of_values, sum_of_values);
    for (const AffineExpression& expr : exprs) {
      relax.AddTerm(expr, 1);
    }
    relaxation->linear_constraints.push_back(relax.Build());
  } else if (num_exprs <=
                 m->GetOrCreate<SatParameters>()->max_all_diff_cut_size() &&
             linearization_level > 1) {
    relaxation->cut_generators.push_back(
        CreateAllDifferentCutGenerator(exprs, m));
  }
}

bool IntervalIsVariable(const IntervalVariable interval,
                        IntervalsRepository* intervals_repository) {
  // Ignore absent rectangles.
  if (intervals_repository->IsAbsent(interval)) {
    return false;
  }

  // Checks non-present intervals.
  if (!intervals_repository->IsPresent(interval)) {
    return true;
  }

  // Checks variable sized intervals.
  if (intervals_repository->MinSize(interval) !=
      intervals_repository->MaxSize(interval)) {
    return true;
  }

  return false;
}

void AddCumulativeCutGenerator(const AffineExpression& capacity,
                               SchedulingConstraintHelper* helper,
                               SchedulingDemandHelper* demands_helper,
                               const std::optional<AffineExpression>& makespan,
                               Model* m, LinearRelaxation* relaxation) {
  relaxation->cut_generators.push_back(CreateCumulativeTimeTableCutGenerator(
      helper, demands_helper, capacity, m));
  relaxation->cut_generators.push_back(
      CreateCumulativeCompletionTimeCutGenerator(helper, demands_helper,
                                                 capacity, m));
  relaxation->cut_generators.push_back(CreateCumulativePrecedenceCutGenerator(
      helper, demands_helper, capacity, m));

  // Checks if at least one rectangle has a variable size, is optional, or if
  // the demand or the capacity are variable.
  bool has_variable_part = false;
  IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
  for (int i = 0; i < helper->NumTasks(); ++i) {
    if (!helper->SizeIsFixed(i)) {
      has_variable_part = true;
      break;
    }
    // Checks variable demand.
    if (!demands_helper->DemandIsFixed(i)) {
      has_variable_part = true;
      break;
    }
  }
  if (has_variable_part || !integer_trail->IsFixed(capacity)) {
    relaxation->cut_generators.push_back(CreateCumulativeEnergyCutGenerator(
        helper, demands_helper, capacity, makespan, m));
  }
}

void AddNoOverlapCutGenerator(SchedulingConstraintHelper* helper,
                              const std::optional<AffineExpression>& makespan,
                              Model* m, LinearRelaxation* relaxation) {
  relaxation->cut_generators.push_back(
      CreateNoOverlapPrecedenceCutGenerator(helper, m));
  relaxation->cut_generators.push_back(
      CreateNoOverlapCompletionTimeCutGenerator(helper, m));

  // Checks if at least one rectangle has a variable size or is optional.
  bool has_variable_or_optional_part = false;
  for (int i = 0; i < helper->NumTasks(); ++i) {
    if (helper->IsAbsent(i)) continue;
    if (!helper->SizeIsFixed(i) || !helper->IsPresent(i)) {
      has_variable_or_optional_part = true;
      break;
    }
  }
  if (has_variable_or_optional_part) {
    relaxation->cut_generators.push_back(
        CreateNoOverlapEnergyCutGenerator(helper, makespan, m));
  }
}

void AddNoOverlap2dCutGenerator(const ConstraintProto& ct, Model* m,
                                LinearRelaxation* relaxation) {
  if (HasEnforcementLiteral(ct)) return;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  std::vector<IntervalVariable> x_intervals =
      mapping->Intervals(ct.no_overlap_2d().x_intervals());
  std::vector<IntervalVariable> y_intervals =
      mapping->Intervals(ct.no_overlap_2d().y_intervals());

  IntervalsRepository* intervals_repository =
      m->GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* x_helper =
      intervals_repository->GetOrCreateHelper(x_intervals);
  SchedulingConstraintHelper* y_helper =
      intervals_repository->GetOrCreateHelper(y_intervals);

  relaxation->cut_generators.push_back(
      CreateNoOverlap2dCompletionTimeCutGenerator(x_helper, y_helper, m));

  // Checks if at least one rectangle has a variable dimension or is optional.
  bool has_variable_part = false;
  for (int i = 0; i < x_intervals.size(); ++i) {
    // Ignore absent rectangles.
    if (intervals_repository->IsAbsent(x_intervals[i]) ||
        intervals_repository->IsAbsent(y_intervals[i])) {
      continue;
    }

    // Checks non-present intervals.
    if (!intervals_repository->IsPresent(x_intervals[i]) ||
        !intervals_repository->IsPresent(y_intervals[i])) {
      has_variable_part = true;
      break;
    }

    // Checks variable sized intervals.
    if (intervals_repository->MinSize(x_intervals[i]) !=
            intervals_repository->MaxSize(x_intervals[i]) ||
        intervals_repository->MinSize(y_intervals[i]) !=
            intervals_repository->MaxSize(y_intervals[i])) {
      has_variable_part = true;
      break;
    }
  }

  if (!has_variable_part) return;

  SchedulingDemandHelper* x_demands_helper =
      intervals_repository->GetOrCreateDemandHelper(y_helper,
                                                    x_helper->Sizes());
  SchedulingDemandHelper* y_demands_helper =
      intervals_repository->GetOrCreateDemandHelper(x_helper,
                                                    y_helper->Sizes());

  std::vector<std::vector<LiteralValueValue>> energies;
  const int num_rectangles = x_helper->NumTasks();
  auto* product_decomposer = m->GetOrCreate<ProductDecomposer>();
  for (int i = 0; i < num_rectangles; ++i) {
    energies.push_back(product_decomposer->TryToDecompose(
        x_helper->Sizes()[i], y_helper->Sizes()[i]));
  }

  relaxation->cut_generators.push_back(CreateNoOverlap2dEnergyCutGenerator(
      x_helper, y_helper, x_demands_helper, y_demands_helper, energies, m));
}

void AddLinMaxCutGenerator(const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation) {
  if (!m->GetOrCreate<SatParameters>()->add_lin_max_cuts()) return;
  if (HasEnforcementLiteral(ct)) return;

  // TODO(user): Support linearization of general target expression.
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  if (ct.lin_max().target().vars_size() != 1) return;
  if (ct.lin_max().target().coeffs(0) != 1) return;
  if (ct.lin_max().target().offset() != 0) return;

  const IntegerVariable target =
      mapping->Integer(ct.lin_max().target().vars(0));
  std::vector<LinearExpression> exprs;
  exprs.reserve(ct.lin_max().exprs_size());
  for (int i = 0; i < ct.lin_max().exprs_size(); ++i) {
    // Note: Cut generator requires all expressions to contain only positive
    // vars.
    exprs.push_back(
        PositiveVarExpr(mapping->GetExprFromProto(ct.lin_max().exprs(i))));
  }

  const std::vector<Literal> alternative_literals =
      CreateAlternativeLiteralsWithView(exprs.size(), m, relaxation);

  // TODO(user): Move this out of here.
  //
  // Add initial big-M linear relaxation.
  // z_vars[i] == 1 <=> target = exprs[i].
  AppendLinMaxRelaxationPart2(target, alternative_literals, exprs, m,
                              relaxation);

  std::vector<IntegerVariable> z_vars;
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  for (const Literal lit : alternative_literals) {
    z_vars.push_back(encoder->GetLiteralView(lit));
    CHECK_NE(z_vars.back(), kNoIntegerVariable);
  }
  relaxation->cut_generators.push_back(
      CreateLinMaxCutGenerator(target, exprs, z_vars, m));
}

// If we have an exactly one between literals l_i, and each l_i => var ==
// value_i, then we can add a strong linear relaxation: var = sum l_i * value_i.
//
// This codes detect this and add the corresponding linear equations.
//
// TODO(user): We can do something similar with just an at most one, however
// it is harder to detect that if all literal are false then none of the implied
// value can be taken.
void AppendElementEncodingRelaxation(Model* m, LinearRelaxation* relaxation) {
  auto* integer_trail = m->GetOrCreate<IntegerTrail>();
  auto* element_encodings = m->GetOrCreate<ElementEncodings>();

  int num_exactly_one_elements = 0;
  for (const IntegerVariable var :
       element_encodings->GetElementEncodedVariables()) {
    for (const auto& [index, literal_value_list] :
         element_encodings->Get(var)) {
      IntegerValue min_value = kMaxIntegerValue;
      for (const auto& literal_value : literal_value_list) {
        min_value = std::min(min_value, literal_value.value);
      }

      LinearConstraintBuilder builder(m, -min_value, -min_value);
      builder.AddTerm(var, IntegerValue(-1));
      for (const auto& [value, literal] : literal_value_list) {
        const IntegerValue delta_min = value - min_value;
        if (delta_min != 0) {
          // If the term has no view, we abort.
          if (!builder.AddLiteralTerm(literal, delta_min)) {
            return;
          }
        }
      }
      ++num_exactly_one_elements;
      LinearConstraint lc = builder.Build();
      if (!PossibleOverflow(*integer_trail, lc)) {
        relaxation->linear_constraints.push_back(std::move(lc));
      }
    }
  }

  if (num_exactly_one_elements != 0) {
    auto* logger = m->GetOrCreate<SolverLogger>();
    SOLVER_LOG(logger,
               "[ElementLinearRelaxation]"
               " #from_exactly_one:",
               num_exactly_one_elements);
  }
}

LinearRelaxation ComputeLinearRelaxation(const CpModelProto& model_proto,
                                         Model* m) {
  LinearRelaxation relaxation;
  const SatParameters& params = *m->GetOrCreate<SatParameters>();

  // Collect AtMostOne to compute better Big-M.
  ActivityBoundHelper activity_bound_helper;
  if (params.linearization_level() > 1) {
    activity_bound_helper.AddAllAtMostOnes(model_proto);
  }

  // Linearize the constraints.
  for (const auto& ct : model_proto.constraints()) {
    TryToLinearizeConstraint(model_proto, ct, params.linearization_level(), m,
                             &relaxation, &activity_bound_helper);
  }

  // Linearize the encoding of variable that are fully encoded.
  int num_loose_equality_encoding_relaxations = 0;
  int num_tight_equality_encoding_relaxations = 0;
  int num_inequality_encoding_relaxations = 0;
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsBoolean(i)) continue;

    const IntegerVariable var = mapping->Integer(i);
    if (m->Get(IsFixed(var))) continue;

    // We first try to linearize the values encoding.
    AppendRelaxationForEqualityEncoding(
        var, *m, &relaxation, &num_tight_equality_encoding_relaxations,
        &num_loose_equality_encoding_relaxations);

    // Then we try to linearize the inequality encoding. Note that on some
    // problem like pizza27i.mps.gz, adding both equality and inequality
    // encoding is a must.
    //
    // Even if the variable is fully encoded, sometimes not all its associated
    // literal have a view (if they are not part of the original model for
    // instance).
    //
    // TODO(user): Should we add them to the LP anyway? this isn't clear as
    // we can sometimes create a lot of Booleans like this.
    const int old = relaxation.linear_constraints.size();
    AppendPartialGreaterThanEncodingRelaxation(var, *m, &relaxation);
    if (relaxation.linear_constraints.size() > old) {
      ++num_inequality_encoding_relaxations;
    }
  }

  // TODO(user): This is similar to AppendRelaxationForEqualityEncoding() above.
  // Investigate if we can merge the code.
  if (params.linearization_level() >= 2) {
    AppendElementEncodingRelaxation(m, &relaxation);
  }

  // TODO(user): I am not sure this is still needed. Investigate and explain why
  // or remove.
  if (!m->GetOrCreate<SatSolver>()->FinishPropagation()) {
    return relaxation;
  }

  // We display the stats before linearizing the at most ones.
  auto* logger = m->GetOrCreate<SolverLogger>();
  if (num_tight_equality_encoding_relaxations != 0 ||
      num_loose_equality_encoding_relaxations != 0 ||
      num_inequality_encoding_relaxations != 0) {
    SOLVER_LOG(logger,
               "[EncodingLinearRelaxation]"
               " #tight_equality:",
               num_tight_equality_encoding_relaxations,
               " #loose_equality:", num_loose_equality_encoding_relaxations,
               " #inequality:", num_inequality_encoding_relaxations);
  }
  if (!relaxation.linear_constraints.empty() ||
      !relaxation.at_most_ones.empty()) {
    SOLVER_LOG(logger,
               "[LinearRelaxationBeforeCliqueExpansion]"
               " #linear:",
               relaxation.linear_constraints.size(),
               " #at_most_ones:", relaxation.at_most_ones.size());
  }

  // Linearize the at most one constraints. Note that we transform them
  // into maximum "at most one" first and we removes redundant ones.
  m->GetOrCreate<BinaryImplicationGraph>()->TransformIntoMaxCliques(
      &relaxation.at_most_ones,
      SafeDoubleToInt64(params.merge_at_most_one_work_limit()));
  for (const std::vector<Literal>& at_most_one : relaxation.at_most_ones) {
    if (at_most_one.empty()) continue;

    LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(1));
    for (const Literal literal : at_most_one) {
      // Note that it is okay to simply ignore the literal if it has no
      // integer view.
      const bool unused ABSL_ATTRIBUTE_UNUSED =
          lc.AddLiteralTerm(literal, IntegerValue(1));
    }
    relaxation.linear_constraints.push_back(lc.Build());
  }

  // We converted all at_most_one to LP constraints, so we need to clear them
  // so that we don't do extra work in the connected component computation.
  relaxation.at_most_ones.clear();

  // Propagate unary constraints.
  {
    SatSolver* sat_solver = m->GetOrCreate<SatSolver>();
    CHECK_EQ(sat_solver->CurrentDecisionLevel(), 0);
    IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
    for (const LinearConstraint& lc : relaxation.linear_constraints) {
      if (lc.num_terms == 0) {
        if (lc.lb > 0 || lc.ub < 0) {
          sat_solver->NotifyThatModelIsUnsat();
          return relaxation;
        }
      } else if (lc.num_terms == 1) {
        const AffineExpression expr(lc.vars[0], lc.coeffs[0]);
        if (lc.lb > integer_trail->LevelZeroLowerBound(expr)) {
          if (!integer_trail->Enqueue(expr.GreaterOrEqual(lc.lb), {}, {})) {
            sat_solver->NotifyThatModelIsUnsat();
            return relaxation;
          }
        }
        if (lc.ub < integer_trail->LevelZeroUpperBound(expr)) {
          if (!integer_trail->Enqueue(expr.LowerOrEqual(lc.ub), {}, {})) {
            sat_solver->NotifyThatModelIsUnsat();
            return relaxation;
          }
        }
      }
    }
    if (!sat_solver->FinishPropagation()) return relaxation;
  }

  // Remove size one LP constraints from the main algorithms, they are not
  // useful.
  relaxation.linear_constraints.erase(
      std::remove_if(
          relaxation.linear_constraints.begin(),
          relaxation.linear_constraints.end(),
          [](const LinearConstraint& lc) { return lc.num_terms <= 1; }),
      relaxation.linear_constraints.end());

  if (!relaxation.linear_constraints.empty() ||
      !relaxation.cut_generators.empty()) {
    SOLVER_LOG(logger,
               "[FinalLinearRelaxation]"
               " #linear:",
               relaxation.linear_constraints.size(),
               " #cut_generators:", relaxation.cut_generators.size());
  }

  return relaxation;
}

}  // namespace sat
}  // namespace operations_research
