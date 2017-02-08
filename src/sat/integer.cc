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

#include "sat/integer.h"

//#include "base/iterator_adaptors.h"
#include "base/stl_util.h"

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

void IntegerEncoder::FullyEncodeVariable(IntegerVariable i_var,
                                         std::vector<IntegerValue> values) {
  CHECK_EQ(0, sat_solver_->CurrentDecisionLevel());
  CHECK(!values.empty());  // UNSAT problem. We don't deal with that here.

  STLSortAndRemoveDuplicates(&values);

  // TODO(user): This case is annoying, not sure yet how to best fix the
  // variable. There is certainly no need to create a Boolean variable, but
  // one needs to talk to IntegerTrail to fix the variable and we don't want
  // the encoder to depend on this. So for now we fail here and it is up to
  // the caller to deal with this case.
  CHECK_NE(values.size(), 1);

  // If the variable has already been fully encoded, we set to false the
  // literals that cannot be true anymore. We also log a warning because ideally
  // these intersection should happen in the presolve.
  if (ContainsKey(full_encoding_index_, i_var)) {
    int num_fixed = 0;
    hash_set<IntegerValue> to_interset(values.begin(), values.end());
    const std::vector<ValueLiteralPair>& encoding = FullDomainEncoding(i_var);
    for (const ValueLiteralPair& p : encoding) {
      if (!ContainsKey(to_interset, p.value)) {
        // TODO(user): also remove this entry from encoding.
        ++num_fixed;
        sat_solver_->AddUnitClause(p.literal.Negated());
      }
    }
    if (num_fixed > 0) {
      LOG(WARNING) << "Domain intersection removed " << num_fixed << " values "
                   << "(out of " << encoding.size() << ").";
    }
    return;
  }

  std::vector<ValueLiteralPair> encoding;
  if (values.size() == 2) {
    const BooleanVariable var = sat_solver_->NewBooleanVariable();
    encoding.push_back({values[0], Literal(var, true)});
    encoding.push_back({values[1], Literal(var, false)});
  } else {
    std::vector<sat::LiteralWithCoeff> cst;
    for (const IntegerValue value : values) {
      const BooleanVariable var = sat_solver_->NewBooleanVariable();
      encoding.push_back({value, Literal(var, true)});
      cst.push_back(LiteralWithCoeff(Literal(var, true), Coefficient(1)));
    }
    CHECK(sat_solver_->AddLinearConstraint(true, sat::Coefficient(1), true,
                                           sat::Coefficient(1), &cst));
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

void IntegerEncoder::FullyEncodeVariable(IntegerVariable i_var, IntegerValue lb,
                                         IntegerValue ub) {
  // TODO(user): optimize the code if it ever become needed.
  CHECK_LE(ub - lb, 10000) << "Large domain for full encoding! investigate.";
  std::vector<IntegerValue> values;
  for (IntegerValue value = lb; value <= ub; ++value) values.push_back(value);
  return FullyEncodeVariable(i_var, std::move(values));
}

void IntegerEncoder::FullyEncodeVariableUsingGivenLiterals(
    IntegerVariable i_var, const std::vector<Literal>& literals,
    const std::vector<IntegerValue>& values) {
  CHECK(!VariableIsFullyEncoded(i_var));

  // Sort the literal.
  std::vector<ValueLiteralPair> encoding;
  std::vector<sat::LiteralWithCoeff> cst;
  for (int i = 0; i < values.size(); ++i) {
    const Literal literal = literals[i];
    const IntegerValue value = values[i];
    encoding.push_back({value, literal});
    cst.push_back(LiteralWithCoeff(literal, Coefficient(1)));
  }
  std::sort(encoding.begin(), encoding.end());

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

void IntegerEncoder::AssociateGivenLiteral(IntegerLiteral i_lit,
                                           Literal literal) {
  // Resize reverse encoding.
  const int new_size =
      1 + std::max(literal.Index(), literal.NegatedIndex()).value();
  if (new_size > reverse_encoding_.size()) reverse_encoding_.resize(new_size);

  // Associate the new literal to i_lit.
  AddImplications(i_lit, literal);
  reverse_encoding_[literal.Index()].push_back(i_lit);

  // Add its negation and associated it with i_lit.Negated().
  //
  // TODO(user): This seems to work for optional variables, but I am not
  // 100% sure why!! I think it works because these literals can only appear
  // in a conflict if the presence literal of the optional variables is true.
  AddImplications(i_lit.Negated(), literal.Negated());
  reverse_encoding_[literal.NegatedIndex()].push_back(i_lit.Negated());
}

Literal IntegerEncoder::CreateAssociatedLiteral(IntegerLiteral i_lit) {
  ++num_created_variables_;
  const BooleanVariable new_var = sat_solver_->NewBooleanVariable();
  const Literal literal(new_var, true);
  AssociateGivenLiteral(i_lit, literal);
  return literal;
}

bool IntegerEncoder::LiteralIsAssociated(IntegerLiteral i) const {
  if (i.var >= encoding_by_var_.size()) return false;
  const std::map<IntegerValue, Literal>& encoding =
      encoding_by_var_[IntegerVariable(i.var)];
  return encoding.find(i.bound) != encoding.end();
}

Literal IntegerEncoder::GetOrCreateAssociatedLiteral(IntegerLiteral i) {
  if (i.var < encoding_by_var_.size()) {
    const std::map<IntegerValue, Literal>& encoding =
        encoding_by_var_[IntegerVariable(i.var)];
    const auto it = encoding.find(i.bound);
    if (it != encoding.end()) return it->second;
  }
  return CreateAssociatedLiteral(i);
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

  // Value encoder.
  //
  // TODO(user): There is no need to maintain the bounds of such variable if
  // they are never used in any constraint!
  //
  // Algorithm:
  //  1/ See if new variables are fully encoded and initialize them.
  //  2/ In the loop below, each time a "min" variable was assigned to false,
  //     update the associated variable bounds, and change the watched "min".
  //     This step is is O(num variables at false between the old and new min).
  //
  // The data structure are reversible.
  watched_min_.SetLevel(trail->CurrentDecisionLevel());
  current_min_.SetLevel(trail->CurrentDecisionLevel());
  var_to_current_lb_interval_index_.SetLevel(trail->CurrentDecisionLevel());
  if (encoder_->GetFullyEncodedVariables().size() != num_encoded_variables_) {
    num_encoded_variables_ = encoder_->GetFullyEncodedVariables().size();

    // for now this is only supported at level zero. Otherwise we need to
    // inspect the trail to properly compute all the min.
    //
    // TODO(user): Don't rescan all the variables from scratch, we could only
    // scan the new ones. But then we need a mecanism to detect the new ones.
    CHECK_EQ(trail->CurrentDecisionLevel(), 0);
    for (const auto& entry : encoder_->GetFullyEncodedVariables()) {
      IntegerVariable var = entry.first;

      // This variable was already added and will be processed below.
      // Note that this is important, otherwise we may call many times
      // watched_min_.Add() on the same literal.
      if (ContainsKey(current_min_, var)) continue;

      const auto& encoding = encoder_->FullDomainEncoding(var);
      for (int i = 0; i < encoding.size(); ++i) {
        if (!trail_->Assignment().LiteralIsFalse(encoding[i].literal)) {
          if (!trail_->Assignment().LiteralIsTrue(encoding[i].literal)) {
            watched_min_.Add(encoding[i].literal.NegatedIndex(), var);
          }
          current_min_.Set(var, i);

          // No reason because we are at level zero.
          if (!Enqueue(IntegerLiteral::GreaterOrEqual(var, encoding[i].value),
                       {}, {})) {
            return false;
          }
          break;
        }
      }
    }
  }

  // Process all the "associated" literals and Enqueue() the corresponding
  // bounds.
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];

    // Bound encoder.
    for (const IntegerLiteral i_lit : encoder_->GetIntegerLiterals(literal)) {
      // The reason is simply the associated literal.
      if (!Enqueue(i_lit, {literal.Negated()}, {})) return false;
    }

    // Value encoder.
    for (IntegerVariable var : watched_min_.Values(literal.Index())) {
      // A watched min value just became false. Note that because current_min_
      // is also updated by Enqueue(), it may be larger than the watched min.
      const int min = current_min_.FindOrDie(var);
      int i = min;
      const auto& encoding = encoder_->FullDomainEncoding(var);
      tmp_literals_reason_.clear();
      for (; i < encoding.size(); ++i) {
        if (!trail_->Assignment().LiteralIsFalse(encoding[i].literal)) break;
        tmp_literals_reason_.push_back(encoding[i].literal);
      }

      // Note(user): we enforce a "== 1" on the encoding literals, but the
      // clause forcing at least one of them to be true may not have propagated
      // in some cases (because we loop in the integer propagators before
      // calling the clause propagator again).
      if (i == encoding.size()) {
        return ReportConflict(tmp_literals_reason_, {LowerBoundAsLiteral(var)});
      }

      // Note that we don't need to delete the old watched min:
      // - its literal has been assigned, so it will not be queried again until
      //   backtrack.
      // - When backtracked over, it will already be set correctly. It seems
      //   less efficient to delete it now and add it back on backtrack.
      watched_min_.Add(encoding[i].literal.NegatedIndex(), var);
      if (i > min) {
        // Note that we also need the fact that all smaller value are false
        // for the propagation. We use the current lower bound for that.
        current_min_.Set(var, i);
        if (!Enqueue(IntegerLiteral::GreaterOrEqual(var, encoding[i].value),
                     tmp_literals_reason_, {LowerBoundAsLiteral(var)})) {
          return false;
        }
      }
    }
  }

  return true;
}

void IntegerTrail::Untrail(const Trail& trail, int literal_trail_index) {
  watched_min_.SetLevel(trail.CurrentDecisionLevel());
  current_min_.SetLevel(trail.CurrentDecisionLevel());
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
  is_empty_literals_.push_back(kNoLiteralIndex);
  vars_.push_back({lower_bound, static_cast<int>(integer_trail_.size())});
  integer_trail_.push_back({lower_bound, i.value()});

  CHECK_EQ(NegationOf(i).value(), vars_.size());
  is_empty_literals_.push_back(kNoLiteralIndex);
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
    for (int i = domain.size() - 1; i >= 0; --i) {
      const ClosedInterval interval = domain[i];
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
    const std::vector<Literal>& literal_reason,
    const std::vector<IntegerLiteral>& integer_reason,
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
    *conflict = literal_reason;

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

std::string ReasonDebugString(const std::vector<Literal>& literal_reason,
                         const std::vector<IntegerLiteral>& integer_reason) {
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
                           const std::vector<Literal>& literal_reason,
                           const std::vector<IntegerLiteral>& integer_reason) {
  DCHECK(AllLiteralsAreFalse(literal_reason));

  // Nothing to do if the bound is not better than the current one.
  if (i_lit.bound <= vars_[i_lit.var].current_bound) return true;
  ++num_enqueues_;

  // This may not indicate an incorectness, but just some propagators that
  // didn't reach a fixed-point at level zero.
  if (DEBUG_MODE && !integer_decision_levels_.empty()) {
    std::vector<Literal> l = literal_reason;
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

  // Deal with fully encoded variable. We want to do that first because this may
  // make the IntegerLiteral bound stronger.
  const int min_index = FindWithDefault(current_min_, var, -1);
  if (min_index >= 0) {
    // Recover the current min, and propagate to false all the values that
    // are in [min, i_lit.value). All these literals have the same reason, so
    // we use the "same reason as" mecanism.
    //
    // TODO(user): We could go even further if the next literals are set to
    // false (but they need to be added to the reason).
    const auto& encoding = encoder_->FullDomainEncoding(var);
    if (i_lit.bound > encoding[min_index].value) {
      int i = min_index;
      for (; i < encoding.size(); ++i) {
        if (i_lit.bound <= encoding[i].value) break;
        const Literal literal = encoding[i].literal.Negated();
        if (!EnqueueAssociatedLiteral(literal, i_lit, literal_reason,
                                      integer_reason,
                                      &first_propagated_variable)) {
          return false;
        }
      }

      if (i == encoding.size()) {
        // Conflict: no possible values left.
        return ReportConflict(literal_reason, integer_reason);
      } else {
        // We have a new min. Note that watched_min_ will be updated on the next
        // call to Propagate() since we just pushed the watched literal if it
        // wasn't already set to false.
        current_min_.Set(var, i);

        // Adjust the bound of i_lit !
        CHECK_GE(encoding[i].value, i_lit.bound);
        i_lit.bound = encoding[i].value.value();
      }
    }
  }

  // Check if the integer variable has an empty domain.
  bool propagate_is_empty = false;
  if (i_lit.bound > UpperBound(var)) {
    if (!IsOptional(var) ||
        trail_->Assignment().LiteralIsFalse(Literal(is_empty_literals_[var]))) {
      std::vector<Literal>* conflict = trail_->MutableConflict();
      *conflict = literal_reason;
      if (IsOptional(var)) {
        conflict->push_back(Literal(is_empty_literals_[var]));
      }

      // This is the same as:
      //   MergeReasonInto(integer_reason, conflict);
      //   MergeReasonInto({UpperBoundAsLiteral(var)}, conflict);
      // but with just one call to MergeReasonIntoInternal() for speed. Note
      // that it may also produce a smaller reason overall.
      DCHECK(tmp_queue_.empty());
      const int size = vars_.size();
      for (const IntegerLiteral& literal : integer_reason) {
        const int trail_index = FindLowestTrailIndexThatExplainBound(literal);
        if (trail_index >= size) tmp_queue_.push_back(trail_index);
      }
      {
        // Add the reason for UpperBoundAsLiteral(var).
        const int trail_index =
            vars_[NegationOf(var).value()].current_trail_index;
        if (trail_index >= size) tmp_queue_.push_back(trail_index);
      }
      MergeReasonIntoInternal(conflict);
      return false;
    } else {
      const Literal is_empty = Literal(is_empty_literals_[var]);
      if (!trail_->Assignment().LiteralIsTrue(is_empty)) {
        // We delay the propagation for some subtle reasons, see below.
        propagate_is_empty = true;
      }
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
    if (propagate_is_empty) {
      const Literal is_empty = Literal(is_empty_literals_[var]);
      trail_->EnqueueWithUnitReason(is_empty);
    }
    vars_[i_lit.var].current_bound = i_lit.bound;
    integer_trail_[i_lit.var].bound = i_lit.bound;
    return true;
  }

  integer_trail_.push_back(
      {/*bound=*/i_lit.bound,
       /*var=*/i_lit.var,
       /*prev_trail_index=*/vars_[i_lit.var].current_trail_index,
       /*literals_reason_start_index=*/static_cast<int32>(
           literals_reason_buffer_.size()),
       /*dependencies_start_index=*/static_cast<int32>(
           bounds_reason_buffer_.size())});
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

  // Subtle: We do that after the IntegerLiteral as been enqueued for 2 reasons:
  // 1/ Any associated literal will use the TrailEntry above as a reason.
  // 2/ The precendence propagator currently rely on this.
  //    TODO(user): The point 2 is brittle, try to clean it up.
  if (propagate_is_empty) {
    const Literal is_empty = Literal(is_empty_literals_[var]);
    EnqueueLiteral(is_empty, literal_reason, integer_reason);
    bounds_reason_buffer_.push_back(UpperBoundAsLiteral(var));
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
    const std::vector<Literal>& literals) const {
  for (const Literal lit : literals) {
    if (!trail_->Assignment().LiteralIsFalse(lit)) return false;
  }
  return true;
}

// TODO(user): If this is called many time on the same variables, it could be
// made faster by using some caching mecanism.
void IntegerTrail::MergeReasonInto(const std::vector<IntegerLiteral>& literals,
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

void IntegerTrail::MergeReasonIntoInternal(std::vector<Literal>* output) const {
  tmp_trail_indices_.clear();
  tmp_var_to_highest_explained_trail_index_.resize(vars_.size(), 0);
  DCHECK(std::all_of(tmp_var_to_highest_explained_trail_index_.begin(),
                     tmp_var_to_highest_explained_trail_index_.end(),
                     [](int v) { return v == 0; }));

  // This implement an iterative DFS on a DAG. Each time a node from the
  // tmp_queue_ is expanded, we change its sign, so that when we go back
  // (equivalent to the return of the recursive call), we can detect that this
  // node was already expanded.
  //
  // To detect nodes from which we already performed the full DFS exploration,
  // we use tmp_var_to_highest_explained_trail_index_.
  //
  // TODO(user): The order in which each trail_index is expanded will change
  // how much of the reason is "minimized". Investigate if some order are better
  // than other.
  while (!tmp_queue_.empty()) {
    const bool already_expored = tmp_queue_.back() < 0;
    const int trail_index = std::abs(tmp_queue_.back());
    const TrailEntry& entry = integer_trail_[trail_index];

    // Since we already have an explanation for a larger bound (ex: x>=4) we
    // don't need to add the explanation for a lower one (ex: x>=2).
    if (trail_index <= tmp_var_to_highest_explained_trail_index_[entry.var]) {
      tmp_queue_.pop_back();
      continue;
    }

    DCHECK_GT(trail_index, 0);
    if (already_expored) {
      // We are in the "return" of the DFS recursive call.
      DCHECK_GT(trail_index,
                tmp_var_to_highest_explained_trail_index_[entry.var]);
      tmp_var_to_highest_explained_trail_index_[entry.var] = trail_index;
      tmp_trail_indices_.push_back(trail_index);
      tmp_queue_.pop_back();
    } else {
      // We make "recursive calls" from this node.
      tmp_queue_.back() = -trail_index;
      for (const IntegerLiteral lit : Dependencies(trail_index)) {
        // Extract the next_trail_index from the returned literal, we can break
        // as soon as we get a negative next_trail_index. See the encoding in
        // Dependencies().
        const int next_trail_index = -lit.var;
        if (next_trail_index < 0) break;
        const TrailEntry& next_entry = integer_trail_[next_trail_index];
        if (next_trail_index >
            tmp_var_to_highest_explained_trail_index_[next_entry.var]) {
          tmp_queue_.push_back(next_trail_index);
        }
      }
    }
  }

  // Cleanup + output the reason.
  for (const int trail_index : tmp_trail_indices_) {
    const TrailEntry& entry = integer_trail_[trail_index];
    tmp_var_to_highest_explained_trail_index_[entry.var] = 0;
    AppendLiteralsReason(trail_index, output);
  }
  STLSortAndRemoveDuplicates(output);
}

ClauseRef IntegerTrail::Reason(const Trail& trail, int trail_index) const {
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

  if (DEBUG_MODE && trail.CurrentDecisionLevel() > 0) CHECK(!reason->empty());
  return ClauseRef(*reason);
}

void IntegerTrail::EnqueueLiteral(
    Literal literal, const std::vector<Literal>& literal_reason,
    const std::vector<IntegerLiteral>& integer_reason) {
  DCHECK(AllLiteralsAreFalse(literal_reason));
  if (integer_decision_levels_.empty()) {
    // Level zero. We don't keep any reason.
    trail_->EnqueueWithUnitReason(literal);
    return;
  }

  const int trail_index = trail_->Index();
  if (trail_index >= boolean_trail_index_to_integer_one_.size()) {
    boolean_trail_index_to_integer_one_.resize(trail_index + 1);
  }
  boolean_trail_index_to_integer_one_[trail_index] = integer_trail_.size();
  integer_trail_.push_back({/*bound=*/IntegerValue(0), /*var=*/-1,
                            /*prev_trail_index=*/-1,
                            /*literals_reason_start_index=*/static_cast<int32>(
                                literals_reason_buffer_.size()),
                            /*dependencies_start_index=*/static_cast<int32>(
                                bounds_reason_buffer_.size())});
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
      //
      // TODO(user): expose the parameter. Early experiments are counter
      // intuitive and seems to indicate that it is better not to re-run the SAT
      // propagators each time we push a literal.
      if (trail->Index() > old_boolean_timestamp) {
        const bool run_sat_propagators_at_higher_priority = false;
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

SatSolver::Status SolveIntegerProblemWithLazyEncoding(
    const std::vector<Literal>& assumptions,
    const std::vector<IntegerVariable>& variables_to_finalize, Model* model) {
  TimeLimit* time_limit = model->Mutable<TimeLimit>();
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  SatSolver::Status status = SatSolver::Status::MODEL_UNSAT;
  for (;;) {
    if (assumptions.empty()) {
      // TODO(user): This one doesn't do Backtrack(0), and doing it seems to
      // trigger a bug in research/devtools/compote/scheduler
      // :instruction_scheduler_test, investigate.
      status = solver->SolveWithTimeLimit(time_limit);
    } else {
      status =
          solver->ResetAndSolveWithGivenAssumptions(assumptions, time_limit);
    }
    if (status != SatSolver::MODEL_SAT) break;

    // Look for an integer variable whose domain is not singleton.
    //
    // Heuristic: we take the first non-fixed variable in the given order and
    // try to fix it to its lower bound.
    //
    // TODO(user): consider better heuristics. Maybe we can use some kind of
    // IntegerVariable activity or something from the CP world. We also tried
    // to take the one with minimum lb amongst the given variables, which work
    // well on some problem but not on other.
    IntegerVariable candidate = kNoIntegerVariable;
    for (const IntegerVariable variable : variables_to_finalize) {
      // Note that we use < and not != for optional variable whose domain may
      // be empty.
      const IntegerValue lb = integer_trail->LowerBound(variable);
      if (lb < integer_trail->UpperBound(variable)) {
        candidate = variable;
        break;
      }
    }

    if (candidate == kNoIntegerVariable) break;

    // This add a literal with good polarity. It is very important that the
    // decision heuristic assign it to false! Otherwise, our heuristic is not
    // good.
    integer_encoder->CreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
        candidate, integer_trail->LowerBound(candidate) + 1));
  }
  return status;
}

}  // namespace sat
}  // namespace operations_research
