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

#include "ortools/sat/integer.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <type_traits>

#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/stl_util.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

std::vector<IntegerVariable> NegationOf(
    const std::vector<IntegerVariable>& vars) {
  std::vector<IntegerVariable> result(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    result[i] = NegationOf(vars[i]);
  }
  return result;
}

IntegerValue AffineExpression::Min(IntegerTrail* integer_trail) const {
  IntegerValue result = constant;
  if (var != kNoIntegerVariable) {
    if (coeff > 0) {
      result += coeff * integer_trail->LowerBound(var);
    } else {
      result += coeff * integer_trail->UpperBound(var);
    }
  }
  return result;
}

IntegerValue AffineExpression::Max(IntegerTrail* integer_trail) const {
  IntegerValue result = constant;
  if (var != kNoIntegerVariable) {
    if (coeff > 0) {
      result += coeff * integer_trail->UpperBound(var);
    } else {
      result += coeff * integer_trail->LowerBound(var);
    }
  }
  return result;
}

bool AffineExpression::IsFixed(IntegerTrail* integer_trail) const {
  if (var == kNoIntegerVariable || coeff == 0) return true;
  return integer_trail->IsFixed(var);
}

void IntegerEncoder::FullyEncodeVariable(IntegerVariable var) {
  if (VariableIsFullyEncoded(var)) return;

  CHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
  CHECK(!(*domains_)[var].IsEmpty());  // UNSAT. We don't deal with that here.
  CHECK_LT((*domains_)[var].Size(), 100000)
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
  for (const int64_t v : (*domains_)[var].Values()) {
    tmp_values_.push_back(IntegerValue(v));
  }
  for (const IntegerValue v : tmp_values_) {
    GetOrCreateLiteralAssociatedToEquality(var, v);
  }

  // Mark var and Negation(var) as fully encoded.
  CHECK_LT(GetPositiveOnlyIndex(var), is_fully_encoded_.size());
  CHECK(!equality_by_var_[GetPositiveOnlyIndex(var)].empty());
  is_fully_encoded_[GetPositiveOnlyIndex(var)] = true;
}

bool IntegerEncoder::VariableIsFullyEncoded(IntegerVariable var) const {
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= is_fully_encoded_.size()) return false;

  // Once fully encoded, the status never changes.
  if (is_fully_encoded_[index]) return true;
  if (!VariableIsPositive(var)) var = PositiveVariable(var);

  // TODO(user): Cache result as long as equality_by_var_[index] is unchanged?
  // It might not be needed since if the variable is not fully encoded, then
  // PartialDomainEncoding() will filter unreachable values, and so the size
  // check will be false until further value have been encoded.
  const int64_t initial_domain_size = (*domains_)[var].Size();
  if (equality_by_var_[index].size() < initial_domain_size) return false;

  // This cleans equality_by_var_[index] as a side effect and in particular,
  // sorts it by values.
  PartialDomainEncoding(var);

  // TODO(user): Comparing the size might be enough, but we want to be always
  // valid even if either (*domains_[var]) or PartialDomainEncoding(var) are
  // not properly synced because the propagation is not finished.
  const auto& ref = equality_by_var_[index];
  int i = 0;
  for (const int64_t v : (*domains_)[var].Values()) {
    if (i < ref.size() && v == ref[i].value) {
      i++;
    }
  }
  if (i == ref.size()) {
    is_fully_encoded_[index] = true;
  }
  return is_fully_encoded_[index];
}

std::vector<IntegerEncoder::ValueLiteralPair>
IntegerEncoder::FullDomainEncoding(IntegerVariable var) const {
  CHECK(VariableIsFullyEncoded(var));
  return PartialDomainEncoding(var);
}

std::vector<IntegerEncoder::ValueLiteralPair>
IntegerEncoder::PartialDomainEncoding(IntegerVariable var) const {
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= equality_by_var_.size()) return {};

  int new_size = 0;
  std::vector<ValueLiteralPair>& ref = equality_by_var_[index];
  for (int i = 0; i < ref.size(); ++i) {
    const ValueLiteralPair pair = ref[i];
    if (sat_solver_->Assignment().LiteralIsFalse(pair.literal)) continue;
    if (sat_solver_->Assignment().LiteralIsTrue(pair.literal)) {
      ref.clear();
      ref.push_back(pair);
      new_size = 1;
      break;
    }
    ref[new_size++] = pair;
  }
  ref.resize(new_size);
  std::sort(ref.begin(), ref.end());

  std::vector<IntegerEncoder::ValueLiteralPair> result = ref;
  if (!VariableIsPositive(var)) {
    std::reverse(result.begin(), result.end());
    for (ValueLiteralPair& ref : result) ref.value = -ref.value;
  }
  return result;
}

std::vector<IntegerEncoder::ValueLiteralPair> IntegerEncoder::RawDomainEncoding(
    IntegerVariable var) const {
  CHECK(VariableIsPositive(var));
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= equality_by_var_.size()) return {};

  return equality_by_var_[index];
}

// Note that by not inserting the literal in "order" we can in the worst case
// use twice as much implication (2 by literals) instead of only one between
// consecutive literals.
void IntegerEncoder::AddImplications(
    const std::map<IntegerValue, Literal>& map,
    std::map<IntegerValue, Literal>::const_iterator it,
    Literal associated_lit) {
  if (!add_implications_) return;
  DCHECK_EQ(it->second, associated_lit);

  // Literal(after) => associated_lit
  auto after_it = it;
  ++after_it;
  if (after_it != map.end()) {
    sat_solver_->AddClauseDuringSearch(
        {after_it->second.Negated(), associated_lit});
  }

  // associated_lit => Literal(before)
  if (it != map.begin()) {
    auto before_it = it;
    --before_it;
    sat_solver_->AddClauseDuringSearch(
        {associated_lit.Negated(), before_it->second});
  }
}

void IntegerEncoder::AddAllImplicationsBetweenAssociatedLiterals() {
  CHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
  add_implications_ = true;
  for (const std::map<IntegerValue, Literal>& encoding : encoding_by_var_) {
    LiteralIndex previous = kNoLiteralIndex;
    for (const auto value_literal : encoding) {
      const Literal lit = value_literal.second;
      if (previous != kNoLiteralIndex) {
        // lit => previous.
        sat_solver_->AddBinaryClause(lit.Negated(), Literal(previous));
      }
      previous = lit.Index();
    }
  }
}

std::pair<IntegerLiteral, IntegerLiteral> IntegerEncoder::Canonicalize(
    IntegerLiteral i_lit) const {
  const IntegerVariable var(i_lit.var);
  IntegerValue after(i_lit.bound);
  IntegerValue before(i_lit.bound - 1);
  CHECK_GE(before, (*domains_)[var].Min());
  CHECK_LE(after, (*domains_)[var].Max());
  int64_t previous = std::numeric_limits<int64_t>::min();
  for (const ClosedInterval& interval : (*domains_)[var]) {
    if (before > previous && before < interval.start) before = previous;
    if (after > previous && after < interval.start) after = interval.start;
    if (after <= interval.end) break;
    previous = interval.end;
  }
  return {IntegerLiteral::GreaterOrEqual(var, after),
          IntegerLiteral::LowerOrEqual(var, before)};
}

Literal IntegerEncoder::GetOrCreateAssociatedLiteral(IntegerLiteral i_lit) {
  if (i_lit.bound <= (*domains_)[i_lit.var].Min()) {
    return GetTrueLiteral();
  }
  if (i_lit.bound > (*domains_)[i_lit.var].Max()) {
    return GetFalseLiteral();
  }

  const auto canonicalization = Canonicalize(i_lit);
  const IntegerLiteral new_lit = canonicalization.first;

  const LiteralIndex index = GetAssociatedLiteral(new_lit);
  if (index != kNoLiteralIndex) return Literal(index);
  const LiteralIndex n_index = GetAssociatedLiteral(canonicalization.second);
  if (n_index != kNoLiteralIndex) return Literal(n_index).Negated();

  ++num_created_variables_;
  const Literal literal(sat_solver_->NewBooleanVariable(), true);
  AssociateToIntegerLiteral(literal, new_lit);

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
  const Domain& domain = (*domains_)[var];
  if (!domain.Contains(value.value())) return GetFalseLiteral();
  if (value == domain.Min() && value == domain.Max()) {
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
  const auto& domain = (*domains_)[i_lit.var];
  const IntegerValue min(domain.Min());
  const IntegerValue max(domain.Max());
  if (i_lit.bound <= min) {
    sat_solver_->AddUnitClause(literal);
  } else if (i_lit.bound > max) {
    sat_solver_->AddUnitClause(literal.Negated());
  } else {
    const auto pair = Canonicalize(i_lit);
    HalfAssociateGivenLiteral(pair.first, literal);
    HalfAssociateGivenLiteral(pair.second, literal.Negated());

    // Detect the case >= max or <= min and properly register them. Note that
    // both cases will happen at the same time if there is just two possible
    // value in the domain.
    if (pair.first.bound == max) {
      AssociateToIntegerEqualValue(literal, i_lit.var, max);
    }
    if (-pair.second.bound == min) {
      AssociateToIntegerEqualValue(literal.Negated(), i_lit.var, min);
    }
  }
}

void IntegerEncoder::AssociateToIntegerEqualValue(Literal literal,
                                                  IntegerVariable var,
                                                  IntegerValue value) {
  // Detect literal view. Note that the same literal can be associated to more
  // than one variable, and thus already have a view. We don't change it in
  // this case.
  const Domain& domain = (*domains_)[var];
  if (value == 1 && domain.Min() >= 0 && domain.Max() <= 1) {
    if (literal.Index() >= literal_view_.size()) {
      literal_view_.resize(literal.Index().value() + 1, kNoIntegerVariable);
      literal_view_[literal.Index()] = var;
    } else if (literal_view_[literal.Index()] == kNoIntegerVariable) {
      literal_view_[literal.Index()] = var;
    }
  }
  if (value == -1 && domain.Min() >= -1 && domain.Max() <= 0) {
    if (literal.Index() >= literal_view_.size()) {
      literal_view_.resize(literal.Index().value() + 1, kNoIntegerVariable);
      literal_view_[literal.Index()] = NegationOf(var);
    } else if (literal_view_[literal.Index()] == kNoIntegerVariable) {
      literal_view_[literal.Index()] = NegationOf(var);
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
      DCHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
      sat_solver_->AddClauseDuringSearch({literal, representative.Negated()});
      sat_solver_->AddClauseDuringSearch({literal.Negated(), representative});
    }
    return;
  }

  // Fix literal for value outside the domain.
  if (!domain.Contains(value.value())) {
    sat_solver_->AddUnitClause(literal.Negated());
    return;
  }

  // Update equality_by_var. Note that due to the
  // equality_to_associated_literal_ hash table, there should never be any
  // duplicate values for a given variable.
  const PositiveOnlyIndex index = GetPositiveOnlyIndex(var);
  if (index >= equality_by_var_.size()) {
    equality_by_var_.resize(index.value() + 1);
    is_fully_encoded_.resize(index.value() + 1);
  }
  equality_by_var_[index].push_back(
      ValueLiteralPair(VariableIsPositive(var) ? value : -value, literal));

  // Fix literal for constant domain.
  if (value == domain.Min() && value == domain.Max()) {
    sat_solver_->AddUnitClause(literal);
    return;
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
  if (new_size > full_reverse_encoding_.size()) {
    full_reverse_encoding_.resize(new_size);
  }
  full_reverse_encoding_[literal.Index()].push_back(le);
  full_reverse_encoding_[literal.Index()].push_back(ge);
}

// TODO(user): The hard constraints we add between associated literals seems to
// work for optional variables, but I am not 100% sure why!! I think it works
// because these literals can only appear in a conflict if the presence literal
// of the optional variables is true.
void IntegerEncoder::HalfAssociateGivenLiteral(IntegerLiteral i_lit,
                                               Literal literal) {
  // Resize reverse encoding.
  const int new_size = 1 + literal.Index().value();
  if (new_size > reverse_encoding_.size()) {
    reverse_encoding_.resize(new_size);
  }
  if (new_size > full_reverse_encoding_.size()) {
    full_reverse_encoding_.resize(new_size);
  }

  // Associate the new literal to i_lit.
  if (i_lit.var >= encoding_by_var_.size()) {
    encoding_by_var_.resize(i_lit.var.value() + 1);
  }
  auto& var_encoding = encoding_by_var_[i_lit.var];
  auto insert_result = var_encoding.insert({i_lit.bound, literal});
  if (insert_result.second) {  // New item.
    AddImplications(var_encoding, insert_result.first, literal);
    if (sat_solver_->Assignment().LiteralIsTrue(literal)) {
      if (sat_solver_->CurrentDecisionLevel() == 0) {
        newly_fixed_integer_literals_.push_back(i_lit);
      }
    }

    // TODO(user): do that for the other branch too?
    reverse_encoding_[literal.Index()].push_back(i_lit);
    full_reverse_encoding_[literal.Index()].push_back(i_lit);
  } else {
    const Literal associated(insert_result.first->second);
    if (associated != literal) {
      DCHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
      sat_solver_->AddClauseDuringSearch({literal, associated.Negated()});
      sat_solver_->AddClauseDuringSearch({literal.Negated(), associated});
    }
  }
}

bool IntegerEncoder::LiteralIsAssociated(IntegerLiteral i) const {
  if (i.var >= encoding_by_var_.size()) return false;
  const std::map<IntegerValue, Literal>& encoding = encoding_by_var_[i.var];
  return encoding.find(i.bound) != encoding.end();
}

LiteralIndex IntegerEncoder::GetAssociatedLiteral(IntegerLiteral i) const {
  if (i.var >= encoding_by_var_.size()) return kNoLiteralIndex;
  const std::map<IntegerValue, Literal>& encoding = encoding_by_var_[i.var];
  const auto result = encoding.find(i.bound);
  if (result == encoding.end()) return kNoLiteralIndex;
  return result->second.Index();
}

LiteralIndex IntegerEncoder::SearchForLiteralAtOrBefore(
    IntegerLiteral i, IntegerValue* bound) const {
  // We take the element before the upper_bound() which is either the encoding
  // of i if it already exists, or the encoding just before it.
  if (i.var >= encoding_by_var_.size()) return kNoLiteralIndex;
  const std::map<IntegerValue, Literal>& encoding = encoding_by_var_[i.var];
  auto after_it = encoding.upper_bound(i.bound);
  if (after_it == encoding.begin()) return kNoLiteralIndex;
  --after_it;
  *bound = after_it->first;
  return after_it->second.Index();
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
    reason_decision_levels_.push_back(literals_reason_starts_.size());
    CHECK_EQ(trail->CurrentDecisionLevel(), integer_search_levels_.size());
  }

  // This is used to map any integer literal out of the initial variable domain
  // into one that use one of the domain value.
  var_to_current_lb_interval_index_.SetLevel(level);

  // This is required because when loading a model it is possible that we add
  // (literal <-> integer literal) associations for literals that have already
  // been propagated here. This often happens when the presolve is off
  // and many variables are fixed.
  //
  // TODO(user): refactor the interaction IntegerTrail <-> IntegerEncoder so
  // that we can just push right away such literal. Unfortunately, this is is
  // a big chunck of work.
  if (level == 0) {
    for (const IntegerLiteral i_lit : encoder_->NewlyFixedIntegerLiterals()) {
      if (IsCurrentlyIgnored(i_lit.var)) continue;
      if (!Enqueue(i_lit, {}, {})) return false;
    }
    encoder_->ClearNewlyFixedIntegerLiterals();

    for (const IntegerLiteral i_lit : integer_literal_to_fix_) {
      if (IsCurrentlyIgnored(i_lit.var)) continue;
      if (!Enqueue(i_lit, {}, {})) return false;
    }
    integer_literal_to_fix_.clear();

    for (const Literal lit : literal_to_fix_) {
      if (trail_->Assignment().LiteralIsFalse(lit)) return false;
      if (trail_->Assignment().LiteralIsTrue(lit)) continue;
      trail_->EnqueueWithUnitReason(lit);
    }
    literal_to_fix_.clear();
  }

  // Process all the "associated" literals and Enqueue() the corresponding
  // bounds.
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    for (const IntegerLiteral i_lit : encoder_->GetIntegerLiterals(literal)) {
      if (IsCurrentlyIgnored(i_lit.var)) continue;

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
  var_to_current_lb_interval_index_.SetLevel(level);
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
  CHECK_GE(target, vars_.size());
  CHECK_LE(target, integer_trail_.size());

  for (int index = integer_trail_.size() - 1; index >= target; --index) {
    const TrailEntry& entry = integer_trail_[index];
    if (entry.var < 0) continue;  // entry used by EnqueueLiteral().
    vars_[entry.var].current_trail_index = entry.prev_trail_index;
    vars_[entry.var].current_bound =
        integer_trail_[entry.prev_trail_index].bound;
  }
  integer_trail_.resize(target);

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
  }

  // We notify the new level once all variables have been restored to their
  // old value.
  for (ReversibleInterface* rev : reversible_classes_) rev->SetLevel(level);
}

void IntegerTrail::ReserveSpaceForNumVariables(int num_vars) {
  // Because we always create both a variable and its negation.
  const int size = 2 * num_vars;
  vars_.reserve(size);
  is_ignored_literals_.reserve(size);
  integer_trail_.reserve(size);
  domains_->reserve(size);
  var_trail_index_cache_.reserve(size);
  tmp_var_to_trail_index_in_queue_.reserve(size);
}

IntegerVariable IntegerTrail::AddIntegerVariable(IntegerValue lower_bound,
                                                 IntegerValue upper_bound) {
  DCHECK_GE(lower_bound, kMinIntegerValue);
  DCHECK_LE(lower_bound, upper_bound);
  DCHECK_LE(upper_bound, kMaxIntegerValue);
  DCHECK(lower_bound >= 0 ||
         lower_bound + std::numeric_limits<int64_t>::max() >= upper_bound);
  DCHECK(integer_search_levels_.empty());
  DCHECK_EQ(vars_.size(), integer_trail_.size());

  const IntegerVariable i(vars_.size());
  is_ignored_literals_.push_back(kNoLiteralIndex);
  vars_.push_back({lower_bound, static_cast<int>(integer_trail_.size())});
  integer_trail_.push_back({lower_bound, i});
  domains_->push_back(Domain(lower_bound.value(), upper_bound.value()));

  // TODO(user): the is_ignored_literals_ Booleans are currently always the same
  // for a variable and its negation. So it may be better not to store it twice
  // so that we don't have to be careful when setting them.
  CHECK_EQ(NegationOf(i).value(), vars_.size());
  is_ignored_literals_.push_back(kNoLiteralIndex);
  vars_.push_back({-upper_bound, static_cast<int>(integer_trail_.size())});
  integer_trail_.push_back({-upper_bound, NegationOf(i)});
  domains_->push_back(Domain(-upper_bound.value(), -lower_bound.value()));

  var_trail_index_cache_.resize(vars_.size(), integer_trail_.size());
  tmp_var_to_trail_index_in_queue_.resize(vars_.size(), 0);

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
  return (*domains_)[var];
}

bool IntegerTrail::UpdateInitialDomain(IntegerVariable var, Domain domain) {
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);

  const Domain& old_domain = InitialVariableDomain(var);
  domain = domain.IntersectionWith(old_domain);
  if (old_domain == domain) return true;

  if (domain.IsEmpty()) return false;
  (*domains_)[var] = domain;
  (*domains_)[NegationOf(var)] = domain.Negation();
  if (domain.NumIntervals() > 1) {
    var_to_current_lb_interval_index_.Set(var, 0);
    var_to_current_lb_interval_index_.Set(NegationOf(var), 0);
  }

  // TODO(user): That works, but it might be better to simply update the
  // bounds here directly. This is because these function might call again
  // UpdateInitialDomain(), and we will abort after realizing that the domain
  // didn't change this time.
  CHECK(Enqueue(IntegerLiteral::GreaterOrEqual(var, IntegerValue(domain.Min())),
                {}, {}));
  CHECK(Enqueue(IntegerLiteral::LowerOrEqual(var, IntegerValue(domain.Max())),
                {}, {}));

  // Set to false excluded literals.
  int i = 0;
  int num_fixed = 0;
  for (const IntegerEncoder::ValueLiteralPair pair :
       encoder_->PartialDomainEncoding(var)) {
    while (i < domain.NumIntervals() && pair.value > domain[i].end) ++i;
    if (i == domain.NumIntervals() || pair.value < domain[i].start) {
      ++num_fixed;
      if (trail_->Assignment().LiteralIsTrue(pair.literal)) return false;
      if (!trail_->Assignment().LiteralIsFalse(pair.literal)) {
        trail_->EnqueueWithUnitReason(pair.literal.Negated());
      }
    }
  }
  if (num_fixed > 0) {
    VLOG(1)
        << "Domain intersection fixed " << num_fixed
        << " equality literal corresponding to values outside the new domain.";
  }

  return true;
}

IntegerVariable IntegerTrail::GetOrCreateConstantIntegerVariable(
    IntegerValue value) {
  auto insert = constant_map_.insert(std::make_pair(value, kNoIntegerVariable));
  if (insert.second) {  // new element.
    const IntegerVariable new_var = AddIntegerVariable(value, value);
    insert.first->second = new_var;
    if (value != 0) {
      // Note that this might invalidate insert.first->second.
      gtl::InsertOrDie(&constant_map_, -value, NegationOf(new_var));
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
  const int index_in_queue = tmp_var_to_trail_index_in_queue_[var];
  if (threshold <= index_in_queue) {
    if (index_in_queue != std::numeric_limits<int32_t>::max())
      has_dependency_ = true;
    return -1;
  }

  DCHECK_GE(threshold, vars_.size());
  int trail_index = vars_[var].current_trail_index;

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

  const int num_vars = vars_.size();
  return trail_index < num_vars ? -1 : trail_index;
}

int IntegerTrail::FindLowestTrailIndexThatExplainBound(
    IntegerLiteral i_lit) const {
  DCHECK_LE(i_lit.bound, vars_[i_lit.var].current_bound);
  if (i_lit.bound <= LevelZeroLowerBound(i_lit.var)) return -1;
  int trail_index = vars_[i_lit.var].current_trail_index;

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
    tmp_indices_[i] = vars_[(*reason)[i].var].current_trail_index;
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
    tmp_indices_.push_back(vars_[var].current_trail_index);
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
  const int num_vars = vars_.size();
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
    IntegerLiteral integer_literal, const LazyReasonFunction& lazy_reason,
    absl::Span<const Literal> literals_reason,
    absl::Span<const IntegerLiteral> bounds_reason) {
  DCHECK(tmp_queue_.empty());
  std::vector<Literal>* conflict = trail_->MutableConflict();
  if (lazy_reason == nullptr) {
    conflict->assign(literals_reason.begin(), literals_reason.end());
    const int num_vars = vars_.size();
    for (const IntegerLiteral& literal : bounds_reason) {
      const int trail_index = FindLowestTrailIndexThatExplainBound(literal);
      if (trail_index >= num_vars) tmp_queue_.push_back(trail_index);
    }
  } else {
    // We use the current trail index here.
    conflict->clear();
    lazy_reason(integer_literal, integer_trail_.size(), conflict, &tmp_queue_);
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
  const int num_vars = vars_.size();
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

bool IntegerTrail::Enqueue(IntegerLiteral i_lit,
                           absl::Span<const Literal> literal_reason,
                           absl::Span<const IntegerLiteral> integer_reason) {
  return EnqueueInternal(i_lit, nullptr, literal_reason, integer_reason,
                         integer_trail_.size());
}

bool IntegerTrail::ConditionalEnqueue(
    Literal lit, IntegerLiteral i_lit, std::vector<Literal>* literal_reason,
    std::vector<IntegerLiteral>* integer_reason) {
  const VariablesAssignment& assignment = trail_->Assignment();
  if (assignment.LiteralIsFalse(lit)) return true;

  // We can always push var if the optional literal is the same.
  //
  // TODO(user): we can also push lit.var if its presence implies lit.
  if (lit.Index() == OptionalLiteralIndex(i_lit.var)) {
    return Enqueue(i_lit, *literal_reason, *integer_reason);
  }

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

bool IntegerTrail::Enqueue(IntegerLiteral i_lit,
                           absl::Span<const Literal> literal_reason,
                           absl::Span<const IntegerLiteral> integer_reason,
                           int trail_index_with_same_reason) {
  return EnqueueInternal(i_lit, nullptr, literal_reason, integer_reason,
                         trail_index_with_same_reason);
}

bool IntegerTrail::Enqueue(IntegerLiteral i_lit,
                           LazyReasonFunction lazy_reason) {
  return EnqueueInternal(i_lit, lazy_reason, {}, {}, integer_trail_.size());
}

bool IntegerTrail::ReasonIsValid(
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  const VariablesAssignment& assignment = trail_->Assignment();
  for (const Literal lit : literal_reason) {
    if (!assignment.LiteralIsFalse(lit)) return false;
  }
  for (const IntegerLiteral i_lit : integer_reason) {
    if (i_lit.bound > vars_[i_lit.var].current_bound) {
      if (IsOptional(i_lit.var)) {
        const Literal is_ignored = IsIgnoredLiteral(i_lit.var);
        LOG(INFO) << "Reason " << i_lit << " is not true!"
                  << " optional variable:" << i_lit.var
                  << " present:" << assignment.LiteralIsFalse(is_ignored)
                  << " absent:" << assignment.LiteralIsTrue(is_ignored)
                  << " current_lb:" << vars_[i_lit.var].current_bound;
      } else {
        LOG(INFO) << "Reason " << i_lit << " is not true!"
                  << " non-optional variable:" << i_lit.var
                  << " current_lb:" << vars_[i_lit.var].current_bound;
      }
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

void IntegerTrail::EnqueueLiteral(
    Literal literal, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  EnqueueLiteralInternal(literal, nullptr, literal_reason, integer_reason);
}

void IntegerTrail::EnqueueLiteralInternal(
    Literal literal, LazyReasonFunction lazy_reason,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  DCHECK(!trail_->Assignment().LiteralIsAssigned(literal));
  DCHECK(lazy_reason != nullptr ||
         ReasonIsValid(literal_reason, integer_reason));
  if (integer_search_levels_.empty()) {
    // Level zero. We don't keep any reason.
    trail_->EnqueueWithUnitReason(literal);
    return;
  }

  // If we are fixing something at a positive level, remember it.
  if (!integer_search_levels_.empty() && integer_reason.empty() &&
      literal_reason.empty() && lazy_reason == nullptr) {
    literal_to_fix_.push_back(literal);
  }

  const int trail_index = trail_->Index();
  if (trail_index >= boolean_trail_index_to_integer_one_.size()) {
    boolean_trail_index_to_integer_one_.resize(trail_index + 1);
  }
  boolean_trail_index_to_integer_one_[trail_index] = integer_trail_.size();

  int reason_index = literals_reason_starts_.size();
  if (lazy_reason != nullptr) {
    if (integer_trail_.size() >= lazy_reasons_.size()) {
      lazy_reasons_.resize(integer_trail_.size() + 1, nullptr);
    }
    lazy_reasons_[integer_trail_.size()] = lazy_reason;
    reason_index = -1;
  } else {
    // Copy the reason.
    literals_reason_starts_.push_back(literals_reason_buffer_.size());
    literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                   literal_reason.begin(),
                                   literal_reason.end());
    bounds_reason_starts_.push_back(bounds_reason_buffer_.size());
    bounds_reason_buffer_.insert(bounds_reason_buffer_.end(),
                                 integer_reason.begin(), integer_reason.end());
  }

  integer_trail_.push_back({/*bound=*/IntegerValue(0),
                            /*var=*/kNoIntegerVariable,
                            /*prev_trail_index=*/-1,
                            /*reason_index=*/reason_index});

  trail_->Enqueue(literal, propagator_id_);
}

// We count the number of propagation at the current level, and returns true
// if it seems really large. Note that we disable this if we are in fixed
// search.
bool IntegerTrail::InPropagationLoop() const {
  const int num_vars = vars_.size();
  return (!integer_search_levels_.empty() &&
          integer_trail_.size() - integer_search_levels_.back() >
              std::max(10000, 10 * num_vars) &&
          parameters_.search_branching() != SatParameters::FIXED_SEARCH);
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
  for (IntegerVariable var(0); var < vars_.size(); var += 2) {
    if (IsCurrentlyIgnored(var)) continue;
    if (!IsFixed(var)) return var;
  }
  return kNoIntegerVariable;
}

bool IntegerTrail::EnqueueInternal(
    IntegerLiteral i_lit, LazyReasonFunction lazy_reason,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason,
    int trail_index_with_same_reason) {
  DCHECK(lazy_reason != nullptr ||
         ReasonIsValid(literal_reason, integer_reason));

  const IntegerVariable var(i_lit.var);

  // No point doing work if the variable is already ignored.
  if (IsCurrentlyIgnored(var)) return true;

  // Nothing to do if the bound is not better than the current one.
  // TODO(user): Change this to a CHECK? propagator shouldn't try to push such
  // bound and waste time explaining it.
  if (i_lit.bound <= vars_[var].current_bound) return true;
  ++num_enqueues_;

  // If the domain of var is not a single intervals and i_lit.bound fall into a
  // "hole", we increase it to the next possible value. This ensure that we
  // never Enqueue() non-canonical literals. See also Canonicalize().
  //
  // Note: The literals in the reason are not necessarily canonical, but then
  // we always map these to enqueued literals during conflict resolution.
  if ((*domains_)[var].NumIntervals() > 1) {
    const auto& domain = (*domains_)[var];
    int index = var_to_current_lb_interval_index_.FindOrDie(var);
    const int size = domain.NumIntervals();
    while (index < size && i_lit.bound > domain[index].end) {
      ++index;
    }
    if (index == size) {
      return ReportConflict(literal_reason, integer_reason);
    } else {
      var_to_current_lb_interval_index_.Set(var, index);
      i_lit.bound = std::max(i_lit.bound, IntegerValue(domain[index].start));
    }
  }

  // Check if the integer variable has an empty domain.
  if (i_lit.bound > UpperBound(var)) {
    // We relax the upper bound as much as possible to still have a conflict.
    const auto ub_reason = IntegerLiteral::LowerOrEqual(var, i_lit.bound - 1);

    if (!IsOptional(var) || trail_->Assignment().LiteralIsFalse(
                                Literal(is_ignored_literals_[var]))) {
      // Note that we want only one call to MergeReasonIntoInternal() for
      // efficiency and a potential smaller reason.
      auto* conflict = InitializeConflict(i_lit, lazy_reason, literal_reason,
                                          integer_reason);
      if (IsOptional(var)) {
        conflict->push_back(Literal(is_ignored_literals_[var]));
      }
      {
        const int trail_index = FindLowestTrailIndexThatExplainBound(ub_reason);
        const int num_vars = vars_.size();  // must be signed.
        if (trail_index >= num_vars) tmp_queue_.push_back(trail_index);
      }
      MergeReasonIntoInternal(conflict);
      return false;
    } else {
      // Note(user): We never make the bound of an optional literal cross. We
      // used to have a bug where we propagated these bounds and their
      // associated literals, and we were reaching a conflict while propagating
      // the associated literal instead of setting is_ignored below to false.
      const Literal is_ignored = Literal(is_ignored_literals_[var]);
      if (integer_search_levels_.empty()) {
        trail_->EnqueueWithUnitReason(is_ignored);
      } else {
        // Here we currently expand any lazy reason because we need to add
        // to it the reason for the upper bound.
        // TODO(user): A possible solution would be to support the two types
        // of reason (lazy and not) at the same time and use the union of both?
        if (lazy_reason != nullptr) {
          lazy_reason(i_lit, integer_trail_.size(), &lazy_reason_literals_,
                      &lazy_reason_trail_indices_);
          std::vector<IntegerLiteral> temp;
          for (const int trail_index : lazy_reason_trail_indices_) {
            const TrailEntry& entry = integer_trail_[trail_index];
            temp.push_back(IntegerLiteral(entry.var, entry.bound));
          }
          EnqueueLiteral(is_ignored, lazy_reason_literals_, temp);
        } else {
          EnqueueLiteral(is_ignored, literal_reason, integer_reason);
        }

        // Hack, we add the upper bound reason here.
        bounds_reason_buffer_.push_back(ub_reason);
      }
      return true;
    }
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

  if (!integer_search_levels_.empty() && integer_reason.empty() &&
      literal_reason.empty() && lazy_reason == nullptr &&
      trail_index_with_same_reason >= integer_trail_.size()) {
    integer_literal_to_fix_.push_back(i_lit);
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
  if (literal_index != kNoLiteralIndex) {
    const Literal to_enqueue = Literal(literal_index);
    if (trail_->Assignment().LiteralIsFalse(to_enqueue)) {
      auto* conflict = InitializeConflict(i_lit, lazy_reason, literal_reason,
                                          integer_reason);
      conflict->push_back(to_enqueue);
      MergeReasonIntoInternal(conflict);
      return false;
    }

    // If the associated literal exactly correspond to i_lit, then we push
    // it first, and then we use it as a reason for i_lit. We do that so that
    // MergeReasonIntoInternal() will not unecessarily expand further the reason
    // for i_lit.
    if (IntegerLiteral::GreaterOrEqual(i_lit.var, bound) == i_lit) {
      if (!trail_->Assignment().LiteralIsTrue(to_enqueue)) {
        EnqueueLiteralInternal(to_enqueue, lazy_reason, literal_reason,
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
        const int trail_index = trail_->Index();
        if (trail_index >= boolean_trail_index_to_integer_one_.size()) {
          boolean_trail_index_to_integer_one_.resize(trail_index + 1);
        }
        boolean_trail_index_to_integer_one_[trail_index] =
            trail_index_with_same_reason;
        trail_->Enqueue(to_enqueue, propagator_id_);
      }
    }
  }

  // Special case for level zero.
  if (integer_search_levels_.empty()) {
    ++num_level_zero_enqueues_;
    vars_[i_lit.var].current_bound = i_lit.bound;
    integer_trail_[i_lit.var.value()].bound = i_lit.bound;

    // We also update the initial domain. If this fail, since we are at level
    // zero, we don't care about the reason.
    trail_->MutableConflict()->clear();
    return UpdateInitialDomain(
        i_lit.var,
        Domain(LowerBound(i_lit.var).value(), UpperBound(i_lit.var).value()));
  }
  DCHECK_GT(trail_->CurrentDecisionLevel(), 0);

  int reason_index = literals_reason_starts_.size();
  if (lazy_reason != nullptr) {
    if (integer_trail_.size() >= lazy_reasons_.size()) {
      lazy_reasons_.resize(integer_trail_.size() + 1, nullptr);
    }
    lazy_reasons_[integer_trail_.size()] = lazy_reason;
    reason_index = -1;
  } else if (trail_index_with_same_reason >= integer_trail_.size()) {
    // Save the reason into our internal buffers.
    literals_reason_starts_.push_back(literals_reason_buffer_.size());
    if (!literal_reason.empty()) {
      literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                     literal_reason.begin(),
                                     literal_reason.end());
    }
    bounds_reason_starts_.push_back(bounds_reason_buffer_.size());
    if (!integer_reason.empty()) {
      bounds_reason_buffer_.insert(bounds_reason_buffer_.end(),
                                   integer_reason.begin(),
                                   integer_reason.end());
    }
  } else {
    reason_index = integer_trail_[trail_index_with_same_reason].reason_index;
  }

  const int prev_trail_index = vars_[i_lit.var].current_trail_index;
  integer_trail_.push_back({/*bound=*/i_lit.bound,
                            /*var=*/i_lit.var,
                            /*prev_trail_index=*/prev_trail_index,
                            /*reason_index=*/reason_index});

  vars_[i_lit.var].current_bound = i_lit.bound;
  vars_[i_lit.var].current_trail_index = integer_trail_.size() - 1;
  return true;
}

bool IntegerTrail::EnqueueAssociatedIntegerLiteral(IntegerLiteral i_lit,
                                                   Literal literal_reason) {
  DCHECK(ReasonIsValid({literal_reason.Negated()}, {}));
  DCHECK(!IsCurrentlyIgnored(i_lit.var));

  // Nothing to do if the bound is not better than the current one.
  if (i_lit.bound <= vars_[i_lit.var].current_bound) return true;
  ++num_enqueues_;

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
    vars_[i_lit.var].current_bound = i_lit.bound;
    integer_trail_[i_lit.var.value()].bound = i_lit.bound;

    // We also update the initial domain. If this fail, since we are at level
    // zero, we don't care about the reason.
    trail_->MutableConflict()->clear();
    return UpdateInitialDomain(
        i_lit.var,
        Domain(LowerBound(i_lit.var).value(), UpperBound(i_lit.var).value()));
  }
  DCHECK_GT(trail_->CurrentDecisionLevel(), 0);

  const int reason_index = literals_reason_starts_.size();
  CHECK_EQ(reason_index, bounds_reason_starts_.size());
  literals_reason_starts_.push_back(literals_reason_buffer_.size());
  bounds_reason_starts_.push_back(bounds_reason_buffer_.size());
  literals_reason_buffer_.push_back(literal_reason.Negated());

  const int prev_trail_index = vars_[i_lit.var].current_trail_index;
  integer_trail_.push_back({/*bound=*/i_lit.bound,
                            /*var=*/i_lit.var,
                            /*prev_trail_index=*/prev_trail_index,
                            /*reason_index=*/reason_index});

  vars_[i_lit.var].current_bound = i_lit.bound;
  vars_[i_lit.var].current_trail_index = integer_trail_.size() - 1;
  return true;
}

void IntegerTrail::ComputeLazyReasonIfNeeded(int trail_index) const {
  const int reason_index = integer_trail_[trail_index].reason_index;
  if (reason_index == -1) {
    const TrailEntry& entry = integer_trail_[trail_index];
    const IntegerLiteral literal(entry.var, entry.bound);
    lazy_reasons_[trail_index](literal, trail_index, &lazy_reason_literals_,
                               &lazy_reason_trail_indices_);
  }
}

absl::Span<const int> IntegerTrail::Dependencies(int trail_index) const {
  const int reason_index = integer_trail_[trail_index].reason_index;
  if (reason_index == -1) {
    return absl::Span<const int>(lazy_reason_trail_indices_);
  }

  const int start = bounds_reason_starts_[reason_index];
  const int end = reason_index + 1 < bounds_reason_starts_.size()
                      ? bounds_reason_starts_[reason_index + 1]
                      : bounds_reason_buffer_.size();
  if (start == end) return {};

  // Cache the result if not already computed. Remark, if the result was never
  // computed then the span trail_index_reason_buffer_[start, end) will either
  // be non-existent or full of -1.
  //
  // TODO(user): For empty reason, we will always recompute them.
  if (end > trail_index_reason_buffer_.size()) {
    trail_index_reason_buffer_.resize(end, -1);
  }
  if (trail_index_reason_buffer_[start] == -1) {
    int new_end = start;
    const int num_vars = vars_.size();
    for (int i = start; i < end; ++i) {
      const int dep =
          FindLowestTrailIndexThatExplainBound(bounds_reason_buffer_[i]);
      if (dep >= num_vars) {
        trail_index_reason_buffer_[new_end++] = dep;
      }
    }
    return absl::Span<const int>(&trail_index_reason_buffer_[start],
                                 new_end - start);
  } else {
    // TODO(user): We didn't store new_end in a previous call, so end might be
    // larger. That is a bit annoying since we have to test for -1 while
    // iterating.
    return absl::Span<const int>(&trail_index_reason_buffer_[start],
                                 end - start);
  }
}

void IntegerTrail::AppendLiteralsReason(int trail_index,
                                        std::vector<Literal>* output) const {
  CHECK_GE(trail_index, vars_.size());
  const int reason_index = integer_trail_[trail_index].reason_index;
  if (reason_index == -1) {
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

// TODO(user): If this is called many time on the same variables, it could be
// made faster by using some caching mecanism.
void IntegerTrail::MergeReasonInto(absl::Span<const IntegerLiteral> literals,
                                   std::vector<Literal>* output) const {
  DCHECK(tmp_queue_.empty());
  const int num_vars = vars_.size();
  for (const IntegerLiteral& literal : literals) {
    const int trail_index = FindLowestTrailIndexThatExplainBound(literal);

    // Any indices lower than that means that there is no reason needed.
    // Note that it is important for size to be signed because of -1 indices.
    if (trail_index >= num_vars) tmp_queue_.push_back(trail_index);
  }
  return MergeReasonIntoInternal(output);
}

// This will expand the reason of the IntegerLiteral already in tmp_queue_ until
// everything is explained in term of Literal.
void IntegerTrail::MergeReasonIntoInternal(std::vector<Literal>* output) const {
  // All relevant trail indices will be >= vars_.size(), so we can safely use
  // zero to means that no literal refering to this variable is in the queue.
  DCHECK(std::all_of(tmp_var_to_trail_index_in_queue_.begin(),
                     tmp_var_to_trail_index_in_queue_.end(),
                     [](int v) { return v == 0; }));

  added_variables_.ClearAndResize(BooleanVariable(trail_->NumVariables()));
  for (const Literal l : *output) {
    added_variables_.Set(l.Variable());
  }

  // During the algorithm execution, all the queue entries that do not match the
  // content of tmp_var_to_trail_index_in_queue_[] will be ignored.
  for (const int trail_index : tmp_queue_) {
    DCHECK_GE(trail_index, vars_.size());
    DCHECK_LT(trail_index, integer_trail_.size());
    const TrailEntry& entry = integer_trail_[trail_index];
    tmp_var_to_trail_index_in_queue_[entry.var] =
        std::max(tmp_var_to_trail_index_in_queue_[entry.var], trail_index);
  }

  // We manage our heap by hand so that we can range iterate over it above, and
  // this initial heapify is faster.
  std::make_heap(tmp_queue_.begin(), tmp_queue_.end());

  // We process the entries by highest trail_index first. The content of the
  // queue will always be a valid reason for the literals we already added to
  // the output.
  tmp_to_clear_.clear();
  while (!tmp_queue_.empty()) {
    const int trail_index = tmp_queue_.front();
    const TrailEntry& entry = integer_trail_[trail_index];
    std::pop_heap(tmp_queue_.begin(), tmp_queue_.end());
    tmp_queue_.pop_back();

    // Skip any stale queue entry. Amongst all the entry refering to a given
    // variable, only the latest added to the queue is valid and we detect it
    // using its trail index.
    if (tmp_var_to_trail_index_in_queue_[entry.var] != trail_index) {
      continue;
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
        CHECK_NE(reason_index, -1);
        {
          const int start = literals_reason_starts_[reason_index];
          const int end = reason_index + 1 < literals_reason_starts_.size()
                              ? literals_reason_starts_[reason_index + 1]
                              : literals_reason_buffer_.size();
          CHECK_EQ(start + 1, end);
          CHECK_EQ(literals_reason_buffer_[start],
                   Literal(associated_lit).Negated());
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

    // Process this entry. Note that if any of the next expansion include the
    // variable entry.var in their reason, we must process it again because we
    // cannot easily detect if it was needed to infer the current entry.
    //
    // Important: the queue might already contains entries refering to the same
    // variable. The code act like if we deleted all of them at this point, we
    // just do that lazily. tmp_var_to_trail_index_in_queue_[var] will
    // only refer to newly added entries.
    tmp_var_to_trail_index_in_queue_[entry.var] = 0;
    has_dependency_ = false;

    ComputeLazyReasonIfNeeded(trail_index);
    AppendLiteralsReason(trail_index, output);
    for (const int next_trail_index : Dependencies(trail_index)) {
      if (next_trail_index < 0) break;
      DCHECK_LT(next_trail_index, trail_index);
      const TrailEntry& next_entry = integer_trail_[next_trail_index];

      // Only add literals that are not "implied" by the ones already present.
      // For instance, do not add (x >= 4) if we already have (x >= 7). This
      // translate into only adding a trail index if it is larger than the one
      // in the queue refering to the same variable.
      const int index_in_queue =
          tmp_var_to_trail_index_in_queue_[next_entry.var];
      if (index_in_queue != std::numeric_limits<int32_t>::max())
        has_dependency_ = true;
      if (next_trail_index > index_in_queue) {
        tmp_var_to_trail_index_in_queue_[next_entry.var] = next_trail_index;
        tmp_queue_.push_back(next_trail_index);
        std::push_heap(tmp_queue_.begin(), tmp_queue_.end());
      }
    }

    // Special case for a "leaf", we will never need this variable again.
    if (!has_dependency_) {
      tmp_to_clear_.push_back(entry.var);
      tmp_var_to_trail_index_in_queue_[entry.var] =
          std::numeric_limits<int32_t>::max();
    }
  }

  // clean-up.
  for (const IntegerVariable var : tmp_to_clear_) {
    tmp_var_to_trail_index_in_queue_[var] = 0;
  }
}

absl::Span<const Literal> IntegerTrail::Reason(const Trail& trail,
                                               int trail_index) const {
  const int index = boolean_trail_index_to_integer_one_[trail_index];
  std::vector<Literal>* reason = trail.GetEmptyVectorToStoreReason(trail_index);
  added_variables_.ClearAndResize(BooleanVariable(trail_->NumVariables()));

  ComputeLazyReasonIfNeeded(index);
  AppendLiteralsReason(index, reason);
  DCHECK(tmp_queue_.empty());
  for (const int prev_trail_index : Dependencies(index)) {
    if (prev_trail_index < 0) break;
    DCHECK_GE(prev_trail_index, vars_.size());
    tmp_queue_.push_back(prev_trail_index);
  }
  MergeReasonIntoInternal(reason);
  return *reason;
}

// TODO(user): Implement a dense version if there is more trail entries
// than variables!
void IntegerTrail::AppendNewBounds(std::vector<IntegerLiteral>* output) const {
  tmp_marked_.ClearAndResize(IntegerVariable(vars_.size()));

  // In order to push the best bound for each variable, we loop backward.
  const int end = vars_.size();
  for (int i = integer_trail_.size(); --i >= end;) {
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

void GenericLiteralWatcher::UpdateCallingNeeds(Trail* trail) {
  // Process any new Literal on the trail.
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (literal.Index() >= literal_to_watcher_.size()) continue;
    for (const auto entry : literal_to_watcher_[literal.Index()]) {
      if (!in_queue_[entry.id]) {
        in_queue_[entry.id] = true;
        queue_by_priority_[id_to_priority_[entry.id]].push_back(entry.id);
      }
      if (entry.watch_index >= 0) {
        id_to_watch_indices_[entry.id].push_back(entry.watch_index);
      }
    }
  }

  // Process the newly changed variables lower bounds.
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var.value() >= var_to_watcher_.size()) continue;
    for (const auto entry : var_to_watcher_[var]) {
      if (!in_queue_[entry.id]) {
        in_queue_[entry.id] = true;
        queue_by_priority_[id_to_priority_[entry.id]].push_back(entry.id);
      }
      if (entry.watch_index >= 0) {
        id_to_watch_indices_[entry.id].push_back(entry.watch_index);
      }
    }
  }

  if (trail->CurrentDecisionLevel() == 0) {
    const std::vector<IntegerVariable>& modified_vars =
        modified_vars_.PositionsSetAtLeastOnce();
    for (const auto& callback : level_zero_modified_variable_callback_) {
      callback(modified_vars);
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
      if (in_queue_[id]) continue;
      in_queue_[id] = true;
      queue_by_priority_[id_to_priority_[id]].push_back(id);
    }
  }

  UpdateCallingNeeds(trail);

  // Note that the priority may be set to -1 inside the loop in order to restart
  // at zero.
  int test_limit = 0;
  for (int priority = 0; priority < queue_by_priority_.size(); ++priority) {
    // We test the time limit from time to time. This is in order to return in
    // case of slow propagation.
    //
    // TODO(user): The queue will not be emptied, but I am not sure the solver
    // will be left in an usable state. Fix if it become needed to resume
    // the solve from the last time it was interrupted.
    if (test_limit > 100) {
      test_limit = 0;
      if (time_limit_->LimitReached()) break;
    }

    std::deque<int>& queue = queue_by_priority_[priority];
    while (!queue.empty()) {
      const int id = queue.front();
      current_id_ = id;
      queue.pop_front();

      // Before we propagate, make sure any reversible structure are up to date.
      // Note that we never do anything expensive more than once per level.
      {
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

      // TODO(user): Maybe just provide one function Propagate(watch_indices) ?
      std::vector<int>& watch_indices_ref = id_to_watch_indices_[id];
      const bool result =
          watch_indices_ref.empty()
              ? watchers_[id]->Propagate()
              : watchers_[id]->IncrementalPropagate(watch_indices_ref);
      if (!result) {
        watch_indices_ref.clear();
        in_queue_[id] = false;
        return false;
      }

      // Update the propagation queue. At this point, the propagator has been
      // removed from the queue but in_queue_ is still true.
      if (id_to_idempotence_[id]) {
        // If the propagator is assumed to be idempotent, then we set in_queue_
        // to false after UpdateCallingNeeds() so this later function will never
        // add it back.
        UpdateCallingNeeds(trail);
        watch_indices_ref.clear();
        in_queue_[id] = false;
      } else {
        // Otherwise, we set in_queue_ to false first so that
        // UpdateCallingNeeds() may add it back if the propagator modified any
        // of its watched variables.
        watch_indices_ref.clear();
        in_queue_[id] = false;
        UpdateCallingNeeds(trail);
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
  return true;
}

void GenericLiteralWatcher::Untrail(const Trail& trail, int trail_index) {
  if (propagation_trail_index_ <= trail_index) {
    // Nothing to do since we found a conflict before Propagate() was called.
    CHECK_EQ(propagation_trail_index_, trail_index);
    return;
  }

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
  id_to_reversible_classes_[id].push_back(rev);
}

void GenericLiteralWatcher::RegisterReversibleInt(int id, int* rev) {
  id_to_reversible_ints_[id].push_back(rev);
}

// This is really close to ExcludeCurrentSolutionAndBacktrack().
std::function<void(Model*)>
ExcludeCurrentSolutionWithoutIgnoredVariableAndBacktrack() {
  return [=](Model* model) {
    SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();

    const int current_level = sat_solver->CurrentDecisionLevel();
    std::vector<Literal> clause_to_exclude_solution;
    clause_to_exclude_solution.reserve(current_level);
    for (int i = 0; i < current_level; ++i) {
      bool include_decision = true;
      const Literal decision = sat_solver->Decisions()[i].literal;

      // Tests if this decision is associated to a bound of an ignored variable
      // in the current assignment.
      const InlinedIntegerLiteralVector& associated_literals =
          encoder->GetIntegerLiterals(decision);
      for (const IntegerLiteral bound : associated_literals) {
        if (integer_trail->IsCurrentlyIgnored(bound.var)) {
          // In this case we replace the decision (which is a bound on an
          // ignored variable) with the fact that the integer variable was
          // ignored. This works because the only impact a bound of an ignored
          // variable can have on the rest of the model is through the
          // is_ignored literal.
          clause_to_exclude_solution.push_back(
              integer_trail->IsIgnoredLiteral(bound.var).Negated());
          include_decision = false;
        }
      }

      if (include_decision) {
        clause_to_exclude_solution.push_back(decision.Negated());
      }
    }

    // Note that it is okay to add duplicates literals in ClauseConstraint(),
    // the clause will be preprocessed correctly.
    sat_solver->Backtrack(0);
    model->Add(ClauseConstraint(clause_to_exclude_solution));
  };
}

}  // namespace sat
}  // namespace operations_research
