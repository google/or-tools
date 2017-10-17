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


#ifndef OR_TOOLS_GRAPH_HAMILTONIAN_PATH_H_
#define OR_TOOLS_GRAPH_HAMILTONIAN_PATH_H_

// Solves the Shortest Hamiltonian Path Problem using a complete algorithm.
// The algorithm was first described in
// M. Held, R.M. Karp, A dynamic programming approach to sequencing problems,
// J. SIAM 10 (1962) 196–210
//
// The Shortest Hamiltonian Path Problem (SHPP) is similar to the Traveling
// Salesperson Problem (TSP).
// You have to visit all the cities, starting from a given one and you
// do not need to return to your starting point. With the TSP, you can start
// anywhere, but you have to return to your start location.
//
// By complete we mean that the algorithm guarantees to compute the optimal
// solution. The algorithm uses dynamic programming. Its time complexity is
// O(n^2 * 2^(n-1)), where n is the number of nodes to be visited, and '^'
// denotes exponentiation. Its space complexity is O(n * 2 ^ (n - 1)).
//
// Note that the naive implementation of the SHPP
// exploring all permutations without memorizing intermediate results would
// have a complexity of (n - 1)! (factorial of (n - 1) ), which is much higher
// than n^2 * 2^(n-1). To convince oneself of this, just use Stirling's
// formula: n! ~ sqrt(2 * pi * n)*( n / exp(1)) ^ n.
// Because of these complexity figures, the algorithm is not practical for
// problems with more than 20 nodes.
//
// Here is how the algorithm works:
// Let us denote the nodes to be visited by their indices 0 .. n - 1
// Let us pick 0 as the starting node.
// Let d(i,j) denote the distance (or cost) from i to j.
// f(S, j) where S is a set of nodes and j is a node in S is defined as follows:
// f(S, j) = min (i in S \ {j},  f(S \ {j}, i) + cost(i, j))
//                                           (j is an element of S)
// Note that this formulation, from the original Held-Karp paper is a bit
// different, but equivalent to the one used in Caseau and Laburthe, Solving
// Small TSPs with Constraints, 1997, ICLP
// f(S, j) = min (i in S, f(S \ {i}, i) + cost(i, j))
//                                           (j is not an element of S)
//
// The advantage of the Held and Karp formulation is that it enables:
// - to build the dynamic programming lattice layer by layer starting from the
// subsets with cardinality 1, and increasing the cardinality.
// - to traverse the dynamic programming lattice using sequential memory
// accesses, making the algorithm cache-friendly, and faster, despite the large
// amount of computation needed to get the position when f(S, j) is stored.
// - TODO(user): implement pruning procedures on top of the Held-Karp algorithm.
//
// The set S can be represented by an integer where bit i corresponds to
// element i in the set. In the following S denotes the integer corresponding
// to set S.
//
// The dynamic programming iteration is implemented in the method Solve.
// The optimal value of the Hamiltonian path starting at 0 is given by
// min (i in S, f(2 ^ n - 1, i))
// The optimal value of the Traveling Salesman tour is given by f(2 ^ n, 0).
// (There is actually no need to duplicate the first node, as all the paths
// are computed from node 0.)
//
// To implement dynamic programming, we store the preceding results of
// computing f(S,j) in an array M[Offset(S,j)]. See the comments about
// LatticeMemoryManager::BaseOffset() to see how this is computed.
//
// Keywords: Traveling Salesman, Hamiltonian Path, Dynamic Programming,
//           Held, Karp.

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stack>
#include <type_traits>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/vector_or_function.h"

namespace operations_research {

// TODO(user): Move the Set-related classbelow to util/bitset.h
// Iterates over the elements of a set represented as an unsigned integer,
// starting from the smallest element.  (See the class Set<Integer> below.)
template <typename Set>
class ElementIterator {
 public:
  explicit ElementIterator(Set set) : current_set_(set) {}
  bool operator!=(const ElementIterator& other) const {
    return current_set_ != other.current_set_;
  }

  // Returns the smallest element in the current_set_.
  int operator*() const { return current_set_.SmallestElement(); }

  // Advances the iterator by removing its smallest element.
  const ElementIterator& operator++() {
    current_set_ = current_set_.RemoveSmallestElement();
    return *this;
  }

 private:
  // The current position of the iterator. Stores the set consisting of the
  // not-yet iterated elements.
  Set current_set_;
};

template <typename Integer>
class Set {
 public:
  // Make this visible to classes using this class.
  typedef Integer IntegerType;

  // Useful constants.
  static const Integer One = static_cast<Integer>(1);
  static const Integer Zero = static_cast<Integer>(0);
  static const int MaxCardinality = 8 * sizeof(Integer);  // NOLINT

  // Construct a set from an Integer.
  explicit Set(Integer n) : value_(n) {
    static_assert(std::is_integral<Integer>::value, "Integral type required");
    static_assert(std::is_unsigned<Integer>::value, "Unsigned type required");
  }

  // Returns the integer corresponding to the set.
  Integer value() const { return value_; }

  static Set FullSet(Integer card) {
    return card == 0 ? Set(0) : Set(~Zero >> (MaxCardinality - card));
  }

  // Returns the singleton set with 'n' as its only element.
  static Set Singleton(Integer n) { return Set(One << n); }

  // Returns a set equal to the calling object, with element n added.
  // If n is already in the set, no operation occurs.
  Set AddElement(int n) const { return Set(value_ | (One << n)); }

  // Returns a set equal to the calling object, with element n removed.
  // If n is not in the set, no operation occurs.
  Set RemoveElement(int n) const { return Set(value_ & ~(One << n)); }

  // Returns true if the calling set contains element n.
  bool Contains(int n) const { return ((One << n) & value_) != 0; }

  // Returns true if 'other' is included in the calling set.
  bool Includes(Set other) const {
    return (value_ & other.value_) == other.value_;
  }

  // Returns the number of elements in the set. Uses the 32-bit version for
  // types that have 32-bits or less. Specialized for uint64.
  int Cardinality() const { return BitCount32(value_); }

  // Returns the index of the smallest element in the set. Uses the 32-bit
  // version for types that have 32-bits or less. Specialized for uint64.
  int SmallestElement() const { return LeastSignificantBitPosition32(value_); }

  // Returns a set equal to the calling object, with its smallest
  // element removed.
  Set RemoveSmallestElement() const { return Set(value_ & (value_ - 1)); }

  // Returns the rank of an element in a set. For the set 11100, ElementRank(4)
  // would return 2. (Ranks start at zero).
  int ElementRank(int n) const {
    DCHECK(Contains(n)) << "n = " << n << ", value_ = " << value_;
    return SingletonRank(Singleton(n));
  }

  // Returns the set consisting of the smallest element of the calling object.
  Set SmallestSingleton() const { return Set(value_ & -value_); }

  // Returns the rank of the singleton's element in the calling Set.
  int SingletonRank(Set singleton) const {
    DCHECK_EQ(singleton.value(), singleton.SmallestSingleton().value());
    return Set(value_ & (singleton.value_ - 1)).Cardinality();
  }

  // STL iterator-related member functions.
  ElementIterator<Set> begin() const {
    return ElementIterator<Set>(Set(value_));
  }
  ElementIterator<Set> end() const { return ElementIterator<Set>(Set(0)); }
  bool operator!=(const Set& other) const { return value_ != other.value_; }

 private:
  // The Integer representing the set.
  Integer value_;
};

template <>
inline int Set<uint64>::SmallestElement() const {
  return LeastSignificantBitPosition64(value_);
}

template <>
inline int Set<uint64>::Cardinality() const {
  return BitCount64(value_);
}

// An iterator for sets of increasing corresponding values that have the same
// cardinality. For example, the sets with cardinality 3 will be listed as
// ...00111, ...01011, ...01101, ...1110, etc...
template <typename SetRange>
class SetRangeIterator {
 public:
  // Make the parameter types visible to SetRangeWithCardinality.
  typedef typename SetRange::SetType SetType;
  typedef typename SetType::IntegerType IntegerType;

  explicit SetRangeIterator(const SetType set) : current_set_(set) {}

  // STL iterator-related methods.
  SetType operator*() const { return current_set_; }
  bool operator!=(const SetRangeIterator& other) const {
    return current_set_ != other.current_set_;
  }

  // Computes the next set with the same cardinality using Gosper's hack.
  // ftp://publications.ai.mit.edu/ai-publications/pdf/AIM-239.pdf ITEM 175
  // Also translated in C https://www.cl.cam.ac.uk/~am21/hakmemc.html
  const SetRangeIterator& operator++() {
    const IntegerType c = current_set_.SmallestSingleton().value();
    const IntegerType a = current_set_.value();
    const IntegerType r = c + current_set_.value();
    // Dividing by c as in HAKMEMC can be avoided by taking into account
    // that c is the smallest singleton of current_set_, and using a shift.
    const IntegerType shift = current_set_.SmallestElement();
    current_set_ = r == 0 ? SetType(0) : SetType(((r ^ a) >> (shift + 2)) | r);
    return *this;
  }

 private:
  // The current set of iterator.
  SetType current_set_;
};

template <typename Set>
class SetRangeWithCardinality {
 public:
  typedef Set SetType;
  // The end_ set is the first set with cardinality card, that does not fit
  // in max_card bits. Thus, its bit at position max_card is set, and the
  // rightmost (card - 1) bits are set.
  SetRangeWithCardinality(int card, int max_card)
      : begin_(Set::FullSet(card)),
        end_(Set::FullSet(card - 1).AddElement(max_card)) {
    DCHECK_LT(0, card);
    DCHECK_LT(0, max_card);
    DCHECK_EQ(card, begin_.Cardinality());
    DCHECK_EQ(card, end_.Cardinality());
  }

  // STL iterator-related methods.
  SetRangeIterator<SetRangeWithCardinality> begin() const {
    return SetRangeIterator<SetRangeWithCardinality>(begin_);
  }
  SetRangeIterator<SetRangeWithCardinality> end() const {
    return SetRangeIterator<SetRangeWithCardinality>(end_);
  }

 private:
  // Keep the beginning and end of the iterator.
  SetType begin_;
  SetType end_;
};

// The Dynamic Programming (DP) algorithm memorizes the values f(set, node) for
// node in set, for all the subsets of cardinality <= max_card_.
// LatticeMemoryManager manages the storage of f(set, node) so that the
// DP iteration access memory in increasing addresses.
template <typename Set, typename CostType>
class LatticeMemoryManager {
 public:
  LatticeMemoryManager() : max_card_(0) {}

  // Reserves memory and fills in the data necessary to access memory.
  void Init(int max_card);

  // Returns the offset in memory for f(s, node), with node contained in s.
  uint64 Offset(Set s, int node) const;

  // Returns the base offset in memory for f(s, node), with node contained in s.
  // This is useful in the Dynamic Programming iterations.
  // Note(user): inlining this function gains about 5%.
  // TODO(user): Investigate how to compute BaseOffset(card - 1, s \ { n })
  // from BaseOffset(card, n) to speed up the DP iteration.
  inline uint64 BaseOffset(int card, Set s) const;

  // Returns the offset delta for a set of cardinality 'card', to which
  // node 'removed_node' is replaced by 'added_node' at 'rank'
  uint64 OffsetDelta(int card, int added_node, int removed_node,
                     int rank) const {
    return card *
           (binomial_coefficients_[added_node][rank] -  // delta for added_node
            binomial_coefficients_[removed_node][rank]);  // for removed_node.
  }

  // Memorizes the value = f(s, node) at the correct offset.
  // This is favored in all other uses than the Dynamic Programming iterations.
  void SetValue(Set s, int node, CostType value);

  // Memorizes 'value' at 'offset'. This is useful in the Dynamic Programming
  // iterations where we want to avoid compute the offset of a pair (set, node).
  void SetValueAtOffset(uint64 offset, CostType value) {
    memory_[offset] = value;
  }

  // Returns the memorized value f(s, node) with node in s.
  // This is favored in all other uses than the Dynamic Programming iterations.
  CostType Value(Set s, int node) const;

  // Returns the memorized value at 'offset'.
  // This is useful in the Dynamic Programming iterations.
  CostType ValueAtOffset(uint64 offset) const { return memory_[offset]; }

 private:
  // Returns true if the values used to manage memory are set correctly.
  // This is intended to only be used in a DCHECK.
  bool CheckConsistency() const;

  // The maximum cardinality of the set on which the lattice is going to be
  // used. This is equal to the number of nodes in the TSP.
  int max_card_;

  // binomial_coefficients_[n][k] contains (n choose k).
  std::vector<std::vector<uint64>> binomial_coefficients_;

  // base_offset_[card] contains the base offset for all f(set, node) with
  // card(set) == card.
  std::vector<int64> base_offset_;

  // memory_[Offset(set, node)] contains the costs of the partial path
  // f(set, node).
  std::vector<CostType> memory_;
};

template <typename Set, typename CostType>
void LatticeMemoryManager<Set, CostType>::Init(int max_card) {
  DCHECK_LT(0, max_card);
  DCHECK_GE(Set::MaxCardinality, max_card);
  if (max_card <= max_card_) return;
  max_card_ = max_card;
  binomial_coefficients_.resize(max_card_ + 1);

  // Initialize binomial_coefficients_ using Pascal's triangle recursion.
  for (int n = 0; n <= max_card_; ++n) {
    binomial_coefficients_[n].resize(n + 2);
    binomial_coefficients_[n][0] = 1;
    for (int k = 1; k <= n; ++k) {
      binomial_coefficients_[n][k] = binomial_coefficients_[n - 1][k - 1] +
                                     binomial_coefficients_[n - 1][k];
    }
    // Extend to (n, n + 1) to minimize branchings in LatticeMemoryManager().
    // This also makes the recurrence above work for k = n.
    binomial_coefficients_[n][n + 1] = 0;
  }
  base_offset_.resize(max_card_ + 1);
  base_offset_[0] = 0;
  // There are k * binomial_coefficients_[max_card_][k] f(S,j) values to store
  // for each group of f(S,j), with card(S) = k. Update base_offset[k]
  // accordingly.
  for (int k = 0; k < max_card_; ++k) {
    base_offset_[k + 1] =
        base_offset_[k] + k * binomial_coefficients_[max_card_][k];
  }
  memory_.resize(0);
  memory_.shrink_to_fit();
  memory_.resize(max_card_ * (1 << (max_card_ - 1)));
  DCHECK(CheckConsistency());
}

template <typename Set, typename CostType>
bool LatticeMemoryManager<Set, CostType>::CheckConsistency() const {
  for (int n = 0; n <= max_card_; ++n) {
    int64 sum = 0;
    for (int k = 0; k <= n; ++k) {
      sum += binomial_coefficients_[n][k];
    }
    DCHECK_EQ(1 << n, sum);
  }
  DCHECK_EQ(0, base_offset_[1]);
  DCHECK_EQ(max_card_ * (1 << (max_card_ - 1)),
            base_offset_[max_card_] + max_card_);
  return true;
}

template <typename Set, typename CostType>
uint64 LatticeMemoryManager<Set, CostType>::BaseOffset(int card,
                                                       Set set) const {
  DCHECK_LT(0, card);
  DCHECK_EQ(set.Cardinality(), card);
  uint64 local_offset = 0;
  int node_rank = 0;
  for (int node : set) {
    // There are binomial_coefficients_[node][node_rank + 1] sets which have
    // node at node_rank.
    local_offset += binomial_coefficients_[node][node_rank + 1];
    ++node_rank;
  }
  DCHECK_EQ(card, node_rank);
  // Note(user): It is possible to get rid of base_offset_[card] by using a 2-D
  // array. It would also make it possible to free all the memory but the layer
  // being constructed and the preceding one, if another lattice of paths is
  // constructed.
  // TODO(user): Evaluate the interest of the above.
  // There are 'card' f(set, j) to store. That is why we need to multiply
  // local_offset by card before adding it to the corresponding base_offset_.
  return base_offset_[card] + card * local_offset;
}

template <typename Set, typename CostType>
uint64 LatticeMemoryManager<Set, CostType>::Offset(Set set, int node) const {
  DCHECK(set.Contains(node));
  return BaseOffset(set.Cardinality(), set) + set.ElementRank(node);
}

template <typename Set, typename CostType>
CostType LatticeMemoryManager<Set, CostType>::Value(Set set, int node) const {
  DCHECK(set.Contains(node));
  return ValueAtOffset(Offset(set, node));
}

template <typename Set, typename CostType>
void LatticeMemoryManager<Set, CostType>::SetValue(Set set, int node,
                                                   CostType value) {
  DCHECK(set.Contains(node));
  SetValueAtOffset(Offset(set, node), value);
}

// Deprecated type.
typedef int PathNodeIndex;

template <typename CostType, typename CostFunction>
class HamiltonianPathSolver {
  // HamiltonianPathSolver computes a minimum Hamiltonian path starting at node
  // 0 over a graph defined by a cost matrix. The cost function need not be
  // symmetric.
  // When the Hamiltonian path is closed, it's a Hamiltonian cycle,
  // i.e. the algorithm solves the Traveling Salesman Problem.
  // Example:

  // std::vector<std::vector<int>> cost_mat;
  // ... fill in cost matrix
  // HamiltonianPathSolver<int, std::vector<std::vector<int>>>
  //     mhp(cost_mat);  // no computation done
  // printf("%d\n", mhp.TravelingSalesmanCost());  // computation done and
  // stored
 public:
  // In 2010, 26 was the maximum solvable with 24 Gigs of RAM, and it took
  // several minutes. With this 2014 version of the code, one may go a little
  // higher, but considering the complexity of the algorithm (n*2^n), and that
  // there are very good ways to solve TSP with more than 32 cities,
  // we limit ourselves to 32 cites.
  // This is why we define the type NodeSet to be 32-bit wide.
  // TODO(user): remove this limitation by using pruning techniques.
  typedef uint32 Integer;
  typedef Set<Integer> NodeSet;

  explicit HamiltonianPathSolver(CostFunction cost);
  HamiltonianPathSolver(int num_nodes, CostFunction cost);

  // Replaces the cost matrix while avoiding re-allocating memory.
  void ChangeCostMatrix(CostFunction cost);
  void ChangeCostMatrix(int num_nodes, CostFunction cost);

  // Returns the cost of the Hamiltonian path from 0 to end_node.
  CostType HamiltonianCost(int end_node);

  // Returns the shortest Hamiltonian path from 0 to end_node.
  std::vector<int> HamiltonianPath(int end_node);

  // Returns the end-node that yields the shortest Hamiltonian path of
  // all shortest Hamiltonian path from 0 to end-node (end-node != 0).
  int BestHamiltonianPathEndNode();

  // Deprecated API. Stores HamiltonianPath(BestHamiltonianPathEndNode()) into
  // *path.
  void HamiltonianPath(std::vector<PathNodeIndex>* path);

  // Returns the cost of the TSP tour.
  CostType TravelingSalesmanCost();

  // Returns the TSP tour in the vector pointed to by the argument.
  std::vector<int> TravelingSalesmanPath();

  // Deprecated API.
  void TravelingSalesmanPath(std::vector<PathNodeIndex>* path);

  // Returns true if there won't be precision issues.
  // This is always true for integers, but not for floating-point types.
  bool IsRobust();

  // Returns true if the cost matrix verifies the triangle inequality.
  bool VerifiesTriangleInequality();

 private:
  // Saturated arithmetic helper class.
  template <typename T,
            bool = true /* Dummy parameter to allow specialization */>
  // Returns the saturated addition of a and b. It is specialized below for
  // int32 and int64.
  struct SaturatedArithmetic {
    static T Add(T a, T b) { return a + b; }
    static T Sub(T a, T b) { return a - b; }
  };
  template <bool Dummy>
  struct SaturatedArithmetic<int64, Dummy> {
    static int64 Add(int64 a, int64 b) { return CapAdd(a, b); }
    static int64 Sub(int64 a, int64 b) { return CapSub(a, b); }
  };
  // TODO(user): implement this natively in saturated_arithmetic.h
  template <bool Dummy>
  struct SaturatedArithmetic<int32, Dummy> {
    static int32 Add(int32 a, int32 b) {
      const int64 a64 = a;
      const int64 b64 = b;
      const int64 min_int32 = kint32min;
      const int64 max_int32 = kint32max;
      return static_cast<int32>(
          std::max(min_int32, std::min(max_int32, a64 + b64)));
    }
    static int32 Sub(int32 a, int32 b) {
      const int64 a64 = a;
      const int64 b64 = b;
      const int64 min_int32 = kint32min;
      const int64 max_int32 = kint32max;
      return static_cast<int32>(
          std::max(min_int32, std::min(max_int32, a64 - b64)));
    }
  };

  template <typename T>
  using Saturated = SaturatedArithmetic<T>;

  // Returns the cost value between two nodes.
  CostType Cost(int i, int j) { return cost_(i, j); }

  // Does all the Dynamic Progamming iterations.
  void Solve();

  // Computes a path by looking at the information in mem_.
  std::vector<int> ComputePath(CostType cost, NodeSet set, int end);

  // Returns true if the path covers all nodes, and its cost is equal to cost.
  bool PathIsValid(const std::vector<int>& path, CostType cost);

  // Cost function used to build Hamiltonian paths.
  MatrixOrFunction<CostType, CostFunction, true> cost_;

  // The number of nodes in the problem.
  int num_nodes_;

  // The cost of the computed TSP path.
  CostType tsp_cost_;

  // The cost of the computed Hamiltonian path.
  std::vector<CostType> hamiltonian_costs_;

  bool robust_;
  bool triangle_inequality_ok_;
  bool robustness_checked_;
  bool triangle_inequality_checked_;
  bool solved_;
  std::vector<int> tsp_path_;

  // The vector of smallest Hamiltonian paths starting at 0, indexed by their
  // end nodes.
  std::vector<std::vector<int>> hamiltonian_paths_;

  // The end node that gives the smallest Hamiltonian path. The smallest
  // Hamiltonian path starting at 0 of all
  // is hamiltonian_paths_[best_hamiltonian_path_end_node_].
  int best_hamiltonian_path_end_node_;

  LatticeMemoryManager<NodeSet, CostType> mem_;
};

// Utility function to simplify building a HamiltonianPathSolver from a functor.
template <typename CostType, typename CostFunction>
HamiltonianPathSolver<CostType, CostFunction> MakeHamiltonianPathSolver(
    int num_nodes, CostFunction cost) {
  return HamiltonianPathSolver<CostType, CostFunction>(num_nodes,
                                                       std::move(cost));
}

template <typename CostType, typename CostFunction>
HamiltonianPathSolver<CostType, CostFunction>::HamiltonianPathSolver(
    CostFunction cost)
    : HamiltonianPathSolver<CostType, CostFunction>(cost.size(), cost) {}

template <typename CostType, typename CostFunction>
HamiltonianPathSolver<CostType, CostFunction>::HamiltonianPathSolver(
    int num_nodes, CostFunction cost)
    : cost_(std::move(cost)),
      num_nodes_(num_nodes),
      tsp_cost_(0),
      hamiltonian_costs_(0),
      robust_(true),
      triangle_inequality_ok_(true),
      robustness_checked_(false),
      triangle_inequality_checked_(false),
      solved_(false) {
  CHECK_GE(NodeSet::MaxCardinality, num_nodes_);
  CHECK(cost_.Check());
}

template <typename CostType, typename CostFunction>
void HamiltonianPathSolver<CostType, CostFunction>::ChangeCostMatrix(
    CostFunction cost) {
  ChangeCostMatrix(cost.size(), cost);
}

template <typename CostType, typename CostFunction>
void HamiltonianPathSolver<CostType, CostFunction>::ChangeCostMatrix(
    int num_nodes, CostFunction cost) {
  robustness_checked_ = false;
  triangle_inequality_checked_ = false;
  solved_ = false;
  cost_.Reset(cost);
  num_nodes_ = num_nodes;
  CHECK_GE(NodeSet::MaxCardinality, num_nodes_);
  CHECK(cost_.Check());
}

template <typename CostType, typename CostFunction>
void HamiltonianPathSolver<CostType, CostFunction>::Solve() {
  if (solved_) return;
  if (num_nodes_ == 0) {
    tsp_cost_ = 0;
    tsp_path_ = {0};
    hamiltonian_paths_.resize(1);
    hamiltonian_costs_.resize(1);
    best_hamiltonian_path_end_node_ = 0;
    hamiltonian_costs_[0] = 0;
    hamiltonian_paths_[0] = {0};
    return;
  }
  mem_.Init(num_nodes_);
  // Initialize the first layer of the search lattice, taking into account
  // that base_offset_[1] == 0. (This is what the DCHECK_EQ is for).
  for (int dest = 0; dest < num_nodes_; ++dest) {
    DCHECK_EQ(dest, mem_.BaseOffset(1, NodeSet::Singleton(dest)));
    mem_.SetValueAtOffset(dest, Cost(0, dest));
  }

  // Populate the dynamic programming lattice layer by layer, by iterating
  // on cardinality.
  for (int card = 2; card <= num_nodes_; ++card) {
    // Iterate on sets of same cardinality.
    for (NodeSet set : SetRangeWithCardinality<Set<uint32>>(card, num_nodes_)) {
      // Using BaseOffset and maintaining the node ranks, to reduce the
      // computational effort for accessing the data.
      const uint64 set_offset = mem_.BaseOffset(card, set);
      // The first subset on which we'll iterate is set.RemoveSmallestElement().
      // Compute its offset. It will be updated incrementaly. This saves about
      // 30-35% of computation time.
      uint64 subset_offset =
          mem_.BaseOffset(card - 1, set.RemoveSmallestElement());
      int prev_dest = set.SmallestElement();
      int dest_rank = 0;
      for (int dest : set) {
        CostType min_cost = std::numeric_limits<CostType>::max();
        const NodeSet subset = set.RemoveElement(dest);
        // We compute the offset for subset from the preceding iteration
        // by taking into account that prev_dest is now in subset, and
        // that dest is now removed from subset.
        subset_offset += mem_.OffsetDelta(card - 1, prev_dest, dest, dest_rank);
        int src_rank = 0;
        for (int src : subset) {
          min_cost = std::min(
              min_cost, Saturated<CostType>::Add(
                            Cost(src, dest),
                            mem_.ValueAtOffset(subset_offset + src_rank)));
          ++src_rank;
        }
        prev_dest = dest;
        mem_.SetValueAtOffset(set_offset + dest_rank, min_cost);
        ++dest_rank;
      }
    }
  }

  const NodeSet full_set = NodeSet::FullSet(num_nodes_);

  // Get the cost of the tsp from node 0. It is the path that leaves 0 and goes
  // through all other nodes, and returns at 0, with minimal cost.
  tsp_cost_ = mem_.Value(full_set, 0);
  tsp_path_ = ComputePath(tsp_cost_, full_set, 0);

  hamiltonian_paths_.resize(num_nodes_);
  hamiltonian_costs_.resize(num_nodes_);
  // Compute the cost of the Hamiltonian paths starting from node 0, going
  // through all the other nodes, and ending at end_node. Compute the minimum
  // one along the way.
  CostType min_hamiltonian_cost = std::numeric_limits<CostType>::max();
  const NodeSet hamiltonian_set = full_set.RemoveElement(0);
  for (int end_node : hamiltonian_set) {
    const CostType cost = mem_.Value(hamiltonian_set, end_node);
    hamiltonian_costs_[end_node] = cost;
    if (cost <= min_hamiltonian_cost) {
      min_hamiltonian_cost = cost;
      best_hamiltonian_path_end_node_ = end_node;
    }
    DCHECK_LE(tsp_cost_, Saturated<CostType>::Add(cost, Cost(end_node, 0)));
    // Get the Hamiltonian paths.
    hamiltonian_paths_[end_node] =
        ComputePath(hamiltonian_costs_[end_node], hamiltonian_set, end_node);
  }

  solved_ = true;
}

template <typename CostType, typename CostFunction>
std::vector<int> HamiltonianPathSolver<CostType, CostFunction>::ComputePath(
    CostType cost, NodeSet set, int end_node) {
  DCHECK(set.Contains(end_node));
  const int path_size = set.Cardinality() + 1;
  std::vector<int> path(path_size, 0);
  NodeSet subset = set.RemoveElement(end_node);
  path[path_size - 1] = end_node;
  int dest = end_node;
  CostType current_cost = cost;
  for (int rank = path_size - 2; rank >= 0; --rank) {
    for (int src : subset) {
      const CostType partial_cost = mem_.Value(subset, src);
      const CostType incumbent_cost =
          Saturated<CostType>::Add(partial_cost, Cost(src, dest));
      // Take precision into account when CosttType is float or double.
      // There is no visible penalty in the case CostType is an integer type.
      if (std::abs(Saturated<CostType>::Sub(current_cost, incumbent_cost)) <=
          std::numeric_limits<CostType>::epsilon() * current_cost) {
        subset = subset.RemoveElement(src);
        current_cost = partial_cost;
        path[rank] = src;
        dest = src;
        break;
      }
    }
  }
  DCHECK_EQ(0, subset.value());
  DCHECK(PathIsValid(path, cost));
  return path;
}

template <typename CostType, typename CostFunction>
bool HamiltonianPathSolver<CostType, CostFunction>::PathIsValid(
    const std::vector<int>& path, CostType cost) {
  NodeSet coverage(0);
  for (int node : path) {
    coverage = coverage.AddElement(node);
  }
  DCHECK_EQ(NodeSet::FullSet(num_nodes_).value(), coverage.value());
  CostType check_cost = 0;
  for (int i = 0; i < path.size() - 1; ++i) {
    check_cost =
        Saturated<CostType>::Add(check_cost, Cost(path[i], path[i + 1]));
  }
  DCHECK_LE(std::abs(Saturated<CostType>::Sub(cost, check_cost)),
            std::numeric_limits<CostType>::epsilon() * cost)
      << "cost = " << cost << " check_cost = " << check_cost;
  return true;
}

template <typename CostType, typename CostFunction>
bool HamiltonianPathSolver<CostType, CostFunction>::IsRobust() {
  if (std::numeric_limits<CostType>::is_integer) return true;
  if (robustness_checked_) return robust_;
  CostType min_cost = std::numeric_limits<CostType>::max();
  CostType max_cost = std::numeric_limits<CostType>::min();

  // We compute the min and max for the cost matrix.
  for (int i = 0; i < num_nodes_; ++i) {
    for (int j = 0; j < num_nodes_; ++j) {
      if (i == j) continue;
      min_cost = std::min(min_cost, Cost(i, j));
      max_cost = std::max(max_cost, Cost(i, j));
    }
  }
  // We determine if the range of the cost matrix is going to
  // make the algorithm not robust because of precision issues.
  robust_ = min_cost >= 0 &&
            min_cost > num_nodes_ * max_cost *
                           std::numeric_limits<CostType>::epsilon();
  robustness_checked_ = true;
  return robust_;
}

template <typename CostType, typename CostFunction>
bool HamiltonianPathSolver<CostType,
                           CostFunction>::VerifiesTriangleInequality() {
  if (triangle_inequality_checked_) return triangle_inequality_ok_;
  triangle_inequality_ok_ = true;
  triangle_inequality_checked_ = true;
  for (int k = 0; k < num_nodes_; ++k) {
    for (int i = 0; i < num_nodes_; ++i) {
      for (int j = 0; j < num_nodes_; ++j) {
        const CostType detour_cost =
            Saturated<CostType>::Add(Cost(i, k), Cost(k, j));
        if (detour_cost < Cost(i, j)) {
          triangle_inequality_ok_ = false;
          return triangle_inequality_ok_;
        }
      }
    }
  }
  return triangle_inequality_ok_;
}

template <typename CostType, typename CostFunction>
int HamiltonianPathSolver<CostType,
                          CostFunction>::BestHamiltonianPathEndNode() {
  Solve();
  return best_hamiltonian_path_end_node_;
}

template <typename CostType, typename CostFunction>
CostType HamiltonianPathSolver<CostType, CostFunction>::HamiltonianCost(
    int end_node) {
  Solve();
  return hamiltonian_costs_[end_node];
}

template <typename CostType, typename CostFunction>
std::vector<int> HamiltonianPathSolver<CostType, CostFunction>::HamiltonianPath(
    int end_node) {
  Solve();
  return hamiltonian_paths_[end_node];
}

template <typename CostType, typename CostFunction>
void HamiltonianPathSolver<CostType, CostFunction>::HamiltonianPath(
    std::vector<PathNodeIndex>* path) {
  *path = HamiltonianPath(best_hamiltonian_path_end_node_);
}

template <typename CostType, typename CostFunction>
CostType
HamiltonianPathSolver<CostType, CostFunction>::TravelingSalesmanCost() {
  Solve();
  return tsp_cost_;
}

template <typename CostType, typename CostFunction>
std::vector<int>
HamiltonianPathSolver<CostType, CostFunction>::TravelingSalesmanPath() {
  Solve();
  return tsp_path_;
}

template <typename CostType, typename CostFunction>
void HamiltonianPathSolver<CostType, CostFunction>::TravelingSalesmanPath(
    std::vector<PathNodeIndex>* path) {
  *path = TravelingSalesmanPath();
}

template <typename CostType, typename CostFunction>
class PruningHamiltonianSolver {
  // PruningHamiltonianSolver computes a minimum Hamiltonian path from node 0
  // over a graph defined by a cost matrix, with pruning. For each search state,
  // PruningHamiltonianSolver computes the lower bound for the future overall
  // TSP cost, and stops further search if it exceeds the current best solution.

  // For the heuristics to determine future lower bound over visited nodeset S
  // and last visited node k, the cost of minimum spanning tree of (V \ S) ∪ {k}
  // is calculated and added to the current cost(S). The cost of MST is
  // guaranteed to be smaller than or equal to the cost of Hamiltonian path,
  // because Hamiltonian path is a spanning tree itself.

  // TODO(user): Use generic map-based cache instead of lattice-based one.
  // TODO(user): Use SaturatedArithmetic for better precision.

 public:
  typedef uint32 Integer;
  typedef Set<Integer> NodeSet;

  explicit PruningHamiltonianSolver(CostFunction cost);
  PruningHamiltonianSolver(int num_nodes, CostFunction cost);

  // Returns the cost of the Hamiltonian path from 0 to end_node.
  CostType HamiltonianCost(int end_node);

  // TODO(user): Add function to return an actual path.
  // TODO(user): Add functions for Hamiltonian cycle.

 private:
  // Returns the cost value between two nodes.
  CostType Cost(int i, int j) { return cost_(i, j); }

  // Solve and get TSP cost.
  void Solve(int end_node);

  // Compute lower bound for remaining subgraph.
  CostType ComputeFutureLowerBound(NodeSet current_set, int last_visited);

  // Cost function used to build Hamiltonian paths.
  MatrixOrFunction<CostType, CostFunction, true> cost_;

  // The number of nodes in the problem.
  int num_nodes_;

  // The cost of the computed TSP path.
  CostType tsp_cost_;

  // If already solved.
  bool solved_;

  // Memoize for dynamic programming.
  LatticeMemoryManager<NodeSet, CostType> mem_;
};

template <typename CostType, typename CostFunction>
PruningHamiltonianSolver<CostType, CostFunction>::PruningHamiltonianSolver(
    CostFunction cost)
    : PruningHamiltonianSolver<CostType, CostFunction>(cost.size(), cost) {}

template <typename CostType, typename CostFunction>
PruningHamiltonianSolver<CostType, CostFunction>::PruningHamiltonianSolver(
    int num_nodes, CostFunction cost)
    : cost_(std::move(cost)),
      num_nodes_(num_nodes),
      tsp_cost_(0),
      solved_(false) {}

template <typename CostType, typename CostFunction>
void PruningHamiltonianSolver<CostType, CostFunction>::Solve(int end_node) {
  if (solved_ || num_nodes_ == 0) return;
  // TODO(user): Use an approximate solution as a base target before solving.

  // TODO(user): Instead of pure DFS, find out the order of sets to compute
  // to utilize cache as possible.

  mem_.Init(num_nodes_);
  NodeSet start_set = NodeSet::Singleton(0);
  std::stack<std::pair<NodeSet, int>> state_stack;
  state_stack.push(std::make_pair(start_set, 0));

  while (!state_stack.empty()) {
    const std::pair<NodeSet, int> current = state_stack.top();
    state_stack.pop();

    const NodeSet current_set = current.first;
    const int last_visited = current.second;
    const CostType current_cost = mem_.Value(current_set, last_visited);

    // TODO(user): Optimize iterating unvisited nodes.
    for (int next_to_visit = 0; next_to_visit < num_nodes_; next_to_visit++) {
      // Let's to as much check possible before adding to stack.

      // Skip if this node is already visited.
      if (current_set.Contains(next_to_visit)) continue;

      // Skip if the end node is prematurely visited.
      const int next_cardinality = current_set.Cardinality() + 1;
      if (next_to_visit == end_node && next_cardinality != num_nodes_) continue;

      const NodeSet next_set = current_set.AddElement(next_to_visit);
      const CostType next_cost =
          current_cost + Cost(last_visited, next_to_visit);

      // Compare with the best cost found so far, and skip if that is better.
      const CostType previous_best = mem_.Value(next_set, next_to_visit);
      if (previous_best != 0 && next_cost >= previous_best) continue;

      // Compute lower bound of Hamiltonian cost, and skip if this is greater
      // than the best Hamiltonian cost found so far.
      const CostType lower_bound =
          ComputeFutureLowerBound(next_set, next_to_visit);
      if (tsp_cost_ != 0 && next_cost + lower_bound >= tsp_cost_) continue;

      // If next is the last node to visit, update tsp_cost_ and skip.
      if (next_cardinality == num_nodes_) {
        tsp_cost_ = next_cost;
        continue;
      }

      // Add to the stack, finally.
      mem_.SetValue(next_set, next_to_visit, next_cost);
      state_stack.push(std::make_pair(next_set, next_to_visit));
    }
  }

  solved_ = true;
}

template <typename CostType, typename CostFunction>
CostType PruningHamiltonianSolver<CostType, CostFunction>::HamiltonianCost(
    int end_node) {
  Solve(end_node);
  return tsp_cost_;
}

template <typename CostType, typename CostFunction>
CostType
PruningHamiltonianSolver<CostType, CostFunction>::ComputeFutureLowerBound(
    NodeSet current_set, int last_visited) {
  // TODO(user): Compute MST.
  return 0;  // For now, return 0 as future lower bound.
}
}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_HAMILTONIAN_PATH_H_
