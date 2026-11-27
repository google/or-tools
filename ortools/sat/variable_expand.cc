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

#include "ortools/sat/variable_expand.h"

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_status.h"
#include "ortools/sat/solution_crush.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

namespace {
enum class EncodingLinear1Type {
  kVarEqValue = 0,
  kVarNeValue,
  kVarGeValue,
  kVarLeValue,
  kVarInDomain,
};
const int kNumEncodingLinear1Types = 5;

template <typename Sink>
void AbslStringify(Sink& sink, EncodingLinear1Type type) {
  switch (type) {
    case EncodingLinear1Type::kVarEqValue:
      sink.Append("kVarEqValue");
      return;
    case EncodingLinear1Type::kVarNeValue:
      sink.Append("kVarNeValue");
      return;
    case EncodingLinear1Type::kVarGeValue:
      sink.Append("kVarGeValue");
      return;
    case EncodingLinear1Type::kVarLeValue:
      sink.Append("kVarLeValue");
      return;
    case EncodingLinear1Type::kVarInDomain:
      sink.Append("kVarInDomain");
      return;
  }
}

enum class EncodingLinear1Status {
  kOk = 0,
  kIgnore,
  kFalse,
};

struct EncodingLinear1 {
  EncodingLinear1Type type;
  int64_t value = kint64min;
  Domain rhs;  // Only used for kVarInDomain.
  int enforcement_literal;
  int constraint_index;

  std::string ToString() const {
    return absl::StrCat("EncodingLinear1(type: ", type, ", value: ", value,
                        ", rhs: ", rhs.ToString(),
                        ", enforcement_literal: ", enforcement_literal,
                        ", constraint_index: ", constraint_index, ")");
  }
};

std::pair<std::optional<EncodingLinear1>, EncodingLinear1Status> ProcessLinear1(
    const ConstraintProto& ct, int constraint_index, const Domain& var_domain) {
  EncodingLinear1 lin;
  lin.enforcement_literal = ct.enforcement_literal(0);
  lin.constraint_index = constraint_index;

  const Domain rhs = ReadDomainFromProto(ct.linear())
                         .InverseMultiplicationBy(ct.linear().coeffs(0))
                         .IntersectionWith(var_domain);
  if (rhs.IsEmpty()) {
    return {std::nullopt, EncodingLinear1Status::kFalse};
  } else if (rhs.IsFixed()) {
    // Because of the intersection above.
    DCHECK(var_domain.Contains(rhs.FixedValue()));
    lin.type = EncodingLinear1Type::kVarEqValue;
    lin.value = rhs.FixedValue();
  } else {
    const Domain complement = var_domain.IntersectionWith(rhs.Complement());
    if (complement.IsEmpty()) {
      return {std::nullopt, EncodingLinear1Status::kIgnore};
    } else if (complement.IsFixed()) {
      DCHECK(var_domain.Contains(complement.FixedValue()));
      lin.type = EncodingLinear1Type::kVarNeValue;
      lin.value = complement.FixedValue();
    } else if (rhs.Min() > complement.Max()) {
      lin.type = EncodingLinear1Type::kVarGeValue;
      lin.value = rhs.Min();
    } else if (rhs.Max() < complement.Min()) {
      lin.type = EncodingLinear1Type::kVarLeValue;
      lin.value = rhs.Max();
    } else {
      lin.type = EncodingLinear1Type::kVarInDomain;
      lin.rhs = rhs;
    }
  }
  return {lin, EncodingLinear1Status::kOk};
}

template <typename Sink>
void AbslStringify(Sink& sink, const EncodingLinear1& lin) {
  sink.Append(lin.ToString());
}

}  // namespace

ValueEncoding::ValueEncoding(int var, PresolveContext* context)
    : var_(var), var_domain_(context->DomainOf(var)), context_(context) {}

void ValueEncoding::AddReferencedValueToEncode(int64_t value) {
  DCHECK(!is_closed_);
  encoded_values_.push_back(value);
  referenced_encoded_values_.push_back(value);
}

void ValueEncoding::AddOptionalValueToEncode(int64_t value) {
  DCHECK(!is_closed_);
  encoded_values_.push_back(value);
}

void ValueEncoding::CanonicalizeEncodedValuesAndAddEscapeValue(
    bool push_down_when_unconstrained, bool has_le_ge_linear1) {
  if (!is_closed_) {
    // Note: is_closed is true if we have forced the full encoding.
    if (!has_le_ge_linear1) {
      // We forget all optional encoded values as we just need one escape value.
      encoded_values_ = referenced_encoded_values_;
    }

    gtl::STLSortAndRemoveDuplicates(&encoded_values_);
    // Add an escape value to the existing encoded values when the encoding is
    // not complete. The choice of the escape value depends on the presence of
    // an objective and its direction. An encoded value not actually referenced
    // by a linear1 constraint can be used as an escape value.
    if (encoded_values_.size() < var_domain_.Size()) {
      const Domain residual = var_domain_.IntersectionWith(
          Domain::FromValues(referenced_encoded_values_).Complement());
      const int64_t escape_value =
          push_down_when_unconstrained ? residual.Min() : residual.Max();
      // NOTE: escape_value can already be in all_encoded_values_.
      encoded_values_.push_back(escape_value);
      gtl::STLSortAndRemoveDuplicates(&encoded_values_);
      if (!has_le_ge_linear1) {
        unique_escape_value_ = escape_value;
      }
    }
    is_closed_ = true;
    is_fully_encoded_ = encoded_values_.size() == var_domain_.Size();
  }
}

const std::vector<int64_t>& ValueEncoding::encoded_values() const {
  DCHECK(is_closed_);
  return encoded_values_;
}

bool ValueEncoding::empty() const {
  DCHECK(is_closed_);
  return encoded_values_.empty();
}

bool ValueEncoding::is_fully_encoded() const {
  DCHECK(is_closed_);
  return is_fully_encoded_;
}

std::optional<int64_t> ValueEncoding::unique_escape_value() const {
  DCHECK(is_closed_);
  return unique_escape_value_;
}

void ValueEncoding::ForceFullEncoding() {
  encoded_values_.clear();
  referenced_encoded_values_.clear();
  encoded_values_.reserve(var_domain_.Size());
  for (const int64_t v : var_domain_.Values()) {
    encoded_values_.push_back(v);
  }
  is_closed_ = true;
  is_fully_encoded_ = true;
}

void ValueEncoding::CreateAllValueEncodingLiterals() {
  DCHECK(is_closed_);
  for (const int64_t value : encoded_values_) {
    encoding_[value] = context_->GetOrCreateVarValueEncoding(var_, value);
  }
}

int ValueEncoding::literal(int64_t value) const {
  DCHECK(is_closed_);
  const auto it = encoding_.find(value);
  DCHECK(it != encoding_.end());
  return it->second;
}

const absl::btree_map<int64_t, int>& ValueEncoding::encoding() const {
  DCHECK(is_closed_);
  return encoding_;
}

OrderEncoding::OrderEncoding(int var, PresolveContext* context,
                             SolutionCrush& solution_crush)
    : var_(var),
      var_domain_(context->DomainOf(var)),
      context_(context),
      solution_crush_(solution_crush) {}

void OrderEncoding::InsertLeLiteral(int64_t value, int literal) {
  if (!tmp_le_to_literals_[value].insert(literal).second) return;
  DCHECK_LT(value, var_domain_.Max());
  const int64_t next_value = var_domain_.ValueAtOrAfter(value + 1);
  if (tmp_ge_to_literals_[next_value].contains(NegatedRef(literal))) {
    const auto [it, inserted] = encoded_le_literal_.insert({value, literal});
    if (!inserted) {
      VLOG(2) << "Duplicate var_le_value literal: " << literal
              << " for value: " << value << " previous: " << it->second;
    }
  }
}

void OrderEncoding::InsertGeLiteral(int64_t value, int literal) {
  if (!tmp_ge_to_literals_[value].insert(literal).second) return;
  DCHECK_GT(value, var_domain_.Min());
  const int64_t previous_value = var_domain_.ValueAtOrBefore(value - 1);
  if (tmp_le_to_literals_[previous_value].contains(NegatedRef(literal))) {
    const auto [it, inserted] =
        encoded_le_literal_.insert({previous_value, NegatedRef(literal)});
    if (!inserted) {
      VLOG(2) << "Duplicate var_le_value literal: " << NegatedRef(literal)
              << " for value: " << previous_value
              << " previous: " << it->second;
    }
  }
}

// In the following examples, x has 5 values: 0, 1, 2, 3, 4, and some order
// encoding literals.
//      0       1      2       3       4
//   x_le_0  x_le_1          x_le_3
//           x_ge_1          x_ge_3  x_ge_4
//
// x_le_0 => not(x == 1) && x_le_1
// x_le_1 => not(x == 2) && not(x == 3) && x_le_3
//
// x_ge_1 => not(x == 0)
// x_ge_3 => not(x == 1) && not(x == 2) && x_ge_1
// x_ge_4 => not(x == 3) && x_ge_3
//
// x_le_0 => x == 0
// x_le_1 => x == 1 || x_le_0
// x_le_3 => x == 3 || x == 2 || x_le_1
//
// x_ge_1 => x == 1 || x == 2 || x_ge_3
// x_ge_3 => x == 3 || x == x_ge_4
// x_ge_4 => x == 4
//
// If we have x_le_0 and x_ge_4, then we can infer x_le_4 and x_ge_0.
// This is done by the code below.
void OrderEncoding::CreateAllOrderEncodingLiterals(
    const ValueEncoding& values) {
  CollectAllOrderEncodingValues();
  if (encoded_le_literal_.empty()) return;

  if (DEBUG_MODE) {
    // Check that all values are present in the encoding.
    for (const auto& [value, literal] : encoded_le_literal_) {
      CHECK(values.encoding().contains(value));
      CHECK(values.encoding().contains(var_domain_.ValueAtOrAfter(value + 1)))
          << "Cannot find " << var_domain_.ValueAtOrAfter(value + 1)
          << " for var <= " << value;
    }
  }

  const int64_t max_le_value = encoded_le_literal_.rbegin()->first;
  const int64_t max_ge_value = var_domain_.ValueAtOrAfter(max_le_value + 1);
  bool has_not_le = false;
  int not_le_enf = 0;
  std::vector<int> not_le_lits;
  bool has_not_ge = true;
  std::vector<int> not_ge_lits;
  bool has_le = true;
  std::vector<int> le_lits;
  bool has_ge = false;
  int ge_enf = 0;
  std::vector<int> ge_lits;

  const auto flush_bool_or = [this](int enf, absl::Span<const int> lits) {
    if (lits.empty()) return;
    context_->AddEnforcedConstraint({enf}, [lits](ConstraintProto* new_ct) {
      new_ct->mutable_bool_or()->mutable_literals()->Assign(lits.begin(),
                                                            lits.end());
    });
  };
  const auto flush_bool_and = [this](int enf, absl::Span<const int> lits) {
    if (lits.empty()) return;
    context_->AddEnforcedConstraint({enf}, [lits](ConstraintProto* new_ct) {
      new_ct->mutable_bool_and()->mutable_literals()->Assign(lits.begin(),
                                                             lits.end());
    });
  };

  for (const auto [value, eq_literal] : values.encoding()) {
    const int ne_literal = NegatedRef(eq_literal);

    // Lower or equal.
    if (has_not_le) {
      not_le_lits.push_back(ne_literal);
    }
    if (has_le) {
      le_lits.push_back(eq_literal);
    }

    const auto it_le = encoded_le_literal_.find(value);
    if (it_le != encoded_le_literal_.end()) {
      const int le_literal = it_le->second;

      DCHECK(has_le);
      flush_bool_or(le_literal, le_lits);
      le_lits.clear();
      if (value < max_le_value) {
        le_lits.push_back(le_literal);
      } else {
        has_le = false;
      }

      if (has_not_le) {
        not_le_lits.push_back(le_literal);
        flush_bool_and(not_le_enf, not_le_lits);
        not_le_lits.clear();
      }
      has_not_le = true;
      not_le_enf = le_literal;
    }

    // Greater or equal.
    if (value > var_domain_.Min()) {  // var >= min is not created.
      const auto it_ge =
          encoded_le_literal_.find(var_domain_.ValueAtOrBefore(value - 1));
      if (it_ge != encoded_le_literal_.end()) {
        const int ge_literal = NegatedRef(it_ge->second);

        if (has_ge) {
          ge_lits.push_back(ge_literal);
          flush_bool_or(ge_enf, ge_lits);
          ge_lits.clear();
        }
        has_ge = true;
        ge_enf = ge_literal;

        DCHECK(has_not_ge);
        flush_bool_and(ge_literal, not_ge_lits);
        not_ge_lits.clear();
        if (value != max_ge_value) {
          not_ge_lits.push_back(ge_literal);
        } else {
          has_not_ge = false;
        }
      }
    }
    if (has_ge) {
      ge_lits.push_back(eq_literal);
    }
    if (has_not_ge) {
      not_ge_lits.push_back(ne_literal);
    }
  }
  DCHECK(!has_le);
  DCHECK(!has_not_ge);
  if (has_not_le) {
    flush_bool_and(not_le_enf, not_le_lits);
  }
  if (has_ge) {
    flush_bool_or(ge_enf, ge_lits);
  }
}

int OrderEncoding::ge_literal(int64_t value) const {
  DCHECK_GT(value, var_domain_.Min());
  const auto it =
      encoded_le_literal_.find(var_domain_.ValueAtOrBefore(value - 1));
  DCHECK(it != encoded_le_literal_.end());
  return NegatedRef(it->second);
}

int OrderEncoding::le_literal(int64_t value) const {
  const auto it = encoded_le_literal_.find(value);
  DCHECK(it != encoded_le_literal_.end());
  return it->second;
}

int OrderEncoding::num_encoded_values() const {
  return encoded_le_literal_.size();
}

void OrderEncoding::CollectAllOrderEncodingValues() {
  for (const auto& [value, lits] : tmp_le_to_literals_) {
    const auto it = encoded_le_literal_.find(value);
    if (it != encoded_le_literal_.end()) continue;
    const int le_literal = context_->NewBoolVar("order encoding");
    solution_crush_.MaybeSetLiteralToOrderEncoding(le_literal, var_, value,
                                                   SolutionCrush::Relation::LE);
    encoded_le_literal_[value] = le_literal;
  }

  for (const auto& [value, lits] : tmp_ge_to_literals_) {
    const int64_t previous_value = var_domain_.ValueAtOrBefore(value - 1);
    const auto it = encoded_le_literal_.find(previous_value);
    if (it != encoded_le_literal_.end()) continue;
    const int ge_literal = context_->NewBoolVar("order encoding");
    solution_crush_.MaybeSetLiteralToOrderEncoding(ge_literal, var_, value,
                                                   SolutionCrush::Relation::GE);
    encoded_le_literal_[previous_value] = NegatedRef(ge_literal);
  }
}

namespace {
enum class EncodingScanStatus {
  kUnsat,
  kAbort,
  kOk,
};

EncodingScanStatus ProcessEncodingConstraints(
    int var, PresolveContext* context, ValueEncoding& values,
    OrderEncoding& order,
    std::vector<std::vector<EncodingLinear1>>& linear_ones_by_type,
    std::vector<int>& constraint_indices,
    bool& var_has_positive_objective_coefficient) {
  // We have a variable that appears only in linear1 constraints. That means
  // that the model should not change feasibility as long as the values that
  // the variable takes do not change the satisfiability of the linear1s. Thus,
  // we have "domains of equivalence" of the possible values of the variable.
  // Suppose we have a variable `v` in the domain `[0, 15]` that is only used in
  // the following linear1:
  // (c1)   x => v <= 10
  // (c2)   y => v == 5
  // (c3)   z => 2 <= v <= 7
  //
  // We can represent that as intervals:
  //
  //             0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
  //             +---------------------------------------------+
  // v <= 10     ...............................*
  // v == 5                     *
  // 2 <= v <= 7       *..............*
  //
  // If we cut the classes of equivalences for `v` into contiguous intervals,
  // they are:
  //  - [0, 1]  (c1, ~c2, ~c3)
  //  - [2, 4]  (c1, ~c2, c3)
  //  - {5}     (c1, c2, c3)
  //  - [6, 7]  (c1, ~c2, c3)
  //  - [8, 10]  (c1, ~c2, ~c3)
  //  - [11, 15] (~c1, ~c2, ~c3)
  //
  // We can pick either the lowest or the highest value of each interval
  // depending on the sign of the objective coefficient. Thus, a possible
  // encoding would be:
  //   - x == 0
  //   - x == 2
  //   - x == 5
  //   - x == 6
  //   - x == 8
  //   - x == 11
  //
  // TODO(user): Pick a single value for each equivalence class, not one
  // per contiguous interval.
  // TODO(user): Support more domains, for now only <= and >= are supported.
  //
  // TODO(user): At the expense of a more complex hint manipulation, we could
  // only use one optional value per variable.
  const Domain& var_domain = context->DomainOf(var);
  constraint_indices.clear();
  var_has_positive_objective_coefficient = false;
  bool var_in_objective = false;
  for (const int c : context->VarToConstraints(var)) {
    if (c == kObjectiveConstraint) {
      const int64_t obj_coeff = context->ObjectiveMap().at(var);
      var_in_objective = true;
      var_has_positive_objective_coefficient = obj_coeff > 0;
      continue;
    }
    if (c < 0) continue;
    constraint_indices.push_back(c);
  }

  const bool push_down_when_unconstrained =
      !var_in_objective || var_has_positive_objective_coefficient;

  // Sort the constraint indices to make the encoding deterministic.
  absl::c_sort(constraint_indices);
  for (const int c : constraint_indices) {
    const ConstraintProto& ct = context->Constraint(c);
    DCHECK_EQ(ct.constraint_case(), ConstraintProto::kLinear);
    DCHECK_EQ(ct.linear().vars().size(), 1);
    DCHECK(RefIsPositive(ct.linear().vars(0)));
    DCHECK_EQ(ct.linear().vars(0), var);
    if (ct.enforcement_literal().size() != 1) {
      context->UpdateRuleStats(
          "TODO variables: linear1 with multiple enforcement literals");
      return EncodingScanStatus::kAbort;
    }

    const auto [opt_lin, status] = ProcessLinear1(ct, c, var_domain);
    if (!opt_lin.has_value()) {
      if (status == EncodingLinear1Status::kFalse) {
        if (context->SetLiteralToFalse(ct.enforcement_literal(0)).unsat()) {
          return EncodingScanStatus::kUnsat;
        }
        continue;
      }
      if (status == EncodingLinear1Status::kIgnore) continue;
    }

    const EncodingLinear1& lin = opt_lin.value();
    VLOG(3) << "ProcessVariableOnlyUsedInEncoding(): var(" << var
            << ") domain: " << var_domain << " linear1: " << lin;

    switch (lin.type) {
      case EncodingLinear1Type::kVarEqValue:
        values.AddReferencedValueToEncode(lin.value);
        if (push_down_when_unconstrained) {
          if (lin.value < var_domain.Max()) {
            values.AddOptionalValueToEncode(
                var_domain.ValueAtOrAfter(lin.value + 1));
          }
        } else {
          if (lin.value > var_domain.Min()) {
            values.AddOptionalValueToEncode(
                var_domain.ValueAtOrBefore(lin.value - 1));
          }
        }
        break;
      case EncodingLinear1Type::kVarNeValue:
        values.AddReferencedValueToEncode(lin.value);
        if (push_down_when_unconstrained) {
          if (lin.value < var_domain.Max()) {
            values.AddOptionalValueToEncode(
                var_domain.ValueAtOrAfter(lin.value + 1));
          }
        } else {
          if (lin.value > var_domain.Min()) {
            values.AddOptionalValueToEncode(
                var_domain.ValueAtOrBefore(lin.value - 1));
          }
        }
        break;
      case EncodingLinear1Type::kVarGeValue: {
        values.AddReferencedValueToEncode(lin.value);
        values.AddReferencedValueToEncode(
            var_domain.ValueAtOrBefore(lin.value - 1));
        order.InsertGeLiteral(lin.value, lin.enforcement_literal);
        break;
      }
      case EncodingLinear1Type::kVarLeValue: {
        values.AddReferencedValueToEncode(lin.value);
        values.AddReferencedValueToEncode(
            var_domain.ValueAtOrAfter(lin.value + 1));
        order.InsertLeLiteral(lin.value, lin.enforcement_literal);
        break;
      }
      case EncodingLinear1Type::kVarInDomain: {
        // TODO(user): fine grained management of the domains.
        break;
      }
    }

    linear_ones_by_type[static_cast<int>(lin.type)].push_back(lin);
  }
  const bool has_le_ge_linear1 =
      !linear_ones_by_type[static_cast<int>(EncodingLinear1Type::kVarGeValue)]
           .empty() ||
      !linear_ones_by_type[static_cast<int>(EncodingLinear1Type::kVarLeValue)]
           .empty();
  values.CanonicalizeEncodedValuesAndAddEscapeValue(
      push_down_when_unconstrained, has_le_ge_linear1);
  return EncodingScanStatus::kOk;
}
}  // namespace

FeasibilityStatus TryToReplaceVariableByItsEncoding(
    int var, PresolveContext* context, SolutionCrush& solution_crush) {
  const Domain var_domain = context->DomainOf(var);
  std::vector<std::vector<EncodingLinear1>> linear_ones_by_type(
      kNumEncodingLinear1Types);
  ValueEncoding values(var, context);
  OrderEncoding order(var, context, solution_crush);
  bool var_has_positive_objective_coefficient = false;
  std::vector<int> constraint_indices;

  // Early abort if we know that the variable is not fully encoded (not enough
  // linear 1), has a large domain and appears in a constrained objective.
  const bool var_in_objective =
      context->VarToConstraints(var).contains(kObjectiveConstraint);
  if (var_in_objective && context->ObjectiveDomainIsConstraining() &&
      var_domain.Size() > 2500 &&
      context->VarToConstraints(var).size() < var_domain.Size()) {
    VLOG(2) << "Early abort. Large domain not fully encoded with objective "
               "constraining";
    return kOk;
  }

  const EncodingScanStatus scan_status = ProcessEncodingConstraints(
      var, context, values, order, linear_ones_by_type, constraint_indices,
      var_has_positive_objective_coefficient);
  if (scan_status == EncodingScanStatus::kUnsat) return kUnsat;
  if (scan_status == EncodingScanStatus::kAbort) return kOk;

  // Helpers to get the linear1 of each type for var.
  const auto& lin_eq =
      linear_ones_by_type[static_cast<int>(EncodingLinear1Type::kVarEqValue)];
  const auto& lin_ne =
      linear_ones_by_type[static_cast<int>(EncodingLinear1Type::kVarNeValue)];
  const auto& lin_ge =
      linear_ones_by_type[static_cast<int>(EncodingLinear1Type::kVarGeValue)];
  const auto& lin_le =
      linear_ones_by_type[static_cast<int>(EncodingLinear1Type::kVarLeValue)];
  const auto& lin_domain =
      linear_ones_by_type[static_cast<int>(EncodingLinear1Type::kVarInDomain)];

  // We force the full encoding if the variable is mostly encoded and some
  // linear1 constraints involve domains that do not correspond to value or
  // order encodings.
  const bool full_encoding_is_not_too_expensive =
      context->IsMostlyFullyEncoded(var) || var_domain.Size() <= 32;
  const bool full_encoding_is_needed =
      (!lin_domain.empty() ||
       (var_in_objective && context->ObjectiveDomainIsConstraining()));
  if (!values.is_fully_encoded() && full_encoding_is_not_too_expensive &&
      full_encoding_is_needed) {
    VLOG(3) << "Forcing full encoding of var: " << var;
    values.ForceFullEncoding();
  }

  // TODO(user): It happens rarely that the variable domain was not restricted.
  // Maybe we should make sure this is the case in more situations, here we just
  // make sure it is the case when lin_domain_is_partition below is true.
  if (lin_domain.size() > 2 &&
      lin_domain[0].enforcement_literal ==
          NegatedRef(lin_domain[1].enforcement_literal)) {
    bool modified = false;
    if (context
            ->IntersectDomainWith(
                var, lin_domain[0].rhs.UnionWith(lin_domain[1].rhs), &modified)
            .unsat()) {
      return kUnsat;
    }
    if (modified) {
      context->UpdateRuleStats(
          "var_encoding: restrict domain due to partition");
    }
  }

  const bool lin_domain_is_partition =
      lin_domain.size() == 2 &&
      lin_domain[0].enforcement_literal ==
          NegatedRef(lin_domain[1].enforcement_literal) &&
      !lin_domain[0].rhs.OverlapsWith(lin_domain[1].rhs);

  // Small heuristic to disable the variable expansion in case the partition
  // constraints are too complex w.r.t. the domain of the variable.
  //
  // This fixes the slowdown of the
  //     2025_skill-allocation_skill_allocation_mzn_2w_2 flatzinc example.
  if (lin_domain_is_partition && values.is_fully_encoded() &&
      lin_domain[0].rhs.NumIntervals() + lin_domain[1].rhs.NumIntervals() >
          var_domain.Size() / 2 &&
      var_domain.Size() > 16) {
    context->UpdateRuleStats(
        "TODO variables: only used in partition encodings");
    return kOk;
  }

  if (values.empty()) {
    // This variable has no value encoding. Either enforced_domains is
    // empty, and in that case, we will not do anything about it, or the
    // variable is not used anymore, and it will be removed later.
    return kOk;
  }

  VLOG(2) << "ProcessVariableOnlyUsedInEncoding(): var(" << var
          << "): " << var_domain << ", size: " << var_domain.Size()
          << ", #encoded_values: " << values.encoded_values().size()
          << ", #ordered_values: " << order.num_encoded_values()
          << ", #var_eq_value: " << lin_eq.size()
          << ", #var_ne_value: " << lin_ne.size()
          << ", #var_ge_value: " << lin_ge.size()
          << ", #var_le_value: " << lin_le.size()
          << ", #var_in_domain: " << lin_domain.size()
          << ", var_in_objective: " << var_in_objective
          << ", var_has_positive_objective_coefficient: "
          << var_has_positive_objective_coefficient;
  if (full_encoding_is_needed &&
      (!values.is_fully_encoded() ||
       var_domain.Size() * lin_domain.size() > 2500)) {
    VLOG(2) << "Abort - fully_encode_var: " << values.is_fully_encoded()
            << ", full_encoding_is_not_too_expensive: "
            << full_encoding_is_not_too_expensive
            << ", full_encoding_is_needed: " << full_encoding_is_needed
            << ", objective_domain_is_constraining: "
            << (var_in_objective && context->ObjectiveDomainIsConstraining())
            << ", var_has_positive_objective_coefficient: "
            << var_has_positive_objective_coefficient
            << ", objective_domain: " << context->ObjectiveDomain().ToString();
    if (var_in_objective) {
      context->UpdateRuleStats(
          "TODO variables: only used in objective and in complex encodings");
    } else {
      context->UpdateRuleStats(
          "TODO variables: only used in large complex encodings");
    }
    return kOk;
  }

  values.CreateAllValueEncodingLiterals();

  // Fix the hinted value if needed.
  //
  // The logic follows the classes of equivalence induced by the value of the
  // literals from the enforced linear1 constraining this variable.
  // Two values are in the same class if all the literals have the same value.
  //
  // We have a heuristic method here:
  // - If the variable is in the domain, we do nothing.
  // - If the variable has only var==value and var!=value encodings. All values
  //   not touched by these linear1 are equivalent. We will reassign them to the
  //   unique escape value.
  // - If the variable also has var>=value and var<=value encodings, we will
  //   push the value of the variable to the closest value in the domain in the
  //   direction of the objective. To this effect, for every contiguous set of
  //   values not in the set of referenced values, the min or the max of that
  //   set has been added to the encoded domain, such that the push up or down
  //   always falls back on an encoded value.
  //
  // TODO(user): we could optimize this as, for instance, we only need to
  // look at values from the order encodings, and not all values when creating
  // the equivalence class in the last case.
  const bool push_down_when_unconstrained =
      !var_in_objective || var_has_positive_objective_coefficient;
  solution_crush.SetOrUpdateVarToDomainWithOptionalEscapeValue(
      var, Domain::FromValues(values.encoded_values()),
      values.unique_escape_value(), push_down_when_unconstrained,
      values.encoding());
  order.CreateAllOrderEncodingLiterals(values);

  // Link all Booleans in our linear1 to the encoding literals.
  for (const EncodingLinear1& info_eq : lin_eq) {
    context->AddImplication(info_eq.enforcement_literal,
                            values.literal(info_eq.value));
  }

  for (const EncodingLinear1& info_ne : lin_ne) {
    context->AddImplication(info_ne.enforcement_literal,
                            NegatedRef(values.literal(info_ne.value)));
  }

  for (const EncodingLinear1& info_ge : lin_ge) {
    context->AddImplication(info_ge.enforcement_literal,
                            order.ge_literal(info_ge.value));
  }

  for (const EncodingLinear1& info_le : lin_le) {
    context->AddImplication(info_le.enforcement_literal,
                            order.le_literal(info_le.value));
  }

  // Special case if the lin_domains form a partition. Currently, we just check
  // for a partition of size 2.
  if (lin_domain_is_partition) {
    // We encode lit => var in domain with:
    //   exactly_one(not(lit), b_v1, b_v2, ...)
    //     where b_vi are the encoding literals for the value in the domain.
    //
    // Note, the use of exactly_one here is correct because this is a partition,
    // and the two equations complement each other.
    for (const EncodingLinear1& info_in : lin_domain) {
      context->AddConstraint([&](ConstraintProto* new_ct) {
        BoolArgumentProto* exo = new_ct->mutable_exactly_one();
        exo->add_literals(NegatedRef(info_in.enforcement_literal));
        for (const int64_t v : info_in.rhs.Values()) {
          exo->add_literals(values.literal(v));
        }
      });
    }
  } else {
    // We encode lit => var in domain with:
    //     not(lit) => forall v in domain.Complement(), x != v
    for (const EncodingLinear1& info_in : lin_domain) {
      const Domain implied_complement =
          var_domain.IntersectionWith(info_in.rhs.Complement());
      context->AddEnforcedConstraint(
          {info_in.enforcement_literal}, [&](ConstraintProto* new_ct) {
            for (const int64_t v : implied_complement.Values()) {
              new_ct->mutable_bool_and()->add_literals(
                  NegatedRef(values.literal(v)));
            }
          });
    }
  }

  // Detect implications between complex encodings.
  const auto add_incompatibility = [context](const EncodingLinear1& info_i,
                                             const EncodingLinear1& info_j) {
    const int e_i = info_i.enforcement_literal;
    const int e_j = info_j.enforcement_literal;
    if (e_i == NegatedRef(e_j)) return;
    context->AddConstraint([e_i, e_j](ConstraintProto* new_ct) {
      BoolArgumentProto* incompatible = new_ct->mutable_bool_or();
      incompatible->add_literals(NegatedRef(e_i));
      incompatible->add_literals(NegatedRef(e_j));
    });
    context->UpdateRuleStats(
        "variables: add at_most_one between incompatible complex encodings");
  };

  for (int i = 0; i < lin_domain.size(); ++i) {
    const EncodingLinear1& info_i = lin_domain[i];
    DCHECK_EQ(info_i.type, EncodingLinear1Type::kVarInDomain);

    // Incompatibilities between x in domain and x >= ge.
    for (const EncodingLinear1& info_j : lin_ge) {
      DCHECK_EQ(info_j.type, EncodingLinear1Type::kVarGeValue);
      if (info_i.rhs.Max() < info_j.value) {
        add_incompatibility(info_i, info_j);
      }
    }

    // Incompatibilities between x in domain and x <= value.
    for (const EncodingLinear1& info_j : lin_le) {
      DCHECK_EQ(info_j.type, EncodingLinear1Type::kVarLeValue);
      if (info_i.rhs.Min() > info_j.value) {
        add_incompatibility(info_i, info_j);
      }
    }

    // Incompatibilities between x in domain_i and x in domain_j.
    for (int j = i + 1; j < lin_domain.size(); ++j) {
      const EncodingLinear1& info_j = lin_domain[j];
      DCHECK_EQ(info_j.type, EncodingLinear1Type::kVarInDomain);
      if (!info_i.rhs.OverlapsWith(info_j.rhs)) {
        add_incompatibility(info_i, info_j);
      }
    }
  }

  // Update the objective if needed. Note that this operation can fail if
  // the new expression results in potential overflow.
  if (var_in_objective) {
    // We subtract the min or the max of the variable from all
    // coefficients. This should reduce the objective size and help with
    // the bounds.
    const int64_t base_value = var_has_positive_objective_coefficient
                                   ? var_domain.Min()
                                   : var_domain.Max();
    // Tricky: We cannot just choose an arbitrary value if the objective has
    // a restrictive domain!
    DCHECK(values.is_fully_encoded() ||
           !context->ObjectiveDomainIsConstraining());

    // Checks for overflow before trying to substitute the variable in the
    // objective.
    int64_t accumulated = std::abs(base_value);
    for (const int64_t value : values.encoded_values()) {
      accumulated = CapAdd(accumulated, std::abs(CapSub(value, base_value)));
      if (accumulated == kint64max) {
        VLOG(2) << "Abort - overflow when converting linear1 to clauses";
        context->UpdateRuleStats(
            "TODO variables: overflow when converting linear1 to clauses");
        return kOk;
      }
    }

    // TODO(user): we could also use a log encoding here if the domain is
    // large and the objective is not constraining.
    ConstraintProto encoding_ct;
    LinearConstraintProto* linear = encoding_ct.mutable_linear();
    const int64_t coeff_in_equality = -1;
    linear->add_vars(var);
    linear->add_coeffs(coeff_in_equality);
    int64_t rhs_value = -base_value;
    for (const auto& [value, literal] : values.encoding()) {
      const int64_t coeff = value - base_value;
      if (coeff == 0) continue;
      if (RefIsPositive(literal)) {
        linear->add_vars(literal);
        linear->add_coeffs(coeff);
      } else {
        // (1 - var) * coeff;
        rhs_value -= coeff;
        linear->add_vars(PositiveRef(literal));
        linear->add_coeffs(-coeff);
      }
    }
    linear->add_domain(rhs_value);
    linear->add_domain(rhs_value);
    const ChangeStatus status = context->SubstituteVariableInObjective(
        var, coeff_in_equality, encoding_ct,
        /*substitution_does_not_change_objective_domain=*/true);
    if (status.unsat()) return kUnsat;
    if (!status.modified()) {
      context->UpdateRuleStats(
          "TODO variables: cannot substitute encoded variable in objective");
      return kOk;
    }
    context->UpdateRuleStats(
        "variables: only used in objective and in encodings");
  } else {
    if ((!lin_eq.empty() || !lin_ne.empty()) && lin_domain.empty()) {
      context->UpdateRuleStats(
          "variables: only used in value and order encodings");
    } else if (!lin_domain.empty()) {
      if (lin_domain_is_partition) {
        context->UpdateRuleStats("variables: only used in partition encodings");
      } else {
        context->UpdateRuleStats("variables: only used in complex encodings");
      }
    } else {
      context->UpdateRuleStats("variables: only used in value encodings");
    }
  }
  if (!values.is_fully_encoded()) {
    VLOG(2) << "Reduce domain size: " << var_domain.Size() << " to  "
            << values.encoded_values().size() << ": " << var_domain << " -> "
            << Domain::FromValues(values.encoded_values());
    context->UpdateRuleStats("variables: reduce domain to encoded values");
  }

  // Clear all involved constraints. We do it in two passes to avoid
  // invalidating the iterator. We also use the constraint variable graph as
  // extra encodings (value, order) may have added new constraints.
  {
    std::vector<int> to_clear;
    for (const int c : context->VarToConstraints(var)) {
      if (c >= 0) to_clear.push_back(c);
    }
    absl::c_sort(to_clear);
    for (const int c : to_clear) {
      context->ClearConstraint(c);
    }
  }

  // This must be done after we removed all the constraints containing var.
  context->AddConstraint([&values](ConstraintProto* new_ct) {
    BoolArgumentProto* arg = new_ct->mutable_exactly_one();
    for (const auto& [value, literal] : values.encoding()) {
      arg->add_literals(literal);
    }
  });

  // To simplify the postsolve, we output a single constraint to infer X from
  // the bi: X = sum bi * (Vi - min_value) + min_value, falling back to
  // per-value enforced equalities when the combined linear constraint would
  // overflow.
  const int64_t var_min = var_domain.Min();
  LinearOverflowChecker overflow_checker;
  bool can_use_single_linear =
      overflow_checker.AddTerm(1, context->MinOf(var), context->MaxOf(var));
  int64_t offset = var_min;
  if (can_use_single_linear) {
    for (const auto& [value, literal] : values.encoding()) {
      const int64_t coeff = CapSub(value, var_min);
      if (AtMinOrMaxInt64(coeff)) {
        can_use_single_linear = false;
        break;
      }
      if (coeff == 0) continue;
      const int64_t term_coeff = RefIsPositive(literal) ? -coeff : coeff;
      if (!overflow_checker.AddTerm(term_coeff, 0, 1)) {
        can_use_single_linear = false;
        break;
      }
      if (!RefIsPositive(literal)) {
        offset = CapAdd(offset, coeff);
        if (AtMinOrMaxInt64(offset)) {
          can_use_single_linear = false;
          break;
        }
      }
    }
    if (offset < -kint64max / 2 || offset > kint64max / 2) {
      can_use_single_linear = false;
    }
  }
  if (can_use_single_linear) {
    ConstraintProto* mapping_ct =
        context->NewMappingConstraint(__FILE__, __LINE__);
    mapping_ct->mutable_linear()->add_vars(var);
    mapping_ct->mutable_linear()->add_coeffs(1);
    for (const auto& [value, literal] : values.encoding()) {
      const int64_t coeff = value - var_min;
      if (coeff == 0) continue;
      if (RefIsPositive(literal)) {
        mapping_ct->mutable_linear()->add_vars(literal);
        mapping_ct->mutable_linear()->add_coeffs(-coeff);
      } else {
        mapping_ct->mutable_linear()->add_vars(PositiveRef(literal));
        mapping_ct->mutable_linear()->add_coeffs(coeff);
      }
    }
    mapping_ct->mutable_linear()->add_domain(offset);
    mapping_ct->mutable_linear()->add_domain(offset);
  } else {
    for (const auto& [value, literal] : values.encoding()) {
      ConstraintProto* mapping_ct =
          context->NewMappingConstraint(__FILE__, __LINE__);
      mapping_ct->add_enforcement_literal(literal);
      mapping_ct->mutable_linear()->add_vars(var);
      mapping_ct->mutable_linear()->add_coeffs(1);
      FillDomainInProto(value, mapping_ct->mutable_linear());
    }
    ConstraintProto* mapping_exo =
        context->NewMappingConstraint(__FILE__, __LINE__);
    for (const auto& [unused_value, literal] : values.encoding()) {
      mapping_exo->mutable_exactly_one()->add_literals(literal);
    }
  }

  context->MarkVariableAsRemoved(var);
  return kOk;
}

}  // namespace sat
}  // namespace operations_research
