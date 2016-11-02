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

#include "base/stl_util.h"

namespace operations_research {
namespace sat {

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
      LOG(WARNING) << "Domain intersection removed " << num_fixed << " values.";
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
    sat_solver_->AddBinaryClauseDuringSearch(after_it->second.Negated(),
                                             literal);
  }
  if (after_it != map_ref.begin()) {
    // literal => Literal(before)
    sat_solver_->AddBinaryClauseDuringSearch(literal.Negated(),
                                             (--after_it)->second);
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
      const auto& encoding = encoder_->FullDomainEncoding(var);
      for (int i = 0; i < encoding.size(); ++i) {
        if (!trail_->Assignment().LiteralIsFalse(encoding[i].literal)) {
          watched_min_.Set(encoding[i].literal.NegatedIndex(), {var, i});
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
    if (watched_min_.ContainsKey(literal.Index())) {
      // A watched min value just became false.
      const auto pair = watched_min_.FindOrDie(literal.Index());
      const IntegerVariable var = pair.first;
      const int min = pair.second;
      const auto& encoding = encoder_->FullDomainEncoding(var);
      std::vector<Literal> literal_reason = {literal.Negated()};
      for (int i = min + 1; i < encoding.size(); ++i) {
        if (!trail_->Assignment().LiteralIsFalse(encoding[i].literal)) {
          watched_min_.EraseOrDie(literal.Index());
          watched_min_.Set(encoding[i].literal.NegatedIndex(), {var, i});
          current_min_.Set(var, i);

          // Note that we also need the fact that all smaller value are false
          // for the propagation. We use the current lower bound for that.
          if (!Enqueue(IntegerLiteral::GreaterOrEqual(var, encoding[i].value),
                       literal_reason, {LowerBoundAsLiteral(var)})) {
            return false;
          }
          break;
        } else {
          literal_reason.push_back(encoding[i].literal);
        }
      }
    }
  }

  return true;
}

void IntegerTrail::Untrail(const Trail& trail, int literal_trail_index) {
  watched_min_.SetLevel(trail.CurrentDecisionLevel());
  current_min_.SetLevel(trail.CurrentDecisionLevel());
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
    const std::vector<Literal>& literals_reason,
    const std::vector<IntegerLiteral>& bounds_reason,
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
    *conflict = literals_reason;

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
    MergeReasonInto(bounds_reason, conflict);
    return false;
  }
  return true;
}

namespace {

std::string ReasonDebugString(const std::vector<Literal>& literals_reason,
                         const std::vector<IntegerLiteral>& bounds_reason) {
  std::string result = "literals:{";
  for (const Literal l : literals_reason) {
    if (result.back() != '{') result += ",";
    result += l.DebugString();
  }
  result += "} bounds:{";
  for (const IntegerLiteral l : bounds_reason) {
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
                           const std::vector<Literal>& literals_reason,
                           const std::vector<IntegerLiteral>& bounds_reason) {
  // Nothing to do if the bound is not better than the current one.
  if (i_lit.bound <= vars_[i_lit.var].current_bound) return true;
  ++num_enqueues_;

  // This may not indicate an incorectness, but just some propagators that
  // didn't reach a fixed-point at level zero.
  if (DEBUG_MODE && !integer_decision_levels_.empty()) {
    std::vector<Literal> l = literals_reason;
    MergeReasonInto(bounds_reason, &l);
    CHECK(!l.empty())
        << "Propagating a literal with no reason at a positive level!\n"
        << "level:" << integer_decision_levels_.size() << " i_lit:" << i_lit
        << " " << ReasonDebugString(literals_reason, bounds_reason) << "\n"
        << DebugString();
  }

  // For the EnqueueWithSameReasonAs() mechanism.
  BooleanVariable first_propagated_variable = kNoBooleanVariable;

  // Deal with fully encoded variable. We want to do that first because this may
  // make the IntegerLiteral bound stronger.
  const IntegerVariable var(i_lit.var);
  if (current_min_.ContainsKey(var)) {
    // Recover the current min, and propagate to false all the values that
    // are in [min, i_lit.value). All these literals have the same reason, so
    // we use the "same reason as" mecanism.
    //
    // TODO(user): We could go even further if the next literals are set to
    // false (but they need to be added to the reason).
    const int min_index = current_min_.FindOrDie(var);
    const auto& encoding = encoder_->FullDomainEncoding(var);
    if (i_lit.bound > encoding[min_index].value) {
      int i = min_index;
      for (; i < encoding.size(); ++i) {
        if (i_lit.bound <= encoding[i].value) break;
        const Literal literal = encoding[i].literal.Negated();
        if (!EnqueueAssociatedLiteral(literal, i_lit, literals_reason,
                                      bounds_reason,
                                      &first_propagated_variable)) {
          return false;
        }
      }

      if (i == encoding.size()) {
        // Conflict: no possible values left.
        return ReportConflict(literals_reason, bounds_reason);
      } else {
        // We have a new min.
        watched_min_.EraseOrDie(encoding[min_index].literal.NegatedIndex());
        watched_min_.Set(encoding[i].literal.NegatedIndex(), {var, i});
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
      *conflict = literals_reason;
      if (IsOptional(var)) {
        conflict->push_back(Literal(is_empty_literals_[var]));
      }

      // This is the same as:
      //   MergeReasonInto(bounds_reason, conflict);
      //   MergeReasonInto({UpperBoundAsLiteral(var)}, conflict);
      // but with just one call to MergeReasonIntoInternal() for speed. Note
      // that it may also produce a smaller reason overall.
      DCHECK(tmp_queue_.empty());
      const int size = vars_.size();
      for (const IntegerLiteral& literal : bounds_reason) {
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
    if (!EnqueueAssociatedLiteral(Literal(literal_index), i_lit,
                                  literals_reason, bounds_reason,
                                  &first_propagated_variable)) {
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
  if (!literals_reason.empty()) {
    literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                   literals_reason.begin(),
                                   literals_reason.end());
  }
  if (!bounds_reason.empty()) {
    CHECK_NE(bounds_reason[0].var, kNoIntegerVariable);
    bounds_reason_buffer_.insert(bounds_reason_buffer_.end(),
                                 bounds_reason.begin(), bounds_reason.end());
  }

  // Subtle: We do that after the IntegerLiteral as been enqueued for 2 reasons:
  // 1/ Any associated literal will use the TrailEntry above as a reason.
  // 2/ The precendence propagator currently rely on this.
  //    TODO(user): The point 2 is brittle, try to clean it up.
  if (propagate_is_empty) {
    const Literal is_empty = Literal(is_empty_literals_[var]);
    EnqueueLiteral(is_empty, literals_reason, bounds_reason);
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
}

void GenericLiteralWatcher::UpdateCallingNeeds() {
  // Process the newly changed variables lower bounds.
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var.value() >= var_to_watcher_ids_.size()) continue;
    for (const int id : var_to_watcher_ids_[var]) {
      if (!in_queue_[id]) {
        in_queue_[id] = true;
        queue_.push_back(id);
      }
    }
  }
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
}

bool GenericLiteralWatcher::Propagate(Trail* trail) {
  while (propagation_trail_index_ < trail->Index()) {
    const Literal literal = (*trail)[propagation_trail_index_++];
    if (literal.Index() >= literal_to_watcher_ids_.size()) continue;
    for (const int id : literal_to_watcher_ids_[literal.Index()]) {
      if (!in_queue_[id]) {
        in_queue_[id] = true;
        queue_.push_back(id);
      }
    }
  }

  const int level = trail->CurrentDecisionLevel();
  rev_int_repository_->SetLevel(level);
  UpdateCallingNeeds();
  while (!queue_.empty()) {
    const int id = queue_.front();
    queue_.pop_front();

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

    if (!watchers_[id]->Propagate()) {
      in_queue_[id] = false;
      return false;
    }
    UpdateCallingNeeds();

    // We mark the node afterwards because we assume that the Propagate() method
    // is idempotent and never need to be called twice in a row. If some
    // propagator don't have this property, we could add an option to call them
    // again until nothing changes.
    in_queue_[id] = false;
  }
  return true;
}

void GenericLiteralWatcher::Untrail(const Trail& trail, int trail_index) {
  if (propagation_trail_index_ <= trail_index) {
    // Nothing to do since we found a conflict before Propagate() was called.
    CHECK_EQ(propagation_trail_index_, trail_index);
    return;
  }

  // This means that we already propagated all there is to propagate
  // at the level trail_index, so we can safely clear modified_vars_ in case
  // it wasn't already done.
  propagation_trail_index_ = trail_index;
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  in_queue_.assign(watchers_.size(), false);
  queue_.clear();

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

  // Initially call everything.
  in_queue_.push_back(true);
  queue_.push_back(id);
  return id;
}

void GenericLiteralWatcher::RegisterReversibleClass(int id,
                                                    ReversibleInterface* rev) {
  id_to_reversible_classes_[id].push_back(rev);
}

void GenericLiteralWatcher::RegisterReversibleInt(int id, int* rev) {
  id_to_reversible_ints_[id].push_back(rev);
}

SatSolver::Status SolveIntegerProblemWithLazyEncoding(
    Model* model, const std::vector<IntegerVariable>& variables_to_finalize) {
  TimeLimit* time_limit = model->Mutable<TimeLimit>();
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  SatSolver::Status status = SatSolver::Status::MODEL_UNSAT;
  for (;;) {
    if (time_limit == nullptr) {
      status = solver->Solve();
    } else {
      status = solver->SolveWithTimeLimit(time_limit);
    }
    if (status != SatSolver::MODEL_SAT) break;

    // Look for an integer variable whose domain is not singleton.
    // Heuristic: we take the one with minimum lb amongst the given variables.
    //
    // TODO(user): consider better heuristics. Maybe we can use some kind of
    // IntegerVariable activity or something from the CP world.
    IntegerVariable candidate = kNoIntegerVariable;
    IntegerValue candidate_lb(0);
    for (const IntegerVariable variable : variables_to_finalize) {
      // Note that we use < and not != for optional variable whose domain may
      // be empty.
      const IntegerValue lb = integer_trail->LowerBound(variable);
      if (lb < integer_trail->UpperBound(variable)) {
        if (candidate == kNoIntegerVariable || lb < candidate_lb) {
          candidate = variable;
          candidate_lb = lb;
        }
      }
    }

    if (candidate == kNoIntegerVariable) break;
    // This add a literal with good polarity. It is very important that the
    // decision heuristic assign it to false! Otherwise, our heuristic is not
    // good.
    integer_encoder->CreateAssociatedLiteral(
        IntegerLiteral::GreaterOrEqual(candidate, candidate_lb + 1));
  }
  return status;
}

}  // namespace sat
}  // namespace operations_research
