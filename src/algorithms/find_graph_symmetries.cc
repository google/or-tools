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
#include "algorithms/find_graph_symmetries.h"

#include <algorithm>
#include "base/join.h"
#include "algorithms/dynamic_partition.h"
#include "algorithms/sparse_permutation.h"
#include "util/iterators.h"

namespace operations_research {

GraphSymmetryFinder::GraphSymmetryFinder(const Graph& graph)
    : graph_(graph),
      tmp_node_mapping_(NumNodes(), -1),
      tmp_node_mask_(NumNodes(), false),
      tmp_in_degree_(NumNodes(), 0),
      tmp_nodes_with_indegree_(NumNodes() + 1) {
  for (int i = 0; i < NumNodes(); ++i) tmp_node_mapping_[i] = i;
  tmp_partition_.Reset(NumNodes());
}

namespace {
// Whether the "arcs1" adjacency list maps to "arcs2" under the
// dense node mapping "node_mapping".
// This method uses a transient bitmask on all the graph nodes, which
// should be entirely false before the call (and will be restored as such
// after it).
//
// TODO(user): Make this method support multi-arcs, and see if that's
// sufficient to make the whole algorithm support multi-arcs.
template<class Graph, class AdjList>
bool AdjacencyListMapsToAdjacencyList(
    const Graph& graph, const AdjList& arcs1, const AdjList& arcs2,
    const std::vector<int>& node_mapping, std::vector<bool>* tmp_node_mask) {
  int num_arcs_delta = 0;
  bool match = true;
  for (const int arc : arcs2) {
    ++num_arcs_delta;
    (*tmp_node_mask)[graph.Head(arc)] = true;
  }
  for (const int arc : arcs1) {
    --num_arcs_delta;
    const int mapped_head = node_mapping[graph.Head(arc)];
    if (!(*tmp_node_mask)[mapped_head]) {
      match = false;
      break;
    }
    (*tmp_node_mask)[mapped_head] = false;
  }
  if (num_arcs_delta != 0) match = false;
  if (!match) {
    // We need to clean up tmp_node_mask.
    for (const int arc : arcs2) (*tmp_node_mask)[graph.Head(arc)] = false;
  }
  return match;
}
}  // namespace

bool GraphSymmetryFinder::IsGraphAutomorphism(const SparsePermutation& perm)
    const {
  bool is_automorphism = true;
  // Build the node mapping for "perm".
  for (int c = 0; c < perm.NumCycles(); ++c) {
    // TODO(user): use the global element->image iterator when it exists.
    int prev = *(perm.Cycle(c).end() - 1);
    for (const int e : perm.Cycle(c)) {
      tmp_node_mapping_[prev] = e;
      prev = e;
    }
  }

  // Compare the outgoing and incoming adjacency lists.
  for (int c = 0; is_automorphism && c < perm.NumCycles(); ++c) {
    // TODO(user): use the global element->image iterator when it exists.
    int prev = *(perm.Cycle(c).end() - 1);
    for (const int e : perm.Cycle(c)) {
      is_automorphism &= AdjacencyListMapsToAdjacencyList(
          graph_, graph_.OutgoingArcs(prev), graph_.OutgoingArcs(e),
          tmp_node_mapping_, &tmp_node_mask_);
      is_automorphism &= AdjacencyListMapsToAdjacencyList(
          graph_, graph_.IncomingArcs(prev), graph_.IncomingArcs(e),
          tmp_node_mapping_, &tmp_node_mask_);
      prev = e;
    }
  }

  // Clean up tmp_node_mapping_.
  for (const int e : perm.Support()) tmp_node_mapping_[e] = e;
  return is_automorphism;
}

void GraphSymmetryFinder::RecursivelyRefinePartitionByAdjacency(
    int first_unrefined_part_index, DynamicPartition* partition) {
  // Assuming that the partition was refined based on the adjacency on
  // parts [0 .. first_unrefined_part_index) already, we simply need to
  // refine parts first_unrefined_part_index ... NumParts()-1, the latter bound
  // being a moving target:
  // When a part #p < first_unrefined_part_index gets modified, it's always
  // split in two: itself, and a new part #p'. Since #p was already refined
  // on, we only need to further refine on *one* of its two split parts.
  // And this will be done because p' > first_unrefined_part_index.
  //
  // Thus, the following loop really does the full recursive refinement as
  // advertised.
  for (int part_index = first_unrefined_part_index;
       part_index < partition->NumParts();  // Moving target!
       ++part_index) {
    // Count the aggregated in-degree of all nodes, only looking at arcs that
    // come from the current part.
    for (const int src : partition->ElementsInPart(part_index)) {
      for (const int arc : graph_.OutgoingArcs(src)) {
        const int dst = graph_.Head(arc);
        const int in_degree = ++tmp_in_degree_[dst];
        if (in_degree == 1) tmp_nodes_with_nonzero_indegree_.push_back(dst);
      }
    }
    // Group the nodes by (nonzero) in-degree. Remember the maximum in-degree.
    int max_in_degree = 0;
    for (const int node : tmp_nodes_with_nonzero_indegree_) {
      const int in_degree = tmp_in_degree_[node];
      tmp_in_degree_[node] = 0;  // To clean up after us.
      max_in_degree = std::max(max_in_degree, in_degree);
      tmp_nodes_with_indegree_[in_degree].push_back(node);
    }
    tmp_nodes_with_nonzero_indegree_.clear();  // To clean up after us.
    // For each in-degree, refine the partition by the set of nodes with
    // that in-degree.
    for (int in_degree = 1; in_degree <= max_in_degree; ++in_degree) {
      partition->Refine(tmp_nodes_with_indegree_[in_degree]);
      tmp_nodes_with_indegree_[in_degree].clear();  // To clean up after us.
    }
  }
}

void GraphSymmetryFinder::DistinguishNodeInPartition(
    int node, DynamicPartition* partition) {
  partition->Refine(std::vector<int>(1, node));
  RecursivelyRefinePartitionByAdjacency(partition->PartOf(node), partition);
}

namespace {
// This method's API is hard to understand out of context. See its call site.
// This creates the permutation that sends each singleton of 'base' to the
// corresponding singleton of 'image'.
void ExtractPermutationFromPartitionSingletons(
    const DynamicPartition& base, const DynamicPartition& image,
    SparsePermutation* permutation) {
  const int num_parts = base.NumParts();
  std::vector<bool> part_was_seen(num_parts, false);
  for (int p = 0; p < num_parts; ++p) {
    if (part_was_seen[p]) continue;
    if (base.SizeOfPart(p) != 1) continue;
    DCHECK_EQ(1, image.SizeOfPart(p));
    const int start = *base.ElementsInPart(p).begin();
    int next = *image.ElementsInPart(p).begin();
    if (next == start) continue;  // Matching singletons.
    // We found non-matching singletons. Iterate over the orbit that they
    // are part of.
    permutation->AddToCurrentCycle(start);
    part_was_seen[p] = true;
    for (int e = next; e != start;) {
      permutation->AddToCurrentCycle(e);
      const int part = base.PartOf(e);
      part_was_seen[part] = true;
      e = *image.ElementsInPart(part).begin();
    }
    permutation->CloseCurrentCycle();
  }
}

void MergeNodeEquivalenceClassesAccordingToPermutation(
    const SparsePermutation& perm, MergingPartition* node_equivalence_classes) {
  for (int c = 0; c < perm.NumCycles(); ++c) {
    // TODO(user): use the global element->image iterator when it exists.
    int prev = -1;
    for (const int e : perm.Cycle(c)) {
      if (prev >= 0) node_equivalence_classes->MergePartsOf(prev, e);
      prev = e;
    }
  }
}
}  // namespace

void GraphSymmetryFinder::FindSymmetries(
    std::vector<int>* node_equivalence_classes_io,
    std::vector<std::unique_ptr<SparsePermutation>>* generators,
    std::vector<int>* factorized_automorphism_group_size) {
  if (node_equivalence_classes_io->size() != NumNodes()) {
    LOG(DFATAL) << "GraphSymmetryFinder::FindSymmetries() was given an invalid"
                << " 'node_equivalence_classes_io' argument.";
    return;
  }
  DynamicPartition base_partition(*node_equivalence_classes_io);

  // First, break all inherent asymmetries in the graph.
  RecursivelyRefinePartitionByAdjacency(/*first_unrefined_part_index=*/ 0,
                                        &base_partition);
  VLOG(4) << "Base partition: "
          << base_partition.DebugString(DynamicPartition::SORT_BY_PART);

  // To find all permutations of the Graph that satisfy the current partition,
  // we pick an element v that is not in a singleton part, and we
  // split the search in two phases:
  // 1) Find (the generators of) all permutations that keep v invariant.
  // 2) For each w in PartOf(v) such that w != v:
  //      find *one* permutation that maps v to w, if it exists.
  //      if it does exists, add this to the generators.
  //
  // The part 1) is recursive.
  //
  // Since we can't really use true recursion because it will be too deep for
  // the stack, we implement it iteratively. To do that, we unroll 1):
  // the "invariant dive" is a single pass that successively refines the node
  // partition with elements from non-singleton parts (the 'invariant node'),
  // until all parts are singletons.
  // We remember which nodes we picked as invariants, and also the successive
  // partition sizes as we refine it, to allow us to backtrack.
  // Then we'll perform 2) in the reverse order, backtracking the stack from 1)
  // as using another dedicated stack for the search (see below).
  struct InvariantDiveState {
    int invariant_node;
    int num_parts_before_refinement;

    InvariantDiveState(int node, int num_parts)
      : invariant_node(node), num_parts_before_refinement(num_parts) {}
  };
  std::vector<InvariantDiveState> invariant_dive_stack;
  for (int invariant_node = 0; invariant_node < NumNodes(); ++invariant_node) {
    if (base_partition.SizeOfPart(base_partition.PartOf(invariant_node)) != 1) {
      invariant_dive_stack.push_back(
          InvariantDiveState(invariant_node, base_partition.NumParts()));
      DistinguishNodeInPartition(invariant_node, &base_partition);
      VLOG(4) << "Invariant dive: invariant node = " << invariant_node
              << "; partition after: "
              << base_partition.DebugString(DynamicPartition::SORT_BY_PART);
    }
  }
  // Now we've dived to the bottom: we're left with the identity permutation,
  // which we don't need as a generator. We move on to phase 2).

  generators->clear();
  MergingPartition node_equivalence_classes;
  node_equivalence_classes.Reset(NumNodes());
  factorized_automorphism_group_size->clear();
  std::vector<std::vector<int>> permutations_displacing_node(NumNodes());

  while (!invariant_dive_stack.empty()) {
    // Backtrack the last step of 1) (the invariant dive).
    const int root_node = invariant_dive_stack.back().invariant_node;
    const int base_num_parts =
        invariant_dive_stack.back().num_parts_before_refinement;
    invariant_dive_stack.pop_back();
    base_partition.UndoRefineUntilNumPartsEqual(base_num_parts);
    VLOG(4) << "Backtracking invariant dive: root node = " << root_node
            << "; partition: "
            << base_partition.DebugString(DynamicPartition::SORT_BY_PART);

    // Now we'll try to map "root_node" to all image nodes that seem compatible
    // and that aren't "root_node" itself.
    //
    // Doing so, we're able to detect potential bad (or good) matches by
    // refining the 'base' partition with "root_node"; and refining the
    // 'image' partition (which represents the partition of images nodes,
    // i.e. the nodes after applying the currently implicit permutation)
    // with that candidate image node: if the two partitions don't match, then
    // the candidate image isn't compatible.
    // If the partitions do match, we might either find the underlying
    // permutation directly, or we might need to further try and map other
    // nodes to their image: this is a recursive search with backtracking.
    //
    // TODO(user): This copy may be slow. Consider moving it upwards; and
    // update both the image and the base partition at the same time.
    DynamicPartition image_partition = base_partition;

    // The potential images of root_node are the nodes in its part. They can be
    // pruned by the already computed equivalence classes.
    std::vector<int> potential_root_image_nodes;
    for (const int e :
         image_partition.ElementsInPart(image_partition.PartOf(root_node))) {
      if (e != root_node) potential_root_image_nodes.push_back(e);
    }
    node_equivalence_classes.KeepOnlyOneNodePerPart(
        &potential_root_image_nodes);

    // Try to map "root_node" to all of its potential images. For each image,
    // we only care about finding a single compatible permutation, if it exists.
    while (!potential_root_image_nodes.empty()) {
      const int root_image_node = potential_root_image_nodes.back();

      std::unique_ptr<SparsePermutation> permutation =
          FindOneSuitablePermutation(root_node, root_image_node,
                                     &base_partition, &image_partition,
                                     *generators, permutations_displacing_node);

      if (permutation.get() != nullptr) {
        // We found a permutation. We store it in the list of generators, and
        // further prune out the remaining 'root' image candidates, taking into
        // account the permutation we just found.
        MergeNodeEquivalenceClassesAccordingToPermutation(
            *permutation, &node_equivalence_classes);
        // HACK(user): to make sure that we keep root_image_node as the
        // representant of its part, we temporarily move it to the front
        // of the vector.
        std::swap(potential_root_image_nodes[0],
                  potential_root_image_nodes.back());
        node_equivalence_classes.KeepOnlyOneNodePerPart(
            &potential_root_image_nodes);
        std::swap(potential_root_image_nodes[0],
                  potential_root_image_nodes.back());

        // Register it onto the permutations_displacing_node vector.
        const int permutation_index = static_cast<int>(generators->size());
        for (const int node : permutation->Support()) {
          permutations_displacing_node[node].push_back(permutation_index);
        }

        // Move the permutation to the generator list (this also transfers
        // ownership).
        generators->push_back(std::move(permutation));
      }

      potential_root_image_nodes.pop_back();
    }

    // We keep track of the size of the orbit of 'root_node' under the
    // current subgroup: this is one of the factors of the total group size.
    // TODO(user): better, more complete explanation.
    factorized_automorphism_group_size->push_back(
        node_equivalence_classes.NumNodesInSamePartAs(root_node));
  }
  node_equivalence_classes.FillEquivalenceClasses(node_equivalence_classes_io);
}

std::unique_ptr<SparsePermutation>
GraphSymmetryFinder::FindOneSuitablePermutation(
    int root_node, int root_image_node, DynamicPartition* base_partition,
    DynamicPartition* image_partition,
    const std::vector<std::unique_ptr<SparsePermutation>>& generators_found_so_far,
    const std::vector<std::vector<int>>& permutations_displacing_node) {
  // TODO(user): break down this method into smaller pieces, if there is a good
  // way.
  std::unique_ptr<SparsePermutation> permutation;

  const int base_num_parts = base_partition->NumParts();
  DCHECK_EQ(base_partition->DebugString(DynamicPartition::SORT_BY_PART),
            image_partition->DebugString(DynamicPartition::SORT_BY_PART));
  DistinguishNodeInPartition(root_node, base_partition);

  DCHECK(search_states_.empty());
  search_states_.push_back(SearchState(
      /*base_node=*/root_node,
      /*potential_image_nodes=*/std::vector<int>(1, root_image_node),
      /*num_parts_before_trying_to_map_base_node=*/base_num_parts,
      /*potential_image_nodes_were_pruned=*/true));
  while (!search_states_.empty()) {
    SearchState* const ss = &search_states_.back();
    // At this stage, we're supposed to have:
    // - A base_partition that has already been refined on ss->base_node
    // - An image partition that hasn't been refined.
    const int unrefined_num_parts = image_partition->NumParts();
    if (ss->potential_image_nodes.empty()) {
      VLOG(4) << "  Backtracking one level up: base_node " << ss->base_node
              << " has no suitable image left to consider.";
      base_partition->UndoRefineUntilNumPartsEqual(unrefined_num_parts);
      image_partition->UndoRefineUntilNumPartsEqual(
          ss->num_parts_before_trying_to_map_base_node);
      search_states_.pop_back();
      continue;
    }

    // Pop the currently chosen image node.
    const int image_node = ss->potential_image_nodes.back();
    ss->potential_image_nodes.pop_back();

    // Try to see if the base permutation and the image permutation would
    // agree.
    // TODO(user, fdid): Optimize this: For example, check for
    // incompatibilities between the two partition as we (recursively)
    // refine them, to catch mismatches earlier.
    VLOG(4) << "    Distinguishing image node " << image_node;
    DistinguishNodeInPartition(image_node, image_partition);
    VLOG(4) << "    Image partition (refined)  : "
            << image_partition->DebugString(DynamicPartition::SORT_BY_PART);

    // Verify the compatibility of the base and image partitions. Since they
    // were compatible before the Refinement, it suffices to search among
    // the newly created parts.
    bool compatible = base_partition->NumParts() == image_partition->NumParts();
    for (int p = unrefined_num_parts;
         compatible && p < base_partition->NumParts(); ++p) {
      compatible &=
          base_partition->ParentOfPart(p) == image_partition->ParentOfPart(p) &&
          base_partition->SizeOfPart(p) == image_partition->SizeOfPart(p);
    }
    int part_to_map = -1;
    if (!compatible) {
      VLOG(4) << "    Incompatible partitions!";
    } else {
      VLOG(4) << "    Partitions look compatible.";
      // Look for non-trivially matching parts, i.e. non-singleton parts that
      // don't have the same elements.
      // TODO(user, fdid): optimize this. Some ideas:
      // - Remember singletons to skip them quickly.
      // - Maybe there's a way to avoid having to re-sort the elements in each
      //   part, or to do some of that sorting in the DynamicPartition code.
      int non_singleton_part = -1;
      for (int p = 0; p < base_partition->NumParts(); ++p) {
        if (base_partition->SizeOfPart(p) == 1) continue;  // Singletons.
        if (non_singleton_part < 0) non_singleton_part = p;
        base_partition->SortElementsInPart(p);
        image_partition->SortElementsInPart(p);
        const DynamicPartition::IterablePart base_part =
            base_partition->ElementsInPart(p);
        const DynamicPartition::IterablePart image_part =
            image_partition->ElementsInPart(p);
        if (!std::equal(base_part.begin(), base_part.end(),
                        image_part.begin())) {
          part_to_map = p;
          break;
        }
      }

      if (part_to_map == -1) {
        // All the parts are matching! We have our permutation candidate:
        // it sends singletons of base_permutation onto the corresponding
        // ones of image_permutation, and is the identity on all other
        // elements.
        permutation.reset(new SparsePermutation(NumNodes()));
        ExtractPermutationFromPartitionSingletons(
            *base_partition, *image_partition, permutation.get());
        VLOG(4) << "    Full partition match! Permutation candidate: "
                << permutation->DebugString();

        if (IsGraphAutomorphism(*permutation)) {
          VLOG(4) << "    Valid automorphism found!";
          break;
        }
        VLOG(4) << "    That permutation is NOT a valid automorphism.";
        permutation.reset(nullptr);
        if (non_singleton_part == -1) {
          VLOG(4) << "    The permutation was fully specified; backtracking";
        } else {
          VLOG(4) << "  Deepening the search...";
          part_to_map = non_singleton_part;
        }
      }
    }
    if (part_to_map == -1) {
      VLOG(4) << "    Apply the backtracking: choosing a different image node.";
      image_partition->UndoRefineUntilNumPartsEqual(unrefined_num_parts);
      // TODO(user): apply a smarter test to decide whether to do the pruning
      // or not: we can accurately estimate the cost of pruning (iterate through
      // all generators found so far) and its estimated benefit (the cost of
      // the search below the state that we're currently in, times the expected
      // number of pruned nodes). Sometimes it may be better to skip the
      // pruning.
      if (!ss->potential_image_nodes_were_pruned &&
          ss->potential_image_nodes.size() > 0) {
        // Prune the remaining potential image nodes: there is no permutation
        // that maps base_node -> image_node that is compatible with the current
        // partitions, so there can't be a permutation that maps base_node -> X
        // either, for all X in the orbit of 'image_node' under valid
        // permutations compatible with the current partitions.
        FindPermutationsCompatibleWithPartition(
            permutations_displacing_node[image_node], generators_found_so_far,
            image_partition, &tmp_compatible_permutations_);
        for (const SparsePermutation* perm : tmp_compatible_permutations_) {
          // TODO(user): ignore cycles that are outside of image_part.
          MergeNodeEquivalenceClassesAccordingToPermutation(*perm,
                                                            &tmp_partition_);
        }
        // HACK(user): temporarily re-inject image_node in the front of
        // 'potential_image_nodes' so that it's the only one we keep in its
        // class.
        ss->potential_image_nodes.push_back(image_node);
        std::swap(ss->potential_image_nodes.back(),
                  ss->potential_image_nodes[0]);
        tmp_partition_.KeepOnlyOneNodePerPart(&ss->potential_image_nodes);
        std::swap(ss->potential_image_nodes.back(),
                  ss->potential_image_nodes[0]);
        ss->potential_image_nodes.pop_back();
        ss->potential_image_nodes_were_pruned = true;

        // Reset "tmp_partition_" sparsely.
        for (const SparsePermutation* perm : tmp_compatible_permutations_) {
          for (const int node : perm->Support()) tmp_partition_.ResetNode(node);
        }
      }
      // Actually backtrack now.
      continue;
    }

    // There are some non-trivially matching parts, so we have to keep
    // searching deeper, by looking into that part.
    const DynamicPartition::IterablePart base_part =
        base_partition->ElementsInPart(part_to_map);
    const DynamicPartition::IterablePart image_part =
        image_partition->ElementsInPart(part_to_map);
    // TODO(user, fdid): try some heuristics to optimize the choice of the
    // base node. For example, select a base node that maps to itself.
    const int next_base_node = *base_part.begin();
    search_states_.push_back(SearchState(
        /*base_node*/ next_base_node,
        /*potential_image_nodes*/ std::vector<int>(image_part.begin(),
                                              image_part.end()),
        /*num_parts_before_trying_to_map_base_node*/ unrefined_num_parts,
        false));
    DistinguishNodeInPartition(next_base_node, base_partition);
    VLOG(4) << "    Distinguishing new base node " << next_base_node;
  }

  // Whether we found a permutation or not, we must reset the partitions to
  // their original state before returning, and clear the search.
  base_partition->UndoRefineUntilNumPartsEqual(base_num_parts);
  image_partition->UndoRefineUntilNumPartsEqual(base_num_parts);
  search_states_.clear();

  return permutation;  // Will return nullptr if uninitialized.
}

void GraphSymmetryFinder::FindPermutationsCompatibleWithPartition(
    const std::vector<int>& permutation_indices,
    const std::vector<std::unique_ptr<SparsePermutation>>& permutation_vector,
    DynamicPartition* partition,
    std::vector<const SparsePermutation*>* compatible_permutations) {
  compatible_permutations->clear();
  // TODO(user): investigate further optimizations: maybe it's possible
  // to incrementally maintain the set of permutations that is compatible
  // with the current partition, instead of recomputing it here?
  for (const int p : permutation_indices) {
    const SparsePermutation& permutation = *permutation_vector[p];
    // First, a quick compatibility check: the permutation's cycles must be
    // smaller or equal to the size of the part that they are included in.
    bool compatible = true;
    for (int c = 0; c < permutation.NumCycles(); ++c) {
      const SparsePermutation::Iterator cycle = permutation.Cycle(c);
      if (cycle.size() >
          partition->SizeOfPart(partition->PartOf(*cycle.begin()))) {
        compatible = false;
        break;
      }
    }
    if (!compatible) continue;
    // Now the full compatibility check: each cycle of the permutation must
    // be fully included in an image part.
    for (int c = 0; c < permutation.NumCycles(); ++c) {
      int part = -1;
      for (const int node : permutation.Cycle(c)) {
        if (partition->PartOf(node) != part) {
          if (part >= 0) {
            compatible = false;
            break;
          }
          part = partition->PartOf(node);  // Initilization of 'part'.
        }
      }
    }
    if (!compatible) continue;
    // The permutation is fully compatible!
    compatible_permutations->push_back(&permutation);
  }
}

}  // namespace operations_research
