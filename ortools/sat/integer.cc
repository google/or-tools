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

#include "ortools/sat/integer.h"

#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/stl_util.h"

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

void IntegerEncoder::FullyEncodeVariable(
    IntegerVariable i_var, const std::vector<ClosedInterval>& domain) {
  CHECK(!VariableIsFullyEncoded(i_var));
  CHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
  CHECK(!domain.empty());  // UNSAT problem. We don't deal with that here.

  std::vector<IntegerValue> values;
  for (const ClosedInterval interval : domain) {
    for (IntegerValue v(interval.start); v <= interval.end; ++v) {
      values.push_back(v);
      CHECK_LT(values.size(), 100000) << "Domain too large for full encoding.";
    }
  }

  // TODO(user): This case is annoying, not sure yet how to best fix the
  // variable. There is certainly no need to create a Boolean variable, but
  // one needs to talk to IntegerTrail to fix the variable and we don't want
  // the encoder to depend on this. So for now we fail here and it is up to
  // the caller to deal with this case.
  CHECK_NE(values.size(), 1);

  std::vector<Literal> literals;
  if (values.size() == 2) {
    const BooleanVariable var = sat_solver_->NewBooleanVariable();
    literals.push_back(Literal(var, true));
    literals.push_back(Literal(var, false));
  } else {
    for (int i = 0; i < values.size(); ++i) {
      const BooleanVariable var = sat_solver_->NewBooleanVariable();
      literals.push_back(Literal(var, true));
    }
  }
  return FullyEncodeVariableUsingGivenLiterals(i_var, literals, values);
}

void IntegerEncoder::FullyEncodeVariableUsingGivenLiterals(
    IntegerVariable i_var, const std::vector<Literal>& literals,
    const std::vector<IntegerValue>& values) {
  CHECK(!VariableIsFullyEncoded(i_var));
  CHECK(!literals.empty());
  CHECK_NE(literals.size(), 1);

  // Sort the literals by values.
  std::vector<ValueLiteralPair> encoding;
  std::vector<sat::LiteralWithCoeff> cst;
  for (int i = 0; i < values.size(); ++i) {
    const Literal literal = literals[i];
    const IntegerValue value = values[i];
    encoding.push_back({value, literal});
    cst.push_back(LiteralWithCoeff(literal, Coefficient(1)));
  }
  std::sort(encoding.begin(), encoding.end());

  // Create the associated literal (<= and >=) in order (best for the
  // implications between them). Note that we only create literals like this for
  // value inside the domain. This is nice since these will be the only kind of
  // literal pushed by Enqueue() (we look at the domain there).
  for (int i = 0; i + 1 < encoding.size(); ++i) {
    const IntegerLiteral i_lit =
        IntegerLiteral::LowerOrEqual(i_var, encoding[i].value);
    const IntegerLiteral i_lit_negated =
        IntegerLiteral::GreaterOrEqual(i_var, encoding[i + 1].value);
    if (i == 0) {
      // Special case for the start.
      HalfAssociateGivenLiteral(i_lit, encoding[0].literal);
      HalfAssociateGivenLiteral(i_lit_negated, encoding[0].literal.Negated());
    } else if (i + 2 == encoding.size()) {
      // Special case for the end.
      HalfAssociateGivenLiteral(i_lit, encoding.back().literal.Negated());
      HalfAssociateGivenLiteral(i_lit_negated, encoding.back().literal);
    } else {
      // Normal case.
      if (!LiteralIsAssociated(i_lit) || !LiteralIsAssociated(i_lit_negated)) {
        const BooleanVariable new_var = sat_solver_->NewBooleanVariable();
        const Literal literal(new_var, true);
        HalfAssociateGivenLiteral(i_lit, literal);
        HalfAssociateGivenLiteral(i_lit_negated, literal.Negated());
      }
    }
  }

  // Now that all literals are created, wire them together using
  //    (X == v)  <=>  (X >= v) and (X <= v).
  for (int i = 1; i + 1 < encoding.size(); ++i) {
    const ValueLiteralPair pair = encoding[i];
    const Literal a(GetAssociatedLiteral(
        IntegerLiteral::GreaterOrEqual(i_var, pair.value)));
    const Literal b(
        GetAssociatedLiteral(IntegerLiteral::LowerOrEqual(i_var, pair.value)));
    sat_solver_->AddBinaryClause(a, pair.literal.Negated());
    sat_solver_->AddBinaryClause(b, pair.literal.Negated());
    sat_solver_->AddProblemClause({a.Negated(), b.Negated(), pair.literal});
  }

  full_encoding_index_[i_var] = full_encoding_.size();
  full_encoding_.push_back(encoding);  // copy because we need it below.

  // Deal with NegationOf(i_var).
  //
  // TODO(user): This seems a bit wasted, but it does simplify the code at a
  // somehow small cost.
  std::reverse(encoding.begin(), encoding.end());
  for (auto& entry : encoding) {
    entry.value = -entry.value;  // Reverse the value.
  }
  full_encoding_index_[NegationOf(i_var)] = full_encoding_.size();
  full_encoding_.push_back(std::move(encoding));
}

// Note that by not inserting the literal in "order" we can in the worst case
// use twice as much implication (2 by literals) instead of only one between
// consecutive literals.
void IntegerEncoder::AddImplications(IntegerLiteral i_lit, Literal literal) {
  if (i_lit.var >= encoding_by_var_.size()) {
    encoding_by_var_.resize(i_lit.var + 1);
  }

  std::map<IntegerValue, Literal>& map_ref =
      encoding_by_var_[IntegerVariable(i_lit.var)];
  CHECK(!ContainsKey(map_ref, i_lit.bound));

  auto after_it = map_ref.lower_bound(i_lit.bound);
  if (after_it != map_ref.end()) {
    // Literal(after) => literal
    if (sat_solver_->CurrentDecisionLevel() == 0) {
      sat_solver_->AddBinaryClause(after_it->second.Negated(), literal);
    } else {
      sat_solver_->AddBinaryClauseDuringSearch(after_it->second.Negated(),
                                               literal);
    }
  }
  if (after_it != map_ref.begin()) {
    // literal => Literal(before)
    if (sat_solver_->CurrentDecisionLevel() == 0) {
      sat_solver_->AddBinaryClause(literal.Negated(), (--after_it)->second);
    } else {
      sat_solver_->AddBinaryClauseDuringSearch(literal.Negated(),
                                               (--after_it)->second);
    }
  }

  // Add the new entry.
  map_ref[i_lit.bound] = literal;
}


Literal IntegerEncoder::CreateAssociatedLiteral(IntegerLiteral i_lit) {
  CHECK(!LiteralIsAssociated(i_lit));
  ++num_created_variables_;
  const BooleanVariable new_var = sat_solver_->NewBooleanVariable();
  const Literal literal(new_var, true);
  AssociateGivenLiteral(i_lit, literal);
  return literal;
}

void IntegerEncoder::AssociateGivenLiteral(IntegerLiteral i_lit,
                                           Literal literal) {
  // TODO(user): convert it to a "domain compatible one".
  CHECK(!LiteralIsAssociated(i_lit));
  HalfAssociateGivenLiteral(i_lit, literal);
  HalfAssociateGivenLiteral(i_lit.Negated(), literal.Negated());
}

// TODO(user): The hard constraints we add between associated literals seems to
// work for optional variables, but I am not 100% sure why!! I think it works
// because these literals can only appear in a conflict if the presence literal
// of the optional variables is true.
void IntegerEncoder::HalfAssociateGivenLiteral(IntegerLiteral i_lit,
                                               Literal literal) {
  // Resize reverse encoding.
  const int new_size = 1 + literal.Index().value();
  if (new_size > reverse_encoding_.size()) reverse_encoding_.resize(new_size);

  // Associate the new literal to i_lit.
  if (!LiteralIsAssociated(i_lit)) {
    AddImplications(i_lit, literal);
    reverse_encoding_[literal.Index()].push_back(i_lit);
  } else {
    const Literal associated(GetAssociatedLiteral(i_lit));
    if (associated != literal) {
      sat_solver_->AddBinaryClause(literal, associated.Negated());
      sat_solver_->AddBinaryClause(literal.Negated(), associated);
    }
  }
}

bool IntegerEncoder::LiteralIsAssociated(IntegerLiteral i) const {
  if (i.var >= encoding_by_var_.size()) return false;
  const std::map<IntegerValue, Literal>& encoding =
      encoding_by_var_[IntegerVariable(i.var)];
  return encoding.find(i.bound) != encoding.end();
}

LiteralIndex IntegerEncoder::GetAssociatedLiteral(IntegerLiteral i) {
  if (i.var >= encoding_by_var_.size()) return kNoLiteralIndex;
  const std::map<IntegerValue, Literal>& encoding =
      encoding_by_var_[IntegerVariable(i.var)];
  const auto result = encoding.find(i.bound);
  if (result == encoding.end()) return kNoLiteralIndex;
  return result->second.Index();
}

Literal IntegerEncoder::GetOrCreateAssociatedLiteral(IntegerLiteral i_lit) {
  if (i_lit.var < encoding_by_var_.size()) {
    const std::map<IntegerValue, Literal>& encoding =
        encoding_by_var_[IntegerVariable(i_lit.var)];
    const auto it = encoding.find(i_lit.bound);
    if (it != encoding.end()) return it->second;
  }
  return CreateAssociatedLiteral(i_lit);
}

LiteralIndex IntegerEncoder::SearchForLiteralAtOrBefore(
    IntegerLiteral i) const {
  // We take the element before the upper_bound() which is either the encoding
  // of i if it already exists, or the encoding just before it.
  if (i.var >= encoding_by_var_.size()) return kNoLiteralIndex;
  const std::map<IntegerValue, Literal>& encoding =
      encoding_by_var_[IntegerVariable(i.var)];
  auto after_it = encoding.upper_bound(i.bound);
  if (after_it == encoding.begin()) return kNoLiteralIndex;
  --after_it;
  return after_it->second.Index();
}

bool IntegerTrail::Propagate(Trail* trail) {
  // Make sure that our internal "integer_decision_levels_" size matches the
  // sat decision levels. At the level zero, integer_decision_levels_ should
  // be empty.
  if (trail->CurrentDecisionLevel() > integer_decision_levels_.size()) {
    integer_decision_levels_.push_back(integer_trail_.size());
    CHECK_EQ(trail->CurrentDecisionLevel(), integer_decision_levels_.size());
  }

  // This is used to map any integer literal out of the initial variable domain
  // into one that use one of the domain value.
  var_to_current_lb_interval_index_.SetLevel(trail->CurrentDecisionLevel());

  // Process all the "associated" literals and Enqueue() the corresponding
  // bounds.
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    for (const IntegerLiteral i_lit : encoder_->GetIntegerLiterals(literal)) {
      if (IsCurrentlyIgnored(i_lit.Var())) continue;

      // The reason is simply the associated literal.
      if (!Enqueue(i_lit, {literal.Negated()}, {})) return false;
    }
  }

  return true;
}

void IntegerTrail::Untrail(const Trail& trail, int literal_trail_index) {
  var_to_current_lb_interval_index_.SetLevel(trail.CurrentDecisionLevel());
  propagation_trail_index_ =
      std::min(propagation_trail_index_, literal_trail_index);

  // Note that if a conflict was detected before Propagate() of this class was
  // even called, it is possible that there is nothing to backtrack.
  const int decision_level = trail.CurrentDecisionLevel();
  if (decision_level >= integer_decision_levels_.size()) return;
  const int target = integer_decision_levels_[decision_level];
  integer_decision_levels_.resize(decision_level);
  CHECK_GE(target, vars_.size());

  // This is needed for the code below to work.
  if (target == integer_trail_.size()) return;

  for (int index = integer_trail_.size() - 1; index >= target; --index) {
    const TrailEntry& entry = integer_trail_[index];
    if (entry.var < 0) continue;  // entry used by EnqueueLiteral().
    vars_[entry.var].current_trail_index = entry.prev_trail_index;
    vars_[entry.var].current_bound =
        integer_trail_[entry.prev_trail_index].bound;
  }

  // Resize vectors.
  literals_reason_buffer_.resize(
      integer_trail_[target].literals_reason_start_index);
  bounds_reason_buffer_.resize(integer_trail_[target].dependencies_start_index);
  integer_trail_.resize(target);
}

IntegerVariable IntegerTrail::AddIntegerVariable(IntegerValue lower_bound,
                                                 IntegerValue upper_bound) {
  CHECK_GE(lower_bound, kMinIntegerValue);
  CHECK_LE(lower_bound, kMaxIntegerValue);
  CHECK_GE(upper_bound, kMinIntegerValue);
  CHECK_LE(upper_bound, kMaxIntegerValue);
  CHECK(integer_decision_levels_.empty());
  CHECK_EQ(vars_.size(), integer_trail_.size());

  const IntegerVariable i(vars_.size());
  is_ignored_literals_.push_back(kNoLiteralIndex);
  vars_.push_back({lower_bound, static_cast<int>(integer_trail_.size())});
  integer_trail_.push_back({lower_bound, i.value()});

  // TODO(user): the is_ignored_literals_ Booleans are currently always the same
  // for a variable and its negation. So it may be better not to store it twice
  // so that we don't have to be careful when setting them.
  CHECK_EQ(NegationOf(i).value(), vars_.size());
  is_ignored_literals_.push_back(kNoLiteralIndex);
  vars_.push_back({-upper_bound, static_cast<int>(integer_trail_.size())});
  integer_trail_.push_back({-upper_bound, NegationOf(i).value()});

  for (SparseBitset<IntegerVariable>* w : watchers_) {
    w->Resize(NumIntegerVariables());
  }
  return i;
}

IntegerVariable IntegerTrail::AddIntegerVariable(
    const std::vector<ClosedInterval>& domain) {
  CHECK(!domain.empty());
  CHECK(IntervalsAreSortedAndDisjoint(domain));
  const IntegerVariable var = AddIntegerVariable(
      IntegerValue(domain.front().start), IntegerValue(domain.back().end));

  // We only stores the vector of closed intervals for "complex" domain.
  if (domain.size() > 1) {
    var_to_current_lb_interval_index_.Set(var, all_intervals_.size());
    for (const ClosedInterval interval : domain) {
      all_intervals_.push_back(interval);
    }
    InsertOrDie(&var_to_end_interval_index_, var, all_intervals_.size());

    // Copy for the negated variable.
    var_to_current_lb_interval_index_.Set(NegationOf(var),
                                          all_intervals_.size());
    for (const ClosedInterval interval : ::gtl::reversed_view(domain)) {
      all_intervals_.push_back({-interval.end, -interval.start});
    }
    InsertOrDie(&var_to_end_interval_index_, NegationOf(var),
                all_intervals_.size());
  }

  return var;
}

// TODO(user): we could return a reference, but this is likely not in any
// critical code path.
std::vector<ClosedInterval> IntegerTrail::InitialVariableDomain(
    IntegerVariable var) const {
  const int interval_index =
      FindWithDefault(var_to_current_lb_interval_index_, var, -1);
  if (interval_index >= 0) {
    return std::vector<ClosedInterval>(
        &all_intervals_[interval_index],
        &all_intervals_[FindOrDie(var_to_end_interval_index_, var)]);
  } else {
    std::vector<ClosedInterval> result;
    result.push_back({LevelZeroBound(var.value()).value(),
                      -LevelZeroBound(NegationOf(var).value()).value()});
    return result;
  }
}

bool IntegerTrail::UpdateInitialDomain(IntegerVariable var,
                                       std::vector<ClosedInterval> domain) {
  domain =
      IntersectionOfSortedDisjointIntervals(domain, InitialVariableDomain(var));
  if (domain.empty()) return false;

  // TODO(user): A bit inefficient as this recreate a vector for no reason.
  if (domain == InitialVariableDomain(var)) return true;

  CHECK(Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(domain.front().start)),
      {}, {}));
  CHECK(Enqueue(
      IntegerLiteral::LowerOrEqual(var, IntegerValue(domain.back().end)), {},
      {}));

  // TODO(user): reuse the memory if possible.
  if (ContainsKey(var_to_current_lb_interval_index_, var)) {
    var_to_current_lb_interval_index_.EraseOrDie(var);
    var_to_end_interval_index_.erase(var);
    var_to_current_lb_interval_index_.EraseOrDie(NegationOf(var));
    var_to_end_interval_index_.erase(NegationOf(var));
  }
  if (domain.size() > 1) {
    var_to_current_lb_interval_index_.Set(var, all_intervals_.size());
    for (const ClosedInterval interval : domain) {
      all_intervals_.push_back(interval);
    }
    InsertOrDie(&var_to_end_interval_index_, var, all_intervals_.size());

    // Copy for the negated variable.
    var_to_current_lb_interval_index_.Set(NegationOf(var),
                                          all_intervals_.size());
    for (const ClosedInterval interval : ::gtl::reversed_view(domain)) {
      all_intervals_.push_back({-interval.end, -interval.start});
    }
    InsertOrDie(&var_to_end_interval_index_, NegationOf(var),
                all_intervals_.size());
  }

  // If the variable is fully encoded, set to false excluded literals.
  if (encoder_->VariableIsFullyEncoded(var)) {
    int i = 0;
    int num_fixed = 0;
    const auto encoding = encoder_->FullDomainEncoding(var);
    for (const auto pair : encoding) {
      while (pair.value > domain[i].end && i < domain.size()) ++i;
      if (i == domain.size() || pair.value < domain[i].start) {
        // Set the literal to false;
        ++num_fixed;
        if (trail_->Assignment().LiteralIsTrue(pair.literal)) return false;
        if (!trail_->Assignment().LiteralIsFalse(pair.literal)) {
          trail_->EnqueueWithUnitReason(pair.literal.Negated());
        }
      }
    }
    if (num_fixed > 0) {
      VLOG(1) << "Domain intersection removed " << num_fixed << " values "
              << "(out of " << encoding.size() << ").";
    }
  }
  return true;
}

IntegerVariable IntegerTrail::GetOrCreateConstantIntegerVariable(
    IntegerValue value) {
  auto insert = constant_map_.insert(std::make_pair(value, kNoIntegerVariable));
  if (insert.second) {  // new element.
    const IntegerVariable new_var = AddIntegerVariable(value, value);
    insert.first->second = new_var;
    if (value != 0) InsertOrDie(&constant_map_, -value, NegationOf(new_var));
  }
  return insert.first->second;
}

int IntegerTrail::NumConstantVariables() const {
  // The +1 if for the special key zero (the only case when we have an odd
  // number of entries).
  return (constant_map_.size() + 1) / 2;
}

int IntegerTrail::FindLowestTrailIndexThatExplainBound(
    IntegerLiteral i_lit) const {
  DCHECK_LE(i_lit.bound, vars_[i_lit.var].current_bound);
  if (i_lit.bound <= LevelZeroBound(i_lit.var)) return -1;
  int trail_index = vars_[i_lit.var].current_trail_index;
  int prev_trail_index = trail_index;
  while (true) {
    const TrailEntry& entry = integer_trail_[trail_index];
    if (entry.bound == i_lit.bound) return trail_index;
    if (entry.bound < i_lit.bound) return prev_trail_index;
    prev_trail_index = trail_index;
    trail_index = entry.prev_trail_index;
  }
}

bool IntegerTrail::EnqueueAssociatedLiteral(
    Literal literal, IntegerLiteral i_lit,
    gtl::Span<Literal> literal_reason,
    gtl::Span<IntegerLiteral> integer_reason,
    BooleanVariable* variable_with_same_reason) {
  if (!trail_->Assignment().VariableIsAssigned(literal.Variable())) {
    if (integer_decision_levels_.empty()) {
      trail_->EnqueueWithUnitReason(literal);
      return true;
    }

    if (*variable_with_same_reason != kNoBooleanVariable) {
      trail_->EnqueueWithSameReasonAs(literal, *variable_with_same_reason);
      return true;
    }
    *variable_with_same_reason = literal.Variable();

    // Subtle: the reason is the same as i_lit, that we will enqueue if no
    // conflict occur at position integer_trail_.size(), so we just refer to
    // this index here. See EnqueueLiteral().
    const int trail_index = trail_->Index();
    if (trail_index >= boolean_trail_index_to_integer_one_.size()) {
      boolean_trail_index_to_integer_one_.resize(trail_index + 1);
    }
    boolean_trail_index_to_integer_one_[trail_index] = integer_trail_.size();
    trail_->Enqueue(literal, propagator_id_);
    return true;
  }
  if (trail_->Assignment().LiteralIsFalse(literal)) {
    std::vector<Literal>* conflict = trail_->MutableConflict();
    conflict->assign(literal_reason.begin(), literal_reason.end());

    // This is tricky, in some corner cases, the same Enqueue() will call
    // EnqueueAssociatedLiteral() on a literal and its opposite. In this case,
    // we don't want to have this in the conflict.
    const AssignmentInfo& info =
        trail_->Info(trail_->ReferenceVarWithSameReason(literal.Variable()));
    if (info.type != propagator_id_ ||
        info.trail_index >= boolean_trail_index_to_integer_one_.size() ||
        boolean_trail_index_to_integer_one_[info.trail_index] !=
            integer_trail_.size()) {
      conflict->push_back(literal);
    }
    MergeReasonInto(integer_reason, conflict);
    return false;
  }
  return true;
}

namespace {

std::string ReasonDebugString(gtl::Span<Literal> literal_reason,
                         gtl::Span<IntegerLiteral> integer_reason) {
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
                           gtl::Span<Literal> literal_reason,
                           gtl::Span<IntegerLiteral> integer_reason) {
  DCHECK(AllLiteralsAreFalse(literal_reason));
  CHECK(!IsCurrentlyIgnored(i_lit.Var()));

  // Nothing to do if the bound is not better than the current one.
  // TODO(user): Change this to a CHECK? propagator shouldn't try to push such
  // bound and waste time explaining it.
  if (i_lit.bound <= vars_[i_lit.var].current_bound) return true;
  ++num_enqueues_;

  // This may not indicate an incorectness, but just some propagators that
  // didn't reach a fixed-point at level zero.
  if (DEBUG_MODE && !integer_decision_levels_.empty()) {
    std::vector<Literal> l(literal_reason.begin(), literal_reason.end());
    MergeReasonInto(integer_reason, &l);
    LOG_IF(WARNING, l.empty())
        << "Propagating a literal with no reason at a positive level!\n"
        << "level:" << integer_decision_levels_.size() << " i_lit:" << i_lit
        << " " << ReasonDebugString(literal_reason, integer_reason) << "\n"
        << DebugString();
  }

  const IntegerVariable var(i_lit.var);

  // If the domain of var is not a single intervals and i_lit.bound fall into a
  // "hole", we increase it to the next possible value.
  {
    int interval_index =
        FindWithDefault(var_to_current_lb_interval_index_, var, -1);
    if (interval_index >= 0) {
      const int end_index = FindOrDie(var_to_end_interval_index_, var);
      while (interval_index < end_index &&
             i_lit.bound > all_intervals_[interval_index].end) {
        ++interval_index;
      }
      if (interval_index == end_index) {
        return ReportConflict(literal_reason, integer_reason);
      } else {
        var_to_current_lb_interval_index_.Set(var, interval_index);
        i_lit.bound = std::max(
            i_lit.bound, IntegerValue(all_intervals_[interval_index].start));
      }
    }
  }

  // For the EnqueueWithSameReasonAs() mechanism.
  BooleanVariable first_propagated_variable = kNoBooleanVariable;

  // Check if the integer variable has an empty domain.
  if (i_lit.bound > UpperBound(var)) {
    // We relax the upper bound as much as possible to still have a conflict.
    const auto ub_reason = IntegerLiteral::LowerOrEqual(var, i_lit.bound - 1);

    if (!IsOptional(var) || trail_->Assignment().LiteralIsFalse(
                                Literal(is_ignored_literals_[var]))) {
      std::vector<Literal>* conflict = trail_->MutableConflict();
      conflict->assign(literal_reason.begin(), literal_reason.end());
      if (IsOptional(var)) {
        conflict->push_back(Literal(is_ignored_literals_[var]));
      }

      // This is the same as:
      //   MergeReasonInto(integer_reason, conflict);
      //   MergeReasonInto({ub_reason)}, conflict);
      // but with just one call to MergeReasonIntoInternal() for speed. Note
      // that it may also produce a smaller reason overall.
      DCHECK(tmp_queue_.empty());
      const int size = vars_.size();
      for (const IntegerLiteral& literal : integer_reason) {
        const int trail_index = FindLowestTrailIndexThatExplainBound(literal);
        if (trail_index >= size) tmp_queue_.push_back(trail_index);
      }
      {
        const int trail_index = FindLowestTrailIndexThatExplainBound(ub_reason);
        if (trail_index >= size) tmp_queue_.push_back(trail_index);
      }
      MergeReasonIntoInternal(conflict);
      return false;
    } else {
      // Note(user): We never make the bound of an optional literal cross. We
      // used to have a bug where we propagated these bound and their associated
      // literal, and we where reaching a conflict while propagating the
      // associated literal instead of setting is_ignored below to false.
      const Literal is_ignored = Literal(is_ignored_literals_[var]);
      if (integer_decision_levels_.empty()) {
        trail_->EnqueueWithUnitReason(is_ignored);
      } else {
        EnqueueLiteral(is_ignored, literal_reason, integer_reason);
        bounds_reason_buffer_.push_back(ub_reason);
      }
      return true;
    }
  }

  // Notify the watchers.
  for (SparseBitset<IntegerVariable>* bitset : watchers_) {
    bitset->Set(IntegerVariable(i_lit.var));
  }

  // Enqueue the strongest associated Boolean literal implied by this one.
  const LiteralIndex literal_index =
      encoder_->SearchForLiteralAtOrBefore(i_lit);
  if (literal_index != kNoLiteralIndex) {
    if (!EnqueueAssociatedLiteral(Literal(literal_index), i_lit, literal_reason,
                                  integer_reason, &first_propagated_variable)) {
      return false;
    }
  }

  // Special case for level zero.
  if (integer_decision_levels_.empty()) {
    vars_[i_lit.var].current_bound = i_lit.bound;
    integer_trail_[i_lit.var].bound = i_lit.bound;
    return true;
  }

  integer_trail_.push_back(
      {/*bound=*/i_lit.bound,
       /*var=*/i_lit.var,
       /*prev_trail_index=*/vars_[i_lit.var].current_trail_index,
       /*literals_reason_start_index=*/
       static_cast<int32>(literals_reason_buffer_.size()),
       /*dependencies_start_index=*/
       static_cast<int32>(bounds_reason_buffer_.size())});
  vars_[i_lit.var].current_bound = i_lit.bound;
  vars_[i_lit.var].current_trail_index = integer_trail_.size() - 1;

  // Save the reason into our internal buffers.
  if (!literal_reason.empty()) {
    literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                   literal_reason.begin(),
                                   literal_reason.end());
  }
  if (!integer_reason.empty()) {
    CHECK_NE(integer_reason[0].var, kNoIntegerVariable);
    bounds_reason_buffer_.insert(bounds_reason_buffer_.end(),
                                 integer_reason.begin(), integer_reason.end());
  }
  return true;
}

BeginEndWrapper<std::vector<IntegerLiteral>::const_iterator>
IntegerTrail::Dependencies(int trail_index) const {
  const int start = integer_trail_[trail_index].dependencies_start_index;
  const int end = trail_index + 1 < integer_trail_.size()
                      ? integer_trail_[trail_index + 1].dependencies_start_index
                      : bounds_reason_buffer_.size();
  if (start < end && bounds_reason_buffer_[start].var >= 0) {
    // HACK. This is a critical code, so we reuse the IntegerLiteral.var to
    // store the result of FindLowestTrailIndexThatExplainBound() applied to all
    // the IntegerLiteral.
    //
    // To detect if we already did the computation, we store the negated index.
    // Note that we will redo the computation in the corner case where all the
    // given IntegerLiteral turn out to be assigned at level zero.
    //
    // TODO(user): We could check that the same IntegerVariable never appear
    // twice. And if it does the one with the lowest bound could be removed.
    int out = start;
    const int size = vars_.size();
    for (int i = start; i < end; ++i) {
      const int dep =
          FindLowestTrailIndexThatExplainBound(bounds_reason_buffer_[i]);
      if (dep >= size) bounds_reason_buffer_[out++].var = -dep;
    }
  }
  const std::vector<IntegerLiteral>::const_iterator b =
      bounds_reason_buffer_.begin() + start;
  const std::vector<IntegerLiteral>::const_iterator e =
      bounds_reason_buffer_.begin() + end;
  return BeginEndRange(b, e);
}

void IntegerTrail::AppendLiteralsReason(int trail_index,
                                        std::vector<Literal>* output) const {
  output->insert(
      output->end(),
      literals_reason_buffer_.begin() +
          integer_trail_[trail_index].literals_reason_start_index,
      trail_index + 1 < integer_trail_.size()
          ? literals_reason_buffer_.begin() +
                integer_trail_[trail_index + 1].literals_reason_start_index
          : literals_reason_buffer_.end());
}

std::vector<Literal> IntegerTrail::ReasonFor(IntegerLiteral literal) const {
  std::vector<Literal> reason;
  MergeReasonInto({literal}, &reason);
  return reason;
}

bool IntegerTrail::AllLiteralsAreFalse(
    gtl::Span<Literal> literals) const {
  for (const Literal lit : literals) {
    if (!trail_->Assignment().LiteralIsFalse(lit)) return false;
  }
  return true;
}

// TODO(user): If this is called many time on the same variables, it could be
// made faster by using some caching mecanism.
void IntegerTrail::MergeReasonInto(gtl::Span<IntegerLiteral> literals,
                                   std::vector<Literal>* output) const {
  DCHECK(tmp_queue_.empty());
  const int size = vars_.size();
  for (const IntegerLiteral& literal : literals) {
    const int trail_index = FindLowestTrailIndexThatExplainBound(literal);

    // Any indices lower than that means that there is no reason needed.
    // Note that it is important for size to be signed because of -1 indices.
    if (trail_index >= size) tmp_queue_.push_back(trail_index);
  }
  return MergeReasonIntoInternal(output);
}

// This will expand the reason of the IntegerLiteral already in tmp_queue_ until
// everything is explained in term of Literal.
void IntegerTrail::MergeReasonIntoInternal(std::vector<Literal>* output) const {
  // All relevant trail indices will be >= vars_.size(), so we can safely use
  // zero to means that no literal refering to this variable is in the queue.
  tmp_var_to_trail_index_in_queue_.resize(vars_.size(), 0);
  DCHECK(std::all_of(tmp_var_to_trail_index_in_queue_.begin(),
                     tmp_var_to_trail_index_in_queue_.end(),
                     [](int v) { return v == 0; }));

  // During the algorithm execution, all the queue entries that do not match the
  // content of tmp_var_to_trail_index_in_queue_[] will be ignored.
  for (const int trail_index : tmp_queue_) {
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

    // If this entry has an associated literal, then we use it as a reason
    // instead of the stored reason. If later this literal needs to be
    // explained, then the associated literal will be expanded with the stored
    // reason.
    {
      const LiteralIndex associated_lit =
          encoder_->GetAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
              IntegerVariable(entry.var), entry.bound));
      if (associated_lit != kNoLiteralIndex) {
        output->push_back(Literal(associated_lit).Negated());

        // Ignore any entries of the queue refering to this variable and make
        // sure no such entry are added later.
        tmp_to_clear_.push_back(entry.var);
        tmp_var_to_trail_index_in_queue_[entry.var] = kint32max;
        continue;
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
    AppendLiteralsReason(trail_index, output);
    tmp_var_to_trail_index_in_queue_[entry.var] = 0;

    // TODO(user): we could speed up Dependencies() by using the indices stored
    // in tmp_var_to_trail_index_in_queue_ instead of redoing
    // FindLowestTrailIndexThatExplainBound() from the latest trail index.
    bool has_dependency = false;
    for (const IntegerLiteral lit : Dependencies(trail_index)) {
      // Extract the next_trail_index from the returned literal, we can break
      // as soon as we get a negative next_trail_index. See the encoding in
      // Dependencies().
      const int next_trail_index = -lit.var;
      if (next_trail_index < 0) break;
      const TrailEntry& next_entry = integer_trail_[next_trail_index];
      has_dependency = true;

      // Only add literals that are not "implied" by the ones already present.
      // For instance, do not add (x >= 4) if we already have (x >= 7). This
      // translate into only adding a trail index if it is larger than the one
      // in the queue refering to the same variable.
      if (next_trail_index > tmp_var_to_trail_index_in_queue_[next_entry.var]) {
        tmp_var_to_trail_index_in_queue_[next_entry.var] = next_trail_index;
        tmp_queue_.push_back(next_trail_index);
        std::push_heap(tmp_queue_.begin(), tmp_queue_.end());
      }
    }

    // Special case for a "leaf", we will never need this variable again.
    if (!has_dependency) {
      tmp_to_clear_.push_back(entry.var);
      tmp_var_to_trail_index_in_queue_[entry.var] = kint32max;
    }
  }

  // clean-up.
  for (const int var : tmp_to_clear_) {
    tmp_var_to_trail_index_in_queue_[var] = 0;
  }
  STLSortAndRemoveDuplicates(output);
}

gtl::Span<Literal> IntegerTrail::Reason(const Trail& trail,
                                               int trail_index) const {
  std::vector<Literal>* reason = trail.GetVectorToStoreReason(trail_index);
  reason->clear();

  const int index = boolean_trail_index_to_integer_one_[trail_index];
  AppendLiteralsReason(index, reason);
  DCHECK(tmp_queue_.empty());
  for (const IntegerLiteral lit : Dependencies(index)) {
    const int next_trail_index = -lit.var;
    if (next_trail_index < 0) break;
    tmp_queue_.push_back(next_trail_index);
  }
  MergeReasonIntoInternal(reason);
  return *reason;
}

void IntegerTrail::EnqueueLiteral(
    Literal literal, gtl::Span<Literal> literal_reason,
    gtl::Span<IntegerLiteral> integer_reason) {
  DCHECK(AllLiteralsAreFalse(literal_reason));
  if (integer_decision_levels_.empty()) {
    // Level zero. We don't keep any reason.
    trail_->EnqueueWithUnitReason(literal);
    return;
  }

  // This may not indicate an incorectness, but just some propagators that
  // didn't reach a fixed-point at level zero.
  if (DEBUG_MODE && !integer_decision_levels_.empty()) {
    std::vector<Literal> l(literal_reason.begin(), literal_reason.end());
    MergeReasonInto(integer_reason, &l);
    LOG_IF(WARNING, l.empty())
        << "Propagating a literal with no reason at a positive level!\n"
        << "level:" << integer_decision_levels_.size() << " lit:" << literal
        << " " << ReasonDebugString(literal_reason, integer_reason) << "\n"
        << DebugString();
  }

  const int trail_index = trail_->Index();
  if (trail_index >= boolean_trail_index_to_integer_one_.size()) {
    boolean_trail_index_to_integer_one_.resize(trail_index + 1);
  }
  boolean_trail_index_to_integer_one_[trail_index] = integer_trail_.size();
  integer_trail_.push_back({/*bound=*/IntegerValue(0), /*var=*/-1,
                            /*prev_trail_index=*/-1,
                            /*literals_reason_start_index=*/
                            static_cast<int32>(literals_reason_buffer_.size()),
                            /*dependencies_start_index=*/
                            static_cast<int32>(bounds_reason_buffer_.size())});
  literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                 literal_reason.begin(), literal_reason.end());
  bounds_reason_buffer_.insert(bounds_reason_buffer_.end(),
                               integer_reason.begin(), integer_reason.end());
  trail_->Enqueue(literal, propagator_id_);
}

GenericLiteralWatcher::GenericLiteralWatcher(
    IntegerTrail* integer_trail, RevRepository<int>* rev_int_repository)
    : SatPropagator("GenericLiteralWatcher"),
      integer_trail_(integer_trail),
      rev_int_repository_(rev_int_repository) {
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
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
}

bool GenericLiteralWatcher::Propagate(Trail* trail) {
  const int level = trail->CurrentDecisionLevel();
  rev_int_repository_->SetLevel(level);
  UpdateCallingNeeds(trail);

  // Note that the priority may be set to -1 inside the loop in order to restart
  // at zero.
  for (int priority = 0; priority < queue_by_priority_.size(); ++priority) {
    std::deque<int>& queue = queue_by_priority_[priority];
    while (!queue.empty()) {
      const int id = queue.front();
      queue.pop_front();

      // Before we propagate, make sure any reversible structure are up to date.
      // Note that we never do anything expensive more than once per level.
      {
        const int low = id_to_greatest_common_level_since_last_call_[id];
        const int high = id_to_level_at_last_call_[id];
        if (low < high || level > low) {  // Equivalent to not all equal.
          id_to_level_at_last_call_[id] = level;
          id_to_greatest_common_level_since_last_call_[id] = level;
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
      const int64 old_integer_timestamp = integer_trail_->num_enqueues();
      const int64 old_boolean_timestamp = trail->Index();

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

      // If the propagator pushed an integer bound, we revert to priority = 0.
      if (integer_trail_->num_enqueues() > old_integer_timestamp) {
        priority = -1;  // Because of the ++priority in the for loop.
      }

      // If the propagator pushed a literal, we have two options.
      if (trail->Index() > old_boolean_timestamp) {
        // Important: for now we need to re-run the clauses propagator each time
        // we push a new literal because some propagator like the arc consistent
        // all diff relies on this.
        //
        // However, on some problem, it seems to work better to not do that. One
        // possible reason is that the reason of a "natural" propagation might
        // be better than one we learned.
        const bool run_sat_propagators_at_higher_priority = true;
        if (run_sat_propagators_at_higher_priority) {
          // We exit in order to rerun all SAT only propagators first. Note that
          // since a literal was pushed we are guaranteed to be called again,
          // and we will resume from priority 0.
          return true;
        } else {
          priority = -1;
        }
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

  const int level = trail.CurrentDecisionLevel();
  rev_int_repository_->SetLevel(level);
  for (int& ref : id_to_greatest_common_level_since_last_call_) {
    ref = std::min(ref, level);
  }
}

// Registers a propagator and returns its unique ids.
int GenericLiteralWatcher::Register(PropagatorInterface* propagator) {
  const int id = watchers_.size();
  watchers_.push_back(propagator);
  id_to_level_at_last_call_.push_back(0);
  id_to_greatest_common_level_since_last_call_.push_back(0);
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

void GenericLiteralWatcher::RegisterReversibleClass(int id,
                                                    ReversibleInterface* rev) {
  id_to_reversible_classes_[id].push_back(rev);
}

void GenericLiteralWatcher::RegisterReversibleInt(int id, int* rev) {
  id_to_reversible_ints_[id].push_back(rev);
}

// TODO(user): the complexity of this heuristic and the one below is ok when
// use_fixed_search() is false because it is not executed often, however, we do
// a linear scan for each search decision when use_fixed_search() is true, which
// seems bad. Improve.
std::function<LiteralIndex()> FirstUnassignedVarAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model) {
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  return [integer_encoder, integer_trail, /*copy*/ vars]() {
    for (const IntegerVariable var : vars) {
      // Note that there is no point trying to fix a currently ignored variable.
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail->LowerBound(var);
      if (lb < integer_trail->UpperBound(var)) {
        return integer_encoder
            ->GetOrCreateAssociatedLiteral(
                IntegerLiteral::LowerOrEqual(var, lb))
            .Index();
      }
    }
    return LiteralIndex(kNoLiteralIndex);
  };
}

std::function<LiteralIndex()> UnassignedVarWithLowestMinAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model) {
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  return [integer_encoder, integer_trail, /*copy */ vars]() {
    IntegerVariable candidate = kNoIntegerVariable;
    IntegerValue candidate_lb;
    for (const IntegerVariable var : vars) {
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail->LowerBound(var);
      if (lb < integer_trail->UpperBound(var) &&
          (candidate == kNoIntegerVariable || lb < candidate_lb)) {
        candidate = var;
        candidate_lb = lb;
      }
    }
    // Fix compilation error on visual studio 2013.
    if (candidate == kNoIntegerVariable) return LiteralIndex(kNoLiteralIndex);
    return integer_encoder
        ->GetOrCreateAssociatedLiteral(
            IntegerLiteral::LowerOrEqual(candidate, candidate_lb))
        .Index();
  };
}

SatSolver::Status SolveIntegerProblemWithLazyEncoding(
    const std::vector<Literal>& assumptions,
    const std::function<LiteralIndex()>& next_decision, Model* model) {
  // TODO(user): Improve the time-limit situation, especially when
  // use_fixed_search() is true where the deterministic time is not updated.
  TimeLimit* time_limit = model->Mutable<TimeLimit>();
  if (time_limit == nullptr) {
    model->SetSingleton(TimeLimit::Infinite());
    time_limit = model->Mutable<TimeLimit>();
  }
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  if (solver->IsModelUnsat()) return SatSolver::MODEL_UNSAT;
  solver->Backtrack(0);
  solver->SetAssumptionLevel(0);

  const SatParameters parameters = solver->parameters();
  if (parameters.use_fixed_search()) {
    CHECK(assumptions.empty()) << "Not supported yet";
    // Note that it is important to do the level-zero propagation if it wasn't
    // already done because EnqueueDecisionAndBackjumpOnConflict() assumes that
    // the solver is in a "propagated" state.
    if (!solver->Propagate()) return SatSolver::MODEL_UNSAT;
  }
  while (!time_limit->LimitReached()) {
    // If we are not in fixed search, let the SAT solver do a full search to
    // instanciate all the already created Booleans.
    if (!parameters.use_fixed_search()) {
      if (assumptions.empty()) {
        const SatSolver::Status status = solver->SolveWithTimeLimit(time_limit);
        if (status != SatSolver::MODEL_SAT) return status;
      } else {
        // TODO(user): We actually don't want to reset the solver from one
        // loop to the next as it is less efficient.
        const SatSolver::Status status =
            solver->ResetAndSolveWithGivenAssumptions(assumptions, time_limit);
        if (status != SatSolver::MODEL_SAT) return status;
      }
    }

    // Look for the "best" non-instanciated integer variable.
    // If all variables are instanciated, we have a solution.
    const LiteralIndex decision =
        next_decision == nullptr ? kNoLiteralIndex : next_decision();
    if (decision == kNoLiteralIndex) return SatSolver::MODEL_SAT;

    // Always try to Enqueue this literal as the next decision so we bypass
    // any default polarity heuristic the solver may use on this literal.
    solver->EnqueueDecisionAndBackjumpOnConflict(Literal(decision));
    if (solver->IsModelUnsat()) return SatSolver::MODEL_UNSAT;
  }
  return SatSolver::Status::LIMIT_REACHED;
}

// Shortcut when there is no assumption, and we consider all variables in
// order for the search decision.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model) {
  const IntegerVariable num_vars =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables();
  std::vector<IntegerVariable> all_variables;
  for (IntegerVariable var(0); var < num_vars; ++var) {
    all_variables.push_back(var);
  }
  return SolveIntegerProblemWithLazyEncoding(
      {}, FirstUnassignedVarAtItsMinHeuristic(all_variables, model), model);
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
        if (integer_trail->IsCurrentlyIgnored(bound.Var())) {
          // In this case we replace the decision (which is a bound on an
          // ignored variable) with the fact that the integer variable was
          // ignored. This works because the only impact a bound of an ignored
          // variable can have on the rest of the model is through the
          // is_ignored literal.
          clause_to_exclude_solution.push_back(
              integer_trail->IsIgnoredLiteral(bound.Var()).Negated());
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
