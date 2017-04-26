// Copyright 2011-2012 Pierre Schaus
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/string_array.h"
#include "ortools/util/vector_map.h"

namespace operations_research {
namespace {
class SoftGCC : public Constraint{
 public:
  static const int64 kUnassigned = kint64min;
  enum FlowType { UF, OF };

  // Constraint the values minval+i to appear between card_mins_i] and
  // card_max_[i] times in x but accept some violations to this rule.
  // For the value vi = minval+i, let ci be the number of occurences
  // in x and viol(vi) = max(0, card_mins_i]-ci, ci-card_max_[i])
  // i.e. viol(vi) is the shortage or excess wrt the prescribed
  // cardinalities.
  SoftGCC(Solver* const solver,
          const std::vector<IntVar*>& vars,
          int64 min_value,
          const std::vector<int64>& card_mins,
          const std::vector<int64>& card_max,
          IntVar* const violation_var)
      : Constraint(solver),
        vars_(vars),
        min_value_(min_value),
        max_value_(min_value + card_max.size() - 1),
        num_values_(card_mins.size()),
        card_mins_(card_mins),
        card_max_(card_max),
        violation_var_(violation_var),
        sum_card_min_(0),
        underflow_(nullptr),
        under_variable_match_(nullptr),
        under_value_match_(nullptr),
        under_total_flow_(0),
        under_next_match_(nullptr),
        under_previous_match_(nullptr),
        overflow_(nullptr),
        over_variable_match_(nullptr),
        over_value_match_(nullptr),
        over_total_flow_(0),
        over_next_match_(nullptr),
        over_previous_match_(nullptr),
        magic_(0),
        dfs_(0),
        component_(0),
        variable_component_(nullptr),
        variable_dfs_(nullptr),
        variable_high_(nullptr),
        value_component_(nullptr),
        value_dfs_(nullptr),
        value_high_(nullptr),
        sink_component_(0),
        sink_dfs_(0),
        sink_high_(0),
        is_var_always_matched_in_underflow_(nullptr),
        is_var_always_matched_in_overflow_(nullptr),
        stack_(nullptr),
        type_(nullptr) {
    for (int64 i = 0; i < num_values_; i++) {
      CHECK_GE(card_mins_[i], 0);
      CHECK_GE(card_max_[i], 0);
      CHECK_LE(card_mins_[i], card_max_[i]);
    }
  }

  virtual void Post() {
    ComputeRangeOfValues();
    AllocateFlow();
    FindInitialFlow();

    AllocateScc();

    Demon* const demon =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);

    for (int k = 0; k < vars_.size(); ++k) {
      vars_[k]->WhenDomain(demon);
    }

    violation_var_->WhenRange(demon);
  }

  virtual void InitialPropagate() {
    for (int64 k = 0; k < vars_.size() ; k++) {
      if (under_variable_match_[k] != kUnassigned) {
        if (!vars_[k]->Contains(under_variable_match_[k])) {
          Unassign(k, UF);
        }
      }
      if (over_variable_match_[k] != kUnassigned) {
        if (!vars_[k]->Contains(over_variable_match_[k])) {
          Unassign(k, OF);
        }
      }
    }

    const int64 min_violation = ViolationValue();

    // Prunes lower bound of violation.
    violation_var_->SetMin(min_violation);

    // Prunes variable domains (the constraint is consistent at this point).
    Prune(min_violation);

    // Prune upper bound of violation if all variables are bound.
    bool all_bound = true;
    for (int64 i = 0 ; i < vars_.size(); i++) {
      if (!vars_[i]->Bound()) {
        all_bound = false;
        break;
      }
    }
    if (all_bound) {
      violation_var_->SetMax(min_violation);
    }
  }


  void ComputeRangeOfValues() {
    const int64 prev_min_value = min_value_;
    const int64 prev_max_value = max_value_;
    const int64 prev_num_values = num_values_;

    for (int64 i = 0; i < vars_.size(); i++) {
      min_value_ = std::min(min_value_, vars_[i]->Min());
      max_value_ = std::max(max_value_, vars_[i]->Max());
    }

    if (prev_min_value != min_value_ || prev_max_value != max_value_) {
      const int64 delta = prev_min_value - min_value_;
      num_values_ = max_value_ - min_value_ + 1;

      // low
      std::vector<int64> new_card_mins(num_values_);

      // up
      std::vector<int64> new_card_max(num_values_);
      for (int64 k = 0; k < num_values_; k++) {
        new_card_max[k] = vars_.size();
      }

      sum_card_min_ = 0;

      for (int64 i = 0 ; i < prev_num_values; i++) {
        if (card_mins_[i] > 0) {
          new_card_mins[i + delta] = card_mins_[i] ;
          sum_card_min_ += card_mins_[i];
        }
      }

      for (int64 i = 0 ; i < prev_num_values; i++) {
        if (card_max_[i] < vars_.size()) {
          new_card_max[i + delta] = card_max_[i] ;
        }
      }
      card_mins_.swap(new_card_mins);
      card_max_.swap(new_card_max);
    } else {
      sum_card_min_ = 0;
      for (int64 i = 0 ; i < num_values_; i++) {
        if (card_mins_[i] > 0) {
          sum_card_min_ += card_mins_[i];
        }
      }
    }
  }

  void AllocateFlow() {
    // flow
    underflow_.reset(NewIntArray(num_values_));
    overflow_.reset(NewIntArray(num_values_));

    // first variable matched
    under_value_match_.reset(new int64 [num_values_]);
    over_value_match_.reset(new int64 [num_values_]);
    for (int64 k = 0; k < num_values_; k++) {
      under_value_match_[k] = kUnassigned;  // unmatched
      over_value_match_[k] = kUnassigned;  // unmatched
    }

    // next variable matched
    under_next_match_.reset(new int64[vars_.size()]);
    over_next_match_.reset(new int64[vars_.size()]);
    for (int64 k = 0; k < vars_.size(); k++) {
      under_next_match_[k] = kUnassigned;  // no next
      over_next_match_[k] = kUnassigned;  // no next
    }

    // previous variable matched
    under_previous_match_.reset(new int64[vars_.size()]);
    over_previous_match_.reset(new int64[vars_.size()]);
    for (int64 k = 0; k < vars_.size(); k++) {
      under_previous_match_[k] = kUnassigned;  // no prev
      over_previous_match_[k] = kUnassigned;  // no prev
    }

    // variable assignment
    under_variable_match_.reset(new int64 [vars_.size()]);
    over_variable_match_.reset(new int64 [vars_.size()]);
    for (int64 k = 0 ; k < vars_.size(); k++) {
      under_variable_match_[k] = kUnassigned; // unmatched
      over_variable_match_[k] = kUnassigned; // unmatched
    }

    variable_seen_.reset(new int64[vars_.size()]);
    value_seen_.reset(new int64[num_values_]);
    for (int64 k = 0; k < vars_.size(); k++) {
      variable_seen_[k] = false;
    }
    for (int64 k = 0; k < num_values_; k++) {
      value_seen_[k] = false;
    }

    under_variable_component_.reset(NewIntArray(vars_.size()));
    under_value_component_.reset(NewIntArray(num_values_));

    magic_ = 0;
  }

  // Assigns value v to variable k and update structures: sizeFlow,
  // flow, var_match, prev, next, value_match
  void Assign(int64 k, int64 v, FlowType ft) {
    // For each value, the quantity of flow into this value.
    int64* flow;
    // For each variable, the value it is matched to.
    int64* var_match;
    // Next variable matched.
    int64* next;
    // Previous variable matched.
    int64* prev;
    // First variable matched to the value.
    int64* value_match;

    if (ft == UF) {
      flow = underflow_.get();
      var_match = under_variable_match_.get();
      next = under_next_match_.get();
      prev = under_previous_match_.get();
      value_match = under_value_match_.get();
      under_total_flow_++;
    } else { // OF
      flow = overflow_.get();
      var_match = over_variable_match_.get();
      next = over_next_match_.get();
      prev = over_previous_match_.get();
      value_match = over_value_match_.get();
      over_total_flow_++;
    }

    Unassign(k, ft);

    // k is now first on the list of v.
    var_match[k] = v;
    flow[v - min_value_]++;
    const int64 nk = value_match[v - min_value_];
    next[k] = nk;
    prev[k] = kUnassigned;
    if (nk != kUnassigned) {
      prev[nk] = k;
    }
    value_match[v - min_value_] = k;
  }

  // Unassings variable k and updates appropriately the structures:
  // sizeFlow, flow, var_match, prev, next, value_match.
  void Unassign(int64 k, FlowType ft) {
    // For each value, the quantity of flow into this value.
    int64* flow;
    // For each variable, the value it is matched to.
    int64* var_match;
    // Neext variable matched.
    int64* next;
    // Previous variable matched.
    int64* prev;
    // First variable matched to the value.
    int64* value_match;

    if (ft == UF) {
      flow = underflow_.get();
      var_match = under_variable_match_.get();
      next = under_next_match_.get();
      prev = under_previous_match_.get();
      value_match = under_value_match_.get();
    } else { // OF
      flow = overflow_.get();
      var_match = over_variable_match_.get();
      next = over_next_match_.get();
      prev = over_previous_match_.get();
      value_match = over_value_match_.get();
    }

    if (var_match[k] != kUnassigned) { // this guy is assigned; must be removed
      if (ft == UF) {
        under_total_flow_--;
      } else {
        over_total_flow_--;
      }

      int64 w = var_match[k];
      flow[w - min_value_]--;
      if (value_match[w - min_value_] == k) { // first in the list
        int64 nk = next[k];
        value_match[w - min_value_] = nk;
        if (nk != kUnassigned) {
          prev[nk] = kUnassigned; // nk is now first
        }
      } else { // not first
        int64 pk = prev[k];
        int64 nk = next[k];
        next[pk] = nk;
        if (nk != kUnassigned) {
          prev[nk] = pk;
        }
      }
      var_match[k] = kUnassigned;
    }
  }

  // Finds a initial flow for both the underflow and overflow.
  void FindInitialFlow() {
    under_total_flow_ = 0;
    over_total_flow_ = 0;
    for (int64 k = 0; k < vars_.size(); k++) {
      int64 var_min = vars_[k]->Min();
      int64 var_max = vars_[k]->Max();
      for (int64 i = var_min; i <= var_max; i++) {
        if (underflow_[i - min_value_] < card_mins_[i - min_value_]) {
          if (vars_[k]->Contains(i)) {
            Assign(k, i, UF);
            break;
          }
        }
      }
      for (int64 i = var_min; i <= var_max; i++) {
        if (overflow_[i - min_value_] < card_max_[i - min_value_]) {
          if (vars_[k]->Contains(i)) {
            Assign(k, i, OF);
            break;
          }
        }
      }
    }
  }

  int64 ViolationValue() {
    int64 buf = FindBestUnderflow();
    int64 bof = FindBestOverflow();
    return buf+bof;
  }

  // Computes and returns the best under flow.
  int64 FindBestUnderflow() {
    for (int64 k = 0;
         k < vars_.size() && under_total_flow_ < vars_.size();
         k++) {
      if (under_variable_match_[k] == kUnassigned) {
        magic_++;
        FindAugmentingPath(k, UF);
      }
    }
    return sum_card_min_ - under_total_flow_;
  }

  // Computes and returns the best over flow.
  int64 FindBestOverflow() {
    // In order to have the best overflow AND underflow, I start from
    // the best under flow to compute the best overflow (very
    // important for the methods
    // hasValInBestAssignment/getValInBestAssignment)
    for (int64 i = min_value_; i <= max_value_; i++) {
      overflow_[i - min_value_] = underflow_[i - min_value_];
      over_value_match_[i - min_value_] = under_value_match_[i - min_value_];
    }
    for (int64 k = 0; k < vars_.size() ; k++) {
      over_variable_match_[k] = under_variable_match_[k];
      over_next_match_[k] = under_next_match_[k];
      over_previous_match_[k] = under_previous_match_[k];
    }
    over_total_flow_ = under_total_flow_;

    for (int64 k = 0;
         k < vars_.size() && over_total_flow_ < vars_.size();
         k++) {
      if (over_variable_match_[k] == kUnassigned) {
        magic_++;
        FindAugmentingPath(k, OF);
      }
    }
    return vars_.size() - over_total_flow_;
  }

  // Finds augmenting path from variable i which is currently not
  // matched (true if found a path, false otherwise).
  bool FindAugmentingPath(int64 k, FlowType ft) {
    int64* const var_match = (ft == UF) ?
        under_variable_match_.get() :
        over_variable_match_.get();

    if (variable_seen_[k] != magic_) {
      variable_seen_[k] = magic_;
      int64 var_min = vars_[k]->Min();
      int64 var_max = vars_[k]->Max();
      if (vars_[k]->Size() == var_max - var_min + 1) {  // Dense domain.
        for (int64 v = var_min; v <= var_max; v++) {
          if (var_match[k] != v && FindAugmentingPathValue(v, ft)) {
            Assign(k, v, ft);
            return true;
          }
        }
      } else {
        for (int64 v = var_min; v <= var_max; v++) {
          if (var_match[k] != v &&
              vars_[k]->Contains(v) &&
              FindAugmentingPathValue(v, ft)) {
            Assign(k, v, ft);
            return true;
          }
        }
      }
    }
    return false;
  }

  bool FindAugmentingPathValue(int64 v, FlowType ft) {
    // For each value, the quantity of flow into this value.
    int64* flow;
    // Bext variable matched.
    int64* next;
    // First variable matched to the value.
    int64* value_match;
    // Capacity (low for ft==UF, up for ft==OF).
    const int64* capa;

    if (ft == UF) {
      flow = underflow_.get();
      next = under_next_match_.get();
      value_match = under_value_match_.get();
      capa = card_mins_.data();
    } else { // OF
      flow = overflow_.get();
      next = over_next_match_.get();
      value_match = over_value_match_.get();
      capa = card_max_.data();
    }

    if (value_seen_[v - min_value_] != magic_) {
      value_seen_[v - min_value_] = magic_;
      if (flow[v - min_value_] < capa[v - min_value_]) {
        return true;
      } else if (flow[v - min_value_] > 0) {
        int64 i = value_match[v - min_value_];
        while (i != kUnassigned) {
          if (FindAugmentingPath(i, ft)) {
            return true;
          }
          i = next[i];
        }
      }
    }
    return false;
  }

  void ComputeIsVarAlwaysMatched(FlowType ft) {
    bool* is_var_always_matched;
    int64* var_match;

    if (ft == UF) {
      is_var_always_matched = is_var_always_matched_in_underflow_.get();
      var_match = under_variable_match_.get();
    } else {
      is_var_always_matched = is_var_always_matched_in_overflow_.get();
      var_match = over_variable_match_.get();
    }

    num_vars_in_component_.resize(component_ + 1);
    for (int64 k = 0; k < vars_.size(); k++) {
      if (var_match[k] == kUnassigned ) {
        num_vars_in_component_[variable_component_[k]]++;
      }
    }
    for (int64 k = 0; k < vars_.size(); k++) {
      is_var_always_matched[k] =
          (var_match[k] != kUnassigned &&
           num_vars_in_component_[variable_component_[k]] == 0);
    }
  }

  void AllocateScc() {
    const int size = vars_.size();
    variable_component_.reset(NewIntArray(size));
    variable_dfs_.reset(NewIntArray(size));
    variable_high_.reset(NewIntArray(size));

    value_component_.reset(NewIntArray(num_values_));
    value_dfs_.reset(NewIntArray(num_values_));
    value_high_.reset(NewIntArray(num_values_));

    stack_.reset(NewIntArray(vars_.size() + num_values_ + 1));
    type_.reset(NewIntArray(vars_.size() + num_values_ + 1));

    is_var_always_matched_in_underflow_ .reset(new bool[vars_.size()]);
    is_var_always_matched_in_overflow_ .reset(new bool[vars_.size()]);

    for (int k = 0; k < size; ++k) {
      is_var_always_matched_in_underflow_[k] = false;
      is_var_always_matched_in_overflow_[k] = false;
    }
  }

  void InitScc() {
    for (int64 k = 0 ; k < vars_.size(); k++) {
      variable_component_[k] = 0;
      variable_dfs_[k] = 0;
      variable_high_[k] = 0;
    }
    for (int64 k = 0 ; k < num_values_; k++) {
      value_component_[k] = 0;
      value_dfs_[k] = 0;
      value_high_[k] = 0;
    }

    sink_component_ = 0;
    sink_dfs_ = 0;
    sink_high_ = 0;

    top_ = 0;
    dfs_ = vars_.size() + num_values_ + 1;
    component_ = 0;
  }

  void FindScc(FlowType ft) {
    InitScc();
    for (int64 k = 0; k < vars_.size(); k++) {
      if (variable_dfs_[k] == 0)
        FindSccVar(k, ft);
    }
  }

  void FindSccVar(int64 k, FlowType ft) {
    // First variable matched to the value.
    int64* var_match = (ft == UF) ?
        under_variable_match_.get() :
        over_variable_match_.get();

    variable_dfs_[k] = dfs_--;
    variable_high_[k] = variable_dfs_[k];
    stack_[top_] = k;
    type_[top_] = 0;
    top_++;
    assert(top_ <= vars_.size() + num_values_ + 1);

    int64 var_min = vars_[k]->Min();
    int64 var_max = vars_[k]->Max();
    if (vars_[k]->Size() == var_max - var_min + 1) {  // Dense domain.
      for (int64 w = var_min; w <= var_max; w++) {
        // Go to every values of the domain of x that is not matched by x.
        if (var_match[k] != w) {
          if (value_dfs_[w - min_value_] == 0) {
            FindSccValue(w, ft);
            if (value_high_[w - min_value_] > variable_high_[k]) {
              variable_high_[k] = value_high_[w - min_value_];
            }
          } else if ((value_dfs_[w - min_value_] > variable_dfs_[k]) &&
                     (value_component_[w - min_value_] == 0)) {
            if (value_dfs_[w - min_value_] > variable_high_[k]) {
              variable_high_[k] = value_dfs_[w - min_value_];
            }
          }
        }
      }
    } else {
      for (int64 w = var_min; w <= var_max; w++) {
        // Go to every values of the domain of x that is not matched by x.
        if (var_match[k] != w && vars_[k]->Contains(w)) {
          if (value_dfs_[w - min_value_] == 0) {
            FindSccValue(w, ft);
            if (value_high_[w - min_value_] > variable_high_[k]) {
              variable_high_[k] = value_high_[w - min_value_];
            }
          } else if ((value_dfs_[w - min_value_] > variable_dfs_[k]) &&
                     (value_component_[w - min_value_] == 0)) {
            if (value_dfs_[w - min_value_] > variable_high_[k]) {
              variable_high_[k] = value_dfs_[w - min_value_];
            }
          }
        }
      }
    }

    // If x is matched go also to every other variables that are not
    // matched (path from x->source->x').
    if (var_match[k] != kUnassigned) {
      for (int64 i = 0; i < vars_.size(); i++) {
        if (var_match[i] == kUnassigned) {
          if (variable_dfs_[i] == 0) {
            FindSccVar(i, ft);
            if (variable_high_[i] > variable_high_[k]) {
              variable_high_[k] = variable_high_[i];
            }
          } else if ((variable_dfs_[i] > variable_dfs_[k]) &&
                     (variable_component_[i] == 0)) {
            if (variable_dfs_[i] > variable_high_[k]) {
              variable_high_[k] = variable_dfs_[i];
            }
          }
        }
      }
    }

    if (variable_high_[k] == variable_dfs_[k]) {
      component_++;
      do {
        assert(top_ > 0);
        int64 i = stack_[--top_];
        int64 t = type_[top_];
        if (t == 0) {
          variable_component_[i] = component_;
        } else if (t == 1) {
          value_component_[i - min_value_] = component_;
        } else {
          sink_component_ = component_;
        }
        if (t == 0 && i == k) {
          break;
        }
      } while (true);
    }
  }

  void FindSccValue(int64 v, FlowType ft) {
    // First variable matched to the value.
    int64*  value_match = (ft == UF) ?
        under_value_match_.get() :
        over_value_match_.get();
    // First variable matched to the value.
    const int64* const capa = (ft == UF) ? card_mins_.data() : card_max_.data();
    // First variable matched to the value.
    int64* next = (ft == UF) ? under_next_match_.get() : over_next_match_.get();
    // First variable matched to the value.
    int64* flow = (ft == UF) ? underflow_.get() : overflow_.get();

    value_dfs_[v - min_value_] = dfs_--;
    value_high_[v - min_value_] = value_dfs_[v - min_value_];
    stack_[top_] = v;
    type_[top_] = 1;
    top_++;
    assert(top_ <= vars_.size() + num_values_ + 1);

    // First go to the variables assigned to this value.
    int64 k = value_match[v - min_value_];
    while (k != kUnassigned) {
      if (variable_dfs_[k] == 0) {
        FindSccVar(k, ft);
        if (variable_high_[k] > value_high_[v - min_value_]) {
          value_high_[v - min_value_] = variable_high_[k];
        }
      } else if ((variable_dfs_[k] > value_dfs_[v - min_value_]) &&
                 (variable_component_[k] == 0)) {
        if (variable_dfs_[k] > value_high_[v - min_value_]) {
          value_high_[v - min_value_] = variable_dfs_[k];
        }
      }
      k = next[k];
    }

    // Next try to see if you can go to the sink.
    if (flow[v - min_value_] < capa[v - min_value_]) {
      // go to the sink
      if (sink_dfs_ == 0) {
        FindSccSink(ft);
        if (sink_high_ > value_high_[v - min_value_]) {
          value_high_[v - min_value_] = sink_high_;
        }
      } else if ((sink_dfs_ > value_dfs_[v - min_value_]) &&
                (sink_component_ == 0)) {
        if (sink_dfs_ > value_high_[v - min_value_]) {
          value_high_[v - min_value_] = sink_dfs_;
        }
      }
    }

    if (value_high_[v - min_value_] == value_dfs_[v - min_value_]) {
      component_++;
      do {
        assert(top_ > 0);
        int64 i = stack_[--top_];
        int64 t = type_[top_];
        if (t == 0) {
          variable_component_[i] = component_;
        } else if (t == 1) {
          value_component_[i - min_value_] = component_;
        } else {
          sink_component_ = component_;
        }
        if (t == 1 && i == v) {
          break;
        }
      } while (true);
    }
  }

  void FindSccSink(FlowType ft) {
    // First variable matched to the value.
    int64* const var_match = (ft == UF) ?
        under_variable_match_.get() :
        over_variable_match_.get();
    // First variable matched to the value.
    int64* const flow = (ft == UF) ? underflow_.get() : overflow_.get();

    sink_dfs_ = dfs_--;
    sink_high_ = sink_dfs_;
    stack_[top_] = kUnassigned;
    type_[top_] = 2;
    top_++;
    assert(top_ <= vars_.size() + num_values_ + 1);

    // From the sink, I can go to the values if the flow in the value
    // is larger than the demand in these value.

    for (int64 i = 0; i < vars_.size(); i++) {
      int64 w = var_match[i];
      if (var_match[i] != kUnassigned) {
        if (flow[w - min_value_] > 0) {  // There is no minimum capacity.
          if (value_dfs_[w - min_value_] == 0) {
            FindSccValue(w, ft);
            if (value_high_[w - min_value_] > sink_high_) {
              sink_high_ = value_high_[w - min_value_];
            }
          } else if ((value_dfs_[w - min_value_] > sink_dfs_) &&
                    (value_component_[w - min_value_] == 0)) {
            if (value_dfs_[w - min_value_] > sink_high_) {
              sink_high_ = value_dfs_[w - min_value_];
            }
          }
        }
      }
    }

    // From the sink we can also go the variables that are not matched.

    for (int64 i = 0; i < vars_.size(); i++) {
      if (var_match[i] == kUnassigned) {
        if (variable_dfs_[i] == 0) {
          FindSccVar(i, ft);
          if (variable_high_[i] > sink_high_) {
            sink_high_ = variable_high_[i];
          }
        } else if ((variable_dfs_[i] > sink_dfs_) &&
                  (variable_component_[i] == 0)) {
          if (variable_dfs_[i] > sink_high_) {
            sink_high_ = variable_dfs_[i];
          }
        }
      }
    }

    if (sink_high_ == sink_dfs_) {
      component_++;
      do {
        assert(top_ > 0);
        int64 i = stack_[--top_];
        int64 t = type_[top_];
        if (t == 0) {
          variable_component_[i] = component_;
        } else if (t == 1) {
          value_component_[i - min_value_] = component_;
        } else {
          sink_component_ = component_;
        }
        if (t == 2) {
          break;
        }
      } while (true);
    }

  }

  void Prune(int64 min_violation) {
    if (min_violation < violation_var_->Max() - 1) {
      return;  // The constraint is GAC.
    }

    // We compute the SCC in Gu and Go and also if a variable is
    // matched in every maximum matching in Gu and Go.

    FindScc(UF);
    ComputeIsVarAlwaysMatched(UF);
    for (int64 v = 0; v < num_values_; v++) {
      under_value_component_[v] = value_component_[v];
    }
    for (int64 k = 0; k < vars_.size(); k++) {
      under_variable_component_[k] = variable_component_[k];
    }

    FindScc(OF);
    ComputeIsVarAlwaysMatched(OF);

    if (min_violation == violation_var_->Max() - 1) {
      for (int64 k = 0; k < vars_.size(); k++) {
        if (over_variable_match_[k] == kUnassigned) {
          continue;  // All values of this variable are consistent.
        }
        const int64 var_min = vars_[k]->Min();
        const int64 var_max = vars_[k]->Max();
        for (int64 w = var_min; w <= var_max; w++) {
          if (under_variable_match_[k] != w && over_variable_match_[k] != w) {
            if ((under_variable_component_[k] !=
                 under_value_component_[w - min_value_] &&
                 (card_mins_[w - min_value_] > 0 ||
                  is_var_always_matched_in_underflow_[k])) &&
                (variable_component_[k] != value_component_[w - min_value_] &&
                 (card_max_[w - min_value_] > 0 ||
                  is_var_always_matched_in_overflow_[k])) ) {
              vars_[k]->RemoveValue(w);
            }
          }
        }
      }
    } else if (min_violation == violation_var_->Max()) {
      // Under-flow filtering.
      for (int64 k = 0; k < vars_.size(); k++) {
        if (over_variable_match_[k] == kUnassigned) {
          continue;  // All values of this variable are consistent.
        }
        const int64 var_min = vars_[k]->Min();
        const int64 var_max = vars_[k]->Max();
        for (int64 w = var_min; w <= var_max; w++) {
          if (under_variable_match_[k] != w && over_variable_match_[k] != w) {
            if (under_variable_component_[k] !=
                under_value_component_[w - min_value_] &&
                (card_mins_[w - min_value_] > 0 ||
                 is_var_always_matched_in_underflow_[k])) {
              vars_[k]->RemoveValue(w);
            }
          }
        }
      }
      // Over-flow filtering.
      for (int64 k = 0; k < vars_.size(); k++) {
        if (over_variable_match_[k] == kUnassigned) {
          continue;  // All values of this variable are consistent.
        }
        const int64 var_min = vars_[k]->Min();
        const int64 var_max = vars_[k]->Max();
        for (int64 w = var_min; w <= var_max; w++) {
          if (over_variable_match_[k] != w) {
            if (variable_component_[k] != value_component_[w - min_value_] &&
                (card_max_[w - min_value_] > 0 ||
                 is_var_always_matched_in_overflow_[k])) {
              vars_[k]->RemoveValue(w);
            }
          }
        }
      }
    }
  }

  virtual std::string DebugString() const {
    return "SoftGCC";
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kGlobalCardinality, this);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->VisitIntegerArgument(ModelVisitor::kValueArgument, min_value_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kMinArgument, card_mins_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kMaxArgument, card_max_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            violation_var_);
    visitor->EndVisitConstraint(ModelVisitor::kGlobalCardinality, this);
  }

 private:
  int64* NewIntArray(int size) {
    int64* const result = new int64[size];
    memset(result, 0, size * sizeof(*result));
    return result;
  }

  std::vector<IntVar*> vars_;
  int64 min_value_;
  int64 max_value_;
  int64 num_values_;
  std::vector<int64> card_mins_;
  std::vector<int64> card_max_;
  IntVar* const violation_var_;
  int64 sum_card_min_;

  //for each value, the quantity of flow into this value
  std::unique_ptr<int64[]> underflow_;
 //for each variable, the value it is matched to
  std::unique_ptr<int64[]> under_variable_match_;
  //first variable matched to the value
  std::unique_ptr<int64[]> under_value_match_;
  //total flow
  int64 under_total_flow_;
  //next variable matched
  std::unique_ptr<int64[]> under_next_match_;
  //previous variable matched
  std::unique_ptr<int64[]> under_previous_match_;

  //for each value, the quantity of flow into this value
  std::unique_ptr<int64[]> overflow_;
  //for each variable, the value it is matched to
  std::unique_ptr<int64[]> over_variable_match_;
  //first variable matched to the value
  std::unique_ptr<int64[]> over_value_match_;
  //total flow
  int64 over_total_flow_;
  //next variable matched
  std::unique_ptr<int64[]> over_next_match_;
  //previous variable matched
  std::unique_ptr<int64[]> over_previous_match_;

  //flags for the dfs_ if the var nodes have been visited
  std::unique_ptr<int64[]> variable_seen_;
  //flags for the dfs_ if the val nodes have been visited
  std::unique_ptr<int64[]> value_seen_;
  //magic_ used for the flag in _variable_seen_ and _value_seen_
  int64 magic_;
  int64 dfs_;
  int64 component_;
  std::unique_ptr<int64[]> variable_component_;
  std::unique_ptr<int64[]> variable_dfs_;
  std::unique_ptr<int64[]> variable_high_;
  std::unique_ptr<int64[]> value_component_;
  std::unique_ptr<int64[]> value_dfs_;
  std::unique_ptr<int64[]> value_high_;
  int64 sink_component_;
  int64 sink_dfs_;
  int64 sink_high_;
  std::unique_ptr<bool[]> is_var_always_matched_in_underflow_;
  std::unique_ptr<bool[]> is_var_always_matched_in_overflow_;
  std::unique_ptr<int64[]> stack_;
  std::unique_ptr<int64[]> type_;
  int64 top_;
  std::vector<int64> num_vars_in_component_;
  std::unique_ptr<int64[]> under_variable_component_;
  std::unique_ptr<int64[]> under_value_component_;
};
}  // namespace


Constraint* MakeSoftGcc(Solver* const solver,
                        const std::vector<IntVar*>& vars,
                        int64 min_value,
                        const std::vector<int64>& card_mins,
                        const std::vector<int64>& card_max,
                        IntVar* const violation_var) {
  return solver->RevAlloc(new SoftGCC(solver,
                                      vars,
                                      min_value,
                                      card_mins,
                                      card_max,
                                      violation_var));
}

Constraint* MakeSoftGcc(Solver* const solver,
                        const std::vector<IntVar*>& vars,
                        int64 min_value,
                        const std::vector<int>& card_mins,
                        const std::vector<int>& card_max,
                        IntVar* const violation_var) {
  return solver->RevAlloc(new SoftGCC(solver,
                                      vars,
                                      min_value,
                                      ToInt64Vector(card_mins),
                                      ToInt64Vector(card_max),
                                      violation_var));
}
}  // namespace operations_research
