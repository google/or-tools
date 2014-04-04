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
// TODO(user, fdid): Refine this toplevel comment when this file settles.

#ifndef OR_TOOLS_ALGORITHMS_FIND_GRAPH_SYMMETRIES_H_
#define OR_TOOLS_ALGORITHMS_FIND_GRAPH_SYMMETRIES_H_

#include "base/unique_ptr.h"
#include <vector>

#include "algorithms/dynamic_partition.h"
#include "graph/graph.h"

namespace operations_research {

class DynamicPartition;
class SparsePermutation;

class GraphSymmetryFinder {
 public:
  typedef ReverseArcMixedGraph<> Graph;
  // "graph" must not have multi-arcs.
  // TODO(user): support multi-arcs.
  explicit GraphSymmetryFinder(const Graph& graph);

  // Whether the given permutation is an automorphism of the graph given at
  // construction. This costs O(sum(degree(x))) (the sum is over all nodes x
  // that are displaced by the permutation).
  bool IsGraphAutomorphism(const SparsePermutation& perm) const;

  // Find a set of generators of the automorphism subgroup of the graph that
  // respects the given node equivalence classes. The generators are themselves
  // permutations of the nodes: see http://en.wikipedia.org/wiki/Automorphism;
  // and these permutations may only map a node onto a node of its equivalence
  // class: two nodes i and j are in the same equivalence class iff
  // node_equivalence_classes_io[i] == node_equivalence_classes_io[j];
  //
  // This set of generators is not necessarily the smallest possible (nor in
  // the number of generators, nor in the size of these generators), but it is
  // minimal in that no generator can be removed while keeping the generated
  // group intact.
  // TODO(user): properly verify the minimality in unit tests.
  //
  // Note that if "generators" is empty, then the graph has no symmetry: the
  // only automorphism is the identity.
  //
  // The equivalence classes are actually an input/output: they are refined
  // according to all asymmetries found. In the end, n1 and n2 will be
  // considered equivalent (i.e. node_equivalence_classes_io[n1] ==
  // node_equivalence_classes_io[n2]) if and only if there exists a
  // permutation of nodes that:
  // - keeps the graph invariant
  // - maps n1 onto n2
  // - maps each node to a node of its original equivalence class.
  //
  // This method also outputs the size of the automorphism group, expressed as
  // a factorized product of integers (note that the size itself may be as
  // large as N!).
  //
  // This method is largely based on the following article, published in 2008:
  // "Faster Symmetry Discovery using Sparsity of Symmetries" by Darga, Sakallah
  // and Markov. http://web.eecs.umich.edu/~imarkov/pubs/conf/dac08-sym.pdf.
  void FindSymmetries(std::vector<int>* node_equivalence_classes_io,
                      std::vector<std::unique_ptr<SparsePermutation>>* generators,
                      std::vector<int>* factorized_automorphism_group_size);

  // **** Methods below are public FOR TESTING ONLY. ****

  // Fully refine the partition of nodes, using the graph as symmetry breaker.
  // This means applying the following steps on each part P of the partition:
  // - Compute the aggregated in-degree of all nodes of the graph, only looking
  //   at arcs originating from nodes in P.
  // - For each in-degree d=1...max_in_degree, refine the partition by the set
  //   of nodes with in-degree d.
  // And recursively applying it on all new or modified parts.
  //
  // In our use cases, we may call this in a scenario where the partition was
  // already partially refined on all parts #0...#K, then you should set
  // "first_unrefined_part_index" to K+1.
  void RecursivelyRefinePartitionByAdjacency(
      int first_unrefined_part_index, DynamicPartition* partition);

  // Special wrapper of the above method: assuming that partition is already
  // fully refined, further refine it by {node}, and propagate by adjacency.
  void DistinguishNodeInPartition(int node, DynamicPartition* partition);

 private:
  const Graph& graph_;

  inline int NumNodes() const { return graph_.num_nodes(); }

  // Internal search code used in FindSymmetries(), split out for readability:
  // find one permutation (if it exists) that maps root_node to root_image_node
  // and such that the image of "base_partition" by that permutation is equal to
  // the "image_partition". If no such permutation exists, returns nullptr.
  //
  // "generators_found_so_far" and "permutations_displacing_node" are used for
  // pruning in the search. The former is just the "generators" vector of
  // FindGraphSymmetries(), with the permutations found so far; and the latter
  // is an inverted index from each node to all permutations (that we found)
  // that displace it.
  std::unique_ptr<SparsePermutation> FindOneSuitablePermutation(
      int root_node, int root_image_node, DynamicPartition* base_partition,
      DynamicPartition* image_partition,
      const std::vector<std::unique_ptr<SparsePermutation>>& generators_found_so_far,
      const std::vector<std::vector<int>>& permutations_displacing_node);

  // Among a list of permutations (the subset of 'permutation_vector' whose
  // indices are "permutation_indices"), find those that are compatible with
  // the given partition, i.e. whose orbits are all included in a partition's
  // part (distinct orbits may be included in distinct parts).
  // This is used uniquely within FindOneSuitablePermutation().
  void FindPermutationsCompatibleWithPartition(
      const std::vector<int>& permutation_indices,
      const std::vector<std::unique_ptr<SparsePermutation>>& permutation_vector,
      DynamicPartition* partition,
      std::vector<const SparsePermutation*>* compatible_permutations);

  // Temporary vector used by IsGraphAutomorphism(). When not in use, the
  // mapping should be tmp_node_mapping_[i] = i for all i.
  mutable std::vector<int> tmp_node_mapping_;
  mutable std::vector<bool> tmp_node_mask_;

  // Temporary vectors transiently used by RefinePartitionByAdjacencyFrom(),
  // and cleaned up after each call.
  std::vector<int> tmp_in_degree_;
  std::vector<int> tmp_nodes_with_nonzero_indegree_;
  std::vector<std::vector<int>> tmp_nodes_with_indegree_;

  // Temporary data used within FindOneSuitablePermutation().
  MergingPartition tmp_partition_;
  std::vector<const SparsePermutation*> tmp_compatible_permutations_;

  // Data structure used by FindOneSuitablePermutation(). See the .cc
  struct SearchState {
    int base_node;
    std::vector<int> potential_image_nodes;
    int num_parts_before_trying_to_map_base_node;
    bool potential_image_nodes_were_pruned;

    SearchState(int bn, const std::vector<int>& vec, int num_parts, bool pruned)
      : base_node(bn),
        potential_image_nodes(vec),
        num_parts_before_trying_to_map_base_node(num_parts),
        potential_image_nodes_were_pruned(pruned) {}
  };
  std::vector<SearchState> search_states_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_FIND_GRAPH_SYMMETRIES_H_
