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

// This class solves the graph automorphism problem
// (https://en.wikipedia.org/wiki/Graph_automorphism), a variant of the famous
// graph isomorphism problem (https://en.wikipedia.org/wiki/Graph_isomorphism).
//
// The algorithm is largely based on the following article, published in 2008:
// "Faster Symmetry Discovery using Sparsity of Symmetries" by Darga, Sakallah
// and Markov. http://web.eecs.umich.edu/~imarkov/pubs/conf/dac08-sym.pdf.
//
// See the comments on the class below for more details.

#ifndef OR_TOOLS_ALGORITHMS_FIND_GRAPH_SYMMETRIES_H_
#define OR_TOOLS_ALGORITHMS_FIND_GRAPH_SYMMETRIES_H_

#include <memory>
#include <vector>

#include "absl/time/time.h"
#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/algorithms/dynamic_permutation.h"
#include "ortools/base/status.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/iterators.h"
#include "ortools/util/stats.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

class SparsePermutation;

class GraphSymmetryFinder {
 public:
  typedef ::util::StaticGraph<> Graph;

  // If the Graph passed to the GraphSymmetryFinder is undirected, i.e.
  // for every arc a->b, b->a is also present, then you should set
  // "is_undirected" to true.
  // This will, in effect, DCHECK() that the graph is indeed undirected,
  // and bypass the need for reverse adjacency lists.
  //
  // If you don't know this in advance, you may use GraphIsSymmetric() from
  // ortools/graph/util.h.
  //
  // "graph" must not have multi-arcs.
  // TODO(user): support multi-arcs.
  GraphSymmetryFinder(const Graph& graph, bool is_undirected);

  // Whether the given permutation is an automorphism of the graph given at
  // construction. This costs O(sum(degree(x))) (the sum is over all nodes x
  // that are displaced by the permutation).
  bool IsGraphAutomorphism(const DynamicPermutation& permutation) const;

  // Find a set of generators of the automorphism subgroup of the graph that
  // respects the given node equivalence classes. The generators are themselves
  // permutations of the nodes: see http://en.wikipedia.org/wiki/Automorphism.
  // These permutations may only map a node onto a node of its equivalence
  // class: two nodes i and j are in the same equivalence class iff
  // node_equivalence_classes_io[i] == node_equivalence_classes_io[j];
  //
  // This set of generators is not necessarily the smallest possible (neither in
  // the number of generators, nor in the size of these generators), but it is
  // minimal in that no generator can be removed while keeping the generated
  // group intact.
  // TODO(user): verify the minimality in unit tests.
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
  // DEADLINE AND PARTIAL COMPLETION:
  // If the deadline passed as argument is reached, this method will return
  // quickly (within a few milliseconds). The outputs may be partially filled:
  // - Each element of "generators", if non-empty, will be a valid permutation.
  // - "node_equivalence_classes_io" will contain the equivalence classes
  //   corresponding to the orbits under all the generators in "generators".
  // - "factorized_automorphism_group_size" will also be incomplete, and
  //   partially valid: its last element may be undervalued. But all prior
  //   elements are valid factors of the automorphism group size.
  util::Status FindSymmetries(
      double time_limit_seconds, std::vector<int>* node_equivalence_classes_io,
      std::vector<std::unique_ptr<SparsePermutation> >* generators,
      std::vector<int>* factorized_automorphism_group_size);

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
  void RecursivelyRefinePartitionByAdjacency(int first_unrefined_part_index,
                                             DynamicPartition* partition);

  // **** Methods below are public FOR TESTING ONLY. ****

  // Special wrapper of the above method: assuming that partition is already
  // fully refined, further refine it by {node}, and propagate by adjacency.
  // Also, optionally collect all the new singletons of the partition in
  // "new_singletons", sorted by their part number in the partition.
  void DistinguishNodeInPartition(int node, DynamicPartition* partition,
                                  std::vector<int>* new_singletons_or_null);

 private:
  const Graph& graph_;

  inline int NumNodes() const { return graph_.num_nodes(); }

  // If the graph isn't symmetric, then we store the reverse adjacency lists
  // here: for each i in 0..NumNodes()-1, the list of nodes that have an
  // outgoing arc to i is stored (sorted by node) in:
  //   flattened_reverse_adj_lists_[reverse_adj_list_index_[i] ...
  //                                reverse_adj_list_index_[i + 1]]
  // and can be iterated on easily with:
  //   for (const int tail : TailsOfIncomingArcsTo(node)) ...
  //
  // If the graph was specified as symmetric upon construction, both these
  // vectors are empty, and TailsOfIncomingArcsTo() crashes.
  std::vector<int> flattened_reverse_adj_lists_;
  std::vector<int> reverse_adj_list_index_;
  util::BeginEndWrapper<std::vector<int>::const_iterator> TailsOfIncomingArcsTo(
      int node) const;

  // Deadline management. Populated upon FindSymmetries().
  mutable std::unique_ptr<TimeLimit> time_limit_;

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
      const std::vector<std::unique_ptr<SparsePermutation> >&
          generators_found_so_far,
      const std::vector<std::vector<int> >& permutations_displacing_node);

  // Data structure used by FindOneSuitablePermutation(). See the .cc
  struct SearchState {
    int base_node;

    // We're tentatively mapping "base_node" to some image node. At first, we
    // just pick a single candidate: we fill "first_image_node". If this
    // candidate doesn't work out, we'll select all other candidates in the same
    // image part, prune them by the symmetries we found already, and put them
    // in "remaining_pruned_image_nodes" (and set "first_image_node" to -1).
    int first_image_node;
    std::vector<int> remaining_pruned_image_nodes;

    int num_parts_before_trying_to_map_base_node;

    // Only parts that are at or beyond this index, or their parent parts, may
    // be mismatching between the base and the image partitions.
    int min_potential_mismatching_part_index;

    SearchState(int bn, int in, int np, int mi)
        : base_node(bn),
          first_image_node(in),
          num_parts_before_trying_to_map_base_node(np),
          min_potential_mismatching_part_index(mi) {}

    std::string DebugString() const;
  };
  std::vector<SearchState> search_states_;

  // Subroutine of FindOneSuitablePermutation(), split out for modularity:
  // With the partial candidate mapping given by "base_partition",
  // "image_partition" and "current_permutation_candidate", determine whether
  // we have a full match (eg. the permutation is a valid candidate).
  // If so, simply return true. If not, return false but also fill
  // "next_base_node" and "next_image_node" with what should be the next mapping
  // decision.
  //
  // This also uses and updates "min_potential_mismatching_part_index_io"
  // to incrementally search for mismatching parts along the partitions.
  //
  // Note(user): there may be false positives, i.e. this method may return true
  // even if the partitions aren't actually a full match, because it uses
  // fingerprints to compare part. This should almost never happen.
  bool ConfirmFullMatchOrFindNextMappingDecision(
      const DynamicPartition& base_partition,
      const DynamicPartition& image_partition,
      const DynamicPermutation& current_permutation_candidate,
      int* min_potential_mismatching_part_index_io, int* next_base_node,
      int* next_image_node) const;

  // Subroutine of FindOneSuitablePermutation(), split out for modularity:
  // Keep only one node of "nodes" per orbit, where the orbits are described
  // by a subset of "all_permutations": the ones with indices in
  // "permutation_indices" and that are compatible with "partition".
  // For each orbit, keep the first node that appears in "nodes".
  void PruneOrbitsUnderPermutationsCompatibleWithPartition(
      const DynamicPartition& partition,
      const std::vector<std::unique_ptr<SparsePermutation> >& all_permutations,
      const std::vector<int>& permutation_indices, std::vector<int>* nodes);

  // Temporary objects used by some of the class methods, and owned by the
  // class to avoid (costly) re-allocation. Their resting states are described
  // in the side comments; with N = NumNodes().
  DynamicPermutation tmp_dynamic_permutation_;            // Identity(N)
  mutable std::vector<bool> tmp_node_mask_;               // [0..N-1] = false
  std::vector<int> tmp_degree_;                           // [0..N-1] = 0.
  std::vector<int> tmp_stack_;                            // Empty.
  std::vector<std::vector<int> > tmp_nodes_with_degree_;  // [0..N-1] = [].
  MergingPartition tmp_partition_;                        // Reset(N).
  std::vector<const SparsePermutation*> tmp_compatible_permutations_;  // Empty.

  // Internal statistics, used for performance tuning and debugging.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("GraphSymmetryFinder"),
          initialization_time("a Initialization", this),
          initialization_refine_time("b  ┗╸Refine", this),
          invariant_dive_time("c Invariant Dive", this),
          main_search_time("d Main Search", this),
          invariant_unroll_time("e  ┣╸Dive unroll", this),
          permutation_output_time("f  ┣╸Permutation output", this),
          search_time("g  ┗╸FindOneSuitablePermutation()", this),
          search_time_fail("h    ┣╸Fail", this),
          search_time_success("i    ┣╸Success", this),
          initial_search_refine_time("j    ┣╸Initial refine", this),
          search_refine_time("k    ┣╸Further refines", this),
          quick_compatibility_time("l    ┣╸Compatibility checks", this),
          quick_compatibility_fail_time("m    ┃ ┣╸Fail", this),
          quick_compatibility_success_time("n    ┃ ┗╸Success", this),
          dynamic_permutation_refinement_time(
              "o    ┣╸Dynamic permutation refinement", this),
          map_election_std_time(
              "p    ┣╸Mapping election / full match detection", this),
          map_election_std_mapping_time("q    ┃ ┣╸Mapping elected", this),
          map_election_std_full_match_time("r    ┃ ┗╸Full Match", this),
          automorphism_test_time("s    ┣╸[Upon full match] Automorphism check",
                                 this),
          automorphism_test_fail_time("t    ┃ ┣╸Fail", this),
          automorphism_test_success_time("u    ┃ ┗╸Success", this),
          search_finalize_time("v    ┣╸[Upon auto success] Finalization", this),
          dynamic_permutation_undo_time(
              "w    ┣╸[Upon auto fail, full] Dynamic permutation undo", this),
          map_reelection_time(
              "x    ┣╸[Upon auto fail, partial] Mapping re-election", this),
          non_singleton_search_time("y    ┃ ┗╸Non-singleton search", this),
          backtracking_time("z    ┗╸Backtracking", this),
          pruning_time("{      ┗╸Pruning", this),
          search_depth("~ Search Stats: search_depth", this) {}

    TimeDistribution initialization_time;
    TimeDistribution initialization_refine_time;
    TimeDistribution invariant_dive_time;
    TimeDistribution main_search_time;
    TimeDistribution invariant_unroll_time;
    TimeDistribution permutation_output_time;
    TimeDistribution search_time;
    TimeDistribution search_time_fail;
    TimeDistribution search_time_success;
    TimeDistribution initial_search_refine_time;
    TimeDistribution search_refine_time;
    TimeDistribution quick_compatibility_time;
    TimeDistribution quick_compatibility_fail_time;
    TimeDistribution quick_compatibility_success_time;
    TimeDistribution dynamic_permutation_refinement_time;
    TimeDistribution map_election_std_time;
    TimeDistribution map_election_std_mapping_time;
    TimeDistribution map_election_std_full_match_time;
    TimeDistribution automorphism_test_time;
    TimeDistribution automorphism_test_fail_time;
    TimeDistribution automorphism_test_success_time;
    TimeDistribution search_finalize_time;
    TimeDistribution dynamic_permutation_undo_time;
    TimeDistribution map_reelection_time;
    TimeDistribution non_singleton_search_time;
    TimeDistribution backtracking_time;
    TimeDistribution pruning_time;

    IntegerDistribution search_depth;
  };
  mutable Stats stats_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_FIND_GRAPH_SYMMETRIES_H_
