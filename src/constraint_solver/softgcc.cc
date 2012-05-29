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

#include "base/stringprintf.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/constraint_solver.h"
#include "util/const_ptr_array.h"
#include "util/string_array.h"
#include "util/vector_map.h"

namespace operations_research {
namespace {
class SoftGCC : public Constraint{
 public:
  static const int64 kUnassigned = kint64min;
  enum FlowType { UF, OF };

  // Constraint the values minval+i to appear between card_mins_i] and
  // card_max_[i] times in x but accept some violations to this rule.
  // For the value vi = minval+i, let ci be the number of occurences
  // in x and viol(vi) = max(0,card_mins_i]-ci,ci-card_max_[i])
  // i.e. viol(vi) is the shortage or excess wrt the prescribed
  // cardinalities.
  SoftGCC(Solver* const solver,
          const std::vector<IntVar*>& vars,
          int64 min_value,
          const int64* const card_mins,
          const int64* const card_max,
          int64 num_values,
          IntVar* const violation_var)
      : Constraint(solver),
        vars_(vars),
        min_value_(min_value),
        max_value_(min_value + num_values - 1),
        num_values_(num_values),
        card_mins_(new int64[num_values]),
        card_max_(new int64[num_values]),
        violation_var_(violation_var) {
    memcpy(card_mins_.get(), card_mins, num_values_ * sizeof(*card_mins));
    memcpy(card_max_.get(), card_max, num_values_ * sizeof(*card_max));
    for (int64 i = 0; i < num_values_; i++) {
      CHECK_GE(card_mins_[i], 0);
      CHECK_GE(card_max_[i], 0);
      CHECK_LE(card_mins_[i], card_max_[i]);
    }
  }

  SoftGCC(Solver* const solver,
          const std::vector<IntVar*>& vars,
          int64 min_value,
          const int* const card_mins,
          const int* const card_max,
          int64 num_values,
          IntVar* const violation_var)
      : Constraint(solver),
        vars_(vars),
        min_value_(min_value),
        max_value_(min_value + num_values - 1),
        num_values_(num_values),
        card_mins_(new int64[num_values]),
        card_max_(new int64[num_values]),
        violation_var_(violation_var) {
    for (int i = 0; i < num_values_; ++i) {
      card_mins_[i] = card_mins[i];
      card_max_[i] = card_max[i];
    }
    for (int64 i = 0; i < num_values_; i++) {
      CHECK_GE(card_mins_[i], 0);
      CHECK_GE(card_max_[i], 0);
      CHECK_LE(card_mins_[i], card_max_[i]);
    }
  }

  virtual void Post() {
    ComputeRangeOfValues();
    AllocateFlow();
    findInitialFlow();

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
          unassign(k,UF);
        }
      }
      if (over_variable_match_[k] != kUnassigned) {
        if (!vars_[k]->Contains(over_variable_match_[k])) {
          unassign(k,OF);
        }
      }
    }

    const int64 min_violation = getValueViolation();

    // Prune lower bound of violation.
    violation_var_->SetMin(min_violation);

    // Prune variable domains (the constraint is consistent at this point).
    prune(min_violation);

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
    for (int64 i = 0; i < vars_.size(); i++) {
      min_value_ = std::min(min_value_, vars_[i]->Min());
      max_value_ = std::max(max_value_, vars_[i]->Max());
    }
    const int64 delta = prev_min_value - min_value_;
    num_values_ = max_value_ - min_value_ + 1;

    // low
    int64* const new_card_mins = new int64[num_values_];

    // up
    int64* const new_card_max = new int64[num_values_];
    for (int64 k = 0; k < num_values_; k++) {
      new_card_max[k] = vars_.size();
    }

    sum_card_min_ = 0;

    for (int64 i = 0 ; i < num_values_; i++) {
      if (card_mins_[i] > 0) {
        new_card_mins[i + delta] = card_mins_[i] ;
        sum_card_min_ += card_mins_[i];
      }
    }

    for (int64 i = 0 ; i < num_values_; i++) {
      if (card_max_[i] < vars_.size()) {
        new_card_max[i + delta] = card_max_[i] ;
      }
    }
    card_mins_.reset(new_card_mins);
    card_max_.reset(new_card_max);
  }

  bool hasValInBestAssignment(int64 i) {
    if (i < 0 || i >= vars_.size()) {
      return false;
    }
    return (over_variable_match_[i] != kUnassigned);
  }

  int64 getValInBestAssignment(int64 i) {
    if (hasValInBestAssignment(i)) {
      return over_variable_match_[i];
    }
    else if (i < vars_.size() && i >= 0) {
      return vars_[i]->Min();
    }
    return kUnassigned;
  }

  int64 getReducedCost(int64 i,int64 v) {
    if (i >= vars_.size() || i < 0 || !vars_[i]->Contains(v)) {
      return kint64max;
    }

    findBestUnderFlow();
    findBestOverFlow();

    if (!hasValInBestAssignment(i) || over_variable_match_[i]==v) {
      return 0;
    }

    int64 reducedCost = 0;

    findSCC(UF);
    computeIsVarAlwaysMatched(UF);
    if (variable_component_[i] != value_component_[v-min_value_] &&
        (card_mins_[v-min_value_]>0 || is_var_always_matched_in_underflow_[i])) {
      reducedCost += 1;
    }

    findSCC(OF);
    computeIsVarAlwaysMatched(OF);
    if (variable_component_[i] != value_component_[v-min_value_] &&
       (card_max_[v-min_value_]>0 || is_var_always_matched_in_overflow_[i])) {
      reducedCost += 1;
    }

    return reducedCost;
  }


  void AllocateFlow() {
    // flow
    underflow_.reset(new int64[num_values_]);
    overflow_.reset(new int64[num_values_]);

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

    // flag
    variable_seen_.reset(new int64[vars_.size()]);

    // flag
    value_seen_.reset(new int64[num_values_]);

    magic_ = 0;
  }

  //assigns value v to variable k and update structures: sizeFlow,
  //flow, varMatch, prev, next, valMatch
  void assign(int64 k,int64 v, FlowType ft) {
    int64* flow;     //for each value, the quantity of flow into this value
    int64* varMatch; //for each variable, the value it is matched to
    int64* next;     //next variable matched
    int64* prev;     //previous variable matched
    int64* valMatch; //first variable matched to the value

    if (ft == UF) {
      flow = underflow_.get();
      varMatch = under_variable_match_.get();
      next = under_next_match_.get();
      prev = under_previous_match_.get();
      valMatch = under_value_match_.get();
      under_total_flow_++;
    }else{ //OF
      flow = overflow_.get();
      varMatch = over_variable_match_.get();
      next = over_next_match_.get();
      prev = over_previous_match_.get();
      valMatch = over_value_match_.get();
      over_total_flow_++;
    }

    unassign(k,ft);

    // k is now first on the list of v
    varMatch[k] = v;
    flow[v-min_value_]++;
    int64 nk = valMatch[v-min_value_];
    next[k] = nk;
    prev[k] = kUnassigned;
    if (nk != kUnassigned)
      prev[nk] = k;
    valMatch[v-min_value_] = k;
  }

  //unassings variable k and updates appropriately the structures: sizeFlow, flow, varMatch, prev, next, valMatch
  void unassign(int64 k, FlowType ft) {
    int64* flow;     //for each value, the quantity of flow into this value
    int64* varMatch; //for each variable, the value it is matched to
    int64* next;     //next variable matched
    int64* prev;     //previous variable matched
    int64* valMatch; //first variable matched to the value

    if (ft == UF) {
      flow = underflow_.get();
      varMatch = under_variable_match_.get();
      next = under_next_match_.get();
      prev = under_previous_match_.get();
      valMatch = under_value_match_.get();
    }else{ // OF
      flow = overflow_.get();
      varMatch = over_variable_match_.get();
      next = over_next_match_.get();
      prev = over_previous_match_.get();
      valMatch = over_value_match_.get();
    }

    if (varMatch[k] != kUnassigned) { // this guy is assigned; must be removed
      if (ft == UF) under_total_flow_--;
      else over_total_flow_--;

      int64 w = varMatch[k];
      flow[w-min_value_]--;
      if (valMatch[w-min_value_] == k) { // first in the list
        int64 nk = next[k];
        valMatch[w-min_value_] = nk;
        if (nk != kUnassigned)
          prev[nk] = kUnassigned; // nk is now first
      }
      else { // not first
        int64 pk = prev[k];
        int64 nk = next[k];
        next[pk] = nk;
        if (nk != kUnassigned)
          prev[nk] = pk;
      }
      varMatch[k] = kUnassigned;
    }
  }

  //finds a initial flow for both the underflow and overflow
  void findInitialFlow() {
    under_total_flow_ = 0;
    over_total_flow_ = 0;
    for (int64 k = 0; k < vars_.size(); k++) {
      int64 var_min = vars_[k]->Min();
      int64 var_max = vars_[k]->Max();
      for (int64 i = var_min; i <= var_max; i++) {
        if (underflow_[i - min_value_] < card_mins_[i - min_value_])
          if (vars_[k]->Contains(i)) {
            assign(k,i,UF);
            break;
          }
      }
      for (int64 i = var_min; i <= var_max; i++) {
        if (overflow_[i - min_value_] < card_max_[i - min_value_])
          if (vars_[k]->Contains(i)) {
            assign(k,i,OF);
            break;
          }
      }
    }
  }

  int64 getValueViolation() {
    int64 buf = findBestUnderFlow();
    int64 bof = findBestOverFlow();
    return buf+bof;
  }

  //computes and returns the best under flow
  int64 findBestUnderFlow() {
    for (int64 k = 0;
         k < vars_.size() && under_total_flow_ < vars_.size();
         k++) {
      if (under_variable_match_[k] == kUnassigned) {
        magic_++;
        findAugmentingPath(k,UF);
      }
    }
    return sum_card_min_ - under_total_flow_;
  }

  //computes and returns the best over flow
  int64 findBestOverFlow() {
    //in order to have the best overflow AND underflow, I start from
    //the best under flow to compute the best overflow (very important
    //for the methods hasValInBestAssignment/getValInBestAssignment)

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

    for (int64 k = 0; k < vars_.size() && over_total_flow_ < vars_.size(); k++) {
      if (over_variable_match_[k] == kUnassigned) {
        magic_++;
        findAugmentingPath(k,OF);
      }
    }
    return vars_.size() - over_total_flow_;
  }

  //finds augmenting path from variable i which is currently not matched (true if found a path, false otherwise)
  bool findAugmentingPath(int64 k, FlowType ft) {
    int64*  varMatch; //for each variable, the value it is matched to
    if (ft == UF)
      varMatch = under_variable_match_.get();
    else
      varMatch = over_variable_match_.get();

    if (variable_seen_[k] != magic_) {
      variable_seen_[k] = magic_;
      int64 var_min = vars_[k]->Min();
      int64 var_max = vars_[k]->Max();
      for (int64 v = var_min; v <= var_max; v++) {
        if (varMatch[k] != v) {
          if (vars_[k]->Contains(v)) {
            if (findAugmentingPathValue(v,ft)) {
              assign(k,v,ft);
              return true;
            }
          }
        }
      }
    }
    return false;
  }

  bool findAugmentingPathValue(int64 v, FlowType ft) {

    int64* flow;     //for each value, the quantity of flow into this value
    int64* next;     //next variable matched
    int64* valMatch; //first variable matched to the value
    const int64* capa;     //capacity (low for ft==UF, up for ft==OF)

    if (ft == UF) {
      flow = underflow_.get();
      next = under_next_match_.get();
      valMatch = under_value_match_.get();
      capa = card_mins_.get();
    }else{ //OF
      flow = overflow_.get();
      next = over_next_match_.get();
      valMatch = over_value_match_.get();
      capa = card_max_.get();
    }

    if (value_seen_[v-min_value_] != magic_) {
      value_seen_[v-min_value_] = magic_;
      if (flow[v-min_value_] < capa[v-min_value_])
        return true;
      else if (flow[v-min_value_] > 0) {
        int64 i = valMatch[v-min_value_];
        while (i != kUnassigned) {
          if (findAugmentingPath(i,ft))
            return true;
          i = next[i];
        }
      }
    }
    return false;
  }

  void computeIsVarAlwaysMatched(FlowType ft) {
    bool* isVarAlwaysMatched;
    int64* varMatch;

    if (ft == UF) {
      isVarAlwaysMatched = is_var_always_matched_in_underflow_.get();
      varMatch = under_variable_match_.get();
    }else{
      isVarAlwaysMatched = is_var_always_matched_in_overflow_.get();
      varMatch = over_variable_match_.get();
    }


    scoped_array<int64> nbVarInComponent_(new int64[component_ + 1]);
    for (int64 k = 0; k < vars_.size(); k++) {
      if (varMatch[k] == kUnassigned ) {
        nbVarInComponent_[variable_component_[k]]++;
      }
    }
    for (int64 k = 0; k < vars_.size(); k++) {
      isVarAlwaysMatched[k] = false;
      if (varMatch[k] != kUnassigned &&
          nbVarInComponent_[variable_component_[k]] == 0) {
        isVarAlwaysMatched[k] = true;
      }
    }
  }

  void AllocateScc() {
    variable_component_.reset(new int64[vars_.size()]);
    variable_dfs_.reset(new int64[vars_.size()]);
    variable_high_.reset(new int64 [vars_.size()]);

    value_component_.reset(new int64 [num_values_]);
    value_dfs_.reset(new int64[num_values_]);
    value_high_.reset(new int64[num_values_]);

    stack_.reset(new int64 [vars_.size()+num_values_+1]);
    type_.reset(new int64 [vars_.size()+num_values_+1]);

    is_var_always_matched_in_underflow_ .reset(new bool[vars_.size()]);
    is_var_always_matched_in_overflow_ .reset(new bool[vars_.size()]);
  }

  void initSCC() {
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

  void findSCC(FlowType ft) {
    initSCC();
    for (int64 k = 0; k < vars_.size(); k++) {
      if (variable_dfs_[k] == 0)
        findSCCvar(k,ft);
    }
  }

  void findSCCvar(int64 k, FlowType ft) {
    int64* varMatch = (ft == UF) ? under_variable_match_.get() : over_variable_match_.get(); //first variable matched to the value

    variable_dfs_[k] = dfs_--;
    variable_high_[k] = variable_dfs_[k];
    stack_[top_] = k;
    type_[top_] = 0;
    top_++;
    assert(top_ <= vars_.size() + num_values_ + 1);


    int64 var_min = vars_[k]->Min();
    int64 var_max = vars_[k]->Max();
    for (int64 w = var_min; w <= var_max; w++) {
      //go to every values of the domain of x that is not matched by x
      if (varMatch[k] != w) {
        if (vars_[k]->Contains(w)) {
          if (value_dfs_[w-min_value_] == 0) {
            findSCCval(w,ft);
            if (value_high_[w-min_value_] > variable_high_[k])
              variable_high_[k] = value_high_[w-min_value_];
          }
          else if ( (value_dfs_[w-min_value_] > variable_dfs_[k]) && (value_component_[w-min_value_] == 0)) {
            if (value_dfs_[w-min_value_] > variable_high_[k])
              variable_high_[k] = value_dfs_[w-min_value_];
          }
        }
      }
    }

    //if x is matched go also to every other variables that are not
    //matched (path from x->source->x')

    if (varMatch[k] != kUnassigned) {
      for (int64 i = 0; i < vars_.size(); i++) {
        if (varMatch[i] == kUnassigned) {
          if (variable_dfs_[i] == 0) {
            findSCCvar(i,ft);
            if (variable_high_[i] > variable_high_[k])
              variable_high_[k] = variable_high_[i];
          }
          else if ( (variable_dfs_[i] > variable_dfs_[k]) && (variable_component_[i] == 0)) {
            if (variable_dfs_[i] > variable_high_[k])
              variable_high_[k] = variable_dfs_[i];
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
        if (t == 0)
          variable_component_[i] = component_;
        else if (t == 1)
          value_component_[i - min_value_] = component_;
        else
          sink_component_ = component_;
        if (t == 0 && i == k)
          break;
      } while (true);
    }
  }

  void findSCCval(int64 v, FlowType ft) {
    //first variable matched to the valeu
    int64*  valMatch = (ft == UF) ?
        under_value_match_.get() :
        over_value_match_.get();
    //first variable matched to the value
    const int64* const capa = (ft == UF) ? card_mins_.get() : card_max_.get();
    //first variable matched to the value
    int64* next = (ft == UF) ? under_next_match_.get() : over_next_match_.get();
    //first variable matched to the value
    int64* flow = (ft == UF) ? underflow_.get() : overflow_.get();

    value_dfs_[v-min_value_] = dfs_--;
    value_high_[v-min_value_] = value_dfs_[v-min_value_];
    stack_[top_] = v;
    type_[top_] = 1;
    top_++;
    assert(top_ <= vars_.size() + num_values_ + 1);

    // first go to the variables assigned to this value
    int64 k = valMatch[v-min_value_];
    while (k != kUnassigned) {
      if (variable_dfs_[k] == 0) {
        findSCCvar(k,ft);
        if (variable_high_[k] > value_high_[v-min_value_])
          value_high_[v-min_value_] = variable_high_[k];
      }
      else if ( (variable_dfs_[k] > value_dfs_[v-min_value_]) && (variable_component_[k] == 0)) {
        if (variable_dfs_[k] > value_high_[v-min_value_])
          value_high_[v-min_value_] = variable_dfs_[k];
      }
      k = next[k];
    }

    // next try to see if you can go to the sink

    if (flow[v-min_value_] < capa[v-min_value_]) {
      // go to the sink
      if (sink_dfs_ == 0) {
        findSCCsink(ft);
        if (sink_high_ > value_high_[v-min_value_])
          value_high_[v-min_value_] = sink_high_;
      }
      else if ( (sink_dfs_ > value_dfs_[v-min_value_]) &&
                (sink_component_ == 0)) {
        if (sink_dfs_ > value_high_[v-min_value_])
          value_high_[v-min_value_] = sink_dfs_;
      }
    }

    if (value_high_[v-min_value_] == value_dfs_[v-min_value_]) {
      component_++;
      do {
        assert(top_ > 0);
        int64 i = stack_[--top_];
        int64 t = type_[top_];
        if (t == 0)
          variable_component_[i] = component_;
        else if (t == 1)
          value_component_[i - min_value_] = component_;
        else
          sink_component_ = component_;
        if (t == 1 && i == v)
          break;
      } while (true);
    }
  }

  void findSCCsink(FlowType ft) {
    //first variable matched to the value
    int64* const varMatch = (ft == UF) ?
        under_variable_match_.get() :
        over_variable_match_.get();
    //first variable matched to the value
    int64* const flow = (ft == UF) ? underflow_.get() : overflow_.get();

    sink_dfs_ = dfs_--;
    sink_high_ = sink_dfs_;
    stack_[top_] = kUnassigned;
    type_[top_] = 2;
    top_++;
    assert(top_ <= vars_.size() + num_values_ + 1);

    //from the sink, I can go to the values if the flow in the value
    //is larger than the demand in these value

    for (int64 i = 0; i < vars_.size(); i++) {
      int64 w = varMatch[i];
      if (varMatch[i] != kUnassigned) {
        if (flow[w-min_value_] > 0) { //there is no minimum capacity
          if (value_dfs_[w-min_value_] == 0) {
            findSCCval(w,ft);
            if (value_high_[w-min_value_] > sink_high_)
              sink_high_ = value_high_[w-min_value_];
          }
          else if ( (value_dfs_[w-min_value_] > sink_dfs_) &&
                    (value_component_[w-min_value_] == 0)) {
            if (value_dfs_[w-min_value_] > sink_high_)
              sink_high_ = value_dfs_[w-min_value_];
          }
        }
      }
    }

    //from the sink we can also go the variables that are not matched

    for (int64 i = 0; i < vars_.size(); i++) {
      if (varMatch[i] == kUnassigned) {
        if (variable_dfs_[i] == 0) {
          findSCCvar(i,ft);
          if (variable_high_[i] > sink_high_)
            sink_high_ = variable_high_[i];
        }
        else if ( (variable_dfs_[i] > sink_dfs_) &&
                  (variable_component_[i] == 0)) {
          if (variable_dfs_[i] > sink_high_)
            sink_high_ = variable_dfs_[i];
        }
      }
    }

    if (sink_high_ == sink_dfs_) {
      component_++;
      do {
        assert(top_ > 0);
        int64 i = stack_[--top_];
        int64 t = type_[top_];
        if (t == 0)
          variable_component_[i] = component_;
        else if (t == 1)
          value_component_[i - min_value_] = component_;
        else
          sink_component_ = component_;
        if (t == 2)
          break;
      } while (true);
    }

  }

  void prune(int64 min_violation) {
    if (min_violation < violation_var_->Max() - 1) {
      return; //the constraint is GAC
    }

    //we compute the SCC in Gu and Go and also if a variable is
    //matched in every maximum matching in Gu and Go

    //source of inefficiency to create the table (memory)?
    scoped_array<int64> variable_component_uf(new int64[vars_.size()]);
    scoped_array<int64> value_component_uf(new int64[num_values_]);

    findSCC(UF);
    computeIsVarAlwaysMatched(UF);
    for (int64 v = 0; v<num_values_; v++) {
      value_component_uf[v] = value_component_[v];
    }
    for (int64 k = 0; k < vars_.size(); k++) {
      variable_component_uf[k] = variable_component_[k];
    }

    findSCC(OF);
    computeIsVarAlwaysMatched(OF);

    if (min_violation == violation_var_->Max()-1) {
      for (int64 k = 0; k < vars_.size(); k++) {
        if ( over_variable_match_[k] == kUnassigned) continue;//all values of this variable are consistent
        int64 var_min = vars_[k]->Min();
        int64 var_max = vars_[k]->Max();
        for (int64 w = var_min; w <= var_max; w++) {
          if (vars_[k]->Contains(w)) {
            if (under_variable_match_[k] != w && over_variable_match_[k] != w) {
              if ((variable_component_uf[k] != value_component_uf[w-min_value_] &&
                   (card_mins_[w-min_value_]>0 || is_var_always_matched_in_underflow_[k])) &&
                  (variable_component_[k] != value_component_[w-min_value_] &&
                   (card_max_[w-min_value_]>0 || is_var_always_matched_in_overflow_[k])) ) {
                vars_[k]->RemoveValue(w);
              }
            }
          }
        }
      }
    }
    else if (min_violation == violation_var_->Max()) {
      //under-flow filtering
      for (int64 k = 0; k < vars_.size(); k++) {
        if ( over_variable_match_[k] == kUnassigned) {
          continue;  //all values of this variable are consistent
        }
        int64 var_min = vars_[k]->Min();
        int64 var_max = vars_[k]->Max();
        for (int64 w = var_min; w <= var_max; w++) {
          if (vars_[k]->Contains(w)) {
            if (under_variable_match_[k] != w && over_variable_match_[k] != w) {
              if (variable_component_uf[k] != value_component_uf[w-min_value_] &&
                  (card_mins_[w-min_value_]>0 || is_var_always_matched_in_underflow_[k])) {
                vars_[k]->RemoveValue(w);
              }
            }
          }
        }
      }
      //over-flow filtering
      //under-flow filtering
      for (int64 k = 0; k < vars_.size(); k++) {
        if ( over_variable_match_[k] == kUnassigned) {
          continue;  //all values of this variable are consistent
        }
        const int64 var_min = vars_[k]->Min();
        const int64 var_max = vars_[k]->Max();
        for (int64 w = var_min; w <= var_max; w++) {
          if (vars_[k]->Contains(w)) {
            if (over_variable_match_[k] != w) {
              if (variable_component_[k] != value_component_[w-min_value_] &&
                  (card_max_[w-min_value_]>0 || is_var_always_matched_in_overflow_[k])) {
                vars_[k]->RemoveValue(w);
              }
            }
          }
        }
      }
    }
  }

 private:
  std::vector<IntVar*> vars_;
  int64 min_value_;
  int64 max_value_;
  int64 num_values_;
  scoped_array<int64> card_mins_;
  scoped_array<int64> card_max_;
  IntVar* const violation_var_;
  int64 sum_card_min_;

  //for each value, the quantity of flow into this value
  scoped_array<int64> underflow_;
 //for each variable, the value it is matched to
  scoped_array<int64> under_variable_match_;
  //first variable matched to the value
  scoped_array<int64> under_value_match_;
  //total flow
  int64 under_total_flow_;
  //next variable matched
  scoped_array<int64> under_next_match_;
  //previous variable matched
  scoped_array<int64> under_previous_match_;

  //for each value, the quantity of flow into this value

  scoped_array<int64> overflow_;
  //for each variable, the value it is matched to
  scoped_array<int64> over_variable_match_;
  //first variable matched to the value
  scoped_array<int64> over_value_match_;
  //total flow
  int64 over_total_flow_;
  //next variable matched
  scoped_array<int64> over_next_match_;
  //previous variable matched
  scoped_array<int64> over_previous_match_;

  //flags for the dfs_ if the var nodes have been visited
  scoped_array<int64> variable_seen_;
  //flags for the dfs_ if the val nodes have been visited
  scoped_array<int64> value_seen_;
  //magic_ used for the flag in _variable_seen_ and _value_seen_
  int64 magic_;
  int64 dfs_;
  int64 component_;
  scoped_array<int64> variable_component_;
  scoped_array<int64> variable_dfs_;
  scoped_array<int64> variable_high_;
  scoped_array<int64> value_component_;
  scoped_array<int64> value_dfs_;
  scoped_array<int64> value_high_;
  int64 sink_component_;
  int64 sink_dfs_;
  int64 sink_high_;
  scoped_array<bool> is_var_always_matched_in_underflow_;
  scoped_array<bool> is_var_always_matched_in_overflow_;
  scoped_array<int64> stack_;
  scoped_array<int64> type_;
  int64 top_;
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
                                      card_mins.data(),
                                      card_max.data(),
                                      card_mins.size(),
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
                                      card_mins.data(),
                                      card_max.data(),
                                      card_mins.size(),
                                      violation_var));
}
}  // namespace operations_research
