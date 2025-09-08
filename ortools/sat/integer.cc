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

#include "ortools/sat/integer.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/bitset.h"
#include "ortools/util/rev.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

std::vector<IntegerVariable> NegationOf(
    absl::Span<const IntegerVariable> vars) {
  std::vector<IntegerVariable> result(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    result[i] = NegationOf(vars[i]);
  }
  return result;
}

std::string ValueLiteralPair::DebugString() const {
  return absl::StrCat("(literal = ", literal.DebugString(),
                      ", value = ", value.value(), ")");
}

std::ostream& operator<<(std::ostream& os, const ValueLiteralPair& p) {
  os << p.DebugString();
  return os;
}

// TODO(user): Reserve vector index by literals? It is trickier, as we might not
// know beforehand how many we will need. Consider alternatives to not waste
// space like using dequeue.
void IntegerEncoder::ReserveSpaceForNumVariables(int num_vars) {
  encoding_by_var_.reserve(num_vars);
  equality_to_associated_literal_.reserve(num_vars);
  equality_by_var_.reserve(num_vars);
}

void IntegerEncoder::FullyEncodeVariable(IntegerVariable var) {
  if (VariableIsFullyEncoded(var)) return;

  CHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
  var = PositiveVariable(var);
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  CHECK(!domains_[index].IsEmpty());  // UNSAT. We don't deal with that here.
  CHECK_LT(domains_[index].Size(), 100000)
      << "Domain too large for full encoding.";

  // TODO(user): Maybe we can optimize the literal creation order and their
  // polarity as our default SAT heuristics initially depends on this.
  //
  // TODO(user): Currently, in some corner cases,
  // GetOrCreateLiteralAssociatedToEquality() might trigger some propagation
  // that update the domain of var, so we need to cache the values to not read
  // garbage. Note that it is okay to call the function on values no longer
  // reachable, as this will just do nothing.
  tmp_values_.clear();
  for (const int64_t v : domains_[index].Values()) {
    tmp_values_.push_back(IntegerValue(v));
  }
  for (const IntegerValue v : tmp_values_) {
    GetOrCreateLiteralAssociatedToEquality(var, v);
  }

  // Mark var and Negation(var) as fully encoded.
  DCHECK_LT(GetPositiveOnlyIndex(var), is_fully_encoded_.size());
  is_fully_encoded_[GetPositiveOnlyIndex(var)] = true;
}

bool IntegerEncoder::VariableIsFullyEncoded(IntegerVariable var) const {
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= is_fully_encoded_.size()) return false;

  // Once fully encoded, the status never changes.
  if (is_fully_encoded_[index]) return true;
  var = PositiveVariable(var);

  // TODO(user): Cache result as long as equality_by_var_[index] is unchanged?
  // It might not be needed since if the variable is not fully encoded, then
  // PartialDomainEncoding() will filter unreachable values, and so the size
  // check will be false until further value have been encoded.
  const int64_t initial_domain_size = domains_[index].Size();
  if (equality_by_var_[index].size() < initial_domain_size) return false;

  // This cleans equality_by_var_[index] as a side effect and in particular,
  // sorts it by values.
  PartialDomainEncoding(var);

  // TODO(user): Comparing the size might be enough, but we want to be always
  // valid even if either (*domains_[var]) or PartialDomainEncoding(var) are
  // not properly synced because the propagation is not finished.
  const auto& ref = equality_by_var_[index];
  int i = 0;
  for (const int64_t v : domains_[index].Values()) {
    if (i < ref.size() && v == ref[i].value) {
      i++;
    }
  }
  if (i == ref.size()) {
    is_fully_encoded_[index] = true;
  }
  return is_fully_encoded_[index];
}

const std::vector<ValueLiteralPair>& IntegerEncoder::FullDomainEncoding(
    IntegerVariable var) const {
  CHECK(VariableIsFullyEncoded(var));
  return PartialDomainEncoding(var);
}

const std::vector<ValueLiteralPair>& IntegerEncoder::PartialDomainEncoding(
    IntegerVariable var) const {
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= equality_by_var_.size()) {
    partial_encoding_.clear();
    return partial_encoding_;
  }

  int new_size = 0;
  partial_encoding_.assign(equality_by_var_[index].begin(),
                           equality_by_var_[index].end());
  for (int i = 0; i < partial_encoding_.size(); ++i) {
    const ValueLiteralPair pair = partial_encoding_[i];
    if (sat_solver_->Assignment().LiteralIsFalse(pair.literal)) continue;
    if (sat_solver_->Assignment().LiteralIsTrue(pair.literal)) {
      partial_encoding_.clear();
      partial_encoding_.push_back(pair);
      new_size = 1;
      break;
    }
    partial_encoding_[new_size++] = pair;
  }
  partial_encoding_.resize(new_size);
  std::sort(partial_encoding_.begin(), partial_encoding_.end(),
            ValueLiteralPair::CompareByValue());

  if (trail_->CurrentDecisionLevel() == 0) {
    // We can cleanup the current encoding in this case.
    equality_by_var_[index].assign(partial_encoding_.begin(),
                                   partial_encoding_.end());
  }

  if (!VariableIsPositive(var)) {
    std::reverse(partial_encoding_.begin(), partial_encoding_.end());
    for (ValueLiteralPair& ref : partial_encoding_) ref.value = -ref.value;
  }
  return partial_encoding_;
}

// Note that by not inserting the literal in "order" we can in the worst case
// use twice as much implication (2 by literals) instead of only one between
// consecutive literals.
void IntegerEncoder::AddImplications(
    const absl::btree_map<IntegerValue, Literal>& map,
    absl::btree_map<IntegerValue, Literal>::const_iterator it,
    Literal associated_lit) {
  if (!add_implications_) return;
  DCHECK_EQ(it->second, associated_lit);

  // Tricky: We compute the literal first because AddClauseDuringSearch() might
  // propagate at level zero and mess up the map.
  LiteralIndex before_index = kNoLiteralIndex;
  if (it != map.begin()) {
    auto before_it = it;
    --before_it;
    before_index = before_it->second.Index();
  }
  LiteralIndex after_index = kNoLiteralIndex;
  {
    auto after_it = it;
    ++after_it;
    if (after_it != map.end()) after_index = after_it->second.Index();
  }

  // Then we add the two implications.
  if (after_index != kNoLiteralIndex) {
    sat_solver_->AddClauseDuringSearch(
        {Literal(after_index).Negated(), associated_lit});
  }
  if (before_index != kNoLiteralIndex) {
    sat_solver_->AddClauseDuringSearch(
        {associated_lit.Negated(), Literal(before_index)});
  }
}

void IntegerEncoder::AddAllImplicationsBetweenAssociatedLiterals() {
  CHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
  add_implications_ = true;

  // This is tricky: AddBinaryClause() might trigger propagation that causes the
  // encoding to be filtered. So we make a copy...
  const int num_vars = encoding_by_var_.size();
  for (PositiveOnlyIndex index(0); index < num_vars; ++index) {
    LiteralIndex previous = kNoLiteralIndex;
    const IntegerVariable var(2 * index.value());
    for (const auto [unused, literal] : PartialGreaterThanEncoding(var)) {
      if (previous != kNoLiteralIndex) {
        // literal => previous.
        sat_solver_->AddBinaryClause(literal.Negated(), Literal(previous));
      }
      previous = literal.Index();
    }
  }
}

std::pair<IntegerLiteral, IntegerLiteral> IntegerEncoder::Canonicalize(
    IntegerLiteral i_lit) const {
  const bool positive = VariableIsPositive(i_lit.var);
  if (!positive) i_lit = i_lit.Negated();

  const IntegerVariable var(i_lit.var);
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  IntegerValue after(i_lit.bound);
  IntegerValue before(i_lit.bound - 1);
  DCHECK_GE(before, domains_[index].Min());
  DCHECK_LE(after, domains_[index].Max());
  int64_t previous = std::numeric_limits<int64_t>::min();
  for (const ClosedInterval& interval : domains_[index]) {
    if (before > previous && before < interval.start) before = previous;
    if (after > previous && after < interval.start) after = interval.start;
    if (after <= interval.end) break;
    previous = interval.end;
  }
  if (positive) {
    return {IntegerLiteral::GreaterOrEqual(var, after),
            IntegerLiteral::LowerOrEqual(var, before)};
  } else {
    return {IntegerLiteral::LowerOrEqual(var, before),
            IntegerLiteral::GreaterOrEqual(var, after)};
  }
}

Literal IntegerEncoder::GetOrCreateAssociatedLiteral(IntegerLiteral i_lit) {
  // Remove trivial literal.
  {
    const PositiveOnlyIndex index = GetPositiveOnlyIndex(i_lit.var);
    if (VariableIsPositive(i_lit.var)) {
      if (i_lit.bound <= domains_[index].Min()) return GetTrueLiteral();
      if (i_lit.bound > domains_[index].Max()) return GetFalseLiteral();
    } else {
      const IntegerValue bound = -i_lit.bound;
      if (bound >= domains_[index].Max()) return GetTrueLiteral();
      if (bound < domains_[index].Min()) return GetFalseLiteral();
    }
  }

  // Canonicalize and see if we have an equivalent literal already.
  const auto canonical_lit = Canonicalize(i_lit);
  if (VariableIsPositive(i_lit.var)) {
    const LiteralIndex index = GetAssociatedLiteral(canonical_lit.first);
    if (index != kNoLiteralIndex) return Literal(index);
  } else {
    const LiteralIndex index = GetAssociatedLiteral(canonical_lit.second);
    if (index != kNoLiteralIndex) return Literal(index).Negated();
  }

  ++num_created_variables_;
  const Literal literal(sat_solver_->NewBooleanVariable(), true);
  AssociateToIntegerLiteral(literal, canonical_lit.first);

  // TODO(user): on some problem this happens. We should probably make sure that
  // we don't create extra fixed Boolean variable for no reason.
  if (sat_solver_->Assignment().LiteralIsAssigned(literal)) {
    VLOG(1) << "Created a fixed literal for no reason!";
  }
  return literal;
}

namespace {
std::pair<PositiveOnlyIndex, IntegerValue> PositiveVarKey(IntegerVariable var,
                                                          IntegerValue value) {
  return std::make_pair(GetPositiveOnlyIndex(var),
                        VariableIsPositive(var) ? value : -value);
}
}  // namespace

LiteralIndex IntegerEncoder::GetAssociatedEqualityLiteral(
    IntegerVariable var, IntegerValue value) const {
  const auto it =
      equality_to_associated_literal_.find(PositiveVarKey(var, value));
  if (it != equality_to_associated_literal_.end()) {
    return it->second.Index();
  }
  return kNoLiteralIndex;
}

Literal IntegerEncoder::GetOrCreateLiteralAssociatedToEquality(
    IntegerVariable var, IntegerValue value) {
  {
    const auto it =
        equality_to_associated_literal_.find(PositiveVarKey(var, value));
    if (it != equality_to_associated_literal_.end()) {
      return it->second;
    }
  }

  // Check for trivial true/false literal to avoid creating variable for no
  // reasons.
  const Domain& domain = domains_[GetPositiveOnlyIndex(var)];
  if (!domain.Contains(VariableIsPositive(var) ? value.value()
                                               : -value.value())) {
    return GetFalseLiteral();
  }
  if (domain.IsFixed()) {
    AssociateToIntegerEqualValue(GetTrueLiteral(), var, value);
    return GetTrueLiteral();
  }

  ++num_created_variables_;
  const Literal literal(sat_solver_->NewBooleanVariable(), true);
  AssociateToIntegerEqualValue(literal, var, value);

  // TODO(user): this happens on some problem. We should probably
  // make sure that we don't create extra fixed Boolean variable for no reason.
  // Note that here we could detect the case before creating the literal. The
  // initial domain didn't contain it, but maybe the one of (>= value) or (<=
  // value) is false?
  if (sat_solver_->Assignment().LiteralIsAssigned(literal)) {
    VLOG(1) << "Created a fixed literal for no reason!";
  }
  return literal;
}

void IntegerEncoder::AssociateToIntegerLiteral(Literal literal,
                                               IntegerLiteral i_lit) {
  // Always transform to positive variable.
  if (!VariableIsPositive(i_lit.var)) {
    i_lit = i_lit.Negated();
    literal = literal.Negated();
  }

  const PositiveOnlyIndex index = GetPositiveOnlyIndex(i_lit.var);
  const Domain& domain = domains_[index];
  const IntegerValue min(domain.Min());
  const IntegerValue max(domain.Max());
  if (i_lit.bound <= min) {
    return (void)sat_solver_->AddUnitClause(literal);
  }
  if (i_lit.bound > max) {
    return (void)sat_solver_->AddUnitClause(literal.Negated());
  }

  if (index >= encoding_by_var_.size()) {
    encoding_by_var_.resize(index.value() + 1);
  }
  auto& var_encoding = encoding_by_var_[index];

  // We just insert the part corresponding to the literal with positive
  // variable.
  const auto canonical_pair = Canonicalize(i_lit);
  const auto [it, inserted] =
      var_encoding.insert({canonical_pair.first.bound, literal});
  if (!inserted) {
    const Literal associated(it->second);
    if (associated != literal) {
      DCHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
      sat_solver_->AddClauseDuringSearch({literal, associated.Negated()});
      sat_solver_->AddClauseDuringSearch({literal.Negated(), associated});
    }
    return;
  }
  AddImplications(var_encoding, it, literal);

  // Corner case if adding implication cause this to be fixed.
  if (sat_solver_->CurrentDecisionLevel() == 0) {
    if (sat_solver_->Assignment().LiteralIsTrue(literal)) {
      delayed_to_fix_->integer_literal_to_fix.push_back(canonical_pair.first);
    }
    if (sat_solver_->Assignment().LiteralIsFalse(literal)) {
      delayed_to_fix_->integer_literal_to_fix.push_back(canonical_pair.second);
    }
  }

  // Resize reverse encoding.
  const int new_size =
      1 + std::max(literal.Index().value(), literal.NegatedIndex().value());
  if (new_size > reverse_encoding_.size()) {
    reverse_encoding_.resize(new_size);
  }
  reverse_encoding_[literal].push_back(canonical_pair.first);
  reverse_encoding_[literal.NegatedIndex()].push_back(canonical_pair.second);

  // Detect the case >= max or <= min and properly register them. Note that
  // both cases will happen at the same time if there is just two possible
  // value in the domain.
  if (canonical_pair.first.bound == max) {
    AssociateToIntegerEqualValue(literal, i_lit.var, max);
  }
  if (-canonical_pair.second.bound == min) {
    AssociateToIntegerEqualValue(literal.Negated(), i_lit.var, min);
  }
}

void IntegerEncoder::AssociateToIntegerEqualValue(Literal literal,
                                                  IntegerVariable var,
                                                  IntegerValue value) {
  // The function is symmetric and we only deal with positive variable.
  if (!VariableIsPositive(var)) {
    var = NegationOf(var);
    value = -value;
  }

  // Detect literal view. Note that the same literal can be associated to more
  // than one variable, and thus already have a view. We don't change it in
  // this case.
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  const Domain& domain = domains_[index];
  if (value == 1 && domain.Min() >= 0 && domain.Max() <= 1) {
    if (literal.Index() >= literal_view_.size()) {
      literal_view_.resize(literal.Index().value() + 1, kNoIntegerVariable);
      literal_view_[literal] = var;
    } else if (literal_view_[literal] == kNoIntegerVariable) {
      literal_view_[literal] = var;
    }
  }

  // We use the "do not insert if present" behavior of .insert() to do just one
  // lookup.
  const auto insert_result = equality_to_associated_literal_.insert(
      {PositiveVarKey(var, value), literal});
  if (!insert_result.second) {
    // If this key is already associated, make the two literals equal.
    const Literal representative = insert_result.first->second;
    if (representative != literal) {
      sat_solver_->AddClauseDuringSearch({literal, representative.Negated()});
      sat_solver_->AddClauseDuringSearch({literal.Negated(), representative});
    }
    return;
  }

  // Fix literal for value outside the domain.
  if (!domain.Contains(value.value())) {
    return (void)sat_solver_->AddUnitClause(literal.Negated());
  }

  // Update equality_by_var. Note that due to the
  // equality_to_associated_literal_ hash table, there should never be any
  // duplicate values for a given variable.
  if (index >= equality_by_var_.size()) {
    equality_by_var_.resize(index.value() + 1);
    is_fully_encoded_.resize(index.value() + 1);
  }
  equality_by_var_[index].push_back({value, literal});

  // Fix literal for constant domain.
  if (domain.IsFixed()) {
    return (void)sat_solver_->AddUnitClause(literal);
  }

  const IntegerLiteral ge = IntegerLiteral::GreaterOrEqual(var, value);
  const IntegerLiteral le = IntegerLiteral::LowerOrEqual(var, value);

  // Special case for the first and last value.
  if (value == domain.Min()) {
    // Note that this will recursively call AssociateToIntegerEqualValue() but
    // since equality_to_associated_literal_[] is now set, the recursion will
    // stop there. When a domain has just 2 values, this allows to call just
    // once AssociateToIntegerEqualValue() and also associate the other value to
    // the negation of the given literal.
    AssociateToIntegerLiteral(literal, le);
    return;
  }
  if (value == domain.Max()) {
    AssociateToIntegerLiteral(literal, ge);
    return;
  }

  // (var == value)  <=>  (var >= value) and (var <= value).
  const Literal a(GetOrCreateAssociatedLiteral(ge));
  const Literal b(GetOrCreateAssociatedLiteral(le));
  sat_solver_->AddClauseDuringSearch({a, literal.Negated()});
  sat_solver_->AddClauseDuringSearch({b, literal.Negated()});
  sat_solver_->AddClauseDuringSearch({a.Negated(), b.Negated(), literal});

  // Update reverse encoding.
  const int new_size = 1 + literal.Index().value();
  if (new_size > reverse_equality_encoding_.size()) {
    reverse_equality_encoding_.resize(new_size);
  }
  reverse_equality_encoding_[literal].push_back({var, value});
}

bool IntegerEncoder::IsFixedOrHasAssociatedLiteral(IntegerLiteral i_lit) const {
  if (!VariableIsPositive(i_lit.var)) i_lit = i_lit.Negated();
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(i_lit.var);
  if (i_lit.bound <= domains_[index].Min()) return true;
  if (i_lit.bound > domains_[index].Max()) return true;
  return GetAssociatedLiteral(i_lit) != kNoLiteralIndex;
}

// TODO(user): Canonicalization might be slow.
LiteralIndex IntegerEncoder::GetAssociatedLiteral(IntegerLiteral i_lit) const {
  IntegerValue bound;
  const auto canonical_pair = Canonicalize(i_lit);
  const LiteralIndex result =
      SearchForLiteralAtOrBefore(canonical_pair.first, &bound);
  if (result != kNoLiteralIndex && bound >= i_lit.bound) {
    return result;
  }
  return kNoLiteralIndex;
}

// Note that we assume the input literal is canonicalized and do not fall into
// a hole. Otherwise, this work but will likely return a literal before and
// not one equivalent to it (which can be after!).
LiteralIndex IntegerEncoder::SearchForLiteralAtOrBefore(
    IntegerLiteral i_lit, IntegerValue* bound) const {
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(i_lit.var);
  if (index >= encoding_by_var_.size()) return kNoLiteralIndex;
  const auto& encoding = encoding_by_var_[index];
  if (VariableIsPositive(i_lit.var)) {
    // We need the entry at or before.
    // We take the element before the upper_bound() which is either the encoding
    // of i if it already exists, or the encoding just before it.
    auto after_it = encoding.upper_bound(i_lit.bound);
    if (after_it == encoding.begin()) return kNoLiteralIndex;
    --after_it;
    *bound = after_it->first;
    return after_it->second.Index();
  } else {
    // We ask for who is implied by -var >= -bound, so we look for
    // the var >= value with value > bound and take its negation.
    auto after_it = encoding.upper_bound(-i_lit.bound);
    if (after_it == encoding.end()) return kNoLiteralIndex;

    // Compute tight bound if there are holes, we have X <= candidate.
    const Domain& domain = domains_[index];
    if (after_it->first <= domain.Min()) return kNoLiteralIndex;
    *bound = -domain.ValueAtOrBefore(after_it->first.value() - 1);
    return after_it->second.NegatedIndex();
  }
}

ABSL_MUST_USE_RESULT bool IntegerEncoder::LiteralOrNegationHasView(
    Literal lit, IntegerVariable* view, bool* view_is_direct) const {
  const IntegerVariable direct_var = GetLiteralView(lit);
  const IntegerVariable opposite_var = GetLiteralView(lit.Negated());
  // If a literal has both views, we want to always keep the same
  // representative: the smallest IntegerVariable.
  if (direct_var != kNoIntegerVariable &&
      (opposite_var == kNoIntegerVariable || direct_var <= opposite_var)) {
    if (view != nullptr) *view = direct_var;
    if (view_is_direct != nullptr) *view_is_direct = true;
    return true;
  }
  if (opposite_var != kNoIntegerVariable) {
    if (view != nullptr) *view = opposite_var;
    if (view_is_direct != nullptr) *view_is_direct = false;
    return true;
  }
  return false;
}

std::vector<ValueLiteralPair> IntegerEncoder::PartialGreaterThanEncoding(
    IntegerVariable var) const {
  std::vector<ValueLiteralPair> result;
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= encoding_by_var_.size()) return result;
  if (VariableIsPositive(var)) {
    for (const auto [value, literal] : encoding_by_var_[index]) {
      result.push_back({value, literal});
    }
    return result;
  }

  // Tricky: we need to account for holes.
  const Domain& domain = domains_[index];
  if (domain.IsEmpty()) return result;
  int i = 0;
  int64_t previous;
  const int num_intervals = domain.NumIntervals();
  for (const auto [value, literal] : encoding_by_var_[index]) {
    while (value > domain[i].end) {
      previous = domain[i].end;
      ++i;
      if (i == num_intervals) break;
    }
    if (i == num_intervals) break;
    if (value <= domain[i].start) {
      if (i == 0) continue;
      result.push_back({-previous, literal.Negated()});
    } else {
      result.push_back({-value + 1, literal.Negated()});
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

bool IntegerEncoder::UpdateEncodingOnInitialDomainChange(IntegerVariable var,
                                                         Domain domain) {
  DCHECK(VariableIsPositive(var));
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= encoding_by_var_.size()) return true;

  // Fix >= literal that can be fixed.
  // We filter and canonicalize the encoding.
  int i = 0;
  int num_fixed = 0;
  tmp_encoding_.clear();
  for (const auto [value, literal] : encoding_by_var_[index]) {
    while (i < domain.NumIntervals() && value > domain[i].end) ++i;
    if (i == domain.NumIntervals()) {
      // We are past the end, so always false.
      if (trail_->Assignment().LiteralIsTrue(literal)) return false;
      if (trail_->Assignment().LiteralIsFalse(literal)) continue;
      ++num_fixed;
      trail_->EnqueueWithUnitReason(literal.Negated());
      continue;
    }
    if (i == 0 && value <= domain[0].start) {
      // We are at or before the beginning, so always true.
      if (trail_->Assignment().LiteralIsTrue(literal)) continue;
      if (trail_->Assignment().LiteralIsFalse(literal)) return false;
      ++num_fixed;
      trail_->EnqueueWithUnitReason(literal);
      continue;
    }

    // Note that we canonicalize the literal if it fall into a hole.
    tmp_encoding_.push_back(
        {std::max<IntegerValue>(value, domain[i].start), literal});
  }
  encoding_by_var_[index].clear();
  for (const auto [value, literal] : tmp_encoding_) {
    encoding_by_var_[index].insert({value, literal});
  }

  // Same for equality encoding.
  // This will be lazily cleaned on the next PartialDomainEncoding() call.
  i = 0;
  for (const ValueLiteralPair pair : PartialDomainEncoding(var)) {
    while (i < domain.NumIntervals() && pair.value > domain[i].end) ++i;
    if (i == domain.NumIntervals() || pair.value < domain[i].start) {
      if (trail_->Assignment().LiteralIsTrue(pair.literal)) return false;
      if (trail_->Assignment().LiteralIsFalse(pair.literal)) continue;
      ++num_fixed;
      trail_->EnqueueWithUnitReason(pair.literal.Negated());
    }
  }

  if (num_fixed > 0) {
    VLOG(1) << "Domain intersection fixed " << num_fixed
            << " encoding literals";
  }

  return true;
}

IntegerTrail::~IntegerTrail() {
  if (parameters_.log_search_progress() && num_decisions_to_break_loop_ > 0) {
    VLOG(1) << "Num decisions to break propagation loop: "
            << num_decisions_to_break_loop_;
  }
}

bool IntegerTrail::Propagate(Trail* trail) {
  const int level = trail->CurrentDecisionLevel();
  for (ReversibleInterface* rev : reversible_classes_) rev->SetLevel(level);

  // Make sure that our internal "integer_search_levels_" size matches the
  // sat decision levels. At the level zero, integer_search_levels_ should
  // be empty.
  if (level > integer_search_levels_.size()) {
    integer_search_levels_.push_back(integer_trail_.size());
    lazy_reason_decision_levels_.push_back(lazy_reasons_.size());
    reason_decision_levels_.push_back(literals_reason_starts_.size());
    CHECK_EQ(level, integer_search_levels_.size());
  }

  // This is required because when loading a model it is possible that we add
  // (literal <-> integer literal) associations for literals that have already
  // been propagated here. This often happens when the presolve is off
  // and many variables are fixed.
  //
  // TODO(user): refactor the interaction IntegerTrail <-> IntegerEncoder so
  // that we can just push right away such literal. Unfortunately, this is is
  // a big chunk of work.
  if (level == 0) {
    for (const IntegerLiteral i_lit : delayed_to_fix_->integer_literal_to_fix) {
      // Note that we do not call Enqueue here but directly the update domain
      // function so that we do not abort even if the level zero bounds were
      // up to date.
      const IntegerValue lb =
          std::max(LevelZeroLowerBound(i_lit.var), i_lit.bound);
      const IntegerValue ub = LevelZeroUpperBound(i_lit.var);
      if (!UpdateInitialDomain(i_lit.var, Domain(lb.value(), ub.value()))) {
        sat_solver_->NotifyThatModelIsUnsat();
        return false;
      }
    }
    delayed_to_fix_->integer_literal_to_fix.clear();

    for (const Literal lit : delayed_to_fix_->literal_to_fix) {
      if (trail_->Assignment().LiteralIsFalse(lit)) {
        sat_solver_->NotifyThatModelIsUnsat();
        return false;
      }
      if (trail_->Assignment().LiteralIsTrue(lit)) continue;
      trail_->EnqueueWithUnitReason(lit);
    }
    delayed_to_fix_->literal_to_fix.clear();
  }

  // Process all the "associated" literals and Enqueue() the corresponding
  // bounds.
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    for (const IntegerLiteral i_lit : encoder_->GetIntegerLiterals(literal)) {
      // The reason is simply the associated literal.
      if (!EnqueueAssociatedIntegerLiteral(i_lit, literal)) {
        return false;
      }
    }
  }

  return true;
}

void IntegerTrail::Untrail(const Trail& trail, int literal_trail_index) {
  ++num_untrails_;
  conditional_lbs_.clear();
  const int level = trail.CurrentDecisionLevel();
  propagation_trail_index_ =
      std::min(propagation_trail_index_, literal_trail_index);

  if (level < first_level_without_full_propagation_) {
    first_level_without_full_propagation_ = -1;
  }

  // Note that if a conflict was detected before Propagate() of this class was
  // even called, it is possible that there is nothing to backtrack.
  if (level >= integer_search_levels_.size()) return;
  const int target = integer_search_levels_[level];
  integer_search_levels_.resize(level);
  CHECK_GE(target, var_lbs_.size());
  CHECK_LE(target, integer_trail_.size());

  for (int index = integer_trail_.size() - 1; index >= target; --index) {
    const TrailEntry& entry = integer_trail_[index];
    if (entry.var < 0) continue;  // entry used by EnqueueLiteral().
    var_trail_index_[entry.var] = entry.prev_trail_index;
    var_lbs_[entry.var] = integer_trail_[entry.prev_trail_index].bound;
  }
  integer_trail_.resize(target);

  // Resize lazy reason.
  lazy_reasons_.resize(lazy_reason_decision_levels_[level]);
  lazy_reason_decision_levels_.resize(level);

  // Clear reason.
  const int old_size = reason_decision_levels_[level];
  reason_decision_levels_.resize(level);
  if (old_size < literals_reason_starts_.size()) {
    literals_reason_buffer_.resize(literals_reason_starts_[old_size]);

    const int bound_start = bounds_reason_starts_[old_size];
    bounds_reason_buffer_.resize(bound_start);
    if (bound_start < trail_index_reason_buffer_.size()) {
      trail_index_reason_buffer_.resize(bound_start);
    }

    literals_reason_starts_.resize(old_size);
    bounds_reason_starts_.resize(old_size);
    cached_sizes_.resize(old_size);
  }

  // We notify the new level once all variables have been restored to their
  // old value.
  for (ReversibleInterface* rev : reversible_classes_) rev->SetLevel(level);
}

void IntegerTrail::ReserveSpaceForNumVariables(int num_vars) {
  // We only store the domain for the positive variable.
  domains_->reserve(num_vars);
  encoder_->ReserveSpaceForNumVariables(num_vars);

  // Because we always create both a variable and its negation.
  const int size = 2 * num_vars;
  var_lbs_.reserve(size);
  var_trail_index_.reserve(size);
  integer_trail_.reserve(size);
  var_trail_index_cache_.reserve(size);
  tmp_var_to_trail_index_in_queue_.reserve(size);

  var_to_trail_index_at_lower_level_.reserve(size);
}

IntegerVariable IntegerTrail::AddIntegerVariable(IntegerValue lower_bound,
                                                 IntegerValue upper_bound) {
  DCHECK_GE(lower_bound, kMinIntegerValue);
  DCHECK_LE(lower_bound, upper_bound);
  DCHECK_LE(upper_bound, kMaxIntegerValue);
  DCHECK(lower_bound >= 0 ||
         lower_bound + std::numeric_limits<int64_t>::max() >= upper_bound);
  DCHECK(integer_search_levels_.empty());
  DCHECK_EQ(var_lbs_.size(), integer_trail_.size());

  const IntegerVariable i(var_lbs_.size());
  var_lbs_.push_back(lower_bound);
  var_trail_index_.push_back(integer_trail_.size());
  integer_trail_.push_back({lower_bound, i});
  domains_->push_back(Domain(lower_bound.value(), upper_bound.value()));

  CHECK_EQ(NegationOf(i).value(), var_lbs_.size());
  var_lbs_.push_back(-upper_bound);
  var_trail_index_.push_back(integer_trail_.size());
  integer_trail_.push_back({-upper_bound, NegationOf(i)});

  var_trail_index_cache_.resize(var_lbs_.size(), integer_trail_.size());
  tmp_var_to_trail_index_in_queue_.resize(var_lbs_.size(), 0);
  var_to_trail_index_at_lower_level_.resize(var_lbs_.size(), 0);

  for (SparseBitset<IntegerVariable>* w : watchers_) {
    w->Resize(NumIntegerVariables());
  }
  return i;
}

IntegerVariable IntegerTrail::AddIntegerVariable(const Domain& domain) {
  CHECK(!domain.IsEmpty());
  const IntegerVariable var = AddIntegerVariable(IntegerValue(domain.Min()),
                                                 IntegerValue(domain.Max()));
  CHECK(UpdateInitialDomain(var, domain));
  return var;
}

const Domain& IntegerTrail::InitialVariableDomain(IntegerVariable var) const {
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (VariableIsPositive(var)) return (*domains_)[index];
  temp_domain_ = (*domains_)[index].Negation();
  return temp_domain_;
}

// Note that we don't support optional variable here. Or at least if you set
// the domain of an optional variable to zero, the problem will be declared
// unsat.
bool IntegerTrail::UpdateInitialDomain(IntegerVariable var, Domain domain) {
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (!VariableIsPositive(var)) {
    var = NegationOf(var);
    domain = domain.Negation();
  }

  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  const Domain& old_domain = (*domains_)[index];
  domain = domain.IntersectionWith(old_domain);
  if (old_domain == domain) return true;

  if (domain.IsEmpty()) return false;
  const bool lb_changed = domain.Min() > old_domain.Min();
  const bool ub_changed = domain.Max() < old_domain.Max();
  (*domains_)[index] = domain;

  // Update directly the level zero bounds.
  DCHECK(
      ReasonIsValid(IntegerLiteral::LowerOrEqual(var, domain.Max()), {}, {}));
  DCHECK(
      ReasonIsValid(IntegerLiteral::GreaterOrEqual(var, domain.Min()), {}, {}));
  DCHECK_GE(domain.Min(), LowerBound(var));
  DCHECK_LE(domain.Max(), UpperBound(var));
  var_lbs_[var] = domain.Min();
  integer_trail_[var.value()].bound = domain.Min();
  var_lbs_[NegationOf(var)] = -domain.Max();
  integer_trail_[NegationOf(var).value()].bound = -domain.Max();

  // Do not forget to update the watchers.
  for (SparseBitset<IntegerVariable>* bitset : watchers_) {
    if (lb_changed) bitset->Set(var);
    if (ub_changed) bitset->Set(NegationOf(var));
  }

  // Update the encoding.
  return encoder_->UpdateEncodingOnInitialDomainChange(var, domain);
}

IntegerVariable IntegerTrail::GetOrCreateConstantIntegerVariable(
    IntegerValue value) {
  auto insert = constant_map_.insert(std::make_pair(value, kNoIntegerVariable));
  if (insert.second) {  // new element.
    const IntegerVariable new_var = AddIntegerVariable(value, value);
    insert.first->second = new_var;
    if (value != 0) {
      // Note that this might invalidate insert.first->second.
      CHECK(constant_map_.emplace(-value, NegationOf(new_var)).second);
    }
    return new_var;
  }
  return insert.first->second;
}

int IntegerTrail::NumConstantVariables() const {
  // The +1 if for the special key zero (the only case when we have an odd
  // number of entries).
  return (constant_map_.size() + 1) / 2;
}

int IntegerTrail::FindTrailIndexOfVarBefore(IntegerVariable var,
                                            int threshold) const {
  // Optimization. We assume this is only called when computing a reason, so we
  // can ignore this trail_index if we already need a more restrictive reason
  // for this var.
  //
  // Hacky: We know this is only called with threshold == trail_index of the
  // trail entry we are trying to explain. So this test can only trigger when a
  // variable was shown to be already implied by the current conflict.
  const int index_in_queue = tmp_var_to_trail_index_in_queue_[var];
  if (threshold <= index_in_queue) {
    // Disable the other optim if we might expand this literal during
    // 1-UIP resolution.
    const int last_decision_index =
        integer_search_levels_.empty() ? 0 : integer_search_levels_.back();
    if (index_in_queue >= last_decision_index) {
      info_is_valid_on_subsequent_last_level_expansion_ = false;
    }
    return -1;
  }

  DCHECK_GE(threshold, var_lbs_.size());
  int trail_index = var_trail_index_[var];

  // Check the validity of the cached index and use it if possible.
  if (trail_index > threshold) {
    const int cached_index = var_trail_index_cache_[var];
    if (cached_index >= threshold && cached_index < trail_index &&
        integer_trail_[cached_index].var == var) {
      trail_index = cached_index;
    }
  }

  while (trail_index >= threshold) {
    trail_index = integer_trail_[trail_index].prev_trail_index;
    if (trail_index >= var_trail_index_cache_threshold_) {
      var_trail_index_cache_[var] = trail_index;
    }
  }

  const int num_vars = var_lbs_.size();
  return trail_index < num_vars ? -1 : trail_index;
}

int IntegerTrail::FindLowestTrailIndexThatExplainBound(
    IntegerLiteral i_lit) const {
  DCHECK_LE(i_lit.bound, var_lbs_[i_lit.var]);
  DCHECK(!IsTrueAtLevelZero(i_lit));

  int trail_index = var_trail_index_[i_lit.var];

  // Check the validity of the cached index and use it if possible. This caching
  // mechanism is important in case of long chain of propagation on the same
  // variable. Because during conflict resolution, we call
  // FindLowestTrailIndexThatExplainBound() with lowest and lowest bound, this
  // cache can transform a quadratic complexity into a linear one.
  {
    const int cached_index = var_trail_index_cache_[i_lit.var];
    if (cached_index < trail_index) {
      const TrailEntry& entry = integer_trail_[cached_index];
      if (entry.var == i_lit.var && entry.bound >= i_lit.bound) {
        trail_index = cached_index;
      }
    }
  }

  int prev_trail_index = trail_index;
  while (true) {
    ++work_done_in_explain_lower_than_;
    if (trail_index >= var_trail_index_cache_threshold_) {
      var_trail_index_cache_[i_lit.var] = trail_index;
    }
    const TrailEntry& entry = integer_trail_[trail_index];
    if (entry.bound == i_lit.bound) return trail_index;
    if (entry.bound < i_lit.bound) return prev_trail_index;
    prev_trail_index = trail_index;
    trail_index = entry.prev_trail_index;
  }
}

// TODO(user): Get rid of this function and only keep the trail index one?
void IntegerTrail::RelaxLinearReason(
    IntegerValue slack, absl::Span<const IntegerValue> coeffs,
    std::vector<IntegerLiteral>* reason) const {
  CHECK_GE(slack, 0);
  if (slack == 0) return;
  const int size = reason->size();
  tmp_indices_.resize(size);
  for (int i = 0; i < size; ++i) {
    CHECK_EQ((*reason)[i].bound, LowerBound((*reason)[i].var));
    CHECK_GE(coeffs[i], 0);
    tmp_indices_[i] = var_trail_index_[(*reason)[i].var];
  }

  RelaxLinearReason(slack, coeffs, &tmp_indices_);

  reason->clear();
  for (const int i : tmp_indices_) {
    reason->push_back(IntegerLiteral::GreaterOrEqual(integer_trail_[i].var,
                                                     integer_trail_[i].bound));
  }
}

void IntegerTrail::AppendRelaxedLinearReason(
    IntegerValue slack, absl::Span<const IntegerValue> coeffs,
    absl::Span<const IntegerVariable> vars,
    std::vector<IntegerLiteral>* reason) const {
  tmp_indices_.clear();
  for (const IntegerVariable var : vars) {
    tmp_indices_.push_back(var_trail_index_[var]);
  }
  if (slack > 0) RelaxLinearReason(slack, coeffs, &tmp_indices_);
  for (const int i : tmp_indices_) {
    reason->push_back(IntegerLiteral::GreaterOrEqual(integer_trail_[i].var,
                                                     integer_trail_[i].bound));
  }
}

void IntegerTrail::RelaxLinearReason(IntegerValue slack,
                                     absl::Span<const IntegerValue> coeffs,
                                     std::vector<int>* trail_indices) const {
  DCHECK_GT(slack, 0);
  DCHECK(relax_heap_.empty());

  // We start by filtering *trail_indices:
  // - remove all level zero entries.
  // - keep the one that cannot be relaxed.
  // - move the other one to the relax_heap_ (and creating the heap).
  int new_size = 0;
  const int size = coeffs.size();
  const int num_vars = var_lbs_.size();
  for (int i = 0; i < size; ++i) {
    const int index = (*trail_indices)[i];

    // We ignore level zero entries.
    if (index < num_vars) continue;

    // If the coeff is too large, we cannot relax this entry.
    const IntegerValue coeff = coeffs[i];
    if (coeff > slack) {
      (*trail_indices)[new_size++] = index;
      continue;
    }

    // This is a bit hacky, but when it is used from MergeReasonIntoInternal(),
    // we never relax a reason that will not be expanded because it is already
    // part of the current conflict.
    const TrailEntry& entry = integer_trail_[index];
    if (entry.var != kNoIntegerVariable &&
        index <= tmp_var_to_trail_index_in_queue_[entry.var]) {
      (*trail_indices)[new_size++] = index;
      continue;
    }

    // Note that both terms of the product are positive.
    const TrailEntry& previous_entry = integer_trail_[entry.prev_trail_index];
    const int64_t diff =
        CapProd(coeff.value(), (entry.bound - previous_entry.bound).value());
    if (diff > slack) {
      (*trail_indices)[new_size++] = index;
      continue;
    }

    relax_heap_.push_back({index, coeff, diff});
  }
  trail_indices->resize(new_size);
  std::make_heap(relax_heap_.begin(), relax_heap_.end());

  while (slack > 0 && !relax_heap_.empty()) {
    const RelaxHeapEntry heap_entry = relax_heap_.front();
    std::pop_heap(relax_heap_.begin(), relax_heap_.end());
    relax_heap_.pop_back();

    // The slack might have changed since the entry was added.
    if (heap_entry.diff > slack) {
      trail_indices->push_back(heap_entry.index);
      continue;
    }

    // Relax, and decide what to do with the new value of index.
    slack -= heap_entry.diff;
    const int index = integer_trail_[heap_entry.index].prev_trail_index;

    // Same code as in the first block.
    if (index < num_vars) continue;
    if (heap_entry.coeff > slack) {
      trail_indices->push_back(index);
      continue;
    }
    const TrailEntry& entry = integer_trail_[index];
    if (entry.var != kNoIntegerVariable &&
        index <= tmp_var_to_trail_index_in_queue_[entry.var]) {
      trail_indices->push_back(index);
      continue;
    }

    const TrailEntry& previous_entry = integer_trail_[entry.prev_trail_index];
    const int64_t diff = CapProd(heap_entry.coeff.value(),
                                 (entry.bound - previous_entry.bound).value());
    if (diff > slack) {
      trail_indices->push_back(index);
      continue;
    }
    relax_heap_.push_back({index, heap_entry.coeff, diff});
    std::push_heap(relax_heap_.begin(), relax_heap_.end());
  }

  // If we aborted early because of the slack, we need to push all remaining
  // indices back into the reason.
  for (const RelaxHeapEntry& entry : relax_heap_) {
    trail_indices->push_back(entry.index);
  }
  relax_heap_.clear();
}

void IntegerTrail::RemoveLevelZeroBounds(
    std::vector<IntegerLiteral>* reason) const {
  int new_size = 0;
  for (const IntegerLiteral literal : *reason) {
    if (literal.bound <= LevelZeroLowerBound(literal.var)) continue;
    (*reason)[new_size++] = literal;
  }
  reason->resize(new_size);
}

std::vector<Literal>* IntegerTrail::InitializeConflict(
    IntegerLiteral integer_literal, bool use_lazy_reason,
    absl::Span<const Literal> literals_reason,
    absl::Span<const IntegerLiteral> bounds_reason) {
  DCHECK(tmp_queue_.empty());
  std::vector<Literal>* conflict = trail_->MutableConflict();
  if (use_lazy_reason) {
    // We use the current trail index here.
    conflict->clear();
    lazy_reasons_.back().Explain(conflict, &tmp_queue_);
  } else {
    conflict->assign(literals_reason.begin(), literals_reason.end());
    for (const IntegerLiteral& literal : bounds_reason) {
      if (IsTrueAtLevelZero(literal)) continue;
      tmp_queue_.push_back(FindLowestTrailIndexThatExplainBound(literal));
    }
  }
  return conflict;
}

namespace {

std::string ReasonDebugString(absl::Span<const Literal> literal_reason,
                              absl::Span<const IntegerLiteral> integer_reason) {
  std::string result = "literals:{";
  for (const Literal l : literal_reason) {
    if (result.back() != '{') result += ",";
    result += l.DebugString();
  }
  result += "} bounds:{";
  for (const IntegerLiteral l : integer_reason) {
    if (result.back() != '{') result += ",";
    result += l.DebugString();
  }
  result += "}";
  return result;
}

}  // namespace

std::string IntegerTrail::DebugString() {
  std::string result = "trail:{";
  const int num_vars = var_lbs_.size();
  const int limit =
      std::min(num_vars + 30, static_cast<int>(integer_trail_.size()));
  for (int i = num_vars; i < limit; ++i) {
    if (result.back() != '{') result += ",";
    result +=
        IntegerLiteral::GreaterOrEqual(IntegerVariable(integer_trail_[i].var),
                                       integer_trail_[i].bound)
            .DebugString();
  }
  if (limit < integer_trail_.size()) {
    result += ", ...";
  }
  result += "}";
  return result;
}

bool IntegerTrail::RootLevelEnqueue(IntegerLiteral i_lit) {
  DCHECK(ReasonIsValid(i_lit, {}, {}));
  if (i_lit.bound <= LevelZeroLowerBound(i_lit.var)) return true;
  if (i_lit.bound > LevelZeroUpperBound(i_lit.var)) {
    sat_solver_->NotifyThatModelIsUnsat();
    return false;
  }
  if (trail_->CurrentDecisionLevel() == 0) {
    if (!Enqueue(i_lit, {}, {})) {
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }
    return true;
  }

  // If the new level zero bounds is greater than or equal to our best bound, we
  // "clear" all entries associated to this variables and only keep the level
  // zero entry.
  //
  // TODO(user): We could still "clear" just a subset of the entries event
  // if the recent ones are still needed.
  if (i_lit.bound >= var_lbs_[i_lit.var]) {
    int index = var_trail_index_[i_lit.var];
    const int num_vars = var_lbs_.size();
    while (index >= num_vars) {
      integer_trail_[index].var = kNoIntegerVariable;
      index = integer_trail_[index].prev_trail_index;
    }
    DCHECK_EQ(index, i_lit.var.value());

    // Point to the level zero entry.
    DCHECK_GE(i_lit.bound, var_lbs_[i_lit.var]);
    var_lbs_[i_lit.var] = i_lit.bound;
    var_trail_index_[i_lit.var] = index;

    // TODO(user): we might update this twice, but it is important to at least
    // increase this as this counter can be used for "timestamping" and here
    // we did change a bound.
    ++num_enqueues_;
  }

  // Update the level-zero bound in any case.
  integer_trail_[i_lit.var.value()].bound = i_lit.bound;

  // Make sure we will update InitialVariableDomain() when we are back
  // at level zero.
  delayed_to_fix_->integer_literal_to_fix.push_back(i_lit);

  return true;
}

bool IntegerTrail::SafeEnqueue(
    IntegerLiteral i_lit, absl::Span<const IntegerLiteral> integer_reason) {
  return SafeEnqueue(i_lit, {}, integer_reason);
}

bool IntegerTrail::SafeEnqueue(
    IntegerLiteral i_lit, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  // Note that ReportConflict() deal correctly with constant literals.
  if (i_lit.IsAlwaysTrue()) return true;
  if (i_lit.IsAlwaysFalse()) {
    return ReportConflict(literal_reason, integer_reason);
  }

  // Most of our propagation code do not use "constant" literal, so to not
  // have to test for them in Enqueue(), we clear them beforehand.
  tmp_cleaned_reason_.clear();
  for (const IntegerLiteral lit : integer_reason) {
    DCHECK(!lit.IsAlwaysFalse());
    if (lit.IsAlwaysTrue()) continue;
    tmp_cleaned_reason_.push_back(lit);
  }
  return Enqueue(i_lit, literal_reason, tmp_cleaned_reason_);
}

bool IntegerTrail::ConditionalEnqueue(
    Literal lit, IntegerLiteral i_lit, std::vector<Literal>* literal_reason,
    std::vector<IntegerLiteral>* integer_reason) {
  const VariablesAssignment& assignment = trail_->Assignment();
  if (assignment.LiteralIsFalse(lit)) return true;

  if (assignment.LiteralIsTrue(lit)) {
    literal_reason->push_back(lit.Negated());
    return Enqueue(i_lit, *literal_reason, *integer_reason);
  }

  if (IntegerLiteralIsFalse(i_lit)) {
    integer_reason->push_back(
        IntegerLiteral::LowerOrEqual(i_lit.var, i_lit.bound - 1));
    EnqueueLiteral(lit.Negated(), *literal_reason, *integer_reason);
    return true;
  }

  // We can't push anything in this case.
  //
  // We record it for this propagation phase (until the next untrail) as this
  // is relatively fast and heuristics can exploit this.
  //
  // Note that currently we only use ConditionalEnqueue() in scheduling
  // propagator, and these propagator are quite slow so this is not visible.
  //
  // TODO(user): We could even keep the reason and maybe do some reasoning using
  // at_least_one constraint on a set of the Boolean used here.
  const auto [it, inserted] =
      conditional_lbs_.insert({{lit.Index(), i_lit.var}, i_lit.bound});
  if (!inserted) {
    it->second = std::max(it->second, i_lit.bound);
  }

  return true;
}

bool IntegerTrail::ReasonIsValid(
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  const VariablesAssignment& assignment = trail_->Assignment();
  for (const Literal lit : literal_reason) {
    if (!assignment.LiteralIsFalse(lit)) return false;
  }
  for (const IntegerLiteral i_lit : integer_reason) {
    if (i_lit.IsAlwaysTrue()) continue;
    if (i_lit.IsAlwaysFalse()) {
      LOG(INFO) << "Reason has a constant false literal!";
      return false;
    }
    if (i_lit.bound > var_lbs_[i_lit.var]) {
      LOG(INFO) << "Reason " << i_lit << " is not true!"
                << " non-optional variable:" << i_lit.var
                << " current_lb:" << var_lbs_[i_lit.var];
      return false;
    }
  }

  // This may not indicate an incorectness, but just some propagators that
  // didn't reach a fixed-point at level zero.
  if (!integer_search_levels_.empty()) {
    int num_literal_assigned_after_root_node = 0;
    for (const Literal lit : literal_reason) {
      if (trail_->Info(lit.Variable()).level > 0) {
        num_literal_assigned_after_root_node++;
      }
    }
    for (const IntegerLiteral i_lit : integer_reason) {
      if (i_lit.IsAlwaysTrue()) continue;
      if (LevelZeroLowerBound(i_lit.var) < i_lit.bound) {
        num_literal_assigned_after_root_node++;
      }
    }
    if (num_literal_assigned_after_root_node == 0) {
      VLOG(2) << "Propagating a literal with no reason at a positive level!\n"
              << "level:" << integer_search_levels_.size() << " "
              << ReasonDebugString(literal_reason, integer_reason) << "\n"
              << DebugString();
    }
  }

  return true;
}

bool IntegerTrail::ReasonIsValid(
    IntegerLiteral i_lit, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  if (!ReasonIsValid(literal_reason, integer_reason)) return false;
  if (debug_checker_ == nullptr) return true;

  std::vector<Literal> clause;
  clause.assign(literal_reason.begin(), literal_reason.end());
  std::vector<IntegerLiteral> lits;
  lits.assign(integer_reason.begin(), integer_reason.end());
  MergeReasonInto(lits, &clause);
  if (!debug_checker_(clause, {i_lit})) {
    LOG(INFO) << "Invalid reason for loaded solution: " << i_lit << " "
              << literal_reason << " " << integer_reason;
    return false;
  }
  return true;
}

bool IntegerTrail::ReasonIsValid(
    Literal lit, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  if (!ReasonIsValid(literal_reason, integer_reason)) return false;
  if (debug_checker_ == nullptr) return true;

  std::vector<Literal> clause;
  clause.assign(literal_reason.begin(), literal_reason.end());
  clause.push_back(lit);
  std::vector<IntegerLiteral> lits;
  lits.assign(integer_reason.begin(), integer_reason.end());
  MergeReasonInto(lits, &clause);
  if (!debug_checker_(clause, {})) {
    LOG(INFO) << "Invalid reason for loaded solution: " << lit << " "
              << literal_reason << " " << integer_reason;
    return false;
  }
  return true;
}

void IntegerTrail::EnqueueLiteral(
    Literal literal, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  EnqueueLiteralInternal(literal, false, literal_reason, integer_reason);
}

bool IntegerTrail::SafeEnqueueLiteral(
    Literal literal, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  if (trail_->Assignment().LiteralIsTrue(literal)) {
    return true;
  } else if (trail_->Assignment().LiteralIsFalse(literal)) {
    return ReportConflict(literal_reason, integer_reason);
  }
  EnqueueLiteralInternal(literal, false, literal_reason, integer_reason);
  return true;
}

void IntegerTrail::EnqueueLiteralInternal(
    Literal literal, bool use_lazy_reason,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  DCHECK(!trail_->Assignment().LiteralIsAssigned(literal));
  DCHECK(use_lazy_reason ||
         ReasonIsValid(literal, literal_reason, integer_reason));
  if (integer_search_levels_.empty()) {
    // Level zero. We don't keep any reason.
    trail_->EnqueueWithUnitReason(literal);
    return;
  }

  // If we are fixing something at a positive level, remember it.
  if (!integer_search_levels_.empty() && integer_reason.empty() &&
      literal_reason.empty() && !use_lazy_reason) {
    delayed_to_fix_->literal_to_fix.push_back(literal);
  }

  const int trail_index = trail_->Index();
  if (trail_index >= boolean_trail_index_to_reason_index_.size()) {
    boolean_trail_index_to_reason_index_.resize(trail_index + 1);
  }

  const int reason_index =
      use_lazy_reason
          ? -static_cast<int>(lazy_reasons_.size())
          : AppendReasonToInternalBuffers(literal_reason, integer_reason);
  boolean_trail_index_to_reason_index_[trail_index] = reason_index;

  trail_->Enqueue(literal, propagator_id_);
}

// We count the number of propagation at the current level, and returns true
// if it seems really large. Note that we disable this if we are in fixed
// search.
bool IntegerTrail::InPropagationLoop() const {
  if (parameters_.propagation_loop_detection_factor() == 0.0) return false;
  return (
      !integer_search_levels_.empty() &&
      integer_trail_.size() - integer_search_levels_.back() >
          std::max(10000.0, parameters_.propagation_loop_detection_factor() *
                                static_cast<double>(var_lbs_.size())) &&
      parameters_.search_branching() != SatParameters::FIXED_SEARCH);
}

void IntegerTrail::NotifyThatPropagationWasAborted() {
  if (first_level_without_full_propagation_ == -1) {
    first_level_without_full_propagation_ = trail_->CurrentDecisionLevel();
  }
}

// We try to select a variable with a large domain that was propagated a lot
// already.
IntegerVariable IntegerTrail::NextVariableToBranchOnInPropagationLoop() const {
  CHECK(InPropagationLoop());
  ++num_decisions_to_break_loop_;
  std::vector<IntegerVariable> vars;
  for (int i = integer_search_levels_.back(); i < integer_trail_.size(); ++i) {
    const IntegerVariable var = integer_trail_[i].var;
    if (var == kNoIntegerVariable) continue;
    if (UpperBound(var) - LowerBound(var) <= 100) continue;
    vars.push_back(var);
  }
  if (vars.empty()) return kNoIntegerVariable;
  std::sort(vars.begin(), vars.end());
  IntegerVariable best_var = vars[0];
  int best_count = 1;
  int count = 1;
  for (int i = 1; i < vars.size(); ++i) {
    if (vars[i] != vars[i - 1]) {
      count = 1;
    } else {
      ++count;
      if (count > best_count) {
        best_count = count;
        best_var = vars[i];
      }
    }
  }
  return best_var;
}

bool IntegerTrail::CurrentBranchHadAnIncompletePropagation() {
  return first_level_without_full_propagation_ != -1;
}

IntegerVariable IntegerTrail::FirstUnassignedVariable() const {
  for (IntegerVariable var(0); var < var_lbs_.size(); var += 2) {
    if (!IsFixed(var)) return var;
  }
  return kNoIntegerVariable;
}

void IntegerTrail::CanonicalizeLiteralIfNeeded(IntegerLiteral* i_lit) {
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(i_lit->var);
  const Domain& domain = (*domains_)[index];
  if (domain.NumIntervals() <= 1) return;
  if (VariableIsPositive(i_lit->var)) {
    i_lit->bound = domain.ValueAtOrAfter(i_lit->bound.value());
  } else {
    i_lit->bound = -domain.ValueAtOrBefore(-i_lit->bound.value());
  }
}

int IntegerTrail::AppendReasonToInternalBuffers(
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  const int reason_index = literals_reason_starts_.size();
  DCHECK_EQ(reason_index, bounds_reason_starts_.size());
  DCHECK_EQ(reason_index, cached_sizes_.size());

  literals_reason_starts_.push_back(literals_reason_buffer_.size());
  if (!literal_reason.empty()) {
    literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                   literal_reason.begin(),
                                   literal_reason.end());
  }

  cached_sizes_.push_back(-1);
  bounds_reason_starts_.push_back(bounds_reason_buffer_.size());
  if (!integer_reason.empty()) {
    bounds_reason_buffer_.insert(bounds_reason_buffer_.end(),
                                 integer_reason.begin(), integer_reason.end());
  }

  return reason_index;
}

int64_t IntegerTrail::NextConflictId() {
  return sat_solver_->num_failures() + 1;
}

bool IntegerTrail::EnqueueInternal(
    IntegerLiteral i_lit, bool use_lazy_reason,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason,
    int trail_index_with_same_reason) {
  DCHECK(use_lazy_reason ||
         ReasonIsValid(i_lit, literal_reason, integer_reason));
  const IntegerVariable var(i_lit.var);

  // Nothing to do if the bound is not better than the current one.
  // TODO(user): Change this to a CHECK? propagator shouldn't try to push such
  // bound and waste time explaining it.
  if (i_lit.bound <= var_lbs_[var]) return true;
  ++num_enqueues_;

  // If the domain of var is not a single intervals and i_lit.bound fall into a
  // "hole", we increase it to the next possible value. This ensure that we
  // never Enqueue() non-canonical literals. See also Canonicalize().
  //
  // Note: The literals in the reason are not necessarily canonical, but then
  // we always map these to enqueued literals during conflict resolution.
  CanonicalizeLiteralIfNeeded(&i_lit);

  // Check if the integer variable has an empty domain.
  if (i_lit.bound > UpperBound(var)) {
    // We relax the upper bound as much as possible to still have a conflict.
    const auto ub_reason = IntegerLiteral::LowerOrEqual(var, i_lit.bound - 1);

    // Note that we want only one call to MergeReasonIntoInternal() for
    // efficiency and a potential smaller reason.
    auto* conflict = InitializeConflict(i_lit, use_lazy_reason, literal_reason,
                                        integer_reason);
    if (!IsTrueAtLevelZero(ub_reason)) {
      tmp_queue_.push_back(FindLowestTrailIndexThatExplainBound(ub_reason));
    }
    MergeReasonIntoInternal(conflict, NextConflictId());
    return false;
  }

  // Stop propagating if we detect a propagation loop. The search heuristic will
  // then take an appropriate next decision. Note that we do that after checking
  // for a potential conflict if the two bounds of a variable cross. This is
  // important, so that in the corner case where all variables are actually
  // fixed, we still make sure no propagator detect a conflict.
  //
  // TODO(user): Some propagation code have CHECKS in place and not like when
  // something they just pushed is not reflected right away. They must be aware
  // of that, which is a bit tricky.
  if (InPropagationLoop()) {
    // Note that we still propagate "big" push as it seems better to do that
    // now rather than to delay to the next decision.
    const IntegerValue lb = LowerBound(i_lit.var);
    const IntegerValue ub = UpperBound(i_lit.var);
    if (i_lit.bound - lb < (ub - lb) / 2) {
      if (first_level_without_full_propagation_ == -1) {
        first_level_without_full_propagation_ = trail_->CurrentDecisionLevel();
      }
      return true;
    }
  }

  // Notify the watchers.
  for (SparseBitset<IntegerVariable>* bitset : watchers_) {
    bitset->Set(i_lit.var);
  }

  // Enqueue the strongest associated Boolean literal implied by this one.
  // Because we linked all such literal with implications, all the one before
  // will be propagated by the SAT solver.
  //
  // Important: It is possible that such literal or even stronger ones are
  // already true! This is because we might push stuff while Propagate() haven't
  // been called yet. Maybe we should call it?
  //
  // TODO(user): It might be simply better and more efficient to simply enqueue
  // all of them here. We have also more liberty to choose the explanation we
  // want. A drawback might be that the implications might not be used in the
  // binary conflict minimization algo.
  IntegerValue bound;
  const LiteralIndex literal_index =
      encoder_->SearchForLiteralAtOrBefore(i_lit, &bound);
  int bool_index = -1;
  if (literal_index != kNoLiteralIndex) {
    const Literal to_enqueue = Literal(literal_index);
    if (trail_->Assignment().LiteralIsFalse(to_enqueue)) {
      auto* conflict = InitializeConflict(i_lit, use_lazy_reason,
                                          literal_reason, integer_reason);
      conflict->push_back(to_enqueue);
      MergeReasonIntoInternal(conflict, NextConflictId());
      return false;
    }

    // If the associated literal exactly correspond to i_lit, then we push
    // it first, and then we use it as a reason for i_lit. We do that so that
    // MergeReasonIntoInternal() will not unecessarily expand further the reason
    // for i_lit.
    if (bound >= i_lit.bound) {
      DCHECK_EQ(bound, i_lit.bound);
      if (!trail_->Assignment().LiteralIsTrue(to_enqueue)) {
        EnqueueLiteralInternal(to_enqueue, use_lazy_reason, literal_reason,
                               integer_reason);
      }
      return EnqueueAssociatedIntegerLiteral(i_lit, to_enqueue);
    }

    if (!trail_->Assignment().LiteralIsTrue(to_enqueue)) {
      if (integer_search_levels_.empty()) {
        trail_->EnqueueWithUnitReason(to_enqueue);
      } else {
        // Subtle: the reason is the same as i_lit, that we will enqueue if no
        // conflict occur at position integer_trail_.size(), so we just refer to
        // this index here.
        bool_index = trail_->Index();
        trail_->Enqueue(to_enqueue, propagator_id_);
      }
    }
  }

  // Special case for level zero.
  if (integer_search_levels_.empty()) {
    ++num_level_zero_enqueues_;
    var_lbs_[i_lit.var] = i_lit.bound;
    integer_trail_[i_lit.var.value()].bound = i_lit.bound;

    // We also update the initial domain. If this fail, since we are at level
    // zero, we don't care about the reason.
    trail_->MutableConflict()->clear();
    return UpdateInitialDomain(
        i_lit.var,
        Domain(LowerBound(i_lit.var).value(), UpperBound(i_lit.var).value()));
  }
  DCHECK_GT(trail_->CurrentDecisionLevel(), 0);

  // If we are not at level zero but there is not reason, we have a root level
  // deduction. Remember it so that we don't forget on the next restart.
  if (!integer_search_levels_.empty() && integer_reason.empty() &&
      literal_reason.empty() && !use_lazy_reason) {
    if (!RootLevelEnqueue(i_lit)) return false;
  }

  int reason_index;
  if (use_lazy_reason) {
    reason_index = -static_cast<int>(lazy_reasons_.size());
  } else if (trail_index_with_same_reason >= integer_trail_.size()) {
    reason_index =
        AppendReasonToInternalBuffers(literal_reason, integer_reason);
  } else {
    reason_index = integer_trail_[trail_index_with_same_reason].reason_index;
  }
  if (bool_index >= 0) {
    if (bool_index >= boolean_trail_index_to_reason_index_.size()) {
      boolean_trail_index_to_reason_index_.resize(bool_index + 1);
    }
    boolean_trail_index_to_reason_index_[bool_index] = reason_index;
  }

  const int prev_trail_index = var_trail_index_[i_lit.var];
  var_lbs_[i_lit.var] = i_lit.bound;
  var_trail_index_[i_lit.var] = integer_trail_.size();
  integer_trail_.push_back({/*bound=*/i_lit.bound,
                            /*var=*/i_lit.var,
                            /*prev_trail_index=*/prev_trail_index,
                            /*reason_index=*/reason_index});

  return true;
}

bool IntegerTrail::EnqueueAssociatedIntegerLiteral(IntegerLiteral i_lit,
                                                   Literal literal_reason) {
  DCHECK(ReasonIsValid(i_lit, {literal_reason.Negated()}, {}));

  // Nothing to do if the bound is not better than the current one.
  if (i_lit.bound <= var_lbs_[i_lit.var]) return true;
  ++num_enqueues_;

  // Make sure we do not fall into a hole.
  CanonicalizeLiteralIfNeeded(&i_lit);

  // Check if the integer variable has an empty domain. Note that this should
  // happen really rarely since in most situation, pushing the upper bound would
  // have resulted in this literal beeing false. Because of this we revert to
  // the "generic" Enqueue() to avoid some code duplication.
  if (i_lit.bound > UpperBound(i_lit.var)) {
    return Enqueue(i_lit, {literal_reason.Negated()}, {});
  }

  // Notify the watchers.
  for (SparseBitset<IntegerVariable>* bitset : watchers_) {
    bitset->Set(i_lit.var);
  }

  // Special case for level zero.
  if (integer_search_levels_.empty()) {
    var_lbs_[i_lit.var] = i_lit.bound;
    integer_trail_[i_lit.var.value()].bound = i_lit.bound;

    // We also update the initial domain. If this fail, since we are at level
    // zero, we don't care about the reason.
    trail_->MutableConflict()->clear();
    return UpdateInitialDomain(
        i_lit.var,
        Domain(LowerBound(i_lit.var).value(), UpperBound(i_lit.var).value()));
  }
  DCHECK_GT(trail_->CurrentDecisionLevel(), 0);

  const int reason_index =
      AppendReasonToInternalBuffers({literal_reason.Negated()}, {});
  const int prev_trail_index = var_trail_index_[i_lit.var];
  var_lbs_[i_lit.var] = i_lit.bound;
  var_trail_index_[i_lit.var] = integer_trail_.size();
  integer_trail_.push_back({/*bound=*/i_lit.bound,
                            /*var=*/i_lit.var,
                            /*prev_trail_index=*/prev_trail_index,
                            /*reason_index=*/reason_index});

  return true;
}

void IntegerTrail::ComputeLazyReasonIfNeeded(int reason_index) const {
  if (reason_index < 0) {
    lazy_reasons_[-reason_index - 1].Explain(&lazy_reason_literals_,
                                             &lazy_reason_trail_indices_);
  }
}

absl::Span<const int> IntegerTrail::Dependencies(int reason_index) const {
  if (reason_index < 0) {
    return absl::Span<const int>(lazy_reason_trail_indices_);
  }

  const int cached_size = cached_sizes_[reason_index];
  if (cached_size == 0) return {};

  const int start = bounds_reason_starts_[reason_index];
  if (cached_size > 0) {
    return absl::MakeSpan(&trail_index_reason_buffer_[start], cached_size);
  }

  // Else we cache.
  DCHECK_EQ(cached_size, -1);
  const int end = reason_index + 1 < bounds_reason_starts_.size()
                      ? bounds_reason_starts_[reason_index + 1]
                      : bounds_reason_buffer_.size();
  if (end > trail_index_reason_buffer_.size()) {
    trail_index_reason_buffer_.resize(end);
  }

  int new_size = 0;
  int* data = trail_index_reason_buffer_.data() + start;
  for (int i = start; i < end; ++i) {
    const IntegerLiteral to_explain = bounds_reason_buffer_[i];
    if (!IsTrueAtLevelZero(to_explain)) {
      data[new_size++] = FindLowestTrailIndexThatExplainBound(to_explain);
    }
  }
  cached_sizes_[reason_index] = new_size;
  if (new_size == 0) return {};
  return absl::MakeSpan(data, new_size);
}

void IntegerTrail::AppendLiteralsReason(int reason_index,
                                        std::vector<Literal>* output) const {
  if (reason_index < 0) {
    for (const Literal l : lazy_reason_literals_) {
      if (!added_variables_[l.Variable()]) {
        added_variables_.Set(l.Variable());
        output->push_back(l);
      }
    }
    return;
  }

  const int start = literals_reason_starts_[reason_index];
  const int end = reason_index + 1 < literals_reason_starts_.size()
                      ? literals_reason_starts_[reason_index + 1]
                      : literals_reason_buffer_.size();
  for (int i = start; i < end; ++i) {
    const Literal l = literals_reason_buffer_[i];
    if (!added_variables_[l.Variable()]) {
      added_variables_.Set(l.Variable());
      output->push_back(l);
    }
  }
}

std::vector<Literal> IntegerTrail::ReasonFor(IntegerLiteral literal) const {
  std::vector<Literal> reason;
  MergeReasonInto({literal}, &reason);
  return reason;
}

void IntegerTrail::MergeReasonInto(absl::Span<const IntegerLiteral> literals,
                                   std::vector<Literal>* output) const {
  DCHECK(tmp_queue_.empty());
  for (const IntegerLiteral& literal : literals) {
    if (literal.IsAlwaysTrue()) continue;
    if (IsTrueAtLevelZero(literal)) continue;
    tmp_queue_.push_back(FindLowestTrailIndexThatExplainBound(literal));
  }
  return MergeReasonIntoInternal(output, -1);
}

// This will expand the reason of the IntegerLiteral already in tmp_queue_ until
// everything is explained in term of Literal.
void IntegerTrail::MergeReasonIntoInternal(std::vector<Literal>* output,
                                           int64_t conflict_id) const {
  // All relevant trail indices will be >= var_lbs_.size(), so we can safely use
  // zero to means that no literal referring to this variable is in the queue.
  DCHECK(std::all_of(tmp_var_to_trail_index_in_queue_.begin(),
                     tmp_var_to_trail_index_in_queue_.end(),
                     [](int v) { return v == 0; }));
  DCHECK(tmp_to_clear_.empty());

  info_is_valid_on_subsequent_last_level_expansion_ = true;
  if (conflict_id == -1 || last_conflict_id_ != conflict_id) {
    // New conflict or a reason was asked outside first UIP resolution.
    // We just clear everything.
    last_conflict_id_ = conflict_id;
    for (const IntegerVariable var : to_clear_for_lower_level_) {
      var_to_trail_index_at_lower_level_[var] = 0;
    }
    to_clear_for_lower_level_.clear();
  }

  const int last_decision_index =
      integer_search_levels_.empty() || conflict_id == -1
          ? 0
          : integer_search_levels_.back();

  added_variables_.ClearAndResize(BooleanVariable(trail_->NumVariables()));
  for (const Literal l : *output) {
    added_variables_.Set(l.Variable());
  }

  // During the algorithm execution, all the queue entries that do not match the
  // content of tmp_var_to_trail_index_in_queue_[] will be ignored.
  for (const int trail_index : tmp_queue_) {
    DCHECK_GE(trail_index, var_lbs_.size());
    DCHECK_LT(trail_index, integer_trail_.size());
    const TrailEntry& entry = integer_trail_[trail_index];
    DCHECK_NE(entry.var, kNoIntegerVariable);
    tmp_var_to_trail_index_in_queue_[entry.var] =
        std::max(tmp_var_to_trail_index_in_queue_[entry.var], trail_index);
  }

  // We manage our heap by hand so that we can range iterate over it above, and
  // this initial heapify is faster.
  std::make_heap(tmp_queue_.begin(), tmp_queue_.end());

  // We process the entries by highest trail_index first. The content of the
  // queue will always be a valid reason for the literals we already added to
  // the output.
  int64_t work_done = 0;
  while (!tmp_queue_.empty()) {
    ++work_done;
    const int trail_index = tmp_queue_.front();
    const TrailEntry& entry = integer_trail_[trail_index];
    std::pop_heap(tmp_queue_.begin(), tmp_queue_.end());
    tmp_queue_.pop_back();

    // Skip any stale queue entry. Amongst all the entry referring to a given
    // variable, only the latest added to the queue is valid and we detect it
    // using its trail index.
    DCHECK_NE(entry.var, kNoIntegerVariable);
    if (tmp_var_to_trail_index_in_queue_[entry.var] != trail_index) {
      continue;
    }

    // Process this entry. Note that if any of the next expansion include the
    // variable entry.var in their reason, we must process it again because we
    // cannot easily detect if it was needed to infer the current entry.
    //
    // Important: the queue might already contains entries referring to the same
    // variable. The code act like if we deleted all of them at this point, we
    // just do that lazily. tmp_var_to_trail_index_in_queue_[var] will
    // only refer to newly added entries.
    //
    // TODO(user): We can and should reset that to the initial value from
    // var_to_trail_index_at_lower_level_ instead of zero.
    tmp_var_to_trail_index_in_queue_[entry.var] = 0;
    has_dependency_ = false;

    // Skip entries that we known are already explained by the part of the
    // conflict not involving the last level.
    if (var_to_trail_index_at_lower_level_[entry.var] >= trail_index) {
      continue;
    }

    // If this literal is not at the highest level, it will always be
    // propagated by the current conflict (even after some 1-UIP resolution
    // step). We save this fact so that future MergeReasonIntoInternal() on
    // the same conflict can just avoid to expand integer literal that are
    // already known to be implied.
    if (trail_index < last_decision_index) {
      tmp_seen_.push_back(trail_index);
    }

    // Set the cache threshold. Since we process trail indices in decreasing
    // order and we only have single linked list, we only want to advance the
    // "cache" up to this threshold.
    var_trail_index_cache_threshold_ = trail_index;

    // If this entry has an associated literal, then it should always be the
    // one we used for the reason. This code DCHECK that.
    if (DEBUG_MODE) {
      const LiteralIndex associated_lit =
          encoder_->GetAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
              IntegerVariable(entry.var), entry.bound));
      if (associated_lit != kNoLiteralIndex) {
        // We check that the reason is the same!
        const int reason_index = integer_trail_[trail_index].reason_index;
        CHECK_GE(reason_index, 0);
        {
          const int start = literals_reason_starts_[reason_index];
          const int end = reason_index + 1 < literals_reason_starts_.size()
                              ? literals_reason_starts_[reason_index + 1]
                              : literals_reason_buffer_.size();
          CHECK_EQ(start + 1, end);

          // Because we can update initial domains, an associated literal might
          // fall in a domain hole and can be different when canonicalized.
          //
          // TODO(user): Make the contract clearer, it is messy right now.
          if (/*DISABLES_CODE*/ (false)) {
            CHECK_EQ(literals_reason_buffer_[start],
                     Literal(associated_lit).Negated());
          }
        }
        {
          const int start = bounds_reason_starts_[reason_index];
          const int end = reason_index + 1 < bounds_reason_starts_.size()
                              ? bounds_reason_starts_[reason_index + 1]
                              : bounds_reason_buffer_.size();
          CHECK_EQ(start, end);
        }
      }
    }

    ComputeLazyReasonIfNeeded(entry.reason_index);
    AppendLiteralsReason(entry.reason_index, output);
    const auto dependencies = Dependencies(entry.reason_index);
    work_done += dependencies.size();
    for (const int next_trail_index : dependencies) {
      DCHECK_LT(next_trail_index, trail_index);
      const TrailEntry& next_entry = integer_trail_[next_trail_index];

      // Tricky: we cache trail_index in Dependencies() but it is possible
      // via RootLevelEnqueue() that some of the trail index listed here are
      // "stale", so we skip any entry with a kNoIntegerVariable.
      if (next_entry.var == kNoIntegerVariable) continue;

      // Only add literals that are not "implied" by the ones already present.
      // For instance, do not add (x >= 4) if we already have (x >= 7). This
      // translate into only adding a trail index if it is larger than the one
      // in the queue referring to the same variable.
      const int index_in_queue =
          tmp_var_to_trail_index_in_queue_[next_entry.var];

      // This means the integer literal had no dependency and is already
      // explained by the literal we added.
      if (index_in_queue >= trail_index) {
        // Disable the other optim if we might expand this literal during
        // 1-UIP resolution.
        if (index_in_queue >= last_decision_index) {
          info_is_valid_on_subsequent_last_level_expansion_ = false;
        }
        continue;
      }

      if (next_trail_index <=
          var_to_trail_index_at_lower_level_[next_entry.var]) {
        continue;
      }

      has_dependency_ = true;
      if (next_trail_index > index_in_queue) {
        tmp_var_to_trail_index_in_queue_[next_entry.var] = next_trail_index;
        tmp_queue_.push_back(next_trail_index);
        std::push_heap(tmp_queue_.begin(), tmp_queue_.end());
      }
    }

    // Special case for a "leaf", we will never need this variable again in the
    // current explanation.
    if (!has_dependency_) {
      tmp_to_clear_.push_back(entry.var);
      tmp_var_to_trail_index_in_queue_[entry.var] = trail_index;
    }
  }

  // Update var_to_trail_index_at_lower_level_.
  if (info_is_valid_on_subsequent_last_level_expansion_) {
    for (const int trail_index : tmp_seen_) {
      if (trail_index == 0) continue;
      const TrailEntry& entry = integer_trail_[trail_index];
      const int old = var_to_trail_index_at_lower_level_[entry.var];
      if (old == 0) {
        to_clear_for_lower_level_.push_back(entry.var);
      }
      var_to_trail_index_at_lower_level_[entry.var] =
          std::max(old, trail_index);
    }
  }
  tmp_seen_.clear();

  // clean-up.
  for (const IntegerVariable var : tmp_to_clear_) {
    tmp_var_to_trail_index_in_queue_[var] = 0;
  }
  tmp_to_clear_.clear();

  time_limit_->AdvanceDeterministicTime(work_done * 5e-9);
}

// TODO(user): If this is called many time on the same variables, it could be
// made faster by using some caching mechanism.
absl::Span<const Literal> IntegerTrail::Reason(const Trail& trail,
                                               int trail_index,
                                               int64_t conflict_id) const {
  std::vector<Literal>* reason = trail.GetEmptyVectorToStoreReason(trail_index);
  added_variables_.ClearAndResize(BooleanVariable(trail_->NumVariables()));

  const int reason_index = boolean_trail_index_to_reason_index_[trail_index];
  ComputeLazyReasonIfNeeded(reason_index);
  AppendLiteralsReason(reason_index, reason);
  DCHECK(tmp_queue_.empty());
  for (const int prev_trail_index : Dependencies(reason_index)) {
    DCHECK_GE(prev_trail_index, var_lbs_.size());

    // Tricky: we cache trail_index in Dependencies() but it is possible
    // via RootLevelEnqueue() that some of the trail indices listed here are
    // "stale", so we skip any entry with a kNoIntegerVariable.
    const TrailEntry& next_entry = integer_trail_[prev_trail_index];
    if (next_entry.var == kNoIntegerVariable) continue;

    tmp_queue_.push_back(prev_trail_index);
  }
  MergeReasonIntoInternal(reason, conflict_id);
  return *reason;
}

void IntegerTrail::AppendNewBounds(std::vector<IntegerLiteral>* output) const {
  return AppendNewBoundsFrom(var_lbs_.size(), output);
}

// TODO(user): Implement a dense version if there is more trail entries
// than variables!
void IntegerTrail::AppendNewBoundsFrom(
    int base_index, std::vector<IntegerLiteral>* output) const {
  tmp_marked_.ClearAndResize(IntegerVariable(var_lbs_.size()));

  // In order to push the best bound for each variable, we loop backward.
  CHECK_GE(base_index, var_lbs_.size());
  for (int i = integer_trail_.size(); --i >= base_index;) {
    const TrailEntry& entry = integer_trail_[i];
    if (entry.var == kNoIntegerVariable) continue;
    if (tmp_marked_[entry.var]) continue;

    tmp_marked_.Set(entry.var);
    output->push_back(IntegerLiteral::GreaterOrEqual(entry.var, entry.bound));
  }
}

GenericLiteralWatcher::GenericLiteralWatcher(Model* model)
    : SatPropagator("GenericLiteralWatcher"),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      rev_int_repository_(model->GetOrCreate<RevIntRepository>()) {
  // TODO(user): This propagator currently needs to be last because it is the
  // only one enforcing that a fix-point is reached on the integer variables.
  // Figure out a better interaction between the sat propagation loop and
  // this one.
  model->GetOrCreate<SatSolver>()->AddLastPropagator(this);

  integer_trail_->RegisterReversibleClass(
      &id_to_greatest_common_level_since_last_call_);
  integer_trail_->RegisterWatcher(&modified_vars_);
  queue_by_priority_.resize(2);  // Because default priority is 1.
}

void GenericLiteralWatcher::ReserveSpaceForNumVariables(int num_vars) {
  var_to_watcher_.reserve(2 * num_vars);
}

void GenericLiteralWatcher::CallOnNextPropagate(int id) {
  if (in_queue_[id]) return;
  in_queue_[id] = true;
  queue_by_priority_[id_to_priority_[id]].push_back(id);
}

void GenericLiteralWatcher::UpdateCallingNeeds(Trail* trail) {
  // Process any new Literal on the trail.
  const int literal_limit = literal_to_watcher_.size();
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (literal.Index() >= literal_limit) continue;
    for (const auto entry : literal_to_watcher_[literal]) {
      CallOnNextPropagate(entry.id);
      if (entry.watch_index >= 0) {
        id_to_watch_indices_[entry.id].push_back(entry.watch_index);
      }
    }
  }

  // Process the newly changed variables lower bounds.
  const int var_limit = var_to_watcher_.size();
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var.value() >= var_limit) continue;
    for (const auto entry : var_to_watcher_[var]) {
      CallOnNextPropagate(entry.id);
      if (entry.watch_index >= 0) {
        id_to_watch_indices_[entry.id].push_back(entry.watch_index);
      }
    }
  }

  if (trail->CurrentDecisionLevel() == 0 &&
      !level_zero_modified_variable_callback_.empty()) {
    modified_vars_for_callback_.Resize(modified_vars_.size());
    for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
      modified_vars_for_callback_.Set(var);
    }
  }

  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
}

bool GenericLiteralWatcher::Propagate(Trail* trail) {
  // Only once per call to Propagate(), if we are at level zero, we might want
  // to call propagators even if the bounds didn't change.
  const int level = trail->CurrentDecisionLevel();
  if (level == 0) {
    for (const int id : propagator_ids_to_call_at_level_zero_) {
      CallOnNextPropagate(id);
    }
  }

  UpdateCallingNeeds(trail);

  // Increase the deterministic time depending on some basic fact about our
  // propagation.
  int64_t num_propagate_calls = 0;
  const int64_t old_enqueue = integer_trail_->num_enqueues();
  auto cleanup =
      ::absl::MakeCleanup([&num_propagate_calls, old_enqueue, this]() {
        const int64_t diff = integer_trail_->num_enqueues() - old_enqueue;
        time_limit_->AdvanceDeterministicTime(1e-8 * num_propagate_calls +
                                              1e-7 * diff);
      });

  // Note that the priority may be set to -1 inside the loop in order to restart
  // at zero.
  int test_limit = 0;
  for (int priority = 0; priority < queue_by_priority_.size(); ++priority) {
    // We test the time limit from time to time. This is in order to return in
    // case of slow propagation.
    //
    // TODO(user): The queue will not be emptied, but I am not sure the solver
    // will be left in an usable state. Fix if it become needed to resume
    // the solve from the last time it was interrupted. In particular, we might
    // want to call UpdateCallingNeeds()?
    if (test_limit > 100) {
      test_limit = 0;
      if (time_limit_->LimitReached()) break;
    }
    if (stop_propagation_callback_ != nullptr && stop_propagation_callback_()) {
      integer_trail_->NotifyThatPropagationWasAborted();
      break;
    }

    std::deque<int>& queue = queue_by_priority_[priority];
    while (!queue.empty()) {
      const int id = queue.front();
      queue.pop_front();

      // Before we propagate, make sure any reversible structure are up to date.
      // Note that we never do anything expensive more than once per level.
      if (id_need_reversible_support_[id]) {
        const int low =
            id_to_greatest_common_level_since_last_call_[IdType(id)];
        const int high = id_to_level_at_last_call_[id];
        if (low < high || level > low) {  // Equivalent to not all equal.
          id_to_level_at_last_call_[id] = level;
          id_to_greatest_common_level_since_last_call_.MutableRef(IdType(id)) =
              level;
          for (ReversibleInterface* rev : id_to_reversible_classes_[id]) {
            if (low < high) rev->SetLevel(low);
            if (level > low) rev->SetLevel(level);
          }
          for (int* rev_int : id_to_reversible_ints_[id]) {
            rev_int_repository_->SaveState(rev_int);
          }
        }
      }

      // This is needed to detect if the propagator propagated anything or not.
      const int64_t old_integer_timestamp = integer_trail_->num_enqueues();
      const int64_t old_boolean_timestamp = trail->Index();

      // Set fields that might be accessed from within Propagate().
      current_id_ = id;
      call_again_ = false;

      // TODO(user): Maybe just provide one function Propagate(watch_indices) ?
      ++num_propagate_calls;
      const bool result =
          id_to_watch_indices_[id].empty()
              ? watchers_[id]->Propagate()
              : watchers_[id]->IncrementalPropagate(id_to_watch_indices_[id]);
      if (!result) {
        id_to_watch_indices_[id].clear();
        in_queue_[id] = false;
        return false;
      }

      // Update the propagation queue. At this point, the propagator has been
      // removed from the queue but in_queue_ is still true.
      if (id_to_idempotence_[id]) {
        // If the propagator is assumed to be idempotent, then we set
        // in_queue_ to false after UpdateCallingNeeds() so this later
        // function will never add it back.
        UpdateCallingNeeds(trail);
        id_to_watch_indices_[id].clear();
        in_queue_[id] = false;
      } else {
        // Otherwise, we set in_queue_ to false first so that
        // UpdateCallingNeeds() may add it back if the propagator modified any
        // of its watched variables.
        id_to_watch_indices_[id].clear();
        in_queue_[id] = false;
        UpdateCallingNeeds(trail);
      }

      if (call_again_) {
        CallOnNextPropagate(current_id_);
      }

      // If the propagator pushed a literal, we exit in order to rerun all SAT
      // only propagators first. Note that since a literal was pushed we are
      // guaranteed to be called again, and we will resume from priority 0.
      if (trail->Index() > old_boolean_timestamp) {
        // Important: for now we need to re-run the clauses propagator each time
        // we push a new literal because some propagator like the arc consistent
        // all diff relies on this.
        //
        // TODO(user): However, on some problem, it seems to work better to not
        // do that. One possible reason is that the reason of a "natural"
        // propagation might be better than one we learned.
        return true;
      }

      // If the propagator pushed an integer bound, we revert to priority = 0.
      if (integer_trail_->num_enqueues() > old_integer_timestamp) {
        ++test_limit;
        priority = -1;  // Because of the ++priority in the for loop.
        break;
      }
    }
  }

  // We wait until we reach the fix point before calling the callback.
  if (trail->CurrentDecisionLevel() == 0) {
    const std::vector<IntegerVariable>& modified_vars =
        modified_vars_for_callback_.PositionsSetAtLeastOnce();
    for (const auto& callback : level_zero_modified_variable_callback_) {
      callback(modified_vars);
    }
    modified_vars_for_callback_.ClearAndResize(
        integer_trail_->NumIntegerVariables());
  }

  return true;
}

void GenericLiteralWatcher::Untrail(const Trail& trail, int trail_index) {
  if (propagation_trail_index_ <= trail_index) {
    // Nothing to do since we found a conflict before Propagate() was called.
    if (DEBUG_MODE) {
      // The assumption is not always true if we are currently aborting.
      if (time_limit_->LimitReached()) return;
      CHECK_EQ(propagation_trail_index_, trail_index)
          << " level " << trail.CurrentDecisionLevel();
    }
    return;
  }

  // Note that we can do that after the test above: If none of the propagator
  // where called, there are still technically "in dive" if we didn't backtrack
  // past their last Propagate() call.
  for (bool* to_reset : bool_to_reset_on_backtrack_) *to_reset = false;
  bool_to_reset_on_backtrack_.clear();

  // We need to clear the watch indices on untrail.
  for (std::deque<int>& queue : queue_by_priority_) {
    for (const int id : queue) {
      id_to_watch_indices_[id].clear();
    }
    queue.clear();
  }

  // This means that we already propagated all there is to propagate
  // at the level trail_index, so we can safely clear modified_vars_ in case
  // it wasn't already done.
  propagation_trail_index_ = trail_index;
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  in_queue_.assign(watchers_.size(), false);
}

// Registers a propagator and returns its unique ids.
int GenericLiteralWatcher::Register(PropagatorInterface* propagator) {
  const int id = watchers_.size();
  watchers_.push_back(propagator);

  id_need_reversible_support_.push_back(false);
  id_to_level_at_last_call_.push_back(0);
  id_to_greatest_common_level_since_last_call_.GrowByOne();
  id_to_reversible_classes_.push_back(std::vector<ReversibleInterface*>());
  id_to_reversible_ints_.push_back(std::vector<int*>());

  id_to_watch_indices_.push_back(std::vector<int>());
  id_to_priority_.push_back(1);
  id_to_idempotence_.push_back(true);

  // Call this propagator at least once the next time Propagate() is called.
  //
  // TODO(user): This initial propagation does not respect any later priority
  // settings. Fix this. Maybe we should force users to pass the priority at
  // registration. For now I didn't want to change the interface because there
  // are plans to implement a kind of "dynamic" priority, and if it works we may
  // want to get rid of this altogether.
  in_queue_.push_back(true);
  queue_by_priority_[1].push_back(id);
  return id;
}

void GenericLiteralWatcher::SetPropagatorPriority(int id, int priority) {
  id_to_priority_[id] = priority;
  if (priority >= queue_by_priority_.size()) {
    queue_by_priority_.resize(priority + 1);
  }
}

void GenericLiteralWatcher::NotifyThatPropagatorMayNotReachFixedPointInOnePass(
    int id) {
  id_to_idempotence_[id] = false;
}

void GenericLiteralWatcher::AlwaysCallAtLevelZero(int id) {
  propagator_ids_to_call_at_level_zero_.push_back(id);
}

void GenericLiteralWatcher::RegisterReversibleClass(int id,
                                                    ReversibleInterface* rev) {
  id_need_reversible_support_[id] = true;
  id_to_reversible_classes_[id].push_back(rev);
}

void GenericLiteralWatcher::RegisterReversibleInt(int id, int* rev) {
  id_need_reversible_support_[id] = true;
  id_to_reversible_ints_[id].push_back(rev);
}

}  // namespace sat
}  // namespace operations_research
