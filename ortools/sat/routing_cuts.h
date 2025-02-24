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

#ifndef OR_TOOLS_SAT_ROUTING_CUTS_H_
#define OR_TOOLS_SAT_ROUTING_CUTS_H_

#include <stdint.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Helper to recover the mapping between nodes and binary relation variables in
// simple cases of route constraints (at most one variable per node and
// "dimension" -- such as time or load, and at most one relation per arc and
// dimension).
class RouteRelationsHelper {
 public:
  // Creates a RouteRelationsHelper for the given RoutesConstraint and
  // associated binary relations. If `flat_node_dim_variables` is empty,
  // infers them from the binary relations, if possible (otherwise, returns
  // nullptr). If `flat_node_dim_variables` is not empty, uses it to
  // initialize the helper (this list should have num_dimensions times num_nodes
  // elements, with the variable associated with node n and dimension d at index
  // n * num_dimensions + d). If there are more than one relation per arc and
  // dimension, a single relation is chosen arbitrarily.
  static std::unique_ptr<RouteRelationsHelper> Create(
      int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
      absl::Span<const Literal> literals,
      absl::Span<const IntegerVariable> flat_node_dim_variables,
      const BinaryRelationRepository& binary_relation_repository);

  // Returns the number of "dimensions", such as time or vehicle load.
  int num_dimensions() const { return num_dimensions_; }

  int num_nodes() const {
    return flat_node_dim_variables_.size() / num_dimensions_;
  }

  int num_arcs() const {
    return flat_arc_dim_relations_.size() / num_dimensions_;
  }

  // Returns the variable associated with the given node and dimension, or
  // kNoIntegerVariable if there is none. The returned variable is always a
  // positive one.
  IntegerVariable GetNodeVariable(int node, int dimension) const {
    return flat_node_dim_variables_[node * num_dimensions_ + dimension];
  }

  // Returns the relation tail_coeff.X + head_coeff.Y \in [lhs, rhs] between the
  // X and Y variables associated with the tail and head of the given arc,
  // respectively, and the given dimension (head_coeff is always positive).
  // Returns an "empty" struct with all fields set to 0 if there is no such
  // relation.
  struct Relation {
    IntegerValue tail_coeff = 0;
    IntegerValue head_coeff = 0;
    IntegerValue lhs;
    IntegerValue rhs;

    bool empty() const { return tail_coeff == 0 && head_coeff == 0; }

    bool operator==(const Relation& r) const {
      return tail_coeff == r.tail_coeff && head_coeff == r.head_coeff &&
             lhs == r.lhs && rhs == r.rhs;
    }
  };
  const Relation& GetArcRelation(int arc, int dimension) const {
    return flat_arc_dim_relations_[arc * num_dimensions_ + dimension];
  }

  void RemoveArcs(absl::Span<const int> sorted_arc_indices);

 private:
  RouteRelationsHelper(int num_dimensions,
                       std::vector<IntegerVariable> flat_node_dim_variables,
                       std::vector<Relation> flat_arc_dim_relations);

  void LogStats() const;

  int num_dimensions_;
  // The variable associated with node n and dimension d (or kNoIntegerVariable
  // if there is none) is at index n * num_dimensions_ + d.
  std::vector<IntegerVariable> flat_node_dim_variables_;
  // The relation associated with arc a and dimension d (or kNoRelation if
  // there is none) is at index a * num_dimensions_ + d.
  std::vector<Relation> flat_arc_dim_relations_;
};

// Computes and fills the node variables of all the routes constraints in
// `output_model` that don't have them, if possible. The node variables are
// inferred from the binary relations in `input_model`. Both models must have
// the same variables (they can reference the same underlying object).
// Returns the number of constraints that were filled, and the total number of
// dimensions added to them.
std::pair<int, int> MaybeFillMissingRoutesConstraintNodeVariables(
    const CpModelProto& input_model, CpModelProto& output_model);

// This is used to represent a "special bin packing" problem where we have
// objects that can either be items with a given demand or bins with a given
// capacity. The problem is to choose the minimum number of objects that will
// be bins, such that the other objects (items) can be packed inside.
struct ItemOrBin {
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
  enum {
    MUST_BE_ITEM = 0,  // capacity will be set at zero.
    ITEM_OR_BIN = 1,
    MUST_BE_BIN = 2,  // demand will be set at zero.
  } type = ITEM_OR_BIN;
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
int ComputeMinNumberOfBins(absl::Span<ItemOrBin> objects, bool* gcd_was_used);

// Helper to compute the minimum flow going out of a subset of nodes, for a
// given RoutesConstraint.
class MinOutgoingFlowHelper {
 public:
  MinOutgoingFlowHelper(int num_nodes, const std::vector<int>& tails,
                        const std::vector<int>& heads,
                        const std::vector<Literal>& literals, Model* model);

  ~MinOutgoingFlowHelper();

  // Returns the minimum flow going out of `subset`, based on a generalization
  // of the CVRP "rounded capacity inequalities", by using the given helper, if
  // possible (this requires all nodes to have an associated variable, and all
  // relations to have +1, -1 coefficients). The complexity is O((subset.size()
  // + num_arcs()) * num_dimensions()).
  int ComputeDemandBasedMinOutgoingFlow(absl::Span<const int> subset,
                                        const RouteRelationsHelper& helper);

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
  // TODO(user): the complexity also depends on the longest route and improves
  // if routes fail quickly. Give a better estimate?
  bool SubsetMightBeServedWithKRoutes(int k, absl::Span<const int> subset);

  // Just for stats reporting.
  void ReportDpSkip() { num_full_dp_skips_++; }

 private:
  void InitializeGraph(absl::Span<const int> subset);

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
  // Given that the set of paths going throug S must be disjoint and serve all
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
  // - lhs or rhs (when negate_variables = true) in X - Y \in [lhs, rhs].
  // - (start and incoming arcs) or (ends and outgoing arcs).
  //
  // Warning: the returned Span<> is only valid until the next call to this
  // function.
  //
  // TODO(user): Given the info for a subset, we can derive bounds for any
  // smaller set included in it. We just have to ignore the MUST_BE_ITEM
  // type as this might no longer be true. That might be interseting.
  absl::Span<ItemOrBin> RelaxIntoSpecialBinPackingProblem(
      absl::Span<const int> subset, int dimension, bool negate_variables,
      bool use_incoming, const RouteRelationsHelper& helper);

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
  const Trail& trail_;
  const IntegerTrail& integer_trail_;
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

  std::vector<ItemOrBin> tmp_bin_packing_problem_;

  // Statistics.
  int64_t num_full_dp_skips_ = 0;
  int64_t num_full_dp_calls_ = 0;
  int64_t num_full_dp_early_abort_ = 0;
  int64_t num_full_dp_work_abort_ = 0;
  absl::flat_hash_map<std::string, int> num_by_type_;
};

// Given a graph with nodes in [0, num_nodes) and a set of arcs (the order is
// important), this will:
//   - Start with each nodes in separate "subsets".
//   - Consider the arc in order, and each time one connects two separate
//     subsets, merge the two subsets into a new one.
//   - Stops when there is only 2 subset left.
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
// `flat_node_dim_variables` must have num_dimensions (possibly 0) times
// num_nodes elements, with the variable associated with node n and dimension d
// at index n * num_dimensions + d.
CutGenerator CreateCVRPCutGenerator(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals, absl::Span<const int64_t> demands,
    absl::Span<const IntegerVariable> flat_node_dim_variables, int64_t capacity,
    Model* model);

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

#endif  // OR_TOOLS_SAT_ROUTING_CUTS_H_
