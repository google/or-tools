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

  // If the variable has already been fully encoded, for now we check that
  // the sets of value is the same.
  //
  // TODO(user): Take the intersection, and handle that case in the constraints
  // creation functions.
  if (ContainsKey(full_encoding_index_, i_var)) {
    const std::vector<ValueLiteralPair>& encoding = FullDomainEncoding(i_var);
    CHECK_EQ(values.size(), encoding.size());
    for (int i = 0; i < values.size(); ++i) {
      CHECK_EQ(values[i], encoding[i].value);
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
  reverse_encoding_[literal.Index()] = i_lit;

  // Add its negation and associated it with i_lit.Negated().
  //
  // TODO(user): This seems to work for optional variables, but I am not
  // 100% sure why!! I think it works because these literals can only appear
  // in a conflict if the presence literal of the optional variables is true.
  AddImplications(i_lit.Negated(), literal.Negated());
  reverse_encoding_[literal.NegatedIndex()] = i_lit.Negated();
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
    const IntegerLiteral i_lit = encoder_->GetIntegerLiteral(literal);
    if (i_lit.var >= 0) {
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
    vars_[entry.var].current_trail_index = entry.prev_trail_index;
    vars_[entry.var].current_bound =
        integer_trail_[entry.prev_trail_index].bound;
  }

  // Resize vectors.
  literals_reason_buffer_.resize(
      integer_trail_[target].literals_reason_start_index);
  dependencies_buffer_.resize(integer_trail_[target].dependencies_start_index);
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
  CHECK_LE(i_lit.bound, vars_[i_lit.var].current_bound);
  if (i_lit.bound <= LevelZeroBound(i_lit.var)) return -1;
  int prev_trail_index = vars_[i_lit.var].current_trail_index;
  int trail_index = prev_trail_index;
  while (i_lit.bound <= integer_trail_[trail_index].bound) {
    prev_trail_index = trail_index;
    trail_index = integer_trail_[trail_index].prev_trail_index;
    CHECK_GE(trail_index, 0);
  }
  return prev_trail_index;
}

bool IntegerTrail::EnqueueAssociatedLiteral(
    Literal literal, IntegerLiteral i_lit,
    const std::vector<Literal>& literals_reason,
    const std::vector<IntegerLiteral>& bounds_reason,
    BooleanVariable* variable_with_same_reason) {
  if (!trail_->Assignment().VariableIsAssigned(literal.Variable())) {
    if (variable_with_same_reason != nullptr) {
      if (*variable_with_same_reason != kNoBooleanVariable) {
        trail_->EnqueueWithSameReasonAs(literal, *variable_with_same_reason);
        return true;
      }
      *variable_with_same_reason = literal.Variable();
    }

    // The reason is simply i_lit and will be expanded lazily when needed.
    //
    // Note(user): Instead of setting the reason to just i_lit, we set it to the
    // reason why i_lit was about to be propagated, otherwise in some corner
    // cases (not 100% clear), i_lit will not be pushed yet when the reason of
    // this literal will be queried and that is an issue.
    EnqueueLiteral(literal, literals_reason, bounds_reason);
    return true;
  }
  if (trail_->Assignment().LiteralIsFalse(literal)) {
    std::vector<Literal>* conflict = trail_->MutableConflict();
    *conflict = literals_reason;
    conflict->push_back(literal);
    MergeReasonInto(bounds_reason, conflict);
    return false;
  }
  return true;
}

bool IntegerTrail::Enqueue(IntegerLiteral i_lit,
                           const std::vector<Literal>& literals_reason,
                           const std::vector<IntegerLiteral>& bounds_reason) {
  // Nothing to do if the bound is not better than the current one.
  if (i_lit.bound <= vars_[i_lit.var].current_bound) return true;
  ++num_enqueues_;

  // For the EnqueueWithSameReasonAs() mechanism.
  BooleanVariable first_propagated_variable = kNoBooleanVariable;

  // Deal with fully encoded variable. We want to do that first because this may
  // make the IntegerLiteral bound stronger.
  const IntegerVariable var(i_lit.var);
  if (current_min_.ContainsKey(var)) {
    // Recover the current min, and propagate to false all the values that
    // are in [min, i_lit.value). All these literals have the same reason, so
    // we use the "same reason as" mecanism.
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
        std::vector<Literal>* conflict = trail_->MutableConflict();
        *conflict = literals_reason;
        MergeReasonInto(bounds_reason, conflict);
        return false;
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
  if (i_lit.bound > UpperBound(var)) {
    if (!IsOptional(var) ||
        trail_->Assignment().LiteralIsFalse(Literal(is_empty_literals_[var]))) {
      std::vector<Literal>* conflict = trail_->MutableConflict();
      *conflict = literals_reason;
      if (IsOptional(var)) {
        conflict->push_back(Literal(is_empty_literals_[var]));
      }
      MergeReasonInto(bounds_reason, conflict);
      MergeReasonInto({UpperBoundAsLiteral(var)}, conflict);
      return false;
    } else {
      // Subtle: We still want to propagate the bounds in this case, so we
      // don't return right away. The precedences propagator rely on this.
      // TODO(user): clean this.
      const Literal is_empty = Literal(is_empty_literals_[var]);
      if (!trail_->Assignment().LiteralIsTrue(is_empty)) {
        std::vector<Literal>* literal_reason_ptr;
        std::vector<IntegerLiteral>* integer_reason_ptr;
        EnqueueLiteral(is_empty, &literal_reason_ptr, &integer_reason_ptr);
        *literal_reason_ptr = literals_reason;
        *integer_reason_ptr = bounds_reason;
        integer_reason_ptr->push_back(UpperBoundAsLiteral(var));
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
                                  literals_reason, bounds_reason, nullptr)) {
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
       /*literals_reason_start_index=*/static_cast<int32>(
           literals_reason_buffer_.size()),
       /*dependencies_start_index=*/static_cast<int32>(
           dependencies_buffer_.size())});
  vars_[i_lit.var].current_bound = i_lit.bound;
  vars_[i_lit.var].current_trail_index = integer_trail_.size() - 1;

  // Copy literals_reason into our internal buffer.
  literals_reason_buffer_.insert(literals_reason_buffer_.end(),
                                 literals_reason.begin(),
                                 literals_reason.end());

  // Convert each IntegerLiteral reason to the index of an entry in the integer
  // trail.
  //
  // TODO(user): Do that lazily when the lazy reason are implemented.
  //
  // TODO(user): Check that the same IntegerVariable never appear twice. If it
  // does the one with the lowest bound could be removed.
  const int size = vars_.size();
  for (IntegerLiteral i_lit : bounds_reason) {
    const int reason_tail_index = FindLowestTrailIndexThatExplainBound(i_lit);
    if (reason_tail_index >= size) {
      dependencies_buffer_.push_back(reason_tail_index);
    }
  }

  return true;
}

BeginEndWrapper<std::vector<int>::const_iterator> IntegerTrail::Dependencies(
    int trail_index) const {
  return BeginEndRange(
      dependencies_buffer_.begin() +
          integer_trail_[trail_index].dependencies_start_index,
      trail_index + 1 < integer_trail_.size()
          ? dependencies_buffer_.begin() +
                integer_trail_[trail_index + 1].dependencies_start_index
          : dependencies_buffer_.end());
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
  tmp_trail_indices_.clear();
  tmp_var_to_highest_explained_trail_index_.resize(vars_.size(), 0);
  DCHECK(std::all_of(tmp_var_to_highest_explained_trail_index_.begin(),
                     tmp_var_to_highest_explained_trail_index_.end(),
                     [](int v) { return v == 0; }));

  const int size = vars_.size();
  for (const IntegerLiteral& literal : literals) {
    const int trail_index = FindLowestTrailIndexThatExplainBound(literal);

    // Any indices lower than that means that there is no reason needed.
    // Note that it is important for size to be signed because of -1 indices.
    if (trail_index >= size) tmp_queue_.push_back(trail_index);
  }

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
      for (const int next_trail_index : Dependencies(trail_index)) {
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
  *reason = literal_reasons_[trail_index];
  MergeReasonInto(integer_reasons_[trail_index], reason);
  if (DEBUG_MODE && trail.CurrentDecisionLevel() > 0) CHECK(!reason->empty());
  return ClauseRef(*reason);
}

void IntegerTrail::EnqueueLiteral(Literal literal,
                                  std::vector<Literal>** literal_reason,
                                  std::vector<IntegerLiteral>** integer_reason) {
  const int trail_index = trail_->Index();
  if (trail_index >= literal_reasons_.size()) {
    literal_reasons_.resize(trail_index + 1);
    integer_reasons_.resize(trail_index + 1);
  }
  literal_reasons_[trail_index].clear();
  integer_reasons_[trail_index].clear();
  if (literal_reason != nullptr) {
    *literal_reason = &literal_reasons_[trail_index];
  }
  if (integer_reason != nullptr) {
    *integer_reason = &integer_reasons_[trail_index];
  }
  trail_->Enqueue(literal, propagator_id_);
}

void IntegerTrail::EnqueueLiteral(
    Literal literal, const std::vector<Literal>& literal_reason,
    const std::vector<IntegerLiteral>& integer_reason) {
  std::vector<Literal>* literal_reason_ptr;
  std::vector<IntegerLiteral>* integer_reason_ptr;
  EnqueueLiteral(literal, &literal_reason_ptr, &integer_reason_ptr);
  *literal_reason_ptr = literal_reason;
  *integer_reason_ptr = integer_reason;
}

GenericLiteralWatcher::GenericLiteralWatcher(IntegerTrail* integer_trail)
    : Propagator("GenericLiteralWatcher"), integer_trail_(integer_trail) {
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

  UpdateCallingNeeds();
  while (!queue_.empty()) {
    const int id = queue_.front();
    queue_.pop_front();

    if (!watchers_[id]->Propagate(trail)) {
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
  if (propagation_trail_index_ > trail_index) {
    // This means that we already propagated all there is to propagate
    // at the level trail_index, so we can safely clear modified_vars_ in case
    // it wasn't already done.
    modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
    in_queue_.assign(watchers_.size(), false);
    queue_.clear();
  }
  propagation_trail_index_ = trail_index;
}

// Registers a propagator and returns its unique ids.
int GenericLiteralWatcher::Register(PropagatorInterface* propagator) {
  const int id = watchers_.size();
  watchers_.push_back(propagator);

  // Initially call everything.
  in_queue_.push_back(true);
  queue_.push_back(id);
  return id;
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
