// Copyright 2010-2022 Google LLC
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

// Algorithms to encode constraints into their SAT representation. Currently,
// this contains one possible encoding of a cardinality constraint as used by
// the core-based optimization algorithm in optimization.h.
//
// This is also known as the incremental totalizer encoding in the literature.

#ifndef OR_TOOLS_SAT_ENCODING_H_
#define OR_TOOLS_SAT_ENCODING_H_

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/types.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// This class represents a number in [0, ub]. The encoding uses ub binary
// variables x_i with i in [0, ub) where x_i means that the number is > i. It is
// called an EncodingNode, because it represents one node of the tree used to
// encode a cardinality constraint.
//
// In practice, not all literals are explicitly created:
// - Only the literals in [lb, current_ub) are "active" at a given time.
// - The represented number is known to be >= lb.
// - It may be greater than current_ub, but the extra literals will be only
//   created lazily. In all our solves, the literal current_ub - 1, will always
//   be assumed to false (i.e. the number will be <= current_ub - 1).
// - Note that lb may increase and ub decrease as more information is learned
//   about this node by the sat solver.
//
// This is roughly based on the cardinality constraint encoding described in:
// Bailleux and Yacine Boufkhad, "Efficient CNF Encoding of Boolean Cardinality
// Constraints", In Proc. of CP 2003, pages 108-122, 2003.
class EncodingNode {
 public:
  EncodingNode() = default;

  // Static creation functions.
  //
  // The generic version constructs a node with value in [lb, ub].
  // New literal "<=x" will be constructed using create_lit(x).
  static EncodingNode ConstantNode(Coefficient weight);
  static EncodingNode LiteralNode(Literal l, Coefficient weight);
  static EncodingNode GenericNode(int lb, int ub,
                                  std::function<Literal(int x)> create_lit,
                                  Coefficient weight);

  // Creates a "full" encoding node on n new variables, the represented number
  // beeing in [lb, ub = lb + n). The variables are added to the given solver
  // with the basic implications linking them:
  //   literal(0) >= ... >= literal(n-1)
  void InitializeFullNode(int n, EncodingNode* a, EncodingNode* b,
                          SatSolver* solver);

  // Creates a "lazy" encoding node representing the sum of a and b.
  // Only one literals will be created by this operation. Note that no clauses
  // linking it with a or b are added by this function.
  void InitializeLazyNode(EncodingNode* a, EncodingNode* b, SatSolver* solver);
  void InitializeLazyCoreNode(Coefficient weight, EncodingNode* a,
                              EncodingNode* b);

  // If we know that all the literals[0] of the given nodes are in "at most one"
  // relationship, we can create a node that is the sum of them with a simple
  // encoding. This does create linking implications.
  void InitializeAmoNode(absl::Span<EncodingNode* const> nodes,
                         SatSolver* solver);

  // Returns a literal with the meaning 'this node number is > i'.
  // The given i must be in [lb_, current_ub).
  Literal GreaterThan(int i) const { return literal(i - lb_); }

  // Accessors to size() and literals in [lb, current_ub).
  int size() const { return literals_.size(); }
  Literal literal(int i) const {
    CHECK_GE(i, 0);
    CHECK_LT(i, literals_.size());
    return literals_[i];
  }

  // Sort by decreasing depth first and then by increasing variable index.
  // This is meant to be used by the priority queue in MergeAllNodesWithPQ().
  bool operator<(const EncodingNode& other) const {
    return depth_ > other.depth_ ||
           (depth_ == other.depth_ && other.for_sorting_ > for_sorting_);
  }

  // Creates a new literals and increases current_ub.
  // Returns false if we were already at the upper bound for this node.
  bool IncreaseCurrentUB(SatSolver* solver);

  // Indicate that the node cannot grow further than its current assumption.
  void TransformToBoolean(SatSolver* solver);

  // Removes the left-side literals fixed to 1. Note that this increases lb_ and
  // reduces the number of active literals. It also removes any right-side
  // literals fixed to 0. If such a literal exists, ub is updated accordingly.
  //
  // Return the overall weight increase.
  Coefficient Reduce(const SatSolver& solver);

  // GetAssumption() might need to create new literals.
  bool AssumptionIs(Literal other) const;
  Literal GetAssumption(SatSolver* solver);
  bool HasNoWeight() const;
  void IncreaseWeightLb();

  // Fix any literal that would cause the weight of this node to go over the
  // gap.
  void ApplyWeightUpperBound(Coefficient gap, SatSolver* solver);

  void set_weight(Coefficient w) {
    weight_lb_ = lb_;
    weight_ = w;
  }
  Coefficient weight() const { return weight_; }

  // The depth is mainly used as an heuristic to decide which nodes to merge
  // first. See the < operator.
  void set_depth(int depth) { depth_ = depth; }
  int depth() const { return depth_; }

  int lb() const { return lb_; }
  int weight_lb() const { return weight_lb_; }
  int current_ub() const { return lb_ + literals_.size(); }
  int ub() const { return ub_; }
  EncodingNode* child_a() const { return child_a_; }
  EncodingNode* child_b() const { return child_b_; }

  // We use the solver to display the current values of the literals.
  std::string DebugString(const VariablesAssignment& assignment) const;

 private:
  int depth_ = 0;
  int lb_ = 0;
  int ub_ = 1;
  BooleanVariable for_sorting_;

  // The weight is only applied for literal >= this lb.
  int weight_lb_ = 0;

  Coefficient weight_;
  EncodingNode* child_a_ = nullptr;
  EncodingNode* child_b_ = nullptr;

  // If not null, will be used instead of creating new variable directly.
  std::function<Literal(int x)> create_lit_ = nullptr;

  // The literals of this node in order.
  std::vector<Literal> literals_;
};

// Merges the two given EncodingNodes by creating a new node that corresponds to
// the sum of the two given ones. Only the left-most binary variable is created
// for the parent node, the other ones will be created later when needed.
EncodingNode LazyMerge(EncodingNode* a, EncodingNode* b, SatSolver* solver);

// Increases the size of the given node by one. To keep all the needed relations
// with its children, we also need to increase their size by one, and so on
// recursively. Also adds all the necessary clauses linking the newly added
// literals.
void IncreaseNodeSize(EncodingNode* node, SatSolver* solver);

// Merges the two given EncodingNode by creating a new node that corresponds to
// the sum of the two given ones. The given upper_bound is interpreted as a
// bound on this sum, and allows creating fewer binary variables.
EncodingNode FullMerge(Coefficient upper_bound, EncodingNode* a,
                       EncodingNode* b, SatSolver* solver);

// Merges all the given nodes two by two until there is only one left. Returns
// the final node which encodes the sum of all the given nodes.
EncodingNode* MergeAllNodesWithDeque(Coefficient upper_bound,
                                     const std::vector<EncodingNode*>& nodes,
                                     SatSolver* solver,
                                     std::deque<EncodingNode>* repository);

// Same as MergeAllNodesWithDeque() but use a priority queue to merge in
// priority nodes with smaller sizes. This also enforce that the sum of nodes
// is greater than its lower bound.
EncodingNode* LazyMergeAllNodeWithPQAndIncreaseLb(
    Coefficient weight, const std::vector<EncodingNode*>& nodes,
    SatSolver* solver, std::deque<EncodingNode>* repository);

// Reduces the nodes using the now fixed literals, update the lower-bound, and
// returns the set of assumptions for the next round of the core-based
// algorithm. Returns an empty set of assumptions if everything is fixed.
void ReduceNodes(Coefficient upper_bound, Coefficient* lower_bound,
                 std::vector<EncodingNode*>* nodes, SatSolver* solver);
std::vector<Literal> ExtractAssumptions(Coefficient stratified_lower_bound,
                                        const std::vector<EncodingNode*>& nodes,
                                        SatSolver* solver);

// Returns the minimum weight of the nodes in the core. Note that the literal in
// the core must appear in the same order as the one in nodes.
Coefficient ComputeCoreMinWeight(const std::vector<EncodingNode*>& nodes,
                                 const std::vector<Literal>& core);

// Returns the maximum node weight under the given upper_bound. Returns zero if
// no such weight exist (note that a node weight is strictly positive, so this
// make sense).
Coefficient MaxNodeWeightSmallerThan(const std::vector<EncodingNode*>& nodes,
                                     Coefficient upper_bound);

// The class reponsible for processing cores and maintaining a Boolean encoding
// of the linear objective.
class ObjectiveEncoder {
 public:
  explicit ObjectiveEncoder(Model* model)
      : params_(*model->GetOrCreate<SatParameters>()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        implications_(model->GetOrCreate<BinaryImplicationGraph>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()) {}

  // Updates the encoding using the given core. The literals in the core must
  // match the order in nodes. Returns false if the model become infeasible.
  bool ProcessCore(const std::vector<Literal>& core, Coefficient min_weight,
                   Coefficient gap, std::string* info);

  void AddBaseNode(EncodingNode node) {
    repository_.push_back(std::move(node));
    nodes_.push_back(&repository_.back());
  }

  // TODO(user): Remove mutable version once refactoring is done.
  const std::vector<EncodingNode*>& nodes() const { return nodes_; }
  std::vector<EncodingNode*>* mutable_nodes() { return &nodes_; }

 private:
  // There is more than one way to create new assumptions and encode the
  // information from this core. This is slightly different from ProcessCore()
  // and follow the algorithm used by many of the top max-SAT solver under the
  // name incremental OLL. This is described in: António Morgado, Carmine
  // Dodaro, Joao Marques-Silva. "Core-Guided MaxSAT with Soft Cardinality
  // Constraints". CP 2014. pp. 564-573. António Morgado, Alexey Ignatiev, Joao
  // Marques-Silva. "MSCG: Robust Core-Guided MaxSAT Solving." JSAT 9. 2014. pp.
  // 129-134.
  //
  // TODO(user): The last time this was tested, it was however not as good as
  // the ProcessCore() version. That might change as we code/change more
  // heuristic, so we keep it around.
  const bool alternative_encoding_ = false;

  // Nodes point into repository_.
  std::vector<EncodingNode*> nodes_;
  std::deque<EncodingNode> repository_;

  const SatParameters& params_;
  SatSolver* sat_solver_;
  BinaryImplicationGraph* implications_;
  ModelRandomGenerator* random_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_ENCODING_H_
