// Copyright 2010-2013 Google
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
// solution.
// The algorithm uses dynamic programming. Its time complexity is O(n*2^n),
// where n is the number of nodes to be visited.
// Its space complexity is also O(n*2^n).
//
// Note that the naive implementation of the SHPP
// exploring all permutations without memorizing intermediate results would
// have a complexity of (n-1)! (factorial of (n-1) ), which is much higher
// than n*2^n. To convince oneself of this, just use Stirling's formula:
// n! ~ sqrt(2*pi*n)*(n/exp(1))^n .
// Because of these complexity figures, the algorithm is not practical for
// problems with more than 20 nodes.
//
// Here is how the algorithm works:
// Let us denote the nodes to be visited by their indices 0..n-1
// Let us pick 0 as the starting node.
// Let d(i,j) denote the distance (or cost) from i to j.
// f(S,j) where S is a set of nodes and j is a node is defined as follows:
// f(S,j) = min(i in S, f(S\{i}, i) + d(i,j))
//
// The set S can be represented by an integer where bit i corresponds to
// element i in the set. In the following S denotes the integer corresponding
// to set S.
//
// The computation of f(S,j) is implemented in the method ComputeShortestPath.
//
// To implement dynamic programming, we store the preceding results of
// computing f(S,i) in an array M[S][i]. Since there are 2^n subsets of the
// set {0..n-1}, there are 2^n rows and n columns in this array. This
// explains the complexity figures.
//
// The array M is initialized as follows:
// for i in (0..n-1) M[0][i] = d(0,i)
// The dynamic programming iteration is as follows:
// for S in (0..2^n-2)
//   for i in (0..n-1) M[S][i]=f(S,i)
//
// The dynamic programming iteration is implemented in the method Solve.
// The optimal value of the Hamiltonian path from 0 to n-1 is given by
// f(2^n-1,n-1).
// The optimal value of the Traveling Salesman tour is given by f(2^n-1,0).
// (There is actually no need to duplicate the first node.)
//
// There are a few tricks in the efficient implementation of this algorithm
// which are explained below.
//
// Keywords: Traveling Salesman, Hamiltonian Path, Dynamic Programming,
//           Held, Karp.

#ifndef OR_TOOLS_GRAPH_HAMILTONIAN_PATH_H_
#define OR_TOOLS_GRAPH_HAMILTONIAN_PATH_H_

#include <math.h>
#include <stddef.h>
#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "util/bitset.h"

namespace operations_research {

typedef int PathNodeIndex;

template <typename T> class HamiltonianPathSolver {
// HamiltonianPathSolver computes a minimum Hamiltonian path over a graph
// defined by a cost matrix. The cost matrix need not be symmetric.
// The Hamiltonian path can be closed, in this case it's a Hamiltonian cycle,
// i.e. the algorithm solves the Travelling Salesman Problem.
// Example:

// std::vector<std::vector<int> > cost_mat;
// ... fill in cost matrix
// HamiltonianPathSolver<int> mhp(cost_mat);     // no computation done
// printf("%d\n", mhp.TravelingSalesmanCost());  // computation done and stored

 public:
  // Currently in 2010, 26 is the maximum you can solve with
  // 24 Gigs of RAM, and it takes several minutes.
  // Considering the complexity of the algorithm (n*2^n), and that
  // there are very good ways to solve TSP with more than 32 cities,
  // we limit ourselves to 32 cites.
  // This is why we define the type NodeSet to be 32-bit wide.
  typedef uint32 NodeSet;

  explicit HamiltonianPathSolver(const std::vector<std::vector<T> >& cost);
  ~HamiltonianPathSolver();

  // Replaces the cost matrix while avoiding re-allocating memory.
  void ChangeCostMatrix(const std::vector<std::vector<T> >& cost);

  // Returns the Hamiltonian path cost.
  T HamiltonianCost();

  // Returns the Hamiltonian path in the vector pointed to by the argument.
  void HamiltonianPath(std::vector<PathNodeIndex>* path);

  // Returns the cost of the TSP tour.
  T TravelingSalesmanCost();

  // Returns the TSP tour in the vector pointed to by the argument.
  void TravelingSalesmanPath(std::vector<PathNodeIndex>* path);

  // Returns true if there won't be precision issues.
  // This is always true for integers, but not for floating-point types.
  bool IsRobust();

  // Returns true if the cost matrix verifies the triangle inequality.
  bool VerifiesTriangleInequality();

 private:
  // Initializes robust_.
  void CheckRobustness();

  // Initializes verifies_triangle_inequality_.
  void CheckTriangleInequality();

  // Perfoms the Dynamic Programming iteration.
  void ComputeShortestPath(NodeSet s, PathNodeIndex dest);

  // Copies the cost matrix passed as argument to the internal data structure.
  void CopyCostMatrix(const std::vector<std::vector<T> >& cost);

  // Reserves memory. Used in constructor and ChangeCostMatrix.
  void Init(const std::vector<std::vector<T> >& cost);

  // Frees memory. Used in destructor and ChangeCostMatrix.
  void Free();

  // Computes path by looking at the information in memory_.
  void Path(PathNodeIndex end,
            std::vector<PathNodeIndex>* path);

  // Does all the Dynamic Progamming iterations. Calls ComputeShortestPath.
  void Solve();

  bool               robust_;
  bool               triangle_inequality_ok_;
  bool               robustness_checked_;
  bool               triangle_inequality_checked_;
  bool               solved_;
  PathNodeIndex      num_nodes_;
  T**                cost_;
  NodeSet            two_power_num_nodes_;
  T**                memory_;
};

static const int kHamiltonianPathSolverPadValue = 1557;

template <typename T>
HamiltonianPathSolver<T>::HamiltonianPathSolver(const std::vector<std::vector<T> >& cost)
    : robust_(true),
      triangle_inequality_ok_(true),
      robustness_checked_(false),
      triangle_inequality_checked_(false),
      solved_(false),
      num_nodes_(0),
      cost_(NULL),
      two_power_num_nodes_(0),
      memory_(NULL) {
  Init(cost);
}

template <typename T> HamiltonianPathSolver<T>::~HamiltonianPathSolver() {
  Free();
}

template <typename T>
void HamiltonianPathSolver<T>::Free() {
  if (num_nodes_ > 0) {
    delete [] memory_[0];
    delete [] memory_;
    for (int i = 0; i < num_nodes_; ++i) {
      delete [] cost_[i];
    }
    delete [] cost_;
  }
}



template <typename T> void HamiltonianPathSolver<T>::
                           ChangeCostMatrix(const std::vector<std::vector<T> >& cost) {
  robustness_checked_ = false;
  triangle_inequality_checked_ = false;
  solved_ = false;
  if (cost.size() == num_nodes_ && num_nodes_ > 0) {
    CopyCostMatrix(cost);
  } else {
    Free();
    Init(cost);
  }
}

template <typename T>
void HamiltonianPathSolver<T>::CopyCostMatrix(const std::vector<std::vector<T> >& cost) {
  for (int i = 0; i < num_nodes_; ++i) {
    CHECK_EQ(num_nodes_, cost[i].size()) << "Cost matrix must be square";
    for (int j = 0; j < num_nodes_; ++j) {
      cost_[i][j] = cost[i][j];
    }
  }
}

template <typename T>
bool HamiltonianPathSolver<T>::IsRobust() {
  if (!robustness_checked_) {
    CheckRobustness();
  }
  return robust_;
}

template <typename T>
bool HamiltonianPathSolver<T>::VerifiesTriangleInequality() {
  if (!triangle_inequality_checked_) {
    CheckTriangleInequality();
  }
  return triangle_inequality_ok_;
}

template <typename T>
void HamiltonianPathSolver<T>::CheckRobustness() {
  T min_cost = std::numeric_limits<T>::max();
  T max_cost = std::numeric_limits<T>::min();

  // We compute the min and max for the cost matrix.
  for (int i = 0; i < num_nodes_; ++i) {
    for (int j = 0; j < num_nodes_; ++j) {
      if (i == j) break;
      min_cost = std::min(min_cost, cost_[i][j]);
      max_cost = std::max(max_cost, cost_[i][j]);
    }
  }
  // We determine if the range of the cost matrix is going to
  // make the algorithm not robust because of precision issues.
  if (min_cost < 0) {
    robust_ = false;
  } else {
    robust_ = (min_cost >
               num_nodes_ * max_cost * std::numeric_limits<T>::epsilon());
  }
  robustness_checked_ = true;
}

template <typename T>
void HamiltonianPathSolver<T>::CheckTriangleInequality() {
  triangle_inequality_ok_ = true;
  triangle_inequality_checked_ = true;
  for (int k = 0; k < num_nodes_; ++k) {
    for (int i = 0; i < num_nodes_; ++i) {
      for (int j = 0; j < num_nodes_; ++j) {
        T detour_cost = cost_[i][k] + cost_[k][j];
        if (detour_cost < cost_[i][j]) {
          triangle_inequality_ok_ = false;
          return;
        }
      }
    }
  }
}

template <typename T>
void HamiltonianPathSolver<T>::Init(const std::vector<std::vector<T> >& cost) {
  num_nodes_ = cost.size();
  if (num_nodes_ > 0) {
    cost_ = new T*[num_nodes_];
    for (int i = 0; i < num_nodes_; ++i) {
      cost_[i] = new T[num_nodes_];
    }

    CopyCostMatrix(cost);

    two_power_num_nodes_ = 1 << num_nodes_;

    // M[S][i] as defined in the header file is transposed and stored as
    // memory_[i][S]. This is due to the fact that the patterns generated
    // by the iteration on the subsets have very similar low bits and thus
    // yield cache misses.
    // To avoid cache misses, we pad the arrays and extend them by a magic
    // number.
    // This results in a 20 to 30% gain depending on architectures.

    const int padded_size = two_power_num_nodes_
                          + kHamiltonianPathSolverPadValue;
    memory_ = new T * [num_nodes_];
    memory_[0] = new T[num_nodes_ * padded_size];
    for (int i = 1; i < num_nodes_; ++i) {
      memory_[i] = memory_[i-1] + padded_size;
    }
  }
}

template <typename T>
void HamiltonianPathSolver<T>::ComputeShortestPath(NodeSet subset,
                                                   PathNodeIndex dest) {
  // We iterate on the set bits in the NodeSet subset, instead of checking
  // which bits are set as in the loop:
  // for (int src = 0; src < num_nodes_; ++src) {
  //   const NodeSet singleton = (1 << src);
  //   if (subset & singleton) { ...
  // This results in a 30% gain.

  const NodeSet first_singleton = LeastSignificantBitWord32(subset);
  const PathNodeIndex first_src =
                                 LeastSignificantBitPosition32(first_singleton);
  NodeSet start_subset = subset - first_singleton;
  T min_cost = memory_[first_src][start_subset] + cost_[first_src][dest];
  NodeSet copy = start_subset;
  while (copy != 0) {
    const NodeSet singleton = LeastSignificantBitWord32(copy);
    const PathNodeIndex src = LeastSignificantBitPosition32(singleton);
    const NodeSet new_subset = subset - singleton;
    const T cost = memory_[src][new_subset] + cost_[src][dest];
    if (cost < min_cost) {
      min_cost = cost;
    }
    copy -= singleton;
  }
  memory_[dest][subset] = min_cost;
}

template <typename T> void HamiltonianPathSolver<T>::Solve() {
  if (solved_) return;
  for (PathNodeIndex dest = 0; dest < num_nodes_; ++dest) {
    memory_[dest][0] = cost_[0][dest];
  }
  for (NodeSet subset = 1; subset < two_power_num_nodes_; ++subset) {
    for (PathNodeIndex dest = 0; dest < num_nodes_; ++dest) {
      ComputeShortestPath(subset, dest);
    }
  }
  solved_ = true;
}


template <typename T> T HamiltonianPathSolver<T>::HamiltonianCost() {
  if (num_nodes_ <= 1) {
    return 0;
  }
  Solve();
  return memory_[num_nodes_ - 1][two_power_num_nodes_ - 1];
}

template <typename T> void HamiltonianPathSolver<T>::
    HamiltonianPath(std::vector<PathNodeIndex>* path) {
  if (num_nodes_ <= 1) {
    path->resize(1);
    (*path)[0] = 0;
    return;
  }
  Solve();
  path->resize(num_nodes_);
  return Path(num_nodes_ - 1, path);
}

// The naive way of getting the optimal path would be to store the subpath
// in the same way the cost is stored in the matrix memory_.
// The end of ComputeShortestPair (int i = 0; i < size; ++i)
// would look like:
// ...
//   if (cost < min_cost) {
//     min_cost = cost;
//     best_path = new_subset;
//   }
//   copy -= singleton;
//   }
//   memory_[dest][subset] = min_cost;
//   path_[dest][subset] = best_path;
// }
//
// One can however get the optimal path from the matrix memory_, by
// considering that:
//     min_cost = memory_[src][subset - singleton] + cost_[src][dest]
// The goal is thus to find a singleton satisfying the above equality.
// The algorithm is therefore straightforward, apart from the fact that
// care has to be taken about precision in case the type parameter T
// is float our double (see below).
//
// The net result of this is that we gain a factor 2 over storing the paths
// in a path_ matrix. Since the algorithm is memory-bound and due to the fact
// that there are also less cache misses, there is a factor 2.5x in speed
// when using this trick.

template <typename T>
void HamiltonianPathSolver<T>::Path(PathNodeIndex end,
                                    std::vector<PathNodeIndex>* path) {
  PathNodeIndex dest = end;
  NodeSet current_set = two_power_num_nodes_ - 1;
  // It may happen that node 0 be on a segment (node i, node j), in which case
  // it would appear in the middle of the path. We explicitly remove it from
  // the set to be explored to avoid this.
  current_set -= 1;
  int i = num_nodes_ - 1;
  while (current_set != 0) {
    NodeSet copy = current_set;
    while (copy != 0) {
      const NodeSet singleton = LeastSignificantBitWord32(copy);
      const PathNodeIndex src = LeastSignificantBitPosition32(singleton);
      const NodeSet incumbent_set = current_set - singleton;
      const double current_cost = memory_[dest][current_set];
      const double incumbent_cost = memory_[src][incumbent_set]
                                  + cost_[src][dest];
      // We take precision into account in case T is float or double.
      // There is no visible penalty in the case T is an integer type.
      if (fabs(current_cost - incumbent_cost)
          <=  std::numeric_limits<T>::epsilon() * current_cost) {
        current_set = incumbent_set;
        dest = src;
        (*path)[i] = dest;
        break;
      }
      copy -= singleton;
    }
    --i;
  }
}

template <typename T> T HamiltonianPathSolver<T>::TravelingSalesmanCost() {
  if (num_nodes_ <= 1) {
    return 0;
  }
  Solve();
  return memory_[0][two_power_num_nodes_ - 1];
}

template <typename T> void HamiltonianPathSolver<T>::
    TravelingSalesmanPath(std::vector<PathNodeIndex>* path) {
  if (num_nodes_ <= 1) {
    path->resize(1);
    (*path)[0] = 0;
    return;
  }
  Solve();
  path->resize(num_nodes_ + 1);
  Path(0, path);
  // To compute the TSP, we actually use the trick of computing the Hamiltonian
  // path starting at node 0 and ending at node 0, instead of duplicating
  // node 0.
  // This brings a speedup of 50%, but we need to remember putting 0 at the
  // end of the path...
  (*path)[num_nodes_] = 0;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_HAMILTONIAN_PATH_H_
