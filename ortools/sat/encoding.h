// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_SAT_ENCODING_H_
#define OR_TOOLS_SAT_ENCODING_H_

#include <deque>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/int_type.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

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
  EncodingNode() {}

  // Constructs a EncodingNode of size one, just formed by the given literal.
  explicit EncodingNode(Literal l);

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

  // Removes the left-side literals fixed to 1 and returns the number of
  // literals removed this way. Note that this increases lb_ and reduces the
  // number of active literals. It also removes any right-side literals fixed to
  // 0. If such a literal exists, ub is updated accordingly.
  int Reduce(const SatSolver& solver);

  // Fix the right-side variables with indices >= to the given upper_bound to
  // false.
  void ApplyUpperBound(int64 upper_bound, SatSolver* solver);

  void set_weight(Coefficient w) { weight_ = w; }
  Coefficient weight() const { return weight_; }

  int depth() const { return depth_; }
  int lb() const { return lb_; }
  int current_ub() const { return lb_ + literals_.size(); }
  int ub() const { return ub_; }
  EncodingNode* child_a() const { return child_a_; }
  EncodingNode* child_b() const { return child_b_; }

 private:
  int depth_;
  int lb_;
  int ub_;
  BooleanVariable for_sorting_;

  Coefficient weight_;
  EncodingNode* child_a_;
  EncodingNode* child_b_;

  // The literals of this node in order.
  std::vector<Literal> literals_;
};

// Note that we use <= because on 32 bits architecture, the size will actually
// be smaller than 64 bytes.
COMPILE_ASSERT(sizeof(EncodingNode) <= 64,
               ERROR_EncodingNode_is_not_well_compacted);

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
// priority nodes with smaller sizes.
EncodingNode* LazyMergeAllNodeWithPQ(const std::vector<EncodingNode*>& nodes,
                                     SatSolver* solver,
                                     std::deque<EncodingNode>* repository);

// Returns a vector with one new EncodingNode by variable in the given
// objective. Sets the offset to the negated sum of the negative coefficient,
// because in this case we negate the literals to have only positive
// coefficients.
std::vector<EncodingNode*> CreateInitialEncodingNodes(
    const std::vector<Literal>& literals,
    const std::vector<Coefficient>& coeffs, Coefficient* offset,
    std::deque<EncodingNode>* repository);
std::vector<EncodingNode*> CreateInitialEncodingNodes(
    const LinearObjective& objective_proto, Coefficient* offset,
    std::deque<EncodingNode>* repository);

// Reduces the nodes using the now fixed literals, update the lower-bound, and
// returns the set of assumptions for the next round of the core-based
// algorithm. Returns an empty set of assumptions if everything is fixed.
std::vector<Literal> ReduceNodesAndExtractAssumptions(
    Coefficient upper_bound, Coefficient stratified_lower_bound,
    Coefficient* lower_bound, std::vector<EncodingNode*>* nodes,
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

// Updates the encoding using the given core. The literals in the core must
// match the order in nodes.
void ProcessCore(const std::vector<Literal>& core, Coefficient min_weight,
                 std::deque<EncodingNode>* repository,
                 std::vector<EncodingNode*>* nodes, SatSolver* solver);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_ENCODING_H_
