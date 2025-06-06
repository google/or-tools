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

#include "ortools/sat/all_different.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sort.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

std::function<void(Model*)> AllDifferentBinary(
    absl::Span<const IntegerVariable> vars) {
  return [=, vars = std::vector<IntegerVariable>(vars.begin(), vars.end())](
             Model* model) {
    // Fully encode all the given variables and construct a mapping value ->
    // List of literal each indicating that a given variable takes this value.
    //
    // Note that we use a map to always add the constraints in the same order.
    absl::btree_map<IntegerValue, std::vector<Literal>> value_to_literals;
    IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
    for (const IntegerVariable var : vars) {
      model->Add(FullyEncodeVariable(var));
      for (const auto& entry : encoder->FullDomainEncoding(var)) {
        value_to_literals[entry.value].push_back(entry.literal);
      }
    }

    // Add an at most one constraint for each value.
    for (const auto& entry : value_to_literals) {
      if (entry.second.size() > 1) {
        model->Add(AtMostOneConstraint(entry.second));
      }
    }

    // If the number of values is equal to the number of variables, we have
    // a permutation. We can add a bool_or for each literals attached to a
    // value.
    if (value_to_literals.size() == vars.size()) {
      for (const auto& entry : value_to_literals) {
        model->Add(ClauseConstraint(entry.second));
      }
    }
  };
}

std::function<void(Model*)> AllDifferentOnBounds(
    absl::Span<const AffineExpression> expressions) {
  return [=, expressions = std::vector<AffineExpression>(
                 expressions.begin(), expressions.end())](Model* model) {
    if (expressions.empty()) return;
    auto* constraint = new AllDifferentBoundsPropagator(
        expressions, model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

std::function<void(Model*)> AllDifferentOnBounds(
    absl::Span<const IntegerVariable> vars) {
  return [=, vars = std::vector<IntegerVariable>(vars.begin(), vars.end())](
             Model* model) {
    if (vars.empty()) return;
    std::vector<AffineExpression> expressions;
    expressions.reserve(vars.size());
    for (const IntegerVariable var : vars) {
      expressions.push_back(AffineExpression(var));
    }
    auto* constraint = new AllDifferentBoundsPropagator(
        expressions, model->GetOrCreate<IntegerTrail>());
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

std::function<void(Model*)> AllDifferentAC(
    absl::Span<const IntegerVariable> variables) {
  return [variables](Model* model) {
    if (variables.size() < 3) return;

    AllDifferentConstraint* constraint =
        new AllDifferentConstraint(variables, model);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

AllDifferentConstraint::AllDifferentConstraint(
    absl::Span<const IntegerVariable> variables, Model* model)
    : num_variables_(variables.size()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  // Initialize literals cache.
  // Note that remap all values appearing here with a dense_index.
  num_values_ = 0;
  absl::flat_hash_map<IntegerValue, int> dense_indexing;
  variable_to_possible_values_.resize(num_variables_);
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  for (int x = 0; x < num_variables_; x++) {
    const IntegerValue lb = integer_trail_->LowerBound(variables[x]);
    const IntegerValue ub = integer_trail_->UpperBound(variables[x]);

    // FullyEncode does not like 1-value domains, handle this case first.
    // TODO(user): Prune now, ignore these variables during solving.
    if (lb == ub) {
      const auto [it, inserted] = dense_indexing.insert({lb, num_values_});
      if (inserted) ++num_values_;

      variable_to_possible_values_[x].push_back(
          {it->second, encoder->GetTrueLiteral()});
      continue;
    }

    // Force full encoding if not already done.
    if (!encoder->VariableIsFullyEncoded(variables[x])) {
      encoder->FullyEncodeVariable(variables[x]);
    }

    // Fill cache with literals, default value is kFalseLiteralIndex.
    for (const auto [value, lit] : encoder->FullDomainEncoding(variables[x])) {
      const auto [it, inserted] = dense_indexing.insert({value, num_values_});
      if (inserted) ++num_values_;

      variable_to_possible_values_[x].push_back({it->second, lit});
    }

    // Not sure it is needed, but lets sort.
    absl::c_sort(
        variable_to_possible_values_[x],
        [](const std::pair<int, Literal>& a, const std::pair<int, Literal>& b) {
          return a.first < b.first;
        });
  }

  variable_to_value_.assign(num_variables_, -1);
  visiting_.resize(num_variables_);
  variable_visited_from_.resize(num_variables_);
  component_number_.resize(num_variables_ + num_values_ + 1);
}

AllDifferentConstraint::AllDifferentConstraint(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals, Model* model)
    : num_variables_(num_nodes),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {
  num_values_ = num_nodes;

  // We assume everything is already dense.
  const int num_arcs = tails.size();
  variable_to_possible_values_.resize(num_variables_);
  for (int a = 0; a < num_arcs; ++a) {
    variable_to_possible_values_[tails[a]].push_back({heads[a], literals[a]});
  }

  variable_to_value_.assign(num_variables_, -1);
  visiting_.resize(num_variables_);
  variable_visited_from_.resize(num_variables_);
  component_number_.resize(num_variables_ + num_values_ + 1);
}

void AllDifferentConstraint::RegisterWith(GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  watcher->SetPropagatorPriority(id, 2);
  for (int x = 0; x < num_variables_; x++) {
    for (const auto [_, lit] : variable_to_possible_values_[x]) {
      // Watch only unbound literals.
      if (!trail_->Assignment().LiteralIsAssigned(lit)) {
        watcher->WatchLiteral(lit, id);
        watcher->WatchLiteral(lit.Negated(), id);
      }
    }
  }
}

bool AllDifferentConstraint::MakeAugmentingPath(int start) {
  // Do a BFS and use visiting_ as a queue, with num_visited pointing
  // at its begin() and num_to_visit its end().
  // To switch to the augmenting path once a nonmatched value was found,
  // we remember the BFS tree in variable_visited_from_.
  int num_to_visit = 0;
  int num_visited = 0;
  // Enqueue start.
  visiting_[num_to_visit++] = start;
  variable_visited_[start] = true;
  variable_visited_from_[start] = -1;

  while (num_visited < num_to_visit) {
    // Dequeue node to visit.
    const int node = visiting_[num_visited++];

    for (const int value : successor_[node]) {
      if (value_visited_[value]) continue;
      value_visited_[value] = true;
      if (value_to_variable_[value] == -1) {
        // value is not matched: change path from node to start, and return.
        int path_node = node;
        int path_value = value;
        while (path_node != -1) {
          int old_value = variable_to_value_[path_node];
          variable_to_value_[path_node] = path_value;
          value_to_variable_[path_value] = path_node;
          path_node = variable_visited_from_[path_node];
          path_value = old_value;
        }
        return true;
      } else {
        // Enqueue node matched to value.
        const int next_node = value_to_variable_[value];
        variable_visited_[next_node] = true;
        visiting_[num_to_visit++] = next_node;
        variable_visited_from_[next_node] = node;
      }
    }
  }
  return false;
}

// The algorithm copies the solver state to successor_, which is used to compute
// a matching. If all variables can be matched, it generates the residual graph
// in separate vectors, computes its SCCs, and filters variable -> value if
// variable is not in the same SCC as value.
// Explanations for failure and filtering are fine-grained:
// failure is explained by a Hall set, i.e. dom(variables) \subseteq {values},
// with |variables| < |values|; filtering is explained by the Hall set that
// would happen if the variable was assigned to the value.
//
// TODO(user): If needed, there are several ways performance could be
// improved.
// If copying the variable state is too costly, it could be maintained instead.
// If the propagator has too many fruitless calls (without failing/pruning),
// we can remember the O(n) arcs used in the matching and the SCC decomposition,
// and guard calls to Propagate() if these arcs are still valid.
bool AllDifferentConstraint::Propagate() {
  // Copy variable state to graph state.
  prev_matching_ = variable_to_value_;
  value_to_variable_.assign(num_values_, -1);
  variable_to_value_.assign(num_variables_, -1);
  successor_.clear();
  const auto assignment = AssignmentView(trail_->Assignment());
  for (int x = 0; x < num_variables_; x++) {
    successor_.Add({});
    for (const auto [value, lit] : variable_to_possible_values_[x]) {
      if (assignment.LiteralIsFalse(lit)) continue;

      // Forward-checking should propagate x != value.
      successor_.AppendToLastVector(value);

      // Seed with previous matching.
      if (prev_matching_[x] == value && value_to_variable_[value] == -1) {
        variable_to_value_[x] = prev_matching_[x];
        value_to_variable_[prev_matching_[x]] = x;
      }
    }
    if (successor_[x].size() == 1) {
      const int value = successor_[x][0];
      if (value_to_variable_[value] == -1) {
        value_to_variable_[value] = x;
        variable_to_value_[x] = value;
      }
    }
  }

  // Compute max matching.
  int x = 0;
  for (; x < num_variables_; x++) {
    if (variable_to_value_[x] == -1) {
      value_visited_.assign(num_values_, false);
      variable_visited_.assign(num_variables_, false);
      MakeAugmentingPath(x);
    }
    if (variable_to_value_[x] == -1) break;  // No augmenting path exists.
  }

  // Fail if covering variables impossible.
  // Explain with the forbidden parts of the graph that prevent
  // MakeAugmentingPath from increasing the matching size.
  if (x < num_variables_) {
    // For now explain all forbidden arcs.
    std::vector<Literal>* conflict = trail_->MutableConflict();
    conflict->clear();
    for (int y = 0; y < num_variables_; y++) {
      if (!variable_visited_[y]) continue;
      for (const auto [value, lit] : variable_to_possible_values_[y]) {
        if (!value_visited_[value]) {
          DCHECK(assignment.LiteralIsFalse(lit));
          conflict->push_back(lit);
        }
      }
    }
    return false;
  }

  // The current matching is a valid solution, now try to filter values.
  // Build residual graph, compute its SCCs.
  residual_graph_successors_.clear();
  for (int x = 0; x < num_variables_; x++) {
    residual_graph_successors_.Add({});
    for (const int succ : successor_[x]) {
      if (succ != variable_to_value_[x]) {
        residual_graph_successors_.AppendToLastVector(num_variables_ + succ);
      }
    }
  }

  const int dummy_node = num_variables_ + num_values_;
  const bool need_dummy = num_variables_ < num_values_;
  for (int value = 0; value < num_values_; value++) {
    residual_graph_successors_.Add({});
    if (value_to_variable_[value] != -1) {
      residual_graph_successors_.AppendToLastVector(value_to_variable_[value]);
    } else if (need_dummy) {
      residual_graph_successors_.AppendToLastVector(dummy_node);
    }
  }
  if (need_dummy) {
    DCHECK_EQ(residual_graph_successors_.size(), dummy_node);
    residual_graph_successors_.Add({});
    for (int x = 0; x < num_variables_; x++) {
      residual_graph_successors_.AppendToLastVector(x);
    }
  }

  // Compute SCCs, make node -> component map.
  struct SccOutput {
    explicit SccOutput(std::vector<int>* c) : components(c) {}
    void emplace_back(int const* b, int const* e) {
      for (int const* it = b; it < e; ++it) {
        (*components)[*it] = num_components;
      }
      ++num_components;
    }
    int num_components = 0;
    std::vector<int>* components;
  };
  SccOutput scc_output(&component_number_);
  FindStronglyConnectedComponents(
      static_cast<int>(residual_graph_successors_.size()),
      residual_graph_successors_, &scc_output);

  // Remove arcs var -> val where SCC(var) -/->* SCC(val).
  for (int x = 0; x < num_variables_; x++) {
    if (successor_[x].size() == 1) continue;
    for (const auto [value, x_lit] : variable_to_possible_values_[x]) {
      if (assignment.LiteralIsFalse(x_lit)) continue;

      const int value_node = value + num_variables_;
      DCHECK_LT(value_node, component_number_.size());
      if (variable_to_value_[x] != value &&
          component_number_[x] != component_number_[value_node]) {
        // We can deduce that x != value. To explain, force x == value,
        // then find another assignment for the variable matched to
        // value. It will fail: explaining why is the same as
        // explaining failure as above, and it is an explanation of x != value.
        value_visited_.assign(num_values_, false);
        variable_visited_.assign(num_variables_, false);
        // Undo x -> old_value and old_variable -> value.
        const int old_variable = value_to_variable_[value];
        DCHECK_GE(old_variable, 0);
        DCHECK_LT(old_variable, num_variables_);
        variable_to_value_[old_variable] = -1;
        const int old_value = variable_to_value_[x];
        value_to_variable_[old_value] = -1;
        variable_to_value_[x] = value;
        value_to_variable_[value] = x;

        value_visited_[value] = true;
        MakeAugmentingPath(old_variable);
        DCHECK_EQ(variable_to_value_[old_variable], -1);  // No reassignment.

        std::vector<Literal>* reason = trail_->GetEmptyVectorToStoreReason();
        for (int y = 0; y < num_variables_; y++) {
          if (!variable_visited_[y]) continue;
          for (const auto [value, y_lit] : variable_to_possible_values_[y]) {
            if (!value_visited_[value]) {
              DCHECK(assignment.LiteralIsFalse(y_lit));
              reason->push_back(y_lit);
            }
          }
        }

        return trail_->EnqueueWithStoredReason(x_lit.Negated());
      }
    }
  }

  return true;
}

AllDifferentBoundsPropagator::AllDifferentBoundsPropagator(
    absl::Span<const AffineExpression> expressions, IntegerTrail* integer_trail)
    : integer_trail_(integer_trail) {
  CHECK(!expressions.empty());

  // We need +2 for sentinels.
  const int capacity = expressions.size() + 2;
  index_to_start_index_.resize(capacity);
  index_to_end_index_.resize(capacity);
  index_is_present_.Resize(capacity);
  index_to_expr_.resize(capacity, kNoIntegerVariable);

  for (int i = 0; i < expressions.size(); ++i) {
    bounds_.push_back({expressions[i]});
    negated_bounds_.push_back({expressions[i].Negated()});
  }
}

bool AllDifferentBoundsPropagator::Propagate() {
  if (!PropagateLowerBounds()) return false;

  // Note that it is not required to swap back bounds_ and negated_bounds_.
  // TODO(user): investigate the impact.
  std::swap(bounds_, negated_bounds_);
  const bool result = PropagateLowerBounds();
  std::swap(bounds_, negated_bounds_);
  return result;
}

void AllDifferentBoundsPropagator::FillHallReason(IntegerValue hall_lb,
                                                  IntegerValue hall_ub) {
  integer_reason_.clear();
  const int limit = GetIndex(hall_ub);
  for (int i = GetIndex(hall_lb); i <= limit; ++i) {
    const AffineExpression expr = index_to_expr_[i];
    integer_reason_.push_back(expr.GreaterOrEqual(hall_lb));
    integer_reason_.push_back(expr.LowerOrEqual(hall_ub));
  }
}

int AllDifferentBoundsPropagator::FindStartIndexAndCompressPath(int index) {
  // First, walk the pointer until we find one pointing to itself.
  int start_index = index;
  while (true) {
    const int next = index_to_start_index_[start_index];
    if (start_index == next) break;
    start_index = next;
  }

  // Second, redo the same thing and make everyone point to the representative.
  while (true) {
    const int next = index_to_start_index_[index];
    if (start_index == next) break;
    index_to_start_index_[index] = start_index;
    index = next;
  }
  return start_index;
}

bool AllDifferentBoundsPropagator::PropagateLowerBounds() {
  // Start by filling the cached bounds and sorting by increasing lb.
  for (CachedBounds& entry : bounds_) {
    entry.lb = integer_trail_->LowerBound(entry.expr);
    entry.ub = integer_trail_->UpperBound(entry.expr);
  }
  IncrementalSort(bounds_.begin(), bounds_.end(),
                  [](CachedBounds a, CachedBounds b) { return a.lb < b.lb; });

  // We will split the affine epressions in vars sorted by lb in contiguous
  // subset with index of the form [start, start + num_in_window).
  int start = 0;
  int num_in_window = 1;

  // Minimum lower bound in the current window.
  IntegerValue min_lb = bounds_.front().lb;

  const int size = bounds_.size();
  for (int i = 1; i < size; ++i) {
    const IntegerValue lb = bounds_[i].lb;

    // If the lower bounds of all the other variables is greater, then it can
    // never fall into a potential hall interval formed by the variable in the
    // current window, so we can split the problem into independent parts.
    if (lb <= min_lb + IntegerValue(num_in_window - 1)) {
      ++num_in_window;
      continue;
    }

    // Process the current window.
    if (num_in_window > 1) {
      absl::Span<CachedBounds> window(&bounds_[start], num_in_window);
      if (!PropagateLowerBoundsInternal(min_lb, window)) {
        return false;
      }
    }

    // Start of the next window.
    start = i;
    num_in_window = 1;
    min_lb = lb;
  }

  // Take care of the last window.
  if (num_in_window > 1) {
    absl::Span<CachedBounds> window(&bounds_[start], num_in_window);
    return PropagateLowerBoundsInternal(min_lb, window);
  }

  return true;
}

bool AllDifferentBoundsPropagator::PropagateLowerBoundsInternal(
    IntegerValue min_lb, absl::Span<CachedBounds> bounds) {
  hall_starts_.clear();
  hall_ends_.clear();

  // All cached lb in bounds will be in [min_lb, min_lb + bounds_.size()).
  // Make sure we change our base_ so that GetIndex() fit in our buffers.
  base_ = min_lb - IntegerValue(1);

  index_is_present_.ResetAllToFalse();

  // Sort bounds by increasing ub.
  std::sort(bounds.begin(), bounds.end(),
            [](CachedBounds a, CachedBounds b) { return a.ub < b.ub; });
  for (const CachedBounds entry : bounds) {
    const AffineExpression expr = entry.expr;

    // Note that it is important to use the cache to make sure GetIndex() is
    // not out of bound in case integer_trail_->LowerBound() changed when we
    // pushed something.
    const IntegerValue lb = entry.lb;
    const int lb_index = GetIndex(lb);
    const bool value_is_covered = index_is_present_[lb_index];

    // Check if lb is in an Hall interval, and push it if this is the case.
    if (value_is_covered) {
      const int hall_index =
          std::lower_bound(hall_ends_.begin(), hall_ends_.end(), lb) -
          hall_ends_.begin();
      if (hall_index < hall_ends_.size() && hall_starts_[hall_index] <= lb) {
        const IntegerValue hs = hall_starts_[hall_index];
        const IntegerValue he = hall_ends_[hall_index];
        FillHallReason(hs, he);
        integer_reason_.push_back(expr.GreaterOrEqual(hs));
        if (!integer_trail_->SafeEnqueue(expr.GreaterOrEqual(he + 1),
                                         integer_reason_)) {
          return false;
        }
      }
    }

    // Update our internal representation of the non-consecutive intervals.
    //
    // If lb is not used, we add a node there, otherwise we add it to the
    // right of the interval that contains lb. In both cases, if there is an
    // interval to the left (resp. right) we merge them.
    int new_index = lb_index;
    int start_index = lb_index;
    int end_index = lb_index;
    if (value_is_covered) {
      start_index = FindStartIndexAndCompressPath(new_index);
      new_index = index_to_end_index_[start_index] + 1;
      end_index = new_index;
    } else {
      if (index_is_present_[new_index - 1]) {
        start_index = FindStartIndexAndCompressPath(new_index - 1);
      }
    }
    if (index_is_present_[new_index + 1]) {
      end_index = index_to_end_index_[new_index + 1];
      index_to_start_index_[new_index + 1] = start_index;
    }

    // Update the end of the representative.
    index_to_end_index_[start_index] = end_index;

    // This is the only place where we "add" a new node.
    {
      index_to_start_index_[new_index] = start_index;
      index_to_expr_[new_index] = expr;
      index_is_present_.Set(new_index);
    }

    // In most situation, we cannot have a conflict now, because it should have
    // been detected before by pushing an interval lower bound past its upper
    // bound. However, it is possible that when we push one bound, other bounds
    // change. So if the upper bound is smaller than the current interval end,
    // we abort so that the conflict reason will be better on the next call to
    // the propagator.
    const IntegerValue end = GetValue(end_index);
    if (end > integer_trail_->UpperBound(expr)) return true;

    // If we have a new Hall interval, add it to the set. Note that it will
    // always be last, and if it overlaps some previous Hall intervals, it
    // always overlaps them fully.
    //
    // Note: It is okay to not use entry.ub here if we want to fetch the last
    // value, but in practice it shouldn't really change when we push a
    // lower_bound and it is faster to use the cached entry.
    if (end == entry.ub) {
      const IntegerValue start = GetValue(start_index);
      while (!hall_starts_.empty() && start <= hall_starts_.back()) {
        hall_starts_.pop_back();
        hall_ends_.pop_back();
      }
      DCHECK(hall_ends_.empty() || hall_ends_.back() < start);
      hall_starts_.push_back(start);
      hall_ends_.push_back(end);
    }
  }
  return true;
}

void AllDifferentBoundsPropagator::RegisterWith(
    GenericLiteralWatcher* watcher) {
  const int id = watcher->Register(this);
  for (const CachedBounds& entry : bounds_) {
    watcher->WatchAffineExpression(entry.expr, id);
  }
  watcher->NotifyThatPropagatorMayNotReachFixedPointInOnePass(id);
}

}  // namespace sat
}  // namespace operations_research
