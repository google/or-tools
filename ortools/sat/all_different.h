// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_ALL_DIFFERENT_H_
#define OR_TOOLS_SAT_ALL_DIFFERENT_H_

#include <functional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Enforces that the given tuple of variables takes different values. This fully
// encodes all the variables and simply enforces a <= 1 constraint on each
// possible values.
std::function<void(Model*)> AllDifferentBinary(
    const std::vector<IntegerVariable>& vars);

// Enforces that the given tuple of variables takes different values.
// Same as AllDifferentBinary() but use a different propagator that only enforce
// the so called "bound consistency" on the variable domains.
//
// Compared to AllDifferentBinary() this doesn't require fully encoding the
// variables and it is also quite fast. Note that the propagation is different,
// this will not remove already taken values from inside a domain, but it will
// propagates more the domain bounds.
std::function<void(Model*)> AllDifferentOnBounds(
    const std::vector<IntegerVariable>& vars);

// This constraint forces all variables to take different values. This is meant
// to be used as a complement to an alldifferent decomposition like
// AllDifferentBinary(): DO NOT USE WITHOUT ONE. Doing the filtering that the
// decomposition can do with an appropriate algorithm should be cheaper and
// yield more accurate explanations.
//
// It uses the matching algorithm described in Regin at AAAI1994:
// "A filtering algorithm for constraints of difference in CSPs".
//
// This will fully encode variables.
std::function<void(Model*)> AllDifferentAC(
    const std::vector<IntegerVariable>& variables);

// Implementation of AllDifferentAC().
class AllDifferentConstraint : PropagatorInterface {
 public:
  AllDifferentConstraint(std::vector<IntegerVariable> variables,
                         IntegerEncoder* encoder, Trail* trail,
                         IntegerTrail* integer_trail);

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // MakeAugmentingPath() is a step in Ford-Fulkerson's augmenting path
  // algorithm. It changes its current internal state (see vectors below)
  // to assign a value to the start vertex using an augmenting path.
  // If it is not possible, it keeps variable_to_value_[start] to -1 and returns
  // false, otherwise it modifies the current assignment and returns true.
  // It uses value/variable_visited to mark the nodes it visits during its
  // search: one can use this information to generate an explanation of failure,
  // or manipulate it to create what-if scenarios without modifying successor_.
  bool MakeAugmentingPath(int start);

  // Accessors to the cache of literals.
  inline LiteralIndex VariableLiteralIndexOf(int x, int64 value);
  inline bool VariableHasPossibleValue(int x, int64 value);

  // This caches all literals of the fully encoded variables.
  // Values of a given variable are 0-indexed using offsets variable_min_value_,
  // the set of all values is globally offset using offset min_all_values_.
  // TODO(user): compare this encoding to a sparser hash_map encoding.
  const int num_variables_;
  const std::vector<IntegerVariable> variables_;
  int64 min_all_values_;
  int64 num_all_values_;
  std::vector<int64> variable_min_value_;
  std::vector<int64> variable_max_value_;
  std::vector<std::vector<LiteralIndex>> variable_literal_index_;

  // Internal state of MakeAugmentingPath().
  // value_to_variable_ and variable_to_value_ represent the current assignment;
  // -1 means not assigned. Otherwise,
  // variable_to_value_[var] = value <=> value_to_variable_[value] = var.
  std::vector<std::vector<int>> successor_;
  std::vector<bool> value_visited_;
  std::vector<bool> variable_visited_;
  std::vector<int> value_to_variable_;
  std::vector<int> variable_to_value_;
  std::vector<int> prev_matching_;
  std::vector<int> visiting_;
  std::vector<int> variable_visited_from_;

  // Internal state of ComputeSCCs().
  // Variable nodes are indexed by [0, num_variables_),
  // value nodes by [num_variables_, num_variables_ + num_all_values_),
  // and a dummy node with index num_variables_ + num_all_values_ is added.
  // The graph passed to ComputeSCCs() is the residual of the possible graph
  // by the current matching, i.e. its arcs are:
  // _ (var, val) if val \in dom(var) and var not matched to val,
  // _ (val, var) if var matched to val,
  // _ (val, dummy) if val not matched to any variable,
  // _ (dummy, var) for all variables.
  // In the original paper, forbidden arcs are identified by detecting that they
  // are not in any alternating cycle or alternating path starting at a
  // free vertex. Adding the dummy node allows to factor the alternating path
  // part in the alternating cycle, and filter with only the SCC decomposition.
  // When num_variables_ == num_all_values_, the dummy node is useless,
  // we add it anyway to simplify the code.
  std::vector<std::vector<int>> residual_graph_successors_;
  std::vector<int> component_number_;

  Trail* trail_;
  IntegerTrail* integer_trail_;
};

// Implement the all different bound consistent propagator with explanation.
// That is, given n variables that must be all different, this propagates the
// bounds of each variables as much as possible. The key is to detect the so
// called Hall interval which are interval of size k that contains the domain
// of k variables. Because all the variables must take different values, we can
// deduce that the domain of the other variables cannot contains such Hall
// interval.
//
// We use a "fast" O(n log n) algorithm.
//
// TODO(user): It might be difficult to find something faster than what is
// implemented here. Some related reference:
// https://cs.uwaterloo.ca/~vanbeek/Publications/ijcai03_TR.pdf
class AllDifferentBoundsPropagator : public PropagatorInterface {
 public:
  AllDifferentBoundsPropagator(const std::vector<IntegerVariable>& vars,
                               IntegerTrail* integer_trail);

  bool Propagate() final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  // We locally cache the lb/ub for faster sorting and to guarantee some
  // invariant when we push bounds.
  struct VarValue {
    IntegerVariable var;
    IntegerValue lb;
    IntegerValue ub;
  };

  // Fills integer_reason_ with the reason why we have the given hall interval.
  void FillHallReason(IntegerValue hall_lb, IntegerValue hall_ub);

  // Do half the job of Propagate(). This will split the variable into
  // independent subset, and call PropagateLowerBoundsInternal() on each of
  // them.
  bool PropagateLowerBounds();
  bool PropagateLowerBoundsInternal(IntegerValue min_lb,
                                    absl::Span<VarValue> vars);

  // Internally, we will maintain a set of non-consecutive integer intervals of
  // the form [start, end]. Each point (i.e. IntegerValue) of such interval will
  // be associated to an unique variable and via an union-find algorithm point
  // to its start. The end only make sense for representative.
  //
  // TODO(user): Because we don't use rank, we have a worst case complexity of
  // O(n log n). We could try a normal Union-find data structure, but then we
  // also have to maintain a start vector.
  //
  // Note that during the execution of the algorithm we start from empty
  // intervals and finish with a set of points of size num_vars.
  //
  // The list of all points are maintained in the dense vectors index_to_*_
  // where we have remapped values to indices (with GetIndex()) to make sure it
  // always fall into the correct range.
  int FindStartIndexAndCompressPath(int index);

  int GetIndex(IntegerValue value) const {
    DCHECK_GE(value, base_);
    DCHECK_LT(value - base_, index_to_start_index_.size());
    return (value - base_).value();
  }

  IntegerValue GetValue(int index) const { return base_ + IntegerValue(index); }

  bool PointIsPresent(int index) const {
    return index_to_var_[index] != kNoIntegerVariable;
  }

  IntegerTrail* integer_trail_;

  // These vector will be either sorted by lb or by ub.
  std::vector<VarValue> vars_;
  std::vector<VarValue> negated_vars_;

  // The list of Hall intervalls detected so far, sorted.
  std::vector<IntegerValue> hall_starts_;
  std::vector<IntegerValue> hall_ends_;

  // Non-consecutive intervals related data-structures.
  IntegerValue base_;
  std::vector<int> indices_to_clear_;
  std::vector<int> index_to_start_index_;
  std::vector<int> index_to_end_index_;
  std::vector<IntegerVariable> index_to_var_;

  // Temporary integer reason.
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(AllDifferentBoundsPropagator);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_ALL_DIFFERENT_H_
