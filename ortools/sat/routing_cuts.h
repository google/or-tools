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

#ifndef ORTOOLS_SAT_ROUTING_CUTS_H_
#define ORTOOLS_SAT_ROUTING_CUTS_H_

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// Helper to recover the mapping between nodes and "cumul" expressions in simple
// cases of route constraints (at most one expression per node and "dimension"
// -- such as time or load, and at most one relation per arc and dimension).
//
// This returns an empty vector and num_dimensions == 0 if nothing is detected.
// Otherwise it returns one expression per node and dimensions.
struct RoutingCumulExpressions {
  const AffineExpression& GetNodeExpression(int node, int dimension) const {
    return flat_node_dim_expressions[node * num_dimensions + dimension];
  }

  int num_dimensions = 0;
  std::vector<AffineExpression> flat_node_dim_expressions;
};
RoutingCumulExpressions DetectDimensionsAndCumulExpressions(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals,
    const BinaryRelationRepository& binary_relation_repository);

// A coeff * var + offset affine expression, where `var` is always a positive
// reference (contrary to AffineExpression, where the coefficient is always
// positive).
struct NodeExpression {
  IntegerVariable var;
  IntegerValue coeff;
  IntegerValue offset;

  NodeExpression() : var(kNoIntegerVariable), coeff(0), offset(0) {}

  NodeExpression(IntegerVariable var, IntegerValue coeff, IntegerValue offset)
      : var(var), coeff(coeff), offset(offset) {}

  explicit NodeExpression(const AffineExpression& expr) {
    if (expr.var == kNoIntegerVariable || VariableIsPositive(expr.var)) {
      var = expr.var;
      coeff = expr.coeff;
    } else {
      var = PositiveVariable(expr.var);
      coeff = -expr.coeff;
    }
    offset = expr.constant;
  }

  bool IsEmpty() const { return var == kNoIntegerVariable; }

  IntegerValue ValueAt(IntegerValue x) const { return coeff * x + offset; }

  NodeExpression Negated() const {
    return NodeExpression(var, -coeff, -offset);
  }
};

// Returns some bounds on y_expr - x_expr, based on the given relation and the
// given variable bounds. r.a (resp. r.b) must have the same variable as x_expr
// (resp. y_expr), which must not be kNoIntegerVariable. Moreover, the
// coefficients of these variables in x_expr, y_expr and r must not be zero.
//
// The returned bounds are generally not the best possible ones (computing them
// is equivalent to solving a MIP -- min(y_expr - x_expr) subject to r, x.var in
// x_var_bounds and y.var in y_var_bounds).
std::pair<IntegerValue, IntegerValue> GetDifferenceBounds(
    const NodeExpression& x_expr, const NodeExpression& y_expr,
    const sat::Relation& r,
    const std::pair<IntegerValue, IntegerValue>& x_var_bounds,
    const std::pair<IntegerValue, IntegerValue>& y_var_bounds);

// Helper to store the result of DetectDimensionsAndCumulExpressions() and also
// recover and store bounds on (node_expr[head] - node_expr[tail]) for each arc
// (tail -> head) assuming the arc is taken.
class RouteRelationsHelper {
 public:
  // Visible for testing.
  RouteRelationsHelper() = default;

  // Creates a RouteRelationsHelper for the given RoutesConstraint and
  // associated binary relations. The vector `flat_node_dim_expressions` should
  // be the result of DetectDimensionsAndCumulExpressions(). If it is empty,
  // this returns nullptr.
  //
  // Otherwise this stores it and also computes the tightest bounds we can on
  // (node_expr[head] - node_expr[tail]) for each arc, assuming the arc is taken
  // (its literal is true).
  static std::unique_ptr<RouteRelationsHelper> Create(
      int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
      absl::Span<const Literal> literals,
      absl::Span<const AffineExpression> flat_node_dim_expressions,
      const BinaryRelationRepository& binary_relation_repository, Model* model);

  // Returns the number of "dimensions", such as time or vehicle load.
  int num_dimensions() const { return num_dimensions_; }

  int num_nodes() const {
    return flat_node_dim_expressions_.size() / num_dimensions_;
  }

  int num_arcs() const {
    return flat_arc_dim_relations_.size() / num_dimensions_;
  }

  // Returns the expression associated with the given node and dimension.
  const AffineExpression& GetNodeExpression(int node, int dimension) const {
    return flat_node_dim_expressions_[node * num_dimensions_ + dimension];
  }

  // Each arc stores the bound on expr[head] - expr[tail] \in [lhs, rhs]. Note
  // that we interpret kMin/kMax integer values as not set. Such bounds will
  // still be valid though because we have a precondition on the input model
  // variables to be within [kMin/2, kMax/2].
  struct HeadMinusTailBounds {
    IntegerValue lhs = kMinIntegerValue;
    IntegerValue rhs = kMaxIntegerValue;

    bool operator==(const HeadMinusTailBounds& r) const {
      return lhs == r.lhs && rhs == r.rhs;
    }
  };
  const HeadMinusTailBounds& GetArcRelation(int arc, int dimension) const {
    DCHECK_GE(dimension, 0);
    DCHECK_LT(dimension, num_dimensions_);
    DCHECK_GE(arc, 0);
    return flat_arc_dim_relations_[arc * num_dimensions_ + dimension];
  }

  bool HasShortestPathsInformation() const {
    return !flat_shortest_path_lbs_.empty();
  }

  // If any of the bound is at the maximum, then there is no path between tail
  // and head.
  bool PathExists(int tail, int head) const {
    if (flat_shortest_path_lbs_.empty()) return true;
    const int num_nodes_squared = num_nodes_ * num_nodes_;
    for (int dim = 0; dim < num_dimensions_; ++dim) {
      if (flat_shortest_path_lbs_[dim * num_nodes_squared + tail * num_nodes_ +
                                  head] ==
          std::numeric_limits<IntegerValue>::max()) {
        return false;
      }
    }
    return true;
  }

  bool PropagateLocalBoundsUsingShortestPaths(
      const IntegerTrail& integer_trail, int tail, int head,
      const absl::flat_hash_map<IntegerVariable, IntegerValue>& input,
      absl::flat_hash_map<IntegerVariable, IntegerValue>* output) const {
    if (flat_shortest_path_lbs_.empty()) return true;
    const int num_nodes_squared = num_nodes_ * num_nodes_;
    for (int dim = 0; dim < num_dimensions_; ++dim) {
      const AffineExpression& tail_expr = GetNodeExpression(tail, dim);
      const AffineExpression& head_expr = GetNodeExpression(head, dim);
      const IntegerValue head_minus_tail_lb =
          flat_shortest_path_lbs_[dim * num_nodes_squared + tail * num_nodes_ +
                                  head];
      if (head_expr.var == kNoIntegerVariable) continue;

      IntegerValue tail_lb = integer_trail.LevelZeroLowerBound(tail_expr);
      if (tail_expr.var != kNoIntegerVariable) {
        auto it = input.find(tail_expr.var);
        if (it != input.end()) {
          tail_lb = std::max(tail_lb, tail_expr.ValueAt(it->second));
        }
      }

      // head_expr >= tail_lb + head_minus_tail_lb;
      const IntegerLiteral i_lit =
          head_expr.GreaterOrEqual(CapAddI(tail_lb, head_minus_tail_lb));
      if (i_lit.bound > integer_trail.LevelZeroUpperBound(i_lit.var)) {
        return false;  // not possible.
      }
      auto [it, inserted] = output->insert({i_lit.var, i_lit.bound});
      if (!inserted) {
        it->second = std::max(it->second, i_lit.bound);
      }
    }
    return true;
  }

  // Returns the level zero lower bound of the offset between the (optionally
  // negated) expressions associated with the head and tail of the given arc,
  // and the given dimension.
  IntegerValue GetArcOffsetLowerBound(int arc, int dimension,
                                      bool negate_expressions) const;

  void RemoveArcs(absl::Span<const int> sorted_arc_indices);

 private:
  RouteRelationsHelper(int num_dimensions,
                       std::vector<AffineExpression> flat_node_dim_expressions,
                       std::vector<HeadMinusTailBounds> flat_arc_dim_relations,
                       std::vector<IntegerValue> flat_shortest_path_lbs);

  void LogStats() const;

  const int num_nodes_ = 0;
  const int num_dimensions_ = 0;

  // The expression associated with node n and dimension d is at index n *
  // num_dimensions_ + d.
  std::vector<AffineExpression> flat_node_dim_expressions_;

  // The relation associated with arc a and dimension d is at index a *
  // num_dimensions_ + d.
  std::vector<HeadMinusTailBounds> flat_arc_dim_relations_;

  // flat_shortest_path_lbs_[dim * num_nodes^2 + i * num_nodes + j] is a lower
  // bounds on node_expression[dim][j] - node_expression[dim][i] whatever the
  // path used to join node i to node j (not using the root 0). It only make
  // sense for i and j != 0.
  std::vector<IntegerValue> flat_shortest_path_lbs_;
};

// Computes and fills the node expressions of all the routes constraints in
// `output_model` that don't have them, if possible. The node expressions are
// inferred from the binary relations in `input_model`. Both models must have
// the same variables (they can reference the same underlying object).
// Returns the number of constraints that were filled, and the total number of
// dimensions added to them.
std::pair<int, int> MaybeFillMissingRoutesConstraintNodeExpressions(
    const CpModelProto& input_model, CpModelProto& output_model);

class SpecialBinPackingHelper {
 public:
  SpecialBinPackingHelper() = default;
  explicit SpecialBinPackingHelper(double dp_effort) : dp_effort_(dp_effort) {}

  // See below.
  enum ItemOrBinType {
    MUST_BE_ITEM = 0,  // capacity will be set at zero.
    ITEM_OR_BIN = 1,
    MUST_BE_BIN = 2,  // demand will be set at zero.
  };

  // This is used to represent a "special bin packing" problem where we have
  // objects that can either be items with a given demand or bins with a given
  // capacity. The problem is to choose the minimum number of objects that will
  // be bins, such that the other objects (items) can be packed inside.
  struct ItemOrBin {
    // The initial routing node that correspond to this object.
    int node = 0;

    // Only one option will apply, this can either be an item with given demand
    // or a bin with given capacity.
    //
    // Important: We support negative demands and negative capacity. We just
    // need that the sum of demand <= capacity for the item in that bin.
    IntegerValue demand = 0;
    IntegerValue capacity = 0;

    // We described the problem where each object can be an item or a bin, but
    // in practice we might have restriction on what object can be which, and we
    // use this field to indicate that.
    //
    // The numerical order is important as we use that in the greedy algorithm.
    // See ComputeMinNumberOfBins() code.
    ItemOrBinType type = ITEM_OR_BIN;
  };

  // Given a "special bin packing" problem as decribed above, return a lower
  // bound on the number of bins that needs to be taken.
  //
  // This simply sorts the object according to a greedy criteria and minimize
  // the number of bins such that the "demands <= capacities" constraint is
  // satisfied.
  //
  // If the problem is infeasible, this will return object.size() + 1, which is
  // a trivially infeasible bound.
  //
  // TODO(user): Use fancier DP to derive tighter bound. Also, when there are
  // many dimensions, the choice of which item go to which bin is correlated,
  // can we exploit this?
  int ComputeMinNumberOfBins(
      absl::Span<ItemOrBin> objects,
      std::vector<int>& objects_that_cannot_be_bin_and_reach_minimum,
      std::string& info);

  // Visible for testing.
  //
  // Returns true if we can greedily pack items with indices [num_bins, size)
  // into the first num_bins bins (with indices [0, num_bins)). This function
  // completely ignores the ItemOrBin types, and just uses the first num_bins
  // objects as bins and the rest as items. It is up to the caller to make sure
  // this is okay.
  //
  // This is just used to quickly assess if a more precise lower bound on the
  // number of bins could gain something. It works in O(num_bins * num_items).
  bool GreedyPackingWorks(int num_bins, absl::Span<const ItemOrBin> objects);

  // Visible for testing.
  //
  // If we look at all the possible sum of item demands, it is possible that
  // some value can never be reached. We use dynamic programming to compute the
  // set of reachable values and tighten the capacities accordingly.
  //
  // Returns true iff we tightened something.
  bool UseDpToTightenCapacities(absl::Span<ItemOrBin> objects);

 private:
  // Note that this will sort the objects so that the "best" bins are first.
  // See implementation.
  int ComputeMinNumberOfBinsInternal(
      absl::Span<ItemOrBin> objects,
      std::vector<int>& objects_that_cannot_be_bin_and_reach_minimum);

  const double dp_effort_ = 1e8;
  std::vector<IntegerValue> tmp_capacities_;
  std::vector<int64_t> tmp_demands_;
  MaxBoundedSubsetSumExact max_bounded_subset_sum_exact_;
};

inline std::ostream& operator<<(std::ostream& os,
                                SpecialBinPackingHelper::ItemOrBin o) {
  os << absl::StrCat("d=", o.demand, " c=", o.capacity, " t=", o.type);
  return os;
}

// Keep the best min outgoing/incoming flow out of a subset.
class BestBoundHelper {
 public:
  BestBoundHelper() = default;
  explicit BestBoundHelper(std::string base_name) : base_name_(base_name) {}

  void Update(int new_bound, std::string name,
              absl::Span<const int> cannot_be_first = {},
              absl::Span<const int> cannot_be_last = {}) {
    if (new_bound < bound_) return;
    if (new_bound > bound_) {
      bound_ = new_bound;
      name_ = name;
      cannot_be_last_.clear();
      cannot_be_first_.clear();
    }
    cannot_be_first_.insert(cannot_be_first_.begin(), cannot_be_first.begin(),
                            cannot_be_first.end());
    cannot_be_last_.insert(cannot_be_last_.begin(), cannot_be_last.begin(),
                           cannot_be_last.end());
    gtl::STLSortAndRemoveDuplicates(&cannot_be_first_);
    gtl::STLSortAndRemoveDuplicates(&cannot_be_last_);
  }

  std::string name() const { return absl::StrCat(base_name_, name_); }
  int bound() const { return bound_; }

  // To serve the current subset with bound() num vehicles, one cannot exit
  // the subset with nodes in CannotBeLast() and one cannot enter the subset
  // with node in CannotBeFirst(). Moreover, one cannot enter from or leave to
  // nodes in OutsideNodeThatCannotBeConnected().
  absl::Span<const int> CannotBeLast() const { return cannot_be_last_; }
  absl::Span<const int> CannotBeFirst() const { return cannot_be_first_; }

 private:
  int bound_ = 0;
  std::string base_name_;
  std::string name_;
  std::vector<int> tmp_;
  std::vector<int> cannot_be_last_;
  std::vector<int> cannot_be_first_;
};

// Helper to compute the minimum flow going out of a subset of nodes, for a
// given RoutesConstraint.
class MinOutgoingFlowHelper {
 public:
  // Warning: The underlying tails/heads/literals might be resized from one
  // call to the next of one of the functions here, so be careful.
  MinOutgoingFlowHelper(int num_nodes, const std::vector<int>& tails,
                        const std::vector<int>& heads,
                        const std::vector<Literal>& literals, Model* model);

  ~MinOutgoingFlowHelper();

  // Returns the minimum flow going out of `subset`, based on a generalization
  // of the CVRP "rounded capacity inequalities", by using the given helper, if
  // possible. The complexity is O((subset.size() + num_arcs()) *
  // num_dimensions()).
  int ComputeDimensionBasedMinOutgoingFlow(absl::Span<const int> subset,
                                           const RouteRelationsHelper& helper,
                                           BestBoundHelper* best_bound);

  // Returns the minimum flow going out of `subset`, based on a conservative
  // estimate of the maximum number of nodes of a feasible path inside this
  // subset. `subset` must not be empty and must not contain the depot (node 0).
  // Paths are approximated by their length and their last node, and can thus
  // contain cycles. The complexity is O(subset.size() ^ 3).
  int ComputeMinOutgoingFlow(absl::Span<const int> subset);

  // Same as above, but uses less conservative estimates (paths are approximated
  // by their set of nodes and their last node -- hence they can't contain
  // cycles). The complexity is O(2 ^ subset.size()).
  int ComputeTightMinOutgoingFlow(absl::Span<const int> subset);

  // Returns false if the given subset CANNOT be served by k routes.
  // Returns true if we have a route or we don't know for sure (if we abort).
  // The parameter k must be positive.
  //
  // Even more costly algo in O(n!/k!*k^(n-k)) that answers the question exactly
  // given the available enforced linear1 and linear2 constraints. However it
  // can stop as soon as one solution is found.
  //
  // If special_node is non-negative, we will only look for routes that
  //   - Start at this special_node if use_forward_direction = true
  //   - End at this special_node if use_forward_direction = false
  //
  // If the RouteRelationsHelper is non null, then we will use "shortest path"
  // bounds instead of recomputing them from the binary relation of the model.
  //
  // TODO(user): the complexity also depends on the longest route and improves
  // if routes fail quickly. Give a better estimate?
  bool SubsetMightBeServedWithKRoutes(
      int k, absl::Span<const int> subset,
      RouteRelationsHelper* helper = nullptr,
      LinearConstraintManager* manager = nullptr, int special_node = -1,
      bool use_forward_direction = true);

  // Advanced. If non-empty, and one of the functions above proved that a subset
  // needs at least k vehicles to serve it, then these vector list the nodes
  // that cannot be first (resp. last) in one of the solution with k routes. If
  // a node is listed here, it means we will need at least k + 1 routes to serve
  // the subset and enter (resp. leave) from that node.
  //
  // This can be used to either reduce the size of the subset and still need
  // k vehicles, or generate stronger cuts.
  absl::Span<const int> CannotBeLast() const { return cannot_be_last_; };
  absl::Span<const int> CannotBeFirst() const { return cannot_be_first_; };

  // Just for stats reporting.
  void ReportDpSkip() { num_full_dp_skips_++; }

 private:
  void PrecomputeDataForSubset(absl::Span<const int> subset);

  // Given a subset S to serve in a route constraint, returns a special bin
  // packing problem (defined above) where the minimum number of bins will
  // correspond to the minimum number of vehicles needed to serve this subset.
  //
  // One way to derive such reduction is as follow.
  //
  // If we look at a path going through the subset, it will touch in order the
  // nodes P = {n_0, ..., n_e}. It will enter S at a "start" node n_0 and leave
  // at a "end" node n_e.
  //
  // We assume (see the RouteRelationsHelper) that each node n has an
  // associated variable X_n, and that each arc t->h has an associated relation
  // lhs(t,h) <= X_h - X_t. Summing all these inequalities along the path above
  // we get:
  //   Sum_{i \in [1..e]} lhs(n_i, n_(i+1)) <= X_(n_e) - X_(n_0)
  // introducing:
  //  - d(n) = min_(i \in S) lhs(i, n)  [minimum incoming weight in subset S]
  //  - UB   = max_(i \in S) upper_bound(X_i)
  // We get:
  //    Sum_{n \in P \ n_0} d(n) <= UB - lower_bound(n_0)
  //
  // Here we can see that the "starting node" n0 is on the "capacity" side and
  // will serve the role of a bin with capacity (UB - lower_bound(n_0)), whereas
  // the other nodes n will be seen as "item" with demands d(i).
  //
  // Given that the set of paths going through S must be disjoint and serve all
  // the nodes, we get exactly the special bin packing problem described above
  // where the starting nodes are the bins and the other inner-nodes are the
  // items.
  //
  // Note that if a node has no incoming arc from within S, it must be a start
  // (i.e. a bin). And if a node has no incoming arcs from outside S, it cannot
  // be a start an must be an inner node (i.e. an item). We can exploit this to
  // derive better bounds.
  //
  // We just explained the reduction using incoming arcs and starts of route,
  // but we can do the same with outgoing arcs and ends of route. We can also
  // change the dimension (the X_i) and variable direction used in the
  // RouteRelationsHelper to exploit relations X_h - X_t <= rhs(t,h) instead.
  //
  // We provide a reduction for the cross product of:
  // - Each possible dimension in the RouteRelationsHelper.
  // - lhs or rhs (when negate_expressions = true) in X - Y \in [lhs, rhs].
  // - (start and incoming arcs) or (ends and outgoing arcs).
  //
  // Warning: the returned Span<> is only valid until the next call to this
  // function.
  //
  // TODO(user): Given the info for a subset, we can derive bounds for any
  // smaller set included in it. We just have to ignore the MUST_BE_ITEM
  // type as this might no longer be true. That might be interesting.
  absl::Span<SpecialBinPackingHelper::ItemOrBin>
  RelaxIntoSpecialBinPackingProblem(absl::Span<const int> subset, int dimension,
                                    bool negate_expressions, bool use_incoming,
                                    const RouteRelationsHelper& helper);

  // Returns the minimum flow going out of a subset of size `subset_size`,
  // assuming that the longest feasible path inside this subset has
  // `longest_path_length` nodes and that there are at most `max_longest_paths`
  // such paths.
  int GetMinOutgoingFlow(int subset_size, int longest_path_length,
                         int max_longest_paths);

  const std::vector<int>& tails_;
  const std::vector<int>& heads_;
  const std::vector<Literal>& literals_;
  const BinaryRelationRepository& binary_relation_repository_;
  const ImpliedBounds& implied_bounds_;
  const Trail& trail_;
  const IntegerTrail& integer_trail_;
  const IntegerEncoder& integer_encoder_;
  const RootLevelLinear2Bounds& root_level_bounds_;
  SharedStatistics* shared_stats_;

  // Temporary data used by ComputeMinOutgoingFlow(). Always contain default
  // values, except while ComputeMinOutgoingFlow() is running.
  // ComputeMinOutgoingFlow() computes, for each i in [0, |subset|), whether
  // each node n in the subset could appear at position i in a feasible path.
  // It computes whether n can appear at position i based on which nodes can
  // appear at position i-1, and based on the arc literals and some linear
  // constraints they enforce. To save memory, it only stores data about two
  // consecutive positions at a time: a "current" position i, and a "next"
  // position i+1.

  std::vector<int> tmp_subset_;
  std::vector<bool> in_subset_;
  std::vector<int> index_in_subset_;

  // For each node n, the indices (in tails_, heads_) of the m->n and n->m arcs
  // inside the subset (self arcs excepted).
  std::vector<std::vector<int>> incoming_arc_indices_;
  std::vector<std::vector<int>> outgoing_arc_indices_;

  // This can only be true for node in the current subset. If a node 'n' has no
  // incoming arcs from outside the subset, the part of a route serving node 'n'
  // in a subset cannot start at that node. And if it has no outoing arc leaving
  // the subset, it cannot end at that node. This can be used to derive tighter
  // bounds.
  std::vector<bool> has_incoming_arcs_from_outside_;
  std::vector<bool> has_outgoing_arcs_to_outside_;

  // If a subset has an unique arc arriving or leaving at a given node, we can
  // derive tighter bounds.
  class UniqueArc {
   public:
    bool IsUnique() const { return num_seen_ == 1; }
    int Get() const { return arc_; }

    void Add(int arc) {
      ++num_seen_;
      arc_ = arc;
    }

   private:
    int num_seen_ = 0;
    int arc_ = 0;
  };
  std::vector<UniqueArc> incoming_arcs_from_outside_;
  std::vector<UniqueArc> outgoing_arcs_to_outside_;

  // For each node n, whether it can appear at the current and next position in
  // a feasible path.
  std::vector<bool> reachable_;
  std::vector<bool> next_reachable_;
  // For each node n, the lower bound of each variable (appearing in a linear
  // constraint enforced by some incoming arc literal), if n appears at the
  // current and next position in a feasible path. Variables not appearing in
  // these maps have no tighter lower bound that the one from the IntegerTrail
  // (at decision level 0).
  std::vector<absl::flat_hash_map<IntegerVariable, IntegerValue>>
      node_var_lower_bounds_;
  std::vector<absl::flat_hash_map<IntegerVariable, IntegerValue>>
      next_node_var_lower_bounds_;

  SpecialBinPackingHelper bin_packing_helper_;
  std::vector<SpecialBinPackingHelper::ItemOrBin> tmp_bin_packing_problem_;

  std::vector<int> objects_that_cannot_be_bin_and_reach_minimum_;

  std::vector<int> cannot_be_last_;
  std::vector<int> cannot_be_first_;

  // Statistics.
  int64_t num_full_dp_skips_ = 0;
  int64_t num_full_dp_calls_ = 0;
  int64_t num_full_dp_early_abort_ = 0;
  int64_t num_full_dp_work_abort_ = 0;
  int64_t num_full_dp_rc_skip_ = 0;
  int64_t num_full_dp_unique_arc_ = 0;
  absl::flat_hash_map<std::string, int> num_by_type_;
};

// Given a graph with nodes in [0, num_nodes) and a set of arcs (the order is
// important), this will:
//   - Start with each nodes in separate "subsets".
//   - Consider the arc in order, and each time one connects two separate
//     subsets, merge the two subsets into a new one.
//   - Stops when there is only 'stop_at_num_components' subset left.
//   - Output all subsets generated this way (at most 2 * num_nodes). The
//     subsets spans will point in the subset_data vector (which will be of size
//     exactly num_nodes).
//
// This is an heuristic to generate interesting cuts for TSP or other graph
// based constraints. We roughly follow the algorithm described in section 6 of
// "The Traveling Salesman Problem, A computational Study", David L. Applegate,
// Robert E. Bixby, Vasek Chvatal, William J. Cook.
//
// Note that this is mainly a "symmetric" case algo, but it does still work for
// the asymmetric case.
//
// TODO(user): Returns the tree instead and let caller call
// ExtractAllSubsetsFromForest().
void GenerateInterestingSubsets(int num_nodes,
                                absl::Span<const std::pair<int, int>> arcs,
                                int stop_at_num_components,
                                std::vector<int>* subset_data,
                                std::vector<absl::Span<const int>>* subsets);

// Given a set of rooted tree on n nodes represented by the parent vector,
// returns the n sets of nodes corresponding to all the possible subtree. Note
// that the output memory is just n as all subset will point into the same
// vector.
//
// This assumes no cycles, otherwise it will not crash but the result will not
// be correct.
//
// In the TSP context, if the tree is a Gomory-Hu cut tree, this will returns
// a set of "min-cut" that contains a min-cut for all node pairs.
//
// TODO(user): This also allocate O(n) memory internally, we could reuse it from
// call to call if needed.
void ExtractAllSubsetsFromForest(
    absl::Span<const int> parent, std::vector<int>* subset_data,
    std::vector<absl::Span<const int>>* subsets,
    int node_limit = std::numeric_limits<int>::max());

// In the routing context, we usually always have lp_value in [0, 1] and only
// looks at arcs with a lp_value that is not too close to zero.
struct ArcWithLpValue {
  int tail;
  int head;
  double lp_value;

  bool operator==(const ArcWithLpValue& o) const {
    return tail == o.tail && head == o.head && lp_value == o.lp_value;
  }
};

// Regroups and sum the lp values on duplicate arcs or reversed arcs
// (tail->head) and (head->tail). As a side effect, we will always
// have tail <= head.
void SymmetrizeArcs(std::vector<ArcWithLpValue>* arcs);

// Given a set of arcs on a directed graph with n nodes (in [0, num_nodes)),
// returns a "parent" vector of size n encoding a rooted Gomory-Hu tree.
//
// Note that usually each edge in the tree is attached a max-flow value (its
// weight), but we don't need it here. It can be added if needed. This tree as
// the property that for all (s, t) pair of nodes, if you take the minimum
// weight edge on the path from s to t and split the tree in two, then this is a
// min-cut for that pair.
//
// IMPORTANT: This algorithm currently "symmetrize" the graph, so we will
// actually have all the min-cuts that minimize sum incoming + sum outgoing lp
// values. The algo do not work as is on an asymmetric graph. Note however that
// because of flow conservation, our outgoing lp values should be the same as
// our incoming one on a circuit/route constraint.
//
// We use a simple implementation described in "Very Simple Methods for All
// Pairs Network Flow Analysis", Dan Gusfield, 1990,
// https://ranger.uta.edu/~weems/NOTES5311/LAB/LAB2SPR21/gusfield.huGomory.pdf
std::vector<int> ComputeGomoryHuTree(
    int num_nodes, absl::Span<const ArcWithLpValue> relevant_arcs);

// Cut generator for the circuit constraint, where in any feasible solution, the
// arcs that are present (variable at 1) must form a circuit through all the
// nodes of the graph. Self arc are forbidden in this case.
//
// In more generality, this currently enforce the resulting graph to be strongly
// connected. Note that we already assume basic constraint to be in the lp, so
// we do not add any cuts for components of size 1.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals, Model* model);

// Almost the same as CreateStronglyConnectedGraphCutGenerator() but for each
// components, computes the demand needed to serves it, and depending on whether
// it contains the depot (node zero) or not, compute the minimum number of
// vehicle that needs to cross the component border.
// `flat_node_dim_expressions` must have num_dimensions (possibly 0) times
// num_nodes elements, with the expression associated with node n and dimension
// d at index n * num_dimensions + d.
CutGenerator CreateCVRPCutGenerator(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals,
    absl::Span<const AffineExpression> flat_node_dim_expressions, Model* model);

// Try to find a subset where the current LP capacity of the outgoing or
// incoming arc is not enough to satisfy the demands.
//
// We support the special value -1 for tail or head that means that the arc
// comes from (or is going to) outside the nodes in [0, num_nodes). Such arc
// must still have a capacity assigned to it.
//
// TODO(user): Support general linear expression for capacities.
// TODO(user): Some model applies the same capacity to both an arc and its
// reverse. Also support this case.
CutGenerator CreateFlowCutGenerator(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<AffineExpression>& arc_capacities,
    std::function<void(const std::vector<bool>& in_subset,
                       IntegerValue* min_incoming_flow,
                       IntegerValue* min_outgoing_flow)>
        get_flows,
    Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_ROUTING_CUTS_H_
