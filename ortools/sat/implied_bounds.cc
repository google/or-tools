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

#include "ortools/sat/implied_bounds.h"

#include <algorithm>

#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

// Just display some global statistics on destruction.
ImpliedBounds::~ImpliedBounds() {
  VLOG(1) << num_deductions_ << " enqueued deductions.";
  VLOG(1) << bounds_.size() << " implied bounds stored.";
  VLOG(1) << num_enqueued_in_var_to_bounds_
          << " implied bounds with view stored.";
}

void ImpliedBounds::Add(Literal literal, IntegerLiteral integer_literal) {
  if (!parameters_.use_implied_bounds()) return;
  const IntegerVariable var = integer_literal.var;

  // Update our local level-zero bound.
  if (var >= level_zero_lower_bounds_.size()) {
    level_zero_lower_bounds_.resize(var.value() + 1, kMinIntegerValue);
    new_level_zero_bounds_.Resize(var + 1);
  }
  level_zero_lower_bounds_[var] = std::max(
      level_zero_lower_bounds_[var], integer_trail_->LevelZeroLowerBound(var));

  // Ignore any Add() with a bound worse than the level zero one.
  // TODO(user): Check that this never happen? it shouldn't.
  if (integer_literal.bound <= level_zero_lower_bounds_[var]) {
    return;
  }

  // We skip any IntegerLiteral referring to a variable with only two
  // consecutive possible values. This is because, once shifted this will
  // already be a variable in [0, 1] so we shouldn't gain much by substituing
  // it.
  if (integer_trail_->LevelZeroLowerBound(var) + 1 >=
      integer_trail_->LevelZeroUpperBound(var)) {
    return;
  }

  // Add or update the current bound.
  const auto key = std::make_pair(literal.Index(), var);
  auto insert_result = bounds_.insert({key, integer_literal.bound});
  if (!insert_result.second) {
    if (insert_result.first->second < integer_literal.bound) {
      insert_result.first->second = integer_literal.bound;
    } else {
      // No new info.
      return;
    }
  }

  // Checks if the variable is now fixed.
  if (integer_trail_->LevelZeroUpperBound(var) == integer_literal.bound) {
    AddLiteralImpliesVarEqValue(literal, var, integer_literal.bound);
  } else {
    const auto it =
        bounds_.find(std::make_pair(literal.Index(), NegationOf(var)));
    if (it != bounds_.end() && it->second == -integer_literal.bound) {
      AddLiteralImpliesVarEqValue(literal, var, integer_literal.bound);
    }
  }

  // Check if we have any deduction. Since at least one of (literal,
  // literal.Negated()) must be true, we can take the min bound as valid at
  // level zero.
  //
  // TODO(user): Like in probing, we can also create hole in the domain if there
  // is some implied bounds for (literal.NegatedIndex, NegagtionOf(var)) that
  // crosses integer_literal.bound.
  const auto it = bounds_.find(std::make_pair(literal.NegatedIndex(), var));
  if (it != bounds_.end()) {
    if (it->second <= level_zero_lower_bounds_[var]) {
      // The other bounds is worse than the new level-zero bound which can
      // happen because of lazy update, so here we just remove it.
      bounds_.erase(it);
    } else {
      const IntegerValue deduction =
          std::min(integer_literal.bound, it->second);
      DCHECK_GT(deduction, level_zero_lower_bounds_[var]);
      DCHECK_GT(deduction, integer_trail_->LevelZeroLowerBound(var));

      // TODO(user): support Enqueueing level zero fact at a positive level.
      // That is, do not loose the info on backtrack. This should be doable. It
      // is also why we return a bool in case of conflict when pushing
      // deduction.
      ++num_deductions_;
      level_zero_lower_bounds_[var] = deduction;
      new_level_zero_bounds_.Set(var);

      VLOG(1) << "Deduction old: "
              << IntegerLiteral::GreaterOrEqual(
                     var, integer_trail_->LevelZeroLowerBound(var))
              << " new: " << IntegerLiteral::GreaterOrEqual(var, deduction);

      // The entries that are equal to the min no longer need to be stored once
      // the level zero bound is enqueued.
      if (it->second == deduction) {
        bounds_.erase(it);
      }
      if (integer_literal.bound == deduction) {
        bounds_.erase(std::make_pair(literal.Index(), var));

        // No need to update var_to_bounds_ in this case.
        return;
      }
    }
  }

  // While the code above deal correctly with optionality, we cannot just
  // register a literal => bound for an optional variable, because the equation
  // might end up in the LP which do not handle them correctly.
  //
  // TODO(user): Maybe we can handle this case somehow, as long as every
  // constraint using this bound is protected by the variable optional literal.
  // Alternativelly we could disable optional variable when we are at
  // linearization level 2.
  if (integer_trail_->IsOptional(var)) return;

  // If we have a new implied bound and the literal has a view, add it to
  // var_to_bounds_. Note that we might add more than one entry with the same
  // literal_view, and we will later need to lazily clean the vector up.
  if (integer_encoder_->GetLiteralView(literal) != kNoIntegerVariable) {
    if (var_to_bounds_.size() <= var) {
      var_to_bounds_.resize(var.value() + 1);
      has_implied_bounds_.Resize(var + 1);
    }
    ++num_enqueued_in_var_to_bounds_;
    has_implied_bounds_.Set(var);
    var_to_bounds_[var].push_back({integer_encoder_->GetLiteralView(literal),
                                   integer_literal.bound, true});
  } else if (integer_encoder_->GetLiteralView(literal.Negated()) !=
             kNoIntegerVariable) {
    if (var_to_bounds_.size() <= var) {
      var_to_bounds_.resize(var.value() + 1);
      has_implied_bounds_.Resize(var + 1);
    }
    ++num_enqueued_in_var_to_bounds_;
    has_implied_bounds_.Set(var);
    var_to_bounds_[var].push_back(
        {integer_encoder_->GetLiteralView(literal.Negated()),
         integer_literal.bound, false});
  }
}

const std::vector<ImpliedBoundEntry>& ImpliedBounds::GetImpliedBounds(
    IntegerVariable var) {
  if (var >= var_to_bounds_.size()) return empty_implied_bounds_;

  // Lazily remove obsolete entries from the vector.
  //
  // TODO(user): Check no duplicate and remove old entry if the enforcement
  // is tighter.
  int new_size = 0;
  std::vector<ImpliedBoundEntry>& ref = var_to_bounds_[var];
  const IntegerValue level_zero_lb = std::max(
      level_zero_lower_bounds_[var], integer_trail_->LevelZeroLowerBound(var));
  level_zero_lower_bounds_[var] = level_zero_lb;
  for (const ImpliedBoundEntry& entry : ref) {
    if (entry.lower_bound <= level_zero_lb) continue;
    ref[new_size++] = entry;
  }
  ref.resize(new_size);

  return ref;
}

void ImpliedBounds::AddLiteralImpliesVarEqValue(Literal literal,
                                                IntegerVariable var,
                                                IntegerValue value) {
  if (!VariableIsPositive(var)) {
    var = NegationOf(var);
    value = -value;
  }
  literal_to_var_to_value_[literal.Index()][var] = value;
}

void ImpliedBounds::ProcessIntegerTrail(Literal first_decision) {
  if (!parameters_.use_implied_bounds()) return;

  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 1);
  tmp_integer_literals_.clear();
  integer_trail_->AppendNewBounds(&tmp_integer_literals_);
  for (const IntegerLiteral lit : tmp_integer_literals_) {
    Add(first_decision, lit);
  }
}

void ImpliedBounds::AddElementEncoding(
    IntegerVariable var, const std::vector<ValueLiteralPair>& encoding) {
  const auto& it = var_to_element_encodings_.find(var);
  if (it == var_to_element_encodings_.end()) {
    element_encoded_variables_.push_back(var);
    var_to_element_encodings_[var].push_back(encoding);
  } else {
    it->second.push_back(encoding);
  }
}

const std::vector<std::vector<ValueLiteralPair>>&
ImpliedBounds::GetElementEncodings(IntegerVariable var) {
  const auto& it = var_to_element_encodings_.find(var);
  if (it == var_to_element_encodings_.end()) {
    return empty_element_encoding_;
  } else {
    return it->second;
  }
}

const std::vector<IntegerVariable>& ImpliedBounds::GetElementEncodedVariables()
    const {
  return element_encoded_variables_;
}

bool ImpliedBounds::EnqueueNewDeductions() {
  CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
  for (const IntegerVariable var :
       new_level_zero_bounds_.PositionsSetAtLeastOnce()) {
    if (!integer_trail_->Enqueue(
            IntegerLiteral::GreaterOrEqual(var, level_zero_lower_bounds_[var]),
            {}, {})) {
      return false;
    }
  }
  new_level_zero_bounds_.SparseClearAll();
  return sat_solver_->FinishPropagation();
}

bool DetectLinearEncodingOfProducts(
    IntegerVariable left_var, IntegerVariable right_var, Model* model,
    std::vector<ValueLiteralPair>* product_encoding) {
  CHECK(product_encoding != nullptr);
  ImpliedBounds* implied_bounds = model->GetOrCreate<ImpliedBounds>();
  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();

  // Fill in the encodings for the left variable, and sort them.
  std::vector<std::vector<ValueLiteralPair>> left_encodings =
      implied_bounds->GetElementEncodings(left_var);
  if (integer_encoder->VariableIsFullyEncoded(left_var)) {
    left_encodings.push_back(integer_encoder->FullDomainEncoding(left_var));
  }
  for (auto& encoding : left_encodings) {
    std::sort(encoding.begin(), encoding.end(),
              ValueLiteralPair::CompareByLiteral());
  }

  // Fill in the encodings for the right variable, and sort them.
  std::vector<std::vector<ValueLiteralPair>> right_encodings =
      implied_bounds->GetElementEncodings(right_var);
  if (integer_encoder->VariableIsFullyEncoded(right_var)) {
    right_encodings.push_back(integer_encoder->FullDomainEncoding(right_var));
  }
  for (auto& encoding : right_encodings) {
    std::sort(encoding.begin(), encoding.end(),
              ValueLiteralPair::CompareByLiteral());
  }

  if (left_encodings.empty() || right_encodings.empty()) return false;

  // If the encodings have exactly the same literals, we are done.
  const auto match_encodings = [product_encoding](
                                   const std::vector<ValueLiteralPair>& left,
                                   const std::vector<ValueLiteralPair>& right) {
    if (left.size() != right.size()) return false;
    for (int i = 0; i < left.size(); ++i) {
      if (left[i].literal != right[i].literal) return false;
    }
    product_encoding->clear();
    for (int i = 0; i < left.size(); ++i) {
      product_encoding->push_back(
          {left[i].value * right[i].value, left[i].literal});
    }
    return true;
  };

  // If one of the variable has an encoding of size two, it is likely that its
  // encoding was stored as (lit, not(lit)). We need to do extra work in this
  // case.
  const auto reconcile_encodings =
      [product_encoding](
          const std::vector<ValueLiteralPair>& size_two_encoding,
          const std::vector<ValueLiteralPair>& general_encoding) {
        if (size_two_encoding.size() != 2) return false;
        if (size_two_encoding[0].literal !=
            size_two_encoding[1].literal.Negated()) {
          return false;
        }
        Literal lit0 = size_two_encoding[0].literal;
        IntegerValue value0 = size_two_encoding[0].value;
        Literal lit1 = size_two_encoding[1].literal;
        IntegerValue value1 = size_two_encoding[1].value;
        for (const ValueLiteralPair e : general_encoding) {
          if (e.literal == lit1) {
            std::swap(lit0, lit1);
            std::swap(value0, value1);
          }
          if (e.literal == lit0) {
            if (product_encoding != nullptr) {
              product_encoding->clear();
              for (const ValueLiteralPair& f : general_encoding) {
                if (f.literal == lit0) {
                  product_encoding->push_back({value0 * f.value, f.literal});
                } else {
                  product_encoding->push_back({value1 * f.value, f.literal});
                }
              }
            }
            return true;
          }
        }
        return false;
      };

  for (const std::vector<ValueLiteralPair>& left : left_encodings) {
    for (const std::vector<ValueLiteralPair>& right : right_encodings) {
      if (match_encodings(left, right) || reconcile_encodings(left, right) ||
          reconcile_encodings(right, left)) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace sat
}  // namespace operations_research
